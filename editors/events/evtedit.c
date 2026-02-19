#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <jansson.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../event_types.h"
#include "../../scripts.h"
#include "../../recycle.h"
#include "../../io/json/json_common.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef struct evtedit_data EVTEDIT_DATA;
typedef struct event_instance EVENT_INSTANCE;
typedef struct event_part EVENT_PART;
typedef struct evt_roster_entry EVT_ROSTER_ENTRY;

struct evt_roster_entry {
    EVT_ROSTER_ENTRY *next;
    int kind;
    long vnum;
    int count;
    int chance;
    int min_level;
    int max_level;
    bool boss;
    char phase[MIL];
};

struct evtedit_data {
    long uid;
    char *name;
    char *description;
    char *announce_msg;
    char *end_msg;
    char *join_msg;
    int16_t event_type;
    int16_t scope_type;
    long scope_area_uid;
    bool scope_floating;
    int16_t sched_type;
    int16_t sched_interval;
    int16_t sched_variance;
    int16_t sched_duration;
    int16_t sched_cooldown;
    int16_t min_level;
    int16_t max_level;
    int16_t min_players;
    int16_t max_players;
    int16_t completion_goal;
    bool leader_required;
    char *display_title;
    char *short_summary;
    char *news_slug;
    char *news_announcement;
    char *news_body;
    char *theme_tags;
    char *spawn_brackets;
    char *collection_brackets;
    char *bracket_mode;
    char *progress_aggregation;
    char *phase_plan;
    long reward_success_script;
    long reward_failure_script;
    bool enabled;
    long flags;
    char *comments;
    EVT_ROSTER_ENTRY *roster;

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
    int progress_kills;
    int progress_items;
    int progress_goal;
    long scope_area_uid;
    bool scope_floating;
    bool leader_phase;
    int phase_index;
    time_t phase_due;
    char phase_name[MIL];
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

#define EVT_PHASEPLAN_MAX_STEPS 64

typedef struct evt_phase_step_def EVT_PHASE_STEP_DEF;
struct evt_phase_step_def {
    char name[MIL];
    int minutes;
    long script_vnum;
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

enum {
    EVT_ROSTER_NPC = 0,
    EVT_ROSTER_OBJECT,
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
EVTEDIT(evtedit_scopeanchor);
EVTEDIT(evtedit_scopefloating);
EVTEDIT(evtedit_schedule);
EVTEDIT(evtedit_interval);
EVTEDIT(evtedit_variance);
EVTEDIT(evtedit_duration);
EVTEDIT(evtedit_cooldown);
EVTEDIT(evtedit_minlevel);
EVTEDIT(evtedit_maxlevel);
EVTEDIT(evtedit_minplayers);
EVTEDIT(evtedit_maxplayers);
EVTEDIT(evtedit_goal);
EVTEDIT(evtedit_leaderrequired);
EVTEDIT(evtedit_title);
EVTEDIT(evtedit_summary);
EVTEDIT(evtedit_newsslug);
EVTEDIT(evtedit_newsannounce);
EVTEDIT(evtedit_newsbody);
EVTEDIT(evtedit_themetags);
EVTEDIT(evtedit_roster);
EVTEDIT(evtedit_spawnbrackets);
EVTEDIT(evtedit_collectionbrackets);
EVTEDIT(evtedit_bracketmode);
EVTEDIT(evtedit_progressagg);
EVTEDIT(evtedit_phaseplan);
EVTEDIT(evtedit_rewardsuccess);
EVTEDIT(evtedit_rewardfail);
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

static const struct flag_type evt_roster_kind_flags[] = {
    { "npc", EVT_ROSTER_NPC, true, NULL },
    { "object", EVT_ROSTER_OBJECT, true, NULL },
    { NULL, 0, false, NULL },
};

static const char *event_format_time_short(time_t when)
{
    static char buf[64];
    struct tm *tm_info;

    if (when <= 0)
        return "(none)";

    tm_info = localtime(&when);
    if (!tm_info)
        return "(invalid)";

    if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info) <= 0)
        return "(invalid)";

    return buf;
}

static const char *event_enum_name(const struct flag_type *table, int value)
{
    const char *name = flag_name(table, value);
    return IS_NULLSTR(name) ? "(unknown)" : name;
}

static bool event_scope_requires_anchor(const EVTEDIT_DATA *evt)
{
    return evt && evt->scope_type != EVT_SCOPE_GLOBAL;
}

static bool event_character_in_scope(const EVENT_INSTANCE *inst, const CHAR_DATA *ch)
{
    if (!inst || !inst->def)
        return false;

    if (inst->def->scope_type == EVT_SCOPE_GLOBAL)
        return true;

    if (!ch || !ch->in_room || !ch->in_room->area)
        return false;

    if (inst->scope_area_uid <= 0)
        return false;

    return ch->in_room->area->uid == inst->scope_area_uid;
}

static void evtedit_roster_free(EVT_ROSTER_ENTRY **head)
{
    EVT_ROSTER_ENTRY *entry;
    EVT_ROSTER_ENTRY *next;

    if (!head)
        return;

    for (entry = *head; entry; entry = next) {
        next = entry->next;
        free_mem(entry, sizeof(*entry));
    }

    *head = NULL;
}

static int evtedit_roster_count(const EVT_ROSTER_ENTRY *head)
{
    const EVT_ROSTER_ENTRY *entry;
    int count = 0;

    for (entry = head; entry; entry = entry->next)
        count++;

    return count;
}

static EVT_ROSTER_ENTRY *evtedit_roster_find(EVT_ROSTER_ENTRY *head, int index)
{
    EVT_ROSTER_ENTRY *entry;
    int current = 1;

    if (index <= 0)
        return NULL;

    for (entry = head; entry; entry = entry->next, current++)
        if (current == index)
            return entry;

    return NULL;
}

static void evtedit_roster_append(EVTEDIT_DATA *evt, EVT_ROSTER_ENTRY *entry)
{
    EVT_ROSTER_ENTRY *tail;

    if (!evt || !entry)
        return;

    entry->next = NULL;

    if (!evt->roster) {
        evt->roster = entry;
        return;
    }

    for (tail = evt->roster; tail->next; tail = tail->next)
        ;

    tail->next = entry;
}

static const char *evtedit_roster_level_window(const EVT_ROSTER_ENTRY *entry)
{
    static char window[64];

    if (!entry)
        return "any";

    if (entry->min_level <= 0 && entry->max_level <= 0)
        return "any";

    if (entry->max_level <= 0) {
        snprintf(window, sizeof(window), "%d+", UMAX(1, entry->min_level));
        return window;
    }

    snprintf(window, sizeof(window), "%d-%d",
        UMAX(1, entry->min_level), UMAX(entry->min_level, entry->max_level));
    return window;
}

static const char *evtedit_roster_phase_name(const EVT_ROSTER_ENTRY *entry)
{
    if (!entry || IS_NULLSTR(entry->phase))
        return "any";

    return entry->phase;
}

static bool evtedit_parse_onoff_token(const char *token, bool *value)
{
    if (IS_NULLSTR(token) || !value)
        return false;

    if (!str_cmp(token, "on") || !str_cmp(token, "yes") || !str_cmp(token, "true") || !str_cmp(token, "1")) {
        *value = true;
        return true;
    }

    if (!str_cmp(token, "off") || !str_cmp(token, "no") || !str_cmp(token, "false") || !str_cmp(token, "0")) {
        *value = false;
        return true;
    }

    return false;
}

static void evtedit_show_identity_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_schedule_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_messages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_meta_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_ensure_loaded(void);
static void event_broadcast(const char *message);
static bool event_stop_definition(EVTEDIT_DATA *evt);

static void evtedit_free_item(EVTEDIT_DATA *evt)
{
    if (!evt)
        return;

    free_string(evt->name);
    free_string(evt->description);
    free_string(evt->announce_msg);
    free_string(evt->end_msg);
    free_string(evt->join_msg);
    free_string(evt->display_title);
    free_string(evt->short_summary);
    free_string(evt->news_slug);
    free_string(evt->news_announcement);
    free_string(evt->news_body);
    free_string(evt->theme_tags);
    free_string(evt->spawn_brackets);
    free_string(evt->collection_brackets);
    free_string(evt->bracket_mode);
    free_string(evt->progress_aggregation);
    free_string(evt->phase_plan);
    free_string(evt->comments);
    evtedit_roster_free(&evt->roster);
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
    evt->scope_area_uid = 0;
    evt->scope_floating = false;
    evt->sched_type = EVT_SCHED_MANUAL;
    evt->sched_interval = 60;
    evt->sched_variance = 0;
    evt->sched_duration = 60;
    evt->sched_cooldown = 0;
    evt->min_level = 0;
    evt->max_level = 0;
    evt->min_players = 0;
    evt->max_players = 0;
    evt->completion_goal = 0;
    evt->leader_required = true;
    evt->display_title = str_dup("");
    evt->short_summary = str_dup("");
    evt->news_slug = str_dup("");
    evt->news_announcement = str_dup("");
    evt->news_body = str_dup("");
    evt->theme_tags = str_dup("");
    evt->spawn_brackets = str_dup("");
    evt->collection_brackets = str_dup("");
    evt->bracket_mode = str_dup("auto_by_level");
    evt->progress_aggregation = str_dup("shared");
    evt->phase_plan = str_dup("");
    evt->reward_success_script = 0;
    evt->reward_failure_script = 0;
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

static const char *event_bracket_spec_for(const EVTEDIT_DATA *evt)
{
    if (!evt)
        return "";

    if (evt->event_type == EVT_TYPE_COLLECTION)
        return IS_NULLSTR(evt->collection_brackets) ? "" : evt->collection_brackets;

    if (evt->event_type == EVT_TYPE_INVASION
        || evt->event_type == EVT_TYPE_WAR_FFA
        || evt->event_type == EVT_TYPE_WAR_GENOCIDE
        || evt->event_type == EVT_TYPE_WAR_JIHAD)
        return IS_NULLSTR(evt->spawn_brackets) ? "" : evt->spawn_brackets;

    return "";
}

typedef enum {
    EVT_PROGRESS_MODE_SHARED = 0,
    EVT_PROGRESS_MODE_PER_BRACKET_ANY,
    EVT_PROGRESS_MODE_PER_BRACKET_ALL,
} evt_progress_mode_t;

static evt_progress_mode_t event_progress_mode(const EVTEDIT_DATA *evt)
{
    if (!evt || IS_NULLSTR(evt->progress_aggregation)
        || !str_cmp(evt->progress_aggregation, "shared")
        || !str_cmp(evt->progress_aggregation, "total"))
        return EVT_PROGRESS_MODE_SHARED;

    if (!str_cmp(evt->progress_aggregation, "per_bracket_all_required"))
        return EVT_PROGRESS_MODE_PER_BRACKET_ALL;

    if (!str_cmp(evt->progress_aggregation, "per_bracket")
        || !str_cmp(evt->progress_aggregation, "per_bracket_any"))
        return EVT_PROGRESS_MODE_PER_BRACKET_ANY;

    return EVT_PROGRESS_MODE_SHARED;
}

static void event_copy_trimmed(char *dst, size_t dst_size, const char *src)
{
    const char *start;
    const char *end;
    size_t len;

    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';

    if (IS_NULLSTR(src))
        return;

    start = src;
    while (*start && isspace((unsigned char)*start))
        start++;

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;

    len = (size_t)(end - start);
    if (len >= dst_size)
        len = dst_size - 1;

    if (len > 0)
        memcpy(dst, start, len);
    dst[len] = '\0';
}

static bool event_parse_phase_plan_step(const char *plan, int step_index,
    char *phase_name, size_t phase_name_size, int *minutes_out, long *script_vnum_out)
{
    const char *cursor;
    int index = 0;

    if (!phase_name || phase_name_size == 0 || !minutes_out || !script_vnum_out)
        return false;

    phase_name[0] = '\0';
    *minutes_out = 0;
    *script_vnum_out = 0;

    if (IS_NULLSTR(plan) || step_index < 0)
        return false;

    cursor = plan;
    while (*cursor) {
        char token[MIL];
        char name_buf[MIL];
        char *at;
        char *hash;
        int pos = 0;
        char *minutes_str = NULL;
        char *script_str = NULL;

        while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ';' || *cursor == ','))
            cursor++;

        if (!*cursor)
            break;

        while (*cursor && *cursor != ';' && *cursor != ',' && pos < MIL - 1)
            token[pos++] = *cursor++;
        token[pos] = '\0';

        event_copy_trimmed(name_buf, sizeof(name_buf), token);

        at = strchr(name_buf, '@');
        hash = strchr(name_buf, '#');

        if (at) {
            *at = '\0';
            minutes_str = at + 1;
        }

        if (hash) {
            *hash = '\0';
            script_str = hash + 1;
        }

        if (minutes_str) {
            char minutes_buf[MIL];
            event_copy_trimmed(minutes_buf, sizeof(minutes_buf), minutes_str);
            if (!IS_NULLSTR(minutes_buf)) {
                char *endptr = NULL;
                long parsed = strtol(minutes_buf, &endptr, 10);
                if (endptr != minutes_buf && parsed > 0)
                    *minutes_out = (int)parsed;
            }
        }

        if (script_str) {
            char script_buf[MIL];
            char *endptr = NULL;
            long parsed;

            event_copy_trimmed(script_buf, sizeof(script_buf), script_str);
            parsed = strtol(script_buf, &endptr, 10);
            if (endptr != script_buf && parsed > 0)
                *script_vnum_out = parsed;
        }

        if (!IS_NULLSTR(name_buf)) {
            if (index == step_index) {
                strncpy(phase_name, name_buf, phase_name_size - 1);
                phase_name[phase_name_size - 1] = '\0';
                return true;
            }
            index++;
        }

        while (*cursor && *cursor != ';' && *cursor != ',')
            cursor++;
    }

    return false;
}

static bool event_validate_phase_name(const char *name, char *error, size_t error_size)
{
    const char *cursor;

    if (error && error_size > 0)
        error[0] = '\0';

    if (IS_NULLSTR(name)) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Phase name cannot be empty.");
        return false;
    }

    cursor = name;
    while (*cursor) {
        if (*cursor == ',' || *cursor == ';' || *cursor == '@' || *cursor == '#') {
            if (error && error_size > 0)
                snprintf(error, error_size,
                    "Phase name cannot contain ',', ';', '@', or '#'.");
            return false;
        }
        cursor++;
    }

    return true;
}

static int event_phase_steps_load(const char *plan,
    EVT_PHASE_STEP_DEF *steps, int max_steps,
    char *error, size_t error_size)
{
    int index = 0;

    if (error && error_size > 0)
        error[0] = '\0';

    if (!steps || max_steps <= 0) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Internal phase buffer unavailable.");
        return -1;
    }

    if (IS_NULLSTR(plan))
        return 0;

    while (true) {
        EVT_PHASE_STEP_DEF step;

        memset(&step, 0, sizeof(step));
        if (!event_parse_phase_plan_step(plan, index,
                step.name, sizeof(step.name), &step.minutes, &step.script_vnum))
            break;

        if (index >= max_steps) {
            if (error && error_size > 0)
                snprintf(error, error_size,
                    "Too many phase steps (maximum %d).", max_steps);
            return -1;
        }

        if (!event_validate_phase_name(step.name, error, error_size))
            return -1;

        steps[index++] = step;
    }

    if (index <= 0) {
        if (error && error_size > 0)
            snprintf(error, error_size,
                "Stored phase plan is invalid. Use phaseplan clear, then rebuild with subcommands.");
        return -1;
    }

    return index;
}

static void event_phase_steps_store(EVTEDIT_DATA *evt,
    const EVT_PHASE_STEP_DEF *steps, int count)
{
    BUFFER *buffer;
    int i;

    if (!evt || count < 0)
        return;

    buffer = new_buf();

    for (i = 0; i < count; i++) {
        add_buf(buffer, steps[i].name);
        if (steps[i].minutes > 0)
            add_buf(buffer, formatf("@%d", steps[i].minutes));
        if (steps[i].script_vnum > 0)
            add_buf(buffer, formatf("#%ld", steps[i].script_vnum));
        if (i + 1 < count)
            add_buf(buffer, ",");
    }

    free_string(evt->phase_plan);
    evt->phase_plan = str_dup(buf_string(buffer));
    free_buf(buffer);
}

static bool event_parse_bracket_token(const char *token, int *min_level, int *max_level)
{
    int min = 0;
    int max = 0;

    if (IS_NULLSTR(token) || !min_level || !max_level)
        return false;

    if (sscanf(token, " %d - %d ", &min, &max) == 2) {
        if (min < 0 || max < 0)
            return false;

        if (min > max) {
            int tmp = min;
            min = max;
            max = tmp;
        }

        *min_level = min;
        *max_level = max;
        return true;
    }

    if (sscanf(token, " %d + ", &min) == 1) {
        if (min < 0)
            return false;

        *min_level = min;
        *max_level = 1000000;
        return true;
    }

    if (sscanf(token, " %d ", &min) == 1) {
        if (min < 0)
            return false;

        *min_level = min;
        *max_level = min;
        return true;
    }

    return false;
}

static bool event_level_bracket_index(const char *spec, int level, int *index_out)
{
    const char *cursor;
    int index = 0;

    if (IS_NULLSTR(spec) || !index_out)
        return false;

    cursor = spec;
    while (*cursor) {
        char token[MIL];
        int pos = 0;
        int min_level = 0;
        int max_level = 0;

        while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ';' || *cursor == ','))
            cursor++;

        if (!*cursor)
            break;

        while (*cursor && *cursor != ';' && *cursor != ',' && pos < MIL - 1)
            token[pos++] = *cursor++;
        token[pos] = '\0';

        if (event_parse_bracket_token(token, &min_level, &max_level)) {
            if (level >= min_level && level <= max_level) {
                *index_out = index;
                return true;
            }
            index++;
        }

        while (*cursor && *cursor != ';' && *cursor != ',')
            cursor++;
    }

    return false;
}

static bool event_roster_phase_matches(const EVT_ROSTER_ENTRY *entry, const char *phase_name)
{
    if (!entry || IS_NULLSTR(entry->phase)
        || !str_cmp(entry->phase, "any")
        || !str_cmp(entry->phase, "all")
        || !str_cmp(entry->phase, "*"))
        return true;

    if (IS_NULLSTR(phase_name))
        return false;

    return !str_cmp(entry->phase, phase_name);
}

static int event_roster_entry_bracket(const EVTEDIT_DATA *evt, const EVT_ROSTER_ENTRY *entry)
{
    const char *spec;
    int level;
    int index = 0;

    if (!evt || !entry)
        return 0;

    spec = event_bracket_spec_for(evt);
    if (IS_NULLSTR(spec))
        return 0;

    if (entry->min_level > 0 && entry->max_level > 0)
        level = (entry->min_level + entry->max_level) / 2;
    else if (entry->min_level > 0)
        level = entry->min_level;
    else if (entry->max_level > 0)
        level = entry->max_level;
    else
        return 0;

    if (!event_level_bracket_index(spec, level, &index))
        return 0;

    return index + 1;
}

static int event_runtime_spawn_roster_for_phase(EVENT_INSTANCE *inst, const char *phase_name)
{
    AREA_DATA *area;
    EVT_ROSTER_ENTRY *entry;
    int spawned = 0;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return 0;

    area = get_area_from_uid(inst->scope_area_uid);
    if (!area)
        return 0;

    for (entry = inst->def->roster; entry; entry = entry->next) {
        int i;
        int bracket;

        if (!event_roster_phase_matches(entry, phase_name))
            continue;

        bracket = event_roster_entry_bracket(inst->def, entry);

        for (i = 0; i < UMAX(1, entry->count); i++) {
            ROOM_INDEX_DATA *room;

            if (entry->chance < 100 && number_percent() > entry->chance)
                continue;

            room = get_random_room_area(NULL, area);
            if (!room)
                continue;

            if (entry->kind == EVT_ROSTER_NPC) {
                MOB_INDEX_DATA *mob_index = get_mob_index(area, entry->vnum);
                CHAR_DATA *mob;

                if (!mob_index)
                    continue;

                mob = create_mobile(mob_index, false);
                if (!mob)
                    continue;

                char_to_room(mob, room);
                event_tag_mobile_spawn(mob, inst->def->uid, inst->instance_id);
                if (bracket > 0)
                    event_set_mobile_spawn_bracket(mob, bracket);
                spawned++;
            } else {
                OBJ_INDEX_DATA *obj_index = get_obj_index(area, entry->vnum);
                OBJ_DATA *obj;

                if (!obj_index)
                    continue;

                obj = create_object(obj_index, 0, true);
                if (!obj)
                    continue;

                obj_to_room(obj, room);
                event_tag_object_spawn(obj, inst->def->uid, inst->instance_id);
                if (bracket > 0)
                    event_set_object_spawn_bracket(obj, bracket);
                spawned++;
            }
        }
    }

    return spawned;
}

static bool event_roster_has_boss_for_phase(const EVENT_INSTANCE *inst, const char *phase_name)
{
    EVT_ROSTER_ENTRY *entry;

    if (!inst || !inst->def)
        return false;

    for (entry = inst->def->roster; entry; entry = entry->next)
        if (entry->kind == EVT_ROSTER_NPC
            && entry->boss
            && event_roster_phase_matches(entry, phase_name))
            return true;

    return false;
}

static bool event_mobile_matches_roster_boss(const EVENT_INSTANCE *inst,
    const CHAR_DATA *mob, const char *phase_name)
{
    EVT_ROSTER_ENTRY *entry;

    if (!inst || !inst->def || !mob || !mob->pIndexData)
        return false;

    for (entry = inst->def->roster; entry; entry = entry->next)
        if (entry->kind == EVT_ROSTER_NPC
            && entry->boss
            && entry->vnum == mob->pIndexData->vnum
            && event_roster_phase_matches(entry, phase_name))
            return true;

    return false;
}

static int event_count_brackets(const char *spec)
{
    const char *cursor;
    int count = 0;

    if (IS_NULLSTR(spec))
        return 0;

    cursor = spec;
    while (*cursor) {
        char token[MIL];
        int pos = 0;
        int min_level = 0;
        int max_level = 0;

        while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ';' || *cursor == ','))
            cursor++;

        if (!*cursor)
            break;

        while (*cursor && *cursor != ';' && *cursor != ',' && pos < MIL - 1)
            token[pos++] = *cursor++;
        token[pos] = '\0';

        if (event_parse_bracket_token(token, &min_level, &max_level))
            count++;

        while (*cursor && *cursor != ';' && *cursor != ',')
            cursor++;
    }

    return count;
}

static bool event_validate_bracket_spec(const char *spec, char *error, size_t error_size)
{
    const char *cursor;
    int previous_max = -1;

    if (error && error_size > 0)
        error[0] = '\0';

    if (IS_NULLSTR(spec))
        return true;

    cursor = spec;
    while (*cursor) {
        char token[MIL];
        int pos = 0;
        int min_level = 0;
        int max_level = 0;

        while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ';' || *cursor == ','))
            cursor++;

        if (!*cursor)
            break;

        while (*cursor && *cursor != ';' && *cursor != ',' && pos < MIL - 1)
            token[pos++] = *cursor++;
        token[pos] = '\0';

        if (!event_parse_bracket_token(token, &min_level, &max_level)) {
            if (error && error_size > 0)
                snprintf(error, error_size, "Invalid bracket token: '%s'", token);
            return false;
        }

        if (previous_max >= 0 && min_level <= previous_max) {
            if (error && error_size > 0)
                snprintf(error, error_size,
                    "Bracket ranges must be non-overlapping and ordered (problem near '%s').",
                    token);
            return false;
        }

        previous_max = max_level;

        while (*cursor && *cursor != ';' && *cursor != ',')
            cursor++;
    }

    return true;
}

static int event_progress_for_bracket(const EVENT_INSTANCE *inst, int bracket_index, bool items)
{
    EVENT_PART *part;
    int total = 0;

    if (!inst || bracket_index < 0)
        return 0;

    for (part = inst->participants; part; part = part->next) {
        if (part->team != bracket_index + 1)
            continue;
        total += items ? part->items_turned : part->kills;
    }

    return total;
}

static bool event_all_brackets_met_goal(const EVENT_INSTANCE *inst, int goal, bool items)
{
    const char *spec;
    int count;
    int i;

    if (!inst || !inst->def || goal <= 0)
        return false;

    spec = event_bracket_spec_for(inst->def);
    count = event_count_brackets(spec);

    if (count <= 0)
        return false;

    for (i = 0; i < count; i++)
        if (event_progress_for_bracket(inst, i, items) < goal)
            return false;

    return true;
}

static bool event_any_bracket_met_goal(const EVENT_INSTANCE *inst, int goal, bool items)
{
    const char *spec;
    int count;
    int i;

    if (!inst || !inst->def || goal <= 0)
        return false;

    spec = event_bracket_spec_for(inst->def);
    count = event_count_brackets(spec);

    if (count <= 0)
        return false;

    for (i = 0; i < count; i++)
        if (event_progress_for_bracket(inst, i, items) >= goal)
            return true;

    return false;
}

static bool event_assign_participant_bracket(EVENT_INSTANCE *inst, CHAR_DATA *ch, int *team_out)
{
    const char *spec;
    bool enforce_by_level;
    int index = 0;

    if (!inst || !inst->def || !ch || !team_out)
        return false;

    *team_out = 0;
    spec = event_bracket_spec_for(inst->def);
    if (IS_NULLSTR(spec))
        return true;

    enforce_by_level = IS_NULLSTR(inst->def->bracket_mode)
        || !str_cmp(inst->def->bracket_mode, "auto_by_level");

    if (event_level_bracket_index(spec, ch->tot_level, &index)) {
        *team_out = index + 1;
        return true;
    }

    if (enforce_by_level)
        return false;

    return true;
}

static bool event_mobile_matches_instance(const CHAR_DATA *mob, const EVENT_INSTANCE *inst)
{
    if (!mob || !inst || !inst->def)
        return false;

    if (mob->event_source_uid <= 0 && mob->event_source_instance_id == 0)
        return true;

    if (mob->event_source_uid != inst->def->uid)
        return false;

    if (mob->event_source_instance_id > 0
        && mob->event_source_instance_id != inst->instance_id)
        return false;

    return true;
}

static bool event_mobile_matches_participant_bracket(const CHAR_DATA *mob, const EVENT_PART *part)
{
    int mob_bracket = 0;

    if (!mob || !part)
        return false;

    if (part->team <= 0)
        return true;

    if (!event_get_mobile_spawn_bracket(mob, &mob_bracket) || mob_bracket <= 0)
        return true;

    return mob_bracket == part->team;
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
    int team = 0;

    if (!inst || !inst->def || !ch)
        return false;

    if (IS_SET(inst->def->flags, EVT_FLAG_PASSIVE) || inst->def->event_type == EVT_TYPE_WORLDSTATE)
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

    if (event_scope_requires_anchor(inst->def)
    && inst->scope_area_uid <= 0
    && inst->scope_floating
    && ch->in_room && ch->in_room->area)
        inst->scope_area_uid = ch->in_room->area->uid;

    if (!event_character_in_scope(inst, ch))
        return false;

    if (!event_assign_participant_bracket(inst, ch, &team))
        return false;

    part = alloc_mem(sizeof(*part));
    memset(part, 0, sizeof(*part));
    part->inst = inst;
    part->ch = ch;
    part->team = team;
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

static int event_default_kill_goal(const EVTEDIT_DATA *evt)
{
    if (!evt)
        return 0;

    if (evt->completion_goal > 0)
        return evt->completion_goal;

    if (evt->event_type == EVT_TYPE_INVASION)
        return evt->min_players > 0 ? evt->min_players : 25;

    return 0;
}

static void event_runtime_run_phase_script(EVENT_INSTANCE *inst, long script_vnum, const char *phase_name)
{
    WNUM wnum;
    SCRIPT_DATA *script;

    if (!inst || !inst->def || script_vnum <= 0)
        return;

    if (!resolve_widevnum(script_vnum, NULL, &wnum) || !wnum.pArea)
        return;

    script = get_script_index(wnum.pArea, wnum.vnum, PRG_APROG);
    if (!script)
        return;

    execute_script(script->vnum, script,
        NULL, NULL, NULL, NULL,
        wnum.pArea, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL,
        (char *)(IS_NULLSTR(phase_name) ? "event_phase" : phase_name),
        "event_phase",
        TRIG_NONE,
        (int)inst->def->uid,
        (int)inst->instance_id,
        inst->phase_index + 1,
        0,
        0);
}

static void event_runtime_apply_phase_step(EVENT_INSTANCE *inst, int phase_index,
    const char *phase_name, int phase_minutes, long phase_script_vnum)
{
    char old_phase_name[MIL];
    int old_phase_index;
    bool old_leader_phase;
    bool phase_changed;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    old_phase_index = inst->phase_index;
    old_leader_phase = inst->leader_phase;
    strncpy(old_phase_name, inst->phase_name, sizeof(old_phase_name) - 1);
    old_phase_name[sizeof(old_phase_name) - 1] = '\0';

    inst->phase_index = phase_index;
    inst->phase_due = phase_minutes > 0 ? current_time + ((time_t)phase_minutes * 60) : 0;

    if (!IS_NULLSTR(phase_name)) {
        strncpy(inst->phase_name, phase_name, sizeof(inst->phase_name) - 1);
        inst->phase_name[sizeof(inst->phase_name) - 1] = '\0';
    } else {
        inst->phase_name[0] = '\0';
    }

    if (inst->def->event_type == EVT_TYPE_INVASION) {
        if (!str_cmp(inst->phase_name, "leader") || !str_cmp(inst->phase_name, "leader_phase"))
            inst->leader_phase = true;
        else if (!IS_NULLSTR(inst->phase_name))
            inst->leader_phase = false;
    }

    inst->dirty = true;

    phase_changed = (old_phase_index != inst->phase_index)
        || (old_leader_phase != inst->leader_phase)
        || str_cmp(old_phase_name, inst->phase_name);

    if (phase_changed
        && !IS_SET(inst->def->flags, EVT_FLAG_NOANNOUNCE)
        && !IS_NULLSTR(inst->phase_name)
        && (old_phase_index >= 0 || !IS_NULLSTR(old_phase_name))) {
        event_broadcast(formatf("{W%s{x enters phase {Y%s{x.\n\r",
            IS_NULLSTR(inst->def->display_title) ? inst->def->name : inst->def->display_title,
            inst->phase_name));
    }

    if (phase_changed)
        event_runtime_spawn_roster_for_phase(inst, inst->phase_name);

    if (phase_script_vnum > 0)
        event_runtime_run_phase_script(inst, phase_script_vnum, inst->phase_name);
}

static void event_runtime_run_reward_script(EVENT_INSTANCE *inst, long script_vnum,
    bool success, const char *reason)
{
    WNUM wnum;
    SCRIPT_DATA *script;
    EVENT_PART *part;
    bool ran = false;
    const char *trigger;
    const char *phrase;

    if (!inst || !inst->def || script_vnum <= 0)
        return;

    if (!resolve_widevnum(script_vnum, NULL, &wnum) || !wnum.pArea)
        return;

    script = get_script_index(wnum.pArea, wnum.vnum, PRG_APROG);
    if (!script)
        return;

    trigger = success ? "event_complete" : "event_fail";
    phrase = IS_NULLSTR(reason) ? trigger : reason;

    for (part = inst->participants; part; part = part->next) {
        if (!part->ch || IS_NPC(part->ch))
            continue;

        execute_script(script->vnum, script,
            NULL, NULL, NULL, NULL,
            wnum.pArea, NULL, NULL,
            part->ch, NULL, NULL, NULL, NULL, NULL,
            NULL,
            (char *)phrase,
            (char *)trigger,
            TRIG_NONE,
            (int)inst->def->uid,
            (int)inst->instance_id,
            success ? 1 : 0,
            part->team,
            inst->participant_count);
        ran = true;
    }

    if (ran)
        return;

    execute_script(script->vnum, script,
        NULL, NULL, NULL, NULL,
        wnum.pArea, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL,
        (char *)phrase,
        (char *)trigger,
        TRIG_NONE,
        (int)inst->def->uid,
        (int)inst->instance_id,
        success ? 1 : 0,
        0,
        inst->participant_count);
}

static bool event_runtime_complete_instance(EVENT_INSTANCE *inst, bool success,
    const char *reason, const char *default_success_msg)
{
    long reward_script = 0;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return false;

    reward_script = success ? inst->def->reward_success_script : inst->def->reward_failure_script;
    event_runtime_run_reward_script(inst, reward_script, success, reason);

    if (!event_stop_definition(inst->def))
        return false;

    if (!IS_NULLSTR(reason))
        event_broadcast(reason);
    else if (success && !IS_NULLSTR(inst->def->end_msg))
        event_broadcast(inst->def->end_msg);
    else if (success && !IS_NULLSTR(default_success_msg))
        event_broadcast(default_success_msg);
    else if (!success)
        event_broadcast("{REvent failed.{x\n\r");

    return true;
}

static bool event_runtime_apply_phase_by_index(EVENT_INSTANCE *inst, int phase_index)
{
    char phase_name[MIL];
    int phase_minutes = 0;
    long phase_script_vnum = 0;

    if (!inst || !inst->def || phase_index < 0)
        return false;

    if (!event_parse_phase_plan_step(inst->def->phase_plan, phase_index,
            phase_name, sizeof(phase_name), &phase_minutes, &phase_script_vnum))
        return false;

    event_runtime_apply_phase_step(inst, phase_index, phase_name, phase_minutes, phase_script_vnum);
    return true;
}

static void event_runtime_update_phase_timers(EVENT_INSTANCE *inst)
{
    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    while (inst->phase_due > 0 && current_time >= inst->phase_due) {
        if (!event_runtime_apply_phase_by_index(inst, inst->phase_index + 1)) {
            inst->phase_due = 0;
            inst->dirty = true;
            break;
        }
    }
}

static EVENT_INSTANCE *event_start_definition(EVTEDIT_DATA *evt, CHAR_DATA *starter)
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
    inst->progress_kills = 0;
    inst->progress_items = 0;
    inst->progress_goal = event_default_kill_goal(evt);
    inst->scope_area_uid = evt->scope_area_uid;
    inst->scope_floating = evt->scope_floating;

    if (inst->scope_floating && starter && starter->in_room && starter->in_room->area)
        inst->scope_area_uid = starter->in_room->area->uid;

    if (event_scope_requires_anchor(evt) && inst->scope_area_uid <= 0)
    {
        free_mem(inst, sizeof(*inst));
        return NULL;
    }

    inst->leader_phase = false;
    inst->phase_index = -1;
    inst->phase_due = 0;
    inst->phase_name[0] = '\0';
    inst->dirty = true;
    inst->next = event_active_head;
    event_active_head = inst;

    if (!event_runtime_apply_phase_by_index(inst, 0)) {
        strncpy(inst->phase_name, "active", sizeof(inst->phase_name) - 1);
        inst->phase_name[sizeof(inst->phase_name) - 1] = '\0';
        event_runtime_spawn_roster_for_phase(inst, inst->phase_name);
    }

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
            if (event_start_definition(evt, NULL)
                && !IS_SET(evt->flags, EVT_FLAG_NOANNOUNCE)
                && !IS_NULLSTR(evt->announce_msg))
                event_broadcast(evt->announce_msg);

            if (evt->sched_type == EVT_SCHED_CALENDAR && evt->sched_interval > 0) {
                time_t step = (time_t)evt->sched_interval * 60;
                time_t next_time = evt->scheduled_time + step;

                while (next_time <= current_time)
                    next_time += step;

                evt->next_auto_time = next_time;
            }

            evt->scheduled_time = 0;
            continue;
        }

        if (evt->sched_type == EVT_SCHED_CALENDAR) {
            if (evt->sched_interval <= 0 || evt->next_auto_time <= 0)
                continue;

            if (current_time < evt->next_auto_time)
                continue;

            if (event_find_active_def(evt))
                continue;

            if (evt->cooldown_until > current_time)
                continue;

            if (event_start_definition(evt, NULL)
                && !IS_SET(evt->flags, EVT_FLAG_NOANNOUNCE)
                && !IS_NULLSTR(evt->announce_msg))
                event_broadcast(evt->announce_msg);

            {
                time_t step = (time_t)evt->sched_interval * 60;
                while (evt->next_auto_time <= current_time)
                    evt->next_auto_time += step;
            }
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

        if (event_start_definition(evt, NULL)
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

        event_runtime_update_phase_timers(inst);

        if (inst->end_time > 0 && current_time >= inst->end_time) {
            if (!IS_NULLSTR(def->end_msg))
                event_broadcast(def->end_msg);
            event_stop_definition(def);
        }
    }
}

static void event_runtime_activate_leader_phase(EVENT_INSTANCE *inst)
{
    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    if (inst->leader_phase)
        return;

    inst->leader_phase = true;
    inst->phase_index = -1;
    inst->phase_due = 0;
    strncpy(inst->phase_name, "leader", sizeof(inst->phase_name) - 1);
    inst->phase_name[sizeof(inst->phase_name) - 1] = '\0';
    inst->dirty = true;
    event_runtime_spawn_roster_for_phase(inst, inst->phase_name);
}

void event_progress_record_kill(CHAR_DATA *killer, CHAR_DATA *victim)
{
    EVENT_INSTANCE *inst;

    if (!killer || !victim || IS_NPC(killer))
        return;

    for (inst = event_active_head; inst; inst = inst->next) {
        EVENT_PART *part;

        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;

        part = event_find_participant(inst, killer);
        if (!part)
            continue;

        if (!event_character_in_scope(inst, killer))
            continue;

        switch (inst->def->event_type) {
        case EVT_TYPE_INVASION:
            if (IS_NPC(victim)
                && event_mobile_matches_instance(victim, inst)
                && event_mobile_matches_participant_bracket(victim, part)) {
                part->kills++;
                inst->progress_kills++;
                inst->dirty = true;

                if (!inst->leader_phase
                    && inst->progress_goal > 0
                    && ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ALL)
                        ? event_all_brackets_met_goal(inst, inst->progress_goal, false)
                        : ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ANY)
                            ? event_any_bracket_met_goal(inst, inst->progress_goal, false)
                            : (inst->progress_kills >= inst->progress_goal)))) {
                    if (inst->def->leader_required) {
                        event_runtime_activate_leader_phase(inst);
                        event_broadcast("{YThe invasion leader has emerged! Slay the leader to end the invasion.{x\n\r");
                    } else if (event_runtime_complete_instance(inst, true, NULL,
                            "{YThe invasion force has been defeated. The invasion is over!{x\n\r")) {
                        return;
                    }
                }
            }
            break;

        case EVT_TYPE_WAR_FFA:
        case EVT_TYPE_WAR_GENOCIDE:
        case EVT_TYPE_WAR_JIHAD:
            if (!IS_NPC(victim)) {
                part->kills++;
                inst->progress_kills++;
                inst->dirty = true;

                if (inst->def->completion_goal > 0
                    && ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ALL)
                        ? event_all_brackets_met_goal(inst, inst->def->completion_goal, false)
                        : ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ANY)
                            ? event_any_bracket_met_goal(inst, inst->def->completion_goal, false)
                            : (inst->progress_kills >= inst->def->completion_goal)))
                    && event_runtime_complete_instance(inst, true, NULL,
                        "{YThe war objective has been reached. The war is over!{x\n\r")) {
                    return;
                }
            }
            break;

        case EVT_TYPE_BOSS:
            if (IS_NPC(victim)
                && event_mobile_matches_instance(victim, inst)
                && event_mobile_matches_participant_bracket(victim, part)
                && (!event_roster_has_boss_for_phase(inst, inst->phase_name)
                    || event_mobile_matches_roster_boss(inst, victim, inst->phase_name))) {
                int goal = inst->def->completion_goal > 0 ? inst->def->completion_goal : 1;

                part->kills++;
                inst->progress_kills++;
                inst->dirty = true;

                if (inst->progress_kills >= goal
                    && event_runtime_complete_instance(inst, true, NULL,
                        "{YThe boss has been defeated. Event complete!{x\n\r")) {
                    return;
                }
            }
            break;

        default:
            break;
        }
    }
}

void event_progress_record_collection_turnin(CHAR_DATA *ch, int items_turned)
{
    EVENT_INSTANCE *inst;

    if (!ch || IS_NPC(ch) || items_turned <= 0)
        return;

    for (inst = event_active_head; inst; inst = inst->next) {
        EVENT_PART *part;

        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->event_type != EVT_TYPE_COLLECTION)
            continue;

        part = event_find_participant(inst, ch);
        if (!part)
            continue;

        if (!event_character_in_scope(inst, ch))
            continue;

        part->items_turned += items_turned;
        inst->progress_items += items_turned;
        inst->dirty = true;

        if (inst->def->completion_goal > 0
            && ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ALL)
                ? event_all_brackets_met_goal(inst, inst->def->completion_goal, true)
                : ((event_progress_mode(inst->def) == EVT_PROGRESS_MODE_PER_BRACKET_ANY)
                    ? event_any_bracket_met_goal(inst, inst->def->completion_goal, true)
                    : (inst->progress_items >= inst->def->completion_goal)))
            && event_runtime_complete_instance(inst, true, NULL,
                "{YCollection objective reached. Event complete!{x\n\r")) {
            return;
        }
    }
}

bool event_progress_complete_invasion_leader(CHAR_DATA *killer, CHAR_DATA *victim)
{
    EVENT_INSTANCE *inst;
    const char *leader_phase_name = "leader";

    if (!killer || !victim || IS_NPC(killer) || !IS_NPC(victim))
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->event_type != EVT_TYPE_INVASION)
            continue;
        if (!inst->leader_phase)
            continue;
        if (!event_mobile_matches_instance(victim, inst))
            continue;
        if (!event_find_participant(inst, killer))
            continue;
        if (event_roster_has_boss_for_phase(inst, leader_phase_name)
            && !event_mobile_matches_roster_boss(inst, victim, leader_phase_name))
            continue;

        if (event_runtime_complete_instance(inst, true, NULL,
                "{YThe invasion leader has fallen. The invasion is over!{x\n\r")) {
            return true;
        }
    }

    return false;
}

void event_tag_mobile_spawn(CHAR_DATA *mob, long event_uid, uint32_t instance_id)
{
    if (!mob)
        return;

    mob->event_source_uid = UMAX(0, event_uid);
    mob->event_source_instance_id = instance_id;
    mob->event_source_bracket = 0;
}

void event_tag_object_spawn(OBJ_DATA *obj, long event_uid, uint32_t instance_id)
{
    if (!obj)
        return;

    obj->event_source_uid = UMAX(0, event_uid);
    obj->event_source_instance_id = instance_id;
    obj->event_source_bracket = 0;
}

void event_set_mobile_spawn_bracket(CHAR_DATA *mob, int bracket)
{
    if (!mob)
        return;

    mob->event_source_bracket = UMAX(0, bracket);
}

void event_set_object_spawn_bracket(OBJ_DATA *obj, int bracket)
{
    if (!obj)
        return;

    obj->event_source_bracket = UMAX(0, bracket);
}

bool event_get_mobile_spawn_source(const CHAR_DATA *mob, long *event_uid, uint32_t *instance_id)
{
    if (!mob)
        return false;

    if (event_uid)
        *event_uid = mob->event_source_uid;
    if (instance_id)
        *instance_id = mob->event_source_instance_id;

    return mob->event_source_uid > 0;
}

bool event_get_object_spawn_source(const OBJ_DATA *obj, long *event_uid, uint32_t *instance_id)
{
    if (!obj)
        return false;

    if (event_uid)
        *event_uid = obj->event_source_uid;
    if (instance_id)
        *instance_id = obj->event_source_instance_id;

    return obj->event_source_uid > 0;
}

bool event_get_mobile_spawn_bracket(const CHAR_DATA *mob, int *bracket)
{
    if (!mob)
        return false;

    if (bracket)
        *bracket = mob->event_source_bracket;

    return mob->event_source_bracket > 0;
}

bool event_get_object_spawn_bracket(const OBJ_DATA *obj, int *bracket)
{
    if (!obj)
        return false;

    if (bracket)
        *bracket = obj->event_source_bracket;

    return obj->event_source_bracket > 0;
}

bool event_get_character_active_bracket(const CHAR_DATA *ch, long *event_uid, uint32_t *instance_id, int *bracket)
{
    EVENT_INSTANCE *inst;
    EVENT_PART *part;

    if (event_uid)
        *event_uid = 0;
    if (instance_id)
        *instance_id = 0;
    if (bracket)
        *bracket = 0;

    if (!ch)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;

        part = event_find_participant(inst, (CHAR_DATA *)ch);
        if (!part)
            continue;

        if (event_uid)
            *event_uid = inst->def->uid;
        if (instance_id)
            *instance_id = inst->instance_id;
        if (bracket)
            *bracket = part->team;
        return true;
    }

    return false;
}

bool event_runtime_get_source_progress(long event_uid, uint32_t instance_id, int *kills, int *items, int *goal)
{
    EVENT_INSTANCE *inst;

    if (kills)
        *kills = 0;
    if (items)
        *items = 0;
    if (goal)
        *goal = 0;

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        if (kills)
            *kills = inst->progress_kills;
        if (items)
            *items = inst->progress_items;
        if (goal) {
            if (inst->def->event_type == EVT_TYPE_INVASION)
                *goal = inst->progress_goal;
            else
                *goal = inst->def->completion_goal;
        }

        return true;
    }

    return false;
}

bool event_runtime_is_source_leader_phase(long event_uid, uint32_t instance_id, bool *leader_phase)
{
    EVENT_INSTANCE *inst;

    if (leader_phase)
        *leader_phase = false;

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        if (leader_phase)
            *leader_phase = inst->leader_phase;
        return true;
    }

    return false;
}

static bool event_runtime_goal_met(const EVENT_INSTANCE *inst, int goal, bool items)
{
    evt_progress_mode_t mode;

    if (!inst || !inst->def || goal <= 0)
        return false;

    mode = event_progress_mode(inst->def);
    if (mode == EVT_PROGRESS_MODE_PER_BRACKET_ALL)
        return event_all_brackets_met_goal(inst, goal, items);
    if (mode == EVT_PROGRESS_MODE_PER_BRACKET_ANY)
        return event_any_bracket_met_goal(inst, goal, items);

    return items ? (inst->progress_items >= goal) : (inst->progress_kills >= goal);
}

static void event_runtime_resolve_completion(EVENT_INSTANCE *inst)
{
    int goal;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    switch (inst->def->event_type) {
    case EVT_TYPE_INVASION:
        goal = inst->progress_goal;
        if (!inst->leader_phase && event_runtime_goal_met(inst, goal, false)) {
            if (inst->def->leader_required) {
                event_runtime_activate_leader_phase(inst);
                event_broadcast("{YThe invasion leader has emerged! Slay the leader to end the invasion.{x\n\r");
            } else if (event_stop_definition(inst->def)) {
                if (!IS_NULLSTR(inst->def->end_msg))
                    event_broadcast(inst->def->end_msg);
                else
                    event_broadcast("{YThe invasion force has been defeated. The invasion is over!{x\n\r");
            }
        }
        break;

    case EVT_TYPE_COLLECTION:
        goal = inst->def->completion_goal;
        if (event_runtime_goal_met(inst, goal, true)
            && event_runtime_complete_instance(inst, true, NULL,
                "{YCollection objective reached. Event complete!{x\n\r")) {
        }
        break;

    case EVT_TYPE_WAR_FFA:
    case EVT_TYPE_WAR_GENOCIDE:
    case EVT_TYPE_WAR_JIHAD:
        goal = inst->def->completion_goal;
        if (event_runtime_goal_met(inst, goal, false)
            && event_runtime_complete_instance(inst, true, NULL,
                "{YThe war objective has been reached. The war is over!{x\n\r")) {
        }
        break;

    case EVT_TYPE_BOSS:
        goal = inst->def->completion_goal > 0 ? inst->def->completion_goal : 1;
        if (event_runtime_goal_met(inst, goal, false)
            && event_runtime_complete_instance(inst, true, NULL,
                "{YThe boss has been defeated. Event complete!{x\n\r")) {
        }
        break;

    default:
        break;
    }
}

bool event_runtime_adjust_progress(const char *event_token, int kills_delta, int items_delta)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    if (kills_delta != 0)
        inst->progress_kills = UMAX(0, inst->progress_kills + kills_delta);
    if (items_delta != 0)
        inst->progress_items = UMAX(0, inst->progress_items + items_delta);

    inst->dirty = true;
    event_runtime_resolve_completion(inst);
    return true;
}

bool event_runtime_get_progress(const char *event_token, int *kills, int *items, int *goal)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (kills)
        *kills = 0;
    if (items)
        *items = 0;
    if (goal)
        *goal = 0;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    if (kills)
        *kills = inst->progress_kills;
    if (items)
        *items = inst->progress_items;
    if (goal) {
        if (evt->event_type == EVT_TYPE_INVASION)
            *goal = inst->progress_goal;
        else
            *goal = evt->completion_goal;
    }

    return true;
}

bool event_runtime_get_source_phase(long event_uid, uint32_t instance_id, char *phase_out, int phase_size)
{
    EVENT_INSTANCE *inst;

    if (phase_out && phase_size > 0)
        phase_out[0] = '\0';

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        if (phase_out && phase_size > 0) {
            const char *phase = !IS_NULLSTR(inst->phase_name)
                ? inst->phase_name
                : (inst->leader_phase ? "leader" : "active");
            strncpy(phase_out, phase, phase_size - 1);
            phase_out[phase_size - 1] = '\0';
        }

        return true;
    }

    return false;
}

bool event_runtime_get_phase(const char *event_token, char *phase_out, int phase_size)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (phase_out && phase_size > 0)
        phase_out[0] = '\0';

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    if (phase_out && phase_size > 0) {
        const char *phase = !IS_NULLSTR(inst->phase_name)
            ? inst->phase_name
            : (inst->leader_phase ? "leader" : "active");
        strncpy(phase_out, phase, phase_size - 1);
        phase_out[phase_size - 1] = '\0';
    }

    return true;
}

bool event_runtime_is_leader_phase(const char *event_token, bool *leader_phase)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (leader_phase)
        *leader_phase = false;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    if (leader_phase)
        *leader_phase = inst->leader_phase;

    return true;
}

bool event_runtime_set_goal(const char *event_token, int goal)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    goal = UMAX(0, goal);

    if (evt->event_type == EVT_TYPE_INVASION)
        inst->progress_goal = goal;
    else
        evt->completion_goal = goal;

    inst->dirty = true;
    event_runtime_resolve_completion(inst);
    return true;
}

bool event_runtime_set_phase(const char *event_token, const char *phase_name)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;
    int phase_index = 0;
    char planned_phase[MIL];
    int phase_minutes = 0;
    long phase_script_vnum = 0;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    while (event_parse_phase_plan_step(evt->phase_plan, phase_index,
        planned_phase, sizeof(planned_phase), &phase_minutes, &phase_script_vnum)) {
        if (!IS_NULLSTR(phase_name) && !str_cmp(phase_name, planned_phase)) {
            event_runtime_apply_phase_step(inst, phase_index, planned_phase,
                phase_minutes, phase_script_vnum);
            return true;
        }
        phase_index++;
    }

    if (evt->event_type == EVT_TYPE_INVASION) {
        if (IS_NULLSTR(phase_name) || !str_cmp(phase_name, "active") || !str_cmp(phase_name, "normal")) {
            event_runtime_apply_phase_step(inst, -1, "active", 0, 0);
            return true;
        }

        if (!str_cmp(phase_name, "leader") || !str_cmp(phase_name, "leader_phase")) {
            event_runtime_apply_phase_step(inst, -1, "leader", 0, 0);
            return true;
        }
    }

    if (!IS_NULLSTR(phase_name)) {
        event_runtime_apply_phase_step(inst, -1, phase_name, 0, 0);
        return true;
    }

    return false;
}

bool event_runtime_next_phase(const char *event_token)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    return event_runtime_apply_phase_by_index(inst, inst->phase_index + 1);
}

bool event_runtime_finish(const char *event_token, bool success, const char *reason)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst)
        return false;

    return event_runtime_complete_instance(inst, success, reason, NULL);
}

static json_t *evtedit_roster_to_json(const EVT_ROSTER_ENTRY *head)
{
    const EVT_ROSTER_ENTRY *entry;
    json_t *array = json_array();

    for (entry = head; entry; entry = entry->next) {
        json_t *obj = json_object();
        json_object_set_new(obj, "kind", json_integer(entry->kind));
        json_object_set_new(obj, "vnum", json_integer(entry->vnum));
        json_object_set_new(obj, "count", json_integer(entry->count));
        json_object_set_new(obj, "chance", json_integer(entry->chance));
        json_object_set_new(obj, "min_level", json_integer(entry->min_level));
        json_object_set_new(obj, "max_level", json_integer(entry->max_level));
        json_object_set_new(obj, "boss", json_integer(entry->boss ? 1 : 0));
        json_object_set_new(obj, "phase", json_string_safe(entry->phase));
        json_array_append_new(array, obj);
    }

    return array;
}

static void evtedit_roster_load_json(EVTEDIT_DATA *evt, json_t *array)
{
    size_t i;
    json_t *entry_obj;

    if (!evt)
        return;

    evtedit_roster_free(&evt->roster);

    if (!json_is_array(array))
        return;

    json_array_foreach(array, i, entry_obj) {
        EVT_ROSTER_ENTRY *entry;

        if (!json_is_object(entry_obj))
            continue;

        entry = alloc_mem(sizeof(*entry));
        memset(entry, 0, sizeof(*entry));

        entry->kind = json_get_int(entry_obj, "kind", EVT_ROSTER_NPC);
        if (entry->kind != EVT_ROSTER_NPC && entry->kind != EVT_ROSTER_OBJECT)
            entry->kind = EVT_ROSTER_NPC;

        entry->vnum = (long)json_get_int(entry_obj, "vnum", 0);
        entry->count = json_get_int(entry_obj, "count", 1);
        entry->chance = json_get_int(entry_obj, "chance", 100);
        entry->min_level = json_get_int(entry_obj, "min_level", 0);
        entry->max_level = json_get_int(entry_obj, "max_level", 0);
        entry->boss = json_get_int(entry_obj, "boss", 0) != 0;
        event_copy_trimmed(entry->phase, sizeof(entry->phase),
            json_get_string(entry_obj, "phase", "any"));

        entry->count = URANGE(1, entry->count, 10000);
        entry->chance = URANGE(1, entry->chance, 100);
        entry->min_level = URANGE(0, entry->min_level, 32767);
        entry->max_level = URANGE(0, entry->max_level, 32767);
        if (entry->max_level > 0 && entry->min_level > entry->max_level)
            entry->min_level = entry->max_level;
        if (entry->kind != EVT_ROSTER_NPC)
            entry->boss = false;
        if (IS_NULLSTR(entry->phase)) {
            strncpy(entry->phase, "any", sizeof(entry->phase) - 1);
            entry->phase[sizeof(entry->phase) - 1] = '\0';
        }

        if (entry->vnum <= 0) {
            free_mem(entry, sizeof(*entry));
            continue;
        }

        evtedit_roster_append(evt, entry);
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
    json_object_set_new(obj, "scope_area_uid", json_integer(evt->scope_area_uid));
    json_object_set_new(obj, "scope_floating", json_integer(evt->scope_floating ? 1 : 0));
    json_object_set_new(obj, "sched_type", json_integer(evt->sched_type));
    json_object_set_new(obj, "sched_interval", json_integer(evt->sched_interval));
    json_object_set_new(obj, "sched_variance", json_integer(evt->sched_variance));
    json_object_set_new(obj, "sched_duration", json_integer(evt->sched_duration));
    json_object_set_new(obj, "sched_cooldown", json_integer(evt->sched_cooldown));
    json_object_set_new(obj, "min_level", json_integer(evt->min_level));
    json_object_set_new(obj, "max_level", json_integer(evt->max_level));
    json_object_set_new(obj, "min_players", json_integer(evt->min_players));
    json_object_set_new(obj, "max_players", json_integer(evt->max_players));
    json_object_set_new(obj, "completion_goal", json_integer(evt->completion_goal));
    json_object_set_new(obj, "leader_required", json_integer(evt->leader_required ? 1 : 0));
    json_object_set_new(obj, "display_title", json_string_safe(evt->display_title));
    json_object_set_new(obj, "short_summary", json_string_safe(evt->short_summary));
    json_object_set_new(obj, "news_slug", json_string_safe(evt->news_slug));
    json_object_set_new(obj, "news_announcement", json_string_safe(evt->news_announcement));
    json_object_set_new(obj, "news_body", json_string_safe(evt->news_body));
    json_object_set_new(obj, "theme_tags", json_string_safe(evt->theme_tags));
    json_object_set_new(obj, "spawn_brackets", json_string_safe(evt->spawn_brackets));
    json_object_set_new(obj, "collection_brackets", json_string_safe(evt->collection_brackets));
    json_object_set_new(obj, "bracket_mode", json_string_safe(evt->bracket_mode));
    json_object_set_new(obj, "progress_aggregation", json_string_safe(evt->progress_aggregation));
    json_object_set_new(obj, "phase_plan", json_string_safe(evt->phase_plan));
    json_object_set_new(obj, "reward_success_script", json_integer(evt->reward_success_script));
    json_object_set_new(obj, "reward_failure_script", json_integer(evt->reward_failure_script));
    json_object_set_new(obj, "enabled", json_integer(evt->enabled ? 1 : 0));
    json_object_set_new(obj, "flags", json_integer(evt->flags));
    json_object_set_new(obj, "comments", json_string_safe(evt->comments));
    json_object_set_new(obj, "roster", evtedit_roster_to_json(evt->roster));
    json_object_set_new(obj, "scheduled_time", json_integer((json_int_t)evt->scheduled_time));

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
        evt->scope_area_uid = (long)json_get_int(entry, "scope_area_uid", 0);
        evt->scope_floating = json_get_int(entry, "scope_floating", 0) != 0;
        evt->sched_type = (int16_t)json_get_int(entry, "sched_type", EVT_SCHED_MANUAL);
        evt->sched_interval = (int16_t)json_get_int(entry, "sched_interval", 60);
        evt->sched_variance = (int16_t)json_get_int(entry, "sched_variance", 0);
        evt->sched_duration = (int16_t)json_get_int(entry, "sched_duration", 60);
        evt->sched_cooldown = (int16_t)json_get_int(entry, "sched_cooldown", 0);
        evt->min_level = (int16_t)json_get_int(entry, "min_level", 0);
        evt->max_level = (int16_t)json_get_int(entry, "max_level", 0);
        evt->min_players = (int16_t)json_get_int(entry, "min_players", 0);
        evt->max_players = (int16_t)json_get_int(entry, "max_players", 0);
        evt->completion_goal = (int16_t)json_get_int(entry, "completion_goal", 0);
        evt->leader_required = json_get_int(entry, "leader_required", 1) != 0;
        evt->display_title = str_dup(json_get_string(entry, "display_title", ""));
        evt->short_summary = str_dup(json_get_string(entry, "short_summary", ""));
        evt->news_slug = str_dup(json_get_string(entry, "news_slug", ""));
        evt->news_announcement = str_dup(json_get_string(entry, "news_announcement", ""));
        evt->news_body = str_dup(json_get_string(entry, "news_body", ""));
        evt->theme_tags = str_dup(json_get_string(entry, "theme_tags", ""));
        evt->spawn_brackets = str_dup(json_get_string(entry, "spawn_brackets", ""));
        evt->collection_brackets = str_dup(json_get_string(entry, "collection_brackets", ""));
        evt->bracket_mode = str_dup(json_get_string(entry, "bracket_mode", "auto_by_level"));
        evt->progress_aggregation = str_dup(json_get_string(entry, "progress_aggregation", "shared"));
        evt->phase_plan = str_dup(json_get_string(entry, "phase_plan", ""));
        evt->reward_success_script = (long)json_get_int(entry, "reward_success_script", 0);
        evt->reward_failure_script = (long)json_get_int(entry, "reward_failure_script", 0);
        evt->enabled = json_get_int(entry, "enabled", 1) != 0;
        evt->flags = (long)json_get_int(entry, "flags", 0);
        evt->comments = str_dup(json_get_string(entry, "comments", ""));
        evtedit_roster_load_json(evt, json_object_get(entry, "roster"));
        evt->scheduled_time = (time_t)json_get_int(entry, "scheduled_time", 0);
        evt->cooldown_until = 0;
        evt->next_auto_time = 0;

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
    { "scopeanchor", evtedit_scopeanchor },
    { "scopefloating", evtedit_scopefloating },
    { "schedule",  evtedit_schedule },
    { "interval",  evtedit_interval },
    { "variance",  evtedit_variance },
    { "duration",  evtedit_duration },
    { "cooldown",  evtedit_cooldown },
    { "minlevel",  evtedit_minlevel },
    { "maxlevel",  evtedit_maxlevel },
    { "minplayers", evtedit_minplayers },
    { "maxplayers", evtedit_maxplayers },
    { "goal",      evtedit_goal },
    { "leaderrequired", evtedit_leaderrequired },
    { "title",     evtedit_title },
    { "summary",   evtedit_summary },
    { "newsslug",  evtedit_newsslug },
    { "newsannounce", evtedit_newsannounce },
    { "newsbody",  evtedit_newsbody },
    { "themetags", evtedit_themetags },
    { "roster",    evtedit_roster },
    { "spawnbrackets", evtedit_spawnbrackets },
    { "collectionbrackets", evtedit_collectionbrackets },
    { "bracketmode", evtedit_bracketmode },
    { "progressagg", evtedit_progressagg },
    { "phaseplan", evtedit_phaseplan },
    { "rewardsuccess", evtedit_rewardsuccess },
    { "rewardfail", evtedit_rewardfail },
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
        send_to_char("\n\r", ch);
        send_to_char("Bracket format: ordered non-overlapping ranges (example: 1-50,51-90,91+).\n\r", ch);
        send_to_char("bracketmode: auto_by_level|open|manual\n\r", ch);
        send_to_char("scopeanchor: <area_uid|here|clear>\n\r", ch);
        send_to_char("scopefloating: [on|off]\n\r", ch);
        send_to_char("roster: list|addnpc|addobj|boss|phase|remove|clear\n\r", ch);
        send_to_char("progressagg: shared|total|per_bracket_any|per_bracket|per_bracket_all_required\n\r", ch);
        send_to_char("phaseplan: use subcommands (list/add/insert/set/name/minutes/script/remove/clear)\n\r", ch);
        send_to_char("rewardsuccess/rewardfail: <scriptvnum|0>\n\r", ch);
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
        send_to_char("        event news <uid|name>\n\r", ch);
        send_to_char("        event enabled\n\r", ch);
        send_to_char("        event enable|disable\n\r", ch);
        send_to_char("        event status\n\r", ch);
        send_to_char("        event join [uid|name]\n\r", ch);
        send_to_char("        event leave [uid|name]\n\r", ch);
        send_to_char("        event start <uid|name>\n\r", ch);
        send_to_char("        event stop <uid|name>\n\r", ch);
        send_to_char("        event schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
        send_to_char("        event tick\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("Passive/worldstate events are status-only and cannot be joined.\n\r", ch);
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
                flag_name(evt_type_flags, evt->event_type),
                flag_name(evt_sched_flags, evt->sched_type),
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
            EVENT_PART *self = event_find_participant(inst, ch);
            bool joined = self != NULL;

            if (!inst->def)
                continue;

            if (inst->end_time > current_time)
                rem = (long)(inst->end_time - current_time);

            if (joined)
                in_any = true;

            printf_to_char(ch,
                "  {W%-24.24s{x [%s] participants=%d%s%s",
                inst->def->name,
                rem > 0 ? formatf("%ldm%02lds left", rem / 60, rem % 60) : "no timer",
                inst->participant_count,
                joined ? " {G(joined){x" : "",
                (IS_SET(inst->def->flags, EVT_FLAG_PASSIVE) || inst->def->event_type == EVT_TYPE_WORLDSTATE)
                    ? " {C(passive){x" : "");

            if (self && self->team > 0)
                printf_to_char(ch, " {Wbracket:{x %d", self->team);

            if (!IS_NULLSTR(inst->phase_name))
                printf_to_char(ch, " {Wphase:{x %s", inst->phase_name);

            if (inst->def->event_type == EVT_TYPE_INVASION && inst->progress_goal > 0)
                printf_to_char(ch, " {WKills:{x %d/%d%s",
                    inst->progress_kills,
                    inst->progress_goal,
                    inst->leader_phase ? " {Y(leader phase){x" : "");
            else if (inst->def->event_type == EVT_TYPE_COLLECTION)
                printf_to_char(ch, " {WTurn-ins:{x %d%s",
                    inst->progress_items,
                    inst->def->completion_goal > 0
                        ? formatf("/%d", inst->def->completion_goal)
                        : "");
            else if ((inst->def->event_type == EVT_TYPE_WAR_FFA
                || inst->def->event_type == EVT_TYPE_WAR_GENOCIDE
                || inst->def->event_type == EVT_TYPE_WAR_JIHAD)
                && inst->def->completion_goal > 0)
                printf_to_char(ch, " {WKills:{x %d/%d",
                    inst->progress_kills,
                    inst->def->completion_goal);
            else if (inst->def->event_type == EVT_TYPE_BOSS)
                printf_to_char(ch, " {WKills:{x %d/%d",
                    inst->progress_kills,
                    inst->def->completion_goal > 0 ? inst->def->completion_goal : 1);

            send_to_char("\n\r", ch);
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

        if (IS_SET(inst->def->flags, EVT_FLAG_PASSIVE) || inst->def->event_type == EVT_TYPE_WORLDSTATE) {
            send_to_char("That event is passive and cannot be joined.\n\r", ch);
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

        {
            EVENT_PART *self = event_find_participant(inst, ch);
            printf_to_char(ch, "You join event '%s'%s%s.\n\r",
                inst->def->name,
                (self && self->team > 0) ? " (bracket " : "",
                (self && self->team > 0) ? formatf("%d)", self->team) : "");
        }
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

    if (!str_cmp(cmd, "news")) {
        argument = one_argument(argument, target);
        if (IS_NULLSTR(target)) {
            send_to_char("Syntax: event news <uid|name>\n\r", ch);
            return;
        }

        evt = event_lookup_definition(target);
        if (!evt) {
            send_to_char("No event found by that uid or name.\n\r", ch);
            return;
        }

        printf_to_char(ch, "{WEvent News Preview:{x %s ({W%ld{x)\n\r", evt->name, evt->uid);
        printf_to_char(ch, "{WTitle:{x %s\n\r",
            IS_NULLSTR(evt->display_title) ? "(not set)" : evt->display_title);
        printf_to_char(ch, "{WSummary:{x %s\n\r",
            IS_NULLSTR(evt->short_summary) ? "(not set)" : evt->short_summary);
        printf_to_char(ch, "{WNews Slug:{x %s\n\r",
            IS_NULLSTR(evt->news_slug) ? "(not set)" : evt->news_slug);
        printf_to_char(ch, "{WTheme Tags:{x %s\n\r",
            IS_NULLSTR(evt->theme_tags) ? "(not set)" : evt->theme_tags);
        printf_to_char(ch, "{WAnnouncement:{x %s\n\r",
            IS_NULLSTR(evt->news_announcement) ? "(not set)" : evt->news_announcement);
        printf_to_char(ch, "{WNews Body:{x %s\n\r",
            IS_NULLSTR(evt->news_body) ? "(not set)" : evt->news_body);
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
        EVENT_PART *self = NULL;

        inst = event_find_active_def(evt);
        if (inst)
            uptime = (long)(current_time - inst->started_at);
        if (inst && inst->end_time > current_time)
            rem = (long)(inst->end_time - current_time);

        printf_to_char(ch, "{WEvent:{x %s ({W%ld{x)\n\r", evt->name, evt->uid);
        if (!IS_NULLSTR(evt->display_title))
            printf_to_char(ch, "{WTitle:{x %s\n\r", evt->display_title);
        if (!IS_NULLSTR(evt->short_summary))
            printf_to_char(ch, "{WSummary:{x %s\n\r", evt->short_summary);
        printf_to_char(ch, "{WType:{x %s  {WScope:{x %s  {WSchedule:{x %s\n\r",
            event_enum_name(evt_type_flags, evt->event_type),
            event_enum_name(evt_scope_flags, evt->scope_type),
            event_enum_name(evt_sched_flags, evt->sched_type));
        printf_to_char(ch, "{WScope Anchor:{x area_uid=%ld  floating=%s\n\r",
            evt->scope_area_uid,
            evt->scope_floating ? "{GYes{x" : "{RNo{x");
        printf_to_char(ch, "{WTiming:{x interval=%d variance=%d duration=%d cooldown=%d\n\r",
            evt->sched_interval, evt->sched_variance,
            evt->sched_duration, evt->sched_cooldown);
        printf_to_char(ch, "{WEligibility:{x minlvl=%d maxlvl=%d minplayers=%d maxplayers=%d\n\r",
            evt->min_level, evt->max_level, evt->min_players, evt->max_players);
        printf_to_char(ch, "{WCompletion:{x goal=%d leader_required=%s\n\r",
            evt->completion_goal,
            evt->leader_required ? "{GYes{x" : "{RNo{x");
        printf_to_char(ch, "{WBrackets:{x mode=%s aggregation=%s\n\r",
            IS_NULLSTR(evt->bracket_mode) ? "(not set)" : evt->bracket_mode,
            IS_NULLSTR(evt->progress_aggregation) ? "(not set)" : evt->progress_aggregation);
        if (!IS_NULLSTR(evt->phase_plan))
            printf_to_char(ch, "{WPhase Plan:{x %s\n\r", evt->phase_plan);
        if (evt->reward_success_script > 0 || evt->reward_failure_script > 0)
            printf_to_char(ch, "{WReward Hooks:{x success=%ld fail=%ld\n\r",
                evt->reward_success_script, evt->reward_failure_script);
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

        if (inst)
            printf_to_char(ch, "{WRuntime Scope Anchor:{x area_uid=%ld (floating=%s)\n\r",
                inst->scope_area_uid,
                inst->scope_floating ? "{GYes{x" : "{RNo{x");

        if (inst) {
            self = event_find_participant(inst, ch);

            if (evt->event_type == EVT_TYPE_INVASION && inst->progress_goal > 0)
                printf_to_char(ch, "{WProgress:{x kills=%d/%d%s\n\r",
                    inst->progress_kills,
                    inst->progress_goal,
                    inst->leader_phase ? " {Y(leader phase active){x" : "");
            else if (evt->event_type == EVT_TYPE_COLLECTION)
                printf_to_char(ch, "{WProgress:{x turn-ins=%d%s\n\r",
                    inst->progress_items,
                    evt->completion_goal > 0 ? formatf("/%d", evt->completion_goal) : "");
            else if ((evt->event_type == EVT_TYPE_WAR_FFA
                || evt->event_type == EVT_TYPE_WAR_GENOCIDE
                || evt->event_type == EVT_TYPE_WAR_JIHAD)
                && evt->completion_goal > 0)
                printf_to_char(ch, "{WProgress:{x kills=%d/%d\n\r",
                    inst->progress_kills,
                    evt->completion_goal);
            else if (evt->event_type == EVT_TYPE_BOSS)
                printf_to_char(ch, "{WProgress:{x kills=%d/%d\n\r",
                    inst->progress_kills,
                    evt->completion_goal > 0 ? evt->completion_goal : 1);

            if (self)
                printf_to_char(ch, "{WYour Contribution:{x kills=%d turn-ins=%d%s\n\r",
                    self->kills,
                    self->items_turned,
                    self->team > 0 ? formatf(" bracket=%d", self->team) : "");

            if (!IS_NULLSTR(inst->phase_name))
                printf_to_char(ch, "{WCurrent Phase:{x %s\n\r", inst->phase_name);

            if (inst->phase_due > current_time)
                printf_to_char(ch, "{WNext Phase:{x in %ldm%02lds\n\r",
                    (long)(inst->phase_due - current_time) / 60,
                    (long)(inst->phase_due - current_time) % 60);
        }

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
        if (!IS_NULLSTR(evt->news_slug))
            printf_to_char(ch, "{WNews Slug:{x %s\n\r", evt->news_slug);
        if (!IS_NULLSTR(evt->news_announcement))
            printf_to_char(ch, "{WNews Announcement:{x %s\n\r", evt->news_announcement);
        if (!IS_NULLSTR(evt->theme_tags))
            printf_to_char(ch, "{WTheme Tags:{x %s\n\r", evt->theme_tags);
        if (!IS_NULLSTR(evt->spawn_brackets))
            printf_to_char(ch, "{WSpawn Brackets:{x %s\n\r", evt->spawn_brackets);
        if (!IS_NULLSTR(evt->collection_brackets))
            printf_to_char(ch, "{WCollection Brackets:{x %s\n\r", evt->collection_brackets);

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

        if (event_scope_requires_anchor(evt)
        && !evt->scope_floating
        && evt->scope_area_uid <= 0) {
            send_to_char("This scoped event needs a scopeanchor before it can start.\n\r", ch);
            return;
        }

        if (event_scope_requires_anchor(evt)
        && evt->scope_floating
        && (!ch->in_room || !ch->in_room->area)) {
            send_to_char("Floating scoped events must be started from a valid room.\n\r", ch);
            return;
        }

        inst = event_start_definition(evt, ch);
        if (!inst) {
            send_to_char("Event could not be started (already active or missing scope anchor).\n\r", ch);
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
        evt->next_auto_time = 0;
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
            event_enum_name(evt_type_flags, evt->event_type),
            event_enum_name(evt_sched_flags, evt->sched_type),
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
    olc_display_string(ctx, theme, "Title:", "title",
        IS_NULLSTR(evt->display_title) ? "" : evt->display_title);
    olc_display_string(ctx, theme, "Summary:", "summary",
        IS_NULLSTR(evt->short_summary) ? "" : evt->short_summary);
    olc_display_type(ctx, theme, "Type:", "type", evt_type_flags, evt->event_type);
    olc_display_string(ctx, theme, "Enabled:", "enabled",
        evt->enabled ? "Yes" : "No");
    olc_display_type(ctx, theme, "Scope:", "scope", evt_scope_flags, evt->scope_type);
    olc_display_number(ctx, theme, "Scope Anchor Area UID:", "scopeanchor", evt->scope_area_uid);
    olc_display_string(ctx, theme, "Scope Floating:", "scopefloating",
        evt->scope_floating ? "Yes" : "No");
    olc_display_flags(ctx, theme, "Flags:", "flags", evt_flags, evt->flags);
}

static void evtedit_show_schedule_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_type(ctx, theme, "Schedule:", "schedule", evt_sched_flags, evt->sched_type);
    olc_display_string(ctx, theme, "Scheduled At:", "schedule at",
        event_format_time_short(evt->scheduled_time));
    olc_display_number(ctx, theme, "Interval:", "interval", evt->sched_interval);
    olc_display_number(ctx, theme, "Variance:", "variance", evt->sched_variance);
    olc_display_number(ctx, theme, "Duration:", "duration", evt->sched_duration);
    olc_display_number(ctx, theme, "Cooldown:", "cooldown", evt->sched_cooldown);
    olc_display_number(ctx, theme, "Min Level:", "minlevel", evt->min_level);
    olc_display_number(ctx, theme, "Max Level:", "maxlevel", evt->max_level);
    olc_display_number(ctx, theme, "Min Players:", "minplayers", evt->min_players);
    olc_display_number(ctx, theme, "Max Players:", "maxplayers", evt->max_players);
    olc_display_number(ctx, theme, "Completion Goal:", "goal", evt->completion_goal);
    olc_display_string(ctx, theme, "Leader Required:", "leaderrequired",
        evt->leader_required ? "Yes" : "No");
    olc_display_string(ctx, theme, "Bracket Mode:", "bracketmode",
        IS_NULLSTR(evt->bracket_mode) ? "" : evt->bracket_mode);
    olc_display_string(ctx, theme, "Progress Aggregation:", "progressagg",
        IS_NULLSTR(evt->progress_aggregation) ? "" : evt->progress_aggregation);
    olc_display_string(ctx, theme, "Phase Plan:", "phaseplan",
        IS_NULLSTR(evt->phase_plan) ? "" : evt->phase_plan);
    olc_display_number(ctx, theme, "Reward Success Script:", "rewardsuccess",
        evt->reward_success_script);
    olc_display_number(ctx, theme, "Reward Fail Script:", "rewardfail",
        evt->reward_failure_script);
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
    olc_display_string(ctx, theme, "News Slug:", "newsslug",
        IS_NULLSTR(evt->news_slug) ? "" : evt->news_slug);
    olc_display_text(ctx, theme, "News Announce:", "newsannounce",
        IS_NULLSTR(evt->news_announcement) ? NULL : evt->news_announcement);
    olc_display_text(ctx, theme, "News Body:", "newsbody",
        IS_NULLSTR(evt->news_body) ? NULL : evt->news_body);
    olc_display_string(ctx, theme, "Theme Tags:", "themetags",
        IS_NULLSTR(evt->theme_tags) ? "" : evt->theme_tags);
    olc_display_text(ctx, theme, "Spawn Brackets:", "spawnbrackets",
        IS_NULLSTR(evt->spawn_brackets) ? NULL : evt->spawn_brackets);
    olc_display_text(ctx, theme, "Collection Brackets:", "collectionbrackets",
        IS_NULLSTR(evt->collection_brackets) ? NULL : evt->collection_brackets);
    olc_display_number(ctx, theme, "Roster Entries:", "roster list",
        evtedit_roster_count(evt->roster));
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

EVTEDIT(evtedit_scopeanchor)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    AREA_DATA *area;

    if (!evt)
        return false;

    if (IS_NULLSTR(argument)) {
        send_to_char("scopeanchor <area_uid|here|clear>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear") || !str_cmp(argument, "none")) {
        evt->scope_area_uid = 0;
        return evtedit_save_after_change(ch);
    }

    if (!str_cmp(argument, "here")) {
        if (!ch->in_room || !ch->in_room->area) {
            send_to_char("You are not in a valid room/area.\n\r", ch);
            return false;
        }

        evt->scope_area_uid = ch->in_room->area->uid;
        return evtedit_save_after_change(ch);
    }

    if (!is_number(argument)) {
        send_to_char("scopeanchor expects an area uid, 'here', or 'clear'.\n\r", ch);
        return false;
    }

    area = get_area_from_uid(atol(argument));
    if (!area) {
        send_to_char("No area exists with that uid.\n\r", ch);
        return false;
    }

    evt->scope_area_uid = area->uid;
    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_scopefloating)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_bool(ch, argument, "scopefloating", "scopefloating [on|off]",
            &evt->scope_floating, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_schedule)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char arg1[MIL];
    time_t when_time;

    if (!evt)
        return false;

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("schedule <manual|recurring|calendar|worldcondition|triggered>\n\r", ch);
        send_to_char("schedule list\n\r", ch);
        send_to_char("schedule at <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
        send_to_char("schedule clear\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "list")) {
        printf_to_char(ch,
            "Schedule: mode=%s anchor=%s interval=%d variance=%d duration=%d cooldown=%d\n\r",
            event_enum_name(evt_sched_flags, evt->sched_type),
            event_format_time_short(evt->scheduled_time),
            evt->sched_interval,
            evt->sched_variance,
            evt->sched_duration,
            evt->sched_cooldown);

        if (evt->next_auto_time > current_time)
            printf_to_char(ch, "Next auto window: %s\n\r",
                event_format_time_short(evt->next_auto_time));

        if (evt->cooldown_until > current_time)
            printf_to_char(ch, "Cooldown until: %s\n\r",
                event_format_time_short(evt->cooldown_until));

        return false;
    }

    if (!str_prefix(arg1, "clear")) {
        evt->scheduled_time = 0;
        evt->next_auto_time = 0;
        send_to_char("Schedule anchor cleared.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(arg1, "at")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: schedule at <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
            return false;
        }

        if (!event_parse_when(argument, &when_time) || when_time <= current_time) {
            send_to_char("Invalid schedule time.\n\r", ch);
            return false;
        }

        evt->scheduled_time = when_time;
        evt->next_auto_time = 0;
        printf_to_char(ch, "Schedule anchor set to %s.\n\r",
            event_format_time_short(evt->scheduled_time));
        return evtedit_save_after_change(ch);
    }

    if (!olc_cmd_type_set_i16(ch, arg1, "schedule",
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

EVTEDIT(evtedit_goal)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_number_i16(ch, argument, "goal", "goal <0-32767>",
            &evt->completion_goal, 0, 32767, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_leaderrequired)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_bool(ch, argument, "leaderrequired", "leaderrequired [on|off]",
            &evt->leader_required, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_title)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "title", "title <text>",
            &evt->display_title, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_summary)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "summary", "summary <text>",
            &evt->short_summary, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_newsslug)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "newsslug", "newsslug <text>",
            &evt->news_slug, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_newsannounce)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "newsannounce", "newsannounce <text>",
            &evt->news_announcement, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_newsbody)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "newsbody", "newsbody <text>",
            &evt->news_body, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_themetags)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_cmd_string(ch, argument, "themetags", "themetags <text>",
            &evt->theme_tags, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_roster)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char cmd[MIL];

    if (!evt)
        return false;

    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd)) {
        send_to_char("Syntax: roster list\n\r", ch);
        send_to_char("        roster addnpc <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off] [phase=<name|any>]\n\r", ch);
        send_to_char("        roster addobj <vnum> [count] [chance] [minlevel] [maxlevel] [phase=<name|any>]\n\r", ch);
        send_to_char("        roster boss <index> <on|off>\n\r", ch);
        send_to_char("        roster phase <index> <name|any>\n\r", ch);
        send_to_char("        roster remove <index>\n\r", ch);
        send_to_char("        roster clear\n\r", ch);
        return false;
    }

    if (!str_prefix(cmd, "list")) {
        EVT_ROSTER_ENTRY *entry;
        int index = 1;

        if (!evt->roster) {
            send_to_char("Roster is empty.\n\r", ch);
            return false;
        }

        send_to_char("#   Type     Vnum      Count Chance Level    Boss Phase\n\r", ch);
        send_to_char("------------------------------------------------------\n\r", ch);

        for (entry = evt->roster; entry; entry = entry->next, index++) {
            printf_to_char(ch, "%-3d %-8s %-9ld %-5d %-6d %-8s %-4s %s\n\r",
                index,
                event_enum_name(evt_roster_kind_flags, entry->kind),
                entry->vnum,
                entry->count,
                entry->chance,
                evtedit_roster_level_window(entry),
                entry->boss ? "yes" : "no",
                evtedit_roster_phase_name(entry));
        }

        return false;
    }

    if (!str_prefix(cmd, "clear")) {
        evtedit_roster_free(&evt->roster);
        send_to_char("Roster cleared.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "remove")) {
        char index_arg[MIL];
        EVT_ROSTER_ENTRY *entry;
        EVT_ROSTER_ENTRY *prev = NULL;
        int target;

        one_argument(argument, index_arg);
        if (!is_number(index_arg)) {
            send_to_char("Syntax: roster remove <index>\n\r", ch);
            return false;
        }

        target = atoi(index_arg);
        if (target <= 0) {
            send_to_char("Index must be 1 or greater.\n\r", ch);
            return false;
        }

        for (entry = evt->roster; entry; entry = entry->next) {
            if (--target == 0)
                break;
            prev = entry;
        }

        if (!entry) {
            send_to_char("No roster entry at that index.\n\r", ch);
            return false;
        }

        if (prev)
            prev->next = entry->next;
        else
            evt->roster = entry->next;

        free_mem(entry, sizeof(*entry));
        send_to_char("Roster entry removed.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "boss")) {
        char index_arg[MIL];
        char value_arg[MIL];
        EVT_ROSTER_ENTRY *entry;
        bool boss_value = false;

        argument = one_argument(argument, index_arg);
        argument = one_argument(argument, value_arg);

        if (!is_number(index_arg) || IS_NULLSTR(value_arg)) {
            send_to_char("Syntax: roster boss <index> <on|off>\n\r", ch);
            return false;
        }

        entry = evtedit_roster_find(evt->roster, atoi(index_arg));
        if (!entry) {
            send_to_char("No roster entry at that index.\n\r", ch);
            return false;
        }

        if (!evtedit_parse_onoff_token(value_arg, &boss_value)) {
            send_to_char("Boss expects on/off (or yes/no).\n\r", ch);
            return false;
        }

        if (entry->kind != EVT_ROSTER_NPC && boss_value) {
            send_to_char("Only NPC roster entries can be marked as boss.\n\r", ch);
            return false;
        }

        entry->boss = (entry->kind == EVT_ROSTER_NPC) ? boss_value : false;
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "phase")) {
        char index_arg[MIL];
        char phase_arg[MIL];
        EVT_ROSTER_ENTRY *entry;
        char error[MSL];

        argument = one_argument(argument, index_arg);
        argument = one_argument(argument, phase_arg);

        if (!is_number(index_arg) || IS_NULLSTR(phase_arg)) {
            send_to_char("Syntax: roster phase <index> <name|any>\n\r", ch);
            return false;
        }

        entry = evtedit_roster_find(evt->roster, atoi(index_arg));
        if (!entry) {
            send_to_char("No roster entry at that index.\n\r", ch);
            return false;
        }

        if (str_cmp(phase_arg, "any")
            && !event_validate_phase_name(phase_arg, error, sizeof(error))) {
            printf_to_char(ch, "Invalid phase name: %s\n\r", error);
            return false;
        }

        if (!str_cmp(phase_arg, "any"))
            strncpy(entry->phase, "any", sizeof(entry->phase) - 1);
        else
            event_copy_trimmed(entry->phase, sizeof(entry->phase), phase_arg);

        entry->phase[sizeof(entry->phase) - 1] = '\0';

        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "addnpc") || !str_prefix(cmd, "addobj")) {
        bool adding_npc = !str_prefix(cmd, "addnpc");
        char vnum_arg[MIL];
        char token[MIL];
        int numeric_values[4];
        int numeric_count = 0;
        int count = 1;
        int chance = 100;
        int min_level = 0;
        int max_level = 0;
        bool boss = false;
        char phase[MIL] = "any";
        EVT_ROSTER_ENTRY *entry;

        argument = one_argument(argument, vnum_arg);
        if (!is_number(vnum_arg) || atol(vnum_arg) <= 0) {
            send_to_char("Syntax: roster addnpc <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off] [phase=<name|any>]\n\r", ch);
            send_to_char("        roster addobj <vnum> [count] [chance] [minlevel] [maxlevel] [phase=<name|any>]\n\r", ch);
            return false;
        }

        while (!IS_NULLSTR(argument)) {
            argument = one_argument(argument, token);
            if (IS_NULLSTR(token))
                break;

            if (is_number(token)) {
                if (numeric_count >= 4) {
                    send_to_char("Too many numeric arguments. Max is [count] [chance] [minlevel] [maxlevel].\n\r", ch);
                    return false;
                }

                numeric_values[numeric_count++] = atoi(token);
                continue;
            }

            if (adding_npc && !str_cmp(token, "boss")) {
                boss = true;
                continue;
            }

            if (!str_prefix(token, "phase=")) {
                char *phase_value = token + 6;
                char error[MSL];

                if (IS_NULLSTR(phase_value)) {
                    send_to_char("phase= expects a phase name or 'any'.\n\r", ch);
                    return false;
                }

                if (str_cmp(phase_value, "any")
                    && !event_validate_phase_name(phase_value, error, sizeof(error))) {
                    printf_to_char(ch, "Invalid phase name: %s\n\r", error);
                    return false;
                }

                event_copy_trimmed(phase, sizeof(phase), phase_value);
                continue;
            }

            if (adding_npc && evtedit_parse_onoff_token(token, &boss))
                continue;

            send_to_char("Invalid token in roster add.\n\r", ch);
            return false;
        }

        if (numeric_count >= 1)
            count = numeric_values[0];
        if (numeric_count >= 2)
            chance = numeric_values[1];
        if (numeric_count >= 3)
            min_level = numeric_values[2];
        if (numeric_count >= 4)
            max_level = numeric_values[3];

        if (count <= 0 || count > 10000) {
            send_to_char("Count must be between 1 and 10000.\n\r", ch);
            return false;
        }

        if (chance <= 0 || chance > 100) {
            send_to_char("Chance must be between 1 and 100.\n\r", ch);
            return false;
        }

        if (min_level < 0 || max_level < 0 || min_level > 32767 || max_level > 32767) {
            send_to_char("Level bounds must be in range 0-32767.\n\r", ch);
            return false;
        }

        if (max_level > 0 && min_level > max_level) {
            send_to_char("Min level cannot exceed max level.\n\r", ch);
            return false;
        }

        entry = alloc_mem(sizeof(*entry));
        memset(entry, 0, sizeof(*entry));
        entry->kind = adding_npc ? EVT_ROSTER_NPC : EVT_ROSTER_OBJECT;
        entry->vnum = atol(vnum_arg);
        entry->count = count;
        entry->chance = chance;
        entry->min_level = min_level;
        entry->max_level = max_level;
        entry->boss = adding_npc ? boss : false;
        event_copy_trimmed(entry->phase, sizeof(entry->phase), phase);
        if (IS_NULLSTR(entry->phase)) {
            strncpy(entry->phase, "any", sizeof(entry->phase) - 1);
            entry->phase[sizeof(entry->phase) - 1] = '\0';
        }
        evtedit_roster_append(evt, entry);

        printf_to_char(ch, "Added %s roster entry: vnum=%ld count=%d chance=%d level=%s boss=%s\n\r",
            adding_npc ? "npc" : "object",
            entry->vnum,
            entry->count,
            entry->chance,
            evtedit_roster_level_window(entry),
            entry->boss ? "yes" : "no");

        return evtedit_save_after_change(ch);
    }

    send_to_char("Unknown roster subcommand. Use: list, addnpc, addobj, boss, remove, clear.\n\r", ch);
    return false;
}

EVTEDIT(evtedit_spawnbrackets)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char error[MSL];
    char *trimmed;

    if (!evt)
        return false;

    trimmed = argument;
    while (*trimmed && isspace((unsigned char)*trimmed))
        trimmed++;

    if (!IS_NULLSTR(trimmed)
        && !event_validate_bracket_spec(trimmed, error, sizeof(error))) {
        printf_to_char(ch,
            "Invalid spawn bracket specification. %s\n\r"
            "Expected format examples: 1-50, 51-90, 91+\n\r",
            error);
        return false;
    }

    if (!olc_cmd_string(ch, argument, "spawnbrackets", "spawnbrackets <text>",
            &evt->spawn_brackets, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_collectionbrackets)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char error[MSL];
    char *trimmed;

    if (!evt)
        return false;

    trimmed = argument;
    while (*trimmed && isspace((unsigned char)*trimmed))
        trimmed++;

    if (!IS_NULLSTR(trimmed)
        && !event_validate_bracket_spec(trimmed, error, sizeof(error))) {
        printf_to_char(ch,
            "Invalid collection bracket specification. %s\n\r"
            "Expected format examples: 1-50, 51-90, 91+\n\r",
            error);
        return false;
    }

    if (!olc_cmd_string(ch, argument, "collectionbrackets", "collectionbrackets <text>",
            &evt->collection_brackets, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_bracketmode)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char mode[MIL];
    int i;
    bool ok = false;
    static const char *valid_modes[] = {
        "auto_by_level",
        "open",
        "manual",
    };

    if (!evt)
        return false;

    one_argument(argument, mode);
    if (!IS_NULLSTR(mode)) {
        for (i = 0; i < (int)elementsof(valid_modes); i++) {
            if (!str_cmp(mode, valid_modes[i])) {
                ok = true;
                break;
            }
        }

        if (!ok) {
            send_to_char("Invalid bracket mode. Use: auto_by_level, open, manual.\n\r", ch);
            return false;
        }
    }

    if (!olc_cmd_string(ch, argument, "bracketmode", "bracketmode <text>",
            &evt->bracket_mode, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_progressagg)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char mode[MIL];

    if (!evt)
        return false;

    one_argument(argument, mode);
    if (!IS_NULLSTR(mode)
        && str_cmp(mode, "shared")
        && str_cmp(mode, "total")
        && str_cmp(mode, "per_bracket_any")
        && str_cmp(mode, "per_bracket")
        && str_cmp(mode, "per_bracket_all_required")) {
        send_to_char("Invalid progress aggregation. Use: shared, total, per_bracket_any, per_bracket, per_bracket_all_required.\n\r", ch);
        return false;
    }

    if (!olc_cmd_string(ch, argument, "progressagg", "progressagg <text>",
            &evt->progress_aggregation, OLC_STR_DEFAULT, NULL, NULL))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_phaseplan)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    EVT_PHASE_STEP_DEF steps[EVT_PHASEPLAN_MAX_STEPS];
    char cmd[MIL];
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    char error[MSL];
    int count;
    int index;

    if (!evt)
        return false;

    count = event_phase_steps_load(evt->phase_plan,
        steps, EVT_PHASEPLAN_MAX_STEPS,
        error, sizeof(error));
    if (count < 0) {
        send_to_char(error, ch);
        send_to_char("\n\r", ch);
        return false;
    }

    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd)) {
        send_to_char("Syntax: phaseplan list\n\r", ch);
        send_to_char("        phaseplan clear\n\r", ch);
        send_to_char("        phaseplan add <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        phaseplan insert <index> <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        phaseplan set <index> <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        phaseplan name <index> <name>\n\r", ch);
        send_to_char("        phaseplan minutes <index> <minutes>\n\r", ch);
        send_to_char("        phaseplan script <index> <scriptvnum|0>\n\r", ch);
        send_to_char("        phaseplan remove <index>\n\r", ch);
        return false;
    }

    if (!str_prefix(cmd, "list")) {
        int i;

        if (count <= 0) {
            send_to_char("Phase plan is empty.\n\r", ch);
            return false;
        }

        send_to_char("{WPhase Plan Steps:{x\n\r", ch);
        for (i = 0; i < count; i++) {
            printf_to_char(ch, "  {W%2d){x name={Y%s{x minutes={C%d{x script={M%ld{x\n\r",
                i + 1,
                steps[i].name,
                steps[i].minutes,
                steps[i].script_vnum);
        }

        return false;
    }

    if (!str_prefix(cmd, "clear")) {
        event_phase_steps_store(evt, steps, 0);
        send_to_char("Phase plan cleared.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "add")) {
        EVT_PHASE_STEP_DEF step;

        memset(&step, 0, sizeof(step));
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (IS_NULLSTR(arg1)) {
            send_to_char("Syntax: phaseplan add <name> [minutes] [scriptvnum]\n\r", ch);
            return false;
        }

        if (count >= EVT_PHASEPLAN_MAX_STEPS) {
            printf_to_char(ch, "Phase plan is at max capacity (%d).\n\r", EVT_PHASEPLAN_MAX_STEPS);
            return false;
        }

        if (!event_validate_phase_name(arg1, error, sizeof(error))) {
            send_to_char(error, ch);
            send_to_char("\n\r", ch);
            return false;
        }

        strncpy(step.name, arg1, sizeof(step.name) - 1);
        step.name[sizeof(step.name) - 1] = '\0';

        if (!IS_NULLSTR(arg2)) {
            if (!is_number(arg2) || atoi(arg2) < 0) {
                send_to_char("Minutes must be a number >= 0.\n\r", ch);
                return false;
            }
            step.minutes = atoi(arg2);
        }

        if (!IS_NULLSTR(arg3)) {
            if (!is_number(arg3) || atol(arg3) < 0) {
                send_to_char("Script vnum must be a number >= 0.\n\r", ch);
                return false;
            }
            step.script_vnum = atol(arg3);
        }

        steps[count++] = step;
        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase step added.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "insert")) {
        EVT_PHASE_STEP_DEF step;

        memset(&step, 0, sizeof(step));
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: phaseplan insert <index> <name> [minutes] [scriptvnum]\n\r", ch);
            return false;
        }

        if (count >= EVT_PHASEPLAN_MAX_STEPS) {
            printf_to_char(ch, "Phase plan is at max capacity (%d).\n\r", EVT_PHASEPLAN_MAX_STEPS);
            return false;
        }

        index = atoi(arg1);
        if (index < 1 || index > count + 1) {
            printf_to_char(ch, "Index must be between 1 and %d.\n\r", count + 1);
            return false;
        }

        if (!event_validate_phase_name(arg2, error, sizeof(error))) {
            send_to_char(error, ch);
            send_to_char("\n\r", ch);
            return false;
        }

        strncpy(step.name, arg2, sizeof(step.name) - 1);
        step.name[sizeof(step.name) - 1] = '\0';

        if (!IS_NULLSTR(arg3)) {
            if (!is_number(arg3) || atoi(arg3) < 0) {
                send_to_char("Minutes must be a number >= 0.\n\r", ch);
                return false;
            }
            step.minutes = atoi(arg3);
        }

        argument = one_argument(argument, arg1);
        if (!IS_NULLSTR(arg1)) {
            if (!is_number(arg1) || atol(arg1) < 0) {
                send_to_char("Script vnum must be a number >= 0.\n\r", ch);
                return false;
            }
            step.script_vnum = atol(arg1);
        }

        memmove(&steps[index], &steps[index - 1],
            (size_t)(count - (index - 1)) * sizeof(steps[0]));
        steps[index - 1] = step;
        count++;

        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase step inserted.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "set")) {
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: phaseplan set <index> <name> [minutes] [scriptvnum]\n\r", ch);
            return false;
        }

        index = atoi(arg1);
        if (index < 1 || index > count) {
            printf_to_char(ch, "Index must be between 1 and %d.\n\r", count);
            return false;
        }

        if (!event_validate_phase_name(arg2, error, sizeof(error))) {
            send_to_char(error, ch);
            send_to_char("\n\r", ch);
            return false;
        }

        strncpy(steps[index - 1].name, arg2, sizeof(steps[index - 1].name) - 1);
        steps[index - 1].name[sizeof(steps[index - 1].name) - 1] = '\0';

        if (!IS_NULLSTR(arg3)) {
            if (!is_number(arg3) || atoi(arg3) < 0) {
                send_to_char("Minutes must be a number >= 0.\n\r", ch);
                return false;
            }
            steps[index - 1].minutes = atoi(arg3);
        } else {
            steps[index - 1].minutes = 0;
        }

        argument = one_argument(argument, arg1);
        if (!IS_NULLSTR(arg1)) {
            if (!is_number(arg1) || atol(arg1) < 0) {
                send_to_char("Script vnum must be a number >= 0.\n\r", ch);
                return false;
            }
            steps[index - 1].script_vnum = atol(arg1);
        } else {
            steps[index - 1].script_vnum = 0;
        }

        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase step updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (!is_number(arg1)) {
        send_to_char("Phaseplan command requires an index for this operation.\n\r", ch);
        return false;
    }

    index = atoi(arg1);
    if (index < 1 || index > count) {
        printf_to_char(ch, "Index must be between 1 and %d.\n\r", count);
        return false;
    }

    if (!str_prefix(cmd, "remove")) {
        memmove(&steps[index - 1], &steps[index],
            (size_t)(count - index) * sizeof(steps[0]));
        count--;
        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase step removed.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "name")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: phaseplan name <index> <name>\n\r", ch);
            return false;
        }

        if (!event_validate_phase_name(arg2, error, sizeof(error))) {
            send_to_char(error, ch);
            send_to_char("\n\r", ch);
            return false;
        }

        strncpy(steps[index - 1].name, arg2, sizeof(steps[index - 1].name) - 1);
        steps[index - 1].name[sizeof(steps[index - 1].name) - 1] = '\0';
        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase name updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "minutes")) {
        if (IS_NULLSTR(arg2) || !is_number(arg2) || atoi(arg2) < 0) {
            send_to_char("Syntax: phaseplan minutes <index> <minutes>=0\n\r", ch);
            return false;
        }

        steps[index - 1].minutes = atoi(arg2);
        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase minutes updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "script")) {
        if (IS_NULLSTR(arg2) || !is_number(arg2) || atol(arg2) < 0) {
            send_to_char("Syntax: phaseplan script <index> <scriptvnum|0>\n\r", ch);
            return false;
        }

        steps[index - 1].script_vnum = atol(arg2);
        event_phase_steps_store(evt, steps, count);
        send_to_char("Phase script updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    send_to_char("Unknown phaseplan subcommand. Use: list, clear, add, insert, set, name, minutes, script, remove.\n\r", ch);
    return false;
}

EVTEDIT(evtedit_rewardsuccess)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char arg[MIL];

    if (!evt)
        return false;

    one_argument(argument, arg);
    if (IS_NULLSTR(arg) || !is_number(arg)) {
        send_to_char("Syntax: rewardsuccess <scriptvnum|0>\n\r", ch);
        return false;
    }

    evt->reward_success_script = UMAX(0, atol(arg));
    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_rewardfail)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char arg[MIL];

    if (!evt)
        return false;

    one_argument(argument, arg);
    if (IS_NULLSTR(arg) || !is_number(arg)) {
        send_to_char("Syntax: rewardfail <scriptvnum|0>\n\r", ch);
        return false;
    }

    evt->reward_failure_script = UMAX(0, atol(arg));
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