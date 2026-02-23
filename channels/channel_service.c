#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <jansson.h>
#include "../merc.h"
#include "../recycle.h"
#include "channel_service.h"
#include "channel_registry.h"
#include "channel_policy.h"
#include "channel_filter.h"
#include "channel_review.h"
#include "channels_common.h"
#include "../account/penalty.h"
#include "../account/preferences.h"
#include "../mxp_links.h"
#include "../requirements.h"

/* BUFFER helper prototypes (implemented in mem.c). */
BUFFER *new_buf(void);
void free_buf(BUFFER *buffer);
char *buf_string(BUFFER *buffer);

static bool channel_service_ready = false;
static time_t channel_subscription_last_sync = 0;
static time_t channel_registry_last_mtime = 0;

#define CHANNEL_REGISTRY_PATH "data/system/channels.json"
#define CHANNEL_STAFF_REPORTS_PATH "data/system/channel_staff_reports.json"
#define CHANNEL_HISTORY_PATH "data/system/channel_history_ring.json"
#define CHANNEL_PERSIST_FLUSH_INTERVAL 30

#define CHANNEL_SUBSCRIPTION_MAX_TOPICS 512

typedef struct channel_subscription_topic {
    char topic[128];
    int refs;
} CHANNEL_SUBSCRIPTION_TOPIC;

static CHANNEL_SUBSCRIPTION_TOPIC channel_active_topics[CHANNEL_SUBSCRIPTION_MAX_TOPICS];
static int channel_active_topic_count = 0;

#define CHANNEL_HISTORY_RING_MAX 2048

typedef struct channel_history_record {
    char report_id[64];
    char reports_json[256];
    char channel_id[32];
    char topic[128];
    char sender_name[64];
    char message_text[1024];
    time_t timestamp;
} CHANNEL_HISTORY_RECORD;

static CHANNEL_HISTORY_RECORD channel_history_ring[CHANNEL_HISTORY_RING_MAX];
static int channel_history_ring_next = 0;
static int channel_history_ring_count = 0;
static unsigned long long channel_history_local_seq = 0ULL;
static bool channel_history_dirty = false;

#define CHANNEL_STAFF_REPORT_RING_MAX 2048

typedef struct channel_staff_report_record {
    char report_id[64];
    char queue_name[128];
    char channel_id[32];
    char reason[64];
    char reporter_name[64];
    char detail_text[2048];
    char ack_by[64];
    time_t timestamp;
    bool acknowledged;
} CHANNEL_STAFF_REPORT_RECORD;

static CHANNEL_STAFF_REPORT_RECORD channel_staff_report_ring[CHANNEL_STAFF_REPORT_RING_MAX];
static int channel_staff_report_ring_next = 0;
static int channel_staff_report_ring_count = 0;
static bool channel_staff_report_dirty = false;
static time_t channel_persist_last_flush = 0;

static bool channel_history_save(void);
static void channel_history_load(void);
static bool channel_staff_report_save(void);
static void channel_staff_report_load(void);
static void channel_service_mark_history_dirty(void);
static void channel_service_mark_staff_report_dirty(void);
static void channel_service_flush_persistence(bool force);

static void channel_history_reset(void)
{
    memset(channel_history_ring, 0, sizeof(channel_history_ring));
    channel_history_ring_next = 0;
    channel_history_ring_count = 0;
    channel_history_local_seq = 0ULL;
    channel_history_dirty = false;
}

static void channel_staff_report_reset(void)
{
    memset(channel_staff_report_ring, 0, sizeof(channel_staff_report_ring));
    channel_staff_report_ring_next = 0;
    channel_staff_report_ring_count = 0;
    channel_staff_report_dirty = false;
}

static void channel_service_mark_history_dirty(void)
{
    channel_history_dirty = true;
}

static void channel_service_mark_staff_report_dirty(void)
{
    channel_staff_report_dirty = true;
}

static void channel_staff_report_copy_entry(CHANNEL_STAFF_REPORT_ENTRY *dst,
                                            const CHANNEL_STAFF_REPORT_RECORD *src)
{
    if (!dst || !src)
        return;

    strlcpy(dst->report_id, src->report_id, sizeof(dst->report_id));
    strlcpy(dst->queue_name, src->queue_name, sizeof(dst->queue_name));
    strlcpy(dst->channel_id, src->channel_id, sizeof(dst->channel_id));
    strlcpy(dst->reason, src->reason, sizeof(dst->reason));
    strlcpy(dst->reporter_name, src->reporter_name, sizeof(dst->reporter_name));
    strlcpy(dst->detail_text, src->detail_text, sizeof(dst->detail_text));
    dst->timestamp = src->timestamp;
}

static CHANNEL_STAFF_REPORT_RECORD *channel_staff_report_find(const char *report_id)
{
    int i;

    if (IS_NULLSTR(report_id))
        return NULL;

    for (i = 0; i < channel_staff_report_ring_count; i++) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];

        if (!str_cmp(record->report_id, report_id))
            return record;
    }

    return NULL;
}

static void channel_staff_report_append(const char *report_id,
                                        const char *queue_name,
                                        const char *channel_id,
                                        const char *reason,
                                        const char *reporter_name,
                                        const char *detail_text)
{
    CHANNEL_STAFF_REPORT_RECORD *record;

    if (IS_NULLSTR(report_id))
        return;

    record = &channel_staff_report_ring[channel_staff_report_ring_next];
    memset(record, 0, sizeof(*record));

    strlcpy(record->report_id, report_id, sizeof(record->report_id));
    strlcpy(record->queue_name,
            IS_NULLSTR(queue_name) ? CHANNEL_REVIEW_STREAM : queue_name,
            sizeof(record->queue_name));
    strlcpy(record->channel_id,
            IS_NULLSTR(channel_id) ? "unknown" : channel_id,
            sizeof(record->channel_id));
    strlcpy(record->reason,
            IS_NULLSTR(reason) ? "none" : reason,
            sizeof(record->reason));
    strlcpy(record->reporter_name,
            IS_NULLSTR(reporter_name) ? "(unknown)" : reporter_name,
            sizeof(record->reporter_name));
        strlcpy(record->detail_text,
            IS_NULLSTR(detail_text) ? "" : detail_text,
            sizeof(record->detail_text));
    record->timestamp = current_time;
    record->acknowledged = false;

    channel_staff_report_ring_next =
        (channel_staff_report_ring_next + 1) % CHANNEL_STAFF_REPORT_RING_MAX;
    if (channel_staff_report_ring_count < CHANNEL_STAFF_REPORT_RING_MAX)
        channel_staff_report_ring_count++;

    channel_service_mark_staff_report_dirty();
}

static bool channel_history_save(void)
{
    json_t *root;
    json_t *items;
    int i;

    root = json_object();
    items = json_array();

    if (!root || !items) {
        if (items)
            json_decref(items);
        if (root)
            json_decref(root);
        return false;
    }

    for (i = channel_history_ring_count - 1; i >= 0; i--) {
        int ring_index = (channel_history_ring_next - 1 - i + CHANNEL_HISTORY_RING_MAX)
            % CHANNEL_HISTORY_RING_MAX;
        CHANNEL_HISTORY_RECORD *record = &channel_history_ring[ring_index];
        json_t *entry = json_object();

        if (!entry)
            continue;

        json_object_set_new(entry, "report_id", json_string(record->report_id));
        json_object_set_new(entry, "reports_json", json_string(record->reports_json));
        json_object_set_new(entry, "channel_id", json_string(record->channel_id));
        json_object_set_new(entry, "topic", json_string(record->topic));
        json_object_set_new(entry, "sender_name", json_string(record->sender_name));
        json_object_set_new(entry, "message_text", json_string(record->message_text));
        json_object_set_new(entry, "timestamp", json_integer((json_int_t)record->timestamp));

        json_array_append_new(items, entry);
    }

    json_object_set_new(root, "version", json_integer(1));
    json_object_set_new(root, "local_seq", json_integer((json_int_t)channel_history_local_seq));
    json_object_set_new(root, "items", items);

    if (json_dump_file(root, CHANNEL_HISTORY_PATH, JSON_INDENT(2)) != 0) {
        log_stringf("ChannelService: unable to persist channel history ring to %s", CHANNEL_HISTORY_PATH);
        json_decref(root);
        return false;
    }

    json_decref(root);
    channel_history_dirty = false;
    return true;
}

static void channel_history_load(void)
{
    json_error_t err;
    json_t *root;
    json_t *items;
    json_t *seq;
    size_t index;
    json_t *value;

    root = json_load_file(CHANNEL_HISTORY_PATH, 0, &err);
    if (!root)
        return;

    channel_history_reset();

    seq = json_object_get(root, "local_seq");
    if (seq && json_is_integer(seq)) {
        json_int_t loaded_seq = json_integer_value(seq);
        if (loaded_seq > 0)
            channel_history_local_seq = (unsigned long long)loaded_seq;
    }

    items = json_object_get(root, "items");
    if (!json_is_array(items)) {
        json_decref(root);
        return;
    }

    json_array_foreach(items, index, value) {
        CHANNEL_HISTORY_RECORD *record;
        const char *report_id;
        const char *reports_json;
        const char *channel_id;
        const char *topic;
        const char *sender_name;
        const char *message_text;
        json_t *timestamp_json;

        if (channel_history_ring_count >= CHANNEL_HISTORY_RING_MAX)
            break;

        if (!json_is_object(value))
            continue;

        channel_id = json_string_value(json_object_get(value, "channel_id"));
        message_text = json_string_value(json_object_get(value, "message_text"));
        if (IS_NULLSTR(channel_id) || IS_NULLSTR(message_text))
            continue;

        record = &channel_history_ring[channel_history_ring_next];
        memset(record, 0, sizeof(*record));

        report_id = json_string_value(json_object_get(value, "report_id"));
        reports_json = json_string_value(json_object_get(value, "reports_json"));
        topic = json_string_value(json_object_get(value, "topic"));
        sender_name = json_string_value(json_object_get(value, "sender_name"));
        timestamp_json = json_object_get(value, "timestamp");

        strlcpy(record->report_id,
                IS_NULLSTR(report_id) ? "" : report_id,
                sizeof(record->report_id));
        strlcpy(record->reports_json,
                IS_NULLSTR(reports_json) ? "" : reports_json,
                sizeof(record->reports_json));
        strlcpy(record->channel_id, channel_id, sizeof(record->channel_id));
        strlcpy(record->topic,
                IS_NULLSTR(topic) ? "" : topic,
                sizeof(record->topic));
        strlcpy(record->sender_name,
                IS_NULLSTR(sender_name) ? "(unknown)" : sender_name,
                sizeof(record->sender_name));
        strlcpy(record->message_text, message_text, sizeof(record->message_text));

        if (timestamp_json && json_is_integer(timestamp_json)) {
            json_int_t ts = json_integer_value(timestamp_json);
            record->timestamp = (ts > 0) ? (time_t)ts : current_time;
        } else {
            record->timestamp = current_time;
        }

        if (IS_NULLSTR(record->report_id)) {
            channel_history_local_seq++;
            snprintf(record->report_id,
                     sizeof(record->report_id),
                     "local-%llu",
                     channel_history_local_seq);
        }

        channel_history_ring_next = (channel_history_ring_next + 1) % CHANNEL_HISTORY_RING_MAX;
        channel_history_ring_count++;
    }

    channel_history_dirty = false;
    json_decref(root);
}

static bool channel_staff_report_save(void)
{
    json_t *root;
    json_t *items;
    int i;

    root = json_object();
    items = json_array();

    if (!root || !items) {
        if (items)
            json_decref(items);
        if (root)
            json_decref(root);
        return false;
    }

    for (i = channel_staff_report_ring_count - 1; i >= 0; i--) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];
        json_t *entry = json_object();

        if (!entry)
            continue;

        json_object_set_new(entry, "report_id", json_string(record->report_id));
        json_object_set_new(entry, "queue_name", json_string(record->queue_name));
        json_object_set_new(entry, "channel_id", json_string(record->channel_id));
        json_object_set_new(entry, "reason", json_string(record->reason));
        json_object_set_new(entry, "reporter_name", json_string(record->reporter_name));
        json_object_set_new(entry, "detail_text", json_string(record->detail_text));
        json_object_set_new(entry, "ack_by", json_string(record->ack_by));
        json_object_set_new(entry, "timestamp", json_integer((json_int_t)record->timestamp));
        json_object_set_new(entry, "acknowledged", json_boolean(record->acknowledged));

        json_array_append_new(items, entry);
    }

    json_object_set_new(root, "version", json_integer(1));
    json_object_set_new(root, "items", items);

    if (json_dump_file(root, CHANNEL_STAFF_REPORTS_PATH, JSON_INDENT(2)) != 0) {
        log_stringf("ChannelService: unable to persist staff report queue to %s", CHANNEL_STAFF_REPORTS_PATH);
        json_decref(root);
        return false;
    }

    json_decref(root);
    channel_staff_report_dirty = false;
    return true;
}

static void channel_staff_report_load(void)
{
    json_error_t err;
    json_t *root;
    json_t *items;
    size_t index;
    json_t *value;

    root = json_load_file(CHANNEL_STAFF_REPORTS_PATH, 0, &err);
    if (!root)
        return;

    channel_staff_report_reset();

    items = json_object_get(root, "items");
    if (!json_is_array(items)) {
        json_decref(root);
        return;
    }

    json_array_foreach(items, index, value) {
        CHANNEL_STAFF_REPORT_RECORD *record;
        const char *report_id;
        const char *queue_name;
        const char *channel_id;
        const char *reason;
        const char *reporter_name;
        const char *detail_text;
        const char *ack_by;
        json_t *timestamp_json;
        json_t *ack_json;

        if (channel_staff_report_ring_count >= CHANNEL_STAFF_REPORT_RING_MAX)
            break;

        if (!json_is_object(value))
            continue;

        report_id = json_string_value(json_object_get(value, "report_id"));
        if (IS_NULLSTR(report_id))
            continue;

        record = &channel_staff_report_ring[channel_staff_report_ring_next];
        memset(record, 0, sizeof(*record));

        queue_name = json_string_value(json_object_get(value, "queue_name"));
        channel_id = json_string_value(json_object_get(value, "channel_id"));
        reason = json_string_value(json_object_get(value, "reason"));
        reporter_name = json_string_value(json_object_get(value, "reporter_name"));
        detail_text = json_string_value(json_object_get(value, "detail_text"));
        ack_by = json_string_value(json_object_get(value, "ack_by"));
        timestamp_json = json_object_get(value, "timestamp");
        ack_json = json_object_get(value, "acknowledged");

        strlcpy(record->report_id, report_id, sizeof(record->report_id));
        strlcpy(record->queue_name,
                IS_NULLSTR(queue_name) ? CHANNEL_REVIEW_STREAM : queue_name,
                sizeof(record->queue_name));
        strlcpy(record->channel_id,
                IS_NULLSTR(channel_id) ? "unknown" : channel_id,
                sizeof(record->channel_id));
        strlcpy(record->reason,
                IS_NULLSTR(reason) ? "none" : reason,
                sizeof(record->reason));
        strlcpy(record->reporter_name,
                IS_NULLSTR(reporter_name) ? "(unknown)" : reporter_name,
                sizeof(record->reporter_name));
        strlcpy(record->detail_text,
            IS_NULLSTR(detail_text) ? "" : detail_text,
            sizeof(record->detail_text));
        strlcpy(record->ack_by,
                IS_NULLSTR(ack_by) ? "" : ack_by,
                sizeof(record->ack_by));

        if (timestamp_json && json_is_integer(timestamp_json))
            record->timestamp = (time_t)json_integer_value(timestamp_json);
        else
            record->timestamp = current_time;

        record->acknowledged = (ack_json && json_is_boolean(ack_json) && json_is_true(ack_json));

        channel_staff_report_ring_next =
            (channel_staff_report_ring_next + 1) % CHANNEL_STAFF_REPORT_RING_MAX;
        channel_staff_report_ring_count++;
    }

    json_decref(root);
    channel_staff_report_dirty = false;
}

static void channel_service_flush_persistence(bool force)
{
    if (force || channel_history_dirty)
        channel_history_save();

    if (force || channel_staff_report_dirty)
        channel_staff_report_save();

    channel_persist_last_flush = current_time;
}

static time_t channel_registry_file_mtime(void)
{
    struct stat st;

    if (stat(CHANNEL_REGISTRY_PATH, &st) != 0)
        return 0;

    return st.st_mtime;
}

static void channel_registry_mark_loaded_mtime(void)
{
    channel_registry_last_mtime = channel_registry_file_mtime();
}

static void channel_registry_reload_if_changed(void)
{
    time_t current_mtime;

    current_mtime = channel_registry_file_mtime();

    if (current_mtime == 0 || current_mtime == channel_registry_last_mtime)
        return;

    if (channel_registry_load(CHANNEL_REGISTRY_PATH)) {
        channel_registry_last_mtime = current_mtime;
        log_string("ChannelService: hot-reloaded channel registry from disk");
    } else {
        log_string("ChannelService: channel registry reload failed; keeping previous definitions");
    }
}

static void channel_history_copy_entry(CHANNEL_HISTORY_ENTRY *dst,
                                       const CHANNEL_HISTORY_RECORD *src)
{
    if (!dst || !src)
        return;

    strlcpy(dst->report_id, src->report_id, sizeof(dst->report_id));
    strlcpy(dst->reports_json, src->reports_json, sizeof(dst->reports_json));
    strlcpy(dst->channel_id, src->channel_id, sizeof(dst->channel_id));
    strlcpy(dst->sender_name, src->sender_name, sizeof(dst->sender_name));
    strlcpy(dst->message_text, src->message_text, sizeof(dst->message_text));
    dst->timestamp = src->timestamp;
}

static void channel_history_append(const char *channel_id,
                                   const char *topic,
                                   const char *sender_name,
                                   const char *message_text,
                                   time_t timestamp,
                                   const char *report_id,
                                   const char *reports_json)
{
    CHANNEL_HISTORY_RECORD *record;

    if (IS_NULLSTR(channel_id) || IS_NULLSTR(message_text))
        return;

    record = &channel_history_ring[channel_history_ring_next];

    memset(record, 0, sizeof(*record));
    strlcpy(record->channel_id, channel_id, sizeof(record->channel_id));
    if (!IS_NULLSTR(topic))
        strlcpy(record->topic, topic, sizeof(record->topic));
    strlcpy(record->sender_name,
            IS_NULLSTR(sender_name) ? "(unknown)" : sender_name,
            sizeof(record->sender_name));
    strlcpy(record->message_text, message_text, sizeof(record->message_text));
    record->timestamp = (timestamp > 0) ? timestamp : current_time;

    if (!IS_NULLSTR(report_id)) {
        strlcpy(record->report_id, report_id, sizeof(record->report_id));
    } else {
        channel_history_local_seq++;
        snprintf(record->report_id,
                 sizeof(record->report_id),
                 "local-%llu",
                 channel_history_local_seq);
    }

    if (!IS_NULLSTR(reports_json))
        strlcpy(record->reports_json, reports_json, sizeof(record->reports_json));

    channel_history_ring_next = (channel_history_ring_next + 1) % CHANNEL_HISTORY_RING_MAX;
    if (channel_history_ring_count < CHANNEL_HISTORY_RING_MAX)
        channel_history_ring_count++;

    channel_service_mark_history_dirty();
}

static bool channel_history_match_channel(const CHANNEL_HISTORY_RECORD *record,
                                          const char *channel_id)
{
    if (!record || IS_NULLSTR(channel_id))
        return false;

    return !str_cmp(record->channel_id, channel_id);
}

static bool channel_history_find_record_by_report_id(const char *channel_id,
                                                     const char *report_id,
                                                     int *out_ring_index)
{
    int i;

    if (IS_NULLSTR(channel_id) || IS_NULLSTR(report_id))
        return false;

    for (i = 0; i < channel_history_ring_count; i++) {
        int ring_index = (channel_history_ring_next - 1 - i + CHANNEL_HISTORY_RING_MAX) % CHANNEL_HISTORY_RING_MAX;
        CHANNEL_HISTORY_RECORD *record = &channel_history_ring[ring_index];

        if (!channel_history_match_channel(record, channel_id))
            continue;

        if (!str_cmp(record->report_id, report_id)) {
            if (out_ring_index)
                *out_ring_index = ring_index;
            return true;
        }
    }

    return false;
}

static void channel_history_append_report_meta(CHANNEL_HISTORY_RECORD *record,
                                               const char *report_id,
                                               const char *source,
                                               const char *reporter)
{
    json_error_t err;
    json_t *root = NULL;
    json_t *items = NULL;
    json_t *entry = NULL;
    char *json_text;

    if (!record || IS_NULLSTR(report_id))
        return;

    if (!IS_NULLSTR(record->reports_json))
        root = json_loads(record->reports_json, 0, &err);

    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        root = json_object();
    }

    items = json_object_get(root, "items");
    if (!items || !json_is_array(items)) {
        items = json_array();
        json_object_set_new(root, "items", items);
    }

    entry = json_object();
    json_object_set_new(entry, "id", json_string(report_id));
    json_object_set_new(entry, "source", json_string(IS_NULLSTR(source) ? "unknown" : source));
    if (!IS_NULLSTR(reporter))
        json_object_set_new(entry, "reporter", json_string(reporter));

    json_array_append_new(items, entry);
    json_object_set_new(root, "count", json_integer((json_int_t)json_array_size(items)));

    json_text = json_dumps(root, JSON_COMPACT);
    if (json_text) {
        strlcpy(record->reports_json, json_text, sizeof(record->reports_json));
        free(json_text);
        channel_service_mark_history_dirty();
    }

    json_decref(root);
}

/* Channel definitions are now owned by channel_registry.c and loaded from
 * data/system/channels.json.  channel_registry_init() is called during
 * channel_service_init() to ensure defaults are available before load. */

static bool channel_id_in_list(LLIST *ids, const char *channel_id)
{
    ITERATOR it;
    char *id;

    if (!ids || IS_NULLSTR(channel_id))
        return false;

    iterator_start(&it, ids);
    while ((id = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(id, channel_id)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

static bool channel_room_is_area_isolated(ROOM_INDEX_DATA *room)
{
    INSTANCE *instance;
    DUNGEON *dungeon;

    if (!room)
        return false;

    instance = get_room_instance(room);
    if (instance) {
        if (IS_SET(instance->flags, INSTANCE_ISOLATED))
            return true;
        if (instance->blueprint && IS_SET(instance->blueprint->flags, INSTANCE_ISOLATED))
            return true;
    }

    dungeon = get_room_dungeon(room);
    if (dungeon) {
        if (IS_SET(dungeon->flags, DUNGEON_ISOLATED))
            return true;
        if (dungeon->index && IS_SET(dungeon->index->flags, DUNGEON_ISOLATED))
            return true;
    }

    return false;
}

static bool channel_definition_available_for_sender(const CHANNEL_DEF_DATA *def, CHAR_DATA *sender)
{
    INSTANCE *instance;
    DUNGEON *dungeon;
    REQUIREMENT_CONTEXT req_context;

    if (!def || !sender)
        return false;

    memset(&req_context, 0, sizeof(req_context));
    req_context.actor = sender;
    if (!requirements_evaluate_text(def->publish_requirements, &req_context, true))
        return false;

    switch (def->scope) {
    case CHANNEL_SCOPE_INSTANCE_ID:
        if (!sender->in_room)
            return false;

        instance = get_room_instance(sender->in_room);
        if (!instance)
            return false;

        if (!instance->blueprint)
            return true;

        if (!instance->blueprint->channel_defs || list_size(instance->blueprint->channel_defs) == 0)
            return true;

        return channel_id_in_list(instance->blueprint->channel_defs, def->id);

    case CHANNEL_SCOPE_DUNGEON_ID:
        if (!sender->in_room)
            return false;

        dungeon = get_room_dungeon(sender->in_room);
        if (!dungeon)
            return false;

        if (!dungeon->index)
            return true;

        if (!dungeon->index->channel_defs || list_size(dungeon->index->channel_defs) == 0)
            return true;

        return channel_id_in_list(dungeon->index->channel_defs, def->id);

    default:
        return true;
    }
}

static bool channel_definition_available_for_recipient(const CHANNEL_DEF_DATA *def,
                                                       CHAR_DATA *recipient)
{
    REQUIREMENT_CONTEXT req_context;

    if (!def || !recipient)
        return false;

    memset(&req_context, 0, sizeof(req_context));
    req_context.actor = recipient;
    return requirements_evaluate_text(def->subscribe_requirements, &req_context, true);
}

static const CHANNEL_DEF_DATA *channel_find_definition(const char *channel_id)
{
    return channel_registry_find(channel_id);
}

static bool channel_has_modifier(const CHANNEL_DEF_DATA *def, long modifier)
{
    return def && ((def->modifiers & modifier) != 0);
}

static void channel_modifier_apply_emote_strip(char *text)
{
    char *cursor;

    if (IS_NULLSTR(text))
        return;

    cursor = text;
    while (*cursor == ' ')
        cursor++;

    while (*cursor == ':' || *cursor == ';' || *cursor == ',') {
        cursor++;
        while (*cursor == ' ')
            cursor++;
    }

    if (cursor != text)
        memmove(text, cursor, strlen(cursor) + 1);
}

static void channel_modifier_apply_color_strip(char *text)
{
    char stripped[MSL];

    if (IS_NULLSTR(text))
        return;

    stripped[0] = '\0';
    STRIP_COLOUR(text, stripped);
    strlcpy(text, stripped, MSL);
}

static void channel_modifier_apply_caps_normalize(char *text)
{
    int alpha_count = 0;
    int upper_count = 0;
    int index;

    if (IS_NULLSTR(text))
        return;

    for (index = 0; text[index] != '\0'; index++) {
        if (isalpha((unsigned char)text[index])) {
            alpha_count++;
            if (isupper((unsigned char)text[index]))
                upper_count++;
        }
    }

    if (alpha_count < 8)
        return;

    if ((upper_count * 100) < (alpha_count * 70))
        return;

    for (index = 0; text[index] != '\0'; index++)
        text[index] = (char)tolower((unsigned char)text[index]);

    for (index = 0; text[index] != '\0'; index++) {
        if (isalpha((unsigned char)text[index])) {
            text[index] = (char)toupper((unsigned char)text[index]);
            break;
        }
    }
}

static void channel_modifier_apply_drunk_speech(CHAR_DATA *sender, char *text)
{
    char *drunk_text;

    if (!sender || IS_NPC(sender) || !sender->pcdata || sender->pcdata->condition[COND_DRUNK] <= 10)
        return;

    drunk_text = makedrunk(text, sender);
    if (drunk_text && drunk_text != text)
        strlcpy(text, drunk_text, MSL);
}

static void channel_modifier_apply_one(CHAR_DATA *sender, char *text, long modifier)
{
    switch (modifier) {
    case CHANNEL_MOD_EMOTE_STRIP:
        channel_modifier_apply_emote_strip(text);
        break;
    case CHANNEL_MOD_COLOR_STRIP:
        channel_modifier_apply_color_strip(text);
        break;
    case CHANNEL_MOD_CAPS_NORMALIZE:
        channel_modifier_apply_caps_normalize(text);
        break;
    case CHANNEL_MOD_DRUNK_SPEECH:
        channel_modifier_apply_drunk_speech(sender, text);
        break;
    default:
        break;
    }
}

static void channel_modifier_apply_ordered(CHAR_DATA *sender,
                                           const CHANNEL_DEF_DATA *def,
                                           char *text,
                                           long text_modifier_mask)
{
    char order_copy[128];
    char *token;
    const long *fallback_order;
    size_t fallback_count = 0;
    long applied_mask = 0;
    int index;

    if (IS_NULLSTR(text) || !def || text_modifier_mask == 0)
        return;

    if (!IS_NULLSTR(def->modifier_order)) {
        strlcpy(order_copy, def->modifier_order, sizeof(order_copy));
        token = strtok(order_copy, ", ");
        while (token) {
            long modifier = channel_modifier_flag_from_name(token);
            if (modifier != 0 && (text_modifier_mask & modifier) != 0 && (applied_mask & modifier) == 0) {
                channel_modifier_apply_one(sender, text, modifier);
                applied_mask |= modifier;
            }
            token = strtok(NULL, ", ");
        }
    }

    fallback_order = channel_text_modifier_fallback_order(&fallback_count);
    for (index = 0; index < (int)fallback_count; index++) {
        long modifier = fallback_order[index];
        if ((text_modifier_mask & modifier) != 0 && (applied_mask & modifier) == 0) {
            channel_modifier_apply_one(sender, text, modifier);
            applied_mask |= modifier;
        }
    }
}

static const char *channel_apply_text_modifiers(CHAR_DATA *sender,
                                                const CHANNEL_DEF_DATA *def,
                                                const char *raw_text,
                                                char *out,
                                                size_t out_sz)
{
    long text_modifier_mask;

    if (IS_NULLSTR(raw_text) || !out || out_sz == 0)
        return raw_text;

    strlcpy(out, raw_text, out_sz);

    if (!def || def->modifiers == 0)
        return raw_text;

    text_modifier_mask = def->modifiers & channel_text_modifier_mask();

    if (text_modifier_mask == 0)
        return raw_text;

    channel_modifier_apply_ordered(sender, def, out, text_modifier_mask);

    return out;
}

static bool channel_build_topic_for_sender(const CHANNEL_DEF_DATA *def,
                                           CHAR_DATA *sender,
                                           char *topic_buf,
                                           size_t topic_buf_sz)
{
    char expanded[128];

    if (!def || !sender || !topic_buf || topic_buf_sz == 0)
        return false;

    if (!channel_definition_available_for_sender(def, sender))
        return false;

    if (!IS_NULLSTR(def->topic_pattern) &&
        channel_topic_expand_pattern(def->topic_pattern, def, sender, expanded, sizeof(expanded))) {
        if (!str_prefix("rt:", expanded))
            strlcpy(topic_buf, expanded, topic_buf_sz);
        else
            snprintf(topic_buf, topic_buf_sz, "rt:%s", expanded);
        return true;
    }

    switch (def->scope) {
    case CHANNEL_SCOPE_GLOBAL:
        snprintf(topic_buf, topic_buf_sz, "rt:%s", def->id);
        return true;

    case CHANNEL_SCOPE_AREA:
        if (!sender->in_room || !sender->in_room->area)
            return false;
        if (channel_room_is_area_isolated(sender->in_room))
            return false;
        return channel_build_area_scope_topic(sender->in_room->area, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_REGION:
        if (!sender->in_room || !sender->in_room->area)
            return false;
        return channel_build_region_scope_topic(get_room_region(sender->in_room),
                                               sender->in_room->area,
                                               topic_buf,
                                               topic_buf_sz);

    case CHANNEL_SCOPE_ROOM_WV:
        if (!sender->in_room)
            return false;
        return channel_build_room_scope_topic(sender->in_room, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_GROUP_ID:
        if (!IS_VALID(sender->group))
            return false;
        return channel_build_group_scope_topic(sender->group, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_CHURCH_ID:
        if (!sender->church)
            return false;
        return channel_build_church_scope_topic(sender->church, topic_buf, topic_buf_sz);

    case CHANNEL_SCOPE_INSTANCE_ID:
        return channel_build_instance_scope_topic(get_room_instance(sender->in_room),
                                                 topic_buf,
                                                 topic_buf_sz);

    case CHANNEL_SCOPE_DUNGEON_ID:
        return channel_build_dungeon_scope_topic(get_room_dungeon(sender->in_room),
                                                topic_buf,
                                                topic_buf_sz);

    case CHANNEL_SCOPE_DIRECT_ENTITY:
        /* For subscription: each player listens on their own entity topic. */
        if (sender->id[0] == 0 && sender->id[1] == 0)
            return false;
        snprintf(topic_buf, topic_buf_sz, "rt:entity:%lu:%lu", sender->id[0], sender->id[1]);
        return true;

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
    int i, j, n;
    DESCRIPTOR_DATA *d;
    CHAR_DATA *npc;
    ITERATOR npc_it;

    memset(desired, 0, sizeof(desired));
    n = channel_registry_count();

    /* 1. Persistent channels: subscribe regardless of online population.
     *    Currently only GLOBAL-scope channels are meaningful as persistent;
     *    a persistent non-GLOBAL channel would need special route logic. */
    for (i = 0; i < n; i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        char topic[128];

        if (!def->persistent)
            continue;

        if (def->scope == CHANNEL_SCOPE_GLOBAL) {
            snprintf(topic, sizeof(topic), "rt:%s", def->id);
            channel_topic_list_add_ref(desired, &desired_count, topic);
        }
    }

    /* 2. PC players: subscribe to topics matching their current context.
     *    Skips GLOBAL (handled above via persistent flag). */
    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *ch = d->original ? d->original : d->character;

        if (!ch || d->connected != CON_PLAYING || !ch->in_room || !ch->in_room->area)
            continue;

        for (j = 0; j < n; j++) {
            const CHANNEL_DEF_DATA *def = channel_registry_get(j);
            char topic[128];

            if (def->persistent && def->scope == CHANNEL_SCOPE_GLOBAL)
                continue;   /* already in desired set */

            if (!channel_definition_available_for_recipient(def, ch))
                continue;

            if (channel_build_topic_for_sender(def, ch, topic, sizeof(topic)))
                channel_topic_list_add_ref(desired, &desired_count, topic);
        }
    }

    /* 3. NPC characters: add subscriptions for context-bound scopes where
     *    NPCs participate.  This ensures the server receives area/group/church
     *    channel traffic even when only NPCs are present — important for
     *    trigger delivery and cross-server Redis routing.
     *
     *    Excluded scopes:
     *      GLOBAL        — handled by persistent flag above
     *      DIRECT_ENTITY — tells to NPCs are delivered locally, not via pubsub
     *      ROOM_WV       — handled when Stage 4 is implemented
     */
    iterator_start(&npc_it, loaded_chars);
    while ((npc = (CHAR_DATA *)iterator_nextdata(&npc_it)) != NULL) {
        if (!IS_NPC(npc) || !npc->in_room || !npc->in_room->area)
            continue;

        for (j = 0; j < n; j++) {
            const CHANNEL_DEF_DATA *def = channel_registry_get(j);
            char topic[128];

            switch (def->scope) {
            case CHANNEL_SCOPE_AREA:
            case CHANNEL_SCOPE_REGION:
            case CHANNEL_SCOPE_ROOM_WV:
            case CHANNEL_SCOPE_GROUP_ID:
            case CHANNEL_SCOPE_CHURCH_ID:
            case CHANNEL_SCOPE_INSTANCE_ID:
            case CHANNEL_SCOPE_DUNGEON_ID:
                if (channel_build_topic_for_sender(def, npc, topic, sizeof(topic)))
                    channel_topic_list_add_ref(desired, &desired_count, topic);
                break;
            default:
                break;
            }
        }
    }
    iterator_stop(&npc_it);

    /* 4. Reconcile: unsubscribe stale topics, subscribe new ones. */
    for (i = 0; i < channel_active_topic_count; i++) {
        if (channel_topic_list_find(desired, desired_count,
                                    channel_active_topics[i].topic) < 0) {
            if (!channel_transport_unsubscribe(channel_active_topics[i].topic))
                log_stringf("ChannelService: unsubscribe failed for '%s'",
                            channel_active_topics[i].topic);
        }
    }

    for (i = 0; i < desired_count; i++) {
        if (channel_topic_list_find(channel_active_topics, channel_active_topic_count,
                                    desired[i].topic) < 0) {
            if (!channel_transport_subscribe(desired[i].topic))
                log_stringf("ChannelService: subscribe failed for '%s'",
                            desired[i].topic);
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
                                       bool honor_wizi,
                                       CHAR_DATA **out_victim)
{
    CHAR_DATA *victim;
    const char *pref_channel = NULL;

    if (!sender || !desc)
        return false;

    victim = desc->original ? desc->original : desc->character;

    if (desc->connected != CON_PLAYING || !victim)
        return false;

    if (victim == sender)
        return false;

    switch (comm_block_flag) {
    case COMM_NOGOSSIP:   pref_channel = "gossip"; break;
    case COMM_NO_OOC:     pref_channel = "ooc"; break;
    case COMM_NOQUOTE:    pref_channel = "quote"; break;
    case COMM_NO_FLAMING: pref_channel = "flame"; break;
    case COMM_NOHELPER:   pref_channel = "helper"; break;
    case COMM_NOMUSIC:    pref_channel = "music"; break;
    case COMM_NOYELL:     pref_channel = "yell"; break;
    case COMM_NOCT:       pref_channel = "chtalk"; break;
    case COMM_NOWIZ:      pref_channel = "immtalk"; break;
    default:              pref_channel = NULL; break;
    }

    if (comm_block_flag != 0) {
        if (!IS_NPC(victim) && pref_channel) {
            if (!pref_check_channel(victim, pref_channel))
                return false;
        } else if (IS_SET(victim->comm, comm_block_flag)) {
            return false;
        }
    }

    if (honor_quiet && IS_SET(victim->comm, COMM_QUIET))
        return false;

    if (honor_ignore && is_ignoring(victim, sender))
        return false;

    /* Respect wizi-level invisibility for channel delivery when enabled. */
    if (honor_wizi && IS_IMMORTAL(sender) && sender->invis_level > victim->tot_level)
        return false;

    if (out_victim)
        *out_victim = victim;

    return true;
}

static bool channel_honors_pers(const char *channel_id)
{
    const CHANNEL_DEF_DATA *def = channel_find_definition(channel_id);
    return def && IS_SET(def->channel_flags, CHANNEL_FLAG_HONOR_PERS);
}

static bool channel_honors_wizi(const char *channel_id)
{
    const CHANNEL_DEF_DATA *def = channel_find_definition(channel_id);
    return def && IS_SET(def->channel_flags, CHANNEL_FLAG_HONOR_WIZI);
}

static bool channel_requires_history_topic_filter(const CHANNEL_DEF_DATA *def)
{
    if (!def)
        return false;

    switch (def->scope) {
    case CHANNEL_SCOPE_GLOBAL:
        return false;
    default:
        return true;
    }
}

static bool channel_history_matches_context(const CHANNEL_HISTORY_RECORD *record,
                                            const CHANNEL_DEF_DATA *def,
                                            CHAR_DATA *viewer)
{
    char expected_topic[128];

    if (!record || !def)
        return false;

    if (!channel_requires_history_topic_filter(def))
        return true;

    if (!viewer || IS_NULLSTR(record->topic))
        return false;

    if (!channel_build_topic_for_sender(def, viewer, expected_topic, sizeof(expected_topic)))
        return false;

    return !str_cmp(record->topic, expected_topic);
}

static long channel_player_flag_bit(const char *channel_id)
{
    if (IS_NULLSTR(channel_id))
        return 0;

    if (!str_cmp(channel_id, "ooc"))
        return FLAG_OOC;
    if (!str_cmp(channel_id, "gossip"))
        return FLAG_GOSSIP;
    if (!str_cmp(channel_id, "quote"))
        return FLAG_QUOTE;
    if (!str_cmp(channel_id, "flame"))
        return FLAG_FLAMING;
    if (!str_cmp(channel_id, "helper"))
        return FLAG_HELPER;
    if (!str_cmp(channel_id, "music"))
        return FLAG_MUSIC;
    if (!str_cmp(channel_id, "yell"))
        return FLAG_YELL;
    if (!str_cmp(channel_id, "chtalk"))
        return FLAG_CT;
    if (!str_cmp(channel_id, "tell"))
        return FLAG_TELLS;

    return 0;
}

static bool channel_should_include_sender_flag(const char *channel_id,
                                               CHAR_DATA *sender,
                                               CHAR_DATA *recipient)
{
    const CHANNEL_DEF_DATA *def;
    long flag_bit;

    if (!sender || !recipient || IS_NULLSTR(channel_id))
        return false;

    if (IS_NPC(sender) || IS_NPC(recipient) || !sender->pcdata || IS_NULLSTR(sender->pcdata->flag))
        return false;

    def = channel_find_definition(channel_id);
    if (!def || !def->allow_player_flags)
        return false;

    flag_bit = channel_player_flag_bit(channel_id);
    if (flag_bit == 0)
        return false;

    return SHOW_CHANNEL_FLAG(recipient, flag_bit);
}

static bool channel_delivery_blocked_by_ban(const char *channel_id,
                                            CHAR_DATA *recipient)
{
    const CHANNEL_DEF_DATA *def;

    if (IS_NULLSTR(channel_id) || !recipient || IS_NPC(recipient) || !recipient->desc || !recipient->desc->account)
        return false;

    def = channel_find_definition(channel_id);
    if (!def || !IS_SET(def->channel_flags, CHANNEL_FLAG_ALLOW_BAN))
        return false;

    return has_channel_ban_penalty(recipient->desc->account, channel_id, recipient->name);
}

static void channel_replace_all_simple(char *text,
                                       size_t text_sz,
                                       const char *needle,
                                       const char *replacement)
{
    char out[MSL];
    const char *src;
    size_t needle_len;
    size_t repl_len;

    if (!text || text_sz == 0 || IS_NULLSTR(needle) || !replacement)
        return;

    needle_len = strlen(needle);
    repl_len = strlen(replacement);
    src = text;
    out[0] = '\0';

    while (*src && strlen(out) < sizeof(out) - 1) {
        const char *pos = strstr(src, needle);
        size_t prefix_len;

        if (!pos)
            break;

        prefix_len = (size_t)(pos - src);
        if (prefix_len > 0)
            strncat(out, src, UMIN(prefix_len, sizeof(out) - strlen(out) - 1));

        if (repl_len > 0)
            strncat(out, replacement, UMIN(repl_len, sizeof(out) - strlen(out) - 1));

        src = pos + needle_len;
    }

    if (*src)
        strncat(out, src, UMIN(strlen(src), sizeof(out) - strlen(out) - 1));

    strlcpy(text, out, text_sz);
}

static void channel_trim_spaces(char *text)
{
    char *start;
    char *end;

    if (!text)
        return;

    start = text;
    while (*start == ' ' || *start == '\t')
        start++;

    if (start != text)
        memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';
}

static bool channel_apply_plaintext_filter_rules(const char *spec,
                                                 bool regex_mode,
                                                 char *io_text,
                                                 size_t text_sz)
{
    char rules[1024];
    char *line;
    char *saveptr = NULL;

    if (IS_NULLSTR(spec) || !io_text || text_sz == 0)
        return false;

    strlcpy(rules, spec, sizeof(rules));

    line = strtok_r(rules, "\n", &saveptr);
    while (line) {
        char rule[512];
        char *sep;

        strlcpy(rule, line, sizeof(rule));
        channel_trim_spaces(rule);

        if (!IS_NULLSTR(rule)) {
            sep = strstr(rule, "=>");
            if (sep) {
                const char *replacement;

                *sep = '\0';
                sep += 2;
                channel_trim_spaces(rule);
                channel_trim_spaces(sep);

                if (!IS_NULLSTR(rule) && channel_filter_text_matches(io_text, rule, regex_mode)) {
                    replacement = sep;
                    if (IS_NULLSTR(replacement))
                        return true;

                    if (regex_mode)
                        strlcpy(io_text, replacement, text_sz);
                    else
                        channel_replace_all_simple(io_text, text_sz, rule, replacement);
                }
            } else if (strchr(rule, ',')) {
                char *tok;
                char *csv_save = NULL;

                tok = strtok_r(rule, ",", &csv_save);
                while (tok) {
                    channel_trim_spaces(tok);
                    if (!IS_NULLSTR(tok) && channel_filter_text_matches(io_text, tok, regex_mode))
                        return true;
                    tok = strtok_r(NULL, ",", &csv_save);
                }
            } else {
                if (channel_filter_text_matches(io_text, rule, regex_mode))
                    return true;
            }
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    return false;
}

static bool channel_apply_pref_filter_spec(const char *spec,
                                           bool regex_mode,
                                           char *io_text,
                                           size_t text_sz)
{
    json_t *root;
    json_error_t err;

    if (IS_NULLSTR(spec) || !io_text || text_sz == 0)
        return false;

    root = json_loads(spec, 0, &err);
    if (!root) {
        return channel_apply_plaintext_filter_rules(spec, regex_mode, io_text, text_sz);
    }

    if (json_is_array(root)) {
        size_t i;
        json_t *item;

        json_array_foreach(root, i, item) {
            if (json_is_string(item)) {
                const char *pattern = json_string_value(item);
                if (!IS_NULLSTR(pattern)
                    && channel_filter_text_matches(io_text, pattern, regex_mode)) {
                    json_decref(root);
                    return true;
                }
            } else if (json_is_object(item)) {
                const char *pattern = json_string_value(json_object_get(item, "match"));
                const char *replacement = json_string_value(json_object_get(item, "replace"));
                json_t *block_val = json_object_get(item, "block");
                bool force_block = (block_val && json_is_boolean(block_val) && json_is_true(block_val));

                if (IS_NULLSTR(pattern)
                    || !channel_filter_text_matches(io_text, pattern, regex_mode))
                    continue;

                if (force_block || IS_NULLSTR(replacement)) {
                    json_decref(root);
                    return true;
                }

                if (regex_mode)
                    strlcpy(io_text, replacement, text_sz);
                else
                    channel_replace_all_simple(io_text, text_sz, pattern, replacement);
            }
        }

        json_decref(root);
        return false;
    }

    if (json_is_object(root)) {
        const char *pattern = json_string_value(json_object_get(root, "match"));
        const char *replacement = json_string_value(json_object_get(root, "replace"));
        json_t *block_val = json_object_get(root, "block");
        bool force_block = (block_val && json_is_boolean(block_val) && json_is_true(block_val));

        if (!IS_NULLSTR(pattern) && channel_filter_text_matches(io_text, pattern, regex_mode)) {
            if (force_block || IS_NULLSTR(replacement)) {
                json_decref(root);
                return true;
            }

            if (regex_mode)
                strlcpy(io_text, replacement, text_sz);
            else
                channel_replace_all_simple(io_text, text_sz, pattern, replacement);
        }

        json_decref(root);
        return false;
    }

    json_decref(root);
    return false;
}

static bool channel_delivery_filtered_by_preferences(const char *channel_id,
                                                     CHAR_DATA *recipient,
                                                     const char *plain_text,
                                                     char *out_text,
                                                     size_t out_text_sz)
{
    ACCOUNT_DATA *acct;
    char key_simple[96];
    char key_regex[96];
    char key_simple_list[96];
    char key_regex_list[96];
    const char *simple_global;
    const char *regex_global;
    const char *simple_global_list;
    const char *regex_global_list;
    const char *simple_channel;
    const char *regex_channel;
    const char *simple_channel_list;
    const char *regex_channel_list;
    char working[MSL];

    if (!recipient || IS_NPC(recipient) || IS_NULLSTR(plain_text) || !out_text || out_text_sz == 0)
        return false;

    acct = recipient->desc ? recipient->desc->account : NULL;
    strlcpy(working, plain_text, sizeof(working));

    simple_global = pref_get_string(acct, recipient, "filter_simple", "");
    regex_global = pref_get_string(acct, recipient, "filter_regex", "");
    simple_global_list = pref_get_string(acct, recipient, "filter_simple_list", "");
    regex_global_list = pref_get_string(acct, recipient, "filter_regex_list", "");

    snprintf(key_simple, sizeof(key_simple), "filter_%s_simple", IS_NULLSTR(channel_id) ? "channel" : channel_id);
    snprintf(key_regex, sizeof(key_regex), "filter_%s_regex", IS_NULLSTR(channel_id) ? "channel" : channel_id);
    snprintf(key_simple_list, sizeof(key_simple_list), "filter_%s_simple_list", IS_NULLSTR(channel_id) ? "channel" : channel_id);
    snprintf(key_regex_list, sizeof(key_regex_list), "filter_%s_regex_list", IS_NULLSTR(channel_id) ? "channel" : channel_id);

    simple_channel = pref_get_string(acct, recipient, key_simple, "");
    regex_channel = pref_get_string(acct, recipient, key_regex, "");
    simple_channel_list = pref_get_string(acct, recipient, key_simple_list, "");
    regex_channel_list = pref_get_string(acct, recipient, key_regex_list, "");

    if (channel_apply_pref_filter_spec(simple_global, false, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(regex_global, true, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(simple_global_list, false, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(regex_global_list, true, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(simple_channel, false, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(regex_channel, true, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(simple_channel_list, false, working, sizeof(working)))
        return true;
    if (channel_apply_pref_filter_spec(regex_channel_list, true, working, sizeof(working)))
        return true;

    strlcpy(out_text, working, out_text_sz);
    return false;
}

static void channel_extract_whois_target(const char *display_name,
                                         char *out,
                                         size_t out_sz)
{
    char stripped[MSL];
    char working[MSL];
    char token[MIL];

    if (!out || out_sz == 0) {
        return;
    }

    out[0] = '\0';

    if (IS_NULLSTR(display_name))
        return;

    stripped[0] = '\0';
    STRIP_COLOUR(display_name, stripped);
    strlcpy(working, stripped, sizeof(working));
    one_argument(working, token);

    if (!IS_NULLSTR(token))
        strlcpy(out, token, out_sz);
}

static void channel_send_targeted_notvict(const char *channel_id,
                                          CHAR_DATA *sender,
                                          CHAR_DATA *target,
                                          const char *plain_text,
                                          const char *fallback_act,
                                          const char *fallback_act_arg)
{
    const CHANNEL_DEF_DATA *def;

    if (!sender || !target || !sender->in_room)
        return;

    def = channel_find_definition(channel_id);
    if (def && !IS_NULLSTR(def->fmt_notvict)) {
        CHAR_DATA *viewer;

        for (viewer = sender->in_room->people; viewer; viewer = viewer->next_in_room) {
            char rendered[MAX_STRING_LENGTH];
            const char *sender_name;
            const char *target_name;

            if (viewer == sender || viewer == target)
                continue;

            sender_name = channel_honors_pers(channel_id) ? pers(sender, viewer) : sender->name;
            target_name = pers(target, viewer);

            snprintf(rendered,
                     sizeof(rendered),
                     def->fmt_notvict,
                     sender_name,
                     target_name,
                     IS_NULLSTR(plain_text) ? "" : plain_text);

            if (!strstr(rendered, "\n\r"))
                strlcat(rendered, "\n\r", sizeof(rendered));

            send_to_char(rendered, viewer);
        }
        return;
    }

    if (!IS_NULLSTR(fallback_act)) {
        act((char *)fallback_act,
            sender,
            target,
            NULL,
            NULL,
            NULL,
            (void *)(IS_NULLSTR(fallback_act_arg) ? "" : fallback_act_arg),
            NULL,
            TO_NOTVICT,
            NULL,
            NULL);
    }
}

static void channel_sender_name_mxp(descriptor_t *desc,
                                    const char *channel_id,
                                    const char *sender_name,
                                    char *out,
                                    size_t out_sz)
{
    const CHANNEL_DEF_DATA *def;
    BUFFER *mxp_buf;
    char whois_target[MIL];
    char whois_cmd[MSL];
    char tell_cmd[MSL];
    char history_cmd[MSL];
    char warn_cmd[MSL];
    char mute_cmd[MSL];
    char ban_cmd[MSL];
    bool can_moderate = false;
    mxp_cmd_hint_t items[6];
    int nitems = 0;
    int i;
    CHAR_DATA *viewer;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';

    if (IS_NULLSTR(sender_name))
        return;

    viewer = (desc && desc->character) ? (desc->original ? desc->original : desc->character) : NULL;
    def = channel_find_definition(channel_id);

    channel_extract_whois_target(sender_name, whois_target, sizeof(whois_target));
    if (IS_NULLSTR(whois_target) || !desc || !isMXP(desc)) {
        strlcpy(out, sender_name, out_sz);
        return;
    }

    if (viewer && IS_IMMORTAL(viewer))
        can_moderate = true;

    if (!can_moderate && viewer && def) {
        for (i = 0; i < def->mod_count; i++) {
            if (!str_cmp(def->moderators[i], viewer->name)) {
                can_moderate = true;
                break;
            }
        }
    }

    snprintf(whois_cmd, sizeof(whois_cmd), "whois %s", whois_target);
    items[nitems].cmd = whois_cmd;
    items[nitems].hint = "Whois player";
    nitems++;

    snprintf(tell_cmd, sizeof(tell_cmd), "tell %s ", whois_target);
    items[nitems].cmd = tell_cmd;
    items[nitems].hint = "Tell player";
    nitems++;

    if (!IS_NULLSTR(channel_id)) {
        snprintf(history_cmd, sizeof(history_cmd), "history %s", channel_id);
        items[nitems].cmd = history_cmd;
        items[nitems].hint = "View channel history";
        nitems++;
    }

    if (can_moderate && !IS_NULLSTR(channel_id)) {
        snprintf(warn_cmd, sizeof(warn_cmd), "chanwarn %s %s", whois_target, channel_id);
        items[nitems].cmd = warn_cmd;
        items[nitems].hint = "Warn on channel";
        nitems++;

        snprintf(mute_cmd, sizeof(mute_cmd), "chanmute %s %s 15m", whois_target, channel_id);
        items[nitems].cmd = mute_cmd;
        items[nitems].hint = "Mute 15m on channel";
        nitems++;

        snprintf(ban_cmd, sizeof(ban_cmd), "chanban %s %s", whois_target, channel_id);
        items[nitems].cmd = ban_cmd;
        items[nitems].hint = "Ban on channel";
        nitems++;
    }

    mxp_buf = new_buf();
    mxp_link_multi(desc, mxp_buf, sender_name, items, nitems);
    strlcpy(out, buf_string(mxp_buf), out_sz);
    free_buf(mxp_buf);
}

static bool channel_supports_service_self_echo(const char *channel_id)
{
    if (IS_NULLSTR(channel_id))
        return false;

    return !str_cmp(channel_id, "gossip")
        || !str_cmp(channel_id, "ooc")
        || !str_cmp(channel_id, "quote")
        || !str_cmp(channel_id, "flame")
        || !str_cmp(channel_id, "helper")
        || !str_cmp(channel_id, "music")
        || !str_cmp(channel_id, "immtalk")
        || !str_cmp(channel_id, "yell")
        || !str_cmp(channel_id, "gtell");
}

static void channel_send_formatted_to_sender(const char *channel_id,
                                             CHAR_DATA *sender,
                                             const char *plain_text)
{
    const CHANNEL_DEF_DATA *def;
    const char *fmt = "{WYou:{x %2$s";
    const char *message_text;
    char message_with_flag[2 * MSL];
    char rendered[MAX_STRING_LENGTH];

    if (!sender || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return;

    def = channel_find_definition(channel_id);
    if (def && !IS_NULLSTR(def->fmt_self))
        fmt = def->fmt_self;

    message_text = plain_text;
    if (channel_should_include_sender_flag(channel_id, sender, sender)) {
        snprintf(message_with_flag, sizeof(message_with_flag), "%s %s",
                 sender->pcdata->flag, plain_text);
        message_text = message_with_flag;
    }

    snprintf(rendered, sizeof(rendered), fmt, "You", message_text);

    if (!strstr(rendered, "\n\r"))
        strlcat(rendered, "\n\r", sizeof(rendered));

    send_to_char(rendered, sender);
}

static void channel_format_for_recipient(const char *channel_id,
                                         CHAR_DATA *sender,
                                         CHAR_DATA *recipient,
                                         const char *plain_text,
                                         char *out,
                                         size_t out_sz)
{
    const CHANNEL_DEF_DATA *def;
    const char *fmt = "{W%1$s:{x %2$s";
    const char *sender_name;
    const char *sender_name_render;
    const char *message_text;
    char sender_name_mxp[MAX_STRING_LENGTH];
    char message_with_flag[2 * MSL];

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';

    if (!sender || !recipient || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return;

    def = channel_find_definition(channel_id);
    if (def && !IS_NULLSTR(def->fmt_receiver))
        fmt = def->fmt_receiver;

    sender_name = channel_honors_pers(channel_id) ? pers(sender, recipient) : sender->name;
    channel_sender_name_mxp(recipient ? recipient->desc : NULL,
                            channel_id,
                            sender_name,
                            sender_name_mxp,
                            sizeof(sender_name_mxp));
    sender_name_render = sender_name_mxp[0] ? sender_name_mxp : sender_name;

    message_text = plain_text;
    if (channel_should_include_sender_flag(channel_id, sender, recipient)) {
        snprintf(message_with_flag, sizeof(message_with_flag), "%s %s",
                 sender->pcdata->flag, plain_text);
        message_text = message_with_flag;
    }

    snprintf(out, out_sz, fmt, sender_name_render, message_text);

    if (!strstr(out, "\n\r"))
        strlcat(out, "\n\r", out_sz);
}

static void channel_send_formatted_to_recipient(const char *channel_id,
                                                CHAR_DATA *sender,
                                                CHAR_DATA *recipient,
                                                const char *plain_text)
{
    char rendered[MAX_STRING_LENGTH];
    char filtered_text[MSL];
    const CHANNEL_DEF_DATA *def;

    if (!sender || !recipient || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return;

    def = channel_find_definition(channel_id);
    if (!def)
        return;

    if (!channel_definition_available_for_recipient(def, recipient))
        return;

    if (channel_delivery_blocked_by_ban(channel_id, recipient))
        return;

    if (channel_delivery_filtered_by_preferences(channel_id, recipient, plain_text,
                                                 filtered_text, sizeof(filtered_text)))
        return;

    channel_format_for_recipient(channel_id, sender, recipient,
                                 filtered_text[0] ? filtered_text : plain_text,
                                 rendered, sizeof(rendered));

    if (rendered[0] != '\0')
        send_to_char(rendered, recipient);
}

static void channel_deliver_ooc_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("ooc");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NO_OOC, true, true, honor_wizi, &victim)) {
            channel_send_formatted_to_recipient("ooc", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_gossip_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("gossip");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOGOSSIP, true, true, honor_wizi, &victim)) {
            channel_send_formatted_to_recipient("gossip", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_quote_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("quote");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOQUOTE, true, true, honor_wizi, &victim)) {
            channel_send_formatted_to_recipient("quote", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_flame_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("flame");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NO_FLAMING, true, true, honor_wizi, &victim)) {
            channel_send_formatted_to_recipient("flame", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_helper_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("helper");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOHELPER, true, true, honor_wizi, &victim) &&
            victim != sender) {
            channel_send_formatted_to_recipient("helper", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_music_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("music");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOMUSIC, true, true, honor_wizi, &victim)) {
            channel_send_formatted_to_recipient("music", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_immtalk_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("immtalk");

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (!channel_can_deliver_to_descriptor(sender, d, COMM_NOWIZ, false, false,
                                               honor_wizi, &victim))
            continue;

        if (victim && victim != sender) {
            channel_send_formatted_to_recipient("immtalk", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_yell_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    bool honor_wizi;

    if (!sender || !sender->in_room || !sender->in_room->area || IS_NULLSTR(plain_text))
        return;

    honor_wizi = channel_honors_wizi("yell");

    if (channel_room_is_area_isolated(sender->in_room))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (channel_can_deliver_to_descriptor(sender, d, COMM_NOYELL, true, true, honor_wizi, &victim) &&
            victim->in_room && victim->in_room->area == sender->in_room->area &&
            !channel_room_is_area_isolated(victim->in_room) &&
            victim != sender) {
            channel_send_formatted_to_recipient("yell", sender, victim, plain_text);
        }
    }
}

static void channel_deliver_gtell_legacy(CHAR_DATA *sender, const char *plain_text)
{
    CHAR_DATA *gch;
    GROUP_DATA *group;
    ITERATOR it;
    bool honor_wizi;

    if (!sender || IS_NULLSTR(plain_text))
        return;

    group = IS_VALID(sender->group) ? sender->group : NULL;
    honor_wizi = channel_honors_wizi("gtell");

    if (IS_VALID(group) && group->members)
        iterator_start(&it, group->members);
    else
        iterator_start(&it, loaded_chars);

    while ((gch = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (gch == sender)
            continue;

        if (honor_wizi && IS_IMMORTAL(sender) && sender->invis_level > gch->tot_level)
            continue;

        if ((IS_VALID(group) && gch->group == group) || (!IS_VALID(group) && is_same_group(gch, sender))) {
            channel_send_formatted_to_recipient("gtell", sender, gch, plain_text);
        }
    }
    iterator_stop(&it);
}

static void channel_format_expand_church_tokens(const char *format_template,
                                                CHAR_DATA *sender,
                                                CHAR_DATA *recipient,
                                                char *out,
                                                size_t out_sz)
{
    const char *cursor;
    const char *church_colour1 = "";
    const char *church_colour2 = "";
    char sender_flag[MSL];

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';
    sender_flag[0] = '\0';

    if (!IS_NULLSTR(format_template)) {
        if (sender && sender->church) {
            church_colour1 = sender->church->colour1 ? sender->church->colour1 : "";
            church_colour2 = sender->church->colour2 ? sender->church->colour2 : "";
        }

        if (sender && recipient && !IS_NPC(sender) && sender->pcdata &&
            !IS_NULLSTR(sender->pcdata->flag) && SHOW_CHANNEL_FLAG(recipient, FLAG_CT)) {
            snprintf(sender_flag, sizeof(sender_flag), "%s ", sender->pcdata->flag);
        }

        cursor = format_template;
        while (*cursor != '\0') {
            if (!str_prefix("$church_colour1", cursor)) {
                strlcat(out, church_colour1, out_sz);
                cursor += strlen("$church_colour1");
                continue;
            }

            if (!str_prefix("$church_colour2", cursor)) {
                strlcat(out, church_colour2, out_sz);
                cursor += strlen("$church_colour2");
                continue;
            }

            if (!str_prefix("$sender_flag", cursor)) {
                strlcat(out, sender_flag, out_sz);
                cursor += strlen("$sender_flag");
                continue;
            }

            {
                char ch[2];
                ch[0] = *cursor;
                ch[1] = '\0';
                strlcat(out, ch, out_sz);
                cursor++;
            }
        }
    }
}

static void channel_deliver_chtalk_legacy(CHAR_DATA *sender, const char *plain_text)
{
    DESCRIPTOR_DATA *d;
    const CHANNEL_DEF_DATA *def = channel_find_definition("chtalk");
    const char *receiver_fmt = "$church_colour2[$church_colour1%1$s$church_colour2] says '$sender_flag$church_colour1%2$s$church_colour2'{x";
    char expanded_fmt[MAX_STRING_LENGTH];
    char msg[MAX_STRING_LENGTH];

    if (!sender || !sender->church || IS_NULLSTR(plain_text))
        return;

    for (d = descriptor_list; d != NULL; d = d->next) {
        CHAR_DATA *victim = NULL;

        if (!channel_can_deliver_to_descriptor(sender, d, COMM_NOCT, true, true,
                                               channel_honors_wizi("chtalk"), &victim))
            continue;

        if (!victim || victim->church != sender->church)
            continue;

        if (def && !IS_NULLSTR(def->fmt_receiver))
            receiver_fmt = def->fmt_receiver;

        channel_format_expand_church_tokens(receiver_fmt, sender, victim,
                                            expanded_fmt, sizeof(expanded_fmt));

        snprintf(msg, sizeof(msg), expanded_fmt[0] ? expanded_fmt : "%s: %s",
             channel_honors_pers("chtalk") ? pers(sender, victim) : sender->name,
             plain_text);

        if (!strstr(msg, "\n\r"))
            strlcat(msg, "\n\r", sizeof(msg));

        send_to_char(msg, d->character);
    }
}

static void channel_deliver_tell_legacy(CHAR_DATA *sender,
                                        CHAR_DATA *recipient,
                                        const char *plain_text)
{
    char msg[2 * MSL];
    char display_text[MSL];
    CHANNEL_TELL_POLICY_BLOCK tell_block;

    if (!sender || !recipient || IS_NULLSTR(plain_text))
        return;

    display_text[0] = '\0';
    STRIP_COLOUR((char *)plain_text, display_text);
    if (!display_text[0])
        return;

    if (!channel_policy_tell_delivery_allowed(sender, recipient, false, &tell_block))
        return;

    if (!IS_NPC(sender) && sender->pcdata->flag && SHOW_CHANNEL_FLAG(recipient, FLAG_TELLS))
        sprintf(msg, "{R%s tells you '%s {R%s{R'{x\n\r", sender->name, sender->pcdata->flag, display_text);
    else if (IS_NPC(sender))
        sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(sender, recipient), display_text);
    else
        sprintf(msg, "{R%s tells you '%s'{x\n\r", sender->name, display_text);

    msg[2] = UPPER(msg[2]);

    if (!IS_NPC(recipient) && recipient->desc == NULL && recipient->pcdata && recipient->pcdata->buffer) {
        add_buf(recipient->pcdata->buffer, msg);
        return;
    }

    if (!channel_policy_tell_delivery_allowed(sender, recipient, true, &tell_block)
        && tell_block == CHANNEL_TELL_POLICY_BLOCK_QUIET)
        return;

    if (!IS_NPC(recipient) && IS_SET(recipient->comm, COMM_AFK) &&
        recipient->pcdata && recipient->pcdata->buffer) {
        add_buf(recipient->pcdata->buffer, msg);
        return;
    }

    send_to_char(msg, recipient);

    recipient->reply = sender;
}

static void channel_notify_staff_report(const char *channel_id,
                                        const char *queue_name,
                                        const char *report_id,
                                        const char *reason,
                                        const char *reporter_name,
                                        const char *detail_text)
{
    char buf[MAX_STRING_LENGTH];

    snprintf(buf,
             sizeof(buf),
             "{Y[Channel Report]{x queue={W%s{x channel={W%s{x id={W%s{x reason={W%s{x by={W%s{x",
             IS_NULLSTR(queue_name) ? "(default)" : queue_name,
             IS_NULLSTR(channel_id) ? "(unknown)" : channel_id,
             IS_NULLSTR(report_id) ? "(none)" : report_id,
             IS_NULLSTR(reason) ? "none" : reason,
             IS_NULLSTR(reporter_name) ? "(unknown)" : reporter_name);

    channel_staff_report_append(report_id,
                                queue_name,
                                channel_id,
                                reason,
                                reporter_name,
                                detail_text);

    wiznet(buf, NULL, NULL, WIZ_SECURE, 0, STAFF_IMMORTAL);
}

int channel_service_staff_report_recent(CHANNEL_STAFF_REPORT_ENTRY *out_entries,
                                        int out_cap,
                                        bool include_acknowledged)
{
    int written = 0;
    int i;

    if (!out_entries || out_cap <= 0)
        return 0;

    for (i = 0; i < channel_staff_report_ring_count && written < out_cap; i++) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];

        if (!include_acknowledged && record->acknowledged)
            continue;

        channel_staff_report_copy_entry(&out_entries[written], record);
        written++;
    }

    return written;
}

int channel_service_staff_report_count(void)
{
    int i;
    int count = 0;

    for (i = 0; i < channel_staff_report_ring_count; i++) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];

        if (!record->acknowledged)
            count++;
    }

    return count;
}

bool channel_service_staff_report_by_id(const char *report_id,
                                        CHANNEL_STAFF_REPORT_ENTRY *out_entry,
                                        bool *out_acknowledged)
{
    CHANNEL_STAFF_REPORT_RECORD *record = channel_staff_report_find(report_id);

    if (!record)
        return false;

    if (out_entry)
        channel_staff_report_copy_entry(out_entry, record);

    if (out_acknowledged)
        *out_acknowledged = record->acknowledged;

    return true;
}

bool channel_service_staff_report_by_index(int index,
                                           bool include_acknowledged,
                                           CHANNEL_STAFF_REPORT_ENTRY *out_entry,
                                           bool *out_acknowledged)
{
    int i;
    int visible = 0;

    if (index <= 0)
        return false;

    for (i = 0; i < channel_staff_report_ring_count; i++) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];

        if (!include_acknowledged && record->acknowledged)
            continue;

        visible++;
        if (visible != index)
            continue;

        if (out_entry)
            channel_staff_report_copy_entry(out_entry, record);

        if (out_acknowledged)
            *out_acknowledged = record->acknowledged;

        return true;
    }

    return false;
}

bool channel_service_staff_report_ack(const char *report_id,
                                      const char *staff_name)
{
    CHANNEL_STAFF_REPORT_RECORD *record = channel_staff_report_find(report_id);

    if (!record)
        return false;

    record->acknowledged = true;
    strlcpy(record->ack_by,
            IS_NULLSTR(staff_name) ? "(unknown)" : staff_name,
            sizeof(record->ack_by));

        channel_service_mark_staff_report_dirty();

    return true;
}

static int channel_staff_report_compact(bool drop_acknowledged, int max_entries)
{
    CHANNEL_STAFF_REPORT_RECORD kept[CHANNEL_STAFF_REPORT_RING_MAX];
    int original_count = channel_staff_report_ring_count;
    int kept_count = 0;
    int i;

    for (i = channel_staff_report_ring_count - 1; i >= 0; i--) {
        int ring_index = (channel_staff_report_ring_next - 1 - i + CHANNEL_STAFF_REPORT_RING_MAX)
            % CHANNEL_STAFF_REPORT_RING_MAX;
        CHANNEL_STAFF_REPORT_RECORD *record = &channel_staff_report_ring[ring_index];

        if (drop_acknowledged && record->acknowledged)
            continue;

        if (kept_count < CHANNEL_STAFF_REPORT_RING_MAX)
            kept[kept_count++] = *record;
    }

    if (max_entries > 0 && kept_count > max_entries) {
        int offset = kept_count - max_entries;
        memmove(kept, kept + offset, sizeof(CHANNEL_STAFF_REPORT_RECORD) * max_entries);
        kept_count = max_entries;
    }

    channel_staff_report_reset();
    for (i = 0; i < kept_count; i++) {
        channel_staff_report_ring[channel_staff_report_ring_next] = kept[i];
        channel_staff_report_ring_next =
            (channel_staff_report_ring_next + 1) % CHANNEL_STAFF_REPORT_RING_MAX;
        channel_staff_report_ring_count++;
    }

    if (original_count != kept_count)
        channel_service_mark_staff_report_dirty();

    return original_count - kept_count;
}

int channel_service_staff_report_purge_acknowledged(void)
{
    return channel_staff_report_compact(true, 0);
}

int channel_service_staff_report_trim(int max_entries)
{
    if (max_entries < 1)
        return 0;

    return channel_staff_report_compact(false, max_entries);
}

static bool channel_should_fire_speech_triggers(CHAR_DATA *sender)
{
    return sender && (!IS_NPC(sender) || IS_SWITCHED(sender));
}

static void channel_fire_say_speech_triggers(CHAR_DATA *sender,
                                             const char *plain_text)
{
    CHAR_DATA *mob;
    CHAR_DATA *mob_next;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    ITERATOR obj_it;

    if (!channel_should_fire_speech_triggers(sender)
        || !sender->in_room
        || IS_NULLSTR(plain_text))
        return;

    for (mob = sender->in_room->people; mob != NULL; mob = mob_next) {
        mob_next = mob->next_in_room;
        if (!IS_NPC(mob) || mob->position == mob->pIndexData->default_pos)
            p_act_trigger((char *)plain_text, mob, NULL, NULL, sender, NULL, NULL, NULL, NULL, TRIG_SPEECH);

        iterator_start(&obj_it, mob->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&obj_it))) {
            p_act_trigger((char *)plain_text, NULL, obj, NULL, sender, NULL, NULL, NULL, NULL, TRIG_SPEECH);
        }
        iterator_stop(&obj_it);

        iterator_start(&obj_it, mob->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&obj_it))) {
            p_act_trigger((char *)plain_text, NULL, obj, NULL, sender, NULL, NULL, NULL, NULL, TRIG_SPEECH);
        }
        iterator_stop(&obj_it);
    }

    for (obj = sender->in_room->contents; obj; obj = obj_next) {
        obj_next = obj->next_content;
        p_act_trigger((char *)plain_text, NULL, obj, NULL, sender, NULL, NULL, NULL, NULL, TRIG_SPEECH);
    }

    p_act_trigger((char *)plain_text, NULL, NULL, sender->in_room, sender, NULL, NULL, NULL, NULL, TRIG_SPEECH);
}

static void channel_fire_targeted_speech_hooks(CHAR_DATA *sender,
                                               CHAR_DATA *target,
                                               const char *plain_text,
                                               int trigger_type,
                                               bool include_quest_talk)
{
    if (!sender || !target || IS_NULLSTR(plain_text))
        return;

    if (channel_should_fire_speech_triggers(sender)
        && (!IS_NPC(target) || target->position == target->pIndexData->default_pos))
    {
        p_act_trigger((char *)plain_text, target, NULL, NULL, sender, NULL, NULL, NULL, NULL, trigger_type);
    }

    if (include_quest_talk && !IS_NPC(sender))
        check_quest_talk_target(sender, target, (char *)plain_text, true);
}

static void channel_fire_object_speech_hook(CHAR_DATA *sender,
                                            OBJ_DATA *target,
                                            const char *plain_text)
{
    if (!sender || !target || IS_NULLSTR(plain_text))
        return;

    if (channel_should_fire_speech_triggers(sender))
        p_act_trigger((char *)plain_text, NULL, target, NULL, sender, NULL, NULL, NULL, NULL, TRIG_SAYTO);
}

static void channel_deliver_say_legacy(CHAR_DATA *sender, const char *plain_text)
{
    int i;
    char msg[MSL];
    char buf[MSL];
    char buf2[MSL];
    char *second;
    bool break_line = true;
    const CHANNEL_DEF_DATA *say_def;

    if (!sender || !sender->in_room || IS_NULLSTR(plain_text))
        return;

    strlcpy(msg, plain_text, sizeof(msg));

    say_def = channel_find_definition("say");

    if (channel_has_modifier(say_def, CHANNEL_MOD_PUNCTUATION_PARSE)) {
        buf[0] = '\0';
        for (i = 0; msg[i] != '\0'; i++) {
            if (msg[i] == '!' && msg[i + 1] != ' ')
                break_line = false;
        }

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), "!");
            while (*second == ' ')
                second++;

            if (*second != '\0') {
                sprintf(buf2, "{C'$T!{C' exclaims $n. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
                sprintf(buf2, "{C'$T!{C' you exclaim. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
                return;
            }
        }

        for (i = 0; msg[i] != '\0'; i++) {
            if (msg[i] == '?' && msg[i + 1] != ' ')
                break_line = false;
        }

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), "?");
            while (*second == ' ')
                second++;

            if (*second != '\0') {
                sprintf(buf2, "{C'$T?{C' asks $n. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
                sprintf(buf2, "{C'$T?{C' you ask. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
                return;
            }
        }

        for (i = 0; msg[i] != '\0'; i++) {
            if (msg[i] == '.' && msg[i + 1] != ' ')
                break_line = false;
        }

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), ".");
            while (*second == ' ')
                second++;

            if (*second != '\0') {
                sprintf(buf2, "{C'$T.{C' says $n. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
                sprintf(buf2, "{C'$T.{C' you say. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
                return;
            }
        }

        i = (int)strlen(msg) - 1;
        if (i >= 0 && msg[i] == '!') {
            if (number_percent() < 50) {
                act("{C'$T{C' exclaims $n.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{C'$T{C' you exclaim.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            } else {
                act("{C$n exclaims, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{CYou exclaim, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            }
        } else if (i >= 0 && msg[i] == '?') {
            if (number_percent() < 50) {
                act("{C$n asks, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{CYou ask, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            } else {
                act("{C'$T{C' asks $n.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{C'$T{C' you ask.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            }
        } else {
            if (number_percent() < 50) {
                act("{C$n says, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{CYou say, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            } else {
                act("{C'$T{C' says $n.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
                act("{C'$T{C' you say.{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
            }
        }
    } else {
        i = (int)strlen(msg) - 1;
        if (i >= 0 && msg[i] == '!') {
            act("{C$n exclaims, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
            act("{CYou exclaim, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
        } else if (i >= 0 && msg[i] == '?') {
            act("{C$n asks, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
            act("{CYou ask, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
        } else {
            act("{C$n says, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
            act("{CYou say, '$T{C'{x", sender, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
        }
    }

    channel_fire_say_speech_triggers(sender, msg);
}

static void channel_deliver_whisper_legacy(CHAR_DATA *sender, CHAR_DATA *target, const char *plain_text)
{
    if (!sender || !target || !sender->in_room || !target->in_room || sender->in_room != target->in_room ||
        IS_NULLSTR(plain_text))
        return;

    act("{CYou whisper to $N '$t'{x", sender, target, NULL, NULL, NULL, (void *)plain_text, NULL, TO_CHAR, NULL, NULL);
    channel_send_targeted_notvict("whisper", sender, target, plain_text,
                                  "{C$n whispers something to $N.{x", "");
    act("{C$n whispers to you '$t'{x", sender, target, NULL, NULL, NULL, (void *)plain_text, NULL, TO_VICT, NULL, NULL);

    channel_fire_targeted_speech_hooks(sender, target, plain_text, TRIG_WHISPER, true);
}

static void channel_deliver_sayto_legacy(CHAR_DATA *sender, CHAR_DATA *target, const char *plain_text)
{
    int i;
    char msg[MSL];
    char buf[MSL];
    char buf2[MSL];
    char *second;
    bool break_line = true;
    const CHANNEL_DEF_DATA *sayto_def;

    if (!sender || !target || !sender->in_room || !target->in_room || sender->in_room != target->in_room ||
        IS_NULLSTR(plain_text))
        return;

    strlcpy(msg, plain_text, sizeof(msg));

    sayto_def = channel_find_definition("sayto");

    if (channel_has_modifier(sayto_def, CHANNEL_MOD_PUNCTUATION_PARSE)) {
        buf[0] = '\0';
        for (i = 0; msg[i] != '\0'; i++)
            if (msg[i] == '!' && msg[i + 1] != ' ')
                break_line = false;

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), "!");
            while (*second == ' ')
                second++;

            if (*second) {
                sprintf(buf2, "{C'$t!{C' exclaims $n to $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                channel_send_targeted_notvict("sayto", sender, target, plain_text, buf2, buf);
                sprintf(buf2, "{C'$t!{C' exclaims $n to you. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_VICT, NULL, NULL);
                sprintf(buf2, "{C'$t!{C' you exclaim to $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_CHAR, NULL, NULL);
                return;
            }
        }

        for (i = 0; msg[i] != '\0'; i++)
            if (msg[i] == '?' && msg[i + 1] != ' ')
                break_line = false;

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), "?");
            while (*second == ' ')
                second++;

            if (*second) {
                sprintf(buf2, "{C'$t!{C' $n asks $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                channel_send_targeted_notvict("sayto", sender, target, plain_text, buf2, buf);
                sprintf(buf2, "{C'$t!{C' $n asks you. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_VICT, NULL, NULL);
                sprintf(buf2, "{C'$t!{C' you ask $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_CHAR, NULL, NULL);
                return;
            }
        }

        for (i = 0; msg[i] != '\0'; i++)
            if (msg[i] == '.' && msg[i + 1] != ' ')
                break_line = false;

        if (break_line) {
            second = stptok(msg, buf, sizeof(buf), ".");
            while (*second == ' ')
                second++;

            if (*second) {
                sprintf(buf2, "{C'$t!{C' says $n to $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                channel_send_targeted_notvict("sayto", sender, target, plain_text, buf2, buf);
                sprintf(buf2, "{C'$t!{C' says $n to you. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_VICT, NULL, NULL);
                sprintf(buf2, "{C'$t!{C' you say to $N. '");
                strcat(buf2, second);
                strcat(buf2, "'{x");
                act(buf2, sender, target, NULL, NULL, NULL, buf, NULL, TO_CHAR, NULL, NULL);
                return;
            }
        }

        i = (int)strlen(msg) - 1;
        if (i >= 0 && msg[i] == '!') {
            if (number_percent() < 50) {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C'$t{C' exclaims $n to $N.{x", msg);
                act("{C'$t{C' exclaims $n to you.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{C'$t{C' you exclaim to $N.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            } else {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C$n exclaims to $N, '$t{C'{x", msg);
                act("{C$n exclaims to you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{CYou exclaim to $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            }
        } else if (i >= 0 && msg[i] == '?') {
            if (number_percent() < 50) {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C$n asks $N, '$t{C'{x", msg);
                act("{C$n asks you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{CYou ask $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            } else {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C'$t{C' $n asks $N.{x", msg);
                act("{C'$t{C' $n asks you.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{C'$t{C' you ask $N.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            }
        } else {
            if (number_percent() < 50) {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C$n says to $N, '$t{C'{x", msg);
                act("{C$n says to you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{CYou say to $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            } else {
                channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                              "{C'$t{C' says $n to $N.{x", msg);
                act("{C'$t{C' says $n to you.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
                act("{C'$t{C' you say to $N.{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
            }
        }
    } else {
        i = (int)strlen(msg) - 1;
        if (i >= 0 && msg[i] == '!') {
            channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                          "{C$n exclaims to $N, '$t{C'{x", msg);
            act("{C$n exclaims to you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
            act("{CYou exclaim to $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
        } else if (i >= 0 && msg[i] == '?') {
            channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                          "{C$n asks $N, '$t{C'{x", msg);
            act("{C$n asks you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
            act("{CYou ask $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
        } else {
            channel_send_targeted_notvict("sayto", sender, target, plain_text,
                                          "{C$n says to $N, '$t{C'{x", msg);
            act("{C$n says to you, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
            act("{CYou say to $N, '$t{C'{x", sender, target, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
        }
    }

    channel_fire_targeted_speech_hooks(sender, target, msg, TRIG_SAYTO, true);
}

static bool channel_deliver_intone_legacy(CHAR_DATA *sender,
                                          OBJ_DATA *target,
                                          const char *plain_text)
{
    if (!sender || !target || IS_NULLSTR(plain_text))
        return false;

    act("{C$n intones to $p '$t'{x", sender, NULL, NULL, target, NULL, (void *)plain_text, NULL, TO_ROOM, NULL, NULL);
    act("{CYou intone to $p '$t'{x", sender, NULL, NULL, target, NULL, (void *)plain_text, NULL, TO_CHAR, NULL, NULL);

    channel_fire_object_speech_hook(sender, target, plain_text);
    return true;
}

static bool channel_dispatch_legacy_by_id(CHAR_DATA *sender, const char *channel_id, const char *plain_text)
{
    if (!sender || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return false;

    if (!str_cmp(channel_id, "ooc")) {
        channel_deliver_ooc_legacy(sender, plain_text);
    } else if (!str_cmp(channel_id, "say")) {
        channel_deliver_say_legacy(sender, plain_text);
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

static bool channel_dispatch_targeted_room_legacy_by_id(CHAR_DATA *sender,
                                                        const char *channel_id,
                                                        CHAR_DATA *target,
                                                        const char *plain_text)
{
    if (!sender || !target || IS_NULLSTR(channel_id) || IS_NULLSTR(plain_text))
        return false;

    if (!str_cmp(channel_id, "whisper")) {
        channel_deliver_whisper_legacy(sender, target, plain_text);
        return true;
    }

    if (!str_cmp(channel_id, "sayto")) {
        channel_deliver_sayto_legacy(sender, target, plain_text);
        return true;
    }

    return false;
}

static void channel_service_receive_message(const CHANNEL_MESSAGE *msg)
{
    CHAR_DATA *sender;
    const CHANNEL_DEFINITION *def;

    if (!msg || IS_NULLSTR(msg->channel_id))
        return;

    if (IS_NULLSTR(msg->message_text))
        return;

    channel_history_append(msg->channel_id,
                           msg->topic,
                           msg->sender_name,
                           msg->message_text,
                           msg->timestamp,
                           msg->history_id,
                           msg->reports_json);

    sender = channel_find_sender(msg->sender_id0, msg->sender_id1, msg->sender_name);
    if (!sender)
        return;

    def = channel_find_definition(msg->channel_id);

    if (def && def->scope == CHANNEL_SCOPE_DIRECT_ENTITY) {
        /* Directed delivery: find specific recipient and deliver only to them. */
        CHAR_DATA *recipient = channel_find_sender(msg->recipient_id0, msg->recipient_id1, NULL);
        if (recipient)
            channel_deliver_tell_legacy(sender, recipient, msg->message_text);
        return;
    }

    if (!str_cmp(msg->channel_id, "whisper") || !str_cmp(msg->channel_id, "sayto")) {
        CHAR_DATA *target = channel_find_sender(msg->recipient_id0, msg->recipient_id1, NULL);
        if (target)
            channel_dispatch_targeted_room_legacy_by_id(sender, msg->channel_id, target, msg->message_text);
        return;
    }

    channel_dispatch_legacy_by_id(sender, msg->channel_id, msg->message_text);
}

bool channel_service_init(void)
{
    if (channel_service_ready)
        return true;

    /* Bootstrap channel definitions: load defaults then overlay from disk. */
    channel_registry_init();
    channel_registry_load(CHANNEL_REGISTRY_PATH);   /* optional; falls back to defaults */
    channel_registry_mark_loaded_mtime();

    if (!channel_transport_init()) {
        log_string("ChannelService: failed to initialize transport");
        return false;
    }

    channel_transport_set_inbound_handler(channel_service_receive_message);

    channel_subscription_last_sync = 0;
    channel_active_topic_count = 0;
    memset(channel_active_topics, 0, sizeof(channel_active_topics));
    channel_history_reset();
    channel_staff_report_reset();
    channel_history_load();
    channel_staff_report_load();
    channel_persist_last_flush = current_time;
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

    channel_service_flush_persistence(true);
    channel_transport_shutdown();
    channel_transport_set_inbound_handler(NULL);
    channel_history_reset();
    channel_staff_report_reset();
    channel_service_ready = false;
    log_string("ChannelService: shutdown");
}

void channel_service_pulse(void)
{
    if (!channel_service_ready)
        return;

    if (current_time > channel_subscription_last_sync) {
        channel_registry_reload_if_changed();
        channel_service_sync_subscriptions();
        channel_subscription_last_sync = current_time;
    }

    if (channel_persist_last_flush <= 0
        || current_time >= (channel_persist_last_flush + CHANNEL_PERSIST_FLUSH_INTERVAL)) {
        channel_service_flush_persistence(false);
    }

    channel_transport_pulse();
}

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text)
{
    const CHANNEL_DEFINITION *def;
    CHANNEL_MESSAGE msg;
    CHANNEL_FILTER_RESULT filter_result;
    const char *delivery_text;
    const char *source_text;
    char topic[128];
    char modified_text[MSL];
    char sender_uid[64];
    char review_id[64];
    char reports_json[256];

    if (!sender || IS_NULLSTR(channel_id) || IS_NULLSTR(raw_text))
        return false;

    reports_json[0] = '\0';

    def = channel_find_definition(channel_id);
    if (!def)
        return false;

    if (!channel_definition_available_for_sender(def, sender)) {
        send_to_char("You do not meet the requirements to use that channel.\n\r", sender);
        return true;
    }

    source_text = channel_apply_text_modifiers(sender, def, raw_text,
                                               modified_text, sizeof(modified_text));

    /* Shared sender revocation policy (channel penalties + tell-family compatibility fallback). */
    if (channel_policy_sender_revoked(sender, channel_id)) {
        send_to_char("You are currently muted on that channel.\n\r", sender);
        return true;
    }

    if (!channel_filter_evaluate(sender, channel_id, source_text, &filter_result))
        return false;

    delivery_text = source_text;
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
    msg.reports_json = NULL;
    msg.message_text = delivery_text;
    msg.timestamp = current_time;

    if (filter_result.queue_for_review) {
        const char *review_stream = IS_NULLSTR(def->review_stream) ? CHANNEL_REVIEW_STREAM : def->review_stream;

        if (!channel_review_queue_append(&msg,
                                         review_stream,
                                         filter_result.decision,
                                         filter_result.reason,
                                         raw_text,
                                         delivery_text,
                                         review_id,
                                         sizeof(review_id))) {
            log_stringf("ChannelService: review queue append failed for channel '%s'", channel_id);
        } else {
            snprintf(reports_json,
                     sizeof(reports_json),
                     "{\"count\":1,\"items\":[{\"id\":\"%s\",\"source\":\"filter\",\"reason\":\"%s\"}]}",
                     review_id,
                     IS_NULLSTR(filter_result.reason) ? "none" : filter_result.reason);
            msg.reports_json = reports_json;
            channel_notify_staff_report(channel_id,
                                        review_stream,
                                        review_id,
                                        filter_result.reason,
                                        sender->name,
                                        delivery_text);
        }
    }

    if (filter_result.decision == CHANNEL_FILTER_BLOCK) {
        send_to_char("Your message was blocked by channel filters.\n\r", sender);
        return true;
    }

    if (channel_supports_service_self_echo(channel_id))
        channel_send_formatted_to_sender(channel_id, sender, delivery_text);

    if (!channel_service_ready)
    {
        channel_history_append(channel_id,
                               NULL,
                               sender->name,
                               delivery_text,
                               current_time,
                               NULL,
                               msg.reports_json);
        return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);
    }

    if (!channel_build_topic_for_sender(def, sender, topic, sizeof(topic)))
        return false;

    msg.topic = topic;

    if (channel_transport_backend_mode() != CHANNEL_BACKEND_LEGACY_ITERATIVE) {
        if (channel_transport_publish(topic, &msg))
            return true;

        log_stringf("ChannelService: publish failed for channel '%s', applying local legacy fallback", channel_id);
        channel_history_append(channel_id,
                               topic,
                               sender->name,
                               delivery_text,
                               current_time,
                               NULL,
                               msg.reports_json);
        return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);
    }

    channel_history_append(channel_id,
                           topic,
                           sender->name,
                           delivery_text,
                           current_time,
                           NULL,
                           msg.reports_json);
    return channel_dispatch_legacy_by_id(sender, channel_id, delivery_text);
}

bool channel_service_channel_available_for_sender(CHAR_DATA *sender,
                                                  const CHANNEL_DEF_DATA *def)
{
    return channel_definition_available_for_sender(def, sender);
}

bool channel_service_send_directed(CHAR_DATA *sender, const char *channel_id,
                                   CHAR_DATA *recipient, const char *raw_text)
{
    const CHANNEL_DEFINITION *def;
    CHANNEL_MESSAGE msg;
    CHANNEL_FILTER_RESULT filter_result;
    const char *delivery_text;
    const char *source_text;
    char topic[128];
    char modified_text[MSL];
    char sender_uid[64];
    char recipient_uid[64];
    char review_id[64];
    char reports_json[256];

    if (!sender || IS_NULLSTR(channel_id) || !recipient || IS_NULLSTR(raw_text))
        return false;

    reports_json[0] = '\0';

    def = channel_find_definition(channel_id);
    if (!def || def->scope != CHANNEL_SCOPE_DIRECT_ENTITY)
        return false;

    if (!channel_definition_available_for_sender(def, sender)) {
        send_to_char("You do not meet the requirements to use that channel.\n\r", sender);
        return true;
    }

    source_text = channel_apply_text_modifiers(sender, def, raw_text,
                                               modified_text, sizeof(modified_text));

    /* Shared sender revocation policy (channel penalties + tell-family compatibility fallback). */
    if (channel_policy_sender_revoked(sender, channel_id)) {
        send_to_char("You are currently muted on that channel.\n\r", sender);
        return true;
    }

    if (!channel_filter_evaluate(sender, channel_id, source_text, &filter_result))
        return false;

    delivery_text = source_text;
    if (filter_result.decision == CHANNEL_FILTER_REDACT)
        delivery_text = filter_result.filtered_text;

    if (filter_result.queue_for_review) {
        memset(&msg, 0, sizeof(msg));
        msg.channel_id = channel_id;
        msg.sender_name = sender->name;
        snprintf(sender_uid, sizeof(sender_uid), "%lu:%lu", sender->id[0], sender->id[1]);
        msg.sender_uid = sender_uid;
        msg.sender_id0 = sender->id[0];
        msg.sender_id1 = sender->id[1];
        snprintf(recipient_uid, sizeof(recipient_uid), "%lu:%lu", recipient->id[0], recipient->id[1]);
        msg.recipient_uid = recipient_uid;
        msg.recipient_id0 = recipient->id[0];
        msg.recipient_id1 = recipient->id[1];
        msg.message_text = delivery_text;
        msg.timestamp = current_time;

        {
            const char *review_stream = IS_NULLSTR(def->review_stream) ? CHANNEL_REVIEW_STREAM : def->review_stream;

            if (!channel_review_queue_append(&msg,
                                             review_stream,
                                             filter_result.decision,
                                             filter_result.reason,
                                             raw_text,
                                             delivery_text,
                                             review_id,
                                             sizeof(review_id))) {
            log_stringf("ChannelService: review queue append failed for channel '%s'", channel_id);
        } else {
            snprintf(reports_json,
                     sizeof(reports_json),
                     "{\"count\":1,\"items\":[{\"id\":\"%s\",\"source\":\"filter\",\"reason\":\"%s\"}]}",
                     review_id,
                     IS_NULLSTR(filter_result.reason) ? "none" : filter_result.reason);
            msg.reports_json = reports_json;
                channel_notify_staff_report(channel_id,
                                            review_stream,
                                            review_id,
                                            filter_result.reason,
                                            sender->name,
                                            delivery_text);
            }
        }
    }

    if (filter_result.decision == CHANNEL_FILTER_BLOCK) {
        send_to_char("Your message was blocked by channel filters.\n\r", sender);
        return true;
    }

    /* Legacy or uninitialized: deliver directly without going through transport. */
    if (!channel_service_ready || channel_transport_backend_mode() == CHANNEL_BACKEND_LEGACY_ITERATIVE) {
        channel_history_append(channel_id,
                               NULL,
                               sender->name,
                               delivery_text,
                               current_time,
                               NULL,
                               msg.reports_json);
        channel_deliver_tell_legacy(sender, recipient, delivery_text);
        return true;
    }

    /* Build recipient entity topic for the pubsub publish. */
    if (!channel_build_entity_topic(recipient, topic, sizeof(topic)))
        return false;

    memset(&msg, 0, sizeof(msg));
    msg.channel_id = channel_id;
    msg.topic = topic;
    msg.sender_name = sender->name;
    snprintf(sender_uid, sizeof(sender_uid), "%lu:%lu", sender->id[0], sender->id[1]);
    msg.sender_uid = sender_uid;
    msg.sender_id0 = sender->id[0];
    msg.sender_id1 = sender->id[1];
    snprintf(recipient_uid, sizeof(recipient_uid), "%lu:%lu", recipient->id[0], recipient->id[1]);
    msg.recipient_uid = recipient_uid;
    msg.recipient_id0 = recipient->id[0];
    msg.recipient_id1 = recipient->id[1];
    if (!IS_NULLSTR(reports_json))
        msg.reports_json = reports_json;
    msg.message_text = delivery_text;
    msg.timestamp = current_time;

    if (channel_transport_publish(topic, &msg))
        return true;

    log_stringf("ChannelService: directed publish failed for channel '%s', using direct fallback", channel_id);
    channel_history_append(channel_id,
                           topic,
                           sender->name,
                           delivery_text,
                           current_time,
                           NULL,
                           msg.reports_json);
    channel_deliver_tell_legacy(sender, recipient, delivery_text);
    return true;
}

bool channel_service_send_room_targeted(CHAR_DATA *sender, const char *channel_id,
                                        CHAR_DATA *target, const char *raw_text)
{
    const CHANNEL_DEFINITION *def;
    CHANNEL_MESSAGE msg;
    CHANNEL_FILTER_RESULT filter_result;
    const char *delivery_text;
    const char *source_text;
    char topic[128];
    char modified_text[MSL];
    char sender_uid[64];
    char recipient_uid[64];
    char review_id[64];
    char reports_json[256];

    if (!sender || !target || IS_NULLSTR(channel_id) || IS_NULLSTR(raw_text))
        return false;

    reports_json[0] = '\0';

    def = channel_find_definition(channel_id);
    if (!def || def->scope != CHANNEL_SCOPE_ROOM_WV)
        return false;

    if (!channel_definition_available_for_sender(def, sender)) {
        send_to_char("You do not meet the requirements to use that channel.\n\r", sender);
        return true;
    }

    source_text = channel_apply_text_modifiers(sender, def, raw_text,
                                               modified_text, sizeof(modified_text));

    if (!channel_filter_evaluate(sender, channel_id, source_text, &filter_result))
        return false;

    delivery_text = source_text;
    if (filter_result.decision == CHANNEL_FILTER_REDACT)
        delivery_text = filter_result.filtered_text;

    if (filter_result.queue_for_review) {
        memset(&msg, 0, sizeof(msg));
        msg.channel_id = channel_id;
        msg.sender_name = sender->name;
        snprintf(sender_uid, sizeof(sender_uid), "%lu:%lu", sender->id[0], sender->id[1]);
        msg.sender_uid = sender_uid;
        msg.sender_id0 = sender->id[0];
        msg.sender_id1 = sender->id[1];
        snprintf(recipient_uid, sizeof(recipient_uid), "%lu:%lu", target->id[0], target->id[1]);
        msg.recipient_uid = recipient_uid;
        msg.recipient_id0 = target->id[0];
        msg.recipient_id1 = target->id[1];
        msg.message_text = delivery_text;
        msg.timestamp = current_time;

        {
            const char *review_stream = IS_NULLSTR(def->review_stream) ? CHANNEL_REVIEW_STREAM : def->review_stream;

            if (!channel_review_queue_append(&msg,
                                             review_stream,
                                             filter_result.decision,
                                             filter_result.reason,
                                             raw_text,
                                             delivery_text,
                                             review_id,
                                             sizeof(review_id))) {
            log_stringf("ChannelService: review queue append failed for channel '%s'", channel_id);
        } else {
            snprintf(reports_json,
                     sizeof(reports_json),
                     "{\"count\":1,\"items\":[{\"id\":\"%s\",\"source\":\"filter\",\"reason\":\"%s\"}]}",
                     review_id,
                     IS_NULLSTR(filter_result.reason) ? "none" : filter_result.reason);
            msg.reports_json = reports_json;
                channel_notify_staff_report(channel_id,
                                            review_stream,
                                            review_id,
                                            filter_result.reason,
                                            sender->name,
                                            delivery_text);
            }
        }
    }

    if (filter_result.decision == CHANNEL_FILTER_BLOCK) {
        send_to_char("Your message was blocked by channel filters.\n\r", sender);
        return true;
    }

    if (!channel_service_ready || channel_transport_backend_mode() == CHANNEL_BACKEND_LEGACY_ITERATIVE) {
        channel_history_append(channel_id,
                               NULL,
                               sender->name,
                               delivery_text,
                               current_time,
                               NULL,
                               msg.reports_json);
        return channel_dispatch_targeted_room_legacy_by_id(sender, channel_id, target, delivery_text);
    }

    if (!channel_build_topic_for_sender(def, sender, topic, sizeof(topic)))
        return false;

    memset(&msg, 0, sizeof(msg));
    msg.channel_id = channel_id;
    msg.topic = topic;
    msg.sender_name = sender->name;
    snprintf(sender_uid, sizeof(sender_uid), "%lu:%lu", sender->id[0], sender->id[1]);
    msg.sender_uid = sender_uid;
    msg.sender_id0 = sender->id[0];
    msg.sender_id1 = sender->id[1];
    snprintf(recipient_uid, sizeof(recipient_uid), "%lu:%lu", target->id[0], target->id[1]);
    msg.recipient_uid = recipient_uid;
    msg.recipient_id0 = target->id[0];
    msg.recipient_id1 = target->id[1];
    if (!IS_NULLSTR(reports_json))
        msg.reports_json = reports_json;
    msg.message_text = delivery_text;
    msg.timestamp = current_time;

    if (channel_transport_publish(topic, &msg))
        return true;

    log_stringf("ChannelService: targeted room publish failed for channel '%s', using direct fallback",
                channel_id);
    channel_history_append(channel_id,
                           topic,
                           sender->name,
                           delivery_text,
                           current_time,
                           NULL,
                           msg.reports_json);
    return channel_dispatch_targeted_room_legacy_by_id(sender, channel_id, target, delivery_text);
}

bool channel_service_send_object_targeted(CHAR_DATA *sender,
                                          const char *channel_id,
                                          OBJ_DATA *target,
                                          const char *raw_text)
{
    const CHANNEL_DEFINITION *def;
    CHANNEL_FILTER_RESULT filter_result;
    CHANNEL_MESSAGE msg;
    const char *delivery_text;
    char review_id[64];
    char reports_json[256];

    if (!sender || !target || IS_NULLSTR(channel_id) || IS_NULLSTR(raw_text))
        return false;

    if (str_cmp(channel_id, "intone"))
        return false;

    reports_json[0] = '\0';
    def = channel_find_definition(channel_id);

    if (channel_policy_global_revoked(sender)) {
        send_to_char("You cannot use channels.\n\r", sender);
        return true;
    }

    if (channel_policy_sender_revoked(sender, channel_id)) {
        send_to_char("You are currently muted on that channel.\n\r", sender);
        return true;
    }

    if (def && !channel_definition_available_for_sender(def, sender)) {
        send_to_char("You do not meet the requirements to use that channel.\n\r", sender);
        return true;
    }

    if (!channel_filter_evaluate(sender, channel_id, raw_text, &filter_result))
        return false;

    delivery_text = raw_text;
    if (filter_result.decision == CHANNEL_FILTER_REDACT)
        delivery_text = filter_result.filtered_text;

    if (filter_result.queue_for_review) {
        const char *review_stream = (def && !IS_NULLSTR(def->review_stream))
            ? def->review_stream
            : CHANNEL_REVIEW_STREAM;

        memset(&msg, 0, sizeof(msg));
        msg.channel_id = channel_id;
        msg.sender_name = sender->name;
        msg.sender_id0 = sender->id[0];
        msg.sender_id1 = sender->id[1];
        msg.message_text = delivery_text;
        msg.timestamp = current_time;

        if (channel_review_queue_append(&msg,
                                        review_stream,
                                        filter_result.decision,
                                        filter_result.reason,
                                        raw_text,
                                        delivery_text,
                                        review_id,
                                        sizeof(review_id))) {
            snprintf(reports_json,
                     sizeof(reports_json),
                     "{\"count\":1,\"items\":[{\"id\":\"%s\",\"source\":\"filter\",\"reason\":\"%s\"}]}",
                     review_id,
                     IS_NULLSTR(filter_result.reason) ? "none" : filter_result.reason);
            channel_notify_staff_report(channel_id,
                                        review_stream,
                                        review_id,
                                        filter_result.reason,
                                        sender->name,
                                        delivery_text);
        }
    }

    if (filter_result.decision == CHANNEL_FILTER_BLOCK) {
        send_to_char("Your message was blocked by channel filters.\n\r", sender);
        return true;
    }

    return channel_deliver_intone_legacy(sender, target, delivery_text);
}

int channel_service_history_recent(CHAR_DATA *viewer,
                                   const char *channel_id,
                                   int limit,
                                   CHANNEL_HISTORY_ENTRY *out_entries,
                                   int out_cap)
{
    const CHANNEL_DEF_DATA *def;
    int i;
    int found = 0;

    if (IS_NULLSTR(channel_id) || !out_entries || out_cap <= 0 || limit <= 0)
        return 0;

    if (limit > out_cap)
        limit = out_cap;

    def = channel_find_definition(channel_id);
    if (!def)
        return 0;

    for (i = 0; i < channel_history_ring_count && found < limit; i++) {
        int ring_index = (channel_history_ring_next - 1 - i + CHANNEL_HISTORY_RING_MAX) % CHANNEL_HISTORY_RING_MAX;
        CHANNEL_HISTORY_RECORD *record = &channel_history_ring[ring_index];

        if (!channel_history_match_channel(record, channel_id))
            continue;

        if (!channel_history_matches_context(record, def, viewer))
            continue;

        channel_history_copy_entry(&out_entries[found], record);
        found++;
    }

    return found;
}

bool channel_service_history_by_index(CHAR_DATA *viewer,
                                      const char *channel_id,
                                      int index,
                                      CHANNEL_HISTORY_ENTRY *out_entry)
{
    const CHANNEL_DEF_DATA *def;
    int i;
    int seen = 0;

    if (IS_NULLSTR(channel_id) || !out_entry || index <= 0)
        return false;

    def = channel_find_definition(channel_id);
    if (!def)
        return false;

    for (i = 0; i < channel_history_ring_count; i++) {
        int ring_index = (channel_history_ring_next - 1 - i + CHANNEL_HISTORY_RING_MAX) % CHANNEL_HISTORY_RING_MAX;
        CHANNEL_HISTORY_RECORD *record = &channel_history_ring[ring_index];

        if (!channel_history_match_channel(record, channel_id))
            continue;

        if (!channel_history_matches_context(record, def, viewer))
            continue;

        seen++;
        if (seen == index) {
            channel_history_copy_entry(out_entry, record);
            return true;
        }
    }

    return false;
}

bool channel_service_history_by_report_id(CHAR_DATA *viewer,
                                          const char *channel_id,
                                          const char *report_id,
                                          CHANNEL_HISTORY_ENTRY *out_entry)
{
    const CHANNEL_DEF_DATA *def;
    int ring_index;

    if (!out_entry)
        return false;

    def = channel_find_definition(channel_id);
    if (!def)
        return false;

    if (!channel_history_find_record_by_report_id(channel_id, report_id, &ring_index))
        return false;

    if (!channel_history_matches_context(&channel_history_ring[ring_index], def, viewer))
        return false;

    channel_history_copy_entry(out_entry, &channel_history_ring[ring_index]);
    return true;
}

static bool channel_report_context_matches(const CHANNEL_HISTORY_RECORD *record,
                                          const CHANNEL_HISTORY_RECORD *target,
                                          const CHANNEL_DEF_DATA *def,
                                          const char *channel_id)
{
    if (!record || !target || !def || IS_NULLSTR(channel_id))
        return false;

    if (!channel_history_match_channel(record, channel_id))
        return false;

    if (def->scope != CHANNEL_SCOPE_DIRECT_ENTITY)
        return true;

    if (IS_NULLSTR(target->topic))
        return true;

    return !str_cmp(record->topic, target->topic);
}

bool channel_service_report_message(CHAR_DATA *reporter,
                                    const char *channel_id,
                                    const char *report_id,
                                    const char *notes)
{
    const CHANNEL_DEF_DATA *def;
    const char *review_stream;
    int target_ring_index;
    CHANNEL_HISTORY_RECORD *target;
    CHANNEL_MESSAGE report_msg;
    char payload[MSL];
    char context[MSL];
    char reporter_uid[64];
    char stream_id[64];
    int collected = 0;
    int scan;

    if (!reporter || IS_NULLSTR(channel_id) || IS_NULLSTR(report_id))
        return false;

    def = channel_find_definition(channel_id);
    review_stream = (!def || IS_NULLSTR(def->review_stream)) ? CHANNEL_REVIEW_STREAM : def->review_stream;

    if (!channel_history_find_record_by_report_id(channel_id, report_id, &target_ring_index))
        return false;

    target = &channel_history_ring[target_ring_index];
    context[0] = '\0';

    scan = target_ring_index;
    while (collected < 10) {
        CHANNEL_HISTORY_RECORD *record;
        char line[256];

        scan = (scan - 1 + CHANNEL_HISTORY_RING_MAX) % CHANNEL_HISTORY_RING_MAX;
        if (scan == target_ring_index)
            break;

        record = &channel_history_ring[scan];
        if (!channel_report_context_matches(record, target, def, channel_id))
            continue;

        snprintf(line,
                 sizeof(line),
                 "[%.63s] %.63s: %.120s || ",
                 record->report_id,
                 record->sender_name,
                 record->message_text);
        strlcat(context, line, sizeof(context));
        collected++;
    }

    memset(&report_msg, 0, sizeof(report_msg));
    report_msg.channel_id = "player_report";
    report_msg.topic = review_stream;
    report_msg.sender_name = reporter->name;
    snprintf(reporter_uid, sizeof(reporter_uid), "%lu:%lu", reporter->id[0], reporter->id[1]);
    report_msg.sender_uid = reporter_uid;
    report_msg.sender_id0 = reporter->id[0];
    report_msg.sender_id1 = reporter->id[1];
    report_msg.timestamp = current_time;

    snprintf(payload,
             sizeof(payload),
             "schema=%d kind=player_report channel=%.31s target_id=%.63s target_sender=%.63s target_text=\"%.512s\" notes=\"%.1024s\" context_prev_count=%d context_prev=\"%.2000s\"",
             CHANNEL_REVIEW_SCHEMA_VERSION,
             channel_id,
             target->report_id,
             target->sender_name,
             target->message_text,
             IS_NULLSTR(notes) ? "(none)" : notes,
             collected,
             context);
    report_msg.message_text = payload;

    if (!channel_transport_append_history(review_stream,
                                          &report_msg,
                                          stream_id,
                                          sizeof(stream_id))) {
        return false;
    }

    channel_history_append_report_meta(target,
                                       stream_id,
                                       "player",
                                       reporter->name);

    channel_notify_staff_report(channel_id,
                                review_stream,
                                stream_id,
                                "player_report",
                                reporter->name,
                                payload);

    return true;
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

    for (i = 0; i < channel_registry_count(); i++) {
        char topic[128];
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);

        if (def &&
            channel_definition_available_for_recipient(def, ch) &&
            channel_build_topic_for_sender(def, ch, topic, sizeof(topic)))
            channel_topic_list_add_ref(topics, &topic_count, topic);
    }

    for (i = 0; i < topic_count; i++) {
        if (i > 0)
            strlcat(out, ", ", out_size);
        strlcat(out, topics[i].topic, out_size);
    }

    return topic_count;
}
