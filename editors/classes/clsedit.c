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

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_CLASS, clazz);
    clsedit_show(ch, "");
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * clsedit - Command interpreter for the class editor
 */
void clsedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;

    if (command[0] == '\0') {
        clsedit_show(ch, argument);
        return;
    }

    for (cmd = 0; clsedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, clsedit_table[cmd].name)) {
            (*clsedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

CLSEDIT(clsedit_show)
{
    CLASS_DATA *clazz;
    BUFFER *buf;

    EDIT_CLASS(ch, clazz);

    buf = new_buf();

    add_buf(buf, formatf("{Y=== Class Editor ==={x\n\r"));
    add_buf(buf, formatf("{cUID:{x  %-6d {cName:{x %s\n\r",
            clazz->uid, clazz->name));

    if (clazz->description && clazz->description[0])
        add_buf(buf, formatf("{cDescription:{x %s\n\r", clazz->description));
    if (clazz->comments && clazz->comments[0])
        add_buf(buf, formatf("{CComments:{x\n\r  %s\n\r", clazz->comments));

    /* Type & flags */
    add_buf(buf, formatf("{cType:{x %s    {cFlags:{x %s\n\r",
            flag_name(class_types, clazz->type),
            clazz->flags ? flag_string(class_flags, clazz->flags) : "none"));

    /* Display / Who names */
    add_buf(buf, formatf("{cDisplay:{x %s  {cWho:{x %s\n\r",
            clazz->display[0] ? clazz->display[0] : "(none)",
            clazz->who[0] ? clazz->who[0] : "(none)"));

    /* Stats */
    add_buf(buf, formatf("{cMax Level:{x %-5d  {cPrimary Stat:{x %s\n\r",
            clazz->max_level,
            (clazz->primary_stat >= 0 && clazz->primary_stat < MAX_STATS)
                ? stat_names[clazz->primary_stat] : "none"));
    add_buf(buf, formatf("{cHP Min:{x %-5d  {cHP Max:{x %-5d  {cGains Mana:{x %s\n\r",
            clazz->hp_min, clazz->hp_max,
            clazz->gains_mana ? "{GYes{x" : "{DNo{x"));

    /* Titles */
    if (clazz->titles && list_size(clazz->titles) > 0) {
        ITERATOR it;
        CLASS_TITLE *title;
        add_buf(buf, "\n\r{cTitles:{x\n\r");
        add_buf(buf, formatf("  {Y%-15s %-20s %-15s %-7s{x\n\r",
                "Keyword", "Display", "Who", "Default"));
        iterator_start(&it, clazz->titles);
        while ((title = (CLASS_TITLE *)iterator_nextdata(&it))) {
            add_buf(buf, formatf("  %-15s %-20s %-15s %s\n\r",
                    title->keyword ? title->keyword : "?",
                    title->display ? title->display : "?",
                    title->who_name ? title->who_name : "?",
                    title->is_default ? "{GYes{x" : "{DNo{x"));
        }
        iterator_stop(&it);
    }

    /* Rewards */
    if (clazz->rewards && list_size(clazz->rewards) > 0) {
        ITERATOR it;
        CLASS_REWARD *reward;
        add_buf(buf, "\n\r{cRewards:{x\n\r");
        add_buf(buf, formatf("  {Y%-5s %-10s %-25s %-6s %-8s %-12s{x\n\r",
                "Lvl", "Type", "Name", "Value", "Scope", "Flags"));
        iterator_start(&it, clazz->rewards);
        while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
            add_buf(buf, formatf("  %-5d %-10s %-25s %-6d %-8s %s\n\r",
                    reward->level,
                    flag_name(reward_types, reward->type),
                    reward->name ? reward->name : "",
                    reward->value,
                    flag_name(reward_scopes, reward->scope),
                    reward->flags ? flag_string(reward_flags, reward->flags) : "none"));
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
            add_buf(buf, "\n\r{cTraits:{x\n\r");
            for (def = trait_def_list; def; def = def->next) {
                int idx = def->index;
                if (idx >= 0 && idx < tc && clazz->trait_values[idx].set) {
                    switch (def->type) {
                        case TRAIT_BOOLEAN:
                            add_buf(buf, formatf("  %-25s %s\n\r",
                                    def->id,
                                    clazz->trait_values[idx].bool_val
                                        ? "{Gtrue{x" : "{Dfalse{x"));
                            break;
                        case TRAIT_INTEGER:
                            add_buf(buf, formatf("  %-25s %d\n\r",
                                    def->id,
                                    clazz->trait_values[idx].int_val));
                            break;
                        case TRAIT_STRING:
                            add_buf(buf, formatf("  %-25s %s\n\r",
                                    def->id,
                                    clazz->trait_values[idx].string_val
                                        ? clazz->trait_values[idx].string_val
                                        : "(null)"));
                            break;
                    }
                }
            }
        }
    }

    /* Enter/Leave callbacks */
    if ((clazz->enter_fun_name && clazz->enter_fun_name[0])
        || (clazz->leave_fun_name && clazz->leave_fun_name[0])) {
        add_buf(buf, "\n\r{cCallbacks:{x\n\r");
        add_buf(buf, formatf("  Enter: %s\n\r",
                clazz->enter_fun_name ? clazz->enter_fun_name : "(none)"));
        add_buf(buf, formatf("  Leave: %s\n\r",
                clazz->leave_fun_name ? clazz->leave_fun_name : "(none)"));
    }

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

/***************************************************************************
 * Command Functions                                                       *
 ***************************************************************************/

CLSEDIT(clsedit_list)
{
    CLASS_DATA *clazz;
    BUFFER *buf;
    int count = 0;

    buf = new_buf();
    add_buf(buf, formatf("{Y%-5s %-20s %-10s %-5s %-5s %-6s %-20s{x\n\r",
            "UID", "Name", "Type", "Max", "HP", "Mana", "Flags"));
    add_buf(buf, formatf("{Y%-5s %-20s %-10s %-5s %-5s %-6s %-20s{x\n\r",
            "-----", "--------------------", "----------", "-----",
            "-----", "------", "--------------------"));

    for (clazz = class_first(); clazz; clazz = clazz->next) {
        if (argument[0] && str_prefix(argument, clazz->name))
            continue;

        add_buf(buf, formatf("%-5d %-20s %-10s %-5d %d-%-2d %-6s %s\n\r",
                clazz->uid,
                clazz->name,
                flag_name(class_types, clazz->type),
                clazz->max_level,
                clazz->hp_min, clazz->hp_max,
                clazz->gains_mana ? "{GYes{x" : "{DNo{x",
                clazz->flags ? flag_string(class_flags, clazz->flags) : "none"));
        count++;
    }

    add_buf(buf, formatf("\n\r%d %s listed.\n\r", count,
            count == 1 ? "class" : "classes"));
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

    olc_set_editor(ch, ED_CLASS, clazz);
    clsedit_show(ch, "");
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

    if (class_find_exact(argument) && class_find_exact(argument) != clazz) {
        send_to_char("A class with that name already exists.\n\r", ch);
        return false;
    }

    free_string(clazz->name);
    clazz->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_description)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: description <text>\n\r", ch);
        send_to_char("        description clear\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(clazz->description);
        clazz->description = &str_empty[0];
        send_to_char("Description cleared.\n\r", ch);
        return true;
    }

    free_string(clazz->description);
    clazz->description = str_dup(argument);
    send_to_char("Description set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_comments)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0')
    {
        string_append(ch, &clazz->comments);
        return true;
    }

    send_to_char("Syntax:  comments\n\r", ch);
    return false;
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

    free_string(clazz->who[bt]);
    clazz->who[bt] = str_dup(argument);
    send_to_char("Who name set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_type)
{
    CLASS_DATA *clazz;
    int value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: type <class type>\n\r", ch);
        send_to_char("Types: mage cleric thief warrior crafting gathering explorer\n\r", ch);
        return false;
    }

    value = flag_value(class_types, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid class type.\n\r", ch);
        return false;
    }

    clazz->type = value;
    send_to_char(formatf("Class type set to: %s\n\r",
            flag_name(class_types, clazz->type)), ch);
    return true;
}

CLSEDIT(clsedit_flags)
{
    CLASS_DATA *clazz;
    long value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: flags <flag list>\n\r", ch);
        send_to_char("Flags: combative no_level caster hidden remort_only default\n\r", ch);
        return false;
    }

    value = flag_value(class_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid flag.\n\r", ch);
        return false;
    }

    clazz->flags ^= value;
    send_to_char(formatf("Flags toggled. Current: %s\n\r",
            flag_string(class_flags, clazz->flags)), ch);
    return true;
}

CLSEDIT(clsedit_maxlevel)
{
    CLASS_DATA *clazz;
    int value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: maxlevel <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 1 || value > MAX_LEVEL) {
        send_to_char(formatf("Max level must be between 1 and %d.\n\r", MAX_LEVEL), ch);
        return false;
    }

    clazz->max_level = value;
    send_to_char("Max level set.\n\r", ch);
    return true;
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

    clazz->primary_stat = value;
    send_to_char(formatf("Primary stat set to: %s\n\r",
            stat_names[value]), ch);
    return true;
}

CLSEDIT(clsedit_hpmin)
{
    CLASS_DATA *clazz;
    int value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: hpmin <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 1 || value > 100) {
        send_to_char("HP min must be between 1 and 100.\n\r", ch);
        return false;
    }

    clazz->hp_min = value;
    send_to_char("HP min set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_hpmax)
{
    CLASS_DATA *clazz;
    int value;

    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: hpmax <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 1 || value > 100) {
        send_to_char("HP max must be between 1 and 100.\n\r", ch);
        return false;
    }

    clazz->hp_max = value;
    send_to_char("HP max set.\n\r", ch);
    return true;
}

CLSEDIT(clsedit_mana)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    if (argument[0] == '\0') {
        send_to_char("Syntax: mana <yes|no>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "yes") || !str_cmp(argument, "true")
        || !str_cmp(argument, "on")) {
        clazz->gains_mana = true;
        send_to_char("Class now gains mana.\n\r", ch);
        return true;
    }

    if (!str_cmp(argument, "no") || !str_cmp(argument, "false")
        || !str_cmp(argument, "off")) {
        clazz->gains_mana = false;
        send_to_char("Class no longer gains mana.\n\r", ch);
        return true;
    }

    send_to_char("Syntax: mana <yes|no>\n\r", ch);
    return false;
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

    EDIT_CLASS(ch, clazz);

    if (!clazz->rewards || list_size(clazz->rewards) == 0) {
        send_to_char("This class has no rewards.\n\r", ch);
        return false;
    }

    buf = new_buf();
    add_buf(buf, formatf("{Y=== Rewards for %s ==={x\n\r\n\r", clazz->name));
    add_buf(buf, formatf("{Y%-5s %-10s %-25s %-6s %-8s %-20s{x\n\r",
            "Lvl", "Type", "Name", "Value", "Scope", "Flags"));
    add_buf(buf, formatf("{Y%-5s %-10s %-25s %-6s %-8s %-20s{x\n\r",
            "-----", "----------", "-------------------------",
            "------", "--------", "--------------------"));

    iterator_start(&it, clazz->rewards);
    while ((reward = (CLASS_REWARD *)iterator_nextdata(&it))) {
        add_buf(buf, formatf("%-5d %-10s %-25s %-6d %-8s %s\n\r",
                reward->level,
                flag_name(reward_types, reward->type),
                reward->name ? reward->name : "",
                reward->value,
                flag_name(reward_scopes, reward->scope),
                reward->flags ? flag_string(reward_flags, reward->flags) : "none"));
    }
    iterator_stop(&it);

    add_buf(buf, formatf("\n\r%d total rewards.\n\r",
            list_size(clazz->rewards)));
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

        add_buf(buf, "{Y=== Available Traits ==={x\n\r");
        add_buf(buf, formatf("{Y%-25s %-8s %-30s{x\n\r",
                "ID", "Type", "Description"));

        for (def = trait_def_list; def; def = def->next) {
            const char *type_str = "?";
            switch (def->type) {
                case TRAIT_BOOLEAN:  type_str = "bool";   break;
                case TRAIT_INTEGER:  type_str = "int";    break;
                case TRAIT_STRING:   type_str = "string"; break;
            }
            add_buf(buf, formatf("%-25s %-8s %s\n\r",
                    def->id, type_str,
                    def->description ? def->description : ""));
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
        clazz->trait_values[idx].set = false;
        send_to_char(formatf("Trait '%s' cleared.\n\r", def->id), ch);
        return true;
    }

    send_to_char("Syntax: trait set <trait_id> <value>\n\r", ch);
    send_to_char("        trait clear <trait_id>\n\r", ch);
    send_to_char("        trait list\n\r", ch);
    return false;
}

CLSEDIT(clsedit_save)
{
    CLASS_DATA *clazz;
    EDIT_CLASS(ch, clazz);

    save_class_data(clazz);
    send_to_char("Class saved to JSON file.\n\r", ch);
    return false;
}
