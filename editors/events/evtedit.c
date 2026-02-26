#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../event_types.h"
#include "../../scripts.h"
#include "../../requirements.h"
#include "../../recycle.h"
#include "../../account/preferences.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef EVENT_INDEX_DATA EVTEDIT_DATA;

#define EVT_PHASEPLAN_MAX_STEPS 64

typedef struct evt_phase_step_def EVT_PHASE_STEP_DEF;
struct evt_phase_step_def {
    char name[MIL];
    int minutes;
    long script_vnum;
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
EVTEDIT(evtedit_stages);
EVTEDIT(evtedit_phaseplan);
EVTEDIT(evtedit_phases);
EVTEDIT(evtedit_rewardphase);
EVTEDIT(evtedit_rewardsuccess);
EVTEDIT(evtedit_rewardfail);
EVTEDIT(evtedit_flags);
EVTEDIT(evtedit_comments);
EVTEDIT(evtedit_addeprog);
EVTEDIT(evtedit_deleprog);
EVTEDIT(evtedit_varset);
EVTEDIT(evtedit_varclear);
EVTEDIT(evtedit_save);
EVTEDIT(evtedit_reload);

static bool evtedit_booted = false;
static EVTEDIT_DATA *evtedit_global_head = NULL;
static EVTEDIT_DATA *evtedit_global_tail = NULL;
static EVENT_INSTANCE *event_active_head = NULL;
static uint32_t event_next_instance_id = 1;
static bool event_system_enabled = true;

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
    { "autoadd", EVT_FLAG_AUTO_ADD, true, NULL },
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

const char *event_index_get_name(const EVENT_INDEX_DATA *event_index)
{
    if (!event_index || IS_NULLSTR(event_index->name))
        return "(unnamed)";

    return event_index->name;
}

long event_index_get_uid(const EVENT_INDEX_DATA *event_index)
{
    if (!event_index)
        return 0;

    return event_index->uid;
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
        free_string(entry->requirements);
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

static const char *evtedit_roster_stage_name(const EVT_ROSTER_ENTRY *entry)
{
    static char stage_buf[32];

    if (!entry || entry->stage <= 0)
        return "any";

    snprintf(stage_buf, sizeof(stage_buf), "%d", entry->stage);
    return stage_buf;
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
static void evtedit_show_stages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_messages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_meta_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static void evtedit_show_scripting_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
static bool evtedit_save_after_change(CHAR_DATA *ch);
static AREA_DATA *evtedit_get_area(void *pEdit);
static EVENT_INSTANCE *event_find_active_def(const EVTEDIT_DATA *evt);
static EVT_STAGE_DEF *event_stage_get_by_index(const EVTEDIT_DATA *evt, int index);
static void evtedit_ensure_loaded(void);
static bool evtedit_save_area(AREA_DATA *area);
static void evtedit_global_register(EVTEDIT_DATA *evt);
static void evtedit_global_unregister(EVTEDIT_DATA *evt);
static void evtedit_rebuild_global_index(void);
static void event_broadcast(const char *message);
static bool event_stop_definition(EVTEDIT_DATA *evt);
static void event_phase_defs_free(EVT_PHASE_DEF **head, int16_t *count);
static void event_stage_defs_free(EVT_STAGE_DEF **head, int16_t *count);
static void event_phase_steps_store(EVTEDIT_DATA *evt,
    const EVT_PHASE_STEP_DEF *steps, int count);
static const char *event_stage_transition_name(int mode);
static const char *event_stage_objective_mode_name(int mode);
static const char *event_stage_objective_type_name(int type);

static void event_phase_defs_free(EVT_PHASE_DEF **head, int16_t *count)
{
    EVT_PHASE_DEF *phase;

    if (!head)
        return;

    while (*head) {
        phase = *head;
        *head = phase->next;
        free_string(phase->name);
        free_mem(phase, sizeof(*phase));
    }

    if (count)
        *count = 0;
}

static void event_stage_objectives_free(EVT_STAGE_OBJECTIVE_DEF **head, int16_t *count)
{
    EVT_STAGE_OBJECTIVE_DEF *objective;

    if (!head)
        return;

    while (*head) {
        objective = *head;
        *head = objective->next;
        free_string(objective->name);
        free_string(objective->data);
        free_mem(objective, sizeof(*objective));
    }

    if (count)
        *count = 0;
}

static void event_stage_defs_free(EVT_STAGE_DEF **head, int16_t *count)
{
    EVT_STAGE_DEF *stage;

    if (!head)
        return;

    while (*head) {
        stage = *head;
        *head = stage->next;
        free_string(stage->name);
        event_stage_objectives_free(&stage->objectives, &stage->objective_count);
        free_mem(stage, sizeof(*stage));
    }

    if (count)
        *count = 0;
}

static void evtedit_free_item(EVTEDIT_DATA *evt)
{
    int hash;
    EVENT_INDEX_DATA *iter;
    EVENT_INDEX_DATA *prev;

    if (!evt)
        return;

    if (evt->area && evt->vnum > 0) {
        hash = (int)(evt->vnum % MAX_KEY_HASH);
        if (hash < 0)
            hash += MAX_KEY_HASH;
        prev = NULL;
        for (iter = evt->area->event_index_hash[hash]; iter; iter = iter->next_hash) {
            if (iter != evt) {
                prev = iter;
                continue;
            }

            if (prev)
                prev->next_hash = iter->next_hash;
            else
                evt->area->event_index_hash[hash] = iter->next_hash;
            break;
        }
    }

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
    event_phase_defs_free(&evt->phases, &evt->phase_count);
    event_stage_defs_free(&evt->stages, &evt->stage_count);
    free_string(evt->phase_plan);
    free_string(evt->comments);
    free_prog_list(evt->progs);
    variable_freelist(&evt->index_vars);
    evtedit_roster_free(&evt->roster);
    free_mem(evt, sizeof(*evt));
}

static bool evtedit_parse_index_ref(CHAR_DATA *ch, const char *input, WNUM *wnum)
{
    char ref[MIL];
    char *sep;
    AREA_DATA *context_area = NULL;
    EVTEDIT_DATA *editing_evt = NULL;

    if (!ch || !wnum || IS_NULLSTR(input))
        return false;

    strncpy(ref, input, sizeof(ref) - 1);
    ref[sizeof(ref) - 1] = '\0';

    if (ch->desc && ch->desc->editor == ED_EVENT && ch->desc->pEdit) {
        editing_evt = (EVTEDIT_DATA *)ch->desc->pEdit;
        if (editing_evt->area)
            context_area = editing_evt->area;
    }

    if (!context_area && ch->in_room && ch->in_room->area)
        context_area = ch->in_room->area;

    sep = strchr(ref, '#');
    if (!sep)
        sep = strchr(ref, ':');

    if (sep && strchr(ref, ':')) {
        *sep++ = '\0';
        if (!is_number(ref) || !is_number(sep))
            return false;
        wnum->pArea = get_area_index(atol(ref));
        wnum->vnum = atol(sep);
        return (wnum->pArea != NULL && wnum->vnum > 0);
    }

    if (strchr(ref, '#')) {
        AREA_DATA *parse_context = olc_relative_widevnum_context(context_area, ref);
        return parse_widevnum(ref, parse_context, wnum);
    }

    if (!is_number(ref) || !context_area)
        return false;

    wnum->pArea = context_area;
    wnum->vnum = atol(ref);
    return (wnum->vnum > 0);
}

static long evtedit_next_vnum_in_area(AREA_DATA *area)
{
    EVTEDIT_DATA *iter;
    long next_vnum = 1;
    int hash;

    if (!area)
        return 0;

    for (hash = 0; hash < MAX_KEY_HASH; hash++) {
        for (iter = area->event_index_hash[hash]; iter != NULL; iter = iter->next_hash) {
            if (iter->vnum >= next_vnum)
                next_vnum = iter->vnum + 1;
        }
    }

    return next_vnum;
}

static EVTEDIT_DATA *evtedit_new(AREA_DATA *area, long vnum, const char *name)
{
    EVTEDIT_DATA *evt = alloc_mem(sizeof(*evt));
    memset(evt, 0, sizeof(*evt));

    evt->area = area;
    evt->vnum = vnum;
    evt->uid = vnum > 0 ? vnum : 0;
    evt->name = str_dup(name ? name : "event");
    evt->description = str_dup("");
    evt->announce_msg = str_dup("");
    evt->end_msg = str_dup("");
    evt->join_msg = str_dup("");
    evt->event_type = EVT_TYPE_COLLECTION;
    evt->scope_type = EVT_SCOPE_AREA;
    evt->scope_area_uid = area ? area->uid : 0;
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
    evt->phases = NULL;
    evt->phase_count = 0;
    evt->stages = NULL;
    evt->stage_count = 0;
    evt->phase_plan = str_dup("");
    evt->reward_phase_script = 0;
    evt->reward_success_script = 0;
    evt->reward_failure_script = 0;
    evt->enabled = true;
    evt->flags = 0;
    evt->comments = str_dup("");

    if (evt->area && evt->vnum > 0)
        event_index_register(evt);

    evtedit_global_register(evt);

    return evt;
}

static EVTEDIT_DATA *evtedit_find_uid(long uid)
{
    return get_event_index(uid);
}

static EVTEDIT_DATA *evtedit_find_name(const char *name)
{
    AREA_DATA *area;
    EVTEDIT_DATA *evt;
    int hash;

    if (IS_NULLSTR(name))
        return NULL;

    for (area = area_first; area; area = area->next) {
        for (hash = 0; hash < MAX_KEY_HASH; hash++) {
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                if (!str_cmp(evt->name, name))
                    return evt;
            }
        }
    }

    return NULL;
}

static bool evtedit_save_area(AREA_DATA *area)
{
    if (!area)
        return false;

    save_area_new(area);
    return true;
}

static void evtedit_global_register(EVTEDIT_DATA *evt)
{
    EVTEDIT_DATA *it;

    if (!evt)
        return;

    evt->next = NULL;

    for (it = evtedit_global_head; it; it = it->next)
        if (it == evt)
            return;

    if (!evtedit_global_head)
        evtedit_global_head = evt;
    else
        evtedit_global_tail->next = evt;

    evtedit_global_tail = evt;
}

static void evtedit_global_unregister(EVTEDIT_DATA *evt)
{
    EVTEDIT_DATA *it;
    EVTEDIT_DATA *prev = NULL;

    if (!evt)
        return;

    for (it = evtedit_global_head; it; prev = it, it = it->next) {
        if (it != evt)
            continue;

        if (prev)
            prev->next = it->next;
        else
            evtedit_global_head = it->next;

        if (evtedit_global_tail == it)
            evtedit_global_tail = prev;

        evt->next = NULL;
        return;
    }
}

static void evtedit_rebuild_global_index(void)
{
    AREA_DATA *area;
    EVTEDIT_DATA *evt;
    int hash;

    evtedit_global_head = NULL;
    evtedit_global_tail = NULL;

    for (area = area_first; area; area = area->next) {
        for (hash = 0; hash < MAX_KEY_HASH; hash++) {
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                evt->next = NULL;
                evtedit_global_register(evt);
            }
        }
    }
}

static int event_list_scope_band_for_player(const EVTEDIT_DATA *evt, CHAR_DATA *ch)
{
    AREA_DATA *current_area;
    AREA_DATA *anchor_area = NULL;
    AREA_REGION *current_region = NULL;

    if (!evt)
        return -1;

    if (evt->scope_area_uid > 0)
        anchor_area = get_area_from_uid(evt->scope_area_uid);

    if (!ch || !ch->in_room || !ch->in_room->area)
        return evt->scope_type == EVT_SCOPE_GLOBAL ? 3 : -1;

    current_area = ch->in_room->area;
    current_region = get_room_region(ch->in_room);

    switch (evt->scope_type) {
    case EVT_SCOPE_ZONES:
    case EVT_SCOPE_BATTLEFIELD:
        if ((!anchor_area || anchor_area == current_area) && current_region != NULL)
            return 0;
        return -1;

    case EVT_SCOPE_REGION:
        if ((!anchor_area || anchor_area == current_area) && current_region != NULL)
            return 1;
        return -1;

    case EVT_SCOPE_AREA:
        if (!anchor_area || anchor_area == current_area)
            return 2;
        return -1;

    case EVT_SCOPE_GLOBAL:
        return 3;

    default:
        return -1;
    }
}

static const char *event_list_scope_title(int band)
{
    switch (band) {
    case 0: return "Local";
    case 1: return "Regional";
    case 2: return "Area";
    case 3: return "Global";
    default: return "Other";
    }
}

#define EVENT_PLAYER_SOON_WINDOW_SECONDS (30 * 60)

static bool event_player_should_show(const EVTEDIT_DATA *evt, CHAR_DATA *ch,
    EVENT_INSTANCE **inst_out, bool *soon_out, long *starts_in_out, int *band_out)
{
    EVENT_INSTANCE *inst;
    long starts_in = 0;
    int band;

    if (inst_out)
        *inst_out = NULL;
    if (soon_out)
        *soon_out = false;
    if (starts_in_out)
        *starts_in_out = 0;
    if (band_out)
        *band_out = -1;

    if (!evt)
        return false;

    band = event_list_scope_band_for_player(evt, ch);
    if (band < 0)
        return false;

    inst = event_find_active_def(evt);
    if (inst && inst->state == EVTS_ACTIVE) {
        if (inst_out)
            *inst_out = inst;
        if (band_out)
            *band_out = band;
        return true;
    }

    if (!evt->enabled)
        return false;

    if (evt->scheduled_time > current_time)
        starts_in = (long)(evt->scheduled_time - current_time);
    else if (evt->next_auto_time > current_time)
        starts_in = (long)(evt->next_auto_time - current_time);

    if (starts_in <= 0 || starts_in > EVENT_PLAYER_SOON_WINDOW_SECONDS)
        return false;

    if (soon_out)
        *soon_out = true;
    if (starts_in_out)
        *starts_in_out = starts_in;
    if (band_out)
        *band_out = band;
    return true;
}

static EVTEDIT_DATA *event_player_lookup_idx(CHAR_DATA *ch, int wanted_idx,
    EVENT_INSTANCE **inst_out, bool *soon_out, long *starts_in_out, int *band_out)
{
    AREA_DATA *area;
    EVTEDIT_DATA *evt;
    int hash;
    int band;
    int idx = 0;

    if (inst_out)
        *inst_out = NULL;
    if (soon_out)
        *soon_out = false;
    if (starts_in_out)
        *starts_in_out = 0;
    if (band_out)
        *band_out = -1;

    if (wanted_idx <= 0)
        return NULL;

    for (band = 0; band <= 3; band++) {
        for (area = area_first; area; area = area->next) {
            for (hash = 0; hash < MAX_KEY_HASH; hash++) {
                for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                    EVENT_INSTANCE *inst = NULL;
                    bool soon = false;
                    long starts_in = 0;
                    int evt_band = -1;

                    if (!event_player_should_show(evt, ch, &inst, &soon, &starts_in, &evt_band))
                        continue;
                    if (evt_band != band)
                        continue;

                    idx++;
                    if (idx != wanted_idx)
                        continue;

                    if (inst_out)
                        *inst_out = inst;
                    if (soon_out)
                        *soon_out = soon;
                    if (starts_in_out)
                        *starts_in_out = starts_in;
                    if (band_out)
                        *band_out = evt_band;
                    return evt;
                }
            }
        }
    }

    return NULL;
}

static AREA_DATA *evtedit_get_area(void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    return evt ? evt->area : NULL;
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

static EVENT_INSTANCE *event_find_runtime_by_ref(long event_uid, uint32_t instance_id)
{
    EVENT_INSTANCE *inst;

    if (event_uid <= 0 || instance_id <= 0)
        return NULL;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (inst->instance_id != instance_id)
            continue;
        return inst;
    }

    return NULL;
}

static bool epstat_parse_runtime_ref(const char *argument, long *event_uid, uint32_t *instance_id)
{
    char arg1[MIL];
    char arg2[MIL];
    char token[MIL];
    char *separator;

    if (event_uid)
        *event_uid = 0;
    if (instance_id)
        *instance_id = 0;

    if (IS_NULLSTR(argument))
        return false;

    argument = one_argument((char *)argument, arg1);
    one_argument((char *)argument, arg2);

    if (IS_NULLSTR(arg1))
        return false;

    strncpy(token, arg1, sizeof(token) - 1);
    token[sizeof(token) - 1] = '\0';

    separator = strchr(token, '#');
    if (!separator)
        separator = strchr(token, ':');
    if (!separator)
        separator = strchr(token, '.');

    if (separator) {
        *separator = '\0';
        separator++;

        if (IS_NULLSTR(token) || IS_NULLSTR(separator))
            return false;
        if (!is_number(token) || !is_number(separator))
            return false;

        if (event_uid)
            *event_uid = atol(token);
        if (instance_id)
            *instance_id = (uint32_t)atol(separator);
        return true;
    }

    if (!is_number(arg1) || !is_number(arg2))
        return false;

    if (event_uid)
        *event_uid = atol(arg1);
    if (instance_id)
        *instance_id = (uint32_t)atol(arg2);

    return true;
}

static const char *epstat_event_state_name(int state)
{
    switch (state) {
    case EVTS_PENDING: return "pending";
    case EVTS_ACTIVE: return "active";
    case EVTS_COMPLETE: return "complete";
    case EVTS_CANCELLED: return "cancelled";
    default: return "unknown";
    }
}

static const char *epstat_quest_status_name(int status)
{
    switch (status) {
    case QUEST_RUN_STATUS_ACTIVE: return "active";
    case QUEST_RUN_STATUS_COMPLETED: return "completed";
    case QUEST_RUN_STATUS_FAILED: return "failed";
    case QUEST_RUN_STATUS_ABANDONED: return "abandoned";
    default: return "unknown";
    }
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
                "Stored phases are invalid. Use phases clear, then rebuild with subcommands.");
        return -1;
    }

    return index;
}

static int event_phase_steps_load_event(const EVTEDIT_DATA *evt,
    EVT_PHASE_STEP_DEF *steps, int max_steps,
    char *error, size_t error_size)
{
    const EVT_PHASE_DEF *phase;
    int index = 0;

    if (error && error_size > 0)
        error[0] = '\0';

    if (!steps || max_steps <= 0) {
        if (error && error_size > 0)
            snprintf(error, error_size, "Internal phase buffer unavailable.");
        return -1;
    }

    if (!evt)
        return 0;

    if (evt->phases && evt->phase_count > 0) {
        for (phase = evt->phases; phase; phase = phase->next) {
            if (index >= max_steps) {
                if (error && error_size > 0)
                    snprintf(error, error_size,
                        "Too many phase steps (maximum %d).", max_steps);
                return -1;
            }

            memset(&steps[index], 0, sizeof(steps[index]));
            strlcpy(steps[index].name,
                IS_NULLSTR(phase->name) ? "phase" : phase->name,
                sizeof(steps[index].name));
            steps[index].minutes = UMAX(0, phase->minutes);
            steps[index].script_vnum = UMAX(0, phase->script_vnum);
            index++;
        }

        return index;
    }

    return event_phase_steps_load(evt->phase_plan, steps, max_steps, error, error_size);
}

static bool event_phase_step_get(const EVTEDIT_DATA *evt, int phase_index,
    char *phase_name, size_t phase_name_size,
    int *phase_minutes, long *phase_script_vnum)
{
    const EVT_STAGE_DEF *stage;
    int index = 0;

    if (!evt || phase_index < 0
        || !phase_name || phase_name_size == 0
        || !phase_minutes || !phase_script_vnum)
        return false;

    phase_name[0] = '\0';
    *phase_minutes = 0;
    *phase_script_vnum = 0;

    for (stage = evt->stages; stage; stage = stage->next, index++) {
        if (index != phase_index)
            continue;

        strlcpy(phase_name,
            IS_NULLSTR(stage->name) ? formatf("stage_%d", phase_index + 1) : stage->name,
            phase_name_size);
        *phase_minutes = UMAX(0, stage->duration_minutes);
        *phase_script_vnum = UMAX(0, stage->on_enter_script);
        return true;
    }

    return false;
}

static EVT_STAGE_OBJECTIVE_DEF *event_stage_add_objective(EVT_STAGE_DEF *stage,
    const char *name, int objective_type, int target_count)
{
    EVT_STAGE_OBJECTIVE_DEF *objective;
    EVT_STAGE_OBJECTIVE_DEF *tail;

    if (!stage)
        return NULL;

    objective = alloc_mem(sizeof(*objective));
    memset(objective, 0, sizeof(*objective));
    objective->name = str_dup(IS_NULLSTR(name) ? "objective" : name);
    objective->objective_type = objective_type;
    objective->target_count = UMAX(0, target_count);
    objective->script_vnum = 0;
    objective->data = str_dup("");

    if (!stage->objectives)
        stage->objectives = objective;
    else {
        tail = stage->objectives;
        while (tail->next)
            tail = tail->next;
        tail->next = objective;
    }

    stage->objective_count++;
    return objective;
}

static EVT_STAGE_DEF *event_add_stage(EVTEDIT_DATA *evt, const char *name,
    int transition_mode, int duration_minutes, long on_enter_script)
{
    EVT_STAGE_DEF *stage;
    EVT_STAGE_DEF *tail;

    if (!evt)
        return NULL;

    stage = alloc_mem(sizeof(*stage));
    memset(stage, 0, sizeof(*stage));
    stage->name = str_dup(IS_NULLSTR(name) ? "stage" : name);
    stage->transition_mode = transition_mode;
    stage->objective_mode = EVT_STAGE_OBJECTIVE_ALL;
    stage->duration_minutes = UMAX(0, duration_minutes);
    stage->on_enter_script = UMAX(0, on_enter_script);
    stage->on_tick_script = 0;
    stage->on_complete_script = 0;

    if (!evt->stages)
        evt->stages = stage;
    else {
        tail = evt->stages;
        while (tail->next)
            tail = tail->next;
        tail->next = stage;
    }

    evt->stage_count++;
    return stage;
}

static void event_stage_append_default_objective(EVTEDIT_DATA *evt, EVT_STAGE_DEF *stage)
{
    int target;

    if (!evt || !stage || stage->objective_count > 0)
        return;

    switch (evt->event_type) {
    case EVT_TYPE_COLLECTION:
        target = UMAX(1, evt->completion_goal);
        event_stage_add_objective(stage, "collect", EVT_STAGE_OBJECTIVE_COLLECT, target);
        break;

    case EVT_TYPE_BOSS:
    case EVT_TYPE_INVASION:
    case EVT_TYPE_WAR_FFA:
    case EVT_TYPE_WAR_GENOCIDE:
    case EVT_TYPE_WAR_JIHAD:
        target = UMAX(1, evt->completion_goal);
        event_stage_add_objective(stage, "kill", EVT_STAGE_OBJECTIVE_KILL, target);
        break;

    default:
        break;
    }
}

static void event_stage_sync_from_phases(EVTEDIT_DATA *evt)
{
    EVT_PHASE_DEF *phase;
    EVT_STAGE_DEF *last_stage = NULL;

    if (!evt || evt->stage_count > 0)
        return;

    if (!evt->phases || evt->phase_count <= 0) {
        last_stage = event_add_stage(evt, "active",
            EVT_STAGE_TRANSITION_ON_COMPLETE, 0, 0);
        event_stage_append_default_objective(evt, last_stage);
        return;
    }

    for (phase = evt->phases; phase; phase = phase->next) {
        int transition_mode = phase->minutes > 0
            ? EVT_STAGE_TRANSITION_ON_TIMER
            : EVT_STAGE_TRANSITION_ON_COMPLETE;

        last_stage = event_add_stage(evt,
            IS_NULLSTR(phase->name) ? "stage" : phase->name,
            transition_mode,
            UMAX(0, phase->minutes),
            UMAX(0, phase->script_vnum));
    }

    event_stage_append_default_objective(evt, last_stage);
}

static void event_phase_sync_from_legacy(EVTEDIT_DATA *evt)
{
    EVT_PHASE_STEP_DEF steps[EVT_PHASEPLAN_MAX_STEPS];
    char error[MSL];
    int count;

    if (!evt || evt->phases || evt->phase_count > 0 || IS_NULLSTR(evt->phase_plan))
        return;

    count = event_phase_steps_load_event(evt,
        steps, EVT_PHASEPLAN_MAX_STEPS,
        error, sizeof(error));
    if (count <= 0)
        return;

    event_phase_steps_store(evt, steps, count);
}

static void event_phase_steps_store(EVTEDIT_DATA *evt,
    const EVT_PHASE_STEP_DEF *steps, int count)
{
    BUFFER *buffer;
    EVT_PHASE_DEF *tail = NULL;
    int i;

    if (!evt || count < 0)
        return;

    event_phase_defs_free(&evt->phases, &evt->phase_count);

    for (i = 0; i < count; i++) {
        EVT_PHASE_DEF *phase;

        phase = alloc_mem(sizeof(*phase));
        memset(phase, 0, sizeof(*phase));
        phase->name = str_dup(IS_NULLSTR(steps[i].name) ? "phase" : steps[i].name);
        phase->minutes = UMAX(0, steps[i].minutes);
        phase->script_vnum = UMAX(0, steps[i].script_vnum);

        if (!evt->phases)
            evt->phases = phase;
        else
            tail->next = phase;

        tail = phase;
        evt->phase_count++;
    }

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

    event_stage_defs_free(&evt->stages, &evt->stage_count);
    event_stage_sync_from_phases(evt);
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

static bool event_roster_matches(const EVT_ROSTER_ENTRY *entry, int phase_index)
{
    if (!entry)
        return false;

    if (entry->stage > 0)
        return (phase_index >= 0) && (entry->stage == (phase_index + 1));

    return true;
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

static bool event_roster_requirements_met(const EVT_ROSTER_ENTRY *entry,
    ROOM_INDEX_DATA *room)
{
    REQUIREMENT_CONTEXT req_context;

    if (!entry || IS_NULLSTR(entry->requirements))
        return true;

    if (!room)
        return false;

    memset(&req_context, 0, sizeof(req_context));
    req_context.self_room = room;

    return requirements_evaluate_text(entry->requirements, &req_context, true);
}

static int event_runtime_spawn_roster_for_phase(EVENT_INSTANCE *inst, int phase_index)
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

        if (!event_roster_matches(entry, phase_index))
            continue;

        bracket = event_roster_entry_bracket(inst->def, entry);

        for (i = 0; i < UMAX(1, entry->count); i++) {
            ROOM_INDEX_DATA *room;
            int attempt;

            if (entry->chance < 100 && number_percent() > entry->chance)
                continue;

            room = NULL;
            for (attempt = 0; attempt < 12; attempt++) {
                room = get_random_room_area(NULL, area);
                if (!room)
                    break;

                if (event_roster_requirements_met(entry, room))
                    break;

                room = NULL;
            }

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

static bool event_roster_has_boss_for_phase(const EVENT_INSTANCE *inst)
{
    EVT_ROSTER_ENTRY *entry;

    if (!inst || !inst->def)
        return false;

    for (entry = inst->def->roster; entry; entry = entry->next)
        if (entry->kind == EVT_ROSTER_NPC
            && entry->boss
            && event_roster_matches(entry, inst->phase_index))
            return true;

    return false;
}

static bool event_mobile_matches_roster_boss(const EVENT_INSTANCE *inst,
    const CHAR_DATA *mob)
{
    EVT_ROSTER_ENTRY *entry;

    if (!inst || !inst->def || !mob || !mob->pIndexData)
        return false;

    for (entry = inst->def->roster; entry; entry = entry->next)
        if (entry->kind == EVT_ROSTER_NPC
            && entry->boss
            && entry->vnum == mob->pIndexData->vnum
            && event_roster_matches(entry, inst->phase_index))
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

    if (mob->event_source.vnum <= 0 && mob->event_source_instance_id == 0)
        return true;

    if (mob->event_source.vnum != inst->def->uid)
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

static bool event_try_auto_add_participant(EVENT_INSTANCE *inst, CHAR_DATA *ch,
    const char *action_name, bool notify)
{
    if (!inst || !inst->def || !ch || IS_NPC(ch))
        return false;

    if (event_find_participant(inst, ch))
        return true;

    if (!IS_SET(inst->def->flags, EVT_FLAG_AUTO_ADD))
        return false;

    if (!event_add_participant(inst, ch))
        return false;

    if (notify) {
        const char *verb = IS_NULLSTR(action_name) ? "event action" : action_name;
        const char *label = !IS_NULLSTR(inst->def->display_title)
            ? inst->def->display_title
            : event_index_get_name(inst->def);

        printf_to_char(ch,
            "{YYou are now participating in %s via %s.{x\n\r"
            "{WUse: {xevent list {Wor{x event info %s\n\r",
            label,
            verb,
            inst->def->name);
    }

    return true;
}

bool event_runtime_ensure_participation_for_action(CHAR_DATA *ch, long event_uid,
    uint32_t instance_id, const char *action_name, bool notify)
{
    EVENT_INSTANCE *inst;

    if (!ch || IS_NPC(ch) || IS_IMMORTAL(ch) || event_uid <= 0 || instance_id == 0)
        return true;

    inst = event_find_runtime_by_ref(event_uid, instance_id);
    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return true;

    if (IS_SET(inst->def->flags, EVT_FLAG_PASSIVE) || inst->def->event_type == EVT_TYPE_WORLDSTATE)
        return true;

    if (!event_find_participant(inst, ch)) {
        if (!event_try_auto_add_participant(inst, ch, action_name, notify)) {
            if (notify)
                send_to_char("You are not participating in that event. Join it first with event join.\n\r", ch);
            return false;
        }
    }

    if (!event_character_in_scope(inst, ch)) {
        if (notify)
            send_to_char("You are outside this event's active area and cannot participate right now.\n\r", ch);
        return false;
    }

    return true;
}

void event_notify_active_events_for_char(CHAR_DATA *ch, bool area_only)
{
    EVENT_RUNTIME_REF refs[32];
    int count;

    if (!ch || IS_NPC(ch) || !ch->in_room || !ch->in_room->area)
        return;

    if (!pref_get_bool(ch->desc ? ch->desc->account : NULL, ch, "eventnotify", true))
        return;

    if (area_only)
        count = event_runtime_collect_for_area(ch->in_room->area, refs, 32);
    else
        count = event_runtime_collect_for_character(ch, refs, 32);

    if (count <= 0)
        return;

    printf_to_char(ch,
        "{YThere %s {W%d{x active event%s here. {WUse:{x event list {Wor{x event info <idx|name>.\n\r",
        count == 1 ? "is" : "are",
        count,
        count == 1 ? " is" : "s are");
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

static bool event_runtime_resolve_script(const EVTEDIT_DATA *evt, long script_vnum,
    WNUM *wnum, SCRIPT_DATA **script)
{
    AREA_DATA *context_area;

    if (!wnum || !script || !evt || script_vnum <= 0)
        return false;

    context_area = evt->area;
    if (!resolve_widevnum(script_vnum, context_area, wnum) || !wnum->pArea || wnum->vnum < 1)
        return false;

    *script = get_script_index(wnum->pArea, wnum->vnum, PRG_APROG);
    return *script != NULL;
}

static void event_runtime_run_phase_script(EVENT_INSTANCE *inst, long script_vnum, const char *phase_name)
{
    WNUM wnum;
    SCRIPT_DATA *script;

    if (!inst || !inst->def || script_vnum <= 0)
        return;

    if (!event_runtime_resolve_script(inst->def, script_vnum, &wnum, &script))
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

static void event_runtime_run_stage_tick_script(EVENT_INSTANCE *inst, const EVT_STAGE_DEF *stage)
{
    WNUM wnum;
    SCRIPT_DATA *script;
    const char *stage_name;

    if (!inst || !inst->def || !stage || stage->on_tick_script <= 0)
        return;

    if (!event_runtime_resolve_script(inst->def, stage->on_tick_script, &wnum, &script))
        return;

    stage_name = IS_NULLSTR(stage->name) ? "stage_tick" : stage->name;

    execute_script(script->vnum, script,
        NULL, NULL, NULL, NULL,
        wnum.pArea, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL,
        (char *)stage_name,
        "stage_tick",
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
        event_runtime_spawn_roster_for_phase(inst, inst->phase_index);

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

    if (!event_runtime_resolve_script(inst->def, script_vnum, &wnum, &script))
        return;

    trigger = success ? "event_complete" : "event_fail";
    phrase = IS_NULLSTR(reason) ? trigger : reason;

    for (part = inst->participants; part; part = part->next) {
        if (!part->ch || IS_NPC(part->ch))
            continue;

        if (success && part->event_completed)
            continue;

        if (success)
            part->event_completed = true;

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

static void event_runtime_run_phase_reward_script(EVENT_INSTANCE *inst,
    int completed_phase_index, const char *completed_phase_name)
{
    WNUM wnum;
    SCRIPT_DATA *script;
    EVENT_PART *part;
    bool ran = false;
    const char *phase_name;

    if (!inst || !inst->def || inst->def->reward_phase_script <= 0)
        return;

    if (!event_runtime_resolve_script(inst->def, inst->def->reward_phase_script, &wnum, &script))
        return;

    phase_name = IS_NULLSTR(completed_phase_name)
        ? formatf("phase_%d", UMAX(0, completed_phase_index) + 1)
        : completed_phase_name;

    for (part = inst->participants; part; part = part->next) {
        if (!part->ch || IS_NPC(part->ch))
            continue;

        part->phases_completed++;

        execute_script(script->vnum, script,
            NULL, NULL, NULL, NULL,
            wnum.pArea, NULL, NULL,
            part->ch, NULL, NULL, NULL, NULL, NULL,
            NULL,
            (char *)phase_name,
            "event_phase_complete",
            TRIG_NONE,
            (int)inst->def->uid,
            (int)inst->instance_id,
            completed_phase_index + 1,
            part->phases_completed,
            inst->participant_count);
        ran = true;
    }

    if (!ran)
        return;

    inst->dirty = true;
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

    if (!event_phase_step_get(inst->def, phase_index,
            phase_name, sizeof(phase_name),
            &phase_minutes, &phase_script_vnum))
        return false;

    event_runtime_apply_phase_step(inst, phase_index, phase_name, phase_minutes, phase_script_vnum);
    return true;
}

static void event_runtime_update_phase_timers(EVENT_INSTANCE *inst)
{
    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    while (inst->phase_due > 0 && current_time >= inst->phase_due) {
        int completed_phase_index = inst->phase_index;
        char completed_phase_name[MIL];

        completed_phase_name[0] = '\0';
        if (!IS_NULLSTR(inst->phase_name)) {
            strncpy(completed_phase_name, inst->phase_name, sizeof(completed_phase_name) - 1);
            completed_phase_name[sizeof(completed_phase_name) - 1] = '\0';
        }

        if (completed_phase_index >= 0)
            event_runtime_run_phase_reward_script(inst, completed_phase_index, completed_phase_name);

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

    if (evt->cooldown_until > current_time)
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
        event_runtime_spawn_roster_for_phase(inst, inst->phase_index);
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
        variable_freelist(&it->runtime_vars);
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
    WNUM_LOAD wload;

    if (IS_NULLSTR(token))
        return NULL;

    if (strchr(token, '#') && parse_widevnum_load(token, &wload) && wload.auid > 0) {
        AREA_DATA *area = get_area_from_uid(wload.auid);
        if (area && wload.vnum > 0)
            return get_event_index_for_area(area, wload.vnum);
    }

    if (is_number(token))
        return get_event_index(atol(token));

    if (!str_cmp(token, "gq") || !str_cmp(token, "globalquest")) {
        for (AREA_DATA *area = area_first; area; area = area->next)
            for (int hash = 0; hash < MAX_KEY_HASH; hash++)
                for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash)
                    if (evt->event_type == EVT_TYPE_COLLECTION)
                        return evt;
    }

    if (!str_cmp(token, "invasion")) {
        for (AREA_DATA *area = area_first; area; area = area->next)
            for (int hash = 0; hash < MAX_KEY_HASH; hash++)
                for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash)
                    if (evt->event_type == EVT_TYPE_INVASION)
                        return evt;
    }

    if (!str_cmp(token, "autowar") || !str_cmp(token, "war")) {
        for (AREA_DATA *area = area_first; area; area = area->next)
            for (int hash = 0; hash < MAX_KEY_HASH; hash++)
                for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash)
                    if (evt->event_type == EVT_TYPE_WAR_FFA
                        || evt->event_type == EVT_TYPE_WAR_GENOCIDE
                        || evt->event_type == EVT_TYPE_WAR_JIHAD)
                        return evt;
    }

    for (AREA_DATA *area = area_first; area; area = area->next)
        for (int hash = 0; hash < MAX_KEY_HASH; hash++)
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash)
                if (!str_cmp(evt->name, token))
                    return evt;

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
    AREA_DATA *area;
    int hash;
    EVENT_INSTANCE *inst;
    EVENT_INSTANCE *next;

    evtedit_ensure_loaded();

    if (!event_system_enabled)
        return;

    for (area = area_first; area; area = area->next) {
        for (hash = 0; hash < MAX_KEY_HASH; hash++) {
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
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
        }
    }

    for (inst = event_active_head; inst; inst = next) {
        EVTEDIT_DATA *def;
        long inst_uid;
        uint32_t inst_id;
        EVT_STAGE_DEF *active_stage;

        next = inst->next;
        def = inst->def;

        if (!def)
            continue;

        inst_uid = def->uid;
        inst_id = inst->instance_id;

        active_stage = event_stage_get_by_index(def, inst->phase_index);
        if (active_stage)
            event_runtime_run_stage_tick_script(inst, active_stage);

        if (!event_find_runtime_by_ref(inst_uid, inst_id))
            continue;

        event_runtime_update_phase_timers(inst);

        if (!event_find_runtime_by_ref(inst_uid, inst_id))
            continue;

        if (inst->end_time > 0 && current_time >= inst->end_time) {
            if (!IS_NULLSTR(def->end_msg))
                event_broadcast(def->end_msg);
            event_stop_definition(def);
        }
    }
}

static void event_runtime_activate_leader_phase(EVENT_INSTANCE *inst)
{
    EVT_PHASE_DEF *phase;
    int phase_index;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    if (inst->leader_phase)
        return;

    phase_index = 0;
    for (phase = inst->def->phases; phase; phase = phase->next, phase_index++) {
        if (!IS_NULLSTR(phase->name)
            && (!str_cmp(phase->name, "leader") || !str_cmp(phase->name, "leader_phase"))) {
            event_runtime_apply_phase_step(inst, phase_index, phase->name,
                UMAX(0, phase->minutes), UMAX(0, phase->script_vnum));
            inst->leader_phase = true;
            return;
        }
    }

    inst->leader_phase = true;
    inst->phase_index = -1;
    inst->phase_due = 0;
    strncpy(inst->phase_name, "leader", sizeof(inst->phase_name) - 1);
    inst->phase_name[sizeof(inst->phase_name) - 1] = '\0';
    inst->dirty = true;
    event_runtime_spawn_roster_for_phase(inst, inst->phase_index);
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
        if (!part) {
            if (!event_try_auto_add_participant(inst, killer, "combat", true))
                continue;
            part = event_find_participant(inst, killer);
            if (!part)
                continue;
        }

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
                && (!event_roster_has_boss_for_phase(inst)
                    || event_mobile_matches_roster_boss(inst, victim))) {
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
        if (!part) {
            if (!event_try_auto_add_participant(inst, ch, "collection", true))
                continue;
            part = event_find_participant(inst, ch);
            if (!part)
                continue;
        }

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
        if (!event_find_participant(inst, killer)) {
            if (!event_try_auto_add_participant(inst, killer, "combat", true))
                continue;
        }
        if (event_roster_has_boss_for_phase(inst)
            && !event_mobile_matches_roster_boss(inst, victim))
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

    mob->event_source.pArea = NULL;
    mob->event_source.vnum = UMAX(0, event_uid);
    mob->event_source_instance_id = instance_id;
    mob->event_source_bracket = 0;
}

void event_tag_object_spawn(OBJ_DATA *obj, long event_uid, uint32_t instance_id)
{
    if (!obj)
        return;

    obj->event_source.pArea = NULL;
    obj->event_source.vnum = UMAX(0, event_uid);
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
        *event_uid = mob->event_source.vnum;
    if (instance_id)
        *instance_id = mob->event_source_instance_id;

    return mob->event_source.vnum > 0;
}

bool event_get_object_spawn_source(const OBJ_DATA *obj, long *event_uid, uint32_t *instance_id)
{
    if (!obj)
        return false;

    if (event_uid)
        *event_uid = obj->event_source.vnum;
    if (instance_id)
        *instance_id = obj->event_source_instance_id;

    return obj->event_source.vnum > 0;
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

static EVT_STAGE_DEF *event_stage_get_by_index(const EVTEDIT_DATA *evt, int index)
{
    EVT_STAGE_DEF *stage;
    int i;

    if (!evt || index < 0)
        return NULL;

    i = 0;
    for (stage = evt->stages; stage; stage = stage->next, i++)
        if (i == index)
            return stage;

    return NULL;
}

static bool event_stage_objective_met(const EVENT_INSTANCE *inst,
    const EVT_STAGE_OBJECTIVE_DEF *objective)
{
    int target;

    if (!inst || !inst->def || !objective)
        return false;

    target = UMAX(0, objective->target_count);

    switch (objective->objective_type) {
    case EVT_STAGE_OBJECTIVE_KILL:
        if (target <= 0)
            return true;
        return event_runtime_goal_met(inst, target, false);

    case EVT_STAGE_OBJECTIVE_COLLECT:
        if (target <= 0)
            return true;
        return event_runtime_goal_met(inst, target, true);

    case EVT_STAGE_OBJECTIVE_SURVIVE:
        if (target <= 0)
            return true;
        return (long)(current_time - inst->started_at) >= (long)target * 60L;

    case EVT_STAGE_OBJECTIVE_CUSTOM:
    default:
        return false;
    }
}

static float event_stage_objective_progress(const EVENT_INSTANCE *inst,
    const EVT_STAGE_OBJECTIVE_DEF *objective)
{
    int target;
    float ratio;

    if (!inst || !inst->def || !objective)
        return 0.0f;

    target = UMAX(0, objective->target_count);
    if (target <= 0)
        return 1.0f;

    switch (objective->objective_type) {
    case EVT_STAGE_OBJECTIVE_KILL:
        ratio = (float)inst->progress_kills / (float)target;
        break;

    case EVT_STAGE_OBJECTIVE_COLLECT:
        ratio = (float)inst->progress_items / (float)target;
        break;

    case EVT_STAGE_OBJECTIVE_SURVIVE:
        ratio = (float)((long)(current_time - inst->started_at)) / (float)(target * 60L);
        break;

    case EVT_STAGE_OBJECTIVE_CUSTOM:
    default:
        ratio = event_stage_objective_met(inst, objective) ? 1.0f : 0.0f;
        break;
    }

    if (ratio < 0.0f)
        ratio = 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;

    return ratio;
}

static float event_stage_completion_ratio(const EVENT_INSTANCE *inst,
    const EVT_STAGE_DEF *stage)
{
    const EVT_STAGE_OBJECTIVE_DEF *objective;
    float ratio = 0.0f;
    float sum = 0.0f;
    int count = 0;

    if (!inst || !stage)
        return 0.0f;

    if (!stage->objectives || stage->objective_count <= 0)
        return (stage->transition_mode == EVT_STAGE_TRANSITION_ON_COMPLETE) ? 0.0f : 1.0f;

    if (stage->objective_mode == EVT_STAGE_OBJECTIVE_ANY) {
        for (objective = stage->objectives; objective; objective = objective->next) {
            float current = event_stage_objective_progress(inst, objective);
            if (current > ratio)
                ratio = current;
        }
        return ratio;
    }

    for (objective = stage->objectives; objective; objective = objective->next) {
        sum += event_stage_objective_progress(inst, objective);
        count++;
    }

    if (count <= 0)
        return 0.0f;

    ratio = sum / (float)count;
    if (ratio < 0.0f)
        ratio = 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;
    return ratio;
}

static float event_instance_completion_ratio(const EVENT_INSTANCE *inst)
{
    const EVTEDIT_DATA *evt;
    const EVT_STAGE_DEF *stage;
    float ratio;
    int stage_count;
    int completed_stages;
    int current_stage;

    if (!inst || !inst->def)
        return 0.0f;

    if (inst->state == EVTS_COMPLETE)
        return 1.0f;

    evt = inst->def;
    stage_count = UMAX(0, evt->stage_count);

    if (stage_count > 0) {
        current_stage = URANGE(0, inst->phase_index, stage_count - 1);
        completed_stages = URANGE(0, current_stage, stage_count);
        stage = event_stage_get_by_index(evt, current_stage);

        ratio = ((float)completed_stages + event_stage_completion_ratio(inst, stage)) / (float)stage_count;
    } else if (evt->completion_goal > 0) {
        int current = inst->progress_kills;

        if (evt->event_type == EVT_TYPE_COLLECTION)
            current = inst->progress_items;

        ratio = (float)current / (float)evt->completion_goal;
    } else {
        ratio = 0.0f;
    }

    if (ratio < 0.0f)
        ratio = 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;

    return ratio;
}

static bool event_stage_objectives_complete(const EVENT_INSTANCE *inst,
    const EVT_STAGE_DEF *stage)
{
    const EVT_STAGE_OBJECTIVE_DEF *objective;
    bool any_met = false;

    if (!inst || !stage)
        return false;

    if (stage->objective_count <= 0 || !stage->objectives)
        return stage->transition_mode != EVT_STAGE_TRANSITION_ON_COMPLETE;

    if (stage->objective_mode == EVT_STAGE_OBJECTIVE_ANY) {
        for (objective = stage->objectives; objective; objective = objective->next)
            if (event_stage_objective_met(inst, objective))
                return true;
        return false;
    }

    for (objective = stage->objectives; objective; objective = objective->next) {
        any_met = true;
        if (!event_stage_objective_met(inst, objective))
            return false;
    }

    return any_met;
}

static void event_runtime_resolve_completion(EVENT_INSTANCE *inst)
{
    EVT_STAGE_DEF *stage;
    bool complete;

    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return;

    stage = event_stage_get_by_index(inst->def, inst->phase_index);
    if (!stage)
        return;

    if (stage->transition_mode != EVT_STAGE_TRANSITION_ON_COMPLETE)
        return;

    complete = event_stage_objectives_complete(inst, stage);
    if (!complete)
        return;

    if (stage->on_complete_script > 0)
        event_runtime_run_phase_script(inst, stage->on_complete_script, stage->name);

    if (!event_runtime_apply_phase_by_index(inst, inst->phase_index + 1))
        (void)event_runtime_complete_instance(inst, true, NULL,
            "{YEvent stage objectives complete. Event finished!{x\n\r");
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

bool event_runtime_get_source_stage_progress(long event_uid, uint32_t instance_id,
    int *stage_index, int *stage_count, int *completion_percent)
{
    EVENT_INSTANCE *inst;
    float completion_ratio;

    if (stage_index)
        *stage_index = 0;
    if (stage_count)
        *stage_count = 0;
    if (completion_percent)
        *completion_percent = 0;

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        if (stage_count)
            *stage_count = UMAX(0, inst->def->stage_count);

        if (stage_index) {
            if (inst->def->stage_count > 0)
                *stage_index = URANGE(1, inst->phase_index + 1, inst->def->stage_count);
            else
                *stage_index = 0;
        }

        if (completion_percent) {
            completion_ratio = event_instance_completion_ratio(inst);
            *completion_percent = URANGE(0, (int)(completion_ratio * 100.0f + 0.5f), 100);
        }

        return true;
    }

    return false;
}

bool event_runtime_get_source_objective_progress(long event_uid, uint32_t instance_id,
    int *objectives_met, int *objectives_total, int *objective_percent)
{
    EVENT_INSTANCE *inst;
    EVT_STAGE_DEF *stage;
    int met = 0;
    int total = 0;
    float ratio = 0.0f;
    EVT_STAGE_OBJECTIVE_DEF *objective;

    if (objectives_met)
        *objectives_met = 0;
    if (objectives_total)
        *objectives_total = 0;
    if (objective_percent)
        *objective_percent = 0;

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        stage = event_stage_get_by_index(inst->def, inst->phase_index);
        if (!stage)
            return true;

        total = UMAX(0, stage->objective_count);
        ratio = event_stage_completion_ratio(inst, stage);

        for (objective = stage->objectives; objective; objective = objective->next)
            if (event_stage_objective_met(inst, objective))
                met++;

        if (objectives_met)
            *objectives_met = met;
        if (objectives_total)
            *objectives_total = total;
        if (objective_percent)
            *objective_percent = URANGE(0, (int)(ratio * 100.0f + 0.5f), 100);

        return true;
    }

    return false;
}

static bool event_runtime_matches_area(const EVENT_INSTANCE *inst, const AREA_DATA *area)
{
    if (!inst || !inst->def || inst->state != EVTS_ACTIVE)
        return false;

    if (inst->def->scope_type == EVT_SCOPE_GLOBAL)
        return true;

    if (!area || inst->scope_area_uid <= 0)
        return false;

    return inst->scope_area_uid == area->uid;
}

static bool event_runtime_matches_room(const EVENT_INSTANCE *inst, const ROOM_INDEX_DATA *room)
{
    if (!event_runtime_matches_area(inst, room ? room->area : NULL))
        return false;

    if (!inst || !inst->def)
        return false;

    switch (inst->def->scope_type) {
    case EVT_SCOPE_REGION:
    case EVT_SCOPE_ZONES:
    case EVT_SCOPE_BATTLEFIELD:
        return room && get_room_region((ROOM_INDEX_DATA *)room) != NULL;
    default:
        return true;
    }
}

static bool event_runtime_ref_exists(const EVENT_RUNTIME_REF *out, int count, long uid, uint32_t instance_id)
{
    int index;

    if (!out || count <= 0)
        return false;

    for (index = 0; index < count; index++) {
        if (out[index].uid == uid && out[index].instance_id == instance_id)
            return true;
    }

    return false;
}

static int event_runtime_ref_push(EVENT_RUNTIME_REF *out, int max_out, int count, long uid, uint32_t instance_id)
{
    if (uid <= 0)
        return count;

    if (event_runtime_ref_exists(out, count, uid, instance_id))
        return count;

    if (out && count < max_out) {
        out[count].uid = uid;
        out[count].instance_id = instance_id;
    }

    return count + 1;
}

static ROOM_INDEX_DATA *event_runtime_instance_anchor_room(const INSTANCE *instance)
{
    if (!instance)
        return NULL;

    if (instance->entrance)
        return instance->entrance;
    if (instance->recall)
        return instance->recall;
    if (instance->environ)
        return instance->environ;
    if (instance->exit)
        return instance->exit;

    return NULL;
}

static ROOM_INDEX_DATA *event_runtime_dungeon_anchor_room(const DUNGEON *dungeon)
{
    if (!dungeon)
        return NULL;

    if (dungeon->entry_room)
        return dungeon->entry_room;
    if (dungeon->exit_room)
        return dungeon->exit_room;

    return NULL;
}

int event_runtime_collect_for_area(const AREA_DATA *area, EVENT_RUNTIME_REF *out, int max_out)
{
    EVENT_INSTANCE *inst;
    int count = 0;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!event_runtime_matches_area(inst, area))
            continue;

        count = event_runtime_ref_push(out, max_out, count, inst->def->uid, inst->instance_id);
    }

    return count;
}

int event_runtime_collect_for_room(const ROOM_INDEX_DATA *room, EVENT_RUNTIME_REF *out, int max_out)
{
    EVENT_INSTANCE *inst;
    int count = 0;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!event_runtime_matches_room(inst, room))
            continue;

        count = event_runtime_ref_push(out, max_out, count, inst->def->uid, inst->instance_id);
    }

    return count;
}

int event_runtime_collect_for_instance(const INSTANCE *instance, EVENT_RUNTIME_REF *out, int max_out)
{
    ROOM_INDEX_DATA *anchor;

    anchor = event_runtime_instance_anchor_room(instance);
    return event_runtime_collect_for_room(anchor, out, max_out);
}

int event_runtime_collect_for_dungeon(const DUNGEON *dungeon, EVENT_RUNTIME_REF *out, int max_out)
{
    ROOM_INDEX_DATA *anchor;

    anchor = event_runtime_dungeon_anchor_room(dungeon);
    return event_runtime_collect_for_room(anchor, out, max_out);
}

int event_runtime_collect_for_character(const CHAR_DATA *ch, EVENT_RUNTIME_REF *out, int max_out)
{
    long source_uid = 0;
    uint32_t source_instance = 0;
    long active_uid = 0;
    uint32_t active_instance = 0;
    int count;

    count = event_runtime_collect_for_room(ch ? ch->in_room : NULL, out, max_out);

    if (event_get_mobile_spawn_source(ch, &source_uid, &source_instance))
        count = event_runtime_ref_push(out, max_out, count, source_uid, source_instance);

    if (event_get_character_active_bracket(ch, &active_uid, &active_instance, NULL))
        count = event_runtime_ref_push(out, max_out, count, active_uid, active_instance);

    return count;
}

int event_runtime_collect_for_object(const OBJ_DATA *obj, EVENT_RUNTIME_REF *out, int max_out)
{
    ROOM_INDEX_DATA *room;
    long source_uid = 0;
    uint32_t source_instance = 0;
    int count;

    room = obj ? obj_room((OBJ_DATA *)obj) : NULL;
    count = event_runtime_collect_for_room(room, out, max_out);

    if (event_get_object_spawn_source(obj, &source_uid, &source_instance))
        count = event_runtime_ref_push(out, max_out, count, source_uid, source_instance);

    return count;
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

bool event_runtime_check_objectives(const char *event_token)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    event_runtime_resolve_completion(inst);
    return true;
}

bool event_runtime_set_phase(const char *event_token, const char *phase_name)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;
    int phase_index = 0;
    EVT_PHASE_DEF *phase;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    for (phase = evt->phases; phase; phase = phase->next) {
        if (!IS_NULLSTR(phase_name) && !str_cmp(phase_name, phase->name)) {
            event_runtime_apply_phase_step(inst, phase_index, phase->name,
                UMAX(0, phase->minutes), UMAX(0, phase->script_vnum));
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

bool event_runtime_start(const char *event_token, CHAR_DATA *starter, long *event_uid_out, uint32_t *instance_id_out)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (event_uid_out)
        *event_uid_out = 0;
    if (instance_id_out)
        *instance_id_out = 0;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_start_definition(evt, starter);
    if (!inst)
        return false;

    evt->scheduled_time = 0;

    if (event_uid_out)
        *event_uid_out = evt->uid;
    if (instance_id_out)
        *instance_id_out = inst->instance_id;

    return true;
}

bool event_runtime_stop(const char *event_token)
{
    EVTEDIT_DATA *evt;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    return event_stop_definition(evt);
}

bool event_runtime_get_vars_by_ref(long event_uid, uint32_t instance_id, pVARIABLE **vars_out)
{
    EVENT_INSTANCE *inst;

    if (vars_out)
        *vars_out = NULL;

    if (event_uid <= 0)
        return false;

    for (inst = event_active_head; inst; inst = inst->next) {
        if (!inst->def || inst->state != EVTS_ACTIVE)
            continue;
        if (inst->def->uid != event_uid)
            continue;
        if (instance_id > 0 && inst->instance_id != instance_id)
            continue;

        if (vars_out)
            *vars_out = &inst->runtime_vars;
        return true;
    }

    return false;
}

bool event_runtime_get_vars(const char *event_token, pVARIABLE **vars_out)
{
    EVTEDIT_DATA *evt;
    EVENT_INSTANCE *inst;

    if (vars_out)
        *vars_out = NULL;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    inst = event_find_active_def(evt);
    if (!inst || inst->state != EVTS_ACTIVE)
        return false;

    if (vars_out)
        *vars_out = &inst->runtime_vars;
    return true;
}

bool event_index_get_vars_by_uid(long event_uid, pVARIABLE **vars_out)
{
    EVTEDIT_DATA *evt;

    if (vars_out)
        *vars_out = NULL;

    if (event_uid <= 0)
        return false;

    evt = get_event_index(event_uid);
    if (!evt)
        return false;

    if (vars_out)
        *vars_out = &evt->index_vars;
    return true;
}

bool event_index_get_vars(const char *event_token, pVARIABLE **vars_out)
{
    EVTEDIT_DATA *evt;

    if (vars_out)
        *vars_out = NULL;

    evt = event_lookup_definition(event_token);
    if (!evt)
        return false;

    if (vars_out)
        *vars_out = &evt->index_vars;
    return true;
}

static void evtedit_ensure_loaded(void)
{
    AREA_DATA *area;
    EVTEDIT_DATA *evt;
    int hash;

    if (evtedit_booted)
        return;

    evtedit_booted = true;

    for (area = area_first; area; area = area->next)
        for (hash = 0; hash < MAX_KEY_HASH; hash++)
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash)
            {
                event_phase_sync_from_legacy(evt);
                event_stage_sync_from_phases(evt);
            }

    evtedit_rebuild_global_index();
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
    { "stages",    evtedit_stages },
    { "rewardphase", evtedit_rewardphase },
    { "rewardsuccess", evtedit_rewardsuccess },
    { "rewardfail", evtedit_rewardfail },
    { "flags",     evtedit_flags },
    { "comments",  evtedit_comments },
    { "addeprog",  evtedit_addeprog },
    { "deleprog",  evtedit_deleprog },
    { "varset",    evtedit_varset },
    { "varclear",  evtedit_varclear },
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
        .count      = 6,
        .tabs       = {
            { "Identity", "Id", evtedit_show_identity_tab },
            { "Schedule", "Sch", evtedit_show_schedule_tab },
            { "Stages", "Stg", evtedit_show_stages_tab },
            { "Messages", "Msg", evtedit_show_messages_tab },
            { "Meta", "Meta", evtedit_show_meta_tab },
            { "Scripting", "Script", evtedit_show_scripting_tab },
        },
    },
    .theme          = &olc_theme_system,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_AREA_FLAG,
    .get_area_fn    = evtedit_get_area,
    .audit_changes  = false,
};

void do_evtedit(CHAR_DATA *ch, char *argument)
{
    EVTEDIT_DATA *evt;
    char arg[MIL];
    WNUM wnum;

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
        send_to_char("        evtedit create [<auid>#<vnum>] [name]\n\r", ch);
        send_to_char("        evtedit save\n\r", ch);
        send_to_char("        evtedit reload\n\r", ch);
        send_to_char("        evtedit <auid>#<vnum>|#<vnum>|<name>\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("Bracket format: ordered non-overlapping ranges (example: 1-50,51-90,91+).\n\r", ch);
        send_to_char("bracketmode: auto_by_level|open|manual\n\r", ch);
        send_to_char("scopeanchor: <area_uid|here|clear>\n\r", ch);
        send_to_char("scopefloating: [on|off]\n\r", ch);
        send_to_char("roster: list|addnpc|addobj|boss|stage|remove|clear\n\r", ch);
        send_to_char("progressagg: shared|total|per_bracket_any|per_bracket|per_bracket_all_required\n\r", ch);
        send_to_char("stages: use subcommands (list/add/insert/set/name/minutes/script/remove/transition/tickscript/obj*)\n\r", ch);
        send_to_char("stages mobadd/objaddspawn: stage-native roster helpers\n\r", ch);
        send_to_char("rewardphase/rewardsuccess/rewardfail: <scriptvnum|0>\n\r", ch);
        send_to_char("addeprog/deleprog: manage attached event progs\n\r", ch);
        send_to_char("varset/varclear: manage event index variables\n\r", ch);
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

    if (evtedit_parse_index_ref(ch, arg, &wnum))
        evt = get_event_index_for_area(wnum.pArea, wnum.vnum);
    else if (is_number(arg))
        evt = evtedit_find_uid(atol(arg));
    else
        evt = evtedit_find_name(arg);

    if (!evt) {
        send_to_char("No event found by that uid or name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &evtedit_def, evt, true);
}

void do_events(CHAR_DATA *ch, char *argument)
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
        send_to_char("Syntax: events list\n\r", ch);
        send_to_char("        events info <uid|name>\n\r", ch);
        send_to_char("        events news <uid|name>\n\r", ch);
        send_to_char("        events enabled\n\r", ch);
        send_to_char("        events enable|disable\n\r", ch);
        send_to_char("        events status\n\r", ch);
        send_to_char("        events join [uid|name]\n\r", ch);
        send_to_char("        events leave [uid|name]\n\r", ch);
        send_to_char("        events start <uid|name>\n\r", ch);
        send_to_char("        events stop <uid|name>\n\r", ch);
        send_to_char("        events schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
        send_to_char("        events tick\n\r", ch);
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

        send_to_char("Event runtime state changed.\n\r", ch);

        send_to_char(turn_on ? "Event system enabled.\n\r"
                         : "Event system disabled; active events stopped.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "list")) {
        AREA_DATA *area;
        int hash;
        int band;
        bool printed_any = false;

        for (band = 0; band <= 3; band++) {
            bool printed_header = false;

            for (area = area_first; area; area = area->next) {
                for (hash = 0; hash < MAX_KEY_HASH; hash++) {
                    for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                        if (event_list_scope_band_for_player(evt, ch) != band)
                            continue;

                        if (!printed_header) {
                            printf_to_char(ch, "{W[%s]{X\n\r", event_list_scope_title(band));
                            send_to_char("{WWNUM           Name                     Type           Schedule       State    Enabled{X\n\r", ch);
                            send_to_char("{D-------------- ------------------------ -------------- -------------- -------- -------{X\n\r", ch);
                            printed_header = true;
                            printed_any = true;
                        }

                        inst = event_find_active_def(evt);
                        printf_to_char(ch, "{W%-14s {x%-24.24s %-14s %-14s %-8s %s{X\n\r",
                            widevnum_string_event(evt, NULL),
                            evt->name,
                            flag_name(evt_type_flags, evt->event_type),
                            flag_name(evt_sched_flags, evt->sched_type),
                            inst ? "{GACTIVE{x" : "{Didle{x",
                            evt->enabled ? "{GYes{x" : "{RNo{x");
                    }
                }
            }
        }

        if (!printed_any)
            send_to_char("No events available in your current scope.\n\r", ch);

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
        printf_to_char(ch, "{WDescription:{x %s\n\r",
            IS_NULLSTR(evt->description) ? "(not set)" : evt->description);
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
        if (evt->phase_count > 0)
            printf_to_char(ch, "{WPhases:{x %d configured\n\r", evt->phase_count);
        if (evt->reward_phase_script > 0
            || evt->reward_success_script > 0
            || evt->reward_failure_script > 0)
            printf_to_char(ch, "{WReward Hooks:{x phase=%ld success=%ld fail=%ld\n\r",
                evt->reward_phase_script,
                evt->reward_success_script,
                evt->reward_failure_script);
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
            float completion_ratio = event_instance_completion_ratio(inst);
            int percent_complete = (int)(completion_ratio * 100.0f + 0.5f);
            int current_stage = 0;
            int stage_total = 0;

            self = event_find_participant(inst, ch);

            printf_to_char(ch, "{WOverall Completion:{x %d%%\n\r", URANGE(0, percent_complete, 100));

            stage_total = UMAX(0, evt->stage_count);
            if (stage_total > 0) {
                current_stage = URANGE(1, inst->phase_index + 1, stage_total);
                printf_to_char(ch, "{WStage Progress:{x %d/%d\n\r", current_stage, stage_total);
            }

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
                printf_to_char(ch, "{WYour Contribution:{x kills=%d turn-ins=%d phases=%d event=%s%s\n\r",
                    self->kills,
                    self->items_turned,
                    self->phases_completed,
                    self->event_completed ? "yes" : "no",
                    self->team > 0 ? formatf(" bracket=%d", self->team) : "");

            if (!IS_NULLSTR(inst->phase_name))
                printf_to_char(ch, "{WCurrent Phase:{x %s\n\r", inst->phase_name);

            if (inst->phase_due > current_time)
                printf_to_char(ch, "{WNext Phase:{x in %ldm%02lds\n\r",
                    (long)(inst->phase_due - current_time) / 60,
                    (long)(inst->phase_due - current_time) % 60);
        } else {
            printf_to_char(ch, "{WOverall Completion:{x 0%%\n\r");
        }

        if (evt->scheduled_time > current_time)
            printf_to_char(ch, "{WScheduled:{x in %ldm%02lds\n\r",
                (long)(evt->scheduled_time - current_time) / 60,
                (long)(evt->scheduled_time - current_time) % 60);
        if (evt->next_auto_time > current_time)
            printf_to_char(ch, "{WNext Auto:{x in %ldm%02lds\n\r",
                (long)(evt->next_auto_time - current_time) / 60,
                (long)(evt->next_auto_time - current_time) % 60);

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
            send_to_char("Syntax: events schedule <uid|name> <+Nm|+Nh|+Nd|YYYY-MM-DD HH:MM>\n\r", ch);
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

    send_to_char("Unknown events subcommand. Type 'events help'.\n\r", ch);
}

void do_event(CHAR_DATA *ch, char *argument)
{
    char cmd[MIL];
    char original[MSL];
    char target[MIL];
    EVTEDIT_DATA *evt = NULL;
    EVENT_INSTANCE *inst = NULL;
    bool soon = false;
    long starts_in = 0;
    int band = -1;

    if (IS_NPC(ch))
        return;

    strlcpy(original, argument ? argument : "", sizeof(original));
    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd) || !str_cmp(cmd, "help")) {
        send_to_char("Syntax: event list\n\r", ch);
        send_to_char("        event info <uid|name|idx>\n\r", ch);
        send_to_char("        event news <uid|name|idx>\n\r", ch);
        send_to_char("        event status\n\r", ch);
        send_to_char("        event join [uid|name|idx]\n\r", ch);
        send_to_char("        event leave [uid|name|idx]\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("For admin/runtime controls use: events <subcommand>\n\r", ch);
        send_to_char("Passive/worldstate events are status-only and cannot be joined.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "list")) {
        AREA_DATA *area;
        int hash;
        int scope_band;
        int idx = 0;
        bool printed_any = false;

        send_to_char("{WEvents ({Gactive{x + {Ystarting soon{x):{X\n\r", ch);

        for (scope_band = 0; scope_band <= 3; scope_band++) {
            bool printed_header = false;

            for (area = area_first; area; area = area->next) {
                for (hash = 0; hash < MAX_KEY_HASH; hash++) {
                    for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                        EVENT_PART *self;

                        if (!event_player_should_show(evt, ch, &inst, &soon, &starts_in, &band))
                            continue;
                        if (band != scope_band)
                            continue;

                        idx++;
                        if (!printed_header) {
                            printf_to_char(ch, "{W[%s]{X\n\r", event_list_scope_title(scope_band));
                            send_to_char("{WIdx  Event                     Type           Status            Joinable{X\n\r", ch);
                            send_to_char("{D---- ------------------------ -------------- ----------------- --------{X\n\r", ch);
                            printed_header = true;
                            printed_any = true;
                        }

                        self = inst ? event_find_participant(inst, ch) : NULL;

                        printf_to_char(ch, "{W%3d){x %-24.24s %-14s %-17s %s{X\n\r",
                            idx,
                            evt->name,
                            flag_name(evt_type_flags, evt->event_type),
                            inst
                                ? formatf("{GACTIVE{x%s", self ? " {G(joined){x" : "")
                                : formatf("{Ystarts in %ldm%02lds{x", starts_in / 60, starts_in % 60),
                            (inst && !IS_SET(evt->flags, EVT_FLAG_PASSIVE)
                                && evt->event_type != EVT_TYPE_WORLDSTATE)
                                ? "{Gyes{x"
                                : "{Dno{x");
                    }
                }
            }
        }

        if (!printed_any)
            send_to_char("No active or soon events are available in your current scope.\n\r", ch);

        return;
    }

    if (!str_cmp(cmd, "enabled")
        || !str_cmp(cmd, "enable")
        || !str_cmp(cmd, "disable")
        || !str_cmp(cmd, "start")
        || !str_cmp(cmd, "stop")
        || !str_cmp(cmd, "schedule")
        || !str_cmp(cmd, "tick")) {
        send_to_char("Use: events <subcommand> for admin/runtime controls.\n\r", ch);
        return;
    }

    if (!str_cmp(cmd, "info")
        || !str_cmp(cmd, "news")
        || !str_cmp(cmd, "join")
        || !str_cmp(cmd, "leave")) {
        char rewritten[MSL];

        if (( !str_cmp(cmd, "join") || !str_cmp(cmd, "leave") ) && IS_NULLSTR(argument)) {
            do_events(ch, original);
            return;
        }

        one_argument(argument, target);
        if (IS_NULLSTR(target)) {
            printf_to_char(ch, "Syntax: event %s <uid|name|idx>\n\r", cmd);
            return;
        }

        if (is_number(target))
            evt = event_player_lookup_idx(ch, atoi(target), &inst, &soon, &starts_in, &band);
        else
            evt = event_lookup_definition(target);

        if (!evt || !event_player_should_show(evt, ch, &inst, &soon, &starts_in, &band)) {
            send_to_char("No visible event found by that id, name, or index.\n\r", ch);
            return;
        }

        if (!str_cmp(cmd, "join") && (!inst || inst->state != EVTS_ACTIVE)) {
            send_to_char("That event is not active yet. Check 'event list' for start timing.\n\r", ch);
            return;
        }

        snprintf(rewritten, sizeof(rewritten), "%s %ld", cmd, evt->uid);
        do_events(ch, rewritten);
        return;
    }

    do_events(ch, original);
}

void do_epstat(CHAR_DATA *ch, char *argument)
{
    long event_uid = 0;
    uint32_t instance_id = 0;
    EVENT_INSTANCE *inst;
    EVENT_INDEX_DATA *evt;
    EVENT_PART *part;
    BUFFER *output;
    char buf[MSL];
    int shown_questers = 0;

    if (!epstat_parse_runtime_ref(argument, &event_uid, &instance_id)
        || event_uid <= 0 || instance_id <= 0) {
        send_to_char("Syntax: epstat <event_uid>#<instance_id>\n\r", ch);
        send_to_char("        epstat <event_uid> <instance_id>\n\r", ch);
        return;
    }

    evtedit_ensure_loaded();

    inst = event_find_runtime_by_ref(event_uid, instance_id);
    if (!inst || !inst->def) {
        send_to_char("No active event runtime found for that reference.\n\r", ch);
        return;
    }

    evt = inst->def;
    output = new_buf();

    sprintf(buf, "Event Runtime: {W%ld#%u{x\n\r", event_uid, instance_id);
    add_buf(output, buf);

    sprintf(buf, "Definition   : {W%s{x ({W%ld{x)\n\r", evt->name, evt->uid);
    add_buf(output, buf);

    sprintf(buf, "State        : {W%s{x  Type: {W%s{x  Scope: {W%s{x\n\r",
        epstat_event_state_name(inst->state),
        event_enum_name(evt_type_flags, evt->event_type),
        event_enum_name(evt_scope_flags, evt->scope_type));
    add_buf(output, buf);

    sprintf(buf, "Participants : {W%d{x\n\r", inst->participant_count);
    add_buf(output, buf);

    sprintf(buf, "Progress     : kills={W%d{x items={W%d{x goal={W%d{x\n\r",
        inst->progress_kills,
        inst->progress_items,
        (evt->event_type == EVT_TYPE_INVASION) ? inst->progress_goal : evt->completion_goal);
    add_buf(output, buf);

    sprintf(buf, "Phase        : {W%s{x%s\n\r",
        !IS_NULLSTR(inst->phase_name) ? inst->phase_name : (inst->leader_phase ? "leader" : "active"),
        inst->leader_phase ? " {Y(leader){x" : "");
    add_buf(output, buf);

    sprintf(buf, "Scope Anchor : area_uid={W%ld{x floating={W%s{x\n\r",
        inst->scope_area_uid,
        inst->scope_floating ? "yes" : "no");
    add_buf(output, buf);

    add_buf(output, "\n\rIndex Variables:\n\r");
    if (evt->index_vars)
        olc_show_index_vars(output, evt->index_vars);
    else
        add_buf(output, "  (none)\n\r");

    add_buf(output, "\n\rRuntime Variables:\n\r");
    if (inst->runtime_vars)
        pstat_variable_list(output, inst->runtime_vars);
    else
        add_buf(output, "  (none)\n\r");

    add_buf(output, "\n\rParticipant Quests:\n\r");
    for (part = inst->participants; part; part = part->next) {
        CHAR_DATA *participant = part->ch;
        QUEST_DATA *run;
        int active_runs = 0;

        if (!participant)
            continue;

        for (run = participant->quest; run; run = run->next) {
            if (run->run_status == QUEST_RUN_STATUS_ACTIVE)
                active_runs++;
        }

        if (active_runs <= 0)
            continue;

        shown_questers++;

        sprintf(buf, "  %s%s {W[team=%d kills=%d turnins=%d phases=%d event=%s]{x active_runs={W%d{x\n\r",
            IS_NPC(participant) ? "(npc) " : "",
            IS_NPC(participant) ? participant->short_descr : participant->name,
            part->team,
            part->kills,
            part->items_turned,
            part->phases_completed,
            part->event_completed ? "yes" : "no",
            active_runs);
        add_buf(output, buf);

        for (run = participant->quest; run; run = run->next) {
            QUEST_INDEX_V2_DATA *index_v2;

            if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            index_v2 = quest_runtime_get_index_v2(run);

            sprintf(buf, "    run={W%ld{x status={W%s{x focused={W%s{x stage={W%d{x index={W%s{x%s\n\r",
                run->run_id,
                epstat_quest_status_name(run->run_status),
                (!IS_NPC(participant) && participant->quest_runtime.focused_run_id == run->run_id) ? "yes" : "no",
                run->current_stage_id,
                index_v2 ? widevnum_string(index_v2->area, index_v2->vnum, NULL) : "(none)",
                (index_v2 && !IS_NULLSTR(index_v2->name)) ? formatf(" ({Y%s{x)", index_v2->name) : "");
            add_buf(output, buf);
        }
    }

    if (shown_questers == 0)
        add_buf(output, "  (no active quest runs found on participants)\n\r");

    if (!ch->lines && strlen(output->string) > MAX_STRING_LENGTH)
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    else
        page_to_char(output->string, ch);
}

void evtedit(CHAR_DATA *ch, char *argument)
{
    evtedit_ensure_loaded();
    olc_editor_interp(ch, argument, &evtedit_def);
}

EVTEDIT(evtedit_list)
{
    EVTEDIT_DATA *evt;
    bool found_any = false;

    (void)argument;

    for (AREA_DATA *area = area_first; area && !found_any; area = area->next)
        for (int hash = 0; hash < MAX_KEY_HASH && !found_any; hash++)
            if (area->event_index_hash[hash])
                found_any = true;

    if (!found_any) {
        send_to_char("No events defined.\n\r", ch);
        return false;
    }

    send_to_char("{WWNUM           Name                     Type           Schedule       Enabled{X\n\r", ch);
    send_to_char("{D-------------- ------------------------ -------------- -------------- -------{X\n\r", ch);
    for (AREA_DATA *area = area_first; area; area = area->next) {
        for (int hash = 0; hash < MAX_KEY_HASH; hash++) {
            for (evt = area->event_index_hash[hash]; evt; evt = evt->next_hash) {
                printf_to_char(ch, "{W%-14s {x%-24.24s %-14s %-14s %s{X\n\r",
                    widevnum_string_event(evt, NULL),
                    evt->name,
                    event_enum_name(evt_type_flags, evt->event_type),
                    event_enum_name(evt_sched_flags, evt->sched_type),
                    evt->enabled ? "{GYes{x" : "{RNo{x");
            }
        }
    }

    return false;
}

EVTEDIT(evtedit_create)
{
    EVTEDIT_DATA *evt;
    WNUM wnum;
    AREA_DATA *target_area = NULL;
    long target_vnum = 0;
    char arg1[MIL];
    char event_name[MSL];
    char *name_arg = NULL;

    one_argument(argument, arg1);

    if (!IS_NULLSTR(arg1) && evtedit_parse_index_ref(ch, arg1, &wnum)) {
        target_area = wnum.pArea;
        target_vnum = wnum.vnum;
        name_arg = one_argument(argument, arg1);
    } else {
        if (!ch->in_room || !ch->in_room->area) {
            send_to_char("You must be in an area, or specify <auid>#<vnum>.\n\r", ch);
            return false;
        }
        target_area = ch->in_room->area;
        target_vnum = evtedit_next_vnum_in_area(target_area);
        if (target_vnum < 1) {
            send_to_char("Unable to allocate an event vnum in this area.\n\r", ch);
            return false;
        }
        name_arg = argument;
    }

    if (!target_area || target_vnum < 1) {
        send_to_char("Invalid event reference. Use <auid>#<vnum> or in-area create.\n\r", ch);
        return false;
    }

    if (get_event_index_for_area(target_area, target_vnum)) {
        send_to_char("An event already exists at that widevnum.\n\r", ch);
        return false;
    }

    if (IS_NULLSTR(name_arg))
        snprintf(event_name, sizeof(event_name), "event_%ld", target_vnum);
    else {
        while (*name_arg && isspace((unsigned char)*name_arg))
            name_arg++;
        snprintf(event_name, sizeof(event_name), "%s", IS_NULLSTR(name_arg) ? "event" : name_arg);
    }

    if (evtedit_find_name(event_name)) {
        send_to_char("An event with that name already exists.\n\r", ch);
        return false;
    }

    evt = evtedit_new(target_area, target_vnum, event_name);
    SET_BIT(target_area->area_flags, AREA_CHANGED);

    if (!evtedit_save_area(target_area))
        send_to_char("Event created, but failed to save the area file.\n\r", ch);
    else
        printf_to_char(ch, "Event created at %ld#%ld (%s).\n\r",
            target_area->uid, target_vnum, event_name);

    olc_editor_enter(ch, &evtedit_def, evt, true);
    return true;
}

EVTEDIT(evtedit_delete)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    AREA_DATA *area;

    if (!evt)
        return false;

    area = evt->area;
    if (area)
        SET_BIT(area->area_flags, AREA_CHANGED);
    evtedit_global_unregister(evt);
    evtedit_free_item(evt);

    if (!evtedit_save_area(area))
        send_to_char("Event deleted, but failed to save the area file.\n\r", ch);
    else
        send_to_char("Event deleted.\n\r", ch);

    edit_done(ch);
    return true;
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
        formatf("%s", widevnum_string_event(evt, NULL)), &evtedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        evtedit_show_identity_tab(ch, ctx, evt);
        evtedit_show_schedule_tab(ch, ctx, evt);
        evtedit_show_stages_tab(ch, ctx, evt);
        evtedit_show_messages_tab(ch, ctx, evt);
        evtedit_show_meta_tab(ch, ctx, evt);
        evtedit_show_scripting_tab(ch, ctx, evt);
    } else {
        switch (tab) {
        case 1:
            evtedit_show_schedule_tab(ch, ctx, evt);
            break;
        case 2:
            evtedit_show_stages_tab(ch, ctx, evt);
            break;
        case 3:
            evtedit_show_messages_tab(ch, ctx, evt);
            break;
        case 4:
            evtedit_show_meta_tab(ch, ctx, evt);
            break;
        case 5:
            evtedit_show_scripting_tab(ch, ctx, evt);
            break;
        default:
            evtedit_show_identity_tab(ch, ctx, evt);
            break;
        }
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
    olc_display_number(ctx, theme, "Stages:", "stages list",
        evt->stage_count);
    olc_display_number(ctx, theme, "Reward Phase Script:", "rewardphase",
        evt->reward_phase_script);
    olc_display_number(ctx, theme, "Reward Success Script:", "rewardsuccess",
        evt->reward_success_script);
    olc_display_number(ctx, theme, "Reward Fail Script:", "rewardfail",
        evt->reward_failure_script);
}

static void evtedit_show_stages_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);
    EVT_STAGE_DEF *stage;
    int i;

    (void)ch;

    if (!evt)
        return;

    olc_display_number(ctx, theme, "Stage Count:", "stages list", evt->stage_count);
    olc_display_infof(ctx, theme,
        "Use 'stages' subcommands: list/add/set/remove/clear/objlist/objadd/objdel/objset");

    if (evt->stage_count <= 0 || !evt->stages) {
        olc_display_infof(ctx, theme, "No stages configured.");
        return;
    }

    i = 1;
    for (stage = evt->stages; stage; stage = stage->next, i++) {
        EVT_STAGE_OBJECTIVE_DEF *objective;
        int oidx;

        olc_display_infof(ctx, theme,
            "%2d) name={Y%s{x transition={G%s{x objmode={G%s{x dur={C%d{x",
            i,
            IS_NULLSTR(stage->name) ? "(unnamed)" : stage->name,
            event_stage_transition_name(stage->transition_mode),
            event_stage_objective_mode_name(stage->objective_mode),
            stage->duration_minutes);
        olc_display_infof(ctx, theme,
            "    hooks: enter={M%ld{x tick={M%ld{x complete={M%ld{x objectives={C%d{x",
            stage->on_enter_script,
            stage->on_tick_script,
            stage->on_complete_script,
            stage->objective_count);

        oidx = 1;
        for (objective = stage->objectives; objective; objective = objective->next, oidx++) {
            olc_display_infof(ctx, theme,
                "      %d) type={Y%s{x target={C%d{x name={M%s{x script={G%ld{x",
                oidx,
                event_stage_objective_type_name(objective->objective_type),
                objective->target_count,
                IS_NULLSTR(objective->name) ? "" : objective->name,
                objective->script_vnum);
        }
    }
}

EVTEDIT(evtedit_rewardphase)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char arg[MIL];

    if (!evt)
        return false;

    one_argument(argument, arg);
    if (IS_NULLSTR(arg) || !is_number(arg)) {
        send_to_char("Syntax: rewardphase <scriptvnum|0>\n\r", ch);
        return false;
    }

    evt->reward_phase_script = UMAX(0, atol(arg));
    return evtedit_save_after_change(ch);
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
    olc_display_infof(ctx, theme, "Use 'save' to write this event's area file.");
}

static void evtedit_show_scripting_tab(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&evtedit_def);

    (void)ch;

    olc_display_scripts(ctx, theme, evt ? evt->progs : NULL, PRG_EPROG,
        "EventProg Vnum", "addeprog", "deleprog");
    olc_display_vars(ctx, theme, evt ? evt->index_vars : NULL, "varset", "varclear");
}

static bool evtedit_save_after_change(CHAR_DATA *ch)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt || !evt->area) {
        send_to_char("Updated, but failed to save the area file.\n\r", ch);
        return true;
    }

    SET_BIT(evt->area->area_flags, AREA_CHANGED);

    if (!evtedit_save_area(evt->area)) {
        send_to_char("Updated, but failed to save the area file.\n\r", ch);
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
        send_to_char("        roster addnpc <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off] [stage=<n|any>]\n\r", ch);
        send_to_char("        roster addobj <vnum> [count] [chance] [minlevel] [maxlevel] [stage=<n|any>]\n\r", ch);
        send_to_char("        roster boss <index> <on|off>\n\r", ch);
        send_to_char("        roster stage <index> <n|any>\n\r", ch);
        send_to_char("        roster req <index> <requirements text|clear|show>\n\r", ch);
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

        send_to_char("#   Type     Vnum      Count Chance Level    Boss Stage Req\n\r", ch);
        send_to_char("--------------------------------------------------------------\n\r", ch);

        for (entry = evt->roster; entry; entry = entry->next, index++) {
            printf_to_char(ch, "%-3d %-8s %-9ld %-5d %-6d %-8s %-4s %-5s %-3s\n\r",
                index,
                event_enum_name(evt_roster_kind_flags, entry->kind),
                entry->vnum,
                entry->count,
                entry->chance,
                evtedit_roster_level_window(entry),
                entry->boss ? "yes" : "no",
                evtedit_roster_stage_name(entry),
                IS_NULLSTR(entry->requirements) ? "no" : "yes");
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

        free_string(entry->requirements);
        free_mem(entry, sizeof(*entry));
        send_to_char("Roster entry removed.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "req")) {
        char index_arg[MIL];
        char action_arg[MIL];
        EVT_ROSTER_ENTRY *entry;

        argument = one_argument(argument, index_arg);
        if (!is_number(index_arg)) {
            send_to_char("Syntax: roster req <index> <requirements text|clear|show>\n\r", ch);
            return false;
        }

        entry = evtedit_roster_find(evt->roster, atoi(index_arg));
        if (!entry) {
            send_to_char("No roster entry at that index.\n\r", ch);
            return false;
        }

        argument = one_argument(argument, action_arg);
        if (IS_NULLSTR(action_arg)) {
            send_to_char("Syntax: roster req <index> <requirements text|clear|show>\n\r", ch);
            return false;
        }

        if (!str_cmp(action_arg, "show")) {
            if (IS_NULLSTR(entry->requirements)) {
                send_to_char("Roster requirements: <none>\n\r", ch);
            } else {
                char *dsl = requirements_json_to_text(entry->requirements);
                if (dsl && dsl[0] != '\0') {
                    printf_to_char(ch, "Roster requirements: %s\n\r", dsl);
                } else {
                    printf_to_char(ch, "Roster requirements (json): %s\n\r", entry->requirements);
                }
                if (dsl)
                    free(dsl);
            }
            return false;
        }

        if (!str_cmp(action_arg, "clear")) {
            free_string(entry->requirements);
            entry->requirements = str_dup("");
            send_to_char("Roster requirements cleared.\n\r", ch);
            return evtedit_save_after_change(ch);
        }

        {
            char err[MSL];
            char req_input[MSL * 2];
            char *json_str;

            req_input[0] = '\0';
            strncat(req_input, action_arg, sizeof(req_input) - 1);
            if (!IS_NULLSTR(argument)) {
                strncat(req_input, " ", sizeof(req_input) - strlen(req_input) - 1);
                strncat(req_input, argument, sizeof(req_input) - strlen(req_input) - 1);
            }

            json_str = requirements_text_to_json(req_input, err, sizeof(err));
            if (!json_str) {
                printf_to_char(ch, "Invalid requirements: %s\n\r", err[0] ? err : "parse error");
                return false;
            }

            free_string(entry->requirements);
            entry->requirements = str_dup(json_str);
            free(json_str);

            send_to_char("Roster requirements updated.\n\r", ch);
            return evtedit_save_after_change(ch);
        }
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

    if (!str_prefix(cmd, "stage")) {
        char index_arg[MIL];
        char stage_arg[MIL];
        EVT_ROSTER_ENTRY *entry;

        argument = one_argument(argument, index_arg);
        argument = one_argument(argument, stage_arg);

        if (!is_number(index_arg) || IS_NULLSTR(stage_arg)) {
            send_to_char("Syntax: roster stage <index> <n|any>\n\r", ch);
            return false;
        }

        entry = evtedit_roster_find(evt->roster, atoi(index_arg));
        if (!entry) {
            send_to_char("No roster entry at that index.\n\r", ch);
            return false;
        }

        if (!str_cmp(stage_arg, "any") || !str_cmp(stage_arg, "*") || !str_cmp(stage_arg, "all")) {
            entry->stage = 0;
            return evtedit_save_after_change(ch);
        }

        if (!is_number(stage_arg) || atoi(stage_arg) <= 0) {
            send_to_char("Stage must be a positive number or 'any'.\n\r", ch);
            return false;
        }

        entry->stage = (int16_t)UMAX(1, atoi(stage_arg));
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
        int stage = 0;
        EVT_ROSTER_ENTRY *entry;

        argument = one_argument(argument, vnum_arg);
        if (!is_number(vnum_arg) || atol(vnum_arg) <= 0) {
            send_to_char("Syntax: roster addnpc <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off] [stage=<n|any>]\n\r", ch);
            send_to_char("        roster addobj <vnum> [count] [chance] [minlevel] [maxlevel] [stage=<n|any>]\n\r", ch);
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

            if (!str_prefix(token, "stage=")) {
                char *stage_value = token + 6;

                if (IS_NULLSTR(stage_value)) {
                    send_to_char("stage= expects a stage number or 'any'.\n\r", ch);
                    return false;
                }

                if (!str_cmp(stage_value, "any") || !str_cmp(stage_value, "*") || !str_cmp(stage_value, "all")) {
                    stage = 0;
                    continue;
                }

                if (!is_number(stage_value) || atoi(stage_value) <= 0) {
                    send_to_char("stage= expects a positive stage number or 'any'.\n\r", ch);
                    return false;
                }

                stage = UMAX(1, atoi(stage_value));
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
        entry->stage = (int16_t)stage;
        entry->requirements = str_dup("");
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

    send_to_char("Unknown roster subcommand. Use: list, addnpc, addobj, boss, stage, req, remove, clear.\n\r", ch);
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

static const char *event_stage_transition_name(int mode)
{
    switch (mode) {
    case EVT_STAGE_TRANSITION_ON_COMPLETE: return "complete";
    case EVT_STAGE_TRANSITION_ON_TIMER:    return "timer";
    case EVT_STAGE_TRANSITION_SCRIPT:      return "script";
    default:                               return "unknown";
    }
}

static int event_stage_transition_parse(const char *value)
{
    if (IS_NULLSTR(value))
        return -1;

    if (!str_cmp(value, "complete") || !str_cmp(value, "on_complete"))
        return EVT_STAGE_TRANSITION_ON_COMPLETE;
    if (!str_cmp(value, "timer") || !str_cmp(value, "on_timer"))
        return EVT_STAGE_TRANSITION_ON_TIMER;
    if (!str_cmp(value, "script") || !str_cmp(value, "manual"))
        return EVT_STAGE_TRANSITION_SCRIPT;

    return -1;
}

static const char *event_stage_objective_mode_name(int mode)
{
    switch (mode) {
    case EVT_STAGE_OBJECTIVE_ANY: return "any";
    case EVT_STAGE_OBJECTIVE_ALL:
    default:                      return "all";
    }
}

static int event_stage_objective_mode_parse(const char *value)
{
    if (IS_NULLSTR(value))
        return -1;

    if (!str_cmp(value, "all"))
        return EVT_STAGE_OBJECTIVE_ALL;
    if (!str_cmp(value, "any"))
        return EVT_STAGE_OBJECTIVE_ANY;

    return -1;
}

static const char *event_stage_objective_type_name(int type)
{
    switch (type) {
    case EVT_STAGE_OBJECTIVE_KILL:    return "kill";
    case EVT_STAGE_OBJECTIVE_COLLECT: return "collect";
    case EVT_STAGE_OBJECTIVE_SURVIVE: return "survive";
    case EVT_STAGE_OBJECTIVE_CUSTOM:  return "custom";
    default:                          return "unknown";
    }
}

static int event_stage_objective_type_parse(const char *value)
{
    if (IS_NULLSTR(value))
        return -1;

    if (!str_cmp(value, "kill"))
        return EVT_STAGE_OBJECTIVE_KILL;
    if (!str_cmp(value, "collect"))
        return EVT_STAGE_OBJECTIVE_COLLECT;
    if (!str_cmp(value, "survive"))
        return EVT_STAGE_OBJECTIVE_SURVIVE;
    if (!str_cmp(value, "custom"))
        return EVT_STAGE_OBJECTIVE_CUSTOM;

    return -1;
}

static EVT_STAGE_DEF *event_stage_by_one_index(EVTEDIT_DATA *evt, int one_based_index)
{
    EVT_STAGE_DEF *stage;
    int i = 1;

    if (!evt || one_based_index < 1)
        return NULL;

    for (stage = evt->stages; stage; stage = stage->next, i++)
        if (i == one_based_index)
            return stage;

    return NULL;
}

static EVT_STAGE_OBJECTIVE_DEF *event_stage_objective_by_one_index(EVT_STAGE_DEF *stage,
    int one_based_index)
{
    EVT_STAGE_OBJECTIVE_DEF *objective;
    int i = 1;

    if (!stage || one_based_index < 1)
        return NULL;

    for (objective = stage->objectives; objective; objective = objective->next, i++)
        if (i == one_based_index)
            return objective;

    return NULL;
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

    event_stage_sync_from_phases(evt);

    argument = one_argument(argument, cmd);

    if (IS_NULLSTR(cmd)) {
        send_to_char("Syntax: stages list\n\r", ch);
        send_to_char("        stages clear\n\r", ch);
        send_to_char("        stages add <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        stages insert <index> <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        stages set <index> <name> [minutes] [scriptvnum]\n\r", ch);
        send_to_char("        stages name <index> <name>\n\r", ch);
        send_to_char("        stages minutes <index> <minutes>\n\r", ch);
        send_to_char("        stages script <index> <scriptvnum|0>\n\r", ch);
        send_to_char("        stages remove <index>\n\r", ch);
        send_to_char("        stages transition <stage#> <complete|timer|script>\n\r", ch);
        send_to_char("        stages tickscript <stage#> <scriptvnum|0>\n\r", ch);
        send_to_char("        stages objmode <stage#> <all|any>\n\r", ch);
        send_to_char("        stages objlist <stage#>\n\r", ch);
        send_to_char("        stages objadd <stage#> <kill|collect|survive|custom> <target> [name]\n\r", ch);
        send_to_char("        stages objset <stage#> <obj#> <kill|collect|survive|custom> <target>\n\r", ch);
        send_to_char("        stages objname <stage#> <obj#> <name>\n\r", ch);
        send_to_char("        stages objremove <stage#> <obj#>\n\r", ch);
        send_to_char("        stages spawns <stage#>\n\r", ch);
        send_to_char("        stages mobadd <stage#> <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off]\n\r", ch);
        send_to_char("        stages objaddspawn <stage#> <vnum> [count] [chance] [minlevel] [maxlevel]\n\r", ch);
        return false;
    }

    if (!str_prefix(cmd, "list")) {
        int i;

        if (count <= 0) {
            send_to_char("Stage plan is empty.\n\r", ch);
            return false;
        }

        send_to_char("{WStage Plan Steps:{x\n\r", ch);
        for (i = 0; i < count; i++) {
            EVT_STAGE_DEF *stage = event_stage_by_one_index(evt, i + 1);
            EVT_STAGE_OBJECTIVE_DEF *objective;
            int oidx;

            printf_to_char(ch, "  {W%2d){x name={Y%s{x minutes={C%d{x script={M%ld{x\n\r",
                i + 1,
                steps[i].name,
                steps[i].minutes,
                steps[i].script_vnum);

            if (!stage)
                continue;

            printf_to_char(ch, "      transition={G%s{x objmode={G%s{x objectives={C%d{x\n\r",
                event_stage_transition_name(stage->transition_mode),
                event_stage_objective_mode_name(stage->objective_mode),
                stage->objective_count);
            printf_to_char(ch, "      hooks: enter={M%ld{x tick={M%ld{x complete={M%ld{x\n\r",
                stage->on_enter_script,
                stage->on_tick_script,
                stage->on_complete_script);

            oidx = 1;
            for (objective = stage->objectives; objective; objective = objective->next, oidx++) {
                printf_to_char(ch,
                    "        {W%d.{x type={Y%s{x target={C%d{x name={M%s{x script={G%ld{x data={D%s{x\n\r",
                    oidx,
                    event_stage_objective_type_name(objective->objective_type),
                    objective->target_count,
                    IS_NULLSTR(objective->name) ? "" : objective->name,
                    objective->script_vnum,
                    IS_NULLSTR(objective->data) ? "" : objective->data);
            }
        }

        return false;
    }

    if (!str_prefix(cmd, "clear")) {
        event_phase_steps_store(evt, steps, 0);
        send_to_char("Stage plan cleared.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "spawns")) {
        EVT_ROSTER_ENTRY *entry;
        int stage_index;
        int roster_index = 1;
        bool printed = false;

        argument = one_argument(argument, arg1);
        if (!is_number(arg1) || atoi(arg1) <= 0) {
            send_to_char("Syntax: stages spawns <stage#>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        send_to_char("#   Type     Vnum      Count Chance Level    Boss Stage Req\n\r", ch);
        send_to_char("--------------------------------------------------------------\n\r", ch);

        for (entry = evt->roster; entry; entry = entry->next, roster_index++) {
            if (entry->stage != 0 && entry->stage != stage_index)
                continue;

            printed = true;
            printf_to_char(ch, "%-3d %-8s %-9ld %-5d %-6d %-8s %-4s %-5s %-3s\n\r",
                roster_index,
                event_enum_name(evt_roster_kind_flags, entry->kind),
                entry->vnum,
                entry->count,
                entry->chance,
                evtedit_roster_level_window(entry),
                entry->boss ? "yes" : "no",
                evtedit_roster_stage_name(entry),
                IS_NULLSTR(entry->requirements) ? "no" : "yes");
        }

        if (!printed)
            send_to_char("No roster entries are assigned to that stage.\n\r", ch);

        return false;
    }

    if (!str_prefix(cmd, "mobadd") || !str_prefix(cmd, "objaddspawn")) {
        bool add_npc = !str_prefix(cmd, "mobadd");
        char roster_cmd[MSL];
        int stage_index;

        argument = one_argument(argument, arg1);
        if (!is_number(arg1) || atoi(arg1) <= 0 || IS_NULLSTR(argument)) {
            if (add_npc)
                send_to_char("Syntax: stages mobadd <stage#> <vnum> [count] [chance] [minlevel] [maxlevel] [boss|on|off]\n\r", ch);
            else
                send_to_char("Syntax: stages objaddspawn <stage#> <vnum> [count] [chance] [minlevel] [maxlevel]\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        snprintf(roster_cmd, sizeof(roster_cmd), "%s %s stage=%d",
            add_npc ? "addnpc" : "addobj",
            argument,
            stage_index);

        return evtedit_roster(ch, roster_cmd);
    }

    if (!str_prefix(cmd, "objlist")) {
        EVT_STAGE_DEF *stage;
        EVT_STAGE_OBJECTIVE_DEF *objective;
        int stage_index;
        int obj_index = 1;

        argument = one_argument(argument, arg1);
        if (!is_number(arg1)) {
            send_to_char("Syntax: stages objlist <stage#>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        printf_to_char(ch, "{WStage %d:{x %s transition={G%s{x objmode={G%s{x\n\r",
            stage_index,
            IS_NULLSTR(stage->name) ? "stage" : stage->name,
            event_stage_transition_name(stage->transition_mode),
            event_stage_objective_mode_name(stage->objective_mode));

        if (stage->objective_count <= 0 || !stage->objectives) {
            send_to_char("  No objectives.\n\r", ch);
            return false;
        }

        for (objective = stage->objectives; objective; objective = objective->next, obj_index++) {
            printf_to_char(ch,
                "  {W%d){x type={Y%s{x target={C%d{x name={M%s{x script={G%ld{x data={D%s{x\n\r",
                obj_index,
                event_stage_objective_type_name(objective->objective_type),
                objective->target_count,
                IS_NULLSTR(objective->name) ? "" : objective->name,
                objective->script_vnum,
                IS_NULLSTR(objective->data) ? "" : objective->data);
        }

        return false;
    }

    if (!str_prefix(cmd, "objadd")) {
        EVT_STAGE_DEF *stage;
        EVT_STAGE_OBJECTIVE_DEF *objective;
        int stage_index;
        int objective_type;
        int target;
        char name_buf[MIL];

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2) || IS_NULLSTR(arg3) || !is_number(arg3)) {
            send_to_char("Syntax: stages objadd <stage#> <kill|collect|survive|custom> <target> [name]\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        objective_type = event_stage_objective_type_parse(arg2);
        if (objective_type < 0) {
            send_to_char("Objective type must be one of: kill, collect, survive, custom.\n\r", ch);
            return false;
        }

        target = UMAX(0, atoi(arg3));
        one_argument(argument, name_buf);
        objective = event_stage_add_objective(stage,
            IS_NULLSTR(name_buf) ? arg2 : name_buf,
            objective_type,
            target);

        if (!objective) {
            send_to_char("Unable to add objective.\n\r", ch);
            return false;
        }

        send_to_char("Objective added.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "objset")) {
        EVT_STAGE_DEF *stage;
        EVT_STAGE_OBJECTIVE_DEF *objective;
        int stage_index;
        int objective_index;
        int objective_type;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, error);

        if (!is_number(arg1) || !is_number(arg2) || IS_NULLSTR(arg3)
            || IS_NULLSTR(error) || !is_number(error)) {
            send_to_char("Syntax: stages objset <stage#> <obj#> <kill|collect|survive|custom> <target>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        objective_index = atoi(arg2);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        objective = event_stage_objective_by_one_index(stage, objective_index);
        if (!objective) {
            printf_to_char(ch, "Objective index must be between 1 and %d.\n\r", UMAX(1, stage->objective_count));
            return false;
        }

        objective_type = event_stage_objective_type_parse(arg3);
        if (objective_type < 0) {
            send_to_char("Objective type must be one of: kill, collect, survive, custom.\n\r", ch);
            return false;
        }

        objective->objective_type = objective_type;
        objective->target_count = UMAX(0, atoi(error));
        send_to_char("Objective updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "objname")) {
        EVT_STAGE_DEF *stage;
        EVT_STAGE_OBJECTIVE_DEF *objective;
        int stage_index;
        int objective_index;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || !is_number(arg2) || IS_NULLSTR(arg3)) {
            send_to_char("Syntax: stages objname <stage#> <obj#> <name>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        objective_index = atoi(arg2);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        objective = event_stage_objective_by_one_index(stage, objective_index);
        if (!objective) {
            printf_to_char(ch, "Objective index must be between 1 and %d.\n\r", UMAX(1, stage->objective_count));
            return false;
        }

        free_string(objective->name);
        objective->name = str_dup(arg3);
        send_to_char("Objective name updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "objremove")) {
        EVT_STAGE_DEF *stage;
        EVT_STAGE_OBJECTIVE_DEF *objective;
        EVT_STAGE_OBJECTIVE_DEF *prev;
        int stage_index;
        int objective_index;
        int i;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);

        if (!is_number(arg1) || !is_number(arg2)) {
            send_to_char("Syntax: stages objremove <stage#> <obj#>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        objective_index = atoi(arg2);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        prev = NULL;
        objective = stage->objectives;
        for (i = 1; objective && i < objective_index; i++) {
            prev = objective;
            objective = objective->next;
        }

        if (!objective) {
            printf_to_char(ch, "Objective index must be between 1 and %d.\n\r", UMAX(1, stage->objective_count));
            return false;
        }

        if (prev)
            prev->next = objective->next;
        else
            stage->objectives = objective->next;

        free_string(objective->name);
        free_string(objective->data);
        free_mem(objective, sizeof(*objective));
        stage->objective_count = UMAX(0, stage->objective_count - 1);

        send_to_char("Objective removed.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "transition")) {
        EVT_STAGE_DEF *stage;
        int stage_index;
        int mode;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: stages transition <stage#> <complete|timer|script>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        mode = event_stage_transition_parse(arg2);
        if (mode < 0) {
            send_to_char("Transition mode must be one of: complete, timer, script.\n\r", ch);
            return false;
        }

        stage->transition_mode = mode;
        send_to_char("Stage transition mode updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "tickscript")) {
        EVT_STAGE_DEF *stage;
        int stage_index;
        long script_vnum;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);

        if (!is_number(arg1) || IS_NULLSTR(arg2) || !is_number(arg2) || atol(arg2) < 0) {
            send_to_char("Syntax: stages tickscript <stage#> <scriptvnum|0>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        script_vnum = UMAX(0, atol(arg2));
        stage->on_tick_script = script_vnum;
        send_to_char("Stage tick script updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "objmode")) {
        EVT_STAGE_DEF *stage;
        int stage_index;
        int mode;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: stages objmode <stage#> <all|any>\n\r", ch);
            return false;
        }

        stage_index = atoi(arg1);
        stage = event_stage_by_one_index(evt, stage_index);
        if (!stage) {
            printf_to_char(ch, "Stage index must be between 1 and %d.\n\r", UMAX(1, evt->stage_count));
            return false;
        }

        mode = event_stage_objective_mode_parse(arg2);
        if (mode < 0) {
            send_to_char("Objective mode must be one of: all, any.\n\r", ch);
            return false;
        }

        stage->objective_mode = mode;
        send_to_char("Stage objective mode updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "add")) {
        EVT_PHASE_STEP_DEF step;

        memset(&step, 0, sizeof(step));
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (IS_NULLSTR(arg1)) {
            send_to_char("Syntax: stages add <name> [minutes] [scriptvnum]\n\r", ch);
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

        strlcpy(step.name, arg1, sizeof(step.name));

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
        send_to_char("Stage step added.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "insert")) {
        EVT_PHASE_STEP_DEF step;

        memset(&step, 0, sizeof(step));
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: stages insert <index> <name> [minutes] [scriptvnum]\n\r", ch);
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

        strlcpy(step.name, arg2, sizeof(step.name));

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
        send_to_char("Stage step inserted.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "set")) {
        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2)) {
            send_to_char("Syntax: stages set <index> <name> [minutes] [scriptvnum]\n\r", ch);
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

        strlcpy(steps[index - 1].name, arg2, sizeof(steps[index - 1].name));

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
        send_to_char("Stage step updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (!is_number(arg1)) {
        send_to_char("Stages command requires an index for this operation.\n\r", ch);
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
        send_to_char("Stage step removed.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "name")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: stages name <index> <name>\n\r", ch);
            return false;
        }

        if (!event_validate_phase_name(arg2, error, sizeof(error))) {
            send_to_char(error, ch);
            send_to_char("\n\r", ch);
            return false;
        }

        strlcpy(steps[index - 1].name, arg2, sizeof(steps[index - 1].name));
        event_phase_steps_store(evt, steps, count);
        send_to_char("Stage name updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "minutes")) {
        if (IS_NULLSTR(arg2) || !is_number(arg2) || atoi(arg2) < 0) {
            send_to_char("Syntax: stages minutes <index> <minutes>=0\n\r", ch);
            return false;
        }

        steps[index - 1].minutes = atoi(arg2);
        event_phase_steps_store(evt, steps, count);
        send_to_char("Stage minutes updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    if (!str_prefix(cmd, "script")) {
        if (IS_NULLSTR(arg2) || !is_number(arg2) || atol(arg2) < 0) {
            send_to_char("Syntax: stages script <index> <scriptvnum|0>\n\r", ch);
            return false;
        }

        steps[index - 1].script_vnum = atol(arg2);
        event_phase_steps_store(evt, steps, count);
        send_to_char("Stage script updated.\n\r", ch);
        return evtedit_save_after_change(ch);
    }

    send_to_char("Unknown stages subcommand. Use: list, clear, add, insert, set, name, minutes, script, remove, transition, tickscript, objmode, objlist, objadd, objset, objname, objremove, spawns, mobadd, objaddspawn.\n\r", ch);
    return false;
}

EVTEDIT(evtedit_stages)
{
    return evtedit_phaseplan(ch, argument);
}

EVTEDIT(evtedit_phases)
{
    send_to_char("'phases' is deprecated; use 'stages'.\n\r", ch);
    return evtedit_phaseplan(ch, argument);
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

EVTEDIT(evtedit_addeprog)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    int tindex, slot;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];
    WNUM script_wnum;
    AREA_DATA *context;

    if (!evt)
        return false;

    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (IS_NULLSTR(num) || IS_NULLSTR(trigger) || IS_NULLSTR(phrase)) {
        send_to_char("Syntax: addeprog [widevnum] [trigger] [phrase]\n\r", ch);
        return false;
    }

    if ((tindex = trigger_index(trigger, PRG_EPROG)) < 0) {
        send_to_char("Valid flags are:\n\r", ch);
        show_help(ch, "eprog");
        return false;
    }

    slot = trigger_table[tindex].slot;
    context = olc_relative_widevnum_context(evt->area, num);
    if (!parse_widevnum(num, context, &script_wnum)) {
        send_to_char("Invalid widevnum format. Use: vnum, #vnum or area#vnum\n\r", ch);
        return false;
    }

    code = get_script_index(script_wnum.pArea, script_wnum.vnum, PRG_EPROG);
    if (!code) {
        send_to_char("No such EVENTProgram.\n\r", ch);
        return false;
    }

    if (!evt->progs)
        evt->progs = new_prog_bank();

    if (edit_trigger_exists(evt->progs, code, tindex, phrase)) {
        send_to_char("That trigger/phrase pair is already attached to that script on this event.\n\r", ch);
        return false;
    }

    list = new_trigger();
    list->vnum = script_wnum.vnum;
    list->script_is_widevnum = (script_wnum.pArea != NULL);
    if (list->script_is_widevnum) {
        list->script_load.auid = script_wnum.pArea->uid;
        list->script_load.vnum = script_wnum.vnum;
    }
    list->trig_type = tindex;
    list->trig_phrase = str_dup(phrase);
    if (is_widevnum_format(phrase)) {
        list->numeric = true;
        list->trig_is_widevnum = true;
        parse_widevnum_load(phrase, &list->trig_load);
        list->trig_number = (int)list->trig_load.vnum;
    } else {
        list->trig_number = atoi(list->trig_phrase);
        list->numeric = is_number(list->trig_phrase);
    }

    list->script = code;
    list_appendlink(evt->progs[slot], list);

    send_to_char("Eprog Added.\n\r", ch);
    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_deleprog)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int group_idx, trig_idx;
    PROG_GROUP groups[MAX_PROG_GROUPS];
    int num_groups;

    if (!evt)
        return false;

    if (!evt->progs) {
        send_to_char("This event has no programs attached.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: deleprog <group#>\n\r", ch);
        send_to_char("        deleprog <group#> <trigger#>\n\r", ch);
        return false;
    }

    if (!is_number(arg1)) {
        send_to_char("Please specify a valid group number.\n\r", ch);
        return false;
    }

    group_idx = atoi(arg1);
    num_groups = prog_build_groups(evt->progs, groups, MAX_PROG_GROUPS, PRG_EPROG);

    if (group_idx < 1 || group_idx > num_groups) {
        send_to_char("Invalid group number.\n\r", ch);
        return false;
    }

    PROG_GROUP *group = &groups[group_idx - 1];

    if (IS_NULLSTR(arg2)) {
        if (edit_delscript(evt->progs, group->script)) {
            send_to_char("Script group removed.\n\r", ch);
            return evtedit_save_after_change(ch);
        }
    } else {
        PROG_GROUP_ENTRY *entry;

        if (!is_number(arg2)) {
            send_to_char("Please specify a valid trigger number within the group.\n\r", ch);
            return false;
        }

        trig_idx = atoi(arg2);
        if (trig_idx < 1 || trig_idx > group->trigger_count) {
            send_to_char("Invalid trigger number within that group.\n\r", ch);
            return false;
        }

        entry = &group->triggers[trig_idx - 1];
        if (edit_deltrigger_specific(evt->progs, group->script,
            entry->entry->trig_type, entry->entry->trig_phrase)) {
            send_to_char("Trigger removed from script group.\n\r", ch);
            return evtedit_save_after_change(ch);
        }
    }

    send_to_char("No such program or trigger found.\n\r", ch);
    return false;
}

EVTEDIT(evtedit_varset)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_varset(&evt->index_vars, ch, argument, false))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_varclear)
{
    EVTEDIT_DATA *evt = (EVTEDIT_DATA *)ch->desc->pEdit;

    if (!evt)
        return false;

    if (!olc_varclear(&evt->index_vars, ch, argument, false))
        return false;

    return evtedit_save_after_change(ch);
}

EVTEDIT(evtedit_save)
{
    EVTEDIT_DATA *evt = ch && ch->desc ? (EVTEDIT_DATA *)ch->desc->pEdit : NULL;
    AREA_DATA *target_area = NULL;

    evtedit_ensure_loaded();

    if (evt && evt->area)
        target_area = evt->area;
    else if (ch && ch->in_room)
        target_area = ch->in_room->area;

    if (!target_area || !evtedit_save_area(target_area)) {
        send_to_char("Failed to save event area data.\n\r", ch);
        return false;
    }

    printf_to_char(ch, "Event area data saved (%ld:%s).\n\r",
        target_area->uid,
        target_area->name ? target_area->name : "(unnamed)");
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

    evtedit_rebuild_global_index();

    if (uid > 0)
        reloaded = evtedit_find_uid(uid);

    if (uid > 0 && !reloaded) {
        send_to_char("Events reloaded, but current event no longer exists.\n\r", ch);
        edit_done(ch);
        return false;
    }

    if (reloaded && ch->desc)
        ch->desc->pEdit = reloaded;

    send_to_char("Event registry refreshed from loaded area data.\n\r", ch);
    return false;
}