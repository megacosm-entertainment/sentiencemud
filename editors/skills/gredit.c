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
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"
#include "../../io/json/json_olc.h"

/***************************************************************************
 * History Helpers                                                         *
 ***************************************************************************/

static OLC_CHANGE_HISTORY *gredit_get_history(void *pEdit)
{
    SKILL_GROUP *group = (SKILL_GROUP *)pEdit;
    return group ? (OLC_CHANGE_HISTORY *)group->olc_history : NULL;
}

static OLC_CHANGE_HISTORY *gredit_ensure_history(SKILL_GROUP *group)
{
    if (!group) return NULL;
    if (!group->olc_history)
        group->olc_history = olc_history_load(OLC_HIST_GROUP, group->name);
    if (!group->olc_history)
        group->olc_history = olc_history_new();
    return (OLC_CHANGE_HISTORY *)group->olc_history;
}

/**
 * Convenience: record a change and mark the group dirty for persistence.
 */
static void gredit_record(SKILL_GROUP *group, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    olc_history_record(gredit_ensure_history(group), ch,
        field, old_val, new_val);
    olc_history_mark_dirty(OLC_HIST_GROUP, group->name,
        (OLC_CHANGE_HISTORY *)group->olc_history);
}

/**
 * Generic callback wrapper for olc_cmd_* helpers.
 */
static void gredit_record_cb(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val)
{
    gredit_record((SKILL_GROUP *)ctx, ch, field, old_val, new_val);
}

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
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF gredit_def = {
    .name           = "GrEdit",
    .editor_type    = ED_GROUP,
    .cmd_table      = gredit_table,
    .show_fn        = gredit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_CREATOR
    },
    .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
    .audit_changes  = true,
    .get_history_fn = gredit_get_history,
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

    if (!olc_editor_check_perm(ch, &gredit_def, NULL)) {
        send_to_char("You don't have permission to edit skill groups.\n\r", ch);
        return;
    }

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

    olc_editor_enter(ch, &gredit_def, group, true);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * gredit - Command interpreter for the skill group editor
 */
void gredit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &gredit_def);
}

/***************************************************************************
 * Show                                                                    *
 ***************************************************************************/

GREDIT(gredit_show)
{
    SKILL_GROUP *group;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&gredit_def);
    OLC_LAYOUT_CTX *ctx;
    ITERATOR it;
    char *skill_name;
    int count = 0;

    EDIT_GROUP(ch, group);

    ctx = olc_display_new(ch, theme);

    olc_display_header(ctx, "GrEdit", group->name, NULL, &gredit_def);

    olc_display_string(ctx, theme, "Name:", "name", group->name);

    olc_display_section(ctx, theme, formatf("Skills (%d total)",
        group->contents ? list_size(group->contents) : 0));

    if (group->contents) {
        iterator_start(&it, group->contents);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            SKILL_DATA *sk = skill_find(skill_name);
            olc_display_infof(ctx, theme,
                "  %s%2d. %s%-30s %s{x",
                theme->label, ++count,
                theme->value, skill_name,
                sk ? (sk->isspell ? "{G[spell]{x" : "{W[skill]{x") : "{R[unknown]{x");
        }
        iterator_stop(&it);
    }

    if (count == 0)
        olc_display_infof(ctx, theme, "  %s(empty){x", theme->unset);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);
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
    return olc_cmd_string(ch, argument, "Name", NULL, &group->name,
        OLC_STR_DEFAULT, group, gredit_record_cb);
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
    olc_history_record(gredit_ensure_history(group), ch,
        "skills", "", skill->name);
    olc_history_mark_dirty(OLC_HIST_GROUP, group->name,
        (OLC_CHANGE_HISTORY *)group->olc_history);
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
            olc_history_record(gredit_ensure_history(group), ch,
                "skills", name, "");
            olc_history_mark_dirty(OLC_HIST_GROUP, group->name,
                (OLC_CHANGE_HISTORY *)group->olc_history);
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
    olc_history_flush(OLC_HIST_GROUP, group->name,
        (OLC_CHANGE_HISTORY *)group->olc_history);
    send_to_char("Group saved to JSON file.\n\r", ch);
    return false;
}
