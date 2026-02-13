/***************************************************************************
 *  gredit.c — OLC Skill Group Editor                                      *
 *                                                                         *
 *  In-game editor for SKILL_GROUP definitions. Allows immortals to view   *
 *  and modify skill groups — named collections of skills used by the      *
 *  class reward system (REWARD_GROUP).                                    *
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
#include "../../skill_data.h"
#include "../../skill_group.h"

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type gredit_table[] =
{
    { "?",              show_help           },
    { "add",            gredit_add          },
    { "commands",       show_commands       },
    { "create",         gredit_create       },
    { "delete",         gredit_delete       },
    { "list",           gredit_list         },
    { "name",           gredit_name         },
    { "remove",         gredit_remove       },
    { "save",           gredit_save         },
    { "show",           gredit_show         },
    { NULL,             0                   }
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_gredit - Enter the skill group editor
 *
 * Syntax:
 *   gredit list [filter]   - List all groups
 *   gredit create <name>   - Create a new group
 *   gredit <name>          - Edit a group by name
 */
void do_gredit(CHAR_DATA *ch, char *argument)
{
    SKILL_GROUP *group;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: gredit <group name>\n\r", ch);
        send_to_char("        gredit list [filter]\n\r", ch);
        send_to_char("        gredit create <name>\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        gredit_list(ch, argument);
        return;
    }

    if (!str_cmp(arg1, "create")) {
        gredit_create(ch, argument);
        return;
    }

    group = skill_group_find(arg1);
    if (!group)
        group = skill_group_search(arg1);

    if (!group) {
        send_to_char("No skill group found with that name.\n\r", ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_GROUP, group);
    gredit_show(ch, "");
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * gredit - Command interpreter for the skill group editor
 */
void gredit(CHAR_DATA *ch, char *argument)
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
        gredit_show(ch, argument);
        return;
    }

    for (cmd = 0; gredit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, gredit_table[cmd].name)) {
            (*gredit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

GREDIT(gredit_show)
{
    SKILL_GROUP *group;
    BUFFER *buf;
    ITERATOR it;
    char *skill_name;
    int count = 0;

    EDIT_GROUP(ch, group);

    buf = new_buf();

    add_buf(buf, formatf("{Y=== Skill Group Editor ==={x\n\r"));
    add_buf(buf, formatf("{cName:{x %s\n\r", group->name));

    add_buf(buf, formatf("\n\r{cSkills:{x (%d total)\n\r",
            group->contents ? list_size(group->contents) : 0));

    if (group->contents) {
        iterator_start(&it, group->contents);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            SKILL_DATA *sk = skill_find(skill_name);
            add_buf(buf, formatf("  %2d. %-30s %s\n\r",
                    ++count,
                    skill_name,
                    sk ? (sk->isspell ? "{G[spell]{x" : "{W[skill]{x") : "{R[unknown]{x"));
        }
        iterator_stop(&it);
    }

    if (count == 0)
        add_buf(buf, "  (empty)\n\r");

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

/***************************************************************************
 * Command Functions                                                       *
 ***************************************************************************/

GREDIT(gredit_list)
{
    SKILL_GROUP *group;
    BUFFER *buf;
    int count = 0;

    buf = new_buf();
    add_buf(buf, formatf("{Y%-30s %-6s{x\n\r", "Group Name", "Skills"));
    add_buf(buf, formatf("{Y%-30s %-6s{x\n\r",
            "------------------------------", "------"));

    for (group = skill_group_first(); group; group = group->next) {
        if (argument[0] && str_prefix(argument, group->name))
            continue;

        add_buf(buf, formatf("%-30s %-6d\n\r",
                group->name,
                group->contents ? list_size(group->contents) : 0));
        count++;
    }

    add_buf(buf, formatf("\n\r%d group%s listed.\n\r",
            count, count == 1 ? "" : "s"));
    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

GREDIT(gredit_name)
{
    SKILL_GROUP *group;
    EDIT_GROUP(ch, group);

    if (argument[0] == '\0') {
        send_to_char("Syntax: name <new name>\n\r", ch);
        return false;
    }

    free_string(group->name);
    group->name = str_dup(argument);
    send_to_char("Group name set.\n\r", ch);
    return true;
}

GREDIT(gredit_add)
{
    SKILL_GROUP *group;
    SKILL_DATA *skill;
    ITERATOR it;
    char *name;

    EDIT_GROUP(ch, group);

    if (argument[0] == '\0') {
        send_to_char("Syntax: add <skill name>\n\r", ch);
        return false;
    }

    skill = skill_find(argument);
    if (!skill)
        skill = skill_search(argument);

    if (!skill) {
        send_to_char("No skill found with that name.\n\r", ch);
        return false;
    }

    /* Check for duplicate */
    if (group->contents) {
        iterator_start(&it, group->contents);
        while ((name = (char *)iterator_nextdata(&it))) {
            if (!str_cmp(name, skill->name)) {
                iterator_stop(&it);
                send_to_char("That skill is already in this group.\n\r", ch);
                return false;
            }
        }
        iterator_stop(&it);
    }

    if (!group->contents)
        group->contents = list_create(false);

    list_appendlink(group->contents, str_dup(skill->name));
    send_to_char(formatf("Added '%s' to the group.\n\r", skill->name), ch);
    return true;
}

GREDIT(gredit_remove)
{
    SKILL_GROUP *group;
    ITERATOR it;
    char *name;

    EDIT_GROUP(ch, group);

    if (argument[0] == '\0') {
        send_to_char("Syntax: remove <skill name>\n\r", ch);
        return false;
    }

    if (!group->contents || list_size(group->contents) == 0) {
        send_to_char("Group has no skills to remove.\n\r", ch);
        return false;
    }

    iterator_start(&it, group->contents);
    while ((name = (char *)iterator_nextdata(&it))) {
        if (!str_prefix(argument, name)) {
            iterator_remcurrent(&it);
            iterator_stop(&it);
            send_to_char(formatf("Removed '%s' from the group.\n\r", name), ch);
            free_string(name);
            return true;
        }
    }
    iterator_stop(&it);

    send_to_char("Skill not found in this group.\n\r", ch);
    return false;
}

GREDIT(gredit_create)
{
    SKILL_GROUP *group;

    if (argument[0] == '\0') {
        send_to_char("Syntax: create <group name>\n\r", ch);
        return false;
    }

    if (skill_group_find(argument)) {
        send_to_char("A group with that name already exists.\n\r", ch);
        return false;
    }

    group = new_skill_group();
    group->name = str_dup(argument);
    group->contents = list_create(false);

    /* Insert into global list (handled by save + reload for now) */
    save_skill_group(group);

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_GROUP, group);

    send_to_char(formatf("Group '%s' created.\n\r", argument), ch);
    gredit_show(ch, "");
    return true;
}

GREDIT(gredit_delete)
{
    send_to_char("Group deletion is not yet supported. Remove all skills and leave it empty.\n\r", ch);
    return false;
}

GREDIT(gredit_save)
{
    SKILL_GROUP *group;
    EDIT_GROUP(ch, group);

    save_skill_group(group);
    send_to_char("Group saved to JSON file.\n\r", ch);
    return false;
}
