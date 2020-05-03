/* This file contains most of the functions associated with the in-game staff
   management system. All source copyright Anton Ouzilov, 2007. */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"
#include "interp.h"

/* A global list of all imms. */
IMMORTAL_DATA		*immortal_list;



/* Top level staff-management handler. For use only by whoever manages
   staff, although all imps can use it. */
void do_staff(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];

    argument = one_argument(argument, arg);

    if (IS_NPC(ch))
	return;

    if (ch->pcdata->security < 10 && !IS_SET(ch->pcdata->immortal->duties, IMMORTAL_STAFF))
    {
	send_to_char("You aren't authorized to do this.\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "list")) {
	do_function(ch, &do_slist, argument);
	return;
    }

    if (!str_cmp(arg, "add")) {
	do_function(ch, &do_sadd, argument);
	return;
    }

    if (!str_cmp(arg, "duty")) {
	do_function(ch, &do_sduty, argument);
	return;
    }

    if (!str_cmp(arg, "supervisor")) {
	do_function(ch, &do_ssupervisor, argument);
	return;
    }

    if (!str_cmp(arg, "delete")) {
	do_function(ch, &do_sdelete, argument);
	return;
    }


    send_to_char("Syntax:  staff list\n\r"
		 "         staff add [immortal]\n\r"
	         "         staff delete [immortal]\n\r"
		 "         staff duty [immortal] [duty]\n\r"
		 "         staff supervisor [immortal] [supervisor|none]\n\r", ch);
}


/* Show the staff list. */
void do_slist(CHAR_DATA *ch, char *argument)
{
    // true laziness exists :P
     do_function(ch, &do_wizlist, "");

}


void do_sadd(CHAR_DATA *ch, char *argument)
{
    IMMORTAL_DATA *immortal;
    char buf[MSL];

    if (argument[0] == '\0' || strlen(argument) < 3) {
	send_to_char("Syntax:  staff add [player name]\n\r",  ch);
	return;
    }

    if (!player_exists(argument))
    {
	send_to_char("That character doesn't exist.\n\r", ch);
	return;
    }

    if (find_immortal(argument) != NULL) {
	send_to_char("There is already an immortal by that name.\n\r", ch);
	return;
    }

    sprintf(buf, "%s", argument);
    buf[0] = UPPER(buf[0]);

    immortal = new_immortal();
    immortal->name = str_dup(buf);
    immortal->imm_flag = str_dup("{RImmortal{x");
    immortal->created = current_time;
    immortal->duties = 0;

    add_immortal(immortal);

    act("Created new immortal $T.", ch, NULL, NULL, NULL, NULL, NULL, immortal->name, TO_CHAR);
    save_immstaff();
}


/* Inserts the immortal into the list, alphabetically sorted. Used here and in save.c */
void add_immortal(IMMORTAL_DATA *immortal)
{
    IMMORTAL_DATA *imm_tmp, *imm_last = NULL;

    /* first imm ever added */
    if (immortal_list == NULL) {
    immortal_list = immortal;
	immortal->next = NULL;
    }

    /* first alphabetically */
    else if (str_cmp(immortal->name, immortal_list->name) < 0) {
	immortal->next = immortal_list;
	immortal_list = immortal;
    }

    /* in the middleor at the end */
    else {
	for (imm_tmp = immortal_list; imm_tmp != NULL; imm_tmp = imm_tmp->next) {
	    if (str_cmp(imm_tmp->name, immortal->name) > 0)
		break;
	    imm_last = imm_tmp;
	}

	imm_last->next = immortal;
	immortal->next = imm_tmp;

    }

}


/* Toggles a duty for an immortal.
   Staff duty [immortal] [duty] */
void do_sduty(CHAR_DATA *ch, char *argument)
{
    int value;
    char arg[MSL];
    IMMORTAL_DATA *immortal;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
	send_to_char("Syntax:  staff duty [immortal] [duty]\n\r", ch);
	send_to_char("For a list of duties, type 'staff duty ?'\n\r", ch);
	return;
    }

    if (arg[0] == '?') {
	show_help(ch, "immortalflags");
	return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
	send_to_char("Immortal not found.\n\r", ch);
	return;
    }

    if ((value = flag_value(immortal_flags, argument)) == NO_FLAG) {
	send_to_char("Invalid duty.\n\r", ch);
	return;
    }

    TOGGLE_BIT(immortal->duties, value);
    send_to_char("Duty bit toggled.\n\r", ch);
    save_immstaff();
}


/* Find an immortal of a certain name in the list.
   Calling it with list = NULL will search the whole imm database. */
IMMORTAL_DATA *find_immortal(char *argument)
{
    IMMORTAL_DATA *immortal;

    if (argument[0] == '\0')
	return NULL;

    for (immortal = immortal_list; immortal != NULL; immortal = immortal->next)  {
	if (!str_prefix(argument, immortal->name))
	    break;
    }

    return immortal;
}


void do_sdelete(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    IMMORTAL_DATA *immortal, *tmp, *last;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
	send_to_char("Syntax:  staff delete [immortal]"
		     "\n\r{RWARNING:{x all information associated with this immortal will be wiped!\n\r", ch);
	return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
	send_to_char("No such immortal.\n\r", ch);
	return;
    }

    act("$T's immortal priveleges have been terminated.", ch, NULL, NULL, NULL, NULL, NULL, immortal->name, TO_CHAR);
    /* Remove it from the global list */
    last = NULL;
    for (tmp = immortal_list; tmp != NULL; tmp = tmp->next) {
	if (tmp == immortal)
	    break;

	last = tmp;
    }

    if (last != NULL)
	last->next = immortal->next;
    else
	immortal_list = immortal->next;

    free_immortal(immortal);
    save_immstaff();
}



/* Sets a person's supervisor */
void do_ssupervisor(CHAR_DATA *ch, char *argument)
{
    char arg[MSL], arg2[MSL];
    IMMORTAL_DATA *immortal;
    IMMORTAL_DATA *leader;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0') {
	send_to_char("Syntax:  staff supervisor [immortal] [supervisor]\n\r", ch);
	return;
    }

    if ((immortal = find_immortal(arg)) == NULL) {
	send_to_char("Immortal not found.\n\r", ch);
	return;
    }

    if ((leader = find_immortal(arg2)) == NULL) {
	send_to_char("Supervisor not found.\n\r", ch);
	return;
    }

    immortal->leader = str_dup(leader->name);
    act("Set $t's supervisor to $T.", ch, NULL, NULL, NULL, NULL, immortal->name, leader->name,  TO_CHAR);
	}




/*
 * Functions to do saving and loading. Immortal list is saved in
 * a tree fashion, using recursion to preserve the order of the list.
 *
 * #IMMORTAL
 * Name Name~
 * Duties Duties~ 		(make sure to print in a understandable fashion)
 * ImmFlag Flag~
 * Poofin String~
 * Poofout String~
 *
 * ...
 * #END
 */


/* Save the imm staff. */
void save_immstaff()
{
    FILE *fp;

    if ((fp = fopen(STAFF_FILE, "w")) == NULL) {
	bug("save_immstaff: couldn't open staff.dat for writing", 0);
	return;
    }

    if (immortal_list)
    save_immortal(fp, immortal_list);

    fprintf(fp, "#END\n");
    fclose(fp);
}


/* Save one immortal and his/her subordinates. */
void save_immortal(FILE *fp, IMMORTAL_DATA *immortal)
{
    if (immortal->next != NULL)
	save_immortal(fp, immortal->next);

    fprintf(fp, "#IMMORTAL\n");
    fprintf(fp, "Name %s~\n", immortal->name);
    fprintf(fp, "Duties %ld\n", immortal->duties);
    fprintf(fp, "Created %ld\n", (long)immortal->created);
    fprintf(fp, "ImmFlag %s~\n", immortal->imm_flag);
    fprintf(fp, "LastOLCCommand %ld\n", immortal->last_olc_command);
    fprintf(fp, "Leader %s~\n", immortal->leader != NULL ? immortal->leader : "None");
    fprintf(fp, "Poofin %s~\n", immortal->bamfin);
    fprintf(fp, "Poofout %s~\n", immortal->bamfout);


    fprintf(fp, "#-IMMORTAL\n");
}


/* These variables are used in various functions in loading/saving, so they
      are global to avoid redundancy. */
static bool fMatch;
static char *word;


/* Read imm staff at bootup. */
void read_immstaff()
{
    FILE *fp;
    IMMORTAL_DATA *immortal;

    if ((fp = fopen(STAFF_FILE, "r")) == NULL) {
	bug("read_immstaff: couldn't open immstaff.dat", 0);
	exit(1);
    }

    for (;;)
    {
	word = fread_word(fp);
	if (!str_cmp(word, "#IMMORTAL"))
	{
	    immortal = read_immortal(fp);
		immortal->next = immortal_list;
	    immortal_list = immortal;
	}

	if (!str_cmp(word, "#END"))
	    break;
    }

    fclose(fp);
}


IMMORTAL_DATA *read_immortal(FILE *fp)
{
    IMMORTAL_DATA *immortal;
    char buf[MSL];

    immortal = new_immortal();

    while (str_cmp((word = fread_word(fp)), "#-IMMORTAL"))
    {
	fMatch = FALSE;
	switch (word[0])
	{
	    case '#':
		break;

	    case 'C':
	        KEY("Created", immortal->created, fread_number(fp));

	    case 'D':
		if (!str_cmp(word, "Duties")) {
		    immortal->duties = fread_number(fp);
		    fMatch = TRUE;
		}
		break;

            case 'I':
		KEYS("ImmFlag",	immortal->imm_flag,	fread_string(fp));
		break;

	    case 'L':
		KEY("LastOLCCommand", immortal->last_olc_command, fread_number(fp));
                KEYS("Leader", immortal->leader, fread_string(fp));

		break;

	    case 'N':
		KEYS("Name",	immortal->name,		fread_string(fp));
		break;

	    case 'P':
		KEYS("Poofin",	immortal->bamfin,	fread_string(fp));
		KEYS("Poofout",	immortal->bamfout,	fread_string(fp));
		break;

	    default:
		sprintf(buf, "read_immortal: no match for word %s", word);
		bug(buf, 0);
		break;
	}
    }

	// Missing creation date
	if(immortal->created < 1)
	{
		DESCRIPTOR_DATA d;
		// TEMPORARY
		if( !load_char_obj(&d, immortal->name) )
		{
		    sprintf(buf, "read_immortal: attempting to correct created timestamp failed for %s", immortal->name);
		    bug(buf, 0);
		}
		else if( !d.character || !d.character->pcdata )
	    {
		    sprintf(buf, "read_immortal: attempting to correct created timestamp failed for %s", immortal->name);
		    bug(buf, 0);
		}
		else
		{
			immortal->created = d.character->pcdata->creation_date;
			d.character->desc = NULL;
			extract_char(d.character, TRUE);
		}
	}


    sprintf(buf, "read_immortal: immortal %s", immortal->name);
    log_string(buf);
    return immortal;
}

