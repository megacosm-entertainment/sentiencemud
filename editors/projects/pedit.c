/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

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


/* Show one project - this is called not only within the OLC editor but also from do_project
   with argument "show [project]" */
PEDIT(pedit_show)
{
    PROJECT_DATA *project;
    PROJECT_BUILDER_DATA *pb;
    STRING_DATA *string;
    char buf[2*MSL], areas[MSL], buf2[MSL], completed[MSL], time[MSL];
    int i;
    long total_time;

    EDIT_PROJECT(ch, project);

    sprintf(buf, "Name:                {g[{x%-30.30s{g]{x\n\r", project->name);
    send_to_char(buf, ch);

    sprintf(areas, "No areas");
    for (i = 0, string = project->areas; string != NULL; string = string->next, i++) {
        if (i == 0)
		    sprintf(areas, "%s", string->string);
		else {
		    sprintf(buf2, ", %s", string->string);
		    strcat(areas, buf2);
		}
    }

    sprintf(buf, "Area(s):             {g[{x%-30.30s{g]{x\n\r", areas);
    send_to_char(buf, ch);

    sprintf(buf, "Project leader:      {g[{x%-30.30s{g]{x\n\r", project->leader);
    send_to_char(buf, ch);

    sprintf(buf, "Security:            {g[{x%-30d{g]{x\n\r", project->security);
    send_to_char(buf, ch);

    sprintf(buf, "Project flags:       {g[{x%-30s{g]{x\n\r", flag_string(project_flags, project->project_flags));
    send_to_char(buf, ch);

    sprintf(buf, "Created:             {x%s", (char *) ctime(&project->created));
    send_to_char(buf, ch);

    sprintf(buf, "Summary:             %s\n\r", project->summary);
    send_to_char(buf, ch);

    sprintf(buf, "Description:         \n\r%s\n\r", project->description);
    send_to_char(buf, ch);

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


    total_time = get_total_minutes(project);
    if (total_time % 60 == 0)
	sprintf(time, "%ld hrs", total_time/60);
    else
	sprintf(time, "%ld hrs %ld min", total_time/60, total_time % 60);
    sprintf(buf, "Total building time: {g[{x%-30s{g]{x\n\r", time);
    send_to_char(buf, ch);

    sprintf(buf, "Completion status:   {g[{W%3d%%{g]{x %-30s\n\r", project->completed, completed);
    send_to_char(buf, ch);

    sprintf(buf, "\n\r{G%-5s %-15s %-15s %s{x\n\r", "#", "Builder", "Time", "Date started" );
    send_to_char(buf, ch);


    sprintf(buf, "{g------------------------------------------------------------------------------------------------------{x\n\r");
    send_to_char(buf, ch);
    for (i = 0, pb = project->builders; pb != NULL; pb = pb->next, i++) {
	// Figure out time string
	if (pb->minutes % 60 == 0)
	    sprintf(time, "%ld hrs", pb->minutes/60);
	else
	    sprintf(time, "%ld hrs %ld min", pb->minutes/60, pb->minutes % 60);

	sprintf(buf, "{g[{G%3d{g] {x%-15s %-15s %s", i, pb->name, time, (char *) ctime(&pb->assigned));
	send_to_char(buf, ch);
    }

    if (project->builders == NULL) {
	sprintf(buf, "No builders.\n\r");
	send_to_char(buf, ch);
    }

    show_project_inquiries(project, ch);


    return false;
}
/* Change the name of a project. */
PEDIT(pedit_name)
{
    PROJECT_DATA *project;

    EDIT_PROJECT(ch, project);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  name [name]\n\r", ch);
	return false;
    }

    free_string(project->name);
    project->name = str_dup(argument);
    send_to_char("Project name set.\n\r", ch);
    return true;
}


/* Change security of a project. */
PEDIT(pedit_security)
{
    PROJECT_DATA *project;
    int sec;

    EDIT_PROJECT(ch, project);

    sec = atoi(argument);
    if (!is_number(argument) || argument[0] == '\0' || sec < 0 || sec > 9) {
	send_to_char("Syntax:  security [0-9]\n\r", ch);
	return false;
    }

    project->security = sec;
    send_to_char("Project security set.\n\r", ch);
    return true;
}


/* Toggle various project flags. */
PEDIT(pedit_pflag)
{
    PROJECT_DATA *project;
    int value;

    EDIT_PROJECT(ch, project);

    if (argument[0] != '\0')
    {
	if ((value = flag_value(project_flags, argument)) != NO_FLAG)
	{
	    TOGGLE_BIT(project->project_flags, value);

	    send_to_char("Project flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  pflag [flag]\n\r"
	    "Type '? projectflags' for a list of flags.\n\r", ch);
    return false;
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

	act("Builder $t removed.", ch, NULL, NULL, NULL, NULL, pb->name, NULL, TO_CHAR);
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
	else
	    pb_tmp->next = pb;

        pb->project = project;

	act("Builder $t added.", ch, NULL, NULL, NULL, NULL, pb->name, NULL, TO_CHAR);
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

    act("Project leader set to $t.", ch, NULL, NULL, NULL, NULL, project->leader, NULL, TO_CHAR);
    return true;
}


/* Change the description (in-depth description) of a project. */
PEDIT(pedit_description)
{
    PROJECT_DATA *project;

    EDIT_PROJECT(ch, project);

    string_append(ch, &project->description);

    return true;
}


/* Change the brief, one-line summary of a project - this is displayed with "project list". */
PEDIT(pedit_summary)
{
    PROJECT_DATA *project;

    EDIT_PROJECT(ch, project);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  summary [string]\n\r", ch);
	return false;
    }

    free_string(project->summary);
    project->summary = str_dup(argument);

    send_to_char("Project summary set.\n\r", ch);
    return true;
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

	act("Area $t removed.", ch, NULL, NULL, NULL, NULL, string->string, NULL, TO_CHAR);

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

	act("Area $t added.", ch, NULL, NULL, NULL, NULL, area->name, NULL, TO_CHAR);
    }

    return true;
}