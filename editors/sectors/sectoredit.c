#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../../merc.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../sectors_runtime.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

SECTOREDIT(sectoredit_list);
SECTOREDIT(sectoredit_show);
SECTOREDIT(sectoredit_name);
SECTOREDIT(sectoredit_description);
SECTOREDIT(sectoredit_class);
SECTOREDIT(sectoredit_flags);
SECTOREDIT(sectoredit_movecost);
SECTOREDIT(sectoredit_healrate);
SECTOREDIT(sectoredit_manarate);
SECTOREDIT(sectoredit_moverate);
SECTOREDIT(sectoredit_soil);
SECTOREDIT(sectoredit_hidemsgs);
SECTOREDIT(sectoredit_affinity);
SECTOREDIT(sectoredit_comments);
SECTOREDIT(sectoredit_save);
SECTOREDIT(sectoredit_reload);

static int sectoredit_resolve_sector(char *argument);
static int sectoredit_current_index(CHAR_DATA *ch);
static void sectoredit_show_basic_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void sectoredit_show_details_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void sectoredit_show_hidemsgs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);
static void sectoredit_show_affinity_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit);

static const struct olc_cmd_type sectoredit_table[] = {
    { "?",         show_help },
    { "commands",  show_commands },
    { "list",      sectoredit_list },
    { "show",      sectoredit_show },
    { "name",      sectoredit_name },
    { "description", sectoredit_description },
    { "class",     sectoredit_class },
    { "flags",     sectoredit_flags },
    { "movecost",  sectoredit_movecost },
    { "health",    sectoredit_healrate },
    { "healrate",  sectoredit_healrate },
    { "mana",      sectoredit_manarate },
    { "manarate",  sectoredit_manarate },
    { "move",      sectoredit_moverate },
    { "soil",      sectoredit_soil },
    { "hidemsgs",  sectoredit_hidemsgs },
    { "affinity",  sectoredit_affinity },
    { "comments",  sectoredit_comments },
    { "save",      sectoredit_save },
    { "reload",    sectoredit_reload },
    { NULL,         0 }
};

static const OLC_EDITOR_DEF sectoredit_def = {
    .name           = "SectorEdit",
    .editor_type    = ED_SECTOR,
    .cmd_table      = sectoredit_table,
    .show_fn        = sectoredit_show,
    .tabs           = {
        .count      = 4,
        .tabs       = {
            { "Basic", "Bas", NULL },
            { "Details", "Det", NULL },
            { "HideMsgs", "Hide", NULL },
            { "Affinity", "Aff", NULL },
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

void do_sectoredit(CHAR_DATA *ch, char *argument)
{
    int sector;
    char arg[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    load_sector_data();

    argument = one_argument(argument, arg);

    if (!olc_editor_check_perm(ch, &sectoredit_def, NULL)) {
        send_to_char("You don't have permission to edit sectors.\n\r", ch);
        return;
    }

    if (IS_NULLSTR(arg)) {
        send_to_char("Syntax: sectoredit list\n\r", ch);
        send_to_char("        sectoredit save\n\r", ch);
        send_to_char("        sectoredit reload\n\r", ch);
        send_to_char("        sectoredit <sector>\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "list")) {
        sectoredit_list(ch, "");
        return;
    }

    if (!str_cmp(arg, "save")) {
        if (!save_sector_data()) {
            send_to_char("Failed to save sectors.json.\n\r", ch);
            return;
        }
        send_to_char("Saved sectors.json.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "reload")) {
        if (!reload_sector_data()) {
            send_to_char("Failed to load sectors.json (defaults restored).\n\r", ch);
            return;
        }
        send_to_char("Reloaded sectors.json.\n\r", ch);
        return;
    }

    sector = sectoredit_resolve_sector(arg);
    if (sector < 0 || sector >= SECT_MAX) {
        send_to_char("No sector found by that name or index.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &sectoredit_def, (void *)(intptr_t)(sector + 1), true);
}

void sectoredit(CHAR_DATA *ch, char *argument)
{
    load_sector_data();
    olc_editor_interp(ch, argument, &sectoredit_def);
}

static int sectoredit_resolve_sector(char *argument)
{
    if (IS_NULLSTR(argument))
        return -1;

    if (is_number(argument))
        return atoi(argument);

    return sector_lookup(argument);
}

static int sectoredit_current_index(CHAR_DATA *ch)
{
    int index;

    if (!ch || !ch->desc || !ch->desc->pEdit)
        return -1;

    index = (int)((intptr_t)ch->desc->pEdit) - 1;
    if (index < 0 || index >= sector_count())
        return -1;

    return index;
}

SECTOREDIT(sectoredit_list)
{
    int i;

    send_to_char("{WIdx  Name                 Move Heal Mana{X\n\r", ch);
    send_to_char("{D---- -------------------- ---- ---- ----{X\n\r", ch);

    for (i = 0; i < sector_count(); i++) {
        printf_to_char(ch, "{W%-4d %-20s %4d %4d %4d{X\n\r",
            i,
            sector_name(i),
            sector_move_cost(i),
            sector_heal_rate(i),
            sector_mana_rate(i));
    }

    return false;
}

SECTOREDIT(sectoredit_show)
{
    OLC_LAYOUT_CTX *ctx;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);
    int index = sectoredit_current_index(ch);
    int tab;

    if (index < 0)
        return false;

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SectorEdit", sector_name(index), formatf("Sector %d", index), &sectoredit_def);

    tab = olc_show_all_tabs_mode(ch) ? -1 : (ch->desc ? ch->desc->nEditTab : 0);
    if (tab < 0) {
        sectoredit_show_basic_tab(ch, ctx, (void *)(intptr_t)(index + 1));
        sectoredit_show_details_tab(ch, ctx, (void *)(intptr_t)(index + 1));
        sectoredit_show_hidemsgs_tab(ch, ctx, (void *)(intptr_t)(index + 1));
        sectoredit_show_affinity_tab(ch, ctx, (void *)(intptr_t)(index + 1));
    } else {
        switch (tab) {
        case 1:
            sectoredit_show_details_tab(ch, ctx, (void *)(intptr_t)(index + 1));
            break;
        case 2:
            sectoredit_show_hidemsgs_tab(ch, ctx, (void *)(intptr_t)(index + 1));
            break;
        case 3:
            sectoredit_show_affinity_tab(ch, ctx, (void *)(intptr_t)(index + 1));
            break;
        default:
            sectoredit_show_basic_tab(ch, ctx, (void *)(intptr_t)(index + 1));
            break;
        }
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    return false;
}

static void sectoredit_show_basic_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    int index;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);
    const char *class_name;

    (void)ch;

    index = (int)((intptr_t)pEdit) - 1;
    if (index < 0 || index >= sector_count())
        return;

    class_name = flag_name(sector_class_table(), sector_class(index));
    if (IS_NULLSTR(class_name))
        class_name = "none";

    olc_display_section(ctx, theme, "Basics");
    olc_display_string(ctx, theme, "Name:", "name", sector_name(index));
    olc_display_string(ctx, theme, "Class:", "class", class_name);
    olc_display_flags(ctx, theme, "Flags:", "flags",
        sector_runtime_flag_table(), sector_runtime_flags_value(index));
    olc_display_number(ctx, theme, "Move Cost:", "movecost", sector_move_cost(index));
    olc_display_number(ctx, theme, "Health Rate:", "health", sector_heal_rate(index));
    olc_display_number(ctx, theme, "Mana Rate:", "mana", sector_mana_rate(index));
    olc_display_number(ctx, theme, "Move Rate:", "move", sector_move_rate(index));
    olc_display_number(ctx, theme, "Soil:", "soil", sector_soil(index));
}

static void sectoredit_show_details_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    int index;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);

    (void)ch;

    index = (int)((intptr_t)pEdit) - 1;
    if (index < 0 || index >= sector_count())
        return;

    olc_display_section(ctx, theme, "Details");
    olc_display_text(ctx, theme, "Description:", "description", sector_description(index));
    olc_display_text(ctx, theme, "Comments:", "comments", sector_comments(index));
}

static void sectoredit_show_hidemsgs_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    int index;
    int i;
    BUFFER *meta;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);

    (void)ch;

    index = (int)((intptr_t)pEdit) - 1;
    if (index < 0 || index >= sector_count())
        return;

    meta = new_buf();
    for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
        add_buf(meta, formatf("%2d) %s\n\r", i + 1,
            IS_NULLSTR(sector_hide_msg(index, i)) ? "{D(empty){X" : sector_hide_msg(index, i)));
    }

    olc_display_section(ctx, theme, "Hide Messages");
    olc_display_text(ctx, theme, "Messages:", "hidemsgs", buf_string(meta));
    free_buf(meta);
}

static void sectoredit_show_affinity_tab(CHAR_DATA *ch, OLC_LAYOUT_CTX *ctx, void *pEdit)
{
    int index;
    int i;
    BUFFER *meta;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&sectoredit_def);

    (void)ch;

    index = (int)((intptr_t)pEdit) - 1;
    if (index < 0 || index >= sector_count())
        return;

    meta = new_buf();
    for (i = 0; i < SECTOR_MAX_AFFINITIES; i++) {
        int catalyst = sector_affinity_catalyst(index, i);
        if (catalyst <= CATALYST_NONE || catalyst >= CATALYST_MAX) {
            add_buf(meta, formatf("%2d) {D(empty){X\n\r", i + 1));
        } else {
            add_buf(meta, formatf("%2d) %-14s %d\n\r", i + 1,
                flag_string(catalyst_types, catalyst),
                sector_affinity_value(index, i)));
        }
    }

    olc_display_section(ctx, theme, "Affinities");
    olc_display_text(ctx, theme, "Entries:", "affinity", buf_string(meta));
    free_buf(meta);
}

SECTOREDIT(sectoredit_name)
{
    int index = sectoredit_current_index(ch);
    char *name;
    bool changed;

    if (index < 0)
        return false;

    name = str_dup(sector_name(index));
    changed = olc_cmd_string(ch, argument, "name", "name <text>",
        &name, OLC_STR_DEFAULT, NULL, NULL);
    if (!changed) {
        free_string(name);
        return false;
    }

    if (!sector_set_name(index, name)) {
        free_string(name);
        return false;
    }

    free_string(name);

    if (!save_sector_data())
        send_to_char("Name updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_description)
{
    int index = sectoredit_current_index(ch);
    char *description;
    bool changed;

    if (index < 0)
        return false;

    description = str_dup(sector_description(index));
    changed = olc_cmd_string_append(ch, argument, "description",
        "description", &description, NULL, NULL);
    if (!changed) {
        free_string(description);
        return false;
    }

    if (!sector_set_description(index, description)) {
        free_string(description);
        return false;
    }

    free_string(description);

    if (!save_sector_data())
        send_to_char("Description updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_class)
{
    int index = sectoredit_current_index(ch);
    int current;
    bool changed;

    if (index < 0)
        return false;

    current = sector_class(index);
    changed = olc_cmd_type_set(ch, argument, "class",
        "Syntax: class <type>\n\r", &current, sector_class_table(), NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_class(index, current))
        return false;

    if (!save_sector_data())
        send_to_char("Class updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_flags)
{
    int index = sectoredit_current_index(ch);
    long flags;
    bool changed;

    if (index < 0)
        return false;

    flags = sector_runtime_flags_value(index);
    changed = olc_cmd_flag_toggle(ch, argument, "flags",
        "Syntax: flags <flag>\n\r", &flags, sector_runtime_flag_table(), NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_runtime_flags(index, flags))
        return false;

    if (!save_sector_data())
        send_to_char("Flags updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_movecost)
{
    int index = sectoredit_current_index(ch);
    int move_cost;
    bool changed;

    if (index < 0)
        return false;

    move_cost = sector_move_cost(index);
    changed = olc_cmd_number(ch, argument, "movecost",
        "Syntax: movecost <1-200>\n\r",
        &move_cost, 1, 200, NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_move_cost(index, move_cost))
        return false;

    if (!save_sector_data())
        send_to_char("Move cost updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_healrate)
{
    int index = sectoredit_current_index(ch);
    int heal_rate;
    bool changed;

    if (index < 0)
        return false;

    heal_rate = sector_heal_rate(index);
    changed = olc_cmd_number(ch, argument, "healrate",
        "Syntax: healrate <1-1000>\n\r",
        &heal_rate, 1, 1000, NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_heal_rate(index, heal_rate))
        return false;

    if (!save_sector_data())
        send_to_char("Heal rate updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_manarate)
{
    int index = sectoredit_current_index(ch);
    int mana_rate;
    bool changed;

    if (index < 0)
        return false;

    mana_rate = sector_mana_rate(index);
    changed = olc_cmd_number(ch, argument, "manarate",
        "Syntax: manarate <1-1000>\n\r",
        &mana_rate, 1, 1000, NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_mana_rate(index, mana_rate))
        return false;

    if (!save_sector_data())
        send_to_char("Mana rate updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_moverate)
{
    int index = sectoredit_current_index(ch);
    int move_rate;
    bool changed;

    if (index < 0)
        return false;

    move_rate = sector_move_rate(index);
    changed = olc_cmd_number(ch, argument, "move",
        "Syntax: move <1-1000>\n\r", &move_rate, 1, 1000, NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_move_rate(index, move_rate))
        return false;

    if (!save_sector_data())
        send_to_char("Move rate updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_soil)
{
    int index = sectoredit_current_index(ch);
    int soil;
    bool changed;

    if (index < 0)
        return false;

    soil = sector_soil(index);
    changed = olc_cmd_number(ch, argument, "soil",
        "Syntax: soil <-100 to 100>\n\r", &soil, -100, 100, NULL, NULL);
    if (!changed)
        return false;

    if (!sector_set_soil(index, soil))
        return false;

    if (!save_sector_data())
        send_to_char("Soil updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_hidemsgs)
{
    int index = sectoredit_current_index(ch);
    char arg1[MIL];
    char arg2[MIL];
    char *rest;
    int slot;

    if (index < 0)
        return false;

    rest = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1) || !str_cmp(arg1, "list")) {
        int i;
        send_to_char("Hide messages:\n\r", ch);
        for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++)
            printf_to_char(ch, "  %d) %s\n\r", i + 1,
                IS_NULLSTR(sector_hide_msg(index, i)) ? "(empty)" : sector_hide_msg(index, i));
        return false;
    }

    if (!str_cmp(arg1, "add")) {
        int i;

        if (IS_NULLSTR(rest)) {
            send_to_char("Syntax: hidemsgs add <text>\n\r", ch);
            return false;
        }

        for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
            if (IS_NULLSTR(sector_hide_msg(index, i))) {
                sector_set_hide_msg(index, i, rest);
                save_sector_data();
                send_to_char("Hide message added.\n\r", ch);
                return true;
            }
        }

        send_to_char("No empty hide-message slots remain; use set/clear.\n\r", ch);
        return false;
    }

    rest = one_argument(rest, arg2);
    if (!is_number(arg2)) {
        send_to_char("Syntax: hidemsgs set <slot> <text>\n\r", ch);
        send_to_char("        hidemsgs clear <slot>\n\r", ch);
        return false;
    }

    slot = atoi(arg2) - 1;
    if (slot < 0 || slot >= SECTOR_MAX_HIDE_MSGS) {
        send_to_char("Hide message slot out of range.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "clear")) {
        if (!sector_set_hide_msg(index, slot, ""))
            return false;
        if (!save_sector_data())
            send_to_char("Hide message cleared, but failed to save sectors.json.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "set")) {
        if (IS_NULLSTR(rest)) {
            send_to_char("Syntax: hidemsgs set <slot> <text>\n\r", ch);
            return false;
        }
        if (!sector_set_hide_msg(index, slot, rest))
            return false;
        if (!save_sector_data())
            send_to_char("Hide message updated, but failed to save sectors.json.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: hidemsgs list\n\r", ch);
    send_to_char("        hidemsgs add <text>\n\r", ch);
    send_to_char("        hidemsgs set <slot> <text>\n\r", ch);
    send_to_char("        hidemsgs clear <slot>\n\r", ch);
    return false;
}

SECTOREDIT(sectoredit_affinity)
{
    int index = sectoredit_current_index(ch);
    char arg1[MIL], arg2[MIL], arg3[MIL], arg4[MIL];
    int slot;
    int catalyst;
    int value;

    if (index < 0)
        return false;

    argument = one_argument(argument, arg1);

    if (IS_NULLSTR(arg1) || !str_cmp(arg1, "list")) {
        int i;
        send_to_char("Affinities:\n\r", ch);
        for (i = 0; i < SECTOR_MAX_AFFINITIES; i++) {
            int c = sector_affinity_catalyst(index, i);
            if (c <= CATALYST_NONE || c >= CATALYST_MAX)
                printf_to_char(ch, "  %d) (empty)\n\r", i + 1);
            else
                printf_to_char(ch, "  %d) %s = %d\n\r", i + 1,
                    flag_string(catalyst_types, c), sector_affinity_value(index, i));
        }
        return false;
    }

    if (!str_cmp(arg1, "clear")) {
        argument = one_argument(argument, arg2);
        if (!is_number(arg2)) {
            send_to_char("Syntax: affinity clear <slot>\n\r", ch);
            return false;
        }

        slot = atoi(arg2) - 1;
        if (!sector_clear_affinity(index, slot)) {
            send_to_char("Invalid affinity slot.\n\r", ch);
            return false;
        }

        if (!save_sector_data())
            send_to_char("Affinity cleared, but failed to save sectors.json.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg1, "set")) {
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);

        if (!is_number(arg2) || IS_NULLSTR(arg3) || !is_number(arg4)) {
            send_to_char("Syntax: affinity set <slot> <type> <value>\n\r", ch);
            return false;
        }

        slot = atoi(arg2) - 1;
        catalyst = flag_value(catalyst_types, arg3);
        value = atoi(arg4);

        if (!sector_set_affinity(index, slot, catalyst, value)) {
            send_to_char("Invalid affinity slot/type/value.\n\r", ch);
            return false;
        }

        if (!save_sector_data())
            send_to_char("Affinity updated, but failed to save sectors.json.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: affinity list\n\r", ch);
    send_to_char("        affinity set <slot> <type> <value>\n\r", ch);
    send_to_char("        affinity clear <slot>\n\r", ch);
    return false;
}

SECTOREDIT(sectoredit_comments)
{
    int index = sectoredit_current_index(ch);
    char *comments;
    bool changed;

    if (index < 0)
        return false;

    comments = str_dup(sector_comments(index));
    changed = olc_cmd_string_append(ch, argument, "comments",
        "comments", &comments, NULL, NULL);
    if (!changed) {
        free_string(comments);
        return false;
    }

    if (!sector_set_comments(index, comments)) {
        free_string(comments);
        return false;
    }

    free_string(comments);

    if (!save_sector_data())
        send_to_char("Comments updated, but failed to save sectors.json.\n\r", ch);

    return true;
}

SECTOREDIT(sectoredit_save)
{
    if (!save_sector_data()) {
        send_to_char("Failed to save sectors.json.\n\r", ch);
        return false;
    }

    send_to_char("Saved sectors.json.\n\r", ch);
    return true;
}

SECTOREDIT(sectoredit_reload)
{
    if (!reload_sector_data()) {
        send_to_char("Failed to load sectors.json (defaults restored).\n\r", ch);
        return false;
    }

    send_to_char("Reloaded sectors.json.\n\r", ch);
    return true;
}
