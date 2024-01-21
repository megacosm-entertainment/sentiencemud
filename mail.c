/* Mail system
   Copyright 2004 Anton Ouzilov
*/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "math.h"
#include "recycle.h"
#include "tables.h"

void do_mailadd(CHAR_DATA *ch, char *argument);
void do_mailcancel(CHAR_DATA *ch, char *argument);
void do_mailinfo(CHAR_DATA *ch, char *argument);
void do_mailrem(CHAR_DATA *ch, char *argument);
void do_mailsend(CHAR_DATA *ch, char *argument);
void do_mailshow(CHAR_DATA *ch, char *argument);
void do_mailto(CHAR_DATA *ch, char *argument);
void do_mailwrite(CHAR_DATA *ch, char *argument);

void do_mail(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];

    argument = one_argument(argument, arg);

    if (ch->in_room != get_room_index(ch->in_room->area, ch->in_room->area->region.post_office)
    && !IS_SET(ch->in_room->room_flag[1], ROOM_POST_OFFICE))
    {
	send_to_char("You must be at a post office.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Syntax:\n\r"
		"mail to <person>  (start a mail package)\n\r"
		"mail show         (show what is currently in the package)\n\r"
		"mail put <object> (add an object to the package)\n\r"
		"mail get <object> (remove an object from the package)\n\r"
		"mail write        (write a message)\n\r"
		"mail cancel       (cancel the mail)\n\r"
		"mail send         (pay postage and send the package)\n\r"
		"mail check        (check your mail)\n\r"
		"mail info         (see the status of mail you've sent/received)\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "to"))
    {
	do_function(ch, &do_mailto, argument);
	return;
    }

    if (!str_cmp(arg, "show"))
    {
	do_function(ch, &do_mailshow, argument);
	return;
    }

    if (!str_cmp(arg, "add") || !str_cmp(arg, "put"))
    {
	do_function(ch, &do_mailadd, argument);
	return;
    }

    if (!str_cmp(arg, "get") || !str_cmp(arg, "rem"))
    {
	do_function(ch, &do_mailrem, argument);
	return;
    }

    if (!str_cmp(arg, "check"))
    {
	check_new_mail(ch);
	return;
    }

    if (!str_cmp(arg, "write"))
    {
	do_function(ch, &do_mailwrite, argument);
	return;
    }

    if (!str_cmp(arg, "cancel"))
    {
	do_function(ch, &do_mailcancel, argument);
	return;
    }

    if (!str_cmp(arg, "send"))
    {
	do_function(ch, &do_mailsend, argument);
	return;
    }

    if (!str_cmp(arg, "info"))
    {
	do_function(ch, &do_mailinfo, argument);
	return;
    }

    do_function(ch, &do_mail, "");
}


/* make a new package to somebody */
void do_mailto(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    MAIL_DATA *mail;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Send a package to whom?\n\r", ch);
	return;
    }

    if (!player_exists(arg))
    {
	send_to_char("There is no such player.\n\r", ch);
	return;
    }

    if (ch->mail != NULL)
    {
	send_to_char("You're already working on a mail package, send that one first!\n\r", ch);
	return;
    }

    mail = new_mail();

    ch->mail = mail;

    arg[0] = UPPER(arg[0]);

    mail->sender = str_dup(ch->name);
    mail->recipient = str_dup(arg);
}


/* show what's in the package */
void do_mailshow(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    BUFFER *buffer;
    MAIL_DATA *mail;
    OBJ_DATA *obj;
    int i = 0;
    int weight = 0;

    if ((mail = ch->mail) == NULL)
    {
	send_to_char("You aren't putting together a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    buffer = new_buf();

    sprintf(buf, "You look in your package to %s and see:\n\r\n\r",
        mail->recipient);
    add_buf(buffer, buf);

    sprintf(buf, "{Y%2s %-30s%4s %-4s{x\n\r", "#", "Item", "Inside", "Wt");
    add_buf(buffer, buf);

    add_buf(buffer, "{Y------------------------------------------------------{x\n\r");

    for (obj = mail->objects; obj != NULL; obj = obj->next_content)
    {
	i++;
	weight += obj->weight;

        if (obj->contains == NULL)
	    sprintf(buf, "{Y%2d{x %-36s %-4d\n\r", i, obj->short_descr, get_obj_weight(obj));
        else
	{
	    sprintf(buf, "{Y%2d{x %-30.30s{Y[{x%-2d{Y]{x   %-4d\n\r", i, obj->short_descr,
	        count_items_list_nest(obj->contains), (get_obj_weight_container(obj) * WEIGHT_MULT(obj))/100);
	}

	add_buf(buffer, buf);
    }

    if (i == 0)
   	add_buf(buffer, "Nothing.\n\r");

    add_buf(buffer, "{Y------------------------------------------------------{x\n\r");

    sprintf(buf, "A total of {Y%d{x items (max is {Y%d{x), totaling {Y%d{x kg in weight (max is {Y%d{x kg).\n\r",
        count_items_mail(mail), MAX_POSTAL_ITEMS, count_weight_mail(mail), MAX_POSTAL_WEIGHT);
    add_buf(buffer, buf);

    if (count_items_mail(mail) > 0)
    {
	sprintf(buf, "This package will cost {Y%d{x silver in postage to send.\n\r",
		count_weight_mail(mail) * SILVER_PER_KG + 50);
	add_buf(buffer, buf);
    }

    if (mail->message != NULL)
    {
	add_buf(buffer, "\n\r{YYou have included the following message with this mail:{x\n\r");
	add_buf(buffer, mail->message);
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/* add an item to the package */
void do_mailadd(CHAR_DATA *ch, char *argument)
{
    MAIL_DATA *mail;
    char arg[MSL];
    char buf[MSL];
    OBJ_DATA *obj, *obj_next;

    if ((mail = ch->mail) == NULL)
    {
	send_to_char("You aren't working on a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Add what to the package?\n\r", ch);
	return;
    }

    // Put obj mail
    if (str_cmp(arg, "all") && str_prefix("all.", arg))
    {
	if ((obj = get_obj_list(ch, arg, ch->carrying)) == NULL)
	{
	    send_to_char("You do not have that object.\n\r", ch);
	    return;
	}

	if (!can_put_obj(ch, obj, NULL, mail, false))
	    return;

	act("You put $p in your package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n puts $p in $s package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	obj_from_char(obj);
	obj_to_mail(obj, mail);
    }
    else // put all mail or put all.obj mail
    {
	int i = 0;
	char short_descr[MSL];
	bool found = true;
	bool any = false;
	OBJ_DATA *match_obj;

	while (found)
	{
	    found = false;
	    i = 0;
	    match_obj = NULL;

	    for (obj = ch->carrying; obj != NULL; obj = obj_next)
	    {
		obj_next = obj->next_content;

		if ((arg[3] == '\0' || is_name(&arg[4], obj->name))
		&&  can_put_obj(ch, obj, NULL, mail, true))
		{
		    sprintf(short_descr, "%s", obj->short_descr);
		    found = true;
		    any = true;
		    break;
		}
	    }

	    if (found)
	    {
		for (obj = ch->carrying; obj != NULL; obj = obj_next)
		{
		    obj_next = obj->next_content;

		    if (str_cmp(obj->short_descr, short_descr)
		    ||  !can_put_obj(ch, obj, NULL, mail, true))
			continue;

		    if (count_weight_mail(mail) >= MAX_POSTAL_WEIGHT)
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $s package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

			    sprintf(buf, "{Y({G%2d{Y) {xYou put $p in your package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
			}

			send_to_char("Your mail package is at the maximum allowable weight.\n\r", ch);
			return;
		    }

		    if (count_items_mail(mail) >= MAX_POSTAL_ITEMS)
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $s package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

			    sprintf(buf, "{Y({G%2d{Y) {xYou put $p in your package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
			}

			send_to_char("Your mail package has the maximum allowable number of items.\n\r", ch);
			return;
		    }

		    if (match_obj == NULL && obj != NULL)
			match_obj = obj;

		    obj_from_char(obj);
		    obj_to_mail(obj, mail);
		    i++;
		}

		if (i > 0 && match_obj != NULL)
		{
		    sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $s package.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

		    sprintf(buf, "{Y({G%2d{Y) {xYou put $p in your package.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
		}
	    }
	    else
	    {
		if (!any)
		{
		    if (arg[3] == '\0')
			act("You have nothing you can put in your package.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    else
			act("You're not carrying any $T you can put in your package.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR);
		}
	    }
	}
    }
}


/* remove an object from a package */
void do_mailrem(CHAR_DATA *ch, char *argument)
{
    MAIL_DATA *mail;
    OBJ_DATA *obj, *obj_next;
    char arg[MSL];

    if ((mail = ch->mail) == NULL)
    {
	send_to_char("You aren't working on a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Take what object out of the package?\n\r", ch);
	return;
    }

    // Mail get <item>
    if (str_cmp(arg, "all") && str_prefix("all.", arg))
    {
	if ((obj = get_obj_list(ch, arg, mail->objects)) == NULL)
	{
	    act("There's no $t in the package.", ch, NULL, NULL, NULL, NULL, arg, NULL, TO_CHAR);
	    return;
	}

	if (!can_get_obj(ch, obj, NULL, mail, false))
	    return;

	act("You take $p out of the package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n takes $p out of $s package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	obj_from_mail(obj);
	obj_to_char(obj, ch);
    }
    else
    {
	// Mail get all, all.<item>
	int i = 0;
	char buf[MSL];
	char short_descr[MSL];
	OBJ_DATA *match_obj;
	bool found = true;
	bool any = false;

	while (found)
	{
	    found = false;
	    i = 0;
	    match_obj = NULL;

	    for (obj = mail->objects; obj != NULL; obj = obj_next)
	    {
		obj_next = obj->next_content;

		if ((arg[3] == '\0' || is_name(&arg[4], obj->name))
		&&  can_get_obj(ch, obj, NULL, mail, true))
		{
		    sprintf(short_descr, "%s", obj->short_descr);
		    found = true;
		    any = true;
		    break;
		}
	    }

	    if (found)
	    {
		for (obj = mail->objects; obj != NULL; obj = obj_next)
		{
		    obj_next = obj->next_content;

		    if (str_cmp(obj->short_descr, short_descr)
		    ||  !can_get_obj(ch, obj, NULL, mail, true))
			continue;

		    if (get_carry_weight(ch) + get_obj_weight(obj) >= can_carry_w(ch))
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n takes $p out of $s package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

			    sprintf(buf, "{Y({G%2d{Y) {xYou take $p out of your package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);

			    send_to_char("You can't carry any more.\n\r", ch);
			}

			return;
		    }

		    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n takes $p out of $s package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

			    sprintf(buf, "{Y({G%2d{Y) {xYou take $p out of your package.", i);
			    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);

			    send_to_char("Your hands are full.\n\r", ch);
			}

			return;
		    }

		    if (match_obj == NULL && obj != NULL)
			match_obj = obj;

                    obj_from_mail(obj);
		    obj_to_char(obj, ch);
		    i++;
		}

		if (i > 0 && match_obj != NULL)
		{
		    sprintf(buf, "{Y({G%2d{Y) {x$n takes $p out of $s package.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

		    sprintf(buf, "{Y({G%2d{Y) {xYou take $p out of your package.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
		}
	    }
	    else
	    {
		if (!any)
		{
		    if (arg[3] == '\0')
			act("You have nothing in your package.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    else
			act("There is no $T in your package.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR);
		}
	    }
	}
    }
}


/* cancel the mail */
void do_mailcancel(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    if (ch->mail == NULL)
    {
	send_to_char("You aren't putting together a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    /* put objects back to person here */
    for (obj = ch->mail->objects; obj != NULL; obj = obj_next)
    {
	obj_next = obj->next_content;

	act("You take $p out of the package.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	obj_from_mail(obj);
	obj_to_char(obj, ch);
    }

    free_mail(ch->mail);
    ch->mail = NULL;
    send_to_char("Mail order cancelled.\n\r", ch);
}


/* include a message with the mail */
void do_mailwrite(CHAR_DATA *ch, char *argument)
{
    MAIL_DATA *mail;

    if ((mail = ch->mail) == NULL)
    {
	send_to_char("You aren't working on a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    if (mail->message == NULL)
    {
	mail->message = str_dup("");
	send_to_char("You take out a quill and begin writing a message.\n\r", ch);
    }
    else
	send_to_char("You take out a quill and look over your message.\n\r", ch);

    string_append(ch, &mail->message);
}


/* send the mail off. hooks it up to global mail_list */
void do_mailsend(CHAR_DATA *ch, char *argument)
{
    MAIL_DATA *mail;
    MAIL_DATA *mail_tmp;
    char buf[MSL];
    int cost;

    if ((mail = ch->mail) == NULL)
    {
	send_to_char("You aren't working on a mail package. Type 'mail to <person>'\n\r", ch);
	return;
    }

    /* pay postage */
    if ((cost = (count_weight_mail(mail) * SILVER_PER_KG) + 50) >
	   ch->silver + 100 * ch->gold)
    {
	sprintf(buf, "Sorry, it would cost %d silver in postage to send this item. You don't have enough.\n\r", cost);
	send_to_char(buf, ch);
	return;
    }

    if (mail->objects == NULL && mail->message == NULL)
    {
	send_to_char("There's nothing in your package to send.\n\r", ch);
	return;
    }

    deduct_cost(ch, cost);

    for (mail_tmp = mail_list; mail_tmp != NULL; mail_tmp = mail_tmp->next)
    {
    	if (mail_tmp->next == NULL)
            break;
    }

    /* hook it up to the list */
    ch->mail = NULL;
    mail->next = NULL;
    if (mail_list != NULL)
	mail_tmp->next = mail;
    else
        mail_list = mail;
    mail->sent_date = current_time;
    mail->status = MAIL_BEING_DELIVERED;

    send_to_char("Mail sent.\n\r", ch);
    write_mail();
}


/* returns weight of a weight package in kg */
int count_weight_mail(MAIL_DATA *mail)
{
    int weight = 0;
    OBJ_DATA *obj;

    if (mail == NULL)
    {
	bug("count_weight_mail: null mail!", 0);
	return -1;
    }

    for (obj = mail->objects; obj != NULL; obj = obj->next_content)
	weight += get_obj_weight(obj);

    return weight;
}


/* count number of items in the mail, including objects inside containers */
int count_items_mail(MAIL_DATA *mail)
{
    if (mail == NULL)
    {
	bug("count_items_mail: null mail!", 0);
	return -1;
    }

    return count_items_list_nest(mail->objects);
}


/* write mail.dat */
void write_mail(void)
{
    MAIL_DATA *mail;
    FILE *fp;

    fp = fopen(MAIL_FILE, "w");
    if (fp == NULL)
    {
	bug("Couldn't load mail.dat", 0);
	exit(1);
    }

    for (mail = mail_list; mail != NULL; mail = mail->next)
    {
	fprintf(fp, "#MAIL\n");
	fprintf(fp, "Sender %s~\n", mail->sender);
	fprintf(fp, "Recipient %s~\n", mail->recipient);
	fprintf(fp, "Sent %ld\n", (long int)mail->sent_date);
	fprintf(fp, "Status %d\n", mail->status);
	fprintf(fp, "PickedUp %d\n", mail->picked_up);
	if (mail->message != NULL)
	    fprintf(fp, "Message %s~\n\n", fix_string(mail->message));

        if (mail->objects != NULL)
	   fwrite_obj_new(NULL, mail->objects, fp, 0);

	fprintf(fp, "#END\n\n"); // end objects
    }

    fprintf(fp, "#ENDMAIL\n");
    fclose(fp);
}


/* load mail.dat into memory */
void read_mail(void)
{
    FILE *fp;
    MAIL_DATA *mail;
    MAIL_DATA *mail_tmp;
    OBJ_DATA *obj;
    char *word;
    OBJ_DATA *listObjNest[MAX_NEST];

    fp = fopen(MAIL_FILE, "r");
    if (fp == NULL)
    {
	bug("Couldn't read mail.dat", 0);
	exit(1);
    }

    for (; ;)
    {
	word = fread_word(fp);
	if (!str_cmp(word, "#MAIL"))
	{
	    mail = new_mail();

	    // read in objects
	    for (; ;)
	    {
		word = fread_word(fp);
		if (!str_cmp(word, "#O"  ))
		{
		    obj = fread_obj_new(fp);
		    listObjNest[obj->nest] = obj;
		    if (obj->nest == 0)
			obj_to_mail(obj, mail);
		    else
		    	obj_to_obj(obj, listObjNest[obj->nest - 1]);
		    continue;
		}
		else
		if (!str_cmp(word, "Sender"))
		    mail->sender = fread_string(fp);
		else
		if (!str_cmp(word, "Recipient"))
		    mail->recipient = fread_string(fp);
		else
		if (!str_cmp(word, "Sent"))
		    mail->sent_date = fread_number(fp);
		else
	        if (!str_cmp(word, "Message"))
		    mail->message = fread_string(fp);
		else
		if (!str_cmp(word, "Status"))
		    mail->status = fread_number(fp);
		else
		if (!str_cmp(word, "PickedUp"))
		    mail->picked_up = fread_number(fp);
		else
		if (!str_cmp(word, "#END"))
		    break;
	    }

	    for (mail_tmp = mail_list; mail_tmp != NULL; mail_tmp = mail_tmp->next)
	    {
	    	if (mail_tmp->next == NULL)
		    break;
	    }

	    mail->next = NULL;
	    if (mail_list == NULL)
	        mail_list = mail;
	    else
	    	mail_tmp->next = mail;
	}
	else
	if (!str_cmp(word, "#ENDMAIL"))
	    break;
    }

    fclose(fp);
}


/* returns true if person has mail in the mail_list */
bool has_mail(CHAR_DATA *ch)
{
    MAIL_DATA *mail;

    if (IS_NPC(ch))
	return false;

    if (ch == NULL)
    {
	bug("has_mail: ch null!", 0);
	return false;
    }

    for (mail = mail_list; mail != NULL; mail = mail->next)
    {
	if (!str_cmp(ch->name, mail->recipient)
	&& !mail->picked_up
	&& mail->status >= MAIL_DELIVERED)
	    return true;
    }

    return false;
}


/* check mail when person walks into post office. Transfer objs and message. */
void check_new_mail(CHAR_DATA *ch)
{
    MAIL_DATA *mail;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    OBJ_DATA *prototype;
    char buf[MSL];
    bool found = false;
    int i;

    for (; ;)
    {
	for (mail = mail_list; mail != NULL; mail = mail->next)
	{
	    if (!str_cmp(ch->name, mail->recipient)
            && !mail->picked_up
	    && mail->status >= MAIL_DELIVERED)
		break;
	}

	// Sorry no mail today
	if (mail == NULL)
	    break;
	else
	    found = true;

	sprintf(buf, "{xYou receive a package from %s.\n\r", mail->sender);
	send_to_char(buf, ch);
        act ("$n picks up a package.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	while (mail->objects)
	{
	    i = 0;
	    sprintf(buf, "%s", mail->objects->short_descr);
	    prototype = NULL;
	    for (obj = mail->objects; obj != NULL; obj = obj_next)
	    {
		obj_next = obj->next_content;

		if (!str_cmp(obj->short_descr, buf))
		{
		    i++;
		    obj_from_mail(obj);
		    obj_to_char(obj, ch);

		    if (!prototype)
			prototype = obj;
		}
	    }

	    if (i > 1)
	    {
		sprintf(buf, "{Y({G%2d{Y) {xYou take $p out of the package.", i);
		act(buf, ch, NULL, NULL, prototype, NULL, NULL, NULL, TO_CHAR);
	    }
	    else
		act("You take $p out of the package.", ch, NULL, NULL, prototype, NULL, NULL, NULL, TO_CHAR);
	}

	mail->picked_up = true;

	// Give them message
	if (mail->message != NULL)
	{
	    OBJ_DATA *parchment;
	    char buf[MSL];

	    parchment = create_object(obj_index_blank_scroll, 1, false);

	    free_string(parchment->name);
	    free_string(parchment->short_descr);
	    free_string(parchment->description);
	    free_string(parchment->full_description);

	    parchment->name = str_dup("papyrus scroll");
	    parchment->short_descr = str_dup("a papyrus scroll");
	    sprintf(buf, "A rolled-up papyrus scroll addressed to %s lies on the ground.", ch->name);
	    parchment->description = str_dup(buf);
	    parchment->full_description = str_dup(mail->message);

	    obj_to_char(parchment, ch);
	    act("You take $p out of the package.", ch, NULL, NULL, parchment, NULL, NULL, NULL, TO_CHAR);
	    act("$n takes $p out of the package.", ch, NULL, NULL, parchment, NULL, NULL, NULL, TO_ROOM);
	}
	//free_mail(mail);
    }

    if (found == true)
	write_mail();
}


/* show recent mails to and from you */
void do_mailinfo(CHAR_DATA *ch, char *argument)
{
    MAIL_DATA *mail;
    BUFFER *buffer;
    char buf[2*MSL];
    int count = 0;
    char status[MSL];

    buffer = new_buf();

    sprintf(buf, "{Y%-2s %-20s %-10s %-10s %-20s\n\r{x",
        "#",
	"Status",
	"Sender",
	"Recipient",
	"Date Sent");
    add_buf(buffer, buf);

    sprintf(buf, "{Y-----------------------------------------------------------------------------{x\n\r");
    add_buf(buffer, buf);

    for (mail = mail_list; mail != NULL; mail = mail->next)
    {
	if (!str_cmp(mail->sender, ch->name)
	|| !str_cmp(mail->recipient, ch->name))
	{
	    if (mail->status < MAIL_DELIVERED)
		sprintf(status, "{yBeing Delivered     {x");
	    else
	    if (mail->picked_up == true)
		sprintf(status, "{GPicked Up           {x");
	    else
		sprintf(status, "{RNot Picked Up       {x");

	    count++;
	    sprintf(buf, "{Y%-2d{x %-20s %-10s %-10s %-20s{x",
	    count,
	    status,
	    mail->sender,
	    mail->recipient,
	    ((char *) ctime(&mail->sent_date)));
	    add_buf(buffer, buf);
	}
    }

    if (count == 0)
	add_buf(buffer, "No mail found.\n\r");

    sprintf(buf, "{Y-----------------------------------------------------------------------------{x\n\r");
    add_buf(buffer, buf);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/* update mail list (this is to implement delivery times) */
void mail_update(void)
{
    MAIL_DATA *mail, *mail_next;
    char buf[MSL];

    for (mail = mail_list; mail != NULL; mail = mail_next)
    {
	mail_next = mail->next;

	mail->status++;
	sprintf(buf, "mail status from %d(%s->%s) to %d\n\r", mail->status - 1, mail->sender, mail->recipient, mail->status);

	wiznet(buf, NULL, NULL, WIZ_TESTING, 0, STAFF_IMPLEMENTOR);

	if (mail->status >= MAIL_BEING_DELIVERED
	&& mail->status < MAIL_DELIVERED)
	    continue;

	if (mail->status >= MAIL_DELETE)
	{
	    if (mail->picked_up)
	    {
		sprintf(buf, "mail_update: deleted mail from %s to %s",
			mail->sender, mail->recipient);
		log_string(buf);

		mail_from_list(mail);
		free_mail(mail);
	    }
	}

	if (mail->status == MAIL_RETURN)
	{
	    free_string(mail->recipient);
	    free_string(mail->message);
	    mail->message = str_dup("{RRETURNED MAIL:{x Package not picked up by recipient.\n\r");
	    mail->recipient = str_dup(mail->sender);
	    mail->status = MAIL_BEING_DELIVERED;
	    write_mail();
	}
    }
}


// put an obj in a mail package
void obj_to_mail(OBJ_DATA *obj, MAIL_DATA *mail)
{
    obj->next_content	 = mail->objects;
    mail->objects	 = obj;

    obj->in_room         = NULL;
    obj->carried_by      = NULL;
    obj->in_mail	 = mail;

    obj->pIndexData->inmail++;
}


// take an object out of a mail package
void obj_from_mail(OBJ_DATA *obj)
{
    OBJ_DATA *temp;
    OBJ_DATA *obj_prev = NULL;
    MAIL_DATA *mail;

    if ((mail = obj->in_mail) == NULL)
    {
	bug("obj_from_mail: obj not in a mail", 0);
	return;
    }

    for (temp = mail->objects; temp != NULL; temp = temp->next_content)
    {
	if (temp == obj)
	    break;

        obj_prev = temp;
    }

    if (obj_prev == NULL)
	mail->objects = obj->next_content;
    else
	obj_prev->next_content = obj->next_content;

    obj->in_mail = NULL;

	--obj->pIndexData->inmail;
}


// take a mail package out of the global list of mail
void mail_from_list(MAIL_DATA *mail)
{
    MAIL_DATA *mail_prev;

    if (mail == NULL)
    {
	bug("mail_from_list: null", 0);
	return;
    }

    if (mail == mail_list)
	mail_list = mail->next;
    else
    for (mail_prev = mail_list; mail_prev != NULL; mail_prev = mail_prev->next)
    {
	if (mail_prev->next == mail)
	{
	    mail_prev->next = mail->next;
	    break;
	}
    }
}
