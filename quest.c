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

static bool check_quest_custom_task_run(CHAR_DATA *ch, QUEST_DATA *run, int task, bool show);
static bool generate_quest_from_object(CHAR_DATA *ch, OBJ_DATA *questobj);
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
static OBJ_DATA *quest_runtime_find_object_target_instance(WNUM target_wnum, AREA_DATA *scope_area);
static bool quest_runtime_attach_objective_target_token(QUEST_DATA *run, QUEST_OBJECTIVE_INDEX_V2_DATA *objective, QUEST_OBJECTIVE_STATE_V2_DATA *state);
static void quest_runtime_clear_target_bindings(QUEST_DATA *run);
static WNUM quest_runtime_get_target_binding(QUEST_DATA *run, const char *name);
static bool quest_runtime_set_target_binding(QUEST_DATA *run, const char *name, WNUM target_wnum);
static WNUM quest_runtime_resolve_objective_target_reference(QUEST_DATA *run, QUEST_INDEX_V2_DATA *quest_index_v2,
    QUEST_STAGE_INDEX_V2_DATA *stage, QUEST_OBJECTIVE_INDEX_V2_DATA *objective);

#define QUEST_LIST_MAX_ENTRIES 128

static CHAR_DATA *quest_find_room_mob_giver(CHAR_DATA *ch, const char *name)
{
    CHAR_DATA *mob;

    if (!ch || !ch->in_room)
        return NULL;

    if (!IS_NULLSTR(name)) {
        mob = get_char_room(ch, NULL, (char *)name);
        if (mob && IS_NPC(mob) && mob->pIndexData && mob->pIndexData->pQuestor)
            return mob;
        return NULL;
    }

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room) {
        if (IS_NPC(mob) && mob->pIndexData && mob->pIndexData->pQuestor)
            return mob;
    }

    return NULL;
}


static bool generate_quest_from_object(CHAR_DATA *ch, OBJ_DATA *questobj)
{
    QUEST_DATA *active_run;
    QUEST_PART_DATA *part;
    int parts;
    int i;

    if (!ch || IS_NPC(ch) || !questobj || !questobj->pIndexData)
        return false;

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

    questobj->tempstore[0] = parts;
    questobj->tempstore[1] = 0;

    if (p_percent_trigger(NULL, questobj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREQUEST, NULL) > 0)
        return false;

    parts = questobj->tempstore[0];
    if (parts < 1)
        parts = 1;

    for (i = 0; i < parts; i++)
    {
        part = new_quest_part();
        part->next = active_run->parts;
        active_run->parts = part;
        part->index = parts - i;

        questobj->tempstore[0] = parts - i;
        if (p_percent_trigger(NULL, questobj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_PART, NULL) <= 0)
            return false;
    }

    return true;
}

static OBJ_DATA *quest_find_room_object_giver(CHAR_DATA *ch, const char *name)
{
    OBJ_DATA *obj;

    if (!ch || !ch->in_room)
        return NULL;

    if (!IS_NULLSTR(name)) {
        obj = get_obj_here(ch, NULL, (char *)name);
        if (obj && obj->pIndexData && obj->progs)
            return obj;
        return NULL;
    }

    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content) {
        if (obj->pIndexData && obj->progs)
            return obj;
    }

    return NULL;
}

static int quest_collect_mob_offerings(MOB_INDEX_DATA *mob_index, QUEST_INDEX_DATA **results, int max_results)
{
    QUEST_LIST *entry;
    int count = 0;

    if (!mob_index || !results || max_results < 1)
        return 0;

    for (entry = mob_index->quests; entry != NULL && count < max_results; entry = entry->next) {
        QUEST_INDEX_DATA *index = get_quest_index(entry->vnum);
        if (index)
            results[count++] = index;
    }

    return count;
}

static int quest_collect_object_offerings(OBJ_INDEX_DATA *obj_index, QUEST_INDEX_DATA **results, int max_results)
{
    QUEST_INDEX_DATA *index;
    int count = 0;

    if (!obj_index || !obj_index->area || !results || max_results < 1)
        return 0;

    for (index = quest_index_list; index != NULL && count < max_results; index = index->next) {
        if (index->area == obj_index->area)
            results[count++] = index;
    }

    return count;
}

static QUEST_INDEX_DATA *quest_find_offering_by_name(QUEST_INDEX_DATA **offerings, int offering_count, const char *name)
{
    int i;

    if (!offerings || offering_count < 1 || IS_NULLSTR(name))
        return NULL;

    if (is_number((char *)name)) {
        i = atoi(name);
        if (i >= 1 && i <= offering_count)
            return offerings[i - 1];
    }

    for (i = 0; i < offering_count; i++) {
        QUEST_INDEX_DATA *index = offerings[i];
        if (index && index->name && !str_infix((char *)name, index->name))
            return index;
    }

    return NULL;
}

static void quest_set_wnum(WNUM_LOAD *load, WNUM *wnum, AREA_DATA *area, long vnum)
{
    if (!load || !wnum) return;

    load->auid = area ? area->uid : 0;
    load->vnum = vnum;
    wnum->pArea = area;
    wnum->vnum = vnum;
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

static bool quest_target_scope_supported_for_player(CHAR_DATA *ch, int scope, bool show_message)
{
    if (scope == QUEST_TARGET_SCOPE_CHARACTER)
        return true;

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
        if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0) {
            CHAR_DATA *group_owner = ch->leader ? ch->leader : ch;
            run->scope_owner_id[0] = group_owner->id[0];
            run->scope_owner_id[1] = group_owner->id[1];
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
        if (ch->leader)
            return uid_match(run->scope_owner_id, ch->leader->id) || uid_match(run->scope_owner_id, ch->id);
        return uid_match(run->scope_owner_id, ch->id);

    case QUEST_TARGET_SCOPE_CHURCH:
        return (ch->church && run->scope_owner_uid > 0 && ch->church->uid == run->scope_owner_uid);

    default:
        return false;
    }
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
        if (show_message)
            send_to_char("Run selector must be a quest index or run id.\n\r", ch);
        return NULL;
    }

    target = atol(selector);
    run = quest_runtime_get_run_by_index(ch, (int)target);
    if (!run)
        run = quest_runtime_get_run_by_id(ch, target);

    if (!run) {
        if (show_message)
            send_to_char("No active quest run matches that selector.\n\r", ch);
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

static void quest_runtime_normalize_target_scope(CHAR_DATA *ch, QUEST_DATA *run)
{
    if (!ch || !run)
        return;

    if (run->target_scope < QUEST_TARGET_SCOPE_CHARACTER
        || run->target_scope > QUEST_TARGET_SCOPE_CHURCH)
        run->target_scope = QUEST_TARGET_SCOPE_CHARACTER;

    quest_scope_owner_seed(ch, run);
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
        send_to_char("QUEST commands: LOG LIST FOCUS POINTS INFO TIME COMMENCE REQUEST CANCEL COMPLETE.\n\r", ch);
        send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
        return;
    }

    //
    // QUEST LOG
    //
    if (!str_cmp(arg1, "log"))
    {
        long age_minutes = 0;
        int index = 1;
        bool shown_any = false;

        if (!IS_QUESTING(ch))
        {
            send_to_char("You have no active quests.\n\r", ch);
            return;
        }

        if (ch->quest_runtime.focused_run_id <= 0)
            ch->quest_runtime.focused_run_id = active_run_id;

        for (run = ch->quest; run != NULL; run = run->next, index++)
        {
            if (!quest_run_accessible_by_player(ch, run, false))
                continue;

            bool focused = (ch->quest_runtime.focused_run_id == run->run_id);
            shown_any = true;

            if (run->generating)
                printf_to_char(ch, "[%d] %sRun %ld (Generating) [%s]\n\r", index, focused ? "* " : "  ", run->run_id, quest_target_scope_name(run->target_scope));
            else
                printf_to_char(ch, "[%d] %sRun %ld [%s]\n\r", index, focused ? "* " : "  ", run->run_id, quest_target_scope_name(run->target_scope));

            if (run->started_at > 0)
                age_minutes = UMAX(0, (long)((current_time - run->started_at) / 60));
            else
                age_minutes = 0;

            if (run->quest_index_v2_auid > 0 && run->quest_index_v2_vnum > 0)
                printf_to_char(ch,
                    "      index(v2): {%ld#%ld{x  started: %ld minute%s ago\n\r",
                    run->quest_index_v2_auid,
                    run->quest_index_v2_vnum,
                    age_minutes,
                    age_minutes == 1 ? "" : "s");
            else if (run->quest_index_auid > 0 && run->quest_index_vnum > 0)
                printf_to_char(ch,
                    "      template: {%ld#%ld{x  started: %ld minute%s ago\n\r",
                    run->quest_index_auid,
                    run->quest_index_vnum,
                    age_minutes,
                    age_minutes == 1 ? "" : "s");
            else
                printf_to_char(ch,
                    "      template: {none/generate{x  started: %ld minute%s ago\n\r",
                    age_minutes,
                    age_minutes == 1 ? "" : "s");

            if (IS_IMMORTAL(ch))
                printf_to_char(ch,
                    "      seed: %llu  stage_seed: %llu  stage_gen: %d  stage_commenced: %d\n\r",
                    run->generation_seed,
                    run->current_stage_seed,
                    run->current_stage_generation,
                    run->current_stage_commenced);
        }

        if (!shown_any)
        {
            send_to_char("You have no active quests available to your character scope.\n\r", ch);
            return;
        }

        printf_to_char(ch, "Use 'quest focus <index|run_id>' to focus a quest.\n\r");
        return;
    }

    //
    // QUEST LIST
    //
    if (!str_cmp(arg1, "list"))
    {
        QUEST_INDEX_DATA *offerings[QUEST_LIST_MAX_ENTRIES];
        QUEST_INDEX_DATA *index;
        CHAR_DATA *giver_mob = NULL;
        OBJ_DATA *giver_obj = NULL;
        char target_name[MSL];
        int offering_count = 0;
        int i;

        target_name[0] = '\0';
        if (!IS_NULLSTR(arg2)) {
            strncpy(target_name, arg2, sizeof(target_name) - 1);
            target_name[sizeof(target_name) - 1] = '\0';
            if (!IS_NULLSTR(argument)) {
                strncat(target_name, " ", sizeof(target_name) - strlen(target_name) - 1);
                strncat(target_name, argument, sizeof(target_name) - strlen(target_name) - 1);
            }
        }

        giver_mob = quest_find_room_mob_giver(ch, target_name[0] ? target_name : NULL);
        if (!giver_mob)
            giver_obj = quest_find_room_object_giver(ch, target_name[0] ? target_name : NULL);

        if (!giver_mob && !giver_obj) {
            send_to_char("No questgiver found here with that name.\n\r", ch);
            return;
        }

        if (giver_mob && giver_mob->pIndexData)
            offering_count = quest_collect_mob_offerings(giver_mob->pIndexData, offerings, QUEST_LIST_MAX_ENTRIES);
        else if (giver_obj && giver_obj->pIndexData)
            offering_count = quest_collect_object_offerings(giver_obj->pIndexData, offerings, QUEST_LIST_MAX_ENTRIES);

        if (giver_mob)
            printf_to_char(ch, "Available quests from {Y%s{x:\n\r", HANDLE(giver_mob));
        else
            printf_to_char(ch, "Available quests from {Y%s{x:\n\r", giver_obj->short_descr ? giver_obj->short_descr : "quest board");

        if (offering_count < 1) {
            send_to_char("  (No indexed quests are currently available.)\n\r", ch);
            return;
        }

        for (i = 0; i < offering_count; i++) {
            index = offerings[i];
            if (!index)
                continue;

            printf_to_char(ch, "  [{Y%d{x] %s ({%ld#%ld{x)\n\r",
                i + 1,
                IS_NULLSTR(index->name) ? "(unnamed quest)" : index->name,
                index->area ? index->area->uid : 0,
                index->vnum);
        }

        send_to_char("Use {Yquest request <quest name or #>{x to request one.\n\r", ch);
        return;
    }

    //
    // QUEST FOCUS
    //
    if (!str_cmp(arg1, "focus"))
    {
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
                    printf_to_char(ch, "Focused quest: %ld [%s]\n\r",
                        ch->quest_runtime.focused_run_id,
                        quest_target_scope_name(focused_run->target_scope));
                else
                    printf_to_char(ch, "Focused quest: %ld\n\r", ch->quest_runtime.focused_run_id);
            }
            return;
        }

        if (is_number(arg2)) {
            long target = atol(arg2);
            QUEST_DATA *target_run = quest_runtime_get_run_by_index(ch, (int)target);
            if (!target_run)
                target_run = quest_runtime_get_run_by_id(ch, target);
            if (!target_run)
            {
                send_to_char("No active quest matches that index.\n\r", ch);
                return;
            }
            if (!quest_run_accessible_by_player(ch, target_run, true))
                return;
            ch->quest_runtime.focused_run_id = target_run->run_id;
            send_to_char("Focused quest updated.\n\r", ch);
            return;
        }

        send_to_char("Focus currently supports quest index or run id.\n\r", ch);
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
                send_to_char("That quest stage cannot be commenced right now.\n\r", ch);
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
    // QUEST INFO
    //
    if (!str_cmp(arg1, "info"))
    {
        QUEST_PART_DATA *part;
        QUEST_DATA *focused_quest;
        int i;
        int total_parts;
        bool totally_complete = false;
        bool found = false;

        focused_quest = quest_runtime_get_focused_run(ch);

        if (ch->quest_runtime.focused_run_id <= 0)
            ch->quest_runtime.focused_run_id = active_run_id;

        if (focused_quest == NULL)
        {
            printf_to_char(ch,
                "Focused quest %ld is not currently active. Use 'quest focus %ld'.\n\r",
                ch->quest_runtime.focused_run_id,
                active_run_id);
            return;
        }

        if (!quest_run_accessible_by_player(ch, focused_quest, true))
            return;

        printf_to_char(ch, "Focused run: {Y%ld{x [%s]\n\r",
            focused_quest->run_id,
            quest_target_scope_name(focused_quest->target_scope));

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

    //
    // Quest Request
    //
    if (!str_cmp(arg1, "request"))
    {
        QUEST_DATA *active_quest;
        QUEST_DATA *new_run;
        QUEST_DATA *existing_run;
        QUEST_INDEX_DATA *offerings[QUEST_LIST_MAX_ENTRIES];
        QUEST_INDEX_DATA *selected_index = NULL;
        CHAR_DATA *giver_mob = NULL;
        OBJ_DATA *giver_obj = NULL;
        char requested_name[MSL];
        int offering_count = 0;

        requested_name[0] = '\0';
        if (!IS_NULLSTR(arg2)) {
            strncpy(requested_name, arg2, sizeof(requested_name) - 1);
            requested_name[sizeof(requested_name) - 1] = '\0';
            if (!IS_NULLSTR(argument)) {
                strncat(requested_name, " ", sizeof(requested_name) - strlen(requested_name) - 1);
                strncat(requested_name, argument, sizeof(requested_name) - strlen(requested_name) - 1);
            }
        }

        giver_mob = quest_find_room_mob_giver(ch, NULL);
        if (!giver_mob)
            giver_obj = quest_find_room_object_giver(ch, NULL);

        if (!giver_mob && !giver_obj)
        {
            send_to_char("You can't do that here\n\r", ch);
            return;
        }

        if (giver_mob && giver_mob->pIndexData)
            offering_count = quest_collect_mob_offerings(giver_mob->pIndexData, offerings, QUEST_LIST_MAX_ENTRIES);
        else if (giver_obj && giver_obj->pIndexData)
            offering_count = quest_collect_object_offerings(giver_obj->pIndexData, offerings, QUEST_LIST_MAX_ENTRIES);

        if (!IS_NULLSTR(requested_name)) {
            selected_index = quest_find_offering_by_name(offerings, offering_count, requested_name);
            if (!selected_index) {
                if (giver_mob)
                    printf_to_char(ch, "No available quest named '%s' from %s. Use {Yquest list{x first.\n\r", requested_name, HANDLE(giver_mob));
                else
                    printf_to_char(ch, "No available quest named '%s' on %s. Use {Yquest list{x first.\n\r",
                        requested_name,
                        giver_obj && giver_obj->short_descr ? giver_obj->short_descr : "that board");
                return;
            }
        }

        if (!IS_AWAKE(ch))
        {
            send_to_char("In your dreams, or what?\n\r", ch);
            return;
        }

        if (giver_mob) {
            act("$n asks $N for a quest.", ch, giver_mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act ("You ask $N for a quest.",ch, giver_mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        } else {
            act("$n examines $p for available quests.", ch, NULL, NULL, giver_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You check $p for available quests.", ch, NULL, NULL, giver_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }

        for (existing_run = ch->quest; existing_run != NULL; existing_run = existing_run->next)
        {
            if (existing_run->generating)
            {
                if (giver_mob) {
                    sprintf(buf, "Finish preparing your current pending quest first, %s.", HANDLE(ch));
                    do_say(giver_mob, buf);
                } else {
                    send_to_char("Finish preparing your current pending quest first.\n\r", ch);
                }
                return;
            }
        }

        if (IS_DEAD(ch))
        {
            if (giver_mob) {
                sprintf(buf, "You must come back to the world of the living first, %s.", HANDLE(ch));
                do_say(giver_mob, buf);
            } else {
                send_to_char("You must come back to the world of the living first.\n\r", ch);
            }
            return;
        }

        if (!IS_IMMORTAL(ch)
            && game_settings.telnet_port != PORT_RAE
            && ch->quest_runtime.mission_allowance < 1)
        {
            sprintf(buf, "You're very brave, %s, but let someone else have a chance.", ch->name);
            if (giver_mob == NULL)
            {
                send_to_char("You need at least one mission allowance.\n\r", ch);
                return;
            }

            do_say(giver_mob, buf);
            sprintf(buf, "You need at least one mission allowance.");
            do_say(giver_mob, buf);
            return;
        }

        new_run = new_quest();
        new_run->next = ch->quest;
        ch->quest = new_run;

        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_attach_active_quest(ch, 0, 0);
        active_quest = quest_runtime_get_focused_run(ch);

        if (!active_quest)
        {
            send_to_char("Unable to initialize quest runtime state.\n\r", ch);
            return;
        }

        active_quest->quest_index_auid = selected_index && selected_index->area ? selected_index->area->uid : 0;
        active_quest->quest_index_vnum = selected_index ? selected_index->vnum : 0;
        active_quest->target_scope = QUEST_TARGET_SCOPE_CHARACTER;
        active_quest->scope_owner_id[0] = 0;
        active_quest->scope_owner_id[1] = 0;
        active_quest->scope_owner_uid = 0;
        quest_scope_owner_seed(ch, active_quest);

        if (giver_mob) {
            active_quest->questgiver_type = QUESTOR_MOB;
            quest_set_wnum(&active_quest->questgiver_load, &active_quest->questgiver_wnum,
                giver_mob->pIndexData->area, giver_mob->pIndexData->vnum);
            active_quest->questreceiver_type = QUESTOR_MOB;
            quest_set_wnum(&active_quest->questreceiver_load, &active_quest->questreceiver_wnum,
                giver_mob->pIndexData->area, giver_mob->pIndexData->vnum);
        } else {
            active_quest->questgiver_type = QUESTOR_OBJ;
            quest_set_wnum(&active_quest->questgiver_load, &active_quest->questgiver_wnum,
                giver_obj->pIndexData->area, giver_obj->pIndexData->vnum);
            active_quest->questreceiver_type = QUESTOR_OBJ;
            quest_set_wnum(&active_quest->questreceiver_load, &active_quest->questreceiver_wnum,
                giver_obj->pIndexData->area, giver_obj->pIndexData->vnum);
        }

        if ((giver_mob && generate_quest(ch, giver_mob))
            || (giver_obj && generate_quest_from_object(ch, giver_obj)))
        {
            active_quest->generating = false;

            if (giver_mob) {
                sprintf(buf, "Thank you, brave %s!", HANDLE(ch));
                do_say(giver_mob, buf);
            } else {
                send_to_char("You have accepted a quest from the board.\n\r", ch);
            }
        }
        else
        {
            if (giver_mob) {
                sprintf(buf, "I'm sorry, %s, but I don't have any quests for you to do. Try again later.", ch->name);
                do_say(giver_mob, buf);
            } else {
                send_to_char("No quests are currently available from this board.\n\r", ch);
            }
            quest_runtime_detach_run(ch, active_quest);
            return;
        }

        if (IS_QUESTING(ch))
        {
            QUEST_PART_DATA *qp;

            ch->countdown = 0;

            for (qp = active_quest->parts; qp != NULL; qp = qp->next)
                ch->countdown += qp->minutes;

            ch->quest_runtime.expiry_modes |= QUEST_EXPIRY_COUNTDOWN;
            ch->quest_runtime.expiry_countdown_minutes = ch->countdown;
            if (!(ch->quest_runtime.expiry_modes & QUEST_EXPIRY_WALL_TIME))
                ch->quest_runtime.expires_at = 0;

            sprintf(buf, "You have %d minutes to complete this quest.", ch->countdown);
            if (giver_mob)
                do_say(giver_mob, buf);
            else
                printf_to_char(ch, "%s\n\r", buf);

            if (!IS_IMMORTAL(ch) && game_settings.telnet_port != PORT_RAE
                && ch->quest_runtime.mission_allowance > 0)
                ch->quest_runtime.mission_allowance--;
        }

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

                quest_runtime_detach_run(ch, active_quest);

                obj->tempstore[0] = 10;
                p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    quest_runtime_reset_expiration(ch);
                }
            }
            else if (room)
            {
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
        int *tempstores;

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
            ch->pcdata->quests_completed++;
            leaderboard_update_score(REPORT_TOP_QUESTS, ch->name, (double)ch->pcdata->quests_completed);
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
        send_to_char("QUEST commands: LOG LIST FOCUS POINTS INFO TIME COMMENCE REQUEST CANCEL COMPLETE.\n\r", ch);
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
        if (ch->quest_runtime.expiry_modes != QUEST_EXPIRY_NONE)
        {
            if (ch->quest_runtime.expiry_modes & (QUEST_EXPIRY_COUNTDOWN | QUEST_EXPIRY_WALL_TIME))
                quest_runtime_tick_expiration(ch, current_time);

            if (quest_runtime_is_expired(ch, current_time)) {
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

        quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_COLLECT, target_wnum, 1);
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
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

        quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_KILL, target_wnum, 1);
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

        quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_TRAVEL, target_wnum, 1);
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
        quest_runtime_apply_objective_event(run, QUEST_OBJECTIVE_CUSTOM_SCRIPT, (WNUM){ .pArea = NULL, .vnum = 0 }, 1);

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

            if (!IS_NULLSTR(objective->destination_variable_name))
                quest_runtime_set_target_binding(run, objective->destination_variable_name, selected_destination);
        }
    }

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

    if (!run || run->scope_owner_id[0] == 0 || run->scope_owner_id[1] == 0)
        return NULL;

    iterator_start(&it, loaded_chars);
    while ((ch = (CHAR_DATA *)iterator_nextdata(&it)) != NULL)
    {
        if (IS_NPC(ch))
            continue;

        if (uid_match(ch->id, run->scope_owner_id))
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
    WNUM target_wnum;
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

    target_wnum = state ? state->selected_target_wnum : wnum_zero;
    if (!target_wnum.pArea || target_wnum.vnum < 1)
        target_wnum = objective->target_wnum;
    if (!target_wnum.pArea || target_wnum.vnum < 1)
        return false;

    scope_area = target_wnum.pArea;

    if (get_mob_index(target_wnum.pArea, target_wnum.vnum) != NULL)
    {
        if (state && (!state->selected_target_wnum.pArea || state->selected_target_wnum.vnum < 1))
        {
            state->selected_target_load.auid = target_wnum.pArea->uid;
            state->selected_target_load.vnum = target_wnum.vnum;
            state->selected_target_wnum = target_wnum;
        }

        if (state && !quest_runtime_spawn_missing_kill_target(run, state))
            return false;

        mob = quest_runtime_find_mob_target_instance(run, target_wnum, scope_area);
        if (!mob)
            return false;

        if (get_token_char(mob, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, mob, NULL, NULL) != NULL;
    }

    if (get_obj_index(target_wnum.pArea, target_wnum.vnum) != NULL)
    {
        obj = quest_runtime_find_object_target_instance(target_wnum, scope_area);
        if (!obj)
            return false;

        if (get_token_obj(obj, token_wnum.vnum, token_wnum.pArea, 1) != NULL)
            return true;

        return give_token(token_index, NULL, obj, NULL) != NULL;
    }

    room = get_room_index(target_wnum.pArea, target_wnum.vnum);
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
                    return false;
            }
        }

        if (objective->objective_type == QUEST_OBJECTIVE_TALK
            || objective->objective_type == QUEST_OBJECTIVE_CUSTOM_SCRIPT)
        {
            WNUM mob_target = state->selected_target_wnum;
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
                    return false;
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
    }

    run->current_stage_commenced = 1;
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

        if (stage->next_stage_id < 1)
        {
            run->run_status = QUEST_RUN_STATUS_COMPLETED;
            run->completed_at = current_time;
            run->failed_at = 0;
            run->abandoned_at = 0;
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
    new_progress = state->progress + delta;
    if (new_progress < 0)
        new_progress = 0;
    if (new_progress > required)
        new_progress = required;

    changed = (new_progress != state->progress);
    state->progress = new_progress;
    state->complete = (state->progress >= required);

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

    state->progress = quest_runtime_objective_required_count(objective);
    state->complete = true;
    quest_runtime_try_advance_stage(run);
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

