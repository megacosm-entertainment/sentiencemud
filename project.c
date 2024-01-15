/* This file contains the OLC act commands as well as various other functions
   associated with the project management system. All source copyright Anton Ouzilov,
   2007. */

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
#include "merc.h"
#include "interp.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"


/* Global variables */
PROJECT_DATA		*project_list;		// List of projects.
PROJECT_INQUIRY_DATA	*project_inquiry_list; 	// List of inquiries.

/* This macro is used in more deeply nested project functions (e.g. do_pinquiry) to
   make sure we never forget to free our buffer before returning. */
#define EXIT_PROJECT_FUNCTION		page_to_char(buf_string(buffer), ch); \
                                        free_buf(buffer); \
                                        return;


/* General-use project handler. Can be called by any immortal, although some functions
   are only accessible to imps or project leaders. */
void do_project(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];

    argument = one_argument(argument, arg);

    if (arg[0] != '\0')
    {
	/* list projects */
	if (!str_prefix(arg, "list")) /* Project list [status] */
	    do_function(ch, &do_plist, argument);
	else /* Project show [project] */
	if (!str_prefix(arg, "show"))
	    do_function(ch, &do_pshow, argument);
	else /* Project inquiry [project] [list|add|view #|edit #|close #] */
	if (!str_prefix(arg, "inquiry"))
	    do_function(ch, &do_pinquiry, argument);
	else /* Project delete [project] wipes it out */
	if (!str_prefix(arg, "delete"))
	    do_function(ch, &do_pdelete, argument);
    }
    else
    {
	send_to_char("Syntax:  project list\n\r"
		     "         project show [project]\n\r"
		     "         project inquiry [project] [list|add|view #|edit #|reply #|close #]\n\r", ch);
	if (ch->tot_level >= MAX_LEVEL - 1)
	    send_to_char("         project inquiry [project] [close #|open #|delete #]\n\r", ch);

	if (ch->tot_level == MAX_LEVEL)
	    send_to_char("         project delete [project]", ch);

    }

}


/* Shows a character a list of all projects he/she has access to. */
void do_plist(CHAR_DATA *ch, char *argument)
{
    char buf[2*MSL];
    char status[MSL];
    int i;
    BUFFER *buffer;
    PROJECT_DATA *project;

    buffer = new_buf();

    sprintf(buf, "{G%-5s %-20.20s %-40.40s %-12s %-24s %-9s\n\r", "#", "Project name", "Summary", "Leader", "Status", "Completion");
    add_buf(buffer, buf);
    add_buf(buffer, "{g--------------------------------------------------------------------------------------------------------------------{x\n\r");

    i = 0;
    for (project = project_list; project != NULL; project = project->next)
    {
	if (!can_view_project(ch, project))
	    continue;

	if ((!str_cmp(argument, "open") 	&& !IS_SET(project->project_flags, PROJECT_OPEN))
	||  (!str_cmp(argument, "closed") 	&& IS_SET(project->project_flags, PROJECT_OPEN))
	||  (!str_cmp(argument, "assigned") 	&& !IS_SET(project->project_flags, PROJECT_ASSIGNED))
	||  (!str_cmp(argument, "unassigned") 	&& IS_SET(project->project_flags, PROJECT_ASSIGNED))
	||  (!str_cmp(argument, "hold") 	&& !IS_SET(project->project_flags, PROJECT_HOLD)))
	    continue;


	/* Figure out status first */
	if (IS_SET(project->project_flags, PROJECT_OPEN))
	    sprintf(status, "Open");
	else
	    sprintf(status, "Closed");

	if (IS_SET(project->project_flags, PROJECT_ASSIGNED))
	    strcat(status, " Assigned");
	else
	    strcat(status, " Unnassigned");

	if (IS_SET(project->project_flags, PROJECT_HOLD))
	    strcat(status, " Hold");

	sprintf(buf, "{g[{G%3d{g] {x%-20.20s %-40.40s %-12s %-24s {%c%-9d\n\r",
		i,
		project->name,
		project->summary,
		project->leader,
		status,
		project->completed > 66 ? 'W' :
		project->completed > 33 ? 'Y' :
		'R',
		project->completed);
	add_buf(buffer, buf);
	i++;
    }

    if (project_list == NULL)
	add_buf(buffer, "No projects found.\n\r");
    add_buf(buffer, "{g--------------------------------------------------------------------------------------------------------------------{x\n\r");

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/* Shows one project and a list of its builders and inquiries to a character. */
void do_pshow(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], arg2[MSL];
    BUFFER *buffer;
    PROJECT_DATA *project;

    argument = one_argument(argument, arg);

    buffer = new_buf();

    if (arg[0] == '\0')
	add_buf(buffer, "Syntax:  project show [project #|project name]\n\r");
    else
    {
	if ((project = find_project(arg)) == NULL || !can_view_project(ch, project))
	    add_buf(buffer, "Project not found.\n\r");
	else
	{
		olc_show_item(ch, project, pedit_show, arg2);
	}
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/* Deletes a project. Use caution. */
void do_pdelete(CHAR_DATA *ch, char *argument)
{
    PROJECT_DATA *project, *project_tmp, *project_last = NULL;

    if ((project = find_project(argument)) == NULL) {
	send_to_char("That project does not exist.\n\r", ch);
	return ;
    }

    if (!can_edit_project(ch, project)) {
	send_to_char("You are not authorized to delete projects.\n\t", ch);
	return;
    }

    for (project_tmp = project_list; project_tmp != NULL; project_tmp = project_tmp->next) {
        if (project_tmp == project)
	    break;

	project_last = project_tmp;
    }

    if (project_last == NULL) {
	project_list = project_list->next;
	free_project(project_tmp);
    }
    else {
	project_last->next = project->next;
	free_project(project_tmp);
    }

    send_to_char("Project deleted.\n\r", ch);
}

/* Handles inquiries on objects. */
void do_pinquiry(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], arg2[MSL], arg3[MSL];
    char buf[MSL];
    BUFFER *buffer;
    PROJECT_DATA *project;
    PROJECT_INQUIRY_DATA *pinq, *pinq_tmp, *reply;

    buffer = new_buf();

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (!str_cmp(arg, "read"))
    {
	pinq = NULL;

	if (count_project_inquiries(ch) == 0)
	    add_buf(buffer, "No new inquiries.\n\r");

	show_oldest_unread_inquiry(ch);

	EXIT_PROJECT_FUNCTION;
    }

    if (arg[0] == '\0')
    {
	add_buf(buffer, "Syntax:  project inquiry [project] [list|add [subject]|view #|edit #|reply #|open #]\n\r");
	EXIT_PROJECT_FUNCTION;
    }

    if ((project = find_project(arg)) == NULL /*|| !has_access_project(ch, project)*/)
    {
	add_buf(buffer, "Project not found.\n\r");
	EXIT_PROJECT_FUNCTION;
    }

    if (arg2[0] == '\0' || !str_cmp(arg2, "list"))
    {
	show_project_inquiries(project, ch);

	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "add"))
    {
	pinq = new_project_inquiry();
	pinq->date = current_time;
	pinq->sender = str_dup(ch->name);
	pinq->project = project;

	/* add to end of list */
	for (pinq_tmp = project->inquiries; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next) {
	    if (pinq_tmp->next == NULL)
		break;
	}

	if (project->inquiries == NULL)
	    project->inquiries = pinq;
	else
	    pinq_tmp->next = pinq;

	if (project_inquiry_list == NULL)
	    project_inquiry_list = pinq;
	else
	{
	    pinq->next_global = project_inquiry_list;
	    project_inquiry_list = pinq;
	}

	ch->pcdata->inquiry_subject = pinq; //Set the "enter subject" prompt
	add_buf(buffer, "Enter inquiry subject: ");
	EXIT_PROJECT_FUNCTION;
    }

    if (arg3[0] == '\0')
    {
	add_buf(buffer, "Syntax:  project inquiry read\n\r"
		        "         project inquiry [project] [list|add|view #|edit #|reply #]\n\r");
	add_buf(buffer, "         project inquiry [project] [close #|open #|delete #]\n\r");
	EXIT_PROJECT_FUNCTION;
    }

    /* All subcommands past this point require an inquiry argument, so let's find it */
    if ((pinq = find_project_inquiry(project, arg3)) == NULL)
    {
	add_buf(buffer, "Project inquiry not found.\n\r");
	EXIT_PROJECT_FUNCTION;
    }

    if (arg2[0] == '\0' || !str_cmp(arg2, "view") || !str_cmp(arg2, "show"))
    {
	show_project_inquiry(pinq, ch);
	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "edit"))
    {
	if (str_cmp(ch->name, pinq->sender))
	{
	    add_buf(buffer, "Only the poster of an inquiry can edit it.\n\r");
	    EXIT_PROJECT_FUNCTION;
	}

	send_to_char("Editing inquiry.\n\r",ch);
	string_append(ch, &pinq->text);
	projects_changed = true;
	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "close"))
    {
	if (ch->tot_level < MAX_LEVEL - 1 && str_cmp(ch->name, pinq->project->leader))
	{
	    add_buf(buffer, "Only the project leader or your group leader can close an inquiry on this project.\n\r");
	    EXIT_PROJECT_FUNCTION;
	}

	pinq->closed = current_time;
	pinq->closed_by = str_dup(ch->name);
	send_to_char("Inquiry closed.\n\r", ch);
	projects_changed = true;
	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "open"))
    {
	if (!pinq->closed)
	{
	    add_buf(buffer, "That inquiry isn't closed.\n\r");
	    EXIT_PROJECT_FUNCTION;
	}

	if (ch->tot_level < MAX_LEVEL - 1 && str_cmp(ch->name, pinq->project->leader))
	{
	    add_buf(buffer, "Only the project leader or your group leader can close an inquiry on this project.\n\r");
	    EXIT_PROJECT_FUNCTION;
	}

	pinq->closed = 0;
	free_string(pinq->closed_by);
	pinq->closed_by = str_dup("N/A");
	add_buf(buffer, "Inquiry opened.\n\r");
	projects_changed = true;
	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "reply"))
    {
	/* if (!has_access_project(ch, project)) {
	   send_to_char("You must be a builder on that project to reply to inquiries within it.\n\r", ch);
	   return;
	   }*/
	if (pinq->closed)
	{
	    sprintf(buf, "That inquiry has been closed by %s.\n\r", pinq->closed_by);
	    add_buf(buffer, buf);
	    EXIT_PROJECT_FUNCTION;
	}

	reply = new_project_inquiry();
	reply->sender = str_dup(ch->name);
	sprintf(buf, "Re: %s", pinq->subject);
	reply->subject = str_dup(buf);
	reply->parent = pinq;

	for (pinq_tmp = pinq->replies; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next)
	{
	    if (pinq_tmp->next == NULL)
		break;
	}

	if (pinq_tmp)
	    pinq_tmp->next = reply;
	else
	    pinq->replies = reply;

	add_buf(buffer, "Posting reply.\n\r");
	string_append(ch, &reply->text);

	projects_changed = true;
	EXIT_PROJECT_FUNCTION;
    }

    if (!str_cmp(arg2, "delete"))
    {
	if (ch->tot_level < MAX_LEVEL)
	{
	    add_buf(buffer, "Only an implementor can delete project inquiries.\n\r");
	    EXIT_PROJECT_FUNCTION;
	}

	extract_project_inquiry(pinq);

	add_buf(buffer, "Inquiry deleted.\n\r");
	projects_changed = true;
	EXIT_PROJECT_FUNCTION;
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/* Start building in a project. Your time will be logged as long as you perform OLC
   commands. */
void do_build(CHAR_DATA *ch, char *argument)
{
    //char buf[MSL];
    PROJECT_DATA *project;
    PROJECT_BUILDER_DATA *pb;

    if (IS_NPC(ch)) {
	bug("do_build: NPC", 0);
	return;
    }

    if (argument[0] == '\0') {
	if (IS_SET(ch->act[0], PLR_BUILDING)) {
	    if (IS_SET(ch->act[0], PLR_BUILDING) && ch->pcdata->immortal->build_project != NULL)
		act("You stop building in $t.", ch, NULL, NULL, NULL, NULL, ch->pcdata->immortal->build_project->name, NULL, TO_CHAR);
	    REMOVE_BIT(ch->act[0], PLR_BUILDING);
	    ch->pcdata->immortal->build_project = NULL;
	    ch->pcdata->immortal->builder = NULL;
	    return;
	}

	send_to_char("Syntax: build [project]\n\r", ch);
	return;
    }

    if ((project = find_project(argument)) == NULL
    ||   !can_view_project(ch, project)) {
	send_to_char("No such project.\n\r", ch);
	return;
    }

    if ((pb = find_project_builder(project, ch->name)) == NULL) {
	act("You aren't a builder on project $t.", ch, NULL, NULL, NULL, NULL, project->name, NULL, TO_CHAR);
	return;
    }

    if (IS_SET(ch->act[0], PLR_BUILDING) && ch->pcdata->immortal->build_project->name != NULL)
	act("You stop building in $t.", ch, NULL, NULL, NULL, NULL, ch->pcdata->immortal->build_project->name, NULL, TO_CHAR);

    ch->pcdata->immortal->build_project = project;

    ch->pcdata->immortal->builder = pb;
    ch->pcdata->immortal->last_olc_command = current_time;

    SET_BIT(ch->act[0], PLR_BUILDING);
    act("You start building in $t.", ch, NULL, NULL, NULL, NULL, project->name, NULL, TO_CHAR);
}


/* Finds a project from either a numerical or string argument. */
PROJECT_DATA *find_project(char *argument)
{
    PROJECT_DATA *project;
    int i;
    int val = -1;

    if (is_number(argument))
	val = atoi(argument);

    for (i = 0, project = project_list; project != NULL; project = project->next, i++)
    {
	if ((val != -1 && i == val) || (argument[0] != '\0' && !str_infix(argument, project->name)))
	    break;
    }

    return project;
}


/* Show a project inquiry and all of its replies to a character. */
void show_project_inquiry(PROJECT_INQUIRY_DATA *pinq, CHAR_DATA *ch)
{
    char buf[MSL];
    BUFFER *buffer;
    PROJECT_INQUIRY_DATA *reply;
    int i;

    buffer = new_buf();

    sprintf(buf,
	    "{GProject:          {x%s\n\r"
	    "{GPosted by:        {x%s\n\r"
	    "{GSubject:          {x%s\n\r"
	    "{GDate posted:      {x%s"
	    "{G------------------------------------------------------------------------{x\n\r"
	    "%s",
	    pinq->project->name,
	    pinq->sender,
	    pinq->subject,
	    (char *)ctime(&pinq->date), pinq->text);
    add_buf(buffer, buf);
    add_buf(buffer, "{G------------------------------------------------------------------------{x\n\r");

    /* Show replies */
    for (i = 0, reply = pinq->replies; reply != NULL; reply = reply->next, i++) {
	sprintf(buf, "{g[{G%3d{g]{x %-12s: %-27.27s %20s{x\n\r",
		i, reply->sender, reply->subject, (char *) ctime(&reply->date));
	add_buf(buffer, buf);

	add_buf(buffer, reply->text);
	add_buf(buffer, "{G------------------------------------------------------------------------{x\n\r");
    }

    if (pinq->closed) {
	sprintf(buf, "{GInquiry closed:{x %-31s %20s", pinq->closed_by, (char *) ctime(&pinq->closed));
	add_buf(buffer, buf);
	add_buf(buffer, "{G------------------------------------------------------------------------{x\n\r");
    }

    EXIT_PROJECT_FUNCTION;
}



/* Finds a builder within a project from either a numerical or name argument. */
PROJECT_BUILDER_DATA *find_project_builder(PROJECT_DATA *project, char *argument)
{
    PROJECT_BUILDER_DATA *pb;
    int i;
    int val = -1;

    if (is_number(argument))
	val = atoi(argument);

    for (i = 0, pb = project->builders; pb != NULL; pb = pb->next, i++) {
	if ((val != -1 && i == val) || (argument[0] != '\0' && !str_cmp(argument, pb->name)))
	    break;
    }

    return pb;
}


/* Finds an inquiry within a project - numerical argument only. */
PROJECT_INQUIRY_DATA *find_project_inquiry(PROJECT_DATA *project, char *argument)
{

    PROJECT_INQUIRY_DATA *pinq;
    int i;
    int val = -1;

    val = atoi(argument);

    for (i = 0, pinq = project->inquiries; pinq != NULL; pinq = pinq->next, i++) {
	if (val != -1 && i == val)
	    break;
    }

    return pinq;
}


/* Find the oldest UNREAD inquiry amongst all of a char's projects, and show it to him/her.
   This ensures that the oldest inquiries are read first, and then the most recent ones.
   "get_last_post() function call makes sure that you get a prompt when either a new inquiry
   is posted or a reply to it is posted.
 */
void show_oldest_unread_inquiry(CHAR_DATA *ch)
{
    PROJECT_INQUIRY_DATA *pinq, *reply;
    PROJECT_INQUIRY_DATA *pinq_oldest_unread = NULL;

    for (pinq = project_inquiry_list; pinq != NULL; pinq = pinq->next_global) {
	if (/*has_access_project(ch, pinq->project)*/true)
	{
	    reply = get_last_post(pinq);
	    if (reply->date > ch->pcdata->last_project_inquiry
	    && !(pinq_oldest_unread != NULL && pinq->date > pinq_oldest_unread->date)
	    && str_cmp(ch->name, reply->sender))
		pinq_oldest_unread = reply;
	}
    }

    if (pinq_oldest_unread != NULL)
    {
	/* For a reply, show the whole parent thread */
	if (pinq_oldest_unread->parent != NULL)
	    show_project_inquiry(pinq_oldest_unread->parent, ch);
	else
	    show_project_inquiry(pinq_oldest_unread, ch);

	ch->pcdata->last_project_inquiry =
	    UMAX(pinq_oldest_unread->date, ch->pcdata->last_project_inquiry);
    }
}


/* Count how many unread inquiries a char has. */
int count_project_inquiries(CHAR_DATA *ch)
{
    int i;
    PROJECT_INQUIRY_DATA *pinq, *reply;

    if (IS_NPC(ch))
	return 0;

    i = 0;
    for (pinq = project_inquiry_list; pinq != NULL; pinq = pinq->next_global) {
	if (/*has_access_project(ch, pinq->project)*/ true) {
	    reply = get_last_post(pinq);
	    if (reply->date > ch->pcdata->last_project_inquiry
	    &&  str_cmp(ch->name, reply->sender))
		i++;
	}
    }


    return i;
}


/* Get all logged building time for a project. */
long get_total_minutes(PROJECT_DATA *project)
{
    PROJECT_BUILDER_DATA *pb;
    long mins = 0;

    for (pb = project->builders; pb != NULL; pb = pb->next)
	mins += pb->minutes;

    return mins;
}


/* If an immortal has access to VIEW a project, this returns true. Currently,
   only implementors are allowed to EDIT projects, but I'm assuming this will
   be changed to accomodate project leaders soon, per Aaron's discretion.

   If you don't have access to a project, you can't access it with plist, pshow,
   and can't add/view inquiries relating to it.

   An immortal has access to view a project if:
   (1) they have at least the security of project->security OR
   (2) they are a builder on the project */
bool can_view_project(CHAR_DATA *ch, PROJECT_DATA *project)
{
    PROJECT_BUILDER_DATA *pb;

    // All 154+ can view any project
    if (ch->tot_level >= MAX_LEVEL - 1)
	return true;

    if (ch->pcdata->security >= project->security)
	return true;

    for (pb = project->builders; pb != NULL; pb = pb->next) {
	if (!str_cmp(ch->name, pb->name))
	    return true;
    }

    return false;
}


bool can_edit_project(CHAR_DATA *ch, PROJECT_DATA *project)
{
    if (ch->tot_level == MAX_LEVEL)
	return true;

    if (!str_cmp(ch->name, project->leader))
	return true;

    return false;
}


/* Show a formatted list of project inquries to a character. */
void show_project_inquiries(PROJECT_DATA *project, CHAR_DATA *ch)
{
    BUFFER *buffer;
    PROJECT_INQUIRY_DATA *inquiry;
    char buf[MSL];
    int i;
    time_t last_post;

    buffer = new_buf();

    sprintf(buf, "\n\r{G%-5s %-38.38s %-12s %-10s %-8s %-20s{x\n\r",
	    "#", "Inquiry subject", "Sender", "Status", "Replies", "Last post");
    add_buf(buffer, buf);

    sprintf(buf, "{g------------------------------------------------------------------------------------------------------{x\n\r");
    add_buf(buffer, buf);

    for (i = 0, inquiry = project->inquiries; inquiry != NULL; inquiry = inquiry->next, i++) {
        last_post = (get_last_post(inquiry))->date;
	sprintf(buf, "{g[{G%3d{g] {x%-38.38s %-12s %-10s %-8d %-20s",
		i,
		inquiry->subject,
		inquiry->sender,
		inquiry->closed ? "Closed" : "Open",
		count_replies(inquiry),
		(char *) ctime(&last_post));
	add_buf(buffer, buf);
    }

    if (project->inquiries == NULL)
    {
	sprintf(buf, "No inquiries.\n\r");
	add_buf(buffer, buf);
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/*
 * OLC commands start here
 */

/* Create a new project. */
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

	olc_set_editor(ch, ED_PROJECT, project);

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


/* Counts how many replies have been made to a project inquiry. */
int count_replies(PROJECT_INQUIRY_DATA *pinq)
{
    PROJECT_INQUIRY_DATA *pinq_tmp;
    int i;

    for (i = 0, pinq_tmp = pinq->replies; pinq_tmp != NULL; pinq_tmp = pinq_tmp->next)
	i++;

    return i;
}


/* Get the last post for an inquiry. */
PROJECT_INQUIRY_DATA *get_last_post(PROJECT_INQUIRY_DATA *pinq)
{
    PROJECT_INQUIRY_DATA *reply, *reply_latest;
    time_t latest_time;

    reply_latest = NULL;
    for (latest_time = pinq->date, reply = pinq->replies; reply != NULL; reply = reply->next) {
		if (reply->date > latest_time) {
		    latest_time = reply->date;
		    reply_latest = reply;
		}
    }

    if (reply_latest == NULL)
		return pinq; // IE the original post itself
    else
		return reply_latest; // The latest reply to the original post
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



/*
 * Functions to do saving and loading. Format is:
 *
 * #PROJECT
 * Name Project Name~
 * Leader Leader~
 * Description Description~
 * Security #~
 * ProjectFlags #~
 * Created #
 * Completed #
 * Area Plith~
 * Area Dungeon Mystica~
 *
 * #BUILDER
 * Name Syn~
 * Minutes #
 * Assigned #
 * #-BUILDER
 *
 * #INQUIRY
 * Sentby Name~
 * Subject String~
 * Text String~
 * SentTime #~
 * Closed Yes/No~
 * #-INQUIRY
 * #-PROJECT
 *
 */

/* Save all projects to a file. Called from do_asave and on system shutdown. */
void save_projects()
{
    FILE *fp;

    if ((fp = fopen(PROJECTS_FILE, "w")) == NULL) {
	bug("write_projects: couldn't open file!", 0);
	return;
    }

    log_string("project.c, save_projects - saving projects");

    if (project_list != NULL)
	save_project(fp, project_list);

    fprintf(fp, "#END\n");

    fclose(fp);

    projects_changed = false;
}


/* Save one project to a file. */
void save_project(FILE *fp, PROJECT_DATA *project)
{
    STRING_DATA *string;

    /* write recursively to preserve order */
    if (project->next != NULL)
	save_project(fp, project->next);

    fprintf(fp, "#PROJECT\n");
    fprintf(fp, "Name %s~\n", project->name);
    fprintf(fp, "Leader %s~\n", project->leader);
    fprintf(fp, "Description %s~\n", fix_string(project->description));
    fprintf(fp, "Summary %s~\n", fix_string(project->summary));
    fprintf(fp, "Security %d\n", project->security);
    fprintf(fp, "ProjectFlags %ld\n", project->project_flags);
    fprintf(fp, "Created %ld\n", (long int)project->created);
    fprintf(fp, "Completed %d\n", project->completed);

    for (string = project->areas; string != NULL; string = string->next)
	fprintf(fp, "Area %s~\n", string->string);

    if (project->builders != NULL)
	save_project_builder(fp, project->builders);

    if (project->inquiries != NULL)
	save_project_inquiry(fp, project->inquiries);

    fprintf(fp, "#-PROJECT\n");
}


/* Save a project builder data to a file.  */
void save_project_builder(FILE *fp, PROJECT_BUILDER_DATA *pb)
{
    if (pb->next != NULL)
	save_project_builder(fp, pb->next);

    fprintf(fp, "#BUILDER\n");
    fprintf(fp, "Name %s~\n", pb->name);
    fprintf(fp, "Minutes %ld\n", pb->minutes);
    fprintf(fp, "Assigned %ld\n", (long int)pb->assigned);
    fprintf(fp, "#-BUILDER\n");
}


/* Save a project inquiry to a file. */
void save_project_inquiry(FILE *fp, PROJECT_INQUIRY_DATA *pinq)
{
    if (pinq->next != NULL)
	save_project_inquiry(fp, pinq->next);

    fprintf(fp, "#INQUIRY\n");
    fprintf(fp, "SentBy %s~\n", pinq->sender);
    fprintf(fp, "Subject %s~\n", pinq->subject);
    fprintf(fp, "Text %s~\n", fix_string(pinq->text));
    fprintf(fp, "Date %ld\n", (long int)pinq->date);
    fprintf(fp, "Closed %ld\n", (long int)pinq->closed);
    fprintf(fp, "ClosedBy %s~\n", pinq->closed_by);

    /* replies */
    if (pinq->replies)
	save_project_inquiry(fp, pinq->replies);
    fprintf(fp, "#-INQUIRY\n");
}

/* These variables are used in various functions in loading/saving, so they
   are global to avoid redundancy. */
static bool fMatch;
static char *word;


/* Read projects.dat and load it into memory. */
void read_projects()
{
    FILE *fp;
    PROJECT_DATA *project;

    fp = fopen(PROJECTS_FILE, "r");
    if (fp == NULL)
    {
	bug("Couldn't read projects.dat", 0);
	exit(1);
    }

    for (;;)
    {
	word = fread_word(fp);
	if (!str_cmp(word, "#PROJECT"))
	{
	    project = read_project(fp);

	    project->next = project_list;
	    project_list = project;
	}

	if (!str_cmp(word, "#END"))
	    break;
    }

    fclose(fp);
}


/* Read one project (#PROJECT to #-PROJECT). */
PROJECT_DATA *read_project(FILE *fp)
{
    PROJECT_DATA *project;
    PROJECT_BUILDER_DATA *pb;
    PROJECT_INQUIRY_DATA *pinq;
    char buf[MSL];

    project = new_project();

    while (str_cmp((word = fread_word(fp)), "#-PROJECT"))
    {
	fMatch = false;
	switch (word[0])
	{
	    case '#':
		if (!str_cmp(word, "#BUILDER")) {
		    pb = read_project_builder(fp);

		    pb->next = project->builders;
		    project->builders = pb;
		    pb->project = project;
		    fMatch = true;
		    break;
		}

		if (!str_cmp(word, "#INQUIRY")) {
		    pinq = read_project_inquiry(fp);
		    pinq->project = project;

		    pinq->next = project->inquiries;
		    project->inquiries = pinq;

		    if (project_inquiry_list == NULL)
			project_inquiry_list = pinq;
		    else {
			pinq->next_global = project_inquiry_list;
			project_inquiry_list = pinq;
		    }

		    fMatch = true;
		    break;
		}

	    case 'A':
		if (!str_cmp(word, "Area")) {
		    STRING_DATA *string, *string_tmp;

		    fMatch = true;
		    string = new_string_data();
		    string->string = fread_string(fp);

		    if (project->areas == NULL)
			project->areas = string;
		    else
		    {
			for (string_tmp = project->areas; string_tmp != NULL; string_tmp = string_tmp->next) {
			    if (string_tmp->next == NULL)
				break;
			}

			string_tmp->next = string;
		    }
		}

		    break;

	    case 'C':
		KEY("Completed", project->completed,	fread_number(fp));
		KEY("Created",	project->created,	fread_number(fp));
		break;

	    case 'D':
		KEYS("Description",project->description, fread_string(fp));
		break;

	    case 'L':
		KEYS("Leader",	project->leader,	fread_string(fp));
		break;

	    case 'N':
		KEYS("Name",	project->name,		fread_string(fp));
		break;

	    case 'P':
		KEY("ProjectFlags", project->project_flags, fread_number(fp));
		break;

	    case 'S':
		KEY("Security",project->security,	fread_number(fp));
                KEY("Summary",	project->summary,	fread_string(fp));

		break;

	    default:
		sprintf(buf, "read_projects: no match for word %s", word);
		bug(buf, 0);
		break;
	}
    }

    sprintf(buf,"read_project: reading project %s (%s), leader %s, sec %d, flags %s",
	    project->name, project->summary, project->leader,
	    project->security, flag_string(project_flags, project->project_flags));
    log_string(buf);

    return project;
}


/* Read one project builder (#BUILDER to #-BUILDER) */
PROJECT_BUILDER_DATA *read_project_builder(FILE *fp)
{
    PROJECT_BUILDER_DATA *pb = new_project_builder();
    char buf[MSL];

    while (str_cmp((word = fread_word(fp)), "#-BUILDER"))
    {
	fMatch = false;
	switch (word[0])
	{
	    case 'A':
                KEY("Assigned", pb->assigned,	fread_number(fp));
		break;

	    case 'M':
		KEY("Minutes",	pb->minutes,	fread_number(fp));
		break;

	    case 'N':
		KEYS("Name",	pb->name,	fread_string(fp));
		break;

	    default:
		sprintf(buf, "read_project_builder: no match for word %s", word);
		bug(buf, 0);
		break;
	}
    }


    return pb;
}


/* Read one project inquiry (#INQUIRY to #-INQUIRY) */
PROJECT_INQUIRY_DATA *read_project_inquiry(FILE *fp)
{
    PROJECT_INQUIRY_DATA *pinq = new_project_inquiry();
    PROJECT_INQUIRY_DATA *reply;
    char buf[MSL];

    while (str_cmp((word = fread_word(fp)), "#-INQUIRY"))
    {
	fMatch = false;
	switch (word[0])
	{
	    case '#':
		if (!str_cmp(word, "#INQUIRY")) {
		    reply = read_project_inquiry(fp);

		    reply->next = pinq->replies;
		    pinq->replies = reply;
		    reply->parent = pinq;

		    fMatch = true;
		}

	    case 'C':
		KEY("Closed",	pinq->closed,	fread_number(fp));
		KEYS("ClosedBy",pinq->closed_by,fread_string(fp));
		break;

	    case 'D':
		KEY("Date",	pinq->date,	fread_number(fp));
		break;

	    case 'S':
		KEYS("SentBy",	pinq->sender,	fread_string(fp));
		KEY("SentTime",	pinq->date,	fread_number(fp));
		KEYS("Subject",	pinq->subject,	fread_string(fp));
		break;

            case 'T':
		KEYS("Text",	pinq->text,	fread_string(fp));
	        break;


	    default:
		sprintf(buf, "read_project_builder: no match for word %s", word);
		bug(buf, 0);
		break;
	}
    }

    return pinq;
}


