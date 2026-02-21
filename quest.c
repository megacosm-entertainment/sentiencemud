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
        if (scope == QUEST_TARGET_SCOPE_GROUP)
            send_to_char("Group-scoped quests are not enabled yet.\n\r", ch);
        else if (scope == QUEST_TARGET_SCOPE_CHURCH)
            send_to_char("Church-scoped quests are not enabled yet.\n\r", ch);
        else
            send_to_char("That quest scope is not currently supported.\n\r", ch);
    }

    return false;
}

static bool quest_run_accessible_by_player(CHAR_DATA *ch, QUEST_DATA *run, bool show_message)
{
    if (!ch || !run || IS_NPC(ch))
        return false;

    if (!quest_target_scope_supported_for_player(ch, run->target_scope, show_message))
        return false;

    if (run->target_scope == QUEST_TARGET_SCOPE_CHARACTER) {
        if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0) {
            run->scope_owner_id[0] = ch->id[0];
            run->scope_owner_id[1] = ch->id[1];
        }

        if (!uid_match(run->scope_owner_id, ch->id)) {
            if (show_message)
                send_to_char("That quest run does not belong to your character.\n\r", ch);
            return false;
        }
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

    if (run->target_scope == QUEST_TARGET_SCOPE_CHARACTER) {
        if (run->scope_owner_id[0] == 0 && run->scope_owner_id[1] == 0) {
            run->scope_owner_id[0] = ch->id[0];
            run->scope_owner_id[1] = ch->id[1];
        }
        run->scope_owner_uid = 0;
    }
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
        send_to_char("QUEST commands: LOG FOCUS POINTS INFO TIME COMMENCE REQUEST CANCEL COMPLETE.\n\r", ch);
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

            if (run->quest_index_auid > 0 && run->quest_index_vnum > 0)
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
            else
                printf_to_char(ch, "Focused quest: %ld\n\r", ch->quest_runtime.focused_run_id);
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
            sprintf(buf, "Focused run: {Y%ld{x (%d other active run%s).\n\r",
                focused_quest->run_id,
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
        else if (ch->countdown > 0)
        {
            other_count = UMAX(0, active_count - 1);
            sprintf(buf, "Focused run: {Y%ld{x (%d other active run%s).\n\r",
                focused_quest ? focused_quest->run_id : 0,
                other_count,
                other_count == 1 ? "" : "s");
            send_to_char(buf, ch);
            sprintf(buf, "Time left for current quest: {Y%d{x minutes.\n\r",
                ch->countdown);
            send_to_char(buf, ch);
        }
        else if (focused_quest)
        {
            other_count = UMAX(0, active_count - 1);
            sprintf(buf, "Focused run: {Y%ld{x (%d other active run%s).\n\r",
                focused_quest->run_id,
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
        int requested_scope = QUEST_TARGET_SCOPE_CHARACTER;

        if (!IS_NULLSTR(arg2))
        {
            if (!str_prefix(arg2, "character") || !str_prefix(arg2, "char") || !str_prefix(arg2, "single"))
                requested_scope = QUEST_TARGET_SCOPE_CHARACTER;
            else if (!str_prefix(arg2, "group"))
                requested_scope = QUEST_TARGET_SCOPE_GROUP;
            else if (!str_prefix(arg2, "church"))
                requested_scope = QUEST_TARGET_SCOPE_CHURCH;
            else
            {
                send_to_char("Syntax: quest request [character|group|church]\n\r", ch);
                return;
            }
        }

        if (!quest_target_scope_supported_for_player(ch, requested_scope, true))
            return;

        /* For the following functions, a QM must be present. */
        for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
        {
            if (IS_NPC(mob) && mob->pIndexData->pQuestor != NULL)
                break;
        }

        if( mob == NULL )
        {
            send_to_char("You can't do that here\n\r", ch);
            return;
        }

        if (!IS_AWAKE(ch))
        {
            send_to_char("In your dreams, or what?\n\r", ch);
            return;
        }

        act("$n asks $N for a quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act ("You ask $N for a quest.",ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        for (existing_run = ch->quest; existing_run != NULL; existing_run = existing_run->next)
        {
            if (existing_run->generating)
            {
                sprintf(buf, "Finish preparing your current pending quest first, %s.", HANDLE(ch));
                do_say(mob, buf);
                return;
            }
        }

        if (IS_DEAD(ch))
        {
            sprintf(buf, "You must come back to the world of the living first, %s.", HANDLE(ch));
            do_say(mob, buf);
            return;
        }

        if (!IS_IMMORTAL(ch)
            && game_settings.telnet_port != PORT_RAE
            && ch->quest_runtime.mission_allowance < 1)
        {
            sprintf(buf, "You're very brave, %s, but let someone else have a chance.", ch->name);
            if (mob == NULL)
            {
                pbugf(LOG_QUEST, "MOB Was null, %s.\n\r", ch->name);
                return;
            }

            do_say(mob, buf);
            sprintf(buf, "You need at least one mission allowance.");
            do_say(mob, buf);
            return;
        }

        new_run = new_quest();
        new_run->next = ch->quest;
        ch->quest = new_run;

        ch->quest_runtime.focused_run_id = 0;
        quest_runtime_attach_active_quest(ch, 0, 0);
        active_quest = quest_runtime_get_focused_run(ch);
        if (!active_quest)
            active_quest = ch->quest;

        if (!active_quest)
        {
            send_to_char("Unable to initialize quest runtime state.\n\r", ch);
            return;
        }

        active_quest->quest_index_auid = 0;
        active_quest->quest_index_vnum = 0;
        active_quest->target_scope = requested_scope;
        active_quest->scope_owner_id[0] = ch->id[0];
        active_quest->scope_owner_id[1] = ch->id[1];
        active_quest->scope_owner_uid = 0;
        active_quest->questgiver_type = QUESTOR_MOB;
        quest_set_wnum(&active_quest->questgiver_load, &active_quest->questgiver_wnum,
            mob->pIndexData->area, mob->pIndexData->vnum);
        active_quest->questreceiver_type = QUESTOR_MOB;
        quest_set_wnum(&active_quest->questreceiver_load, &active_quest->questreceiver_wnum,
            mob->pIndexData->area, mob->pIndexData->vnum);

        if (generate_quest(ch, mob))
        {
            active_quest->generating = false;

            sprintf(buf, "Thank you, brave %s!", HANDLE(ch));
            do_say(mob, buf);
        }
        else
        {
            sprintf(buf, "I'm sorry, %s, but I don't have any quests for you to do. Try again later.", ch->name);
            do_say(mob, buf);
            quest_runtime_detach_run(ch, active_quest);
            return;
        }

        if (IS_QUESTING(ch))
        {
            QUEST_PART_DATA *qp;

            ch->countdown = 0;

            for (qp = active_quest->parts; qp != NULL; qp = qp->next)
                ch->countdown += qp->minutes;

            sprintf(buf, "You have %d minutes to complete this quest.", ch->countdown);
            do_say(mob, buf);

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
                ch->countdown = 0;
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
                    ch->countdown = 0;
                }
            }
            else if (obj)
            {
                // Objects will not complain by default

                quest_runtime_detach_run(ch, active_quest);

                obj->tempstore[0] = 10;
                p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    ch->countdown = 0;
                }
            }
            else if (room)
            {
                quest_runtime_detach_run(ch, active_quest);

                room->tempstore[0] = 10;
                p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUEST_CANCEL, NULL);

                if (!IS_QUESTING(ch)) {
                    ch->countdown = 0;
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
                ch->countdown = 0;
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
            ch->countdown = 0;	// @@@NIB Not doing this was causing nextquest to come up
                                //	10 minutes if nextquest had expired
        }
    }
    else
    {
        send_to_char("QUEST commands: LOG FOCUS POINTS INFO TIME COMMENCE REQUEST CANCEL COMPLETE.\n\r", ch);
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
        active_run = ch->quest;
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
        }

        if (IS_QUESTING(ch))
        {
        active_quest = quest_runtime_get_focused_run(ch);
        if (!active_quest)
            active_quest = ch->quest;

        if (active_quest && !active_quest->generating)
        {
        if (ch->quest_runtime.expiry_modes != QUEST_EXPIRY_NONE)
        {
            if (ch->quest_runtime.expiry_modes & (QUEST_EXPIRY_COUNTDOWN | QUEST_EXPIRY_WALL_TIME))
                quest_runtime_tick_expiration(ch, current_time);

            if (quest_runtime_is_expired(ch, current_time)) {
                quest_runtime_detach_run(ch, active_quest);
                if (!IS_QUESTING(ch)) {
                    sprintf(buf, "{RYou have run out of time for your quest!{x\n\r");
                } else {
                    sprintf(buf, "{RYour focused quest run has expired.{x\n\r");
                }
                send_to_char(buf, ch);
                continue;
            }
        }
        else if (--ch->countdown <= 0)
        {
            quest_runtime_detach_run(ch, active_quest);
            if (!IS_QUESTING(ch)) {
                sprintf(buf, "{RYou have run out of time for your quest!{x\n\r");
            } else {
                sprintf(buf, "{RYour focused quest run has expired.{x\n\r");
            }
            send_to_char(buf, ch);
        }

        countdown_remaining = ch->countdown;
        if (ch->quest_runtime.expiry_modes & QUEST_EXPIRY_COUNTDOWN)
            countdown_remaining = ch->quest_runtime.expiry_countdown_minutes;

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

    if (ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
        perrf(LOG_QUEST, "check_quest_rescue_mob: NPC");
        return;
    }

    for (run = ch->quest; run != NULL; run = run->next)
    {
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

    if (ch->quest != NULL)
    {
        for (run = ch->quest; run != NULL; run = run->next)
        {
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
        }
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    int i;

    if (ch->quest == NULL || !IS_NPC(mob))
        return;

    if (IS_NPC(ch))
    {
        pbugf(LOG_QUEST, "NPC");
        return;
    }

    for (run = ch->quest; run != NULL; run = run->next)
    {
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
    }
}

void check_quest_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool show)
{
    QUEST_DATA *run;
    QUEST_PART_DATA *part;
    ROOM_INDEX_DATA *target_room;
    int i;

    if (ch->quest == NULL)
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

    for (run = ch->quest; run != NULL; run = run->next)
    {
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
        run = ch->quest;

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
        run = ch->quest;

    parts = 1;
    for (part = run->parts; part != NULL; part = part->next)
    {
        parts++;
    }

    return parts;
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

