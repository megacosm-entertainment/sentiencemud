/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*       ROM 2.4 is copyright 1993-1995 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@pacinfo.com)                              *
*           Gabrielle Taylor (gtaylor@pacinfo.com)                         *
*           Brian Moore (rom@rom.efn.org)                                  *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
*  Automated Quest code written by Vassago of MOONGATE, moongate.ams.com   *
*  4000. Copyright (c) 1996 Ryan Addams, All Rights Reserved. Use of this  *
*  code is allowed provided you add a credit line to the effect of:        *
*  "Quest Code (c) 1996 Ryan Addams" to your logon screen with the rest    *
*  of the standard diku/rom credits. If you use this or a modified version *
*  of this code, let me know via email: moongate@moongate.ams.com. Further *
*  updates will be posted to the rom mailing list. If you'd like to get    *
*  the latest version of quest.c, please send a request to the above add-  *
*  ress. Quest Code v2.00.                                                 *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "magic.h"
#include "tables.h"
#include "scripts.h"
#include "requirements.h"

static bool check_quest_custom_task_run(CHAR_DATA *ch, QUEST_DATA *run, int task, bool show);
static int quest_runtime_apply_objective_event(QUEST_DATA *run, int objective_type, WNUM target_wnum, int delta);
static unsigned long long quest_runtime_mix_seed(unsigned long long value);
static void quest_runtime_ensure_generation_seed(QUEST_DATA *run);
static bool quest_runtime_compile_generated_stage(QUEST_DATA *run, QUEST_STAGE_INDEX_V2_DATA *stage, unsigned long long stage_seed);
static bool quest_runtime_commence_current_stage(QUEST_DATA *run);
static QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *quest_runtime_pick_objective_pool_entry(QUEST_OBJECTIVE_INDEX_V2_DATA *objective, unsigned long long seed);
static ROOM_INDEX_DATA *quest_runtime_pick_room_in_area(AREA_DATA *area, unsigned long long seed);
static int quest_runtime_count_live_mobs_by_index(WNUM target_wnum, AREA_DATA *scope_area);
static bool quest_runtime_spawn_missing_kill_target(QUEST_DATA *run, QUEST_OBJECTIVE_STATE_V2_DATA *state);
static CHAR_DATA *quest_runtime_get_owner_character(QUEST_DATA *run);
static void quest_runtime_apply_spawn_owner_lock(QUEST_DATA *run, CHAR_DATA *mob);
static CHAR_DATA *quest_runtime_find_mob_target_instance(QUEST_DATA *run, WNUM target_wnum, AREA_DATA *scope_area);
static CHAR_DATA *quest_runtime_find_unbound_mob_target_instance(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective,
    WNUM target_wnum, AREA_DATA *scope_area);
static OBJ_DATA *quest_runtime_find_object_target_instance(WNUM target_wnum, AREA_DATA *scope_area);
static bool quest_runtime_attach_objective_target_token(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_STATE_V2_DATA *state);
static bool quest_runtime_attach_objective_destination_token(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_STATE_V2_DATA *state);
static void quest_runtime_clear_target_bindings(QUEST_DATA *run);
static WNUM quest_runtime_get_target_binding(QUEST_DATA *run, const char *name);
static bool quest_runtime_set_target_binding(QUEST_DATA *run, const char *name, WNUM target_wnum);
static bool quest_runtime_scope_owner_online(QUEST_DATA *run);
static void quest_runtime_propagate_scoped_objective_event(CHAR_DATA *actor, QUEST_DATA *source_run, int objective_type, WNUM target_wnum, int delta);
static int quest_runtime_stage_rank(QUEST_INDEX_V2_DATA *quest_index_v2, int stage_id);
static bool quest_runtime_merge_stage_progress(QUEST_DATA *target_run, QUEST_DATA *source_run);
static bool quest_runtime_sync_run_from_reference(QUEST_DATA *target_run, QUEST_DATA *reference_run, bool allow_stage_advance);
static int quest_runtime_sync_group_cluster(CHAR_DATA *actor, QUEST_DATA *anchor_run, bool allow_stage_advance);
static int quest_runtime_objective_required_count(QUEST_OBJECTIVE_INDEX_V2_DATA *objective);
static void quest_runtime_unbind_strict_targets(QUEST_DATA *run);
static bool quest_runtime_is_mob_strictly_bound(CHAR_DATA *mob, QUEST_DATA *exclude_run, int exclude_objective_id);
static bool quest_runtime_objective_target_matches(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, WNUM target_wnum);
static void quest_runtime_resolve_script_context(QUEST_DATA *run, CHAR_DATA **mob, OBJ_DATA **obj, ROOM_INDEX_DATA **room, AREA_DATA **area);
static bool quest_runtime_fire_qprog_trigger(QUEST_DATA *run, int trig_type, const char *phrase, CHAR_DATA *enactor);
static void quest_runtime_fire_stage_lifecycle_trigger(QUEST_DATA *run, int trig_type, int stage_id);
static void quest_runtime_fire_objective_lifecycle_trigger(QUEST_DATA *run, int trig_type, int objective_id);
static void quest_runtime_fire_quest_lifecycle_trigger(QUEST_DATA *run, int trig_type, const char *phrase);
static void quest_runtime_fire_quest_lifecycle_trigger_actor(QUEST_DATA *run, int trig_type, const char *phrase, CHAR_DATA *enactor);
static void quest_runtime_mark_run_failed(QUEST_DATA *run, int failed_status, const char *reason_phrase);
static void quest_runtime_record_terminal_history(QUEST_DATA *run);
static void quest_runtime_fire_stage_script(QUEST_DATA *run, const char *script_name);
static void quest_runtime_reset_expiration(CHAR_DATA *ch);
static WNUM quest_runtime_resolve_objective_target_reference(QUEST_DATA *run, QUEST_INDEX_V2_DATA *quest_index_v2,
    QUEST_STAGE_INDEX_V2_DATA *stage, QUEST_OBJECTIVE_INDEX_V2_DATA *objective);
static const char *quest_run_display_name(QUEST_DATA *run);
static void quest_runtime_apply_rewards(QUEST_DATA *run, CHAR_DATA *enactor);
static void quest_runtime_emit_objective_message(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, bool failed);
static void quest_runtime_emit_stage_message(QUEST_DATA *run, QUEST_STAGE_INDEX_V2_DATA *stage, bool failed);

#define QUEST_LIST_MAX_ENTRIES 128

/* Collect enabled v2 quests from a mob's quests_v2 list */
static int quest_collect_mob_v2_offerings(MOB_INDEX_DATA *mob_index, QUEST_INDEX_V2_DATA **results, int max_results)
{
    QUEST_V2_LIST *entry;
    AREA_DATA *fallback;
    int count = 0;

    if (!mob_index || !results || max_results < 1)
        return 0;

    fallback = mob_index->area ? mob_index->area : get_system_area_fallback();

    for (entry = mob_index->quests_v2; entry != NULL && count < max_results; entry = entry->next) {
        if (!entry->wnum.pArea && entry->load.vnum > 0)
            resolve_wnum_load(&entry->load, &entry->wnum, fallback);
        if (!entry->wnum.pArea)
            continue;
        QUEST_INDEX_V2_DATA *qi = get_quest_index_v2_wnum(entry->wnum);
        if (qi && qi->enabled)
            results[count++] = qi;
    }

    return count;
}

/* Collect enabled v2 quests from an object's quests_v2 list */
static int quest_collect_obj_v2_offerings(OBJ_INDEX_DATA *obj_index, QUEST_INDEX_V2_DATA **results, int max_results)
{
    QUEST_V2_LIST *entry;
    AREA_DATA *fallback;
    int count = 0;

    if (!obj_index || !results || max_results < 1)
        return 0;

    fallback = obj_index->area ? obj_index->area : get_system_area_fallback();

    for (entry = obj_index->quests_v2; entry != NULL && count < max_results; entry = entry->next) {
        if (!entry->wnum.pArea && entry->load.vnum > 0)
            resolve_wnum_load(&entry->load, &entry->wnum, fallback);
        if (!entry->wnum.pArea)
            continue;
        QUEST_INDEX_V2_DATA *qi = get_quest_index_v2_wnum(entry->wnum);
        if (qi && qi->enabled)
            results[count++] = qi;
    }

    return count;
}

/* Find a v2 quest offering by list index (1-based) or name infix */
static QUEST_INDEX_V2_DATA *quest_find_v2_offering_by_name(QUEST_INDEX_V2_DATA **offerings, int count, const char *name)
{
    int i;

    if (!offerings || count < 1 || IS_NULLSTR(name))
        return NULL;

    if (is_number((char *)name)) {
        i = atoi(name);
        if (i >= 1 && i <= count)
            return offerings[i - 1];
    }

    for (i = 0; i < count; i++) {
        if (offerings[i] && !IS_NULLSTR(offerings[i]->name)
        &&  !str_infix((char *)name, offerings[i]->name))
            return offerings[i];
    }

    return NULL;
}

static void quest_part_resolve(WNUM_LOAD *load, WNUM *wnum)
{
    if (!load || !wnum || wnum->pArea || load->vnum < 1) {
        return;
    }

    AREA_DATA *fallback = NULL;
    WNUM res;
    if (resolve_widevnum(load->vnum, NULL, &res))
        fallback = res.pArea;
    if (!fallback) fallback = get_system_area_fallback();
    resolve_wnum_load(load, wnum, fallback);
}

static void quest_runtime_normalize_template_link(CHAR_DATA *ch, QUEST_DATA *run)
{
    WNUM quest_index_wnum = wnum_zero;
    AREA_DATA *quest_index_area = NULL;

    if (!ch || !run)
        return;

    if (run->quest_index_vnum <= 0)
        return;

    if (run->quest_index_auid > 0)
        quest_index_area = get_area_from_uid(run->quest_index_auid);

    if (quest_index_area) {
        quest_index_wnum.pArea = quest_index_area;
        quest_index_wnum.vnum = run->quest_index_vnum;
    } else if (!resolve_widevnum(run->quest_index_vnum, NULL, &quest_index_wnum)) {
        quest_index_wnum = wnum_zero;
    }

    if (!get_quest_index_wnum(quest_index_wnum)) {
        if (run->quest_index_vnum > 0 || run->quest_index_auid > 0) {
            plogf(LOG_QUEST,
                  "Clearing stale quest template link for %s run %ld (auid=%ld vnum=%ld)",
                  ch->name ? ch->name : "(unknown)",
                  run->run_id,
                  run->quest_index_auid,
                  run->quest_index_vnum);
        }
        run->quest_index_auid = 0;
        run->quest_index_vnum = 0;
    } else if (quest_index_wnum.pArea) {
        run->quest_index_auid = quest_index_wnum.pArea->uid;
        run->quest_index_vnum = quest_index_wnum.vnum;
    }
}

static void quest_runtime_seed_run_vars_from_index(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *index_v2;

    if (!run || run->vars)
        return;

    index_v2 = quest_runtime_get_index_v2(run);
    if (!index_v2 || !index_v2->index_vars)
        return;

    variable_copylist(&index_v2->index_vars, &run->vars, false);
    variables_resolve_rsg_bindings(&run->vars);
}

static const char *quest_target_scope_name(int scope)
{
    switch (scope)
    {
    case QUEST_TARGET_SCOPE_CHARACTER: return "character";
    case QUEST_TARGET_SCOPE_GROUP: return "group";
    case QUEST_TARGET_SCOPE_CHURCH: return "church";
    default: return "unknown";
    }
}

static const char *quest_class_name(int quest_class)
{
    switch (quest_class)
    {
    case QUEST_CLASS_NARRATIVE: return "narrative";
    case QUEST_CLASS_MISSION: return "mission";
    default: return "unknown";
    }
}

static const char *quest_type_name(int quest_type)
{
    switch (quest_type)
    {
    case QUEST_TYPE_MAIN_STORY: return "main";
    case QUEST_TYPE_SIDE_QUEST: return "side";
    case QUEST_TYPE_UNLOCK: return "unlock";
    case QUEST_TYPE_CLASS_QUEST: return "class";
    case QUEST_TYPE_EVENT: return "event";
    case QUEST_TYPE_OTHER: return "other";
    default: return "unknown";
    }
}

static const char *quest_category_name(int category)
{
    switch (category)
    {
    case QUEST_LOG_CATEGORY_NONE: return "none";
    case QUEST_LOG_CATEGORY_REGIONAL: return "regional";
    case QUEST_LOG_CATEGORY_CLASS: return "class";
    case QUEST_LOG_CATEGORY_STORY: return "story";
    case QUEST_LOG_CATEGORY_CHURCH: return "church";
    case QUEST_LOG_CATEGORY_DUNGEON: return "dungeon";
    case QUEST_LOG_CATEGORY_CRAFTING: return "crafting";
    case QUEST_LOG_CATEGORY_EVENT: return "event";
    case QUEST_LOG_CATEGORY_OTHER: return "other";
    default: return "unknown";
    }
}

static const char *quest_run_status_name(int run_status)
{
    switch (run_status)
    {
    case QUEST_RUN_STATUS_ACTIVE: return "active";
    case QUEST_RUN_STATUS_COMPLETED: return "completed";
    case QUEST_RUN_STATUS_FAILED: return "failed";
    case QUEST_RUN_STATUS_ABANDONED: return "abandoned";
    default: return "unknown";
    }
}

static bool quest_parse_history_status(const char *name, int *status)
{
    if (IS_NULLSTR(name) || !status)
        return false;

    if (!str_prefix(name, "all")) {
        *status = -1;
        return true;
    }

    if (!str_prefix(name, "completed") || !str_prefix(name, "complete")) {
        *status = QUEST_RUN_STATUS_COMPLETED;
        return true;
    }

    if (!str_prefix(name, "failed") || !str_prefix(name, "fail")) {
        *status = QUEST_RUN_STATUS_FAILED;
        return true;
    }

    if (!str_prefix(name, "abandoned") || !str_prefix(name, "abandon") || !str_prefix(name, "cancelled")) {
        *status = QUEST_RUN_STATUS_ABANDONED;
        return true;
    }

    return false;
}

static bool quest_parse_history_category(const char *name, int *category)
{
    if (IS_NULLSTR(name) || !category)
        return false;

    if (is_number((char *)name)) {
        int value = atoi(name);
        if (value >= QUEST_LOG_CATEGORY_NONE && value <= QUEST_LOG_CATEGORY_OTHER) {
            *category = value;
            return true;
        }
        return false;
    }

    if (!str_prefix(name, "none")) { *category = QUEST_LOG_CATEGORY_NONE; return true; }
    if (!str_prefix(name, "regional")) { *category = QUEST_LOG_CATEGORY_REGIONAL; return true; }
    if (!str_prefix(name, "class")) { *category = QUEST_LOG_CATEGORY_CLASS; return true; }
    if (!str_prefix(name, "story")) { *category = QUEST_LOG_CATEGORY_STORY; return true; }
    if (!str_prefix(name, "church")) { *category = QUEST_LOG_CATEGORY_CHURCH; return true; }
    if (!str_prefix(name, "dungeon")) { *category = QUEST_LOG_CATEGORY_DUNGEON; return true; }
    if (!str_prefix(name, "crafting")) { *category = QUEST_LOG_CATEGORY_CRAFTING; return true; }
    if (!str_prefix(name, "event")) { *category = QUEST_LOG_CATEGORY_EVENT; return true; }
    if (!str_prefix(name, "other")) { *category = QUEST_LOG_CATEGORY_OTHER; return true; }

    return false;
}

static bool quest_parse_type(const char *name, int *type)
{
    if (IS_NULLSTR(name) || !type)
        return false;

    if (is_number((char *)name)) {
        int value = atoi(name);
        if (value >= QUEST_TYPE_MAIN_STORY && value <= QUEST_TYPE_OTHER) {
            *type = value;
            return true;
        }
        return false;
    }

    if (!str_prefix(name, "main")) { *type = QUEST_TYPE_MAIN_STORY; return true; }
    if (!str_prefix(name, "side")) { *type = QUEST_TYPE_SIDE_QUEST; return true; }
    if (!str_prefix(name, "unlock")) { *type = QUEST_TYPE_UNLOCK; return true; }
    if (!str_prefix(name, "class")) { *type = QUEST_TYPE_CLASS_QUEST; return true; }
    if (!str_prefix(name, "event")) { *type = QUEST_TYPE_EVENT; return true; }
    if (!str_prefix(name, "other")) { *type = QUEST_TYPE_OTHER; return true; }

    return false;
}

static time_t quest_history_terminal_time(QUEST_HISTORY_DATA *history)
{
    if (!history)
        return 0;

    if (history->completed_at > 0)
        return history->completed_at;
    if (history->failed_at > 0)
        return history->failed_at;
    if (history->abandoned_at > 0)
        return history->abandoned_at;
    return history->started_at;
}

static bool quest_history_matches_filters(QUEST_HISTORY_DATA *history, int status_filter, int category_filter)
{
    if (!history)
        return false;

    if (status_filter >= 0 && history->run_status != status_filter)
        return false;

    if (category_filter >= 0 && history->category != category_filter)
        return false;

    return true;
}

static QUEST_HISTORY_DATA *quest_history_find_by_name(CHAR_DATA *ch, const char *name)
{
    QUEST_HISTORY_DATA *history;
    QUEST_HISTORY_DATA *partial = NULL;

    if (!ch || IS_NPC(ch) || !ch->pcdata || IS_NULLSTR(name))
        return NULL;

    for (history = ch->pcdata->quest_history; history != NULL; history = history->next)
    {
        if (IS_NULLSTR(history->name))
            continue;

        if (!str_cmp(name, history->name))
            return history;

        if (!str_infix(name, history->name))
        {
            if (partial)
                return NULL;
            partial = history;
        }
    }

    return partial;
}

static void quest_history_trim_missions(CHAR_DATA *ch)
{
    QUEST_HISTORY_DATA *history;
    QUEST_HISTORY_DATA *prev = NULL;
    QUEST_HISTORY_DATA *next;
    int kept_missions = 0;
    int mission_limit;

    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return;

    mission_limit = game_settings.mission_history_limit;
    if (mission_limit < 1)
        return;

    for (history = ch->pcdata->quest_history; history != NULL; history = next)
    {
        next = history->next;

        if (history->quest_class != QUEST_CLASS_MISSION)
        {
            prev = history;
            continue;
        }

        kept_missions++;
        if (kept_missions <= mission_limit)
        {
            prev = history;
            continue;
        }

        if (prev)
            prev->next = next;
        else
            ch->pcdata->quest_history = next;

        free_quest_history(history);
    }
}

static void quest_runtime_record_terminal_history(QUEST_DATA *run)
{
    CHAR_DATA *owner;
    QUEST_INDEX_V2_DATA *index_v2;
    QUEST_HISTORY_DATA *history;
    QUEST_HISTORY_DATA *scan;
    const char *run_name;

    if (!run || run->run_status == QUEST_RUN_STATUS_ACTIVE)
        return;

    owner = quest_runtime_get_owner_character(run);
    if (!owner || IS_NPC(owner) || !owner->pcdata)
        return;

    for (scan = owner->pcdata->quest_history; scan != NULL; scan = scan->next)
    {
        if (scan->run_id != run->run_id)
            continue;

        if (scan->started_at == run->started_at
            && scan->quest_index_v2_auid == run->quest_index_v2_auid
            && scan->quest_index_v2_vnum == run->quest_index_v2_vnum)
            return;
    }

    index_v2 = quest_runtime_get_index_v2(run);
    run_name = quest_run_display_name(run);

    history = new_quest_history();
    history->run_id = run->run_id;
    history->quest_index_v2_auid = run->quest_index_v2_auid;
    history->quest_index_v2_vnum = run->quest_index_v2_vnum;
    history->run_status = run->run_status;
    history->target_scope = run->target_scope;
    history->started_at = run->started_at;
    history->completed_at = run->completed_at;
    history->failed_at = run->failed_at;
    history->abandoned_at = run->abandoned_at;

    if (index_v2)
    {
        history->quest_class = index_v2->quest_class;
        history->quest_type = index_v2->quest_type;
        history->category = index_v2->category;
    }

    free_string(history->name);
    history->name = str_dup(IS_NULLSTR(run_name) ? "(unknown quest)" : run_name);

    history->next = owner->pcdata->quest_history;
    owner->pcdata->quest_history = history;

    if (run->run_status == QUEST_RUN_STATUS_COMPLETED)
    {
        owner->pcdata->quests_completed++;
        leaderboard_update_score(REPORT_TOP_QUESTS, owner->name, (double)owner->pcdata->quests_completed);

        if (history->quest_class == QUEST_CLASS_MISSION)
            owner->pcdata->missions_completed++;
    }

    quest_history_trim_missions(owner);
}

static void quest_show_history_entry(CHAR_DATA *ch, QUEST_HISTORY_DATA *history)
{
    time_t terminal_time;
    long age_minutes;

    if (!ch || !history)
        return;

    terminal_time = quest_history_terminal_time(history);
    age_minutes = terminal_time > 0 ? UMAX(0, (long)((current_time - terminal_time) / 60)) : 0;

    printf_to_char(ch, "History: {Y%s{x\n\r",
        IS_NULLSTR(history->name) ? "(unknown quest)" : history->name);
    printf_to_char(ch, "Run: %ld  Status: %s  Scope: %s\n\r",
        history->run_id,
        quest_run_status_name(history->run_status),
        quest_target_scope_name(history->target_scope));
    printf_to_char(ch, "Class: %s  Type: %s  Category: %s\n\r",
        quest_class_name(history->quest_class),
        quest_type_name(history->quest_type),
        quest_category_name(history->category));

    if (history->started_at > 0)
        printf_to_char(ch, "Started: %s", ctime(&history->started_at));
    if (history->completed_at > 0)
        printf_to_char(ch, "Completed: %s", ctime(&history->completed_at));
    if (history->failed_at > 0)
        printf_to_char(ch, "Failed: %s", ctime(&history->failed_at));
    if (history->abandoned_at > 0)
        printf_to_char(ch, "Abandoned: %s", ctime(&history->abandoned_at));

    if (terminal_time > 0)
        printf_to_char(ch, "Terminal event: %ld minute%s ago\n\r",
            age_minutes,
            age_minutes == 1 ? "" : "s");
}

static bool quest_parse_index_v2_ref(CHAR_DATA *ch, const char *input, WNUM *wnum)
{
    char ref[MIL];
    char *sep;
    AREA_DATA *area;

    if (!ch || !wnum || IS_NULLSTR(input))
        return false;

    strncpy(ref, input, sizeof(ref) - 1);
    ref[sizeof(ref) - 1] = '\0';

    sep = strchr(ref, '#');
    if (!sep)
        sep = strchr(ref, ':');

    if (sep) {
        *sep++ = '\0';
        if (!is_number(ref) || !is_number(sep))
            return false;

        area = get_area_index(atol(ref));
        if (!area)
            return false;

        wnum->pArea = area;
        wnum->vnum = atol(sep);
        return wnum->vnum > 0;
    }

    if (!is_number(ref))
        return false;

    if (!ch->in_room || !ch->in_room->area)
        return false;

    wnum->pArea = ch->in_room->area;
    wnum->vnum = atol(ref);
    return wnum->vnum > 0;
}

static const char *quest_objective_type_name(int objective_type)
{
    switch (objective_type)
    {
    case QUEST_OBJECTIVE_KILL: return "kill";
    case QUEST_OBJECTIVE_COLLECT: return "collect";
    case QUEST_OBJECTIVE_TALK: return "talk";
    case QUEST_OBJECTIVE_TRAVEL: return "travel";
    case QUEST_OBJECTIVE_LOCATE: return "locate";
    case QUEST_OBJECTIVE_RESCUE: return "rescue";
    case QUEST_OBJECTIVE_ESCORT: return "escort";
    case QUEST_OBJECTIVE_CUSTOM_SCRIPT: return "custom";
    default: return "unknown";
    }
}

static const char *quest_objective_visible_label(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    if (!objective)
        return "(objective)";

    if (!IS_NULLSTR(objective->description))
        return objective->description;

    if (!IS_NULLSTR(objective->target_tag))
        return objective->target_tag;

    return quest_objective_type_name(objective->objective_type);
}

static void quest_runtime_emit_objective_message(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, bool failed)
{
    CHAR_DATA *owner;
    const char *custom;
    bool silent_default;
    const char *label;

    if (!run || !objective)
        return;

    owner = quest_runtime_get_owner_character(run);
    if (!owner || IS_NPC(owner))
        return;

    custom = failed ? objective->fail_message : objective->complete_message;
    silent_default = failed ? objective->silent_fail : objective->silent_complete;
    if (!IS_NULLSTR(custom))
    {
        printf_to_char(owner, "%s\n\r", custom);
        return;
    }

    if (silent_default)
        return;

    label = quest_objective_visible_label(objective);
    if (failed)
        printf_to_char(owner, "Objective failed: {R%s{x\n\r", label);
    else
        printf_to_char(owner, "Objective complete: {G%s{x\n\r", label);
}

static void quest_runtime_emit_stage_message(QUEST_DATA *run, QUEST_STAGE_INDEX_V2_DATA *stage, bool failed)
{
    CHAR_DATA *owner;
    const char *custom;
    bool silent_default;
    const char *name;

    if (!run || !stage)
        return;

    owner = quest_runtime_get_owner_character(run);
    if (!owner || IS_NPC(owner))
        return;

    custom = failed ? stage->fail_message : stage->complete_message;
    silent_default = failed ? stage->silent_fail : stage->silent_complete;
    if (!IS_NULLSTR(custom))
    {
        printf_to_char(owner, "%s\n\r", custom);
        return;
    }

    if (silent_default)
        return;

    name = IS_NULLSTR(stage->name) ? "(unnamed stage)" : stage->name;
    if (failed)
        printf_to_char(owner, "Stage failed: {R%s{x\n\r", name);
    else
        printf_to_char(owner, "Stage complete: {G%s{x\n\r", name);
}

static int quest_objective_required_display_count(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    int required;

    if (!objective)
        return 1;

    required = objective->required_count;
    if (required < 1)
        required = objective->quantity;
    if (required < 1)
        required = 1;

    return required;
}

static const char *quest_objective_target_summary(QUEST_DATA *run,
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective,
    char *buf,
    size_t buf_size,
    bool include_widevnum)
{
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    WNUM target;
    MOB_INDEX_DATA *mob_index;
    OBJ_INDEX_DATA *obj_index;
    ROOM_INDEX_DATA *room;
    char pool_suffix[8];

#define QUEST_TARGET_LABEL_MOB(m) \
    (IS_NULLSTR((m)->list_name) ? \
        (IS_NULLSTR((m)->short_descr) ? \
            (IS_NULLSTR((m)->player_name) ? "(unnamed)" : (m)->player_name) : \
            (m)->short_descr) : \
        (m)->list_name)
#define QUEST_TARGET_LABEL_OBJ(o) \
    (IS_NULLSTR((o)->list_name) ? \
        (IS_NULLSTR((o)->short_descr) ? \
            (IS_NULLSTR((o)->name) ? "(unnamed)" : (o)->name) : \
            (o)->short_descr) : \
        (o)->list_name)
#define QUEST_TARGET_LABEL_ROOM(r) \
    (IS_NULLSTR((r)->name) ? "(unnamed room)" : (r)->name)

    if (!buf || buf_size < 2)
        return "target: (invalid)";

    if (!objective)
    {
        snprintf(buf, buf_size, "target: (none)");
        return buf;
    }

    state = run ? quest_runtime_get_objective_state(run, objective->id, false) : NULL;
    target = (state && state->selected_target_wnum.pArea && state->selected_target_wnum.vnum > 0)
        ? state->selected_target_wnum
        : objective->target_wnum;

    pool_suffix[0] = '\0';

    if (!target.pArea || target.vnum < 1)
    {
        snprintf(buf, buf_size, "target: (unspecified)%s", pool_suffix);
        return buf;
    }

    if (objective->objective_type == QUEST_OBJECTIVE_COLLECT)
    {
        obj_index = get_obj_index(target.pArea, target.vnum);
        if (obj_index)
        {
            if (include_widevnum)
                snprintf(buf, buf_size, "target: %ld#%ld (obj: %s)%s",
                    target.pArea->uid,
                    target.vnum,
                    QUEST_TARGET_LABEL_OBJ(obj_index),
                    pool_suffix);
            else
                snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_OBJ(obj_index), pool_suffix);
            return buf;
        }
    }

    if (objective->objective_type == QUEST_OBJECTIVE_TRAVEL)
    {
        room = get_room_index(target.pArea, target.vnum);
        if (room)
        {
            if (include_widevnum)
                snprintf(buf, buf_size, "target: %ld#%ld (room: %s)%s",
                    target.pArea->uid,
                    target.vnum,
                    QUEST_TARGET_LABEL_ROOM(room),
                    pool_suffix);
            else
                snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_ROOM(room), pool_suffix);
            return buf;
        }
    }

    if (objective->objective_type == QUEST_OBJECTIVE_KILL
    ||  objective->objective_type == QUEST_OBJECTIVE_TALK
    ||  objective->objective_type == QUEST_OBJECTIVE_RESCUE
    ||  objective->objective_type == QUEST_OBJECTIVE_ESCORT)
    {
        mob_index = get_mob_index(target.pArea, target.vnum);
        if (mob_index)
        {
            if (include_widevnum)
                snprintf(buf, buf_size, "target: %ld#%ld (mob: %s)%s",
                    target.pArea->uid,
                    target.vnum,
                    QUEST_TARGET_LABEL_MOB(mob_index),
                    pool_suffix);
            else
                snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_MOB(mob_index), pool_suffix);
            return buf;
        }
    }

    obj_index = get_obj_index(target.pArea, target.vnum);
    if (obj_index)
    {
        if (include_widevnum)
            snprintf(buf, buf_size, "target: %ld#%ld (obj: %s)%s",
                target.pArea->uid,
                target.vnum,
                QUEST_TARGET_LABEL_OBJ(obj_index),
                pool_suffix);
        else
            snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_OBJ(obj_index), pool_suffix);
        return buf;
    }

    mob_index = get_mob_index(target.pArea, target.vnum);
    if (mob_index)
    {
        if (include_widevnum)
            snprintf(buf, buf_size, "target: %ld#%ld (mob: %s)%s",
                target.pArea->uid,
                target.vnum,
                QUEST_TARGET_LABEL_MOB(mob_index),
                pool_suffix);
        else
            snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_MOB(mob_index), pool_suffix);
        return buf;
    }

    room = get_room_index(target.pArea, target.vnum);
    if (room)
    {
        if (include_widevnum)
            snprintf(buf, buf_size, "target: %ld#%ld (room: %s)%s",
                target.pArea->uid,
                target.vnum,
                QUEST_TARGET_LABEL_ROOM(room),
                pool_suffix);
        else
            snprintf(buf, buf_size, "target: %s%s", QUEST_TARGET_LABEL_ROOM(room), pool_suffix);
        return buf;
    }

    if (include_widevnum)
        snprintf(buf, buf_size, "target: %ld#%ld%s", target.pArea->uid, target.vnum, pool_suffix);
    else
        snprintf(buf, buf_size, "target: (unknown target)%s", pool_suffix);

#undef QUEST_TARGET_LABEL_MOB
#undef QUEST_TARGET_LABEL_OBJ
#undef QUEST_TARGET_LABEL_ROOM

    return buf;
}


static void quest_objective_target_options_display(CHAR_DATA *ch,
    QUEST_DATA *run,
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective,
    bool include_widevnum)
{
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
    int pool_count = 0;

    if (!ch || !objective || objective->target_mode != QUEST_OBJECTIVE_TARGET_POOL)
        return;

    for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next)
        pool_count++;

    if (pool_count < 2)
        return;

    state = run ? quest_runtime_get_objective_state(run, objective->id, false) : NULL;

    send_to_char("       options:\n\r", ch);

    for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next)
    {
        WNUM pool_target = pool_entry->target_wnum;
        MOB_INDEX_DATA *mob_index = NULL;
        OBJ_INDEX_DATA *obj_index = NULL;
        ROOM_INDEX_DATA *room = NULL;
        const char *entry_label = "(unspecified)";
        bool selected = false;

        if (state)
        {
            if (state->selected_pool_entry_id > 0)
                selected = (state->selected_pool_entry_id == pool_entry->id);
            else if (state->selected_target_wnum.pArea && state->selected_target_wnum.vnum > 0
                && state->selected_target_wnum.pArea == pool_target.pArea
                && state->selected_target_wnum.vnum == pool_target.vnum)
            {
                selected = true;
            }
        }

        if (pool_target.pArea && pool_target.vnum > 0)
        {
            if (objective->objective_type == QUEST_OBJECTIVE_COLLECT)
            {
                obj_index = get_obj_index(pool_target.pArea, pool_target.vnum);
                if (obj_index)
                    entry_label = IS_NULLSTR(obj_index->list_name)
                        ? (IS_NULLSTR(obj_index->short_descr)
                            ? (IS_NULLSTR(obj_index->name) ? "(unnamed)" : obj_index->name)
                            : obj_index->short_descr)
                        : obj_index->list_name;
            }
            else if (objective->objective_type == QUEST_OBJECTIVE_TRAVEL)
            {
                room = get_room_index(pool_target.pArea, pool_target.vnum);
                if (room)
                    entry_label = IS_NULLSTR(room->name) ? "(unnamed room)" : room->name;
            }
            else
            {
                mob_index = get_mob_index(pool_target.pArea, pool_target.vnum);
                if (mob_index)
                    entry_label = IS_NULLSTR(mob_index->list_name)
                        ? (IS_NULLSTR(mob_index->short_descr)
                            ? (IS_NULLSTR(mob_index->player_name) ? "(unnamed)" : mob_index->player_name)
                            : mob_index->short_descr)
                        : mob_index->list_name;
            }

            if (!str_cmp(entry_label, "(unspecified)"))
            {
                obj_index = get_obj_index(pool_target.pArea, pool_target.vnum);
                if (obj_index)
                    entry_label = IS_NULLSTR(obj_index->list_name)
                        ? (IS_NULLSTR(obj_index->short_descr)
                            ? (IS_NULLSTR(obj_index->name) ? "(unnamed)" : obj_index->name)
                            : obj_index->short_descr)
                        : obj_index->list_name;
            }
            if (!str_cmp(entry_label, "(unspecified)"))
            {
                mob_index = get_mob_index(pool_target.pArea, pool_target.vnum);
                if (mob_index)
                    entry_label = IS_NULLSTR(mob_index->list_name)
                        ? (IS_NULLSTR(mob_index->short_descr)
                            ? (IS_NULLSTR(mob_index->player_name) ? "(unnamed)" : mob_index->player_name)
                            : mob_index->short_descr)
                        : mob_index->list_name;
            }
            if (!str_cmp(entry_label, "(unspecified)"))
            {
                room = get_room_index(pool_target.pArea, pool_target.vnum);
                if (room)
                    entry_label = IS_NULLSTR(room->name) ? "(unnamed room)" : room->name;
            }
        }

        if (include_widevnum && pool_target.pArea && pool_target.vnum > 0)
            printf_to_char(ch, "         - %s%ld#%ld (%s)%s\n\r",
                selected ? "*" : "",
                pool_target.pArea->uid,
                pool_target.vnum,
                entry_label,
                selected ? "*" : "");
        else
            printf_to_char(ch, "         - %s%s%s\n\r",
                selected ? "*" : "",
                entry_label,
                selected ? "*" : "");
    }
}

static const char *quest_runtime_commence_blocker(QUEST_DATA *run)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;

    if (!run)
        return "quest runtime data is missing";

    if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return "quest is not active";

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
        return "current stage is missing";

    if (run->current_stage_commenced != 0)
        return "stage is already commenced";

    if (stage->stage_source == QUEST_STAGE_SOURCE_GENERATED && run->current_stage_generation == 0)
        return "generated stage data is not ready yet";

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        state = quest_runtime_get_objective_state(run, objective->id, false);
        if (!state)
            return "objective runtime state is missing";

        if (objective->target_mode == QUEST_OBJECTIVE_TARGET_POOL)
        {
            if (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1)
                return "a pooled objective has no selected target";
        }

        if (objective->target_token_wnum.pArea && objective->target_token_wnum.vnum > 0)
        {
            if (!get_token_index(objective->target_token_wnum.pArea, objective->target_token_wnum.vnum))
                return "target token reference is invalid";
        }

        if (objective->destination_token_wnum.pArea && objective->destination_token_wnum.vnum > 0)
        {
            if (!get_token_index(objective->destination_token_wnum.pArea, objective->destination_token_wnum.vnum))
                return "destination token reference is invalid";
        }
    }

    return "required targets or token attachments could not be resolved";
}

static bool quest_target_scope_supported_for_player(CHAR_DATA *ch, int scope, bool show_message)
{
    if (scope == QUEST_TARGET_SCOPE_CHARACTER)
        return true;

    if (scope == QUEST_TARGET_SCOPE_GROUP)
    {
        if (ch && IS_VALID(ch->group) && ch->group->id[0] != 0)
            return true;

        if (show_message && ch)
            send_to_char("You must be in a player group to use group-scoped quests.\n\r", ch);
        return false;
    }

    if (scope == QUEST_TARGET_SCOPE_CHURCH)
    {
        if (ch && ch->church && ch->church->uid > 0
            && ch->church_member
            && has_church_permission(ch->church_member, CHURCH_PERM_ACCEPT_QUESTS))
            return true;

        if (show_message && ch)
            send_to_char("You must belong to a church and have church quest permission to use church-scoped quests.\n\r", ch);
        return false;
    }

    if (show_message && ch) {
        send_to_char("Foundation currently supports character-scoped quests only.\n\r", ch);
    }

    return false;
}

static void quest_scope_owner_seed(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run)
        return;

    switch (run->target_scope)
    {
    case QUEST_TARGET_SCOPE_CHARACTER:
        if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0) {
            run->scope_owner_id[0] = ch->id[0];
            run->scope_owner_id[1] = ch->id[1];
        }
        run->scope_owner_uid = 0;
        break;

    case QUEST_TARGET_SCOPE_GROUP:
        if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0 && IS_VALID(ch->group)) {
            run->scope_owner_id[0] = ch->group->id[0];
            run->scope_owner_id[1] = ch->group->id[1];
        }
        run->scope_owner_uid = 0;
        break;

    case QUEST_TARGET_SCOPE_CHURCH:
        run->scope_owner_id[0] = 0;
        run->scope_owner_id[1] = 0;
        if (run->scope_owner_uid <= 0 && ch->church)
            run->scope_owner_uid = ch->church->uid;
        break;

    default:
        run->target_scope = QUEST_TARGET_SCOPE_CHARACTER;
        run->scope_owner_id[0] = ch->id[0];
        run->scope_owner_id[1] = ch->id[1];
        run->scope_owner_uid = 0;
        break;
    }
}

static bool quest_scope_owner_matches_player(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run || IS_NPC(ch))
        return false;

    switch (run->target_scope)
    {
    case QUEST_TARGET_SCOPE_CHARACTER:
        return uid_match(run->scope_owner_id, ch->id);

    case QUEST_TARGET_SCOPE_GROUP:
        return IS_VALID(ch->group) && uid_match(run->scope_owner_id, ch->group->id);

    case QUEST_TARGET_SCOPE_CHURCH:
        return (ch->church && run->scope_owner_uid > 0 && ch->church->uid == run->scope_owner_uid);

    default:
        return false;
    }
}

static bool quest_runtime_scope_owner_online(QUEST_DATA *run)
{
    CHAR_DATA *ch;
    ITERATOR it;

    if (!run)
        return false;

    iterator_start(&it, loaded_chars);
    while ((ch = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(ch))
            continue;

        switch (run->target_scope)
        {
        case QUEST_TARGET_SCOPE_CHARACTER:
            if (uid_match(run->scope_owner_id, ch->id))
            {
                iterator_stop(&it);
                return true;
            }
            break;

        case QUEST_TARGET_SCOPE_GROUP:
            if (IS_VALID(ch->group) && uid_match(run->scope_owner_id, ch->group->id))
            {
                iterator_stop(&it);
                return true;
            }
            break;

        case QUEST_TARGET_SCOPE_CHURCH:
            if (run->scope_owner_uid > 0 && ch->church && ch->church->uid == run->scope_owner_uid)
            {
                iterator_stop(&it);
                return true;
            }
            break;

        default:
            break;
        }
    }
    iterator_stop(&it);

    return false;
}

static void quest_runtime_propagate_scoped_objective_event(CHAR_DATA *actor, QUEST_DATA *source_run, int objective_type, WNUM target_wnum, int delta)
{
    CHAR_DATA *member;
    QUEST_DATA *peer_run;
    ITERATOR it;

    if (!source_run || delta == 0)
        return;

    if (source_run->target_scope == QUEST_TARGET_SCOPE_CHARACTER)
        return;

    iterator_start(&it, loaded_chars);
    while ((member = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(member) || member == actor || !member->quest)
            continue;

        if (!quest_scope_owner_matches_player(member, source_run))
            continue;

        for (peer_run = member->quest; peer_run != NULL; peer_run = peer_run->next)
        {
            if (peer_run == source_run)
                continue;

            if (peer_run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            if (peer_run->target_scope != source_run->target_scope)
                continue;

            if (peer_run->quest_index_v2_auid != source_run->quest_index_v2_auid
                || peer_run->quest_index_v2_vnum != source_run->quest_index_v2_vnum)
                continue;

            quest_runtime_apply_objective_event(peer_run, objective_type, target_wnum, delta);
        }
    }
    iterator_stop(&it);

    quest_runtime_sync_group_cluster(actor, source_run, true);
}

static int quest_runtime_stage_rank(QUEST_INDEX_V2_DATA *quest_index_v2, int stage_id)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    int rank = 0;

    if (!quest_index_v2 || stage_id < 1)
        return -1;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
    {
        rank++;
        if (stage->id == stage_id)
            return rank;
    }

    return -1;
}

static bool quest_runtime_merge_stage_progress(QUEST_DATA *target_run, QUEST_DATA *source_run)
{
    QUEST_STAGE_INDEX_V2_DATA *target_stage;
    QUEST_STAGE_INDEX_V2_DATA *source_stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    bool changed = false;

    if (!target_run || !source_run || target_run == source_run)
        return false;

    if (target_run->run_status != QUEST_RUN_STATUS_ACTIVE
        || source_run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    if (target_run->current_stage_id < 1
        || source_run->current_stage_id < 1
        || target_run->current_stage_id != source_run->current_stage_id)
        return false;

    target_stage = quest_runtime_get_current_stage(target_run);
    source_stage = quest_runtime_get_current_stage(source_run);

    if (!target_stage || !source_stage || target_stage->id != source_stage->id)
        return false;

    if ((target_stage->stage_source == QUEST_STAGE_SOURCE_GENERATED
         || source_stage->stage_source == QUEST_STAGE_SOURCE_GENERATED)
        && target_run->current_stage_seed != source_run->current_stage_seed)
    {
        return false;
    }

    if (source_run->current_stage_commenced && !target_run->current_stage_commenced)
    {
        if (quest_runtime_commence_current_stage(target_run))
            changed = true;
    }

    for (objective = target_stage->objectives; objective != NULL; objective = objective->next)
    {
        QUEST_OBJECTIVE_STATE_V2_DATA *source_state;
        QUEST_OBJECTIVE_STATE_V2_DATA *target_state;
        int required;
        int old_progress;
        bool old_complete;
        int merged_progress;

        source_state = quest_runtime_get_objective_state(source_run, objective->id, false);
        if (!source_state)
            continue;

        target_state = quest_runtime_get_objective_state(target_run, objective->id, true);
        if (!target_state)
            continue;

        required = quest_runtime_objective_required_count(objective);
        old_progress = target_state->progress;
        old_complete = target_state->complete;

        merged_progress = UMAX(target_state->progress, source_state->progress);
        merged_progress = URANGE(0, merged_progress, required);

        target_state->progress = merged_progress;

        if (source_state->complete || merged_progress >= required)
            target_state->complete = true;

        if (!old_complete && target_state->complete)
        {
            quest_runtime_emit_objective_message(target_run, objective, false);
            quest_runtime_fire_objective_lifecycle_trigger(target_run, TRIG_OBJECTIVE_COMPLETED, objective->id);
        }

        if (target_state->progress != old_progress || target_state->complete != old_complete)
            changed = true;
    }

    if (quest_runtime_try_advance_stage(target_run))
        changed = true;

    return changed;
}

static bool quest_runtime_sync_run_from_reference(QUEST_DATA *target_run, QUEST_DATA *reference_run, bool allow_stage_advance)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int reference_rank;
    int target_rank;
    bool changed = false;

    if (!target_run || !reference_run || target_run == reference_run)
        return false;

    if (target_run->run_status != QUEST_RUN_STATUS_ACTIVE
        || reference_run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    if (target_run->quest_index_v2_auid != reference_run->quest_index_v2_auid
        || target_run->quest_index_v2_vnum != reference_run->quest_index_v2_vnum)
        return false;

    if (target_run->target_scope != QUEST_TARGET_SCOPE_GROUP
        || reference_run->target_scope != QUEST_TARGET_SCOPE_GROUP)
        return false;

    if (!uid_match(target_run->scope_owner_id, reference_run->scope_owner_id))
        return false;

    quest_index_v2 = quest_runtime_get_index_v2(reference_run);
    if (!quest_index_v2)
        return false;

    reference_rank = quest_runtime_stage_rank(quest_index_v2, reference_run->current_stage_id);
    target_rank = quest_runtime_stage_rank(quest_index_v2, target_run->current_stage_id);

    if (allow_stage_advance
        && reference_rank >= 0
        && target_rank >= 0
        && target_rank < reference_rank)
    {
        QUEST_STAGE_INDEX_V2_DATA *reference_stage = quest_runtime_get_current_stage(reference_run);

        if (reference_stage && reference_stage->stage_source != QUEST_STAGE_SOURCE_GENERATED)
        {
            if (quest_runtime_set_stage(target_run, reference_run->current_stage_id))
            {
                changed = true;

                if (reference_run->current_stage_commenced && !target_run->current_stage_commenced)
                {
                    if (quest_runtime_commence_current_stage(target_run))
                        changed = true;
                }
            }
        }
    }

    if (quest_runtime_merge_stage_progress(target_run, reference_run))
        changed = true;

    return changed;
}

static int quest_runtime_sync_group_cluster(CHAR_DATA *actor, QUEST_DATA *anchor_run, bool allow_stage_advance)
{
    QUEST_DATA *best_run = NULL;
    QUEST_INDEX_V2_DATA *quest_index_v2;
    CHAR_DATA *member;
    QUEST_DATA *run;
    ITERATOR it;
    int best_rank = -1;
    int changed_runs = 0;

    if (!anchor_run || anchor_run->target_scope != QUEST_TARGET_SCOPE_GROUP)
        return 0;

    if (anchor_run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return 0;

    quest_index_v2 = quest_runtime_get_index_v2(anchor_run);
    if (!quest_index_v2)
        return 0;

    iterator_start(&it, loaded_chars);
    while ((member = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(member) || !member->quest)
            continue;

        for (run = member->quest; run != NULL; run = run->next)
        {
            int rank;

            if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            if (run->target_scope != QUEST_TARGET_SCOPE_GROUP)
                continue;

            if (!uid_match(run->scope_owner_id, anchor_run->scope_owner_id))
                continue;

            if (run->quest_index_v2_auid != anchor_run->quest_index_v2_auid
                || run->quest_index_v2_vnum != anchor_run->quest_index_v2_vnum)
                continue;

            rank = quest_runtime_stage_rank(quest_index_v2, run->current_stage_id);
            if (!best_run || rank > best_rank)
            {
                best_run = run;
                best_rank = rank;
            }
        }
    }
    iterator_stop(&it);

    if (!best_run)
        return 0;

    iterator_start(&it, loaded_chars);
    while ((member = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(member) || !member->quest)
            continue;

        for (run = member->quest; run != NULL; run = run->next)
        {
            bool changed = false;

            if (run == best_run)
                continue;

            if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            if (run->target_scope != QUEST_TARGET_SCOPE_GROUP)
                continue;

            if (!uid_match(run->scope_owner_id, anchor_run->scope_owner_id))
                continue;

            if (run->quest_index_v2_auid != anchor_run->quest_index_v2_auid
                || run->quest_index_v2_vnum != anchor_run->quest_index_v2_vnum)
                continue;

            changed = quest_runtime_sync_run_from_reference(run, best_run, allow_stage_advance);
            if (changed)
                changed_runs++;
        }
    }
    iterator_stop(&it);

    if (actor && !IS_NPC(actor))
    {
        QUEST_DATA *actor_run;

        for (actor_run = actor->quest; actor_run != NULL; actor_run = actor_run->next)
        {
            if (actor_run == best_run)
                continue;

            if (actor_run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            if (actor_run->target_scope != QUEST_TARGET_SCOPE_GROUP)
                continue;

            if (!uid_match(actor_run->scope_owner_id, anchor_run->scope_owner_id))
                continue;

            if (actor_run->quest_index_v2_auid != anchor_run->quest_index_v2_auid
                || actor_run->quest_index_v2_vnum != anchor_run->quest_index_v2_vnum)
                continue;

            if (quest_runtime_sync_run_from_reference(best_run, actor_run, false))
            {
                changed_runs++;
                break;
            }
        }
    }

    return changed_runs;
}

static void quest_runtime_unbind_strict_targets(QUEST_DATA *run)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;

    if (!run)
        return;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
        return;

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        if (!objective->strict_target)
            continue;

        state = quest_runtime_get_objective_state(run, objective->id, false);
        if (!state)
            continue;

        state->selected_target_uid[0] = 0;
        state->selected_target_uid[1] = 0;
    }
}

static bool quest_runtime_is_mob_strictly_bound(CHAR_DATA *mob, QUEST_DATA *exclude_run, int exclude_objective_id)
{
    CHAR_DATA *owner;
    QUEST_DATA *run;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    ITERATOR it;

    if (!mob || !IS_NPC(mob) || (!mob->id[0] && !mob->id[1]))
        return false;

    iterator_start(&it, loaded_chars);
    while ((owner = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(owner))
            continue;

        for (run = owner->quest; run != NULL; run = run->next)
        {
            if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
                continue;

            stage = quest_runtime_get_current_stage(run);
            if (!stage)
                continue;

            for (objective = stage->objectives; objective != NULL; objective = objective->next)
            {
                if (!objective->strict_target)
                    continue;

                if (run == exclude_run && objective->id == exclude_objective_id)
                    continue;

                state = quest_runtime_get_objective_state(run, objective->id, false);
                if (!state)
                    continue;

                if (state->selected_target_uid[0] == mob->id[0]
                    && state->selected_target_uid[1] == mob->id[1])
                {
                    iterator_stop(&it);
                    return true;
                }
            }
        }
    }
    iterator_stop(&it);

    return false;
}


static void quest_runtime_resolve_script_context(QUEST_DATA *run, CHAR_DATA **mob, OBJ_DATA **obj, ROOM_INDEX_DATA **room, AREA_DATA **area)
{
    CHAR_DATA *owner;

    if (mob)
        *mob = NULL;
    if (obj)
        *obj = NULL;
    if (room)
        *room = NULL;
    if (area)
        *area = NULL;

    if (!run)
        return;

    owner = quest_runtime_get_owner_character(run);
    if (owner && owner->in_room)
    {
        if (room)
            *room = owner->in_room;
        if (area)
            *area = owner->in_room->area;

        switch (run->questgiver_type)
        {
        case QUESTOR_MOB:
            if (mob)
            {
                CHAR_DATA *vch;
                for (vch = owner->in_room->people; vch != NULL; vch = vch->next_in_room)
                {
                    if (IS_NPC(vch) && wnum_match_mob(run->questgiver_wnum, vch))
                    {
                        *mob = vch;
                        break;
                    }
                }
            }
            break;

        case QUESTOR_OBJ:
            if (obj)
            {
                OBJ_DATA *vobj;
                for (vobj = owner->in_room->contents; vobj != NULL; vobj = vobj->next_content)
                {
                    if (wnum_match_obj(run->questgiver_wnum, vobj))
                    {
                        *obj = vobj;
                        break;
                    }
                }
            }
            break;

        case QUESTOR_ROOM:
            if (room && wnum_match_room(run->questgiver_wnum, owner->in_room))
                *room = owner->in_room;
            break;
        }
    }

    if (area && !*area)
    {
        if (run->quest_index_v2_auid > 0)
            *area = get_area_index(run->quest_index_v2_auid);
    }
}


static void quest_runtime_fire_stage_script(QUEST_DATA *run, const char *script_name)
{
    CHAR_DATA *owner;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;
    AREA_DATA *area;

    if (!run || IS_NULLSTR(script_name))
        return;

    quest_runtime_fire_qprog_trigger(run, TRIG_QUEST_PART, script_name, NULL);

    owner = quest_runtime_get_owner_character(run);
    mob = NULL;
    obj = NULL;
    room = NULL;
    area = NULL;

    quest_runtime_resolve_script_context(run, &mob, &obj, &room, &area);

    if (mob || obj || room)
        p_percent_trigger(mob, obj, room, NULL, owner, NULL, NULL, NULL, NULL, TRIG_QUEST_PART, (char *)script_name);

    if (area)
        p_percent2_trigger(area, NULL, NULL, owner, NULL, NULL, NULL, NULL, TRIG_RECKONING, (char *)script_name);
}

static bool quest_runtime_fire_qprog_trigger(QUEST_DATA *run, int trig_type, const char *phrase, CHAR_DATA *enactor)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    CHAR_DATA *owner;
    int ret;
    const char *safe_phrase;

    if (!run)
        return false;

    safe_phrase = IS_NULLSTR(phrase) ? "" : phrase;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2 || !quest_index_v2->progs)
        return false;

    owner = enactor ? enactor : quest_runtime_get_owner_character(run);

    ret = p_lifecycle_bank_trigger(quest_index_v2->progs,
        quest_index_v2->area, NULL, NULL,
        run,
        owner, NULL, NULL,
        NULL, NULL,
        trig_type, (char *)safe_phrase);

    return ret != PRET_NOSCRIPT;
}

static void quest_runtime_fire_stage_lifecycle_trigger(QUEST_DATA *run, int trig_type, int stage_id)
{
    char phrase[MIL];

    if (!run || stage_id < 1)
        return;

    sprintf(phrase, "%d", stage_id);
    quest_runtime_fire_qprog_trigger(run, trig_type, phrase, NULL);
}

static void quest_runtime_fire_objective_lifecycle_trigger(QUEST_DATA *run, int trig_type, int objective_id)
{
    char phrase[MIL];

    if (!run || objective_id < 1)
        return;

    sprintf(phrase, "%d", objective_id);
    quest_runtime_fire_qprog_trigger(run, trig_type, phrase, NULL);
}

static void quest_runtime_fire_quest_lifecycle_trigger(QUEST_DATA *run, int trig_type, const char *phrase)
{
    quest_runtime_fire_quest_lifecycle_trigger_actor(run, trig_type, phrase, NULL);
}

static void quest_runtime_fire_quest_lifecycle_trigger_actor(QUEST_DATA *run, int trig_type, const char *phrase, CHAR_DATA *enactor)
{
    if (!run)
        return;

    quest_runtime_fire_qprog_trigger(run, trig_type, phrase, enactor);
}

static void quest_runtime_mark_run_failed(QUEST_DATA *run, int failed_status, const char *reason_phrase)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return;

    if (failed_status != QUEST_RUN_STATUS_FAILED && failed_status != QUEST_RUN_STATUS_ABANDONED)
        failed_status = QUEST_RUN_STATUS_FAILED;

    run->run_status = failed_status;
    run->completed_at = 0;
    if (failed_status == QUEST_RUN_STATUS_ABANDONED)
    {
        run->abandoned_at = current_time;
        run->failed_at = 0;
    }
    else
    {
        run->failed_at = current_time;
        run->abandoned_at = 0;
    }

    stage = quest_runtime_get_current_stage(run);
    if (stage && run->current_stage_commenced)
    {
        quest_runtime_emit_stage_message(run, stage, true);
        quest_runtime_fire_stage_lifecycle_trigger(run, TRIG_STAGE_FAILED, stage->id);

        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            QUEST_OBJECTIVE_STATE_V2_DATA *state;

            state = quest_runtime_get_objective_state(run, objective->id, false);
            if (state && state->complete)
                continue;

            quest_runtime_emit_objective_message(run, objective, true);
            quest_runtime_fire_objective_lifecycle_trigger(run, TRIG_OBJECTIVE_FAILED, objective->id);
        }
    }

    quest_runtime_fire_quest_lifecycle_trigger(run, TRIG_QUEST_FAILED, reason_phrase);
    quest_runtime_record_terminal_history(run);
}

static bool quest_run_accessible_by_player(CHAR_DATA *ch, QUEST_DATA *run, bool show_message)
{
    if (!ch || !run || IS_NPC(ch))
        return false;

    if (!quest_target_scope_supported_for_player(ch, run->target_scope, show_message))
        return false;

    quest_scope_owner_seed(ch, run);

    if (!quest_scope_owner_matches_player(ch, run)) {
        if (show_message)
            send_to_char("That quest run does not belong to your current quest scope owner.\n\r", ch);
        return false;
    }

    return true;
}

static const char *quest_run_display_name(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *index_v2;

    if (!run)
        return "(unknown quest)";

    if (run->quest_index_v2_vnum > 0) {
        index_v2 = quest_runtime_get_index_v2(run);
        if (index_v2 && !IS_NULLSTR(index_v2->name))
            return index_v2->name;
    }

    return "(legacy quest)";
}

static QUEST_DATA *quest_runtime_get_accessible_run_by_index(CHAR_DATA *ch, int index)
{
    QUEST_DATA *run;
    int visible = 0;

    if (!ch || IS_NPC(ch) || index < 1)
        return NULL;

    quest_runtime_attach_active_quest(ch, 0, 0);

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        visible++;
        if (visible == index)
            return run;
    }

    return NULL;
}

static QUEST_DATA *quest_runtime_find_run_by_name(CHAR_DATA *ch, const char *name)
{
    QUEST_DATA *run;
    QUEST_DATA *partial = NULL;

    if (!ch || IS_NPC(ch) || IS_NULLSTR(name))
        return NULL;

    quest_runtime_attach_active_quest(ch, 0, 0);

    for (run = ch->quest; run != NULL; run = run->next)
    {
        const char *run_name;

        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        run_name = quest_run_display_name(run);
        if (IS_NULLSTR(run_name))
            continue;

        if (!str_cmp(name, run_name))
            return run;

        if (!str_infix(name, run_name))
        {
            if (partial)
                return NULL;
            partial = run;
        }
    }

    return partial;
}

static QUEST_DATA *quest_resolve_command_run(CHAR_DATA *ch, const char *selector, bool show_message)
{
    QUEST_DATA *run;
    long target;

    if (!ch || IS_NPC(ch))
        return NULL;

    if (IS_NULLSTR(selector)) {
        run = quest_runtime_get_focused_run(ch);
        if (!run)
            run = ch->quest;

        if (!quest_run_accessible_by_player(ch, run, show_message))
            return NULL;

        return run;
    }

    if (!is_number((char *)selector)) {
        run = quest_runtime_find_run_by_name(ch, selector);
        if (!run) {
            if (show_message)
                send_to_char("No active quest matches that name (or the name is ambiguous).\n\r", ch);
            return NULL;
        }

        if (!quest_run_accessible_by_player(ch, run, show_message))
            return NULL;

        return run;
    }

    target = atol(selector);
    run = quest_runtime_get_accessible_run_by_index(ch, (int)target);
    if (!run)
        run = quest_runtime_get_run_by_id(ch, target);

    if (!run) {
        if (show_message)
            send_to_char("No active quest matches that selector.\n\r", ch);
        return NULL;
    }

    if (!quest_run_accessible_by_player(ch, run, show_message))
        return NULL;

    return run;
}

static bool is_quest_shop_object(OBJ_INDEX_DATA *obj_index)
{
    SHOP_DATA *shop;

    if (!obj_index) {
        return false;
    }

    for (shop = shop_first; shop != NULL; shop = shop->next) {
        SHOP_STOCK_DATA *stock;
        for (stock = shop->stock; stock != NULL; stock = stock->next) {
            if (stock->type != STOCK_OBJECT || stock->obj == NULL) {
                continue;
            }

            if (stock->obj == obj_index && stock->qp > 0) {
                return true;
            }
        }
    }

    return false;
}

#define QUEST_TOKEN_COUNT 6
static bool quest_tokens_resolved = false;
static WNUM quest_token_wnums[QUEST_TOKEN_COUNT];
static const WNUM_LOAD quest_token_loads[QUEST_TOKEN_COUNT] = {
    { 966, 100100 },
    { 966, 100101 },
    { 966, 100102 },
    { 966, 100103 },
    { 966, 100104 },
    { 966, 100105 }
};

static void resolve_quest_tokens(void)
{
    int i;

    if (quest_tokens_resolved) {
        return;
    }

    for (i = 0; i < QUEST_TOKEN_COUNT; i++) {
        WNUM_LOAD load = quest_token_loads[i];
        AREA_DATA *fallback = get_area_from_uid(load.auid);
        if (!fallback) {
            fallback = get_system_area_fallback();
        }
        quest_token_wnums[i] = wnum_zero;
        resolve_wnum_load(&load, &quest_token_wnums[i], fallback);
    }

    quest_tokens_resolved = true;
}

static bool quest_runtime_group_scope_snapshot_enabled(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;

    if (!run)
        return true;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2)
        return true;

    return IS_SET(quest_index_v2->flags, QUESTV2_FLAG_GROUP_SCOPE_SNAPSHOT);
}

static void quest_runtime_normalize_target_scope(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run)
        return;

    if (run->target_scope < QUEST_TARGET_SCOPE_CHARACTER
        || run->target_scope > QUEST_TARGET_SCOPE_CHURCH)
        run->target_scope = QUEST_TARGET_SCOPE_CHARACTER;

    if (run->target_scope == QUEST_TARGET_SCOPE_GROUP
        && (!IS_VALID(ch->group) || !uid_match(run->scope_owner_id, ch->group->id)))
    {
        if (quest_runtime_group_scope_snapshot_enabled(run)) {
            run->target_scope = QUEST_TARGET_SCOPE_CHARACTER;
            run->scope_owner_id[0] = ch->id[0];
            run->scope_owner_id[1] = ch->id[1];
            run->scope_owner_uid = 0;
        } else {
            return;
        }
    }

    if (run->target_scope == QUEST_TARGET_SCOPE_CHURCH
        && (!ch->church || ch->church->uid <= 0 || ch->church->uid != run->scope_owner_uid))
    {
        return;
    }

    quest_scope_owner_seed(ch, run);
}

void quest_runtime_snapshot_group_runs_to_character(CHAR_DATA *ch, const unsigned long group_id[2])
{
    quest_runtime_handle_group_scope_loss(ch, group_id);
}

void quest_runtime_handle_group_scope_loss(CHAR_DATA *ch, const unsigned long group_id[2])
{
    QUEST_DATA *run;
    QUEST_DATA *next;
    QUEST_DATA *prev = NULL;
    bool removed_focus = false;
    bool removed_any = false;

    if (!ch || IS_NPC(ch) || !ch->quest || !group_id)
        return;

    for (run = ch->quest; run != NULL; run = next)
    {
        bool snapshot;

        next = run->next;

        if (run->target_scope != QUEST_TARGET_SCOPE_GROUP)
        {
            prev = run;
            continue;
        }

        if (!uid_match(run->scope_owner_id, group_id))
        {
            prev = run;
            continue;
        }

        snapshot = quest_runtime_group_scope_snapshot_enabled(run);

        if (!snapshot)
        {
            if (run->run_id == ch->quest_runtime.focused_run_id)
                removed_focus = true;

            if (prev)
                prev->next = next;
            else
                ch->quest = next;

            run->next = NULL;
            free_quest(run);
            removed_any = true;
            continue;
        }

        run->target_scope = QUEST_TARGET_SCOPE_CHARACTER;
        run->scope_owner_id[0] = ch->id[0];
        run->scope_owner_id[1] = ch->id[1];
        run->scope_owner_uid = 0;
        prev = run;
    }

    if (!removed_any)
        return;

    if (!ch->quest)
    {
        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_reset_expiration(ch);
        return;
    }

    if (removed_focus || ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = ch->quest->run_id;
}

void quest_runtime_sync_group_runs_for_character(CHAR_DATA *ch, bool allow_stage_advance)
{
    QUEST_DATA *run;

    if (!ch || IS_NPC(ch) || !IS_VALID(ch->group) || !ch->quest)
        return;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
            continue;

        if (run->target_scope != QUEST_TARGET_SCOPE_GROUP)
            continue;

        if (!uid_match(run->scope_owner_id, ch->group->id))
            continue;

        if (run->generating)
            continue;

        quest_runtime_sync_group_cluster(ch, run, allow_stage_advance);
    }
}

static bool quest_runtime_should_purge_group_run(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run || run->target_scope != QUEST_TARGET_SCOPE_GROUP)
        return false;

    if (quest_runtime_group_scope_snapshot_enabled(run))
        return false;

    if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0)
        return true;

    if (!IS_VALID(ch->group))
        return true;

    return !uid_match(run->scope_owner_id, ch->group->id);
}

static void quest_runtime_purge_stale_group_runs(CHAR_DATA *ch)
{
    QUEST_DATA *run;
    QUEST_DATA *next;
    QUEST_DATA *prev = NULL;
    bool removed_focus = false;
    bool removed_any = false;

    if (!ch || IS_NPC(ch) || !ch->quest)
        return;

    for (run = ch->quest; run != NULL; run = next)
    {
        next = run->next;

        if (!quest_runtime_should_purge_group_run(ch, run))
        {
            prev = run;
            continue;
        }

        if (run->run_id == ch->quest_runtime.focused_run_id)
            removed_focus = true;

        if (prev)
            prev->next = next;
        else
            ch->quest = next;

        run->next = NULL;
        free_quest(run);
        removed_any = true;
    }

    if (!removed_any)
        return;

    if (!ch->quest)
    {
        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_reset_expiration(ch);
        return;
    }

    if (removed_focus || ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = ch->quest->run_id;
}

void quest_runtime_remove_church_runs(CHAR_DATA *ch, long church_uid)
{
    QUEST_DATA *run;
    QUEST_DATA *next;
    QUEST_DATA *prev = NULL;
    bool removed_focus = false;
    bool removed_any = false;

    if (!ch || IS_NPC(ch) || !ch->quest || church_uid <= 0)
        return;

    for (run = ch->quest; run != NULL; run = next)
    {
        next = run->next;

        if (run->target_scope != QUEST_TARGET_SCOPE_CHURCH)
        {
            prev = run;
            continue;
        }

        if (run->scope_owner_uid != church_uid)
        {
            prev = run;
            continue;
        }

        if (run->run_id == ch->quest_runtime.focused_run_id)
            removed_focus = true;

        if (prev)
            prev->next = next;
        else
            ch->quest = next;

        run->next = NULL;
        free_quest(run);
        removed_any = true;
    }

    if (!removed_any)
        return;

    if (!ch->quest)
    {
        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_reset_expiration(ch);
        return;
    }

    if (removed_focus || ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = ch->quest->run_id;
}

static bool quest_runtime_should_purge_church_run(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run || run->target_scope != QUEST_TARGET_SCOPE_CHURCH)
        return false;

    if (run->scope_owner_uid <= 0)
        return true;

    if (!ch->church || ch->church->uid <= 0)
        return true;

    return ch->church->uid != run->scope_owner_uid;
}

static void quest_runtime_purge_stale_church_runs(CHAR_DATA *ch)
{
    QUEST_DATA *run;
    QUEST_DATA *next;
    QUEST_DATA *prev = NULL;
    bool removed_focus = false;
    bool removed_any = false;

    if (!ch || IS_NPC(ch) || !ch->quest)
        return;

    for (run = ch->quest; run != NULL; run = next)
    {
        next = run->next;

        if (!quest_runtime_should_purge_church_run(ch, run))
        {
            prev = run;
            continue;
        }

        if (run->run_id == ch->quest_runtime.focused_run_id)
            removed_focus = true;

        if (prev)
            prev->next = next;
        else
            ch->quest = next;

        run->next = NULL;
        free_quest(run);
        removed_any = true;
    }

    if (!removed_any)
        return;

    if (!ch->quest)
    {
        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_reset_expiration(ch);
        return;
    }

    if (removed_focus || ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = ch->quest->run_id;
}

long quest_runtime_attach_active_quest(CHAR_DATA *ch, long quest_index_auid, long quest_index_vnum)
{
    QUEST_DATA *run;
    QUEST_DATA *target_run;

    if (!ch || IS_NPC(ch) || ch->quest == NULL) {
        return 0;
    }

    if (ch->quest_runtime.next_run_id <= 0) {
        ch->quest_runtime.next_run_id = 1;
    }

    quest_runtime_purge_stale_group_runs(ch);
    quest_runtime_purge_stale_church_runs(ch);

    if (ch->quest == NULL)
        return 0;

    for (run = ch->quest; run != NULL; run = run->next) {
        if (run->run_id <= 0) {
            run->run_id = ch->quest_runtime.next_run_id++;
        } else if (run->run_id >= ch->quest_runtime.next_run_id) {
            ch->quest_runtime.next_run_id = run->run_id + 1;
        }

        if (run->started_at <= 0) {
            run->started_at = current_time;
        }

        quest_runtime_ensure_generation_seed(run);

        quest_runtime_normalize_target_scope(ch, run);
        quest_runtime_normalize_template_link(ch, run);
        quest_runtime_seed_run_vars_from_index(run);
    }

    if (ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = ch->quest->run_id;

    target_run = ch->quest;
    if (ch->quest_runtime.focused_run_id > 0) {
        for (run = ch->quest; run != NULL; run = run->next) {
            if (run->run_id == ch->quest_runtime.focused_run_id) {
                target_run = run;
                break;
            }
        }
    }

    if (quest_index_vnum > 0 && target_run) {
        target_run->quest_index_auid = quest_index_auid;
        target_run->quest_index_vnum = quest_index_vnum;
        quest_runtime_normalize_template_link(ch, target_run);
        quest_runtime_seed_run_vars_from_index(target_run);
    }

    return target_run ? target_run->run_id : ch->quest->run_id;
}


QUEST_DATA *quest_runtime_get_run_by_id(CHAR_DATA *ch, long run_id)
{
    QUEST_DATA *run;

    if (!ch || IS_NPC(ch) || ch->quest == NULL)
        return NULL;

    quest_runtime_attach_active_quest(ch, 0, 0);
    if (run_id <= 0)
        run_id = ch->quest->run_id;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (run->run_id == run_id)
            return run;
    }

    return NULL;
}


QUEST_DATA *quest_runtime_get_run_by_index(CHAR_DATA *ch, int index)
{
    QUEST_DATA *run;
    int i;

    if (!ch || IS_NPC(ch) || ch->quest == NULL || index < 1)
        return NULL;

    quest_runtime_attach_active_quest(ch, 0, 0);

    i = 1;
    for (run = ch->quest; run != NULL; run = run->next, i++)
    {
        if (i == index)
            return run;
    }

    return NULL;
}


int quest_runtime_get_active_run_count(CHAR_DATA *ch)
{
    QUEST_DATA *run;
    int count = 0;

    if (!ch || IS_NPC(ch))
        return 0;

    for (run = ch->quest; run != NULL; run = run->next)
        count++;

    return count;
}


QUEST_DATA *quest_runtime_get_focused_run(CHAR_DATA *ch)
{
    QUEST_DATA *run;

    if (!ch || IS_NPC(ch))
        return NULL;

    if (ch->quest_runtime.focused_run_id <= 0)
        ch->quest_runtime.focused_run_id = quest_runtime_attach_active_quest(ch, 0, 0);

    run = quest_runtime_get_run_by_id(ch, ch->quest_runtime.focused_run_id);
    if (run && quest_run_accessible_by_player(ch, run, false))
        return run;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (quest_run_accessible_by_player(ch, run, false))
        {
            ch->quest_runtime.focused_run_id = run->run_id;
            return run;
        }
    }

    return NULL;
}


/**
 * quest_runtime_find_run_by_v2_wnum - Find a quest run by its v2 index widevnum
 *
 * Searches the character's active quest run list for a run matching
 * the given quest index v2 area UID and vnum. If auid is 0, matches
 * on vnum alone (legacy/local lookup).
 *
 * @param ch    Character to search
 * @param auid  Area UID of the quest index (0 for vnum-only match)
 * @param vnum  Vnum of the quest index
 * @return      First matching QUEST_DATA, or NULL if not found
 */
QUEST_DATA *quest_runtime_find_run_by_v2_wnum(CHAR_DATA *ch, long auid, long vnum)
{
    QUEST_DATA *run;

    if (!ch || IS_NPC(ch) || vnum <= 0)
        return NULL;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (auid > 0)
        {
            if (run->quest_index_v2_auid == auid
                && run->quest_index_v2_vnum == vnum)
                return run;
        }
        else
        {
            if (run->quest_index_v2_vnum == vnum)
                return run;
        }
    }

    return NULL;
}


/**
 * quest_history_find_by_v2_wnum - Find a quest history entry by v2 index widevnum
 *
 * Searches the character's quest history for an entry matching the given
 * quest index v2 area UID and vnum. Returns the first match (most recent).
 *
 * @param ch    Character to search
 * @param auid  Area UID of the quest index (0 for vnum-only match)
 * @param vnum  Vnum of the quest index
 * @return      First matching QUEST_HISTORY_DATA, or NULL if not found
 */
QUEST_HISTORY_DATA *quest_history_find_by_v2_wnum(CHAR_DATA *ch, long auid, long vnum)
{
    QUEST_HISTORY_DATA *history;

    if (!ch || IS_NPC(ch) || !ch->pcdata || vnum <= 0)
        return NULL;

    for (history = ch->pcdata->quest_history; history != NULL; history = history->next)
    {
        if (auid > 0)
        {
            if (history->quest_index_v2_auid == auid
                && history->quest_index_v2_vnum == vnum)
                return history;
        }
        else
        {
            if (history->quest_index_v2_vnum == vnum)
                return history;
        }
    }

    return NULL;
}


/**
 * quest_history_count_by_v2_wnum - Count quest history entries by status
 *
 * Counts how many quest history entries match the given v2 index widevnum
 * and have the specified run status. Useful for tracking repeatable quest
 * completions or failures.
 *
 * @param ch      Character to search
 * @param auid    Area UID of the quest index (0 for vnum-only match)
 * @param vnum    Vnum of the quest index
 * @param status  Run status to count (QUEST_RUN_STATUS_COMPLETED, etc.)
 * @return        Number of matching history entries
 */
int quest_history_count_by_v2_wnum(CHAR_DATA *ch, long auid, long vnum, int status)
{
    QUEST_HISTORY_DATA *history;
    int count = 0;

    if (!ch || IS_NPC(ch) || !ch->pcdata || vnum <= 0)
        return 0;

    for (history = ch->pcdata->quest_history; history != NULL; history = history->next)
    {
        if (history->run_status != status)
            continue;

        if (auid > 0)
        {
            if (history->quest_index_v2_auid == auid
                && history->quest_index_v2_vnum == vnum)
                count++;
        }
        else
        {
            if (history->quest_index_v2_vnum == vnum)
                count++;
        }
    }

    return count;
}


static void quest_runtime_detach_run(CHAR_DATA *ch, QUEST_DATA *run)
{
    QUEST_DATA *iter;
    QUEST_DATA *prev = NULL;
    long removed_run_id;

    if (!ch || IS_NPC(ch) || !run)
        return;

    for (iter = ch->quest; iter != NULL; iter = iter->next)
    {
        if (iter == run)
            break;
        prev = iter;
    }

    if (!iter)
        return;

    removed_run_id = iter->run_id;

    if (prev)
        prev->next = iter->next;
    else
        ch->quest = iter->next;

    iter->next = NULL;
    free_quest(iter);

    if (!ch->quest)
    {
        ch->quest_runtime.focused_run_id = 0;
        return;
    }

    quest_runtime_attach_active_quest(ch, 0, 0);
    if (ch->quest_runtime.focused_run_id == removed_run_id
        || !quest_runtime_get_run_by_id(ch, ch->quest_runtime.focused_run_id))
        ch->quest_runtime.focused_run_id = ch->quest->run_id;
}

bool quest_runtime_has_token(CHAR_DATA *ch, WNUM token_wnum)
{
    if (!ch || IS_NPC(ch) || !token_wnum.pArea || token_wnum.vnum < 1) {
        return false;
    }

    return get_token_char(ch, token_wnum.vnum, token_wnum.pArea, 1) != NULL;
}

TOKEN_DATA *quest_runtime_add_token(CHAR_DATA *ch, WNUM token_wnum)
{
    TOKEN_DATA *existing;
    TOKEN_INDEX_DATA *token_index;

    if (!ch || IS_NPC(ch) || !token_wnum.pArea || token_wnum.vnum < 1) {
        return NULL;
    }

    existing = get_token_char(ch, token_wnum.vnum, token_wnum.pArea, 1);
    if (existing) {
        return existing;
    }

    token_index = get_token_index(token_wnum.pArea, token_wnum.vnum);
    if (!token_index || token_index->type != TOKEN_QUEST) {
        return NULL;
    }

    return give_token(token_index, ch, NULL, NULL);
}

bool quest_runtime_remove_token(CHAR_DATA *ch, WNUM token_wnum, int count)
{
    int removed = 0;

    if (!ch || IS_NPC(ch) || !token_wnum.pArea || token_wnum.vnum < 1) {
        return false;
    }

    while (count <= 0 || removed < count) {
        TOKEN_DATA *token = get_token_char(ch, token_wnum.vnum, token_wnum.pArea, 1);
        if (!token) {
            break;
        }

        token_from_char(token);
        free_token(token);
        ++removed;
    }

    return removed > 0;
}

static void quest_runtime_update_player(CHAR_DATA *ch)
{
    QUEST_DATA *run;
    int mission_cap;
    long elapsed_seconds;
    long elapsed_minutes;
    int gain;
    time_t now;

    if (!ch || IS_NPC(ch) || !ch->pcdata) {
        return;
    }

    if (ch->quest_runtime.mission_allowance < 0) {
        ch->quest_runtime.mission_allowance = 0;
    }

    ch->quest_runtime.points_bank = ch->questpoints;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (run->run_status != QUEST_RUN_STATUS_ACTIVE)
            continue;

        if (!quest_runtime_scope_owner_online(run))
            quest_runtime_unbind_strict_targets(run);
    }

    mission_cap = game_settings.max_mission_allowance;
    if (ch->church) {
        mission_cap += UMAX(0, ch->church->quest_data.mission_allowance_bonus);
    }

    if (mission_cap <= 0 || game_settings.inc_missions <= 0) {
        return;
    }

    if (ch->quest_runtime.mission_allowance > mission_cap) {
        ch->quest_runtime.mission_allowance = mission_cap;
    }

    now = current_time;
    if (ch->quest_runtime.allowance_last_update <= 0) {
        ch->quest_runtime.allowance_last_update = now;
        return;
    }

    elapsed_seconds = (long)(now - ch->quest_runtime.allowance_last_update);
    if (elapsed_seconds < 60) {
        return;
    }

    elapsed_minutes = elapsed_seconds / 60;
    gain = (int)(elapsed_minutes * game_settings.inc_missions);

    ch->quest_runtime.mission_allowance = UMIN(mission_cap, ch->quest_runtime.mission_allowance + gain);
    ch->quest_runtime.allowance_last_update = now;
}

bool quest_runtime_manual_trigger_ready(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata || !ch->in_room || !ch->in_room->area) {
        return false;
    }

    if (!(ch->quest_runtime.expiry_modes & QUEST_EXPIRY_MANUAL_AREA)) {
        return true;
    }

    if (ch->quest_runtime.manual_trigger_area_uid <= 0) {
        return false;
    }

    return ch->in_room->area->uid == ch->quest_runtime.manual_trigger_area_uid;
}

bool quest_runtime_is_expired(CHAR_DATA *ch, time_t now)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata) {
        return false;
    }

    if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_WALL_TIME)
        && ch->quest_runtime.expires_at > 0
        && now >= ch->quest_runtime.expires_at) {
        return true;
    }

    if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
        && ch->quest_runtime.expiry_countdown_minutes <= 0) {
        return true;
    }

    return false;
}

void quest_runtime_tick_expiration(CHAR_DATA *ch, time_t now)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata) {
        return;
    }

    if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
        && ch->quest_runtime.expiry_countdown_minutes > 0) {
        --ch->quest_runtime.expiry_countdown_minutes;
    }

    ch->countdown = UMAX(0, ch->quest_runtime.expiry_countdown_minutes);
    if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_WALL_TIME)
        && ch->quest_runtime.expires_at > 0
        && ch->quest_runtime.expires_at < now) {
        ch->quest_runtime.expires_at = now;
    }
}

static void quest_runtime_reset_expiration(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch))
        return;

    ch->countdown = 0;
    ch->quest_runtime.expiry_countdown_minutes = 0;
    ch->quest_runtime.expires_at = 0;
    ch->quest_runtime.manual_trigger_area_uid = 0;
    ch->quest_runtime.expiry_modes = QUEST_EXPIRY_NONE;
}

OBJ_DATA *generate_quest_scroll(CHAR_DATA *ch, QUEST_DATA *run, char *questgiver, long vnum,
    char *header, char *footer, char *prefix, char *suffix, int line_width)
{
    AREA_DATA *scroll_area = NULL;
    WNUM wnum;
    if (resolve_widevnum(vnum, NULL, &wnum))
        scroll_area = wnum.pArea;
    if (!scroll_area) scroll_area = get_system_area_fallback();
    OBJ_INDEX_DATA *scroll_index = get_obj_index(scroll_area, vnum);
    if( scroll_index == NULL )
    {
        scroll_index = get_reserved_obj_index("obj_quest_scroll");
    }

    OBJ_DATA *scroll = create_object(scroll_index, 0, true);
    if( scroll != NULL )
    {
        /*
        sprintf(buf2,
            "{W  .-.--------------------------------------------------------------------------------------.-.\n\r"
            "((o))                                                                                         )\n\r"
            "{W \\U/_________________________________________________________________________________________/\n\r"
            "{W  |\n\r"
            "{W  |  {xNoble %s{x,\n\r{W  |\n\r"
            "{W  |  {xThis is an official quest scroll given to you by %s.\n\r"
            "{W  |  {xUpon this scroll is my seal, and my approval to go to any\n\r"
            "{W  |  {xmeasures in order to complete the set of tasks I have listed.\n\r"
            "{W  |  {xReturn to me once you have completed these tasks, and you\n\r"
            "{W  |  {xshall be justly rewarded.\n\r{W  |  {x\n\r",
            ch->name, questgiver);
        */

        BUFFER *buffer = new_buf();
        char buf[MSL];

        // Need to add overflow protection
        char *replace1 = string_replace_static(header, "$PLAYER$", ch->name);
        char *replace2 = string_replace_static(replace1, "$QUESTOR$", questgiver);
        add_buf(buffer, replace2);

        for (QUEST_PART_DATA *part = run ? run->parts : NULL; part != NULL; part = part->next)
        {
            if( line_width > 0 && !IS_NULLSTR(suffix) )
            {
                int width = line_width + get_colour_width(part->description);

                sprintf(buf, "%s%-*.*s%s\n\r", prefix, width, width, part->description, suffix);
            }
            else
            {
                sprintf(buf, "%s%s\n\r", prefix, part->description);
            }
            add_buf(buffer, buf);
        }

        /*
        sprintf(buf, "{W  |__________________________________________________________________________________________\n\r"
            "{W /A\\                                                                                         \\\n\r"
            "((o))                                                                                         )\n\r"
            "{W  '-'----------------------------------------------------------------------------------------'\n\r");*/

        // Need to add overflow protection
        replace1 = string_replace_static(footer, "$PLAYER$", ch->name);
        replace2 = string_replace_static(replace1, "$QUESTOR$", questgiver);
        add_buf(buffer, replace2);

        free_string(scroll->full_description);
        scroll->full_description = str_dup(buffer->string);

        free_buf(buffer);
    }

    return scroll;
}

/*
 * quest_list_show_v2 - print one v2 quest entry in the quest list
 *
 * Evaluates prerequisites and appends a tag if the player can't take it:
 *   {R(locked: reason){x  — unmet, has player_string
 *   {D(locked: additional requirements){x  — unmet, hidden flag, no player_string
 *   {D(locked){x          — unmet, no player_string, not hidden
 *   (nothing)             — met
 */
static void quest_list_show_v2(CHAR_DATA *ch, QUEST_INDEX_V2_DATA *qv2,
                               int *total_shown, bool indent4)
{
    const char *qname  = IS_NULLSTR(qv2->name) ? "(unnamed quest)" : qv2->name;
    const char *indent = indent4 ? "    " : "  ";
    char        req_tag[MAX_INPUT_LENGTH];
    bool        meets = true;
    char       *ps    = NULL;

    req_tag[0] = '\0';

    if (!IS_IMMORTAL(ch) && !IS_NULLSTR(qv2->prerequisites)) {
        REQUIREMENT_CONTEXT ctx;

        memset(&ctx, 0, sizeof(ctx));
        ctx.actor      = ch;
        ctx.self_quest = qv2;
        meets = requirements_evaluate_text(qv2->prerequisites, &ctx, true);
        ps    = requirements_get_player_string(qv2->prerequisites);
    }

    if (!meets) {
        if (ps && *ps)
            snprintf(req_tag, sizeof(req_tag), " {R(locked: %s){x", ps);
        else if (requirements_is_hidden(qv2->prerequisites))
            snprintf(req_tag, sizeof(req_tag), " {D(locked: additional requirements){x");
        else
            snprintf(req_tag, sizeof(req_tag), " {D(locked){x");
    }

    if (ps) free(ps);

    printf_to_char(ch, "%s[{Y%d{x] %s%s\n\r",
        indent, ++(*total_shown), qname, req_tag);
}

/*
 * quest_inspect_show_v2 - show pre-accept details for a v2 quest
 *
 * Displays the quest name, quest-level description, allowance cost,
 * prerequisites (with met/unmet indicator), and rewards.
 * Only shows prerequisite lines that carry a player_string annotation.
 * If overall prerequisites fail but have no player_string, a generic
 * "Requirements not met" line is shown instead.
 */
static void quest_inspect_show_v2(CHAR_DATA *ch, QUEST_INDEX_V2_DATA *qv2)
{
    QUEST_REWARD_INDEX_V2_DATA       *rew;
    REQUIREMENT_CONTEXT               ctx;
    bool                              meets        = true;
    bool                              has_prereqs  = false;
    char                             *ps           = NULL;

    line(ch, 50, "{Y", "-");
    printf_to_char(ch, "{Y%s{x\n\r",
        IS_NULLSTR(qv2->name) ? "(unnamed quest)" : qv2->name);
    line(ch, 50, "{Y", "-");

    /* Description */
    if (!IS_NULLSTR(qv2->description))
        printf_to_char(ch, "%s\n\r", qv2->description);

    /* Allowance cost */
    if (qv2->allowance_cost > 0)
        printf_to_char(ch, "{WCost:{x %d mission allowance%s\n\r",
            qv2->allowance_cost,
            qv2->allowance_cost == 1 ? "" : "s");

    /* Prerequisites */
    if (!IS_NULLSTR(qv2->prerequisites)) {
        has_prereqs = true;
        memset(&ctx, 0, sizeof(ctx));
        ctx.actor      = ch;
        ctx.self_quest = qv2;
        meets = requirements_evaluate_text(qv2->prerequisites, &ctx, true);
        ps    = requirements_get_player_string(qv2->prerequisites);

        if (ps && *ps) {
            printf_to_char(ch, "{WRequires:{x %s%s{x\n\r",
                meets ? "{G\u2713 " : "{R\u2717 ", ps);
        } else if (!meets) {
            send_to_char("{WRequires:{x {Radditional requirements not met{x\n\r", ch);
        }

        if (ps) free(ps);
    }

    /* Rewards */
    if (qv2->rewards) {
        send_to_char("{WRewards:{x\n\r", ch);
        for (rew = qv2->rewards; rew; rew = rew->next) {
            if (!IS_NULLSTR(rew->display_string)) {
                printf_to_char(ch, "  %s\n\r", rew->display_string);
                continue;
            }
            switch (rew->reward_type) {
                case QUEST_REWARD_POINTS:
                    printf_to_char(ch, "  %d quest point%s\n\r",
                        rew->amount, rew->amount == 1 ? "" : "s");
                    break;
                case QUEST_REWARD_CURRENCY:
                    printf_to_char(ch, "  %d %s\n\r",
                        rew->amount,
                        IS_NULLSTR(rew->currency) ? "coin(s)" : rew->currency);
                    break;
                case QUEST_REWARD_REPUTATION:
                {
                    REPUTATION_INDEX_DATA *rep_index = NULL;

                    if (rew->target_wnum.pArea && rew->target_wnum.vnum > 0)
                        rep_index = get_reputation_index_wnum(rew->target_wnum);

                    if (rep_index && !IS_NULLSTR(rep_index->name))
                        printf_to_char(ch, "  %d reputation with %s\n\r", rew->amount, rep_index->name);
                    else
                        printf_to_char(ch, "  %d reputation\n\r", rew->amount);
                    break;
                }
                case QUEST_REWARD_TOKEN:
                    printf_to_char(ch, "  token (x%d)\n\r", rew->amount);
                    break;
                case QUEST_REWARD_ITEM:
                {
                    OBJ_INDEX_DATA *obj_index = NULL;
                    long item_count = UMAX(1, rew->amount);
                    const char *item_name = "item reward";
                    char item_name_buf[MSL];
                    size_t idx;

                    if (rew->target_wnum.pArea && rew->target_wnum.vnum > 0)
                        obj_index = get_obj_index(rew->target_wnum.pArea, rew->target_wnum.vnum);

                    if (obj_index)
                    {
                        if (!IS_NULLSTR(obj_index->list_name))
                            item_name = obj_index->list_name;
                        else if (!IS_NULLSTR(obj_index->short_descr))
                            item_name = obj_index->short_descr;
                        else if (!IS_NULLSTR(obj_index->name))
                            item_name = obj_index->name;
                    }

                    snprintf(item_name_buf, sizeof(item_name_buf), "%s", item_name);
                    for (idx = 0; item_name_buf[idx] != '\0'; idx++)
                    {
                        if (item_name_buf[idx] == '{' && item_name_buf[idx + 1] != '\0')
                        {
                            idx++;
                            continue;
                        }

                        if (item_name_buf[idx] >= 'a' && item_name_buf[idx] <= 'z')
                        {
                            item_name_buf[idx] = UPPER(item_name_buf[idx]);
                            break;
                        }
                    }

                    if (item_count > 1)
                        printf_to_char(ch, "  %s (x%ld)\n\r", item_name_buf, item_count);
                    else
                        printf_to_char(ch, "  %s\n\r", item_name_buf);
                    break;
                }
                case QUEST_REWARD_SCRIPT:
                    /* intentionally silent */
                    break;
                default:
                    break;
            }
        }
    }

    line(ch, 50, "{Y", "-");

    if (has_prereqs && !meets)
        send_to_char("Use {Yquest list{x — items marked {R(locked){x cannot be accepted yet.\n\r", ch);
    else
        send_to_char("Use {Yquest accept <name>{x to take this quest.\n\r", ch);
}

void do_quest(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    ROOM_INDEX_DATA *room = NULL;
    char buf[MAX_STRING_LENGTH];
    char target_arg[MSL];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    long active_run_id;
    QUEST_DATA *run;
    ITERATOR it;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    quest_runtime_update_player(ch);

    active_run_id = quest_runtime_attach_active_quest(ch, 0, 0);
    if (active_run_id <= 0)
        active_run_id = 1;

    if (arg1[0] == '\0')
    {
        send_to_char("QUEST commands: LOG HISTORY LIST INSPECT ACCEPT FOCUS SYNC POINTS INFO DETAILS TIME COMMENCE CANCEL COMPLETE GRANT.\n\r", ch);
        send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
        return;
    }

    if (!str_cmp(arg1, "grant"))
    {
        CHAR_DATA *victim;
        QUEST_DATA *active_quest;
        QUEST_INDEX_V2_DATA *quest_index_v2;
        WNUM wnum;
        char quest_ref[MIL];

        if (!IS_IMMORTAL(ch)) {
            send_to_char("You do not have access to that command.\n\r", ch);
            return;
        }

        one_argument(argument, quest_ref);

        if (IS_NULLSTR(arg2) || IS_NULLSTR(quest_ref)) {
            send_to_char("Syntax: quest grant <player> <auid>#<vnum>\n\r", ch);
            send_to_char("        (You may use bare <vnum> while standing in the quest's area.)\n\r", ch);
            return;
        }

        victim = get_char_world(ch, arg2);
        if (!victim || IS_NPC(victim)) {
            send_to_char("Quest grant target must be an online player character.\n\r", ch);
            return;
        }

        if (!quest_parse_index_v2_ref(ch, quest_ref, &wnum)) {
            send_to_char("Invalid quest reference. Use <auid>#<vnum> (or bare <vnum> in-area).\n\r", ch);
            return;
        }

        quest_index_v2 = get_quest_index_v2_wnum(wnum);
        if (!quest_index_v2) {
            send_to_char("Quest grant failed: v2 quest index not found.\n\r", ch);
            return;
        }

        if (!quest_target_scope_supported_for_player(victim, quest_index_v2->target_scope, false)) {
            send_to_char("Quest grant failed: target scope currently unsupported for that player.\n\r", ch);
            return;
        }

        active_quest = new_quest();
        active_quest->next = victim->quest;
        victim->quest = active_quest;

        victim->quest_runtime.focused_run_id = 0;
        quest_runtime_attach_active_quest(victim, 0, 0);
        active_quest = quest_runtime_get_focused_run(victim);

        if (!active_quest) {
            send_to_char("Quest grant failed: unable to initialize runtime state.\n\r", ch);
            return;
        }

        active_quest->quest_index_auid = 0;
        active_quest->quest_index_vnum = 0;
        active_quest->target_scope = quest_index_v2->target_scope;
        active_quest->scope_owner_id[0] = 0;
        active_quest->scope_owner_id[1] = 0;
        active_quest->scope_owner_uid = 0;
        quest_scope_owner_seed(victim, active_quest);

        /* Prerequisites check (bypassed if granting immortal forces it) */
        if (!IS_IMMORTAL(ch) && !IS_NULLSTR(quest_index_v2->prerequisites)) {
            REQUIREMENT_CONTEXT prereq_ctx;
            prereq_ctx.actor      = victim;
            prereq_ctx.self_mob   = NULL;
            prereq_ctx.self_obj   = NULL;
            prereq_ctx.self_room  = NULL;
            prereq_ctx.self_token = NULL;
            prereq_ctx.self_quest = quest_index_v2;
            if (!requirements_evaluate_text(quest_index_v2->prerequisites, &prereq_ctx, true)) {
                quest_runtime_detach_run(victim, active_quest);
                printf_to_char(ch, "Quest grant failed: %s does not meet the prerequisites.\n\r", victim->name);
                return;
            }
        }

        if (!quest_runtime_bind_index_v2(active_quest, wnum)) {
            quest_runtime_detach_run(victim, active_quest);
            send_to_char("Quest grant failed: unable to bind quest index.\n\r", ch);
            return;
        }

        quest_runtime_fire_quest_lifecycle_trigger_actor(active_quest, TRIG_QUEST_ACCEPTED, "grant", victim);
        quest_runtime_fire_quest_lifecycle_trigger_actor(active_quest, TRIG_QUEST_FOCUSED, "grant", victim);
        quest_runtime_try_advance_stage(active_quest);

        printf_to_char(ch, "Granted quest %ld#%ld to %s (run %ld).\n\r",
            wnum.pArea ? wnum.pArea->uid : 0,
            wnum.vnum,
            victim->name,
            active_quest->run_id);

        if (victim != ch)
            printf_to_char(victim, "An immortal granted you quest {%ld#%ld{x (run %ld).\n\r",
                wnum.pArea ? wnum.pArea->uid : 0,
                wnum.vnum,
                active_quest->run_id);

        return;
    }

    //
    // QUEST LOG  (own quests; arg2 reserved for future category/type filter)
    // QUEST ADMINLOG <player>  (immortal view of another player's quest log)
    //
    if (!str_cmp(arg1, "log") || !str_cmp(arg1, "adminlog"))
    {
        long age_minutes = 0;
        int index = 0;
        bool shown_any = false;
        bool admin_view = false;
        bool explicit_type = false;
        bool explicit_category = false;
        int type_filter = -1;
        int category_filter = -1;
        CHAR_DATA *view_ch = ch;
        long view_active_run_id;
        QUEST_DATA *focused_run;

        if (!str_cmp(arg1, "adminlog"))
        {
            if (!IS_IMMORTAL(ch))
            {
                send_to_char("Only immortals can use quest adminlog.\n\r", ch);
                return;
            }

            if (IS_NULLSTR(arg2))
            {
                send_to_char("Usage: quest adminlog <player>\n\r", ch);
                return;
            }

            view_ch = get_char_world(ch, arg2);
            if (!view_ch || IS_NPC(view_ch))
            {
                send_to_char("Quest adminlog target must be an online player character.\n\r", ch);
                return;
            }

            admin_view = (view_ch != ch);
        }

        // For "quest log", parse optional type/category filters from remaining args
        if (!admin_view && !IS_NULLSTR(arg2))
        {
            char parse_buf[MSL];
            char token[MIL];
            char type_name[MIL];
            char category_name[MIL];
            bool expect_type = false;
            bool expect_category = false;

            parse_buf[0] = '\0';
            strncat(parse_buf, arg2, sizeof(parse_buf) - strlen(parse_buf) - 1);
            if (!IS_NULLSTR(argument))
            {
                strncat(parse_buf, " ", sizeof(parse_buf) - strlen(parse_buf) - 1);
                strncat(parse_buf, argument, sizeof(parse_buf) - strlen(parse_buf) - 1);
            }

            argument = parse_buf;
            while (!IS_NULLSTR(argument))
            {
                argument = one_argument(argument, token);
                if (IS_NULLSTR(token))
                    break;

                if (expect_type)
                {
                    if (!quest_parse_type(token, &type_filter))
                    {
                        send_to_char("Unknown quest type. Use main, side, unlock, class, event, or other.\n\r", ch);
                        return;
                    }
                    explicit_type = true;
                    expect_type = false;
                    continue;
                }

                if (expect_category)
                {
                    if (!quest_parse_history_category(token, &category_filter))
                    {
                        send_to_char("Unknown category. Use none/regional/class/story/church/dungeon/crafting/event/other.\n\r", ch);
                        return;
                    }
                    explicit_category = true;
                    expect_category = false;
                    continue;
                }

                if (!str_prefix(token, "type"))
                {
                    if (token[4] == ':' || token[4] == '=')
                    {
                        strncpy(type_name, token + 5, sizeof(type_name) - 1);
                        type_name[sizeof(type_name) - 1] = '\0';
                        if (!quest_parse_type(type_name, &type_filter))
                        {
                            send_to_char("Unknown quest type. Use main, side, unlock, class, event, or other.\n\r", ch);
                            return;
                        }
                        explicit_type = true;
                    }
                    else
                        expect_type = true;
                    continue;
                }

                if (!str_prefix(token, "category"))
                {
                    if (token[8] == ':' || token[8] == '=')
                    {
                        strncpy(category_name, token + 9, sizeof(category_name) - 1);
                        category_name[sizeof(category_name) - 1] = '\0';
                        if (!quest_parse_history_category(category_name, &category_filter))
                        {
                            send_to_char("Unknown category. Use none/regional/class/story/church/dungeon/crafting/event/other.\n\r", ch);
                            return;
                        }
                        explicit_category = true;
                    }
                    else
                        expect_category = true;
                    continue;
                }

                send_to_char("Usage: quest log [type:<type>] [category:<category>]\n\r"
                             "  Types: main, side, unlock, class, event, other\n\r"
                             "  Categories: none, regional, class, story, church, dungeon, crafting, event, other\n\r", ch);
                return;
            }

            if (expect_type)
            {
                send_to_char("Missing quest type value.\n\r", ch);
                return;
            }
            if (expect_category)
            {
                send_to_char("Missing category value.\n\r", ch);
                return;
            }
        }

        if (!IS_QUESTING(view_ch))
        {
            if (admin_view)
                printf_to_char(ch, "%s has no active quests.\n\r", view_ch->name);
            else
                send_to_char("You have no active quests.\n\r", ch);
            return;
        }

        view_active_run_id = quest_runtime_attach_active_quest(view_ch, 0, 0);
        if (view_active_run_id <= 0)
            view_active_run_id = 1;

        if (view_ch->quest_runtime.focused_run_id <= 0)
            view_ch->quest_runtime.focused_run_id = view_active_run_id;

        if (admin_view)
            printf_to_char(ch, "Admin view: %s's quest log\n\r", view_ch->name);
        else
        {
            printf_to_char(ch, "Quest log");
            if (explicit_type)
                printf_to_char(ch, " type:%s", quest_type_name(type_filter));
            if (explicit_category)
                printf_to_char(ch, " category:%s", quest_category_name(category_filter));
            send_to_char("\n\r", ch);
        }

        for (run = view_ch->quest; run != NULL; run = run->next)
        {
            QUEST_INDEX_V2_DATA *run_index_v2;
            QUEST_STAGE_INDEX_V2_DATA *stage;
            const char *run_name = "";
            const char *stage_name = "";

            if (!quest_run_accessible_by_player(view_ch, run, false))
                continue;

            run_index_v2 = quest_runtime_get_index_v2(run);

            // Apply log filters (only skip when we have index data to filter against)
            if (run_index_v2 && type_filter >= 0 && run_index_v2->quest_type != type_filter)
                continue;
            if (run_index_v2 && category_filter >= 0 && run_index_v2->category != category_filter)
                continue;

            index++;

            bool focused = (view_ch->quest_runtime.focused_run_id == run->run_id);
            shown_any = true;

            if (run_index_v2 && !IS_NULLSTR(run_index_v2->name))
                run_name = run_index_v2->name;

            if (IS_NULLSTR(run_name))
                run_name = "(unnamed quest)";

            printf_to_char(ch, "[%d] %s%s [%s]\n\r",
                index,
                focused ? "* " : "  ",
                run_name,
                quest_target_scope_name(run->target_scope));

            if (run->started_at > 0)
                age_minutes = UMAX(0, (long)((current_time - run->started_at) / 60));
            else
                age_minutes = 0;

            stage = quest_runtime_get_current_stage(run);
            if (run->generating)
                stage_name = "(generating)";
            else if (stage && !IS_NULLSTR(stage->name))
                stage_name = stage->name;
            else if (stage)
                stage_name = "(unnamed stage)";
            else
                stage_name = "(none)";

            printf_to_char(ch,
                "      stage: %s (%s)  started: %ld minute%s ago\n\r",
                stage_name,
                run->current_stage_commenced ? "commenced" : "not commenced",
                age_minutes,
                age_minutes == 1 ? "" : "s");

            if (admin_view)
                printf_to_char(ch,
                    "      [debug] run_status:%d stage_id:%d stage_commenced:%d stage_gen:%d\n\r"
                    "      [debug] generation_seed:%llu stage_seed:%llu\n\r",
                    run->run_status,
                    run->current_stage_id,
                    run->current_stage_commenced,
                    run->current_stage_generation,
                    run->generation_seed,
                    run->current_stage_seed);
        }

        if (!shown_any)
        {
            if (admin_view)
                printf_to_char(ch, "%s has no active quests available to their character scope.\n\r", view_ch->name);
            else if (type_filter >= 0 || category_filter >= 0)
                send_to_char("No active quests match your filters.\n\r", ch);
            else
                send_to_char("You have no active quests available to your character scope.\n\r", ch);
            return;
        }

        focused_run = quest_runtime_get_focused_run(view_ch);
        if (focused_run && quest_run_accessible_by_player(view_ch, focused_run, false) && !focused_run->generating)
        {
            if (focused_run->quest_index_v2_vnum > 0)
            {
                QUEST_INDEX_V2_DATA *quest_index_v2;
                QUEST_STAGE_INDEX_V2_DATA *stage;
                QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
                bool shown_objective = false;

                quest_index_v2 = quest_runtime_get_index_v2(focused_run);
                stage = quest_runtime_get_current_stage(focused_run);

                if (quest_index_v2)
                    printf_to_char(ch, "\n\rFocused quest%s: {Y%s{x\n\r",
                        admin_view ? " [admin]" : "",
                        IS_NULLSTR(quest_index_v2->name) ? "(unnamed quest)" : quest_index_v2->name);

                if (stage)
                {
                    printf_to_char(ch, "Current stage {Y%d{x: %s\n\r",
                        stage->id,
                        IS_NULLSTR(stage->name) ? "(unnamed stage)" : stage->name);

                    for (objective = stage->objectives; objective != NULL; objective = objective->next)
                    {
                        QUEST_OBJECTIVE_STATE_V2_DATA *state;
                        int required;
                        int progress;
                        bool objective_complete;
                        bool objective_failed;
                        bool objective_pending;
                        char indicator_buf[128];

                        state = quest_runtime_get_objective_state(focused_run, objective->id, false);
                        objective_complete = (state && state->complete);
                        objective_failed = (!objective_complete
                            && focused_run->run_status == QUEST_RUN_STATUS_FAILED);
                        objective_pending = (!objective_complete
                            && !objective_failed
                            && !focused_run->current_stage_commenced);

                        required = quest_objective_required_display_count(objective);
                        progress = state ? state->progress : 0;

                        indicator_buf[0] = '\0';
                        if (objective->optional)
                            strncat(indicator_buf, " {C[optional]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                        if (objective_complete)
                            strncat(indicator_buf, " {G[completed]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                        else if (objective_failed && objective->optional)
                            strncat(indicator_buf, " {R[failed]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                        else if (objective_pending)
                            strncat(indicator_buf, " {Y[pending]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);

                        {
                            char target_buf[MSL];

                            printf_to_char(ch, "  [{Y%d{x] %s {D(%s){x %d/%d%s\n\r",
                                objective->id,
                                quest_objective_visible_label(objective),
                                quest_objective_type_name(objective->objective_type),
                                progress,
                                required,
                                indicator_buf);
                            printf_to_char(ch, "       %s\n\r",
                                quest_objective_target_summary(focused_run, objective, target_buf, sizeof(target_buf),
                                    admin_view || IS_STAFF(ch, STAFF_GIMP)));
                            quest_objective_target_options_display(ch, focused_run, objective,
                                admin_view || IS_STAFF(ch, STAFF_GIMP));
                        }
                        shown_objective = true;
                    }

                    if (!shown_objective)
                        send_to_char("  (No active objectives on current stage.)\n\r", ch);
                }
            }
        }

        if (!admin_view)
            printf_to_char(ch, "Use 'quest focus <index|name>' to focus a quest.\n\r");
        return;
    }

    //
    // QUEST HISTORY
    //
    if (!str_cmp(arg1, "history"))
    {
        QUEST_HISTORY_DATA *history;
        QUEST_HISTORY_DATA *matched;
        int shown = 0;
        int index = 0;
        int status_filter = QUEST_RUN_STATUS_COMPLETED;
        int category_filter = -1;
        bool explicit_status = false;
        bool explicit_category = false;
        char selector[MSL];

        selector[0] = '\0';

        if (!IS_NULLSTR(arg2))
        {
            char parse_buf[MSL];
            char token[MIL];
            char category_name[MIL];
            char status_name[MIL];
            bool expect_status = false;
            bool expect_category = false;

            parse_buf[0] = '\0';
            strncat(parse_buf, arg2, sizeof(parse_buf) - strlen(parse_buf) - 1);
            if (!IS_NULLSTR(argument))
            {
                strncat(parse_buf, " ", sizeof(parse_buf) - strlen(parse_buf) - 1);
                strncat(parse_buf, argument, sizeof(parse_buf) - strlen(parse_buf) - 1);
            }

            argument = parse_buf;
            while (!IS_NULLSTR(argument))
            {
                argument = one_argument(argument, token);
                if (IS_NULLSTR(token))
                    break;

                if (expect_status)
                {
                    if (!quest_parse_history_status(token, &status_filter))
                    {
                        send_to_char("Unknown history status. Use completed, failed, abandoned, or all.\n\r", ch);
                        return;
                    }
                    explicit_status = true;
                    expect_status = false;
                    continue;
                }

                if (expect_category)
                {
                    if (!quest_parse_history_category(token, &category_filter))
                    {
                        send_to_char("Unknown history category. Use none/regional/class/story/church/dungeon/crafting/event/other.\n\r", ch);
                        return;
                    }
                    explicit_category = true;
                    expect_category = false;
                    continue;
                }

                if (!str_prefix(token, "status"))
                {
                    if (token[6] == ':' || token[6] == '=')
                    {
                        strncpy(status_name, token + 7, sizeof(status_name) - 1);
                        status_name[sizeof(status_name) - 1] = '\0';
                        if (!quest_parse_history_status(status_name, &status_filter))
                        {
                            send_to_char("Unknown history status. Use completed, failed, abandoned, or all.\n\r", ch);
                            return;
                        }
                        explicit_status = true;
                    }
                    else
                        expect_status = true;
                    continue;
                }

                if (!str_prefix(token, "category"))
                {
                    if (token[8] == ':' || token[8] == '=')
                    {
                        strncpy(category_name, token + 9, sizeof(category_name) - 1);
                        category_name[sizeof(category_name) - 1] = '\0';
                        if (!quest_parse_history_category(category_name, &category_filter))
                        {
                            send_to_char("Unknown history category. Use none/regional/class/story/church/dungeon/crafting/event/other.\n\r", ch);
                            return;
                        }
                        explicit_category = true;
                    }
                    else
                        expect_category = true;
                    continue;
                }

                if (selector[0] != '\0')
                    strncat(selector, " ", sizeof(selector) - strlen(selector) - 1);
                strncat(selector, token, sizeof(selector) - strlen(selector) - 1);
            }

            if (expect_status)
            {
                send_to_char("Missing history status value.\n\r", ch);
                return;
            }
            if (expect_category)
            {
                send_to_char("Missing history category value.\n\r", ch);
                return;
            }
        }

        if (!ch->pcdata || !ch->pcdata->quest_history)
        {
            send_to_char("No quest history is recorded yet.\n\r", ch);
            return;
        }

        if (!IS_NULLSTR(selector))
        {
            matched = quest_history_find_by_name(ch, selector);
            if (!matched)
            {
                send_to_char("No quest history entry matches that name (or the name is ambiguous).\n\r", ch);
                return;
            }

            if (!quest_history_matches_filters(matched, status_filter, category_filter))
            {
                send_to_char("A matching history entry exists but does not match the active filters.\n\r", ch);
                return;
            }

            quest_show_history_entry(ch, matched);
            return;
        }

        printf_to_char(ch, "Quest history (missions completed: {Y%ld{x)",
            ch->pcdata->missions_completed);
        if (explicit_status)
            printf_to_char(ch, " status:%s", status_filter < 0 ? "all" : quest_run_status_name(status_filter));
        if (explicit_category)
            printf_to_char(ch, " category:%s", quest_category_name(category_filter));
        send_to_char("\n\r", ch);

        for (history = ch->pcdata->quest_history; history != NULL; history = history->next)
        {
            time_t terminal_time;
            long age_minutes;

            if (!quest_history_matches_filters(history, status_filter, category_filter))
                continue;

            index++;
            terminal_time = quest_history_terminal_time(history);
            age_minutes = terminal_time > 0 ? UMAX(0, (long)((current_time - terminal_time) / 60)) : 0;

            printf_to_char(ch, "  [{Y%d{x] %s\n\r",
                index,
                IS_NULLSTR(history->name) ? "(unknown quest)" : history->name);
            printf_to_char(ch, "       status:%s class:%s type:%s category:%s scope:%s\n\r",
                quest_run_status_name(history->run_status),
                quest_class_name(history->quest_class),
                quest_type_name(history->quest_type),
                quest_category_name(history->category),
                quest_target_scope_name(history->target_scope));
            if (terminal_time > 0)
                printf_to_char(ch, "       terminal:%ld minute%s ago\n\r",
                    age_minutes,
                    age_minutes == 1 ? "" : "s");

            shown++;
        }

        if (shown < 1)
            send_to_char("No history entries matched your filters.\n\r", ch);
        else
            send_to_char("Use {Yquest history <name>{x or {Yquest info <name>{x for details.\n\r", ch);

        return;
    }

    //
    // QUEST LIST
    //
    if (!str_cmp(arg1, "list"))
    {
        QUEST_INDEX_V2_DATA *v2_offerings[QUEST_LIST_MAX_ENTRIES];
        char target_name[MSL];
        int total_shown = 0;
        int v2_count, i;
        bool found_any_giver = false;
        CHAR_DATA *scan_ch;
        OBJ_DATA *scan_obj;

        if (!ch->in_room)
        {
            send_to_char("You can't do that here.\n\r", ch);
            return;
        }

        target_name[0] = '\0';
        if (!IS_NULLSTR(arg2)) {
            strncpy(target_name, arg2, sizeof(target_name) - 1);
            target_name[sizeof(target_name) - 1] = '\0';
            if (!IS_NULLSTR(argument)) {
                strncat(target_name, " ", sizeof(target_name) - strlen(target_name) - 1);
                strncat(target_name, argument, sizeof(target_name) - strlen(target_name) - 1);
            }
        }

        if (target_name[0])
        {
            /* Named target: find specific mob or obj in room */
            CHAR_DATA *mob = NULL;
            OBJ_DATA *obj = NULL;

            for (scan_ch = ch->in_room->people; scan_ch; scan_ch = scan_ch->next_in_room) {
                if (!IS_NPC(scan_ch) || !scan_ch->pIndexData) continue;
                if (is_name(target_name, scan_ch->name)) { mob = scan_ch; break; }
            }
            if (!mob) {
                for (scan_obj = ch->in_room->contents; scan_obj; scan_obj = scan_obj->next_content) {
                    if (!scan_obj->pIndexData) continue;
                    if (is_name(target_name, scan_obj->name)) { obj = scan_obj; break; }
                }
            }

            if (!mob && !obj) {
                send_to_char("No mob or object by that name found here.\n\r", ch);
                return;
            }

            if (mob) {
                v2_count = quest_collect_mob_v2_offerings(mob->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
                if (v2_count > 0) {
                    printf_to_char(ch, "Available quests from {Y%s{x:\n\r", HANDLE(mob));
                    for (i = 0; i < v2_count; i++) {
                        if (!v2_offerings[i]) continue;
                        quest_list_show_v2(ch, v2_offerings[i], &total_shown, false);
                    }
                    found_any_giver = true;
                }
            }
            if (obj) {
                v2_count = quest_collect_obj_v2_offerings(obj->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
                if (v2_count > 0) {
                    printf_to_char(ch, "Available quests from {Y%s{x:\n\r",
                        obj->short_descr ? obj->short_descr : "object");
                    for (i = 0; i < v2_count; i++) {
                        if (!v2_offerings[i]) continue;
                        quest_list_show_v2(ch, v2_offerings[i], &total_shown, false);
                    }
                    found_any_giver = true;
                }
            }
        }
        else
        {
            /* No arg: scan all mobs and objects in room */
            for (scan_ch = ch->in_room->people; scan_ch; scan_ch = scan_ch->next_in_room)
            {
                if (!IS_NPC(scan_ch) || !scan_ch->pIndexData) continue;
                v2_count = quest_collect_mob_v2_offerings(scan_ch->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
                if (v2_count < 1) continue;

                found_any_giver = true;
                printf_to_char(ch, "  {Y%s{x:\n\r", HANDLE(scan_ch));
                for (i = 0; i < v2_count; i++) {
                    if (!v2_offerings[i]) continue;
                    quest_list_show_v2(ch, v2_offerings[i], &total_shown, true);
                }
            }

            for (scan_obj = ch->in_room->contents; scan_obj; scan_obj = scan_obj->next_content)
            {
                if (!scan_obj->pIndexData) continue;
                v2_count = quest_collect_obj_v2_offerings(scan_obj->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
                if (v2_count < 1) continue;

                found_any_giver = true;
                printf_to_char(ch, "  {Y%s{x:\n\r",
                    scan_obj->short_descr ? scan_obj->short_descr : "object");
                for (i = 0; i < v2_count; i++) {
                    if (!v2_offerings[i]) continue;
                    quest_list_show_v2(ch, v2_offerings[i], &total_shown, true);
                }
            }
        }

        if (!found_any_giver || total_shown < 1) {
            send_to_char("No quests are available here.\n\r", ch);
            return;
        }

        send_to_char("Use {Yquest accept <name>{x to take a quest.\n\r", ch);
        return;
    }

    //
    // QUEST INSPECT (pre-accept details for a named v2 quest)
    //
    if (!str_cmp(arg1, "inspect"))
    {
        QUEST_INDEX_V2_DATA *v2_offerings[QUEST_LIST_MAX_ENTRIES];
        QUEST_INDEX_V2_DATA *selected = NULL;
        CHAR_DATA *scan_ch;
        OBJ_DATA *scan_obj;
        char quest_name[MSL];
        int v2_count;

        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: quest inspect <quest name>\n\r", ch);
            send_to_char("Use {Yquest list{x to see quests available here.\n\r", ch);
            return;
        }

        quest_name[0] = '\0';
        strncpy(quest_name, arg2, sizeof(quest_name) - 1);
        quest_name[sizeof(quest_name) - 1] = '\0';
        if (!IS_NULLSTR(argument)) {
            strncat(quest_name, " ", sizeof(quest_name) - strlen(quest_name) - 1);
            strncat(quest_name, argument, sizeof(quest_name) - strlen(quest_name) - 1);
        }

        if (!ch->in_room) {
            send_to_char("You can't do that here.\n\r", ch);
            return;
        }

        for (scan_ch = ch->in_room->people; scan_ch && !selected; scan_ch = scan_ch->next_in_room) {
            if (!IS_NPC(scan_ch) || !scan_ch->pIndexData) continue;
            v2_count = quest_collect_mob_v2_offerings(scan_ch->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
            selected = quest_find_v2_offering_by_name(v2_offerings, v2_count, quest_name);
        }

        for (scan_obj = ch->in_room->contents; scan_obj && !selected; scan_obj = scan_obj->next_content) {
            if (!scan_obj->pIndexData) continue;
            v2_count = quest_collect_obj_v2_offerings(scan_obj->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
            selected = quest_find_v2_offering_by_name(v2_offerings, v2_count, quest_name);
        }

        if (!selected) {
            send_to_char("No available quest by that name here. Use {Yquest list{x first.\n\r", ch);
            return;
        }

        quest_inspect_show_v2(ch, selected);
        return;
    }

    //
    // QUEST ACCEPT (player-initiated v2 quest)
    //
    if (!str_cmp(arg1, "accept"))
    {
        QUEST_INDEX_V2_DATA *v2_offerings[QUEST_LIST_MAX_ENTRIES];
        QUEST_INDEX_V2_DATA *selected = NULL;
        QUEST_DATA *active_quest;
        CHAR_DATA *scan_ch;
        OBJ_DATA *scan_obj;
        char quest_name[MSL];
        WNUM wnum;
        REQUIREMENT_CONTEXT prereq_ctx;
        int v2_count;

        if (IS_NULLSTR(arg2))
        {
            send_to_char("Syntax: quest accept <quest name>\n\r", ch);
            send_to_char("Use {Yquest list{x to see quests available here.\n\r", ch);
            return;
        }

        quest_name[0] = '\0';
        strncpy(quest_name, arg2, sizeof(quest_name) - 1);
        quest_name[sizeof(quest_name) - 1] = '\0';
        if (!IS_NULLSTR(argument)) {
            strncat(quest_name, " ", sizeof(quest_name) - strlen(quest_name) - 1);
            strncat(quest_name, argument, sizeof(quest_name) - strlen(quest_name) - 1);
        }

        if (!ch->in_room)
        {
            send_to_char("You can't do that here.\n\r", ch);
            return;
        }

        /* Scan mobs in room for the named v2 quest */
        for (scan_ch = ch->in_room->people; scan_ch && !selected; scan_ch = scan_ch->next_in_room) {
            if (!IS_NPC(scan_ch) || !scan_ch->pIndexData) continue;
            v2_count = quest_collect_mob_v2_offerings(scan_ch->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
            selected = quest_find_v2_offering_by_name(v2_offerings, v2_count, quest_name);
        }

        /* Scan objects in room for the named v2 quest */
        for (scan_obj = ch->in_room->contents; scan_obj && !selected; scan_obj = scan_obj->next_content) {
            if (!scan_obj->pIndexData) continue;
            v2_count = quest_collect_obj_v2_offerings(scan_obj->pIndexData, v2_offerings, QUEST_LIST_MAX_ENTRIES);
            selected = quest_find_v2_offering_by_name(v2_offerings, v2_count, quest_name);
        }

        if (!selected)
        {
            send_to_char("No available quest by that name here. Use {Yquest list{x first.\n\r", ch);
            return;
        }

        if (!IS_AWAKE(ch))
        {
            send_to_char("In your dreams, or what?\n\r", ch);
            return;
        }

        if (IS_DEAD(ch))
        {
            send_to_char("You must come back to the world of the living first.\n\r", ch);
            return;
        }

        if (!IS_IMMORTAL(ch) && selected->allowance_cost > 0
        &&  game_settings.telnet_port != PORT_RAE
        &&  ch->quest_runtime.mission_allowance < selected->allowance_cost)
        {
            printf_to_char(ch, "You need at least %d mission allowance%s to accept that quest.\n\r",
                selected->allowance_cost,
                selected->allowance_cost == 1 ? "" : "s");
            return;
        }

        active_quest = new_quest();
        active_quest->next = ch->quest;
        ch->quest = active_quest;

        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_attach_active_quest(ch, 0, 0);
        active_quest = quest_runtime_get_focused_run(ch);

        if (!active_quest)
        {
            send_to_char("Quest accept failed: unable to initialize runtime state.\n\r", ch);
            return;
        }

        active_quest->quest_index_auid = 0;
        active_quest->quest_index_vnum = 0;
        active_quest->target_scope = selected->target_scope;
        active_quest->scope_owner_id[0] = 0;
        active_quest->scope_owner_id[1] = 0;
        active_quest->scope_owner_uid = 0;
        quest_scope_owner_seed(ch, active_quest);

        /* Prerequisites check */
        if (!IS_NULLSTR(selected->prerequisites)) {
            prereq_ctx.actor      = ch;
            prereq_ctx.self_mob   = NULL;
            prereq_ctx.self_obj   = NULL;
            prereq_ctx.self_room  = NULL;
            prereq_ctx.self_token = NULL;
            prereq_ctx.self_quest = selected;
            if (!requirements_evaluate_text(selected->prerequisites, &prereq_ctx, true)) {
                quest_runtime_detach_run(ch, active_quest);
                send_to_char("You do not meet the prerequisites for that quest.\n\r", ch);
                return;
            }
        }

        wnum.pArea = selected->area;
        wnum.vnum = selected->vnum;
        if (!quest_runtime_bind_index_v2(active_quest, wnum)) {
            quest_runtime_detach_run(ch, active_quest);
            send_to_char("Quest accept failed: unable to bind quest data.\n\r", ch);
            return;
        }

        if (!IS_IMMORTAL(ch) && selected->allowance_cost > 0)
            ch->quest_runtime.mission_allowance -= selected->allowance_cost;

        quest_runtime_fire_quest_lifecycle_trigger_actor(active_quest, TRIG_QUEST_ACCEPTED, "player_accept", ch);
        quest_runtime_fire_quest_lifecycle_trigger_actor(active_quest, TRIG_QUEST_FOCUSED, "player_accept", ch);
        quest_runtime_try_advance_stage(active_quest);

        if (selected->quest_class != QUEST_CLASS_MISSION
            && active_quest->run_status == QUEST_RUN_STATUS_ACTIVE
            && active_quest->current_stage_commenced == 0)
        {
            (void)quest_runtime_commence_current_stage(active_quest);
        }

        printf_to_char(ch, "You have accepted the quest: {Y%s{x.\n\r",
            IS_NULLSTR(selected->name) ? "(unnamed quest)" : selected->name);
        if (selected->quest_class != QUEST_CLASS_MISSION)
            send_to_char("This quest begins immediately.\n\r", ch);
        return;
    }

    //
    // QUEST FOCUS
    //
    if (!str_cmp(arg1, "focus"))
    {
        QUEST_INDEX_V2_DATA *quest_index_v2;
        const char *quest_name;

        if (!IS_QUESTING(ch))
        {
            send_to_char("You have no active quests to focus.\n\r", ch);
            return;
        }

        if (IS_NULLSTR(arg2))
        {
            if (ch->quest_runtime.focused_run_id <= 0)
                send_to_char("No focused quest set.\n\r", ch);
            else {
                QUEST_DATA *focused_run = quest_runtime_get_focused_run(ch);
                if (focused_run && quest_run_accessible_by_player(ch, focused_run, false))
                {
                    quest_name = quest_run_display_name(focused_run);
                    if (IS_NULLSTR(quest_name))
                        quest_name = "(unnamed quest)";

                    printf_to_char(ch, "Focused quest: %s [%s]\n\r",
                        quest_name,
                        quest_target_scope_name(focused_run->target_scope));
                }
                else
                    send_to_char("No focused quest set.\n\r", ch);
            }
            return;
        }

        strncpy(target_arg, arg2, sizeof(target_arg) - 1);
        target_arg[sizeof(target_arg) - 1] = '\0';
        if (!IS_NULLSTR(argument))
        {
            strncat(target_arg, " ", sizeof(target_arg) - strlen(target_arg) - 1);
            strncat(target_arg, argument, sizeof(target_arg) - strlen(target_arg) - 1);
        }

        run = quest_resolve_command_run(ch, target_arg, true);
        if (!run)
            return;

        ch->quest_runtime.focused_run_id = run->run_id;
        quest_runtime_fire_quest_lifecycle_trigger_actor(run, TRIG_QUEST_FOCUSED, "manual", ch);
        quest_index_v2 = quest_runtime_get_index_v2(run);
        quest_name = quest_index_v2 && !IS_NULLSTR(quest_index_v2->name)
            ? quest_index_v2->name
            : quest_run_display_name(run);

        if (run->target_scope == QUEST_TARGET_SCOPE_GROUP
            && run->run_status == QUEST_RUN_STATUS_ACTIVE
            && !run->generating)
        {
            quest_runtime_sync_group_cluster(ch, run, true);
        }

        printf_to_char(ch, "Focused quest: {Y%s{x\n\r",
            IS_NULLSTR(quest_name) ? "(unnamed quest)" : quest_name);
        return;
    }

    //
    // QUEST SYNC
    //
    if (!str_cmp(arg1, "sync"))
    {
        QUEST_DATA *source_run;
        int synced;

        target_arg[0] = '\0';
        if (!IS_NULLSTR(arg2))
        {
            strncpy(target_arg, arg2, sizeof(target_arg) - 1);
            target_arg[sizeof(target_arg) - 1] = '\0';
            if (!IS_NULLSTR(argument))
            {
                strncat(target_arg, " ", sizeof(target_arg) - strlen(target_arg) - 1);
                strncat(target_arg, argument, sizeof(target_arg) - strlen(target_arg) - 1);
            }
        }

        source_run = quest_resolve_command_run(ch, target_arg[0] ? target_arg : NULL, true);
        if (!source_run)
            return;

        if (source_run->run_status != QUEST_RUN_STATUS_ACTIVE || source_run->generating)
        {
            send_to_char("Only active, non-pending quest runs can be synchronized.\n\r", ch);
            return;
        }

        if (source_run->target_scope != QUEST_TARGET_SCOPE_GROUP)
        {
            send_to_char("Quest sync currently supports only group-scoped runs.\n\r", ch);
            return;
        }

        synced = quest_runtime_sync_group_cluster(ch, source_run, true);
        if (synced > 0)
            printf_to_char(ch, "Synchronized %d matching group run%s for this quest template.\n\r",
                synced,
                synced == 1 ? "" : "s");
        else
            send_to_char("No matching group quest runs needed synchronization.\n\r", ch);

        return;
    }

    //
    // Quest commence
    //
    if (!str_cmp(arg1, "commence"))
    {
        QUEST_DATA *focused_quest;
        QUEST_PART_DATA *part;
        bool stage_already_commenced;
        int i;
        int task_index = -1;

        focused_quest = quest_runtime_get_focused_run(ch);

        if (focused_quest == NULL || focused_quest->generating)
        {
            send_to_char("You are not on a quest.\n\r", ch);
            return;
        }

        if (!quest_run_accessible_by_player(ch, focused_quest, true))
            return;

        if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_MANUAL_AREA)
            && !quest_runtime_manual_trigger_ready(ch))
        {
            send_to_char("You are not at the objective location yet.\n\r", ch);
            return;
        }

        if (focused_quest->quest_index_v2_vnum > 0)
        {
            stage_already_commenced = (focused_quest->current_stage_commenced != 0);
            if (!quest_runtime_commence_current_stage(focused_quest))
            {
                printf_to_char(ch, "That quest stage cannot be commenced right now (%s).\n\r",
                    quest_runtime_commence_blocker(focused_quest));
                send_to_char("Use 'quest info' to review stage status and objective setup.\n\r", ch);
                return;
            }

            if (IS_NULLSTR(arg2))
            {
                if (stage_already_commenced)
                    send_to_char("Current quest stage is already commenced.\n\r", ch);
                else
                    send_to_char("Current quest stage commenced.\n\r", ch);
                return;
            }
        }

        if (IS_NULLSTR(arg2))
        {
            i = 0;
            for (part = focused_quest->parts; part != NULL; part = part->next)
            {
                i++;
                if (part->complete || !part->custom_task)
                    continue;

                task_index = i;
                break;
            }

            if (task_index < 1)
            {
                send_to_char("There is no pending custom objective to commence.\n\r", ch);
                return;
            }
        }
        else
        {
            strncpy(target_arg, arg2, sizeof(target_arg) - 1);
            target_arg[sizeof(target_arg) - 1] = '\0';
            if (!IS_NULLSTR(argument))
            {
                strncat(target_arg, " ", sizeof(target_arg) - strlen(target_arg) - 1);
                strncat(target_arg, argument, sizeof(target_arg) - strlen(target_arg) - 1);
            }

            if (is_number(target_arg))
            {
                task_index = atoi(target_arg);
            }
            else
            {
                i = 0;
                for (part = focused_quest->parts; part != NULL; part = part->next)
                {
                    i++;
                    if (part->complete || !part->custom_task)
                        continue;

                    if (!str_infix(target_arg, part->description))
                    {
                        task_index = i;
                        break;
                    }
                }
            }
        }

        if (task_index < 1)
        {
            send_to_char("No matching objective found. Use 'quest info' to view objective indexes.\n\r", ch);
            return;
        }

        if (!check_quest_custom_task_run(ch, focused_quest, task_index, true))
        {
            send_to_char("That objective cannot be commenced right now.\n\r", ch);
            return;
        }

        return;
    }

    //
    // QUEST INFO / DETAILS
    //
    if (!str_cmp(arg1, "info") || !str_cmp(arg1, "details"))
    {
        QUEST_PART_DATA *part;
        QUEST_DATA *focused_quest;
        QUEST_INDEX_V2_DATA *quest_index_v2;
        QUEST_STAGE_INDEX_V2_DATA *stage;
        QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
        long age_minutes = 0;
        int i;
        int total_parts;
        bool totally_complete = false;
        bool found = false;

        target_arg[0] = '\0';
        if (!IS_NULLSTR(arg2))
        {
            strncpy(target_arg, arg2, sizeof(target_arg) - 1);
            target_arg[sizeof(target_arg) - 1] = '\0';
            if (!IS_NULLSTR(argument))
            {
                strncat(target_arg, " ", sizeof(target_arg) - strlen(target_arg) - 1);
                strncat(target_arg, argument, sizeof(target_arg) - strlen(target_arg) - 1);
            }

            focused_quest = quest_resolve_command_run(ch, target_arg, false);
            if (!focused_quest)
            {
                QUEST_HISTORY_DATA *history = quest_history_find_by_name(ch, target_arg);
                if (!history)
                {
                    send_to_char("No active or historical quest matches that name (or the name is ambiguous).\n\r", ch);
                    return;
                }

                quest_show_history_entry(ch, history);
                return;
            }

            ch->quest_runtime.focused_run_id = focused_quest->run_id;
        }
        else
        {
            focused_quest = quest_runtime_get_focused_run(ch);

            if (ch->quest_runtime.focused_run_id <= 0)
            {
                ch->quest_runtime.focused_run_id = active_run_id;
                focused_quest = quest_runtime_get_focused_run(ch);
            }

            if (focused_quest == NULL)
            {
                send_to_char("No focused quest. Use 'quest focus <index|name>' or 'quest info <index|name>'.\n\r", ch);
                return;
            }
        }

        if (!quest_run_accessible_by_player(ch, focused_quest, true))
            return;

        total_parts = 0;

        if (focused_quest == NULL)
        {
            send_to_char("You are not on a quest.\n\r", ch);
            return;
        }

        if (focused_quest->generating)
        {
            send_to_char("You are still waiting for your quest.\n\r",ch);
            return;
        }

        if (focused_quest->quest_index_v2_vnum > 0)
        {
            bool any_incomplete = false;

            quest_index_v2 = quest_runtime_get_index_v2(focused_quest);
            stage = quest_runtime_get_current_stage(focused_quest);

            if (!quest_index_v2)
            {
                send_to_char("Quest index data is unavailable for this run.\n\r", ch);
                return;
            }

            if (IS_STAFF(ch, STAFF_GIMP))
            {
                if (quest_index_v2->area && quest_index_v2->area->uid > 0)
                    printf_to_char(ch, "Quest: {Y%s{x ({Y%ld#%ld{x)\n\r",
                        IS_NULLSTR(quest_index_v2->name) ? "(unnamed quest)" : quest_index_v2->name,
                        quest_index_v2->area->uid,
                        quest_index_v2->vnum);
                else
                    printf_to_char(ch, "Quest: {Y%s{x ({Y%ld{x)\n\r",
                        IS_NULLSTR(quest_index_v2->name) ? "(unnamed quest)" : quest_index_v2->name,
                        quest_index_v2->vnum);
            }
            else
            {
                printf_to_char(ch, "Quest: {Y%s{x\n\r",
                    IS_NULLSTR(quest_index_v2->name) ? "(unnamed quest)" : quest_index_v2->name);
            }

            if (focused_quest->started_at > 0)
                age_minutes = UMAX(0, (long)((current_time - focused_quest->started_at) / 60));
            else
                age_minutes = 0;
            printf_to_char(ch, "Started: %ld minute%s ago\n\r",
                age_minutes,
                age_minutes == 1 ? "" : "s");

            if (!IS_NULLSTR(quest_index_v2->description))
                printf_to_char(ch, "Description: %s\n\r", quest_index_v2->description);

            if (!stage)
            {
                send_to_char("Current stage: (none)\n\r", ch);
                return;
            }

            printf_to_char(ch, "Current stage {Y%d{x: %s\n\r",
                stage->id,
                IS_NULLSTR(stage->name) ? "(unnamed stage)" : stage->name);
            printf_to_char(ch, "Stage status: %s\n\r",
                focused_quest->current_stage_commenced ? "commenced" : "not commenced");
            if (!IS_NULLSTR(stage->description))
                printf_to_char(ch, "Stage description: %s\n\r", stage->description);

            if (!stage->objectives)
            {
                send_to_char("No objectives on the current stage.\n\r", ch);
                return;
            }

            for (objective = stage->objectives; objective != NULL; objective = objective->next)
            {
                QUEST_OBJECTIVE_STATE_V2_DATA *state;
                int required;
                int progress;
                bool objective_complete;
                bool objective_failed;
                bool objective_pending;
                char indicator_buf[128];

                state = quest_runtime_get_objective_state(focused_quest, objective->id, false);
                required = quest_objective_required_display_count(objective);
                progress = state ? state->progress : 0;
                objective_complete = (state && state->complete);
                objective_failed = (!objective_complete)
                    && objective->optional
                    && focused_quest->run_status == QUEST_RUN_STATUS_FAILED;
                objective_pending = (!objective_complete
                    && !objective_failed
                    && !focused_quest->current_stage_commenced);

                if (!objective_complete && focused_quest->run_status == QUEST_RUN_STATUS_ACTIVE)
                    any_incomplete = true;

                indicator_buf[0] = '\0';
                if (objective->optional)
                    strncat(indicator_buf, " {C[optional]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                if (objective_complete)
                    strncat(indicator_buf, " {G[completed]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                else if (objective_failed)
                    strncat(indicator_buf, " {R[failed]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);
                else if (objective_pending)
                    strncat(indicator_buf, " {Y[pending]{x", sizeof(indicator_buf) - strlen(indicator_buf) - 1);

                printf_to_char(ch, "  [{Y%d{x] %s {D(%s){x %d/%d%s\n\r",
                    objective->id,
                    quest_objective_visible_label(objective),
                    quest_objective_type_name(objective->objective_type),
                    progress,
                    required,
                    indicator_buf);
                {
                    char target_buf[MSL];
                    printf_to_char(ch, "       %s\n\r",
                        quest_objective_target_summary(focused_quest, objective, target_buf, sizeof(target_buf),
                            IS_STAFF(ch, STAFF_GIMP)));
                    quest_objective_target_options_display(ch, focused_quest, objective,
                        IS_STAFF(ch, STAFF_GIMP));
                }
            }

            if (!focused_quest->current_stage_commenced)
                send_to_char("Use {Yquest commence{x to begin this stage.\n\r", ch);

            if (!any_incomplete && focused_quest->run_status == QUEST_RUN_STATUS_ACTIVE)
            {
                send_to_char("{YCurrent stage objectives are complete.{x\n\r", ch);
                send_to_char("Use {Yquest complete{x when ready to turn in if the run is finished.\n\r", ch);
            }

            return;
        }

        part = focused_quest->parts;
        while(part != NULL)
        {
            if (!part->complete)
                found = true;
            total_parts++;
            part = part->next;
        }

        if (!found)
            totally_complete = true;

        i = 1;
        for (part = focused_quest->parts; part != NULL; part = part->next, i++)
        {
            if (part->complete)
            {
                sprintf(buf, "You have completed task {Y%d{x: %s\n\r", i,
                    IS_NULLSTR(part->description) ? "(no description)" : part->description);
                send_to_char(buf, ch);
            }
            else
            {
                sprintf(buf, "Task {Y%d{x is not complete: %s\n\r", i,
                    IS_NULLSTR(part->description) ? "(no description)" : part->description);
                totally_complete = false;
                send_to_char(buf, ch);

                if (part->custom_task)
                {
                    sprintf(buf, "  -> Use {Yquest commence %d{x here when ready.\n\r", i);
                    send_to_char(buf, ch);
                }
            }
        }

        if (totally_complete)
        {
            send_to_char("{YYour quest is complete!{x\n\r"
                "Turn quest in before your time runs out!\n\r", ch);
        }
        return;
    }


    //
    // QUEST POINTS
    //
    if (!str_cmp(arg1, "points"))
    {
        sprintf(buf, "You have {Y%d{x quest points.\n\r", ch->questpoints);
        send_to_char(buf, ch);
        return;
    }


    //
    // Quest time
    //
    if (!str_cmp(arg1, "time"))
    {
        QUEST_DATA *focused_quest = quest_runtime_get_focused_run(ch);
        int active_count = 0;
        int other_count;
        int countdown_remaining = 0;

        for (run = ch->quest; run != NULL; run = run->next)
        {
            if (quest_run_accessible_by_player(ch, run, false))
                active_count++;
        }

        if (!IS_QUESTING(ch))
        {
            sprintf(buf, "Mission allowances available: {Y%d{x\n\r",
                UMAX(0, ch->quest_runtime.mission_allowance));
            send_to_char(buf, ch);
            send_to_char("You aren't currently on a quest.\n\r", ch);
        }
        else if (focused_quest && focused_quest->generating)
        {
            other_count = UMAX(0, active_count - 1);
            sprintf(buf, "Focused run: {Y%ld{x [%s] (%d other active run%s).\n\r",
                focused_quest->run_id,
                quest_target_scope_name(focused_quest->target_scope),
                other_count,
                other_count == 1 ? "" : "s");
            send_to_char(buf, ch);
            send_to_char("You are still waiting for your quest.\n\r"
                "If you wish to abandon the pending quest, use QUEST CANCEL.\n\r",ch);
        }
        else if (focused_quest && !quest_run_accessible_by_player(ch, focused_quest, true))
        {
            return;
        }
        else
        {
            if (ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
                countdown_remaining = UMAX(0, ch->quest_runtime.expiry_countdown_minutes);
            else if (ch->countdown > 0)
                countdown_remaining = ch->countdown;
        }

        if (countdown_remaining > 0)
        {
            other_count = UMAX(0, active_count - 1);
            sprintf(buf, "Focused run: {Y%ld{x [%s] (%d other active run%s).\n\r",
                focused_quest ? focused_quest->run_id : 0,
                focused_quest ? quest_target_scope_name(focused_quest->target_scope) : "unknown",
                other_count,
                other_count == 1 ? "" : "s");
            send_to_char(buf, ch);
            sprintf(buf, "Time left for current quest: {Y%d{x minutes.\n\r",
                countdown_remaining);
            send_to_char(buf, ch);
        }
        else if (focused_quest)
        {
            other_count = UMAX(0, active_count - 1);
            sprintf(buf, "Focused run: {Y%ld{x [%s] (%d other active run%s).\n\r",
                focused_quest->run_id,
                quest_target_scope_name(focused_quest->target_scope),
                other_count,
                other_count == 1 ? "" : "s");
            send_to_char(buf, ch);
            send_to_char("No countdown timer is currently active for this run.\n\r", ch);
        }
        return;
    }

    if (!str_cmp(arg1, "request"))
    {
        send_to_char("{Yquest request{x has been retired. Use {Yquest list{x and {Yquest accept <name>{x.\n\r", ch);
        return;
    }

    //
    // Quest cancel
    //
    if (!str_cmp(arg1, "cancel"))
    {
        QUEST_DATA *active_quest;

        if (!IS_AWAKE(ch))
        {
            send_to_char("In your dreams, or what?\n\r", ch);
            return;
        }

        if( ch->quest == NULL )
        {
            send_to_char("You are not on a quest.\n\r", ch);
            return;
        }

        active_quest = quest_resolve_command_run(ch, arg2, true);
        if (!active_quest)
            return;

        ch->quest_runtime.focused_run_id = active_quest->run_id;

        if( active_quest->generating )
        {
            quest_runtime_mark_run_failed(active_quest, QUEST_RUN_STATUS_ABANDONED, "cancelled");
            quest_runtime_detach_run(ch, active_quest);
            if (!IS_QUESTING(ch)) {
                quest_runtime_reset_expiration(ch);
            }
            send_to_char("Pending quest cancelled.\n\r", ch);
        }
        else
        {
            // Check for the questGIVER
            switch(active_quest->questgiver_type)
            {
            case QUESTOR_MOB:
                for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
                {
                if (IS_NPC(mob) && wnum_match_mob(active_quest->questgiver_wnum, mob))
                        break;
                }
                break;

            case QUESTOR_OBJ:
                // Check inventory using lcarrying
                if (ch->lcarrying) {
                    iterator_start(&it, ch->lcarrying);
                    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                        if (wnum_match_obj(active_quest->questgiver_wnum, obj)) {
                            iterator_stop(&it);
                            break;
                        }
                    }
                    iterator_stop(&it);
                }
                
                // If not found in inventory, check room contents
                if (obj == NULL) {
                    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content) {
                        if (wnum_match_obj(active_quest->questgiver_wnum, obj))
                            break;
                    }
                }
                break;

            case QUESTOR_ROOM:
                if (!ch->in_room->wilds && !ch->in_room->source &&
                    wnum_match_room(active_quest->questgiver_wnum, ch->in_room)) {
                    room = ch->in_room;
                    break;
                }
                break;
            }

            if (mob)
            {
                // Mobs will complain
                act("$n informs $N $e has cancelled $s quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                act("You inform $N you have cancelled your quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                sprintf(buf,
                    "I am most displeased with your efforts, %s! This is "
                    "obviously a job for someone with more talent than you.",
                    ch->name);
                do_say(mob, buf);

                quest_runtime_mark_run_failed(active_quest, QUEST_RUN_STATUS_ABANDONED, "cancelled");
                quest_runtime_detach_run(ch, active_quest);

                mob->tempstore[0] = 10;
                p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    quest_runtime_reset_expiration(ch);
                }
            }
            else if (obj)
            {
                // Objects will not complain by default

                quest_runtime_mark_run_failed(active_quest, QUEST_RUN_STATUS_ABANDONED, "cancelled");
                quest_runtime_detach_run(ch, active_quest);

                obj->tempstore[0] = 10;
                p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    quest_runtime_reset_expiration(ch);
                }
            }
            else if (room)
            {
                quest_runtime_mark_run_failed(active_quest, QUEST_RUN_STATUS_ABANDONED, "cancelled");
                quest_runtime_detach_run(ch, active_quest);

                room->tempstore[0] = 10;
                p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    quest_runtime_reset_expiration(ch);
                }
            }
            else
            {
                send_to_char("You can't do that here\n\r", ch);
            }
        }

        return;
    }

    //
    // Quest complete
    //
    if (!str_cmp(arg1, "complete"))
    {
        QUEST_DATA *active_quest;
        QUEST_PART_DATA *part;
        bool found;
        bool incomplete;
        int reward;
        int pointreward;
        int pracreward;
        int expreward;
        int i;
        int *tempstores = NULL;

        if (!IS_AWAKE(ch))
        {
            send_to_char("In your dreams, or what?\n\r", ch);
            return;
        }

        active_quest = quest_resolve_command_run(ch, arg2, true);
        if (!active_quest)
            return;

        ch->quest_runtime.focused_run_id = active_quest->run_id;

        if (active_quest == NULL || active_quest->generating)
        {
            send_to_char("You are not on a quest.\n\r", ch);
            return;
        }

        // Check for the questRECEIVER
        switch(active_quest->questreceiver_type)
        {
        case QUESTOR_MOB:
            for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
            {
                if (IS_NPC(mob) && wnum_match_mob(active_quest->questreceiver_wnum, mob))
                {
                    tempstores = mob->tempstore;
                    break;
                }
            }
            break;

        case QUESTOR_OBJ:
            // Check inventory using lcarrying
            if (ch->lcarrying) {
                iterator_start(&it, ch->lcarrying);
                while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                    if (wnum_match_obj(active_quest->questreceiver_wnum, obj)) {
                        tempstores = obj->tempstore;
                        iterator_stop(&it);
                        break;
                    }
                }
                iterator_stop(&it);
            }
            
            // If not found in inventory, check room contents
            if (obj == NULL) {
                for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content) {
                    if (wnum_match_obj(active_quest->questreceiver_wnum, obj)) {
                        tempstores = obj->tempstore;
                        break;
                    }
                }
            }
            break;

        case QUESTOR_ROOM:
            if (!ch->in_room->wilds && !ch->in_room->source &&
                wnum_match_room(active_quest->questreceiver_wnum, ch->in_room)) {
                room = ch->in_room;
                tempstores = room->tempstore;
            }
            break;
        }

        if (!mob && !obj && !room)
        {
            send_to_char("You can't do that here\n\r", ch);
            return;
        }

        if (tempstores == NULL)
        {
            send_to_char("You can't do that here\n\r", ch);
            return;
        }

        if (mob)
        {
            act("$n informs $N $e $z completed $s quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, get_verb_form(ch, "has", "have"), NULL);
            act("You inform $N you have completed your quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }

        found = false;
        incomplete = false;
        for (part = active_quest->parts; part != NULL; part = part->next)
        {
            if (part->complete)
                found = true;
            if (!part->complete)
                incomplete = true;
        }

        if (!found)
        {
            if (mob) {
                sprintf(buf,
                    "I am most displeased with your efforts, %s! This is "
                    "obviously a job for someone with more talent than you.",
                    ch->name);
                do_say(mob, buf);
            }

            p_percent_trigger(mob, obj, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);

            quest_runtime_detach_run(ch, active_quest);

            tempstores[0] = 10;
            p_percent_trigger(mob, obj, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_POSTQUEST, NULL);

            if (!IS_QUESTING(ch)) {
                quest_runtime_reset_expiration(ch);
            }

            return;
        }

        pointreward = 0;
        reward = 0;
        pracreward = 0;
        expreward = 0;
        i = 0;

        plogf(LOG_QUEST, "(complete) Checking quest parts...");

        // Add up all the different rewards.
        for (part = active_quest->parts; part != NULL; part = part->next)
        {
            i++;

            if (part->complete)
            {
                reward += number_range(500, 1000);
                pointreward += number_range(10, 20);
                expreward += number_range(ch->tot_level * 50,
                                         ch->tot_level * 100);
                pracreward += 1;

                if (ch->pcdata->second_sub_class_warrior == CLASS_WARRIOR_CRUSADER)
                {
                    pointreward += 5;
                    if (number_percent() < 10)
                    {
                        pracreward += number_range(0, 1);
                    }

                    expreward += number_range(1000, 5000);
                }
            }

            // If object, return the object.
            if (part->pObj != NULL)
            {
                if (ch == part->pObj->carried_by)
                {
                    if (mob)
                    {
                        act("You hand $p to $N.", ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        act("$n hands $p to $N.", ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    }

                    extract_obj(part->pObj);

                    part->pObj = NULL;
                }
            }
        }

        if (!incomplete)
        {
            if (mob)
            {
                sprintf(buf, "Congratulations on completing your quest!");
                do_say(mob, buf);
            }
            active_quest->run_status = QUEST_RUN_STATUS_COMPLETED;
            active_quest->completed_at = current_time;
            active_quest->failed_at = 0;
            active_quest->abandoned_at = 0;
            quest_runtime_record_terminal_history(active_quest);
        }
        else
        {
            if (mob)
            {
                sprintf(buf, "I see you haven't fully completed your quest, "
                            "but I applaud your courage anyway!");
                do_say(mob, buf);
            }
            pracreward -= number_range(2, 5);
            pointreward -= number_range(10, 20);
            pracreward = UMAX(pracreward, 0);
            pointreward = UMAX(pointreward, 0);

            active_quest->run_status = QUEST_RUN_STATUS_FAILED;
            active_quest->completed_at = 0;
            active_quest->failed_at = current_time;
            active_quest->abandoned_at = 0;
            quest_runtime_record_terminal_history(active_quest);
        }

        tempstores[0] = expreward;			// Experience
        tempstores[1] = pointreward;		// QP
        tempstores[2] = pracreward;			// Practices
        tempstores[3] = reward;				// Silver
        if (incomplete)
            p_percent_trigger(mob, obj, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);
        else
            p_percent_trigger(mob, obj, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_COMPLETE, NULL);
        expreward = tempstores[0];
        pointreward = tempstores[1];
        pracreward = tempstores[2];
        reward = tempstores[3];

        // Clamp to zero
        expreward = UMAX(expreward, 0);
        reward = UMAX(reward, 0);
        pracreward = UMAX(pracreward, 0);
        pointreward = UMAX(pointreward, 0);

        if (boost_table[BOOST_QP].boost != 100)
            pointreward = (pointreward * boost_table[BOOST_QP].boost) / 100;


        if (mob) {
            sprintf(buf, "As a reward, I am giving you %d quest points and %d silver.",
                    pointreward, reward);
            do_say(mob, buf);
        }
        else
        {
            sprintf(buf, "As a reward, you receive %d quest points and %d silver.\n\r", pointreward, reward);
            send_to_char(buf, ch);
        }

        // Only display "QUEST POINTS boost!" if a qp boost is active -- Areo
        if (boost_table[BOOST_QP].boost != 100)
            send_to_char("{WQUEST POINTS boost!{x\n\r", ch);

        ch->silver += reward;
        ch->questpoints += pointreward;

        if (number_percent() < 90 && pracreward > 0)
        {
            sprintf(buf, "You gain %d practices!\n\r", pracreward);
            send_to_char(buf, ch);
            ch->practice += pracreward;
        }
        else { /* AO don't nerf it completely */
            pracreward /= number_range(1, 4);
            pracreward = UMAX(1, pracreward);

            sprintf(buf, "You gain %d practices!\n\r", pracreward);
            send_to_char(buf, ch);
            ch->practice += pracreward;
        }

        if (ch->tot_level < 120)
        {
            //sprintf(buf, "You gain %d experience points!\n\r", expreward);
            //send_to_char(buf, ch);

            gain_exp(ch, NULL, expreward, true);
        }
/* Syn - disabling
  send_to_char("You receive 1 military quest point!\n\r", ch);
  award_ship_quest_points(ch->in_room->area->place_flags, ch, 1);
*/

        tempstores[0] = 10;
        p_percent_trigger(mob, obj, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_POSTQUEST, NULL);

        quest_runtime_detach_run(ch, active_quest);
        if (!IS_QUESTING(ch)) {
            quest_runtime_reset_expiration(ch);
        }
    }
    else
    {
        send_to_char("QUEST commands: LOG HISTORY LIST INSPECT ACCEPT FOCUS SYNC POINTS INFO DETAILS TIME COMMENCE CANCEL COMPLETE.\n\r", ch);
        send_to_char("For more information, type 'HELP QUEST'.\n\r", ch);
    }
}


/*
 * Generate a quest. Returns true if a quest is found.
 */
bool generate_quest(CHAR_DATA *ch, CHAR_DATA *questman)
{
    QUEST_DATA *active_run;
    QUEST_PART_DATA *part;
    OBJ_DATA *scroll;
    int parts;
    int i;

    active_run = quest_runtime_get_focused_run(ch);
    if (!active_run)
        return false;

    active_run->generating = true;
    active_run->scripted = false;

    if (ch->tot_level <= 30)
        parts = number_range(1, 3);
    else if (ch->tot_level <= 60)
        parts = number_range(3, 6);
    else if (ch->tot_level <= 90)
        parts = number_range(7, 9);
    else
        parts = number_range(8, 15);

    /* fun */
    bool bFun = number_percent() < 5;
    if (bFun)
        parts = parts * 2;

    QUESTOR_DATA *qd = questman->pIndexData->pQuestor;

    // MORE FUN
    questman->tempstore[0] = parts;				// Number of parts to do (In-Out)
    questman->tempstore[1] = bFun ? 1 : 0;		// Whether this was a F.U.N. quest (In)
    questman->tempstore[2] = qd->scroll;		// Default quest scroll item
    if(p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_PREQUEST, NULL))
        return false;
    parts = questman->tempstore[0];				// Updated number of parts to do
    if( parts < 1 ) parts = 1;					//    Require at least one part.

    long scroll_vnum = questman->tempstore[2];	// Get value back
    if( scroll_vnum < 1 )
        scroll_vnum = qd->scroll;

    for (i = 0; i < parts; i++)
    {
        part = new_quest_part();
        part->next = active_run->parts;
        active_run->parts = part;
        part->index = parts - i;

        if (generate_quest_part(ch, questman, part, parts - i))
            continue;
        else
            return false;
    }

    // create the scroll
    scroll = generate_quest_scroll(ch, active_run, questman->short_descr, scroll_vnum,
        qd->header, qd->footer, qd->prefix, qd->suffix, qd->line_width);

    if( scroll == NULL )
    {
        // COMPLAIN
        return false;
    }

    free_string(scroll->name);
    free_string(scroll->short_descr);
    free_string(scroll->description);

    scroll->name = str_dup(qd->keywords);
    scroll->short_descr = str_dup(qd->short_descr);
    scroll->description = str_dup(qd->long_descr);


    act("$N gives $p to $n.", ch, questman, NULL, scroll, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("$N gives you $p.",   ch, questman, NULL, scroll, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    obj_to_char(scroll, ch);
    return true;
}

/* Set up a quest part. */
bool generate_quest_part(CHAR_DATA *ch, CHAR_DATA *questman, QUEST_PART_DATA *part, int partno)
{
    questman->tempstore[0] = partno;							// Which quest part *IS* this?  Needed for the "questcomplete" command

    // The quest part must return a positive value to be valid
    //  returning a zero due to "end 0" or not having the QUEST_PART trigger will be considered invalid
    //  errors in script execution will be negative, so will be considered invalid.
    return p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_PART, NULL) > 0;
}


/* Called from update_handler() by pulse_area */
void quest_update(void)
{
    DESCRIPTOR_DATA *d;
    CHAR_DATA *ch;
    QUEST_DATA *active_quest;
    int countdown_remaining;
    char buf[MAX_STRING_LENGTH];
    plogf(LOG_QUEST, "Update quests...");

    for (d = descriptor_list; d != NULL; d = d->next)
    {
    if (d->character != NULL && d->connected == CON_PLAYING)
    {
        ch = d->character;

        quest_runtime_update_player(ch);

        if ((ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
            && ch->countdown > 0
            && ch->quest_runtime.expiry_countdown_minutes <= 0) {
            ch->quest_runtime.expiry_countdown_minutes = ch->countdown;
        } else if (!(ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
            && ch->countdown > 0) {
            ch->quest_runtime.expiry_modes |= QUEST_EXPIRY_COUNTDOWN;
            ch->quest_runtime.expiry_countdown_minutes = ch->countdown;
        }

        if (IS_QUESTING(ch))
        {
        active_quest = quest_runtime_get_focused_run(ch);
        if (!active_quest)
            continue;

        if (active_quest && !active_quest->generating)
        {
        if (active_quest->run_status == QUEST_RUN_STATUS_ACTIVE)
            quest_runtime_fire_qprog_trigger(active_quest, TRIG_RANDOM, NULL, NULL);

        if (ch->quest_runtime.expiry_modes != QUEST_EXPIRY_NONE)
        {
            if (ch->quest_runtime.expiry_modes & (QUEST_EXPIRY_COUNTDOWN | QUEST_EXPIRY_WALL_TIME))
                quest_runtime_tick_expiration(ch, current_time);

            if (quest_runtime_is_expired(ch, current_time)) {
                quest_runtime_mark_run_failed(active_quest, QUEST_RUN_STATUS_FAILED, "expired");
                quest_runtime_detach_run(ch, active_quest);
                if (!IS_QUESTING(ch)) {
                    quest_runtime_reset_expiration(ch);
                    sprintf(buf, "{RYou have run out of time for your quest!{x\n\r");
                } else {
                    sprintf(buf, "{RYour focused quest run has expired.{x\n\r");
                }
                send_to_char(buf, ch);
                continue;
            }
        }

        countdown_remaining = 0;
        if (ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
            countdown_remaining = ch->quest_runtime.expiry_countdown_minutes;
        else if (ch->countdown > 0)
            countdown_remaining = ch->countdown;

        if (countdown_remaining > 0 && countdown_remaining < 6)
        {
            sprintf(buf, "You only have {Y%d{x minutes remaining to "
                "finish your quest!\n\r", countdown_remaining);
            send_to_char(buf, ch);
            continue;
        }
        }
        }
    }
    }
}


bool is_quest_token(OBJ_DATA *obj)
{
    int i;

    if (!obj || !obj->pIndexData) {
        return false;
    }

    resolve_quest_tokens();

    for (i = 0; i < QUEST_TOKEN_COUNT; i++) {
        if (quest_token_wnums[i].pArea &&
            wnum_match_obj(quest_token_wnums[i], obj)) {
            return true;
        }
    }

    return false;
}


void check_quest_rescue_mob(CHAR_DATA *ch, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    CHAR_DATA *mob;
    char buf[MAX_STRING_LENGTH];
    int i;
    bool found = true;

    if (!ch || ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
        perrf(LOG_QUEST, "check_quest_rescue_mob: NPC");
        return;
    }

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        i = 0;
        for (part = run->parts; part != NULL; part = part->next)
        {
            i++;

            // already did it
            if (part->complete == true)
                continue;

            found = false;
            mob = ch->in_room->people;
            quest_part_resolve(&part->mob_rescue_load, &part->mob_rescue_wnum);
            while (mob != NULL)
            {
                if (IS_NPC(mob) && wnum_match_mob(part->mob_rescue_wnum, mob) && !part->complete)
                {
                    if( show ) {
                        sprintf(buf, "Thank you for rescuing me, %s!", ch->name);
                        do_say(mob, buf);
                    }

                    if (mob->master != NULL)
                        stop_follower(mob,show);

                    add_follower(mob, ch, show);

                    if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_AGGRESSIVE))
                        REMOVE_BIT(mob->act[0], ACT_AGGRESSIVE);

                    found = true;
                    break;
                }

                mob = mob->next_in_room;
            }

            if (found && !part->complete)
            {
                if( show )
                {
                    sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
                    send_to_char(buf, ch);
                }

                part->complete = true;
                break;
            }
        }
    }
}


void check_quest_retrieve_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    WNUM target_wnum;
    int i;

    if (obj == NULL || obj->item_type == ITEM_MONEY)
    {
        pbugf(LOG_QUEST, "bad obj!");
        return;
    }

    if (IS_NPC(ch))
    {
        pbugf(LOG_QUEST, "NPC");
        return;
    }

    if (!ch || ch->quest == NULL)
        return;

    target_wnum.pArea = (obj && obj->pIndexData) ? obj->pIndexData->area : NULL;
    target_wnum.vnum = (obj && obj->pIndexData) ? obj->pIndexData->vnum : 0;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        i = 0;
        for (part = run->parts; part != NULL; part = part->next)
        {
            i++;

            // already did it
            if (part->complete == true)
                continue;

            if (part->pObj == obj)
            {
                if( show )
                {
                    char buf[MAX_STRING_LENGTH];
                    sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
                    send_to_char(buf, ch);
                }

                part->complete = true;
            }
        }

        if (quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_COLLECT, target_wnum, 1) > 0)
            quest_runtime_propagate_scoped_objective_event(ch, run, QUEST_OBJECTIVE_COLLECT, target_wnum, 1);
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    WNUM target_wnum;
    int i;

    if (!ch || ch->quest == NULL || !IS_NPC(mob))
        return;

    if (IS_NPC(ch))
    {
        pbugf(LOG_QUEST, "NPC");
        return;
    }

    if (!IS_NULLSTR(mob->owner)
        && str_cmp(mob->owner, "(no owner)")
        && str_cmp(mob->owner, ch->name))
    {
        return;
    }

    target_wnum.pArea = (mob && mob->pIndexData) ? mob->pIndexData->area : NULL;
    target_wnum.vnum = (mob && mob->pIndexData) ? mob->pIndexData->vnum : 0;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        i = 0;
        for (part = run->parts; part != NULL; part = part->next)
        {
            i++;

            // already did it
            if (part->complete == true)
                continue;

            quest_part_resolve(&part->mob_load, &part->mob_wnum);
            if (wnum_match_mob(part->mob_wnum, mob) && !part->complete)
            {
                if( show ) {
                    char buf[MAX_STRING_LENGTH];
                    sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
                    send_to_char(buf, ch);
                }

                part->complete = true;
            }
        }

        if (run->run_status != QUEST_RUN_STATUS_ACTIVE || run->generating || !run->current_stage_commenced)
            continue;

        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            continue;

        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            if (objective->objective_type != QUEST_OBJECTIVE_KILL)
                continue;

            if (!quest_runtime_objective_target_matches(run, objective, target_wnum))
                continue;

            if (objective->strict_target)
            {
                state = quest_runtime_get_objective_state(run, objective->id, false);
                if (!state)
                    continue;

                if (state->selected_target_uid[0] != mob->id[0]
                    || state->selected_target_uid[1] != mob->id[1])
                    continue;
            }

            if (quest_runtime_update_objective_progress(run, objective->id, 1))
                quest_runtime_propagate_scoped_objective_event(ch, run, QUEST_OBJECTIVE_KILL, target_wnum, 1);
        }
    }
}

void check_quest_talk_target(CHAR_DATA *ch, CHAR_DATA *victim, const char *message, bool show)
{
    QUEST_DATA *run;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    WNUM target_wnum;

    (void)show;

    if (!ch || IS_NPC(ch) || !victim || !IS_NPC(victim) || !victim->pIndexData)
        return;

    if (!ch->quest)
        return;

    target_wnum.pArea = victim->pIndexData->area;
    target_wnum.vnum = victim->pIndexData->vnum;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        if (run->run_status != QUEST_RUN_STATUS_ACTIVE || run->generating || !run->current_stage_commenced)
            continue;

        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            continue;

        for (objective = stage->objectives; objective != NULL; objective = objective->next)
        {
            if (objective->objective_type != QUEST_OBJECTIVE_TALK)
                continue;

            if (!quest_runtime_objective_target_matches(run, objective, target_wnum))
                continue;

            if (objective->strict_target)
            {
                state = quest_runtime_get_objective_state(run, objective->id, false);
                if (!state)
                    continue;

                if (state->selected_target_uid[0] != victim->id[0]
                    || state->selected_target_uid[1] != victim->id[1])
                    continue;
            }

            {
                const char *required_phrase = NULL;

                if (!IS_NULLSTR(objective->talk_phrase))
                    required_phrase = objective->talk_phrase;
                else if (!IS_NULLSTR(objective->target_tag))
                    required_phrase = objective->target_tag;

                if (!IS_NULLSTR(required_phrase)
                    && (IS_NULLSTR(message) || str_infix(required_phrase, message)))
                    continue;
            }

            quest_runtime_update_objective_progress(run, objective->id, 1);
        }
    }
}

void check_quest_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    ROOM_INDEX_DATA *target_room;
    WNUM target_wnum;
    int i;

    if (!ch || ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
        pbugf(LOG_QUEST, "NPC");
        return;
    }

    if (room == NULL)
    {
        pbugf(LOG_QUEST, "checking a null room");
        return;
    }

    target_wnum.pArea = room->area;
    target_wnum.vnum = room->vnum;

    for (run = ch->quest; run != NULL; run = run->next)
    {
        if (!quest_run_accessible_by_player(ch, run, false))
            continue;

        i = 0;
        for (part = run->parts; part != NULL; part = part->next)
        {
            i++;

            // already did it
            if (part->complete == true)
                continue;

            quest_part_resolve(&part->room_load, &part->room_wnum);
            target_room = get_room_index(part->room_wnum.pArea, part->room_wnum.vnum);

            /* Not going by room vnum to prevent multiple rooms with the same name */
            if (target_room != NULL && !str_cmp(target_room->name, room->name))
            {

                if( show )
                {
                    char buf[MAX_STRING_LENGTH];
                    sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
                    send_to_char(buf, ch);
                }

                part->complete = true;
            }
        }

        if (quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_TRAVEL, target_wnum, 1) > 0)
            quest_runtime_propagate_scoped_objective_event(ch, run, QUEST_OBJECTIVE_TRAVEL, target_wnum, 1);
    }
}

static bool check_quest_custom_task_run(CHAR_DATA *ch, QUEST_DATA *run, int task, bool show)
{
    QUEST_PART_DATA *part;
    int i;

    if (run == NULL)
        return false;

    if (IS_NPC(ch))
    {
        pbugf(LOG_QUEST, "check_quest_custom_task: NPC");
        return false;
    }

    i = 0;
    for (part = run->parts; part != NULL; part = part->next)
    {
        i++;

        // Not the current task nor is a custom task
        if( task != i || !part->custom_task )
            continue;

        // already did it
        if (part->complete == true)
            continue;


        if( show )
        {
            char buf[MAX_STRING_LENGTH];
            sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
            send_to_char(buf, ch);
        }
        part->complete = true;
        if (quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_CUSTOM_SCRIPT, (WNUM){ .pArea = NULL, .vnum = 0 }, 1) > 0)
            quest_runtime_propagate_scoped_objective_event(ch, run, QUEST_OBJECTIVE_CUSTOM_SCRIPT, (WNUM){ .pArea = NULL, .vnum = 0 }, 1);

        return true;
    }

    return false;
}


bool check_quest_custom_task(CHAR_DATA *ch, int task, bool show)
{
    QUEST_DATA *run;

    if (!ch)
        return false;

    run = quest_runtime_get_focused_run(ch);
    if (!run)
        return false;

    return check_quest_custom_task_run(ch, run, task, show);
}

int count_quest_parts(CHAR_DATA *ch)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    int parts;

    if (!ch || ch->quest == NULL)
        return 0;

    run = quest_runtime_get_focused_run(ch);
    if (!run)
        return 0;

    parts = 1;
    for (part = run->parts; part != NULL; part = part->next)
    {
        parts++;
    }

    return parts;
}


bool quest_index_v2_register(QUEST_INDEX_V2_DATA *quest_index_v2)
{
    int slot;

    if (!quest_index_v2 || !quest_index_v2->area || quest_index_v2->vnum < 1)
        return false;

    if (get_quest_index_v2_wnum((WNUM){ .pArea = quest_index_v2->area, .vnum = quest_index_v2->vnum }) != NULL)
        return false;

    slot = (int)(quest_index_v2->vnum % MAX_KEY_HASH);
    if (slot < 0)
        slot += MAX_KEY_HASH;

    quest_index_v2->next = quest_index_v2_list;
    quest_index_v2_list = quest_index_v2;

    if (quest_index_v2->area->quest_index_v2_hash[slot] == NULL)
        quest_index_v2->area->quest_index_v2_hash[slot] = quest_index_v2;

    return true;
}


void quest_index_v2_unregister(QUEST_INDEX_V2_DATA *quest_index_v2)
{
    QUEST_INDEX_V2_DATA *prev;
    QUEST_INDEX_V2_DATA *cur;
    int slot;

    if (!quest_index_v2)
        return;

    prev = NULL;
    cur = quest_index_v2_list;
    while (cur)
    {
        if (cur == quest_index_v2)
        {
            if (prev)
                prev->next = cur->next;
            else
                quest_index_v2_list = cur->next;
            break;
        }

        prev = cur;
        cur = cur->next;
    }

    if (quest_index_v2->area && quest_index_v2->vnum > 0)
    {
        slot = (int)(quest_index_v2->vnum % MAX_KEY_HASH);
        if (slot < 0)
            slot += MAX_KEY_HASH;

        if (quest_index_v2->area->quest_index_v2_hash[slot] == quest_index_v2)
            quest_index_v2->area->quest_index_v2_hash[slot] = NULL;
    }

    quest_index_v2->next = NULL;
}


void quest_index_v2_clear_registry(void)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_INDEX_V2_DATA *next;

    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = next)
    {
        next = quest_index_v2->next;
        if (quest_index_v2->area && quest_index_v2->vnum > 0)
        {
            int slot = (int)(quest_index_v2->vnum % MAX_KEY_HASH);
            if (slot < 0)
                slot += MAX_KEY_HASH;
            if (quest_index_v2->area->quest_index_v2_hash[slot] == quest_index_v2)
                quest_index_v2->area->quest_index_v2_hash[slot] = NULL;
        }
        quest_index_v2->next = NULL;
    }

    quest_index_v2_list = NULL;
}


QUEST_INDEX_V2_DATA *get_quest_index_v2(long vnum)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;

    if (vnum < 1)
        return NULL;

    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next)
    {
        if (quest_index_v2->vnum == vnum)
            return quest_index_v2;
    }

    return NULL;
}


QUEST_INDEX_V2_DATA *get_quest_index_v2_wnum(WNUM wnum)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int slot;

    if (!wnum.pArea || wnum.vnum < 1)
        return NULL;

    slot = (int)(wnum.vnum % MAX_KEY_HASH);
    if (slot < 0)
        slot += MAX_KEY_HASH;

    quest_index_v2 = wnum.pArea->quest_index_v2_hash[slot];
    if (quest_index_v2 && quest_index_v2->area == wnum.pArea && quest_index_v2->vnum == wnum.vnum)
        return quest_index_v2;

    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next)
    {
        if (quest_index_v2->area == wnum.pArea && quest_index_v2->vnum == wnum.vnum)
            return quest_index_v2;
    }

    return NULL;
}


QUEST_STAGE_INDEX_V2_DATA *quest_index_v2_get_stage(QUEST_INDEX_V2_DATA *quest_index_v2, int stage_id)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;

    if (!quest_index_v2 || stage_id < 1)
        return NULL;

    for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
    {
        if (stage->id == stage_id)
            return stage;
    }

    return NULL;
}


void fix_quests_v2(void)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *pool_entry;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    AREA_DATA *fallback;
    AREA_DATA *pArea;
    int iHash;

    for (quest_index_v2 = quest_index_v2_list; quest_index_v2 != NULL; quest_index_v2 = quest_index_v2->next)
    {
        fallback = quest_index_v2->area ? quest_index_v2->area : get_system_area_fallback();

        for (stage = quest_index_v2->stages; stage != NULL; stage = stage->next)
        {
            for (objective = stage->objectives; objective != NULL; objective = objective->next)
            {
                resolve_wnum_load(&objective->target_load, &objective->target_wnum, fallback);
                resolve_wnum_load(&objective->destination_load, &objective->destination_wnum, fallback);
                resolve_wnum_load(&objective->target_token_load, &objective->target_token_wnum, fallback);
                resolve_wnum_load(&objective->destination_token_load, &objective->destination_token_wnum, fallback);

                for (pool_entry = objective->pool_entries; pool_entry != NULL; pool_entry = pool_entry->next)
                {
                    resolve_wnum_load(&pool_entry->target_load, &pool_entry->target_wnum, fallback);
                }
            }
        }

        for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
        {
            resolve_wnum_load(&reward->target_load, &reward->target_wnum, fallback);
        }
    }

    /* Resolve quests_v2 wnums on mob and obj indices */
    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            MOB_INDEX_DATA *mob;
            for (mob = pArea->mob_index_hash[iHash]; mob != NULL; mob = mob->next)
            {
                QUEST_V2_LIST *qv2;
                for (qv2 = mob->quests_v2; qv2 != NULL; qv2 = qv2->next)
                    resolve_wnum_load(&qv2->load, &qv2->wnum, pArea);
            }

            OBJ_INDEX_DATA *obj;
            for (obj = pArea->obj_index_hash[iHash]; obj != NULL; obj = obj->next)
            {
                QUEST_V2_LIST *qv2;
                for (qv2 = obj->quests_v2; qv2 != NULL; qv2 = qv2->next)
                    resolve_wnum_load(&qv2->load, &qv2->wnum, pArea);
            }
        }
    }
}


QUEST_OBJECTIVE_INDEX_V2_DATA *quest_stage_index_v2_get_objective(QUEST_STAGE_INDEX_V2_DATA *stage, int objective_id)
{
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;

    if (!stage || objective_id < 1)
        return NULL;

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        if (objective->id == objective_id)
            return objective;
    }

    return NULL;
}


static bool quest_runtime_objective_target_matches(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, WNUM target_wnum)
{
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    WNUM selected = wnum_zero;

    if (!objective)
        return false;

    if (run)
    {
        state = quest_runtime_get_objective_state(run, objective->id, false);
        if (state && state->selected_target_wnum.pArea && state->selected_target_wnum.vnum > 0)
            selected = state->selected_target_wnum;
    }

    if (selected.pArea && selected.vnum > 0)
    {
        if (!target_wnum.pArea || target_wnum.vnum < 1)
            return false;

        return selected.pArea == target_wnum.pArea
            && selected.vnum == target_wnum.vnum;
    }

    if (objective->target_mode == QUEST_OBJECTIVE_TARGET_POOL)
        return false;

    if (!objective->target_wnum.pArea || objective->target_wnum.vnum < 1)
        return true;

    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return false;

    return objective->target_wnum.pArea == target_wnum.pArea
        && objective->target_wnum.vnum == target_wnum.vnum;
}


static int quest_runtime_objective_required_count(QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    int required;

    if (!objective)
        return 1;

    required = objective->required_count;
    if (required < 1)
        required = objective->quantity;
    if (required < 1)
        required = 1;

    return required;
}


static void quest_runtime_clear_target_bindings(QUEST_DATA *run)
{
    QUEST_TARGET_BINDING_V2_DATA *binding;
    QUEST_TARGET_BINDING_V2_DATA *next;

    if (!run)
        return;

    binding = run->target_bindings;
    while (binding)
    {
        next = binding->next;
        free_quest_target_binding_v2(binding);
        binding = next;
    }

    run->target_bindings = NULL;
}


static WNUM quest_runtime_get_target_binding(QUEST_DATA *run, const char *name)
{
    QUEST_TARGET_BINDING_V2_DATA *binding;

    if (!run || IS_NULLSTR(name))
        return wnum_zero;

    for (binding = run->target_bindings; binding != NULL; binding = binding->next)
    {
        if (!IS_NULLSTR(binding->name) && !str_cmp(binding->name, name))
            return binding->target_wnum;
    }

    return wnum_zero;
}


static bool quest_runtime_set_target_binding(QUEST_DATA *run, const char *name, WNUM target_wnum)
{
    QUEST_TARGET_BINDING_V2_DATA *binding;

    if (!run || IS_NULLSTR(name) || !target_wnum.pArea || target_wnum.vnum < 1)
        return false;

    for (binding = run->target_bindings; binding != NULL; binding = binding->next)
    {
        if (!IS_NULLSTR(binding->name) && !str_cmp(binding->name, name))
        {
            binding->target_load.auid = target_wnum.pArea->uid;
            binding->target_load.vnum = target_wnum.vnum;
            binding->target_wnum = target_wnum;
            return true;
        }
    }

    binding = new_quest_target_binding_v2();
    if (!binding)
        return false;

    free_string(binding->name);
    binding->name = str_dup(name);
    binding->target_load.auid = target_wnum.pArea->uid;
    binding->target_load.vnum = target_wnum.vnum;
    binding->target_wnum = target_wnum;
    binding->next = run->target_bindings;
    run->target_bindings = binding;

    return true;
}


static WNUM quest_runtime_resolve_objective_target_reference(QUEST_DATA *run, QUEST_INDEX_V2_DATA *quest_index_v2,
    QUEST_STAGE_INDEX_V2_DATA *stage, QUEST_OBJECTIVE_INDEX_V2_DATA *objective)
{
    QUEST_STAGE_INDEX_V2_DATA *ref_stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *ref_objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *ref_state;
    WNUM resolved;

    if (!objective)
        return wnum_zero;

    if (!IS_NULLSTR(objective->target_ref_name))
    {
        resolved = quest_runtime_get_target_binding(run, objective->target_ref_name);
        if (resolved.pArea && resolved.vnum > 0)
            return resolved;
    }

    if (!quest_index_v2 || objective->target_ref_stage_id < 1 || objective->target_ref_objective_id < 1)
        return wnum_zero;

    ref_stage = quest_index_v2_get_stage(quest_index_v2, objective->target_ref_stage_id);
    if (!ref_stage)
        return wnum_zero;

    ref_objective = quest_stage_index_v2_get_objective(ref_stage, objective->target_ref_objective_id);
    if (!ref_objective)
        return wnum_zero;

    if (run && stage && ref_stage->id == stage->id)
    {
        ref_state = quest_runtime_get_objective_state(run, ref_objective->id, false);
        if (ref_state && ref_state->selected_target_wnum.pArea && ref_state->selected_target_wnum.vnum > 0)
            return ref_state->selected_target_wnum;
    }

    if (ref_objective->target_wnum.pArea && ref_objective->target_wnum.vnum > 0)
        return ref_objective->target_wnum;

    return wnum_zero;
}


static unsigned long long quest_runtime_mix_seed(unsigned long long value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value = value ^ (value >> 31);

    if (value == 0)
        value = 0x1a2b3c4d5e6f7890ULL;

    return value;
}


static void quest_runtime_ensure_generation_seed(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    unsigned long long seed;

    if (!run || run->generation_seed != 0)
        return;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (quest_index_v2
        && quest_index_v2->seed_policy == QUEST_SEED_POLICY_FIXED
        && quest_index_v2->fixed_seed != 0)
    {
        run->generation_seed = quest_runtime_mix_seed(quest_index_v2->fixed_seed);
        return;
    }

    seed = (unsigned long long)(run->quest_index_v2_auid & 0xFFFFFFFFULL);
    seed = (seed << 32) ^ (unsigned long long)(run->quest_index_v2_vnum & 0xFFFFFFFFULL);
    seed ^= ((unsigned long long)(run->run_id & 0xFFFFFFFFULL) << 1);
    seed ^= ((unsigned long long)(run->started_at > 0 ? run->started_at : current_time) << 3);

    run->generation_seed = quest_runtime_mix_seed(seed);
}


unsigned long long quest_runtime_seed_for_stage(QUEST_DATA *run, int stage_id)
{
    unsigned long long seed;

    if (!run || stage_id < 1)
        return 0;

    quest_runtime_ensure_generation_seed(run);

    seed = run->generation_seed;
    seed ^= (unsigned long long)(stage_id & 0xFFFFFFFFULL) * 0x9e3779b97f4a7c15ULL;
    seed ^= (unsigned long long)(run->quest_index_v2_auid & 0xFFFFFFFFULL) << 7;
    seed ^= (unsigned long long)(run->quest_index_v2_vnum & 0xFFFFFFFFULL) << 19;

    return quest_runtime_mix_seed(seed);
}


static bool quest_runtime_compile_generated_stage(QUEST_DATA *run, QUEST_STAGE_INDEX_V2_DATA *stage, unsigned long long stage_seed)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *selected_entry;
    WNUM selected_target;
    WNUM selected_destination;
    bool stage_target_resolved = false;
    bool stage_destination_resolved = false;

    if (!run || !stage)
        return false;

    quest_index_v2 = quest_runtime_get_index_v2(run);

    run->current_stage_seed = stage_seed;

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        state = quest_runtime_get_objective_state(run, objective->id, true);
        if (!state)
            return false;

        state->selected_pool_entry_id = 0;
        state->selected_target_load.auid = 0;
        state->selected_target_load.vnum = 0;
        state->selected_target_wnum = wnum_zero;
        state->selected_target_uid[0] = 0;
        state->selected_target_uid[1] = 0;
        state->selected_destination_load.auid = 0;
        state->selected_destination_load.vnum = 0;
        state->selected_destination_wnum = wnum_zero;

        selected_target = objective->target_wnum;

        if ((!selected_target.pArea || selected_target.vnum < 1)
            && (!IS_NULLSTR(objective->target_ref_name)
                || (objective->target_ref_stage_id > 0 && objective->target_ref_objective_id > 0)))
        {
            selected_target = quest_runtime_resolve_objective_target_reference(run, quest_index_v2, stage, objective);
        }

        if ((!selected_target.pArea || selected_target.vnum < 1)
            && objective->target_mode == QUEST_OBJECTIVE_TARGET_POOL
            && objective->pool_entries)
        {
            unsigned long long objective_seed = quest_runtime_mix_seed(
                stage_seed ^ ((unsigned long long)(objective->id & 0xFFFFFFFFULL) * 0x9e3779b97f4a7c15ULL));

            selected_entry = quest_runtime_pick_objective_pool_entry(objective, objective_seed);
            if (selected_entry)
            {
                state->selected_pool_entry_id = selected_entry->id;
                selected_target = selected_entry->target_wnum;
            }
            else
            {
                selected_target = wnum_zero;
            }
        }

        if (selected_target.pArea && selected_target.vnum > 0)
        {
            state->selected_target_load.auid = selected_target.pArea->uid;
            state->selected_target_load.vnum = selected_target.vnum;
            state->selected_target_wnum = selected_target;
            stage_target_resolved = true;
            quest_runtime_fire_objective_lifecycle_trigger(run, TRIG_OBJECTIVE_TARGET_RESOLVED, objective->id);

            if (!IS_NULLSTR(objective->target_variable_name))
                quest_runtime_set_target_binding(run, objective->target_variable_name, selected_target);
        }

        selected_destination = objective->destination_wnum;
        if ((!selected_destination.pArea || selected_destination.vnum < 1)
            && !IS_NULLSTR(objective->destination_ref_name))
        {
            selected_destination = quest_runtime_get_target_binding(run, objective->destination_ref_name);
        }

        if (selected_destination.pArea && selected_destination.vnum > 0)
        {
            state->selected_destination_load.auid = selected_destination.pArea->uid;
            state->selected_destination_load.vnum = selected_destination.vnum;
            state->selected_destination_wnum = selected_destination;
            stage_destination_resolved = true;
            quest_runtime_fire_objective_lifecycle_trigger(run, TRIG_OBJECTIVE_DEST_RESOLVED, objective->id);

            if (!IS_NULLSTR(objective->destination_variable_name))
                quest_runtime_set_target_binding(run, objective->destination_variable_name, selected_destination);
        }
    }

    if (stage_target_resolved)
        quest_runtime_fire_stage_lifecycle_trigger(run, TRIG_STAGE_TARGET_RESOLVED, stage->id);

    if (stage_destination_resolved)
        quest_runtime_fire_stage_lifecycle_trigger(run, TRIG_STAGE_DEST_RESOLVED, stage->id);

    run->current_stage_generation = 1;

    return true;
}


static QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *quest_runtime_pick_objective_pool_entry(QUEST_OBJECTIVE_INDEX_V2_DATA *objective, unsigned long long seed)
{
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *entry;
    QUEST_OBJECTIVE_POOL_ENTRY_V2_DATA *fallback = NULL;
    int total_weight = 0;
    int roll;

    if (!objective || !objective->pool_entries)
        return NULL;

    for (entry = objective->pool_entries; entry != NULL; entry = entry->next)
    {
        if (!fallback)
            fallback = entry;

        if (entry->weight > 0)
            total_weight += entry->weight;
    }

    if (total_weight < 1)
        return fallback;

    roll = (int)(seed % (unsigned long long)total_weight) + 1;

    for (entry = objective->pool_entries; entry != NULL; entry = entry->next)
    {
        int weight = UMAX(0, entry->weight);
        if (weight < 1)
            continue;

        roll -= weight;
        if (roll <= 0)
            return entry;
    }

    return fallback;
}


static ROOM_INDEX_DATA *quest_runtime_pick_room_in_area(AREA_DATA *area, unsigned long long seed)
{
    ROOM_INDEX_DATA *room;
    int iHash;
    int count = 0;
    int pick;

    if (!area)
        return NULL;

    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (room = room_index_hash[iHash]; room != NULL; room = room->next)
        {
            if (room->area == area)
                count++;
        }
    }

    if (count < 1)
        return NULL;

    pick = (int)(seed % (unsigned long long)count);

    for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for (room = room_index_hash[iHash]; room != NULL; room = room->next)
        {
            if (room->area != area)
                continue;

            if (pick-- == 0)
                return room;
        }
    }

    return NULL;
}


static int quest_runtime_count_live_mobs_by_index(WNUM target_wnum, AREA_DATA *scope_area)
{
    CHAR_DATA *mob;
    ITERATOR it;
    int count = 0;

    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return 0;

    iterator_start(&it, loaded_chars);
    while ((mob = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (!IS_NPC(mob) || !mob->pIndexData)
            continue;

        if (mob->pIndexData->area != target_wnum.pArea || mob->pIndexData->vnum != target_wnum.vnum)
            continue;

        if (scope_area && (!mob->in_room || mob->in_room->area != scope_area))
            continue;

        if (mob->fighting != NULL)
            continue;

        count++;
    }
    iterator_stop(&it);

    return count;
}


static CHAR_DATA *quest_runtime_get_owner_character(QUEST_DATA *run)
{
    CHAR_DATA *ch;
    ITERATOR it;

    if (!run)
        return NULL;

    if (run->target_scope == QUEST_TARGET_SCOPE_CHARACTER
        && (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0))
        return NULL;

    if (run->target_scope == QUEST_TARGET_SCOPE_GROUP
        && (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0))
        return NULL;

    if (run->target_scope == QUEST_TARGET_SCOPE_CHURCH && run->scope_owner_uid <= 0)
        return NULL;

    iterator_start(&it, loaded_chars);
    while ((ch = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(ch))
            continue;

        if (run->target_scope == QUEST_TARGET_SCOPE_CHARACTER && uid_match(ch->id, run->scope_owner_id))
        {
            iterator_stop(&it);
            return ch;
        }

        if (run->target_scope == QUEST_TARGET_SCOPE_GROUP
            && IS_VALID(ch->group)
            && uid_match(ch->group->id, run->scope_owner_id))
        {
            iterator_stop(&it);
            return ch;
        }

        if (run->target_scope == QUEST_TARGET_SCOPE_CHURCH
            && ch->church
            && ch->church->uid == run->scope_owner_uid)
        {
            iterator_stop(&it);
            return ch;
        }
    }
    iterator_stop(&it);

    return NULL;
}


static void quest_runtime_apply_spawn_owner_lock(QUEST_DATA *run, CHAR_DATA *mob)
{
    CHAR_DATA *owner;

    if (!run || !mob || !IS_NPC(mob))
        return;

    owner = quest_runtime_get_owner_character(run);
    if (!owner || IS_NULLSTR(owner->name))
        return;

    if (mob->owner)
        free_string(mob->owner);
    mob->owner = str_dup(owner->name);
}


static bool quest_runtime_spawn_missing_kill_target(QUEST_DATA *run, QUEST_OBJECTIVE_STATE_V2_DATA *state)
{
    MOB_INDEX_DATA *mob_index;
    AREA_DATA *scope_area;
    ROOM_INDEX_DATA *spawn_room;
    CHAR_DATA *spawned;
    unsigned long long spawn_seed;

    if (!run || !state || !state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1)
        return false;

    scope_area = state->selected_target_wnum.pArea;
    if (quest_runtime_count_live_mobs_by_index(state->selected_target_wnum, scope_area) > 0)
        return true;

    mob_index = get_mob_index(state->selected_target_wnum.pArea, state->selected_target_wnum.vnum);
    if (!mob_index)
        return false;

    spawn_seed = quest_runtime_mix_seed(run->current_stage_seed
        ^ ((unsigned long long)(state->objective_id & 0xFFFFFFFFULL) << 17)
        ^ ((unsigned long long)(state->selected_target_wnum.vnum & 0xFFFFFFFFULL) << 5));

    spawn_room = quest_runtime_pick_room_in_area(scope_area, spawn_seed);
    if (!spawn_room)
        return false;

    spawned = create_mobile(mob_index, false);
    if (!spawned)
        return false;

    quest_runtime_apply_spawn_owner_lock(run, spawned);
    char_to_room(spawned, spawn_room);
    return true;
}


static CHAR_DATA *quest_runtime_find_mob_target_instance(QUEST_DATA *run, WNUM target_wnum, AREA_DATA *scope_area)
{
    CHAR_DATA *mob;
    CHAR_DATA *owner;
    ITERATOR it;

    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return NULL;

    owner = quest_runtime_get_owner_character(run);

    iterator_start(&it, loaded_chars);
    while ((mob = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (!IS_NPC(mob) || !mob->pIndexData)
            continue;

        if (mob->pIndexData->area != target_wnum.pArea || mob->pIndexData->vnum != target_wnum.vnum)
            continue;

        if (scope_area && (!mob->in_room || mob->in_room->area != scope_area))
            continue;

        if (owner && !IS_NULLSTR(mob->owner)
            && str_cmp(mob->owner, "(no owner)")
            && str_cmp(mob->owner, owner->name))
            continue;

        iterator_stop(&it);
        return mob;
    }
    iterator_stop(&it);

    return NULL;
}


static CHAR_DATA *quest_runtime_find_unbound_mob_target_instance(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective,
    WNUM target_wnum, AREA_DATA *scope_area)
{
    CHAR_DATA *mob;
    CHAR_DATA *owner;
    ITERATOR it;

    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return NULL;

    owner = quest_runtime_get_owner_character(run);

    iterator_start(&it, loaded_chars);
    while ((mob = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (!IS_NPC(mob) || !mob->pIndexData)
            continue;

        if (mob->pIndexData->area != target_wnum.pArea || mob->pIndexData->vnum != target_wnum.vnum)
            continue;

        if (scope_area && (!mob->in_room || mob->in_room->area != scope_area))
            continue;

        if (owner && !IS_NULLSTR(mob->owner)
            && str_cmp(mob->owner, "(no owner)")
            && str_cmp(mob->owner, owner->name))
            continue;

        if (objective && objective->strict_target
            && quest_runtime_is_mob_strictly_bound(mob, run, objective->id))
            continue;

        iterator_stop(&it);
        return mob;
    }
    iterator_stop(&it);

    return NULL;
}


static OBJ_DATA *quest_runtime_find_object_target_instance(WNUM target_wnum, AREA_DATA *scope_area)
{
    OBJ_DATA *obj;
    ITERATOR it;

    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return NULL;

    iterator_start(&it, loaded_objects);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it)) != NULL)
    {
        ROOM_INDEX_DATA *room;

        if (!obj->pIndexData)
            continue;

        if (obj->pIndexData->area != target_wnum.pArea || obj->pIndexData->vnum != target_wnum.vnum)
            continue;

        if (!scope_area)
        {
            iterator_stop(&it);
            return obj;
        }

        room = obj_room(obj);
        if (room && room->area == scope_area)
        {
            iterator_stop(&it);
            return obj;
        }
    }
    iterator_stop(&it);

    return NULL;
}


static bool quest_runtime_attach_objective_target_token(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_STATE_V2_DATA *state)
{
    TOKEN_INDEX_DATA *token_index;
    WNUM token_wnum;
    WNUM attach_wnum;
    AREA_DATA *scope_area;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;

    if (!run || !objective)
        return false;

    token_wnum = objective->target_token_wnum;
    if (!token_wnum.pArea || token_wnum.vnum < 1)
        return true;

    token_index = get_token_index(token_wnum.pArea, token_wnum.vnum);
    if (!token_index)
        return false;

    attach_wnum = state ? state->selected_target_wnum : wnum_zero;
    if (!attach_wnum.pArea || attach_wnum.vnum < 1)
        attach_wnum = objective->target_wnum;
    if (!attach_wnum.pArea || attach_wnum.vnum < 1)
        return false;

    scope_area = attach_wnum.pArea;

    if (get_mob_index(attach_wnum.pArea, attach_wnum.vnum) != NULL)
    {
        if (state && (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1))
        {
            state->selected_target_load.auid = attach_wnum.pArea->uid;
            state->selected_target_load.vnum = attach_wnum.vnum;
            state->selected_target_wnum = attach_wnum;
        }

        if (state && !quest_runtime_spawn_missing_kill_target(run, state))
            return false;

        mob = quest_runtime_find_mob_target_instance(run, attach_wnum, scope_area);
        if (!mob)
            return false;

        if (get_token_char(mob, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, mob, NULL, NULL) != NULL;
    }

    if (get_obj_index(attach_wnum.pArea, attach_wnum.vnum) != NULL)
    {
        obj = quest_runtime_find_object_target_instance(attach_wnum, scope_area);
        if (!obj)
            return false;

        if (get_token_obj(obj, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, NULL, obj, NULL) != NULL;
    }

    room = get_room_index(attach_wnum.pArea, attach_wnum.vnum);
    if (room)
    {
        if (get_token_room(room, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, NULL, NULL, room) != NULL;
    }

    return false;
}


static bool quest_runtime_attach_objective_destination_token(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_STATE_V2_DATA *state)
{
    TOKEN_INDEX_DATA *token_index;
    WNUM token_wnum;
    WNUM attach_wnum;
    AREA_DATA *scope_area;
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    ROOM_INDEX_DATA *room;

    if (!run || !objective)
        return false;

    token_wnum = objective->destination_token_wnum;
    if (!token_wnum.pArea || token_wnum.vnum < 1)
        return true;

    token_index = get_token_index(token_wnum.pArea, token_wnum.vnum);
    if (!token_index)
        return false;

    attach_wnum = state ? state->selected_destination_wnum : wnum_zero;
    if (!attach_wnum.pArea || attach_wnum.vnum < 1)
        attach_wnum = objective->destination_wnum;
    if (!attach_wnum.pArea || attach_wnum.vnum < 1)
        return false;

    scope_area = attach_wnum.pArea;

    if (get_mob_index(attach_wnum.pArea, attach_wnum.vnum) != NULL)
    {
        if (state && (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1))
        {
            state->selected_target_load.auid = attach_wnum.pArea->uid;
            state->selected_target_load.vnum = attach_wnum.vnum;
            state->selected_target_wnum = attach_wnum;
        }

        if (state && !quest_runtime_spawn_missing_kill_target(run, state))
            return false;

        mob = quest_runtime_find_mob_target_instance(run, attach_wnum, scope_area);
        if (!mob)
            return false;

        if (get_token_char(mob, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, mob, NULL, NULL) != NULL;
    }

    if (get_obj_index(attach_wnum.pArea, attach_wnum.vnum) != NULL)
    {
        obj = quest_runtime_find_object_target_instance(attach_wnum, scope_area);
        if (!obj)
            return false;

        if (get_token_obj(obj, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, NULL, obj, NULL) != NULL;
    }

    room = get_room_index(attach_wnum.pArea, attach_wnum.vnum);
    if (room)
    {
        if (get_token_room(room, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, NULL, NULL, room) != NULL;
    }

    return false;
}


static bool quest_runtime_commence_current_stage(QUEST_DATA *run)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
        return false;

    if (run->current_stage_commenced != 0)
        return true;

    if (stage->stage_source == QUEST_STAGE_SOURCE_GENERATED && run->current_stage_generation == 0)
    {
        if (!quest_runtime_compile_generated_stage(run, stage, run->current_stage_seed))
            return false;
    }

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        state = quest_runtime_get_objective_state(run, objective->id, true);
        if (!state)
            return false;

        if (objective->target_mode == QUEST_OBJECTIVE_TARGET_POOL)
        {
            if (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1)
                return false;
        }

        if (objective->objective_type == QUEST_OBJECTIVE_KILL)
        {
            WNUM kill_target = state->selected_target_wnum;
            CHAR_DATA *strict_mob = NULL;
            if (!kill_target.pArea || kill_target.vnum < 1)
                kill_target = objective->target_wnum;

            if (kill_target.pArea && kill_target.vnum > 0)
            {
                if (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1)
                {
                    state->selected_target_load.auid = kill_target.pArea->uid;
                    state->selected_target_load.vnum = kill_target.vnum;
                    state->selected_target_wnum = kill_target;
                }

                if (!quest_runtime_spawn_missing_kill_target(run, state))
                    plogf(LOG_DEBUG,
                        "quest_runtime_commence_current_stage: spawn check failed for kill objective %d (%ld#%ld), proceeding",
                        objective->id,
                        kill_target.pArea ? kill_target.pArea->uid : 0,
                        kill_target.vnum);

                if (objective->strict_target)
                {
                    if (!quest_runtime_scope_owner_online(run))
                        return false;

                    strict_mob = quest_runtime_find_unbound_mob_target_instance(run, objective, kill_target, kill_target.pArea);
                    if (!strict_mob)
                        return false;

                    state->selected_target_uid[0] = strict_mob->id[0];
                    state->selected_target_uid[1] = strict_mob->id[1];
                }
                else
                {
                    state->selected_target_uid[0] = 0;
                    state->selected_target_uid[1] = 0;
                }
            }
        }

        if (objective->objective_type == QUEST_OBJECTIVE_TALK
            || objective->objective_type == QUEST_OBJECTIVE_CUSTOM_SCRIPT)
        {
            WNUM mob_target = state->selected_target_wnum;
            CHAR_DATA *strict_mob = NULL;
            if (!mob_target.pArea || mob_target.vnum < 1)
                mob_target = objective->target_wnum;

            if (mob_target.pArea && mob_target.vnum > 0
                && get_mob_index(mob_target.pArea, mob_target.vnum) != NULL)
            {
                if (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1)
                {
                    state->selected_target_load.auid = mob_target.pArea->uid;
                    state->selected_target_load.vnum = mob_target.vnum;
                    state->selected_target_wnum = mob_target;
                }

                if (!quest_runtime_spawn_missing_kill_target(run, state))
                    plogf(LOG_DEBUG,
                        "quest_runtime_commence_current_stage: spawn check failed for talk/custom objective %d (%ld#%ld), proceeding",
                        objective->id,
                        mob_target.pArea ? mob_target.pArea->uid : 0,
                        mob_target.vnum);

                if (objective->strict_target)
                {
                    if (!quest_runtime_scope_owner_online(run))
                        return false;

                    strict_mob = quest_runtime_find_unbound_mob_target_instance(run, objective, mob_target, mob_target.pArea);
                    if (!strict_mob)
                        return false;

                    state->selected_target_uid[0] = strict_mob->id[0];
                    state->selected_target_uid[1] = strict_mob->id[1];
                }
                else
                {
                    state->selected_target_uid[0] = 0;
                    state->selected_target_uid[1] = 0;
                }
            }
        }

        switch (objective->objective_type)
        {
        case QUEST_OBJECTIVE_COLLECT:
        case QUEST_OBJECTIVE_KILL:
            break;

        case QUEST_OBJECTIVE_TRAVEL:
        case QUEST_OBJECTIVE_TALK:
        case QUEST_OBJECTIVE_CUSTOM_SCRIPT:
        default:
            break;
        }

        if (!quest_runtime_attach_objective_target_token(run, objective, state))
            return false;

        if (!quest_runtime_attach_objective_destination_token(run, objective, state))
            return false;
    }

    run->current_stage_commenced = (int)current_time;
    quest_runtime_fire_stage_lifecycle_trigger(run, TRIG_STAGE_COMMENCED, stage->id);
    return true;
}


QUEST_INDEX_V2_DATA *quest_runtime_get_index_v2(QUEST_DATA *run)
{
    WNUM wnum;

    if (!run || run->quest_index_v2_vnum < 1)
        return NULL;

    wnum.pArea = get_area_index(run->quest_index_v2_auid);
    wnum.vnum = run->quest_index_v2_vnum;

    return get_quest_index_v2_wnum(wnum);
}


QUEST_STAGE_INDEX_V2_DATA *quest_runtime_get_current_stage(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    int stage_id;

    if (!run)
        return NULL;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2)
        return NULL;

    stage_id = run->current_stage_id;
    if (stage_id < 1)
    {
        stage_id = quest_index_v2->entry_stage_id;
        if (stage_id < 1 && quest_index_v2->stages)
            stage_id = quest_index_v2->stages->id;
    }

    if (stage_id < 1)
        return NULL;

    return quest_index_v2_get_stage(quest_index_v2, stage_id);
}


bool quest_runtime_is_stage_complete(QUEST_DATA *run)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    int required_total = 0;
    int required_complete = 0;
    bool any_complete = false;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
        return false;

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        state = quest_runtime_get_objective_state(run, objective->id, false);
        if (state && state->complete)
            any_complete = true;

        if (!objective->optional)
        {
            required_total++;
            if (state && state->complete)
                required_complete++;
        }
    }

    if (stage->completion_mode == QUEST_STAGE_COMPLETE_ANY)
    {
        if (required_total > 0)
            return required_complete > 0;
        return any_complete;
    }

    if (required_total == 0)
        return true;

    return required_complete >= required_total;
}


bool quest_runtime_try_advance_stage(QUEST_DATA *run)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    int entry_stage_id;
    bool changed = false;
    int guard = 0;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2)
        return false;

    if (run->current_stage_id < 1)
    {
        entry_stage_id = quest_index_v2->entry_stage_id;
        if (entry_stage_id < 1 && quest_index_v2->stages)
            entry_stage_id = quest_index_v2->stages->id;

        if (entry_stage_id < 1)
            return false;

        if (!quest_runtime_set_stage(run, entry_stage_id))
            return false;

        changed = true;
    }

    while (guard++ < 64)
    {
        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            return changed;

        if (!quest_runtime_is_stage_complete(run))
            return changed;

        quest_runtime_emit_stage_message(run, stage, false);
        quest_runtime_fire_stage_lifecycle_trigger(run, TRIG_STAGE_COMPLETED, stage->id);

        if (stage->next_stage_id < 1)
        {
            if (!IS_NULLSTR(stage->on_exit_script))
                quest_runtime_fire_stage_script(run, stage->on_exit_script);

            run->run_status = QUEST_RUN_STATUS_COMPLETED;
            run->completed_at = current_time;
            run->failed_at = 0;
            run->abandoned_at = 0;
            quest_runtime_apply_rewards(run, NULL);
            quest_runtime_fire_quest_lifecycle_trigger(run, TRIG_QUEST_COMPLETED, "complete");
            quest_runtime_record_terminal_history(run);
            return true;
        }

        if (!quest_runtime_set_stage(run, stage->next_stage_id))
            return changed;

        changed = true;
    }

    return changed;
}


bool quest_runtime_update_objective_progress(QUEST_DATA *run, int objective_id, int delta)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    int required;
    int new_progress;
    bool changed;
    bool was_complete;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE || objective_id < 1)
        return false;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
    {
        if (!quest_runtime_try_advance_stage(run))
            return false;
        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            return false;
    }

    objective = quest_stage_index_v2_get_objective(stage, objective_id);
    if (!objective)
        return false;

    state = quest_runtime_get_objective_state(run, objective_id, true);
    if (!state)
        return false;

    required = quest_runtime_objective_required_count(objective);
    was_complete = state->complete;
    new_progress = state->progress + delta;
    if (new_progress < 0)
        new_progress = 0;
    if (new_progress > required)
        new_progress = required;

    changed = (new_progress != state->progress);
    state->progress = new_progress;
    state->complete = (state->progress >= required);

    if (!was_complete && state->complete)
    {
        quest_runtime_emit_objective_message(run, objective, false);
        quest_runtime_fire_objective_lifecycle_trigger(run, TRIG_OBJECTIVE_COMPLETED, objective_id);
    }

    quest_runtime_try_advance_stage(run);
    return changed;
}


bool quest_runtime_complete_objective(QUEST_DATA *run, int objective_id)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    QUEST_OBJECTIVE_STATE_V2_DATA *state;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE || objective_id < 1)
        return false;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
    {
        if (!quest_runtime_try_advance_stage(run))
            return false;
        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            return false;
    }

    objective = quest_stage_index_v2_get_objective(stage, objective_id);
    if (!objective)
        return false;

    state = quest_runtime_get_objective_state(run, objective_id, true);
    if (!state)
        return false;

    if (!state->complete)
    {
        quest_runtime_emit_objective_message(run, objective, false);
        quest_runtime_fire_objective_lifecycle_trigger(run, TRIG_OBJECTIVE_COMPLETED, objective_id);
    }

    state->progress = quest_runtime_objective_required_count(objective);
    state->complete = true;
    quest_runtime_try_advance_stage(run);
    return true;
}


bool quest_runtime_fail_objective(QUEST_DATA *run, int objective_id, const char *reason_phrase)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE || objective_id < 1)
        return false;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
        return false;

    objective = quest_stage_index_v2_get_objective(stage, objective_id);
    if (!objective)
        return false;

    quest_runtime_mark_run_failed(run, QUEST_RUN_STATUS_FAILED,
        IS_NULLSTR(reason_phrase) ? "objective_failed" : reason_phrase);
    return true;
}


/* Apply all configured rewards from the quest index to the quest owner.
 * Rewards include quest points, currency (silver/gold), reputation,
 * tokens placed on the player, items, and inline scripts.
 * enactor may be provided to override the scope-owner lookup. */
static void quest_runtime_apply_rewards(QUEST_DATA *run, CHAR_DATA *enactor)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_REWARD_INDEX_V2_DATA *reward;
    CHAR_DATA *ch;

    if (!run)
        return;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2 || !quest_index_v2->rewards)
        return;

    ch = enactor ? enactor : quest_runtime_get_owner_character(run);
    if (!ch || IS_NPC(ch))
        return;

    for (reward = quest_index_v2->rewards; reward != NULL; reward = reward->next)
    {
        switch (reward->reward_type)
        {
            case QUEST_REWARD_POINTS:
            {
                long amount = UMAX(reward->amount, 0);
                if (amount <= 0)
                    break;
                ch->questpoints += (int)amount;
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                else
                    printf_to_char(ch, "You gain {Y%ld{x quest point%s!\n\r",
                        amount, amount == 1 ? "" : "s");
                break;
            }

            case QUEST_REWARD_CURRENCY:
            {
                long amount = UMAX(reward->amount, 0);
                bool is_gold;
                if (amount <= 0)
                    break;
                is_gold = !IS_NULLSTR(reward->currency)
                    && !str_prefix(reward->currency, "gold");
                if (is_gold)
                    ch->gold += amount;
                else
                    ch->silver += amount;
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                else
                    printf_to_char(ch, "You receive {Y%ld{x %s coin%s!\n\r",
                        amount,
                        is_gold ? "gold" : "silver",
                        amount == 1 ? "" : "s");
                break;
            }

            case QUEST_REWARD_REPUTATION:
            {
                REPUTATION_INDEX_DATA *repIndex;
                long given = 0;
                if (!reward->target_wnum.pArea || reward->target_wnum.vnum < 1)
                    break;
                repIndex = get_reputation_index_wnum(reward->target_wnum);
                if (!repIndex)
                    break;
                gain_reputation(ch, repIndex, reward->amount, NULL, &given, false);
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                else if (given != 0)
                    printf_to_char(ch, "You gain {Y%ld{x reputation with {W%s{x.\n\r",
                        given, repIndex->name);
                break;
            }

            case QUEST_REWARD_TOKEN:
            {
                TOKEN_INDEX_DATA *token_index;
                if (!reward->target_wnum.pArea || reward->target_wnum.vnum < 1)
                    break;
                token_index = get_token_index(
                    reward->target_wnum.pArea, reward->target_wnum.vnum);
                if (!token_index)
                    break;
                if (!give_token(token_index, ch, NULL, NULL))
                    break;
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                break;
            }

            case QUEST_REWARD_ITEM:
            {
                OBJ_INDEX_DATA *obj_index;
                OBJ_DATA *obj;
                long count = UMAX(reward->amount, 1);
                long i;
                if (!reward->target_wnum.pArea || reward->target_wnum.vnum < 1)
                    break;
                obj_index = get_obj_index(
                    reward->target_wnum.pArea, reward->target_wnum.vnum);
                if (!obj_index)
                    break;
                for (i = 0; i < count; i++)
                {
                    obj = create_object(obj_index, 0, true);
                    if (obj)
                        obj_to_char(obj, ch);
                }
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                else
                    printf_to_char(ch, "You receive {C%s{x as a reward.\n\r",
                        obj_index->short_descr);
                break;
            }

            case QUEST_REWARD_SCRIPT:
            {
                if (IS_NULLSTR(reward->script))
                    break;
                if (!IS_NULLSTR(reward->display_string))
                    printf_to_char(ch, "%s\n\r", reward->display_string);
                quest_runtime_fire_stage_script(run, reward->script);
                break;
            }
        }
    }
}


bool quest_runtime_complete_run(QUEST_DATA *run, const char *reason_phrase)
{
    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    run->run_status = QUEST_RUN_STATUS_COMPLETED;
    run->completed_at = current_time;
    run->failed_at = 0;
    run->abandoned_at = 0;
    quest_runtime_apply_rewards(run, NULL);
    quest_runtime_fire_quest_lifecycle_trigger(run, TRIG_QUEST_COMPLETED,
        IS_NULLSTR(reason_phrase) ? "forced" : reason_phrase);
    quest_runtime_record_terminal_history(run);
    return true;
}


bool quest_runtime_fail_run(QUEST_DATA *run, int failed_status, const char *reason_phrase)
{
    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return false;

    quest_runtime_mark_run_failed(run, failed_status,
        IS_NULLSTR(reason_phrase) ? "failed" : reason_phrase);
    return true;
}


static int quest_runtime_apply_objective_event(QUEST_DATA *run, int objective_type, WNUM target_wnum, int delta)
{
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    int updates = 0;
    int stage_id_before;

    if (!run || run->run_status != QUEST_RUN_STATUS_ACTIVE)
        return 0;

    stage = quest_runtime_get_current_stage(run);
    if (!stage)
    {
        if (!quest_runtime_try_advance_stage(run))
            return 0;
        stage = quest_runtime_get_current_stage(run);
        if (!stage)
            return 0;
    }

    stage_id_before = stage->id;

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        if (objective->objective_type != objective_type)
            continue;

        if (!quest_runtime_objective_target_matches(run, objective, target_wnum))
            continue;

        if (quest_runtime_update_objective_progress(run, objective->id, delta))
            updates++;

        if (run->run_status != QUEST_RUN_STATUS_ACTIVE || run->current_stage_id != stage_id_before)
            break;
    }

    return updates;
}


bool quest_runtime_bind_index_v2(QUEST_DATA *run, WNUM wnum)
{
    if (!run || !wnum.pArea || wnum.vnum < 1)
        return false;

    if (!get_quest_index_v2_wnum(wnum))
        return false;

    run->quest_index_v2_auid = wnum.pArea->uid;
    run->quest_index_v2_vnum = wnum.vnum;
    return true;
}


void quest_runtime_clear_objective_states(QUEST_DATA *run)
{
    QUEST_OBJECTIVE_STATE_V2_DATA *state;
    QUEST_OBJECTIVE_STATE_V2_DATA *next;

    if (!run)
        return;

    state = run->objective_states;
    while (state)
    {
        next = state->next;
        free_quest_objective_state_v2(state);
        state = next;
    }

    run->objective_states = NULL;
}


QUEST_OBJECTIVE_STATE_V2_DATA *quest_runtime_get_objective_state(QUEST_DATA *run, int objective_id, bool create_if_missing)
{
    QUEST_OBJECTIVE_STATE_V2_DATA *state;

    if (!run || objective_id < 1)
        return NULL;

    for (state = run->objective_states; state != NULL; state = state->next)
    {
        if (state->objective_id == objective_id)
            return state;
    }

    if (!create_if_missing)
        return NULL;

    state = new_quest_objective_state_v2();
    state->objective_id = objective_id;
    state->progress = 0;
    state->complete = false;
    state->next = run->objective_states;
    run->objective_states = state;
    return state;
}


bool quest_runtime_set_stage(QUEST_DATA *run, int stage_id)
{
    QUEST_INDEX_V2_DATA *quest_index_v2;
    QUEST_STAGE_INDEX_V2_DATA *stage;
    QUEST_STAGE_INDEX_V2_DATA *previous_stage;
    QUEST_OBJECTIVE_INDEX_V2_DATA *objective;
    unsigned long long stage_seed;

    if (!run || stage_id < 1)
        return false;

    quest_index_v2 = quest_runtime_get_index_v2(run);
    if (!quest_index_v2)
        return false;

    stage = quest_index_v2_get_stage(quest_index_v2, stage_id);
    if (!stage)
        return false;

    previous_stage = quest_runtime_get_current_stage(run);

    if (previous_stage && previous_stage->id != stage->id && !IS_NULLSTR(previous_stage->on_exit_script))
        quest_runtime_fire_stage_script(run, previous_stage->on_exit_script);

    if (run->current_stage_id < 1)
        quest_runtime_clear_target_bindings(run);

    stage_seed = quest_runtime_seed_for_stage(run, stage_id);
    if (stage->stage_source == QUEST_STAGE_SOURCE_GENERATED && stage->generator_salt != 0)
        stage_seed = quest_runtime_mix_seed(stage_seed ^ stage->generator_salt);
    run->current_stage_seed = stage_seed;
    run->current_stage_generation = 0;
    run->current_stage_commenced = 0;

    quest_runtime_clear_objective_states(run);

    for (objective = stage->objectives; objective != NULL; objective = objective->next)
    {
        QUEST_OBJECTIVE_STATE_V2_DATA *state = quest_runtime_get_objective_state(run, objective->id, true);
        if (!state)
            return false;
        state->progress = 0;
        state->complete = false;
    }

    if (!quest_runtime_compile_generated_stage(run, stage, stage_seed))
        return false;

    run->current_stage_id = stage_id;

    if (stage->auto_commence)
    {
        if (!quest_runtime_commence_current_stage(run))
            return false;
    }

    if (!IS_NULLSTR(stage->on_enter_script))
        quest_runtime_fire_stage_script(run, stage->on_enter_script);

    return true;
}


bool is_quest_item(OBJ_DATA *obj)
{
    if (!obj || !obj->pIndexData) {
        return false;
    }

    return is_quest_shop_object(obj->pIndexData);
}


QUEST_INDEX_DATA *get_quest_index(long vnum)
{
    QUEST_INDEX_DATA *quest_index;

    for (quest_index = quest_index_list; quest_index != NULL;
          quest_index = quest_index->next)
    {
        if (quest_index->vnum == vnum)
            return quest_index;
    }

    return NULL;
}


QUEST_INDEX_DATA *get_quest_index_wnum(WNUM wnum)
{
    QUEST_INDEX_DATA *quest_index;

    if (!wnum.pArea || wnum.vnum < 1)
        return NULL;

    for (quest_index = quest_index_list; quest_index != NULL; quest_index = quest_index->next)
    {
        if (quest_index->area == wnum.pArea && quest_index->vnum == wnum.vnum)
            return quest_index;
    }

    return NULL;
}


void check_quest_part_complete(CHAR_DATA *ch, int type)
{
}

CHAR_DATA *get_renewer_here(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mob;

    if (argument[0] == '\0')
    {
        for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
        {
            if (IS_NPC(mob) && IS_SET(mob->act[1], ACT2_RENEWER))
            {
                return mob;
            }
        }

        send_to_char("Renew with whom?\n\r", ch);
        return NULL;
    }
    else
    {
        if ((mob = get_char_room(ch, NULL, argument)) == NULL)
        {
            send_to_char("They aren't here.\n\r", ch);
            return NULL;
        }

        if (!IS_NPC(mob) || !IS_SET(mob->act[1], ACT2_RENEWER))
        {
            // Make a tell?
            act("You cannot do that with $N.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return NULL;
        }

        return mob;
    }

}

// RENEW PET[ <RENEWER>]
// RENEW MOUNT[ <RENEWER>]
// RENEW GUARD <MOBILE>[ <RENEWER>]
// RENEW OBJECT <OBJECT>[ <RENEWER>]
// RENEW OTHER <KEYWORD>[ <RENEWER>]
void do_renew(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mob;
    OBJ_DATA *obj;
    int cost;
    char buf[MSL+1];
    char arg1[MIL+1];
    char arg2[MIL+1];
    char arg3[MIL+1];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0')
    {
        send_to_char("Renew what?\n\r", ch);
        send_to_char("RENEW PET[ <RENEWER>]              to renew your pet.\n\r", ch);
        send_to_char("RENEW MOUNT[ <RENEWER>]            to renew your mount.\n\r", ch);
        send_to_char("RENEW GUARD <MOBILE>[ <RENEWER>]   to renew one of your guards.\n\r", ch);
        send_to_char("RENEW OBJECT <OBJECT>[ <RENEWER>]  to renew an item.\n\r", ch);
        send_to_char("RENEW CUSTOM <KEYWORD>[ <RENEWER>] to renew any custom service or good.\n\r", ch);
        send_to_char("RENEW LIST[ <RENEWER>]             asks for a list of things that can be renewed.\n\r", ch);
        return;
    }


    if( !str_prefix(arg1, "pet") )
    {
        mob = get_renewer_here(ch, arg2);
        if( mob == NULL )
            return;

        if( ch->pet == NULL )
        {
            send_to_char("You don't have a pet.\n\r", ch);
            return;
        }

        // Requires a pet
        if( mob->shop != NULL )
        {
            cost = ch->pet->tot_level * ch->pet->tot_level;
            mob->tempstore[0] = UMAX(cost, 1);
            mob->tempstore[1] = STOCK_PET;
            if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
                return;

            // Check QUESTPOINTS
            cost = mob->tempstore[0];
            if( cost <= 0 )
            {
                sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if (ch->questpoints < cost)
            {
                sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
                do_say(mob, buf);
                return;
            }

            mob->tempstore[0] = cost;
            mob->tempstore[1] = STOCK_PET;
            p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_RENEW, NULL);

            sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
            act(buf, ch->pet, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD, NULL, NULL);
            ch->questpoints -= cost;
        }
        else
        {
            sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
            do_say(mob, buf);
        }
        return;
    }
    else if(!str_prefix(arg1, "mount") )
    {
        mob = get_renewer_here(ch, arg2);
        if( mob == NULL )
            return;

        if( !MOUNTED(ch) )
        {
            send_to_char("You aren't mounted.\n\r", ch);
            return;
        }

        if( !str_cmp(ch->mount->owner, ch->name) )
        {
            // Yet?
            send_to_char("Personal mounts cannot be renewed.\n\r", ch);
            return;
        }

        if( mob->shop != NULL )
        {
            cost = 25 * ch->mount->tot_level * ch->mount->tot_level / 10;
            mob->tempstore[0] = UMAX(cost, 1);
            mob->tempstore[1] = STOCK_MOUNT;
            if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
                return;

            // Check QUESTPOINTS
            cost = mob->tempstore[0];
            if( cost <= 0 )
            {
                sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if (ch->questpoints < cost)
            {
                sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
                do_say(mob, buf);
                return;
            }

            mob->tempstore[0] = cost;
            mob->tempstore[1] = STOCK_MOUNT;
            p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_RENEW, NULL);

            sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
            act(buf, ch->mount, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD, NULL, NULL);
            ch->questpoints -= cost;
        }
        else
        {
            sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
            do_say(mob, buf);
        }

        return;
    }
    else if(!str_prefix(arg1, "guard") )
    {
        mob = get_renewer_here(ch, arg3);
        if( mob == NULL )
            return;

        CHAR_DATA *guard = get_char_room(ch, NULL, arg2);
        if( guard == NULL )
        {
            send_to_char("They aren't here.\n\r", ch);
            return;
        }

        if( guard->master != ch )
        {
            send_to_char("They are not following you.\n\r", ch);
            return;
        }

        if( mob->shop != NULL )
        {
            cost = 5 * guard->tot_level * guard->tot_level;
            mob->tempstore[0] = UMAX(cost, 1);
            mob->tempstore[1] = STOCK_GUARD;
            if(p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
                return;

            // Check QUESTPOINTS
            cost = mob->tempstore[0];
            if( cost <= 0 )
            {
                sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if (ch->questpoints < cost)
            {
                sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
                do_say(mob, buf);
                return;
            }

            mob->tempstore[0] = cost;
            mob->tempstore[1] = STOCK_GUARD;
            p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_RENEW, NULL);

            sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
            act(buf, guard, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD, NULL, NULL);
            ch->questpoints -= cost;
        }
        else
        {
            sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
            do_say(mob, buf);
        }

        return;
    }
    else if( !str_prefix(arg1, "object") )
    {
        mob = get_renewer_here(ch, arg3);
        if( mob == NULL )
            return;

        if ((obj = get_obj_carry(ch, arg2, ch)) == NULL)
        {
            sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }

        cost = obj->cost/10;
        mob->tempstore[0] = UMAX(cost, 1);
        mob->tempstore[1] = STOCK_OBJECT;
        if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRERENEW, NULL) <= 0)
            return;

        cost = mob->tempstore[0];
        if( cost <= 0 )
        {
            sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }


        if (ch->questpoints < cost)
        {
            sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that item.", pers(ch, mob), cost);
            do_say(mob, buf);
            return;
        }

        mob->tempstore[0] = cost;
        mob->tempstore[1] = STOCK_OBJECT;
        p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_RENEW, NULL);

        sprintf(buf, "{YYou renew $p with $N for %d quest points.{x", cost);
        act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        ch->questpoints -= cost;

        return;
    }
    else if( !str_prefix(arg1, "custom") )
    {
        mob = get_renewer_here(ch, arg3);
        if( mob == NULL )
            return;

        mob->tempstore[0] = 0;	// Customs REQUIRE the script to specify the cost
        mob->tempstore[1] = STOCK_CUSTOM;
        if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRERENEW, arg2) <= 0)
            return;

        cost = mob->tempstore[0];
        if( cost <= 0 )
        {
            sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }


        if (ch->questpoints < cost)
        {
            sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
            do_say(mob, buf);
            return;
        }

        mob->tempstore[0] = cost;
        mob->tempstore[1] = STOCK_CUSTOM;
        free_string(mob->tempstring);
        mob->tempstring = &str_empty[0];
        p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW, arg2);

        if(!IS_NULLSTR(mob->tempstring))
        {
            sprintf(buf, "{YYou renew %s with $N for %d quest points.{x", mob->tempstring, cost);
        }
        else
        {
            sprintf(buf, "{YYou renew %s with $N for %d quest points.{x", arg2, cost);
        }
        act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        ch->questpoints -= cost;

        return;
    }
    else if(!str_prefix(arg1, "list"))
    {
        mob = get_renewer_here(ch, arg2);
        if( mob == NULL )
            return;


        act("{YYou ask $N for a list of things $E can renew.{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n asks $N for a list of things $E can renew.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW_LIST, NULL))
            return;

        sprintf(buf, "I don't really do anything special here.");
        do_say(mob, buf);
        return;
    }

/*
    if( mob->shop != NULL )
    {
        // ** Shopkeeper, find stock entries only **

        // Find the stock item to renew
        SHOP_STOCK_DATA *stock = get_stockonly_keeper(ch, mob, char *arg1);
        if( stock == NULL )
        {
            sprintf(buf, "Sorry %s, I do not stock that.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }

        obj = NULL;
        victim = NULL;
        switch(stock->type)
        {
        case STOCK_OBJECT:
            if( stock->obj == NULL )
            {
                sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if( (obj = get_obj_vnum_carry(ch, stock->obj->vnum, mob)) == NULL )
            {
                sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }
            break;
        case STOCK_PET:
            if( stock->mob == NULL || ch->pet == NULL )
            {
                sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if( ch->pet->pIndexData != stock->mob )
            {
                sprintf(buf, "You don't have that pet, %s.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }

            if( stock->duration > 0 )
            {
                // This needs
                if( !IS_SET(ch->pet->act[1], ACT2_HIRED) || (ch->pet->hired_to < 1) )
                {
                    sprintf(buf, "Sorry %s, but you already own that pet.", pers(ch, mob));
                    do_say(mob, buf);
                    return;
                }
            }


            if( (obj = get_obj_vnum_carry(ch, stock->obj->vnum, mob)) == NULL )
            {
                sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
                do_say(mob, buf);
                return;
            }
            break;


        // Call PRERENEW
        mob->tempstore[0] = 0;
        mob->tempstore[1] = stock->type;
        mob->tempstore[2] = stock->entity.wnum.vnum;
        if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
            return;


        // Check QUESTPOINTS
        cost = mob->tempstore[0];
        if( cost <= 0 )
        {
            sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }

        if (ch->questpoints < cost)
        {
            sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
            do_say(mob, buf);
            return;
        }


        sprintf(buf, "You renew $p to $N for %d quest points.", cost);
        act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

        // Call RENEW
    }
    else
    {
        if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
        {
            sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }

        cost = obj->cost/10;
        cost = UMAX(cost, 1);

        mob->tempstore[0] = cost;
        if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_PRERENEW, NULL) <= 0)
            return;

        cost = mob->tempstore[0];
        if( cost <= 0 )
        {
            sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
            do_say(mob, buf);
            return;
        }


        if (ch->questpoints < cost)
        {
            sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that item.", pers(ch, mob), cost);
            do_say(mob, buf);
            return;
        }

        sprintf(buf, "You renew $p to $N for %d quest points.", cost);
        act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);

        act("$n shows $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
        act("$N chants a mantra over $p, then hands it back to $n.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

        p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_RENEW, NULL);

        ch->questpoints -= cost;
    }
*/

}

