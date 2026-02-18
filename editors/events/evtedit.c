#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../io/json/json_common.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef struct evtedit_data EVTEDIT_DATA;
typedef struct event_instance EVENT_INSTANCE;
typedef struct event_part EVENT_PART;

struct evtedit_data {
    long uid;
    char *name;
    char *description;
    char *announce_msg;
    char *end_msg;
    char *join_msg;
    int16_t event_type;
    int16_t scope_type;
    int16_t sched_type;
    int16_t sched_interval;
    int16_t sched_variance;
    int16_t sched_duration;
    int16_t sched_cooldown;
    int16_t min_level;
    int16_t max_level;
    int16_t min_players;
    int16_t max_players;
    bool enabled;
    long flags;
    char *comments;

    time_t scheduled_time;
    time_t cooldown_until;
    time_t next_auto_time;

    EVTEDIT_DATA *next;
};

struct event_instance {
    uint32_t instance_id;
    EVTEDIT_DATA *def;
    int state;
    time_t started_at;
    time_t end_time;
    EVENT_PART *participants;
    int participant_count;
    bool dirty;
    EVENT_INSTANCE *next;
};

struct event_part {
    EVENT_PART *next;
    EVENT_INSTANCE *inst;
    CHAR_DATA *ch;
    int kills;
    int items_turned;
    int team;
};

enum {
    EVT_TYPE_COLLECTION = 0,
    EVT_TYPE_INVASION,
    EVT_TYPE_BOSS,
    EVT_TYPE_WAR_FFA,
    EVT_TYPE_WAR_GENOCIDE,
    EVT_TYPE_WAR_JIHAD,
    EVT_TYPE_WORLDSTATE,
    EVT_TYPE_CUSTOM,
};

enum {
    EVT_SCOPE_GLOBAL = 0,
    EVT_SCOPE_AREA,
    EVT_SCOPE_REGION,
    EVT_SCOPE_ZONES,
    EVT_SCOPE_BATTLEFIELD,
};

enum {
    EVT_SCHED_MANUAL = 0,
    EVT_SCHED_RECURRING,
    EVT_SCHED_CALENDAR,
    EVT_SCHED_WORLDCONDITION,
    EVT_SCHED_TRIGGERED,
};

enum {
    EVT_FLAG_WINNER_ONLY      = (A),
    EVT_FLAG_ALL_PARTS        = (B),
    EVT_FLAG_TOP3             = (C),
    EVT_FLAG_NOANNOUNCE       = (D),
    EVT_FLAG_JOINLATE         = (E),
    EVT_FLAG_EXCLUSIVE_PLAYER = (F),
    EVT_FLAG_UNIQUE_GLOBAL    = (G),
    EVT_FLAG_SCALING          = (H),
    EVT_FLAG_REPEATABLE       = (I),
    EVT_FLAG_PASSIVE          = (J),
};

enum {
    EVTS_PENDING = 0,
    EVTS_ACTIVE,
    EVTS_COMPLETE,
    EVTS_CANCELLED,
};

EVTEDIT(evtedit_list);
EVTEDIT(evtedit_create);
EVTEDIT(evtedit_delete);
EVTEDIT(evtedit_show);
EVTEDIT(evtedit_name);
EVTEDIT(evtedit_description);
EVTEDIT(evtedit_announce);
EVTEDIT(evtedit_endmsg);
EVTEDIT(evtedit_joinmsg);
EVTEDIT(evtedit_type);
EVTEDIT(evtedit_enabled);
EVTEDIT(evtedit_scope);
EVTEDIT(evtedit_schedule);
EVTEDIT(evtedit_interval);
EVTEDIT(evtedit_variance);
EVTEDIT(evtedit_duration);
EVTEDIT(evtedit_cooldown);
EVTEDIT(evtedit_minlevel);
EVTEDIT(evtedit_maxlevel);
EVTEDIT(evtedit_minplayers);
EVTEDIT(evtedit_maxplayers);
EVTEDIT(evtedit_flags);
EVTEDIT(evtedit_comments);
EVTEDIT(evtedit_save);
EVTEDIT(evtedit_reload);

static EVTEDIT_DATA *evtedit_list_head = NULL;
static EVTEDIT_DATA *evtedit_list_tail = NULL;
static long evtedit_next_uid = 1;
static bool evtedit_booted = false;
static EVENT_INSTANCE *event_active_head = NULL;
static uint32_t event_next_instance_id = 1;
static bool event_system_enabled = true;

#define EVTEDIT_JSON_FILE SYSTEM_DIR "events.json"
#define EVTEDIT_JSON_FORMAT "events"
#define EVTEDIT_JSON_VERSION 1

static const struct flag_type evt_type_flags[] = {
    { "collection", EVT_TYPE_COLLECTION, true, NULL },
    { "invasion", EVT_TYPE_INVASION, true, NULL },
    { "boss", EVT_TYPE_BOSS, true, NULL },
    { "war-ffa", EVT_TYPE_WAR_FFA, true, NULL },
    { "war-genocide", EVT_TYPE_WAR_GENOCIDE, true, NULL },
    { "war-jihad", EVT_TYPE_WAR_JIHAD, true, NULL },
    { "worldstate", EVT_TYPE_WORLDSTATE, true, NULL },
    { "custom", EVT_TYPE_CUSTOM, true, NULL },
    { NULL, 0, false, NULL },
};

static const struct flag_type evt_scope_flags[] = {
    { "global", EVT_SCOPE_GLOBAL, true, NULL },
    { "area", EVT_SCOPE_AREA, true, NULL },
    { "region", EVT_SCOPE_REGION, true, NULL },
    { "zones", EVT_SCOPE_ZONES, true, NULL },
    { "battlefield", EVT_SCOPE_BATTLEFIELD, true, NULL },
    { NULL, 0, false, NULL },
};

static const struct flag_type evt_sched_flags[] = {
    { "manual", EVT_SCHED_MANUAL, true, NULL },
    { "recurring", EVT_SCHED_RECURRING, true, NULL },
    { "calendar", EVT_SCHED_CALENDAR, true, NULL },
    { "worldcondition", EVT_SCHED_WORLDCONDITION, true, NULL },
    { "triggered", EVT_SCHED_TRIGGERED, true, NULL },
    { NULL, 0, false, NULL },
};

static const struct flag_type evt_flags[] = {
    { "winner_only", EVT_FLAG_WINNER_ONLY, true, NULL },
    { "all_parts", EVT_FLAG_ALL_PARTS, true, NULL },
    { "top3", EVT_FLAG_TOP3, true, NULL },
    { "noannounce", EVT_FLAG_NOANNOUNCE, true, NULL },
    { "joinlate", EVT_FLAG_JOINLATE, true, NULL },
    { "exclusive_player", EVT_FLAG_EXCLUSIVE_PLAYER, true, NULL },
    { "unique_global", EVT_FLAG_UNIQUE_GLOBAL, true, NULL },
    { "scaling", EVT_FLAG_SCALING, true, NULL },
    { "repeatable", EVT_FLAG_REPEATABLE, true, NULL },
    { "passive", EVT_FLAG_PASSIVE, true, NULL },
    { NULL, 0, false, NULL },
};

static void evtedit_show_identity_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_schedule_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_messages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_meta_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_ensure_loaded(void);
static void event_broadcast(const char *message);

static void evtedit_free_item(EVTEDIT_DATA *evt)
{
    if (!evt)
        return;

    free_string(evt->name);
    free_string(evt->description);
    free_string(evt->announce_msg);
    free_string(evt->end_msg);
    free_string(evt->join_msg);
    free_string(evt->comments);
    free_mem(evt, sizeof(*evt));
}

static void evtedit_clear_all(void)
{
    EVTEDIT_DATA *evt = evtedit_list_head;
    EVTEDIT_DATA *next;

    while (evt) {
        next = evt->next;
        evtedit_free_item(evt);
        evt = next;
    }

    evtedit_list_head = NULL;
    evtedit_list_tail = NULL;
    evtedit_next_uid = 1;
}

static EVTEDIT_DATA *evtedit_new(const char *name)
{
    EVTEDIT_DATA *evt = alloc_mem(sizeof(*evt));
    memset(evt, 0, sizeof(*evt));

    evt->uid = evtedit_next_uid++;
    evt->name = str_dup(name ? name : "event");
    evt->description = str_dup("");
    evt->announce_msg = str_dup("");
    evt->end_msg = str_dup("");
    evt->join_msg = str_dup("");
    evt->event_type = EVT_TYPE_COLLECTION;
    evt->scope_type = EVT_SCOPE_GLOBAL;
    evt->sched_type = EVT_SCHED_MANUAL;
    evt->sched_interval = 60;
    evt->sched_variance = 0;
    evt->sched_duration = 60;
    evt->sched_cooldown = 0;
    evt->min_level = 0;
    evt->max_level = 0;
    evt->min_players = 0;
    evt->max_players = 0;
    evt->enabled = true;
    evt->flags = 0;
    evt->comments = str_dup("");

    if (!evtedit_list_head)
        evtedit_list_head = evt;
    else
        evtedit_list_tail->next = evt;
    evtedit_list_tail = evt;

    return evt;
}

static EVTEDIT_DATA *evtedit_find_uid(long uid)
{
    EVTEDIT_DATA *evt;

    for (evt = evtedit_list_head; evt; evt = evt->next)
        if (evt->uid == uid)
            return evt;

    return NULL;
}

static EVTEDIT_DATA *evtedit_find_name(const char *name)
{
    EVTEDIT_DATA *evt;

    if (IS_NULLSTR(name))
        return NULL;

    for (evt = evtedit_list_head; evt; evt = evt->next)
        if (!str_cmp(evt->name, name))
            return evt;

    return NULL;
}

static EVENT_INSTANCE *event_find_active_def(const EVTEDIT_DATA *evt)
{
    EVENT_INSTANCE *it;

    if (!evt)
        return NULL;

    for (it = event_active_head; it; it = it->next)
        if (it->def == evt)
            return it;

    return NULL;
}

static EVENT_PART *event_find_participant(EVENT_INSTANCE *inst, CHAR_DATA *ch)
{
    EVENT_PART *part;

    if (!inst || !ch)
        return NULL;

    for (part = inst->participants; part; part = part->next)
        if (part->ch == ch)
            return part;

    return NULL;
}

static bool event_player_in_exclusive(CHAR_DATA *ch)
{
    EVENT_INSTANCE *inst;

    if (!ch)
        return false;

    for (inst = event_active_head; inst; inst = inst->next)
        if (IS_SET(inst->def->flags, EVT_FLAG_EXCLUSIVE_PLAYER)
            && event_find_participant(inst, ch))
            return true;

    return false;
}

static int event_roll_variance(int variance)
{
    if (variance <= 0)
        return 0;

    return number_range(-variance, variance);
}

static void event_set_next_recurring(EVTEDIT_DATA *evt)
{
    int minutes;

    if (!evt || evt->sched_interval <= 0)
        return;

    minutes = evt->sched_interval + event_roll_variance(evt->sched_variance);
    if (minutes < 1)
        minutes = 1;

    evt->next_auto_time = current_time + (minutes * 60);
}

static bool event_add_participant(EVENT_INSTANCE *inst, CHAR_DATA *ch)
{
    EVENT_PART *part;

    if (!inst || !ch)
        return false;

    if (event_find_participant(inst, ch))
        return false;

    if (inst->def->max_players > 0 && inst->participant_count >= inst->def->max_players)
        return false;

    if (inst->def->min_level > 0 && ch->tot_level < inst->def->min_level)
        return false;

    if (inst->def->max_level > 0 && ch->tot_level > inst->def->max_level)
        return false;

    if (IS_SET(inst->def->flags, EVT_FLAG_EXCLUSIVE_PLAYER) && event_player_in_exclusive(ch))
        return false;

    part = alloc_mem(sizeof(*part));
    memset(part, 0, sizeof(*part));
    part->inst = inst;
    part->ch = ch;
    part->next = inst->participants;
    inst->participants = part;
    inst->participant_count++;
    inst->dirty = true;

    if (!IS_NULLSTR(inst->def->join_msg))
        act(inst->def->join_msg, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, POS_DEAD, NULL);

    return true;
}

static bool event_remove_participant(EVENT_INSTANCE *inst, CHAR_DATA *ch)
{
    EVENT_PART *part;
    EVENT_PART *prev = NULL;

    if (!inst || !ch)
        return false;

    for (part = inst->participants; part; prev = part, part = part->next) {
        if (part->ch != ch)
            continue;

        if (prev)
            prev->next = part->next;
        else
            inst->participants = part->next;

        free_mem(part, sizeof(*part));
        inst->participant_count--;
        inst->dirty = true;
        return true;
    }

    return false;
}

static void event_clear_participants(EVENT_INSTANCE *inst)
{
    EVENT_PART *part;
    EVENT_PART *next;

    if (!inst)
        return;

    for (part = inst->participants; part; part = next) {
        next = part->next;
        free_mem(part, sizeof(*part));
    }

    inst->participants = NULL;
    inst->participant_count = 0;
}

static EVENT_INSTANCE *event_start_definition(EVTEDIT_DATA *evt)
{
    EVENT_INSTANCE *inst;

    if (!evt || !event_system_enabled || !evt->enabled)
        return NULL;

    if (IS_SET(evt->flags, EVT_FLAG_UNIQUE_GLOBAL) && event_find_active_def(evt))
        return NULL;

    inst = alloc_mem(sizeof(*inst));
    memset(inst, 0, sizeof(*inst));
    inst->instance_id = event_next_instance_id++;
    inst->def = evt;
    inst->state = EVTS_ACTIVE;
    inst->started_at = current_time;
    inst->end_time = evt->sched_duration > 0
        ? current_time + (evt->sched_duration * 60)
        : 0;
    inst->dirty = true;
    inst->next = event_active_head;
    event_active_head = inst;

    if (evt->sched_cooldown > 0)
        evt->cooldown_until = current_time + (evt->sched_cooldown * 60);
    else
        evt->cooldown_until = current_time;

    if (evt->sched_type == EVT_SCHED_RECURRING)
        event_set_next_recurring(evt);

    return inst;
}

static bool event_stop_definition(EVTEDIT_DATA *evt)
{
    EVENT_INSTANCE *it;
    EVENT_INSTANCE *prev = NULL;

    if (!evt)
        return false;

    for (it = event_active_head; it; prev = it, it = it->next) {
        if (it->def != evt)
            continue;

        if (prev)
            prev->next = it->next;
        else
            event_active_head = it->next;

        event_clear_participants(it);
        free_mem(it, sizeof(*it));

        if (evt->sched_cooldown > 0)
            evt->cooldown_until = current_time + (evt->sched_cooldown * 60);
        else
            evt->cooldown_until = current_time;

        if (evt->sched_type == EVT_SCHED_RECURRING)
            event_set_next_recurring(evt);

        return true;
    }

    return false;
}

static void event_stop_all(bool announce_end)
{
    while (event_active_head) {
        EVTEDIT_DATA *evt = event_active_head->def;

        if (!evt)
            break;

        if (announce_end && !IS_NULLSTR(evt->end_msg))
            event_broadcast(evt->end_msg);

        if (!event_stop_definition(evt))
            break;
    }
}

static EVENT_INSTANCE *event_find_joinable_for(CHAR_DATA *ch)
{
    EVENT_INSTANCE *inst;

    if (!ch)
        return NULL;

    for (inst = event_active_head; inst; inst = inst->next) {
        EVTEDIT_DATA *evt = inst->def;

        if (!evt || inst->state != EVTS_ACTIVE)
            continue;
        if (!evt->enabled)
            continue;
        if (IS_SET(evt->flags, EVT_FLAG_PASSIVE) || evt->event_type == EVT_TYPE_WORLDSTATE)
            continue;
        if (evt->min_level > 0 && ch->tot_level < evt->min_level)
            continue;
        if (evt->max_level > 0 && ch->tot_level > evt->max_level)
            continue;

        return inst;
    }

    return NULL;
}

static bool event_parse_when(const char *when, time_t *out_when)
{
    int amount;
    char unit;
    struct tm tm_time;
    int year;
    int month;
    int day;
    int hour;
    int minute;

    if (!out_when || IS_NULLSTR(when))
        return false;

    if (sscanf(when, "+%d%c", &amount, &unit) == 2 && amount >= 0) {
        long delta = 0;

        switch (LOWER(unit)) {
        case 'm': delta = amount * 60L; break;
        case 'h': delta = amount * 3600L; break;
        case 'd': delta = amount * 86400L; break;
        default: return false;
        }

        *out_when = current_time + delta;
        return true;
    }

    if (sscanf(when, "%d-%d-%d %d:%d", &year, &month, &day, &hour, &minute) == 5) {
        memset(&tm_time, 0, sizeof(tm_time));
        tm_time.tm_year = year - 1900;
        tm_time.tm_mon = month - 1;
        tm_time.tm_mday = day;
        tm_time.tm_hour = hour;
        tm_time.tm_min = minute;
        tm_time.tm_sec = 0;
        tm_time.tm_isdst = -1;
        *out_when = mktime(&tm_time);
        return *out_when != (time_t)-1;
    }

    return false;
}

static void event_broadcast(const char *message)
{
    DESCRIPTOR_DATA *d;

    if (IS_NULLSTR(message))
        return;

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *to = d->character;

        if (!to || d->connected != CON_PLAYING || IS_NPC(to))
            continue;

        if (IS_SET(to->comm, COMM_NOAUTOWAR))
            continue;

        send_to_char(message, to);
        if (message[strlen(message) - 1] != '\n')
            send_to_char("\n\r", to);
    }
}

static EVTEDIT_DATA *event_lookup_definition(const char *token)
{
    EVTEDIT_DATA *evt;

    if (IS_NULLSTR(token))
        return NULL;

    if (is_number(token))
        return evtedit_find_uid(atol(token));

    if (!str_cmp(token, "gq") || !str_cmp(token, "globalquest")) {
        for (evt = evtedit_list_head; evt; evt = evt->next)
            if (evt->event_type == EVT_TYPE_COLLECTION)
                return evt;
    }

    if (!str_cmp(token, "invasion")) {
        for (evt = evtedit_list_head; evt; evt = evt->next)
            if (evt->event_type == EVT_TYPE_INVASION)
                return evt;
    }

    if (!str_cmp(token, "autowar") || !str_cmp(token, "war")) {
        for (evt = evtedit_list_head; evt; evt = evt->next)
            if (evt->event_type == EVT_TYPE_WAR_FFA
                || evt->event_type == EVT_TYPE_WAR_GENOCIDE
                || evt->event_type == EVT_TYPE_WAR_JIHAD)
                return evt;
    }

    return evtedit_find_name(token);
}

static bool event_user_has_staff_control(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch))
        return false;

    return get_staff_rank(ch) >= STAFF_CREATOR;
}

void event_runtime_update(void)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;
    EVENT_INSTANCE *next;

    evtedit_ensure_loaded();

    if (!event_system_enabled)
        return;

    for (evt = evtedit_list_head; evt; evt = evt->next) {
        if (!evt->enabled)
            continue;

        if (evt->scheduled_time > 0
            && evt->scheduled_time <= current_time
            && !event_find_active_def(evt)
            && evt->cooldown_until <= current_time) {
            if (event_start_definition(evt)
                && !IS_SET(evt->flags, EVT_FLAG_NOANNOUNCE)
                && !IS_NULLSTR(evt->announce_msg))
                event_broadcast(evt->announce_msg);
            evt->scheduled_time = 0;
            continue;
        }

        if (evt->sched_type != EVT_SCHED_RECURRING)
            continue;

        if (evt->sched_interval <= 0)
            continue;

        if (evt->next_auto_time == 0)
            event_set_next_recurring(evt);

        if (current_time < evt->next_auto_time)
            continue;

        if (event_find_active_def(evt))
            continue;

        if (evt->cooldown_until > current_time)
            continue;

        if (event_start_definition(evt)
            && !IS_SET(evt->flags, EVT_FLAG_NOANNOUNCE)
            && !IS_NULLSTR(evt->announce_msg))
            event_broadcast(evt->announce_msg);
    }

    for (inst = event_active_head; inst; inst = next) {
        EVTEDIT_DATA *def;

        next = inst->next;
        def = inst->def;

        if (!def)
            continue;

        if (inst->end_time > 0 && current_time >= inst->end_time) {
            if (!IS_NULLSTR(def->end_msg))
                event_broadcast(def->end_msg);
            event_stop_definition(def);
        }
    }
}

static json_t *evtedit_item_to_json(const EVTEDIT_DATA *evt)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "uid", json_integer(evt->uid));
    json_object_set_new(obj, "name", json_string_safe(evt->name));
    json_object_set_new(obj, "description", json_string_safe(evt->description));
    json_object_set_new(obj, "announce_msg", json_string_safe(evt->announce_msg));
    json_object_set_new(obj, "end_msg", json_string_safe(evt->end_msg));
    json_object_set_new(obj, "join_msg", json_string_safe(evt->join_msg));
    json_object_set_new(obj, "event_type", json_integer(evt->event_type));
    json_object_set_new(obj, "scope_type", json_integer(evt->scope_type));
    json_object_set_new(obj, "sched_type", json_integer(evt->sched_type));
    json_object_set_new(obj, "sched_interval", json_integer(evt->sched_interval));
    json_object_set_new(obj, "sched_variance", json_integer(evt->sched_variance));
    json_object_set_new(obj, "sched_duration", json_integer(evt->sched_duration));
    json_object_set_new(obj, "sched_cooldown", json_integer(evt->sched_cooldown));
    json_object_set_new(obj, "min_level", json_integer(evt->min_level));
    json_object_set_new(obj, "max_level", json_integer(evt->max_level));
    json_object_set_new(obj, "min_players", json_integer(evt->min_players));
    json_object_set_new(obj, "max_players", json_integer(evt->max_players));
    json_object_set_new(obj, "enabled", json_integer(evt->enabled ? 1 : 0));
    json_object_set_new(obj, "flags", json_integer(evt->flags));
    json_object_set_new(obj, "comments", json_string_safe(evt->comments));

    return obj;
}

static bool evtedit_save_to_json(void)
{
    json_t *root = json_object();
    json_t *events = json_array();
    EVTEDIT_DATA *evt;

    json_object_set_new(root, "_format", json_string(EVTEDIT_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(EVTEDIT_JSON_VERSION));
    json_object_set_new(root, "next_uid", json_integer(evtedit_next_uid));
    json_object_set_new(root, "events_enabled", json_integer(event_system_enabled ? 1 : 0));

    for (evt = evtedit_list_head; evt; evt = evt->next)
        json_array_append_new(events, evtedit_item_to_json(evt));

    json_object_set_new(root, "events", events);

    return json_file_save(root, EVTEDIT_JSON_FILE, "evtedit_save_to_json",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}

static bool evtedit_load_from_json(void)
{
    json_t *root;
    json_t *events = NULL;
    size_t i;
    json_t *entry;
    long max_uid = 0;

    root = json_file_load(EVTEDIT_JSON_FILE, "events", &events,
        "evtedit_load_from_json");
    if (!root)
        return false;

    if (!json_is_array(events)) {
        json_decref(root);
        return false;
    }

    evtedit_clear_all();

    json_array_foreach(events, i, entry) {
        EVTEDIT_DATA *evt;

        if (!json_is_object(entry))
            continue;

        evt = alloc_mem(sizeof(*evt));
        memset(evt, 0, sizeof(*evt));

        evt->uid = json_get_int(entry, "uid", 0);
        evt->name = str_dup(json_get_string(entry, "name", "event"));
        evt->description = str_dup(json_get_string(entry, "description", ""));
        evt->announce_msg = str_dup(json_get_string(entry, "announce_msg", ""));
        evt->end_msg = str_dup(json_get_string(entry, "end_msg", ""));
        evt->join_msg = str_dup(json_get_string(entry, "join_msg", ""));
        evt->event_type = (int16_t)json_get_int(entry, "event_type", EVT_TYPE_COLLECTION);
        evt->scope_type = (int16_t)json_get_int(entry, "scope_type", EVT_SCOPE_GLOBAL);
        evt->sched_type = (int16_t)json_get_int(entry, "sched_type", EVT_SCHED_MANUAL);
        evt->sched_interval = (int16_t)json_get_int(entry, "sched_interval", 60);
        evt->sched_variance = (int16_t)json_get_int(entry, "sched_variance", 0);
        evt->sched_duration = (int16_t)json_get_int(entry, "sched_duration", 60);
        evt->sched_cooldown = (int16_t)json_get_int(entry, "sched_cooldown", 0);
        evt->min_level = (int16_t)json_get_int(entry, "min_level", 0);
        evt->max_level = (int16_t)json_get_int(entry, "max_level", 0);
        evt->min_players = (int16_t)json_get_int(entry, "min_players", 0);
        evt->max_players = (int16_t)json_get_int(entry, "max_players", 0);
        evt->enabled = json_get_int(entry, "enabled", 1) != 0;
        evt->flags = (long)json_get_int(entry, "flags", 0);
        evt->comments = str_dup(json_get_string(entry, "comments", ""));

        if (evt->uid <= 0)
            evt->uid = ++max_uid;
        if (evt->uid > max_uid)
            max_uid = evt->uid;

        if (!evtedit_list_head)
            evtedit_list_head = evt;
        else
            evtedit_list_tail->next = evt;
        evtedit_list_tail = evt;
    }

    evtedit_next_uid = json_get_int(root, "next_uid", max_uid + 1);
    if (evtedit_next_uid <= max_uid)
        evtedit_next_uid = max_uid + 1;

    event_system_enabled = json_get_int(root, "events_enabled", 1) != 0;

    json_decref(root);
    return true;
}

static void evtedit_ensure_loaded(void)
{
    if (evtedit_booted)
        return;

    evtedit_booted = true;

    if (!evtedit_load_from_json()) {
        evtedit_clear_all();
        evtedit_save_to_json();
    }
}

static const struct olc_cmd_type evtedit_table[] = {
    { "?",         show_help },
    { "commands",  show_commands },
    { "list",      evtedit_list },
    { "create",    evtedit_create },
    { "delete",    evtedit_delete },
    { "show",      evtedit_show },
    { "name",      evtedit_name },
    { "description", evtedit_description },
    { "announce",  evtedit_announce },
    { "endmsg",    evtedit_endmsg },
    { "joinmsg",   evtedit_joinmsg },
    { "type",      evtedit_type },
    { "enabled",   evtedit_enabled },
    { "scope",     evtedit_scope },
    { "schedule",  evtedit_schedule },
    { "interval",  evtedit_interval },
    { "variance",  evtedit_variance },
    { "duration",  evtedit_duration },
    { "cooldown",  evtedit_cooldown },
    { "minlevel",  evtedit_minlevel },
    { "maxlevel",  evtedit_maxlevel },
    { "minplayers", evtedit_minplayers },
    { "maxplayers", evtedit_maxplayers },
    { "flags",     evtedit_flags },
    { "comments",  evtedit_comments },
    { "save",      evtedit_save },
    { "reload",    evtedit_reload },
    { NULL,          0 }
};

static const OLC_EDITOR_DEF evtedit_def = {
    .name           = "EVTEdit",
    .editor_type    = ED_EVENT,
    .cmd_table      = evtedit_table,
    .show_fn        = evtedit_show,
    .tabs           = {
        .count      = 4,
        .tabs       = {
            { "Identity", "Id", evtedit_show_identity_tab },
            { "Schedule", "Sch", evtedit_show_schedule_tab },
            { "Messages", "Msg", evtedit_show_messages_tab },
            { "Meta", "Meta", evtedit_show_meta_tab },
        },
    },
    .theme          = &olc_theme_system,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = false,
};

void do_evtedit(CHAR_DATA *ch, char *argument)
{
    EVTEDIT_DATA *evt;
    char arg[MIL];

    if (IS_NPC(ch))
        return;

    evtedit_ensure_loaded();

    if (!olc_editor_check_perm(ch, &evtedit_def, NULL)) {
        send_to_char("You don't have permission to edit events.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (IS_NULLSTR(arg)) {
        send_to_char("Syntax: evtedit list\n\r", ch);
        send_to_char("        evtedit create <name>\n\r", ch);
        send_to_char("        evtedit save\n\r", ch);
        send_to_char("        evtedit reload\n\r", ch);
        send_to_char("        evtedit <uid|name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "list")) {
        evtedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg, "create")) {
        evtedit_create(ch, argument);
        return;
    }

    if (!str_cmp(arg, "save")) {
        evtedit_save(ch, argument);
        return;
    }

    if (!str_cmp(arg, "reload")) {
        evtedit_reload(ch, argument);
        return;
    }

    if (is_number(arg))
        evt = evtedit_find_uid(atol(arg));
    else
        evt = evtedit_find_name(arg);

    if (!evt) {
        send_to_char("No event found by that uid or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &evtedit_def, evt, true);
}

void do_event(CHAR_DATA *ch, char *argument)
{
    char cmd[MIL];
    char target[MIL];
    char when[MIL];
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (IS_NPC(ch))
        return;

    evtedit_ensure_loaded();
    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd) || !str_cmp(cmd, "help")) {
        send_to_char("Syntax: event list\n\r", ch);
        send_to_char("        event info <uid|name>\n\r", ch);
        send_to_char("        event enabled\n\r", ch);
        send_to_char("        event enable|disable\n\r", ch);
        send_to_char("        event status\n\r", ch);
        send_to_char("        event join [uid|name]\n\r", ch);
        send_to_char("        event leave [uid|name]\n\r", ch);
        send_to_char("        event start <uid|name>\n\r", ch);
        send_to_char("        event stop <uid|name>\n\r", ch);
        send_to_char("        event schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "enabled")) {
        printf_to_char(ch, "Event system is currently %s.\n\r",
            event_system_enabled ? "{GENABLED{x" : "{RDISABLED{x");
        return;
    }

    if (!str_cmp(cmd, "enable") || !str_cmp(cmd, "disable")) {
        bool turn_on = !str_cmp(cmd, "enable");

        if (!event_user_has_staff_control(ch)) {
            send_to_char("You do not have permission to change event runtime state.\n\r", ch);
            return;
        }

        if (event_system_enabled == turn_on) {
            send_to_char(turn_on ? "Event system is already enabled.\n\r"
                             : "Event system is already disabled.\n\r", ch);
            return;
        }

        event_system_enabled = turn_on;
        if (!turn_on)
            event_stop_all(false);

        if (!evtedit_save_to_json())
            send_to_char("Runtime state changed, but failed to save events.json.\n\r", ch);

        send_to_char(turn_on ? "Event system enabled.\n\r"
                         : "Event system disabled; active events stopped.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "list")) {
        if (!evtedit_list_head) {
            send_to_char("No events defined.\n\r", ch);
            return;
        }

        send_to_char("{WUID   Name                     Type           Schedule       State    Enabled{X\n\r", ch);
        send_to_char("{D----- ------------------------ -------------- -------------- -------- -------{X\n\r", ch);
        for (evt = evtedit_list_head; evt; evt = evt->next) {
            inst = event_find_active_def(evt);
            printf_to_char(ch, "{W%-5ld {x%-24.24s %-14s %-14s %-8s %s{X\n\r",
                evt->uid,
                evt->name,
                flag_string(evt_type_flags, evt->event_type),
                flag_string(evt_sched_flags, evt->sched_type),
                inst ? "{GACTIVE{x" : "{Didle{x",
                evt->enabled ? "{GYes{x" : "{RNo{x");
        }

        return;
    }

    if (!str_cmp(cmd, "status")) {
        bool in_any = false;

        if (!event_active_head) {
            send_to_char("No active events.\n\r", ch);
            return;
        }

        send_to_char("{WActive Event Status:{X\n\r", ch);
        for (inst = event_active_head; inst; inst = inst->next) {
            long rem = 0;
            bool joined = event_find_participant(inst, ch) != NULL;

            if (!inst->def)
                continue;

            if (inst->end_time > current_time)
                rem = (long)(inst->end_time - current_time);

            if (joined)
                in_any = true;

            printf_to_char(ch,
                "  {W%-24.24s{x [%s] participants=%d%s%s\n\r",
                inst->def->name,
                rem > 0 ? formatf("%ldm%02lds left", rem / 60, rem % 60) : "no timer",
                inst->participant_count,
                joined ? " {G(joined){x" : "",
                (IS_SET(inst->def->flags, EVT_FLAG_PASSIVE) || inst->def->event_type == EVT_TYPE_WORLDSTATE)
                    ? " {C(passive){x" : "");
        }

        if (!in_any)
            send_to_char("You are not currently joined to any active events.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "join")) {
        if (!event_system_enabled) {
            send_to_char("Events are currently disabled.\n\r", ch);
            return;
        }

        argument = one_argument(argument, target);

        if (!IS_NULLSTR(target)) {
            evt = event_lookup_definition(target);
            inst = evt ? event_find_active_def(evt) : NULL;
        } else {
            inst = event_find_joinable_for(ch);
        }

        if (!inst || !inst->def) {
            send_to_char("No joinable active event found.\n\r", ch);
            return;
        }

        if (event_find_participant(inst, ch)) {
            send_to_char("You are already participating in that event.\n\r", ch);
            return;
        }

        if (!event_add_participant(inst, ch)) {
            send_to_char("You are not eligible to join that event right now.\n\r", ch);
            return;
        }

        printf_to_char(ch, "You join event '%s'.\n\r", inst->def->name);
        return;
    }

    if (!str_cmp(cmd, "leave")) {
        bool removed = false;

        argument = one_argument(argument, target);

        if (!IS_NULLSTR(target)) {
            evt = event_lookup_definition(target);
            inst = evt ? event_find_active_def(evt) : NULL;

            if (!inst || !event_remove_participant(inst, ch)) {
                send_to_char("You are not participating in that event.\n\r", ch);
                return;
            }

            printf_to_char(ch, "You leave event '%s'.\n\r", inst->def->name);
            return;
        }

        for (inst = event_active_head; inst; inst = inst->next)
            if (event_remove_participant(inst, ch))
                removed = true;

        if (!removed) {
            send_to_char("You are not participating in any active events.\n\r", ch);
            return;
        }

        send_to_char("You leave all active events.\n\r", ch);
        return;
    }

    argument = one_argument(argument, target);
    evt = event_lookup_definition(target);

    if (!evt) {
        send_to_char("No event found by that uid or name.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "info")) {
        long uptime = 0;
        long rem = 0;

        inst = event_find_active_def(evt);
        if (inst)
            uptime = (long)(current_time - inst->started_at);
        if (inst && inst->end_time > current_time)
            rem = (long)(inst->end_time - current_time);

        printf_to_char(ch, "{WEvent:{x %s ({W%ld{x)\n\r", evt->name, evt->uid);
        printf_to_char(ch, "{WType:{x %s  {WScope:{x %s  {WSchedule:{x %s\n\r",
            flag_string(evt_type_flags, evt->event_type),
            flag_string(evt_scope_flags, evt->scope_type),
            flag_string(evt_sched_flags, evt->sched_type));
        printf_to_char(ch, "{WTiming:{x interval=%d variance=%d duration=%d cooldown=%d\n\r",
            evt->sched_interval, evt->sched_variance,
            evt->sched_duration, evt->sched_cooldown);
        printf_to_char(ch, "{WEligibility:{x minlvl=%d maxlvl=%d minplayers=%d maxplayers=%d\n\r",
            evt->min_level, evt->max_level, evt->min_players, evt->max_players);
        printf_to_char(ch, "{WEnabled:{x definition=%s system=%s\n\r",
            evt->enabled ? "{GYes{x" : "{RNo{x",
            event_system_enabled ? "{GYes{x" : "{RNo{x");
        printf_to_char(ch, "{WState:{x %s",
            inst ? "{GACTIVE{x" : "{Didle{x");
        if (inst)
            printf_to_char(ch, " {W(uptime %ld sec){x", uptime);
        if (rem > 0)
            printf_to_char(ch, " {W(remaining %ldm%02lds){x", rem / 60, rem % 60);
        send_to_char("\n\r", ch);

        if (evt->scheduled_time > current_time)
            printf_to_char(ch, "{WScheduled:{x in %ldm%02lds\n\r",
                (long)(evt->scheduled_time - current_time) / 60,
                (long)(evt->scheduled_time - current_time) % 60);
        if (evt->next_auto_time > current_time)
            printf_to_char(ch, "{WNext Auto:{x in %ldm%02lds\n\r",
                (long)(evt->next_auto_time - current_time) / 60,
                (long)(evt->next_auto_time - current_time) % 60);

        if (!IS_NULLSTR(evt->description))
            printf_to_char(ch, "{WDescription:{x %s\n\r", evt->description);
        if (!IS_NULLSTR(evt->announce_msg))
            printf_to_char(ch, "{WAnnounce:{x %s\n\r", evt->announce_msg);
        if (!IS_NULLSTR(evt->end_msg))
            printf_to_char(ch, "{WEnd Msg:{x %s\n\r", evt->end_msg);

        return;
    }

    if (!str_cmp(cmd, "start")) {
        if (!event_user_has_staff_control(ch)) {
            send_to_char("You do not have permission to start events.\n\r", ch);
            return;
        }

        if (!event_system_enabled) {
            send_to_char("Events are currently disabled.\n\r", ch);
            return;
        }

        if (!evt->enabled) {
            send_to_char("That event definition is disabled.\n\r", ch);
            return;
        }

        inst = event_start_definition(evt);
        if (!inst) {
            send_to_char("Event is already active.\n\r", ch);
            return;
        }

        evt->scheduled_time = 0;

        printf_to_char(ch, "Started event '%s'.\n\r", evt->name);
        if (!IS_SET(evt->flags, EVT_FLAG_NOANNOUNCE) && !IS_NULLSTR(evt->announce_msg))
            event_broadcast(evt->announce_msg);
        return;
    }

    if (!str_cmp(cmd, "stop")) {
        if (!event_user_has_staff_control(ch)) {
            send_to_char("You do not have permission to stop events.\n\r", ch);
            return;
        }

        if (!event_stop_definition(evt)) {
            send_to_char("Event is not active.\n\r", ch);
            return;
        }

        printf_to_char(ch, "Stopped event '%s'.\n\r", evt->name);
        if (!IS_NULLSTR(evt->end_msg))
            event_broadcast(evt->end_msg);
        return;
    }

    if (!str_cmp(cmd, "schedule")) {
        time_t when_time;

        if (!event_user_has_staff_control(ch)) {
            send_to_char("You do not have permission to schedule events.\n\r", ch);
            return;
        }

        if (!event_system_enabled) {
            send_to_char("Events are currently disabled.\n\r", ch);
            return;
        }

        if (!evt->enabled) {
            send_to_char("That event definition is disabled.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: event schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
            return;
        }

        strncpy(when, argument, sizeof(when) - 1);
        when[sizeof(when) - 1] = '\0';

        if (!event_parse_when(when, &when_time) || when_time <= current_time) {
            send_to_char("Invalid schedule time.\n\r", ch);
            return;
        }

        evt->scheduled_time = when_time;
        printf_to_char(ch, "Scheduled event '%s' for %s", evt->name, ctime(&when_time));
        return;
    }

    if (!str_cmp(cmd, "tick")) {
        if (!event_user_has_staff_control(ch)) {
            send_to_char("You do not have permission to force ticks.\n\r", ch);
            return;
        }

        event_runtime_update();
        send_to_char("Event runtime tick processed.\n\r", ch);
        return;
    }

    send_to_char("Unknown event subcommand. Type 'event help'.\n\r", ch);
}

void evtedit(CHAR_DATA *ch, char *argument)
{
    evtedit_ensure_loaded();
    olc_editor_interp(ch, argument, &evtedit_def);
}

EVTEDIT(evtedit_list)
{
    EVTEDIT_DATA *evt;

    (void)argument;

    if (!evtedit_list_head) {
        send_to_char("No events defined.\n\r", ch);
        return false;
    }

    send_to_char("{WUID   Name                     Type           Schedule       Enabled{X\n\r", ch);
    send_to_char("{D----- ------------------------ -------------- -------------- -------{X\n\r", ch);
    for (evt = evtedit_list_head; evt; evt = evt->next) {
        printf_to_char(ch, "{W%-5ld {x%-24.24s %-14s %-14s %s{X\n\r",
            evt->uid,
            evt->name,
            flag_string(evt_type_flags, evt->event_type),
            flag_string(evt_sched_flags, evt->sched_type),
            evt->enabled ? "{GYes{x" : "{RNo{x");
    }

    return false;
}

EVTEDIT(evtedit_create)
{
    EVTEDIT_DATA *evt;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }

    if (evtedit_find_name(argument)) {
        send_to_char("An event with that name already exists.\n\r", ch);
        return false;
    }

    evt = evtedit_new(argument);

    if (!evtedit_save_to_json())
        send_to_char("Event created, but failed to save events.json.\n\r", ch);
    else
        send_to_char("Event created.\n\r", ch);

    olc_editor_enter(ch, &evtedit_def, evt, true);
    return true;
}

EVTEDIT(evtedit_delete)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    EVTEDIT_DATA *it;
    EVTEDIT_DATA *prev = NULL;

    if (!evt)
        return false;

    for (it = evtedit_list_head; it; prev = it, it = it->next) {
        if (it != evt)
            continue;

        if (prev)
            prev->next = it->next;
        else
            evtedit_list_head = it->next;

        if (evtedit_list_tail == it)
            evtedit_list_tail = prev;

        evtedit_free_item(it);

        if (!evtedit_save_to_json())
            send_to_char("Event deleted, but failed to save events.json.\n\r", ch);
        else
            send_to_char("Event deleted.\n\r", ch);

        edit_done(ch);
        return true;
    }

    return false;
}

EVTEDIT(evtedit_show)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    if (!evt)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "EVTEdit", evt->name,
        formatf("UID %ld", evt->uid), &evtedit_def);

    tab = ch->desc ? ch->desc->nEditTab : 0;
    switch (tab) {
    case 1:
        evtedit_show_schedule_tab(ch, ctx, evt);
        break;
    case 2:
        evtedit_show_messages_tab(ch, ctx, evt);
        break;
    case 3:
        evtedit_show_meta_tab(ch, ctx, evt);
        break;
    default:
        evtedit_show_identity_tab(ch, ctx, evt);
        break;
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static void evtedit_show_identity_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_string(ctx, theme, "Name:", "name", evt->name);
    olc_display_type(ctx, theme, "Type:", "type", evt_type_flags, evt->event_type);
    olc_display_string(ctx, theme, "Enabled:", "enabled",
        evt->enabled ? "Yes" : "No");
    olc_display_type(ctx, theme, "Scope:", "scope", evt_scope_flags, evt->scope_type);
    olc_display_flags(ctx, theme, "Flags:", "flags", evt_flags, evt->flags);
}

static void evtedit_show_schedule_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_type(ctx, theme, "Schedule:", "schedule", evt_sched_flags, evt->sched_type);
    olc_display_number(ctx, theme, "Interval:", "interval", evt->sched_interval);
    olc_display_number(ctx, theme, "Variance:", "variance", evt->sched_variance);
    olc_display_number(ctx, theme, "Duration:", "duration", evt->sched_duration);
    olc_display_number(ctx, theme, "Cooldown:", "cooldown", evt->sched_cooldown);
    olc_display_number(ctx, theme, "Min Level:", "minlevel", evt->min_level);
    olc_display_number(ctx, theme, "Max Level:", "maxlevel", evt->max_level);
    olc_display_number(ctx, theme, "Min Players:", "minplayers", evt->min_players);
    olc_display_number(ctx, theme, "Max Players:", "maxplayers", evt->max_players);
}

static void evtedit_show_messages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_text(ctx, theme, "Description:", "description",
        IS_NULLSTR(evt->description) ? NULL : evt->description);
    olc_display_text(ctx, theme, "Announce:", "announce",
        IS_NULLSTR(evt->announce_msg) ? NULL : evt->announce_msg);
    olc_display_text(ctx, theme, "End Msg:", "endmsg",
        IS_NULLSTR(evt->end_msg) ? NULL : evt->end_msg);
    olc_display_text(ctx, theme, "Join Msg:", "joinmsg",
        IS_NULLSTR(evt->join_msg) ? NULL : evt->join_msg);
}

static void evtedit_show_meta_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_number(ctx, theme, "UID:", NULL, evt->uid);
    olc_display_string(ctx, theme, "System Enabled:", NULL,
        event_system_enabled ? "Yes" : "No");
    olc_display_text(ctx, theme, "Comments:", "comments",
        IS_NULLSTR(evt->comments) ? NULL : evt->comments);
    olc_display_infof(ctx, theme, "Use 'save' to write events.json to disk.");
}

static bool evtedit_save_after_change(CHAR_DATA *ch)
{
    if (!evtedit_save_to_json()) {
        send_to_char("Updated, but failed to save events.json.\n\r", ch);
        return true;
    }

    return true;
}

EVTEDIT(evtedit_name)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!IS_NULLSTR(argument)
        && evtedit_find_name(argument)
        && str_cmp(evt->name, argument)) {
        send_to_char("An event with that name already exists.\n\r", ch);
        return false;
    }

    if (!olc_cmd_string(ch, argument, "name", "name <new name>",
            &evt->name, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_description)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "description", "description <text>",
            &evt->description, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_announce)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "announce", "announce <text>",
            &evt->announce_msg, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_endmsg)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "endmsg", "endmsg <text>",
            &evt->end_msg, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_joinmsg)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "joinmsg", "joinmsg <text>",
            &evt->join_msg, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_type)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_type_set_i16(ch, argument, "type",
            "type <collection|invasion|boss|war-ffa|war-genocide|war-jihad|worldstate|custom>",
            &evt->event_type, evt_type_flags, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_enabled)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_bool(ch, argument, "enabled", "enabled [on|off]",
            &evt->enabled, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_scope)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_type_set_i16(ch, argument, "scope",
            "scope <global|area|region|zones|battlefield>",
            &evt->scope_type, evt_scope_flags, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_schedule)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_type_set_i16(ch, argument, "schedule",
            "schedule <manual|recurring|calendar|worldcondition|triggered>",
            &evt->sched_type, evt_sched_flags, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_interval)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "interval", "interval <0-32767>",
            &evt->sched_interval, 0, 32767, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_variance)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "variance", "variance <0-32767>",
            &evt->sched_variance, 0, 32767, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_duration)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "duration", "duration <0-32767>",
            &evt->sched_duration, 0, 32767, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_cooldown)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "cooldown", "cooldown <0-32767>",
            &evt->sched_cooldown, 0, 32767, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_minlevel)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "minlevel", "minlevel <0-32767>",
            &evt->min_level, 0, 32767, NULL, NULL))
        return false;

    if (evt->max_level > 0 && evt->min_level > evt->max_level) {
        send_to_char("Min level cannot exceed max level.\n\r", ch);
        return false;
    }

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_maxlevel)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "maxlevel", "maxlevel <0-32767>",
            &evt->max_level, 0, 32767, NULL, NULL))
        return false;

    if (evt->max_level > 0 && evt->min_level > evt->max_level) {
        send_to_char("Max level cannot be less than min level.\n\r", ch);
        return false;
    }

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_minplayers)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "minplayers", "minplayers <0-32767>",
            &evt->min_players, 0, 32767, NULL, NULL))
        return false;

    if (evt->max_players > 0 && evt->min_players > evt->max_players) {
        send_to_char("Min players cannot exceed max players.\n\r", ch);
        return false;
    }

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_maxplayers)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "maxplayers", "maxplayers <0-32767>",
            &evt->max_players, 0, 32767, NULL, NULL))
        return false;

    if (evt->max_players > 0 && evt->min_players > evt->max_players) {
        send_to_char("Max players cannot be less than min players.\n\r", ch);
        return false;
    }

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_flags)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_flag_toggle(ch, argument, "flags", "flags <flag>",
            &evt->flags, evt_flags, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_comments)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "comments", "comments <text>",
            &evt->comments, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_save)
{
    evtedit_ensure_loaded();

    if (!evtedit_save_to_json()) {
        send_to_char("Failed to save events.json.\n\r", ch);
        return false;
    }

    send_to_char("Event data saved to events.json.\n\r", ch);
    return false;
}

EVTEDIT(evtedit_reload)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    EVTEDIT_DATA *reloaded = NULL;
    long uid = 0;

    evtedit_ensure_loaded();

    if (evt)
        uid = evt->uid;

    if (!evtedit_load_from_json()) {
        send_to_char("Failed to reload events.json.\n\r", ch);
        return false;
    }

    if (uid > 0)
        reloaded = evtedit_find_uid(uid);

    if (uid > 0 && !reloaded) {
        send_to_char("Events reloaded, but current event no longer exists.\n\r", ch);
        edit_done(ch);
        return false;
    }

    if (reloaded && ch->desc)
        ch->desc->pEdit = reloaded;

    send_to_char("Events reloaded from disk.\n\r", ch);
    return false;
}