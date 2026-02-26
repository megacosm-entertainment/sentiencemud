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
#include "../../class_data.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

/**
 * Return the OLC_CHANGE_HISTORY for a SKILL_DATA, or NULL if none exists.
 */
static OLC_CHANGE_HISTORY *skedit_get_history(void *pEdit)
{
    SKILL_DATA *skill = (SKILL_DATA *)pEdit;
    return skill ? (OLC_CHANGE_HISTORY *)skill->olc_history : NULL;
}

/**
 * Ensure a skill has an allocated change history, creating one if needed.
 * Tries to load persisted history from disk first.
 */
static OLC_CHANGE_HISTORY *skedit_ensure_history(SKILL_DATA *skill)
{
    if (!skill) return NULL;
    if (!skill->olc_history) {
        skill->olc_history = olc_history_load(OLC_HIST_SKILL, skill->name);
        if (!skill->olc_history)
            skill->olc_history = olc_history_new();
    }
    return (OLC_CHANGE_HISTORY *)skill->olc_history;
}

/**
 * Convenience: record a change and mark the skill dirty for persistence.
 */
static void skedit_record(SKILL_DATA *skill, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(skedit_ensure_history(skill), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_SKILL, skill->name, skill->olc_history);
}

/**
 * Generic callback wrapper for olc_cmd_* helpers.
 */
static void skedit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    skedit_record((SKILL_DATA *)ctx, ch, field, old_val, new_val);
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type skedit_table[] =
{
    { "?",              show_help           },
    { "beats",          skedit_beats        },
    { "commands",       show_commands       },
    { "comments",       skedit_comments     },
    { "damtype",        skedit_damtype      },
    { "description",    skedit_description  },
    { "difficulty",     skedit_difficulty   },
    { "display",        skedit_display      },
    { "flags",          skedit_flags        },
    { "helpkeyword",    skedit_helpkeyword  },
    { "list",           skedit_list         },
    { "mana",           skedit_mana         },
    { "msgobj",         skedit_msgobj       },
    { "msgoff",         skedit_msgoff       },
    { "name",           skedit_name         },
    { "position",       skedit_position     },
    { "save",           skedit_save         },
    { "show",           skedit_show         },
    { "spellfun",       skedit_spellfun     },
    { "summary",        skedit_summary      },
    { "target",         skedit_target       },
    { NULL,             0                   }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF skedit_def = {
    .name           = "SkEdit",
    .editor_type    = ED_SKILL,
    .cmd_table      = skedit_table,
    .show_fn        = skedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = skedit_get_history,
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

    if (!olc_editor_check_perm(ch, &skedit_def, NULL)) {
        send_to_char("You don't have permission to edit skills.\n\r", ch);
        return;
    }

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

    olc_editor_enter(ch, &skedit_def, skill, true);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * skedit - Command interpreter for the skill editor
 */
void skedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &skedit_def);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

SKEDIT(skedit_show)
{
    SKILL_DATA *skill;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&skedit_def);
    OLC_LAYOUT_CTX *ctx;

    EDIT_SKILL(ch, skill);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "SkEdit", skill->name,
        formatf("UID %d", skill->uid), &skedit_def);

    olc_display_pair(ctx, theme,
        "Name:", "name", skill->name,
        "Type:", NULL, skill->isspell ? "Spell" : "Skill");

    olc_display_string(ctx, theme, "Display:", "display",
        (skill->display && skill->display[0]) ? skill->display : NULL);
    olc_display_string(ctx, theme, "Summary:", "summary", skill->summary);
    olc_display_string(ctx, theme, "Help Keyword:", "helpkeyword", skill->help_keyword);

    olc_display_text(ctx, theme, "Description:", "description", skill->description);

    if (!IS_NULLSTR(skill->comments))
        olc_display_text(ctx, theme, "Comments:", "comments", skill->comments);

    olc_display_section(ctx, theme, "Properties");

    olc_display_flags(ctx, theme, "Flags:", "flags", skill_flags, skill->flags);
    olc_display_number(ctx, theme, "Difficulty:", "difficulty", skill->difficulty);
    olc_display_pair(ctx, theme,
        "Mana:", "mana", formatf("%d", skill->min_mana),
        "Beats:", "beats", formatf("%d", skill->beats));
    olc_display_string(ctx, theme, "Position:", "position",
        position_table[UMAX(0, skill->minimum_position)].name);
    olc_display_type(ctx, theme, "Target:", "target",
        spell_target_types, skill->target);

    if (skill->isspell) {
        olc_display_string(ctx, theme, "Spell Fun:", "spellfun",
            skill->spell_fun_name ? skill->spell_fun_name : "spell_null");
    }

    olc_display_section(ctx, theme, "Messages");

    olc_display_string(ctx, theme, "Damage Noun:", "damtype", skill->noun_damage);
    olc_display_string(ctx, theme, "Msg Off:", "msgoff", skill->msg_off);
    olc_display_string(ctx, theme, "Msg Obj:", "msgobj", skill->msg_obj);

    /* Legacy class levels */
    {
        bool has_legacy = false;
        for (int i = 0; i < MAX_CLASS; i++) {
            if (skill->skill_level[i] < LEVEL_HERO) {
                has_legacy = true;
                break;
            }
        }
        if (has_legacy) {
            olc_display_section(ctx, theme, "Legacy Class Levels");
            for (int i = 0; i < MAX_CLASS; i++) {
                if (skill->skill_level[i] < LEVEL_HERO) {
                    const char *legacy_name = class_name_from_legacy(i);
                    olc_display_string(ctx, theme,
                        formatf("  %s:", legacy_name ? legacy_name : "unknown"), NULL,
                        formatf("%d/%d", skill->skill_level[i], skill->rating[i]));
                }
            }
        }
    }

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
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
    add_buf(buf, formatf("{Y%-5s %-30s %-6s %-5s %-5s{x\n\r",
            "UID", "Name", "Type", "Mana", "Beats"));
    add_buf(buf, formatf("{Y%-5s %-30s %-6s %-5s %-5s{x\n\r",
            "-----", "------------------------------", "------",
            "-----", "-----"));

    for (skill = skill_first(); skill; skill = skill->next) {
        if (filter_spells && !skill->isspell) continue;
        if (filter_skills && skill->isspell) continue;
        if (arg[0] && !filter_spells && !filter_skills
            && str_prefix(arg, skill->name))
            continue;

        add_buf(buf, formatf("%-5d %-30s %-6s %-5d %-5d\n\r",
                skill->uid,
                skill->name,
                skill->isspell ? "spell" : "skill",
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
    return olc_cmd_string(ch, argument, "Name", NULL, &skill->name,
        OLC_STR_DEFAULT, skill, skedit_record_cb);
}

SKEDIT(skedit_display)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Display", NULL, &skill->display,
        OLC_STR_CLEARABLE, skill, skedit_record_cb);
}

SKEDIT(skedit_summary)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Summary", NULL, &skill->summary,
        OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, skill, skedit_record_cb);
}

SKEDIT(skedit_description)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Description", NULL, &skill->description,
        OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, skill, skedit_record_cb);
}

SKEDIT(skedit_comments)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string_append(ch, argument, "Comments", NULL, &skill->comments,
        skill, skedit_record_cb);
}

SKEDIT(skedit_helpkeyword)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Help Keyword", NULL, &skill->help_keyword,
        OLC_STR_CLEARABLE | OLC_STR_CLEAR_NULL, skill, skedit_record_cb);
}

SKEDIT(skedit_difficulty)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_number_i16(ch, argument, "Difficulty", NULL, &skill->difficulty,
        INT_MIN, INT_MAX, skill, skedit_record_cb);
}

SKEDIT(skedit_mana)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_number_i16(ch, argument, "Mana", NULL, &skill->min_mana,
        0, 9999, skill, skedit_record_cb);
}

SKEDIT(skedit_beats)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_number_i16(ch, argument, "Beats", NULL, &skill->beats,
        0, 999, skill, skedit_record_cb);
}

SKEDIT(skedit_target)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_type_set_i16(ch, argument, "Target", NULL, &skill->target,
        spell_target_types, skill, skedit_record_cb);
}

SKEDIT(skedit_position)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_type_set_i16(ch, argument, "Position", NULL, &skill->minimum_position,
        position_flags, skill, skedit_record_cb);
}

SKEDIT(skedit_damtype)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Damage Noun",
        "Syntax: damtype <damage noun>\n\r"
        "        damtype clear\n\r",
        &skill->noun_damage, OLC_STR_CLEARABLE, skill, skedit_record_cb);
}

SKEDIT(skedit_msgoff)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Msg Off",
        "Syntax: msgoff <wear-off message>\n\r"
        "        msgoff clear\n\r",
        &skill->msg_off, OLC_STR_CLEARABLE, skill, skedit_record_cb);
}

SKEDIT(skedit_msgobj)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);
    return olc_cmd_string(ch, argument, "Msg Obj",
        "Syntax: msgobj <object wear-off message>\n\r"
        "        msgobj clear\n\r",
        &skill->msg_obj, OLC_STR_CLEARABLE, skill, skedit_record_cb);
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
        skedit_record(skill, ch,
            "spellfun", skill->spell_fun_name, "spell_null");
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

    skedit_record(skill, ch,
        "spellfun", skill->spell_fun_name, argument);
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
    EDIT_SKILL(ch, skill);
    return olc_cmd_flag_toggle(ch, argument, "Flags", NULL, &skill->flags,
        skill_flags, skill, skedit_record_cb);
}

SKEDIT(skedit_save)
{
    SKILL_DATA *skill;
    EDIT_SKILL(ch, skill);

    save_skill_data(skill);
    olc_history_flush(OLC_HIST_SKILL, skill->name, skill->olc_history);
    send_to_char("Skill saved to JSON file.\n\r", ch);
    return false;
}
