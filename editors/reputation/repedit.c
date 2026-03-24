#include <stdio.h>
#include <string.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

REPEDIT(repedit_list);
REPEDIT(repedit_show);
REPEDIT(repedit_create);
REPEDIT(repedit_name);
REPEDIT(repedit_description);
REPEDIT(repedit_comments);
REPEDIT(repedit_flags);
REPEDIT(repedit_initial);
REPEDIT(repedit_token);
REPEDIT(repedit_rank);

static REPUTATION_INDEX_DATA *repedit_current(CHAR_DATA *ch);
static bool repedit_mark_changed(REPUTATION_INDEX_DATA *rep);
static void repedit_reorder_ranks(REPUTATION_INDEX_DATA *rep);
static REPUTATION_INDEX_RANK_DATA *repedit_new_rank(const char *name);
static void repedit_show_basic_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void repedit_show_ranks_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static const struct flag_type reputation_flags_local[] = {
    { "hidden", REPUTATION_HIDDEN, true, NULL },
    { "peaceful", REPUTATION_PEACEFUL, true, NULL },
    { "on_update", REPUTATION_ON_UPDATE, true, NULL },
    { "on_encounter", REPUTATION_ON_ENCOUNTER, true, NULL },
    { NULL, 0, false, NULL }
};

static const struct flag_type reputation_rank_flags_local[] = {
    { "norankup", REPUTATION_RANK_NORANKUP, true, NULL },
    { "paragon", REPUTATION_RANK_PARAGON, true, NULL },
    { "reset_paragon", REPUTATION_RANK_RESET_PARAGON, true, NULL },
    { "peaceful", REPUTATION_RANK_PEACEFUL, true, NULL },
    { "hostile", REPUTATION_RANK_HOSTILE, true, NULL },
    { NULL, 0, false, NULL }
};

typedef struct color_name_type {
    const char *name;
    char color;
} COLOR_NAME;

static const COLOR_NAME color_name_table[] = {
    { "blue", 'B' },
    { "cyan", 'C' },
    { "dark", 'D' },
    { "green", 'G' },
    { "magenta", 'M' },
    { "red", 'R' },
    { "white", 'W' },
    { "yellow", 'Y' },
    { NULL, ' ' }
};

static const struct olc_cmd_type repedit_table[] = {
    { "?", show_help },
    { "commands", show_commands },
    { "list", repedit_list },
    { "show", repedit_show },
    { "create", repedit_create },
    { "name", repedit_name },
    { "description", repedit_description },
    { "comments", repedit_comments },
    { "flags", repedit_flags },
    { "initial", repedit_initial },
    { "token", repedit_token },
    { "rank", repedit_rank },
    { NULL, 0 }
};

static const OLC_EDITOR_DEF repedit_def = {
    .name           = "RepEdit",
    .editor_type    = ED_REPUTATION,
    .cmd_table      = repedit_table,
    .show_fn        = repedit_show,
    .tabs           = {
        .count      = 2,
        .tabs       = {
            { "Basic", "Bas", repedit_show_basic_tab },
            { "Ranks", "Rnk", repedit_show_ranks_tab },
        },
    },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR,
    },
    .change_mode    = OLC_CHANGE_NONE,
    .audit_changes  = false,
};

void do_repedit(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    WNUM wnum;
    REPUTATION_INDEX_DATA *rep;

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &repedit_def, NULL)) {
        send_to_char("You don't have permission to edit reputations.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (IS_NULLSTR(arg)) {
        send_to_char("Syntax: repedit list\n\r", ch);
        send_to_char("        repedit create <widevnum>\n\r", ch);
        send_to_char("        repedit <widevnum>\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "list")) {
        repedit_list(ch, "");
        return;
    }

    if (!str_cmp(arg, "create")) {
        repedit_create(ch, argument);
        return;
    }

    if (!parse_widevnum(arg, ch->in_room ? ch->in_room->area : NULL, &wnum) || !wnum.pArea) {
        send_to_char("Please specify a valid reputation widevnum.\n\r", ch);
        return;
    }

    rep = get_reputation_index(wnum.pArea, wnum.vnum);
    if (!rep) {
        send_to_char("No reputation exists with that widevnum.\n\r", ch);
        send_to_char("Use 'repedit create <widevnum>' first.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &repedit_def, rep, true);
}

void repedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &repedit_def);
}

static REPUTATION_INDEX_DATA *repedit_current(CHAR_DATA *ch)
{
    if (!ch || !ch->desc || !ch->desc->pEdit)
        return NULL;

    return (REPUTATION_INDEX_DATA *)ch->desc->pEdit;
}

static bool repedit_mark_changed(REPUTATION_INDEX_DATA *rep)
{
    if (!rep || !rep->area)
        return false;

    SET_BIT(rep->area->area_flags, AREA_CHANGED);
    return true;
}

static void repedit_reorder_ranks(REPUTATION_INDEX_DATA *rep)
{
    int ordinal = 0;
    ITERATOR it;
    REPUTATION_INDEX_RANK_DATA *rank;

    if (!rep || !rep->ranks)
        return;

    iterator_start(&it, rep->ranks);
    while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it))) {
        rank->ordinal = ++ordinal;
    }
    iterator_stop(&it);
}

static REPUTATION_INDEX_RANK_DATA *repedit_new_rank(const char *name)
{
    REPUTATION_INDEX_RANK_DATA *rank = alloc_perm(sizeof(*rank));
    if (!rank)
        return NULL;

    memset(rank, 0, sizeof(*rank));
    rank->valid = true;
    rank->name = str_dup(IS_NULLSTR(name) ? "Unnamed" : name);
    rank->description = str_dup("");
    rank->comments = str_dup("");
    rank->capacity = 1;
    rank->color = 'Y';
    return rank;
}

REPEDIT(repedit_list)
{
    AREA_DATA *area;
    int count = 0;

    send_to_char("{WUID#VNUM        Name                           Area{x\n\r", ch);
    send_to_char("{D---------------------------------------------------------------{x\n\r", ch);

    for (area = area_first; area != NULL; area = area->next) {
        for (int i = 0; i < MAX_KEY_HASH; i++) {
            REPUTATION_INDEX_DATA *rep;
            for (rep = area->reputation_index_hash[i]; rep; rep = rep->next) {
                printf_to_char(ch, "{W%-14s %-30.30s %s{x\n\r",
                    widevnum_string(area, rep->vnum, NULL),
                    rep->name ? rep->name : "(unnamed)",
                    area->name ? area->name : "(no area)");
                count++;
            }
        }
    }

    if (count < 1)
        send_to_char("No reputations are defined.\n\r", ch);

    return false;
}

REPEDIT(repedit_show)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    const OLC_EDITOR_THEME *theme = olc_get_theme(&repedit_def);
    OLC_LAYOUT_CTX *ctx;
    int tab;

    if (!rep)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx,
        "RepEdit",
        rep->name ? rep->name : "(unnamed)",
        widevnum_string(rep->area, rep->vnum, NULL),
        &repedit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        repedit_show_basic_tab(ch, ctx, rep);
        repedit_show_ranks_tab(ch, ctx, rep);
    } else {
        switch (tab) {
        case 1:
            repedit_show_ranks_tab(ch, ctx, rep);
            break;
        default:
            repedit_show_basic_tab(ch, ctx, rep);
            break;
        }
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

static void repedit_show_basic_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    REPUTATION_INDEX_DATA *rep = (REPUTATION_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&repedit_def);
    char initial_pair[MIL];

    if (!rep)
        return;

    snprintf(initial_pair, sizeof(initial_pair), "rank=%d points=%ld",
        rep->initial_rank, rep->initial_reputation);

    olc_display_section(ctx, theme, "Identity");
    olc_display_string(ctx, theme, "Name:", "name", rep->name);
    olc_display_string(ctx, theme, "Area:", NULL,
        (rep->area && rep->area->name) ? rep->area->name : "");
    olc_display_string(ctx, theme, "Created By:", NULL, rep->created_by);

    olc_display_blank(ctx);
    olc_display_section(ctx, theme, "Configuration");
    olc_display_flags(ctx, theme, "Flags:", "flags", reputation_flags_local, rep->flags);
    olc_display_string(ctx, theme, "Initial:", "initial", initial_pair);
    if (rep->token)
        olc_display_string(ctx, theme, "Token:", "token", widevnum_string_token(rep->token, rep->area));
    else if (rep->token_load.vnum > 0)
        olc_display_string(ctx, theme, "Token:", "token", formatf("%ld#%ld", rep->token_load.auid, rep->token_load.vnum));
    else
        olc_display_string(ctx, theme, "Token:", "token", "(none)");

    olc_display_blank(ctx);
    olc_display_text(ctx, theme, "Description:", "description", rep->description);
    olc_display_text(ctx, theme, "Comments:", "comments", rep->comments);
}

static void repedit_show_ranks_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    REPUTATION_INDEX_DATA *rep = (REPUTATION_INDEX_DATA *)pEdit;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&repedit_def);
    ITERATOR it;
    REPUTATION_INDEX_RANK_DATA *rank;

    if (!rep)
        return;

    olc_display_section(ctx, theme, "Ranks");
    if (!rep->ranks || list_size(rep->ranks) < 1) {
        olc_display_infof(ctx, theme, "{D(none){x");
        return;
    }

    iterator_start(&it, rep->ranks);
    while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it))) {
        olc_display_infof(ctx, theme,
            "{W%2d{x) {W%-20.20s{x {%c%c{x cap={W%ld{x flags={W%s{x",
            rank->ordinal,
            rank->name ? rank->name : "",
            rank->color ? rank->color : 'Y',
            rank->color ? rank->color : 'Y',
            rank->capacity,
            flag_string(reputation_rank_flags_local, rank->flags));
    }
    iterator_stop(&it);
}

REPEDIT(repedit_create)
{
    WNUM wnum;

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum) || !wnum.pArea) {
        send_to_char("Syntax: create <widevnum>\n\r", ch);
        return false;
    }

    if (!IS_BUILDER(ch, wnum.pArea)) {
        send_to_char("You cannot create reputations in that area.\n\r", ch);
        return false;
    }

    if (get_reputation_index(wnum.pArea, wnum.vnum)) {
        send_to_char("A reputation with that widevnum already exists.\n\r", ch);
        return false;
    }

    REPUTATION_INDEX_DATA *rep = alloc_perm(sizeof(*rep));
    if (!rep)
        return false;

    memset(rep, 0, sizeof(*rep));
    rep->valid = true;
    rep->area = wnum.pArea;
    rep->vnum = wnum.vnum;
    rep->name = str_dup("New Reputation");
    rep->description = str_dup("");
    rep->comments = str_dup("");
    rep->created_by = str_dup(ch->name ? ch->name : "unknown");
    rep->ranks = list_create(false);
    rep->initial_rank = 1;

    int hash = wnum.vnum % MAX_KEY_HASH;
    rep->next = wnum.pArea->reputation_index_hash[hash];
    wnum.pArea->reputation_index_hash[hash] = rep;

    repedit_mark_changed(rep);

    olc_editor_enter(ch, &repedit_def, rep, true);
    send_to_char("Reputation created.\n\r", ch);
    return true;
}

REPEDIT(repedit_name)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    bool changed;

    if (!rep)
        return false;

    changed = olc_cmd_string(ch, argument, "name", "name <text>",
        &rep->name, OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
    if (!changed)
        return false;

    repedit_mark_changed(rep);
    return true;
}

REPEDIT(repedit_description)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    bool changed;

    if (!rep)
        return false;

    changed = olc_cmd_string_append(ch, argument, "description", "description",
        &rep->description, NULL, NULL);
    if (!changed)
        return false;

    repedit_mark_changed(rep);
    return true;
}

REPEDIT(repedit_comments)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    bool changed;

    if (!rep)
        return false;

    changed = olc_cmd_string_append(ch, argument, "comments", "comments",
        &rep->comments, NULL, NULL);
    if (!changed)
        return false;

    repedit_mark_changed(rep);
    return true;
}

REPEDIT(repedit_flags)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    bool changed;

    if (!rep)
        return false;

    changed = olc_cmd_flag_toggle(ch, argument, "flags", "flags <flag>",
        &rep->flags, reputation_flags_local, NULL, NULL);
    if (!changed)
        return false;

    repedit_mark_changed(rep);
    return true;
}

REPEDIT(repedit_initial)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    char arg1[MIL];
    int rank_no;
    long points;

    if (!rep)
        return false;

    argument = one_argument(argument, arg1);
    if (!is_number(arg1) || !is_number(argument)) {
        send_to_char("Syntax: initial <rank#> <points>\n\r", ch);
        return false;
    }

    rank_no = atoi(arg1);
    REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep, rank_no);
    if (!rank) {
        send_to_char("Invalid rank number.\n\r", ch);
        return false;
    }

    points = atol(argument);
    if (points < 0 || points >= rank->capacity) {
        printf_to_char(ch, "Points must be between 0 and %ld.\n\r", rank->capacity - 1);
        return false;
    }

    rep->initial_rank = rank_no;
    rep->initial_reputation = points;
    repedit_mark_changed(rep);
    send_to_char("Initial rank/reputation set.\n\r", ch);
    return true;
}

REPEDIT(repedit_token)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    WNUM wnum;

    if (!rep)
        return false;

    if (!str_prefix(argument, "none")) {
        rep->token = NULL;
        rep->token_load.auid = 0;
        rep->token_load.vnum = 0;
        repedit_mark_changed(rep);
        send_to_char("Token cleared.\n\r", ch);
        return true;
    }

    if (!parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &wnum) || !wnum.pArea) {
        send_to_char("Syntax: token <widevnum|none>\n\r", ch);
        return false;
    }

    TOKEN_INDEX_DATA *token = get_token_index(wnum.pArea, wnum.vnum);
    if (!token) {
        send_to_char("No token exists with that widevnum.\n\r", ch);
        return false;
    }

    rep->token = token;
    rep->token_load.auid = token->area ? token->area->uid : 0;
    rep->token_load.vnum = token->vnum;
    repedit_mark_changed(rep);
    send_to_char("Token set.\n\r", ch);
    return true;
}

static char repedit_color_lookup(const char *name)
{
    for (int i = 0; color_name_table[i].name; i++)
        if (!str_prefix(name, color_name_table[i].name))
            return color_name_table[i].color;
    return ' ';
}

REPEDIT(repedit_rank)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    char arg1[MIL], arg2[MIL];

    if (!rep)
        return false;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_NULLSTR(arg1)) {
        send_to_char("Syntax: rank add <name>\n\r", ch);
        send_to_char("        rank clear\n\r", ch);
        send_to_char("        rank <#> remove\n\r", ch);
        send_to_char("        rank <#> name <text>\n\r", ch);
        send_to_char("        rank <#> capacity <n>\n\r", ch);
        send_to_char("        rank <#> color <name>\n\r", ch);
        send_to_char("        rank <#> flags <flag>\n\r", ch);
        send_to_char("        rank <#> description\n\r", ch);
        send_to_char("        rank <#> comments\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "add")) {
        if (IS_NULLSTR(arg2) && IS_NULLSTR(argument)) {
            send_to_char("Syntax: rank add <name>\n\r", ch);
            return false;
        }

        char *name = IS_NULLSTR(argument) ? arg2 : argument;
        REPUTATION_INDEX_RANK_DATA *rank = repedit_new_rank(name);
        if (!rank)
            return false;

        rank->uid = ++rep->top_rank_uid;
        list_appendlink(rep->ranks, rank);
        repedit_reorder_ranks(rep);
        repedit_mark_changed(rep);
        send_to_char("Rank added.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "clear")) {
        if (rep->ranks)
            list_clear(rep->ranks);
        rep->initial_rank = 1;
        rep->initial_reputation = 0;
        repedit_mark_changed(rep);
        send_to_char("Ranks cleared.\n\r", ch);
        return true;
    }

    if (!is_number(arg1)) {
        send_to_char("First argument must be a rank number, or add/clear.\n\r", ch);
        return false;
    }

    int index = atoi(arg1);
    REPUTATION_INDEX_RANK_DATA *rank = get_reputation_rank(rep, index);
    if (!rank) {
        send_to_char("Invalid rank number.\n\r", ch);
        return false;
    }

    if (!str_prefix(arg2, "remove")) {
        list_remnthlink(rep->ranks, index, true);
        repedit_reorder_ranks(rep);
        if (rep->initial_rank > list_size(rep->ranks)) {
            rep->initial_rank = UMAX(1, list_size(rep->ranks));
            rep->initial_reputation = 0;
        }
        repedit_mark_changed(rep);
        send_to_char("Rank removed.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg2, "name")) {
        bool changed = olc_cmd_string(ch, argument, "Rank Name",
            "rank <#> name <text>", &rank->name, OLC_STR_DEFAULT, NULL, NULL);
        if (!changed)
            return false;

        repedit_mark_changed(rep);
        return true;
    }

    if (!str_prefix(arg2, "capacity")) {
        long cap = atol(argument);
        if (cap < 1) {
            send_to_char("Capacity must be >= 1.\n\r", ch);
            return false;
        }
        rank->capacity = cap;
        if (rep->initial_rank == index)
            rep->initial_reputation = UMIN(rep->initial_reputation, cap - 1);
        repedit_mark_changed(rep);
        send_to_char("Rank capacity set.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg2, "color")) {
        char color = repedit_color_lookup(argument);
        if (color == ' ') {
            send_to_char("Valid colors: blue cyan dark green magenta red white yellow\n\r", ch);
            return false;
        }
        rank->color = color;
        repedit_mark_changed(rep);
        send_to_char("Rank color set.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg2, "flags")) {
        bool changed = olc_cmd_flag_toggle(ch, argument, "Rank Flags",
            "rank <#> flags <flag>", &rank->flags, reputation_rank_flags_local, NULL, NULL);
        if (!changed)
            return false;

        repedit_mark_changed(rep);
        return true;
    }

    if (!str_prefix(arg2, "description")) {
        bool changed = olc_cmd_string_append(ch, argument, "Rank Description",
            "rank <#> description", &rank->description, NULL, NULL);
        if (!changed)
            return false;

        repedit_mark_changed(rep);
        return true;
    }

    if (!str_prefix(arg2, "comments")) {
        bool changed = olc_cmd_string_append(ch, argument, "Rank Comments",
            "rank <#> comments", &rank->comments, NULL, NULL);
        if (!changed)
            return false;

        repedit_mark_changed(rep);
        return true;
    }

    send_to_char("Unknown rank subcommand.\n\r", ch);
    return false;
}
