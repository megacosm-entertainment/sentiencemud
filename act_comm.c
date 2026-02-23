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
 **************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "merc.h"
#include "interp.h"
#include "olc.h"
#include "recycle.h"
#include "tables.h"
#include "io/cache/redis_cache.h"
#include "channel_service.h"
#include "channel_policy.h"
#include "traits.h"
#include "account/preferences.h"
#include "mxp_links.h"
#include "requirements.h"

static void group_sync_legacy_state(GROUP_DATA *group);
bool can_speak_channels(CHAR_DATA *ch);

void string_end_chreport(CHAR_DATA *ch)
{
    char *notes;
    bool submitted;
    const char *cursor;
    bool has_content = false;

    if (!ch || !ch->desc)
        return;

    notes = ch->temp_log_entry;
    ch->temp_log_entry = NULL;

    for (cursor = notes; cursor && *cursor != '\0'; cursor++) {
        if (!isspace((unsigned char)*cursor)) {
            has_content = true;
            break;
        }
    }

    if (!has_content) {
        free_string(notes);
        free_string(ch->temp_report_channel);
        free_string(ch->temp_report_message_id);
        ch->temp_report_channel = NULL;
        ch->temp_report_message_id = NULL;
        send_to_char("Report cancelled (no notes entered).\n\r", ch);
        return;
    }

    submitted = channel_service_report_message(ch,
                                               ch->temp_report_channel,
                                               ch->temp_report_message_id,
                                               notes);

    free_string(notes);
    free_string(ch->temp_report_channel);
    free_string(ch->temp_report_message_id);
    ch->temp_report_channel = NULL;
    ch->temp_report_message_id = NULL;

    if (submitted)
        send_to_char("Report submitted to staff review with surrounding message context.\n\r", ch);
    else
        send_to_char("Unable to submit report for that message ID.\n\r", ch);
}

static const CHANNEL_DEF_DATA *history_find_channel(const char *name_or_command,
                                                    bool *out_ambiguous)
{
    int i;
    int j;
    const CHANNEL_DEF_DATA *match = NULL;

    if (out_ambiguous)
        *out_ambiguous = false;

    if (IS_NULLSTR(name_or_command))
        return NULL;

    match = channel_registry_find(name_or_command);
        if (match)
        return match;

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        bool matches_id;
        bool matches_command;
        bool matches_alias = false;

        if (!def)
            continue;

        matches_id = !str_prefix(name_or_command, def->id);
        matches_command = !IS_NULLSTR(def->command) && !str_prefix(name_or_command, def->command);
        for (j = 0; j < def->alias_count; j++) {
            if (!IS_NULLSTR(def->aliases[j]) && !str_prefix(name_or_command, def->aliases[j])) {
                matches_alias = true;
                break;
            }
        }

        if (!matches_id && !matches_command && !matches_alias)
            continue;

        if (match && match != def) {
            if (out_ambiguous)
                *out_ambiguous = true;
            return NULL;
        }

        match = def;
    }

    return match;
}

static const CHANNEL_DEF_DATA *resolve_channel_command(const char *input,
                                                       bool *out_ambiguous)
{
    const CHANNEL_DEF_DATA *exact_match = NULL;
    const CHANNEL_DEF_DATA *prefix_match = NULL;
    int i;
    int j;

    if (out_ambiguous)
        *out_ambiguous = false;

    if (IS_NULLSTR(input))
        return NULL;

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);

        if (!def)
            continue;

        if (!str_cmp(input, def->id) || (!IS_NULLSTR(def->command) && !str_cmp(input, def->command))) {
            if (exact_match && exact_match != def) {
                if (out_ambiguous)
                    *out_ambiguous = true;
                return NULL;
            }
            exact_match = def;
            continue;
        }

        for (j = 0; j < def->alias_count; j++) {
            if (!IS_NULLSTR(def->aliases[j]) && !str_cmp(input, def->aliases[j])) {
                if (exact_match && exact_match != def) {
                    if (out_ambiguous)
                        *out_ambiguous = true;
                    return NULL;
                }
                exact_match = def;
                break;
            }
        }
    }

    if (exact_match)
        return exact_match;

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        bool matches_id;
        bool matches_command;
        bool matches_alias = false;

        if (!def)
            continue;

        matches_id = !str_prefix(input, def->id);
        matches_command = !IS_NULLSTR(def->command) && !str_prefix(input, def->command);
        for (j = 0; j < def->alias_count; j++) {
            if (!IS_NULLSTR(def->aliases[j]) && !str_prefix(input, def->aliases[j])) {
                matches_alias = true;
                break;
            }
        }

        if (!matches_id && !matches_command && !matches_alias)
            continue;

        if (prefix_match && prefix_match != def) {
            if (out_ambiguous)
                *out_ambiguous = true;
            return NULL;
        }

        prefix_match = def;
    }

    return prefix_match;
}

static bool channel_guard_string_editor_commands(CHAR_DATA *ch, const char *argument)
{
    if (IS_NULLSTR(argument))
        return false;

    if (strlen(argument) == 1
        && (argument[0] == 'h'
            || argument[0] == 's'
            || argument[0] == 'f'
            || argument[0] == 'c')) {
        send_to_char("Are you sure that's all you want to say?\n\r", ch);
        return true;
    }

    if (!str_prefix("r ", argument)) {
        send_to_char("You're not in the string editor!\n\r", ch);
        return true;
    }

    if (!str_prefix("ld ", argument)
        || !str_prefix("lr ", argument)
        || !str_prefix("li ", argument)
        || !str_prefix("/ ", argument)) {
        send_to_char("You're not in the string editor.\n\r", ch);
        return true;
    }

    return false;
}

bool dispatch_dynamic_channel_command(CHAR_DATA *ch, const char *command, char *argument)
{
    const CHANNEL_DEF_DATA *def;
    ACCOUNT_DATA *account;
    char pref_key[80];
    char buf[MAX_STRING_LENGTH];
    bool ambiguous = false;
    bool enabled;

    if (!ch || IS_NULLSTR(command))
        return false;

    def = resolve_channel_command(command, &ambiguous);
    if (!def) {
        if (ambiguous)
            send_to_char("That channel alias is ambiguous. Please be more specific.\n\r", ch);
        return ambiguous;
    }

    account = (ch->desc ? ch->desc->account : NULL);
    snprintf(pref_key, sizeof(pref_key), "channel_%s", def->id);

    if (IS_NULLSTR(argument)) {
        if (IS_NPC(ch) || !ch->pcdata)
            return true;

        enabled = pref_get_bool(account, ch, pref_key, true);

        if (!enabled) {
            REQUIREMENT_CONTEXT req_context;

            memset(&req_context, 0, sizeof(req_context));
            req_context.actor = ch;

            if (!requirements_evaluate_text(def->subscribe_requirements, &req_context, true)) {
                send_to_char("You do not meet the requirements to toggle this channel on.\n\r", ch);
                return true;
            }
        }

        pref_set_bool(&ch->pcdata->preferences, PREF_CAT_CHANNEL, pref_key, !enabled);

        if (enabled)
            printf_to_char(ch, "%s channel is now OFF.\n\r", def->name);
        else
            printf_to_char(ch, "%s channel is now ON.\n\r", def->name);

        return true;
    }

    if (!IS_SET(def->channel_flags, CHANNEL_FLAG_IGNORE_QUIET) && IS_SET(ch->comm, COMM_QUIET)) {
        send_to_char("You must turn off quiet mode first.\n\r", ch);
        return true;
    }

    if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM)) {
        send_to_char("No one can hear you.\n\r", ch);
        return true;
    }

    if (channel_policy_global_revoked(ch)) {
        send_to_char("The gods have revoked your channel priviliges.\n\r", ch);
        return true;
    }

    if (IS_SET(def->channel_flags, CHANNEL_FLAG_RESPECT_SILENCE) && IS_AFFECTED2(ch, AFF2_SILENCE)) {
        send_to_char("You attempt to say something but fail!\n\r", ch);
        act("$n opens $s mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return true;
    }

    if (IS_SET(def->channel_flags, CHANNEL_FLAG_TOGGLE_ONLY)) {
        printf_to_char(ch, "You cannot talk over the %s channel.\n\r", def->name);
        return true;
    }

    if (IS_SET(def->channel_flags, CHANNEL_FLAG_GUARD_STR_EDIT_CMDS)
        && channel_guard_string_editor_commands(ch, argument)) {
        return true;
    }

    if (!IS_NPC(ch) && ch->pcdata)
        pref_set_bool(&ch->pcdata->preferences, PREF_CAT_CHANNEL, pref_key, true);

    buf[0] = '\0';
    STRIP_COLOUR(argument, buf);

    if (!buf[0]) {
        send_to_char("Is that all you want to say?\n\r", ch);
        return true;
    }

    if (!channel_service_send(ch, def->id, buf))
        send_to_char("That channel is currently unavailable.\n\r", ch);

    return true;
}




/**
 * do_clear - Clear the player's terminal screen
 *
 * Sends ANSI escape codes to clear the screen and move cursor to home position.
 * Works on terminals that support ANSI escape sequences.
 *
 * @param ch        The character executing the command
 * @param argument  Unused
 *
 * Planned refactor: Destination unspecified in original MOVED comment
 */
void do_clear (CHAR_DATA * ch, char *argument)
{
    send_to_char ("\x01B[2J\x01B[H", ch);
}

/**
 * do_delet - Safety stub for character deletion
 *
 * Prevents accidental deletion by requiring the full "delete" command.
 * This catches users who type "delet" or other partial matches.
 *
 * @param ch        The character executing the command
 * @param argument  Unused
 *
 * Planned refactor: player.c (never executed)
 */
void do_delet(CHAR_DATA *ch, char *argument)
{
    send_to_char("You must type the full command to delete yourself.\n\r",ch);
}


/**
 * do_delete - Permanently delete a player character
 *
 * Allows low-level characters to delete themselves. Requires confirmation
 * by typing the command twice. Characters over level 15 or who have remorted
 * must contact immortals for deletion (security measure).
 *
 * @param ch        The character attempting deletion
 * @param argument  If non-empty during confirmation, cancels the deletion
 *
 * Security: Only allows self-deletion for characters level 15 or below
 *           and who have not remorted.
 *
 * Planned refactor: player.c (never executed)
 */
void do_delete(CHAR_DATA *ch, char *argument)
{
    char strsave[MAX_INPUT_LENGTH];
    char player_dir_buf[MAX_INPUT_LENGTH];
    const char *player_dir;

    if (IS_NPC(ch)) return;

    if (ch->tot_level > 15 || IS_REMORT(ch)) {
        send_to_char("To have your character deleted contact the Immortals.\n\r"
                "For security purposes those over level 15 may not delete themselves.\n\r", ch);
        return;
    }

    if (ch->pcdata->confirm_delete) {
        if (argument[0]) {
            send_to_char("Delete status removed.\n\r",ch);
            ch->pcdata->confirm_delete = false;
        } else {
            player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
            snprintf(strsave, sizeof(strsave), "%s%c/%s", player_dir, tolower(ch->name[0]), capitalize(ch->name));
            redis_leaderboard_remove_all(ch->name);
            wiznet("$N turns $Mself into line noise.",ch,NULL,0,0,0);
            stop_fighting(ch,true);
            do_function(ch, &do_quit, NULL);
            unlink(strsave);
        }
        return;
    }

    if (argument[0]) {
        send_to_char("Syntax is 'delete' with no argument.\n\r",ch);
        return;
    }

    send_to_char("Type delete again to confirm this command.\n\r",ch);
    send_to_char("{RWARNING: this command is irreversible.{x\n\r",ch);
    send_to_char("Typing delete with an argument will undo delete status.\n\r", ch);
    ch->pcdata->confirm_delete = true;
    wiznet("$N is contemplating deletion.",ch,NULL,0,0,get_staff_rank(ch));
}

/**
 * do_channels - Display all communication channels and their current status
 *
 * Shows ON/OFF status for all available channels including gossip, OOC,
 * yell, flaming, auction, music, etc. Also displays current prompt,
 * player flag, and which channels display player flags.
 *
 * Immortals see additional channels (god channel).
 * Helpers see the helper channel.
 * Church members see church talk status.
 *
 * @param ch        The character viewing channel status
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_channels(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    REQUIREMENT_CONTEXT req_context;
    int i;
    int shown = 0;
    char flag_capable[MAX_STRING_LENGTH];

    (void)argument;

    memset(&req_context, 0, sizeof(req_context));
    req_context.actor = ch;
    flag_capable[0] = '\0';

    send_to_char("{Ychannel         receive role{x\n\r", ch);
    send_to_char("{Y-------------------------------------------------{x\n\r", ch);

    for (i = 0; i < channel_registry_count(); i++) {
        const CHANNEL_DEF_DATA *def = channel_registry_get(i);
        bool can_publish;
        bool can_subscribe;
        bool can_moderate = false;
        const char *role;
        const char *recv;
        int j;

        if (!def)
            continue;

        can_publish = channel_service_channel_available_for_sender(ch, def);
        can_subscribe = requirements_evaluate_text(def->subscribe_requirements,
                                                   &req_context,
                                                   true);

        if (IS_IMMORTAL(ch))
            can_moderate = true;
        else {
            for (j = 0; j < def->mod_count; j++) {
                if (!str_cmp(def->moderators[j], ch->name)) {
                    can_moderate = true;
                    break;
                }
            }
        }

        if (!can_publish && !can_subscribe && !can_moderate)
            continue;

        if (can_moderate)
            role = "Moderator";
        else if (can_publish && can_subscribe)
            role = "Send/Receive";
        else if (can_publish)
            role = "Send";
        else
            role = "Receive";

        recv = pref_check_channel(ch, def->id) ? "{WON{X" : "{DOFF{X";

        snprintf(buf,
                 sizeof(buf),
                 "{W%-15.15s{x %-7s %-14.14s\n\r",
                 def->id,
                 recv,
                 role);
        send_to_char(buf, ch);
        shown++;

        if (def->alias_count > 0) {
            int a;
            char alias_buf[MAX_STRING_LENGTH];

            alias_buf[0] = '\0';
            for (a = 0; a < def->alias_count; a++) {
                if (IS_NULLSTR(def->aliases[a]))
                    continue;

                if (alias_buf[0] != '\0')
                    strlcat(alias_buf, ", ", sizeof(alias_buf));

                strlcat(alias_buf, def->aliases[a], sizeof(alias_buf));
            }

            if (alias_buf[0] != '\0') {
                strlcpy(buf, "{D  aliases:{x ", sizeof(buf));
                strlcat(buf, alias_buf, sizeof(buf));
                strlcat(buf, "\n\r", sizeof(buf));
                send_to_char(buf, ch);
            }
        }

        if (def->allow_player_flags) {
            if (flag_capable[0] != '\0')
                strlcat(flag_capable, ", ", sizeof(flag_capable));
            strlcat(flag_capable, def->id, sizeof(flag_capable));
        }
    }

    if (shown == 0)
        send_to_char("(No channels currently available in this context.)\n\r", ch);

    send_to_char("{Y-------------------------------------------------{x\n\r", ch);
    send_to_char("{YCommunication toggles{x\n\r", ch);

    send_to_char("notify         ",ch);
    send_to_char((IS_SET(ch->comm,COMM_NOTIFY) ? "{WON{X\n\r" : "{DOFF{X\n\r"),ch);

    send_to_char("quiet mode     ",ch);
    send_to_char((IS_SET(ch->comm,COMM_QUIET) ? "{WON{X\n\r" : "{DOFF{X\n\r"),ch);

    send_to_char("battle spam    ",ch);
    send_to_char((IS_SET(ch->comm,COMM_NOBATTLESPAM) ? "{DOFF{X\n\r" : "{WON{X\n\r"),ch);

    send_to_char("form state     ", ch);
    send_to_char((IS_SET(ch->comm,COMM_SHOW_FORM_STATE) ? "{WON{X\n\r" : "{DOFF{X\n\r"),ch);

    if (IS_SET(ch->comm,COMM_AFK)) send_to_char("You are AFK.\n\r",ch);

    if (!ch->pcdata->flag || IS_NULLSTR(ch->pcdata->flag))
        send_to_char("You currently have no flag.\n\r", ch);
    else {
        sprintf(buf, "Your flag is: %s{x\n\r", ch->pcdata->flag);
        send_to_char(buf, ch);
    }

    if (!ch->pcdata->channel_flags)
        send_to_char("You will not see player flags on any channels.\n\r", ch);
    else {
        sprintf(buf, "Player flags will be displayed on: %s\n\r", flag_string(channel_flags, ch->pcdata->channel_flags));
        send_to_char(buf, ch);
    }

    if (flag_capable[0] != '\0') {
        send_to_char("Your current channels that support player flags: ", ch);
        send_to_char(flag_capable, ch);
        send_to_char("\n\r", ch);
    }

    if (channel_policy_sender_revoked(ch, "tell") || channel_policy_sender_revoked(ch, "gtell"))
        send_to_char("You cannot use tells.\n\r",ch);

    if (channel_policy_global_revoked(ch)) send_to_char("You cannot use channels.\n\r",ch);
}

void do_history(CHAR_DATA *ch, char *argument)
{
    char arg1[MIL];
    char arg2[MIL];
    char arg3[MIL];
    const CHANNEL_DEF_DATA *def;
    bool ambiguous = false;
    CHANNEL_HISTORY_ENTRY entries[15];
    CHANNEL_HISTORY_ENTRY entry;
    int count;
    int i;

    #define HISTORY_REPORT_LINK(_ch,_def,_rid,_out,_outsz) do { \
        BUFFER *mxp_buf = new_buf(); \
        char cmd_info[MSL]; \
        char cmd_report[MSL]; \
        mxp_cmd_hint_t items[2]; \
        snprintf(cmd_info, sizeof(cmd_info), "history %s info %s", (_def)->id, (_rid)); \
        snprintf(cmd_report, sizeof(cmd_report), "history %s report %s ", (_def)->id, (_rid)); \
        items[0].cmd = cmd_info; items[0].hint = "Show history details"; \
        items[1].cmd = cmd_report; items[1].hint = "Report this message"; \
        mxp_link_multi((_ch)->desc, mxp_buf, (_rid), items, 2); \
        strlcpy((_out), buf_string(mxp_buf), (_outsz)); \
        free_buf(mxp_buf); \
    } while (0)

    #define HISTORY_REPORT_COUNT(_json,_out_count) do { \
        const char *_p = (_json); \
        (_out_count) = 0; \
        if (!IS_NULLSTR(_p)) { \
            _p = strstr(_p, "\"count\":"); \
            if (_p) (_out_count) = atoi(_p + 8); \
        } \
    } while (0)

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);
    if (arg1[0] == '\0') {
        send_to_char("Syntax: history <channel>\n\r", ch);
        send_to_char("        history <channel> info <#|message-id>\n\r", ch);
        send_to_char("        history <channel> report <#|message-id> [notes]\n\r", ch);
        return;
    }

    def = history_find_channel(arg1, &ambiguous);
    if (!def) {
        if (ambiguous)
            send_to_char("That channel prefix is ambiguous. Please be more specific.\n\r", ch);
        else
            send_to_char("No such channel.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg2);
    if (arg2[0] == '\0') {
        char buf[MSL];
        int shown = 0;

        count = channel_service_history_recent(ch,
                               def->id,
                                               (int)(sizeof(entries) / sizeof(entries[0])),
                                               entries,
                                               (int)(sizeof(entries) / sizeof(entries[0])));
        if (count <= 0) {
            send_to_char("No recent history for that channel.\n\r", ch);
            return;
        }

        sprintf(buf, "{YRecent history for %s (%s){x\n\r", def->id, def->name);
        send_to_char(buf, ch);

        for (i = 0; i < count; i++) {
            char when_buf[32];
            char report_link[512];
            char reported_marker[64];
            char filtered_text[sizeof(entries[i].message_text)];
            int report_count = 0;
            bool show_report_link = (ch->desc && isMXP(ch->desc));
            struct tm *tm_info = localtime(&entries[i].timestamp);

            if (channel_service_apply_preference_filters(def->id,
                                                         ch,
                                                         entries[i].message_text,
                                                         filtered_text,
                                                         sizeof(filtered_text))) {
                continue;
            }

            if (tm_info)
                strftime(when_buf, sizeof(when_buf), "%Y-%m-%d %H:%M", tm_info);
            else
                strlcpy(when_buf, "unknown-time", sizeof(when_buf));

            if (show_report_link)
                HISTORY_REPORT_LINK(ch, def, entries[i].report_id, report_link, sizeof(report_link));
            else
                report_link[0] = '\0';

            HISTORY_REPORT_COUNT(entries[i].reports_json, report_count);
            if (report_count > 0) {
                if (IS_IMMORTAL(ch))
                    snprintf(reported_marker, sizeof(reported_marker), " {R[REPORT:%d]{x", report_count);
                else
                    reported_marker[0] = '\0';
            } else {
                reported_marker[0] = '\0';
            }

            if (show_report_link)
                snprintf(buf,
                         sizeof(buf),
                         "{Y#%2d{x [%s] {W%.48s{x {D[%s]{x%s: %.3000s\n\r",
                         i + 1,
                         when_buf,
                         entries[i].sender_name,
                         report_link,
                         reported_marker,
                         filtered_text);
            else
                snprintf(buf,
                         sizeof(buf),
                         "{Y#%2d{x [%s] {W%.48s{x%s: %.3000s\n\r",
                         i + 1,
                         when_buf,
                         entries[i].sender_name,
                         reported_marker,
                         filtered_text);
            send_to_char(buf, ch);
            shown++;
        }

        if (shown == 0) {
            send_to_char("No visible history for that channel after your filters.\n\r", ch);
            return;
        }

        send_to_char("Use 'history <channel> info <#|message-id>' for full details.\n\r", ch);
        return;
    }

    if (str_cmp(arg2, "info")) {
        if (!str_cmp(arg2, "report")) {
            CHANNEL_HISTORY_ENTRY report_target;
            char report_id[64];

            argument = one_argument(argument, arg3);
            if (IS_NULLSTR(arg3)) {
                send_to_char("Syntax: history <channel> report <#|message-id> [notes]\n\r", ch);
                return;
            }

            if (is_number(arg3)) {
                int index = atoi(arg3);

                if (index <= 0) {
                    send_to_char("Please provide a positive history entry number.\n\r", ch);
                    return;
                }

                if (!channel_service_history_by_index(ch, def->id, index, &report_target)) {
                    send_to_char("No history entry exists at that index.\n\r", ch);
                    return;
                }

                strlcpy(report_id, report_target.report_id, sizeof(report_id));
            } else {
                if (!channel_service_history_by_report_id(ch, def->id, arg3, &report_target)) {
                    send_to_char("No history entry exists for that message ID.\n\r", ch);
                    return;
                }

                strlcpy(report_id, arg3, sizeof(report_id));
            }

            if (IS_NULLSTR(argument)) {
                free_string(ch->temp_report_channel);
                free_string(ch->temp_report_message_id);
                free_string(ch->temp_log_entry);

                ch->temp_report_channel = str_dup(def->id);
                ch->temp_report_message_id = str_dup(report_id);
                ch->temp_log_entry = str_dup("");

                send_to_char("Enter additional report notes. Type @ when done.\n\r", ch);
                string_append(ch, &ch->temp_log_entry);
                ch->desc->editor = ED_CHREPORT;
                return;
            }

            if (!channel_service_report_message(ch, def->id, report_id, argument)) {
                send_to_char("Unable to submit report for that message ID.\n\r", ch);
                return;
            }

            send_to_char("Report submitted to staff review with surrounding message context.\n\r", ch);
            return;
        }

        send_to_char("Syntax: history <channel> info <#|message-id>\n\r", ch);
        send_to_char("        history <channel> report <#|message-id> [notes]\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg3);
    if (IS_NULLSTR(arg3)) {
        send_to_char("Please provide a history index or message ID.\n\r", ch);
        return;
    }

    if (is_number(arg3)) {
        int index = atoi(arg3);
        if (index <= 0) {
            send_to_char("Please provide a positive history entry number.\n\r", ch);
            return;
        }

        if (!channel_service_history_by_index(ch, def->id, index, &entry)) {
            send_to_char("No history entry exists at that index.\n\r", ch);
            return;
        }
    } else {
        if (!channel_service_history_by_report_id(ch, def->id, arg3, &entry)) {
            send_to_char("No history entry exists for that message ID.\n\r", ch);
            return;
        }
    }

    {
        char buf[MSL];
        char when_buf[64];
        char report_link[512];
        int report_count = 0;
        struct tm *tm_info = localtime(&entry.timestamp);

        if (tm_info)
            strftime(when_buf, sizeof(when_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        else
            strlcpy(when_buf, "unknown-time", sizeof(when_buf));

        if (ch->desc && isMXP(ch->desc))
            HISTORY_REPORT_LINK(ch, def, entry.report_id, report_link, sizeof(report_link));
        else
            strlcpy(report_link, entry.report_id, sizeof(report_link));

        HISTORY_REPORT_COUNT(entry.reports_json, report_count);

        if (ch->desc && isMXP(ch->desc)) {
            snprintf(buf,
                     sizeof(buf),
                     "{YHistory detail{x\n\r"
                     "  Channel : {W%.32s{x (%.32s)\n\r"
                     "  Index   : #%.32s\n\r"
                     "  Time    : %.63s\n\r"
                     "  Sender  : %.64s\n\r"
                     "  Report  : {W%s{x\n\r"
                     "  Reports : %d\n\r"
                     "  Meta    : %.1200s\n\r"
                     "  Message : %.2400s\n\r",
                     def->id,
                     def->name,
                     arg3,
                     when_buf,
                     entry.sender_name,
                     report_link,
                     report_count,
                     IS_NULLSTR(entry.reports_json) ? "(none)" : entry.reports_json,
                     entry.message_text);
        } else {
            snprintf(buf,
                     sizeof(buf),
                     "{YHistory detail{x\n\r"
                     "  Channel : {W%.32s{x (%.32s)\n\r"
                     "  Index   : #%.32s\n\r"
                     "  Time    : %.63s\n\r"
                     "  Sender  : %.64s\n\r"
                     "  Report  : {W%.128s{x\n\r"
                     "  Reports : %d\n\r"
                     "  Meta    : %.1200s\n\r"
                     "  Message : %.2400s\n\r",
                     def->id,
                     def->name,
                     arg3,
                     when_buf,
                     entry.sender_name,
                     report_link,
                     report_count,
                     IS_NULLSTR(entry.reports_json) ? "(none)" : entry.reports_json,
                     entry.message_text);
        }
        send_to_char(buf, ch);
        send_to_char("Use this ID or index for reporting: history <channel> report <#|message-id> [notes]\n\r", ch);
    }

    #undef HISTORY_REPORT_LINK
    #undef HISTORY_REPORT_COUNT
}


/**
 * do_quiet - Toggle quiet mode
 *
 * When quiet mode is enabled, the character will not receive most channel
 * communications. Use qlist command to allow specific people to still
 * reach you while in quiet mode.
 *
 * @param ch        The character toggling quiet mode
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_quiet(CHAR_DATA *ch, char * argument)
{
    if (IS_SET(ch->comm,COMM_QUIET))
        send_to_char("Quiet mode removed.\n\r",ch);
    else
        send_to_char("Quiet mode set.\n\r",ch);
    TOGGLE_BIT(ch->comm,COMM_QUIET);
}


/**
 * do_afk - Toggle Away From Keyboard mode
 *
 * Marks the character as AFK. Tells received while AFK are buffered
 * and can be viewed later with 'replay'. An optional message can be
 * set to inform people who try to contact you.
 *
 * @param ch        The character toggling AFK
 * @param argument  Optional AFK message (max 250 characters)
 *
 * Planned refactor: channels.c (never executed)
 */
void do_afk(CHAR_DATA *ch, char * argument)
{
    if (IS_NPC(ch)) return;

    if (IS_SET(ch->comm,COMM_AFK)) {
        send_to_char("AFK mode removed. Type 'replay' to see tells.\n\r",ch);

        free_string(ch->pcdata->afk_message);
        ch->pcdata->afk_message = NULL;
    } else {
        send_to_char("You are now in AFK mode.\n\r",ch);
        ch->pcdata->afk_message = NULL;

        if (argument[0]) {
            if (strlen(argument) > 250) argument[250] = '\0';
            send_to_char("AFK message set.\n\r", ch);

            ch->pcdata->afk_message = str_dup(argument);
        }
    }
    TOGGLE_BIT(ch->comm,COMM_AFK);
}


/**
 * do_replay - Display buffered tells received while AFK
 *
 * Shows all tells that were received while the character was AFK or
 * otherwise unavailable. Clears the buffer after displaying.
 *
 * @param ch        The character viewing their buffered tells
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_replay(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch)) {
        send_to_char("You can't replay.\n\r",ch);
        return;
    }

    if (!buf_string(ch->pcdata->buffer)[0]) {
        send_to_char("You have no tells to replay.\n\r",ch);
        return;
    }

    page_to_char(buf_string(ch->pcdata->buffer),ch);
    clear_buf(ch->pcdata->buffer);
}


/**
 * can_speak_channels - Check if a character can use communication channels
 *
 * Validates that the character is not blocked from using channels due to
 * quiet mode, being in a ROOM_NOCOMM room, or having channels revoked.
 *
 * @param ch  The character to check
 *
 * @return true if character can speak on channels, false otherwise
 *         Also sends appropriate error message to the character
 *
 * Planned refactor: channels.c (never executed)
 */
bool can_speak_channels(CHAR_DATA *ch)
{
    if (IS_SET(ch->comm,COMM_QUIET)) {
        send_to_char("You must turn off quiet mode first.\n\r",ch);
        return false;
    }

    if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM)) {
        send_to_char("No one can hear you.\n\r", ch);
        return false;
    }

    if (channel_policy_global_revoked(ch)) {
        send_to_char("The gods have revoked your channel priviliges.\n\r",ch);
        return false;
    }

    return true;
}

/**
 * do_ooc - Out of Character global chat channel
 *
 * Allows players to communicate out of character across the entire game.
 * If no argument is provided, toggles the channel on/off.
 * Supports player flags and respects ignore lists.
 *
 * @param ch        The character speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_ooc(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "ooc", argument);
}


/**
 * church_echo - Send a message to all online members of a church
 *
 * Broadcasts a message to every connected player who belongs to the
 * specified church organization.
 *
 * @param church   The church whose members should receive the message
 * @param message  The message to send
 *
 * Planned refactor: church.c (never executed)
 */
void church_echo(CHURCH_DATA *church, char *message)
{
    DESCRIPTOR_DATA *d;

    if (!church) {
        pbugf(LOG_ERROR, "Attempted to church_echo from null church.");
        return;
    }

    for (d = descriptor_list; d; d = d->next) {
        CHAR_DATA *victim = d->original ? d->original : d->character;

        if (d->connected == CON_PLAYING && victim->church == church)
            send_to_char(message, victim);
    }
}


/**
 * gecho - Global echo to all connected players
 *
 * Sends a message to every player currently connected and playing.
 * No filtering based on channel settings.
 *
 * @param message  The message to broadcast
 *
 * Planned refactor: channels.c (never executed)
 */
void gecho(char *message)
{
    DESCRIPTOR_DATA *d;
    for (d = descriptor_list; d; d = d->next)
        if (d->connected == CON_PLAYING)
            send_to_char(message, (d->original ? d->original : d->character));
}


/**
 * do_gossip - Global gossip communication channel
 *
 * Main social channel for global communication. If no argument, toggles
 * channel on/off. Includes safeguards against accidental string editor
 * commands (h, s, f, c, r, ld, lr, li, /).
 * Applies drunk speech modification when character is intoxicated.
 * Supports player flags and respects ignore lists.
 *
 * @param ch        The character speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_gossip(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "gossip", argument);
}


/**
 * do_flame - Flaming/heated debate communication channel
 *
 * Channel for heated discussions or arguments. If no argument, toggles
 * channel on/off. Supports drunk speech modification, player flags, and
 * respects ignore lists.
 *
 * @param ch        The character speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_flame(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "flame", argument);
}


/**
 * do_helper - Helper channel for designated helpers and immortals
 *
 * Communication channel restricted to players with PLR_HELPER flag or
 * immortals. Regular players can turn the channel off but cannot turn
 * it back on. Used for helping newbies and coordinating helper activities.
 *
 * @param ch        The character speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Restrictions: Only helpers/immortals can enable the channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_helper(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "helper", argument);
}


/**
 * do_hints - Toggle receiving system hints
 *
 * Controls whether the player receives game hints and tips. This is a
 * receive-only channel - players cannot broadcast on it, only toggle
 * whether they see hints.
 *
 * @param ch        The character toggling hints
 * @param argument  If provided, informs user they cannot speak on this channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_hints(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "hints", argument);
}


/**
 * do_music - Music sharing communication channel
 *
 * Channel for sharing music-related content. If no argument, toggles
 * channel on/off. Supports drunk speech modification, player flags, and
 * respects ignore lists.
 *
 * @param ch        The character speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Planned refactor: channels.c (never executed)
 */
void do_music(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "music", argument);
}


/**
 * do_immtalk - Immortal-only communication channel
 *
 * Private channel for staff/immortal communication. Only visible to
 * characters with immortal status. If no argument, toggles channel on/off.
 *
 * @param ch        The immortal speaking
 * @param argument  Message to send, or empty to toggle channel
 *
 * Access: Immortals only
 *
 * Planned refactor: channels.c (never executed)
 */
void do_immtalk(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "immtalk", argument);
}


/**
 * do_say - Room-local speech with dynamic sentence formatting
 *
 * Allows a character to speak to everyone in the room. The output format
 * varies based on punctuation:
 *   - Exclamations (!) use "exclaims"
 *   - Questions (?) use "asks"
 *   - Statements (.) use "says"
 * Format randomly alternates between styles like:
 *   "'Hello!' exclaims Bob." or "Bob exclaims, 'Hello!'"
 *
 * Triggers TRIG_SPEECH on: mobs in room, objects in room, worn items,
 * inventory items, and the room itself.
 *
 * @param ch        The character speaking
 * @param argument  The message to say
 *
 * Blocked by: AFF2_SILENCE affect
 * Modifiers: Drunk speech when intoxicated
 *
 * Planned refactor: speech.c (never executed)
 */
void do_say(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH], msg[MSL];
    int i;
    char *second;
    bool break_line = true;

    if (IS_AFFECTED2(ch, AFF2_SILENCE))
    {
    send_to_char("You attempt to say something but fail!\n\r", ch);
    act("$n opens $e mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
    }

    msg[0] = '\0';
    STRIP_COLOUR(argument, msg);

    if(!msg[0]) {
        send_to_char("Say what?\n\r", ch);
        return;
    }

    if (channel_service_send(ch, "say", msg))
    return;

    buf[0] = '\0';
    for (i = 0; msg[i] != '\0'; i++)
    {
    if (msg[i] == '!'
    && msg[i+1] != ' ')
    break_line = false;
    }
    if (break_line)
    {
    second = stptok(msg, buf, sizeof(buf), "!");
    while(*second == ' ')
    second++;

    if (*second != '\0')
    {
    sprintf(buf2, "{C'$T!{C' exclaims $n. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
    sprintf(buf2, "{C'$T!{C' you exclaim. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
    return;
    }
    }

    for (i = 0; msg[i] != '\0'; i++)
    {
    if (msg[i] == '?'
    && msg[i+1] != ' ')
    break_line = false;
    }

    if (break_line)
    {
    second = stptok(msg, buf, sizeof(buf), "?");
    while(*second == ' ')
    second++;

    if (*second != '\0')
    {
    sprintf(buf2, "{C'$T?{C' asks $n. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
    sprintf(buf2, "{C'$T?{C' you ask. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
    return;
    }

    }

    for (i = 0; msg[i] != '\0'; i++)
    {
    if (msg[i] == '.'
    && msg[i+1] != ' ') break_line = false;
    }

    if (break_line)
    {
    second = stptok(msg, buf, sizeof(buf), ".");
    while(*second == ' ')
    second++;

    if (*second != '\0')
    {
    sprintf(buf2, "{C'$T.{C' says $n. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_ROOM, NULL, NULL);
    sprintf(buf2, "{C'$T.{C' you say. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, NULL, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);
    return;
    }
    }

    if (msg[strlen(msg)-1] == '!')
    {
    if (number_percent() < 50)
    {
    act("{C'$T{C' exclaims $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{C'$T{C' you exclaim.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    else
    {
    act("{C$n exclaims, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{CYou exclaim, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    }
    else
    if (msg[strlen(msg)-1] == '?')
    {
    if (number_percent() < 50)
    {
    act("{C$n asks, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{CYou ask, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    else
    {
    act("{C'$T{C' asks $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{C'$T{C' you ask.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    }
    else
    {
    if (number_percent() < 50)
    {
    act("{C$n says, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{CYou say, '$T{C'{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    else
    {
    act("{C'$T{C' says $n.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_ROOM, NULL, NULL);
    act("{C'$T{C' you say.{x", ch, NULL, NULL, NULL, NULL, NULL, msg, TO_CHAR, NULL, NULL);
    }
    }

    if (!IS_NPC(ch) || IS_SWITCHED(ch))
    {
    CHAR_DATA *mob, *mob_next;
    OBJ_DATA *obj, *obj_next;

    for (mob = ch->in_room->people; mob != NULL; mob = mob_next) {
        mob_next = mob->next_in_room;
        if (!IS_NPC(mob) || mob->position == mob->pIndexData->default_pos)
            p_act_trigger(msg, mob, NULL, NULL, ch, NULL, NULL,NULL, NULL,TRIG_SPEECH  );

ITERATOR obj_it;


// Inventory
iterator_start(&obj_it, mob->lcarrying);
while ((obj = (OBJ_DATA *)iterator_nextdata(&obj_it))) {
    p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SPEECH);
}
iterator_stop(&obj_it);

// Worn items
iterator_start(&obj_it, mob->lworn);
while ((obj = (OBJ_DATA *)iterator_nextdata(&obj_it))) {
    p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SPEECH);
}
iterator_stop(&obj_it);
    }

    for (obj = ch->in_room->contents; obj; obj = obj_next) {
    obj_next = obj->next_content;
    p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH);
    }

    p_act_trigger(msg, NULL, NULL, ch->in_room, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH);
    }
}

/**
 * do_tells - Toggle receiving private tells
 *
 * Allows the player to turn off/on receiving private messages (tells)
 * from other players.
 *
 * @param ch        The character toggling tells
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_tells(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "tells", argument);
}


/**
 * do_tell - Send a private message to another player
 *
 * Sends a private message to a specific player anywhere in the game.
 * Handles various scenarios:
 *   - Buffering messages for AFK recipients
 *   - Buffering messages for link-dead players
 *   - Respecting ignore lists (unless sender is higher-level immortal)
 *   - Respecting quiet mode and tell restrictions
 *   - Wizi level visibility for immortals
 *
 * Sets both sender's and recipient's reply pointer for easy replies.
 * Supports player flags.
 *
 * @param ch        The character sending the tell
 * @param argument  "<target> <message>"
 *
 * Blocked by: COMM_NOTELL (revoked by gods), ROOM_NOCOMM
 *
 * Planned refactor: channels.c (never executed)
 */
void do_tell(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char msg[2*MSL];
    CHAR_DATA *victim;
    CHANNEL_TELL_POLICY_BLOCK tell_block;

    if (channel_policy_sender_revoked(ch, "tell"))
    {
        send_to_char("Your tells have been revoked.\n\r", ch);
        return;
    }

    if (IS_SET(ch->in_room->room_flag[0], ROOM_NOCOMM))
    {
        send_to_char("You can't seem to gather enough energy to do it.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
        send_to_char("Tell whom what?\n\r", ch);
        return;
    }

    if ((victim = get_char_world(NULL, arg)) == NULL || (IS_NPC(victim) && victim->in_room != ch->in_room))
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (IS_SWITCHED(victim))
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    /* AO 010217 respect wizi only */
    if (IS_IMMORTAL(victim) && victim->invis_level > ch->tot_level) {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (!channel_policy_tell_delivery_allowed(ch, victim, false, &tell_block)) {
        if (tell_block == CHANNEL_TELL_POLICY_BLOCK_IGNORE) {
            IGNORE_DATA *ignore;

            for (ignore = victim->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
            {
                if (!str_cmp(ignore->name, ch->name)) break;
            }

            sprintf(buf, "{R$E $Z ignoring you.{x\n\r{RReason:{x %s", ignore->reason);
            act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR,NULL, get_verb_form(victim, "is", "are"));
            return;
        }

        send_to_char("Your message didn't get through.\n\r", ch);
        return;
    }

    if (IS_SET(ch->comm, COMM_QUIET) && !can_tell_while_quiet(victim, ch))
    {
        send_to_char("You must turn off quiet mode first.\n\r", ch);
        return;
    }

    buf[0] = '\0';
    STRIP_COLOUR(argument, buf);
    if(!buf[0]) {
        send_to_char("Tell whom what?\n\r",ch);
        return;
    }


    if (victim->desc == NULL && !IS_NPC(victim))
    {
        act("$N has lost $S link... try again later.", ch,victim,NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);

        if (!IS_NPC(ch)) {
                if(ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_TELLS))
                    sprintf(msg, "{R%s tells you '%s {R%s'{x\n\r", ch->name, ch->pcdata->flag, buf);
                else
                    sprintf(msg, "{R%s tells you '%s'{x\n\r", ch->name, buf);
        } else
            sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(ch, victim), buf);

        msg[2] = UPPER(msg[2]);

        add_buf(victim->pcdata->buffer,msg);
        victim->reply = ch;
        return;
    }

    if (!channel_policy_tell_delivery_allowed(ch, victim, true, &tell_block)
        && tell_block == CHANNEL_TELL_POLICY_BLOCK_QUIET)
    {
        act("$E $Z not receiving tells.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, get_verb_form(victim, "is", "are"));
        return;
    }

    if (IS_SET(victim->comm,COMM_AFK))
    {
        if (!IS_NPC(ch)) {
                if(ch->pcdata->flag != NULL && SHOW_CHANNEL_FLAG(victim, FLAG_TELLS))
                    sprintf(msg, "{R%s tells you '%s {R%s'{x\n\r", ch->name, ch->pcdata->flag, buf);
                else
                    sprintf(msg, "{R%s tells you '%s'{x\n\r", ch->name, buf);
        } else
            sprintf(msg, "{R%s tells you '%s'{x\n\r", pers(ch, victim), buf);

        msg[2] = UPPER(msg[2]);
        add_buf(victim->pcdata->buffer,msg);
        victim->reply = ch;

        sprintf(buf, "{R$E $Z AFK, and has been idle for %d minutes.{x", victim->timer);
        act(buf, ch,victim,NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, get_verb_form(victim, "is", "are"));
        sprintf(buf, "{RMessage: %s{x\n\r",
            victim->pcdata->afk_message == NULL ? "none" : victim->pcdata->afk_message);
        send_to_char(buf, ch);
        return;
    }

    /* Sender echo — shown immediately regardless of transport. */
    if (!IS_NPC(ch) && ch->pcdata->flag != NULL && IS_SET(ch->pcdata->channel_flags, FLAG_TELLS))
        sprintf(msg, "{RYou tell %s '%s {R%s{R'{x\n\r", pers(victim, ch), ch->pcdata->flag, buf);
    else
        sprintf(msg, "{RYou tell %s '%s{R'{x\n\r", pers(victim, ch), buf);
    send_to_char(msg, ch);

    /* Idle notification to sender (use msg buffer to avoid clobbering buf). */
    if (victim->timer > 1)
    {
        sprintf(msg, "{RNote: $E $Z been idle for %d minutes.{x", victim->timer);
        act(msg, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, get_verb_form(victim, "has", "have"));
    }

    /* Sender's reply pointer set immediately. */
    ch->reply = victim;

    /* Recipient reply pointer set immediately for local tell/reply parity. */
    victim->reply = ch;

    /*
    * Recipient delivery: channel service handles formatting and reply pointer
    * on recipient. Falls back to direct delivery in
     * legacy or uninitialized mode.  buf still holds the stripped message text.
     */
    channel_service_send_directed(ch, "tell", victim, buf);
}



/**
 * do_reply - Reply to the last person who sent you a tell
 *
 * Convenience command to quickly respond to the last person who sent
 * you a private message. Uses the ch->reply pointer set by do_tell.
 *
 * Note: Uses victim->name directly rather than pers() to avoid issues
 * with shaped vampires and shifted vampires/slayers.
 *
 * @param ch        The character replying
 * @param argument  The reply message
 *
 * Planned refactor: channels.c (never executed)
 */
void do_reply(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    char buf[MAX_STRING_LENGTH];

    if ((victim = ch->reply) == NULL /*|| !can_see(ch, victim)*/) {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    sprintf(buf, "'%s' ", victim->name);	// Was pers(victim, ch), which posed a problem for shaped vampires and shifted vampires/slayers
    strcat(buf, argument);

    do_function(ch, &do_tell, buf);
    return;

}


/**
 * do_yell - Area-wide communication
 *
 * Broadcasts a message to all players in the same area. If no argument,
 * toggles receiving yells on/off. Supports drunk speech, player flags,
 * and respects ignore lists and quiet mode.
 *
 * @param ch        The character yelling
 * @param argument  Message to yell, or empty to toggle receiving yells
 *
 * Blocked by: global channel revocation policy, ROOM_NOCOMM
 *
 * Planned refactor: channels.c (never executed)
 */
void do_yell(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "yell", argument);
}


/**
 * do_emote - Display a custom action/roleplay emote
 *
 * Allows the player to display a custom action message to everyone
 * in the room. The character's name is prefixed to the message.
 * Example: "emote laughs heartily" shows "Bob laughs heartily"
 *
 * Temporarily disables MOBtrigger to prevent script triggers from
 * firing on emotes.
 *
 * @param ch        The character emoting
 * @param argument  The emote text (action description)
 *
 * Restriction: Cannot be used while switched into another form
 *
 * Planned refactor: speech.c (never executed)
 */
void do_emote(CHAR_DATA *ch, char *argument)
{
    if (IS_SWITCHED(ch))
    {
    send_to_char("You can't show your emotions.\n\r", ch);
    return;
    }

    if (argument[0] == '\0')
    {
    send_to_char("Emote what?\n\r", ch);
    return;
    }

    MOBtrigger = false;
    act("$n $T{x", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_ROOM, NULL, NULL);
    act("$n $T{x", ch, NULL, NULL, NULL, NULL, NULL, argument, TO_CHAR, NULL, NULL);
    MOBtrigger = true;
}


/**
 * do_quit - Save character and exit the game
 *
 * Saves the character and disconnects from the game. Performs extensive
 * cleanup including:
 *   - Unshifting vampires/werewolves (preserves shift state for relogin)
 *   - Releasing pulled carts
 *   - Returning mail package items to inventory
 *   - Removing PURGE_QUIT tokens (with TRIG_TOKEN_REMOVED trigger)
 *   - Saving last worn equipment positions
 *   - Removing worn item affects
 *   - Resetting immortal bank accounts (below MAX_LEVEL)
 *   - Resetting manastore
 *   - Saving account metadata
 *   - Updating Redis cache status
 *   - Handling mounts (returning owned mounts home)
 *   - Extracting quest objects
 *   - Closing duplicate connections (anti-cheat)
 *
 * @param ch        The character quitting
 * @param argument  If set, triggers TRIG_QUIT before quitting
 *
 * Restrictions: Cannot quit while fighting, in auction, stunned/dead,
 *               in ROOM_NO_QUIT rooms, or while switched
 *
 * Planned refactor: player.c (never executed)
 */
void do_quit(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d,*d_next;
    OBJ_DATA *obj;
    AFFECT_DATA *paf;
    int id[2];
    TOKEN_DATA *token, *token_next;

    if (IS_SWITCHED(ch))
    {
    send_to_char("You can't quit in morphed form.\n\r", ch);
    return;
    }

    if (IS_NPC(ch))
    return;

    if (!ch->pcdata->quit_on_input) {
        if (ch->position == POS_FIGHTING)
        {
        send_to_char("No way! You are fighting.\n\r", ch);
        return;
        }

        if (auction_info.high_bidder == ch
        ||  auction_info.owner == ch)
        {
        send_to_char("You still have a stake in the auction!\n\r",ch);
        return;
        }

        if (ch->position < POS_STUNNED)
        {
        send_to_char("You're not DEAD yet.\n\r", ch);
        return;
        }

        if (IS_SET(ch->in_room->room_flag[1], ROOM_NO_QUIT))
        {
        send_to_char("You can't quit here.\n\r", ch);
        return;
        }

        /* If person is putting together a package give them back items etc */
        if (ch->mail != NULL)
        {
        OBJ_DATA *obj;
        OBJ_DATA *obj_next;

        for (obj = ch->mail->objects; obj != NULL; obj = obj_next)
        {
        obj_next = obj->next_content;

        obj_from_mail(obj);
        obj_to_char(obj, ch);
        act("You take $p from your package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n takes $p from $s package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, get_verb_form(ch, "takes", "take"), NULL);
        }

        send_to_char("You discard your mail package.\n\r", ch);
        free_mail(ch->mail);
        ch->mail = NULL;
        }
/* WHY???? AO 010417
        if (ch->quest != NULL)
        {
        send_to_char("You can't quit, you're still on a quest!\n\r", ch);
        return;
        }
*/
        if(argument) {
            p_percent_trigger( ch, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUIT, NULL);
            // This requested input, pause and wait for input before quiting)
            if(ch->desc && ch->desc->input) {
                ch->pcdata->quit_on_input = true;
                return;
            }
        }
    }

    /* Make sure to unshift them first. Leave ch->shifted ON so we know to re-shift them
       back on login.*/
    if (IS_SHIFTED(ch)) {
    shift_char(ch, true);
    { const char *_sf = race_get_trait_string(ch->race, "shift_form"); ch->shifted = (_sf && !str_cmp(_sf, "werewolf")) ? SHIFTED_WEREWOLF : SHIFTED_SLAYER; }
    }

    if (ch->pulled_cart != NULL)
    do_function(ch, &do_drop, ch->pulled_cart->name);

    /* -- Removing this for now, working on bringing legacy up to 2.0 state. Leaving commented to return to later.
    if (auto_war != NULL && ch->in_war)
    {
    char_from_team(ch);

    if (ch->in_room != NULL && ch->in_room == get_reserved_room_index("room_war_staging"))
    {
    char_from_room(ch);
    char_to_room(ch, get_reserved_room_index("room_default_recall"));
    }

    test_for_end_of_war();
    }
    */

    if (ch->ambush != NULL)
    send_to_char("You stop your ambush.\n\r", ch);

    /* remove any PURGE_QUIT tokens on the character */
    for (token = ch->tokens; token != NULL; token = token_next) {
    token_next = token->next;

    if (IS_SET(token->flags, TOKEN_PURGE_QUIT)) {
        p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "char update: token %s(%ld) char %s(%ld) was purged on quit",
        token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        token_from_char(token);
        free_token(token);
    }
    }


    send_to_char(
    "Alas, all good things must come to an end.\n\r", ch);
    send_to_char(
    "{MThanks for playing {WS E N T I E N C E{M.\n\r", ch);
    send_to_char(
    "{MWe hope you enjoyed your stay and see you again soon!{x\n\r", ch);

    act("$$n has left the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    sprintf(log_buf, "%s has quit.", ch->name);

    plog(LOG_INFO, log_buf);
    wiznet("$N rejoins the real world.",
    ch, NULL, WIZ_LOGINS, 0, get_staff_rank(ch));

    /* save wearing info */
    save_last_wear(ch);

    /* Remove hitpoint type affects */
ITERATOR it;


iterator_start(&it, ch->lworn);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    for (paf = obj->affected; paf != NULL; paf = paf->next)
        affect_modify(ch, paf, false);
}
iterator_stop(&it);

    /* Reset imms bank accounts */
    if (IS_IMMORTAL(ch)
    &&  ch->tot_level < MAX_LEVEL)
    {
    ch->gold = 0;
    ch->silver = 0;
    ch->pcdata->bankbalance = 0;
    }

    // Reset manastore to zero - even on imms
    ch->manastore = 0;

    save_char_obj(ch);

    // Save account to persist metadata updates (last_login, last_area, level, etc.)
    if (ch->desc && ch->desc->account) {
        save_account(ch->desc->account);
    }

    // Mark character as inactive in Redis cache
    redis_set_char_active(ch->name, false);

    if (MOUNTED(ch))
    {
    CHAR_DATA *mount;

    die_follower(ch);

    mount = MOUNTED(ch);

    ch->mount = NULL;
    ch->riding = false;

    mount->rider = NULL;
    mount->riding = false;

    if (!str_cmp(mount->pIndexData->owner, ch->name))
    {
    act("$n wanders on home.", mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, get_verb_form(mount, "wanders", "wander"), NULL);
    if (mount->home_room != NULL)
    {
    char_from_room(mount);
    char_to_room(mount, mount->home_room);
    }
    else
    extract_char(mount, true);
    }
    }

    /* kill quest obj's linked to char, to be restored upon boot-up */
    if (ch->quest != NULL) {
        QUEST_PART_DATA *part;

        for (part = ch->quest->parts; part != NULL; part = part->next) {
        if (part->pObj != NULL)
            extract_obj(part->pObj);
        }
    }

    id[0] = ch->id[0];
    id[1] = ch->id[1];

    d = ch->desc;

    connection_remove(d);

    extract_char(ch, true);

    if (d != NULL)
    close_socket(d);

    /* toast evil cheating bastards (?) */
    for (d = descriptor_list; d != NULL; d = d_next)
    {
    CHAR_DATA *tch;

    d_next = d->next;
    tch = d->original ? d->original : d->character;
    if (tch && tch->id[0] == id[0] && tch->id[1] == id[1])
    {
    extract_char(tch,true);
    close_socket(d);
    }
    }
}

/**
 * do_logout - Return to account menu without disconnecting
 *
 * Saves the character and returns to the account/character selection menu.
 * Similar to do_quit but doesn't close the connection - instead transitions
 * the descriptor to CON_ACCOUNT_MENU state.
 *
 * Performs similar cleanup to do_quit:
 *   - Unshifting vampires/werewolves
 *   - Releasing pulled carts
 *   - Returning mail package items
 *   - Removing PURGE_QUIT tokens
 *   - Saving worn equipment positions
 *   - Removing worn item affects
 *   - Resetting immortal bank accounts
 *   - Resetting manastore
 *   - Updating account character entry
 *   - Handling mounts
 *
 * @param ch        The character logging out
 * @param argument  If non-empty, triggers TRIG_QUIT before logout
 *
 * Restrictions: Same as do_quit (fighting, auction, stunned, no_quit room, switched)
 */
void do_logout(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;
    ACCOUNT_DATA *account = NULL;
    OBJ_DATA *obj;
    AFFECT_DATA *paf;
    TOKEN_DATA *token, *token_next;

    if (ch->position == POS_FIGHTING)
    {
        send_to_char("No way! You are fighting.\n\r", ch);
        return;
    }

    if (auction_info.high_bidder == ch || auction_info.owner == ch)
    {
        send_to_char("You still have a stake in the auction!\n\r", ch);
        return;
    }

    if (ch->position < POS_STUNNED)
    {
        send_to_char("You're not DEAD yet.\n\r", ch);
        return;
    }

    if (IS_SET(ch->in_room->room_flag[1], ROOM_NO_QUIT))
    {
        send_to_char("You can't logout here.\n\r", ch);
        return;
    }

    /* If person is putting together a package give them back items etc */
    if (ch->mail != NULL)
    {
        OBJ_DATA *obj;
        OBJ_DATA *obj_next;

        for (obj = ch->mail->objects; obj != NULL; obj = obj_next)
        {
            obj_next = obj->next_content;

            obj_from_mail(obj);
            obj_to_char(obj, ch);
            act("You take $p from your package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n takes $p from $s package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, get_verb_form(ch, "takes", "take"), NULL);
        }

        send_to_char("You discard your mail package.\n\r", ch);
        free_mail(ch->mail);
        ch->mail = NULL;
    }

    /* Check for triggers */
    if (argument && argument[0] != '\0')
    {
        p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_QUIT, NULL);
        if (ch->desc && ch->desc->input)
        {
            ch->pcdata->quit_on_input = true;
            return;
        }
    }

    /* Make sure to unshift them first. Leave ch->shifted ON so we know to re-shift them
       back on login.*/
    if (IS_SHIFTED(ch))
    {
        shift_char(ch, true);
        { const char *_sf = race_get_trait_string(ch->race, "shift_form"); ch->shifted = (_sf && !str_cmp(_sf, "werewolf")) ? SHIFTED_WEREWOLF : SHIFTED_SLAYER; }
    }

    if (ch->pulled_cart != NULL)
        do_function(ch, &do_drop, ch->pulled_cart->name);

    if (ch->ambush != NULL)
        send_to_char("You stop your ambush.\n\r", ch);

    /* remove any PURGE_QUIT tokens on the character */
    for (token = ch->tokens; token != NULL; token = token_next)
    {
        token_next = token->next;

        if (IS_SET(token->flags, TOKEN_PURGE_QUIT))
        {
            p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

            plogf(LOG_INFO, "token %s(%ld) char %s(%ld) was purged on logout",
                token->name, token->pIndexData->vnum, HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
            token_from_char(token);
            free_token(token);
        }
    }

    send_to_char("You return to the account menu.\n\r", ch);
    act("$n has left the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    plogf(LOG_INFO, "%s has logged out to character selection.", ch->name);

    wiznet("$N returns to character selection.", ch, NULL, WIZ_LOGINS, 0, get_staff_rank(ch));

    /* save wearing info */
    save_last_wear(ch);

    /* Remove hitpoint type affects */
ITERATOR it;


iterator_start(&it, ch->lworn);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    for (paf = obj->affected; paf != NULL; paf = paf->next)
        affect_modify(ch, paf, false);
}
iterator_stop(&it);

    /* Reset imms bank accounts */
    if (IS_IMMORTAL(ch) && !IS_IMPLEMENTOR(ch))
    {
        ch->gold = 0;
        ch->silver = 0;
        ch->pcdata->bankbalance = 0;
    }

    // Reset manastore to zero - even on imms
    ch->manastore = 0;

    // Update account character entry with latest info
    if (ch->desc && ch->desc->account)
        account_add_character(ch->desc->account, ch);

    save_char_obj(ch);

    if (MOUNTED(ch))
    {
        CHAR_DATA *mount;

        die_follower(ch);

        mount = MOUNTED(ch);

        ch->mount = NULL;
        ch->riding = false;

        mount->rider = NULL;
        mount->riding = false;

        if (!str_cmp(mount->pIndexData->owner, ch->name))
        {
            act("$n wanders on home.", mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            if (mount->home_room != NULL)
            {
                char_from_room(mount);
                char_to_room(mount, mount->home_room);
            }
            else
                extract_char(mount, true);
        }
    }

    d = ch->desc;

    if (d != NULL)
    {
        // Save reference to the account before disconnecting character
        if (d->account)
            account = d->account;

        if (d->editor != 0)
            edit_done(ch);

        // Properly detach the character from the descriptor before extracting
        d->character = NULL;
        ch->desc = NULL;
        
        // Extract the character
        extract_char(ch, true);
        
        // Restore the account connection and transition to account menu
        if (account) {
            d->account = account;
            d->connected = CON_ACCOUNT_MENU;
            d->incomm[0] = '\0';

            
            d->showstr_head	= NULL;
            d->showstr_point = NULL;
            d->outsize	= 2000;
            d->pEdit		= NULL;			/* OLC */
            d->pString	= NULL;			/* OLC */
            d->editor	= 0;			/* OLC */
            d->mfa_verified = false;
            

            
            // Clear out any input buffer
            if (d->inbuf[0]) 
                d->inbuf[0] = '\0';
            
            display_account_menu(d);
        } else {
            // If we somehow lost the account reference, close the connection
            write_to_buffer(d, "\n\rError returning to account menu. Disconnecting...\n\r", 0);
            close_socket(d);
        }
    }
}


/**
 * do_save - Manually save character data to disk
 *
 * Saves the character's current state to their pfile. Non-immortals
 * are rate-limited to one manual save every 3 seconds without blocking
 * other input.
 *
 * @param ch        The character saving
 * @param argument  Unused
 *
 * Restriction: Cannot save while switched into another form
 *
 * Planned refactor: player.c (never executed)
 */
void do_save(CHAR_DATA *ch, char *argument)
{
    if (IS_SWITCHED(ch))
    {
    send_to_char("You can't save in morphed form.\n\r", ch);
    return;
    }

    if (IS_NPC(ch))
    return;

    /* Rate-limit manual saves for non-immortals without blocking input */
    int cooldown = game_settings.save_cooldown_seconds > 0 ? game_settings.save_cooldown_seconds : 3;
    if (!IS_IMMORTAL(ch) && ch->pcdata->last_manual_save > 0
        && current_time - ch->pcdata->last_manual_save < cooldown)
    {
        int time_remaining = cooldown - (current_time - ch->pcdata->last_manual_save);
        char buf[MSL];
        snprintf(buf, sizeof(buf), "You saved recently. Please wait %d seconds before saving again.\n\r", time_remaining);
        send_to_char(buf, ch);
        return;
    }

    save_char_obj(ch);
    send_to_char("Saving.\n\r", ch);

    if (!IS_IMMORTAL(ch))
        ch->pcdata->last_manual_save = current_time;
}


/**
 * do_follow - Follow another character
 *
 * Makes the character follow another character in the room. Following
 * causes the follower to automatically move when the leader moves.
 * Following yourself stops following your current leader.
 *
 * @param ch        The character who wants to follow
 * @param argument  Name of character to follow, or "self" to stop following
 *
 * Restrictions:
 *   - Cannot follow while charmed (prefers current master)
 *   - Cannot follow dead characters
 *   - Cannot follow someone with PLR_NOFOLLOW unless immortal
 *   - Cannot follow if you have followers of your own
 *
 * Planned refactor: groups.c (never executed)
 */
void do_follow(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *rch;

    one_argument(argument, arg);

    if (is_dead(ch))
    return;

    if (arg[0] == '\0')
    {
    send_to_char("Follow whom?\n\r", ch);
    return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
    send_to_char("They aren't here.\n\r", ch);
    return;
    }

    if (IS_DEAD(victim))
    {
    send_to_char("You can't follow a shadow.\n\r", ch);
    return;
    }

    if (IS_AFFECTED(ch, AFF_CHARM) && ch->master != NULL)
    {
    act("But you'd rather follow $N!", ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    if (victim == ch)
    {
    if (ch->master == NULL)
    {
    send_to_char("You already follow yourself.\n\r", ch);
    return;
    }
    stop_follower(ch,true);
    return;
    }

    if (!IS_NPC(victim) && IS_SET(victim->act[0],PLR_NOFOLLOW) && !IS_IMMORTAL(ch))
    {
    act("$N doesn't seem to want any followers.\n\r", ch,victim,NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
    {
    if (rch->master == ch)
    {
    send_to_char("You can't follow someone else while you have followers of your own.\n\r", ch);
    return;
    }
    }

    REMOVE_BIT(ch->act[0],PLR_NOFOLLOW);

    if (ch->master != NULL)
    stop_follower(ch,true);

    add_follower(ch, victim,true);
}


/**
 * add_follower - Make a character follow another
 *
 * Sets up the follower relationship between two characters. The follower
 * will automatically move when the master moves. Also removes the follower
 * from any current group.
 *
 * @param ch      The character who will follow
 * @param master  The character to be followed
 * @param show    Whether to display follow messages to both parties
 *
 * Errors: Logs error if ch is invalid or already has a master
 *
 * Planned refactor: groups.c (never executed)
 */
void add_follower(CHAR_DATA *ch, CHAR_DATA *master, bool show)
{
    if (!IS_VALID(ch))
    {
    pbugf(LOG_ERROR, "Invalid ch.");
    return;
    }

    if (ch->master != NULL)
    {
    pbugf(LOG_ERROR, "Non-null master.");
    return;
    }

    ch->master = master;
    stop_grouped(ch);

    if (can_see(master, ch)) {
    if(show) act("$n now follows you.", ch, master, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }

    if(show) act("You now follow $N.",  ch, master, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
}

/**
 * stop_follower - Stop a character from following their master
 *
 * Breaks the follower relationship. Also removes charm effects if present,
 * clears pet pointer if this was the master's pet, and removes from group.
 *
 * @param ch    The character who will stop following
 * @param show  Whether to display stop-following messages
 *
 * Errors: Logs error if ch is invalid or has no master
 *
 * Planned refactor: groups.c (never executed)
 */
void stop_follower(CHAR_DATA *ch, bool show)
    {
    if (!IS_VALID(ch))
    {
    pbugf(LOG_ERROR, "Invalid ch.");
    return;
    }

    if (ch->master == NULL)
    {
    pbugf(LOG_ERROR, "Null master.");
    return;
    }

    if (IS_AFFECTED(ch, AFF_CHARM))
    {
    REMOVE_BIT(ch->affected_by[0], AFF_CHARM);
    affect_strip(ch, skill_resolve_gsn("charm person"));
    }

    if (can_see(ch->master, ch) && ch->in_room) {
    if(show) {
    act("$n stops following you.",     ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL   );
    act("You stop following $N.",      ch, ch->master, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL   );
    }
    }

    if (IS_VALID(ch->master->pet) && ch->master->pet == ch)
    ch->master->pet = NULL;

    ch->master = NULL;
    stop_grouped(ch);
}


/**
 * nuke_pets - Remove all pets and mounts from a character
 *
 * Stops following and extracts the character's pet from the game.
 * Also clears mount relationships.
 *
 * @param ch  The character whose pets/mounts should be removed
 *
 * Planned refactor: groups.c (never executed)
 */
void nuke_pets(CHAR_DATA *ch)
{
    CHAR_DATA *pet;

    if ((pet = ch->pet) != NULL)
    {
    stop_follower(pet,true);

    if (pet->in_room != NULL)
    act("$N slowly fades away.",ch,pet, NULL,NULL, NULL, NULL, NULL,TO_NOTVICT, NULL, NULL);

    extract_char(pet,true);
    }

    ch->pet = NULL;

    if (ch->mount)
    {
    if (ch->mount != NULL)
    {
    ch->mount->rider = NULL;
    ch->mount->riding = false;
    }
    }

    ch->mount = NULL;
    ch->riding = false;
}


/**
 * die_follower - Clean up all follower/group relationships
 *
 * Called when a character dies or logs out. Handles:
 *   - Stopping following if character was following someone
 *   - Clearing master's pet pointer if this was their pet
 *   - Removing from group
 *   - Making all followers of this character stop following
 *   - Reassigning group leadership for anyone in this character's group
 *
 * @param ch  The character whose relationships should be cleaned up
 *
 * Planned refactor: groups.c (never executed)
 */
void die_follower(CHAR_DATA *ch)
{
    CHAR_DATA *fch;
    ITERATOR it;

    if (ch->master != NULL)
    {
        ch->master->pet = NULL;
        stop_follower(ch,true);
    }

    stop_grouped(ch);

    iterator_start(&it, loaded_chars);
    while(( fch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if (fch->master == ch)
            stop_follower(fch,true);

        if (fch->leader == ch)
            add_grouped(fch, fch, true);
    }
    iterator_stop(&it);
}


/**
 * do_order - Command a charmed mob or mount to perform an action
 *
 * Allows a character to give orders to their charmed followers or mount.
 * Only certain safe commands are allowed to prevent abuse.
 *
 * @param ch        The character giving orders
 * @param argument  "<target|mount> <command>"
 *
 * Allowed commands: movement (north/south/etc), stand, eat, drink,
 *                   sit, rest, sleep, open, close
 * Blocked commands: delete, mob (security)
 *
 * Restrictions:
 *   - Cannot order while charmed yourself
 *   - Target must be charmed by you, or be your mount
 *   - Target must be in the same room
 *
 * Planned refactor: groups.c (never executed)
 */
void do_order(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg );
    argument = one_argument(argument, arg2);

    if (!str_cmp(arg2,"delete") || !str_cmp(arg2,"mob"))
    {
    send_to_char("That will NOT be done.\n\r",ch);
    return;
    }

    if (arg[0] == '\0' || argument[0] == '\0')
    {
    send_to_char("Order whom to do what?\n\r", ch);
    return;
    }

    if (IS_AFFECTED(ch, AFF_CHARM))
    {
    send_to_char("You feel like taking, not giving, orders.\n\r", ch);
    return;
    }

    if (!strcmp(arg, "mount"))
    {
    if (!ch->mount)
    {
    send_to_char("Your don't have a mount.\n\r", ch);
    return;
    }

    if (ch->mount->in_room != ch->in_room)
    {
    send_to_char("Your mount isn't here!\n\r", ch);
    return;
    }
    else
    victim = ch->mount;
    }
    else if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
    send_to_char("They aren't here.\n\r", ch);
    return;
    }

    if (victim == ch)
    {
    send_to_char("Just do it.\n\r", ch);
    return;
    }

    if (!IS_AFFECTED(victim, AFF_CHARM) || victim->master != ch
    ||  (IS_IMMORTAL(victim) && victim->trust >= ch->trust))
    {
    act("$N has no interest in taking orders from you.",
    ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    if ( !str_prefix(arg2, "north") ||
    !str_prefix(arg2, "south") ||
    !str_prefix(arg2, "west") ||
    !str_prefix(arg2, "east") ||
    !str_prefix(arg2, "stand") ||
    !str_prefix(arg2, "eat") ||
    !str_prefix(arg2, "drink") ||
    !str_prefix(arg2, "sit") ||
    !str_prefix(arg2, "rest") ||
    !str_prefix(arg2, "sleep") ||
    !str_prefix(arg2, "open") ||
    !str_prefix(arg2, "close"))
    {
    sprintf(buf,"%s orders you to \'%s\'.", ch->name, argument);
    send_to_char(buf, victim);
    interpret(victim, argument);
    return;
    }
    else
    {
    act("$N ignores your orders.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }
}


/**
 * do_ungroup - Leave or disband the current group
 *
 * Removes the character from their group and cleans up all follower
 * relationships by calling die_follower().
 *
 * @param ch        The character ungrouping
 * @param argument  Unused
 *
 * Planned refactor: groups.c (never executed)
 */
void do_ungroup(CHAR_DATA *ch, char *argument)
{
    send_to_char("Ungrouping.\n\r", ch);
    die_follower(ch);
}


/**
 * do_group - View group status or add/remove group members
 *
 * Without argument: Displays all group members with their level, race,
 * name, HP/mana/move stats, and hired NPC expiration times.
 *
 * With argument: Adds or removes the named character from the group.
 * Only the group leader can add/remove members.
 *
 * @param ch        The character viewing/managing group
 * @param argument  Name of character to add/remove, or empty to view
 *
 * Restrictions:
 *   - Cannot manage group while following someone else
 *   - Cannot remove charmed mobs from group
 *   - Max group size: 9 members
 *
 * Planned refactor: groups.c (never executed)
 */
void do_group(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    ITERATOR it;

    one_argument(argument, arg);

    /* Show group status */
    if (arg[0] == '\0') {
        CHAR_DATA *gch;
        CHAR_DATA *leader;
        GROUP_DATA *group;

        leader = (ch->leader != NULL) ? ch->leader : ch;
        group = IS_VALID(ch->group) ? ch->group : (IS_VALID(leader) ? leader->group : NULL);

        if (IS_VALID(group) && IS_VALID(group->leader))
            leader = group->leader;

        sprintf(buf, "{Y%s's group is currently formed by:\n\r", pers(leader, ch));
        send_to_char(buf, ch);

        if (IS_VALID(group) && group->members)
            iterator_start(&it, group->members);
        else
            iterator_start(&it, loaded_chars);

        while(( gch = (CHAR_DATA *)iterator_nextdata(&it))) {
            if ((IS_VALID(group) && gch->group == group) || (!IS_VALID(group) && (is_same_group(gch, ch) || gch == ch))) {
                char name[MSL];
                char race[MSL];
                char hired_time[100];

                sprintf(name, "%s", pers(gch, ch)) ;
                name[0] = UPPER(name[0]);

                if (gch->race)
                {
                    sprintf(race, "%s", gch->race->name);
                    race[0] = UPPER(race[0]);
                }
                else
                {
                    strcpy(race, "Unknown");
                }

                if( IS_NPC(gch) && IS_SET(gch->act[1], ACT2_HIRED) )
                {
                    strftime(hired_time, 100, "%Y-%m-%d %X %Z", localtime(&gch->hired_to));
                }
                sprintf(buf,
                    "{B[{G%3d %s%-6.6s{B] {G%-15.15s {w%6ld{B/{w%ld {Bhp {w%6ld{B/{w%ld {Bmana {w%6ld{B/{w%ld {Bmv{x %s%s{X\n\r",
                    gch->tot_level,
                    IS_NPC(gch) ? "{A" : "{Y",
                    race,
                    name,
                    gch->hit,   gch->max_hit,
                    gch->mana,  gch->max_mana,
                    gch->move,  gch->max_move,
                    IS_SET(gch->act[1], ACT2_HIRED) ? "{BUntil:{W " : "",
                    IS_SET(gch->act[1], ACT2_HIRED) ? hired_time : "");
                send_to_char(buf, ch);
            }
        }
        iterator_stop(&it);
        return;
    }

    /* Put someone else into your group (or remove them)*/
    if ((victim = get_char_room(ch, NULL, arg)) == NULL) {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (ch->master != NULL || (ch->leader != NULL && ch->leader != ch)) {
        send_to_char("But you are following someone else!\n\r", ch);
        return;
    }

    if (victim->master != ch && ch != victim) {
        act_new("$N $Z following you.",ch,victim,NULL, NULL, get_verb_form(victim,"isn't", "aren't"),NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
        return;
    }

    if (IS_AFFECTED(victim,AFF_CHARM)) {
        send_to_char("You can't remove charmed mobs from your group.\n\r",ch);
        return;
    }

    if (IS_AFFECTED(ch,AFF_CHARM)) {
        act_new("You like your master too much to leave $m!", ch,victim,NULL,NULL, NULL, NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
        return;
    }

    if (is_same_group(victim, ch) && ch != victim) {
        stop_grouped(victim);
        act_new("$n removes $N from $s group.", ch,victim,NULL,NULL,NULL, NULL,NULL,NULL,NULL,TO_NOTVICT,POS_RESTING,NULL);
        act_new("$n removes you from $s group.", ch,victim,NULL, NULL,NULL,NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
        act_new("You remove $N from your group.", ch,victim,NULL,NULL,NULL, NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
        return;
    }

    if (ch == victim) {
        send_to_char("That would be pointless.\n\r", ch);
        return;
    }

    if (!add_grouped(victim, ch,true))
        return;

    act_new("$N joins $n's group.",ch,victim,NULL,NULL,NULL,NULL,NULL,NULL,NULL,TO_NOTVICT,POS_RESTING,NULL);
    act_new("You join $n's group.",ch,victim,NULL,NULL,NULL,NULL,NULL,NULL,NULL,TO_VICT,POS_SLEEPING,NULL);
    act_new("$N joins your group.",ch,victim,NULL,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
}


/**
 * do_split - Split gold and silver among group members in the room
 *
 * Divides the specified amount of currency equally among all non-charmed
 * group members present in the same room. The splitter keeps any remainder.
 *
 * @param ch        The character splitting currency
 * @param argument  "<silver> [gold]" - amounts to split
 *
 * Notes:
 *   - Only counts group members in the same room
 *   - Charmed mobs don't receive a share
 *   - Requires at least 2 eligible recipients
 *   - Splitter gets the remainder from division
 *
 * Planned refactor: groups.c (never executed)
 */
void do_split(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH],arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *gch;
    int members;
    long amount_gold = 0, amount_silver = 0;
    long share_gold, share_silver;
    long extra_gold, extra_silver;

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
    send_to_char("Split how much?\n\r", ch);
    return;
    }

    amount_silver = atol(arg1);

    if (arg2[0] != '\0')
    amount_gold = atol(arg2);

    if (amount_gold < 0 || amount_silver < 0)
    {
    send_to_char("Your group wouldn't like that.\n\r", ch);
    return;
    }

    if (amount_gold == 0 && amount_silver == 0)
    {
    send_to_char("You hand out zero coins, but no one notices.\n\r", ch);
    return;
    }

    if (ch->gold <  amount_gold || ch->silver < amount_silver)
    {
    send_to_char("You don't have that much to split.\n\r", ch);
    return;
    }

    members = 0;
    for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
    {
    if (is_same_group(gch, ch) && !IS_AFFECTED(gch,AFF_CHARM))
    members++;
    }

    if (members < 2)
    {
    send_to_char("Just keep it all.\n\r", ch);
    return;
    }

    share_silver = amount_silver / members;
    extra_silver = amount_silver % members;

    share_gold   = amount_gold / members;
    extra_gold   = amount_gold % members;

    if (share_gold == 0 && share_silver == 0)
    {
    send_to_char("Don't even bother, cheapskate.\n\r", ch);
    return;
    }

    ch->silver	-= amount_silver;
    ch->silver	+= share_silver + extra_silver;
    ch->gold 	-= amount_gold;
    ch->gold 	+= share_gold + extra_gold;

    if (share_silver > 0)
    {
    sprintf(buf,
    "You split %ld silver coins. Your share is %ld silver.\n\r",
    amount_silver,share_silver + extra_silver);
    send_to_char(buf,ch);
    }

    if (share_gold > 0)
    {
    sprintf(buf,
    "You split %ld gold coins. Your share is %ld gold.\n\r",
    amount_gold,share_gold + extra_gold);
    send_to_char(buf,ch);
    }

    if (share_gold == 0)
    {
    sprintf(buf,"$n splits %ld silver coins. Your share is %ld silver.",
    amount_silver,share_silver);
    }
    else if (share_silver == 0)
    {
    sprintf(buf,"$n splits %ld gold coins. Your share is %ld gold.",
    amount_gold,share_gold);
    }
    else
    {
    sprintf(buf,
    "$n splits %ld silver and %ld gold coins, giving you %ld silver and %ld gold.\n\r",
    amount_silver,amount_gold,share_silver,share_gold);
    }

    for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
    {
    if (gch != ch && is_same_group(gch,ch) && gch->pcdata != NULL)
    {
    act(buf, ch, gch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    gch->gold += share_gold;
    gch->silver += share_silver;
    }
    }
}


/**
 * do_gtell - Send a message to all group members
 *
 * Broadcasts a message to every member of the character's group,
 * regardless of location (unlike split which requires same room).
 *
 * @param ch        The character sending the group message
 * @param argument  The message to send
 *
 * Blocked by: COMM_NOTELL (revoked tells)
 *
 * Planned refactor: groups.c (never executed)
 */
void do_gtell(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *gch;
    GROUP_DATA *group;
    bool another_person = false;
    ITERATOR it;

    if (argument[0] == '\0') {
        send_to_char("Tell your group what?\n\r", ch);
        return;
    }

    if (channel_policy_sender_revoked(ch, "gtell")) {
        send_to_char("Your message didn't get through!\n\r", ch);
        return;
    }

    group = IS_VALID(ch->group) ? ch->group : NULL;

    if (IS_VALID(group) && group->members)
        iterator_start(&it, group->members);
    else
        iterator_start(&it, loaded_chars);

    while(( gch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
        if ((IS_VALID(group) && gch->group == group) || (!IS_VALID(group) && is_same_group(gch, ch))) {
            if (gch != ch)
                another_person = true;
        }
    }
    iterator_stop(&it);

    if (another_person) {
        if (!channel_service_send(ch, "gtell", argument)) {
            send_to_char("{CYou tell your group '", ch);
            send_to_char(argument, ch);
            send_to_char("'{x\n\r", ch);

            if (IS_VALID(group) && group->members)
                iterator_start(&it, group->members);
            else
                iterator_start(&it, loaded_chars);

            while ((gch = (CHAR_DATA *)iterator_nextdata(&it))) {
                if (gch != ch && ((IS_VALID(group) && gch->group == group) || (!IS_VALID(group) && is_same_group(gch, ch))))
                    act_new("{C$$n tells the group '$t'{x", ch, gch, NULL, NULL, NULL, NULL, NULL, argument, NULL, TO_VICT, POS_SLEEPING, NULL);
            }
            iterator_stop(&it);
        }
    } else
        send_to_char("There are no members in your group.\n\r", ch);

    return;
}


GROUP_DATA *group_create(CHAR_DATA *leader, bool allow_npc_only)
{
    GROUP_DATA *group;

    if (!IS_VALID(leader))
        return NULL;

    if (IS_VALID(leader->group))
        return leader->group;

    if (!loaded_groups)
        return NULL;

    group = alloc_mem(sizeof(GROUP_DATA));
    memset(group, 0, sizeof(GROUP_DATA));
    VALIDATE(group);

    group->members = list_create(false);
    if (!group->members) {
        INVALIDATE(group);
        free_mem(group, sizeof(GROUP_DATA));
        return NULL;
    }

    group->id[0] = (unsigned long)get_pc_id();
    group->id[1] = 0;
    group->leader = leader;
    group->allow_npc_only = allow_npc_only;

    list_appendlink(loaded_groups, group);
    group_add_member(group, leader);

    return group;
}


bool group_add_member(GROUP_DATA *group, CHAR_DATA *ch)
{
    if (!IS_VALID(group) || !IS_VALID(ch) || !group->members)
        return false;

    if (ch->group == group)
        return true;

    if (IS_VALID(ch->group) && ch->group != group)
        group_remove_member(ch, true);

    if (!list_hasdata(group->members, ch))
        list_appendlink(group->members, ch);

    ch->group = group;

    if (!IS_NPC(ch))
        group->player_count++;

    if (!IS_VALID(group->leader))
        group->leader = ch;

    group_sync_legacy_state(group);

    if (!IS_NPC(ch))
        quest_runtime_sync_group_runs_for_character(ch, true);

    return true;
}


void group_disband(GROUP_DATA *group)
{
    CHAR_DATA *member;
    ITERATOR it;

    if (!IS_VALID(group))
        return;

    if (group->members) {
        iterator_start(&it, group->members);
        while ((member = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (IS_VALID(member) && member->group == group) {
                if (!IS_NPC(member))
                    quest_runtime_handle_group_scope_loss(member, group->id);
                member->group = NULL;
                member->leader = NULL;
                member->num_grouped = 0;
                if (member->lgroup)
                    list_clear(member->lgroup);
            }
        }
        iterator_stop(&it);
    }

    if (loaded_groups && list_hasdata(loaded_groups, group))
        list_remlink(loaded_groups, group, false);

    list_destroy(group->members);
    group->members = NULL;

    INVALIDATE(group);
    free_mem(group, sizeof(GROUP_DATA));
}


void group_remove_member(CHAR_DATA *ch, bool disband_if_empty)
{
    GROUP_DATA *group;
    CHAR_DATA *member;
    CHAR_DATA *fallback = NULL;
    bool was_player;
    ITERATOR it;

    if (!IS_VALID(ch) || !IS_VALID(ch->group))
        return;

    group = ch->group;
    was_player = !IS_NPC(ch);

    if (was_player)
        quest_runtime_handle_group_scope_loss(ch, group->id);

    if (group->members && list_hasdata(group->members, ch))
        list_remlink(group->members, ch, false);

    ch->group = NULL;
    ch->leader = NULL;

    if (was_player && group->player_count > 0)
        group->player_count--;

    if (group->leader == ch)
        group->leader = NULL;

    if (group->members && list_size(group->members) > 0 && !IS_VALID(group->leader)) {
        iterator_start(&it, group->members);
        while ((member = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (!IS_VALID(member))
                continue;

            if (!IS_NPC(member)) {
                group->leader = member;
                break;
            }

            if (!fallback)
                fallback = member;
        }
        iterator_stop(&it);

        if (!IS_VALID(group->leader))
            group->leader = fallback;
    }

    if (!group->members || list_size(group->members) < 1) {
        if (disband_if_empty)
            group_disband(group);
        return;
    }

    if (!group->allow_npc_only && group->player_count < 1) {
        group_disband(group);
        return;
    }

    group_sync_legacy_state(group);

    if (IS_VALID(group->leader) && !IS_NPC(group->leader))
        quest_runtime_sync_group_runs_for_character(group->leader, true);
}


void groups_clear_all(void)
{
    GROUP_DATA *group;

    if (!loaded_groups)
        return;

    while ((group = (GROUP_DATA *)list_nthdata(loaded_groups, 1)) != NULL)
        group_disband(group);
}


static void group_sync_legacy_state(GROUP_DATA *group)
{
    CHAR_DATA *member;
    CHAR_DATA *leader;
    ITERATOR it;

    if (!IS_VALID(group) || !group->members)
        return;

    leader = group->leader;

    iterator_start(&it, group->members);
    while ((member = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (!IS_VALID(member))
            continue;

        member->num_grouped = 0;
        if (member->lgroup)
            list_clear(member->lgroup);
    }
    iterator_stop(&it);

    if (!IS_VALID(leader) || !list_hasdata(group->members, leader))
        leader = NULL;

    group->leader = leader;

    if (!IS_VALID(leader)) {
        iterator_start(&it, group->members);
        while ((member = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (IS_VALID(member))
                member->leader = NULL;
        }
        iterator_stop(&it);
        return;
    }

    leader->leader = NULL;

    iterator_start(&it, group->members);
    while ((member = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (!IS_VALID(member) || member == leader)
            continue;

        member->leader = leader;
        leader->num_grouped++;
        if (leader->lgroup && !list_hasdata(leader->lgroup, member))
            list_appendlink(leader->lgroup, member);
    }
    iterator_stop(&it);
}


/**
 * is_same_group - Check if two characters are in the same group
 *
 * Determines if two characters share a group, considering:
 *   - Same character (always true)
 *   - Mount/rider relationships (treated as same group)
 *   - Same group leader
 *
 * @param ach  First character to check
 * @param bch  Second character to check
 *
 * @return true if characters are in the same group, false otherwise
 *
 * Planned refactor: groups.c (never executed)
 */
bool is_same_group(CHAR_DATA *ach, CHAR_DATA *bch)
{
    if (ach == NULL || bch == NULL)
    return false;

    if (ach == bch)
    return true;

    /* Mount fix */
    if (ach == MOUNTED(bch) || bch == MOUNTED(ach))
    return true;

    if (IS_VALID(ach->group) && ach->group == bch->group)
    return true;

    if (ach->leader != NULL)
    ach = ach->leader;

    if (bch->leader != NULL)
    bch = bch->leader;

    return ach == bch;
}


/**
 * do_colour - Toggle ANSI color display
 *
 * Enables or disables color codes in game output. When off, color
 * codes are stripped before sending text to the player.
 *
 * @param ch        The character toggling color
 * @param argument  Unused (color configuration not implemented)
 *
 * Note: Color configuration was planned but never implemented.
 *
 * Planned refactor: player.c (never executed)
 */
void do_colour(CHAR_DATA *ch, char *argument)
{
    char arg[ MAX_STRING_LENGTH ];

    argument = one_argument(argument, arg);

    if (!*arg)
    {
    if(!IS_SET(ch->act[0], PLR_COLOUR))
    {
    SET_BIT(ch->act[0], PLR_COLOUR);
    send_to_char("{bC{ro{yl{co{gr{x is now {rON!{x\n\r", ch);
    }
    else
    {
    send_to_char_bw("Colour is now OFF.\n\r", ch);
    REMOVE_BIT(ch->act[0], PLR_COLOUR);
    }

    return;
    }
    else
    {
    send_to_char_bw("Colour Configuration is unavailable in this\n\r", ch);
    send_to_char_bw("version of colour, sorry\n\r", ch);
    }
}

/**
 * add_grouped - Add a character to a group
 *
 * Adds a character to another character's group. Enforces max group size
 * of 9 members. Fires TRIG_GROUPED script trigger.
 *
 * @param ch      The character being added to the group
 * @param master  The group leader
 * @param show    Whether to display messages if group is full
 *
 * @return true if successfully added, false if group is full
 *
 * Triggers: TRIG_GROUPED
 *
 * Planned refactor: groups.c (never executed)
 */
bool add_grouped(CHAR_DATA *ch, CHAR_DATA *master, bool show)
{
    GROUP_DATA *group;

    if (!IS_VALID(ch) || !IS_VALID(master))
        return false;

    if (IS_VALID(ch->group) && IS_VALID(master->group) && ch->group == master->group)
        return true;

    group = master->group;
    if (!IS_VALID(group))
        group = group_create(master, IS_NPC(master));

    if (!IS_VALID(group))
        return false;

    if (master->num_grouped >= 9)
    {
    if(show) {
    send_to_char("You may only have 9 people in your group.\n\r", master);
    act("$N's group is currently full.", ch, master, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
    return false;
    }

    if (!group_add_member(group, ch))
        return false;

    p_percent_trigger( ch, NULL, NULL, NULL, master, NULL, NULL, NULL, NULL, TRIG_GROUPED, NULL);

    return true;
}


/**
 * stop_grouped - Remove a character from their group
 *
 * Removes the character from their current group, decrements the leader's
 * group count, and fires TRIG_UNGROUPED script trigger.
 *
 * @param ch  The character being removed from the group
 *
 * Triggers: TRIG_UNGROUPED
 * Errors: Logs error if ch is invalid
 *
 * Planned refactor: groups.c (never executed)
 */
void stop_grouped(CHAR_DATA *ch)
{
    CHAR_DATA *leader;
    GROUP_DATA *group;

    if (!IS_VALID(ch))
    {
    pbugf(LOG_ERROR, "Invalid ch.");
    return;
    }

    if (IS_VALID(ch->group)) {
        group = ch->group;
        leader = group->leader;

        group_remove_member(ch, true);
        ch->leader = NULL;

        p_percent_trigger( ch, NULL, NULL, NULL, leader, NULL, NULL, NULL, NULL, TRIG_UNGROUPED, NULL);
        return;
    }


    if (ch->leader != ch)
        if (ch->leader != NULL) {
            ch->leader->num_grouped--;
            if( list_hasdata(ch->leader->lgroup, ch))
                list_remlink(ch->leader->lgroup, ch, false);

        }

    leader = ch->leader;

    ch->leader = NULL;

    p_percent_trigger( ch, NULL, NULL, NULL, leader, NULL, NULL, NULL, NULL, TRIG_UNGROUPED, NULL);

}



/**
 * crier_announce - Broadcast a town crier announcement
 *
 * Sends a message formatted as a town crier announcement to all connected
 * players who have announcements enabled and are not in quiet mode.
 *
 * @param argument  The announcement message
 *
 * Respects: channel announce preference, COMM_QUIET
 *
 * Planned refactor: channels.c (never executed)
 */
void crier_announce(char *argument)
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;

    sprintf(buf, "{MThe Town Crier announces '%s{M'{x\n\r", argument);
    for (d = descriptor_list; d != NULL; d = d->next)
    {
    CHAR_DATA *victim;

    victim = d->original ? d->original : d->character;

    if (d->connected == CON_PLAYING
    &&  pref_check_channel(victim, "announce")
    &&  !IS_SET(victim->comm,COMM_QUIET))
    send_to_char(buf, victim);
    }
}


/**
 * do_announcements - Toggle receiving town crier announcements
 *
 * Allows players to turn off/on receiving town crier broadcast messages.
 *
 * @param ch        The character toggling announcements
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_announcements(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "announcements", argument);
}


/**
 * double_xp - Award 30 minutes of double experience in a player's honor
 *
 * Broadcasts a ceremony message and activates or extends the double XP
 * boost. If a boost is already active, extends it by 30 minutes.
 * Sets boost to 200% (double).
 *
 * @param victim  The character being honored (name used in announcement)
 *
 * Modifies: boost_table[BOOST_EXPERIENCE]
 *
 * Planned refactor: channels.c (never executed)
 */
void double_xp(CHAR_DATA *victim)
{
    struct tm *exp_time;
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "{gIn ceremony of %s, Sentience is blessed with 30 minutes of {GD{YO{GU{YB{GL{YE {GE{YX{GP{YE{GR{YI{GE{YN{GC{YE{G!{x\n\r", victim->name);
    gecho(buf);

    if (boost_table[BOOST_EXPERIENCE].timer == 0)
    exp_time = localtime(&current_time);
    else
    exp_time = localtime(&boost_table[BOOST_EXPERIENCE].timer);
    exp_time->tm_min += 30;
    boost_table[BOOST_EXPERIENCE].timer = mktime(exp_time);
    boost_table[BOOST_EXPERIENCE].boost = 200;
}


/**
 * do_ignore - Manage the player's ignore list
 *
 * Without argument: Displays the current ignore list with names and reasons.
 * With argument: Adds or removes a player from the ignore list.
 *   - If the name is already ignored, removes them
 *   - If new, validates the player exists (online or pfile) and adds them
 *
 * Ignored players cannot send tells or channel messages to this character.
 *
 * @param ch        The character managing their ignore list
 * @param argument  "<name> [reason]" to add/remove, or empty to view list
 *
 * Note: Validates player existence via online lookup or pfile check
 *
 * Planned refactor: channels.c (never executed)
 */
void do_ignore(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    IGNORE_DATA *ignore;
    IGNORE_DATA *ignore_prev;
    CHAR_DATA *victim;
    FILE *fp;
    char player_name[MAX_STRING_LENGTH];
    char player_dir_buf[MAX_STRING_LENGTH];
    const char *player_dir;
    bool found_char;
    bool remove = false;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    int i = 0;

    sprintf(buf, "{Y #   %-12s %s{x\n\r",
    "Person", "Reason");
    send_to_char(buf, ch);

    line(ch, 35, NULL, NULL);
    for (ignore = ch->pcdata->ignoring; ignore != NULL;
    ignore = ignore->next)
    {
    i++;
    sprintf(buf, "{Y%2d): {x%-12s (%.50s)\n\r",
    i,
    capitalize(ignore->name),
    ignore->reason);
    send_to_char(buf, ch);
    }

    if (i == 0)
    send_to_char("No ignores found.\n\r", ch);

    line(ch, 35, NULL, NULL);

    return;
    }

    /* Remove one */
    ignore_prev = NULL;
    for (ignore = ch->pcdata->ignoring; ignore != NULL; ignore = ignore->next)
    {
    if (!str_prefix(arg, ignore->name))
    {
    remove = true;
    break;
    }

    ignore_prev = ignore;
    }

    if (remove)
    {
    sprintf(buf,
    "You are no longer ignoring %s.\n\r", capitalize(ignore->name));
    send_to_char(buf, ch);
    if (ignore_prev == NULL)
    ch->pcdata->ignoring = ignore->next;
    else
    ignore_prev->next = ignore->next;

    free_ignore(ignore);
    return;
    }

    /* Add a new one */
    if (!str_cmp(arg, ch->name))
    {
    send_to_char("You try very hard to ignore yourself, "
    "but fail. Rats!\n\r", ch);
    return;
    }

    if (strlen(arg) > 12)
    arg[12] = '\0';

    if ((victim = get_char_world(ch, arg)) != NULL
    && !IS_NPC(victim))
    {
    found_char = true;
    sprintf(arg, "%s", capitalize(victim->name));
    }
    else
    {
    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(player_name, sizeof(player_name), "%s%c/%s", player_dir, tolower(arg[0]), capitalize(arg));
    if ((fp = fopen(player_name, "r")) == NULL)
    {
    found_char = false;
    }
    else
    {
    found_char = true;
    fclose (fp);
    }
    }

    if (!found_char)
    {
    send_to_char("That player doesn't exist.\n\r", ch);
    return;
    }

    ignore = new_ignore();

    ignore->name = str_dup(arg);

    if (argument[0] == '\0')
    ignore->reason = str_dup("None");
    else
    ignore->reason = str_dup(argument);

    ignore->next = ch->pcdata->ignoring;
    ch->pcdata->ignoring = ignore;

    sprintf(buf, "You are now ignoring %s.\n\r", capitalize(arg));
    send_to_char(buf, ch);
}


/**
 * do_notify - Toggle login notifications
 *
 * Controls whether the player receives notifications when other players
 * log into the game.
 *
 * @param ch        The character toggling notifications
 * @param argument  Unused
 *
 * Planned refactor: channels.c (never executed)
 */
void do_notify(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
    return;

    if (IS_SET(ch->comm, COMM_NOTIFY))
    {
    send_to_char("You will no longer be notified of people entering the game.\n\r", ch);
    REMOVE_BIT(ch->comm, COMM_NOTIFY);
    }
    else
    {
    send_to_char("You will now be notified when people enter the game.\n\r", ch);
    SET_BIT(ch->comm, COMM_NOTIFY);
    }
}


/**
 * do_formstate - Toggle formation HP display in combat
 *
 * Controls whether the player sees HP percentages for their group
 * formation members during combat.
 *
 * @param ch        The character toggling formation state display
 * @param argument  Unused
 *
 * Planned refactor: groups.c (never executed)
 */
void do_formstate(CHAR_DATA *ch, char *argument)
{
    if (IS_SET(ch->comm, COMM_SHOW_FORM_STATE))
    {
    REMOVE_BIT(ch->comm, COMM_SHOW_FORM_STATE);
    act("You will no longer see your formation's health in combat.",
    ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
    else
    {
    SET_BIT(ch->comm, COMM_SHOW_FORM_STATE);
    act("You will now see your formation's health in combat.",
    ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
}


/**
 * sector_echo - Echo a message to all rooms in an area with a specific sector type
 *
 * Broadcasts a message to all players in rooms within the specified area
 * that have the matching sector type. Excludes cloned rooms and instance
 * sections. Immortals see a "SECTOR ECHO>" prefix.
 *
 * @param area     The area to broadcast in
 * @param message  The message to send
 * @param sector   The sector type to match (e.g., SECT_FOREST, SECT_WATER)
 *
 * Planned refactor: Destination unspecified in original MOVED comment
 */
void sector_echo(AREA_DATA *area, char *message, int sector)
{
    DESCRIPTOR_DATA *d;

    if (area == NULL)
    return;

    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING &&
            d->character->in_room != NULL &&
            !room_is_clone(d->character->in_room) &&
            !IS_VALID(d->character->in_room->instance_section) &&
            d->character->in_room->area == area &&
            room_in_sector(d->character->in_room, sector))
        {
            if (IS_IMMORTAL(d->character))
                send_to_char("SECTOR ECHO> ", d->character);
            send_to_char(message, d->character);
            send_to_char("\n\r", d->character);
        }
    }
}

/**
 * area_echo - Echo a message to all rooms in an area
 *
 * Broadcasts a message to all players in rooms within the specified area.
 * Excludes cloned rooms and instance sections.
 * Immortals see an "AREA ECHO>" prefix.
 *
 * @param area     The area to broadcast in
 * @param message  The message to send
 */
void area_echo(AREA_DATA *area, char *message)
{
    DESCRIPTOR_DATA *d;

    if (area == NULL)
    return;

    for (d = descriptor_list; d; d = d->next)
    {
        if (d->connected == CON_PLAYING &&
            d->character->in_room != NULL &&
            !room_is_clone(d->character->in_room) &&
            !IS_VALID(d->character->in_room->instance_section) &&
            d->character->in_room->area == area)
        {
            if (IS_IMMORTAL(d->character))
                send_to_char("AREA ECHO> ", d->character);
            send_to_char(message, d->character);
            send_to_char("\n\r", d->character);
        }
    }
}


/**
 * do_autoeq - Toggle showing empty equipment slots
 *
 * Controls whether the equipment display shows "<empty>" for slots
 * without items equipped.
 *
 * @param ch        The character toggling the setting
 * @param argument  Unused
 *
 * Planned refactor: player.c (never executed)
 */
void do_autoeq(CHAR_DATA *ch, char *argument)
{
    if (IS_SET(ch->act[0], PLR_AUTOEQ))
    {
    send_to_char("You will no longer see empty equipment spots.\n\r", ch);
    REMOVE_BIT(ch->act[0], PLR_AUTOEQ);
    return;
    }
    else
    {
    send_to_char("You will now see empty equipment spots.\n\r", ch);
    SET_BIT(ch->act[0], PLR_AUTOEQ);
    return;
    }
}


/**
 * do_qlist - Manage the quiet list (who can tell you while in quiet mode)
 *
 * Without argument or "show": Displays the current quiet list.
 * With "clear": Removes everyone from the quiet list.
 * With a name: Adds or removes that player from the quiet list.
 *
 * Players on your quiet list can still send you tells even when you
 * have quiet mode enabled.
 *
 * @param ch        The character managing their quiet list
 * @param argument  "show", "clear", or a player name
 *
 * Max list size: 20 entries
 *
 * Planned refactor: channels.c (never executed)
 */
void do_qlist(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[MSL];
    char player_name[MSL];
    char player_dir_buf[MSL];
    const char *player_dir;
    bool found_char;
    FILE *fp;
    STRING_DATA *string;
    STRING_DATA *string_prev;
    bool found;
    int i;

    argument = one_argument(argument, arg);
    arg[0] = UPPER(arg[0]);
    if (strlen(arg) > 12)
    arg[12] = '\0';

    if (arg[0] == '\0' || !str_cmp(arg, "show"))
    {
    send_to_char("{YQuiet list:{x\n\r", ch);
    line(ch, 45, NULL, NULL);
    i = 0;
    for (string = ch->pcdata->quiet_people; string != NULL;
    string = string->next)
    {
    sprintf(buf, "{Y%2d):{x %s\n\r", i + 1, string->string);
    send_to_char(buf, ch);
    i++;
    }

    if (i == 0)
    send_to_char("Nobody.\n\r", ch);

    line(ch, 45, NULL, NULL);

    return;
    }

    /* take everyone off */
    if (!str_cmp(arg, "clear"))
    {
    STRING_DATA *string_next;

    for (string = ch->pcdata->quiet_people; string != NULL; string = string_next)
    {
    string_next = string->next;
    do_function(ch, &do_qlist, string->string);
    }

    send_to_char("Quiet list cleared.\n\r", ch);
    return;
    }

    if (!str_cmp(arg, ch->name))
    {
    send_to_char("That would be pointless.\n\r", ch);
    return;
    }

    found = false;
    string_prev = NULL;
    for (string = ch->pcdata->quiet_people; string != NULL;
    string = string->next)
    {
    if (!str_prefix(arg, string->string))
    {
    found = true;
    break;
    }

    string_prev = string;
    }

    if (found)
    {
    act("Removed $t from quiet list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR, NULL, NULL);
    if (string_prev != NULL)
    string_prev->next = string->next;
    else
    ch->pcdata->quiet_people = string->next;
    free_string_data(string);
    return;
    }
    else
    {
    CHAR_DATA *victim;

    i = 0;
    for (string = ch->pcdata->quiet_people; string != NULL;
    string = string->next)
    i++;

    if (i > 19)
    {
    send_to_char("Sorry, maximum is 20 people.\n\r", ch);
    return;
    }

    if ((victim = get_char_world(ch, arg)) != NULL
    && !IS_NPC(victim))
    {
    found_char = true;
    sprintf(arg, "%s", capitalize(victim->name));
    }
    else
    {
    player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    snprintf(player_name, sizeof(player_name), "%s%c/%s", player_dir, tolower(arg[0]), capitalize(arg));
    if ((fp = fopen(player_name, "r")) == NULL)
    {
    found_char = false;
    }
    else
    {
    found_char = true;
    fclose (fp);
    }
    }

    if (!found_char)
    {
    send_to_char("That player doesn't exist.\n\r", ch);
    return;
    }

    string = new_string_data();
    string->string = str_dup(arg);
    act("Added $t to your quiet list.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR, NULL, NULL);
    string->next = ch->pcdata->quiet_people;
    ch->pcdata->quiet_people = string;
    }
}

/**
 * do_whisper - Whisper privately to another character in the room
 *
 * Sends a private message to a specific person in the room. Others in
 * the room see that a whisper occurred but not the content.
 * Triggers TRIG_WHISPER on the recipient.
 *
 * @param ch        The character whispering
 * @param argument  "<target> <message>"
 *
 * Blocked by: AFF2_SILENCE
 * Triggers: TRIG_WHISPER on victim
 *
 * Planned refactor: speech.c (never executed)
 */
void do_whisper(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    argument = one_argument(argument, arg);
    if (arg[0] == '\0' || argument[0] == '\0')
    {
    send_to_char("Syntax: whisper <person> <message>\n\r", ch);
    return;
    }

    if (IS_AFFECTED2(ch, AFF2_SILENCE))
    {
    send_to_char("You attempt to say something but fail!\n\r", ch);
    act("$n opens $s mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
    send_to_char("They aren't in the room.\n\r", ch);
    return;
    }

    if (ch == victim)
    {
    send_to_char("That would be pointless.\n\r", ch);
    return;
    }

    if (channel_service_send_room_targeted(ch, "whisper", victim, argument))
    return;

    act("{CYou whisper to $N '$t'{x", ch, victim,NULL,NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
    act("{C$n whispers something to $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    act("{C$n whispers to you '$t'{x", ch, victim,NULL,NULL, NULL, argument, NULL, TO_VICT, NULL, NULL);

    /* This should only do whisper trigger?
        if (!IS_NPC(ch))
        p_act_trigger(argument, victim, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SPEECH); */

    if (!IS_NPC(ch))
    p_act_trigger(argument, victim, NULL, NULL, ch, NULL, NULL, NULL, NULL,TRIG_WHISPER);

    if (!IS_NPC(ch))
    check_quest_talk_target(ch, victim, argument, true);
}

/**
 * do_catchup - Mark all notes, news, and changes as read
 *
 * Convenience command to catch up on all bulletin board systems at once.
 * Calls catchup on notes, news, and changes.
 *
 * @param ch        The character catching up
 * @param argument  Unused
 *
 * Planned refactor: bulletin.c (never executed)
 */
void do_catchup(CHAR_DATA *ch, char *argument)
{
    do_function(ch, &do_note, "catchup");
    do_function(ch, &do_news, "catchup");
    do_function(ch, &do_changes, "catchup");
    send_to_char("Done.\n\r", ch);
}


/**
 * do_danger - Set sense-danger skill range
 *
 * Configures how far away the sense-danger skill (sith class) can detect
 * hostile players. Range is limited by skill level (skill/10 - 1, max 9).
 *
 * @param ch        The character setting danger range
 * @param argument  The range value (0 to max_range)
 *
 * Requires: gsn_sense_danger skill
 *
 * Planned refactor: fight.c (never executed)
 */
void do_danger(CHAR_DATA *ch, char *argument)
{
    int range;
    int max_range;
    char buf[MSL];

    int16_t sn_danger = skill_resolve_gsn("sense danger");
    if (get_skill(ch, sn_danger) == 0)
    {
    send_to_char("You can't sense danger from players.\n\r", ch);
    return;
    }

    max_range = URANGE(1, get_skill(ch, sn_danger) / 10 - 1, 9);

    if (!is_number(argument))
    {
    sprintf(buf, "Syntax: danger <0-%d>\n\r", max_range);
    send_to_char(buf, ch);
    return;
    }

    if ((range = atoi(argument)) < 0 || range > max_range)
    {
    sprintf(buf, "Syntax: danger <0-%d>\n\r", max_range);
    send_to_char(buf, ch);
    return;
    }

    sprintf(buf, "Set danger range to %d.\n\r", range);
    send_to_char(buf, ch);

    ch->pcdata->danger_range = range;
}


/**
 * do_toggle - View or toggle various player settings
 *
 * Without argument: Displays all available toggle settings and their
 * current status (ON/OFF).
 * With argument: Toggles the specified setting.
 *
 * Settings are defined in pc_set_table and can affect act[], comm,
 * or other character flags. Some settings are inverted (NO_* flags).
 *
 * @param ch        The character viewing/toggling settings
 * @param argument  Setting name to toggle, or empty to view all
 *
 * Planned refactor: player.c (never executed)
 */
void do_toggle(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[2*MSL];
    char status[MSL];
    bool found;
    bool is_on;
    int i;
    long *field;
    long vector;
    ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;

    /* Show current settings */
    if (argument[0] == '\0')
    {
    sprintf(buf, "{Y%-15s %s{x\n\r", "Setting", "Status");
    send_to_char(buf, ch);

    line(ch, 22, NULL, NULL);

    for (i = 0; pc_set_table[i].name != NULL; i++)
    {
    if (ch->pcdata->staff_rank >= pc_set_table[i].min_rank)
    {
    if (pc_set_table[i].vector == 0
    &&  pc_set_table[i].vector2 == 0
    &&  pc_set_table[i].vector_comm == 0)
    {
    is_on = pref_get_bool(acct, ch, pc_set_table[i].name,
        pc_set_table[i].default_state == SETTING_ON);
    }
    else
    {
    if (pc_set_table[i].vector != 0)
    {
    vector = pc_set_table[i].vector;
    field = &ch->act[0];
    }
    else if (pc_set_table[i].vector2 != 0)
    {
    vector = pc_set_table[i].vector2;
    field = &ch->act[1];
    }
    else if (pc_set_table[i].vector_comm != 0)
    {
    vector = pc_set_table[i].vector_comm;
    field = &ch->comm;
    }
    else
    {
    pbugf(LOG_ERROR, "do_toggle: no good vector/field for setting %s",
    pc_set_table[i].name);
    continue;
    }

    is_on = IS_SET(*field, vector);
    if (pc_set_table[i].inverted)
    is_on = !is_on;
    }

    if (is_on)
    sprintf(status, "{WON{x");
    else
    sprintf(status, "{DOFF{x");

    sprintf(buf, "%-15s %s\n\r", pc_set_table[i].name, status);
    send_to_char(buf, ch);
    }
    }

    return;
    }

    /* Toggle a setting */
    argument = one_argument(argument, arg);

    found = false;
    for (i = 0; pc_set_table[i].name != NULL; i++)
    {
    if (!str_prefix(arg, pc_set_table[i].name)
    &&  ch->pcdata->staff_rank >= pc_set_table[i].min_rank) {
    found = true;
    break;
    }
    }

    if (!found) {
    send_to_char("That is not a valid setting.\n\r", ch);
    return;
    }

    if (pc_set_table[i].vector == 0
    &&  pc_set_table[i].vector2 == 0
    &&  pc_set_table[i].vector_comm == 0)
    {
    bool current = pref_get_bool(acct, ch, pc_set_table[i].name,
        pc_set_table[i].default_state == SETTING_ON);
    bool new_state = !current;

    pref_set_bool(&ch->pcdata->preferences, PREF_CAT_TOGGLE,
        pc_set_table[i].name, new_state);

    save_char_obj(ch);

    sprintf(buf, "%s is now %s. {Y(character override){x\n\r",
        pc_set_table[i].name,
        new_state ? "{WON{x" : "{DOFF{x");
    send_to_char(buf, ch);
    return;
    }

    if (pc_set_table[i].vector != 0)
    {
    vector = pc_set_table[i].vector;
    field = &ch->act[0];
    }
    else if (pc_set_table[i].vector2 != 0)
    {
    vector = pc_set_table[i].vector2;
    field = &ch->act[1];
    }
    else if (pc_set_table[i].vector_comm != 0)
    {
    vector = pc_set_table[i].vector_comm;
    field = &ch->comm;
    }
    else
    {
    pbugf(LOG_ERROR, "do_toggle: no good vector/field for setting %s",
    pc_set_table[i].name);
    return;
    }

    if (pc_set_table[i].inverted)
    {
    if (IS_SET(*field, vector))
    {
    REMOVE_BIT(*field, vector);
    act("$t is now {WON{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR, NULL, NULL);
    }
    else
    {
    SET_BIT(*field, vector);
    act("$t is now {WOFF{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR, NULL, NULL);
    }
    }
    else
    {
    if (IS_SET(*field, vector))
    {
    REMOVE_BIT(*field, vector);
    act("$t is now {DOFF{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR, NULL, NULL);
    }
    else
    {
    SET_BIT(*field, vector);
    act("$t is now {WON{x.", ch, NULL, NULL, NULL, NULL, pc_set_table[i].name, NULL, TO_CHAR, NULL, NULL);
    }
    }

    /* Persist as character preference override so account defaults
     * do not overwrite this choice on next login. */
    is_on = IS_SET(*field, vector);
    if (pc_set_table[i].inverted)
    is_on = !is_on;

    pref_set_bool(&ch->pcdata->preferences, PREF_CAT_TOGGLE,
    pc_set_table[i].name, is_on);

    save_char_obj(ch);
}

/**
 * do_quote - Quote sharing communication channel
 *
 * Global channel for sharing quotes. If no argument, toggles channel on/off.
 * Supports drunk speech, player flags, and respects ignore lists and quiet mode.
 *
 * @param ch        The character quoting
 * @param argument  Quote text to share, or empty to toggle channel
 *
 * Blocked by: COMM_QUIET, ROOM_NOCOMM, global channel revocation policy
 *
 * Planned refactor: channels.c (never executed)
 */
void do_quote(CHAR_DATA *ch, char *argument)
{
    (void)dispatch_dynamic_channel_command(ch, "quote", argument);
}


/**
 * do_flag - Set a personal chat flag or control flag visibility on channels
 *
 * With channel name: Toggles showing player flags on that channel.
 * With "none": Removes the player's flag.
 * With other text: Sets the text as the player's chat flag (max 10 visible chars).
 *
 * The flag is displayed alongside messages on channels where flag visibility is enabled.
 *
 * @param ch        The character managing their flag
 * @param argument  "<channel>", "none", or flag text
 *
 * Channels: gossip, ooc, yell, flame, quote, helper, tells, music, ct
 *
 * Planned refactor: player.c (never executed)
 */
void do_flag(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[2*MSL];
    int value;
    /*char *c;
     int colours; */

    argument = one_argument_norm(argument, arg);

    if (IS_NPC(ch)) {
    pbugf(LOG_ERROR, "NPC");
    return;
    }

    if (arg[0] == '\0') {
    send_to_char("Syntax: flag <new flag|none>\n\r"
    "        flag <gossip|ooc|yell|flame|quote|helper|tells|music|ct>\n\r", ch);
    return;
    }

    if ((value = flag_value(channel_flags, arg)) != NO_FLAG) {
    if (!IS_SET(ch->pcdata->channel_flags, value)) {
    SET_BIT(ch->pcdata->channel_flags, value);
    sprintf(buf, "You will now see player flags on the %s channel.\n\r", flag_name(channel_flags, value));
    } else {
    REMOVE_BIT(ch->pcdata->channel_flags, value);
    sprintf(buf, "You will no longer see player flags on the %s channel.\n\r", flag_name(channel_flags, value));
    }

    send_to_char(buf, ch);
    return;
    }

    if (!str_cmp(arg, "none")) {
    send_to_char("Flag removed.\n\r", ch);
    free_string(ch->pcdata->flag);
    ch->pcdata->flag = NULL;
    return;
    }

    if (strlen_no_colours(arg) > 10) {
    send_to_char("Flag length limit is 10 characters (not counting colour codes).\n\r", ch);
    return;
    }
    /*
    colours = 0;
    for (c = arg; *c != '\0'; c++) {
    if (*c == '{')
    colours++;
    }


    if (colours > 5) {
    send_to_char("You may only use 5 colour codes in your flag.\n\r", ch);
    return;
    }
    */
    free_string(ch->pcdata->flag);
    ch->pcdata->flag = str_dup(arg);
    sprintf(buf, "You have changed your flag to \"%s{x\".\n\r", arg);
    send_to_char(buf, ch);
}

/**
 * do_sayto - Targeted speech to a specific character in the room
 *
 * Similar to do_say but explicitly directed at a target. Uses the same
 * dynamic sentence formatting based on punctuation (!/?/.).
 * Triggers TRIG_SAYTO on the target (useful for NPC interactions).
 *
 * Added by NIB (Nibelung) 2007-01-21 for targeted speech to mobiles.
 *
 * @param ch        The character speaking
 * @param argument  "<target> <message>"
 *
 * Blocked by: AFF2_SILENCE
 * Triggers: TRIG_SAYTO on victim
 * Modifiers: Drunk speech when intoxicated
 *
 * Planned refactor: speech.c (never executed)
 */
void do_sayto(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    char buf[MSL];
    char buf2[MSL], msg[MSL];
    int i;
    char *second;
    CHAR_DATA *victim;
    bool break_line = true;

    if (!argument[0]) {
    send_to_char("Say to whom and what?\n\r", ch);
    return;
    }

    argument = one_argument(argument,arg);
    if (!(victim = get_char_room(ch, NULL, arg))) {
    send_to_char("They aren't in the room.\n\r", ch);
    return;
    }
    if (victim == ch) {
    send_to_char("Talking to yourself is a sure sign that you need help.\n\r", ch);
    return;
    }

    if (!argument[0]) {
    send_to_char("Say what?\n\r", ch);
    return;
    }

    if (IS_AFFECTED2(ch, AFF2_SILENCE)) {
    send_to_char("You attempt to say something but fail!\n\r", ch);
    act("$n opens $s mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
    }

    msg[0] = '\0';
    STRIP_COLOUR(argument, msg);

    if (!msg[0]) {
    send_to_char("Say what?\n\r", ch);
    return;
    }

    if (channel_service_send_room_targeted(ch, "sayto", victim, msg))
    return;

    buf[0] = '\0';
    for (i = 0; msg[i]; i++)
    if (msg[i] == '!' && msg[i+1] != ' ')
    break_line = false;

    if (break_line) {
    second = stptok(msg, buf, sizeof(buf), "!");
    while(*second == ' ')
    second++;

    if (*second) {
    sprintf(buf2, "{C'$t!{C' exclaims $n to $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL,buf,NULL, TO_NOTVICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' exclaims $n to you. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL,buf, NULL, TO_VICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' you exclaim to $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL,buf, NULL, TO_CHAR, NULL, NULL);
    return;
    }
    }

    for (i = 0; msg[i]; i++)
    if (msg[i] == '?' && msg[i+1] != ' ')
    break_line = false;

    if (break_line) {
    second = stptok(msg, buf, sizeof(buf), "?");
    while(*second == ' ')
    second++;

    if (*second) {
    sprintf(buf2, "{C'$t!{C' $n asks $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_NOTVICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' $n asks you. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_VICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' you ask $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_CHAR, NULL, NULL);
    return;
    }
    }

    for (i = 0; msg[i] != '\0'; i++)
    if (msg[i] == '.' && msg[i+1] != ' ')
    break_line = false;

    if (break_line) {
    second = stptok(msg, buf, sizeof(buf), ".");
    while(*second == ' ')
    second++;

    if (*second) {
    sprintf(buf2, "{C'$t!{C' says $n to $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_NOTVICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' says $n to you. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_VICT, NULL, NULL);
    sprintf(buf2, "{C'$t!{C' you say to $N. '");
    strcat(buf2, second);
    strcat(buf2, "'{x");
    act(buf2, ch, victim, NULL, NULL, NULL, buf, NULL, TO_CHAR, NULL, NULL);
    return;
    }
    }

    i = strlen(msg)-1;
    if (msg[i] == '!') {
    if (number_percent() < 50) {
    act("{C'$t{C' exclaims $n to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C'$t{C' exclaims $n to you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{C'$t{C' you exclaim to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    } else {
    act("{C$n exclaims to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C$n exclaims to you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{CYou exclaim to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    }
    } else if (msg[i] == '?') {
    if (number_percent() < 50) {
    act("{C$n asks $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C$n asks you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{CYou ask $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    } else {
    act("{C'$t{C' $n asks $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C'$t{C' $n asks you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{C'$t{C' you ask $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    }
    } else {
    if (number_percent() < 50) {
    act("{C$n says to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C$n says to you, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{CYou say to $N, '$t{C'{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    } else {
    act("{C'$t{C' says $n to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_NOTVICT, NULL, NULL);
    act("{C'$t{C' says $n to you.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_VICT, NULL, NULL);
    act("{C'$t{C' you say to $N.{x", ch, victim, NULL, NULL, NULL, msg, NULL, TO_CHAR, NULL, NULL);
    }
    }

    if ((!IS_NPC(ch) || IS_SWITCHED(ch)) &&
    (!IS_NPC(victim) || victim->position == victim->pIndexData->default_pos))
    p_act_trigger(msg, victim, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_SAYTO);

    if (!IS_NPC(ch))
    check_quest_talk_target(ch, victim, msg, true);

}

/**
 * do_intone - Speak to an object in the room
 *
 * Allows a character to direct speech at an object rather than a character.
 * This is primarily used for triggering object scripts that respond to speech.
 * The message is displayed to all characters in the room and triggers TRIG_SAYTO
 * on the target object.
 *
 * @param ch        The character speaking
 * @param argument  Format: "<object> <message>"
 *
 * Blocked by: AFF2_SILENCE (character is silenced)
 * Drunk speech: Message is slurred if character is intoxicated
 * Triggers: TRIG_SAYTO on target object
 *
 * Planned refactor: speech.c (never executed)
 */
void do_intone(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    char msg[MSL];
    OBJ_DATA *obj;

    if (!argument[0]) {
    send_to_char("Intone to what and what?\n\r", ch);
    return;
    }

    argument = one_argument(argument,arg);
    if (!(obj = get_obj_here(ch, NULL, arg))) {
    act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR, NULL, NULL);
    return;
    }

    if (!argument[0]) {
    send_to_char("Say what?\n\r", ch);
    return;
    }

    if (IS_AFFECTED2(ch, AFF2_SILENCE)) {
    send_to_char("You attempt to say something but fail!\n\r", ch);
    act("$n opens $s mouth but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
    }

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
    argument = makedrunk(argument,ch);

    msg[0] = '\0';
    STRIP_COLOUR(argument, msg);

    if (!msg[0]) {
    send_to_char("Say what?\n\r", ch);
    return;
    }

    if (channel_service_send_object_targeted(ch, "intone", obj, msg))
    return;

    act("{C$n intones to $p '$t'{x", ch, NULL, NULL, obj, NULL, msg, NULL, TO_ROOM, NULL, NULL);
    act("{CYou intone to $p '$t'{x", ch, NULL, NULL, obj, NULL, msg, NULL, TO_CHAR, NULL, NULL);

    if ((!IS_NPC(ch) || IS_SWITCHED(ch)))
    p_act_trigger(msg, NULL, obj, NULL, ch, NULL, NULL, NULL, NULL,TRIG_SAYTO);
}

/**
 * do_pronouns - View or set custom character pronouns
 *
 * Allows player characters to customize their pronouns for roleplay purposes.
 * Supports all five pronoun forms (subjective, objective, possessive adjective,
 * possessive pronoun, reflexive) plus verb conjugation preference.
 *
 * Subcommands:
 * - (no args): Display current pronouns and usage examples
 * - default: Reset all pronouns to body type defaults
 * - he/subjective <value>: Set subjective pronoun (he/she/they)
 * - him/objective <value>: Set objective pronoun (him/her/them)
 * - his/possessive_adjective <value>: Set possessive adjective (his/her/their)
 * - hers/possessive_pronoun <value>: Set possessive pronoun (his/hers/theirs)
 * - himself/reflexive <value>: Set reflexive pronoun (himself/herself/themself)
 * - verb <singular|plural|default>: Set verb conjugation preference
 * - show <subj> <obj> <poss_adj> <poss_pron> <refl> [verb]: Preview pronoun set
 *
 * @param ch        The character setting pronouns (must be a player)
 * @param argument  The subcommand and optional value
 *
 * Blocked by: IS_NPC (NPCs cannot set custom pronouns)
 * Max length: 20 characters per pronoun
 */
void do_pronouns(CHAR_DATA *ch, char *argument) {
    char arg1[MIL], arg2[MIL];
    char buf[MSL];

    if (IS_NPC(ch)) {
        send_to_char("NPCs cannot set custom pronouns.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Your current pronouns:\n\r", ch);
        sprintf(buf, "  Subjective (he/she/they): {C%s{x\n\r", get_he_she(ch)); send_to_char(buf, ch);
        sprintf(buf, "  Objective (him/her/them): {C%s{x\n\r", get_him_her(ch)); send_to_char(buf, ch);
        sprintf(buf, "  Possessive Adj (his/her): {C%s{x\n\r", get_his_her(ch)); send_to_char(buf, ch);
        sprintf(buf, "  Possessive Pron (his/hers): {C%s{x\n\r", get_his_hers(ch)); send_to_char(buf, ch);
        sprintf(buf, "  Reflexive (himself/herself): {C%s{x\n\r", get_himself_herself(ch)); send_to_char(buf, ch);
        const char *vpref_str = "Default";
        if (ch->verb_preference == VERB_FORM_SINGULAR) vpref_str = "Singular";
        else if (ch->verb_preference == VERB_FORM_PLURAL) vpref_str = "Plural";
        sprintf(buf, "  Verb Preference: {C%s{x\n\r\n\r", vpref_str); send_to_char(buf, ch);
        display_pronoun_examples(ch, get_he_she(ch), get_him_her(ch), get_his_her(ch), get_his_hers(ch), get_himself_herself(ch), ch->verb_preference);
        send_to_char("\n\rSyntax: pronouns [type] [value]\n\r", ch);
        send_to_char("Types: he, him, his, hers, himself, verb, default, show\n\r", ch);
        send_to_char("For 'verb': singular, plural, default\n\r", ch);
        send_to_char("For 'show': pronouns show <subj> <obj> <poss_adj> <poss_pron> <refl> [verb_pref]\n\r", ch);
        return;
    }

    if (!str_prefix(arg1, "default")) {
        free_string(ch->pronoun_he_she); ch->pronoun_he_she = str_dup(body_type_info[ch->body_type].default_he_she);
        free_string(ch->pronoun_him_her); ch->pronoun_him_her = str_dup(body_type_info[ch->body_type].default_him_her);
        free_string(ch->pronoun_his_her); ch->pronoun_his_her = str_dup(body_type_info[ch->body_type].default_his_her);
        free_string(ch->pronoun_his_hers); ch->pronoun_his_hers = str_dup(body_type_info[ch->body_type].default_his_hers);
        free_string(ch->pronoun_himself_herself); ch->pronoun_himself_herself = str_dup(body_type_info[ch->body_type].default_himself_herself);
        ch->verb_preference = body_type_info[ch->body_type].verb_preference;
        send_to_char("Pronouns reset to defaults for your body type.\n\r", ch);
        display_pronoun_examples(ch, get_he_she(ch), get_him_her(ch), get_his_her(ch), get_his_hers(ch), get_himself_herself(ch), ch->verb_preference);
        return;
    }

    if (!str_prefix(arg1, "he") || !str_prefix(arg1, "subjective")) {
        if (arg2[0] == '\0') { send_to_char("Set subjective pronoun to what?\n\r", ch); return; }
        if (strlen(arg2) > 20) { send_to_char("Pronoun is too long (max 20 chars).\n\r", ch); return; }
        free_string(ch->pronoun_he_she); ch->pronoun_he_she = str_dup(arg2);
        send_to_char("Subjective pronoun set.\n\r", ch);
    } else if (!str_prefix(arg1, "him") || !str_prefix(arg1, "objective")) {
        if (arg2[0] == '\0') { send_to_char("Set objective pronoun to what?\n\r", ch); return; }
        if (strlen(arg2) > 20) { send_to_char("Pronoun is too long (max 20 chars).\n\r", ch); return; }
        free_string(ch->pronoun_him_her); ch->pronoun_him_her = str_dup(arg2);
        send_to_char("Objective pronoun set.\n\r", ch);
    } else if (!str_prefix(arg1, "his") || !str_prefix(arg1, "possessive_adjective")) {
        if (arg2[0] == '\0') { send_to_char("Set possessive adjective to what?\n\r", ch); return; }
        if (strlen(arg2) > 20) { send_to_char("Pronoun is too long (max 20 chars).\n\r", ch); return; }
        free_string(ch->pronoun_his_her); ch->pronoun_his_her = str_dup(arg2);
        send_to_char("Possessive adjective set.\n\r", ch);
    } else if (!str_prefix(arg1, "hers") || !str_prefix(arg1, "possessive_pronoun")) { // "hers" is a bit ambiguous, maybe "poss_pron"
        if (arg2[0] == '\0') { send_to_char("Set possessive pronoun to what?\n\r", ch); return; }
        if (strlen(arg2) > 20) { send_to_char("Pronoun is too long (max 20 chars).\n\r", ch); return; }
        free_string(ch->pronoun_his_hers); ch->pronoun_his_hers = str_dup(arg2);
        send_to_char("Possessive pronoun set.\n\r", ch);
    } else if (!str_prefix(arg1, "himself") || !str_prefix(arg1, "reflexive")) {
        if (arg2[0] == '\0') { send_to_char("Set reflexive pronoun to what?\n\r", ch); return; }
        if (strlen(arg2) > 20) { send_to_char("Pronoun is too long (max 20 chars).\n\r", ch); return; }
        free_string(ch->pronoun_himself_herself); ch->pronoun_himself_herself = str_dup(arg2);
        send_to_char("Reflexive pronoun set.\n\r", ch);
    } else if (!str_prefix(arg1, "verb")) {
        if (arg2[0] == '\0') { send_to_char("Set verb preference to singular, plural, or default?\n\r", ch); return; }
        if (!str_prefix(arg2, "singular")) ch->verb_preference = VERB_FORM_SINGULAR;
        else if (!str_prefix(arg2, "plural")) ch->verb_preference = VERB_FORM_PLURAL;
        else if (!str_prefix(arg2, "default")) ch->verb_preference = VERB_FORM_DEFAULT;
        else { send_to_char("Invalid verb preference. Use singular, plural, or default.\n\r", ch); return; }
        send_to_char("Verb preference set.\n\r", ch);
    } else if (!str_prefix(arg1, "show")) {
        char p_subj[MIL], p_obj[MIL], p_pa[MIL], p_pp[MIL], p_refl[MIL], p_verb_str[MIL];
        verb_form_preference_t vpref_show = VERB_FORM_DEFAULT;

        argument = one_argument(argument, p_subj); // arg2 is first pronoun
        argument = one_argument(argument, p_obj);
        argument = one_argument(argument, p_pa);
        argument = one_argument(argument, p_pp);
        argument = one_argument(argument, p_refl);
        one_argument(argument, p_verb_str); // Optional

        if (p_subj[0]=='\0' || p_obj[0]=='\0' || p_pa[0]=='\0' || p_pp[0]=='\0' || p_refl[0]=='\0') {
            send_to_char("Syntax: pronouns show <subj> <obj> <poss_adj> <poss_pron> <refl> [verb_pref]\n\r", ch);
            return;
        }
        if (p_verb_str[0] != '\0') {
            if (!str_prefix(p_verb_str, "singular")) vpref_show = VERB_FORM_SINGULAR;
            else if (!str_prefix(p_verb_str, "plural")) vpref_show = VERB_FORM_PLURAL;
            // Default is already VERB_FORM_DEFAULT
        }
        display_pronoun_examples(ch, p_subj, p_obj, p_pa, p_pp, p_refl, vpref_show);
        return; // Don't show current pronouns after "show"
    }
     else {
        do_pronouns(ch, ""); // Show help
        return;
    }
    // Display updated examples
    display_pronoun_examples(ch, get_he_she(ch), get_him_her(ch), get_his_her(ch), get_his_hers(ch), get_himself_herself(ch), ch->verb_preference);
}