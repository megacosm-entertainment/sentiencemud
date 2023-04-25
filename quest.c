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
#include "recycle.h"
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


OBJ_DATA *generate_quest_scroll(CHAR_DATA *ch, char *questgiver, WNUM wnum,
	char *header, char *footer, char *prefix, char *suffix, int line_width)
{
	OBJ_INDEX_DATA *scroll_index = get_obj_index(wnum.pArea, wnum.vnum);
	if( scroll_index == NULL )
	{
		scroll_index = obj_index_quest_scroll;
	}

	OBJ_DATA *scroll = create_object(scroll_index, 0, TRUE);
	if( scroll != NULL )
	{
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
			ch->name, questgiver);
		*/

		BUFFER *buffer = new_buf();
		char buf[MSL];

		// Need to add overflow protection
		char *replace1 = string_replace_static(header, "$PLAYER$", ch->name);
		char *replace2 = string_replace_static(replace1, "$QUESTOR$", questgiver);
		add_buf(buffer, replace2);

		for (QUEST_PART_DATA *part = ch->quest->parts; part != NULL; part = part->next)
		{
			if( line_width > 0 && !IS_NULLSTR(suffix) )
			{
				int width = line_width + get_colour_width(part->description);

				sprintf(buf, "%s%-*.*s%s\n\r", prefix, width, width, part->description, suffix);
			}
			else
			{
				sprintf(buf, "%s%s\n\r", prefix, part->description);
			}
			add_buf(buffer, buf);
		}

		/*
		sprintf(buf, "{W  |__________________________________________________________________________________________\n\r"
			"{W /A\\                                                                                         \\\n\r"
			"((o))                                                                                         )\n\r"
			"{W  '-'----------------------------------------------------------------------------------------'\n\r");*/

		// Need to add overflow protection
		replace1 = string_replace_static(footer, "$PLAYER$", ch->name);
		replace2 = string_replace_static(replace1, "$QUESTOR$", questgiver);
		add_buf(buffer, replace2);

		free_string(scroll->full_description);
		scroll->full_description = str_dup(buffer->string);

		free_buf(buffer);
	}

	return scroll;
}

void do_quest(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
//	OBJ_DATA *obj = NULL;
	char buf[MAX_STRING_LENGTH];
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);

	if (arg1[0] == '\0')
	{
		send_to_char("QUEST commands: POINTS INFO TIME REQUEST CANCEL COMPLETE.\n\r", ch);
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

		if (ch->quest->generating)
		{
			send_to_char("You are still waiting for your quest.\n\r",ch);
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
			send_to_char("{YYour quest is complete!{x\n\r"
				"Turn quest in before your time runs out!\n\r", ch);
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
		else if (ch->quest->generating)
		{
			send_to_char("You are still waiting for your quest.\n\r"
				"If you wish to abandon the pending quest, use QUEST CANCEL.\n\r",ch);
		}
		else if (ch->countdown > 0)
		{
			sprintf(buf, "Time left for current quest: {Y%d{x minutes.\n\r",
				ch->countdown);
			send_to_char(buf, ch);
		}
		return;
	}

	///////////////////////////////////////////
	//
	// Quest buy - [DEPRECATED] - leaving in for the time being
	//
    if (!str_cmp(arg1, "buy"))
    {
		send_to_char("{YQUEST BUY has been deprecated.  Please use the BUY command instead.\n\r", ch);
		return;
    }
	//
	// Quest sell - [DEPRECATED] - leaving in for the time being
	//
    if (!str_cmp(arg1, "sell"))
    {
		send_to_char("{YQUEST SELL has been deprecated.  Please use the SELL command instead.\n\r", ch);
		return;
    }
	//
	// Quest inspect - [DEPRECATED] - leaving in for the time being
	//
    if (!str_cmp(arg1, "inspect"))
    {
		send_to_char("{YQUEST INSPECT has been deprecated.  Please use the INSPECT command instead.\n\r", ch);
		return;
    }
	//
	// Quest renew - [DEPRECATED] - leaving in for the time being
	//
    if (!str_cmp(arg1, "renew"))
    {
		send_to_char("{YQUEST RENEW has been deprecated.  Please use the RENEW command instead.\n\r", ch);
		return;
    }
	//
	// Quest list - [DEPRECATED] - leaving in for the time being
	//
    if (!str_cmp(arg1, "list"))
    {
		send_to_char("{YQUEST LIST has been deprecated.  Please use the LIST command instead.\n\r", ch);
		return;
    }
	///////////////////////////////////////////

	//
	// Quest Request
	//
    if (!str_cmp(arg1, "request"))
    {
		/* For the following functions, a QM must be present. */
		for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && mob->pIndexData->pQuestor != NULL)
				break;
		}

		if( mob == NULL )
		{
			send_to_char("You can't do that here\n\r", ch);
			return;
		}

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
		ch->quest->questgiver_type = QUESTOR_MOB;
		ch->quest->questgiver.pArea = mob->pIndexData->area;
		ch->quest->questgiver.vnum = mob->pIndexData->vnum;
		ch->quest->questreceiver_type = QUESTOR_MOB;
		ch->quest->questreceiver.pArea = mob->pIndexData->area;
		ch->quest->questreceiver.vnum = mob->pIndexData->vnum;

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

			ch->countdown = 0;

			for (qp = ch->quest->parts; qp != NULL; qp = qp->next)
				ch->countdown += qp->minutes;

			sprintf(buf, "You have %d minutes to complete this quest.", ch->countdown);
			do_say(mob, buf);
		}

		return;
	}

	//
	// Quest cancel
	//
	if (!str_cmp(arg1, "cancel"))
	{
		if (!IS_AWAKE(ch))
		{
			send_to_char("In your dreams, or what?\n\r", ch);
			return;
		}

		if( ch->quest == NULL )
		{
			send_to_char("You are not on a quest.\n\r", ch);
			return;
		}

		if( ch->quest->generating )
		{
			ch->countdown = 0;
			ch->nextquest = 0;
			free_quest(ch->quest);
			ch->quest = NULL;
			send_to_char("Pending quest cancelled.\n\r", ch);
		}
		else
		{
			// Check for the questGIVER
			switch(ch->quest->questgiver_type)
			{
			case QUESTOR_MOB:
				for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
				{
					if (IS_NPC(mob) && wnum_match_mob(ch->quest->questgiver, mob))
						break;
				}
				break;

			case QUESTOR_OBJ:
				// Check inventory first?
				for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
				{
					if (wnum_match_obj(ch->quest->questgiver, obj))
						break;
				}
				if( obj == NULL )
				{
					for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
					{
						if (wnum_match_obj(ch->quest->questgiver, obj))
							break;
					}
				}
				break;

			case QUESTOR_ROOM:
				if( !ch->in_room->wilds && !ch->in_room->source &&
					wnum_match_room(ch->quest->questgiver, ch->in_room))
				{
					room = ch->in_room;
					break;
				}
				break;
			}

			if( mob )
			{
				// Mobs will complain
				act("$n informs $N $e has cancelled $s quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				act ("You inform $N you have cancelled your quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

				sprintf(buf,
					"I am most displeased with your efforts, %s! This is "
					"obviously a job for someone with more talent than you.",
					ch->name);
				do_say(mob, buf);

				free_quest(ch->quest);
				ch->quest = NULL;
				ch->countdown = 0;

				mob->tempstore[0] = 10;
				p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_CANCEL, NULL);

				ch->nextquest = mob->tempstore[0];
				if(ch->nextquest < 1) ch->nextquest = 1;
			}
			else if( obj )
			{
				// Objects will not complain by default

				free_quest(ch->quest);
				ch->quest = NULL;
				ch->countdown = 0;

				obj->tempstore[0] = 10;
				p_percent_trigger( NULL, obj, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_CANCEL, NULL);

				ch->nextquest = obj->tempstore[0];
				if(ch->nextquest < 1) ch->nextquest = 1;
			}
			else if( room )
			{
				free_quest(ch->quest);
				ch->quest = NULL;
				ch->countdown = 0;

				room->tempstore[0] = 10;
				p_percent_trigger( NULL, NULL, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_CANCEL, NULL);

				ch->nextquest = room->tempstore[0];
				if(ch->nextquest < 1) ch->nextquest = 1;
			}
			else
			{
				send_to_char("You can't do that here\n\r", ch);
			}
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
		int *tempstores;

		if (!IS_AWAKE(ch))
		{
			send_to_char("In your dreams, or what?\n\r", ch);
			return;
		}

		if( ch->quest == NULL || ch->quest->generating )
		{
			send_to_char("You are not on a quest.\n\r", ch);
			return;
		}

		// Check for the questRECEIVER
		switch(ch->quest->questreceiver_type)
		{
		case QUESTOR_MOB:
			for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
			{
				if (IS_NPC(mob) && wnum_match_mob(ch->quest->questreceiver, mob))
				{
					tempstores = mob->tempstore;
					break;
				}
			}
			break;

		case QUESTOR_OBJ:
			// Check inventory first?
			for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
			{
				if (wnum_match_obj(ch->quest->questreceiver, obj))
				{
					tempstores = obj->tempstore;
					break;
				}
			}
			if( obj == NULL )
			{
				for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
				{
					if (wnum_match_obj(ch->quest->questreceiver, obj))
					{
						tempstores = obj->tempstore;
						break;
					}
				}
			}
			break;

		case QUESTOR_ROOM:
			if( !ch->in_room->wilds && !ch->in_room->source &&
				wnum_match_room(ch->quest->questreceiver, ch->in_room) )
			{
				room = ch->in_room;
				tempstores = room->tempstore;
			}
			break;
		}

		if( !mob && !obj && !room )
		{
			send_to_char("You can't do that here\n\r", ch);
			return;
		}

		if( mob )
		{
			act("$n informs $N $e has completed $s quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act ("You inform $N you have completed your quest.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

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
			if( mob ) {
				sprintf(buf,
					"I am most displeased with your efforts, %s! This is "
					"obviously a job for someone with more talent than you.",
					ch->name);
				do_say(mob, buf);
			}

			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);

			free_quest(ch->quest);
			ch->quest = NULL;
			ch->countdown = 0;

			tempstores[0] = 10;
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTQUEST, NULL);

			ch->nextquest = tempstores[0];
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

				// TODO: turn this into a TRAIT
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
					if( mob )
					{
						act("You hand $p to $N.",ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_CHAR);
						act("$n hands $p to $N.",ch, mob, NULL, part->pObj, NULL, NULL, NULL, TO_ROOM);
					}

					extract_obj(part->pObj);

					part->pObj = NULL;
				}
			}
		}

		if (!incomplete)
		{
			if( mob )
			{
				sprintf(buf, "Congratulations on completing your quest!");
				do_say(mob,buf);
			}
			ch->pcdata->quests_completed++;
		}
		else
		{
			if( mob )
			{
				sprintf(buf, "I see you haven't fully completed your quest, "
					"but I applaud your courage anyway!");
				do_say(mob,buf);
			}
		    pracreward -= number_range(2,5);
			pointreward -= number_range(10,20);
			pracreward = UMAX(pracreward,0);
			pointreward = UMAX(pointreward, 0);
		}

		tempstores[0] = expreward;			// Experience
		tempstores[1] = pointreward;		// QP
		tempstores[2] = pracreward;			// Practices
		tempstores[3] = reward;				// Silver
		// TODO: any other rewards?
		if(incomplete)
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_INCOMPLETE, NULL);
		else
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_COMPLETE, NULL);
		expreward = tempstores[0];
		pointreward = tempstores[1];
		pracreward = tempstores[2];
		reward = tempstores[3];

		// Clamp to zero
		expreward = UMAX(expreward,0);
		reward = UMAX(reward,0);
		pracreward = UMAX(pracreward,0);
		pointreward = UMAX(pointreward, 0);

		if (boost_table[BOOST_QP].boost != 100)
			pointreward = (pointreward * boost_table[BOOST_QP].boost)/100;


		if( mob ) {
			sprintf(buf, "As a reward, I am giving you %d quest points and %d silver.",
				pointreward, reward);
			do_say(mob,buf);
		}
		else
		{
			sprintf(buf, "As a reward, you receive %d quest points and %d silver.\n\r", pointreward, reward);
			send_to_char(buf, ch);
		}

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

		// TODO: Change to a test, as we might add a way for players to disable XP gain
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

		tempstores[0] = 10;
		p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTQUEST, NULL);

		ch->nextquest = tempstores[0];
		if(ch->nextquest < 1) ch->nextquest = 1;

		ch->countdown = 0;	// @@@NIB Not doing this was causing nextquest to come up
							//	10 minutes if nextquest had expired
		free_quest(ch->quest);
		ch->quest = NULL;
    }
    else
    {
		send_to_char("QUEST commands: POINTS INFO TIME REQUEST CANCEL COMPLETE.\n\r", ch);
		send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
    }
}


/*
 * Generate a quest. Returns TRUE if a quest is found.
 */
bool generate_quest(CHAR_DATA *ch, CHAR_DATA *questman)
{
	QUEST_PART_DATA *part;
	OBJ_DATA *scroll;
	int parts;
	int i;

	ch->quest->generating = TRUE;
	ch->quest->scripted = FALSE;

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

	QUESTOR_DATA *qd = questman->pIndexData->pQuestor;

	// MORE FUN
	questman->tempstore[0] = parts;				// Number of parts to do (In-Out)
	questman->tempstore[1] = bFun ? 1 : 0;		// Whether this was a F.U.N. quest (In)
	questman->tempstore[2] = qd->scroll.auid;	// Default quest scroll item (AUID)
	questman->tempstore[3] = qd->scroll.vnum;	// Default quest scroll item (VNUM)
	if(p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_PREQUEST, NULL))
		return FALSE;
	parts = questman->tempstore[0];				// Updated number of parts to do
	if( parts < 1 ) parts = 1;					//    Require at least one part.

	WNUM scroll_wnum;
	scroll_wnum.pArea = get_area_from_uid(questman->tempstore[2]);
	scroll_wnum.vnum = questman->tempstore[3];
	if( !scroll_wnum.pArea || scroll_wnum.vnum < 1 )
	{
		scroll_wnum.pArea = get_area_from_uid(qd->scroll.auid);
		scroll_wnum.vnum = qd->scroll.vnum;
	}

	for (i = 0; i < parts; i++)
	{
		part = new_quest_part();
		part->next = ch->quest->parts;
		ch->quest->parts = part;
		part->index = parts - i;

		if (generate_quest_part(ch, questman, part, parts - i))
			continue;
		else
			return FALSE;
	}

	// create the scroll
	scroll = generate_quest_scroll(ch, questman->short_descr, scroll_wnum,
		qd->header, qd->footer, qd->prefix, qd->suffix, qd->line_width);

	if( scroll == NULL )
	{
		// COMPLAIN
		return FALSE;
	}

	free_string(scroll->name);
	free_string(scroll->short_descr);
	free_string(scroll->description);

	scroll->name = str_dup(qd->keywords);
	scroll->short_descr = str_dup(qd->short_descr);
	scroll->description = str_dup(qd->long_descr);


    act("$N gives $p to $n.", ch, questman, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
    act("$N gives you $p.",   ch, questman, NULL, scroll, NULL, NULL, NULL, TO_CHAR);
    obj_to_char(scroll, ch);
    return TRUE;
}

/* Set up a quest part. */
bool generate_quest_part(CHAR_DATA *ch, CHAR_DATA *questman, QUEST_PART_DATA *part, int partno)
{
	// There's no automatic stuff...
	questman->tempstore[0] = partno;							// Which quest part *IS* this?  Needed for the "questcomplete" command

	// The quest part must return a positive value to be valid
	//  returning a zero due to "end 0" or not having the QUEST_PART trigger will be considered invalid
	//  errors in script execution will be negative, so will be considered invalid.
    return p_percent_trigger( questman, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_QUEST_PART, NULL) > 0;
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
	    if (IS_QUESTING(ch) && !ch->quest->generating)
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


void check_quest_rescue_mob(CHAR_DATA *ch, bool show)
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
			if (IS_NPC(mob) && mob->pIndexData->area == part->area && mob->pIndexData->vnum == part->mob_rescue && !part->complete)
		    {
				if( show ) {
		        	sprintf(buf, "Thank you for rescuing me, %s!", ch->name);
		        	do_say(mob, buf);
				}

				if (mob->master != NULL)
					stop_follower(mob,show);

				add_follower(mob, ch, show);

				if (IS_NPC(mob) && IS_SET(mob->act, ACT_AGGRESSIVE))
				    REMOVE_BIT(mob->act, ACT_AGGRESSIVE);

				found = TRUE;
				break;
		    }

		    mob = mob->next_in_room;
		}

		if (found && !part->complete)
		{
			if( show )
			{
				sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
				send_to_char(buf, ch);
			}

			part->complete = TRUE;
			break;
		}
    }
}


void check_quest_retrieve_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool show)
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
				if( show )
				{
					char buf[MAX_STRING_LENGTH];
					sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
					send_to_char(buf, ch);
				}

				part->complete = TRUE;
			}
		}
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob, bool show)
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

        if (IS_NPC(mob) && part->area == mob->pIndexData->area && part->mob == mob->pIndexData->vnum && !part->complete)
		{
			if( show ) {
				char buf[MAX_STRING_LENGTH];
				sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
				send_to_char(buf, ch);
			}

			part->complete = TRUE;
		}
    }
}

void check_quest_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool show)
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

		target_room = get_room_index(part->area, part->room);

		/* Not going by room vnum to prevent multiple rooms with the same name */
		if (target_room != NULL && !str_cmp(target_room->name, room->name))
		{

			if( show )
			{
				char buf[MAX_STRING_LENGTH];
				sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
				send_to_char(buf, ch);
			}

			part->complete = TRUE;
		}
    }
}

bool check_quest_custom_task(CHAR_DATA *ch, int task, bool show)
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


		if( show )
		{
		    char buf[MAX_STRING_LENGTH];
	    	sprintf(buf, "{YYou have completed task %d of your quest!{x\n\r", i);
	    	send_to_char(buf, ch);
		}
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


// TODO: widevnum
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


QUEST_INDEX_DATA *get_quest_index(AREA_DATA *area, long vnum)
{
	if (!area) return NULL;
	
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

CHAR_DATA *get_renewer_here(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *mob;

	if (argument[0] == '\0')
	{
		for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && IS_SET(mob->act2, ACT2_RENEWER))
			{
				return mob;
			}
		}

		send_to_char("Renew with whom?\n\r", ch);
		return NULL;
	}
	else
	{
		if ((mob = get_char_room(ch, NULL, argument)) == NULL)
		{
			send_to_char("They aren't here.\n\r", ch);
			return NULL;
		}

		if (!IS_NPC(mob) || !IS_SET(mob->act2, ACT2_RENEWER))
		{
			// Make a tell?
			act("You cannot do that with $N.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return NULL;
		}

		return mob;
	}

}

// RENEW PET[ <RENEWER>]
// RENEW MOUNT[ <RENEWER>]
// RENEW GUARD <MOBILE>[ <RENEWER>]
// RENEW OBJECT <OBJECT>[ <RENEWER>]
// RENEW OTHER <KEYWORD>[ <RENEWER>]
void do_renew(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	int cost;
	char buf[MSL+1];
	char arg1[MIL+1];
	char arg2[MIL+1];
	char arg3[MIL+1];

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);

	if (arg1[0] == '\0')
	{
		send_to_char("Renew what?\n\r", ch);
		send_to_char("RENEW PET[ <RENEWER>]              to renew your pet.\n\r", ch);
		send_to_char("RENEW MOUNT[ <RENEWER>]            to renew your mount.\n\r", ch);
		send_to_char("RENEW GUARD <MOBILE>[ <RENEWER>]   to renew one of your guards.\n\r", ch);
		send_to_char("RENEW OBJECT <OBJECT>[ <RENEWER>]  to renew an item.\n\r", ch);
		send_to_char("RENEW CUSTOM <KEYWORD>[ <RENEWER>] to renew any custom service or good.\n\r", ch);
		send_to_char("RENEW LIST[ <RENEWER>]             asks for a list of things that can be renewed.\n\r", ch);
		return;
	}


	if( !str_prefix(arg1, "pet") )
	{
		mob = get_renewer_here(ch, arg2);
		if( mob == NULL )
			return;

		if( ch->pet == NULL )
		{
			send_to_char("You don't have a pet.\n\r", ch);
			return;
		}

		// Requires a pet
		if( mob->shop != NULL )
		{
			cost = ch->pet->tot_level * ch->pet->tot_level;
			mob->tempstore[0] = UMAX(cost, 1);
			mob->tempstore[1] = STOCK_PET;
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->questpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_PET;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_RENEW, NULL);

			sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
			act(buf, ch->pet, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->questpoints -= cost;
		}
		else
		{
			sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
			do_say(mob, buf);
		}
		return;
	}
	else if(!str_prefix(arg1, "mount") )
	{
		mob = get_renewer_here(ch, arg2);
		if( mob == NULL )
			return;

		if( !MOUNTED(ch) )
		{
			send_to_char("You aren't mounted.\n\r", ch);
			return;
		}

		if( !str_cmp(ch->mount->owner, ch->name) )
		{
			// Yet?
			send_to_char("Personal mounts cannot be renewed.\n\r", ch);
			return;
		}

		if( mob->shop != NULL )
		{
			cost = 25 * ch->mount->tot_level * ch->mount->tot_level / 10;
			mob->tempstore[0] = UMAX(cost, 1);
			mob->tempstore[1] = STOCK_MOUNT;
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->questpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_MOUNT;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_RENEW, NULL);

			sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
			act(buf, ch->mount, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->questpoints -= cost;
		}
		else
		{
			sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
			do_say(mob, buf);
		}

		return;
	}
	else if(!str_prefix(arg1, "guard") )
	{
		mob = get_renewer_here(ch, arg3);
		if( mob == NULL )
			return;

		CHAR_DATA *guard = get_char_room(ch, NULL, arg2);
		if( guard == NULL )
		{
			send_to_char("They aren't here.\n\r", ch);
			return;
		}

		if( guard->master != ch )
		{
			send_to_char("They are not following you.\n\r", ch);
			return;
		}

		if( mob->shop != NULL )
		{
			cost = 5 * guard->tot_level * guard->tot_level;
			mob->tempstore[0] = UMAX(cost, 1);
			mob->tempstore[1] = STOCK_GUARD;
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->questpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_GUARD;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_RENEW, NULL);

			sprintf(buf, "{YYou renew $n with $N for %d quest points.{x", cost);
			act(buf, guard, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->questpoints -= cost;
		}
		else
		{
			sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
			do_say(mob, buf);
		}

		return;
	}
	else if( !str_prefix(arg1, "object") )
	{
		mob = get_renewer_here(ch, arg3);
		if( mob == NULL )
			return;

		if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
		{
			sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		cost = obj->cost/10;
		mob->tempstore[0] = UMAX(cost, 1);
		mob->tempstore[1] = STOCK_OBJECT;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRERENEW, NULL) <= 0)
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

		mob->tempstore[0] = cost;
		mob->tempstore[1] = STOCK_OBJECT;
		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_RENEW, NULL);

		sprintf(buf, "{YYou renew $p with $N for %d quest points.{x", cost);
		act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		ch->questpoints -= cost;

		return;
	}
	else if( !str_prefix(arg1, "custom") )
	{
		mob = get_renewer_here(ch, arg3);
		if( mob == NULL )
			return;

		mob->tempstore[0] = 0;	// Customs REQUIRE the script to specify the cost
		mob->tempstore[1] = STOCK_CUSTOM;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRERENEW, arg2) <= 0)
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
			sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
			do_say(mob, buf);
			return;
		}

		mob->tempstore[0] = cost;
		mob->tempstore[1] = STOCK_CUSTOM;
		free_string(mob->tempstring);
		mob->tempstring = &str_empty[0];
		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW, arg2);

		if(!IS_NULLSTR(mob->tempstring))
		{
			sprintf(buf, "{YYou renew %s with $N for %d quest points.{x", mob->tempstring, cost);
		}
		else
		{
			sprintf(buf, "{YYou renew %s with $N for %d quest points.{x", arg2, cost);
		}
		act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		ch->questpoints -= cost;

		return;
	}
	else if(!str_prefix(arg1, "list"))
	{
		mob = get_renewer_here(ch, arg2);
		if( mob == NULL )
			return;


		act("{YYou ask $N for a list of things $E can renew.{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n asks $N for a list of things $E can renew.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW_LIST, NULL))
			return;

		sprintf(buf, "I don't really do anything special here.");
		do_say(mob, buf);
		return;
	}

/*
	if( mob->shop != NULL )
	{
		// ** Shopkeeper, find stock entries only **

		// Find the stock item to renew
		SHOP_STOCK_DATA *stock = get_stockonly_keeper(ch, mob, char *arg1);
		if( stock == NULL )
		{
			sprintf(buf, "Sorry %s, I do not stock that.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		obj = NULL;
		victim = NULL;
		switch(stock->type)
		{
		case STOCK_OBJECT:
			if( stock->obj == NULL )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if( (obj = get_obj_vnum_carry(ch, stock->obj->vnum, mob)) == NULL )
			{
				sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}
			break;
		case STOCK_PET:
			if( stock->mob == NULL || ch->pet == NULL )
			{
				sprintf(buf, "Sorry %s, but I cannot help you with that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if( ch->pet->pIndexData != stock->mob )
			{
				sprintf(buf, "You don't have that pet, %s.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if( stock->duration > 0 )
			{
				// This needs
				if( !IS_SET(ch->pet->act2, ACT2_HIRED) || (ch->pet->hired_to < 1) )
				{
					sprintf(buf, "Sorry %s, but you already own that pet.", pers(ch, mob));
					do_say(mob, buf);
					return;
				}
			}


			if( (obj = get_obj_vnum_carry(ch, stock->obj->vnum, mob)) == NULL )
			{
				sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}
			break;


		// Call PRERENEW
		mob->tempstore[0] = 0;
		mob->tempstore[1] = stock->type;
		mob->tempstore[2] = stock->vnum;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRERENEW, NULL) <= 0)
			return;


		// Check QUESTPOINTS
		cost = mob->tempstore[0];
		if( cost <= 0 )
		{
			sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		if (ch->questpoints < cost)
		{
			sprintf(buf, "Sorry %s, but it would take %d quest points for me to renew that.", pers(ch, mob), cost);
			do_say(mob, buf);
			return;
		}


		sprintf(buf, "You renew $p to $N for %d quest points.", cost);
		act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		// Call RENEW
	}
	else
	{
		if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
		{
			sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		cost = obj->cost/10;
		cost = UMAX(cost, 1);

		mob->tempstore[0] = cost;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_PRERENEW, NULL) <= 0)
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

		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_RENEW, NULL);

		ch->questpoints -= cost;
	}
*/

}

