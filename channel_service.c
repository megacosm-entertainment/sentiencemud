#include <string.h>
#include "merc.h"
#include "channel_service.h"

static bool channel_service_ready = false;

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

static void channel_deliver_ooc_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    char msg[2 * MSL];

    if (!sender || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = d->original ? d->original : d->character;

        if (d->connected == CON_PLAYING && d->character != sender &&
            !IS_SET(victim->comm, COMM_NO_OOC) &&
            !IS_SET(victim->comm, COMM_QUIET) &&
            !is_ignoring(d->character, sender)) {
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
        CHAR_DATA *victim = d->original ? d->original : d->character;

        if (d->connected == CON_PLAYING && d->character && d->character != sender &&
            !IS_SET(victim->comm, COMM_NOGOSSIP) &&
            !IS_SET(victim->comm, COMM_QUIET) &&
            !is_ignoring(d->character, sender)) {
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

static void channel_service_receive_message(const CHANNEL_MESSAGE *msg)
{
    CHAR_DATA *sender;

    if (!msg || IS_NULLSTR(msg->channel_id) || IS_NULLSTR(msg->message_text))
        return;

    sender = channel_find_sender(msg->sender_id0, msg->sender_id1, msg->sender_name);
    if (!sender)
        return;

    if (!str_cmp(msg->channel_id, "ooc"))
        channel_deliver_ooc_legacy(sender, msg->message_text);
    else if (!str_cmp(msg->channel_id, "gossip"))
        channel_deliver_gossip_legacy(sender, msg->message_text);
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

    channel_service_ready = true;
    log_stringf("ChannelService: initialized with backend '%s'", channel_transport_backend_name());
    return true;
}

void channel_service_shutdown(void)
{
    if (!channel_service_ready)
        return;

    channel_transport_shutdown();
    channel_transport_set_inbound_handler(NULL);
    channel_service_ready = false;
    log_string("ChannelService: shutdown");
}

void channel_service_pulse(void)
{
    if (!channel_service_ready)
        return;

    channel_transport_pulse();
}

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text)
{
    CHANNEL_MESSAGE msg;
    const char *topic = "rt:stub";

    if (!channel_service_ready || !sender || IS_NULLSTR(channel_id) || IS_NULLSTR(raw_text))
        return false;

    msg.channel_id = channel_id;
    msg.topic = topic;
    msg.sender_name = sender->name;
    msg.sender_id0 = sender->id[0];
    msg.sender_id1 = sender->id[1];
    msg.message_text = raw_text;
    msg.timestamp = current_time;

    if (channel_transport_backend_mode() != CHANNEL_BACKEND_LEGACY_ITERATIVE)
        return channel_transport_publish(topic, &msg);

    if (!str_cmp(channel_id, "ooc")) {
        channel_deliver_ooc_legacy(sender, raw_text);
    } else if (!str_cmp(channel_id, "gossip")) {
        channel_deliver_gossip_legacy(sender, raw_text);
    }

    return true;
}

const char *channel_service_backend_name(void)
{
    return channel_transport_backend_name();
}
