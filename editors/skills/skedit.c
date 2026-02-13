/***************************************************************************
 *  skedit.c — OLC Skill/Spell Editor                                     *
 *                                                                         *
 *  In-game editor for SKILL_DATA definitions. Allows immortals to view    *
 *  and modify skill/spell properties including name, level, mana cost,    *
 *  target type, spell function, damage noun, messages, flags, and         *
 *  per-class availability.                                                *
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

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type skedit_table[] =
{
    { "?",              show_help           },
    { "beats",          skedit_beats        },
    { "class",          skedit_class        },
    { "commands",       show_commands       },
    { "damtype",        skedit_damtype      },
    { "difficulty",     skedit_difficulty   },
    { "display",        skedit_display      },
    { "flags",          skedit_flags        },
    { "level",          skedit_level        },
    { "list",           skedit_list         },
    { "mana",           skedit_mana         },
    { "msgobj",         skedit_msgobj       },
    { "msgoff",         skedit_msgoff       },
    { "name",           skedit_name         },
    { "position",       skedit_position     },
    { "save",           skedit_save         },
    { "show",           skedit_show         },
    { "spellfun",       skedit_spellfun     },
    { "target",         skedit_target       },
    { NULL,             0                   }
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_skedit - Enter the skill editor
 *
 * Syntax:
 *   skedit list [filter]   - List all skills
 *   skedit <name>          - Edit a skill by name
 */
void do_skedit(CHAR_DATA *ch, char *argument)
{
    SKILL_DATA *skill;
    char arg1[MAX_INPUT_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: skedit <skill name>\n\r", ch);
        send_to_char("        skedit list [filter]\n\r", ch);
        return;
    }

    if (!str_cmp(arg1, "list")) {
        skedit_list(ch, argument);
        return;
    }

    skill = skill_find(arg1);
    if (!skill)
        skill = skill_search(arg1);

    if (!skill) {
        send_to_char("No skill or spell found with that name.\n\r", ch);
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    olc_set_editor(ch, ED_SKILL, skill);
    skedit_show(ch, "");
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * skedit - Command interpreter for the skill editor
 */
void skedit(CHAR_DATA *ch, char *argument)
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
        skedit_show(ch, argument);
        return;
    }

    for (cmd = 0; skedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, skedit_table[cmd].name)) {
            (*skedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }

    interpret(ch, arg);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

SKEDIT(skedit_show)
{
    SKILL_DATA *skill;
    BUFFER *buf;
    ITERATOR it;
    SKILL_CLASS_LEVEL *scl;

    EDIT_SKILL(ch, skill);

    buf = new_buf();

    add_buf(buf, formatf("{Y=== Skill Editor ==={x\n\r"));
    add_buf(buf, formatf("{cUID:{x  %-6d {cType:{x %s\n\r",
            skill->uid, skill->isspell ? "{GSpell{x" : "{WSkill{x"));
    add_buf(buf, formatf("{cName:{x %s\n\r", skill->name));
    if (skill->display && skill->display[0])
        add_buf(buf, formatf("{cDisplay:{x %s\n\r", skill->display));

    add_buf(buf, formatf("{cFlags:{x %s\n\r",
            skill->flags ? flag_string(skill_flags, skill->flags) : "none"));

    add_buf(buf, formatf("{cDefault Level:{x %d    {cDifficulty:{x %d\n\r",
            skill->default_level, skill->difficulty));
    add_buf(buf, formatf("{cMana:{x %-5d  {cBeats:{x %-5d  {cPosition:{x %s\n\r",
            skill->min_mana, skill->beats,
            position_table[UMAX(0, skill->minimum_position)].name));
    add_buf(buf, formatf("{cTarget:{x %s\n\r",
            flag_string(spell_target_types, skill->target)));

    if (skill->isspell) {
        add_buf(buf, formatf("{cSpell Fun:{x %s\n\r",
                skill->spell_fun_name ? skill->spell_fun_name : "spell_null"));
    }

    add_buf(buf, formatf("{cDamage Noun:{x %s\n\r",
            skill->noun_damage ? skill->noun_damage : "(none)"));
    add_buf(buf, formatf("{cMsg Off:{x %s\n\r",
            skill->msg_off ? skill->msg_off : "(none)"));
    add_buf(buf, formatf("{cMsg Obj:{x %s\n\r",
            skill->msg_obj ? skill->msg_obj : "(none)"));

    /* Class availability */
    if (skill->class_levels && list_size(skill->class_levels) > 0) {
        add_buf(buf, "\n\r{cClass Availability:{x\n\r");
        add_buf(buf, formatf("  {Y%-20s %-6s %-6s{x\n\r",
                "Class", "Level", "Rating"));
        add_buf(buf, formatf("  {Y%-20s %-6s %-6s{x\n\r",
                "--------------------", "------", "------"));

        iterator_start(&it, skill->class_levels);
        while ((scl = (SKILL_CLASS_LEVEL *)iterator_nextdata(&it))) {
            add_buf(buf, formatf("  %-20s %-6d %-6d\n\r",
                    scl->class_name ? scl->class_name : "?",
                    scl->level, scl->rating));
        }
        iterator_stop(&it);
    }

    /* Legacy class levels */
    bool has_legacy = false;
    for (int i = 0; i < MAX_CLASS; i++) {
        if (skill->skill_level[i] < LEVEL_HERO) {
            has_legacy = true;
            break;
        }
    }
    if (has_legacy) {
        add_buf(buf, "\n\r{cLegacy Class Levels:{x ");
        for (int i = 0; i < MAX_CLASS; i++) {
            if (skill->skill_level[i] < LEVEL_HERO)
                add_buf(buf, formatf("%s=%d/%d ",
                        class_table[i].name,
                        skill->skill_level[i],
                        skill->rating[i]));
        }
        add_buf(buf, "\n\r");
    }

    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

/***************************************************************************
 * Command Functions                                                       *
 ***************************************************************************/

SKEDIT(skedit_list)
{
    SKILL_DATA *skill;
    BUFFER *buf;
    int count = 0;
    bool filter_spells = false;
    bool filter_skills = false;
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);
    if (!str_cmp(arg, "spells")) filter_spells = true;
    else if (!str_cmp(arg, "skills")) filter_skills = true;

    buf = new_buf();
    add_buf(buf, formatf("{Y%-5s %-30s %-6s %-5s %-5s %-5s{x\n\r",
            "UID", "Name", "Type", "Level", "Mana", "Beats"));
    add_buf(buf, formatf("{Y%-5s %-30s %-6s %-5s %-5s %-5s{x\n\r",
            "-----", "------------------------------", "------",
            "-----", "-----", "-----"));

    for (skill = skill_first(); skill; skill = skill->next) {
        if (filter_spells && !skill->isspell) continue;
        if (filter_skills && skill->isspell) continue;
        if (arg[0] && !filter_spells && !filter_skills
            && str_prefix(arg, skill->name))
            continue;

        add_buf(buf, formatf("%-5d %-30s %-6s %-5d %-5d %-5d\n\r",
                skill->uid,
                skill->name,
                skill->isspell ? "spell" : "skill",
                skill->default_level,
                skill->min_mana,
                skill->beats));
        count++;
    }

    add_buf(buf, formatf("\n\r%d %s listed.\n\r", count,
            count == 1 ? "entry" : "entries"));
    page_to_char(buf_string(buf), ch);
    free_buf(buf);
    return false;
}

SKEDIT(skedit_name)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: name <new name>\n\r", ch);
        return false;
    }

    free_string(skill->name);
    skill->name = str_dup(argument);
    send_to_char("Name set.\n\r", ch);
    return true;
}

SKEDIT(skedit_display)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: display <display name>\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(skill->display);
        skill->display = &str_empty[0];
        send_to_char("Display name cleared.\n\r", ch);
        return true;
    }

    free_string(skill->display);
    skill->display = str_dup(argument);
    send_to_char("Display name set.\n\r", ch);
    return true;
}

SKEDIT(skedit_level)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: level <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 1 || value > MAX_LEVEL) {
        send_to_char(formatf("Level must be between 1 and %d.\n\r", MAX_LEVEL), ch);
        return false;
    }

    skill->default_level = value;
    send_to_char("Default level set.\n\r", ch);
    return true;
}

SKEDIT(skedit_difficulty)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: difficulty <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    skill->difficulty = value;
    send_to_char("Difficulty set.\n\r", ch);
    return true;
}

SKEDIT(skedit_mana)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: mana <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 0 || value > 9999) {
        send_to_char("Mana must be between 0 and 9999.\n\r", ch);
        return false;
    }

    skill->min_mana = value;
    send_to_char("Mana cost set.\n\r", ch);
    return true;
}

SKEDIT(skedit_beats)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: beats <number>\n\r", ch);
        return false;
    }

    value = atoi(argument);
    if (value < 0 || value > 999) {
        send_to_char("Beats must be between 0 and 999.\n\r", ch);
        return false;
    }

    skill->beats = value;
    send_to_char("Beats set.\n\r", ch);
    return true;
}

SKEDIT(skedit_target)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: target <target type>\n\r", ch);
        show_help(ch, "target");
        return false;
    }

    value = flag_value(spell_target_types, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid target type.\n\r", ch);
        return false;
    }

    skill->target = value;
    send_to_char("Target set.\n\r", ch);
    return true;
}

SKEDIT(skedit_position)
{
    SKILL_DATA *skill;
    int value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: position <position>\n\r", ch);
        return false;
    }

    value = flag_value(position_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid position.\n\r", ch);
        return false;
    }

    skill->minimum_position = value;
    send_to_char("Minimum position set.\n\r", ch);
    return true;
}

SKEDIT(skedit_damtype)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: damtype <damage noun>\n\r", ch);
        send_to_char("        damtype clear\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(skill->noun_damage);
        skill->noun_damage = &str_empty[0];
        send_to_char("Damage noun cleared.\n\r", ch);
        return true;
    }

    free_string(skill->noun_damage);
    skill->noun_damage = str_dup(argument);
    send_to_char("Damage noun set.\n\r", ch);
    return true;
}

SKEDIT(skedit_msgoff)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: msgoff <wear-off message>\n\r", ch);
        send_to_char("        msgoff clear\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(skill->msg_off);
        skill->msg_off = &str_empty[0];
        send_to_char("Wear-off message cleared.\n\r", ch);
        return true;
    }

    free_string(skill->msg_off);
    skill->msg_off = str_dup(argument);
    send_to_char("Wear-off message set.\n\r", ch);
    return true;
}

SKEDIT(skedit_msgobj)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: msgobj <object wear-off message>\n\r", ch);
        send_to_char("        msgobj clear\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "clear")) {
        free_string(skill->msg_obj);
        skill->msg_obj = &str_empty[0];
        send_to_char("Object wear-off message cleared.\n\r", ch);
        return true;
    }

    free_string(skill->msg_obj);
    skill->msg_obj = str_dup(argument);
    send_to_char("Object wear-off message set.\n\r", ch);
    return true;
}

SKEDIT(skedit_spellfun)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: spellfun <function name>\n\r", ch);
        send_to_char("        spellfun none\n\r", ch);
        return false;
    }

    if (!str_cmp(argument, "none") || !str_cmp(argument, "spell_null")) {
        skill->spell_fun = spell_null;
        free_string(skill->spell_fun_name);
        skill->spell_fun_name = str_dup("spell_null");
        skill->isspell = false;
        send_to_char("Spell function cleared — skill is now a non-spell skill.\n\r", ch);
        return true;
    }

    SPELL_FUN *fun = spell_fun_lookup(argument);
    if (!fun) {
        send_to_char("Unknown spell function. Use 'none' to clear.\n\r", ch);
        return false;
    }

    skill->spell_fun = fun;
    free_string(skill->spell_fun_name);
    skill->spell_fun_name = str_dup(argument);
    skill->isspell = (fun != spell_null);
    send_to_char("Spell function set.\n\r", ch);
    return true;
}

SKEDIT(skedit_flags)
{
    SKILL_DATA *skill;
    long value;

    EDIT_SKILL(ch, skill);

    if (argument[0] == '\0') {
        send_to_char("Syntax: flags <flag list>\n\r", ch);
        send_to_char("Available flags: racial remort no_practice no_improve passive token_driven\n\r", ch);
        return false;
    }

    value = flag_value(skill_flags, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid flag.\n\r", ch);
        return false;
    }

    skill->flags ^= value;
    send_to_char(formatf("Flags toggled. Current: %s\n\r",
            flag_string(skill_flags, skill->flags)), ch);
    return true;
}

/**
 * skedit_class - Manage per-class skill availability
 *
 * Syntax:
 *   class add <class_name> <level> [rating]
 *   class remove <class_name>
 *   class list
 */
SKEDIT(skedit_class)
{
    SKILL_DATA *skill;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];

    EDIT_SKILL(ch, skill);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: class add <class_name> <level> [rating]\n\r", ch);
        send_to_char("        class remove <class_name>\n\r", ch);
        return false;
    }

    if (!str_prefix(arg1, "add")) {
        SKILL_CLASS_LEVEL *scl;
        int level, rating = 1;

        if (arg2[0] == '\0' || arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("Syntax: class add <class_name> <level> [rating]\n\r", ch);
            return false;
        }

        level = atoi(arg3);
        if (argument[0] && is_number(argument))
            rating = atoi(argument);

        /* Check for existing entry */
        if (skill->class_levels) {
            ITERATOR it;
            iterator_start(&it, skill->class_levels);
            while ((scl = (SKILL_CLASS_LEVEL *)iterator_nextdata(&it))) {
                if (scl->class_name && !str_cmp(scl->class_name, arg2)) {
                    scl->level = level;
                    scl->rating = rating;
                    iterator_stop(&it);
                    send_to_char(formatf("Updated %s: level %d, rating %d.\n\r",
                                arg2, level, rating), ch);
                    return true;
                }
            }
            iterator_stop(&it);
        }

        /* Add new entry */
        if (!skill->class_levels)
            skill->class_levels = list_create(false);

        scl = new_skill_class_level();
        scl->class_name = str_dup(arg2);
        scl->level = level;
        scl->rating = rating;
        list_appendlink(skill->class_levels, scl);

        send_to_char(formatf("Added %s: level %d, rating %d.\n\r",
                    arg2, level, rating), ch);
        return true;
    }

    if (!str_prefix(arg1, "remove")) {
        SKILL_CLASS_LEVEL *scl;
        ITERATOR it;

        if (arg2[0] == '\0') {
            send_to_char("Syntax: class remove <class_name>\n\r", ch);
            return false;
        }

        if (!skill->class_levels) {
            send_to_char("No class entries to remove.\n\r", ch);
            return false;
        }

        iterator_start(&it, skill->class_levels);
        while ((scl = (SKILL_CLASS_LEVEL *)iterator_nextdata(&it))) {
            if (scl->class_name && !str_cmp(scl->class_name, arg2)) {
                iterator_remcurrent(&it);
                free_string(scl->class_name);
                iterator_stop(&it);
                send_to_char(formatf("Removed class entry '%s'.\n\r", arg2), ch);
                return true;
            }
        }
        iterator_stop(&it);

        send_to_char("Class entry not found.\n\r", ch);
        return false;
    }

    send_to_char("Syntax: class add <class_name> <level> [rating]\n\r", ch);
    send_to_char("        class remove <class_name>\n\r", ch);
    return false;
}

SKEDIT(skedit_save)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    save_skill_data(skill);
    send_to_char("Skill saved to JSON file.\n\r", ch);
    return false;
}
