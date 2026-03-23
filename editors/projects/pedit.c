/***************************************************************************
 *  pedit.c — OLC Project Editor                                          *
 *                                                                         *
 *  In-game editor for PROJECT_DATA definitions. Allows implementors to   *
 *  create and manage building projects including assigned areas, builders,*
 *  completion tracking, and project metadata.                             *
 *                                                                         *
 *  Migrated to the unified OLC Editor Framework.                          *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"
#include "../common.h"
#include "../common/olc_editor.h"
#include "../common/olc_display.h"
#include "../common/olc_commands.h"

/***************************************************************************
 * Change Tracking                                                         *
 ***************************************************************************/

/**
 * pedit_mark_changed - Mark projects as needing save
 *
 * Sets the global projects_changed flag so the project list
 * is saved on the next area save cycle.
 *
 * @param ch     Character who made the change
 * @param pEdit  PROJECT_DATA being edited
 */
static void pedit_mark_changed(CHAR_DATA *ch, void *pEdit)
{
    (void)ch;
    (void)pEdit;
    projects_changed = true;
}

/***************************************************************************
 * Command Table                                                           *
 ***************************************************************************/

const struct olc_cmd_type pedit_table[] =
{
    { "?",          show_help       },
    { "area",       pedit_area      },
    { "builder",    pedit_builder   },
    { "commands",   show_commands   },
    { "completed",  pedit_completed },
    { "create",     pedit_create    },
    { "description",pedit_description },
    { "leader",     pedit_leader    },
    { "name",       pedit_name      },
    { "pflag",      pedit_pflag     },
    { "security",   pedit_security  },
    { "show",       pedit_show      },
    { "summary",    pedit_summary   },
    { NULL,         0               }
};

/***************************************************************************
 * Editor Definition                                                       *
 ***************************************************************************/

static const OLC_EDITOR_DEF pedit_def = {
    .name           = "PEdit",
    .editor_type    = ED_PROJECT,
    .cmd_table      = pedit_table,
    .show_fn        = pedit_show,
    .tabs           = { .count = 0 },
    .theme          = &olc_theme_data,
    .perm           = {
        .flags          = OLC_PERM_STAFF_RANK,
        .min_staff_rank = STAFF_IMPLEMENTOR
    },
    .change_mode    = OLC_CHANGE_CUSTOM,
    .mark_changed_fn = pedit_mark_changed,
    .audit_changes  = false,
    .get_history_fn = NULL,
};

/***************************************************************************
 * Entry Point                                                             *
 ***************************************************************************/

/**
 * do_pedit - Enter the project editor
 *
 * Syntax:
 *   pedit <project #>      - Edit project by number
 *   pedit <project name>   - Edit project by name
 *   pedit create            - Create a new project
 */
void do_pedit(CHAR_DATA *ch, char *argument)
{
    PROJECT_DATA *project;
    int value;
    int i;
    char arg[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
        return;

    if (!olc_editor_check_perm(ch, &pedit_def, NULL)) {
        send_to_char("PEdit: Insufficient security to edit projects.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Syntax: pedit <project #|project name>\n\r", ch);
        return;
    }

    if (is_number(arg)) {
        value = atoi(arg);
        for (project = project_list, i = 0; project != NULL; project = project->next, i++) {
            if (i == value)
                break;
        }

        if (project == NULL) {
            send_to_char("Project number not found.\n\r", ch);
            return;
        }
    } else if (!str_cmp(arg, "create")) {
        if (pedit_create(ch, ""))
            olc_editor_enter(ch, &pedit_def, ch->desc->pEdit, false);
        return;
    } else {
        for (project = project_list; project != NULL; project = project->next) {
            if (!str_infix(arg, project->name))
                break;
        }

        if (project == NULL) {
            send_to_char("Project not found.\n\r", ch);
            return;
        }
    }

    olc_editor_enter(ch, &pedit_def, project, false);
}

/***************************************************************************
 * Interpreter                                                             *
 ***************************************************************************/

/**
 * pedit - Command interpreter for the project editor
 */
void pedit(CHAR_DATA *ch, char *argument)
{
    olc_editor_interp(ch, argument, &pedit_def);
}

/***************************************************************************
 * Commands                                                                *
 ***************************************************************************/

PEDIT(pedit_create)
{
    PROJECT_DATA *project;
    PROJECT_DATA *project_tmp;

    if (ch->tot_level < MAX_LEVEL) {
    send_to_char("Currently, only Implementors can create projects.\n\r", ch);
    return false;
    }

    project = new_project();
    for (project_tmp = project_list; project_tmp != NULL; project_tmp = project_tmp->next) {
    if (project_tmp->next == NULL)
        break;
    }

    if (project_tmp != NULL)
    project_tmp->next = project;
    else
    project_list = project;

    ch->desc->pEdit     =   (void *)project;

    project->created = current_time;
    projects_changed = true;
    send_to_char("Project Created.\n\r", ch);
    return false;
}


/**
 * pedit_show - Display current project data
 *
 * Uses the unified OLC display framework for consistent formatting.
 * Called both from within the editor and from "project show <name>".
 *
 * @param ch        Character viewing
 * @param argument  Unused
 * @return          false (no data changed)
 */
PEDIT(pedit_show)
{
    PROJECT_DATA *project;
    PROJECT_BUILDER_DATA *pb;
    STRING_DATA *string;
    const OLC_EDITOR_THEME *theme = olc_get_theme(&pedit_def);
    OLC_LAYOUT_CTX *ctx;
    char areas[MSL], time_str[MSL];
    int i;
    long total_time;

    EDIT_PROJECT(ch, project);

    ctx = olc_display_new(ch, theme);

    /* Count project number for ID */
    i = 0;
    {
        PROJECT_DATA *p;
        for (p = project_list; p != NULL; p = p->next, i++) {
            if (p == project)
                break;
        }
    }

    olc_display_header(ctx, "PEdit", project->name,
        formatf("#%d", i), &pedit_def);

    olc_display_string(ctx, theme, "Name:", "name", project->name);

    /* Build area list string */
    areas[0] = '\0';
    for (i = 0, string = project->areas; string != NULL; string = string->next, i++) {
        if (i > 0)
            strcat(areas, ", ");
        strncat(areas, string->string, sizeof(areas) - strlen(areas) - 1);
    }
    olc_display_string(ctx, theme, "Area(s):", "area",
        areas[0] ? areas : NULL);

    olc_display_string(ctx, theme, "Leader:", "leader", project->leader);
    olc_display_number(ctx, theme, "Security:", "security", project->security);
    olc_display_flags(ctx, theme, "Flags:", "pflag",
        project_flags, project->project_flags);
    olc_display_string(ctx, theme, "Created:", NULL,
        (char *)ctime(&project->created));
    olc_display_string(ctx, theme, "Summary:", "summary", project->summary);

    olc_display_section(ctx, theme, "Progress");

    /* Build completion bar */
    {
        char completed[MSL];
        completed[0] = '\0';
        for (i = 0; i < project->completed; i += 4) {
            if (i > 80)
                strcat(completed, "{G=");
            else if (i > 60)
                strcat(completed, "{g=");
            else if (i > 40)
                strcat(completed, "{Y=");
            else if (i > 20)
                strcat(completed, "{r=");
            else
                strcat(completed, "{R=");
        }
        strcat(completed, "{x");

        olc_display_string(ctx, theme, "Completed:", "completed",
            formatf("{W%3d%%{x %s", project->completed, completed));
    }

    total_time = get_total_minutes(project);
    if (total_time % 60 == 0)
        snprintf(time_str, sizeof(time_str), "%ld hrs", total_time / 60);
    else
        snprintf(time_str, sizeof(time_str), "%ld hrs %ld min",
            total_time / 60, total_time % 60);
    olc_display_string(ctx, theme, "Build Time:", NULL, time_str);

    olc_display_section(ctx, theme, "Builders");

    for (i = 0, pb = project->builders; pb != NULL; pb = pb->next, i++) {
        if (pb->minutes % 60 == 0)
            snprintf(time_str, sizeof(time_str), "%ld hrs", pb->minutes / 60);
        else
            snprintf(time_str, sizeof(time_str), "%ld hrs %ld min",
                pb->minutes / 60, pb->minutes % 60);

        olc_display_infof(ctx, theme,
            "{g[{G%3d{g]{x %-15s %-15s %s",
            i, pb->name, time_str, (char *)ctime(&pb->assigned));
    }

    if (project->builders == NULL)
        olc_display_infof(ctx, theme, "  No builders.");

    olc_display_text(ctx, theme, "Description:", "description",
        project->description);

    olc_display_footer(ctx, theme);

    page_to_char(buf_string(ctx->buffer), ch);
    olc_layout_free(ctx);

    show_project_inquiries(project, ch);

    return false;
}
/**
 * pedit_name - Set the project name
 */
PEDIT(pedit_name)
{
    PROJECT_DATA *project;
    EDIT_PROJECT(ch, project);
    return olc_cmd_string(ch, argument, "Name", NULL, &project->name,
        OLC_STR_DEFAULT | OLC_STR_UTF8_RESTRICT, NULL, NULL);
}


/**
 * pedit_security - Set the project security level (0-9)
 */
PEDIT(pedit_security)
{
    PROJECT_DATA *project;
    EDIT_PROJECT(ch, project);
    return olc_cmd_number(ch, argument, "Security", NULL,
        &project->security, 0, 9, NULL, NULL);
}


/**
 * pedit_pflag - Toggle project flags
 */
PEDIT(pedit_pflag)
{
    PROJECT_DATA *project;
    EDIT_PROJECT(ch, project);
    return olc_cmd_flag_toggle(ch, argument, "Flags", NULL,
        &project->project_flags, project_flags, NULL, NULL);
}


/* Add or remove builders from a project in a toggle style fashion. */
PEDIT(pedit_builder)
{
    PROJECT_DATA *project;
    PROJECT_BUILDER_DATA *pb, *pb_prev, *pb_tmp;
    char arg[MSL];
    bool found = false;

    EDIT_PROJECT(ch, project);

    argument = one_argument(argument, arg);
    if (arg[0] == '\0') {
    send_to_char("Syntax:  builder [immortal name]\n\r", ch);
    return false;
    }

    /* taking a builder off */
    pb_prev = NULL;
    for (pb = project->builders; pb != NULL; pb = pb->next) {
    if (!str_cmp(pb->name, arg)) {
        found = true;
        break;
    }

    pb_prev = pb;
    }

    if (found)
    {
    if (pb_prev == NULL)
        project->builders = pb->next;
    else
        pb_prev->next = pb->next;

    act("Builder $t removed.", ch, NULL, NULL, NULL, NULL, pb->name, NULL, TO_CHAR, NULL, NULL);
    free_project_builder(pb);
    }
    else
    {
        if (find_immortal(arg) == NULL) {
        send_to_char("That immortal doesn't exist.\n\r", ch);
        return false;
    }

    pb = new_project_builder();
    arg[0] = UPPER(arg[0]);
    pb->name = str_dup(arg);
    pb->assigned = current_time;

    for (pb_tmp = project->builders; pb_tmp != NULL; pb_tmp = pb_tmp->next) {
        if (pb_tmp->next == NULL)
        break;
    }

    if (project->builders == NULL)
        project->builders = pb;
    else {
        pb_tmp->next = pb;
        pb->project = project;
    }

    act("Builder $t added.", ch, NULL, NULL, NULL, NULL, pb->name, NULL, TO_CHAR, NULL, NULL);
    }


    return true;
}


/* Toggle progress level of a project. Can only be controlled by the leader and other high lvl imms. */
PEDIT(pedit_completed)
{
    PROJECT_DATA *project;
    int val;

    EDIT_PROJECT(ch, project);

    if (argument[0] == '\0' || !is_number(argument)) {
    send_to_char("Syntax:  completed [%%completed]\n\r", ch);
    return false;
    }

    if ((val = atoi(argument)) < 0 || val > 100) {
    send_to_char("Valid completion is 0-100%.\n\r", ch);
    return false;
    }

    if (ch->tot_level < MAX_LEVEL && str_cmp(ch->name, project->leader)) {
    send_to_char("Sorry, only the project leader, the head builder, or an implementor can adjust project completion status.\n\r", ch);
    return false;
    }

    project->completed = val;
    send_to_char("Project % completed set.\n\r", ch);

    return true;
}


/* Change the person assigned as the leader of a project. */
PEDIT(pedit_leader)
{
    PROJECT_DATA *project;
    char arg[MSL];

    EDIT_PROJECT(ch, project);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
    send_to_char("Syntax:  leader [immortal name]\n\r", ch);
    return false;
    }

    if (get_char_world(NULL, arg) == NULL && !player_exists(arg)) {
    send_to_char("Immortal not found.\n\r", ch);
    return false;
    }

    free_string(project->leader);
    arg[0] = UPPER(arg[0]);
    project->leader = str_dup(arg);

    act("Project leader set to $t.", ch, NULL, NULL, NULL, NULL, project->leader, NULL, TO_CHAR, NULL, NULL);
    return true;
}


/**
 * pedit_description - Edit the project description (opens string editor)
 */
PEDIT(pedit_description)
{
    PROJECT_DATA *project;
    EDIT_PROJECT(ch, project);
    return olc_cmd_string_append(ch, argument, "Description", NULL,
        &project->description, NULL, NULL);
}


/**
 * pedit_summary - Set the project summary line
 */
PEDIT(pedit_summary)
{
    PROJECT_DATA *project;
    EDIT_PROJECT(ch, project);
    return olc_cmd_string(ch, argument, "Summary", NULL, &project->summary,
        OLC_STR_DEFAULT, NULL, NULL);
}


/* Add or remove associated areas to a project. */
PEDIT(pedit_area)
{
    PROJECT_DATA *project;
    AREA_DATA *area;
    STRING_DATA *string, *string_last;
    char arg[MSL];

    EDIT_PROJECT(ch, project);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
    send_to_char("Syntax:  area [area name]\n\r", ch);
    return false;
    }

    string_last = NULL;
    for (string = project->areas; string != NULL; string = string->next) {
    if (!str_infix(arg, string->string))
        break;

    string_last = string;
    }

    /* Found in list of areas, remove */
    if (string != NULL)
    {
    if (!string_last)
       project->areas = string->next;
    else
       string_last->next = string->next;

    act("Area $t removed.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR, NULL, NULL);

    free_string_data(string);
    }
    else /* Not found, add to area list */
    {
    for (area = area_first; area != NULL; area = area->next) {
        if (!str_infix(arg, area->name))
        break;
    }

    if (area == NULL) {
        send_to_char("Area not found.\n\r", ch);
        return false;
    }

    /* Add it to the end of the list */
    for (string_last = project->areas; string_last != NULL; string_last = string_last->next) {
        if (string_last->next == NULL)
        break;
    }

    string = new_string_data();
    string->string = str_dup(area->name);
    if (string_last)
        string_last->next = string;
    else
        project->areas = string;

    act("Area $t added.", ch, NULL, NULL, NULL, NULL, area->name, NULL, TO_CHAR, NULL, NULL);
    }

    return true;
}