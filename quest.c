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
*       ROM 2.4 is copyright 1993-1995 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@pacinfo.com)                              *
*           Gabrielle Taylor (gtaylor@pacinfo.com)                         *
*           Brian Moore (rom@rom.efn.org)                                  *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
*  Automated Quest code written by Vassago of MOONGATE, moongate.ams.com   *
*  4000. Copyright (c) 1996 Ryan Addams, All Rights Reserved. Use of this  *
*  code is allowed provided you add a credit line to the effect of:        *
*  "Quest Code (c) 1996 Ryan Addams" to your logon screen with the rest    *
*  of the standard diku/rom credits. If you use this or a modified version *
*  of this code, let me know via email: moongate@moongate.ams.com. Further *
*  updates will be posted to the rom mailing list. If you'd like to get    *
*  the latest version of quest.c, please send a request to the above add-  *
*  ress. Quest Code v2.00.                                                 *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "magic.h"
#include "tables.h"

#define QUESTPART_GETITEM	1
#define QUESTPART_RESCUE	2
#define QUESTPART_SLAY		3
#define QUESTPART_GOTO		4
#define QUESTPART_SCRIPT	5
#define QUESTPARTS_BUILTIN	4

/* Roscharch's items */
const long quest_item_table[] =
{
    100006,
    100007,
    100106,
    100010,
    100110,
    100056,
    100057,
    100047,
    100060,
    0
};


/* King Alemnos's items */
const long quest2_item_table[] =
{
    100033,
    100034,
    100035,
    100037,
    100038,
    100039,
    100059,
    100090,
    100098,
    100109,
    0
};


/* crap token items */
const long quest_item_token_table[] =
{
    100100,
    100101,
    100102,
    100103,
    100104,
    100105,
    0
};


void do_quest(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *mob;
//	OBJ_DATA *obj = NULL;
	char buf[MAX_STRING_LENGTH];
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);

	if (arg1[0] == '\0')
	{
		send_to_char("QUEST commands: POINTS INFO TIME REQUEST COMPLETE RENEW.\n\r", ch);
		send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
		return;
	}

	//
	// QUEST INFO
	//
	if (!str_cmp(arg1, "info"))
	{
		QUEST_PART_DATA *part;
		int i;
		int total_parts;
		bool totally_complete = FALSE;
		bool found = FALSE;

		total_parts = 0;

		if (ch->quest == NULL)
		{
			send_to_char("You are not on a quest.\n\r", ch);
			return;
		}

		part = ch->quest->parts;
		while(part != NULL)
		{
			if (!part->complete)
				found = TRUE;
			total_parts++;
			part = part->next;
		}

		if (!found)
			totally_complete = TRUE;

		i = 1;
		for (part = ch->quest->parts; part != NULL; part = part->next, i++)
		{
			if (part->complete)
			{
				sprintf(buf, "You have completed task {Y%d{x of your quest!\n\r", i);
				send_to_char(buf, ch);
			}
			else
			{
				sprintf(buf, "Task {Y%d{x of your quest is not complete.\n\r", i);
				totally_complete = FALSE;
				send_to_char(buf, ch);
			}
		}

		if (totally_complete)
		{
			sprintf(buf, "{YYour quest is complete!{x\n\r"
				"Get back to %s before your time runs out!\n\r",
				get_mob_index(ch->quest->questgiver)->short_descr);
			send_to_char(buf, ch);
		}
		return;
	}


	//
	// QUEST POINTS
	//
	if (!str_cmp(arg1, "points"))
	{
		sprintf(buf, "You have {Y%d{x quest points.\n\r", ch->questpoints);
		send_to_char(buf, ch);
		return;
	}


	//
    // Quest time
    //
	if (!str_cmp(arg1, "time"))
	{
		if (!IS_QUESTING(ch))
		{
			if (ch->nextquest > 1)
			{
				sprintf(buf, "There are %d minutes remaining until you can "
					"go on another quest.\n\r", ch->nextquest);
				send_to_char(buf, ch);
			}
			else if (ch->nextquest == 1)
			{
				sprintf(buf, "There is less than a minute remaining until "
					"you can go on another quest.\n\r");
				send_to_char(buf, ch);
			}
			else if (ch->nextquest == 0)
				send_to_char("You aren't currently on a quest.\n\r",ch);
		}
		else if (ch->countdown > 0)
		{
			sprintf(buf, "Time left for current quest: {Y%d{x minutes.\n\r",
				ch->countdown);
			send_to_char(buf, ch);
		}
		return;
	}

    /* For the following functions, a QM must be present. */
    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_NPC(mob) && mob->pIndexData->pQuestor != NULL)
		    break;
    }

    if (mob == NULL)
    {
        send_to_char("You can't do that here.\n\r",ch);
        return;
    }

	//
	// Quest renew - adjust level for 1/10 the price
	//
    if (!str_cmp(arg1, "renew"))
    {
		OBJ_DATA *obj;
//		bool fQuestItem = FALSE;
//		int i;
		int cost;

		if (arg2[0] == '\0')
		{
			sprintf(buf, "Renew what, %s?", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		if ((obj = get_obj_carry(ch, arg2, ch)) == NULL)
		{
			sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		cost = obj->cost/10;
		cost = UMAX(cost, 1);

		mob->tempstore[0] = obj->cost;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_QUEST_PRERENEW, NULL))
			return;

		cost = mob->tempstore[0];
		if( cost <= 0 )
		{
			sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}


		if (ch->questpoints < cost)
		{
			sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that item.", pers(ch, mob), cost);
			do_say(mob, buf);
			return;
		}

		sprintf(buf, "You renew $p to $N for %d quest points.", cost);
		act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		act("$n shows $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("$N chants a mantra over $p, then hands it back to $n.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_QUEST_RENEW, NULL);

		ch->questpoints -= cost;

#if 0
        if (!fQuestItem || obj->catalyst)
	{
	    sprintf(buf, "That would be quite counter-productive, %s.",
		    pers(ch, mob));
	    do_say(mob, buf);
	    return;
	}



	obj->level = ch->tot_level;
	if (obj->item_type == ITEM_ARMOUR)
	    set_armour_obj(obj);

	ch->questpoints -= cost;
#endif
		return;
    }



	//
	// Quest Request
	//
    if (!str_cmp(arg1, "request"))
    {
		if (!IS_AWAKE(ch))
		{
			send_to_char("In your dreams, or what?\n\r", ch);
			return;
		}

		act("$n asks $N for a quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act ("You ask $N for a quest.",ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		if (IS_QUESTING(ch))
		{
			sprintf(buf, "But you're already on a quest!");
			do_say(mob, buf);
			return;
		}

		if (IS_DEAD(ch))
		{
			sprintf(buf, "You must come back to the world of the living first, %s.", HANDLE(ch));
			do_say(mob, buf);
			return;
		}

		if (ch->nextquest > 0 && !IS_IMMORTAL(ch) && port != PORT_RAE)
		{
			sprintf(buf, "You're very brave, %s, but let someone else have a chance.", ch->name);
			if (mob == NULL)
			{
				sprintf(buf, "do_quest(), quest request: MOB Was null, %s.\n\r", ch->name);
				bug (buf, 0);
				return;
			}

			do_say(mob, buf);
			sprintf(buf, "Come back later.");
			do_say(mob, buf);
			return;
		}

		ch->quest = new_quest();
		ch->quest->questgiver = mob->pIndexData->vnum;
		if (generate_quest(ch, mob))
		{
			ch->quest->generating = FALSE;

			sprintf(buf, "Thank you, brave %s!", HANDLE(ch));
			do_say(mob, buf);
		}
		else
		{
			sprintf(buf, "I'm sorry, %s, but I don't have any quests for you to do. Try again later.", ch->name);
			ch->nextquest = 3;
			do_say(mob, buf);
			free_quest(ch->quest);
			ch->quest = NULL;
			return;
		}

		if (IS_QUESTING(ch))
		{
			QUEST_PART_DATA *qp;
			int i = 0;

			for (qp = ch->quest->parts; qp != NULL; qp = qp->next)
				i++;

			ch->countdown = i * number_range(10,20);

			sprintf(buf, "You have %d minutes to complete this quest.", ch->countdown);
			do_say(mob, buf);
		}

		return;
	}

	//
	// Quest complete
	//
    if (!str_cmp(arg1, "complete"))
    {
		QUEST_PART_DATA *part;
		bool found;
		bool incomplete;
		int reward;
		int pointreward;
		int pracreward;
		int expreward;
		int i;

		if (!IS_AWAKE(ch))
		{
			send_to_char("In your dreams, or what?\n\r", ch);
			return;
		}

		if (ch->quest == NULL ||
			ch->quest->questgiver != mob->pIndexData->vnum)
        {
			sprintf(buf, "I never sent you on a quest! "
	    		"Perhaps you're thinking of someone else.");
			do_say(mob,buf);
			return;
		}

		act("$n informs $N $e has completed $s quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act ("You inform $N you have completed your quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		found = FALSE;
		incomplete = FALSE;
		for (part = ch->quest->parts; part != NULL; part = part->next)
		{
			if (part->complete)
				found = TRUE;
			if (!part->complete)
				incomplete = TRUE;
		}

		if (!found)
		{
			sprintf(buf,
				"I am most displeased with your efforts, %s! This is "
				"obviously a job for someone with more talent than you.",
				ch->name);
			do_say(mob, buf);

			p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);

			free_quest(ch->quest);
			ch->quest = NULL;
			ch->countdown = 0;

			mob->tempstore[0] = 10;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTQUEST, NULL);

			ch->nextquest = mob->tempstore[0];
			if(ch->nextquest < 1) ch->nextquest = 1;

			return;
		}

		pointreward = 0;
		reward = 0;
		pracreward = 0;
		expreward = 0;
		i = 0;

		log_string("quest.c, do_quest: (complete) Checking quest parts...");

		// Add up all the different rewards.
		for (part = ch->quest->parts; part != NULL; part = part->next)
		{
			i++;

			if (part->complete)
			{
				reward += number_range(500, 1000);
				pointreward += number_range(10,20);
				expreward += number_range(ch->tot_level*50,
				ch->tot_level*100);
				pracreward += 1;

				if (ch->pcdata->second_sub_class_warrior == CLASS_WARRIOR_CRUSADER)
				{
					pointreward += 5;
					if (number_percent() < 10)
					{
						pracreward += number_range(0, 1);
					}

					expreward += number_range(1000,5000);
				}
			}

			// If object, return the object.
			if (part->pObj != NULL)
			{
				if (ch == part->pObj->carried_by)
				{
					act("You hand $p to $N.",ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_CHAR);
					act("$n hands $p to $N.",ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_ROOM);

					extract_obj(part->pObj);

					part->pObj = NULL;
				}
			}
		}

		if (!incomplete)
		{
			sprintf(buf, "Congratulations on completing your quest!");
			do_say(mob,buf);
			ch->pcdata->quests_completed++;
		}
		else
		{
		    sprintf(buf, "I see you haven't fully completed your quest, "
			    "but I applaud your courage anyway!");
		    do_say(mob,buf);
		    pracreward -= number_range(2,5);
			pointreward -= number_range(10,20);
			pracreward = UMAX(pracreward,0);
			pointreward = UMAX(pointreward, 0);
		}

		mob->tempstore[0] = expreward;			// Experience
		mob->tempstore[1] = pointreward;		// QP
		mob->tempstore[2] = pracreward;			// Practices
		mob->tempstore[3] = reward;				// Silver
		if(incomplete)
			p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);
		else
			p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_COMPLETE, NULL);
		expreward = mob->tempstore[0];
		pointreward = mob->tempstore[1];
		pracreward = mob->tempstore[2];
		reward = mob->tempstore[3];

		// Clamp to zero
		expreward = UMAX(expreward,0);
		reward = UMAX(reward,0);
		pracreward = UMAX(pracreward,0);
		pointreward = UMAX(pointreward, 0);

		if (boost_table[BOOST_QP].boost != 100)
			pointreward = (pointreward * boost_table[BOOST_QP].boost)/100;


		sprintf(buf, "As a reward, I am giving you %d quest points and %d silver.",
			pointreward, reward);
		do_say(mob,buf);

		// Only display "QUEST POINTS boost!" if a qp boost is active -- Areo
		if(boost_table[BOOST_QP].boost != 100)
			send_to_char("{WQUEST POINTS boost!{x\n\r", ch);

		ch->silver += reward;
		ch->questpoints += pointreward;

		if (number_percent() < 90 && pracreward > 0)
		{
			sprintf(buf, "You gain %d practices!\n\r", pracreward);
			send_to_char(buf, ch);
			ch->practice += pracreward;
		} else { /* AO don't nerf it completely */
			pracreward /= number_range(1,4);
			pracreward = UMAX(1,pracreward);

			sprintf(buf, "You gain %d practices!\n\r", pracreward);
			send_to_char(buf, ch);
			ch->practice += pracreward;
		}

		if(ch->tot_level < 120)
		{
			sprintf(buf, "You gain %d experience points!\n\r", expreward);
			send_to_char(buf, ch);

			gain_exp(ch, expreward);
		}
/* Syn - disabling
  send_to_char("You receive 1 military quest point!\n\r", ch);
  award_ship_quest_points(ch->in_room->area->place_flags, ch, 1);
*/

		mob->tempstore[0] = 10;
		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTQUEST, NULL);

		ch->nextquest = mob->tempstore[0];
		if(ch->nextquest < 1) ch->nextquest = 1;

		ch->countdown = 0;	// @@@NIB Not doing this was causing nextquest to come up
							//	10 minutes if nextquest had expired
		free_quest(ch->quest);
		ch->quest = NULL;
    }
    else
    {
		send_to_char("QUEST commands: POINTS INFO TIME REQUEST COMPLETE RENEW.\n\r", ch);
		send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
    }
}


/*
 * Generate a quest. Returns TRUE if a quest is found.
 */
bool generate_quest(CHAR_DATA *ch, CHAR_DATA *questman)
{
	char buf[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH*8];
	QUEST_PART_DATA *part;
	OBJ_DATA *scroll;
	int parts;
	int i;

	ch->quest->generating = TRUE;

	if (ch->tot_level <= 30)
		parts = number_range(1, 3);
	else if (ch->tot_level <= 60)
		parts = number_range(3, 6);
	else if (ch->tot_level <= 90)
		parts = number_range(7, 9);
	else
		parts = number_range(8, 15);

	/* fun */
	bool bFun = number_percent() < 5;
	if (bFun)
		parts = parts * 2;

	// MORE FUN
	questman->tempstore[0] = parts;				// Number of parts to do (In-Out)
	questman->tempstore[1] = bFun ? 1 : 0;		// Whether this was a F.U.N. quest (In)
	if(p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_PREQUEST, NULL))
		return FALSE;
	parts = questman->tempstore[0];				// Updated number of parts to do
	if( parts < 1 ) parts = 1;					//    Require at least one part.




	// create the scroll
	scroll = create_object(get_obj_index(OBJ_VNUM_QUEST_SCROLL), 0, TRUE);
	free_string(scroll->full_description);

	QUESTOR_DATA *qd = questman->pIndexData->pQuestor;

	char *replace1 = string_replace_static(qd->header, "$PLAYER$", ch->name);
	char *replace2 = string_replace_static(replace1, "$QUESTOR$", questman->short_descr);

	strcpy(buf2, replace2);

	/*
	sprintf(buf2,
		"{W  .-.--------------------------------------------------------------------------------------.-.\n\r"
		"((o))                                                                                         )\n\r"
		"{W \\U/_________________________________________________________________________________________/\n\r"
		"{W  |\n\r"
		"{W  |  {xNoble %s{x,\n\r{W  |\n\r"
		"{W  |  {xThis is an official quest scroll given to you by %s.\n\r"
		"{W  |  {xUpon this scroll is my seal, and my approval to go to any\n\r"
		"{W  |  {xmeasures in order to complete the set of tasks I have listed.\n\r"
		"{W  |  {xReturn to me once you have completed these tasks, and you\n\r"
		"{W  |  {xshall be justly rewarded.\n\r{W  |  {x\n\r",
		ch->name, questman->short_descr);
	*/

	for (i = 0; i < parts; i++)
	{
		part = new_quest_part();
		part->next = ch->quest->parts;
		ch->quest->parts = part;
		part->index = parts - i;

		if (generate_quest_part(ch, questman, part, parts - i))
			continue;
		else
		{
			free_obj(scroll); // no memory leak now
			return FALSE;
		}
	}

	/* Moving all quest-scroll generation shit into here. AO 010517  */
	for (i = 1, part = ch->quest->parts; part != NULL; part = part->next, i++)
	{
		if( qd->line_width > 0 && strlen(qd->suffix) > 0 )
		{
			char *plaintext = nocolour(part->description);
			int plen = strlen(plaintext);
			free_string(plaintext);
			int len = strlen(part->description);

			int width = qd->line_width + len - plen;



			sprintf(buf, "%s%-*.*s%s\n\r", qd->prefix, width, width, part->description, qd->suffix);
		}
		else
		{
			sprintf(buf, "%s%s\n\r", qd->prefix, part->description);
		}
		strcat(buf2, buf);

#if 0
		if (part->pObj != NULL)
		{
			sprintf(buf, "Retrieve {Y%s{x from {Y%s{x in {Y%s{x.\n\r",
				part->pObj->short_descr, part->pObj->in_room->name, part->pObj->in_room->area->name);
		}
		else if (part->mob_rescue != -1)
		{
			mob = get_mob_index(part->mob_rescue);

			victim = get_char_world_index(NULL, mob);

			sprintf(buf, "Rescue {Y%s{x from {Y%s{x in {Y%s{x.\n\r",
				victim->short_descr,
				victim->in_room->name,
				victim->in_room->area->name);
		}
		else if (part->mob != -1)
		{
			mob = get_mob_index(part->mob);

			victim = get_char_world_index(NULL, mob);

			sprintf(buf, "Slay {Y%s{x.  %s was last seen in {Y%s{x.\n\r",
				victim->short_descr,
				victim->sex == SEX_MALE ? "He" :
				victim->sex == SEX_FEMALE ? "She" : "It",
				victim->in_room->area->name);
		}
		else if (part->room != -1)
		{
			ROOM_INDEX_DATA *room = get_room_index(part->room);

			sprintf(buf, "Travel to {Y%s{x in {Y%s{x.\n\r", room->name, room->area->name);
		}
		else if ( !IS_NULLSTR(part->custom_task) )
			sprintf(buf, "%s{x\n\r", part->custom_task);
#endif
	}

	/*
    sprintf(buf, "{W  |__________________________________________________________________________________________\n\r"
	    "{W /A\\                                                                                         \\\n\r"
	    "((o))                                                                                         )\n\r"
	    "{W  '-'----------------------------------------------------------------------------------------'\n\r");*/

	replace1 = string_replace_static(qd->footer, "$PLAYER$", ch->name);
	replace2 = string_replace_static(replace1, "$QUESTOR$", questman->short_descr);
	strcat(buf2, replace2);


    scroll->full_description = str_dup(buf2);

    act("$N gives $p to $n.", ch, questman, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
    act("$N gives you $p.",   ch, questman, NULL, scroll, NULL, NULL, NULL, TO_CHAR);
    obj_to_char(scroll, ch);
    return TRUE;
}

/* Set up a quest part. */
bool generate_quest_part(CHAR_DATA *ch, CHAR_DATA *questman, QUEST_PART_DATA *part, int partno)
{
	questman->tempstore[0] = partno;							// Which quest part *IS* this?  Needed for the "questcomplete" command
	if(p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_PART, NULL))
		return FALSE;


#if 0
    OBJ_DATA *item;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *rand_room;
    int rand;

    if (ch == NULL)
    {
		bug("generate_quest_part: ch NULL", 0);
		return FALSE;
    }

	int task_type = number_range(1, QUESTPARTS_BUILTIN + extra_tasks);
    switch (task_type)
    {
	case QUESTPART_GETITEM:
	    rand = number_range(0,5);

	    item = create_object(get_obj_index(quest_item_token_table[rand]), 0, FALSE);
	    item->owner = str_dup(ch->name);
	    part->pObj = item;

	    rand_room = NULL;
	    while (rand_room == NULL) {
			if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
				rand_room = get_random_room(ch, FIRST_CONTINENT);
			else
				rand_room = get_random_room(ch, SECOND_CONTINENT);
	    }

	    obj_to_room(item, rand_room);

	    part->obj = item->pIndexData->vnum;
	    break;

	case QUESTPART_RESCUE:
		if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
			victim = get_random_mob(ch, FIRST_CONTINENT);
		else
			victim = get_random_mob(ch, SECOND_CONTINENT);

		if (victim == NULL)
			return FALSE;

		part->mob_rescue = victim->pIndexData->vnum;
		break;

	case QUESTPART_SLAY:
		if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
		    victim = get_random_mob(ch, FIRST_CONTINENT);
		else
		    victim = get_random_mob(ch, SECOND_CONTINENT);

		if (victim == NULL)
		    return FALSE;

		part->mob = victim->pIndexData->vnum;
		break;

	case QUESTPART_GOTO:
		if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
			rand_room = get_random_room(ch, FIRST_CONTINENT);
		else
			rand_room = get_random_room(ch, SECOND_CONTINENT);

		part->room = rand_room->vnum;
		/* AO 010417 Will implement a "Travel to <place> and do <shit>, but this is a start */
		break;

	// SCRIPTED TASKS
	default:

		questman->tempstore[0] = partno;							// Which quest part *IS* this?  Needed for the "questcomplete" command
		questman->tempstore[1] = task_type - QUESTPARTS_BUILTIN;	// If extra_tasks is 5, I want 5-9 to become 1-5 in the script
		if(p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_PART, NULL))
			return FALSE;

		// Did not get a quest part
		if(IS_NULLSTR(questman->tempstring))
			return FALSE;

		part->custom_task = questman->tempstring;
		questman->tempstring = NULL;

		break;
    }
#endif

    return TRUE;
}


/* Called from update_handler() by pulse_area */
void quest_update(void)
{
    DESCRIPTOR_DATA *d;
    CHAR_DATA *ch;
    char buf[MAX_STRING_LENGTH];
    log_string("Update quests...");

    for (d = descriptor_list; d != NULL; d = d->next)
    {
	if (d->character != NULL && d->connected == CON_PLAYING)
	{
	    ch = d->character;

	    if (ch->quest == NULL && ch->nextquest > 0)
	    {
		ch->nextquest--;
		if (ch->nextquest == 0)
		{
		    send_to_char("{WYou may now quest again.{x\n\r",ch);
		    return;
		}
	    }
	    else
	    if (IS_QUESTING(ch))
	    {
		if (--ch->countdown <= 0)
		{
		    ch->nextquest = 0;
		    sprintf(buf,
			"{RYou have run out of time for your quest!\n\r"
			"You may quest again in %d minutes.{x\n\r",ch->nextquest);
		    send_to_char(buf, ch);
		    free_quest(ch->quest);
		    ch->quest = NULL;
		}

		if (ch->countdown > 0 && ch->countdown < 6)
		{
		    sprintf(buf, "You only have {Y%d{x minutes remaining to "
			    "finish your quest!\n\r", ch->countdown);
		    send_to_char(buf, ch);
		    return;
		}
	    }
	}
    }
}


bool is_quest_token(OBJ_DATA *obj)
{
    int i = 0;

    for (; quest_item_token_table[i] != 0; i++)
    {
        if (obj->pIndexData->vnum == quest_item_token_table[i])
	    return TRUE;
    }

    return FALSE;
}


void check_quest_rescue_mob(CHAR_DATA *ch)
{
    QUEST_PART_DATA *part;
    CHAR_DATA *mob;
    char buf[MAX_STRING_LENGTH];
    int i;
    bool found = TRUE;

    if (ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
		bug("check_quest_rescue_mob: NPC", 0);
		return;
    }

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;

		// already did it
		if (part->complete == TRUE)
			continue;

		found = FALSE;
		mob = ch->in_room->people;
		while (mob != NULL)
		{
			if (IS_NPC(mob) && mob->pIndexData->vnum == part->mob_rescue && !part->complete)
		    {
		        sprintf(buf, "Thank you for rescuing me, %s!", ch->name);
		        do_say(mob, buf);

				if (mob->master != NULL)
					stop_follower(mob,TRUE);

				add_follower(mob, ch, TRUE);

				if (IS_NPC(mob) && IS_SET(mob->act, ACT_AGGRESSIVE))
				    REMOVE_BIT(mob->act, ACT_AGGRESSIVE);

				found = TRUE;
				break;
		    }

		    mob = mob->next_in_room;
		}

		if (found && !part->complete)
		{
			sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
			send_to_char(buf, ch);

			part->complete = TRUE;
			break;
		}
    }
}


void check_quest_retrieve_obj(CHAR_DATA *ch, OBJ_DATA *obj)
{
    QUEST_PART_DATA *part;
    int i;

    if (obj == NULL || obj->item_type == ITEM_MONEY)
    {
		bug("check_quest_retrieve_obj: bad obj!", 0);
		return;
    }

    if (IS_NPC(ch))
    {
		bug("check_quest_retrieve_obj: NPC", 0);
		return;
    }

    if (ch->quest != NULL)
    {

		i = 0;
		for (part = ch->quest->parts; part != NULL; part = part->next)
		{
			i++;

			// already did it
			if (part->complete == TRUE)
				continue;

			if (part->pObj == obj)
			{
				char buf[MAX_STRING_LENGTH];

				part->complete = TRUE;

				sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
				send_to_char(buf, ch);
			}
		}
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob)
{
    QUEST_PART_DATA *part;
    int i;

    if (ch->quest == NULL || !IS_NPC(mob))
        return;

    if (IS_NPC(ch))
    {
		bug("check_quest_slay_mob: NPC", 0);
		return;
    }

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;

		// already did it
		if (part->complete == TRUE)
			continue;

        if (part->mob == mob->pIndexData->vnum && !part->complete)
		{
			char buf[MAX_STRING_LENGTH];

			sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
			send_to_char(buf, ch);

			part->complete = TRUE;
		}
    }
}

void check_quest_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room)
{
    QUEST_PART_DATA *part;
    ROOM_INDEX_DATA *target_room;
    int i;

    if (ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
		bug("check_quest_travel_room: NPC", 0);
		return;
    }

    if (room == NULL)
    {
		bug("check_quest_travel_room: checking a null room",0);
		return;
    }

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;

		// already did it
		if (part->complete == TRUE)
			continue;

		target_room = get_room_index(part->room);

		/* Not going by room vnum to prevent multiple rooms with the same name */
		if (target_room != NULL && !str_cmp(target_room->name, room->name))
		{
			char buf[MAX_STRING_LENGTH];

			sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
			send_to_char(buf, ch);

			part->complete = TRUE;
		}
    }
}

bool check_quest_custom_task(CHAR_DATA *ch, int task)
{
	QUEST_PART_DATA *part;
	int i;

	if (ch->quest == NULL)
		return FALSE;

    if (IS_NPC(ch))
    {
		bug("check_quest_custom_task: NPC", 0);
		return FALSE;
    }

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;

		// Not the current task nor is a custom task
        if( task != i || !part->custom_task )
			continue;

		// already did it
		if (part->complete == TRUE)
	    	continue;

	    char buf[MAX_STRING_LENGTH];

	    sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
	    send_to_char(buf, ch);
	    part->complete = TRUE;

	    return TRUE;
    }

    return FALSE;
}

int count_quest_parts(CHAR_DATA *ch)
{
    QUEST_PART_DATA *part;
    int parts;

    if (ch->quest == NULL)
        return 0;

    parts = 1;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        parts++;
    }

    return parts;
}


bool is_quest_item(OBJ_DATA *obj)
{
    int i;

    for (i = 0; quest_item_table[i] != 0; i++)
    {
	if (obj->pIndexData->vnum == quest_item_table[i])
	    return TRUE;
    }

    for (i = 0; quest2_item_table[i] != 0; i++)
    {
	if (obj->pIndexData->vnum == quest2_item_table[i])
	    return TRUE;
    }

    return FALSE;
}


QUEST_INDEX_DATA *get_quest_index(long vnum)
{
/*    QUEST_INDEX_DATA *quest_index;

    for (quest_index = quest_index_list; quest_index != NULL;
          quest_index = quest_index->next)
    {
	if (quest_index->vnum == vnum)
	    return quest_index;
    }
*/
    return NULL;
}


void check_quest_part_complete(CHAR_DATA *ch, int type)
{
}
