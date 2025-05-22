/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "strings.h"
#include "merc.h"
#include "recycle.h"
#include "tables.h"


extern FILE *                  fpArea;
extern char                    strArea[MAX_INPUT_LENGTH];
NOTE_DATA *note_list;
NOTE_DATA *news_list;
NOTE_DATA *changes_list;


void parse_note(CHAR_DATA *ch, char *argument, int type)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    NOTE_DATA *pnote;
    NOTE_DATA **list;
    char *list_name;
    long vnum;
    long anum;
    BUFFER *buffer;

    if (IS_NPC(ch))
	return;

    switch(type)
    {
	default:
	    return;
        case NOTE_NOTE:
            list = &note_list;
	    list_name = "notes";
            break;
        case NOTE_NEWS:
            list = &news_list;
	    list_name = "news";
            break;
        case NOTE_CHANGES:
            list = &changes_list;
	    list_name = "changes";
            break;
    }

    argument = one_argument(argument, arg);
    smash_tilde(argument);

    if (arg[0] == '\0' || !str_prefix(arg, "read"))
    {
        bool fAll;

        if (!str_cmp(argument, "all"))
        {
            fAll = true;
            anum = 0;
        }
        else if (argument[0] == '\0' || !str_prefix(argument, "next"))
        /* read next unread note */
        {
            vnum = 0;
            for (pnote = *list; pnote != NULL; pnote = pnote->next)
            {
                if (!hide_note(ch,pnote))
                {
                    sprintf(buf, "\n\r{YNumber: {X%ld\n\r"
		                  "{Y%s:{X %s\n\r"
				  "{YDated:{X %s\n\r"
				  "{YTo:{X %s\n\r",
                        vnum,
                        pnote->sender,
                        pnote->subject,
                        pnote->date,
                        note_display_recipients_for(ch, pnote));
                    send_to_char(buf, ch);
                    page_to_char(pnote->text, ch);
                    update_read(ch,pnote);
                    return;
                }

                else if (is_note_to(ch,pnote))
                    vnum++;
            }

	    sprintf(buf,"You have no unread %s.\n\r",list_name);
	    send_to_char(buf,ch);
            return;
        }
        else if (is_number(argument))
        {
            fAll = false;
            anum = atoi(argument);
        }
        else
        {
            send_to_char("Read which number?\n\r", ch);
            return;
        }

        vnum = 0;
        for (pnote = *list; pnote != NULL; pnote = pnote->next)
        {
            if (is_note_to(ch, pnote) && (vnum++ == anum || fAll))
            {
                sprintf(buf, "{YNumber: {X%2ld\n\r{Y%s:{X %s\n\r{YDated:{X %s\n\r{YTo:{X %s\n\r",
                    vnum - 1,
                    pnote->sender,
                    pnote->subject,
                    pnote->date,
                    note_display_recipients_for(ch, pnote)
                   );
                send_to_char(buf, ch);
                page_to_char(pnote->text, ch);
		update_read(ch,pnote);
                return;
            }

        }

	sprintf(buf,"There aren't that many %s.\n\r",list_name);
	send_to_char(buf,ch);
        return;
    }

    if (!str_prefix(arg, "list"))
    {
	buffer = new_buf();

	vnum = 0;
	for (pnote = *list; pnote != NULL; pnote = pnote->next)
	{
	    if (is_note_to(ch, pnote))
	    {
		sprintf(buf, "{r[{R%3ld{r]{x %s: %s\n\r",
		    vnum, //hide_note(ch,pnote) ? " " : "N",
		    pnote->sender, pnote->subject);
		add_buf(buffer, buf);
		vnum++;
	    }
	}

	if (!vnum)
	{
	    switch(type)
	    {
		case NOTE_NOTE:
		    send_to_char("There are no notes for you.\n\r",ch);
		    break;
		case NOTE_NEWS:
		    send_to_char("There is no news for you.\n\r",ch);
		    break;
		case NOTE_CHANGES:
		    send_to_char("There are no changes for you.\n\r",ch);
		    break;
	    }
	}

	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
	return;
    }

    if (!str_prefix(arg, "remove"))
    {
        if (!is_number(argument))
        {
            send_to_char("Note remove which number?\n\r", ch);
            return;
        }

        anum = atoi(argument);
        vnum = 0;
        for (pnote = *list; pnote != NULL; pnote = pnote->next)
        {
            if (is_note_to(ch, pnote) && vnum++ == anum)
            {
                note_remove(ch, pnote, false);
		sprintf(buf, "%s removed.\n\r", list_name);
		buf[0] = UPPER(buf[0]);
		send_to_char(buf, ch);
                return;
            }
        }

	sprintf(buf,"There aren't that many %s.\n\r",list_name);
	send_to_char(buf,ch);
        return;
    }

    if (!str_prefix(arg, "delete") && get_staff_rank(ch) >= STAFF_CREATOR)
    {
        if (!is_number(argument))
        {
            send_to_char("Delete which number?\n\r", ch);
            return;
        }

        anum = atoi(argument);
        vnum = 0;
        for (pnote = *list; pnote != NULL; pnote = pnote->next)
        {
            if (is_note_to(ch, pnote) && vnum++ == anum)
            {
                note_remove(ch, pnote,true);
		sprintf(buf, "%s deleted.\n\r", list_name);
		buf[0] = UPPER(buf[0]);
		send_to_char(buf, ch);
                return;
            }
        }

 	sprintf(buf,"There aren't that many %s.\n\r",list_name);
	send_to_char(buf,ch);
        return;
    }

    if (!str_prefix(arg,"catchup"))
    {
	switch(type)
	{
	    case NOTE_NOTE:
		ch->pcdata->last_note = current_time;
		break;
	    case NOTE_NEWS:
		ch->pcdata->last_news = current_time;
		break;
	    case NOTE_CHANGES:
		ch->pcdata->last_changes = current_time;
		break;
	}
	return;
    }

    /* below this point only certain people can edit notes */
    if ((type == NOTE_NEWS && !IS_IMMORTAL(ch))
    ||  (type == NOTE_CHANGES && !IS_IMMORTAL(ch)))
    {
	sprintf(buf,"You aren't high enough level to write %s.\n\r",list_name);
	send_to_char(buf,ch);
	return;
    }

    if (!str_cmp(arg, "edit"))
    {
	note_attach(ch,type);
	if (ch->pnote->type != type)
	{
	    send_to_char(
		"You already have a different note in progress.\n\r",ch);
	    return;
	}

	string_append(ch, &ch->pnote->text);
	return;
    }

    if (!str_prefix(arg, "subject"))
    {
	note_attach(ch,type);
        if (ch->pnote->type != type)
        {
            send_to_char(
                "You already have a different note in progress.\n\r",ch);
            return;
        }

	free_string(ch->pnote->subject);
	ch->pnote->subject = str_dup(argument);
	send_to_char("Subject set.\n\r", ch);
	return;
    }

if (!str_prefix(arg, "to"))
{
    note_attach(ch, type);
    if (ch->pnote->type != type)
    {
        send_to_char("You already have a different note in progress.\n\r", ch);
        return;
    }

    char *rest = argument;
    char type_arg[MAX_INPUT_LENGTH];
    rest = one_argument(rest, type_arg);

    // Clear previous recipients
    free_string(ch->pnote->to_list);
    ch->pnote->to_list = str_dup(argument);

    // Clear all recipient fields
    free_string(ch->pnote->to_characters);
    free_string(ch->pnote->to_accounts);
    free_string(ch->pnote->to_churches);
    free_string(ch->pnote->to_staff_ranks);
    free_string(ch->pnote->to_staff_duties);

    // Lowercase for alias matching
    char type_arg_lc[MAX_INPUT_LENGTH];
    strcpy(type_arg_lc, type_arg);
    for (char *p = type_arg_lc; *p; ++p) *p = LOWER(*p);

    // Handle staff/admin/immortal aliases
    if (!str_cmp(type_arg_lc, "@staff") || !str_cmp(type_arg_lc, "@admin") ||
        !str_cmp(type_arg_lc, "@admins") || !str_cmp(type_arg_lc, "@imms") ||
        !str_cmp(type_arg_lc, "@imm") || !str_cmp(type_arg_lc, "@immortal") ||
        !str_cmp(type_arg_lc, "@immortals") || !str_cmp(type_arg_lc, "@gods"))
    {
        ch->pnote->recipient_type = NOTE_RECIPIENT_STAFF_RANK;
        free_string(ch->pnote->to_staff_ranks);
        ch->pnote->to_staff_ranks = str_dup("immortal");
        send_to_char("Recipients set to staff (Immortal+).\n\r", ch);
        return;
    }

if (!str_cmp(type_arg_lc, "@church"))
{
    if (!ch->church)
    {
        send_to_char("You are not a member of a church.\n\r", ch);
        return;
    }
    ch->pnote->recipient_type = NOTE_RECIPIENT_CHURCH;
    free_string(ch->pnote->to_churches);

    // Surround with quotes if there are spaces
    if (strchr(ch->church->name, ' '))
    {
        char quoted[MAX_INPUT_LENGTH];
        snprintf(quoted, sizeof(quoted), "\"%s\"", ch->church->name);
        ch->pnote->to_churches = str_dup(quoted);
    }
    else
    {
        ch->pnote->to_churches = str_dup(ch->church->name);
    }

    send_to_char("Recipients set to your church.\n\r", ch);
    return;
}
    // Validate and set recipients by type
    if (!str_cmp(type_arg_lc, "account")) {
        if (!IS_IMMORTAL(ch)) {
            send_to_char("Only admins can address to accounts.\n\r", ch);
            return;
        }
        // Validate each account
        char account_name[MAX_INPUT_LENGTH];
        char valid_accounts[MAX_STRING_LENGTH] = "";
        bool found_any = false;
        char pbuf[MAX_STRING_LENGTH];
        strncpy(pbuf, rest, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        while (*p != '\0') {
            p = one_argument(p, account_name);
            if (account_name[0] == '\0') break;
            bool loaded = FALSE;
            ACCOUNT_DATA *acct = get_account_online_or_offline(account_name, &loaded);
            if (acct) {
                if (found_any) strcat(valid_accounts, " ");
                strcat(valid_accounts, account_name);
                found_any = true;
                if (loaded) free_account(acct);
            } else {
                sprintf(buf, "No such account: %s\n\r", account_name);
                send_to_char(buf, ch);
            }
        }
        if (!found_any) {
            send_to_char("No valid accounts specified.\n\r", ch);
            return;
        }
        ch->pnote->recipient_type = NOTE_RECIPIENT_ACCOUNT;
        ch->pnote->to_accounts = str_dup(valid_accounts);
        send_to_char("Account recipients set.\n\r", ch);
        return;
    } else if (!str_cmp(type_arg_lc, "church")) {
        // Validate each church
        char church_name[MAX_INPUT_LENGTH];
        char valid_churches[MAX_STRING_LENGTH] = "";
        bool found_any = false;
        char pbuf[MAX_STRING_LENGTH];
        strncpy(pbuf, rest, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        while (*p != '\0') {
            p = one_argument(p, church_name);
            if (church_name[0] == '\0') break;
            if (get_church_by_name(church_name)) {
                if (found_any) strcat(valid_churches, " ");
                strcat(valid_churches, church_name);
                found_any = true;
            } else {
                sprintf(buf, "No such church: %s\n\r", church_name);
                send_to_char(buf, ch);
            }
        }
        if (!found_any) {
            send_to_char("No valid churches specified.\n\r", ch);
            return;
        }
        ch->pnote->recipient_type = NOTE_RECIPIENT_CHURCH;
        ch->pnote->to_churches = str_dup(valid_churches);
        send_to_char("Church recipients set.\n\r", ch);
        return;
    } else if (!str_cmp(type_arg_lc, "duty")) {
        // Validate each duty
        char duty_name[MAX_INPUT_LENGTH];
        char valid_duties[MAX_STRING_LENGTH] = "";
        bool found_any = false, found_invalid = false;
        char pbuf[MAX_STRING_LENGTH];
        strncpy(pbuf, rest, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        while (*p != '\0') {
            p = one_argument(p, duty_name);
            if (duty_name[0] == '\0') break;
            if (flag_value(immortal_flags, duty_name) != NO_FLAG) {
                if (found_any) strcat(valid_duties, " ");
                strcat(valid_duties, duty_name);
                found_any = true;
            } else {
                sprintf(buf, "No such staff duty: %s\n\r", duty_name);
                send_to_char(buf, ch);
                found_invalid = true;
            }
        }

        if (!found_any) {
            send_to_char("No valid staff duties specified.\n\r", ch);
            show_staff_duties(ch);
            return;
        }
        if (found_invalid)
            show_staff_duties(ch);
        ch->pnote->recipient_type = NOTE_RECIPIENT_STAFF_DUTY;
        ch->pnote->to_staff_duties = str_dup(valid_duties);
        send_to_char("Staff duty recipients set.\n\r", ch);
        return;
    } else if (!str_cmp(type_arg_lc, "rank")) {
        // Validate each rank
        char rank_name[MAX_INPUT_LENGTH];
        char valid_ranks[MAX_STRING_LENGTH] = "";
        bool found_any = false, found_invalid = false;
        char pbuf[MAX_STRING_LENGTH];
        strncpy(pbuf, rest, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        while (*p != '\0') {
            p = one_argument(p, rank_name);
            if (rank_name[0] == '\0') break;
            if (flag_value(staff_ranks, rank_name) != NO_FLAG) {
                for (int i = 0; staff_ranks[i].name != NULL; i++) {
                    if (!str_cmp(staff_ranks[i].name, rank_name) && staff_ranks[i].settable) {
                        // Valid selectable rank
                        if (found_any) strcat(valid_ranks, " ");
                        strcat(valid_ranks, rank_name);
                        found_any = true;
                        break;
                    }
                }
            } else {
                sprintf(buf, "No such staff rank: %s\n\r", rank_name);
                send_to_char(buf, ch);
                found_invalid = true;
            }
        }
        if (!found_any) {
            send_to_char("No valid staff ranks specified.\n\r", ch);
            show_staff_ranks(ch);
            return;
        }
        if (found_invalid)
            show_staff_ranks(ch);

        ch->pnote->recipient_type = NOTE_RECIPIENT_STAFF_RANK;
        ch->pnote->to_staff_ranks = str_dup(valid_ranks);
        send_to_char("Staff rank recipients set.\n\r", ch);
        return;
    } else if (!str_cmp(type_arg_lc, "all")) {
        if (!IS_IMMORTAL(ch)) {
            send_to_char("Only admins can address to all.\n\r", ch);
            return;
        }
        ch->pnote->recipient_type = NOTE_RECIPIENT_ALL;
        send_to_char("Recipients set to all.\n\r", ch);
        return;
    } else {
        // Default: treat all arguments as character names
        char char_name[MAX_INPUT_LENGTH];
        char valid_chars[MAX_STRING_LENGTH] = "";
        bool found_any = false;
        char pbuf[MAX_STRING_LENGTH];
        strncpy(pbuf, argument, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        while (*p != '\0') {
            p = one_argument(p, char_name);
            if (char_name[0] == '\0') break;
            if (player_exists(char_name)) {
                if (found_any) strcat(valid_chars, " ");
                strcat(valid_chars, char_name);
                found_any = true;
            } else {
                sprintf(buf, "No such player: %s\n\r", char_name);
                send_to_char(buf, ch);
            }
        }
        if (!found_any) {
            send_to_char("No valid player names specified.\n\r", ch);
            return;
        }
        ch->pnote->recipient_type = NOTE_RECIPIENT_CHARACTER;
        ch->pnote->to_characters = str_dup(valid_chars);
        send_to_char("Character recipients set.\n\r", ch);
        return;
    }
}

    if (!str_prefix(arg, "clear"))
    {
	if (ch->pnote != NULL)
	{
	    free_note(ch->pnote);
	    ch->pnote = NULL;
	}

	sprintf(buf, "%s cleared.\n\r", list_name);
	buf[0] = UPPER(buf[0]);
	send_to_char(buf, ch);
	return;
    }

    if (!str_prefix(arg, "show"))
    {
	if (ch->pnote == NULL)
	{
	    send_to_char("You have no note in progress.\n\r", ch);
	    return;
	}

	if (ch->pnote->type != type)
	{
	    send_to_char("You aren't working on that kind of note.\n\r",ch);
	    return;
	}

	sprintf(buf, "\n\r{Y%s:{X %s\n\r{YTo:{X %s\n\r",
	    ch->pnote->sender,
	    ch->pnote->subject,
	    note_display_recipients(ch->pnote) ? note_display_recipients(ch->pnote) : "none"
	   );
	send_to_char(buf, ch);
	send_to_char(ch->pnote->text, ch);
	return;
    }

    if (!str_prefix(arg, "post") || !str_prefix(arg, "send"))
    {
	char *strtime;

	if (ch->pnote == NULL)
	{
	    send_to_char("You have no note in progress.\n\r", ch);
	    return;
	}

        if (ch->pnote->type != type)
        {
            send_to_char("You aren't working on that kind of note.\n\r",ch);
            return;
        }

if (
    (!ch->pnote->to_list || !*ch->pnote->to_list) &&
    (!ch->pnote->to_characters || !*ch->pnote->to_characters) &&
    (!ch->pnote->to_accounts || !*ch->pnote->to_accounts) &&
    (!ch->pnote->to_churches || !*ch->pnote->to_churches) &&
    (!ch->pnote->to_staff_ranks || !*ch->pnote->to_staff_ranks) &&
    (!ch->pnote->to_staff_duties || !*ch->pnote->to_staff_duties) &&
    ch->pnote->recipient_type != NOTE_RECIPIENT_ALL
)
{
    send_to_char("You need to provide a valid recipient.\n\r", ch);
	    return;
	}

	if (!str_cmp(ch->pnote->subject,""))
	{
	    send_to_char("You need to set a subject.\n\r",ch);
	    return;
	}

	ch->pnote->next			= NULL;
	strtime				= ctime(&current_time);
	strtime[strlen(strtime)-1]	= '\0';
	ch->pnote->date			= str_dup(strtime);
	ch->pnote->date_stamp		= current_time;

	append_note(ch->pnote);
	ch->pnote = NULL;
	return;
    }

    send_to_char("Valid commands are:\n\r"
		  "read to show edit clear subject post\n\r", ch);
}


int count_spool(CHAR_DATA *ch, NOTE_DATA *spool)
{
    int count = 0;
    NOTE_DATA *pnote;

    for (pnote = spool; pnote != NULL; pnote = pnote->next)
	if (!hide_note(ch,pnote))
	    count++;

    return count;
}


void do_unread(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    int count;
    bool found = false;

    if (IS_NPC(ch))
	return;

    if ((count = count_spool(ch,news_list)) > 0)
    {
	found = true;
	sprintf(buf,"There %s %d new news article%s waiting.\n\r",
	    count > 1 ? "are" : "is",count, count > 1 ? "s" : "");
	send_to_char(buf,ch);
    }
    if ((count = count_spool(ch,changes_list)) > 0)
    {
	found = true;
	sprintf(buf,"There %s %d change%s waiting to be read.\n\r",
	    count > 1 ? "are" : "is", count, count > 1 ? "s" : "");
        send_to_char(buf,ch);
    }
    if ((count = count_spool(ch,note_list)) > 0)
    {
	found = true;
	sprintf(buf,"{GYou have {Y%d {Gnew note%s waiting.{x\n\r",
	    count, count > 1 ? "s" : "");
	send_to_char(buf,ch);
    }

    if (!found)
	send_to_char("You have no unread notes.\n\r",ch);
}


void do_note(CHAR_DATA *ch,char *argument)
{
    parse_note(ch,argument,NOTE_NOTE);
}


void do_news(CHAR_DATA *ch,char *argument)
{
    parse_note(ch,argument,NOTE_NEWS);
}


void do_changes(CHAR_DATA *ch,char *argument)
{
    parse_note(ch,argument,NOTE_CHANGES);
}


void save_notes(int type)
{
    FILE *fp;
    char *name;
    NOTE_DATA *pnote;

    switch (type)
    {
	default:
	    return;
	case NOTE_NOTE:
	    name = NOTE_FILE;
	    pnote = note_list;
	    break;
	case NOTE_NEWS:
	    name = NEWS_FILE;
	    pnote = news_list;
	    break;
	case NOTE_CHANGES:
	    name = CHANGES_FILE;
	    pnote = changes_list;
	    break;
    }

    fclose(fpReserve);
    if ((fp = fopen(name, "w")) == NULL)
	perror(name);
    else
    {
	for (; pnote != NULL; pnote = pnote->next)
	{
	    fprintf(fp, "Sender  %s~\n", pnote->sender);
	    fprintf(fp, "Date    %s~\n", pnote->date);
	    fprintf(fp, "Stamp   %ld\n", (long int)pnote->date_stamp);
	    fprintf(fp, "To      %s~\n", pnote->to_list);
	    fprintf(fp, "Subject %s~\n", pnote->subject);
        fprintf(fp, "RecipientType %d\n", (int)pnote->recipient_type);
        fprintf(fp, "ToCharacters %s~\n", pnote->to_characters ? pnote->to_characters : "");
        fprintf(fp, "ToAccounts %s~\n", pnote->to_accounts ? pnote->to_accounts : "");
        fprintf(fp, "ToChurches %s~\n", pnote->to_churches ? pnote->to_churches : "");
        fprintf(fp, "ToStaffRanks %s~\n", pnote->to_staff_ranks ? pnote->to_staff_ranks : "");
        fprintf(fp, "ToStaffDuties %s~\n", pnote->to_staff_duties ? pnote->to_staff_duties : "");
	    fprintf(fp, "Text\n%s~\n",   fix_string(pnote->text));
	}
	fclose(fp);
	fpReserve = fopen(NULL_FILE, "r");
   	return;
    }
}


void load_notes(void)
{
    load_thread(NOTE_FILE,&note_list, NOTE_NOTE, 14*24*60*60);
    load_thread(NEWS_FILE,&news_list, NOTE_NEWS, 0);
    load_thread(CHANGES_FILE,&changes_list,NOTE_CHANGES, 0);
}


void load_thread(char *name, NOTE_DATA **list, int type, time_t free_time)
{
    FILE *fp;
    NOTE_DATA *pnotelast;

    if ((fp = fopen(name, "r")) == NULL)
	return;

    pnotelast = NULL;
    for (; ;)
    {
	NOTE_DATA *pnote;
	char letter;

	do
	{
	    letter = getc(fp);
            if (feof(fp))
            {
                fclose(fp);
                return;
            }
        }
        while (ISSPACE(letter));
        ungetc(letter, fp);

        pnote           = alloc_perm(sizeof(*pnote));
        memset(pnote, 0, sizeof(*pnote)); // Ensure all fields are zeroed


        if (str_cmp(fread_word(fp), "sender"))
            break;
        pnote->sender   = fread_string(fp);

        if (str_cmp(fread_word(fp), "date"))
            break;
        pnote->date     = fread_string(fp);

        if (str_cmp(fread_word(fp), "stamp"))
            break;
        pnote->date_stamp = fread_number(fp);

        if (str_cmp(fread_word(fp), "to"))
            break;
        pnote->to_list  = fread_string(fp);

        if (str_cmp(fread_word(fp), "subject"))
            break;
        pnote->subject  = fread_string(fp);

        // New fields (optional for backward compatibility)
        char *word = fread_word(fp);
        if (!str_cmp(word, "RecipientType"))
            pnote->recipient_type = fread_number(fp);
        else
            pnote->recipient_type = NOTE_RECIPIENT_CHARACTER; // Default/fallback

        word = fread_word(fp);
        if (!str_cmp(word, "ToCharacters"))
            pnote->to_characters = fread_string(fp);
        else
            pnote->to_characters = str_dup("");

        word = fread_word(fp);
        if (!str_cmp(word, "ToAccounts"))
            pnote->to_accounts = fread_string(fp);
        else
            pnote->to_accounts = str_dup("");

        word = fread_word(fp);
        if (!str_cmp(word, "ToChurches"))
            pnote->to_churches = fread_string(fp);
        else
            pnote->to_churches = str_dup("");

        word = fread_word(fp);
        if (!str_cmp(word, "ToStaffRanks"))
            pnote->to_staff_ranks = fread_string(fp);
        else
            pnote->to_staff_ranks = str_dup("");

        word = fread_word(fp);
        if (!str_cmp(word, "ToStaffDuties"))
            pnote->to_staff_duties = fread_string(fp);
        else
            pnote->to_staff_duties = str_dup("");

        if (str_cmp(fread_word(fp), "text"))
            break;
        pnote->text     = fread_string(fp);

        if (free_time && pnote->date_stamp < current_time - free_time)
        {
	    free_note(pnote);
            continue;
        }

	pnote->type = type;

        if (*list == NULL)
            *list = pnote;
        else
            pnotelast->next = pnote;

        pnotelast = pnote;
    }

fclose(fp);
return;
}


void append_note(NOTE_DATA *pnote)
{
    FILE *fp;
    char *name;
    NOTE_DATA **list;
    NOTE_DATA *last;

    switch(pnote->type)
    {
	default:
	    return;
	case NOTE_NOTE:
	    name = NOTE_FILE;
	    list = &note_list;
	    break;
	case NOTE_NEWS:
	     name = NEWS_FILE;
	     list = &news_list;
	     break;
	case NOTE_CHANGES:
	     name = CHANGES_FILE;
	     list = &changes_list;
	     break;
    }

    if (*list == NULL)
	*list = pnote;
    else
    {
	for (last = *list; last->next != NULL; last = last->next);
	last->next = pnote;
    }

    fclose(fpReserve);
    if ((fp = fopen(name, "a")) == NULL)
    {
        perror(name);
    }
    else
    {
        fprintf(fp, "Sender  %s~\n", pnote->sender);
        fprintf(fp, "Date    %s~\n", pnote->date);
        fprintf(fp, "Stamp   %ld\n", (long int)pnote->date_stamp);
        fprintf(fp, "To      %s~\n", pnote->to_list);
        fprintf(fp, "Subject %s~\n", pnote->subject);
        fprintf(fp, "RecipientType %d\n", (int)pnote->recipient_type);
        fprintf(fp, "ToCharacters %s~\n", pnote->to_characters ? pnote->to_characters : "");
        fprintf(fp, "ToAccounts %s~\n", pnote->to_accounts ? pnote->to_accounts : "");
        fprintf(fp, "ToChurches %s~\n", pnote->to_churches ? pnote->to_churches : "");
        fprintf(fp, "ToStaffRanks %s~\n", pnote->to_staff_ranks ? pnote->to_staff_ranks : "");
        fprintf(fp, "ToStaffDuties %s~\n", pnote->to_staff_duties ? pnote->to_staff_duties : "");
        fprintf(fp, "Text\n%s~\n", pnote->text);
        fclose(fp);
    }
    fpReserve = fopen(NULL_FILE, "r");
}


bool is_note_to(CHAR_DATA *ch, NOTE_DATA *pnote)
{
    if (!str_cmp(ch->name, pnote->sender))
        return true;

    switch (pnote->recipient_type) {
        case NOTE_RECIPIENT_ALL:
            return true;
        case NOTE_RECIPIENT_CHARACTER:
            // Support multiple character recipients (space-separated)
            if (is_exact_name(ch->name, pnote->to_characters))
                return true;
            break;
        case NOTE_RECIPIENT_ACCOUNT:
            if (ch->desc && ch->desc->account &&
                is_exact_name(ch->desc->account->username, pnote->to_accounts))
                return true;
            break;
        case NOTE_RECIPIENT_CHURCH:
            if (ch->church && is_exact_name(ch->church->name, pnote->to_churches))
                return true;
            break;
        case NOTE_RECIPIENT_STAFF_RANK:
            // Assume to_staff_ranks is a space-separated list of rank names or numbers
            if (is_staff_rank_in_list(ch, pnote->to_staff_ranks))
                return true;
            break;
        case NOTE_RECIPIENT_STAFF_DUTY:
            // Assume to_staff_duties is a space-separated list of duty names or numbers
            if (is_staff_duty_in_list(ch, pnote->to_staff_duties))
                return true;
            break;
        default:
            break;
    }

    // Legacy support for "staff", "immortals", etc.
    if (IS_IMMORTAL(ch) && (
        is_exact_name("immortal", pnote->to_list) ||
        is_exact_name("immortals", pnote->to_list) ||
        is_exact_name("imms", pnote->to_list) ||
        is_exact_name("gods", pnote->to_list) ||
        is_exact_name("staff", pnote->to_list) ||
        is_exact_name("admins", pnote->to_list)))
        return true;

    // Also support legacy multi-character recipients in to_list
    if (is_name(ch->name, pnote->to_list))
        return true;

    return false;
}


void note_attach(CHAR_DATA *ch, int type)
{
    NOTE_DATA *pnote;

    if (ch->pnote != NULL)
        return;

    pnote = new_note();

    pnote->next           = NULL;
    pnote->sender         = str_dup(ch->name);
    pnote->date           = str_dup("");
    pnote->to_list        = str_dup("");
    pnote->subject        = str_dup("");
    pnote->text           = str_dup("");
    pnote->type           = type;
    pnote->recipient_type = NOTE_RECIPIENT_CHARACTER;
    pnote->to_characters  = str_dup("");
    pnote->to_accounts    = str_dup("");
    pnote->to_churches    = str_dup("");
    pnote->to_staff_ranks = str_dup("");
    pnote->to_staff_duties= str_dup("");
    ch->pnote             = pnote;
}



void note_remove(CHAR_DATA *ch, NOTE_DATA *pnote, bool delete)
{
    char to_new[MAX_INPUT_LENGTH];
    char to_one[MAX_INPUT_LENGTH];
    NOTE_DATA *prev;
    NOTE_DATA **list;
    char *to_list;

    if (!delete)
    {
	/* make a new list */
	to_new[0]	= '\0';
	to_list	= pnote->to_list;
	while (*to_list != '\0')
	{
	    to_list	= one_argument(to_list, to_one);
	    if (to_one[0] != '\0' && str_cmp(ch->name, to_one))
	    {
		strcat(to_new, " ");
		strcat(to_new, to_one);
	    }
	}
	/* Just a simple recipient removal? */
	if (str_cmp(ch->name, pnote->sender) && to_new[0] != '\0')
	{
	    free_string(pnote->to_list);
	    pnote->to_list = str_dup(to_new + 1);
	    return;
	}
    }

    /* nuke the whole note */
    switch(pnote->type)
    {
	default:
	    return;
	case NOTE_NOTE:
	    list = &note_list;
	    break;
	case NOTE_NEWS:
	    list = &news_list;
	    break;
	case NOTE_CHANGES:
	    list = &changes_list;
	    break;
    }

    /*
     * Remove note from linked list.
     */
    if (pnote == *list)
	*list = pnote->next;
    else
    {
	for (prev = *list; prev != NULL; prev = prev->next)
	{
	    if (prev->next == pnote)
		break;
	}

	if (prev == NULL)
	{
	    bug("Note_remove: pnote not found.", 0);
	    return;
	}

	prev->next = pnote->next;
    }

    save_notes(pnote->type);
    free_note(pnote);
}


bool hide_note(CHAR_DATA *ch, NOTE_DATA *pnote)
{
    time_t last_read;

    if (IS_NPC(ch))
	return true;

    switch (pnote->type)
    {
	default:
	    return true;
	case NOTE_NOTE:
	    last_read = ch->pcdata->last_note;
	    break;
	case NOTE_NEWS:
	    last_read = ch->pcdata->last_news;
	    break;
	case NOTE_CHANGES:
	    last_read = ch->pcdata->last_changes;
	    break;
    }

    if (pnote->date_stamp <= last_read)
	return true;

    if (!str_cmp(ch->name,pnote->sender))
	return true;

    if (!is_note_to(ch,pnote))
	return true;

    return false;
}


void update_read(CHAR_DATA *ch, NOTE_DATA *pnote)
{
    time_t stamp;

    if (IS_NPC(ch))
	return;

    stamp = pnote->date_stamp;

    switch (pnote->type)
    {
        default:
            return;
        case NOTE_NOTE:
	    ch->pcdata->last_note = UMAX(ch->pcdata->last_note,stamp);
            break;
        case NOTE_NEWS:
	    ch->pcdata->last_news = UMAX(ch->pcdata->last_news,stamp);
            break;
        case NOTE_CHANGES:
	    ch->pcdata->last_changes = UMAX(ch->pcdata->last_changes,stamp);
            break;
    }
}


int count_note(CHAR_DATA *ch, int type)
{
    NOTE_DATA *pnote;
    NOTE_DATA **list;
    int counter;

    if (IS_NPC(ch))
	return 0;

    switch(type)
    {
	default:
	    return 0;
        case NOTE_NOTE:
            list = &note_list;
            break;
        case NOTE_NEWS:
            list = &news_list;
            break;
        case NOTE_CHANGES:
            list = &changes_list;
            break;
    }

    counter = 0;
    for (pnote = *list; pnote != NULL; pnote = pnote->next)
    {
	if (!hide_note(ch,pnote))
	{
	    counter++;
	}
    }

    return counter;
}

const char *note_display_recipients(NOTE_DATA *pnote)
{
    static char buf[MAX_STRING_LENGTH];
    const char *recips = NULL;
    const char *label = NULL;
    const char *color = NULL;
    char temp[1024] = "";

    switch (pnote->recipient_type) {
        case NOTE_RECIPIENT_ALL:
            snprintf(buf, sizeof(buf), "({YAll{X)");
            return buf;
        case NOTE_RECIPIENT_ACCOUNT:
            recips = pnote->to_accounts && *pnote->to_accounts ? pnote->to_accounts : "(none)";
            label = "Account";
            color = "{G";
            break;
        case NOTE_RECIPIENT_CHURCH:
            recips = pnote->to_churches && *pnote->to_churches ? pnote->to_churches : "(none)";
            label = "Church";
            color = "{B";
            break;
        case NOTE_RECIPIENT_STAFF_RANK:
            recips = pnote->to_staff_ranks && *pnote->to_staff_ranks ? pnote->to_staff_ranks : "(none)";
            label = "Staff Rank";
            color = "{M";
            break;
        case NOTE_RECIPIENT_STAFF_DUTY:
            recips = pnote->to_staff_duties && *pnote->to_staff_duties ? pnote->to_staff_duties : "(none)";
            label = "Staff Duty";
            color = "{C";
            break;
        case NOTE_RECIPIENT_CHARACTER:
        default:
            recips = pnote->to_characters && *pnote->to_characters ? pnote->to_characters : "(none)";
            label = "Personal";
            color = "{Y";
            break;
    }

    // Convert space-separated list to comma-separated
    if (recips && strcmp(recips, "(none)")) {
        char name[MAX_INPUT_LENGTH];
        char pbuf[1024];
        strncpy(pbuf, recips, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        temp[0] = '\0';
        bool first = true;
        while (*p != '\0') {
            p = one_argument(p, name);
            if (name[0] == '\0') break;
            if (!first)
                strcat(temp, ", ");
            strcat(temp, name);
            first = false;
        }
        recips = temp;
    }

    snprintf(buf, sizeof(buf), "(%s%s{X) %s", color, label, recips ? recips : "(none)");
    return buf;
}

const char *note_display_recipients_for(CHAR_DATA *viewer, NOTE_DATA *pnote)
{
    static char buf[MAX_STRING_LENGTH];
    const char *recips = NULL;
    const char *label = NULL;
    const char *color = NULL;
    char temp[1024] = "";

    switch (pnote->recipient_type) {
        case NOTE_RECIPIENT_ALL:
            snprintf(buf, sizeof(buf), "({YAll{X)");
            return buf;
        case NOTE_RECIPIENT_ACCOUNT:
            label = "Account";
            color = "{G";
            // Hide account names from non-admins
            if (!IS_IMMORTAL(viewer)) {
                // Show only their character name if they are a recipient
                if (is_note_to(viewer, pnote))
                    snprintf(buf, sizeof(buf), "({GAccount{X) %s", viewer->name);
                else
                    snprintf(buf, sizeof(buf), "({GAccount{X)");
                return buf;
            }
            recips = pnote->to_accounts && *pnote->to_accounts ? pnote->to_accounts : "(none)";
            break;
        case NOTE_RECIPIENT_CHURCH:
            recips = pnote->to_churches && *pnote->to_churches ? pnote->to_churches : "(none)";
            label = "Church";
            color = "{B";
            break;
        case NOTE_RECIPIENT_STAFF_RANK:
            recips = pnote->to_staff_ranks && *pnote->to_staff_ranks ? pnote->to_staff_ranks : "(none)";
            label = "Staff Rank";
            color = "{M";
            break;
        case NOTE_RECIPIENT_STAFF_DUTY:
            recips = pnote->to_staff_duties && *pnote->to_staff_duties ? pnote->to_staff_duties : "(none)";
            label = "Staff Duty";
            color = "{C";
            break;
        case NOTE_RECIPIENT_CHARACTER:
        default:
            recips = pnote->to_characters && *pnote->to_characters ? pnote->to_characters : "(none)";
            label = "Personal";
            color = "{Y";
            break;
    }

    // Convert space-separated list to comma-separated
    if (recips && strcmp(recips, "(none)")) {
        char name[MAX_INPUT_LENGTH];
        char pbuf[1024];
        strncpy(pbuf, recips, sizeof(pbuf));
        pbuf[sizeof(pbuf)-1] = '\0';
        char *p = pbuf;
        temp[0] = '\0';
        bool first = true;
        while (*p != '\0') {
            p = one_argument(p, name);
            if (name[0] == '\0') break;
            if (!first)
                strcat(temp, ", ");
            strcat(temp, name);
            first = false;
        }
        recips = temp;
    }

    snprintf(buf, sizeof(buf), "(%s%s{X) %s", color, label, recips ? recips : "(none)");
    return buf;
}
