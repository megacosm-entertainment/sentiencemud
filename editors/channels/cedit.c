/***************************************************************************
 *  cedit.c — OLC Channel Editor scaffold                                 *
 *                                                                         *
 *  Framework-based channel definition editor aligned with existing        *
 *  editors (e.g. cmdedit/medit).                                         *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jansson.h>

#include "../../strings.h"
#include "../../merc.h"
#include "../../olc.h"
#include "channel_filter.h"
#include "channel_service.h"
#include "channel_registry.h"
#include "channels_common.h"
#include "../../recycle.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef CHANNEL_DEF_DATA CEDIT_CHANNEL_DATA;

static void cedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void cedit_show_moderation_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void cedit_show_review_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void cedit_show_format_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static bool cedit_show(CHAR_DATA *ch, char *argument);
static bool cedit_create(CHAR_DATA *ch, char *argument);
static bool cedit_name(CHAR_DATA *ch, char *argument);
static bool cedit_scope(CHAR_DATA *ch, char *argument);
static bool cedit_topic(CHAR_DATA *ch, char *argument);
static bool cedit_flags(CHAR_DATA *ch, char *argument);
static bool cedit_modifiers(CHAR_DATA *ch, char *argument);
static bool cedit_modorder(CHAR_DATA *ch, char *argument);
static bool cedit_format(CHAR_DATA *ch, char *argument);
static bool cedit_history_config(CHAR_DATA *ch, char *argument);
static bool cedit_aliases(CHAR_DATA *ch, char *argument);
static bool cedit_filter(CHAR_DATA *ch, char *argument);

static void cedit_filter_trim_rule(char *text)
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
static bool cedit_review(CHAR_DATA *ch, char *argument);
static bool cedit_modadd(CHAR_DATA *ch, char *argument);
static bool cedit_moddel(CHAR_DATA *ch, char *argument);
static bool cedit_modlist(CHAR_DATA *ch, char *argument);
static bool cedit_punish(CHAR_DATA *ch, char *argument);
static bool cedit_comments(CHAR_DATA *ch, char *argument);
static bool cedit_requirements(CHAR_DATA *ch, char *argument);
static bool cedit_help(CHAR_DATA *ch, char *argument);
static bool cedit_summary(CHAR_DATA *ch, char *argument);

static bool cedit_persistent(CHAR_DATA *ch, char *argument);
static bool cedit_save(CHAR_DATA *ch, char *argument);
static const char *cedit_registry_file_path(char *buffer, size_t buffer_size);

static bool cedit_changed = false;

static CEDIT_CHANNEL_DATA cedit_channels[32];
static int cedit_channel_count = 0;

static const struct flag_type cedit_channel_flags[] = {
    { "allow_warn",      (A), true, NULL },
    { "allow_mute",      (B), true, NULL },
    { "allow_ban",       CHANNEL_FLAG_ALLOW_BAN, true, NULL },
    { "auto_filter",     (C), true, NULL },
    { "staff_review",    (D), true, NULL },
    { "mod_actions",     (E), true, NULL },
    { "honor_pers",      CHANNEL_FLAG_HONOR_PERS, true, NULL },
    { "honor_wizi",      CHANNEL_FLAG_HONOR_WIZI, true, NULL },
    { "is_ooc",          CHANNEL_FLAG_IS_OOC, true, NULL },
    { "respect_silence", CHANNEL_FLAG_RESPECT_SILENCE, true, NULL },
    { "ignore_quiet",    CHANNEL_FLAG_IGNORE_QUIET, true, NULL },
    { "guard_str_edit",  CHANNEL_FLAG_GUARD_STR_EDIT_CMDS, true, NULL },
    { NULL,               0, false, NULL }
};

static const char *cedit_registry_file_path(char *buffer, size_t buffer_size)
{
    return resolve_game_path(SYSTEM_DIR "channels.json", buffer, buffer_size);
}

static CEDIT_CHANNEL_DATA *cedit_find_channel(const char *id)
{
    int i;

    if (IS_NULLSTR(id))
        return NULL;

    for (i = 0; i < cedit_channel_count; i++) {
        if (!str_cmp(cedit_channels[i].id, id))
            return &cedit_channels[i];
    }

    return NULL;
}

static void cedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    (void)ch;
    (void)pEdit;
    cedit_changed = true;
}

const struct olc_cmd_type cedit_table[] = {
    { "?",        show_help },
    { "aliases",  cedit_aliases },
    { "commands", show_commands },
    { "comments", cedit_comments },
    { "create",   cedit_create },
    { "filter",   cedit_filter },
    { "flags",    cedit_flags },
    { "format",   cedit_format },
    { "history",  cedit_history_config },
    { "sethelp",  cedit_help },
    { "modadd",   cedit_modadd },
    { "moddel",   cedit_moddel },
    { "modifiers",cedit_modifiers },
    { "modorder", cedit_modorder },
    { "modlist",  cedit_modlist },
    { "name",       cedit_name },
    { "persistent", cedit_persistent },
    { "punish",     cedit_punish },
    { "review",     cedit_review },
    { "requirements", cedit_requirements },
    { "save",       cedit_save },
    { "scope",      cedit_scope },
    { "topic",      cedit_topic },
    { "show",       cedit_show },
    { "summary",    cedit_summary },
    { NULL,          0 }
};

static const OLC_EDITOR_DEF cedit_def = {
    .name            = "CEdit",
    .editor_type     = ED_CEDIT,
    .cmd_table       = cedit_table,
    .show_fn         = cedit_show,
    .tabs            = {
        .count = 4,
        .tabs = {
            { "General",    "Gen", cedit_show_general_tab },
            { "Moderation", "Mod", cedit_show_moderation_tab },
            { "Review",     "Rev", cedit_show_review_tab },
            { "Format",     "Fmt", cedit_show_format_tab },
        }
    },
    .theme           = &olc_theme_system,
    .perm            = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_IMPLEMENTOR
    },
    .change_mode     = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = cedit_mark_changed,
    .audit_changes   = false,
};

void cedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &cedit_def);
}

void do_cedit(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &cedit_def, NULL)) {
        send_to_char("CEdit: Insufficient security to edit channel definitions.\n\r", ch);
        return;
    }

    if (cedit_channel_count == 0) {
        /* Load working copies from the live channel registry. */
        int i, n = channel_registry_count();

        for (i = 0; i < n && cedit_channel_count < (int)elementsof(cedit_channels); i++) {
            const CHANNEL_DEF_DATA *def = channel_registry_get(i);
            CEDIT_CHANNEL_DATA     *cch = &cedit_channels[cedit_channel_count++];

            memset(cch, 0, sizeof(*cch));
            *cch = *def;
        }
    }

    if (arg1[0] == '\0') {
        send_to_char("Syntax: cedit <channel_id>\n\r", ch);
        send_to_char("        cedit create <channel_id>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        if (cedit_create(ch, argument))
            olc_editor_enter(ch, &cedit_def, ch->desc->pEdit, false);
        return;
    }

    channel = cedit_find_channel(arg1);
    if (!channel) {
        send_to_char("CEdit: No channel definition by that id.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &cedit_def, channel, true);
}

static bool cedit_show(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    (void)argument;

    if (!ch || !ch->desc || !ch->desc->pEdit)
        return false;

    channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "CEdit", channel->id, channel->name, &cedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        int i;
        for (i = 0; i < cedit_def.tabs.count; i++) {
            if (cedit_def.tabs.tabs[i].show_fn)
                cedit_def.tabs.tabs[i].show_fn(ch, ctx, (void *)channel);
        }
    } else if (tab >= 0 && tab < cedit_def.tabs.count && cedit_def.tabs.tabs[tab].show_fn) {
        cedit_def.tabs.tabs[tab].show_fn(ch, ctx, (void *)channel);
    } else {
        cedit_show_general_tab(ch, ctx, (void *)channel);
    }

    olc_display_section(ctx, theme, "Coders' Comments");
    olc_display_text(ctx, theme, NULL, "comments", channel->comments);

    olc_display_footer(ctx, theme);
    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static bool cedit_create(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: create <channel_id>\n\r", ch);
        return false;
    }

    if (cedit_channel_count >= (int)elementsof(cedit_channels)) {
        send_to_char("CEdit: channel definition capacity reached.\n\r", ch);
        return false;
    }

    if (cedit_find_channel(argument)) {
        send_to_char("CEdit: that channel id already exists.\n\r", ch);
        return false;
    }

    channel = &cedit_channels[cedit_channel_count++];
    memset(channel, 0, sizeof(*channel));
    strlcpy(channel->id,      argument, sizeof(channel->id));
    strlcpy(channel->name,    argument, sizeof(channel->name));
    strlcpy(channel->command, argument, sizeof(channel->command));
    channel->scope              = CHANNEL_SCOPE_GLOBAL;
    channel->allow_player_flags = true;
    channel->persistent         = false;
    channel->topic_pattern[0]   = '\0';
    channel->channel_flags      = 0;
    channel->filter_mode        = CHANNEL_FILTER_ALLOW;
    channel->light_warn_threshold = 2;
    channel->light_mute_minutes   = 15;
    channel->mod_count            = 0;
    strlcpy(channel->review_stream, "audit:filtered_messages", sizeof(channel->review_stream));
    channel->modifiers              = 0;
    channel->modifier_order[0]      = '\0';
    strlcpy(channel->fmt_self, "You: %2$s", sizeof(channel->fmt_self));
    strlcpy(channel->fmt_receiver, "%1$s: %2$s", sizeof(channel->fmt_receiver));
    channel->history_max_len        = 50;
    channel->history_max_age_seconds = 900;
    channel->comments[0]            = '\0';
    channel->help_keywords[0]       = '\0';
    channel->summary[0]             = '\0';

    /* Register immediately so channel_service_send can route to this id. */
    channel_registry_upsert(channel);

    ch->desc->pEdit = channel;
    send_to_char("Channel definition created.\n\r", ch);
    return true;
}

static bool cedit_name(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: name <display name>\n\r", ch);
        return false;
    }

    strlcpy(channel->name, argument, sizeof(channel->name));
    send_to_char("Name set.\n\r", ch);
    return true;
}

static bool cedit_scope(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    bool ok = false;
    CHANNEL_SCOPE scope;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: scope <global|area|region|room_wv|direct_entity|group_id|church_id|instance_id|dungeon_id>\n\r", ch);
        return false;
    }

    scope = channel_scope_from_name(argument, &ok);
    if (!ok) {
        send_to_char("CEdit: invalid scope value.\n\r", ch);
        return false;
    }

    channel->scope = scope;
    send_to_char("Scope set.\n\r", ch);
    return true;
}

static bool cedit_topic(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: topic <pattern|clear>\n\r", ch);
        send_to_char("  Example: topic rt:room:inst:$entity_id1:$entity_id2\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        channel->topic_pattern[0] = '\0';
        send_to_char("Topic pattern cleared (scope default routing will be used).\n\r", ch);
        return true;
    }

    strlcpy(channel->topic_pattern, argument, sizeof(channel->topic_pattern));
    send_to_char("Topic pattern updated.\n\r", ch);
    return true;
}

static bool cedit_aliases(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];
    char arg2[MSL];
    int i;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1) || !str_cmp(arg1, "list")) {
        BUFFER *output = new_buf();

        add_buf(output, "Channel aliases:\n\r");
        if (channel->alias_count < 1) {
            add_buf(output, "  (none)\n\r");
        } else {
            for (i = 0; i < channel->alias_count; i++)
                add_buf(output, formatf("  %2d) %s\n\r", i + 1, channel->aliases[i]));
        }

        page_to_char(buf_string(output), ch);
        free_buf(output);
        return false;
    }

    if (!str_cmp(arg1, "clear")) {
        channel->alias_count = 0;
        for (i = 0; i < CHANNEL_ALIAS_MAX; i++)
            channel->aliases[i][0] = '\0';

        send_to_char("Channel aliases cleared.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "add")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: aliases add <alias>\n\r", ch);
            return false;
        }

        if (!str_cmp(arg2, channel->id)
            || (!IS_NULLSTR(channel->command) && !str_cmp(arg2, channel->command))) {
            send_to_char("That alias duplicates the channel id/command.\n\r", ch);
            return false;
        }

        for (i = 0; i < channel->alias_count; i++) {
            if (!str_cmp(channel->aliases[i], arg2)) {
                send_to_char("That alias already exists.\n\r", ch);
                return false;
            }
        }

        if (channel->alias_count >= CHANNEL_ALIAS_MAX) {
            send_to_char("Alias list is full.\n\r", ch);
            return false;
        }

        strlcpy(channel->aliases[channel->alias_count], arg2, CHANNEL_ALIAS_LEN);
        channel->alias_count++;
        send_to_char("Alias added.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "del") || !str_cmp(arg1, "remove")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: aliases del <alias>\n\r", ch);
            return false;
        }

        for (i = 0; i < channel->alias_count; i++) {
            if (!str_cmp(channel->aliases[i], arg2)) {
                int j;
                for (j = i; j < channel->alias_count - 1; j++)
                    strlcpy(channel->aliases[j], channel->aliases[j + 1], CHANNEL_ALIAS_LEN);

                channel->alias_count--;
                channel->aliases[channel->alias_count][0] = '\0';
                send_to_char("Alias removed.\n\r", ch);
                return true;
            }
        }

        send_to_char("Alias not found.\n\r", ch);
        return false;
    }

    send_to_char("Syntax: aliases [list]\n\r", ch);
    send_to_char("        aliases add <alias>\n\r", ch);
    send_to_char("        aliases del <alias>\n\r", ch);
    send_to_char("        aliases clear\n\r", ch);
    return false;
}

static bool cedit_flags(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: flags <playerflags|chanflag> [on|off]\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "playerflags")) {
        if (!str_cmp(argument, "on"))
            channel->allow_player_flags = true;
        else if (!str_cmp(argument, "off"))
            channel->allow_player_flags = false;
        else
            channel->allow_player_flags = !channel->allow_player_flags;

        send_to_char(channel->allow_player_flags
            ? "Allow player flags is now ON.\n\r"
            : "Allow player flags is now OFF.\n\r", ch);
        return true;
    }

    return olc_cmd_flag_toggle(ch, arg1, "Channel Flags", "flags <chanflag>",
        &channel->channel_flags, cedit_channel_flags, channel, NULL);
}

static bool cedit_filter(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];
    char arg2[MSL];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: filter <on|off> [allow|redact|block|review]\n\r", ch);
        send_to_char("        filter simple <add|del|list> ...\n\r", ch);
        send_to_char("        filter regex <add|del|list> ...\n\r", ch);
        send_to_char("        filter clear <simple|regex|all>\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "simple")) {
        if (!str_cmp(arg2, "list")) {
            char copy[512];
            char *line;
            char *saveptr = NULL;
            int idx = 0;
            char out[MSL];

            send_to_char("Simple filter rules:\n\r", ch);
            if (IS_NULLSTR(channel->filter_simple)) {
                send_to_char("  (none)\n\r", ch);
                return true;
            }

            strlcpy(copy, channel->filter_simple, sizeof(copy));
            line = strtok_r(copy, "\n", &saveptr);
            while (line) {
                cedit_filter_trim_rule(line);
                if (!IS_NULLSTR(line)) {
                    snprintf(out, sizeof(out), "  %2d) %s\n\r", ++idx, line);
                    send_to_char(out, ch);
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }

            if (idx == 0)
                send_to_char("  (none)\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "add")) {
            char match[MIL];
            char rule[512];
            char updated[512];
            size_t required_len;

            argument = one_argument(argument, match);
            if (IS_NULLSTR(match)) {
                send_to_char("Syntax: filter simple add <match> [replacement]\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
                strlcpy(rule, match, sizeof(rule));
            else {
                required_len = strlen(match) + 2 + strlen(argument) + 1;
                if (required_len > sizeof(rule)) {
                    send_to_char("CEdit: filter rule is too long.\n\r", ch);
                    return false;
                }

                strlcpy(rule, match, sizeof(rule));
                strlcat(rule, "=>", sizeof(rule));
                strlcat(rule, argument, sizeof(rule));
            }

            updated[0] = '\0';
            if (!IS_NULLSTR(channel->filter_simple)) {
                strlcpy(updated, channel->filter_simple, sizeof(updated));
                strlcat(updated, "\n", sizeof(updated));
            }
            strlcat(updated, rule, sizeof(updated));

            if (strlen(updated) >= sizeof(channel->filter_simple)) {
                send_to_char("CEdit: filter rule set is too large.\n\r", ch);
                return false;
            }

            strlcpy(channel->filter_simple, updated, sizeof(channel->filter_simple));
            send_to_char("Simple filter rule added.\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "del")) {
            int del_idx;
            int idx = 0;
            bool removed = false;
            char copy[512];
            char rebuilt[512];
            char *line;
            char *saveptr = NULL;

            if (!is_number(argument) || (del_idx = atoi(argument)) <= 0) {
                send_to_char("Syntax: filter simple del <index>\n\r", ch);
                return false;
            }

            rebuilt[0] = '\0';
            strlcpy(copy, channel->filter_simple, sizeof(copy));
            line = strtok_r(copy, "\n", &saveptr);
            while (line) {
                cedit_filter_trim_rule(line);
                if (!IS_NULLSTR(line)) {
                    idx++;
                    if (idx == del_idx) {
                        removed = true;
                    } else {
                        if (rebuilt[0] != '\0')
                            strlcat(rebuilt, "\n", sizeof(rebuilt));
                        strlcat(rebuilt, line, sizeof(rebuilt));
                    }
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }

            if (!removed) {
                send_to_char("CEdit: no simple rule at that index.\n\r", ch);
                return false;
            }

            strlcpy(channel->filter_simple, rebuilt, sizeof(channel->filter_simple));
            send_to_char("Simple filter rule removed.\n\r", ch);
            return true;
        }

        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: filter simple <add|del|list> ...\n\r", ch);
            return false;
        }

        strlcpy(channel->filter_simple, argument, sizeof(channel->filter_simple));
        send_to_char("Simple filter rules replaced.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "regex")) {
        if (!str_cmp(arg2, "list")) {
            char copy[512];
            char *line;
            char *saveptr = NULL;
            int idx = 0;
            char out[MSL];

            send_to_char("Regex filter rules:\n\r", ch);
            if (IS_NULLSTR(channel->filter_regex)) {
                send_to_char("  (none)\n\r", ch);
                return true;
            }

            strlcpy(copy, channel->filter_regex, sizeof(copy));
            line = strtok_r(copy, "\n", &saveptr);
            while (line) {
                cedit_filter_trim_rule(line);
                if (!IS_NULLSTR(line)) {
                    snprintf(out, sizeof(out), "  %2d) %s\n\r", ++idx, line);
                    send_to_char(out, ch);
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }

            if (idx == 0)
                send_to_char("  (none)\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "add")) {
            char match[MIL];
            char rule[512];
            char updated[512];
            size_t required_len;

            argument = one_argument(argument, match);
            if (IS_NULLSTR(match)) {
                send_to_char("Syntax: filter regex add <pattern> [replacement]\n\r", ch);
                return false;
            }

            if (IS_NULLSTR(argument))
                strlcpy(rule, match, sizeof(rule));
            else {
                required_len = strlen(match) + 2 + strlen(argument) + 1;
                if (required_len > sizeof(rule)) {
                    send_to_char("CEdit: filter rule is too long.\n\r", ch);
                    return false;
                }

                strlcpy(rule, match, sizeof(rule));
                strlcat(rule, "=>", sizeof(rule));
                strlcat(rule, argument, sizeof(rule));
            }

            updated[0] = '\0';
            if (!IS_NULLSTR(channel->filter_regex)) {
                strlcpy(updated, channel->filter_regex, sizeof(updated));
                strlcat(updated, "\n", sizeof(updated));
            }
            strlcat(updated, rule, sizeof(updated));

            if (strlen(updated) >= sizeof(channel->filter_regex)) {
                send_to_char("CEdit: filter rule set is too large.\n\r", ch);
                return false;
            }

            strlcpy(channel->filter_regex, updated, sizeof(channel->filter_regex));
            send_to_char("Regex filter rule added.\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "del")) {
            int del_idx;
            int idx = 0;
            bool removed = false;
            char copy[512];
            char rebuilt[512];
            char *line;
            char *saveptr = NULL;

            if (!is_number(argument) || (del_idx = atoi(argument)) <= 0) {
                send_to_char("Syntax: filter regex del <index>\n\r", ch);
                return false;
            }

            rebuilt[0] = '\0';
            strlcpy(copy, channel->filter_regex, sizeof(copy));
            line = strtok_r(copy, "\n", &saveptr);
            while (line) {
                cedit_filter_trim_rule(line);
                if (!IS_NULLSTR(line)) {
                    idx++;
                    if (idx == del_idx) {
                        removed = true;
                    } else {
                        if (rebuilt[0] != '\0')
                            strlcat(rebuilt, "\n", sizeof(rebuilt));
                        strlcat(rebuilt, line, sizeof(rebuilt));
                    }
                }
                line = strtok_r(NULL, "\n", &saveptr);
            }

            if (!removed) {
                send_to_char("CEdit: no regex rule at that index.\n\r", ch);
                return false;
            }

            strlcpy(channel->filter_regex, rebuilt, sizeof(channel->filter_regex));
            send_to_char("Regex filter rule removed.\n\r", ch);
            return true;
        }

        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: filter regex <add|del|list> ...\n\r", ch);
            return false;
        }

        strlcpy(channel->filter_regex, argument, sizeof(channel->filter_regex));
        send_to_char("Regex filter rules replaced.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "clear")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: filter clear <simple|regex|all>\n\r", ch);
            return false;
        }

        if (!str_cmp(arg2, "simple") || !str_cmp(arg2, "all"))
            channel->filter_simple[0] = '\0';

        if (!str_cmp(arg2, "regex") || !str_cmp(arg2, "all"))
            channel->filter_regex[0] = '\0';

        if (str_cmp(arg2, "simple") && str_cmp(arg2, "regex") && str_cmp(arg2, "all")) {
            send_to_char("CEdit: expected simple, regex, or all.\n\r", ch);
            return false;
        }

        send_to_char("Filter pattern settings cleared.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "on")) channel->filter_enabled = true;
    else if (!str_cmp(arg1, "off")) channel->filter_enabled = false;
    else {
        send_to_char("CEdit: filter state must be on or off.\n\r", ch);
        return false;
    }

    if (arg2[0] != '\0') {
        bool ok = false;
        channel->filter_mode = channel_filter_mode_from_name(arg2, &ok);
        if (!ok) {
            send_to_char("CEdit: invalid filter mode.\n\r", ch);
            return false;
        }
    }

    send_to_char("Filter settings updated.\n\r", ch);
    return true;
}

static bool cedit_review(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: review <on|off|stream> [stream_name]\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "on")) {
        channel->review_enabled = true;
        send_to_char("Review queue enabled.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "off")) {
        channel->review_enabled = false;
        send_to_char("Review queue disabled.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "stream")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: review stream <stream_name>\n\r", ch);
            return false;
        }

        strlcpy(channel->review_stream, argument, sizeof(channel->review_stream));
        send_to_char("Review stream updated.\n\r", ch);
        return true;
    }

    send_to_char("CEdit: expected on/off/stream.\n\r", ch);
    return false;
}

static bool cedit_modadd(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    int i;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: modadd <player_name>\n\r", ch);
        return false;
    }

    if (channel->mod_count >= (int)elementsof(channel->moderators)) {
        send_to_char("CEdit: moderator list is full.\n\r", ch);
        return false;
    }

    for (i = 0; i < channel->mod_count; i++) {
        if (!str_cmp(channel->moderators[i], argument)) {
            send_to_char("CEdit: that moderator already exists.\n\r", ch);
            return false;
        }
    }

    strlcpy(channel->moderators[channel->mod_count], argument,
        sizeof(channel->moderators[channel->mod_count]));
    channel->mod_count++;
    send_to_char("Moderator added.\n\r", ch);
    return true;
}

static bool cedit_moddel(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    int i;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: moddel <player_name>\n\r", ch);
        return false;
    }

    for (i = 0; i < channel->mod_count; i++) {
        if (!str_cmp(channel->moderators[i], argument)) {
            for (; i < channel->mod_count - 1; i++)
                strlcpy(channel->moderators[i], channel->moderators[i + 1], sizeof(channel->moderators[i]));

            channel->mod_count--;
            channel->moderators[channel->mod_count][0] = '\0';
            send_to_char("Moderator removed.\n\r", ch);
            return true;
        }
    }

    send_to_char("CEdit: moderator not found.\n\r", ch);
    return false;
}

static bool cedit_modlist(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    BUFFER *output;
    int i;

    (void)argument;

    output = new_buf();
    add_buf(output, "Moderators:\n\r");

    if (channel->mod_count == 0) {
        add_buf(output, "  (none)\n\r");
    } else {
        for (i = 0; i < channel->mod_count; i++)
            add_buf(output, formatf("  %2d) %s\n\r", i + 1, channel->moderators[i]));
    }

    page_to_char(buf_string(output), ch);
    free_buf(output);
    return false;
}

static bool cedit_punish(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: punish <warn_threshold|mute_minutes> <value>\n\r", ch);
        return false;
    }

    if (IS_NULLSTR(argument) || !is_number(argument)) {
        send_to_char("CEdit: numeric value required.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "warn_threshold")) {
        int value = atoi(argument);
        if (value < 0 || value > 10) {
            send_to_char("CEdit: warn_threshold must be 0-10.\n\r", ch);
            return false;
        }
        channel->light_warn_threshold = value;
        send_to_char("Warn threshold updated.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "mute_minutes")) {
        int value = atoi(argument);
        if (value < 0 || value > 1440) {
            send_to_char("CEdit: mute_minutes must be 0-1440.\n\r", ch);
            return false;
        }
        channel->light_mute_minutes = value;
        send_to_char("Mute duration updated.\n\r", ch);
        return true;
    }

    send_to_char("CEdit: expected warn_threshold or mute_minutes.\n\r", ch);
    return false;
}

static bool cedit_comments(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: comments <text|clear>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        channel->comments[0] = '\0';
        send_to_char("Comments cleared.\n\r", ch);
        return true;
    }

    strlcpy(channel->comments, argument, sizeof(channel->comments));
    send_to_char("Comments updated.\n\r", ch);
    return true;
}

static bool cedit_help(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    HELP_DATA *pHelp;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: sethelp <keywords|#index|clear>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        channel->help_keywords[0] = '\0';
        send_to_char("Channel help keywords cleared.\n\r", ch);
        return true;
    }

    if (argument[0] == '#') {
        int index = atoi(argument + 1);
        if (index < 0 || index > 32000) {
            send_to_char("That help index is out of range.\n\r", ch);
            return false;
        }

        pHelp = lookup_help_index(index, get_staff_rank(ch), topHelpCat);
        if (pHelp == NULL) {
            act("There is no helpfile with index $t.", ch, NULL, NULL, NULL, NULL, argument + 1, NULL, TO_CHAR, NULL, NULL);
            return false;
        }
    } else {
        pHelp = lookup_help_exact(argument, get_staff_rank(ch), topHelpCat);
        if (pHelp == NULL) {
            act("There is no helpfile with keywords $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
            return false;
        }
    }

    strlcpy(channel->help_keywords, pHelp->keyword, sizeof(channel->help_keywords));
    send_to_char(formatf("Channel help keywords set to %s.\n\r", pHelp->keyword), ch);
    return true;
}

static bool cedit_summary(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: summary <text|clear>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        channel->summary[0] = '\0';
        send_to_char("Channel summary cleared.\n\r", ch);
        return true;
    }

    strlcpy(channel->summary, argument, sizeof(channel->summary));
    send_to_char("Channel summary set.\n\r", ch);
    return true;
}

static bool cedit_requirements(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];
    char arg2[MSL];
    char *original_argument = argument;
    char *target = NULL;
    size_t target_sz = 0;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: requirements publish <json|clear>\n\r", ch);
        send_to_char("        requirements subscribe <json|clear>\n\r", ch);
        send_to_char("        requirements clear <publish|subscribe|all>\n\r", ch);
        send_to_char("        requirements show\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "show")) {
        BUFFER *output = new_buf();

        add_buf(output, "Requirement Specs:\n\r");
        add_buf(output, formatf("  Publish  : %s\n\r",
                                channel->publish_requirements[0] ? channel->publish_requirements : "(none)"));
        add_buf(output, formatf("  Subscribe: %s\n\r",
                                channel->subscribe_requirements[0] ? channel->subscribe_requirements : "(none)"));

        page_to_char(buf_string(output), ch);
        free_buf(output);
        return false;
    }

    if (!str_cmp(arg1, "clear")) {
        if (IS_NULLSTR(arg2)) {
            send_to_char("Syntax: requirements clear <publish|subscribe|all>\n\r", ch);
            return false;
        }

        if (!str_cmp(arg2, "publish")) {
            channel->publish_requirements[0] = '\0';
            send_to_char("Publish requirements cleared.\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "subscribe")) {
            channel->subscribe_requirements[0] = '\0';
            send_to_char("Subscribe requirements cleared.\n\r", ch);
            return true;
        }

        if (!str_cmp(arg2, "all")) {
            channel->publish_requirements[0] = '\0';
            channel->subscribe_requirements[0] = '\0';
            send_to_char("Publish and subscribe requirements cleared.\n\r", ch);
            return true;
        }

        send_to_char("CEdit: expected publish, subscribe, or all.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "publish")) {
        target = channel->publish_requirements;
        target_sz = sizeof(channel->publish_requirements);
    } else if (!str_cmp(arg1, "subscribe")) {
        target = channel->subscribe_requirements;
        target_sz = sizeof(channel->subscribe_requirements);
    } else {
        send_to_char("CEdit: expected publish, subscribe, clear, or show.\n\r", ch);
        return false;
    }

    if (IS_NULLSTR(arg2) && IS_NULLSTR(argument)) {
        send_to_char("CEdit: JSON requirement spec required (or 'clear').\n\r", ch);
        return false;
    }

    if (!str_cmp(arg2, "clear") && IS_NULLSTR(argument)) {
        target[0] = '\0';
        send_to_char("Requirements cleared.\n\r", ch);
        return true;
    }

    {
        const char *spec = original_argument;
        json_t *parsed;
        json_error_t err;

        while (*spec == ' ')
            spec++;
        spec = one_argument((char *)spec, arg1);
        spec = one_argument((char *)spec, arg2);

        while (*spec == ' ')
            spec++;

        parsed = json_loads(spec, 0, &err);
        if (!parsed) {
            send_to_char(formatf("CEdit: invalid JSON at line %d: %s\n\r", err.line, err.text), ch);
            return false;
        }

        if (!json_is_object(parsed)) {
            json_decref(parsed);
            send_to_char("CEdit: requirement spec must be a JSON object.\n\r", ch);
            return false;
        }

        json_decref(parsed);
        strlcpy(target, spec, target_sz);
    }

    send_to_char("Requirements updated.\n\r", ch);
    return true;
}

static bool cedit_persistent(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;


    if (!IS_NULLSTR(argument)) {
        if (!str_cmp(argument, "on"))       channel->persistent = true;
        else if (!str_cmp(argument, "off")) channel->persistent = false;
        else {
            send_to_char("Syntax: persistent <on|off>\n\r", ch);
            return false;
        }
    } else {
        channel->persistent = !channel->persistent;
    }

    send_to_char(channel->persistent
        ? "Channel is now PERSISTENT (always subscribed).\n\r"
        : "Channel is now TRANSIENT (context-driven subscription).\n\r", ch);
    return true;
}

static bool cedit_save(CHAR_DATA *ch, char *argument)
{
    int i;
    char registry_path[MAX_INPUT_LENGTH];
    const char *resolved_registry;

    (void)argument;

    /* Push all working copies back into the live registry. */
    for (i = 0; i < cedit_channel_count; i++) {
        CEDIT_CHANNEL_DATA *cch = &cedit_channels[i];
        if (cch->command[0] == '\0')
            strlcpy(cch->command, cch->id, sizeof(cch->command));

        if (!channel_registry_upsert(cch)) {
            send_to_char(formatf("CEdit: registry full — could not save '%s'.\n\r",
                                 cch->id), ch);
        }
    }

    resolved_registry = cedit_registry_file_path(registry_path, sizeof(registry_path));
    if (channel_registry_save(resolved_registry))
        send_to_char(formatf("Channel definitions saved to %s.\n\r", resolved_registry), ch);
    else
        send_to_char("CEdit: save failed — check server logs.\n\r", ch);

    cedit_changed = false;
    return false;
}

static bool cedit_modifiers(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: modifiers <punctuation_parse|emote_strip|color_strip|caps_normalize|drunk_speech>\n\r", ch);
        return false;
    }

    return olc_cmd_flag_toggle(ch, argument, "Modifiers", "modifiers <modifier>",
        &channel->modifiers, channel_modifier_flag_table(), channel, NULL);
}

static bool cedit_modorder(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char work[256];
    char normalized[128];
    long seen = 0;
    char *token;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: modorder <comma-separated modifier names|clear>\n\r", ch);
        send_to_char("  Example: modorder emote_strip,color_strip,caps_normalize,drunk_speech\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        channel->modifier_order[0] = '\0';
        send_to_char("Modifier order cleared (runtime fallback order will be used).\n\r", ch);
        return true;
    }

    work[0] = '\0';
    normalized[0] = '\0';
    strlcpy(work, argument, sizeof(work));

    token = strtok(work, ", ");
    while (token) {
        long bit = channel_modifier_flag_from_name(token);

        if (bit == 0) {
            send_to_char(formatf("CEdit: unknown modifier in order: %s\n\r", token), ch);
            return false;
        }

        if ((seen & bit) != 0) {
            send_to_char(formatf("CEdit: duplicate modifier in order: %s\n\r", token), ch);
            return false;
        }

        seen |= bit;

        if (normalized[0] != '\0')
            strlcat(normalized, ",", sizeof(normalized));
        strlcat(normalized, token, sizeof(normalized));

        token = strtok(NULL, ", ");
    }

    if (normalized[0] == '\0') {
        send_to_char("CEdit: modorder requires at least one modifier name.\n\r", ch);
        return false;
    }

    strlcpy(channel->modifier_order, normalized, sizeof(channel->modifier_order));
    send_to_char("Modifier order updated.\n\r", ch);
    return true;
}

static bool cedit_format(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: format self <format_string>\n\r", ch);
        send_to_char("        format receiver <format_string>\n\r", ch);
        send_to_char("        format notvict <format_string>\n\r", ch);
        send_to_char("        format clear <self|receiver|notvict>\n\r", ch);
        send_to_char("  Placeholders: %%1$s = sender, %%2$s = receiver/target, %%3$s = message\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "clear")) {
        char arg2[MSL];
        one_argument(argument, arg2);

        if (!str_cmp(arg2, "self")) {
            channel->fmt_self[0] = '\0';
            send_to_char("Self format cleared (will use system default).\n\r", ch);
            return true;
        }
        if (!str_cmp(arg2, "receiver")) {
            channel->fmt_receiver[0] = '\0';
            send_to_char("Receiver format cleared (will use system default).\n\r", ch);
            return true;
        }
        if (!str_cmp(arg2, "notvict")) {
            channel->fmt_notvict[0] = '\0';
            send_to_char("Notvict format cleared (will use legacy fallback).\n\r", ch);
            return true;
        }
        send_to_char("CEdit: expected 'self', 'receiver', or 'notvict' after clear.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "self")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: format self <format_string>\n\r", ch);
            return false;
        }
        strlcpy(channel->fmt_self, argument, sizeof(channel->fmt_self));
        send_to_char("Self format string set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "receiver")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: format receiver <format_string>\n\r", ch);
            return false;
        }
        strlcpy(channel->fmt_receiver, argument, sizeof(channel->fmt_receiver));
        send_to_char("Receiver format string set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "notvict")) {
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: format notvict <format_string>\n\r", ch);
            return false;
        }
        strlcpy(channel->fmt_notvict, argument, sizeof(channel->fmt_notvict));
        send_to_char("Notvict format string set.\n\r", ch);
        return true;
    }

    send_to_char("CEdit: expected self, receiver, notvict, or clear.\n\r", ch);
    return false;
}

static bool cedit_history_config(CHAR_DATA *ch, char *argument)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)ch->desc->pEdit;
    char arg1[MSL];

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: history max_len <count>    (0 = unlimited)\n\r", ch);
        send_to_char("        history max_age <seconds>  (0 = unlimited)\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "max_len")) {
        int value;

        if (IS_NULLSTR(argument) || !is_number(argument)) {
            send_to_char("CEdit: numeric value required.\n\r", ch);
            return false;
        }

        value = atoi(argument);
        if (value < 0 || value > 10000) {
            send_to_char("CEdit: max_len must be 0-10000.\n\r", ch);
            return false;
        }

        channel->history_max_len = value;
        send_to_char(value == 0
            ? "History max_len set to unlimited.\n\r"
            : "History max_len updated.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "max_age")) {
        int value;

        if (IS_NULLSTR(argument) || !is_number(argument)) {
            send_to_char("CEdit: numeric value required.\n\r", ch);
            return false;
        }

        value = atoi(argument);
        if (value < 0 || value > 604800) {
            send_to_char("CEdit: max_age must be 0-604800 (0=unlimited, max=7 days).\n\r", ch);
            return false;
        }

        channel->history_max_age_seconds = value;
        send_to_char(value == 0
            ? "History max_age set to unlimited.\n\r"
            : "History max_age updated.\n\r", ch);
        return true;
    }

    send_to_char("CEdit: expected max_len or max_age.\n\r", ch);
    return false;
}

static void cedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);

    (void)ch;

    olc_display_string(ctx, theme, "Id:", NULL, channel->id);
    olc_display_string(ctx, theme, "Name:", "name", channel->name);
    olc_display_string(ctx, theme, "Command:", NULL, channel->command[0] ? channel->command : channel->id);
    if (channel->alias_count > 0) {
        char aliases_buf[256];

        aliases_buf[0] = '\0';
        for (int i = 0; i < channel->alias_count; i++) {
            if (aliases_buf[0] != '\0')
                strlcat(aliases_buf, ", ", sizeof(aliases_buf));
            strlcat(aliases_buf, channel->aliases[i], sizeof(aliases_buf));
        }
        olc_display_string(ctx, theme, "Aliases:", "aliases", aliases_buf);
    } else {
        olc_display_string(ctx, theme, "Aliases:", "aliases", "(none)");
    }
    olc_display_string(ctx, theme, "Scope:", "scope", channel_scope_to_display_name(channel->scope));
    olc_display_string(ctx, theme, "Topic Pattern:", "topic",
        channel->topic_pattern[0] ? channel->topic_pattern : "(scope default)");
    olc_display_bool(ctx, theme, "Persistent:", "persistent", channel->persistent);
    olc_display_infof(ctx, theme, "persistent on = always subscribed; off = subscribe only while scope has active entities");
    olc_display_bool(ctx, theme, "Allow Player Flags:", "flags playerflags", channel->allow_player_flags);
    olc_display_flags(ctx, theme, "Channel Flags:", "flags", cedit_channel_flags, channel->channel_flags);
    olc_display_flags(ctx, theme, "Modifiers:", "modifiers", channel_modifier_flag_table(), channel->modifiers);
    olc_display_string(ctx, theme, "Modifier Order:", "modorder",
        channel->modifier_order[0] ? channel->modifier_order : "(runtime default)");
    olc_display_string(ctx, theme, "Publish Req:", "requirements publish",
        channel->publish_requirements[0] ? channel->publish_requirements : "(none)");
    olc_display_string(ctx, theme, "Subscribe Req:", "requirements subscribe",
        channel->subscribe_requirements[0] ? channel->subscribe_requirements : "(none)");
    if (channel->help_keywords[0]
        && lookup_help_exact(channel->help_keywords, get_staff_rank(ch), topHelpCat) != NULL) {
        HELP_DATA *pHelp = lookup_help_exact(channel->help_keywords, get_staff_rank(ch), topHelpCat);
        olc_display_string(ctx, theme, "Help:", "sethelp",
            formatf("'\t<send href=\"help #%d\">{W%s{x\t</send>' ({W#%d{x)",
                pHelp->index, channel->help_keywords, pHelp->index));
    } else {
        olc_display_string(ctx, theme, "Help:", "sethelp",
            channel->help_keywords[0] ? formatf("{R%s{x", channel->help_keywords) : "(none set)");
    }
    olc_display_string(ctx, theme, "Summary:", "summary",
        channel->summary[0] ? channel->summary : "(none)");
}

static void cedit_show_moderation_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);
    int i;

    (void)ch;

    olc_display_bool(ctx, theme, "Filter Enabled:", "filter", channel->filter_enabled);
    olc_display_string(ctx, theme, "Filter Mode:", "filter", channel_filter_mode_to_name(channel->filter_mode));
    olc_display_string(ctx, theme, "Simple Match:", "filter simple",
        channel->filter_simple[0] ? channel->filter_simple : "(none)");
    olc_display_string(ctx, theme, "Regex Match:", "filter regex",
        channel->filter_regex[0] ? channel->filter_regex : "(none)");
    olc_display_number(ctx, theme, "Warn Threshold:", "punish warn_threshold", channel->light_warn_threshold);
    olc_display_number(ctx, theme, "Mute Minutes:", "punish mute_minutes", channel->light_mute_minutes);

    olc_display_section(ctx, theme, "Moderators");
    if (channel->mod_count == 0)
        olc_display_string(ctx, theme, "List:", "modadd", "(none)");
    else {
        for (i = 0; i < channel->mod_count; i++)
            olc_display_string(ctx, theme, formatf("%2d)", i + 1), NULL, channel->moderators[i]);
    }
    olc_display_infof(ctx, theme, "Use: modadd <name>, moddel <name>, modlist");
}

static void cedit_show_review_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);

    (void)ch;

    olc_display_bool(ctx, theme, "Review Enabled:", "review", channel->review_enabled);
    olc_display_string(ctx, theme, "Review Stream:", "review stream", channel->review_stream);
    olc_display_infof(ctx, theme, "Staff review queue stream for flagged content.");
}

static void cedit_show_format_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);

    (void)ch;

    olc_display_section(ctx, theme, "Format Strings");
    olc_display_string(ctx, theme, "Self:",
        "format self",
        channel->fmt_self[0] ? channel->fmt_self : "(system default)");
    olc_display_string(ctx, theme, "Receiver:",
        "format receiver",
        channel->fmt_receiver[0] ? channel->fmt_receiver : "(system default)");
    olc_display_string(ctx, theme, "Notvict:",
        "format notvict",
        channel->fmt_notvict[0] ? channel->fmt_notvict : "(legacy fallback)");
    olc_display_infof(ctx, theme, "Placeholders: %%1$s = sender, %%2$s = receiver/target, %%3$s = message");
    olc_display_infof(ctx, theme, "Use: format self|receiver|notvict <string>  or  format clear self|receiver|notvict");

    olc_display_section(ctx, theme, "History Retention");
    if (channel->history_max_len == 0)
        olc_display_string(ctx, theme, "Max Messages:", "history max_len", "unlimited");
    else
        olc_display_number(ctx, theme, "Max Messages:", "history max_len", channel->history_max_len);
    if (channel->history_max_age_seconds == 0)
        olc_display_string(ctx, theme, "Max Age:", "history max_age", "unlimited");
    else
        olc_display_number(ctx, theme, "Max Age (sec):", "history max_age", channel->history_max_age_seconds);
    olc_display_infof(ctx, theme, "Use: history max_len <n>  or  history max_age <seconds>  (0 = unlimited)");
}
