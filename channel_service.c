#include <stdio.h>
#include <string.h>
#include "merc.h"
#include "channel_service.h"
#include "channel_filter.h"
#include "channel_review.h"

static bool channel_service_ready = false;
static time_t channel_subscription_last_sync = 0;

#define CHANNEL_SUBSCRIPTION_MAX_TOPICS 512

typedef struct channel_subscription_topic {
    char topic[128];
    int refs;
} CHANNEL_SUBSCRIPTION_TOPIC;

static CHANNEL_SUBSCRIPTION_TOPIC channel_active_topics[CHANNEL_SUBSCRIPTION_MAX_TOPICS];
static int channel_active_topic_count = 0;

static const CHANNEL_DEFINITION channel_definitions[] = {
    { "gossip", "Gossip", "gossip", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "ooc", "OOC", "ooc", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "quote", "Quote", "quote", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "flame", "Flame", "flame", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "helper", "Helper", "helper", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "music", "Music", "music", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "immtalk", "Immtalk", "immtalk", CHANNEL_SCOPE_GLOBAL, true, {0} },
    { "yell", "Yell", "yell", CHANNEL_SCOPE_AREA, true, {0} },
    { "gtell", "Group Tell", "gtell", CHANNEL_SCOPE_GROUP_ID, true, {0} },
    { "chtalk", "Church Talk", "chtalk", CHANNEL_SCOPE_CHURCH_ID, true, {0} },
    { NULL, NULL, NULL, CHANNEL_SCOPE_GLOBAL, false, {0} }
};

static bool channel_build_area_scope_topic(AREA_DATA *area, char *topic_buf, size_t topic_buf_sz)
{
    if (!area || !topic_buf || topic_buf_sz == 0)
        return false;

    if (!IS_NULLSTR(area->area_topic)) {
        snprintf(topic_buf, topic_buf_sz, "rt:area:%s", area->area_topic);
        return true;
    }

    if (area->uid > 0) {
        snprintf(topic_buf, topic_buf_sz, "rt:area:%ld", area->uid);
        return true;
    }

    return false;
}

static bool channel_build_region_scope_topic(AREA_REGION *region, AREA_DATA *fallback_area,
                                             char *topic_buf, size_t topic_buf_sz)
{
    if (!topic_buf || topic_buf_sz == 0)
        return false;

    if (region && !IS_NULLSTR(region->topic)) {
        snprintf(topic_buf, topic_buf_sz, "rt:region:%s", region->topic);
        return true;
    }

    if (region && region->uid > 0) {
        snprintf(topic_buf, topic_buf_sz, "rt:region:%ld", region->uid);
        return true;
    }

    return channel_build_area_scope_topic(fallback_area, topic_buf, topic_buf_sz);
}

static bool channel_build_group_scope_topic(GROUP_DATA *group, char *topic_buf, size_t topic_buf_sz)
{
    if (!group || !topic_buf || topic_buf_sz == 0)
        return false;

    if (group->id[0] == 0 && group->id[1] == 0)
        return false;

    snprintf(topic_buf, topic_buf_sz, "rt:group:%lu:%lu", group->id[0], group->id[1]);
    return true;
}

static bool channel_build_church_scope_topic(CHURCH_DATA *church, char *topic_buf, size_t topic_buf_sz)
{
    if (!church || !topic_buf || topic_buf_sz == 0)
        return false;

    if (church->uid <= 0)
        return false;

    snprintf(topic_buf, topic_buf_sz, "rt:church:%ld", church->uid);
    return true;
}

static const CHANNEL_DEFINITION *channel_find_definition(const char *channel_id)
{
    int i;

    if (IS_NULLSTR(channel_id))
        return NULL;

    for (i = 0; channel_definitions[i].id; i++) {
        if (!str_cmp(channel_definitions[i].id, channel_id))
            return &channel_definitions[i];
    }

    return NULL;
}

static bool channel_build_topic_for_sender(const CHANNEL_DEFINITION *def,
                                           CHAR_DATA *sender,
                                           char *topic_buf,
                                           size_t topic_buf_sz)
{
    if (!def || !sender || !topic_buf || topic_buf_sz == 0)
        return false;

    switch (def->scope) {
    case CHANNEL_SCOPE_GLOBAL:
        snprintf(topic_buf, topic_buf_sz, "rt:%s", def->id);
        return true;

    case CHANNEL_SCOPE_AREA:
        if (!sender->in_room || !sender->in_room->area)
            return false;
        return channel_build_area_scope_topic(sender->in_room->area, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_REGION:
        if (!sender->in_room || !sender->in_room->area)
            return false;
        return channel_build_region_scope_topic(get_room_region(sender->in_room),
                                               sender->in_room->area,
                                               topic_buf,
                                               topic_buf_sz);

    case CHANNEL_SCOPE_GROUP_ID:
        if (!IS_VALID(sender->group))
            return false;
        return channel_build_group_scope_topic(sender->group, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_CHURCH_ID:
        if (!sender->church)
            return false;
        return channel_build_church_scope_topic(sender->church, topic_buf, topic_buf_sz);

    default:
        snprintf(topic_buf, topic_buf_sz, "rt:%s", def->id);
        return true;
    }
}

static bool channel_topic_list_add_ref(CHANNEL_SUBSCRIPTION_TOPIC *topics,
                                       int *topic_count,
                                       const char *topic)
{
    int i;

    if (!topics || !topic_count || IS_NULLSTR(topic))
        return false;

    for (i = 0; i < *topic_count; i++) {
        if (!str_cmp(topics[i].topic, topic)) {
            topics[i].refs++;
            return true;
        }
    }

    if (*topic_count >= CHANNEL_SUBSCRIPTION_MAX_TOPICS)
        return false;

    strlcpy(topics[*topic_count].topic, topic, sizeof(topics[*topic_count].topic));
    topics[*topic_count].refs = 1;
    (*topic_count)++;
    return true;
}

static int channel_topic_list_find(CHANNEL_SUBSCRIPTION_TOPIC *topics,
                                   int topic_count,
                                   const char *topic)
{
    int i;

    if (!topics || IS_NULLSTR(topic))
        return -1;

    for (i = 0; i < topic_count; i++) {
        if (!str_cmp(topics[i].topic, topic))
            return i;
    }

    return -1;
}

static void channel_service_sync_subscriptions(void)
{
    CHANNEL_SUBSCRIPTION_TOPIC desired[CHANNEL_SUBSCRIPTION_MAX_TOPICS];
    int desired_count = 0;
    int i;
    DESCRIPTOR_DATA *d;

    memset(desired, 0, sizeof(desired));

    for (i = 0; channel_definitions[i].id; i++) {
        char topic[128];

        if (channel_definitions[i].scope == CHANNEL_SCOPE_GLOBAL) {
            snprintf(topic, sizeof(topic), "rt:%s", channel_definitions[i].id);
            channel_topic_list_add_ref(desired, &desired_count, topic);
        }
    }

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *ch = d->original ? d->original : d->character;
        int j;

        if (!ch || d->connected != CON_PLAYING || !ch->in_room || !ch->in_room->area)
            continue;

        for (j = 0; channel_definitions[j].id; j++) {
            char topic[128];

            if (channel_definitions[j].scope == CHANNEL_SCOPE_GLOBAL)
                continue;

            if (channel_build_topic_for_sender(&channel_definitions[j], ch, topic, sizeof(topic)))
                channel_topic_list_add_ref(desired, &desired_count, topic);
        }
    }

    for (i = 0; i < channel_active_topic_count; i++) {
        if (channel_topic_list_find(desired, desired_count, channel_active_topics[i].topic) < 0) {
            if (!channel_transport_unsubscribe(channel_active_topics[i].topic)) {
                log_stringf("ChannelService: unsubscribe failed for topic '%s'", channel_active_topics[i].topic);
            }
        }
    }

    for (i = 0; i < desired_count; i++) {
        if (channel_topic_list_find(channel_active_topics, channel_active_topic_count, desired[i].topic) < 0) {
            if (!channel_transport_subscribe(desired[i].topic)) {
                log_stringf("ChannelService: subscribe failed for topic '%s'", desired[i].topic);
            }
        }
    }

    memcpy(channel_active_topics, desired, sizeof(desired));
    channel_active_topic_count = desired_count;
}

static CHAR_DATA *channel_find_sender(unsigned long id0, unsigned long id1, const char *name)
{
    DESCRIPTOR_DATA *d;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *ch = d->original ? d->original : d->character;

        if (!ch)
            continue;

        if (id0 > 0 && ch->id[0] == id0 && ch->id[1] == id1)
            return ch;
    }

    if (!IS_NULLSTR(name))
        return get_char_world(NULL, (char *)name);

    return NULL;
}

bool channel_can_deliver_to_descriptor(CHAR_DATA *sender,
                                       DESCRIPTOR_DATA *desc,
                                       long comm_block_flag,
                                       bool honor_quiet,
                                       bool honor_ignore,
                                       CHAR_DATA **out_victim)
{
    CHAR_DATA *victim;

    if (!sender || !desc)
        return false;

    victim = desc->original ? desc->original : desc->character;

    if (desc->connected != CON_PLAYING || !victim)
        return false;

    if (victim == sender)
        return false;

    if (comm_block_flag != 0 && IS_SET(victim->comm, comm_block_flag))
        return false;

    if (honor_quiet && IS_SET(victim->comm, COMM_QUIET))
        return false;

    if (honor_ignore && is_ignoring(victim, sender))
        return false;

    if (out_victim)
        *out_victim = victim;

    return true;
}

static void channel_deliver_ooc_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NO_OOC, true, true, &victim)) {
            if (!IS_NPC(d->character) && !IS_NPC(sender) && sender->pcdata->flag &&
                SHOW_CHANNEL_FLAG(d->character, FLAG_OOC))
                sprintf(msg, "%s {G%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "{G%s", plain_text);

            act_new("{g$$n says OOC: {G$t{x", sender, d->character, NULL, NULL, NULL,
                    NULL, NULL, msg, NULL, TO_VICT, POS_SLEEPING, NULL);
        }
    }
}

static void channel_deliver_gossip_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOGOSSIP, true, true, &victim)) {
            if (!IS_NPC(d->character) && !IS_NPC(sender) && sender->pcdata->flag &&
                SHOW_CHANNEL_FLAG(d->character, FLAG_GOSSIP))
                sprintf(msg, "%s {M%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "%s", plain_text);

            act_new("{M$$n gossips '$t{M'{x", sender, d->character, NULL, NULL, NULL,
                    NULL, NULL, msg, NULL, TO_VICT, POS_SLEEPING, NULL);
        }
    }
}

static void channel_deliver_quote_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOQUOTE, true, true, &victim)) {
            if (!IS_NPC(sender) && sender->pcdata->flag != NULL &&
                !IS_NPC(victim) && SHOW_CHANNEL_FLAG(victim, FLAG_QUOTE))
                sprintf(msg, "%s {W%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "%s", plain_text);

            act_new("{x$$n quotes {D\"{W$t{D\"{x", sender, victim, NULL, NULL, NULL,
                    NULL, NULL, msg, NULL, TO_VICT, POS_SLEEPING, NULL);
        }
    }
}

static void channel_deliver_flame_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NO_FLAMING, true, true, &victim)) {
            if (!IS_NPC(d->character) && !IS_NPC(sender) && sender->pcdata->flag &&
                SHOW_CHANNEL_FLAG(d->character, FLAG_FLAMING))
                sprintf(msg, "%s {r%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "{r%s", plain_text);

            act_new("{r({WF{r): $$n flames '$t{r'{x", sender, d->character, NULL, NULL, NULL,
                    NULL, NULL, msg, NULL, TO_VICT, POS_DEAD, NULL);
        }
    }
}

static void channel_deliver_helper_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOHELPER, true, true, &victim) &&
            (IS_SET(victim->act[0], PLR_HELPER) || IS_IMMORTAL(victim)) &&
            victim != sender) {
            sprintf(msg, "{Y({BH{Y)--> %s: %s{Y'{x\n\r", sender->name, plain_text);
            send_to_char(msg, victim);
        }
    }
}

static void channel_deliver_music_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOMUSIC, true, true, &victim)) {
            if (!IS_NPC(victim) && !IS_NPC(sender) && sender->pcdata->flag &&
                SHOW_CHANNEL_FLAG(victim, FLAG_MUSIC))
                sprintf(msg, "%s {Y%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "%s", plain_text);

            act_new("{Y($$n): o/~ $t{x", sender, d->character, NULL, NULL, NULL,
                    NULL, NULL, msg, NULL, TO_VICT, POS_SLEEPING, NULL);
        }
    }
}

static void channel_deliver_immtalk_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        if (d->connected == CON_PLAYING && d->character && IS_IMMORTAL(d->character) &&
            d->character != sender && !IS_SET(d->character->comm, COMM_NOWIZ)) {
            act_new("{B[{G$$n{B]: $t{x", sender, d->character, NULL, NULL, NULL,
                    NULL, NULL, (void *)plain_text, NULL, TO_VICT, POS_DEAD, NULL);
        }
    }
}

static void channel_deliver_yell_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || !sender->in_room || !sender->in_room->area || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOYELL, true, true, &victim) &&
            victim->in_room && victim->in_room->area == sender->in_room->area &&
            victim != sender) {
            if (!IS_NPC(sender) && sender->pcdata->flag && SHOW_CHANNEL_FLAG(victim, FLAG_YELL))
                sprintf(msg, "%s {Y%s", sender->pcdata->flag, plain_text);
            else
                sprintf(msg, "%s", plain_text);

            act("{Y$n yells '$t{Y'{x", sender, d->character, NULL, NULL, NULL,
                msg, NULL, TO_VICT, NULL, NULL);
        }
    }
}

static void channel_deliver_gtell_legacy(CHAR_DATA *sender, const char *plain_text)
{
    CHAR_DATA *gch;
    GROUP_DATA *group;
    ITERATOR it;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    group = IS_VALID(sender->group) ? sender->group : NULL;

    if (IS_VALID(group) && group->members)
        iterator_start(&it, group->members);
    else
        iterator_start(&it, loaded_chars);

    while ((gch = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (gch == sender)
            continue;

        if ((IS_VALID(group) && gch->group == group) || (!IS_VALID(group) && is_same_group(gch, sender))) {
            act_new("{C$$n tells the group '$t'{x", sender, gch, NULL, NULL, NULL,
                    NULL, NULL, (void *)plain_text, NULL, TO_VICT, POS_SLEEPING, NULL);
        }
    }
    iterator_stop(&it);
}

static void channel_deliver_chtalk_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[MAX_STRING_LENGTH];

    if (!sender || !sender->church || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d != NULL; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (!channel_can_deliver_to_descriptor(sender, d, COMM_NOCT, true, true, &victim))
            continue;

        if (!victim || victim->church != sender->church)
            continue;

        if (!IS_NPC(sender) && sender->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_CT)) {
            sprintf(msg, "%s[%s%s%s] says '%s %s%s%s'{x\n\r",
                sender->church->colour2,
                sender->church->colour1,
                sender->name,
                sender->church->colour2,
                sender->pcdata->flag,
                sender->church->colour1,
                plain_text,
                sender->church->colour2);
        } else {
            sprintf(msg, "%s[%s%s%s] says '%s%s%s'{x\n\r",
                sender->church->colour2,
                sender->church->colour1,
                sender->name,
                sender->church->colour2,
                sender->church->colour1,
                plain_text,
                sender->church->colour2);
        }

        send_to_char(msg, d->character);
    }
}

static bool channel_dispatch_legacy_by_id(CHAR_DATA *sender, const char *channel_id, const char *plain_text)
{
    if (!sender || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return false;

    if (!str_cmp(channel_id, "ooc")) {
        channel_deliver_ooc_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "gossip")) {
        channel_deliver_gossip_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "quote")) {
        channel_deliver_quote_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "flame")) {
        channel_deliver_flame_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "helper")) {
        channel_deliver_helper_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "music")) {
        channel_deliver_music_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "immtalk")) {
        channel_deliver_immtalk_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "yell")) {
        channel_deliver_yell_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "gtell")) {
        channel_deliver_gtell_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "chtalk")) {
        channel_deliver_chtalk_legacy(sender, plain_text);
    } else {
        return false;
    }

    return true;
}

static void channel_service_receive_message(const CHANNEL_MESSAGE *msg)
{
    CHAR_DATA *sender;

    if (!msg || IS_NULLSTR(msg->channel_id))
        return;

    if (IS_NULLSTR(msg->message_text))
        return;

    sender = channel_find_sender(msg->sender_id0, msg->sender_id1, msg->sender_name);
    if (!sender)
        return;

    channel_dispatch_legacy_by_id(sender, msg->channel_id, msg->message_text);
}

bool channel_service_init(void)
{
    if (channel_service_ready)
        return true;

    if (!channel_transport_init()) {
        log_string("ChannelService: failed to initialize transport");
        return false;
    }

    channel_transport_set_inbound_handler(channel_service_receive_message);

    channel_subscription_last_sync = 0;
    channel_active_topic_count = 0;
    memset(channel_active_topics, 0, sizeof(channel_active_topics));
    channel_service_sync_subscriptions();

    channel_service_ready = true;
    log_stringf("ChannelService: initialized with backend '%s'", channel_transport_backend_name());
    return true;
}

void channel_service_shutdown(void)
{
    int i;

    if (!channel_service_ready)
        return;

    for (i = 0; i < channel_active_topic_count; i++) {
        channel_transport_unsubscribe(channel_active_topics[i].topic);
    }
    channel_active_topic_count = 0;
    memset(channel_active_topics, 0, sizeof(channel_active_topics));

    channel_transport_shutdown();
    channel_transport_set_inbound_handler(NULL);
    channel_service_ready = false;
    log_string("ChannelService: shutdown");
}

void channel_service_pulse(void)
{
    if (!channel_service_ready)
        return;

    if (current_time > channel_subscription_last_sync) {
        channel_service_sync_subscriptions();
        channel_subscription_last_sync = current_time;
    }

    channel_transport_pulse();
}

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text)
{
    const CHANNEL_DEFINITION *def;
    CHANNEL_MESSAGE msg;
    CHANNEL_FILTER_RESULT filter_result;
    const char *delivery_text;
    char topic[128];
    char sender_uid[64];
    char review_id[64];

    if (!sender || IS_NULLSTR(channel_id) || IS_NULLSTR(raw_text))
        return false;

    def = channel_find_definition(channel_id);
    if (!def)
        return false;

    if (!channel_filter_evaluate(sender, channel_id, raw_text, &filter_result))
        return false;

    delivery_text = raw_text;
    if (filter_result.decision == CHANNEL_FILTER_REDACT)
        delivery_text = filter_result.filtered_text;

    memset(&msg, 0, sizeof(msg));
    msg.channel_id = channel_id;
    msg.sender_name = sender->name;
    snprintf(sender_uid, sizeof(sender_uid), "%lu:%lu", sender->id[0], sender->id[1]);
    msg.sender_uid = sender_uid;
    msg.sender_id0 = sender->id[0];
    msg.sender_id1 = sender->id[1];
    msg.history_stream = NULL;
    msg.history_id = NULL;
    msg.message_text = delivery_text;
    msg.timestamp = current_time;

    if (filter_result.queue_for_review) {
        if (!channel_review_queue_append(&msg,
                                         filter_result.decision,
                                         filter_result.reason,
                                         raw_text,
                                         delivery_text,
                                         review_id,
                                         sizeof(review_id))) {
            log_stringf("ChannelService: review queue append failed for channel '%s'", channel_id);
        }
    }

    if (filter_result.decision == CHANNEL_FILTER_BLOCK) {
        send_to_char("Your message was blocked by channel filters.\n\r", sender);
        return true;
    }

    if (!channel_service_ready)
        return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);

    if (!channel_build_topic_for_sender(def, sender, topic, sizeof(topic)))
        return false;

    msg.topic = topic;

    if (channel_transport_backend_mode() != CHANNEL_BACKEND_LEGACY_ITERATIVE) {
        if (channel_transport_publish(topic, &msg))
            return true;

        log_stringf("ChannelService: publish failed for channel '%s', applying local legacy fallback", channel_id);
        return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);
    }

    return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);
}

const char *channel_service_backend_name(void)
{
    return channel_transport_backend_name();
}

int channel_service_describe_subscriptions(CHAR_DATA *ch, char *out, size_t out_size)
{
    CHANNEL_SUBSCRIPTION_TOPIC topics[CHANNEL_SUBSCRIPTION_MAX_TOPICS];
    int topic_count = 0;
    int i;

    if (!out || out_size == 0)
        return 0;

    out[0] = '\0';

    if (!ch)
        return 0;

    memset(topics, 0, sizeof(topics));

    for (i = 0; channel_definitions[i].id; i++) {
        char topic[128];

        if (channel_build_topic_for_sender(&channel_definitions[i], ch, topic, sizeof(topic)))
            channel_topic_list_add_ref(topics, &topic_count, topic);
    }

    for (i = 0; i < topic_count; i++) {
        if (i > 0)
            strlcat(out, ", ", out_size);
        strlcat(out, topics[i].topic, out_size);
    }

    return topic_count;
}
