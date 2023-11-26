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
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "merc.h"
#include "interp.h"
#include "olc.h"
#include "wilds.h"
#include "scripts.h"

void dungeon_check_commence(DUNGEON *dng, CHAR_DATA *ch);

char *	const	dir_name	[]		=
{
    "north", "east", "south", "west", "up", "down", "northeast",  "northwest", "southeast", "southwest"
};

int parse_door(char *name)
{
	int i;
	for(i = 0; i < MAX_DIR; i++) {
		if( !str_cmp(name, dir_name[i]) )
			return i;
	}

	return -1;
}


const	sh_int	rev_dir		[]		=
{
    2, 3, 0, 1, 5, 4, 9, 8, 7, 6
};


const	sh_int	movement_loss	[SECT_MAX]	=
{
	1,		/* SECT_INSIDE */
	2,		/* SECT_CITY */
	2,		/* SECT_FIELD */
	3,		/* SECT_FOREST */
	4,		/* SECT_HILLS */
	6,		/* SECT_MOUNTAIN */
	15,		/* SECT_WATER_SWIM */
	50,		/* SECT_WATER_NOSWIM */
	6,		/* SECT_TUNDRA */
	2,		/* SECT_AIR */
	7,		/* SECT_DESERT */
	5,		/* SECT_NETHERWORLD */
	2,		/* SECT_DOCK */
	3,		/* SECT_ENCHANTED_FOREST */
	17,		/* SECT_TOXIC_BOG */
	1,		/* SECT_CURSED_SANCTUM */
	5,		/* SECT_BRAMBLE */
	17,		/* SECT_SWAMP */
	15,		/* SECT_ACID */
	50,		/* SECT_LAVA */
	7,		/* SECT_SNOW */
	3,		/* SECT_ICE */
	4,		/* SECT_CAVE */
	50,		/* SECT_UNDERWATER */
	50,		/* SECT_DEEP_UNDERWATER */
	4,		/* SECT_JUNGLE */
};

int get_player_classnth(CHAR_DATA *ch)
{
	int num = 4;

	if(ch->pcdata->class_mage == -1) --num;
	if(ch->pcdata->class_cleric == -1) --num;
	if(ch->pcdata->class_thief == -1) --num;
	if(ch->pcdata->class_warrior == -1) --num;

	return UMAX(1,num);
}

ROOM_INDEX_DATA *exit_destination(EXIT_DATA *pexit)
{
	ROOM_INDEX_DATA *in_room = NULL;
	ROOM_INDEX_DATA *to_room = NULL;
	WILDS_DATA *in_wilds = NULL;
	WILDS_DATA *to_wilds = NULL;
	WILDS_TERRAIN *pTerrain;
	int to_vroom_x = 0;
	int to_vroom_y = 0;

	if(!pexit || !pexit->from_room) {
/*		wiznet("exit_destination()->NULL pexit or NULL pexit->from_room",NULL,NULL,WIZ_TESTING,0,0); */
		return NULL;
	}

	in_room = pexit->from_room;
	in_wilds = in_room->wilds;

	if ( IS_SET(pexit->exit_info, EX_PREVFLOOR) )
	{
		//char buf[MSL];

		if( !IS_VALID(pexit->from_room->instance_section) ||
			!IS_VALID(pexit->from_room->instance_section->instance) ||
			!IS_VALID(pexit->from_room->instance_section->instance->dungeon) )
		{
			return NULL;
		}

		int floor = pexit->from_room->instance_section->instance->floor - 1;
		DUNGEON *dungeon = pexit->from_room->instance_section->instance->dungeon;

		if( floor < 1 )	// Heading out the dungeon
			return dungeon->entry_room;
		else
		{
			INSTANCE *prev_floor = list_nthdata(dungeon->floors, floor);

			if( prev_floor )
				return prev_floor->exit;
		}

		return NULL;
	}

	if ( IS_SET(pexit->exit_info, EX_NEXTFLOOR) )
	{
		if( !IS_VALID(pexit->from_room->instance_section) ||
			!IS_VALID(pexit->from_room->instance_section->instance) ||
			!IS_VALID(pexit->from_room->instance_section->instance->dungeon) )
		{
			return NULL;
		}

		int floor = pexit->from_room->instance_section->instance->floor + 1;
		DUNGEON *dungeon = pexit->from_room->instance_section->instance->dungeon;

		if( floor > list_size(dungeon->floors) )	// Heading out the dungeon
			return dungeon->exit_room;
		else
		{
			INSTANCE *next_floor = list_nthdata(dungeon->floors, floor);

			if( next_floor )
				return next_floor->entrance;
		}

		return NULL;
	}

	if (pexit->from_room->wilds) {
		/* Char is in a wilds virtual room. */
		if (IS_SET(pexit->exit_info, EX_VLINK)) {
			/* This is a vlink to different wilderness location, be it on the same map or not */
			if (pexit->wilds.wilds_uid > 0) {
				to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
				to_vroom_x = pexit->wilds.x;
				to_vroom_y = pexit->wilds.y;

				if (!(pTerrain = get_terrain_by_coors(to_wilds, to_vroom_x, to_vroom_y))) {
/*					wiznet("exit_destination()->NULL A",NULL,NULL,WIZ_TESTING,0,0); */
					return NULL;
				}

				to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
				if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y))) {
/*					wiznet("exit_destination()->NULL B",NULL,NULL,WIZ_TESTING,0,0); */
					return NULL;
				}

			/* Otherwise, Exit leads to a static room. */
			} else if (!(to_room = pexit->u1.to_room)) {
/*				wiznet("exit_destination()->NULL C",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}
		} else {
			/* In wilds and exit leads to another vroom. */
			to_wilds = in_wilds;
			to_vroom_x = get_wilds_vroom_x_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);
			to_vroom_y = get_wilds_vroom_y_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);

			if (!(pTerrain = get_terrain_by_coors(in_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL D",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}

			to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
			if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL E",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}
		}
	} else {
		/* Char is in a static room. */
		if (IS_SET(pexit->exit_info, EX_VLINK)) {
			/* Exit is a vlink, leading to a wilds vroom. */
			to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
			to_vroom_x = pexit->wilds.x;
			to_vroom_y = pexit->wilds.y;

			if (!(pTerrain = get_terrain_by_coors(to_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL F",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}

			to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
			if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL G",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}

		} else if (IS_SET(pexit->exit_info, EX_ENVIRONMENT)) {
			if(!IS_SET(pexit->from_room->room_flag[1],ROOM_VIRTUAL_ROOM)) {
/*				wiznet("exit_destination()->NULL H",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}

			if(!(to_room = get_environment(pexit->from_room))) {
/*				wiznet("exit_destination()->NULL I",NULL,NULL,WIZ_TESTING,0,0); */
				return NULL;
			}
		} else if (!(to_room = pexit->u1.to_room)) {
/*			wiznet("exit_destination()->NULL J",NULL,NULL,WIZ_TESTING,0,0); */
			return NULL;
		}
	}

/*	wiznet("exit_destination()->return K",NULL,NULL,WIZ_TESTING,0,0); */
	return to_room;
}

bool exit_destination_data(EXIT_DATA *pexit, DESTINATION_DATA *pDest)
{
	ROOM_INDEX_DATA *in_room = NULL;
	ROOM_INDEX_DATA *to_room = NULL;
	WILDS_DATA *in_wilds = NULL;
	WILDS_DATA *to_wilds = NULL;
	int to_vroom_x = 0;
	int to_vroom_y = 0;

	if(!pexit || !pexit->from_room || !pDest) {
		return FALSE;
	}

	in_room = pexit->from_room;
	in_wilds = in_room->wilds;

	if (pexit->from_room->wilds) {
		/* Char is in a wilds virtual room. */
		if (IS_SET(pexit->exit_info, EX_VLINK)) {
			/* This is a vlink to different wilderness location, be it on the same map or not */
			if (pexit->wilds.wilds_uid > 0) {
				to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
				to_vroom_x = pexit->wilds.x;
				to_vroom_y = pexit->wilds.y;

				if (!check_for_bad_room(to_wilds, to_vroom_x, to_vroom_y)) {
/*					wiznet("exit_destination()->NULL A",NULL,NULL,WIZ_TESTING,0,0); */
					return FALSE;
				}

				to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
				if( !to_room ) {
					pDest->wilds = to_wilds;
					pDest->wx = to_vroom_x;
					pDest->wy = to_vroom_y;
				}


			/* Otherwise, Exit leads to a static room. */
			} else if (!(to_room = pexit->u1.to_room)) {
				return FALSE;
			}
		} else {
			/* In wilds and exit leads to another vroom. */
			to_wilds = in_wilds;
			to_vroom_x = get_wilds_vroom_x_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);
			to_vroom_y = get_wilds_vroom_y_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);

			if (!check_for_bad_room(in_wilds, to_vroom_x, to_vroom_y)) {
				return FALSE;
			}

			to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
			if( !to_room ) {
				pDest->wilds = to_wilds;
				pDest->wx = to_vroom_x;
				pDest->wy = to_vroom_y;
			}
		}
	} else {
		/* Char is in a static room. */
		if (IS_SET(pexit->exit_info, EX_VLINK)) {
			/* Exit is a vlink, leading to a wilds vroom. */
			to_wilds = get_wilds_from_uid(NULL, pexit->wilds.wilds_uid);
			to_vroom_x = pexit->wilds.x;
			to_vroom_y = pexit->wilds.y;

			if (!check_for_bad_room(to_wilds, to_vroom_x, to_vroom_y)) {
/*				wiznet("exit_destination()->NULL F",NULL,NULL,WIZ_TESTING,0,0); */
				return FALSE;
			}

			to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
			if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL G",NULL,NULL,WIZ_TESTING,0,0); */
				return FALSE;
			}

		} else if (IS_SET(pexit->exit_info, EX_ENVIRONMENT)) {
			if(!IS_SET(pexit->from_room->room_flag[1],ROOM_VIRTUAL_ROOM)) {
/*				wiznet("exit_destination()->NULL H",NULL,NULL,WIZ_TESTING,0,0); */
				return FALSE;
			}

			if(!(to_room = get_environment(pexit->from_room))) {
/*				wiznet("exit_destination()->NULL I",NULL,NULL,WIZ_TESTING,0,0); */
				return FALSE;
			}
		} else if (!(to_room = pexit->u1.to_room)) {
			return FALSE;
		}
	}

	pDest->room = to_room;

	return TRUE;
}

void move_char(CHAR_DATA *ch, int door, bool follow)
{
	CHAR_DATA *fch;
	CHAR_DATA *fch_next;
	ROOM_INDEX_DATA *in_room;
	ROOM_INDEX_DATA *to_room;
//	WILDS_DATA *in_wilds = NULL;
/*	WILDS_DATA *to_wilds = NULL;
	WILDS_TERRAIN *pTerrain; */
	EXIT_DATA *pexit;
/*	AREA_DATA *pArea = NULL;
	int to_vroom_x = 0;
	int to_vroom_y = 0; */
	char buf[MAX_STRING_LENGTH];
	int move;

	/* Check door variable is valid */
	if (door < 0 || door >= MAX_DIR) {
		bug("Do_move: bad door %d.", door);
		return;
	}

	/* Check char's in_room index pointer is valid */
	if (!ch->in_room) {
		sprintf(buf, "move_char: ch->in_room was null for %s (%ld)", HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
		bug(buf, 0);
		return;
	}

	// Allow scripting to screw with the direction of travel
	ch->tempstore[0] = door;
	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MOVE_CHAR, dir_name[ch->tempstore[0]],0,0,0,0,0))
		return;

	ch->in_room->tempstore[0] = URANGE(0,ch->tempstore[0],(MAX_DIR-1));;
	if(p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MOVE_CHAR, dir_name[ch->in_room->tempstore[0]],0,0,0,0,0))
		return;

	door = URANGE(0,ch->in_room->tempstore[0],(MAX_DIR-1));

	/* Exit trigger, if activated, bail out. Only PCs are triggered. */
	if (!IS_NPC(ch) && (p_exit_trigger(ch, door, PRG_MPROG,0,0,0,0,0) ||
			p_exit_trigger(ch, door, PRG_OPROG,0,0,0,0,0) || p_exit_trigger(ch, door, PRG_RPROG,0,0,0,0,0)))
		return;

	/* Check if char is "on" something. preventing movement */
	if (ch->on && ch->on_compartment && !IS_SET(ch->on_compartment->flags, COMPARTMENT_ALLOW_MOVE)) {
		act("You must get off $p first.", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	/* Ok, take a copy of the char's location pointers */
	in_room = ch->in_room;

	DUNGEON *in_dungeon = get_room_dungeon(in_room);
	INSTANCE *in_instance = get_room_instance(in_room);

	// Check whether they are in a dun
	if (IS_VALID(in_dungeon) && !IS_SET(in_dungeon->flags, DUNGEON_COMMENCED))
	{
		send_to_char("The dungeon hasn't commenced yet.  Please wait to move.\n\r", ch);
		return;
	}

	if (!(pexit = in_room->exit[door])) {
		send_to_char("Alas, you cannot move that way.\n\r", ch);
/*		wiznet("move_char()-> NULL pexit",NULL,NULL,WIZ_TESTING,0,0); */
		return;
	}

	// Requires that you MUST see the exit in order to use it.
	if (IS_SET(pexit->exit_info, EX_HIDDEN) && !IS_SET(pexit->exit_info, EX_FOUND) && IS_SET(pexit->exit_info, EX_MUSTSEE) )
	{
		// If they are not an immortal or have holylight off, they can't use the exit
		if( !IS_IMMORTAL(ch) || !IS_SET(ch->act[0],PLR_HOLYLIGHT) )
		{
			send_to_char("Alas, you cannot move that way.\n\r", ch);
			return;
		}
	}

	if(!(to_room = exit_destination(pexit))) {
		send_to_char ("Alas, you cannot go that way.\n\r", ch);
/*		wiznet("move_char()-> NULL to_room",NULL,NULL,WIZ_TESTING,0,0); */
		return;
	}

	if(!can_see_room (ch, to_room)) {
		send_to_char ("Alas, you cannot go that way.\n\r", ch);
		return;
	}

	/* @@@NIB - Get this going again soon :D
	drunk_walk(ch, door);
	*/

	if (IS_SET(pexit->exit_info, EX_HIDDEN) && (ch->level <= LEVEL_IMMORTAL) && (!IS_AFFECTED(ch, AFF_PASS_DOOR))) {
		send_to_char("{ROuch!{w You found a secret passage!{x\n\r", ch);
		REMOVE_BIT(pexit->exit_info, EX_HIDDEN);
	}

	move = 0; /* for NPCs only */
	if (!IS_NPC(ch)) {
		if (in_room->sector_type == SECT_AIR || to_room->sector_type == SECT_AIR) {
			if (MOUNTED(ch)) {
				if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING)) {
					send_to_char("Your mount can't fly.\n\r", ch);
					return;
				}
			} else {
				if (!IS_AFFECTED(ch, AFF_FLYING) && !IS_IMMORTAL(ch)) {
					send_to_char("You can't fly.\n\r", ch);
					return;
				}
			}
		}

		if ((in_room->sector_type == SECT_WATER_NOSWIM || to_room->sector_type == SECT_WATER_NOSWIM) &&
			MOUNTED(ch) && !IS_AFFECTED(MOUNTED(ch), AFF_FLYING)) {
			sprintf(buf,"You can't take your mount there.\n\r");
			send_to_char(buf, ch);
			return;
		}

		if (in_room->sector_type == SECT_WATER_NOSWIM && !IS_SET(ch->parts, PART_FINS) && !IS_IMMORTAL(ch)) {
			if(IS_SET(in_room->room_flag[1],ROOM_CITYMOVE))
				WAIT_STATE(ch, 4);
			else
				WAIT_STATE(ch, 8);
		}

		if ((in_room->sector_type != SECT_WATER_NOSWIM && to_room->sector_type == SECT_WATER_NOSWIM) &&
			!IS_AFFECTED(ch,AFF_FLYING)) {
			act("You dive into the deep water and begin to swim.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n dives into the deep water and begins to swim.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}

		if (in_room->sector_type == SECT_WATER_NOSWIM && to_room->sector_type != SECT_WATER_NOSWIM &&
			!IS_AFFECTED(ch,AFF_FLYING)) {
			act("You can touch the ground here.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n stops swimming and stands.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}

		{	/* City-move will reduce movement, if greater, to city movement */
			// FINS changed NOSWIM to SWIM
			int m1, m2;

			m1 = UMIN(SECT_MAX-1, in_room->sector_type);
			if(IS_SET(ch->parts, PART_FINS) && m1 == SECT_WATER_NOSWIM) m1 = SECT_WATER_SWIM;
			if(IS_SET(in_room->room_flag[1],ROOM_CITYMOVE) && movement_loss[m1] > movement_loss[SECT_CITY]) m1 = SECT_CITY;

			m2 = UMIN(SECT_MAX-1, to_room->sector_type);
			if(IS_SET(ch->parts, PART_FINS) && m2 == SECT_WATER_NOSWIM) m2 = SECT_WATER_SWIM;
			if(IS_SET(to_room->room_flag[1],ROOM_CITYMOVE) && movement_loss[m2] > movement_loss[SECT_CITY]) m2 = SECT_CITY;


			/* Average movement between different sector types */
			move = (movement_loss[m1] + movement_loss[m2]) / 2;
		}

		/* If crusader, 25% less movement in the wilds */
		if (ch->pcdata->second_sub_class_warrior == CLASS_WARRIOR_CRUSADER && IN_WILDERNESS(ch))
			move -= move / 4;

		if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_RANGER)
			move -= move / 4;

		if (!MOUNTED(ch)) {
			/* conditional effects */
			if (IS_AFFECTED(ch,AFF_FLYING) || IS_AFFECTED(ch,AFF_HASTE))
				move /= 2;
			if (IS_AFFECTED(ch,AFF_SLOW))
				move *= 2;

			if (ch->move < move) {
				send_to_char("You are too exhausted.\n\r", ch);
				return;
			}
		} else {
			if (IS_AFFECTED(MOUNTED(ch), AFF_FLYING) || IS_AFFECTED(MOUNTED(ch), AFF_HASTE))
				move /= 2;

			if (IS_AFFECTED(MOUNTED(ch), AFF_SLOW))
				move *= 2;

			if (MOUNTED(ch)->move < move) {
				send_to_char("Your mount is too exhausted.\n\r", ch);
				return;
			}
		}
	}

	if (!can_move_room(ch, door, to_room)) {
		/* If they are hunting this means they have run into some obstacle so stop */
		if (ch->hunting) ch->hunting = NULL;
		return;
	}

	if (!MOUNTED(ch)) {
		if (!IS_IMMORTAL(ch) && !is_float_user(ch)) ch->move -= move;
	} else {
		if (!IS_IMMORTAL(MOUNTED(ch)) && !is_float_user(MOUNTED(ch))) MOUNTED(ch)->move -= move;
	}

	/* echo messages */
	if (!IS_AFFECTED(ch, AFF_SNEAK) &&  ch->invis_level < 150) {
		if (in_room->sector_type == SECT_WATER_NOSWIM)
			act("{W$n swims $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
		else if (PULLING_CART(ch))
			act("{W$n leaves $T, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, dir_name[door], TO_ROOM);
		else if (MOUNTED(ch)) {
			if(!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
				strcpy(buf, "{W$n leaves $T, riding on $N.{x");
			else
				strcpy(buf, "{W$n soars $T, on $N.{x");
			act(buf, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
		} else {
			if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
				act("{W$n stumbles off drunkenly on $s way $T.{x", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM);
			else
				act("{W$n leaves $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
		}
	}

	check_room_shield_source(ch, true);

	if (!IS_DEAD(ch)) {
		check_room_flames(ch, true);
		if ((!IS_NPC(ch) && IS_DEAD(ch)) || (IS_NPC(ch) && ch->hit < 1))
			return;
	}

	/* moving your char negates your ambush */
	if (ch->ambush) {
		send_to_char("You stop your ambush.\n\r", ch);
		free_ambush(ch->ambush);
		ch->ambush = NULL;
	}

	/* Cancels your reciting too. This is incase move_char is called
	   from some other function and doesnt go through interpret(). */
	if (ch->recite > 0) {
		send_to_char("You stop reciting.\n\r", ch);
		act("$n stops reciting.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		ch->recite = 0;
	}

	ch->on = NULL;
	ch->on_compartment = NULL;

	char_from_room(ch);

	/* VIZZWILDS */
	if (to_room->wilds)
		char_to_vroom (ch, to_room->wilds, to_room->x, to_room->y);
	else
		char_to_room(ch, to_room);

	DUNGEON *to_dungeon = get_room_dungeon(to_room);

	INSTANCE *to_instance = get_room_instance(to_room);

	if (!IS_AFFECTED(ch, AFF_SNEAK) && ch->invis_level < LEVEL_HERO) {
		if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
		{
			WNUM wnum;
			wnum.pArea = in_dungeon->index->area;
			wnum.vnum = in_dungeon->index->vnum;

			OBJ_DATA *portal = get_room_dungeon_portal(to_room, wnum);

			if( IS_VALID(portal) )
			{
				if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
				{
					act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, dir_name[door], TO_ROOM);
				}
				else
				{
					act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM);
				}
			}
			else if(MOUNTED(ch))
			{
				if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
					act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else

					act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				if( !IS_NULLSTR(in_dungeon->index->zone_out) )
					act(in_dungeon->index->zone_out, ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
				else
					act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
			}
		}
		else if (in_room->sector_type == SECT_WATER_NOSWIM)
			act("{W$n swims in.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
		else if (PULLING_CART(ch))
			act("{W$n has arrived, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, NULL, TO_ROOM);
		else if(!MOUNTED(ch)) {
			if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
				act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			else
				act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
		} else {
			if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
				act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			else
			act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}

	move_cart(ch,to_room,TRUE);

	/* fry vampires*/
	if (IS_OUTSIDE(ch) && IS_VAMPIRE(ch) && number_percent() < 75)
		hurt_vampires(ch);

	do_function(ch, &do_look, "auto");

	check_see_hidden(ch);

	if (!IS_WILDERNESS(ch->in_room))
		check_traps(ch, true);

	if (IS_VALID(to_dungeon))
	{
		dungeon_check_commence(to_dungeon, ch);
	}

	if (in_room == to_room) /* no circular following */
		return;

	for (fch = in_room->people; fch != NULL; fch = fch_next) {
		fch_next = fch->next_in_room;

		if (fch->master == ch && IS_AFFECTED(fch,AFF_CHARM) && fch->position < POS_STANDING)
			do_function(fch, &do_stand, "");

		if (fch->master == ch && fch->position == POS_STANDING && can_see_room(fch,to_room)) {
			act("{WYou follow $N.{x", fch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			move_char(fch, door, TRUE);
		}
	}

	if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
	{
		p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL,0,0,0,0,0);
	}

	if( IS_VALID(to_instance) && to_instance != in_instance )
	{
		p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL,0,0,0,0,0);
	}

	/*
	* If someone is following the char, these triggers get activated
	* for the followers before the char, but it's safer this way...
	*/
	p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL,0,0,0,0,0);

	if (!IS_NPC(ch)) {
		p_greet_trigger(ch, PRG_MPROG,0,0,0,0,0);
		p_greet_trigger(ch, PRG_OPROG,0,0,0,0,0);
		p_greet_trigger(ch, PRG_RPROG,0,0,0,0,0);
	}

	if (!IS_DEAD(ch)) check_rocks(ch, true);
	if (!IS_DEAD(ch)) check_ice(ch, true);
	if (!IS_DEAD(ch)) check_room_flames(ch, true);
	if (!IS_DEAD(ch)) check_ambush(ch);

	/* Enable this?
	  if (IS_SET(ch->in_room->room_flag[1], ROOM_POST_OFFICE))
	      check_new_mail(ch);*/

	if (MOUNTED(ch) && number_percent() == 1 && get_skill(ch, gsk_riding) > 0)
		check_improve(ch, gsk_riding, TRUE, 8);

	if (!MOUNTED(ch) && get_skill(ch, gsk_trackless_step) > 0 && number_percent() == 1)
		check_improve(ch, gsk_trackless_step, TRUE, 8);

	/* Druids regenerate in nature */
	if (get_profession(ch, SUBCLASS_CLERIC) == CLASS_CLERIC_DRUID && is_in_nature(ch)) {
		ch->move += number_range(1,3);
		ch->move = UMIN(ch->move, ch->max_move);
		ch->hit += number_range(1,3);
		ch->hit  = UMIN(ch->hit, ch->max_hit);
	}

	if (!IS_NPC(ch))
		check_quest_rescue_mob(ch, true);

}

/* combat/hidden.c */
void check_ambush(CHAR_DATA *ch)
{
    CHAR_DATA *ach;
    AMBUSH_DATA *ambush;
    char command[MSL];
    char buf[MSL];

    if (ch->in_room == NULL)
    {
	sprintf(buf, "check_ambush: for %s (%ld), in_room was null!",
	    IS_NPC(ch) ? ch->short_descr : ch->name,
	    IS_NPC(ch) ? ch->pIndexData->vnum : 0);
	bug(buf, 0);
	return;
    }

    for (ach = ch->in_room->people; ach != NULL; ach = ach->next_in_room)
    {
	if (ach->ambush != NULL)
	{
	    ambush = ach->ambush;
	    if ((ambush->type == AMBUSH_PC && IS_NPC(ch))
		   || (ambush->type == AMBUSH_NPC && !IS_NPC(ch)))
		continue;

	    if (ch->tot_level < ambush->min_level
	    || ch->tot_level > ambush->max_level)
		continue;

            if (number_percent() > get_skill(ach, gsk_ambush))
	    {
		ach->position = POS_STANDING;
		act("You jump out of nowhere but $N notices you!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("You notice $n jump out of nowhere!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$n jumps out of nowhere trying to surprise $N, but fails!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		check_improve(ach, gsk_ambush, 4, FALSE);
		continue;
	    }

	    ach->position = POS_STANDING;

	    sprintf(command, "%s %s", ambush->command, ch->name);
	    act("{RYou jump out of nowhere, surprising $N!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	    act("{R$n jumps out of nowhere, surprising you!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	    act("{R$n jumps out of nowhere, surprising $N!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	    interpret(ach, command);

	    /* end the ambush */
	    free_ambush(ach->ambush);
	    ach->ambush = NULL;

	    check_improve(ach, gsk_ambush, 4, TRUE);
	}
    }
}


bool check_rocks(CHAR_DATA *ch, bool show)
{
	CHAR_DATA *vch, *vch_next;

	if (ch->in_room == NULL)
		return FALSE;

	if (IS_SET(ch->in_room->room_flag[0], ROOM_ROCKS) &&
		number_percent() < 75 &&
		!IS_DEAD(ch) &&
		!IS_AFFECTED(ch, AFF_FLYING))
	{
		if( show )
		{
			act("{yLoose rocks fall from the ceiling!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
		}

		for (vch = ch->in_room->people; vch != NULL; vch = vch_next)
		{
			vch_next = vch->next_in_room;

			if (number_percent() < 50)
			{
				if( show )
				{
					act("{RYou are struck by a rock!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{R$n is struck by a rock!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				vch->set_death_type = DEATHTYPE_ROCKS;
				damage(vch, vch, number_range(vch->tot_level * 4, vch->tot_level * 10), NULL, TYPE_UNDEFINED, DAM_BASH, FALSE);
			}
		}
	}

	if ((!IS_NPC(ch) && IS_DEAD(ch)) ||
		( IS_NPC(ch) && ch->in_room == NULL))
		return TRUE;

	return FALSE;
}


bool check_ice(CHAR_DATA *ch, bool show)
{
    if (ch->in_room == NULL)
	return FALSE;

    if (IS_IMMORTAL(ch))
	return FALSE;

    if (!IS_SET(ch->in_room->room_flag[1], ROOM_ICY)
    && check_ice_storm(ch->in_room) == FALSE)
	return FALSE;

    if (number_percent() > get_curr_stat(ch, STAT_DEX) * 2 + ch->tot_level / 20 && !IS_AFFECTED(ch, AFF_FLYING))
    {
		if( show )
		{
			act("{xYou slip on the icy ground and fall down!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{x$n slips on the icy ground and falls down!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		damage(ch, ch, UMIN(ch->max_hit/2, 50), NULL, TYPE_UNDEFINED, DAM_BASH, FALSE);
		ch->position = POS_RESTING;
		return TRUE;
    }

    return FALSE;
}


bool check_room_flames(CHAR_DATA *ch, bool show)
{
	OBJ_DATA *obj;

	if (ch->in_room == NULL)
		return FALSE;

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
	{
		if (obj->item_type == ITEM_ROOM_FLAME)
		{
			if (!IS_DEAD(ch) &&
				(IS_SET(ch->in_room->room_flag[0], ROOM_PK) ||
					IS_SET(ch->in_room->room_flag[0], ROOM_CHAOTIC) ||
					IS_SET(ch->in_room->room_flag[0], ROOM_ARENA) ||
					is_pk(ch) || IS_NPC(ch)))
			{
				if( show )
				{
					act("{RYou are scorched by flames!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{R$n is scorched by flames!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				damage(ch, ch, number_range(50, 500), NULL, TYPE_UNDEFINED, DAM_FIRE, FALSE);

				if (!IS_DEAD(ch) && number_percent() < 10)
				{
					AFFECT_DATA af;
					memset(&af,0,sizeof(af));

					if( show )
					{
						act("{DYou are blinded by smoke!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						act("{D$n is blinded by smoke!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
					af.where     = TO_AFFECTS;
					af.group     = AFFGROUP_PHYSICAL;
					af.skill      = gsk_blindness;
					af.level     = 3; /* obj->level; */
					af.location  = APPLY_HITROLL;
					af.modifier  = -4;
					af.duration  = 2;
					af.bitvector = AFF_BLIND;
					af.bitvector2 = 0;
					af.slot	= WEAR_NONE;
				}
			}
			else if( show )
			{
				act("{RYou pass through the scorching inferno unscathed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{R$n passes through the scorching inferno unscathed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
		}
	}

	if ((!IS_NPC(ch) && IS_DEAD(ch)) || ch->in_room == NULL)
		return TRUE;

	return FALSE;
}


void check_room_shield_source(CHAR_DATA *ch, bool show)
{
    OBJ_DATA *obj;

	if( show )
	{
		for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
			if (obj->item_type == ITEM_ROOM_ROOMSHIELD)
			{
				act("{YYou pass through the shimmering shield.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{Y$n passes through the shimmering shield.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
	}
}

bool can_move_room(CHAR_DATA *ch, int door, ROOM_INDEX_DATA *room)
{
	OBJ_DATA *obj;
	EXIT_DATA *pexit;
	ROOM_INDEX_DATA *in_room;
	char buf[MAX_STRING_LENGTH];
	char exit[MSL];

	in_room = ch->in_room;

	if (!room || !in_room) {
		if (ch->pIndexData)
			bug("Room was null in can_move_room, ch vnum is ", ch->pIndexData->vnum);
		else {
			sprintf(buf, "Room was null in can_move_room, char was %s", ch->name);
			bug(buf, 0);
		}
		return FALSE;
	}

	exit_name(in_room, door, exit);

	pexit = in_room->exit[door];
	if (IS_SET(pexit->exit_info, EX_CLOSED)	&& !(IS_IMMORTAL(ch) && !IS_NPC(ch)) &&
		(!IS_AFFECTED(ch, AFF_PASS_DOOR) || IS_SET(pexit->exit_info,EX_NOPASS))) {
		if (IS_AFFECTED(ch, AFF_PASS_DOOR))
			act("Something blocks you from passing through $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		else {
			act("$T is closed.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		}

		return FALSE;
	}

	/* If the exit is flagged aerial, then without flight, you will need some kind of climbing gear to bypass this. */
	if (IS_SET(pexit->exit_info, EX_AERIAL) && !mobile_is_flying(ch)) {
		send_to_char("Alas, you cannot move that way.\n\r", ch);
		return FALSE;
	}

	if ((is_room_pk(room, TRUE) || is_pk(ch)) &&
		!(IS_IMMORTAL(ch) && !IS_NPC(ch))) {
		for (obj = room->contents; obj != NULL; obj = obj->next_content) {
			if (obj->item_type == ITEM_ROOM_ROOMSHIELD) {
				act("{YYou try to move $T, but run into an invisible wall!{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR);
				act("{Y$n tries to move $T, but runs into an invisible wall!{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
				sprintf(buf, "{YYou hear a {R***BONK***{Y as someone runs into the shield from the %s!{x\n\r", dir_name[rev_dir[door]]);
				room_echo(room, buf);
				return FALSE;
			}
		}
	}

	/* Syn - why the hell wasn't this ever here in the first place? */
	// Changed to so that mobs that aren't following anyone will obey no_mob.
	// Those that are following others will be allowed through.
	if (IS_NPC(ch) && (!IS_VALID(ch->master) || ch->master == ch) && IS_SET(room->room_flag[0], ROOM_NO_MOB))
		return FALSE;

	if (IS_AFFECTED(ch, AFF_WEB)) {
		send_to_char("{RDespite your attempts to move, the webs hold you in place.{x\n\r", ch);
		return FALSE;
	}

	if (IS_AFFECTED2(ch, AFF2_ENSNARE)) {
		send_to_char("The vines clutching your body prevent you from moving!{x\n\r", ch);
		act("$n attempts to move, but the vines hold $m in place!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	if (IS_SET(pexit->exit_info, EX_CLOSED) && IS_SET(pexit->exit_info, EX_BARRED)) {
		act("$T is barred shut.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		return FALSE;
	}

	if (IS_AFFECTED(ch, AFF_CHARM) && ch->master && in_room == ch->master->in_room) {
		send_to_char("You don't have the freedom to do that.\n\r", ch);
		return FALSE;
	}

	if (!is_room_owner(ch,room) && room_is_private(room, ch)) {
		send_to_char("That room is private right now.\n\r", ch);
		return FALSE;
	}

	if(!is_room_unlocked(ch, room) )
	{
		if(p_percent2_trigger(room->area, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREENTER, dir_name[door],0,0,0,0,0))
			send_to_char("You cannot enter that place yet.\n\r", ch);

		return FALSE;
	}


	if(p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREENTER, dir_name[door],0,0,0,0,0))
		return FALSE;

	if (MOUNTED(ch) && (MOUNTED(ch)->position < POS_FIGHTING)) {
		send_to_char("Your mount must be standing.\n\r", ch);
		return FALSE;
	}

	if ( ch->pulled_cart )
	{
		CHAR_DATA *mount = MOUNTED(ch);

		if (mount)
		{
			if (ch->pulled_cart->value[2] > get_curr_stat(mount, STAT_STR)) {
				act("$N isn't strong enough to pull $p.", ch, mount, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
				act("$N struggles to pull $p but is too weak.", ch, mount, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_NOTVICT);
				return FALSE;
			}
		}
		else
		{
			if (ch->pulled_cart->value[2] > get_curr_stat(ch, STAT_STR)) {
				act("You aren't strong enough to pull $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
				act("$n attempts to pull $p but is too weak.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM);
				return FALSE;
			}
		}
	}


	/* Cant move into rooms with passwords */
	if (room->chat_room && room->chat_room->password && str_cmp(room->chat_room->password, "none") &&
		!(IS_IMMORTAL(ch) && !IS_NPC(ch))) {
		sprintf(buf, "#%s is password protected.\n\rUse /join <chatroom> <password>.\n\r", room->chat_room->name);
		send_to_char(buf, ch);
		return FALSE;
	}

	if (!str_cmp(ch->in_room->area->name, "Arena") && str_cmp(room->area->name, "Arena") && !IS_NPC(ch) && location_isset(&ch->pcdata->room_before_arena)) {
		ROOM_INDEX_DATA *to_room;

		if ((to_room = location_to_room(&ch->pcdata->room_before_arena))) {
			act("{YA swirl of colours surrounds you as you travel back to $t.{x", ch, NULL, NULL, NULL, NULL, to_room->name, NULL, TO_CHAR);
			act("{YA swirl of colours surrounds $n as $e vanishes.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			char_from_room(ch);
			char_to_room(ch, to_room);
			location_clear(&ch->pcdata->room_before_arena);
			do_function(ch, &do_look, "auto");
		} else {
			location_clear(&ch->pcdata->room_before_arena);
			return TRUE;
		}

		return FALSE;
	}

	if (check_ice(ch, true))
		return FALSE;

	if (ch->mail) {
		send_to_char("You must finish your mail and either send it or cancel it before you leave the post office.\n\r", ch);
		return FALSE;
	}

	return TRUE;
}

void drunk_walk(CHAR_DATA *ch, int door)
{
	EXIT_DATA *pexit;
	ROOM_INDEX_DATA *in_room;
	ROOM_INDEX_DATA *to_room;

	in_room = ch->in_room;

	if (!(pexit = in_room->exit[door]) || !(to_room = pexit->u1.to_room ) || !can_see_room(ch,pexit->u1.to_room)) {
		if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10) {
			if (!IS_NPC(ch) && number_percent() < (10 + ch->pcdata->condition[COND_DRUNK])) {
				if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10) {
					act("You drunkenly slam face-first into the 'exit' on your way $T.", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR);
					act("$n drunkenly slams face-first into the 'exit' on $s way $T.", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
					/* damage(ch, ch, 3, 0, DAM_BASH , FALSE); */
				} else {
					act("You drunkenly face-first into the 'exit' on your way $T. WHAM!", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR);
					act("$n slams face-first into the 'exit' on $s way $T. WHAM!", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
					/* damage(ch, ch, 3, 0, DAM_BASH, FALSE); */
				}
			} else {
				if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10) {
					act("You stumble about aimlessly and fall down drunk.", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_CHAR);
					act("$n stumbles about aimlessly and falls down drunk.", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM);
					ch->position = POS_RESTING;
				} else {
					act("You almost go $T, but suddenly realize that there's no exit there.",ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_CHAR);
					act("$n looks like $e's about to go $T, but suddenly stops short and looks confused.",ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM);
				}
			}
		} else
			send_to_char("Alas, you cannot go that way.\n\r", ch);
	}
}


void do_search(CHAR_DATA *ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH];
    char exit[MSL];
    EXIT_DATA *pexit;
    EXIT_DATA *pexit_rev;
    bool found = FALSE;
    int door = 0;
    OBJ_DATA *obj;

    sprintf(buf, "%s searched in %s (%ld) (argument=%s)", ch->name,
	    ch->in_room->name, ch->in_room->vnum, (IS_NULLSTR(argument)?"(nothing)":argument));
    log_string(buf);

    if( IS_NULLSTR(argument) )
    {
		act("You start searching the room for hidden exits and items...", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
		{
			if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90))
			{
				REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
				act("$n has uncovered $p!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				act("You have uncovered $p!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				found = TRUE;
			}
		}

		for (door = 0; door <= 9; door++)
		{
			if ((pexit = ch->in_room->exit[door]) != NULL && pexit->u1.to_room != NULL &&
				(pexit_rev = pexit->u1.to_room->exit[rev_dir[door]]) != NULL &&
				IS_SET(pexit->exit_info, EX_HIDDEN) && !IS_SET(pexit->exit_info, (EX_FOUND|EX_NOSEARCH)))
			{
				exit_name(ch->in_room, door, exit);
				if (door == DIR_UP)
				{
					if (exit[0] == '\0')
						sprintf(buf, "You have discovered an entrance above you.\n\r");
					else
						sprintf(buf, "You have discovered %s above you.\n\r", exit);
				} else if (door == DIR_DOWN) {
					if (exit[0] == '\0')
						sprintf(buf, "You have discovered an entrance beneath you.\n\r");
					else
						sprintf(buf, "You have discovered %s beneath you.\n\r", exit);
				} else {
					if (exit[0] == '\0')
						sprintf(buf, "You have discovered an entrance to your %s.\n\r", dir_name[door]);
					else {
						sprintf(buf, "You have discovered %s to your %s.\n\r",
						exit, dir_name[door]);
					}
				}

				send_to_char(buf, ch);
				found = TRUE;
				SET_BIT(pexit->exit_info, EX_FOUND);
				SET_BIT(pexit_rev->exit_info, EX_FOUND);
			}
		}
	} else if( !str_cmp(argument, "self") ) {

		act("You start searching your inventory...", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
		{
			if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90))
			{
				REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
				act("You have uncovered $p{x!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				found = TRUE;
			}
		}

	} else {
		OBJ_DATA *container;

		if( (container = get_obj_here(ch, NULL, argument)) == NULL )
		{
			act("You see no $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR);
			return;
		}

		act("You start searching $p{x...", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);

		for (obj = container->contains; obj != NULL; obj = obj->next_content)
		{
			if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90))
			{
				REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
				act("You have uncovered $p{x inside $P{x!", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);
				found = TRUE;
			}
		}
	}

    if (!found)
		act("You find nothing unusual.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
}

void do_north(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTH, FALSE);
    return;
}

void do_east(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_EAST, FALSE);
    return;
}

void do_south(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTH, FALSE);
    return;
}

void do_west(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_WEST, FALSE);
    return;
}

void do_northeast(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTHEAST, FALSE);
    return;
}

void do_northwest(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTHWEST, FALSE);
    return;
}

void do_southeast(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTHEAST, FALSE);
    return;
}

void do_southwest(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTHWEST, FALSE);
    return;
}

void do_up(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_UP, FALSE);
    return;
}


void do_down(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_DOWN, FALSE);
    return;
}


int find_door(CHAR_DATA *ch, char *arg, bool show)
{
    EXIT_DATA *pexit;
    int door;

	 if (!str_cmp(arg, "n") || !str_cmp(arg, "north")) door = 0;
    else if (!str_cmp(arg, "e") || !str_cmp(arg, "east" )) door = 1;
    else if (!str_cmp(arg, "s") || !str_cmp(arg, "south")) door = 2;
    else if (!str_cmp(arg, "w") || !str_cmp(arg, "west" )) door = 3;
    else if (!str_cmp(arg, "u") || !str_cmp(arg, "up"   )) door = 4;
    else if (!str_cmp(arg, "d") || !str_cmp(arg, "down" )) door = 5;
    else if (!str_cmp(arg, "ne") || !str_cmp(arg, "northeast" )) door = 6;
    else if (!str_cmp(arg, "nw") || !str_cmp(arg, "northwest" )) door = 7;
    else if (!str_cmp(arg, "se") || !str_cmp(arg, "southeast" )) door = 8;
    else if (!str_cmp(arg, "sw") || !str_cmp(arg, "southwest" )) door = 9;
    else if (!ch)
    	return -1;
    else {
		for (door = 0; door < MAX_DIR; door++)
		{
			if ((pexit = ch->in_room->exit[door]) != NULL &&
				IS_SET(pexit->exit_info, EX_ISDOOR) &&
				pexit->keyword != NULL &&
				strlen(arg) > 2 &&
				str_infix(arg, "the and") &&
				is_name(arg, pexit->keyword))
				break;
		}

		if( door >= MAX_DIR )
		{
			if (show)
			    act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);

			return -1;
		}
    }

    if(ch) {
		if ((pexit = ch->in_room->exit[door]) == NULL) {
			if (show)
				act("I see no door $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);
			return -1;
		}

		if(IS_SET(pexit->exit_info, EX_HIDDEN) && !IS_SET(pexit->exit_info, EX_FOUND) && IS_SET(pexit->exit_info, EX_MUSTSEE) )
		{
			if(!IS_IMMORTAL(ch) || !IS_SET(ch->act[0],PLR_HOLYLIGHT))
			{
				if (show)
					act("I see no door $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR);
				return -1;
			}
		}

		if (!IS_SET(pexit->exit_info, EX_ISDOOR)) {
			if (show)
			    send_to_char("You can't do that.\n\r", ch);
			return -1;
		}
    }

    return door;
}

void do_open(CHAR_DATA *ch, char *argument)
{
	char *start = argument;
	char arg[MAX_INPUT_LENGTH];
	char exit[MSL];
	OBJ_DATA *obj, *key = NULL;
	int door;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Open what?\n\r", ch);
		return;
	}

	/* Open object */
	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		// Ok, need determine if we can target things unambiguously
		if (obj_oclu_ambiguous(obj) && argument[0] == '\0')
		{
			act("Open what on $p?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			obj_oclu_show_parts(ch, obj);
			return;
		}

		OCLU_CONTEXT context;
		if (!oclu_get_context(&context, obj, argument))
		{
			act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		// portal stuff, has different flags, as they are treated more like exits
		if (context.item_type == ITEM_PORTAL)
		{
			if (!IS_SET(PORTAL(obj)->exit,EX_CLOSED))
			{
				send_to_char("It's already open.\n\r",ch);
				return;
			}

			if( PORTAL(obj)->lock )
			{
				if (IS_SET(PORTAL(obj)->lock->flags, LOCK_LOCKED))
				{
					if ((key = lockstate_getkey(ch,PORTAL(obj)->lock)) != NULL)
					{
						do_function(ch, &do_unlock, start);
						do_function(ch, &do_open, start);
						return;
					}

					send_to_char("It's locked.\n\r", ch);
					return;
				}
			}

			REMOVE_BIT(PORTAL(obj)->exit,EX_CLOSED);
			if (context.is_default)
			{
				act("You open $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				act("$n opens $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_ROOM);
			}
			else
			{
				act("You open $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n opens $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_ROOM);
			}
			p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, ch, NULL, NULL, TRIG_OPEN, NULL,context.which,context.is_default,0,0,0);
			return;
		}

		if (!IS_SET(*context.flags, CONT_CLOSED))
		{
			send_to_char("It's already open.\n\r", ch);
			return;
		}
		if (!IS_SET(*context.flags, CONT_CLOSEABLE))
		{
			send_to_char("You can't do that.\n\r", ch);
			return;
		}

		if (IS_SET(*context.flags, CONT_PUSHOPEN))
		{
			send_to_char("You need to push it open.\n\r", ch);
			return;
		}

		if( *context.lock )
		{
			if (IS_SET((*context.lock)->flags, LOCK_LOCKED))
			{
				if ((key = lockstate_getkey(ch, *context.lock)) != NULL)
				{
					do_function(ch, &do_unlock, start);
					do_function(ch, &do_open, start);
					return;
				}

				send_to_char("It's locked.\n\r", ch);
				return;
			}
		}

		REMOVE_BIT((*context.flags), CONT_CLOSED);
		if (context.item_type == ITEM_BOOK)
		{
			// Special handling for books.
			if (BOOK(obj)->open_page > 0)
			{
				BOOK(obj)->current_page = BOOK(obj)->open_page;
			}
			else if (BOOK(obj)->current_page < 1)
				BOOK(obj)->current_page = 1;

			char page[MIL];
			sprintf(page, "to page %d", BOOK(obj)->current_page);

			if (context.is_default)
			{
				act("You open $p $T.",ch, NULL, NULL,obj, NULL, NULL, page,TO_CHAR);
				act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				act("You open $t on $p $T.",ch, NULL, NULL,obj, NULL, context.label, page,TO_CHAR);
				act("$n opens $t on $p.", ch, NULL, NULL, obj, NULL, context.label, NULL, TO_ROOM);
			}
		}
		else
		{
			if (context.is_default)
			{
				act("You open $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				act("You open $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n opens $t on $p.", ch, NULL, NULL, obj, NULL, context.label, NULL, TO_ROOM);
			}
		}

		p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, ch, NULL, NULL, TRIG_OPEN, NULL,context.which,context.is_default,0,0,0);
		return;
	}

	/* Open door */
	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		exit_name(ch->in_room, door, exit);

		pexit = ch->in_room->exit[door];
		if (!IS_SET(pexit->exit_info, EX_CLOSED))
		{
			send_to_char("It's already open.\n\r", ch);
			return;
		}
		if ( IS_SET(pexit->exit_info, EX_BARRED))
		{
			send_to_char("It has been barred.\n\r", ch);
			return;
		}
		if ( IS_SET(pexit->door.lock.flags, LOCK_LOCKED))
		{
			if ((key = lockstate_getkey(ch, &pexit->door.lock)) != NULL)
			{
				do_function(ch, &do_unlock, dir_name[door]);
				do_function(ch, &do_open, dir_name[door]);
				return;
			}

			send_to_char("It's locked.\n\r", ch);
			return;
		}

		REMOVE_BIT(pexit->exit_info, EX_CLOSED);
		act("$n opens $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);
		act("You open $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_OPEN,0,0,0,0,0);

		if ((to_room   = pexit->u1.to_room) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
			pexit_rev->u1.to_room == ch->in_room)
		{
			CHAR_DATA *rch;

			exit_name(to_room, rev_dir[door], exit);

			REMOVE_BIT(pexit_rev->exit_info, EX_CLOSED);
			for (rch = to_room->people; rch != NULL; rch = rch->next_in_room)
				act("$T opens.", rch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		}
	}
}


void do_close(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char exit[MSL];
	OBJ_DATA *obj;
	int door;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Close what?\n\r", ch);
		return;
	}

	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		// Ok, need determine if we can target things unambiguously
		if (obj_oclu_ambiguous(obj) && argument[0] == '\0')
		{
			act("Close what on $p?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			obj_oclu_show_parts(ch, obj);
			return;
		}

		OCLU_CONTEXT context;
		if (!oclu_get_context(&context, obj, argument))
		{
			act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		/* portal stuff */
		if (context.item_type == ITEM_PORTAL)
		{
			if (!IS_SET(PORTAL(obj)->exit,EX_ISDOOR) ||
				IS_SET(PORTAL(obj)->exit,EX_NOCLOSE))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->exit,EX_CLOSED))
			{
				send_to_char("It's already closed.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->exit,EX_BROKEN))
			{
				send_to_char("That door has been destroyed. It cannot be closed.\n\r",ch);
				return;
			}

			SET_BIT(PORTAL(obj)->exit,EX_CLOSED);
			if (context.is_default)
			{
				act("You close $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				act("$n closes $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_ROOM);
			}
			else
			{
				act("You close $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n closes $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_ROOM);
			}
			p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CLOSE, NULL,context.which,context.is_default,0,0,0);
			return;
		}

		if (IS_SET(*context.flags, CONT_CLOSED))
		{
			send_to_char("It's already closed.\n\r", ch);
			return;
		}
		if (!IS_SET(*context.flags, CONT_CLOSEABLE))
		{
			send_to_char("You can't do that.\n\r", ch);
			return;
		}

		if (context.is_default)
		{
			act("You close $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
			act("$n closes $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act("You close $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
			act("$n closes $t on $p.", ch, NULL, NULL, obj, NULL, context.label, NULL, TO_ROOM);
		}

		SET_BIT(*context.flags, CONT_CLOSED);

		if (*context.lock && IS_SET(*context.flags, CONT_CLOSELOCK))
		{
			act("$p locks once closed.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			SET_BIT((*context.lock)->flags, LOCK_LOCKED);
		}
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CLOSE, NULL,context.which,context.is_default,0,0,0);
		return;
	}

	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		/* 'close door' */
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		exit_name(ch->in_room, door, exit);

		pexit	= ch->in_room->exit[door];
		if (IS_SET(pexit->exit_info, EX_CLOSED))
		{
			send_to_char("It's already closed.\n\r", ch);
			return;
		}

		if (IS_SET(pexit->exit_info, EX_BROKEN)) {
			send_to_char("That door has been destroyed. It cannot be closed.\n\r", ch);
			return;
		}

		SET_BIT(pexit->exit_info, EX_CLOSED);
		act("$n closes $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);
		act("You close $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_CLOSE,0,0,0,0,0);

		/* close the other side */
		if ((to_room   = pexit->u1.to_room) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != 0 &&
			pexit_rev->u1.to_room == ch->in_room)
		{
			CHAR_DATA *rch;

			exit_name(to_room, rev_dir[door], exit);

			SET_BIT(pexit_rev->exit_info, EX_CLOSED);
			for (rch = to_room->people; rch != NULL; rch = rch->next_in_room)
				act("The $T closes.", rch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		}
	}

	return;
}


void use_key(CHAR_DATA *ch, OBJ_DATA *key, LOCK_STATE *lock)
{
	CHURCH_DATA *church;
	char buf[MSL];

	if (ch == NULL)
	{
		bug("use_key: ch was null", 0);
		return;
	}

	if (key == NULL)
	{
		bug("use_key: key was null", 0);
		return;
	}

	if (lock == NULL)
	{
		bug("use_key: lock was null", 0);
		return;
	}

	/* can only use a church-temple key if you're in that church */
	for (church = church_list; church != NULL; church = church->next)
	{
		if (church->hall_area == key->pIndexData->area && church->key == key->pIndexData->vnum && ch->church != church)
		{
			sprintf(buf, "Rent by the spiritual powers of %s, $p dissipates into nothingness.\n\r", church->name);
			act(buf, ch, NULL, NULL, key, NULL, NULL, NULL, TO_CHAR);
			act(buf, ch, NULL, NULL, key, NULL, NULL, NULL, TO_ROOM);
			extract_obj(key);
			return;
		}
	}

	if (key->fragility != OBJ_FRAGILE_SOLID)
	{
		switch (key->fragility)
		{
		case OBJ_FRAGILE_STRONG:
			if (number_percent() < 25)
				key->condition--;
			break;
		case OBJ_FRAGILE_NORMAL:
			if (number_percent() < 50)
				key->condition--;
			break;
		case OBJ_FRAGILE_WEAK:
			key->condition--;
			break;
		default:
			break;
		}

		if (IS_SET(lock->flags, LOCK_SNAPKEY) || key->condition <= 0)
		{
			act("$p snaps and breaks.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_ALL);
			extract_obj(key);
		}
	}
}

void do_lock(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	OBJ_DATA *obj;
	OBJ_DATA *key;
	int door;
	char exit[MSL];
	int ret;
	bool uk;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Lock what?\n\r", ch);
		return;
	}

	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		// Ok, need determine if we can target things unambiguously
		if (obj_oclu_ambiguous(obj) && argument[0] == '\0')
		{
			act("Lock what on $p?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			obj_oclu_show_parts(ch, obj);
			return;
		}

		OCLU_CONTEXT context;
		if (!oclu_get_context(&context, obj, argument))
		{
			act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		/* portal stuff */
		if (context.item_type == ITEM_PORTAL)
		{
			if (!IS_SET(PORTAL(obj)->exit,EX_ISDOOR) ||
				IS_SET(PORTAL(obj)->exit,EX_NOCLOSE))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (!IS_SET(PORTAL(obj)->exit,EX_CLOSED))
			{
				send_to_char("It's not closed.\n\r",ch);
				return;
			}

			if (!lockstate_functional(PORTAL(obj)->lock))
			{
				send_to_char("It can't be locked.\n\r",ch);
				return;
			}

			if ((key = lockstate_getkey(ch,PORTAL(obj)->lock)) == NULL)
			{
				send_to_char("You lack the key.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->lock->flags,LOCK_BROKEN))
			{
				send_to_char("The lock is broken.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->lock->flags,LOCK_JAMMED))
			{
				send_to_char("The lock has been jammed.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->lock->flags,LOCK_LOCKED))
			{
				send_to_char("It's already locked.\n\r",ch);
				return;
			}

			// If $(obj1) is defined, $(obj) is the KEY
			if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRELOCK, NULL,0,0,0,0,0)))
			{
				if (ret != PRET_SILENT)
				{
					act("You can't lock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				}
				return;
			}

			// If $(obj2) is defined, $(obj) is the PORTAL
			if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_PRELOCK, NULL,context.which,0,0,0,0)))
			{
				if (ret != PRET_SILENT)
				{
					act("You can't lock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				}
				return;
			}

			if (context.is_default)
			{
				act("You lock $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				act("$n locks $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_ROOM);
			}
			else
			{
				act("You lock $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n locks $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_ROOM);
			}

			SET_BIT(PORTAL(obj)->lock->flags,LOCK_LOCKED);

			uk = TRUE;
			if (p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_LOCK, NULL,context.which,context.is_default,0,0,0))
				uk = FALSE;
			if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_LOCK, NULL,context.which,context.is_default,0,0,0))
				uk = FALSE;
			if (uk)
				use_key(ch, key, PORTAL(obj)->lock);
			return;
		}

		if (!IS_SET(*context.flags, CONT_CLOSED))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}

		if (!lockstate_functional(*context.lock))
		{
			send_to_char("It can't be locked.\n\r",ch);
			return;
		}

		if ((key = lockstate_getkey(ch,*context.lock)) == NULL)
		{
			send_to_char("You lack the key.\n\r",ch);
			return;
		}

		if (IS_SET((*context.lock)->flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}

		if (IS_SET((*context.lock)->flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}

		if (IS_SET((*context.lock)->flags, LOCK_LOCKED))
		{
			send_to_char("It's already locked.\n\r", ch);
			return;
		}

		// If $(obj1) is defined, $(obj) is the KEY
		if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRELOCK, NULL,context.which,context.is_default,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				if (context.is_default)
					act("You can't lock $p with that.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				else
					act("You can't lock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
			}
			return;
		}

		// If $(obj2) is defined, $(obj) is the OBJECT
		if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_PRELOCK, NULL,context.which,context.is_default,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				if (context.is_default)
					act("You can't lock $p with that.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				else
					act("You can't lock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
			}
			return;
		}

		if (context.is_default)
		{
			act("You lock $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
			act("$n locks $p.",ch, NULL, NULL,obj, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act("You lock $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
			act("$n locks $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL, TO_ROOM);
		}

		SET_BIT((*context.lock)->flags, LOCK_LOCKED);

		uk = TRUE;
		if (p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_LOCK, NULL,context.which,context.is_default,0,0,0))
			uk = FALSE;
		if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_LOCK, NULL,context.which,context.is_default,0,0,0))
			uk = FALSE;
		if (uk)
			use_key(ch, key, *context.lock);
		return;
	}

	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		/* 'lock door' */
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		pexit	= ch->in_room->exit[door];
		if (!IS_SET(pexit->exit_info, EX_CLOSED))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}
		if (!lockstate_functional(&pexit->door.lock))
		{
			send_to_char("It can't be locked.\n\r",ch);
			return;
		}
		if ((key = lockstate_getkey(ch,&pexit->door.lock)) == NULL)
		{
			send_to_char("You lack the key.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags, LOCK_LOCKED))
		{
			send_to_char("It's already locked.\n\r", ch);
			return;
		}

		// If neither $(obj1) nor $(obj2) are defined, it is a KEY targeting a room
		key->tempstore[0] = door;
		if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRELOCK, NULL,0,0,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				act("You can't lock $t with that.",ch, NULL, NULL, NULL, NULL, dir_name[door],NULL,TO_CHAR);
			}
			return;
		}

		// TODO: Add a way to do PRELOCK here on the room
		
		SET_BIT(pexit->door.lock.flags, LOCK_LOCKED);
		/* send_to_char("*Click*\n\r", ch); */
		exit_name(ch->in_room, door, exit);
		act("You lock $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		act("$n locks $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);

		/* lock the other side */
		if ((to_room   = pexit->u1.to_room) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != 0 &&
			pexit_rev->u1.to_room == ch->in_room)
			SET_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);

		key->tempstore[0] = door;
		uk = TRUE;
		if(p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LOCK, NULL,0,0,0,0,0))
			uk = FALSE;
		if(p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_LOCK,0,0,0,0,0))
			uk = FALSE;
		if (uk)
			use_key(ch, key, &pexit->door.lock);
	}
}


void do_unlock(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char exit[MSL];
	OBJ_DATA *obj;
	OBJ_DATA *key;
	int door;
	int ret;
	bool uk;

	one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Unlock what?\n\r", ch);
		return;
	}

	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		// Ok, need determine if we can target things unambiguously
		if (obj_oclu_ambiguous(obj) && argument[0] == '\0')
		{
			act("Unlock what on $p?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			obj_oclu_show_parts(ch, obj);
			return;
		}

		OCLU_CONTEXT context;
		if (!oclu_get_context(&context, obj, argument))
		{
			act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if (context.item_type == ITEM_PORTAL)
		{
			if (!IS_SET(PORTAL(obj)->exit,EX_ISDOOR))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (!IS_SET(PORTAL(obj)->exit,EX_CLOSED))
			{
				send_to_char("It's not closed.\n\r",ch);
				return;
			}

			if (!lockstate_functional(PORTAL(obj)->lock))
			{
				send_to_char("It can't be locked.\n\r",ch);
				return;
			}

			if ((key = lockstate_getkey(ch,PORTAL(obj)->lock)) == NULL)
			{
				send_to_char("You lack the key.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->lock->flags,LOCK_BROKEN))
			{
				send_to_char("The lock is broken.\n\r",ch);
				return;
			}

			if (IS_SET(PORTAL(obj)->lock->flags,LOCK_JAMMED))
			{
				send_to_char("The lock has been jammed.\n\r",ch);
				return;
			}

			if (!IS_SET(PORTAL(obj)->lock->flags,LOCK_LOCKED))
			{
				send_to_char("It's already unlocked.\n\r",ch);
				return;
			}

			// If $(obj1) is defined, $(obj) is the KEY
			if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREUNLOCK, NULL,context.which,context.is_default,0,0,0)))
			{
				if (ret != PRET_SILENT)
				{
					if (context.is_default)
						act("You can't unlock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
					else
						act("You can't unlock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				}
				return;
			}

			// If $(obj2) is defined, $(obj) is the PORTAL
			if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_PREUNLOCK, NULL,context.which,context.is_default,0,0,0)))
			{
				if (ret != PRET_SILENT)
				{
					if (context.is_default)
						act("You can't unlock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
					else
						act("You can't unlock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				}
				return;
			}

			if (context.is_default)
			{
				act("You unlock $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				act("$n unlocks $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
			}
			else
			{
				act("You unlock $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n unlocks $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_ROOM);
			}

			REMOVE_BIT(PORTAL(obj)->lock->flags,LOCK_LOCKED);

			uk = TRUE;
			if (p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_UNLOCK, NULL,context.which,context.is_default,0,0,0))
				uk = FALSE;
			if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_UNLOCK, NULL,context.which,context.is_default,0,0,0))
				uk = FALSE;
			if (uk)
				use_key(ch, key, PORTAL(obj)->lock);
			return;
		}

		
		if (!IS_SET(*(context.flags), CONT_CLOSED))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}

		if (!lockstate_functional(*context.lock))
		{
			send_to_char("It can't be locked.\n\r",ch);
			return;
		}

		if ((key = lockstate_getkey(ch,*context.lock)) == NULL)
		{
			send_to_char("You lack the key.\n\r",ch);
			return;
		}
		if (IS_SET((*context.lock)->flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}
		if (IS_SET((*context.lock)->flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}
		if (!IS_SET((*context.lock)->flags,LOCK_LOCKED))
		{
			send_to_char("It's already unlocked.\n\r",ch);
			return;
		}

		// If $(obj1) is defined, $(obj) is the KEY
		if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREUNLOCK, NULL,context.which,0,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				if (context.is_default)
					act("You can't unlock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				else
					act("You can't unlock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label,NULL,TO_CHAR);
			}
			return;
		}

		// If $(obj2) is defined, $(obj) is the OBJECT
		if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_PREUNLOCK, NULL,context.which,0,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				if (context.is_default)
					act("You can't unlock $p with that.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				else
					act("You can't unlock $t on $p with that.",ch, NULL, NULL,obj, NULL, context.label,NULL,TO_CHAR);
			}
			return;
		}

		if (context.is_default)
		{
			act("You unlock $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
			act("$n unlocks $p.",ch, NULL, NULL,obj, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			act("You unlock $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
			act("$n unlocks $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL, TO_ROOM);
		}

		REMOVE_BIT((*context.lock)->flags,LOCK_LOCKED);

		uk = TRUE;
		if (p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_UNLOCK, NULL,context.which,context.is_default,0,0,0))
			uk = FALSE;
		if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, key, TRIG_UNLOCK, NULL,context.which,context.is_default,0,0,0))
			uk = FALSE;
		if (uk)
			use_key(ch, key, *context.lock);
		return;
	}

	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		/* 'unlock door' */
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		pexit = ch->in_room->exit[door];
		if (!IS_SET(pexit->exit_info, EX_CLOSED))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}
		if (!lockstate_functional(&pexit->door.lock))
		{
			send_to_char("It can't be locked.\n\r",ch);
			return;
		}
		if ((key = lockstate_getkey(ch,&pexit->door.lock)) == NULL)
		{
			send_to_char("You lack the key.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}
		if (!IS_SET(pexit->door.lock.flags, LOCK_LOCKED))
		{
			send_to_char("It's already unlocked.\n\r", ch);
			return;
		}

		// If neither $(obj1) nor $(obj2) are defined, it is a KEY targeting a room
		key->tempstore[0] = door;
		if ((ret = p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRELOCK, NULL,0,0,0,0,0)))
		{
			if (ret != PRET_SILENT)
			{
				act("You can't lock $t with that.",ch, NULL, NULL, NULL, NULL, dir_name[door],NULL,TO_CHAR);
			}
			return;
		}

		// TODO: Add a way to do PRELOCK here on the room
		
		REMOVE_BIT(pexit->door.lock.flags, LOCK_LOCKED);
		/* send_to_char("*Click*\n\r", ch); */
		exit_name(ch->in_room, door, exit);
		act("You unlock $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		act("$n unlocks $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);

		/* unlock the other side */
		if ((to_room   = pexit->u1.to_room) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
			pexit_rev->u1.to_room == ch->in_room)
			REMOVE_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);

		key->tempstore[0] = door;
		uk = TRUE;
		if(p_percent_trigger(NULL, key, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_UNLOCK, NULL,0,0,0,0,0))
			uk = FALSE;
		if(p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_UNLOCK,0,0,0,0,0))
			uk = FALSE;
		if(uk)
			use_key(ch, key, &pexit->door.lock);
	}
}


void do_pick(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	OBJ_DATA *obj;
	int door;

	if (IS_NPC(ch))
	return;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Pick what?\n\r", ch);
		return;
	}

	if (MOUNTED(ch))
	{
		send_to_char("You can't pick locks while mounted.\n\r", ch);
		return;
	}

	WAIT_STATE(ch, gsk_pick_lock->beats);

	if (get_profession(ch, SECOND_SUBCLASS_THIEF) != CLASS_THIEF_HIGHWAYMAN)
	{
		int skill = get_skill(ch, gsk_pick_lock);

		if (number_percent() > UMAX(skill, 20))
		{
			send_to_char("You failed.\n\r", ch);
			check_improve(ch,gsk_pick_lock,FALSE,2);
			return;
		}
	}

	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		/* portal stuff */
		if (obj->item_type == ITEM_PORTAL)
		{
			if (!IS_SET(obj->value[1],EX_ISDOOR))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (!IS_SET(obj->value[1],EX_CLOSED))
			{
				send_to_char("It's not closed.\n\r",ch);
				return;
			}

			if (!lockstate_functional(obj->lock))
			{
				send_to_char("It can't be unlocked.\n\r",ch);
				return;
			}

			if (IS_SET(obj->lock->flags,LOCK_BROKEN))
			{
				send_to_char("The lock is broken.\n\r",ch);
				return;
			}

			if (IS_SET(obj->lock->flags,LOCK_JAMMED))
			{
				send_to_char("The lock has been jammed.\n\r",ch);
				return;
			}

			if (!IS_SET(obj->lock->flags, LOCK_LOCKED) )
			{
				send_to_char("It's already unlocked.\n\r", ch);
				return;
			}

			if (number_percent() >= obj->lock->pick_chance)
			{
				send_to_char("You failed.\n\r",ch);
				return;
			}

			REMOVE_BIT(obj->lock->flags,LOCK_LOCKED);
			act("You pick the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			act("$n picks the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
			check_improve(ch,gsk_pick_lock,TRUE,2);
			return;
		}

		if (IS_CONTAINER(obj))
		{
			if (!IS_SET(CONTAINER(obj)->flags, CONT_CLOSED))
			{
				send_to_char("It's not closed.\n\r", ch);
				return;
			}
		}
		else if (obj->item_type == ITEM_BOOK)
		{
			if (!IS_SET(obj->value[1], CONT_CLOSED))
			{
				send_to_char("It's not closed.\n\r", ch);
				return;
			}
		}

		if (!lockstate_functional(obj->lock))
		{
			send_to_char("It can't be unlocked.\n\r",ch);
			return;
		}

		if (IS_SET(obj->lock->flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}

		if (IS_SET(obj->lock->flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}

		if (!IS_SET(obj->lock->flags, LOCK_LOCKED) )
		{
			send_to_char("It's already unlocked.\n\r", ch);
			return;
		}

		if (number_percent() >= obj->lock->pick_chance)
		{
			send_to_char("You failed.\n\r",ch);
			return;
		}

		REMOVE_BIT(obj->lock->flags,LOCK_LOCKED);
		act("You pick the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		act("$n picks the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
		check_improve(ch,gsk_pick_lock,TRUE,2);
		return;
	}

	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		/* 'pick door' */
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		pexit = ch->in_room->exit[door];
		if (!IS_SET(pexit->exit_info, EX_CLOSED) && !IS_IMMORTAL(ch))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}
		if (!lockstate_functional(&pexit->door.lock) && !IS_IMMORTAL(ch))
		{
			send_to_char("It can't be picked.\n\r", ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_BROKEN))
		{
			send_to_char("The lock is broken.\n\r",ch);
			return;
		}
		if (IS_SET(pexit->door.lock.flags,LOCK_JAMMED))
		{
			send_to_char("The lock has been jammed.\n\r",ch);
			return;
		}
		if (!IS_SET(pexit->door.lock.flags, LOCK_LOCKED))
		{
			send_to_char("It's already unlocked.\n\r", ch);
			return;
		}

		if ((number_percent() >= pexit->door.lock.pick_chance) && !IS_IMMORTAL(ch))
		{
			send_to_char("You failed.\n\r", ch);
			return;
		}

		REMOVE_BIT(pexit->door.lock.flags, LOCK_LOCKED);
		send_to_char("*Click*\n\r", ch);
		act("$n picks the $d.", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);
		check_improve(ch,gsk_pick_lock,TRUE,2);

		/* pick the other side */
		if ((to_room   = pexit->u1.to_room ) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
			pexit_rev->u1.to_room == ch->in_room)
		{
			REMOVE_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);
		}
	}
}

bool can_wake_up(CHAR_DATA *ch, CHAR_DATA *waker, bool silent)
{
	int ret;

	if (ch->position != POS_SLEEPING) return FALSE;

	if (waker != ch)
	{
		if(IS_IMMORTAL(waker))
			return TRUE;

		if (!IS_NPC(ch) && IS_SET(ch->act[1], PLR_NO_WAKE))
		{
			if (!silent)
				act("You can't wake $N up!", waker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return FALSE;
		}
	}

    if (IS_AFFECTED(ch, AFF_SLEEP))
	{
		if (!silent)
		{
			if (waker != ch)
				act("You can't wake $N up!", waker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				send_to_char("You can't wake up!\n\r", ch);
		}
		return FALSE;
	}

	ret = p_percent_trigger(ch, NULL, NULL, NULL, waker, NULL, NULL, ch->on, NULL, TRIG_PREWAKE, NULL,0,0,0,0,0);
	if (ret)
	{
		if (!silent)
		{
			if (waker != ch)
				act("You can't wake $N up!", waker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				send_to_char("You can't wake up!\n\r", ch);
		}
		return FALSE;
	}

	if (ch->on)
	{
		ret = p_percent_trigger(NULL, ch->on, NULL, NULL, waker, ch, NULL, NULL, NULL, TRIG_PREWAKE, NULL,0,0,0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				if (waker != ch)
					act("You can't wake $N up!", waker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				else
					send_to_char("You can't wake up!\n\r", ch);
			}

			return FALSE;
		}
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, waker, ch, NULL, ch->on, NULL, TRIG_PREWAKE, NULL,0,0,0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			if (waker != ch)
				act("You can't wake $N up!", waker, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				send_to_char("You can't wake up!\n\r", ch);
		}

		return FALSE;
	}

	return TRUE;
}

// This is *ONLY* called when checking standing on a furniture compartment
bool can_stand(CHAR_DATA *ch, OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment, bool silent)
{
	int ret;
	
	if (IS_VALID(obj) && (!IS_FURNITURE(obj) || (IS_VALID(compartment) && !compartment->standing)))
	{
		if (!silent)
			send_to_char("You can't seem to find a place to stand.\n\r",ch);
		return FALSE;
	}

	ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESTAND, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to stand!\n\r", ch);
		}

		return FALSE;
	}

	if (ch->on_compartment && ch->on_compartment != compartment)
	{
		if (compartment_is_closed(ch->on_compartment))
		{
			if (!silent)
				act_new("You need to open the compartment first before you can leave.", ch,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PRESTEPOFF, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				act("You are unable to step off from $p!", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR);
			}

			return FALSE;
		}
	}

	if (obj && compartment)
	{
		if (compartment->max_occupants >= 0 && furniture_count_users(obj, compartment) >= compartment->max_occupants)
		{
			if (!silent)
				act_new("There's no room to stand on $p.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		if (compartment_is_closed(compartment))
		{
			if (!silent)
				act_new("That compartment is closed.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_PRESTAND, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				char *positional = furniture_get_positional(compartment->standing);
				char *short_descr = furniture_get_short_description(obj, compartment);

				act("You are unable to stand $t $T!", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
			}

			return FALSE;
		}
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESTAND, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to stand here!\n\r", ch);
		}

		return FALSE;
	}

	return TRUE;
}

void do_stand(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;
	FURNITURE_COMPARTMENT *compartment = NULL;
	int old_ordinal = ch->on_compartment ? ch->on_compartment->ordinal : 0;
	int new_ordinal = 0;

	// Stand <furniture>
	if (argument[0] != '\0')
	{
		char arg[MIL];

		argument = one_argument(argument, arg);

		if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
		{
			send_to_char("You don't see that here.\n\r",ch);
			return;
		}

		if (!IS_FURNITURE(obj))
		{
			send_to_char("You cannot possibly stand on that.\n\r", ch);
			return;
		}

		compartment = furniture_find_compartment(ch, obj, argument, "stand");
		if (!IS_VALID(compartment)) return;

		if (!compartment->standing)
		{
			send_to_char("There is no where to stand on that.\n\r", ch);
			return;
		}

		new_ordinal = compartment->ordinal;
	}

	switch(ch->position)
	{
		// TODO: POS_HANGING

		case POS_SLEEPING:
			if (can_wake_up(ch, ch, FALSE))
			{
				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				if (ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);

				if (can_stand(ch, obj, compartment, FALSE))
				{
					ch->position = POS_STANDING;
					if(ch->on_compartment && ch->on_compartment != compartment)
					{
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
					}
					ch->on = obj;
					ch->on_compartment = compartment;

					if (obj == NULL)
					{
						act("You wake and stand.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						act("$n wakes and stands.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
					else
					{
						char *positional = furniture_get_positional(compartment->standing);
						char *short_descr = furniture_get_short_description(obj, compartment);

						act("You wake and stand $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
						act("$n wakes and stands $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
					}

					p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
					if (ch->on)
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
					p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				}
			}
			break;

		case POS_SITTING:
		case POS_RESTING:
			if (can_stand(ch, obj, compartment, FALSE))
			{
				ch->position = ch->fighting == NULL ? POS_STANDING : POS_FIGHTING;
				if (ch->bashed > 0)
					ch->bashed = 0;

				if(ch->on_compartment && ch->on_compartment != compartment)
				{
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
				}

				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj == NULL)
				{
					act("You stand.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n stands.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					char *positional = furniture_get_positional(compartment->standing);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You stand $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n stands $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
			}
			break;

		case POS_STANDING:
			if (ch->on_compartment == compartment)
			{
				send_to_char("You are already standing.\n\r", ch);
			}
			else if (can_stand(ch, obj, compartment, FALSE))
			{
				ch->position = POS_STANDING;

				if (ch->on)
				{
					char *short_descr = furniture_get_short_description(ch->on, ch->on_compartment);

					if (IS_SET(ch->on_compartment->standing, FURNITURE_AT))
					{
						act("You get off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_ON))
					{
						act("You get off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_IN))
					{
						act("You get out of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets out of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_ABOVE))
					{
						act("You get down from $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets down from $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_UNDER))
					{
						act("You get out from under $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets out from under $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,0,0,0,0,0);
				}

				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj)
				{
					char *positional = furniture_get_positional(compartment->standing);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You stand $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n stands $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
			}
			break;

		case POS_FIGHTING:
			// Difference between this and STANDING is that this will maintain the FIGHTING state
			// This will allow mechanics where you are fighting and need to stand on something mid-fight
			if (ch->on_compartment == compartment)
			{
				send_to_char("You are already standing.\n\r", ch);
			}
			else if (can_stand(ch, obj, compartment, FALSE))
			{
				if (ch->on)
				{
					char *short_descr = furniture_get_short_description(ch->on, ch->on_compartment);

					if (IS_SET(ch->on_compartment->standing, FURNITURE_AT))
					{
						act("You get off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_ON))
					{
						act("You get off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets off of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_IN))
					{
						act("You get out of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets out of $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_ABOVE))
					{
						act("You get down from $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets down from $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}
					else if (IS_SET(ch->on_compartment->standing, FURNITURE_UNDER))
					{
						act("You get out from under $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_CHAR);
						act("$n gets out from under $t.",ch, NULL, NULL,ch->on, NULL, short_descr, NULL,TO_ROOM);
					}

					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,0,0,0,0,0);
				}

				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj)
				{
					char *positional = furniture_get_positional(compartment->standing);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You stand $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n stands $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				// Still fire off the STAND triggers, even while fighting
				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_STAND, NULL,new_ordinal,0,0,0,0);
			}
			break;
		
	}
}

bool can_rest(CHAR_DATA *ch, OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment, bool silent)
{
	int ret;

	// Are there monsters nearby? >.>

	if (IS_VALID(obj) && (!IS_FURNITURE(obj) || (IS_VALID(compartment) && !compartment->resting)))
	{
		if (!silent)
			send_to_char("You can't seem to find a place to rest.\n\r",ch);
		return FALSE;
	}

	ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PREREST, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to rest!\n\r", ch);
		}

		return FALSE;
	}

	if (ch->on_compartment && ch->on_compartment != compartment)
	{
		if (compartment_is_closed(ch->on_compartment))
		{
			if (!silent)
				act_new("You need to open the compartment first before you can leave.", ch,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PRESTEPOFF, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				act("You are unable to step off from $p!", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR);
			}

			return FALSE;
		}
	}

	if (compartment && ch->on_compartment != compartment)
	{
		if (compartment->max_occupants >= 0 && furniture_count_users(obj, compartment) >= compartment->max_occupants)
		{
			if (!silent)
				act_new("There's no room to rest on $p.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		if (compartment_is_closed(compartment))
		{
			if (!silent)
				act_new("That compartment is closed.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREST, NULL,0,0,0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				char *positional = furniture_get_positional(compartment->resting);
				char *short_descr = furniture_get_short_description(obj, compartment);

				act("You are unable to rest $t $T!", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
			}

			return FALSE;
		}
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PREREST, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to rest here!\n\r", ch);
		}

		return FALSE;
	}

	return TRUE;
}


void do_rest(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;
	FURNITURE_COMPARTMENT *compartment = NULL;
	int old_ordinal = ch->on_compartment ? ch->on_compartment->ordinal : 0;
	int new_ordinal = 0;

    if (MOUNTED(ch))
    {
        send_to_char("You can't rest while mounted.\n\r", ch);
        return;
    }

    if (RIDDEN(ch))
    {
        send_to_char("You can't rest while being ridden.\n\r", ch);
        return;
    }

	if (ch->position == POS_RESTING)
	{
		send_to_char("You are already resting.\n\r", ch);
		return;
	}

    if (ch->position == POS_FIGHTING)
    {
		send_to_char("You are already fighting!\n\r",ch);
		return;
    }

    /* okay, now that we know we can rest, find an object to rest on */
    if (argument[0] != '\0')
    {
		char arg[MIL];

		argument = one_argument(argument, arg);

		if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
		{
			send_to_char("You don't see that here.\n\r",ch);
			return;
		}

		if (!IS_FURNITURE(obj))
		{
			send_to_char("You cannot possibly stand on that.\n\r", ch);
			return;
		}

		compartment = furniture_find_compartment(ch, obj, argument, "rest");
		if (!IS_VALID(compartment)) return;

		if (!compartment->resting)
		{
			send_to_char("There is no where to rest on that.\n\r", ch);
			return;
		}

		new_ordinal = compartment->ordinal;
    }
    else
	{
		obj = ch->on;
		compartment = ch->on_compartment;
		new_ordinal = (compartment ? compartment->ordinal : 0);
	}

	switch(ch->position)
	{
		case POS_SLEEPING:
			if (can_wake_up(ch, ch, FALSE))
			{
				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				if (ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);

				if (can_rest(ch, obj, compartment, FALSE))
				{
					ch->position = POS_RESTING;
					if(ch->on_compartment && ch->on_compartment != compartment)
					{
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
					}

					ch->on = obj;
					ch->on_compartment = compartment;

					if (obj == NULL)
					{
						act("You wake and rest.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						act("$n wakes and rests.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
					else
					{
						char *positional = furniture_get_positional(compartment->resting);
						char *short_descr = furniture_get_short_description(obj, compartment);

						act("You wake and rest $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
						act("$n wakes and rests $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
					}

					p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
					if(ch->on)
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
					p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
				}
			}
			break;

		case POS_SITTING:
			if (can_rest(ch, obj, compartment, FALSE))
			{
				ch->position = POS_RESTING;
				if(ch->on_compartment && ch->on_compartment != compartment)
				{
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
				}
				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj == NULL)
				{
					act("You rest.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n rests.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					char *positional = furniture_get_positional(compartment->standing);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You rest $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n rests $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
			}
			break;

		case POS_STANDING:
			if (can_rest(ch, obj, compartment, FALSE))
			{
				ch->position = POS_RESTING;
				if(ch->on_compartment && ch->on_compartment != compartment)
				{
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
				}
				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj == NULL)
				{
					send_to_char("You rest.\n\r", ch);
					act("$n sits down and rests.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					char *positional = furniture_get_positional(compartment->standing);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You sit down $t $T and rest.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n sits down $t $T and rests.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_REST, NULL,new_ordinal,0,0,0,0);
			}
	}

}

bool can_sit(CHAR_DATA *ch, OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment, bool silent)
{
	int ret;

	if (IS_VALID(obj) && (!IS_FURNITURE(obj) || (IS_VALID(compartment) && !compartment->sitting)))
	{
		if (!silent)
			send_to_char("You can't seem to find a place to sit.\n\r",ch);
		return FALSE;
	}

	ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESIT, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to sit!\n\r", ch);
		}

		return FALSE;
	}

	if (ch->on_compartment && ch->on_compartment != compartment)
	{
		if (compartment_is_closed(ch->on_compartment))
		{
			if (!silent)
				act_new("You need to open the compartment first before you can leave.", ch,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PRESTEPOFF, NULL,0,0,0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				act("You are unable to step off from $p!", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR);
			}

			return FALSE;
		}
	}

	if (compartment && ch->on_compartment != compartment)
	{
		if (compartment->max_occupants >= 0 && furniture_count_users(obj, compartment) >= compartment->max_occupants)
		{
			if (!silent)
				act_new("There's no room to rest on $p.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		if (compartment_is_closed(compartment))
		{
			if (!silent)
				act_new("That compartment is closed.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRESIT, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				char *positional = furniture_get_positional(compartment->sitting);
				char *short_descr = furniture_get_short_description(obj, compartment);

				act("You are unable to sit $t $T!", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
			}

			return FALSE;
		}
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESIT, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,0);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to sit here!\n\r", ch);
		}

		return FALSE;
	}

	return TRUE;
}

void do_sit (CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;
	FURNITURE_COMPARTMENT *compartment = NULL;
	int old_ordinal = ch->on_compartment ? ch->on_compartment->ordinal : 0;
	int new_ordinal = 0;

    if (MOUNTED(ch))
    {
        send_to_char("You can't sit while mounted.\n\r", ch);
        return;
    }

    if (RIDDEN(ch))
    {
        send_to_char("You can't sit while being ridden.\n\r", ch);
        return;
    }

	if (ch->position == POS_SITTING)
	{
		send_to_char("You are already sitting.\n\r", ch);
		return;
	}

    if (ch->position == POS_FIGHTING)
    {
		send_to_char("Maybe you should finish this fight first?\n\r",ch);
		return;
    }

    /* okay, now that we know we can sit, find an object to sit on */
	if (argument[0] != '\0')
	{
		char arg[MIL];

		argument = one_argument(argument, arg);

		if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
		{
			send_to_char("You don't see that here.\n\r",ch);
			return;
		}

		if (!IS_FURNITURE(obj))
		{
			send_to_char("You cannot possibly stand on that.\n\r", ch);
			return;
		}

		compartment = furniture_find_compartment(ch, obj, argument, "sit");
		if (!IS_VALID(compartment)) return;

		if (!compartment->sitting)
		{
			send_to_char("There is no where to sit on that.\n\r", ch);
			return;
		}

		new_ordinal = compartment->ordinal;
	}
    else
	{
		obj = ch->on;
		compartment = ch->on_compartment;
		new_ordinal = (compartment ? compartment->ordinal : 0);
	}

	switch(ch->position)
	{
		case POS_SLEEPING:
			if (can_wake_up(ch, ch, FALSE))
			{
				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				if (ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_WAKE, NULL,old_ordinal,0,0,0,0);

				if (can_sit(ch, obj, compartment, FALSE))
				{
					ch->position = POS_SITTING;
					if(ch->on_compartment && ch->on_compartment != compartment)
					{
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
					}
					ch->on = obj;
					ch->on_compartment = compartment;

					if (obj == NULL)
					{
						act("You wake and sit.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						act("$n wakes and sits.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					}
					else
					{
						char *positional = furniture_get_positional(compartment->sitting);
						char *short_descr = furniture_get_short_description(obj, compartment);

						act("You wake and sit $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
						act("$n wakes and sits $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
					}

					p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
					if(ch->on)
						p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
					p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
				}
			}
			break;

		case POS_RESTING:
			if (can_sit(ch, obj, compartment, FALSE))
			{
				ch->position = POS_SITTING;
				if(ch->on_compartment && ch->on_compartment != compartment)
				{
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
				}
				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj == NULL)
				{
					send_to_char("You stop resting.\n\r", ch);
				}
				else
				{
					char *positional = furniture_get_positional(compartment->sitting);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You sit $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n sits $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
			}
			break;

		case POS_STANDING:
			if (can_sit(ch, obj, compartment, FALSE))
			{
				ch->position = POS_SITTING;
				if(ch->on_compartment && ch->on_compartment != compartment)
				{
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
				}
				ch->on = obj;
				ch->on_compartment = compartment;

				if (obj == NULL)
				{
					send_to_char("You sit.\n\r", ch);
					act("$n sits down.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					char *positional = furniture_get_positional(compartment->sitting);
					char *short_descr = furniture_get_short_description(obj, compartment);

					act("You sit down $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
					act("$n sits down $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
				}

				p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
				if(ch->on)
					p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
				p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SIT, NULL,new_ordinal,0,0,0,0);
			}
	}
}

bool can_sleep(CHAR_DATA *ch, OBJ_DATA *obj, FURNITURE_COMPARTMENT *compartment, bool silent)
{
	int ret;

	if (IS_VALID(obj) && (!IS_FURNITURE(obj) || (IS_VALID(compartment) && !compartment->sleeping)))
	{
		if (!silent)
			send_to_char("You can't seem to find a place to sleep.\n\r",ch);
		return FALSE;
	}

	ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESLEEP, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,PRESLEEP_NORMAL);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to sleep!\n\r", ch);
		}

		return FALSE;
	}

	if (ch->on_compartment && ch->on_compartment != compartment)
	{
		if (compartment_is_closed(ch->on_compartment))
		{
			if (!silent)
				act_new("You need to open the compartment first before you can leave.", ch,NULL,NULL,NULL,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PRESTEPOFF, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,PRESLEEP_NORMAL);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				act("You are unable to step off from $p!", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR);
			}

			return FALSE;
		}
	}

	if (compartment && ch->on_compartment != compartment)
	{
		if (compartment->max_occupants >= 0 && furniture_count_users(obj, compartment) >= compartment->max_occupants)
		{
			if (!silent)
				act_new("There's no room to rest on $p.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		if (compartment_is_closed(compartment))
		{
			if (!silent)
				act_new("That compartment is closed.", ch,NULL,NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRESLEEP, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,PRESLEEP_NORMAL);
		if (ret)
		{
			if (!silent && ret != PRET_SILENT)
			{
				char *positional = furniture_get_positional(compartment->sleeping);
				char *short_descr = furniture_get_short_description(obj, compartment);

				act("You are unable to sleep $t $T!", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
			}

			return FALSE;
		}
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, obj, TRIG_PRESLEEP, NULL,(ch->on_compartment?ch->on_compartment->ordinal:0),(compartment?compartment->ordinal:0),0,0,PRESLEEP_NORMAL);
	if (ret)
	{
		if (!silent && ret != PRET_SILENT)
		{
			send_to_char("You are unable to sleep here!\n\r", ch);
		}

		return FALSE;
	}

	return TRUE;
}

void do_sleep(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;
	FURNITURE_COMPARTMENT *compartment = NULL;
	int old_ordinal = ch->on_compartment ? ch->on_compartment->ordinal : 0;
	int new_ordinal = 0;

    if (MOUNTED(ch))
    {
        send_to_char("You can't sleep while mounted.\n\r", ch);
        return;
    }

    if (RIDDEN(ch))
    {
        send_to_char("You can't sleep while being ridden.\n\r", ch);
        return;
    }

	if (ch->position == POS_SLEEPING)
	{
	    send_to_char("You are already sleeping.\n\r", ch);
		return;
	}

    if (ch->position == POS_FIGHTING)
    {
		send_to_char("Maybe you should finish this fight first?\n\r",ch);
		return;
    }

    switch (ch->position)
    {
	case POS_RESTING:
	case POS_SITTING:
	case POS_STANDING:
		if (argument[0] != '\0')
		{
			char arg[MIL];

			argument = one_argument(argument, arg);

			if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
			{
				send_to_char("You don't see that here.\n\r",ch);
				return;
			}

			if (!IS_FURNITURE(obj))
			{
				send_to_char("You cannot possibly stand on that.\n\r", ch);
				return;
			}

			compartment = furniture_find_compartment(ch, obj, argument, "sleep");
			if (!IS_VALID(compartment)) return;

			if (!compartment->sleeping)
			{
				send_to_char("There is no where to sleep on that.\n\r", ch);
				return;
			}

			new_ordinal = compartment->ordinal;
		}
		else
		{
			obj = ch->on;
			compartment = ch->on_compartment;
			new_ordinal = (compartment ? compartment->ordinal : 0);
		}

		if (can_sleep(ch, obj, compartment, FALSE))
		{
			if(ch->on_compartment && ch->on_compartment != compartment)
			{
				p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_STEPOFF, NULL,old_ordinal,0,0,0,0);
			}
			ch->on = obj;
			ch->on_compartment = compartment;

			if(obj == NULL)
			{
				send_to_char("You go to sleep.\n\r", ch);
				act("$n goes to sleep.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				char *positional = furniture_get_positional(compartment->sleeping);
				char *short_descr = furniture_get_short_description(obj, compartment);

				act("You go to sleep $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_CHAR);
				act("$n goes to sleep $t $T.", ch, NULL, NULL, obj, NULL, positional, short_descr, TO_ROOM);
			}

			ch->position = POS_SLEEPING;
		
			p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SLEEP, NULL,new_ordinal,0,0,0,0);
			if(ch->on)
				p_percent_trigger(NULL, ch->on, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SLEEP, NULL,new_ordinal,0,0,0,0);
			p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, ch->on, NULL, TRIG_SLEEP, NULL,new_ordinal,0,0,0,0);
		}
	    break;
    }
}


void do_wake(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;

	one_argument(argument, arg);
	if (arg[0] == '\0')
	{
		do_function(ch, &do_stand, "");
		return;
	}

	if (!IS_AWAKE(ch))
	{
		send_to_char("You are asleep yourself!\n\r", ch);
		return;
	}

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

    if (IS_AWAKE(victim))
	{
		act("$N is already awake.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (can_wake_up(victim, ch, FALSE))
	{
	    act_new("$n wakes you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT,POS_SLEEPING,NULL);
		victim->position = POS_SITTING;	// Wake them up, but then let them try to stand

		do_stand(victim, "");
	}
}


void do_sneak(CHAR_DATA *ch, char *argument)
{
    AFFECT_DATA af;
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);
    if (arg[0] != '\0' && (IS_IMMORTAL(ch)))
    {
	ROOM_INDEX_DATA *room;

	if ((room = find_location(ch, arg)) == NULL)
	{
	    send_to_char("Couldn't find that location.\n\r", ch);
	    return;
	}

	char_from_room(ch);
	char_to_room(ch, room);
	do_function(ch, &do_look, "auto");
	return;
    }

    if (MOUNTED(ch))
    {
        send_to_char("You can't sneak while riding.\n\r", ch);
        return;
    }

    if (ch->fighting != NULL)
    {
	send_to_char("You can't sneak while fighting.\n\r", ch);
	return;
    }

    send_to_char("You attempt to move silently.\n\r", ch);
    affect_strip(ch, gsk_sneak);

    if (IS_AFFECTED(ch,AFF_SNEAK))
	return;

memset(&af,0,sizeof(af));

    if (number_percent() < get_skill(ch, gsk_sneak))
    {
	check_improve(ch,gsk_sneak,TRUE,3);
	af.where     = TO_AFFECTS;
	af.group     = AFFGROUP_PHYSICAL;
	af.skill     = gsk_sneak;
	af.level     = ch->level;
	af.duration  = ch->level;
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = AFF_SNEAK;
	af.bitvector2 = 0;
		af.slot	= WEAR_NONE;
	affect_to_char(ch, &af);
        send_to_char("You successfully move into a sneaking position.\n\r", ch);
    }
    else
    {
	check_improve(ch,gsk_sneak,FALSE,3);
        send_to_char("You fail to move silently.\n\r", ch);
    }
}


void do_hide(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;

    /* take care of hide <obj> */
    if (argument[0] != '\0')
    {
		char arg1[MIL];
		char arg2[MIL];

		argument = one_argument(argument, arg1);
		argument = one_argument(argument, arg2);

		if ((obj = get_obj_carry(ch, arg1, ch)) != NULL)
		{
			char buf[MAX_STRING_LENGTH];
			char buf2[MAX_STRING_LENGTH];
			int chance;
			CHAR_DATA *others;

			if (!can_drop_obj(ch, obj, TRUE) || IS_SET(obj->extra[1], ITEM_KEPT)) {
				send_to_char("You can't let go of it.\n\r", ch);
				return;
			}

			if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREHIDE, NULL,0,0,0,0,0))
				return;

			if( !str_cmp(arg2, "in") )
			{
				// We are trying to hide something inside an object
				OBJ_DATA *container;

				if( IS_NULLSTR(argument) )
				{
					act("Hide it in what?", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if( (container = get_obj_inv(ch, argument, FALSE)) == NULL )
				{
					act("You don't have that item.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if (!can_put_obj(ch, obj, container, NULL, FALSE))
					return;

				if(p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREHIDE_IN, NULL,0,0,0,0,0))
					return;

				obj_from_char(obj);
				obj_to_obj(obj, container);

				p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_HIDE, NULL,0,0,0,0,0);
			}
			else if( !str_cmp(arg2, "on") )
			{
				CHAR_DATA *victim;

				int sneak1;
				int sneak2;

				if( IS_NULLSTR(argument) )
				{
					act("Hide it on whom?", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if( (victim = get_char_room(ch, NULL, argument)) == NULL )
				{
					act("They aren't here.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if( victim == ch )
				{
					send_to_char("You would you hide that on yourself?", ch);
					return;
				}

				if( (victim->carry_number + get_obj_number(obj)) > can_carry_n(victim))
				{
					act("$N can't carry that.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if( (get_carry_weight(victim) + get_obj_weight(obj)) > can_carry_w(victim))
				{
					act("$N can't carry that.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return;
				}

				if(p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREHIDE_IN, NULL,0,0,0,0,0))
					return;

				act("You deftly hide $p on $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);

				// Do a skill test
				sneak1 = get_skill(ch, gsk_sneak) * ch->tot_level;
				if( IS_REMORT(ch) ) sneak1 = 3 * sneak1 / 2;	// 50% boost
				if( IS_SAGE(ch) ) sneak1 = 3 * sneak1 / 2;		// 50% boost
				if( number_percent() < get_skill(ch, gsk_deception) ) sneak1 *= 2;

				sneak2 = get_skill(victim, gsk_sneak) * victim->tot_level;
				if( IS_REMORT(victim) ) sneak2 = 3 * sneak2 / 2;	// 50% boost
				if( IS_SAGE(victim) ) sneak2 = 3 * sneak2 / 2;		// 50% boost
				if( number_percent() < get_skill(victim, gsk_deception) ) sneak2 *= 2;

				// Check if victim is awake or if the victim is immortal and hider is not
				if( IS_AWAKE(victim) || (IS_IMMORTAL(victim) && !IS_IMMORTAL(ch)) )
				{
					// Check if hider's sneak score is weaker and if the hider is a player or the victim is immortal
					if( (sneak1 < sneak2) && (!IS_IMMORTAL(ch) || IS_IMMORTAL(victim)) )
						act("$n hides something on you.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
				}

				obj_from_char(obj);
				obj_to_char(obj,victim);

				p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_HIDE, NULL,0,0,0,0,0);
			}
			else
			{
				if (ch->in_room->sector_type == SECT_WATER_NOSWIM)
				{
					send_to_char("You would never see it again.\n\r", ch);
					return;
				}

				if (ch->in_room->sector_type == SECT_AIR)
				{
					send_to_char("Nowhere to hide it when you're floating...\n\r", ch);
					return;
				}

				chance = number_range (0, 4);

				switch(ch->in_room->sector_type)
				{
					case SECT_INSIDE:
				case SECT_CITY:
					if (chance == 0)
						sprintf(buf2, "in the corner");
					else if (chance == 1)
						sprintf(buf2, "amidst the shadows");
					else if (chance == 2)
						sprintf(buf2, "beneath some forgotten trash");
					else if (chance == 3)
						sprintf(buf2, "in a poorly lit area");
					else
						sprintf(buf2, "from view");
					break;
				case SECT_FIELD:
					if (chance == 0)
						sprintf(buf2, "among the grasses");
					else if (chance == 1)
						sprintf(buf2, "in a bed of flowers");
					else if (chance == 2)
						sprintf(buf2, "under a pile of stones");
					else if (chance == 3)
						sprintf(buf2, "in a small hole");
					else
						sprintf(buf2, "from sight");
					break;
				case SECT_FOREST:
					if (chance == 0)
						sprintf(buf2, "inside a tree");
					else if (chance == 1)
						sprintf(buf2, "under a stump");
					else if (chance == 2)
						sprintf(buf2, "in the thick vegetation");
					else if (chance == 3)
						sprintf(buf2, "in the branches of a tree");
					else
						sprintf(buf, "from sight");
					break;
				case SECT_HILLS:
					if (chance == 0)
						sprintf(buf2, "under a large rock");
					else
						sprintf(buf2, "from sight");
					break;
				case SECT_MOUNTAIN:
					sprintf(buf2, "in the deep mountain crags");
					break;
				case SECT_WATER_SWIM:
					sprintf(buf2, "in the sands beneath your feet");
					break;
				case SECT_TUNDRA:
					sprintf(buf2, "beneath a large pile of snow");
					break;
				case SECT_DESERT:
					sprintf(buf2, "under a pile of desert sand");
					break;
				case SECT_NETHERWORLD:
					sprintf(buf2, "beneath a pile of bones");
					break;
				case SECT_DOCK:
					sprintf(buf2, "under a couple of planks");
					break;
				}

				act("You deftly hide $p $t.", ch, NULL, NULL, obj, NULL, buf2, NULL, TO_CHAR);
				for (others = ch->in_room->people;
					  others != NULL; others = others->next_in_room)
				{
					if (((number_percent() < get_skill(ch, gsk_deception)) ||
						( number_percent() < get_skill(ch, gsk_hide))) &&
						ch != others &&
						can_see_obj(others, obj))
				{
						act("You notice $N hide $p $t.", others, ch, NULL, obj, NULL, buf2, NULL, TO_CHAR);
				}
				}
				obj_from_char(obj);
				obj_to_room(obj, ch->in_room);

				p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_HIDE, NULL,0,0,0,0,0);
			}
			SET_BIT(obj->extra[0], ITEM_HIDDEN);
			return;
		}
		else
		{
			act("You don't have that item.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
    }

	// Hiding self
    if (IS_SET(ch->affected_by[0], AFF_HIDE))
    {
    	send_to_char("You are already hidden.\n\r", ch);
		return;
    }

    if (MOUNTED(ch))
    {
        send_to_char("You can't hide while riding.\n\r", ch);
        return;
    }

    if (ch->fighting != NULL)
    {
		send_to_char("You can't hide while fighting.\n\r", ch);
		return;
    }

	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREHIDE, NULL,0,0,0,0,0))
		return;

    send_to_char("You attempt to hide.\n\r", ch);

    HIDE_STATE(ch, gsk_hide->beats);
    return;
}


void hide_end(CHAR_DATA *ch)
{
    CHAR_DATA *rch;

    if (number_percent() < get_skill(ch, gsk_hide))
    {
		SET_BIT(ch->affected_by[0], AFF_HIDE);
        for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
		{
            if (get_skill(rch, gsk_deception) > 0)
		    {
		        if (number_percent() < get_skill(rch, gsk_deception))
				{
		            act("{D$n hides in the shadows.{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				    check_improve(rch, gsk_deception, TRUE, 1);
				}
		    }
		}

		send_to_char("You successfully hide in the shadows.\n\r{x", ch);
		check_improve(ch,gsk_hide,TRUE,3);

		// Allow for other fun stuff to occur when you are fully hidden
		p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_HIDDEN, NULL,0,0,0,0,0);
    }
    else
    {
		send_to_char("You fail to hide in the shadows.\n\r", ch);
		check_improve(ch,gsk_hide,FALSE,3);
    }
}

void do_visible(CHAR_DATA *ch, char *argument)
{
    affect_strip(ch, gsk_invis			);
    affect_strip(ch, gsk_mass_invis			);
    affect_strip(ch, gsk_sneak			);
    affect_strip(ch, gsk_improved_invisibility);
    affect_strip(ch, gsk_cloak_of_guile);
    REMOVE_BIT   (ch->affected_by[0], AFF_HIDE		);
    REMOVE_BIT   (ch->affected_by[0], AFF_INVISIBLE	);
    REMOVE_BIT   (ch->affected_by[0], AFF_SNEAK		);
    REMOVE_BIT   (ch->affected_by[1], AFF2_IMPROVED_INVIS);
    REMOVE_BIT   (ch->affected_by[1], AFF2_CLOAK_OF_GUILE);
    send_to_char("You reveal yourself.\n\r", ch);
}

void do_recall(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;

    if (IS_NPC(ch))
    {
	send_to_char("Only players can recall.\n\r",ch);
	return;
    }

    if (ch->tot_level > 30)
    {
	send_to_char("Your prayers are unanswered.\n\r", ch);
	return;
    }

    if (IS_DEAD(ch))
    {
	send_to_char("You can't, you are dead.\n\r", ch);
	return;
    }

    if(p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL,NULL,0,0,0,0,0) ||
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL,NULL,0,0,0,0,0))
	return;


    act("$n prays for transportation!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    if (!(location = get_recall_room(ch))) {
	send_to_char("You are completely lost.\n\r", ch);
	return;
    }

    if (ch->in_room == location)
	return;
	/* Adding area_no_recall check to go with corresponding area flag - Areo 08-10-2006 */
    if (IS_SET(ch->in_room->room_flag[0], ROOM_NO_RECALL)
    ||   IS_AFFECTED(ch, AFF_CURSE)
    || IS_SET(ch->in_room->area->area_flags, AREA_NO_RECALL))
    {
	send_to_char("Nothing happens.\n\r", ch);
	return;
    }

    if (ch->fighting != NULL)
    {
	send_to_char("You cannot concentrate enough to recall.\n\r", ch);
	return;
    }

    ch->move /= 2;
    act("{D$n disappears.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, location);
    act("{D$n appears in the room.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    do_function(ch, &do_look, "auto");


    if (ch->pet && !ch->pet->fighting) {
	ch->pet->move /= 2;
	act("{D$n disappears.{x", ch->pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	char_from_room(ch->pet);
	char_to_room(ch->pet, location);
	act("{D$n appears in the room.{x", ch->pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	do_function(ch->pet, &do_look, "auto");
    }
    if (ch->mount && !ch->mount->fighting) {
	ch->mount->move /= 2;
	act("{D$n disappears.{x", ch->mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	char_from_room(ch->mount);
	char_to_room(ch->mount, location);
	act("{D$n appears in the room.{x", ch->mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	do_function(ch->mount, &do_look, "auto");
    }
}

void do_fade(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int door;
    int counter;

    argument = one_argument(argument, arg);

    if (!IS_VALID(ch))
    {
        bug("act_comm.c, do_fade, invalid ch.", 0);
        return;
    }

    if (get_skill(ch, gsk_fade) == 0)
    {
	send_to_char("Huh?\n\r", ch);
	return;
    }

    if (IS_SOCIAL(ch))
    {
	send_to_char("Your dimensional powers are useless here.\n\r", ch);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Fade which way?\n\r", ch);
	return;
    }

    if (ch->pulled_cart != NULL)
    {
	act("You can't fade while pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    /* special parsing so people can abbreviate with nw, se. etc. */
    if (!str_cmp(arg, "ne"))
	sprintf(arg, "northeast");
    else
    if (!str_cmp(arg, "nw"))
	sprintf(arg, "northwest");
    else
    if (!str_cmp(arg, "se"))
	sprintf(arg, "southeast");
    else
    if (!str_cmp(arg, "sw"))
	sprintf(arg, "southwest");

    if (ch->fighting != NULL)
    {
	send_to_char("You can't, you are fighting!\n\r", ch);
	return;
    }

    door = -1;
    for (counter = 0; counter < MAX_DIR; counter++)
    {
	if (!str_prefix(arg, dir_name[counter]))
	{
	    door = counter;
	    break;
	}
    }

    if (door == -1)
    {
	send_to_char("That isn't a direction.\n\r", ch);
	return;
    }

	if( IS_SET(ch->in_room->area->area_flags, AREA_NO_FADING) )
    {
		send_to_char("A magical interference dampens your dimensional powers.\n\r", ch);
		return;
    }

    ch->fade_dir = door;
    FADE_STATE(ch, 4);

    act("{W$n fades to a different dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM);
    act("{WYou fade to a different dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR);
    check_improve(ch,gsk_fade,TRUE,1);
}


void fade_end(CHAR_DATA *ch)
{
	int counter = 0;
	int max_fade;
	int beats;
	int skill;

	if( ch->force_fading > 0)
	{
		skill = ch->force_fading;
	}
	else
	{
		skill = get_skill(ch, gsk_fade);

	}
	beats = 8 - (skill / 25);
	/*
	   Skill   Beats
		0-24 = 8
	   25-49 = 7
	   50-74 = 6
	   75-99 = 5
		100  = 4
	*/
	max_fade = 3 + ((skill > 75) ? (skill/5 - 15) : 0);

	for (counter = 0; counter < max_fade; counter++) {
		if (!move_success(ch)) {
			act("{W$n fades in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			ch->fade_dir = -1;	/* @@@NIB : 20071020 */
			ch->force_fading = 0;
			return;
		} else if (counter != 2)
			act("{W$n fades in then off to the $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[ch->fade_dir], TO_ROOM);
	}

	do_function(ch, &do_look, "auto");
	FADE_STATE(ch, beats);
	act("{W$n fades in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}


bool move_success(CHAR_DATA *ch)
{
	ROOM_INDEX_DATA *in_room;
	ROOM_INDEX_DATA *to_room;
	WILDS_DATA *to_wilds = NULL;
	EXIT_DATA *pexit;
	int door;
	int to_vroom_x = 0;
	int to_vroom_y = 0;

	in_room = ch->in_room;
	door = ch->fade_dir;

	if( IS_SET(ch->in_room->area->area_flags, AREA_NO_FADING) )
    {
		act("Magical interference stops your ability to fade.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
    }

	if (ch->in_room && (ch->in_room->sector_type == SECT_WATER_NOSWIM ||
		ch->in_room->sector_type == SECT_WATER_SWIM)) {
		act("Magical interference stops your ability to fade.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (!IS_NPC(ch) && (p_exit_trigger(ch, door, PRG_MPROG,0,0,0,0,0) || p_exit_trigger(ch, door, PRG_OPROG,0,0,0,0,0) || p_exit_trigger(ch, door, PRG_RPROG,0,0,0,0,0)))
		return FALSE;

	if (!(pexit = in_room->exit[door])) {
		//Updated show_room_to_char to show_room -- Tieryo 08/18/2010
		show_room(ch,ch->in_room,false,false,false);
	        act("\n\rYou can't go any further.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	        return FALSE;
	}

	if (IS_SET(pexit->exit_info, EX_CLOSED) &&
		(!IS_AFFECTED(ch, AFF_PASS_DOOR) || IS_SET(pexit->exit_info,EX_NOPASS))) {
		//Updated show_room_to_char to show_room --Tieryo 08/18/2010
		show_room(ch,ch->in_room,false,false,false);
	        act("\n\rYou can't go any further.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	        return FALSE;
	}

	if(!(to_room = exit_destination(pexit))) {
		send_to_char ("Alas, you cannot go that way.\n\r", ch);
		return FALSE;
	}

	if(!can_see_room (ch, to_room)) {
		send_to_char ("Alas, you cannot go that way.\n\r", ch);
		return FALSE;
	}

	char_from_room(ch);
	/* VIZZWILDS */
	if (to_wilds)
		char_to_vroom (ch, to_wilds, to_vroom_x, to_vroom_y);
	else
		char_to_room(ch, to_room);

	return TRUE;
}


void do_bar(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char exit[MSL];
	OBJ_DATA *obj;
	int door;

	one_argument(argument, arg);

	if (get_skill(ch, gsk_bar) == 0)
	{
		send_to_char("You have no knowledge of this skill.\n\r", ch);
		return;
	}

	if (arg[0] == '\0')
	{
		send_to_char("Bar what?\n\r", ch);
		return;
	}

	if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
	{
		if (obj->item_type == ITEM_PORTAL)
		{
			if (!IS_SET(obj->value[1],EX_ISDOOR) ||
				IS_SET(obj->value[1],EX_NOCLOSE))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (!IS_SET(obj->value[1],EX_CLOSED))
			{
				send_to_char("It's not closed.\n\r",ch);
				return;
			}

			if (IS_SET(obj->value[1],EX_BARRED))
			{
				send_to_char("It's already barred.\n\r",ch);
				return;
			}

			if (IS_SET(obj->value[1],EX_NOBAR))
			{
				act("You can't find a way to bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				return;
			}

			SET_BIT(obj->value[1],EX_BARRED);
			act("You bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			act("$n bars up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
			check_improve(ch, gsk_bar, TRUE, 1);
			return;
		}

		act("You can't bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		return;
	}

	if ((door = find_door(ch, arg, TRUE)) >= 0)
	{
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		pexit = ch->in_room->exit[door];

		if (!IS_SET(pexit->exit_info,EX_ISDOOR) ||
			IS_SET(pexit->exit_info,EX_NOCLOSE))
		{
			send_to_char("You can't do that.\n\r",ch);
			return;
		}

		if (!IS_SET(pexit->exit_info, EX_CLOSED))
		{
			send_to_char("It's not closed.\n\r", ch);
			return;
		}
		if (IS_SET(pexit->exit_info, EX_BARRED))
		{
			send_to_char("It's already barred.\n\r", ch);
			return;
		}
		if (IS_SET(pexit->exit_info, EX_NOBAR))
		{
			send_to_char("You can't bar it.\n\r", ch);
			return;
		}

		exit_name(ch->in_room, door, exit);

		SET_BIT(pexit->exit_info, EX_BARRED);
		act("You bar up the $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
		act("$n bars the $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);
		check_improve(ch, gsk_bar, TRUE, 1);

		/* bar the other side */
		if ((to_room   = pexit->u1.to_room) != NULL &&
			(pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
			pexit_rev->u1.to_room == ch->in_room)
		{
			SET_BIT(pexit_rev->exit_info, EX_BARRED);
		}
	}
}

void do_jam(CHAR_DATA *ch, char *argument)
{

}

void do_evasion(CHAR_DATA *ch, char *argument)
{
    AFFECT_DATA af;

    if (get_skill(ch, gsk_evasion) == 0)
    {
	send_to_char("You know nothing of this skill.\n\r", ch);
	return;
    }

    if (MOUNTED(ch))
    {
        send_to_char("You can't be that evasive while riding.\n\r", ch);
        return;
    }

    if (IS_AFFECTED2(ch, AFF2_EVASION))
    {
	send_to_char("You are already as evasive as can be.\n\r", ch);
	return;
    }

memset(&af,0,sizeof(af));
    if (number_percent() < get_skill(ch, gsk_evasion))
    {
	af.where     = TO_AFFECTS;
	af.group     = AFFGROUP_PHYSICAL;
	af.skill     = gsk_evasion;
	af.level     = ch->tot_level;
	af.duration  = ch->tot_level/3;
	af.location  = APPLY_DEX;
	af.modifier  = 3;
	af.bitvector = 0;
        af.bitvector2 = AFF2_EVASION;
		af.slot	= WEAR_NONE;
	affect_to_char(ch, &af);
        send_to_char("You shroud yourself in your cloak, prepared to be evasive.\n\r", ch);
	check_improve(ch,gsk_evasion,TRUE,3);
    }
    else
    {
        send_to_char("You fail to be any more evasive.\n\r", ch);
	check_improve(ch,gsk_evasion,FALSE,3);
    }
}


void do_warp(CHAR_DATA *ch, char *argument)
{
	send_to_char("Warp speed! NOW!!!\n\r", ch);
	return;
}


void check_see_hidden(CHAR_DATA *ch)
{
	OBJ_DATA *obj;

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
		if (IS_SET(obj->extra[1], ITEM_SEE_HIDDEN) &&
			obj->wear_loc != WEAR_NONE)
			break;
	}

	if (obj)
	{
		int i = 0;
		EXIT_DATA *temp_exit;

		for (temp_exit = ch->in_room->exit[0]; i < MAX_DIR; temp_exit = ch->in_room->exit[i++])
		{
			if (temp_exit != NULL &&
				IS_SET(temp_exit->exit_info, EX_HIDDEN) &&
				!IS_SET(temp_exit->exit_info, (EX_FOUND|EX_NOSEARCH)))
			{
				act("{Y$p{Y begins to vibrate and hum.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("{Y$n's $p{Y begins to vibrate and hum.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				return;
			}
		}
	}
}

void check_traps(CHAR_DATA *ch, bool show)
{
	EXIT_DATA *exit;
	OBJ_DATA *obj;
	int i;

	if (ch == NULL)
	{
		bug("checked traps for null ch!", 0);
		return;
	}

	for (i = 0; i < MAX_DIR; i++)
	{
		exit = ch->in_room->exit[i];
		if (exit == NULL ||
			IS_SET(exit->exit_info, EX_HIDDEN) ||
			exit->u1.to_room == NULL)
			continue;

		if (IS_SET(exit->u1.to_room->room_flag[0],ROOM_DEATH_TRAP) &&
			number_percent() < get_skill(ch, gsk_detect_traps))
		{
			if( show )
				act("{RYou sense a strong feeling of danger coming from the $t.{x", ch, NULL, NULL, NULL, NULL, dir_name[i], NULL, TO_CHAR);

			if (number_percent() < 5)
				check_improve_show(ch, gsk_detect_traps, TRUE, 5, show);
		}
	}

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
	{
		if (!IS_SET(obj->extra[0], ITEM_HIDDEN) &&
			IS_SET(obj->extra[1], ITEM_TRAPPED) &&
			number_percent() < get_skill(ch, gsk_detect_traps))
		{
			if ( show )
				act("{RYou sense a strong feeling of danger coming from $p.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

			if (number_percent() < 5)
				check_improve_show(ch, gsk_detect_traps, TRUE, 5, show);
		}
	}
}

void do_ambush(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char arg3[MSL];
    AMBUSH_DATA *ambush;
    int min;
    int max;
    int type;

    if (get_skill(ch, gsk_ambush) == 0)
    {
	send_to_char("You know nothing of this skill.\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (!str_cmp(arg, "stop"))
    {
	if (ch->ambush == NULL)
	{
	    send_to_char("You aren't ambushing anybody.\n\r", ch);
	}
	else
	{
	    send_to_char("You stop your ambush.\n\r", ch);
	    free_ambush(ch->ambush);
	    ch->ambush = NULL;
	}

	return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0'
    || argument[0] == '\0' || !is_number(arg2) || !is_number(arg3))
    {
	send_to_char("Syntax: ambush <pc|npc|all> <min level> <max level> <command>\n\r", ch);
	send_to_char("        ambush stop\n\r", ch);
	return;
    }

    if (ch->ambush != NULL)
    {
	send_to_char("You stop your ambush.\n\r", ch);
	free_ambush(ch->ambush);
	ch->ambush = NULL;
    }

    if (!str_cmp(arg, "pc"))
	type = AMBUSH_PC;
    else
    if (!str_cmp(arg, "npc"))
	type = AMBUSH_NPC;
    else
    if (!str_cmp(arg, "all"))
	type = AMBUSH_ALL;
    else
    {
	send_to_char("Syntax: ambush <pc|npc|all> <min level> <max level> <command>\n\r", ch);
	return;
    }

    min = atoi(arg2);
    max = atoi(arg3);
    if (min < 1 || min > 500 || max < 1 || max > 500)
    {
	send_to_char("Level range is 1-500.\n\r", ch);
	return;
    }

    ambush = new_ambush();
    ambush->type = type;
    ambush->min_level = min;
    ambush->max_level = max;
    ambush->command = str_dup(argument);
    ch->ambush = ambush;
    act("You find a good place to hide and crouch down.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("$n finds a good place to hide and crouches down.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}

void do_pk(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mob = NULL;

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
		if (IS_SET(mob->act[0], ACT_PRACTICE))
		    break;
    }

    if (mob == NULL)
    {
	send_to_char("To toggle PK, you must be at a guildmaster.\n\r", ch);
	return;
    }

    if (ch->tot_level < 31 && !IS_REMORT(ch))
    {
	send_to_char("You must be at least level 31 to toggle PK.\n\r", ch);
	return;
    }

    if (ch->church != NULL && ch->church->pk == TRUE)
    {
	send_to_char("You are already in a PK church. You don't need to toggle PK.\n\r", ch);
	return;
    }

    if (ch->pneuma < 5000)
    {
	send_to_char("Toggling PK costs 5000 pneuma. You don't have enough.\n\r", ch);
	return;
    }

    if (IS_SET(ch->act[0], PLR_PK))
    {
	send_to_char("{RAre you SURE you want to toggle off PK? The cost is 5000 pneuma.{x\n\r", ch);
	ch->personal_pk_question = TRUE;
    }
    else
    {
	send_to_char("{RAre you SURE you want toggle on PK? The cost is 5000 pneuma.{x\n\r", ch);
	ch->personal_pk_question = TRUE;
    }
}

void do_knock(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char exit[MSL];
    int door;

    one_argument(argument, arg);

    if (arg[0] == '\0') {
	send_to_char("Knock in which direction?\n\r", ch);
	return;
    }

    /* Knock the door */
    if((door = find_door(ch, arg, TRUE)) >= 0) {
	ROOM_INDEX_DATA *to_room;
	EXIT_DATA *pexit;
	EXIT_DATA *pexit_rev;

	exit_name(ch->in_room, door, exit);

	pexit = ch->in_room->exit[door];
	if (!IS_SET(pexit->exit_info, EX_CLOSED)) {
		send_to_char("It is not closed.\n\r", ch);
		return;
	}

	act("$n knocks on $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);
	act("You knock on $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR);
	p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_KNOCK,0,0,0,0,0);

	if ((to_room = pexit->u1.to_room) != NULL
	&& (pexit_rev = to_room->exit[rev_dir[door]]) != NULL
	&& pexit_rev->u1.to_room == ch->in_room) {
		if(to_room->people) {
			exit_name(to_room, rev_dir[door], exit);
			act("Knocking comes from $T.", to_room->people, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM);
		}
		p_direction_trigger(ch, to_room, rev_dir[door], PRG_RPROG, TRIG_KNOCKING,0,0,0,0,0);
	}
    }
}

void do_takeoff(CHAR_DATA *ch, char *argument)
{
	int chance;
	int weight;
	AFFECT_DATA af;

	if(MOUNTED(ch)) {
		if(IS_AFFECTED(MOUNTED(ch), AFF_FLYING) || is_affected(MOUNTED(ch), gsk_flight)) {
			act("$N is already flying.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if(!p_percent_trigger(MOUNTED(ch), NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TAKEOFF,NULL,0,0,0,0,0)) {
			if(!IS_SET(MOUNTED(ch)->parts,PART_WINGS)) {
				act("$N doesn't seem to be able to fly.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return;
			}

			if(number_range(0,MOUNTED(ch)->max_move/get_curr_stat(MOUNTED(ch),STAT_CON)) > MOUNTED(ch)->move) {
				act("$N appears too exhausted to take flight.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				send_to_char("You are too exhausted to take flight.\n\r",ch);
				return;
			}

			weight = get_carry_weight(MOUNTED(ch));
			if(RIDDEN(ch)) weight += get_carry_weight(RIDDEN(ch)) + size_weight[RIDDEN(ch)->size]; /* plus weight of rider

			   if the weight is too high, it uses more...
  			if((number_range(0,can_carry_w(MOUNTED(ch)))) < weight) {
  				send_to_char("Your weight is encumbering you too much.\n\r",ch);
  				return;
  			}
*/
			act("$n spreads $s wings, taking a few strokes in the air before taking off.", MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		af.where     = TO_AFFECTS;
		af.group     = AFFGROUP_PHYSICAL;
		af.skill      = gsk_flight;
		af.level     = MOUNTED(ch)->tot_level;
		af.duration  = -1;
		af.location  = APPLY_NONE;
		af.modifier  = 0;
		af.bitvector = AFF_FLYING;
		af.bitvector2 = 0;
		af.custom_name = NULL;
		af.slot	= WEAR_NONE;
		affect_to_char(MOUNTED(ch), &af);
	} else {
		if(IS_AFFECTED(ch, AFF_FLYING) || is_affected(ch, gsk_flight)) {
			send_to_char("You are already flying.\n\r", ch);
			return;
		}

		/* Trigger is responsible for messages! */
		if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TAKEOFF,NULL,0,0,0,0,0)) {
			chance = get_skill(ch, gsk_flight);

			if(!chance) {
				send_to_char("You have no knowledge of physical flight.\n\r",ch);
				return;
			}



			if(!IS_SET(ch->parts,PART_WINGS)) {
				send_to_char("You need wings to take flight.\n\r",ch);
				return;
			}

			if(number_range(0,ch->max_move/get_curr_stat(ch,STAT_CON)) > ch->move) {
				send_to_char("You are too exhausted to take flight.\n\r",ch);
				return;
			}

			weight = get_carry_weight(ch);
			if(RIDDEN(ch)) weight += get_carry_weight(RIDDEN(ch)) + size_weight[RIDDEN(ch)->size]; /* plus weight of rider */

			/* if the weight is too high, it uses more... */
			if(!IS_IMMORTAL(ch) && (number_range(0,can_carry_w(ch))) < weight) {
				send_to_char("Your weight is encumbering you too much.\n\r",ch);
				return;
			}

			if(chance < number_percent()) {
				send_to_char("You flap your wings in effort to take off but fail to generate lift.\n\r", ch);
				act("$n flaps $s wings in effort to take off but fails to generate lift.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				check_improve(ch,gsk_flight,FALSE,3);
				return;
			}

			send_to_char("You spread your wings, taking a few strokes in the air before taking off.\n\r", ch);
			act("$n spreads $s wings, taking a few strokes in the air before taking off.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
		af.where     = TO_AFFECTS;
		af.group     = AFFGROUP_PHYSICAL;
		af.skill      = gsk_flight;
		af.level     = ch->tot_level;
		af.duration  = -1;
		af.location  = APPLY_NONE;
		af.modifier  = 0;
		af.bitvector = AFF_FLYING;
		af.bitvector2 = 0;
		af.custom_name = NULL;
		af.slot	= WEAR_NONE;
		affect_to_char(ch, &af);
		check_improve(ch,gsk_flight,TRUE,3);
	}
}

void do_land(CHAR_DATA *ch, char *argument)
{
	if(MOUNTED(ch)) {
		if (!mobile_is_flying(MOUNTED(ch)))
		{
			act("$N doesn't seem to be airborne.", ch, MOUNTED(ch),NULL,NULL,NULL,NULL, NULL, TO_CHAR);
			return;
		}

		if (!is_affected(MOUNTED(ch), gsk_flight) && !is_affected(MOUNTED(ch), gsk_fly))
		{
			act("Something is keeping $N airborne.", ch, MOUNTED(ch),NULL,NULL,NULL,NULL, NULL, TO_CHAR);
			return;
		}

		if(!p_percent_trigger(MOUNTED(ch), NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LAND,NULL,0,0,0,0,0)) {
			if(is_affected(ch, gsk_flight)) {
				if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
					ch->in_room->sector_type == SECT_WATER_SWIM ||
					ch->in_room->sector_type == SECT_UNDERWATER ||
					ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
					act("Diving down, you descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("Diving down, $n descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				} else {
					act("Diving down, you descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("Diving down, $n descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
			} else {
				if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
					ch->in_room->sector_type == SECT_WATER_SWIM ||
					ch->in_room->sector_type == SECT_UNDERWATER ||
					ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
					act("You slowly descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n slowly descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				} else {
					act("You slowly descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n slowly descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
			}
		}

		affect_strip(MOUNTED(ch), gsk_flight);
		affect_strip(MOUNTED(ch), gsk_fly);
	} else {
		if (!mobile_is_flying(ch))
		{
			send_to_char("You don't appear to be airborne.\n\r", ch);
			return;
		}

		if (!is_affected(ch, gsk_flight) && !is_affected(ch, gsk_fly))
		{
			send_to_char("Something is keeping you airborne.\n\r", ch);
			return;
		}

		if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LAND,NULL,0,0,0,0,0)) {
			if(is_affected(ch, gsk_flight)) {
				if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
					ch->in_room->sector_type == SECT_WATER_SWIM ||
					ch->in_room->sector_type == SECT_UNDERWATER ||
					ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
					act("Diving down, you descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("Diving down, $n descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				} else {
					act("Diving down, you descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("Diving down, $n descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
			} else {
				if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
					ch->in_room->sector_type == SECT_WATER_SWIM ||
					ch->in_room->sector_type == SECT_UNDERWATER ||
					ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
					act("You slowly descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n slowly descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				} else {
					act("You slowly descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n slowly descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
			}
		}

		affect_strip(ch, gsk_flight);
		affect_strip(ch, gsk_fly);
	}
}
