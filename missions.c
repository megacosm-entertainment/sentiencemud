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

#define MISSIONPART_GETITEM	1
#define MISSIONPART_RESCUE	2
#define MISSIONPART_SLAY		3
#define MISSIONPART_GOTO		4
#define MISSIONPART_SCRIPT	5
#define MISSIONPARTS_BUILTIN	4

OBJ_DATA *generate_mission_scroll(CHAR_DATA *ch, char *giver, MISSION_DATA *mission, WNUM wnum,
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
		char *replace2 = string_replace_static(replace1, "$MISSIONARY$", giver);
		add_buf(buffer, replace2);

		for (MISSION_PART_DATA *part = mission->parts; part != NULL; part = part->next)
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
		replace2 = string_replace_static(replace1, "$MISSIONARY$", giver);
		add_buf(buffer, replace2);

		free_string(scroll->full_description);
		scroll->full_description = str_dup(buffer->string);

		free_buf(buffer);
	}

	return scroll;
}

static bool __classes_compatible(CLASS_DATA *a, CLASS_DATA *b)
{
	if (IS_SET(a->flags, CLASS_COMBATIVE))
	{
		// Both are combative classes
		if (IS_SET(b->flags, CLASS_COMBATIVE))
			return true;
		
		return false;
	}
	else if (IS_SET(b->flags, CLASS_COMBATIVE))
		return false;

	// Only if they are both non-combative
	return a->type == b->type;
}

void do_mission(CHAR_DATA *ch, char *argument)
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
		send_to_char("MISSION commands: POINTS INFO TIME REQUEST CANCEL COMPLETE.\n\r", ch);
		send_to_char("For more information, type 'HELP MISSION'.\n\r",ch);
		return;
	}

	//
	// MISSION INFO
	//
	if (!str_cmp(arg1, "info"))
	{
		if (arg2[0] == '\0')
		{
			if (list_size(ch->missions) > 0)
			{
				send_to_char("You are on the following missions:\n\r", ch);

				int m = 0;
				ITERATOR mit;
				MISSION_DATA *mission;
				iterator_start(&mit, ch->missions);
				while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
				{
					int parts = 0;
					int completed = 0;
					for(MISSION_PART_DATA *part = mission->parts; part; part = part->next)
					{
						++parts;
						if (part->complete) ++completed;
					}
					if (completed < parts)
						send_to_char(formatf("{W%2d{x) {Y%d{x minute%s left, {Y%d{x/{Y%d{x completed.\n\r", ++m, mission->timer, (mission->timer == 1)?"":"s", completed, parts), ch);
					else
						send_to_char(formatf("{W%2d{x) {Y%d{x minute%s left, {Ytotally{x completed.\n\r", ++m, mission->timer, (mission->timer == 1)?"":"s"), ch);
				}
				iterator_stop(&mit);
			}
			else
				send_to_char("You are not on a mission.\n\r", ch);
		}
		else
		{
			MISSION_DATA *mission;
			int index = 1;
			if (list_size(ch->missions) > 1)
			{
				if (!is_number(arg2) || (index = atoi(arg2)) < 1 || index > list_size(ch->missions))
				{
					send_to_char("Syntax:  mission info <index>\n\r", ch);
					send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(ch->missions)), ch);
					return;
				}
			}

			mission = (MISSION_DATA *)list_nthdata(ch->missions, index);

			send_to_char(formatf("Information about Mission {Y#%d{x\n\r", index), ch);
			
			int parts = 0;
			int completed = 0;
			MISSION_PART_DATA *part;

			for(part = mission->parts; part; part = part->next)
			{
				++parts;
				if (part->complete)
				{
					send_to_char(formatf("Task {Y%d{x: {Wcompleted{x\n\r", parts), ch);
					++completed;
				}
				else
					send_to_char(formatf("Task {Y%d{x: {Dnot completed{x\n\r", parts), ch);
			}

			if (completed == parts)
			{
				send_to_char("{YYour mission is complete!{x\n\r", ch);
				send_to_char("Turn in your mission before time runs out.\n\r", ch);
			}
		}
		return;
	}


	//
	// MISSION POINTS
	//
	if (!str_cmp(arg1, "points"))
	{
		sprintf(buf, "You have {Y%d{x mission points.\n\r", ch->missionpoints);
		send_to_char(buf, ch);
		return;
	}


	//
    // Mission time
    //
	if (!str_cmp(arg1, "time"))
	{
		if (ch->nextmission > 1)
		{
			sprintf(buf, "There are %d minutes remaining until you can go on another mission.\n\r", ch->nextmission);
			send_to_char(buf, ch);
		}
		else if (ch->nextmission == 1)
		{
			sprintf(buf, "There is less than a minute remaining until you can go on another mission.\n\r");
			send_to_char(buf, ch);
		}
		else if (list_size(ch->missions) < 1)
			send_to_char("You aren't currently on a mission.\n\r",ch);
		else
		{
			send_to_char("You are on the following missions:\n\r", ch);

			int m = 0;
			ITERATOR mit;
			MISSION_DATA *mission;
			iterator_start(&mit, ch->missions);
			while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
			{
				send_to_char(formatf("{W%2d{x) {Y%d{x minute%s left.\n\r", ++m, mission->timer, (mission->timer == 1)?"":"s"), ch);
			}
			iterator_stop(&mit);
		}
		return;
	}

	//
	// Mission Request
	//
    if (!str_cmp(arg1, "request"))
    {
		// TODO: Add player property for the default mission mode (for when they just do `mission request`)
		sh_int mode = MISSION_MODE_AUTO;

		/* For the following functions, a QM must be present. */
		for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && mob->pIndexData->pMissionary != NULL)
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

		CLASS_DATA *focus = NULL;
		sh_int clazz_type = CLASS_NONE;
		if (!str_prefix(arg2, "current"))
		{
			if (!str_prefix(argument, "type"))
			{
				mode = MISSION_MODE_CLASS_TYPE;
			}
			else if (!str_prefix(argument, "class"))
			{
				mode = MISSION_MODE_CLASS;
			}
			else
			{
				send_to_char("Syntax: mission request current class|type\n\r", ch);
				return;
			}
		}
		else if (arg2[0] == '?')
		{
			BUFFER *buffer = new_buf();

			add_buf(buffer, "Valid Types of Requests:\n\r");
			add_buf(buffer, formatf("%s\n\r", MXPCreateSend(ch->desc, "mission request current class", "Current Class")));
			add_buf(buffer, formatf("%s\n\r", MXPCreateSend(ch->desc, "mission request current type", "Current Type")));

			add_buf(buffer, "\n\rClass Types:\n\r");
			for(int i = 0; class_types[i].name; i++)
			{
				add_buf(buffer, formatf(" %s\n\r", MXPCreateSend(ch->desc, formatf("mission request %s", class_types[i].name), formatf("{+%s", class_types[i].name))));
			}

			ITERATOR lit;
			CLASS_LEVEL *level;
			add_buf(buffer, "\n\rClasses:\n\r");
			iterator_start(&lit, ch->pcdata->classes);
			while((level = (CLASS_LEVEL *)iterator_nextdata(&lit)))
			{
				add_buf(buffer, formatf(" %s\n\r", MXPCreateSend(ch->desc, formatf("mission request %s", level->clazz->name), formatf("{+%s", level->clazz->name))));
			}
			iterator_stop(&lit);


			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
			return;
		}
		else
		{
			focus = get_class_data(arg2);

			if (IS_VALID(focus))
			{
				if (!has_class_level(ch, focus))
				{
					send_to_char(formatf("You are not %s %s.\n\r", get_article(focus->display[ch->sex], false), focus->display[ch->sex]), ch);
					return;
				}

				clazz_type = focus->type;
			}
			else if( (clazz_type = stat_lookup(arg2, class_types, CLASS_NONE)) == CLASS_NONE )
			{
				send_to_char("No such class or class type.  Use 'mission request ?'.\n\r", ch);
				do_mission(ch, "request ?");
				return;
			}
		}

		// TODO: Add restrictions on what the missionary can provide

		act("$n asks $N for a mission.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act ("You ask $N for a mission.",ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		if (list_size(ch->missions) >= gconfig.max_missions)
		{
			sprintf(buf, "But you're already on enough missions!");
			do_say(mob, buf);
			return;
		}

		if (IS_DEAD(ch))
		{
			sprintf(buf, "You must come back to the world of the living first, %s.", HANDLE(ch));
			do_say(mob, buf);
			return;
		}

		if (!IS_IMMORTAL(ch))
		{
			if (ch->allowed_missions < 1 || list_size(ch->missions) >= gconfig.max_missions)
			{
				sprintf(buf, "You're very brave, %s, but let someone else have a chance.", ch->name);
				if (mob == NULL)
				{
					sprintf(buf, "do_mission(), mission request: MOB Was null, %s.\n\r", ch->name);
					bug (buf, 0);
					return;
				}

				do_say(mob, buf);
				sprintf(buf, "Come back later.");
				do_say(mob, buf);
				return;
			}
		}

		CLASS_DATA *clazz = get_current_class(ch);
		if (!IS_VALID(clazz))
		{
			send_to_char("Please select a class before requesting a mission.\n\r", ch);
			return;
		}

		ch->pending_mission = new_mission();
		ch->pending_mission->giver_type = MISSIONARY_MOB;
		ch->pending_mission->giver.pArea = mob->pIndexData->area;
		ch->pending_mission->giver.vnum = mob->pIndexData->vnum;
		ch->pending_mission->receiver_type = MISSIONARY_MOB;
		ch->pending_mission->receiver.pArea = mob->pIndexData->area;
		ch->pending_mission->receiver.vnum = mob->pIndexData->vnum;
		ch->pending_mission->clazz = clazz;				// This will determine the kind of mission parts you get
		ch->pending_mission->clazz_type = clazz_type;

		switch(mode)
		{
			case MISSION_MODE_AUTO:
				ch->pending_mission->clazz_restricted = false;
				ch->pending_mission->clazz_type_restricted = false;
				break;
			
			case MISSION_MODE_CLASS_TYPE:
				ch->pending_mission->clazz_restricted = false;
				ch->pending_mission->clazz_type_restricted = true;
				break;

			case MISSION_MODE_CLASS:
				ch->pending_mission->clazz_restricted = true;
				ch->pending_mission->clazz_type_restricted = false;
				break;
		}

		if (generate_mission(ch, mob, ch->pending_mission))
		{
			ch->pending_mission->generating = FALSE;

			sprintf(buf, "Thank you, brave %s!", HANDLE(ch));
			do_say(mob, buf);
		}
		else
		{
			sprintf(buf, "I'm sorry, %s, but I don't have any missions for you to do. Try again later.", ch->name);
			ch->nextmission = 3;
			do_say(mob, buf);
			free_mission(ch->pending_mission);
			ch->pending_mission = NULL;
			return;
		}

		if (ch->pending_mission != NULL)
		{
			MISSION_PART_DATA *mp;

			ch->pending_mission->timer = 0;
			for (mp = ch->pending_mission->parts; mp != NULL; mp = mp->next)
				ch->pending_mission->timer += mp->minutes;

			sprintf(buf, "You have %ld minute%s to complete this mission.", ch->pending_mission->timer, (ch->pending_mission->timer == 1)?"":"s");
			do_say(mob, buf);

			list_appendlink(ch->missions, ch->pending_mission);
			ch->pending_mission = NULL;
		}

		return;
	}

	//
	// Mission cancel
	//
	if (!str_cmp(arg1, "cancel"))
	{
		if (!IS_AWAKE(ch))
		{
			send_to_char("In your dreams, or what?\n\r", ch);
			return;
		}

		if (list_size(ch->missions) < 1)
		{
			send_to_char("You are not on a mission.\n\r", ch);
			return;
		}

		MISSION_DATA *mission;
		if (list_size(ch->missions) > 1)
		{
			int index;
			if (!is_number(arg2) || (index = atoi(arg2)) < 1 || index > list_size(ch->missions))
			{
				send_to_char("Syntax:  mission cancel <index>\n\r", ch);

				send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(ch->missions)), ch);
				return;
			}

			mission = list_nthdata(ch->missions, index);
		}
		else
			mission = list_nthdata(ch->missions, 1);


		if( mission == NULL )
		{
			send_to_char("No such mission.\n\r", ch);
			return;
		}

		if( mission->generating )
		{
			ch->nextmission = 1;
			send_to_char("Pending mission cancelled.\n\r", ch);
		}
		else
		{
			// Check for the GIVER
			switch(mission->giver_type)
			{
			case MISSIONARY_MOB:
				for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
				{
					if (IS_NPC(mob) && wnum_match_mob(mission->giver, mob))
						break;
				}
				break;

			case MISSIONARY_OBJ:
				// Check inventory first?
				for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
				{
					if (wnum_match_obj(mission->giver, obj))
						break;
				}
				if( obj == NULL )
				{
					for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
					{
						if (wnum_match_obj(mission->giver, obj))
							break;
					}
				}
				break;

			case MISSIONARY_ROOM:
				if( !ch->in_room->wilds && !ch->in_room->source &&
					wnum_match_room(mission->giver, ch->in_room))
				{
					room = ch->in_room;
					break;
				}
				break;
			}

			if( mob )
			{
				// Mobs will complain
				act("$n informs $N $e has cancelled $s mission.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				act ("You inform $N you have cancelled your mission.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

				sprintf(buf,
					"I am most displeased with your efforts, %s! This is obviously a job for someone with more talent than you.",
					ch->name);
				do_say(mob, buf);

				mob->tempstore[0] = 10;
				p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_CANCEL, NULL,0,0,0,0,0);
				ch->nextmission = mob->tempstore[0];
			}
			else if( obj )
			{
				// Objects will not complain by default

				obj->tempstore[0] = 10;
				p_percent_trigger( NULL, obj, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_CANCEL, NULL,0,0,0,0,0);
				ch->nextmission = obj->tempstore[0];
			}
			else if( room )
			{
				room->tempstore[0] = 10;
				p_percent_trigger( NULL, NULL, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_CANCEL, NULL,0,0,0,0,0);
				ch->nextmission = room->tempstore[0];
			}
			else
			{
				send_to_char("You can't do that here\n\r", ch);
				return;
			}
		}

		if(ch->nextmission < 1) ch->nextmission = 1;
		list_remlink(ch->missions, mission, true);
		return;
	}

	//
	// Mission complete
	//
    if (!str_cmp(arg1, "complete"))
    {
		MISSION_PART_DATA *part;
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

		if (list_size(ch->missions) < 1)
		{
			send_to_char("You are not on a mission.\n\r", ch);
			return;
		}

		MISSION_DATA *mission;
		if (list_size(ch->missions) > 1)
		{
			int index;
			if (!is_number(arg2) || (index = atoi(arg2)) < 1 || index > list_size(ch->missions))
			{
				send_to_char("Syntax:  mission complete <index>\n\r", ch);
				send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(ch->missions)), ch);
				return;
			}

			mission = list_nthdata(ch->missions, index);
		}
		else
			mission = list_nthdata(ch->missions, 1);


		if( mission == NULL )
		{
			send_to_char("No such mission.\n\r", ch);
			return;
		}

		// Check for the RECEIVER
		switch(mission->receiver_type)
		{
		case MISSIONARY_MOB:
			for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
			{
				if (IS_NPC(mob) && wnum_match_mob(mission->receiver, mob))
				{
					tempstores = mob->tempstore;
					break;
				}
			}
			break;

		case MISSIONARY_OBJ:
			// Check inventory first?
			for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
			{
				if (wnum_match_obj(mission->receiver, obj))
				{
					tempstores = obj->tempstore;
					break;
				}
			}
			if( obj == NULL )
			{
				for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
				{
					if (wnum_match_obj(mission->receiver, obj))
					{
						tempstores = obj->tempstore;
						break;
					}
				}
			}
			break;

		case MISSIONARY_ROOM:
			if( !ch->in_room->wilds && !ch->in_room->source &&
				wnum_match_room(mission->receiver, ch->in_room) )
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

		CLASS_LEVEL *level = get_class_level(ch, NULL);
		if (!IS_VALID(level))
		{
			send_to_char("Please select a class before completing a mission.\n\r", ch);
			return;
		}

		if (mission->clazz_restricted)
		{
			if (level->clazz != mission->clazz)
			{
				send_to_char(formatf("Please switch to %s before completing this mission.\n\r", mission->clazz->name), ch);
				return;
			}
		}
		else if (mission->clazz_type_restricted)
		{
			if(level->clazz->type != mission->clazz_type)
			{
				char *type = flag_string(class_types, mission->clazz_type);

				send_to_char(formatf("Please switch to %s %s class before completing this mission.\n\r", get_article(type, false), type), ch);
				return;
			}
		}
		else if (!__classes_compatible(level->clazz, mission->clazz))
		{
			if (IS_SET(mission->clazz->flags, CLASS_COMBATIVE))
			{
				send_to_char("Please select a combat class before completing this mission.\n\r", ch);
			}
			else
			{
				char *type = flag_string(class_types, mission->clazz_type);

				send_to_char(formatf("Please switch to %s %s class before completing this mission.\n\r", get_article(type, false), type), ch);
			}

			return;
		}
		// Compatible

		if( mob )
		{
			act("$n informs $N $e has completed $s mission.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act ("You inform $N you have completed your mission.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

		found = FALSE;
		incomplete = FALSE;
		for (part = mission->parts; part != NULL; part = part->next)
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
					"I am most displeased with your efforts, %s! This is obviously a job for someone with more talent than you.",
					ch->name);
				do_say(mob, buf);
			}

			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_INCOMPLETE, NULL,0,0,0,0,0);

			tempstores[0] = 10;
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTMISSION, NULL,0,0,0,0,0);

			ch->nextmission = tempstores[0];
			if(ch->nextmission < 1) ch->nextmission = 1;

			list_remlink(ch->missions, mission, true);
			return;
		}

		pointreward = 0;
		reward = 0;
		pracreward = 0;
		expreward = 0;
		i = 0;

		log_string("missions.c, do_mission: (complete) Checking mission parts...");

		// Add up all the different rewards.
		for (part = mission->parts; part != NULL; part = part->next)
		{
			i++;

			if (part->complete)
			{
				reward += number_range(500, 1000);
				pointreward += number_range(10,20);
				expreward += number_range(ch->tot_level*50,
				ch->tot_level*100);
				pracreward += 1;

				// TODO: turn this into a trait
				if (get_current_class(ch) == gcl_crusader)
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
				sprintf(buf, "Congratulations on completing your mission!");
				do_say(mob,buf);
			}
			ch->pcdata->missions_completed++;
		}
		else
		{
			if( mob )
			{
				sprintf(buf, "I see you haven't fully completed your mission, but I applaud your courage anyway!");
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
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_INCOMPLETE, NULL,0,0,0,0,0);
		else
			p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_COMPLETE, NULL,0,0,0,0,0);
		expreward = tempstores[0];
		pointreward = tempstores[1];
		pracreward = tempstores[2];
		reward = tempstores[3];

		// Clamp to zero
		expreward = UMAX(expreward,0);
		reward = UMAX(reward,0);
		pracreward = UMAX(pracreward,0);
		pointreward = UMAX(pointreward, 0);

		if (boost_table[BOOST_MP].boost != 100)
			pointreward = (pointreward * boost_table[BOOST_MP].boost)/100;


		if( mob ) {
			sprintf(buf, "As a reward, I am giving you %d mission points and %d silver.",
				pointreward, reward);
			do_say(mob,buf);
		}
		else
		{
			sprintf(buf, "As a reward, you receive %d mission points and %d silver.\n\r", pointreward, reward);
			send_to_char(buf, ch);
		}

		// Only display "MISSION POINTS boost!" if a mp boost is active -- Areo
		if(boost_table[BOOST_MP].boost != 100)
			send_to_char("{WMISSION POINTS boost!{x\n\r", ch);

		ch->silver += reward;
		ch->missionpoints += pointreward;

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
		if(level->level < level->clazz->max_level)
		{
			sprintf(buf, "You gain %d experience points!\n\r", expreward);
			send_to_char(buf, ch);

			gain_exp(ch, NULL, expreward);
		}

		tempstores[0] = 10;
		p_percent_trigger( mob, obj, room, NULL, ch, NULL, NULL,NULL, NULL, TRIG_POSTMISSION, NULL,0,0,0,0,0);

		ch->nextmission = tempstores[0];
		if(ch->nextmission < 1) ch->nextmission = 1;

		list_remlink(ch->missions, mission, true);
    }
    else
    {
		send_to_char("MISSION commands: POINTS INFO TIME REQUEST CANCEL COMPLETE.\n\r", ch);
		send_to_char("For more information, type 'HELP MISSION'.\n\r",ch);
    }
}


/*
 * Generate a quest. Returns TRUE if a quest is found.
 */
bool generate_mission(CHAR_DATA *ch, CHAR_DATA *missionary, MISSION_DATA *mission)
{
	MISSION_PART_DATA *part;
	OBJ_DATA *scroll;
	int parts;
	int i;

	mission->generating = TRUE;
	mission->scripted = FALSE;

	int total_level = 0;
	if (mission->clazz_restricted)
	{
		CLASS_LEVEL *level = get_class_level(ch, mission->clazz);
		if (!IS_VALID(level)) return false;

		total_level = level->level;
	}
	else if(mission->clazz_type_restricted || !IS_SET(mission->clazz->flags, CLASS_COMBATIVE))
	{
		// TODO: Replace with appropriate type of level counter
		ITERATOR lit;
		CLASS_LEVEL *level;
		iterator_start(&lit, ch->pcdata->classes);
		while((level = (CLASS_LEVEL *)iterator_nextdata(&lit)))
		{
			if (level->clazz->type == mission->clazz_type)
				total_level += level->level;
		}
		iterator_stop(&lit);
	}
	else
	{
		ITERATOR lit;
		CLASS_LEVEL *level;
		iterator_start(&lit, ch->pcdata->classes);
		while((level = (CLASS_LEVEL *)iterator_nextdata(&lit)))
		{
			if (IS_SET(level->clazz->flags, CLASS_COMBATIVE))
				total_level += level->level;
		}
		iterator_stop(&lit);
	}

	if (total_level <= 30)
		parts = number_range(1, 3);
	else if (total_level <= 60)
		parts = number_range(3, 6);
	else if (total_level <= 90)
		parts = number_range(7, 9);
	else
		parts = number_range(8, 15);

	/* fun */
	bool bFun = number_percent() < 5;
	if (bFun)
		parts = parts * 2;

	MISSIONARY_DATA *md = missionary->pIndexData->pMissionary;

	// MORE FUN
	missionary->tempstore[0] = parts;				// Number of parts to do (In-Out)
	missionary->tempstore[1] = bFun ? 1 : 0;		// Whether this was a F.U.N. quest (In)
	missionary->tempstore[2] = md->scroll.auid;	// Default quest scroll item (AUID)
	missionary->tempstore[3] = md->scroll.vnum;	// Default quest scroll item (VNUM)
	// register1 is total level for use in number of parts
	if(p_percent_trigger( missionary, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_PREMISSION, NULL,total_level,0,0,0,0))
		return FALSE;
	parts = missionary->tempstore[0];				// Updated number of parts to do
	if( parts < 1 ) parts = 1;					//    Require at least one part.

	WNUM scroll_wnum;
	scroll_wnum.pArea = get_area_from_uid(missionary->tempstore[2]);
	scroll_wnum.vnum = missionary->tempstore[3];
	if( !scroll_wnum.pArea || scroll_wnum.vnum < 1 )
	{
		scroll_wnum.pArea = get_area_from_uid(md->scroll.auid);
		scroll_wnum.vnum = md->scroll.vnum;
	}

	for (i = 0; i < parts; i++)
	{
		part = new_mission_part();
		part->next = mission->parts;
		mission->parts = part;
		part->index = parts - i;

		if (generate_mission_part(ch, missionary, mission, part, parts - i))
			continue;
		else
			return FALSE;
	}

	// create the scroll
	scroll = generate_mission_scroll(ch, missionary->short_descr, mission, scroll_wnum,
		md->header, md->footer, md->prefix, md->suffix, md->line_width);

	if( scroll == NULL )
	{
		// COMPLAIN
		return FALSE;
	}

	free_string(scroll->name);
	free_string(scroll->short_descr);
	free_string(scroll->description);

	scroll->name = str_dup(md->keywords);
	scroll->short_descr = str_dup(md->short_descr);
	scroll->description = str_dup(md->long_descr);


    act("$N gives $p to $n.", ch, missionary, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
    act("$N gives you $p.",   ch, missionary, NULL, scroll, NULL, NULL, NULL, TO_CHAR);
    obj_to_char(scroll, ch);
    return TRUE;
}

/* Set up a mission part. */
bool generate_mission_part(CHAR_DATA *ch, CHAR_DATA *missionary, MISSION_DATA *mission, MISSION_PART_DATA *part, int partno)
{
	// The mission part must return a positive value to be valid
	//  returning a zero due to "end 0" or not having the QUEST_PART trigger will be considered invalid
	//  errors in script execution will be negative, so will be considered invalid.
	// Part Number is register1
    return p_percent_trigger( missionary, NULL, NULL, NULL, ch, NULL, NULL,NULL, NULL, TRIG_MISSION_PART, NULL,partno,0,0,0,0) > 0;
}


/* Called from update_handler() by pulse_area */
void mission_update(void)
{
	DESCRIPTOR_DATA *d;
	CHAR_DATA *ch;
	log_string("Update quests...");

	for (d = descriptor_list; d != NULL; d = d->next)
	{
		if (d->character != NULL && d->connected == CON_PLAYING)
		{
			ch = d->character;

			if (ch->nextmission > 0)
			{
				ch->nextmission--;
				if (ch->nextmission == 0)
				{
					send_to_char("{WYou may now request a mission again.{x\n\r", ch);
					return;
				}
			}

			if (list_size(ch->missions) > 0)
			{
				ITERATOR mit;
				MISSION_DATA *mission;

				iterator_start(&mit, ch->missions);
				while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
				{
					if (!mission->generating)
					{
						if (--mission->timer <= 0)
						{
							send_to_char("{RYou have run out of time for a mission!{x\n\r", ch);
							iterator_remcurrent(&mit);
						}
						else if (mission->timer < 6)
						{
							send_to_char(formatf("{RYou only have {W%d{R minute%s remaining to finish a mission!\n\r", mission->timer, (mission->timer == 1)?"":"s"), ch);
						}
					}
				}
				iterator_stop(&mit);
			}
		}
	}
}



void check_mission_rescue_mob(CHAR_DATA *ch, bool show)
{
    CHAR_DATA *mob;
    char buf[MAX_STRING_LENGTH];
    int i;
    bool found = TRUE;

    if (IS_NPC(ch))
    {
		bug("check_mission_rescue_mob: NPC", 0);
		return;
    }

	if (list_size(ch->missions) > 0)
	{
		ITERATOR mit;
		MISSION_DATA *mission;
		iterator_start(&mit, ch->missions);
		while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
		{
		    MISSION_PART_DATA *part;
			i = 0;
			for(part = mission->parts; part; part = part->next)
			{
				++i;

				if (part->complete) continue;

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

						// TODO: Mark mob as having been grabbed by a mission, so it skips aggression checks
						if (IS_NPC(mob) && IS_SET(mob->act[0], ACT_AGGRESSIVE))
							REMOVE_BIT(mob->act[0], ACT_AGGRESSIVE);
						
						// TOOD: Remove AFF2_AGGRESSIVE

						found = TRUE;
						break;
					}

					mob = mob->next_in_room;
				}

				if (found && !part->complete)
				{
					if( show )
					{
						sprintf(buf, "{YYou have completed task %d of a mission!{x\n\r", i);
						send_to_char(buf, ch);
					}

					part->complete = TRUE;
					break;
				}


			}
		}
		iterator_stop(&mit);
	}
}


void check_mission_retrieve_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool show)
{
    char buf[MAX_STRING_LENGTH];
    int i;

    if (obj == NULL || IS_MONEY(obj))
    {
		bug("check_mission_retrieve_obj: bad obj!", 0);
		return;
    }

    if (IS_NPC(ch))
    {
		bug("check_mission_retrieve_obj: NPC", 0);
		return;
    }

	if (list_size(ch->missions) > 0)
	{
		ITERATOR mit;
		MISSION_DATA *mission;
		iterator_start(&mit, ch->missions);
		while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
		{
		    MISSION_PART_DATA *part;
			i = 0;
			for(part = mission->parts; part; part = part->next)
			{
				++i;

				if (part->complete) continue;

				if (part->pObj == obj)
				{
					if( show )
					{
						sprintf(buf, "{YYou have completed task %d of a mission!{x\n\r", i);
						send_to_char(buf, ch);
					}

					part->complete = TRUE;
				}
			}
		}
		iterator_stop(&mit);
	}
}


void check_mission_slay_mob(CHAR_DATA *ch, CHAR_DATA *mob, bool show)
{
    char buf[MAX_STRING_LENGTH];
    int i;

    if (!IS_NPC(mob))
    {
		bug("check_mission_slay_mob: bad mob!", 0);
		return;
    }

    if (IS_NPC(ch))
    {
		bug("check_mission_slay_mob: NPC", 0);
		return;
    }

	if (list_size(ch->missions) > 0)
	{
		ITERATOR mit;
		MISSION_DATA *mission;
		iterator_start(&mit, ch->missions);
		while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
		{
		    MISSION_PART_DATA *part;
			i = 0;
			for(part = mission->parts; part; part = part->next)
			{
				++i;

				if (part->complete) continue;

				if (IS_NPC(mob) && part->area == mob->pIndexData->area && part->mob == mob->pIndexData->vnum && !part->complete)
				{
					if( show ) {
						sprintf(buf, "{YYou have completed task %d of a mission!{x\n\r", i);
						send_to_char(buf, ch);
					}

					part->complete = TRUE;
				}
			}
		}
		iterator_stop(&mit);
	}
}

void check_mission_travel_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, bool show)
{
    char buf[MAX_STRING_LENGTH];
    int i;

    if (room == NULL)
    {
		bug("check_mission_travel_room: bad room!", 0);
		return;
    }

    if (IS_NPC(ch))
    {
		bug("check_mission_travel_room: NPC", 0);
		return;
    }

	if (list_size(ch->missions) > 0)
	{
		ITERATOR mit;
		MISSION_DATA *mission;
		iterator_start(&mit, ch->missions);
		while((mission = (MISSION_DATA *)iterator_nextdata(&mit)))
		{
		    MISSION_PART_DATA *part;
			i = 0;
			for(part = mission->parts; part; part = part->next)
			{
				++i;

				if (part->complete) continue;

				ROOM_INDEX_DATA *target_room = get_room_index(part->area, part->room);

				/* Not going by room vnum to prevent multiple rooms with the same name */
				if (target_room != NULL && !str_cmp(target_room->name, room->name))
				{

					if( show )
					{
						sprintf(buf, "{YYou have completed task %d of a mission!{x\n\r", i);
						send_to_char(buf, ch);
					}

					part->complete = TRUE;
				}
			}
		}
		iterator_stop(&mit);
	}
}

bool check_mission_custom_task(CHAR_DATA *ch, int mission_no, int task, bool show)
{
    char buf[MAX_STRING_LENGTH];
    int i;

    if (IS_NPC(ch))
    {
		bug("check_mission_custom_task: NPC", 0);
		return false;
    }

	if (mission_no < 1 || mission_no > list_size(ch->missions))
	{
		bug("check_mission_custom_task: bad mission", 0);
		return false;
	}

	MISSION_DATA *mission = list_nthdata(ch->missions, mission_no);

	bool found = false;
	if (mission)
	{
		MISSION_PART_DATA *part;
		i = 0;
		for(part = mission->parts; part; part = part->next)
		{
			++i;

			// Not the current task nor is a custom task
			if( task != i || !part->custom_task )
				continue;

			if (part->complete) continue;

			if( show )
			{
				sprintf(buf, "{YYou have completed task %d of a mission!{x\n\r", i);
				send_to_char(buf, ch);
			}

			part->complete = true;
			found = true;
		}
	}

	return found;
}




CHAR_DATA *get_renewer_here(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *mob;

	if (argument[0] == '\0')
	{
		for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
		{
			if (IS_NPC(mob) && IS_SET(mob->act[1], ACT2_RENEWER))
			{
				// They must be peaceful with them
				if(check_mob_factions_peaceful(ch, mob))
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

		if (!IS_NPC(mob) || !IS_SET(mob->act[1], ACT2_RENEWER) || !check_mob_factions_peaceful(ch, mob))
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
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_PRERENEW, NULL,0,0,0,0,0) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->missionpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d mission points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_PET;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->pet, NULL, NULL, NULL, TRIG_RENEW, NULL,0,0,0,0,0);

			sprintf(buf, "{YYou renew $n with $N for %d mission points.{x", cost);
			act(buf, ch->pet, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->missionpoints -= cost;
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
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_PRERENEW, NULL,0,0,0,0,0) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->missionpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d mission points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_MOUNT;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, ch->mount, NULL, NULL, NULL, TRIG_RENEW, NULL,0,0,0,0,0);

			sprintf(buf, "{YYou renew $n with $N for %d mission points.{x", cost);
			act(buf, ch->mount, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->missionpoints -= cost;
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
			if(p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_PRERENEW, NULL,0,0,0,0,0) <= 0)
				return;

			// Check QUESTPOINTS
			cost = mob->tempstore[0];
			if( cost <= 0 )
			{
				sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
				do_say(mob, buf);
				return;
			}

			if (ch->missionpoints < cost)
			{
				sprintf(buf, "Sorry %s, but it would take %d mission points for me to renew that.", pers(ch, mob), cost);
				do_say(mob, buf);
				return;
			}

			mob->tempstore[0] = cost;
			mob->tempstore[1] = STOCK_GUARD;
			p_percent_trigger( mob, NULL, NULL, NULL, ch, guard, NULL, NULL, NULL, TRIG_RENEW, NULL,0,0,0,0,0);

			sprintf(buf, "{YYou renew $n with $N for %d mission points.{x", cost);
			act(buf, guard, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
			ch->missionpoints -= cost;
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

		if ((obj = get_obj_carry(ch, arg2, ch)) == NULL)
		{
			sprintf(buf, "You don't have that item, %s.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}

		cost = obj->cost/10;
		mob->tempstore[0] = UMAX(cost, 1);
		mob->tempstore[1] = STOCK_OBJECT;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRERENEW, NULL,0,0,0,0,0) <= 0)
			return;

		cost = mob->tempstore[0];
		if( cost <= 0 )
		{
			sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}


		if (ch->missionpoints < cost)
		{
			sprintf(buf, "Sorry %s, but it would take %d mission points for me to renew that item.", pers(ch, mob), cost);
			do_say(mob, buf);
			return;
		}

		mob->tempstore[0] = cost;
		mob->tempstore[1] = STOCK_OBJECT;
		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_RENEW, NULL,0,0,0,0,0);

		sprintf(buf, "{YYou renew $p with $N for %d mission points.{x", cost);
		act(buf, ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		ch->missionpoints -= cost;

		return;
	}
	else if( !str_prefix(arg1, "custom") )
	{
		mob = get_renewer_here(ch, arg3);
		if( mob == NULL )
			return;

		mob->tempstore[0] = 0;	// Customs REQUIRE the script to specify the cost
		mob->tempstore[1] = STOCK_CUSTOM;
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRERENEW, arg2,0,0,0,0,0) <= 0)
			return;

		cost = mob->tempstore[0];
		if( cost <= 0 )
		{
			sprintf(buf, "Sorry %s, but I cannot improve that.", pers(ch, mob));
			do_say(mob, buf);
			return;
		}


		if (ch->missionpoints < cost)
		{
			sprintf(buf, "Sorry %s, but it would take %d mission points for me to renew that.", pers(ch, mob), cost);
			do_say(mob, buf);
			return;
		}

		mob->tempstore[0] = cost;
		mob->tempstore[1] = STOCK_CUSTOM;
		free_string(mob->tempstring);
		mob->tempstring = &str_empty[0];
		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW, arg2,0,0,0,0,0);

		if(!IS_NULLSTR(mob->tempstring))
		{
			sprintf(buf, "{YYou renew %s with $N for %d mission points.{x", mob->tempstring, cost);
		}
		else
		{
			sprintf(buf, "{YYou renew %s with $N for %d mission points.{x", arg2, cost);
		}
		act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		ch->missionpoints -= cost;

		return;
	}
	else if(!str_prefix(arg1, "list"))
	{
		mob = get_renewer_here(ch, arg2);
		if( mob == NULL )
			return;


		act("{YYou ask $N for a list of things $E can renew.{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n asks $N for a list of things $E can renew.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		if(p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_RENEW_LIST, NULL,0,0,0,0,0))
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
				if( !IS_SET(ch->pet->act[1], ACT2_HIRED) || (ch->pet->hired_to < 1) )
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

		p_percent_trigger( mob, NULL, NULL, NULL, ch, NULL, NULL,obj, NULL, TRIG_RENEW, NULL,0,0,0,0,0);

		ch->questpoints -= cost;
	}
*/

}


bool is_mission_mob(CHAR_DATA *ch, CHAR_DATA *victim)
{
	if (!IS_NPC(victim)) return false;

	bool found = false;
	ITERATOR mit;
	MISSION_DATA *mission;

	iterator_start(&mit, ch->missions);
	while(!found && (mission = (MISSION_DATA *)iterator_nextdata(&mit)))
	{
		for(MISSION_PART_DATA *part = mission->parts; part; part = part->next)
		{
			if (part->mob != -1 && !part->complete)
			{
				if (part->mob == victim->pIndexData->vnum && part->area == victim->pIndexData->area)
				{
					found = true;
					break;
				}
			}
		}
	}
	iterator_stop(&mit);

	return found;
}

