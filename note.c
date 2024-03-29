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
                        pnote->to_list);
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
                    pnote->to_list
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

    if (!str_prefix(arg, "delete") && get_trust(ch) >= MAX_LEVEL - 1)
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
	if (!str_cmp(argument, "All") && !IS_IMMORTAL(ch))
	{
	    send_to_char("Only the immortals can address to all.\n\r", ch);
	    return;
	}
	note_attach(ch,type);
        if (ch->pnote->type != type)
        {
            send_to_char(
                "You already have a different note in progress.\n\r",ch);
            return;
        }
	free_string(ch->pnote->to_list);
	ch->pnote->to_list = str_dup(argument);
	//sprintf(buf, "Started a %s.\n\r", list_name);
	//send_to_char(buf, ch);
	return;
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
	    ch->pnote->to_list
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

	if (!str_cmp(ch->pnote->to_list,""))
	{
	    send_to_char(
            "You need to provide a recipient.\n\r", ch);
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

    strcpy(strArea, NOTE_FILE);
    fpArea = fp;
    bug("Load_notes: bad key word.", 0);
    exit(1);
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
        fprintf(fp, "Text\n%s~\n", pnote->text);
        fclose(fp);
    }
    fpReserve = fopen(NULL_FILE, "r");
}


bool is_note_to(CHAR_DATA *ch, NOTE_DATA *pnote)
{
    if (!str_cmp(ch->name, pnote->sender))
	return true;

    if (is_exact_name("all", pnote->to_list))
	return true;

    if (ch->church != NULL
    && is_name(ch->church->name, pnote->to_list))
        return true;

    if (ch->tot_level == MAX_LEVEL
    &&  (is_exact_name(pnote->to_list, "coder")
         || is_exact_name(pnote->to_list, "coders")
         || is_exact_name(pnote->to_list, "imp")
         || is_exact_name(pnote->to_list, "implementor")
         || is_exact_name(pnote->to_list, "implementors")))
	return true;

    if (IS_IMMORTAL(ch)
	    && (is_exact_name("immortal", pnote->to_list)
		|| is_exact_name("immortals", pnote->to_list)
		|| is_exact_name("imms", pnote->to_list)
		|| is_exact_name("gods", pnote->to_list)
	        || is_exact_name("staff", pnote->to_list)
		|| is_exact_name("slackers", pnote->to_list)))
	return true;

    if (is_exact_name(ch->name, pnote->to_list))
	return true;

    return false;
}


void note_attach(CHAR_DATA *ch, int type)
{
    NOTE_DATA *pnote;

    if (ch->pnote != NULL)
	return;

    pnote = new_note();

    pnote->next		= NULL;
    pnote->sender	= str_dup(ch->name);
    pnote->date		= str_dup("");
    pnote->to_list	= str_dup("");
    pnote->subject	= str_dup("");
    pnote->text		= str_dup("");
    pnote->type		= type;
    ch->pnote		= pnote;
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
