#include <stdio.h>
#include <string.h>

#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../common.h"
#include "../common/olc_editor.h"

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
        .count      = 1,
        .tabs       = {
            { "Basic", "Bas", NULL },
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
    if (!rep)
        return false;

    BUFFER *buffer = new_buf();
    if (!buffer)
        return false;

    bprintf(buffer, "Reputation: {W%s{x\n\r", widevnum_string(rep->area, rep->vnum, NULL));
    bprintf(buffer, "Name: {W%s{x\n\r", rep->name ? rep->name : "");
    bprintf(buffer, "Area: {W%s{x\n\r", rep->area && rep->area->name ? rep->area->name : "");
    bprintf(buffer, "Created By: {W%s{x\n\r", rep->created_by ? rep->created_by : "");
    bprintf(buffer, "Flags: {W%s{x\n\r", flag_string(reputation_flags_local, rep->flags));
    bprintf(buffer, "Initial Rank: {W%d{x\n\r", rep->initial_rank);
    bprintf(buffer, "Initial Reputation: {W%ld{x\n\r", rep->initial_reputation);

    if (rep->token)
        bprintf(buffer, "Token: {W%s{x\n\r", widevnum_string_token(rep->token, rep->area));
    else if (rep->token_load.vnum > 0)
        bprintf(buffer, "Token: {W%ld#%ld{x\n\r", rep->token_load.auid, rep->token_load.vnum);

    bprintf(buffer, "Description:\n\r%s\n\r", rep->description ? rep->description : "");
    bprintf(buffer, "Comments:\n\r%s\n\r", rep->comments ? rep->comments : "");

    bprintf(buffer, "\n\rRanks:\n\r");
    if (rep->ranks && list_size(rep->ranks) > 0) {
        ITERATOR it;
        REPUTATION_INDEX_RANK_DATA *rank;
        iterator_start(&it, rep->ranks);
        while ((rank = (REPUTATION_INDEX_RANK_DATA *)iterator_nextdata(&it))) {
            bprintf(buffer, "  {W%2d{x) %-20.20s {%c%c{x cap=%ld flags=%s\n\r",
                rank->ordinal,
                rank->name ? rank->name : "",
                rank->color ? rank->color : 'Y',
                rank->color ? rank->color : 'Y',
                rank->capacity,
                flag_string(reputation_rank_flags_local, rank->flags));
        }
        iterator_stop(&it);
    } else {
        bprintf(buffer, "  (none)\n\r");
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
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
    if (!rep)
        return false;

    if (IS_NULLSTR(argument)) {
        send_to_char("Syntax: name <text>\n\r", ch);
        return false;
    }

    smash_tilde(argument);
    free_string(rep->name);
    rep->name = str_dup(argument);
    repedit_mark_changed(rep);
    send_to_char("Name set.\n\r", ch);
    return true;
}

REPEDIT(repedit_description)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    if (!rep)
        return false;

    if (IS_NULLSTR(argument)) {
        string_append(ch, &rep->description);
        repedit_mark_changed(rep);
        return true;
    }

    send_to_char("Syntax: description\n\r", ch);
    return false;
}

REPEDIT(repedit_comments)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    if (!rep)
        return false;

    if (IS_NULLSTR(argument)) {
        string_append(ch, &rep->comments);
        repedit_mark_changed(rep);
        return true;
    }

    send_to_char("Syntax: comments\n\r", ch);
    return false;
}

REPEDIT(repedit_flags)
{
    REPUTATION_INDEX_DATA *rep = repedit_current(ch);
    long value;

    if (!rep)
        return false;

    if (IS_NULLSTR(argument) || (value = flag_value(reputation_flags_local, argument)) == NO_FLAG) {
        send_to_char("Syntax: flags <flag>\n\r", ch);
        send_to_char("Valid flags: hidden peaceful on_update on_encounter\n\r", ch);
        return false;
    }

    TOGGLE_BIT(rep->flags, value);
    repedit_mark_changed(rep);
    send_to_char("Flags updated.\n\r", ch);
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
        if (IS_NULLSTR(argument)) {
            send_to_char("Syntax: rank <#> name <text>\n\r", ch);
            return false;
        }
        free_string(rank->name);
        rank->name = str_dup(argument);
        repedit_mark_changed(rep);
        send_to_char("Rank name set.\n\r", ch);
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
        long value = flag_value(reputation_rank_flags_local, argument);
        if (value == NO_FLAG) {
            send_to_char("Valid flags: norankup paragon reset_paragon peaceful hostile\n\r", ch);
            return false;
        }
        TOGGLE_BIT(rank->flags, value);
        repedit_mark_changed(rep);
        send_to_char("Rank flags updated.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg2, "description")) {
        string_append(ch, &rank->description);
        repedit_mark_changed(rep);
        return true;
    }

    if (!str_prefix(arg2, "comments")) {
        string_append(ch, &rank->comments);
        repedit_mark_changed(rep);
        return true;
    }

    send_to_char("Unknown rank subcommand.\n\r", ch);
    return false;
}
