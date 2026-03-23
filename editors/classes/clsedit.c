/***************************************************************************
 *  clsedit.c — OLC Class Editor                                          *
 *                                                                         *
 *  In-game editor for CLASS_DATA definitions. Allows immortals to view    *
 *  and modify class properties including name, type, flags, stats, HP,    *
 *  mana, rewards, titles, and traits.                                    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../recycle.h"
#include "../../class_data.h"
#include "../../skill_data.h"
#include "../../skill_group.h"
#include "../../traits.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/***************************************************************************
 * Forward Declarations                                                    *
 ***************************************************************************/

DECLARE_OLC_FUN(clsedit_show);
DECLARE_OLC_FUN(clsedit_create);
DECLARE_OLC_FUN(clsedit_name);
DECLARE_OLC_FUN(clsedit_description);
DECLARE_OLC_FUN(clsedit_comments);
DECLARE_OLC_FUN(clsedit_display);
DECLARE_OLC_FUN(clsedit_who);
DECLARE_OLC_FUN(clsedit_type);
DECLARE_OLC_FUN(clsedit_flags);
DECLARE_OLC_FUN(clsedit_maxlevel);
DECLARE_OLC_FUN(clsedit_primary);
DECLARE_OLC_FUN(clsedit_hpmin);
DECLARE_OLC_FUN(clsedit_hpmax);
DECLARE_OLC_FUN(clsedit_mana);
DECLARE_OLC_FUN(clsedit_list);
DECLARE_OLC_FUN(clsedit_reward);
DECLARE_OLC_FUN(clsedit_rewards);
DECLARE_OLC_FUN(clsedit_rewardflags);
DECLARE_OLC_FUN(clsedit_title);
DECLARE_OLC_FUN(clsedit_trait);
DECLARE_OLC_FUN(clsedit_save);
DECLARE_OLC_FUN(clsedit_xpaccept);
DECLARE_OLC_FUN(clsedit_xpcurve);
DECLARE_OLC_FUN(clsedit_xptable);

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type clsedit_table[] =
{
    { "?",              show_help           },
    { "commands",       show_commands       },
    { "comments",       clsedit_comments    },
    { "create",         clsedit_create      },
    { "description",    clsedit_description },
    { "display",        clsedit_display     },
    { "flags",          clsedit_flags       },
    { "hpmax",          clsedit_hpmax       },
    { "hpmin",          clsedit_hpmin       },
    { "list",           clsedit_list        },
    { "mana",           clsedit_mana        },
    { "maxlevel",       clsedit_maxlevel    },
    { "name",           clsedit_name        },
    { "primary",        clsedit_primary     },
    { "reward",         clsedit_reward      },
    { "rewardflags",    clsedit_rewardflags },
    { "rewards",        clsedit_rewards     },
    { "save",           clsedit_save        },
    { "show",           clsedit_show        },
    { "title",          clsedit_title       },
    { "trait",          clsedit_trait        },
    { "type",           clsedit_type        },
    { "who",            clsedit_who         },
    { "xpaccept",       clsedit_xpaccept    },
    { "xpcurve",        clsedit_xpcurve     },
    { "xptable",        clsedit_xptable     },
    { NULL,             0                   }
};

/***************************************************************************
 * Stat name helpers                                                       *
 ***************************************************************************/

static const char *stat_names[] = { "str", "int", "wis", "dex", "con" };

static int clsedit_stat_lookup(const char *name)
{
    for (int i = 0; i < MAX_STATS; i++) {
        if (!str_cmp(name, stat_names[i]))
            return i;
    }
    return -1;
}

static void clsedit_format_xp_accept_mask(long mask, char *out, size_t out_size)
{
    bool first = true;
    size_t used = 0;

    if (!out || out_size == 0)
        return;

    out[0] = '\0';

    if (IS_SET(mask, XP_MASK_COMBAT) && used < out_size) {
        snprintf(out + used, out_size - used, "%scombat", first ? "" : ", ");
        used = strlen(out);
        first = false;
    }
    if (IS_SET(mask, XP_MASK_CRAFTING) && used < out_size) {
        snprintf(out + used, out_size - used, "%scrafting", first ? "" : ", ");
        used = strlen(out);
        first = false;
    }
    if (IS_SET(mask, XP_MASK_GATHERING) && used < out_size) {
        snprintf(out + used, out_size - used, "%sgathering", first ? "" : ", ");
        used = strlen(out);
        first = false;
    }
    if (IS_SET(mask, XP_MASK_EXPLORATION) && used < out_size) {
        snprintf(out + used, out_size - used, "%sexploration", first ? "" : ", ");
        used = strlen(out);
        first = false;
    }

    if (first)
        snprintf(out, out_size, "none");
}

static long clsedit_xp_mask_from_token(const char *token, bool *ok)
{
    if (ok)
        *ok = true;

    if (!token || !token[0]) {
        if (ok) *ok = false;
        return 0;
    }

    if (!str_cmp(token, "combat"))
        return XP_MASK_COMBAT;
    if (!str_cmp(token, "crafting"))
        return XP_MASK_CRAFTING;
    if (!str_cmp(token, "gathering"))
        return XP_MASK_GATHERING;
    if (!str_cmp(token, "exploration") || !str_cmp(token, "explore"))
        return XP_MASK_EXPLORATION;
    if (!str_cmp(token, "noncombat") || !str_cmp(token, "non-combat"))
        return XP_MASK_NON_COMBAT;
    if (!str_cmp(token, "all") || !str_cmp(token, "any"))
        return XP_MASK_ANY;
    if (!str_cmp(token, "none"))
        return 0;

    if (ok)
        *ok = false;
    return 0;
}

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

static OLC_CHANGE_HISTORY *clsedit_get_history(void *pEdit)
{
    CLASS_DATA *clazz = (CLASS_DATA *)pEdit;
    return clazz ? (OLC_CHANGE_HISTORY *)clazz->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *clsedit_ensure_history(CLASS_DATA *clazz)
{
    if (!clazz) return NULL;
    if (!clazz->olc_history)
        clazz->olc_history = olc_history_load(OLC_HIST_CLASS, clazz->name);
    if (!clazz->olc_history)
        clazz->olc_history = olc_history_new();
    return (OLC_CHANGE_HISTORY *)clazz->olc_history;
}

static void clsedit_record(CLASS_DATA *clazz, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(clsedit_ensure_history(clazz), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_CLASS, clazz->name,
        (OLC_CHANGE_HISTORY *)clazz->olc_history);
}

/** Generic callback wrapper for olc_cmd_* helpers. */
static void clsedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    clsedit_record((CLASS_DATA *)ctx, ch, field, old_val, new_val);
}

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF clsedit_def = {
    .name           = "ClsEdit",
    .editor_type    = ED_CLASS,
    .cmd_table      = clsedit_table,
    .show_fn        = clsedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = clsedit_get_history,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_clsedit - Enter the class editor
 *
 * Syntax:
 *   clsedit list [filter]   - List all classes
 *   clsedit create <name>   - Create a new class
 *   clsedit <name>          - Edit a class by name
 */
void do_clsedit(CHAR_DATA *ch, char *argument)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &clsedit_def, NULL)) {
        send_to_char("You don't have permission to edit classes.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: clsedit <class name>\n\r", ch);
        send_to_char("        clsedit list [filter]\n\r", ch);
        send_to_char("        clsedit create <name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        clsedit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        clsedit_create(ch, argument);
        return;
    }

    clazz = class_find(arg1);

    if (!clazz) {
        send_to_char("No class found with that name.\n\r", ch);
        return;
    }

    olc_editor_enter(ch, &clsedit_def, clazz, true);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * clsedit - Command interpreter for the class editor
 */
void clsedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &clsedit_def);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

CLSEDIT(clsedit_show)
{
    CLASS_DATA *clazz;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&clsedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_CLASS(ch, clazz);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "ClsEdit", clazz->name,
        formatf("UID %d", clazz->uid), &clsedit_def);

    olc_display_string(ctx, theme, "Name:", "name", clazz->name);
    if (clazz->description && clazz->description[0])
        olc_display_string(ctx, theme, "Description:", "description", clazz->description);
    if (clazz->comments && clazz->comments[0])
        olc_display_text(ctx, theme, "Comments:", "comments", clazz->comments);

    /* Type & flags */
    olc_display_string(ctx, theme, "Type:", "type", flag_name(class_types, clazz->type));
    olc_display_flags(ctx, theme, "Flags:", "flags", class_flags, clazz->flags);

    /* Display / Who names */
    olc_display_pair(ctx, theme,
        "Display:", "display", clazz->display[0] ? clazz->display[0] : "(none)",
        "Who:", "who", clazz->who[0] ? clazz->who[0] : "(none)");

    /* Stats */
    olc_display_pair(ctx, theme,
        "Max Level:", "maxlevel", formatf("%d", clazz->max_level),
        "Primary Stat:", "primary",
        (clazz->primary_stat >= 0 && clazz->primary_stat < MAX_STATS)
            ? stat_names[clazz->primary_stat] : "none");
    olc_display_pair(ctx, theme,
        "HP Min:", "hpmin", formatf("%d", clazz->hp_min),
        "HP Max:", "hpmax", formatf("%d", clazz->hp_max));
    olc_display_bool(ctx, theme, "Gains Mana:", "mana", clazz->gains_mana);

    /* XP curve */
    if (clazz->xp_table && clazz->xp_table_size > 0) {
        olc_display_string(ctx, theme, "XP Table:", "xptable",
            formatf("custom (%d entries)", clazz->xp_table_size));
    } else {
        olc_display_string(ctx, theme, "XP Table:", "xptable",
            "default curve");
    }

    {
        long effective_mask = (clazz->xp_accept_mask != 0)
            ? clazz->xp_accept_mask
            : class_default_xp_accept_mask(clazz);
        char accept_buf[128];
        const char *default_curve = class_default_xp_curve_name();
        const char *curve_name = (clazz->xp_curve_id && clazz->xp_curve_id[0])
            ? clazz->xp_curve_id
            : (default_curve ? default_curve : "built-in");

        clsedit_format_xp_accept_mask(effective_mask, accept_buf, sizeof(accept_buf));
        olc_display_string(ctx, theme, "XP Accept:", "xpaccept",
            formatf("%s (%s)", accept_buf,
                (clazz->xp_accept_mask != 0) ? "explicit" : "default"));
        olc_display_string(ctx, theme, "XP Curve:", "xpcurve",
            formatf("%s (%s)", curve_name,
                (clazz->xp_curve_id && clazz->xp_curve_id[0]) ? "explicit" : "default"));
    }

    /* Titles */
    if (clazz->titles && list_size(clazz->titles) > 0) {
        ITERATOR it;
        CLASS_TITLE *title;
        olc_display_section(ctx, theme, "Titles");
        olc_display_infof(ctx, theme, "  %s%-15s %-20s %-15s %-7s{x",
            theme->label, "Keyword", "Display", "Who", "Default");
        iterator_start(&it, clazz->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            olc_display_infof(ctx, theme, "  %s%-15s%s %-20s %-15s %s",
                theme->value,
                title->keyword ? title->keyword : "?",
                "{x",
                title->display ? title->display : "?",
                title->who_name ? title->who_name : "?",
                title->is_default ? "{GYes{x" : "{DNo{x");
        }
        iterator_stop(&it);
    }

    /* Rewards */
    if (clazz->rewards && list_size(clazz->rewards) > 0) {
        ITERATOR it;
        CLASS_REWARD *reward;
        olc_display_section(ctx, theme, "Rewards");
        olc_display_infof(ctx, theme, "  %s%-5s %-10s %-25s %-6s %-8s %-12s{x",
            theme->label, "Lvl", "Type", "Name", "Value", "Scope", "Flags");
        iterator_start(&it, clazz->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
            olc_display_infof(ctx, theme, "  %s%-5d%s %-10s %-25s %-6d %-8s %s",
                theme->value, reward->level, "{x",
                flag_name(reward_types, reward->type),
                reward->name ? reward->name : "",
                reward->value,
                flag_name(reward_scopes, reward->scope),
                reward->flags ? flag_string(reward_flags, reward->flags) : "none");
        }
        iterator_stop(&it);
    }

    /* Traits */
    if (clazz->trait_values) {
        int tc = trait_def_count;
        bool has_traits = false;

        for (int i = 0; i < tc; i++) {
            if (clazz->trait_values[i].set) {
                has_traits = true;
                break;
            }
        }

        if (has_traits) {
            TRAIT_DEF *def;
            olc_display_section(ctx, theme, "Traits");
            for (def = trait_def_list; def; def = def->next) {
                int idx = def->index;
                if (idx >= 0 && idx < tc && clazz->trait_values[idx].set) {
                    switch (def->type) {
                        case TRAIT_BOOLEAN:
                            olc_display_bool(ctx, theme,
                                formatf("  %s:", def->id), NULL,
                                clazz->trait_values[idx].bool_val);
                            break;
                        case TRAIT_INTEGER:
                            olc_display_number(ctx, theme,
                                formatf("  %s:", def->id), NULL,
                                clazz->trait_values[idx].int_val);
                            break;
                        case TRAIT_STRING:
                            olc_display_string(ctx, theme,
                                formatf("  %s:", def->id), NULL,
                                clazz->trait_values[idx].string_val
                                    ? clazz->trait_values[idx].string_val
                                    : "(null)");
                            break;
                    }
                }
            }
        }
    }

    /* Enter/Leave callbacks */
    if ((clazz->enter_fun_name && clazz->enter_fun_name[0])
        || (clazz->leave_fun_name && clazz->leave_fun_name[0])) {
        olc_display_section(ctx, theme, "Callbacks");
        olc_display_string(ctx, theme, "  Enter:", NULL,
            clazz->enter_fun_name ? clazz->enter_fun_name : "(none)");
        olc_display_string(ctx, theme, "  Leave:", NULL,
            clazz->leave_fun_name ? clazz->leave_fun_name : "(none)");
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
    return false;
}

/***************************************************************************
 * Command Functions                                                       *
 ***************************************************************************/

CLSEDIT(clsedit_list)
{
    CLASS_DATA *clazz;
    BUFFER *buf;
    bool append_ok = true;
    int count = 0;

    buf = new_buf();
    if (!add_buf(buf, formatf("{Y%-5s %-20s %-10s %-5s %-5s %-6s %-20s{x\n\r",
            "UID", "Name", "Type", "Max", "HP", "Mana", "Flags"))
        || !add_buf(buf, formatf("{Y%-5s %-20s %-10s %-5s %-5s %-6s %-20s{x\n\r",
            "-----", "--------------------", "----------", "-----",
            "-----", "------", "--------------------"))) {
        send_to_char("Class list output exceeded buffer limits.\n\r", ch);
        free_buf(buf);
        return false;
    }

    for (clazz = class_first(); clazz; clazz = clazz->next) {
        if (argument[0] && str_prefix(argument, clazz->name))
            continue;

        if (!add_buf(buf, formatf("%-5d %-20s %-10s %-5d %d-%-2d %-6s %s\n\r",
                clazz->uid,
                clazz->name,
                flag_name(class_types, clazz->type),
                clazz->max_level,
                clazz->hp_min, clazz->hp_max,
                clazz->gains_mana ? "{GYes{x" : "{DNo{x",
                clazz->flags ? flag_string(class_flags, clazz->flags) : "none"))) {
            append_ok = false;
            break;
        }
        count++;
    }

    if (append_ok && !add_buf(buf, formatf("\n\r%d %s listed.\n\r", count,
            count == 1 ? "class" : "classes")))
        append_ok = false;

    if (!append_ok) {
        send_to_char("Class list output exceeded buffer limits.\n\r", ch);
        free_buf(buf);
        return false;
    }

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

CLSEDIT(clsedit_create)
{
    CLASS_DATA *clazz;

    if (argument[0] == '\0') {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }

    if (class_find_exact(argument)) {
        send_to_char("A class with that name already exists.\n\r", ch);
        return false;
    }

    clazz = new_class_data();
    clazz->name = str_dup(argument);
    clazz->type = CLASS_TYPE_WARRIOR;
    clazz->max_level = 30;
    clazz->hp_min = 6;
    clazz->hp_max = 10;

    /* Assign next available UID */
    int max_uid = 0;
    CLASS_DATA *iter;
    for (iter = class_first(); iter; iter = iter->next) {
        if (iter->uid > max_uid)
            max_uid = iter->uid;
    }
    clazz->uid = max_uid + 1;

    /* Set default display/who from name */
    for (int i = 0; i < BODY_TYPE_MAX; i++) {
        clazz->display[i] = str_dup(argument);
        clazz->who[i] = str_dup(argument);
    }

    /* Initialize traits */
    class_init_traits(clazz);

    save_class_data(clazz);

    send_to_char(formatf("Class '%s' created (UID %d).\n\r",
                clazz->name, clazz->uid), ch);

    olc_editor_enter(ch, &clsedit_def, clazz, true);
    return true;
}

CLSEDIT(clsedit_name)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: name <new name>\n\r", ch);
        return false;
    }

    // Validate the input can be a name
    if (!olc_validate_name(ch, argument))
        return false;

    if (class_find_exact(argument) && class_find_exact(argument) != clazz) {
        send_to_char("A class with that name already exists.\n\r", ch);
        return false;
    }

    clsedit_record(clazz, ch, "name", clazz->name, argument);
    free_string(clazz->name);
    clazz->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_description)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_string(ch, argument, "Description", NULL, &clazz->description,
        OLC_STR_CLEARABLE, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_comments)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_string_append(ch, argument, "Comments", NULL,
        &clazz->comments, clazz, clsedit_record_cb);
}

/**
 * clsedit_display - Set the display name for a body type
 *
 * Syntax:
 *   display <body_type> <name>
 *   display all <name>
 */
CLSEDIT(clsedit_display)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0' || argument[0] == '\0') {
        send_to_char("Syntax: display <body_type> <name>\n\r", ch);
        send_to_char("        display all <name>\n\r", ch);
        send_to_char("Body types: male female neutral other\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "all")) {
        clsedit_record(clazz, ch, "display_all",
            clazz->display[0] ? clazz->display[0] : "(none)", argument);
        for (int i = 0; i < BODY_TYPE_MAX; i++) {
            free_string(clazz->display[i]);
            clazz->display[i] = str_dup(argument);
        }
        send_to_char("All display names set.\n\r", ch);
        return true;
    }

    int bt = -1;
    if (!str_cmp(arg1, "male"))    bt = 0;
    if (!str_cmp(arg1, "female"))  bt = 1;
    if (!str_cmp(arg1, "neutral")) bt = 2;
    if (!str_cmp(arg1, "other"))   bt = 3;

    if (bt < 0 || bt >= BODY_TYPE_MAX) {
        send_to_char("Invalid body type. Use: male female neutral other all\n\r", ch);
        return false;
    }

    clsedit_record(clazz, ch, formatf("display_%s", arg1),
        clazz->display[bt] ? clazz->display[bt] : "(none)", argument);
    free_string(clazz->display[bt]);
    clazz->display[bt] = str_dup(argument);
    send_to_char("Display name set.\n\r", ch);
    return true;
}

/**
 * clsedit_who - Set the who-list display name for a body type
 *
 * Syntax:
 *   who <body_type> <name>
 *   who all <name>
 */
CLSEDIT(clsedit_who)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0' || argument[0] == '\0') {
        send_to_char("Syntax: who <body_type> <name>\n\r", ch);
        send_to_char("        who all <name>\n\r", ch);
        send_to_char("Body types: male female neutral other\n\r", ch);
        return false;
    }

    if (!str_cmp(arg1, "all")) {
        clsedit_record(clazz, ch, "who_all",
            clazz->who[0] ? clazz->who[0] : "(none)", argument);
        for (int i = 0; i < BODY_TYPE_MAX; i++) {
            free_string(clazz->who[i]);
            clazz->who[i] = str_dup(argument);
        }
        send_to_char("All who names set.\n\r", ch);
        return true;
    }

    int bt = -1;
    if (!str_cmp(arg1, "male"))    bt = 0;
    if (!str_cmp(arg1, "female"))  bt = 1;
    if (!str_cmp(arg1, "neutral")) bt = 2;
    if (!str_cmp(arg1, "other"))   bt = 3;

    if (bt < 0 || bt >= BODY_TYPE_MAX) {
        send_to_char("Invalid body type. Use: male female neutral other all\n\r", ch);
        return false;
    }

    clsedit_record(clazz, ch, formatf("who_%s", arg1),
        clazz->who[bt] ? clazz->who[bt] : "(none)", argument);
    free_string(clazz->who[bt]);
    clazz->who[bt] = str_dup(argument);
    send_to_char("Who name set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_type)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_type_set_i16(ch, argument, "Type",
        "Syntax: type <class type>\n\r"
        "Types: mage cleric thief warrior crafting gathering explorer\n\r",
        &clazz->type, class_types, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_flags)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_flag_toggle(ch, argument, "Flags",
        "Syntax: flags <flag list>\n\r"
        "Flags: combative no_level caster hidden remort_only default\n\r",
        &clazz->flags, class_flags, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_maxlevel)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_number_i16(ch, argument, "Max Level", NULL,
        &clazz->max_level, 1, MAX_LEVEL, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_primary)
{
    CLASS_DATA *clazz;
    int value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: primary <stat>\n\r", ch);
        send_to_char("Stats: str int wis dex con\n\r", ch);
        return false;
    }

    value = clsedit_stat_lookup(argument);
    if (value < 0) {
        send_to_char("Invalid stat. Use: str int wis dex con\n\r", ch);
        return false;
    }

    clsedit_record(clazz, ch, "primary",
        (clazz->primary_stat >= 0 && clazz->primary_stat < MAX_STATS)
            ? stat_names[clazz->primary_stat] : "none",
        stat_names[value]);
    clazz->primary_stat = value;
    send_to_char(formatf("Primary stat set to: %s\n\r",
            stat_names[value]), ch);
    return true;
}

CLSEDIT(clsedit_hpmin)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_number(ch, argument, "HP Min", NULL,
        &clazz->hp_min, 1, 100, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_hpmax)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_number(ch, argument, "HP Max", NULL,
        &clazz->hp_max, 1, 100, clazz, clsedit_record_cb);
}

CLSEDIT(clsedit_mana)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);
    return olc_cmd_bool(ch, argument, "Gains Mana", NULL,
        &clazz->gains_mana, clazz, clsedit_record_cb);
}

/**
 * clsedit_reward - Add or remove class rewards
 *
 * Syntax:
 *   reward <level> skill <name> [rating]
 *   reward <level> group <name>
 *   reward <level> title <keyword> <display> <who>
 *   reward <level> bonus <stat> <value>
 *   reward <level> token <wnum>
 *   reward <level> trait <name> [value]
 *   reward <level> song <name>
 *   reward <level> custom <key> <value>
 *   reward remove <level> <type> [name]
 */
CLSEDIT(clsedit_reward)
{
    CLASS_DATA *clazz;
    char arg_level[MAX_INPUT_LENGTH];
    char arg_type[MAX_INPUT_LENGTH];
    char arg_name[MAX_INPUT_LENGTH];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg_level);
    argument = one_argument(argument, arg_type);

    if (arg_level[0] == '\0' || arg_type[0] == '\0') {
        send_to_char("Syntax: reward <level> skill|group|title|bonus|token|trait|song|custom <name> [value]\n\r", ch);
        send_to_char("        reward remove <level> <type> [name]\n\r", ch);
        return false;
    }

    /* Handle 'reward remove' */
    if (!str_cmp(arg_level, "remove")) {
        int level;
        int type_val;
        ITERATOR it;
        CLASS_REWARD *reward;

        if (!is_number(arg_type)) {
            send_to_char("Syntax: reward remove <level> <type> [name]\n\r", ch);
            return false;
        }

        level = atoi(arg_type);
        argument = one_argument(argument, arg_name);

        type_val = flag_value(reward_types, arg_name);
        if (type_val == NO_FLAG) {
            send_to_char("Invalid reward type.\n\r", ch);
            return false;
        }

        if (!clazz->rewards) {
            send_to_char("No rewards to remove.\n\r", ch);
            return false;
        }

        iterator_start(&it, clazz->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
            if (reward->level == level && reward->type == type_val) {
                /* If a name filter given, check it */
                if (argument[0] && reward->name
                    && str_cmp(argument, reward->name))
                    continue;

                iterator_remcurrent(&it);
                clsedit_record(clazz, ch, "reward",
                    formatf("L%d %s %s", reward->level,
                        flag_name(reward_types, reward->type),
                        reward->name ? reward->name : ""),
                    "(removed)");
                free_class_reward(reward);
                iterator_stop(&it);
                send_to_char("Reward removed.\n\r", ch);
                return true;
            }
        }
        iterator_stop(&it);

        send_to_char("Matching reward not found.\n\r", ch);
        return false;
    }

    /* Adding a reward */
    if (!is_number(arg_level)) {
        send_to_char("Level must be a number.\n\r", ch);
        return false;
    }

    int level = atoi(arg_level);
    if (level < 1 || level > MAX_LEVEL) {
        send_to_char(formatf("Level must be between 1 and %d.\n\r", MAX_LEVEL), ch);
        return false;
    }

    int type_val = flag_value(reward_types, arg_type);
    if (type_val == NO_FLAG) {
        send_to_char("Invalid reward type. Valid: skill group title bonus token trait song custom\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg_name);

    if (arg_name[0] == '\0'
        && type_val != REWARD_CUSTOM) {
        send_to_char("You must provide a name for the reward.\n\r", ch);
        return false;
    }

    CLASS_REWARD *reward = new_class_reward();
    reward->level = level;
    reward->type = type_val;
    reward->name = str_dup(arg_name);
    reward->scope = REWARD_SCOPE_CLASS;

    /* Parse type-specific value */
    switch (type_val) {
        case REWARD_SKILL:
            reward->value = 1; /* Default rating */
            if (argument[0] && is_number(argument))
                reward->value = atoi(argument);
            break;

        case REWARD_BONUS:
            if (argument[0] == '\0' || !is_number(argument)) {
                send_to_char("Syntax: reward <level> bonus <stat> <value>\n\r", ch);
                free_class_reward(reward);
                return false;
            }
            reward->value = atoi(argument);
            break;

        case REWARD_TRAIT:
            if (argument[0] && is_number(argument))
                reward->value = atoi(argument);
            else
                reward->value = 1; /* Boolean true */
            break;

        case REWARD_SONG:
            /* Name is the song name, no value needed */
            break;

        default:
            if (argument[0] && is_number(argument))
                reward->value = atoi(argument);
            break;
    }

    if (!clazz->rewards)
        clazz->rewards = list_create(false);

    /* Insert sorted by level */
    ITERATOR it;
    CLASS_REWARD *existing;
    bool inserted = false;

    iterator_start(&it, clazz->rewards);
    while ((existing = (CLASS_REWARD *)iterator_nextdata(&it))) {
        if (existing->level > level) {
            iterator_insert_before(&it, reward);
            inserted = true;
            break;
        }
    }
    iterator_stop(&it);

    if (!inserted)
        list_appendlink(clazz->rewards, reward);

    clsedit_record(clazz, ch, "reward", "(added)",
        formatf("L%d %s %s (val %d)", level,
            flag_name(reward_types, type_val), arg_name, reward->value));
    send_to_char(formatf("Reward added: level %d %s '%s' (value %d).\n\r",
                level, flag_name(reward_types, type_val),
                arg_name, reward->value), ch);
    return true;
}

/**
 * clsedit_rewards - Display all rewards for this class
 */
CLSEDIT(clsedit_rewards)
{
    CLASS_DATA *clazz;
    BUFFER *buf;
    ITERATOR it;
    CLASS_REWARD *reward;
    bool append_ok = true;

    EDIT_CLASS(ch, clazz);

    if (!clazz->rewards || list_size(clazz->rewards) == 0) {
        send_to_char("This class has no rewards.\n\r", ch);
        return false;
    }

    buf = new_buf();
    if (!add_buf(buf, formatf("{Y=== Rewards for %s ==={x\n\r\n\r", clazz->name))
        || !add_buf(buf, formatf("{Y%-5s %-10s %-25s %-6s %-8s %-20s{x\n\r",
            "Lvl", "Type", "Name", "Value", "Scope", "Flags"))
        || !add_buf(buf, formatf("{Y%-5s %-10s %-25s %-6s %-8s %-20s{x\n\r",
            "-----", "----------", "-------------------------",
            "------", "--------", "--------------------"))) {
        send_to_char("Class reward output exceeded buffer limits.\n\r", ch);
        free_buf(buf);
        return false;
    }

    iterator_start(&it, clazz->rewards);
    while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
        if (!add_buf(buf, formatf("%-5d %-10s %-25s %-6d %-8s %s\n\r",
                reward->level,
                flag_name(reward_types, reward->type),
                reward->name ? reward->name : "",
                reward->value,
                flag_name(reward_scopes, reward->scope),
                reward->flags ? flag_string(reward_flags, reward->flags) : "none"))) {
            append_ok = false;
            break;
        }
    }
    iterator_stop(&it);

    if (append_ok && !add_buf(buf, formatf("\n\r%d total rewards.\n\r",
            list_size(clazz->rewards))))
        append_ok = false;

    if (!append_ok) {
        send_to_char("Class reward output exceeded buffer limits.\n\r", ch);
        free_buf(buf);
        return false;
    }

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

/**
 * clsedit_rewardflags - Set flags on a specific reward
 *
 * Syntax:
 *   rewardflags <level> <type> <flags>
 */
CLSEDIT(clsedit_rewardflags)
{
    CLASS_DATA *clazz;
    char arg_level[MAX_INPUT_LENGTH];
    char arg_type[MAX_INPUT_LENGTH];
    ITERATOR it;
    CLASS_REWARD *reward;

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg_level);
    argument = one_argument(argument, arg_type);

    if (arg_level[0] == '\0' || arg_type[0] == '\0' || argument[0] == '\0') {
        send_to_char("Syntax: rewardflags <level> <type> <flags>\n\r", ch);
        send_to_char("Flags: revoke_on_leave one_time hidden\n\r", ch);
        return false;
    }

    if (!is_number(arg_level)) {
        send_to_char("Level must be a number.\n\r", ch);
        return false;
    }

    int level = atoi(arg_level);
    int type_val = flag_value(reward_types, arg_type);
    if (type_val == NO_FLAG) {
        send_to_char("Invalid reward type.\n\r", ch);
        return false;
    }

    long flag_val = flag_value(reward_flags, argument);
    if (flag_val == NO_FLAG) {
        send_to_char("Invalid flag.\n\r", ch);
        return false;
    }

    if (!clazz->rewards) {
        send_to_char("No rewards to modify.\n\r", ch);
        return false;
    }

    iterator_start(&it, clazz->rewards);
    while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
        if (reward->level == level && reward->type == type_val) {
            clsedit_record(clazz, ch, "rewardflags",
                reward->flags ? flag_string(reward_flags, reward->flags) : "none",
                flag_string(reward_flags, reward->flags ^ flag_val));
            reward->flags ^= flag_val;
            iterator_stop(&it);
            send_to_char(formatf("Reward flags toggled. Current: %s\n\r",
                    reward->flags ? flag_string(reward_flags, reward->flags)
                                  : "none"), ch);
            return true;
        }
    }
    iterator_stop(&it);

    send_to_char("Matching reward not found.\n\r", ch);
    return false;
}

/**
 * clsedit_title - Manage class titles
 *
 * Syntax:
 *   title add <keyword> <display> <who_name>
 *   title remove <keyword>
 *   title default <keyword>
 */
CLSEDIT(clsedit_title)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: title add <keyword> <display> <who_name>\n\r", ch);
        send_to_char("        title remove <keyword>\n\r", ch);
        send_to_char("        title default <keyword>\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "add")) {
        argument = one_argument(argument, arg3);

        if (arg2[0] == '\0' || arg3[0] == '\0' || argument[0] == '\0') {
            send_to_char("Syntax: title add <keyword> <display> <who_name>\n\r", ch);
            return false;
        }

        /* Check for duplicate keyword */
        if (class_find_title(clazz, arg2)) {
            send_to_char("A title with that keyword already exists.\n\r", ch);
            return false;
        }

        CLASS_TITLE *title = new_class_title();
        title->keyword = str_dup(arg2);
        title->display = str_dup(arg3);
        title->who_name = str_dup(argument);
        title->is_default = false;

        if (!clazz->titles)
            clazz->titles = list_create(false);

        /* If first title, make it default */
        if (list_size(clazz->titles) == 0)
            title->is_default = true;

        list_appendlink(clazz->titles, title);
        clsedit_record(clazz, ch, "title", "(added)",
            formatf("%s %s %s", arg2, arg3, argument));
        send_to_char(formatf("Title '%s' added.\n\r", arg2), ch);
        return true;
    }

    if (!str_prefix(arg1, "remove")) {
        ITERATOR it;
        CLASS_TITLE *title;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: title remove <keyword>\n\r", ch);
            return false;
        }

        if (!clazz->titles) {
            send_to_char("No titles to remove.\n\r", ch);
            return false;
        }

        iterator_start(&it, clazz->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            if (!str_cmp(title->keyword, arg2)) {
                iterator_remcurrent(&it);
                clsedit_record(clazz, ch, "title",
                    formatf("%s %s", title->keyword,
                        title->display ? title->display : ""),
                    "(removed)");
                free_class_title(title);
                iterator_stop(&it);
                send_to_char(formatf("Title '%s' removed.\n\r", arg2), ch);
                return true;
            }
        }
        iterator_stop(&it);

        send_to_char("Title not found.\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "default")) {
        ITERATOR it;
        CLASS_TITLE *title;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: title default <keyword>\n\r", ch);
            return false;
        }

        if (!clazz->titles) {
            send_to_char("No titles available.\n\r", ch);
            return false;
        }

        /* Clear all defaults, then set the matching one */
        bool found = false;
        iterator_start(&it, clazz->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            if (!str_cmp(title->keyword, arg2)) {
                title->is_default = true;
                found = true;
            } else {
                title->is_default = false;
            }
        }
        iterator_stop(&it);

        if (!found) {
            send_to_char("Title not found.\n\r", ch);
            return false;
        }

        clsedit_record(clazz, ch, "title_default", "(changed)", arg2);
        send_to_char(formatf("Title '%s' set as default.\n\r", arg2), ch);
        return true;
    }

    send_to_char("Syntax: title add <keyword> <display> <who_name>\n\r", ch);
    send_to_char("        title remove <keyword>\n\r", ch);
    send_to_char("        title default <keyword>\n\r", ch);
    return false;
}

/**
 * clsedit_trait - Manage class traits
 *
 * Syntax:
 *   trait set <trait_id> <value>
 *   trait clear <trait_id>
 *   trait list
 */
CLSEDIT(clsedit_trait)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: trait set <trait_id> <value>\n\r", ch);
        send_to_char("        trait clear <trait_id>\n\r", ch);
        send_to_char("        trait list\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "list")) {
        TRAIT_DEF *def;
        BUFFER *buf = new_buf();
        bool append_ok = true;

        if (!add_buf(buf, "{Y=== Available Traits ==={x\n\r")
            || !add_buf(buf, formatf("{Y%-25s %-8s %-30s{x\n\r",
                "ID", "Type", "Description"))) {
            send_to_char("Class trait list output exceeded buffer limits.\n\r", ch);
            free_buf(buf);
            return false;
        }

        for (def = trait_def_list; def; def = def->next) {
            const char *type_str = "?";
            switch (def->type) {
                case TRAIT_BOOLEAN:  type_str = "bool";   break;
                case TRAIT_INTEGER:  type_str = "int";    break;
                case TRAIT_STRING:   type_str = "string"; break;
            }
            if (!add_buf(buf, formatf("%-25s %-8s %s\n\r",
                    def->id, type_str,
                    def->description ? def->description : ""))) {
                append_ok = false;
                break;
            }
        }

        if (!append_ok) {
            send_to_char("Class trait list output exceeded buffer limits.\n\r", ch);
            free_buf(buf);
            return false;
        }

        page_to_char(buf_string(buf), ch);
        free_buf(buf);
        return false;
    }

    if (!str_prefix(arg1, "set")) {
        TRAIT_DEF *def;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: trait set <trait_id> <value>\n\r", ch);
            return false;
        }

        def = trait_def_lookup(arg2);
        if (!def) {
            send_to_char("Unknown trait ID.\n\r", ch);
            return false;
        }

        if (!clazz->trait_values) {
            class_init_traits(clazz);
        }

        int idx = def->index;
        clazz->trait_values[idx].set = true;

        switch (def->type) {
            case TRAIT_BOOLEAN:
                if (!str_cmp(argument, "true") || !str_cmp(argument, "yes")
                    || !str_cmp(argument, "1")) {
                    clazz->trait_values[idx].bool_val = true;
                } else if (!str_cmp(argument, "false") || !str_cmp(argument, "no")
                    || !str_cmp(argument, "0")) {
                    clazz->trait_values[idx].bool_val = false;
                } else {
                    /* Default: set to true */
                    clazz->trait_values[idx].bool_val = true;
                }
                break;

            case TRAIT_INTEGER:
                if (argument[0] == '\0' || !is_number(argument)) {
                    send_to_char("Integer value required.\n\r", ch);
                    return false;
                }
                clazz->trait_values[idx].int_val = atoi(argument);
                break;

            case TRAIT_STRING:
                if (argument[0] == '\0') {
                    send_to_char("String value required.\n\r", ch);
                    return false;
                }
                free_string(clazz->trait_values[idx].string_val);
                clazz->trait_values[idx].string_val = str_dup(argument);
                break;
        }

        clsedit_record(clazz, ch, formatf("trait_%s", def->id),
            "(unset)", argument[0] ? argument : "true");
        send_to_char(formatf("Trait '%s' set.\n\r", def->id), ch);
        return true;
    }

    if (!str_prefix(arg1, "clear")) {
        TRAIT_DEF *def;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: trait clear <trait_id>\n\r", ch);
            return false;
        }

        def = trait_def_lookup(arg2);
        if (!def) {
            send_to_char("Unknown trait ID.\n\r", ch);
            return false;
        }

        if (!clazz->trait_values) {
            send_to_char("No traits set.\n\r", ch);
            return false;
        }

        int idx = def->index;
        clsedit_record(clazz, ch, formatf("trait_%s", def->id),
            "(set)", "(cleared)");
        clazz->trait_values[idx].set = false;
        send_to_char(formatf("Trait '%s' cleared.\n\r", def->id), ch);
        return true;
    }

    send_to_char("Syntax: trait set <trait_id> <value>\n\r", ch);
    send_to_char("        trait clear <trait_id>\n\r", ch);
    send_to_char("        trait list\n\r", ch);
    return false;
}

/**
 * clsedit_xpaccept - Manage class XP acceptance routing policy
 *
 * Syntax:
 *   xpaccept show
 *   xpaccept list
 *   xpaccept set <type...>
 *   xpaccept add <type>
 *   xpaccept remove <type>
 *   xpaccept default
 */
CLSEDIT(clsedit_xpaccept)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    long effective_mask;
    char mask_buf[128];

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || !str_prefix(arg1, "show")) {
        effective_mask = (clazz->xp_accept_mask != 0)
            ? clazz->xp_accept_mask
            : class_default_xp_accept_mask(clazz);
        clsedit_format_xp_accept_mask(effective_mask, mask_buf, sizeof(mask_buf));

        send_to_char(formatf("XP accept policy: %s (%s).\n\r",
            mask_buf,
            (clazz->xp_accept_mask != 0) ? "explicit override" : "default by class type"), ch);
        return false;
    }

    if (!str_prefix(arg1, "list")) {
        send_to_char("XP types: combat, crafting, gathering, exploration\n\r", ch);
        send_to_char("Shortcuts: noncombat, all, none\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "default") || !str_prefix(arg1, "clear")) {
        long old_effective = (clazz->xp_accept_mask != 0)
            ? clazz->xp_accept_mask
            : class_default_xp_accept_mask(clazz);
        char old_buf[128];

        clsedit_format_xp_accept_mask(old_effective, old_buf, sizeof(old_buf));
        clazz->xp_accept_mask = 0;
        clsedit_record(clazz, ch, "xpaccept", old_buf, "default");
        send_to_char("XP accept policy reset to class default behavior.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "set")) {
        long new_mask = 0;
        char token[MAX_INPUT_LENGTH];
        char old_buf[128];
        char new_buf[128];
        bool ok;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: xpaccept set <type...>\n\r", ch);
            return false;
        }

        do {
            long bit = clsedit_xp_mask_from_token(arg2, &ok);
            if (!ok) {
                send_to_char(formatf("Unknown XP type '%s'. Use 'xpaccept list'.\n\r", arg2), ch);
                return false;
            }
            new_mask |= bit;
            argument = one_argument(argument, token);
            snprintf(arg2, sizeof(arg2), "%s", token);
        } while (arg2[0] != '\0');

        clsedit_format_xp_accept_mask(
            (clazz->xp_accept_mask != 0) ? clazz->xp_accept_mask : class_default_xp_accept_mask(clazz),
            old_buf, sizeof(old_buf));
        clsedit_format_xp_accept_mask(new_mask, new_buf, sizeof(new_buf));

        clazz->xp_accept_mask = new_mask;
        clsedit_record(clazz, ch, "xpaccept", old_buf, new_buf);
        send_to_char(formatf("XP accept policy set to: %s\n\r", new_buf), ch);
        return true;
    }

    if (!str_prefix(arg1, "add") || !str_prefix(arg1, "remove")) {
        long bit;
        bool ok;
        char old_buf[128];
        char new_buf[128];
        long effective;

        if (arg2[0] == '\0') {
            send_to_char(formatf("Syntax: xpaccept %s <type>\n\r",
                !str_prefix(arg1, "add") ? "add" : "remove"), ch);
            return false;
        }

        bit = clsedit_xp_mask_from_token(arg2, &ok);
        if (!ok || bit == 0 || bit == XP_MASK_ANY) {
            send_to_char("Use a specific type: combat, crafting, gathering, exploration.\n\r", ch);
            return false;
        }

        effective = (clazz->xp_accept_mask != 0)
            ? clazz->xp_accept_mask
            : class_default_xp_accept_mask(clazz);

        clsedit_format_xp_accept_mask(effective, old_buf, sizeof(old_buf));

        if (!str_prefix(arg1, "add"))
            SET_BIT(effective, bit);
        else
            REMOVE_BIT(effective, bit);

        clazz->xp_accept_mask = effective;
        clsedit_format_xp_accept_mask(effective, new_buf, sizeof(new_buf));
        clsedit_record(clazz, ch, "xpaccept", old_buf, new_buf);
        send_to_char(formatf("XP accept policy is now: %s\n\r", new_buf), ch);
        return true;
    }

    send_to_char("Syntax: xpaccept show\n\r", ch);
    send_to_char("        xpaccept list\n\r", ch);
    send_to_char("        xpaccept set <type...>\n\r", ch);
    send_to_char("        xpaccept add <type>\n\r", ch);
    send_to_char("        xpaccept remove <type>\n\r", ch);
    send_to_char("        xpaccept default\n\r", ch);
    return false;
}

/**
 * clsedit_xpcurve - Manage class named XP curve selection
 *
 * Syntax:
 *   xpcurve show
 *   xpcurve list
 *   xpcurve set <curve_id>
 *   xpcurve clear
 */
CLSEDIT(clsedit_xpcurve)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    const char *default_curve;

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    default_curve = class_default_xp_curve_name();

    if (arg1[0] == '\0' || !str_prefix(arg1, "show")) {
        const char *curve_name = (clazz->xp_curve_id && clazz->xp_curve_id[0])
            ? clazz->xp_curve_id
            : (default_curve ? default_curve : "built-in");
        send_to_char(formatf("XP curve: %s (%s).\n\r",
            curve_name,
            (clazz->xp_curve_id && clazz->xp_curve_id[0])
                ? "explicit override"
                : "default"), ch);
        return false;
    }

    if (!str_prefix(arg1, "list")) {
        int count = class_xp_curve_count();

        send_to_char("Named XP curves:\n\r", ch);
        if (count <= 0) {
            send_to_char("  (none loaded; using built-in fallback table)\n\r", ch);
            return false;
        }

        for (int i = 0; i < count; i++) {
            const char *name = class_xp_curve_name(i);
            if (!name)
                continue;
            send_to_char(formatf("  %s%s\n\r", name,
                (default_curve && !str_cmp(name, default_curve)) ? " {Y(default){x" : ""), ch);
        }
        return false;
    }

    if (!str_prefix(arg1, "clear") || !str_prefix(arg1, "default")) {
        char old_curve[MAX_INPUT_LENGTH];

        snprintf(old_curve, sizeof(old_curve), "%s",
            (clazz->xp_curve_id && clazz->xp_curve_id[0]) ? clazz->xp_curve_id : "default");

        free_string(clazz->xp_curve_id);
        clazz->xp_curve_id = str_dup("");
        clsedit_record(clazz, ch, "xpcurve", old_curve, "default");
        send_to_char("XP curve reset to default.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "set")) {
        char old_curve[MAX_INPUT_LENGTH];

        if (arg2[0] == '\0') {
            send_to_char("Syntax: xpcurve set <curve_id>\n\r", ch);
            return false;
        }

        if (!class_xp_curve_exists(arg2)) {
            send_to_char("Unknown curve ID. Use 'xpcurve list' to see available curves.\n\r", ch);
            return false;
        }

        snprintf(old_curve, sizeof(old_curve), "%s",
            (clazz->xp_curve_id && clazz->xp_curve_id[0]) ? clazz->xp_curve_id : "default");
        free_string(clazz->xp_curve_id);
        clazz->xp_curve_id = str_dup(arg2);
        clsedit_record(clazz, ch, "xpcurve", old_curve, arg2);
        send_to_char(formatf("XP curve set to '%s'.\n\r", arg2), ch);
        return true;
    }

    send_to_char("Syntax: xpcurve show\n\r", ch);
    send_to_char("        xpcurve list\n\r", ch);
    send_to_char("        xpcurve set <curve_id>\n\r", ch);
    send_to_char("        xpcurve clear\n\r", ch);
    return false;
}

/**
 * clsedit_xptable - Manage per-class XP table overrides
 *
 * Syntax:
 *   xptable                 - show active XP values for this class
 *   xptable show            - same as above
 *   xptable clear           - remove custom XP table (use default curve)
 *   xptable set <level> <xp> - set XP required to go level -> level+1
 *   xptable calc linear <base> <step>
 *   xptable calc geometric <base> <percent>
 *   xptable calc quadratic <base> <step> <curve>
 *
 * Notes:
 *   Levels are 1-based and valid range is 1..(max_level-1).
 *   geometric percent is per-level multiplier in percent (e.g., 115 = +15% each level).
 */
CLSEDIT(clsedit_xptable)
{
    CLASS_DATA *clazz;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    const long *table;
    int table_size = 0;

    EDIT_CLASS(ch, clazz);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0' || !str_prefix(arg1, "show")) {
        BUFFER *buf = new_buf();
        bool append_ok = true;
        int max_entries = UMAX(0, clazz->max_level - 1);

        if (clazz->xp_table && clazz->xp_table_size > 0) {
            table = clazz->xp_table;
            table_size = clazz->xp_table_size;
        } else {
            table = class_default_xp_table(&table_size);
        }

        if (!add_buf(buf, formatf("{Y=== XP Table for %s ==={x\n\r", clazz->name))
            || !add_buf(buf, formatf("Source: %s\n\r",
                (clazz->xp_table && clazz->xp_table_size > 0) ? "custom" : "default"))
            || !add_buf(buf, "Format: level -> XP required for next level\n\r\n\r")) {
            send_to_char("XP table output exceeded buffer limits.\n\r", ch);
            free_buf(buf);
            return false;
        }

        if (max_entries <= 0) {
            if (!add_buf(buf, "No level transitions (max_level <= 1).\n\r"))
                append_ok = false;
        } else {
            for (int level = 1; level <= max_entries; level++) {
                long xp = (level - 1 < table_size) ? table[level - 1] : 0;
                if (!add_buf(buf, formatf("%3d -> %ld\n\r", level, xp))) {
                    append_ok = false;
                    break;
                }
            }
        }

        if (!append_ok) {
            send_to_char("XP table output exceeded buffer limits.\n\r", ch);
            free_buf(buf);
            return false;
        }

        page_to_char(buf_string(buf), ch);
        free_buf(buf);
        return false;
    }

    if (!str_prefix(arg1, "clear")) {
        if (!clazz->xp_table || clazz->xp_table_size <= 0) {
            send_to_char("This class already uses the default XP curve.\n\r", ch);
            return false;
        }

        clsedit_record(clazz, ch, "xptable",
            formatf("custom (%d entries)", clazz->xp_table_size), "default");
        clazz->xp_table = NULL;
        clazz->xp_table_size = 0;
        send_to_char("Custom XP table cleared. Class now uses the default curve.\n\r", ch);
        return true;
    }

    if (!str_prefix(arg1, "set")) {
        int level;
        long xp;
        int required_entries;
        long *new_table;
        const long *base_table;
        int base_size = 0;

        if (arg2[0] == '\0' || arg3[0] == '\0' || !is_number(arg2) || !is_number(arg3)) {
            send_to_char("Syntax: xptable set <level> <xp>\n\r", ch);
            return false;
        }

        level = atoi(arg2);
        xp = atol(arg3);

        required_entries = UMAX(0, clazz->max_level - 1);
        if (required_entries <= 0) {
            send_to_char("Cannot set XP table: class max_level is too low.\n\r", ch);
            return false;
        }

        if (level < 1 || level > required_entries) {
            send_to_char(formatf("Level must be between 1 and %d for this class.\n\r", required_entries), ch);
            return false;
        }

        if (xp < 1) {
            send_to_char("XP value must be at least 1.\n\r", ch);
            return false;
        }

        if (clazz->xp_table && clazz->xp_table_size > 0) {
            base_table = clazz->xp_table;
            base_size = clazz->xp_table_size;
        } else {
            base_table = class_default_xp_table(&base_size);
        }

        new_table = (long *)alloc_perm(sizeof(long) * required_entries);
        if (!new_table) {
            send_to_char("Memory allocation failed while updating XP table.\n\r", ch);
            return false;
        }

        for (int i = 0; i < required_entries; i++) {
            new_table[i] = (i < base_size) ? base_table[i] : 0;
        }

        new_table[level - 1] = xp;

        clazz->xp_table = new_table;
        clazz->xp_table_size = required_entries;

        clsedit_record(clazz, ch, "xptable_set",
            formatf("level %d = %ld", level,
                (level - 1 < base_size) ? base_table[level - 1] : 0),
            formatf("level %d = %ld", level, xp));

        send_to_char(formatf("XP table updated: level %d now requires %ld XP.\n\r", level, xp), ch);
        return true;
    }

    if (!str_prefix(arg1, "calc")) {
        int required_entries;
        long *new_table;

        required_entries = UMAX(0, clazz->max_level - 1);
        if (required_entries <= 0) {
            send_to_char("Cannot generate XP table: class max_level is too low.\n\r", ch);
            return false;
        }

        if (arg2[0] == '\0') {
            send_to_char("Syntax: xptable calc linear <base> <step>\n\r", ch);
            send_to_char("        xptable calc geometric <base> <percent>\n\r", ch);
            send_to_char("        xptable calc quadratic <base> <step> <curve>\n\r", ch);
            return false;
        }

        new_table = (long *)alloc_perm(sizeof(long) * required_entries);
        if (!new_table) {
            send_to_char("Memory allocation failed while generating XP table.\n\r", ch);
            return false;
        }

        if (!str_prefix(arg2, "linear")) {
            long base, step;
            if (arg3[0] == '\0' || argument[0] == '\0' || !is_number(arg3) || !is_number(argument)) {
                send_to_char("Syntax: xptable calc linear <base> <step>\n\r", ch);
                return false;
            }

            base = atol(arg3);
            step = atol(argument);
            if (base < 1 || step < 0) {
                send_to_char("linear requires: base >= 1 and step >= 0.\n\r", ch);
                return false;
            }

            for (int i = 0; i < required_entries; i++) {
                long xp = base + (step * i);
                new_table[i] = UMAX(1, xp);
            }

            clazz->xp_table = new_table;
            clazz->xp_table_size = required_entries;
            clsedit_record(clazz, ch, "xptable_calc", "(generated)",
                formatf("linear base=%ld step=%ld", base, step));
            send_to_char(formatf("XP table generated with linear curve (base=%ld, step=%ld).\n\r", base, step), ch);
            return true;
        }

        if (!str_prefix(arg2, "geometric")) {
            long base, percent;
            long current;
            if (arg3[0] == '\0' || argument[0] == '\0' || !is_number(arg3) || !is_number(argument)) {
                send_to_char("Syntax: xptable calc geometric <base> <percent>\n\r", ch);
                return false;
            }

            base = atol(arg3);
            percent = atol(argument);
            if (base < 1 || percent < 100) {
                send_to_char("geometric requires: base >= 1 and percent >= 100.\n\r", ch);
                return false;
            }

            current = base;
            for (int i = 0; i < required_entries; i++) {
                new_table[i] = UMAX(1, current);
                current = UMAX(1, (current * percent) / 100);
            }

            clazz->xp_table = new_table;
            clazz->xp_table_size = required_entries;
            clsedit_record(clazz, ch, "xptable_calc", "(generated)",
                formatf("geometric base=%ld percent=%ld", base, percent));
            send_to_char(formatf("XP table generated with geometric curve (base=%ld, percent=%ld).\n\r", base, percent), ch);
            return true;
        }

        if (!str_prefix(arg2, "quadratic")) {
            char arg4[MAX_INPUT_LENGTH];
            long base, step, curve;

            argument = one_argument(argument, arg4);
            if (arg3[0] == '\0' || arg4[0] == '\0'
                || !is_number(arg3) || !is_number(arg4) || !is_number(argument)) {
                send_to_char("Syntax: xptable calc quadratic <base> <step> <curve>\n\r", ch);
                return false;
            }

            base = atol(arg3);
            step = atol(arg4);
            curve = atol(argument);

            if (base < 1 || step < 0 || curve < 0) {
                send_to_char("quadratic requires: base >= 1, step >= 0, curve >= 0.\n\r", ch);
                return false;
            }

            for (int i = 0; i < required_entries; i++) {
                long xp = base + (step * i) + (curve * i * i);
                new_table[i] = UMAX(1, xp);
            }

            clazz->xp_table = new_table;
            clazz->xp_table_size = required_entries;
            clsedit_record(clazz, ch, "xptable_calc", "(generated)",
                formatf("quadratic base=%ld step=%ld curve=%ld", base, step, curve));
            send_to_char(formatf("XP table generated with quadratic curve (base=%ld, step=%ld, curve=%ld).\n\r", base, step, curve), ch);
            return true;
        }

        send_to_char("Unknown curve type. Use: linear, geometric, quadratic.\n\r", ch);
        return false;
    }

    send_to_char("Syntax: xptable\n\r", ch);
    send_to_char("        xptable show\n\r", ch);
    send_to_char("        xptable set <level> <xp>\n\r", ch);
    send_to_char("        xptable calc linear <base> <step>\n\r", ch);
    send_to_char("        xptable calc geometric <base> <percent>\n\r", ch);
    send_to_char("        xptable calc quadratic <base> <step> <curve>\n\r", ch);
    send_to_char("        xptable clear\n\r", ch);
    return false;
}

CLSEDIT(clsedit_save)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    save_class_data(clazz);
    olc_history_flush(OLC_HIST_CLASS, clazz->name,
        (OLC_CHANGE_HISTORY *)clazz->olc_history);
    send_to_char("Class saved to JSON file.\n\r", ch);
    return false;
}
