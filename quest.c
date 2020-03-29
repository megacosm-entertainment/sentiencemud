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
    OBJ_DATA *obj = NULL;
    char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0')
    {
	send_to_char("QUEST commands: POINTS INFO TIME INSPECT REQUEST "
	    "COMPLETE LIST SELL RENEW BUY.\n\r", ch);
	send_to_char("For more information, type 'HELP QUEST'.\n\r",ch);
	return;
    }

   /*
    * Quest Info
    */
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
	    	sprintf(buf,
	    	"You have completed task {Y%d{x of your quest!\n\r",
	    	i);
		send_to_char(buf, ch);
	    }
	    else
	    {
	        sprintf(buf,
	        "Task {Y%d{x of your quest is not complete.\n\r",
	        i);
		totally_complete = FALSE;
		send_to_char(buf, ch);
	    }
	}

	if (totally_complete)
	{
            sprintf(buf,
	    "{YYour quest is complete!{x\n\r"
	    "Get back to %s before your time runs out!\n\r",
	    get_mob_index(ch->quest->questgiver)->short_descr);
            send_to_char(buf, ch);
        }
	return;
    }


   /*
    * Quest points
    */
    if (!str_cmp(arg1, "points"))
    {
        sprintf(buf, "You have {Y%d{x quest points.\n\r", ch->questpoints);
        send_to_char(buf, ch);
        return;
    }


   /*
    * Quest time
    */
    else if (!str_cmp(arg1, "time"))
    {
        if (!IS_QUESTING(ch))
        {
            if (ch->nextquest > 1)
            {
                sprintf(buf,
		"There are %d minutes remaining until you can "
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
        if (IS_NPC(mob) && IS_SET(mob->act, ACT_QUESTOR))
	    break;
    }

    if (mob == NULL)
    {
        send_to_char("You can't do that here.\n\r",ch);
        return;
    }


   /*
    * Quest List
    */
    if (!str_cmp(arg1, "list"))
    {
	int i = 0;
	OBJ_INDEX_DATA *pItem;

	if (!IS_AWAKE(ch))
	{
	    send_to_char("In your dreams, or what?\n\r", ch);
	    return;
	}

        act("$n asks $N for a list of quest items.",  ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        act ("You ask $N for a list of quest items.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	sprintf(buf, "{YNum Cost   Name{x\n\r");
	send_to_char(buf, ch);
	sprintf(buf, "{Y---------------------------------------------\n\r");
	send_to_char(buf, ch);

	if (mob->pIndexData->vnum == VNUM_QUESTOR_1)
	{
	    for (i = 0; quest_item_table[i] != 0; i++)
	    {
		pItem = get_obj_index(quest_item_table[i]);

		sprintf(buf, "{M%2d) {x%-6ld %-30s\n\r",
		    i + 1,
		    pItem->cost,
		    pItem->short_descr);

		send_to_char(buf, ch);
	    }

	    sprintf(buf, "{M%2d) {x%-6d %-30s\n\r",
		    ++i, 300, "15 practices");
	    send_to_char(buf, ch);
	}

	if (mob->pIndexData->vnum == VNUM_QUESTOR_2)
	{
	    for (i = 0; quest2_item_table[i] != 0; i++)
	    {
		pItem = get_obj_index(quest2_item_table[i]);

		sprintf(buf, "{M%2d) {x%-6ld %-30s\n\r",
		    i + 1,
		    pItem->cost,
		    pItem->short_descr);

		send_to_char(buf, ch);
	    }

	    sprintf(buf, "{M%2d) {x%-6d %-30s\n\r",
		    ++i, 300, "15 practices");
	    send_to_char(buf, ch);
	}

	return;
    }

   /*
    * Quest buy
    */
    else if (!str_cmp(arg1, "buy"))
    {
	int i;
	bool found = FALSE;
	OBJ_INDEX_DATA *pIndex = NULL;

	if (!IS_AWAKE(ch))
	{
	    send_to_char("In your dreams, or what?\n\r", ch);
	    return;
	}

        if (arg2[0] == '\0')
        {
            send_to_char("To buy an item, type 'QUEST BUY <item>'.\n\r",ch);
            return;
        }

	if (!str_cmp(arg2, "pracs")
	|| !str_cmp(arg2, "practices")
	|| !str_cmp(arg2, "prac"))
	{
  	    if (ch->questpoints < 300)
	    {
	        sprintf(buf, "Sorry %s, but you don't have enough quest points"
		" to make that purchase.", ch->name);
		do_say(mob, buf);
		return;
	    }

//	    act("$n buys 15 practices.", ch, NULL, NULL, TO_ROOM);
	    act("You buy 15 practices.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    ch->questpoints -= 300;
	    ch->practice += 15;
	    return;
	}

	// We are buying a quest item.
	if (mob->pIndexData->vnum == VNUM_QUESTOR_1)
	for (i = 0; quest_item_table[i] != 0; i++)
	{
	    pIndex = get_obj_index(quest_item_table[i]);

	    if (is_name(arg2, pIndex->name))
	    {
		found = TRUE;
		break;
	    }
	}

	if (mob->pIndexData->vnum == VNUM_QUESTOR_2)
	for (i = 0; quest2_item_table[i] != 0; i++)
	{
	    pIndex = get_obj_index(quest2_item_table[i]);

	    if (is_name(arg2, pIndex->name))
	    {
		found = TRUE;
		break;
	    }
	}

	if (found == FALSE)
	{
	    sprintf(buf, "Sorry %s, but I don't have that item.",
		    ch->name);
	    do_say(mob, buf);
	    return;
	}

	if (pIndex->cost > ch->questpoints)
	{
	    sprintf(buf, "Sorry %s, but you don't have enough quest points"
		    " to make that purchase.", ch->name);
	    do_say(mob, buf);
	    return;
	}

	obj = create_object(pIndex, ch->tot_level, TRUE);
	ch->questpoints -= pIndex->cost;

	if (obj != NULL)
        {
	    obj->level = ch->tot_level;
	    if (obj->item_type == ITEM_ARMOUR)
	    {
		set_armour_obj(obj);
	    }

            act("$N gives $p to $n.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
            act("$N gives you $p.",   ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
            obj_to_char(obj, ch);
        }
        return;
    }

   /*
    * Quest renew - adjust level for 1/10 the price
    */
    else if (!str_cmp(arg1, "renew"))
    {
	OBJ_DATA *obj;
	bool fQuestItem = FALSE;
	int i;
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

	for (i = 0; quest_item_table[i] != 0; i++)
	{
	    if (obj->pIndexData->vnum == quest_item_table[i])
	    {
		fQuestItem = TRUE;
		break;
	    }
	}

	if (!fQuestItem)
	{
	    for (i = 0; quest2_item_table[i] != 0; i++)
	    {
		if (obj->pIndexData->vnum == quest2_item_table[i])
		{
		    fQuestItem = TRUE;
		    break;
		}
	    }
	}

        if (!fQuestItem || obj->catalyst)
	{
	    sprintf(buf, "That would be quite counter-productive, %s.",
		    pers(ch, mob));
	    do_say(mob, buf);
	    return;
	}

	cost = obj->cost/10;
	cost = UMAX(cost, 1);

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

	obj->level = ch->tot_level;
	if (obj->item_type == ITEM_ARMOUR)
	    set_armour_obj(obj);

	ch->questpoints -= cost;
	return;
    }


   /*
    * Quest sell
    */
    else if (!str_cmp(arg1, "sell"))
    {
	OBJ_DATA *obj;
	bool fQuestItem = FALSE;
	int roll;
	int cost;
	int i;

	if (arg2[0] == '\0')
	{
	    sprintf(buf, "Sell what, %s?", pers(ch, mob));
	    do_say(mob, buf);
	    return;
	}

	if ((obj = get_obj_carry(ch, arg2, ch)) == NULL)
	{
	    sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
	    do_say(mob, buf);
	    return;
	}

	for (i = 0; quest_item_table[i] != 0; i++)
	{
	    if (obj->pIndexData->vnum == quest_item_table[i])
	    {
		fQuestItem = TRUE;
		break;
	    }
	}

	if (!fQuestItem)
	{
	    for (i = 0; quest2_item_table[i] != 0; i++)
	    {
		if (obj->pIndexData->vnum == quest2_item_table[i])
		{
		    fQuestItem = TRUE;
		    break;
		}
	    }
	}

	if (obj->pIndexData->vnum == OBJ_VNUM_SWORD_SENT)
	    fQuestItem = TRUE;

        /* no selling moonstones as they can be found in the game */
        if (!fQuestItem || obj->item_type == ITEM_CATALYST)
	{
	    sprintf(buf, "What could I possibly do with %s, %s?",
		    obj->short_descr, pers(ch, mob));
	    do_say(mob, buf);
	    return;
	}

	cost = obj->cost/3;

	roll = number_percent();
	if (roll < get_skill(ch,gsn_haggle))
	{
	    act("You haggle with $N.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    cost += obj->cost/5 * roll/100;
	    check_improve(ch,gsn_haggle,TRUE,4);
	}

	cost = UMAX(cost, 1);

	sprintf(buf, "You sell $p to $N for %d quest points.", cost);
	act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	act("$n sells $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	ch->questpoints += cost;
	extract_obj(obj);
	return;
    }

   /*
    * Quest Inspect
    */
    else if (!str_cmp(arg1, "inspect"))
    {
	OBJ_INDEX_DATA *pObj;
	OBJ_DATA *an_obj;
	int i = 0;
	bool found = FALSE;

	if (!IS_AWAKE(ch))
	{
	    send_to_char("In your dreams, or what?\n\r", ch);
	    return;
	}

	pObj = get_obj_index(1);
	while (pObj != NULL)
	{
	    if (mob->pIndexData->vnum == VNUM_QUESTOR_1)
		pObj = get_obj_index(quest_item_table[i++]);
	    if (mob->pIndexData->vnum == VNUM_QUESTOR_2)
		pObj = get_obj_index(quest2_item_table[i++]);

	    if (pObj != NULL && is_name(arg2, pObj->name))
	    {
		found = TRUE;
		break;
	    }
	}

	if (!found)
	{
	    sprintf(buf, "Sorry %s, I don't sell that item.",
		    ch->name);
	    do_say(mob, buf);
	    return;
	}

	an_obj = create_object(pObj, 0, TRUE);

	act("You ask $N for some information about $p.", ch, mob, NULL, an_obj, NULL, NULL, NULL, TO_CHAR);
	act("$n asks $N for some information about $p.", ch, mob, NULL, an_obj, NULL, NULL, NULL, TO_ROOM);

	obj_to_char(an_obj, ch);

	spell_identify(0, ch->tot_level, ch,an_obj, TARGET_OBJ, WEAR_NONE);
	extract_obj(an_obj) ;
	return;
    }

   /*
    * Quest Request
    */
    else if (!str_cmp(arg1, "request"))
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
            sprintf(buf,
	    "You're very brave, %s, but let someone else have a chance.",
	    ch->name);
	    if (mob == NULL)
	    {
	        sprintf(buf, "do_quest(), quest request: MOB Was null, %s.\n\r",
		ch->name);
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

            sprintf(buf, "You have %d minutes to complete this quest.",
			    ch->countdown);
            do_say(mob, buf);
        }
        return;
    }

   /*
    * Quest complete
    */
    else if (!str_cmp(arg1, "complete"))
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

	if (ch->quest == NULL
	|| ch->quest->questgiver != mob->pIndexData->vnum)
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
	    free_quest(ch->quest);
	    ch->quest = NULL;
	    ch->countdown = 0;

	    ch->nextquest = 10;

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

	if (boost_table[BOOST_QP].boost != 100)
	    pointreward = (pointreward * boost_table[BOOST_QP].boost)/100;

        sprintf(buf,
	"As a reward, I am giving you %d quest points and %d silver.",
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
	    sprintf(buf, "You gain %d experience points!\n\r",
		    expreward);
	    send_to_char(buf, ch);

	    gain_exp(ch, expreward);
	}
/* Syn - disabling
  send_to_char("You receive 1 military quest point!\n\r", ch);
  award_ship_quest_points(ch->in_room->area->place_flags, ch, 1);
*/

    ch->nextquest = 10;

    ch->countdown = 0;	// @@@NIB Not doing this was causing nextquest to come up
    			//	10 minutes if nextquest had expired
    free_quest(ch->quest);
    ch->quest = NULL;
    }
    else
    {
	send_to_char("QUEST commands: POINTS INFO TIME INSPECT REQUEST "
	    "COMPLETE LIST SELL RENEW BUY.\n\r", ch);
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
    MOB_INDEX_DATA *mob;
    CHAR_DATA *victim;
    QUEST_PART_DATA *part;
    OBJ_DATA *scroll;
    int parts;
    int i;

    if (ch->tot_level <= 30)
        parts = number_range(1, 3);
    else if (ch->tot_level <= 60)
        parts = number_range(3, 6);
    else if (ch->tot_level <= 90)
        parts = number_range(7, 9);
    else
        parts = number_range(8, 15);

    /* fun */
    if (number_percent() < 5)
	    parts = parts * 2;

    // create the scroll
    scroll = create_object(get_obj_index(OBJ_VNUM_QUEST_SCROLL), 0, TRUE);
    free_string(scroll->full_description);

    sprintf(buf2, 
	    "{W  .-.--------------------------------------------------------------------------------------.-.\n\r"
	    "((o))                                                                                         )\n\r"
	    "{W \\U/_________________________________________________________________________________________/\n\r"
	    "{W  |\n\r"
	    "{W  |  {xNoble %s,\n\r{x  |\n\r"
	    "{W  |  {xThis is an official quest scroll given to you by %s.\n\r"
	    "{W  |  {xUpon this scroll is my seal, and my approval to go to any\n\r"
	    "{W  |  {xmeasures in order to complete the set of tasks I have listed.\n\r"
	    "{W  |  {xReturn to me once you have completed these tasks, and you\n\r"
	    "{W  |  {xshall be justly rewarded.\n\r  |  \n\r",
	    ch->name, questman->short_descr);

    for (i = 0; i < parts; i++)
    {
	part = new_quest_part();
	part->next = ch->quest->parts;
	ch->quest->parts = part;

        if (generate_quest_part(ch, questman, part))
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
	sprintf(buf, "{W  |  {xTask {Y%d{x: ", i);
	strcat(buf2, buf);

	if (part->pObj != NULL)
	    sprintf(buf, "Retrieve {Y%s{x from {Y%s{x in {Y%s{x.\n\r",
		    part->pObj->short_descr, part->pObj->in_room->name, part->pObj->in_room->area->name);

	if (part->mob_rescue != -1) {
	    mob = get_mob_index(part->mob_rescue);

	    victim = get_char_world_index(NULL, mob);

	    sprintf(buf, "Rescue {Y%s{x from {Y%s{x in {Y%s{x.\n\r",
		    victim->short_descr,
		    victim->in_room->name,
		    victim->in_room->area->name);
	}

	if (part->mob != -1) {
	    mob = get_mob_index(part->mob);

	    victim = get_char_world_index(NULL, mob); 

	    sprintf(buf, "Slay {Y%s{x.  %s was last seen in {Y%s{x.\n\r",
		    victim->short_descr,
		    victim->sex == SEX_MALE ? "He" :
		    victim->sex == SEX_FEMALE ? "She" : "It",
		    victim->in_room->area->name);
	}

	if (part->room != -1) {
	    ROOM_INDEX_DATA *room = get_room_index(part->room);

	    sprintf(buf, "Travel to {Y%s{x in {Y%s{x.\n\r", room->name, room->area->name);
	}

	    strcat(buf2, buf);
    }

    sprintf(buf, "{W  |__________________________________________________________________________________________\n\r"
	    "{W /A\\                                                                                         \\\n\r"
	    "((o))                                                                                         )\n\r"
	    "{W  '-'----------------------------------------------------------------------------------------'\n\r");
    strcat(buf2, buf);


    scroll->full_description = str_dup(buf2);

    act("$N gives $p to $n.", ch, questman, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
    act("$N gives you $p.",   ch, questman, NULL, scroll, NULL, NULL, NULL, TO_CHAR);
    obj_to_char(scroll, ch);
    return TRUE;
}


/* Set up a quest part. */
bool generate_quest_part(CHAR_DATA *ch, CHAR_DATA *questman, QUEST_PART_DATA *part)
{
    OBJ_DATA *item;
    CHAR_DATA *victim;
    ROOM_INDEX_DATA *rand_room;
    int rand;

    if (ch == NULL)
    {
	bug("generate_quest_part: ch NULL", 0);
	return FALSE;
    }
    
    switch (number_range(1, 4))
    { /* AO 010417 In R&D :)
	case 0:

	    if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
	    {
		item = get_random_obj(ch, FIRST_CONTINENT);
	    }
	    else
	    {
		item = get_random_obj(ch, SECOND_CONTINENT);
	    }

	    if (item == NULL)
		return FALSE;

	    part->mob = -1;
	    part->mob_rescue = -1;
	    part->obj = item->pIndexData->vnum;
	    part->obj_sac = -1;

	    sprintf(buf, "{YTask %d:{x Retrieve {C%s{x from {C%s{x in {R%s{x.\n\r", partnum+1,
		    item->short_descr, item->in_room->name, item->in_room->area->name);
	    strcat(buf2, buf);
	    break;
*/
	case 1:
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

	case 2:
	    if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
		victim = get_random_mob(ch, FIRST_CONTINENT);
	    else
		victim = get_random_mob(ch, SECOND_CONTINENT);

	    if (victim == NULL)
		return FALSE;

	    part->mob_rescue = victim->pIndexData->vnum;
	    break;
/* AO 010417 WHY is this here twice?
	case 2:
	    if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
	    {
		victim = get_random_mob(ch, FIRST_CONTINENT);
	    }
	    else
	    {
		victim = get_random_mob(ch, SECOND_CONTINENT);
	    }

	    if (victim == NULL)
	    {
		return FALSE;
	    }

	    part->obj = -1;
	    part->obj_sac = -1;
	    part->mob = -1;
	    part->mob_rescue = victim->pIndexData->vnum;

	    sprintf(buf, "{YTask %d:{x Rescue %s from %s in {R%s{x.\n\r",
		    partnum+1,
		    victim->short_descr,
		    victim->in_room->name,
		    victim->in_room->area->name);
	    strcat(buf2, buf);
	    break;
*/
	case 3:
		if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
		    victim = get_random_mob(ch, FIRST_CONTINENT);
		else
		    victim = get_random_mob(ch, SECOND_CONTINENT);

		if (victim == NULL)
		    return FALSE;

		part->mob = victim->pIndexData->vnum;
		break;

	case 4:
	    if (questman->pIndexData->vnum == VNUM_QUESTOR_1)
		rand_room = get_random_room(ch, FIRST_CONTINENT);
	    else
		rand_room = get_random_room(ch, SECOND_CONTINENT);

	    part->room = rand_room->vnum;
	    /* AO 010417 Will implement a "Travel to <place> and do <shit>, but this is a start */
	    break;
    }

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
    int total_parts;
    int i;
    bool found = TRUE;

    if (ch->quest == NULL)
        return;

    if (IS_NPC(ch))
    {
        bug("check_quest_rescue_mob: NPC", 0);
	return;
    }

    total_parts = count_quest_parts(ch);

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
	    if (IS_NPC(mob)
	    && mob->pIndexData->vnum == part->mob_rescue
	    && !part->complete)
	    {
	        sprintf(buf, "Thank you for rescuing me, %s!", ch->name);
                do_say(mob, buf);

		if (mob->master != NULL)
                    stop_follower(mob,TRUE);

		add_follower(mob, ch, TRUE);

		if (IS_NPC(mob)
		&& IS_SET(mob->act, ACT_AGGRESSIVE))
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
    int total_parts;
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
        total_parts = count_quest_parts(ch);

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
		part->pObj = obj;

		sprintf(buf,
		"{YYou have completed task %d of your quest!{x\n\r", i);
		send_to_char(buf, ch);
	    }
	}
    }
}


void check_quest_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob)
{
    QUEST_PART_DATA *part;
    int i;
    int total_parts;

    if (ch->quest == NULL || !IS_NPC(mob))
        return;

    if (IS_NPC(ch))
    {
         bug("check_quest_slay_mob: NPC", 0);
	 return;
    }

    total_parts = count_quest_parts(ch);

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;

	// already did it
	if (part->complete == TRUE)
	    continue;

        if (part->mob == mob->pIndexData->vnum
        && !part->complete)
	{
	    char buf[MAX_STRING_LENGTH];

	    sprintf(buf,
	        "{YYou have completed task %d of your quest!{x\n\r", i);

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
    int total_parts;

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

    total_parts = count_quest_parts(ch);

    i = 0;
    for (part = ch->quest->parts; part != NULL; part = part->next)
    {
        i++;
	
	// already did it
	if (part->complete == TRUE)
	    continue;

	target_room = get_room_index(part->room);

	/* Not going by room vnum to prevent multiple rooms with the same name */
	if (target_room != NULL
	&& !str_cmp(target_room->name, room->name)) 
	{
	    char buf[MAX_STRING_LENGTH];

	    sprintf(buf,
	        "{YYou have completed task %d of your quest!{x\n\r", i);

	    send_to_char(buf, ch);

	    part->complete = TRUE;
	}
    }
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
