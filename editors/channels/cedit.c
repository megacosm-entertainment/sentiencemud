/***************************************************************************
 *  cedit.c — OLC Channel Editor scaffold                                 *
 *                                                                         *
 *  Framework-based channel definition editor aligned with existing        *
 *  editors (e.g. cmdedit/medit).                                         *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../strings.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../channel_service.h"
#include "../../recycle.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

typedef enum cedit_filter_mode {
    CEDIT_FILTER_ALLOW = 0,
    CEDIT_FILTER_REDACT,
    CEDIT_FILTER_BLOCK,
    CEDIT_FILTER_REVIEW
} CEDIT_FILTER_MODE;

typedef struct cedit_channel_data {
    char id[32];
    char name[64];
    CHANNEL_SCOPE scope;
    bool allow_player_flags;
    long channel_flags;

    bool filter_enabled;
    CEDIT_FILTER_MODE filter_mode;

    int light_warn_threshold;
    int light_mute_minutes;

    int mod_count;
    char moderators[8][32];

    bool review_enabled;
    char review_stream[64];
    char *comments;
} CEDIT_CHANNEL_DATA;

static void cedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void cedit_show_moderation_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void cedit_show_review_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static bool cedit_show(CHAR_DATA *ch, char *argument);
static bool cedit_create(CHAR_DATA *ch, char *argument);
static bool cedit_name(CHAR_DATA *ch, char *argument);
static bool cedit_scope(CHAR_DATA *ch, char *argument);
static bool cedit_flags(CHAR_DATA *ch, char *argument);
static bool cedit_filter(CHAR_DATA *ch, char *argument);
static bool cedit_review(CHAR_DATA *ch, char *argument);
static bool cedit_modadd(CHAR_DATA *ch, char *argument);
static bool cedit_moddel(CHAR_DATA *ch, char *argument);
static bool cedit_modlist(CHAR_DATA *ch, char *argument);
static bool cedit_punish(CHAR_DATA *ch, char *argument);
static bool cedit_comments(CHAR_DATA *ch, char *argument);

static bool cedit_changed = false;

static CEDIT_CHANNEL_DATA cedit_channels[32];
static int cedit_channel_count = 0;

static const struct flag_type cedit_channel_flags[] = {
    { "allow_warn",      (A), true, NULL },
    { "allow_mute",      (B), true, NULL },
    { "auto_filter",     (C), true, NULL },
    { "staff_review",    (D), true, NULL },
    { "mod_actions",     (E), true, NULL },
    { NULL,               0, false, NULL }
};

static const char *cedit_filter_mode_name(CEDIT_FILTER_MODE mode)
{
    switch (mode) {
    case CEDIT_FILTER_REDACT: return "redact";
    case CEDIT_FILTER_BLOCK: return "block";
    case CEDIT_FILTER_REVIEW: return "review";
    case CEDIT_FILTER_ALLOW:
    default:
        return "allow";
    }
}

static CHANNEL_SCOPE cedit_parse_scope(const char *text, bool *ok)
{
    if (ok) *ok = true;

    if (IS_NULLSTR(text)) {
        if (ok) *ok = false;
        return CHANNEL_SCOPE_GLOBAL;
    }

    if (!str_cmp(text, "global")) return CHANNEL_SCOPE_GLOBAL;
    if (!str_cmp(text, "area")) return CHANNEL_SCOPE_AREA;
    if (!str_cmp(text, "region")) return CHANNEL_SCOPE_REGION;
    if (!str_cmp(text, "room_wv")) return CHANNEL_SCOPE_ROOM_WV;
    if (!str_cmp(text, "direct_entity")) return CHANNEL_SCOPE_DIRECT_ENTITY;
    if (!str_cmp(text, "group_id")) return CHANNEL_SCOPE_GROUP_ID;
    if (!str_cmp(text, "church_id")) return CHANNEL_SCOPE_CHURCH_ID;

    if (ok) *ok = false;
    return CHANNEL_SCOPE_GLOBAL;
}

static const char *cedit_scope_name(CHANNEL_SCOPE scope)
{
    switch (scope) {
    case CHANNEL_SCOPE_AREA: return "AREA";
    case CHANNEL_SCOPE_REGION: return "REGION";
    case CHANNEL_SCOPE_ROOM_WV: return "ROOM_WV";
    case CHANNEL_SCOPE_DIRECT_ENTITY: return "DIRECT_ENTITY";
    case CHANNEL_SCOPE_GROUP_ID: return "GROUP_ID";
    case CHANNEL_SCOPE_CHURCH_ID: return "CHURCH_ID";
    case CHANNEL_SCOPE_GLOBAL:
    default:
        return "GLOBAL";
    }
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
    { "commands", show_commands },
    { "comments", cedit_comments },
    { "create",   cedit_create },
    { "filter",   cedit_filter },
    { "flags",    cedit_flags },
    { "modadd",   cedit_modadd },
    { "moddel",   cedit_moddel },
    { "modlist",  cedit_modlist },
    { "name",     cedit_name },
    { "punish",   cedit_punish },
    { "review",   cedit_review },
    { "scope",    cedit_scope },
    { "show",     cedit_show },
    { NULL,        0 }
};

static const OLC_EDITOR_DEF cedit_def = {
    .name            = "CEdit",
    .editor_type     = ED_CEDIT,
    .cmd_table       = cedit_table,
    .show_fn         = cedit_show,
    .tabs            = {
        .count = 3,
        .tabs = {
            { "General",    "Gen", cedit_show_general_tab },
            { "Moderation", "Mod", cedit_show_moderation_tab },
            { "Review",     "Rev", cedit_show_review_tab },
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
        CEDIT_CHANNEL_DATA seed[] = {
            { "gossip", "Gossip", CHANNEL_SCOPE_GLOBAL, true, 0, false, CEDIT_FILTER_ALLOW, 2, 15, 0, {{0}}, false, "audit:filtered_messages", NULL },
            { "ooc", "OOC", CHANNEL_SCOPE_GLOBAL, true, 0, false, CEDIT_FILTER_ALLOW, 2, 15, 0, {{0}}, false, "audit:filtered_messages", NULL },
            { "yell", "Yell", CHANNEL_SCOPE_AREA, true, 0, false, CEDIT_FILTER_ALLOW, 2, 15, 0, {{0}}, false, "audit:filtered_messages", NULL },
            { "gtell", "Group Tell", CHANNEL_SCOPE_GROUP_ID, true, 0, false, CEDIT_FILTER_ALLOW, 2, 15, 0, {{0}}, false, "audit:filtered_messages", NULL },
            { "chtalk", "Church Talk", CHANNEL_SCOPE_CHURCH_ID, true, 0, false, CEDIT_FILTER_ALLOW, 2, 15, 0, {{0}}, false, "audit:filtered_messages", NULL },
        };
        int i;

        for (i = 0; i < (int)elementsof(seed); i++) {
            cedit_channels[cedit_channel_count] = seed[i];
            cedit_channels[cedit_channel_count].comments = str_dup("");
            cedit_channel_count++;
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
    strlcpy(channel->id, argument, sizeof(channel->id));
    strlcpy(channel->name, argument, sizeof(channel->name));
    channel->scope = CHANNEL_SCOPE_GLOBAL;
    channel->allow_player_flags = true;
    channel->channel_flags = 0;
    channel->filter_mode = CEDIT_FILTER_ALLOW;
    channel->light_warn_threshold = 2;
    channel->light_mute_minutes = 15;
    channel->mod_count = 0;
    strlcpy(channel->review_stream, "audit:filtered_messages", sizeof(channel->review_stream));
    channel->comments = str_dup("");

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
        send_to_char("Syntax: scope <global|area|region|room_wv|direct_entity|group_id|church_id>\n\r", ch);
        return false;
    }

    scope = cedit_parse_scope(argument, &ok);
    if (!ok) {
        send_to_char("CEdit: invalid scope value.\n\r", ch);
        return false;
    }

    channel->scope = scope;
    send_to_char("Scope set.\n\r", ch);
    return true;
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
        return false;
    }

    if (!str_cmp(arg1, "on")) channel->filter_enabled = true;
    else if (!str_cmp(arg1, "off")) channel->filter_enabled = false;
    else {
        send_to_char("CEdit: filter state must be on or off.\n\r", ch);
        return false;
    }

    if (arg2[0] != '\0') {
        if (!str_cmp(arg2, "allow")) channel->filter_mode = CEDIT_FILTER_ALLOW;
        else if (!str_cmp(arg2, "redact")) channel->filter_mode = CEDIT_FILTER_REDACT;
        else if (!str_cmp(arg2, "block")) channel->filter_mode = CEDIT_FILTER_BLOCK;
        else if (!str_cmp(arg2, "review")) channel->filter_mode = CEDIT_FILTER_REVIEW;
        else {
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

    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &channel->comments, NULL, NULL);
}

static void cedit_show_general_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);

    (void)ch;

    olc_display_string(ctx, theme, "Id:", NULL, channel->id);
    olc_display_string(ctx, theme, "Name:", "name", channel->name);
    olc_display_string(ctx, theme, "Scope:", "scope", cedit_scope_name(channel->scope));
    olc_display_bool(ctx, theme, "Allow Player Flags:", "flags playerflags", channel->allow_player_flags);
    olc_display_flags(ctx, theme, "Channel Flags:", "flags", cedit_channel_flags, channel->channel_flags);
}

static void cedit_show_moderation_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    CEDIT_CHANNEL_DATA *channel = (CEDIT_CHANNEL_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&cedit_def);
    int i;

    (void)ch;

    olc_display_bool(ctx, theme, "Filter Enabled:", "filter", channel->filter_enabled);
    olc_display_string(ctx, theme, "Filter Mode:", "filter", cedit_filter_mode_name(channel->filter_mode));
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
