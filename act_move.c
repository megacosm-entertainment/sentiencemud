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
#include "traits.h"
#include "skill_data.h"


/**
 * dir_name - String names for each direction
 *
 * Array indexed by DIR_* constants (0-9) providing human-readable
 * direction names. Used for display and parsing direction arguments.
 *
 * Index mapping:
 *   [0] = "north"      [5] = "down"
 *   [1] = "east"       [6] = "northeast"
 *   [2] = "south"      [7] = "northwest"
 *   [3] = "west"       [8] = "southeast"
 *   [4] = "up"         [9] = "southwest"
 */
char *	const	dir_name	[]		=
{
    "north", "east", "south", "west", "up", "down", "northeast",  "northwest", "southeast", "southwest"
};

/**
 * parse_door - Convert direction string to numeric direction
 *
 * @param name  Direction name to parse (e.g., "north", "east")
 *
 * @return Direction constant (0-9) if found, -1 if invalid direction
 */
int parse_door(char *name)
{
    int i;
    for(i = 0; i < MAX_DIR; i++) {
        if( !str_cmp(name, dir_name[i]) )
            return i;
    }

    return -1;
}


/**
 * rev_dir - Reverse direction lookup table
 *
 * Maps each direction to its opposite. Used when updating exits
 * on both sides of a door, or determining where a character came from.
 *
 * Mapping: north<->south, east<->west, up<->down,
 *          northeast<->southwest, northwest<->southeast
 */
const	int16_t	rev_dir		[]		=
{
    2, 3, 0, 1, 5, 4, 9, 8, 7, 6
};


/**
 * movement_loss - Movement point cost per sector type
 *
 * Array indexed by SECT_* constants defining how many movement points
 * are consumed when traveling through each terrain type. Higher values
 * indicate more difficult terrain.
 *
 * Cost ranges:
 *   1-2  = Easy (roads, inside, city, air)
 *   3-7  = Moderate (forest, hills, desert, cave)
 *   15-17 = Difficult (swimming, swamp, toxic bog)
 *   50   = Very difficult (deep water, lava, underwater)
 */
const	int16_t	movement_loss	[SECT_MAX]	=
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
    1,		/* SECT_PAVED_ROAD */
    1,		/* SECT_DIRT_ROAD */
};

/**
 * get_player_classnth - Count how many classes a player has
 *
 * @param ch  Player character to check
 *
 * @return Number of active classes (1-4), minimum 1
 */
int get_player_classnth(CHAR_DATA *ch)
{
    int num = 4;

    if(ch->pcdata->class_mage == -1) --num;
    if(ch->pcdata->class_cleric == -1) --num;
    if(ch->pcdata->class_thief == -1) --num;
    if(ch->pcdata->class_warrior == -1) --num;

    return UMAX(1,num);
}

/**
 * exit_destination - Calculate the destination room for an exit
 *
 * Resolves complex exit types including wilderness vlinks, dungeon floor
 * transitions (EX_PREVFLOOR/EX_NEXTFLOOR), and environment exits.
 * Creates wilderness virtual rooms on-demand if they don't exist.
 *
 * @param pexit  Exit data structure to resolve
 *
 * @return Destination room pointer, or NULL if exit is invalid/blocked
 */
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

/**
 * exit_destination_data - Populate destination data structure for an exit
 *
 * Similar to exit_destination() but populates a DESTINATION_DATA structure
 * instead of returning a room directly. Useful when destination may be
 * a wilderness location that doesn't have a vroom created yet.
 *
 * @param pexit  Exit data structure to resolve
 * @param pDest  Destination data structure to populate with result
 *
 * @return true if destination is valid, false if exit is invalid/blocked
 */
bool exit_destination_data(EXIT_DATA *pexit, DESTINATION_DATA *pDest)
{
    ROOM_INDEX_DATA *in_room = NULL;
    ROOM_INDEX_DATA *to_room = NULL;
    WILDS_DATA *in_wilds = NULL;
    WILDS_DATA *to_wilds = NULL;
    int to_vroom_x = 0;
    int to_vroom_y = 0;

    if(!pexit || !pexit->from_room || !pDest) {
        return false;
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
                    return false;
                }

                to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
                if( !to_room ) {
                    pDest->wilds = to_wilds;
                    pDest->wx = to_vroom_x;
                    pDest->wy = to_vroom_y;
                }


            /* Otherwise, Exit leads to a static room. */
            } else if (!(to_room = pexit->u1.to_room)) {
                return false;
            }
        } else {
            /* In wilds and exit leads to another vroom. */
            to_wilds = in_wilds;
            to_vroom_x = get_wilds_vroom_x_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);
            to_vroom_y = get_wilds_vroom_y_by_dir(in_wilds, in_room->x, in_room->y, pexit->orig_door);

            if (!check_for_bad_room(in_wilds, to_vroom_x, to_vroom_y)) {
                return false;
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
                return false;
            }

            to_room = get_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y);
            if(!to_room && !(to_room = create_wilds_vroom(to_wilds, to_vroom_x, to_vroom_y))) {
/*				wiznet("exit_destination()->NULL G",NULL,NULL,WIZ_TESTING,0,0); */
                return false;
            }

        } else if (IS_SET(pexit->exit_info, EX_ENVIRONMENT)) {
            if(!IS_SET(pexit->from_room->room_flag[1],ROOM_VIRTUAL_ROOM)) {
/*				wiznet("exit_destination()->NULL H",NULL,NULL,WIZ_TESTING,0,0); */
                return false;
            }

            if(!(to_room = get_environment(pexit->from_room))) {
/*				wiznet("exit_destination()->NULL I",NULL,NULL,WIZ_TESTING,0,0); */
                return false;
            }
        } else if (!(to_room = pexit->u1.to_room)) {
            return false;
        }
    }

    pDest->room = to_room;

    return true;
}

/**
 * move_char - Move a character in a given direction
 *
 * Core movement function handling all aspects of character movement:
 * - Validates exit exists and is accessible
 * - Checks movement restrictions (doors, terrain, flight, swimming)
 * - Calculates and deducts movement points
 * - Handles mount movement
 * - Displays leave/arrive messages
 * - Triggers TRIG_MOVE_CHAR, TRIG_EXIT, TRIG_ENTRY, TRIG_GREET scripts
 * - Checks environmental hazards (rocks, ice, flames, traps)
 * - Moves followers
 *
 * @param ch      Character attempting to move
 * @param door    Direction constant (DIR_NORTH, etc.)
 * @param follow  true if character is following someone (unused in function)
 *
 * Triggers: TRIG_MOVE_CHAR, TRIG_EXIT, TRIG_ENTRY, TRIG_GREET
 */
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
        pbugf(LOG_ERROR, "Bad door %d.", door);
        return;
    }

    /* Check char's in_room index pointer is valid */
    if (!ch->in_room) {
        pbugf(LOG_ERROR, "move_char: ch->in_room was null for %s (%ld)", HANDLE(ch), IS_NPC(ch) ? ch->pIndexData->vnum : 0);
        return;
    }

    // Allow scripting to screw with the direction of travel
    ch->tempstore[0] = door;
    if( p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MOVE_CHAR, dir_name[ch->tempstore[0]]) )
        return;

    ch->in_room->tempstore[0] = URANGE(0,ch->tempstore[0],(MAX_DIR-1));;
    if( p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_MOVE_CHAR, dir_name[ch->in_room->tempstore[0]]) )
        return;

    door = URANGE(0,ch->in_room->tempstore[0],(MAX_DIR-1));


    /* Exit trigger, if activated, bail out. Only PCs are triggered. */
    if (!IS_NPC(ch) && (p_exit_trigger(ch, door, PRG_MPROG) ||
            p_exit_trigger(ch, door, PRG_OPROG) || p_exit_trigger(ch, door, PRG_RPROG)))
        return;

    /* Check if char is "on" something. preventing movement */
    if (ch->on) {
        act("You must get off $p first.", ch, NULL, NULL, ch->on, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    /* Ok, take a copy of the char's location pointers */
    in_room = ch->in_room;

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
            act("You dive into the deep water and begin to swim.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n dives into the deep water and begins to swim.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }

        if (in_room->sector_type == SECT_WATER_NOSWIM && to_room->sector_type != SECT_WATER_NOSWIM &&
            !IS_AFFECTED(ch,AFF_FLYING)) {
            act("You can touch the ground here.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n stops swimming and stands.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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
            act("{W$n swims $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
        else if (PULLING_CART(ch))
            act("{W$n leaves $T, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
        else if (MOUNTED(ch)) {
            if(!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
                strcpy(buf, "{W$n leaves $T, riding on $N.{x");
            else
                strcpy(buf, "{W$n soars $T, on $N.{x");
            act(buf, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
        } else {
            if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
                act("{W$n stumbles off drunkenly on $s way $T.{x", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM, NULL, NULL);
            else
                act("{W$n leaves $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
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
        act("$n stops reciting.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        ch->recite = 0;
    }

    char_from_room(ch);

    /* VIZZWILDS */
    if (to_room->wilds)
        char_to_vroom (ch, to_room->wilds, to_room->x, to_room->y);
    else
        char_to_room(ch, to_room);

    DUNGEON *in_dungeon = get_room_dungeon(in_room);
    DUNGEON *to_dungeon = get_room_dungeon(to_room);

    INSTANCE *in_instance = get_room_instance(in_room);
    INSTANCE *to_instance = get_room_instance(to_room);

    if (!IS_AFFECTED(ch, AFF_SNEAK) && ch->invis_level < LEVEL_HERO) {
        if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
        {
            OBJ_DATA *portal = get_room_dungeon_portal(to_room, in_dungeon->index->vnum);

            if( IS_VALID(portal) )
            {
                if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
                {
                    act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, portal, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
                }
                else
                {
                    act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                }
            }
            else if(MOUNTED(ch))
            {
                if( !IS_NULLSTR(in_dungeon->index->zone_out_mount) )
                    act(in_dungeon->index->zone_out_mount, ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                else

                    act("{W$n materializes, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
            else
            {
                if( !IS_NULLSTR(in_dungeon->index->zone_out) )
                    act(in_dungeon->index->zone_out, ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
                else
                    act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
        }
        else if (in_room->sector_type == SECT_WATER_NOSWIM)
            act("{W$n swims in.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
        else if (PULLING_CART(ch))
            act("{W$n has arrived, pulling $p.{x", ch, NULL, NULL, PULLING_CART(ch), NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        else if(!MOUNTED(ch)) {
            if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
                act("{W$n stumbles in drunkenly.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            else
                act("{W$n has arrived.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        } else {
            if (!IS_AFFECTED(MOUNTED(ch), AFF_FLYING))
                act("{W$n has arrived, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            else
            act("{W$n soars in, riding on $N.{x", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
    }

    move_cart(ch,to_room,true);

    /* fry vampires*/
    if (IS_OUTSIDE(ch) && race_get_trait_bool(ch->race, "sunlight_vulnerability") && number_percent() < 75)
        hurt_vampires(ch);

    do_function(ch, &do_look, "auto");

    check_see_hidden(ch);

    if (!IS_WILDERNESS(ch->in_room))
        check_traps(ch, true);

    if (in_room == to_room) /* no circular following */
        return;

    for (fch = in_room->people; fch != NULL; fch = fch_next) {
        fch_next = fch->next_in_room;

        if (fch->master == ch && IS_AFFECTED(fch,AFF_CHARM) && fch->position < POS_STANDING)
            do_function(fch, &do_stand, "");

        if (fch->master == ch && fch->position == POS_STANDING && can_see_room(fch,to_room)) {
            act("{WYou follow $N.{x", fch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            move_char(fch, door, true);
        }
    }

    if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
    {
        p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL);
    }

    if( IS_VALID(to_instance) && to_instance != in_instance )
    {
        p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL);
    }

    /*
    * If someone is following the char, these triggers get activated
    * for the followers before the char, but it's safer this way...
    */
    p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL);

    if (!IS_NPC(ch)) {
        p_greet_trigger(ch, PRG_MPROG);
        p_greet_trigger(ch, PRG_OPROG);
        p_greet_trigger(ch, PRG_RPROG);
    }

    if (!IS_DEAD(ch)) check_rocks(ch, true);
    if (!IS_DEAD(ch)) check_ice(ch, true);
    if (!IS_DEAD(ch)) check_room_flames(ch, true);
    if (!IS_DEAD(ch)) check_ambush(ch);

    /* Enable this?
      if (IS_SET(ch->in_room->room_flag[1], ROOM_POST_OFFICE))
          check_new_mail(ch);*/

    if (MOUNTED(ch) && number_percent() == 1 && get_skill(ch, gsn_riding) > 0)
        check_improve(ch, gsn_riding, true, 8);

    if (!MOUNTED(ch) && get_skill(ch, gsn_trackless_step) > 0 && number_percent() == 1)
        check_improve(ch, gsn_trackless_step, true, 8);

    /* Nature regen: regenerate in nature */
    if (ch_has_trait(ch, "nature_regen") && is_in_nature(ch)) {
        ch->move += number_range(1,3);
        ch->move = UMIN(ch->move, ch->max_move);
        ch->hit += number_range(1,3);
        ch->hit  = UMIN(ch->hit, ch->max_hit);
    }

    if (!IS_NPC(ch))
        check_quest_rescue_mob(ch, true);

}

/**
 * check_ambush - Check if any ambushers in the room should attack
 *
 * Called when a character enters a room. Scans all characters in the room
 * for active ambushes and triggers them if the entering character matches
 * the ambush criteria (PC/NPC type and level range).
 *
 * @param ch  Character who just entered the room
 *
 * Planned refactor: combat/hidden.c
 */
void check_ambush(CHAR_DATA *ch)
{
    CHAR_DATA *ach;
    AMBUSH_DATA *ambush;
    char command[MSL];

    if (ch->in_room == NULL)
    {
    pbugf(LOG_ERROR, "For %s (%ld), in_room was null!",
        IS_NPC(ch) ? ch->short_descr : ch->name,
        IS_NPC(ch) ? ch->pIndexData->vnum : 0);
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

            if (number_percent() > get_skill(ach, gsn_ambush))
        {
        ach->position = POS_STANDING;
        act("You jump out of nowhere but $N notices you!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("You notice $n jump out of nowhere!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("$n jumps out of nowhere trying to surprise $N, but fails!", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        check_improve(ach, gsn_ambush, 4, false);
        continue;
        }

        ach->position = POS_STANDING;

        sprintf(command, "%s %s", ambush->command, ch->name);
        act("{RYou jump out of nowhere, surprising $N!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{R$n jumps out of nowhere, surprising you!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("{R$n jumps out of nowhere, surprising $N!{x", ach, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        interpret(ach, command);

        /* end the ambush */
        free_ambush(ach->ambush);
        ach->ambush = NULL;

        check_improve(ach, gsn_ambush, 4, true);
    }
    }
}


/**
 * check_rocks - Check for falling rocks hazard in ROOM_ROCKS rooms
 *
 * In rooms flagged ROOM_ROCKS, there's a 75% chance of falling rocks.
 * Each character in the room has a 50% chance of being struck for
 * bash damage scaled to their level. Flying characters are immune.
 *
 * @param ch    Character whose room to check
 * @param show  If true, display damage messages
 *
 * @return true if character died from rocks, false otherwise
 */
bool check_rocks(CHAR_DATA *ch, bool show)
{
    CHAR_DATA *vch, *vch_next;

    if (ch->in_room == NULL)
        return false;

    if (IS_SET(ch->in_room->room_flag[0], ROOM_ROCKS) &&
        number_percent() < 75 &&
        !IS_DEAD(ch) &&
        !IS_AFFECTED(ch, AFF_FLYING))
    {
        if( show )
        {
            act("{yLoose rocks fall from the ceiling!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        }

        for (vch = ch->in_room->people; vch != NULL; vch = vch_next)
        {
            vch_next = vch->next_in_room;

            if (number_percent() < 50)
            {
                if( show )
                {
                    act("{RYou are struck by a rock!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("{R$n is struck by a rock!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
                vch->set_death_type = DEATHTYPE_ROCKS;
                damage(vch, vch, number_range(vch->tot_level * 4, vch->tot_level * 10), 0, DAM_BASH, false);
            }
        }
    }

    if ((!IS_NPC(ch) && IS_DEAD(ch)) ||
        ( IS_NPC(ch) && ch->in_room == NULL))
        return true;

    return false;
}


/**
 * check_ice - Check for slipping on icy surfaces
 *
 * In rooms flagged ROOM_ICY or affected by ice storm, non-flying
 * characters may slip and fall. Chance is based on DEX and level.
 * Falling causes bash damage and puts character in resting position.
 *
 * @param ch    Character to check
 * @param show  If true, display slip/fall messages
 *
 * @return true if character slipped, false otherwise
 */
bool check_ice(CHAR_DATA *ch, bool show)
{
    if (ch->in_room == NULL)
    return false;

    if (IS_IMMORTAL(ch))
    return false;

    if (!IS_SET(ch->in_room->room_flag[1], ROOM_ICY)
    && check_ice_storm(ch->in_room) == false)
    return false;

    if (number_percent() > get_curr_stat(ch, STAT_DEX) * 2 + ch->tot_level / 20 && !IS_AFFECTED(ch, AFF_FLYING))
    {
        if( show )
        {
            act("{xYou slip on the icy ground and fall down!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("{x$n slips on the icy ground and falls down!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        damage(ch, ch, UMIN(ch->max_hit/2, 50), 0, DAM_BASH, false);
        ch->position = POS_RESTING;
        return true;
    }

    return false;
}


/**
 * check_room_flames - Check for ITEM_ROOM_FLAME damage in room
 *
 * Applies fire damage from ITEM_ROOM_FLAME objects. Only damages
 * characters in PK/CPK/Arena rooms, PKers, or NPCs. Non-PK players
 * pass through unharmed. Has 10% chance to blind with smoke.
 *
 * @param ch    Character to check
 * @param show  If true, display damage messages
 *
 * @return true if character died from flames, false otherwise
 */
bool check_room_flames(CHAR_DATA *ch, bool show)
{
    OBJ_DATA *obj;

    if (ch->in_room == NULL)
        return false;

    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
    {
        if (obj->item_type == ITEM_ROOM_FLAME)
        {
            if (!IS_DEAD(ch) &&
                (IS_SET(ch->in_room->room_flag[0], ROOM_PK) ||
                    IS_SET(ch->in_room->room_flag[0], ROOM_CPK) ||
                    IS_SET(ch->in_room->room_flag[0], ROOM_ARENA) ||
                    is_pk(ch) || IS_NPC(ch)))
            {
                if( show )
                {
                    act("{RYou are scorched by flames!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("{R$n is scorched by flames!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
                damage(ch, ch, number_range(50, 500), TYPE_UNDEFINED, DAM_FIRE, false);

                if (!IS_DEAD(ch) && number_percent() < 10)
                {
                    AFFECT_DATA af;
                    memset(&af,0,sizeof(af));

                    if( show )
                    {
                        act("{DYou are blinded by smoke!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        act("{D$n is blinded by smoke!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    }
                    af.where     = TO_AFFECTS;
                    af.group     = AFFGROUP_PHYSICAL;
                    af.type      = gsn_blindness;
    af.skill = skill_from_sn(af.type);
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
                act("{RYou pass through the scorching inferno unscathed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("{R$n passes through the scorching inferno unscathed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
        }
    }

    if ((!IS_NPC(ch) && IS_DEAD(ch)) || ch->in_room == NULL)
        return true;

    return false;
}


/**
 * check_room_shield_source - Display message when passing through room shield
 *
 * Shows a message when character passes through an ITEM_ROOM_ROOMSHIELD
 * object in the room. This is purely cosmetic - the shield blocking
 * logic is handled in can_move_room().
 *
 * @param ch    Character passing through
 * @param show  If true, display the shield passage message
 */
void check_room_shield_source(CHAR_DATA *ch, bool show)
{
    OBJ_DATA *obj;

    if( show )
    {
        for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
            if (obj->item_type == ITEM_ROOM_ROOMSHIELD)
            {
                act("{YYou pass through the shimmering shield.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("{Y$n passes through the shimmering shield.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
    }
}

/**
 * can_move_room - Check if character can move to a destination room
 *
 * Validates all conditions that might prevent movement:
 * - Closed/locked doors, barred exits
 * - Web/ensnare effects
 * - Charmed characters can't leave master
 * - Private rooms, password-protected chatrooms
 * - Room shields in PK areas
 * - NO_MOB flag for NPCs
 * - Mount position requirements
 * - Cart pulling weight checks
 * - Arena exit handling
 * - Ice slipping, mail in progress
 *
 * @param ch    Character attempting to move
 * @param door  Direction of movement
 * @param room  Destination room
 *
 * @return true if movement allowed, false if blocked
 *
 * Triggers: TRIG_PREENTER
 */
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
            pbugf(LOG_ERROR, "Room was null in can_move_room, ch vnum is %ld", ch->pIndexData->vnum);
        else {
            pbugf(LOG_ERROR, "Room was null in can_move_room, char was %s", ch->name);
        }
        return false;
    }

    exit_name(in_room, door, exit);

    pexit = in_room->exit[door];
    if (IS_SET(pexit->exit_info, EX_CLOSED)	&& !(IS_IMMORTAL(ch) && !IS_NPC(ch)) &&
        (!IS_AFFECTED(ch, AFF_PASS_DOOR) || IS_SET(pexit->exit_info,EX_NOPASS))) {
        if (IS_AFFECTED(ch, AFF_PASS_DOOR))
            act("Something blocks you from passing through $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        else {
            act("$T is closed.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        }

        return false;
    }

    /* If the exit is flagged aerial, then without flight, you will need some kind of climbing gear to bypass this. */
    if (IS_SET(pexit->exit_info, EX_AERIAL) && !mobile_is_flying(ch)) {
        send_to_char("Alas, you cannot move that way.\n\r", ch);
        return false;
    }

    if ((is_room_pk(room, true) || is_pk(ch)) &&
        !(IS_IMMORTAL(ch) && !IS_NPC(ch))) {
        for (obj = room->contents; obj != NULL; obj = obj->next_content) {
            if (obj->item_type == ITEM_ROOM_ROOMSHIELD) {
                act("{YYou try to move $T, but run into an invisible wall!{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR, NULL, NULL);
                act("{Y$n tries to move $T, but runs into an invisible wall!{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
                sprintf(buf, "{YYou hear a {R***BONK***{Y as someone runs into the shield from the %s!{x\n\r", dir_name[rev_dir[door]]);
                room_echo(room, buf);
                return false;
            }
        }
    }

    /* Syn - why the hell wasn't this ever here in the first place? */
    if (IS_NPC(ch) && IS_SET(room->room_flag[0], ROOM_NO_MOB))
        return false;

    if (IS_AFFECTED(ch, AFF_WEB)) {
        send_to_char("{RDespite your attempts to move, the webs hold you in place.{x\n\r", ch);
        return false;
    }

    if (IS_AFFECTED2(ch, AFF2_ENSNARE)) {
        send_to_char("The vines clutching your body prevent you from moving!{x\n\r", ch);
        act("$n attempts to move, but the vines hold $m in place!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return false;
    }

    if (IS_SET(pexit->exit_info, EX_CLOSED) && IS_SET(pexit->exit_info, EX_BARRED)) {
        act("$T is barred shut.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        return false;
    }

    if (IS_AFFECTED(ch, AFF_CHARM) && ch->master && in_room == ch->master->in_room) {
        send_to_char("You don't have the freedom to do that.\n\r", ch);
        return false;
    }

    if (!is_room_owner(ch,room) && room_is_private(room, ch)) {
        send_to_char("That room is private right now.\n\r", ch);
        return false;
    }

    if(!is_room_unlocked(ch, room) )
    {
        if(p_percent2_trigger(room->area, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREENTER, dir_name[door]))
            send_to_char("You cannot enter that place yet.\n\r", ch);

        return false;
    }


    if(p_percent_trigger(NULL, NULL, room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREENTER, dir_name[door]))
        return false;

    if (MOUNTED(ch) && (MOUNTED(ch)->position < POS_FIGHTING)) {
        send_to_char("Your mount must be standing.\n\r", ch);
        return false;
    }

    if ( ch->pulled_cart )
    {
        CHAR_DATA *mount = MOUNTED(ch);

        if (mount)
        {
            if (ch->pulled_cart->value[2] > get_curr_stat(mount, STAT_STR)) {
                act("$N isn't strong enough to pull $p.", ch, mount, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N struggles to pull $p but $Z too weak.", ch, mount, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_NOTVICT, NULL, get_verb_form(mount, "is", "are"));
                return false;
            }
        }
        else
        {
            if (ch->pulled_cart->value[2] > get_curr_stat(ch, STAT_STR)) {
                act("You aren't strong enough to pull $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n attempts to pull $p but $Z too weak.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM, get_verb_form(ch, "is", "are"), NULL);
                return false;
            }
        }
    }


    /* Cant move into rooms with passwords */
    if (room->chat_room && room->chat_room->password && str_cmp(room->chat_room->password, "none") &&
        !(IS_IMMORTAL(ch) && !IS_NPC(ch))) {
        sprintf(buf, "#%s is password protected.\n\rUse /join <chatroom> <password>.\n\r", room->chat_room->name);
        send_to_char(buf, ch);
        return false;
    }

    if (!str_cmp(ch->in_room->area->name, "Arena") && str_cmp(room->area->name, "Arena") && !IS_NPC(ch) && location_isset(&ch->pcdata->room_before_arena)) {
        ROOM_INDEX_DATA *to_room;

        if ((to_room = location_to_room(&ch->pcdata->room_before_arena))) {
            act("{YA swirl of colours surrounds you as you travel back to $t.{x", ch, NULL, NULL, NULL, NULL, to_room->name, NULL, TO_CHAR, NULL, NULL);
            act("{YA swirl of colours surrounds $n as $e $z.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, get_verb_form(ch, "vanishes","vanish"), NULL);
            char_from_room(ch);
            char_to_room(ch, to_room);
            location_clear(&ch->pcdata->room_before_arena);
            do_function(ch, &do_look, "auto");
        } else {
            location_clear(&ch->pcdata->room_before_arena);
            return true;
        }

        return false;
    }

    if (check_ice(ch, true))
        return false;

    if (ch->mail) {
        send_to_char("You must finish your mail and either send it or cancel it before you leave the post office.\n\r", ch);
        return false;
    }

    return true;
}

/**
 * drunk_walk - Handle drunk character movement failures
 *
 * When a drunk character tries to move in an invalid direction,
 * displays humorous failure messages based on drunkenness level.
 * May cause character to fall down or walk into walls.
 *
 * @param ch    Drunk character attempting movement
 * @param door  Direction they tried to move
 *
 * @note Currently not called - commented out in move_char()
 */
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
                    act("You drunkenly slam face-first into the 'exit' on your way $T.", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR, NULL, NULL);
                    act("$n drunkenly slams face-first into the 'exit' on $s way $T.", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
                    /* damage(ch, ch, 3, 0, DAM_BASH , false); */
                } else {
                    act("You drunkenly face-first into the 'exit' on your way $T. WHAM!", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR, NULL, NULL);
                    act("$n slams face-first into the 'exit' on $s way $T. WHAM!", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
                    /* damage(ch, ch, 3, 0, DAM_BASH, false); */
                }
            } else {
                if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10) {
                    act("You stumble about aimlessly and fall down drunk.", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_CHAR, NULL, NULL);
                    act("$n stumbles about aimlessly and falls down drunk.", ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM, NULL, NULL);
                    ch->position = POS_RESTING;
                } else {
                    act("You almost go $T, but suddenly realize that there's no exit there.",ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_CHAR, NULL, NULL);
                    act("$n looks like $e's about to go $T, but suddenly stops short and looks confused.",ch, NULL, NULL, NULL, NULL,NULL,dir_name[door],TO_ROOM, NULL, NULL);
                }
            }
        } else
            send_to_char("Alas, you cannot go that way.\n\r", ch);
    }
}


/**
 * do_search - Search for hidden exits and items
 *
 * Player command to search for hidden content:
 * - No argument: Search room for hidden exits (EX_HIDDEN) and hidden items
 * - "self": Search inventory for hidden items
 * - <object>: Search inside a container for hidden items
 *
 * Hidden exits found are marked with EX_FOUND flag. Hidden items have
 * ITEM_HIDDEN flag removed when discovered. Discovery has 60-90% chance.
 *
 * @param ch        Character performing search
 * @param argument  Optional: "self" or container name to search
 */
void do_search(CHAR_DATA *ch, char *argument)
{
    char buf[2*MAX_STRING_LENGTH];
    char exit[MSL];
    EXIT_DATA *pexit;
    EXIT_DATA *pexit_rev;
    bool found = false;
    int door = 0;
    OBJ_DATA *obj;

    plogf(LOG_INFO, "%s searched in %s (%ld) (argument=%s)", ch->name,
        ch->in_room->name, ch->in_room->vnum, (IS_NULLSTR(argument)?"(nothing)":argument));

    if( IS_NULLSTR(argument) )
    {
        act("You start searching the room for hidden exits and items...", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
        {
            if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90))
            {
                REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
                act("$n has uncovered $p!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                act("You have uncovered $p!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                found = true;
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
                found = true;
                SET_BIT(pexit->exit_info, EX_FOUND);
                SET_BIT(pexit_rev->exit_info, EX_FOUND);
            }
        }
    } else if( !str_cmp(argument, "self") ) {

        act("You start searching your inventory...", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

ITERATOR it;
OBJ_DATA *obj;
iterator_start(&it, ch->lcarrying);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90)) {
        REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
        act("You have uncovered $p{x!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        found = true;
    }
}
iterator_stop(&it);

    } else {
        OBJ_DATA *container;

        if( (container = get_obj_here(ch, NULL, argument)) == NULL )
        {
            act("You see no $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        act("You start searching $p{x...", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        for (obj = container->contains; obj != NULL; obj = obj->next_content)
        {
            if (IS_SET(obj->extra[0], ITEM_HIDDEN) && number_percent() < number_range(60, 90))
            {
                REMOVE_BIT(obj->extra[0], ITEM_HIDDEN);
                act("You have uncovered $p{x inside $P{x!", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                found = true;
            }
        }
    }

    if (!found)
        act("You find nothing unusual.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
}

/** @brief Move character north */
void do_north(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTH, false);
    return;
}

/** @brief Move character east */
void do_east(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_EAST, false);
    return;
}

/** @brief Move character south */
void do_south(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTH, false);
    return;
}

/** @brief Move character west */
void do_west(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_WEST, false);
    return;
}

/** @brief Move character northeast */
void do_northeast(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTHEAST, false);
    return;
}

/** @brief Move character northwest */
void do_northwest(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_NORTHWEST, false);
    return;
}

/** @brief Move character southeast */
void do_southeast(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTHEAST, false);
    return;
}

/** @brief Move character southwest */
void do_southwest(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_SOUTHWEST, false);
    return;
}

/** @brief Move character up */
void do_up(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_UP, false);
    return;
}


/** @brief Move character down */
void do_down(CHAR_DATA *ch, char *argument)
{
    move_char(ch, DIR_DOWN, false);
    return;
}


/**
 * find_door - Find a door by direction name or keyword
 *
 * Parses direction argument (n/north, e/east, etc.) or door keyword.
 * Validates the exit exists and is a door (EX_ISDOOR flag).
 *
 * @param ch    Character looking for the door (can be NULL for direction-only)
 * @param arg   Direction name, abbreviation, or door keyword
 * @param show  If true, display error messages to character
 *
 * @return Direction constant (0-9) if found, -1 if not found
 */
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
                act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR, NULL, NULL);

            return -1;
        }
    }

    if(ch) {
        if ((pexit = ch->in_room->exit[door]) == NULL) {
            if (show)
                act("I see no door $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR, NULL, NULL);
            return -1;
        }

        if(IS_SET(pexit->exit_info, EX_HIDDEN) && !IS_SET(pexit->exit_info, EX_FOUND) && IS_SET(pexit->exit_info, EX_MUSTSEE) )
        {
            if(!IS_IMMORTAL(ch) || !IS_SET(ch->act[0],PLR_HOLYLIGHT))
            {
                if (show)
                    act("I see no door $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg, TO_CHAR, NULL, NULL);
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

/**
 * do_open - Open a door, container, portal, or book
 *
 * Opens the specified target if it's closed and unlocked.
 * Automatically unlocks first if player has the key.
 * Updates both sides of room exits.
 *
 * @param ch        Character performing the action
 * @param argument  Target to open (direction, object name)
 *
 * Triggers: TRIG_OPEN (for objects)
 */
void do_open(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char exit[MSL];
    OBJ_DATA *obj, *key = NULL;
    int door;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Open what?\n\r", ch);
        return;
    }

    /* Open object */
    if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
    {
        /* portal stuff */
        if (obj->item_type == ITEM_PORTAL)
        {
            if (!IS_SET(obj->value[1],EX_CLOSED))
            {
                send_to_char("It's already open.\n\r",ch);
                return;
            }

            if( obj->lock )
            {
                if (IS_SET(obj->lock->flags, LOCK_LOCKED))
                {
                    if ((key = lockstate_getkey(ch,obj->lock)) != NULL)
                    {
                        do_function(ch, &do_unlock, obj->name);
                        do_function(ch, &do_open, obj->name);
                        return;
                    }

                    send_to_char("It's locked.\n\r", ch);
                    return;
                }
            }

            REMOVE_BIT(obj->value[1],EX_CLOSED);
            act("You open $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n opens $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
            return;
        }

        if (obj->item_type != ITEM_CONTAINER && obj->item_type != ITEM_BOOK)
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }

        if (!IS_SET(obj->value[1], CONT_CLOSED))
        {
            send_to_char("It's already open.\n\r", ch);
            return;
        }
        if (!IS_SET(obj->value[1], CONT_CLOSEABLE))
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }
        /* @@@NIB : 20070126 */
        if (IS_SET(obj->value[1], CONT_PUSHOPEN))
        {
            send_to_char("You need to push it open.\n\r", ch);
            return;
        }
        if( obj->lock )
        {
            if (IS_SET(obj->lock->flags, LOCK_LOCKED))
            {
                if ((key = lockstate_getkey(ch, obj->lock)) != NULL)
                {
                    do_function(ch, &do_unlock, obj->name);
                    do_function(ch, &do_open, obj->name);
                    return;
                }

                send_to_char("It's locked.\n\r", ch);
                return;
            }
        }

        REMOVE_BIT(obj->value[1], CONT_CLOSED);
        act("You open $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, ch, NULL, NULL, TRIG_OPEN, NULL);

        return;
    }

    /* Open door */
    if ((door = find_door(ch, arg, true)) >= 0)
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
        act("$n opens $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM, NULL, NULL);
        act("You open $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_OPEN);

        if ((to_room   = pexit->u1.to_room) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
            pexit_rev->u1.to_room == ch->in_room)
        {
            CHAR_DATA *rch;

            exit_name(to_room, rev_dir[door], exit);

            REMOVE_BIT(pexit_rev->exit_info, EX_CLOSED);
            for (rch = to_room->people; rch != NULL; rch = rch->next_in_room)
                act("$T opens.", rch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        }
    }
}


/**
 * do_close - Close a door, container, portal, or book
 *
 * Closes the specified target if it's open and closeable.
 * Handles CONT_CLOSELOCK containers that auto-lock when closed.
 * Updates both sides of room exits.
 *
 * @param ch        Character performing the action
 * @param argument  Target to close (direction, object name)
 *
 * Triggers: TRIG_CLOSE (for objects)
 */
void do_close(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char exit[MSL];
    OBJ_DATA *obj;
    int door;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Close what?\n\r", ch);
        return;
    }

    if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
    {
        /* portal stuff */
        if (obj->item_type == ITEM_PORTAL)
        {
            if (!IS_SET(obj->value[1],EX_ISDOOR) ||
                IS_SET(obj->value[1],EX_NOCLOSE))
            {
                send_to_char("You can't do that.\n\r",ch);
                return;
            }

            if (IS_SET(obj->value[1],EX_CLOSED))
            {
                send_to_char("It's already closed.\n\r",ch);
                return;
            }

            if (IS_SET(obj->value[1],EX_BROKEN))
            {
                send_to_char("That door has been destroyed. It cannot be closed.\n\r",ch);
                return;
            }

            SET_BIT(obj->value[1],EX_CLOSED);
            act("You close $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n closes $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
            return;
        }

        /* 'close object' */
        if (obj->item_type != ITEM_CONTAINER && obj->item_type != ITEM_BOOK)
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }
        if (IS_SET(obj->value[1], CONT_CLOSED))
        {
            send_to_char("It's already closed.\n\r", ch);
            return;
        }
        if (!IS_SET(obj->value[1], CONT_CLOSEABLE))
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }

        SET_BIT(obj->value[1], CONT_CLOSED);
        act("You close $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n closes $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        if (obj->lock && IS_SET(obj->value[1], CONT_CLOSELOCK))
        {
            act("$p locks once closed.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL, NULL, NULL);
            SET_BIT(obj->lock->flags, LOCK_LOCKED);
        }
        p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CLOSE, NULL);
        return;
    }

    if ((door = find_door(ch, arg, true)) >= 0)
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
        act("$n closes $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM, NULL, NULL);
        act("You close $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_CLOSE);

        /* close the other side */
        if ((to_room   = pexit->u1.to_room) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != 0 &&
            pexit_rev->u1.to_room == ch->in_room)
        {
            CHAR_DATA *rch;

            exit_name(to_room, rev_dir[door], exit);

            SET_BIT(pexit_rev->exit_info, EX_CLOSED);
            for (rch = to_room->people; rch != NULL; rch = rch->next_in_room)
                act("The $T closes.", rch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        }
    }

    return;
}


/**
 * get_key - Find a key by vnum in character's inventory or keyrings
 *
 * Searches character's inventory for a key matching the specified vnum.
 * Also searches inside any ITEM_KEYRING objects the character carries.
 *
 * @param ch    Character to search
 * @param vnum  Object vnum of the key to find
 *
 * @return Pointer to key object if found, NULL otherwise
 */
OBJ_DATA *get_key(CHAR_DATA *ch, int vnum)
{
    OBJ_DATA *obj;
    OBJ_DATA *key;

    if( vnum < 1 ) return NULL;

ITERATOR it;

// Search inventory for the key directly
iterator_start(&it, ch->lcarrying);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    if (obj->pIndexData->vnum == vnum)
    {
        iterator_stop(&it);
        return obj;
    }
}
iterator_stop(&it);

// Search keyrings in inventory
iterator_start(&it, ch->lcarrying);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    if (obj->pIndexData->item_type == ITEM_KEYRING)
    {
        for (key = obj->contains; key != NULL; key = key->next_content)
        {
            if (key->pIndexData->vnum == vnum)
            {
                iterator_stop(&it);
                return key;
            }
        }
    }
}
iterator_stop(&it);

return NULL;

}


/**
 * use_key - Apply wear from using a key
 *
 * Called after a key is used to lock/unlock something.
 * Handles:
 * - Church temple keys: Destroyed if used by non-church member
 * - Key fragility: May degrade condition based on fragility level
 * - Key breaking: Extracts key if condition reaches 0
 *
 * @param ch   Character who used the key
 * @param key  Key object that was used
 */
void use_key(CHAR_DATA *ch, OBJ_DATA *key)
{
    CHURCH_DATA *church;
    char buf[MSL];

    if (ch == NULL)
    {
        pbugf(LOG_ERROR, "ch was null", 0);
        return;
    }

    if (key == NULL)
    {
        pbugf(LOG_ERROR, "key was null", 0);
        return;
    }

    /* can only use a church-temple key if you're in that church */
ITERATOR it;
iterator_start(&it, list_churches);
while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
{
    if (church->key == key->pIndexData->vnum && ch->church != church)
    {
        sprintf(buf, "Rent by the spiritual powers of %s, $p dissipates into nothingness.\n\r", church->name);
        act(buf, ch, NULL, NULL, key, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act(buf, ch, NULL, NULL, key, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        extract_obj(key);
        iterator_stop(&it);
        return;
    }
}
iterator_stop(&it);

    switch (key->fragility)
    {
    case OBJ_FRAGILE_SOLID: break;
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

    if (key->condition <= 0)
    {
        act("$p snaps and breaks.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        extract_obj(key);
    }
}

/**
 * do_lock - Lock a door, container, or portal
 *
 * Locks the specified target using an appropriate key from inventory.
 * Checks for functional lock, key possession, and lock state (not broken/jammed).
 * Updates both sides of room exits.
 *
 * @param ch        Character performing the action
 * @param argument  Target to lock (direction, object name)
 */
void do_lock(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *key;
    int door;
    char exit[MSL];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Lock what?\n\r", ch);
        return;
    }

    if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
    {
        /* portal stuff */
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

            if (!lockstate_functional(obj->lock))
            {
                send_to_char("It can't be locked.\n\r",ch);
                return;
            }

            if ((key = lockstate_getkey(ch,obj->lock)) == NULL)
            {
                send_to_char("You lack the key.\n\r",ch);
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

            if (IS_SET(obj->lock->flags,LOCK_LOCKED))
            {
                send_to_char("It's already locked.\n\r",ch);
                return;
            }

            SET_BIT(obj->lock->flags,LOCK_LOCKED);
            act("You lock $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n locks $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_ROOM, NULL, NULL);

            use_key(ch, key);
            return;
        }

        /* 'lock object' */
        if (obj->item_type != ITEM_CONTAINER && obj->item_type != ITEM_BOOK)
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }
        if (!IS_SET(obj->value[1], CONT_CLOSED))
        {
            send_to_char("It's not closed.\n\r", ch);
            return;
        }
        if (!lockstate_functional(obj->lock))
        {
            send_to_char("It can't be locked.\n\r",ch);
            return;
        }
        if ((key = lockstate_getkey(ch,obj->lock)) == NULL)
        {
            send_to_char("You lack the key.\n\r",ch);
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

        if (IS_SET(obj->lock->flags, LOCK_LOCKED))
        {
            send_to_char("It's already locked.\n\r", ch);
            return;
        }

        SET_BIT(obj->lock->flags, LOCK_LOCKED);
        act("You lock $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n locks $p with $P.",ch, NULL, NULL,obj, key, NULL, NULL, TO_ROOM, NULL, NULL);

        use_key(ch, key);
        return;
    }

    if ((door = find_door(ch, arg, true)) >= 0)
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

        SET_BIT(pexit->door.lock.flags, LOCK_LOCKED);
        /* send_to_char("*Click*\n\r", ch); */
        exit_name(ch->in_room, door, exit);
        act("You lock $T with $p.", ch, NULL, NULL, key, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        act("$n locks $T with $p.", ch, NULL, NULL, key, NULL, NULL, exit, TO_ROOM, NULL, NULL);

        /* lock the other side */
        if ((to_room   = pexit->u1.to_room) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != 0 &&
            pexit_rev->u1.to_room == ch->in_room)
            SET_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);

        use_key(ch, key);
    }
}


/**
 * do_unlock - Unlock a door, container, or portal
 *
 * Unlocks the specified target using an appropriate key from inventory.
 * Checks for functional lock, key possession, and lock state.
 * LOCK_SNAPKEY locks destroy the key after unlocking.
 * Updates both sides of room exits.
 *
 * @param ch        Character performing the action
 * @param argument  Target to unlock (direction, object name)
 */
void do_unlock(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char exit[MSL];
    OBJ_DATA *obj;
    OBJ_DATA *key;
    int door;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Unlock what?\n\r", ch);
        return;
    }

    if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
    {
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
                send_to_char("It can't be locked.\n\r",ch);
                return;
            }

            if ((key = lockstate_getkey(ch,obj->lock)) == NULL)
            {
                send_to_char("You lack the key.\n\r",ch);
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

            if (!IS_SET(obj->lock->flags,LOCK_LOCKED))
            {
                send_to_char("It's already unlocked.\n\r",ch);
                return;
            }

            REMOVE_BIT(obj->lock->flags,LOCK_LOCKED);
            act("You unlock $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n unlocks $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_ROOM, NULL, NULL);
            use_key(ch, key);
            return;
        }

        /* 'unlock object' */
        if (obj->item_type != ITEM_CONTAINER && obj->item_type != ITEM_BOOK)
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }
        if (!IS_SET(obj->value[1], CONT_CLOSED))
        {
            send_to_char("It's not closed.\n\r", ch);
            return;
        }
        if (!lockstate_functional(obj->lock))
        {
            send_to_char("It can't be locked.\n\r",ch);
            return;
        }

        if ((key = lockstate_getkey(ch,obj->lock)) == NULL)
        {
            send_to_char("You lack the key.\n\r",ch);
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
        if (!IS_SET(obj->lock->flags,LOCK_LOCKED))
        {
            send_to_char("It's already unlocked.\n\r",ch);
            return;
        }

        REMOVE_BIT(obj->lock->flags,LOCK_LOCKED);
        act("You unlock $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n unlocks $p with $P.",ch, NULL, NULL,obj, key, NULL,NULL, TO_ROOM, NULL, NULL);

        if (key && !is_name("house",key->name))
        {
            --key->condition;

            if (IS_SET(obj->lock->flags,LOCK_SNAPKEY) || key->condition <= 0)
            {
                act("$p snaps and breaks in the lock.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$p snaps and breaks in the lock.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                extract_obj(key);
            }
        }
        return;
    }

    if ((door = find_door(ch, arg, true)) >= 0)
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

        REMOVE_BIT(pexit->door.lock.flags, LOCK_LOCKED);
        /* send_to_char("*Click*\n\r", ch); */
        exit_name(ch->in_room, door, exit);
        act("You unlock $T with $p.", ch, NULL, NULL, key, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        act("$n unlocks $T with $p.", ch, NULL, NULL, key, NULL, NULL, exit, TO_ROOM, NULL, NULL);

        /* unlock the other side */
        if ((to_room   = pexit->u1.to_room) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
            pexit_rev->u1.to_room == ch->in_room)
            REMOVE_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);

        use_key(ch, key);
    }
}


/**
 * do_pick - Attempt to pick a lock without a key
 *
 * Uses gsn_pick_lock skill to unlock doors, containers, and portals.
 * Highwaymen automatically succeed. Others have chance based on skill
 * and lock's pick_chance value. Cannot pick broken or jammed locks.
 *
 * @param ch        Character attempting to pick (players only)
 * @param argument  Target lock to pick (direction, object name)
 */
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

    WAIT_STATE(ch, skill_table[gsn_pick_lock].beats);

    if (!ch_has_trait(ch, "lockpick_mastery"))
    {
        int skill = get_skill(ch,gsn_pick_lock);

        if (number_percent() > UMAX(skill, 20))
        {
            send_to_char("You failed.\n\r", ch);
            check_improve(ch,gsn_pick_lock,false,2);
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
            act("You pick the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n picks the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
            check_improve(ch,gsn_pick_lock,true,2);
            return;
        }

        /* 'pick object' */
        if (obj->item_type != ITEM_CONTAINER && obj->item_type != ITEM_BOOK)
        {
            send_to_char("That's not a container.\n\r", ch);
            return;
        }
        if (!IS_SET(obj->value[1], CONT_CLOSED))
        {
            send_to_char("It's not closed.\n\r", ch);
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
        act("You pick the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n picks the lock on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        check_improve(ch,gsn_pick_lock,true,2);
        return;
    }

    if ((door = find_door(ch, arg, true)) >= 0)
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
        act("$n picks the $d.", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM, NULL, NULL);
        check_improve(ch,gsn_pick_lock,true,2);

        /* pick the other side */
        if ((to_room   = pexit->u1.to_room ) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
            pexit_rev->u1.to_room == ch->in_room)
        {
            REMOVE_BIT(pexit_rev->door.lock.flags, LOCK_LOCKED);
        }
    }
}

/**
 * do_stand - Stand up from sitting, resting, or sleeping
 *
 * Changes character position to standing. Can optionally specify
 * furniture to stand at/on/in. Wakes sleeping characters.
 * Clears bashed state.
 *
 * @param ch        Character to stand
 * @param argument  Optional furniture name to stand at
 */
void do_stand(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;

    /* "stand <x>" */
    if (argument[0] != '\0')
    {
    if (ch->position == POS_FIGHTING)
    {
        send_to_char("Maybe you should finish fighting first?\n\r",ch);
        return;
    }

    if ((obj = get_obj_list(ch, argument, ch->in_room->contents)) == NULL)
    {
        send_to_char("You don't see that here.\n\r",ch);
        return;
    }

    if (obj->item_type != ITEM_FURNITURE
    ||  (!IS_SET(obj->value[2],STAND_AT)
    &&   !IS_SET(obj->value[2],STAND_ON)
    &&   !IS_SET(obj->value[2],STAND_IN)))
    {
        send_to_char("You can't seem to find a place to stand.\n\r",ch);
        return;
    }

    if (ch->on != obj && count_users(obj) >= obj->value[0])
    {
        act_new("There's no room to stand on $p.", ch,NULL,NULL, NULL, NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
        return;
    }

     ch->on = obj;
    if (IS_SET(obj->value[2],STAND_AT))
    {
        act("You stand at $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n stands at $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else if (IS_SET(obj->value[2],STAND_ON))
    {
        act("You stand on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n stands on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    else
    {
        act("You stand in $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n stands in $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }
    return;
    }

    switch (ch->position)
    {
    case POS_SLEEPING:
        if (IS_AFFECTED(ch, AFF_SLEEP))
        { send_to_char("You can't wake up!\n\r", ch); return; }

        if (obj == NULL)
        {
        send_to_char("You wake and stand up.\n\r", ch);
        act("$n wakes and stands up.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        ch->on = NULL;
        }
        else if (IS_SET(obj->value[2],STAND_AT))
        {
           act_new("You wake and stand at $p.",ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_DEAD,NULL);
           act("$n wakes and stands at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],STAND_ON))
        {
        act_new("You wake and stand on $p.",ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_DEAD,NULL);
        act("$n wakes and stands on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act_new("You wake and stand in $p.",ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_DEAD,NULL);
        act("$n wakes and stands in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }

        ch->position = POS_STANDING;
        do_function(ch, &do_look, "auto");
        break;

    case POS_RESTING:
    case POS_SITTING:
        if (obj == NULL)
        {
        send_to_char("You stand up.\n\r", ch);
        act("$n stands up.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        ch->on = NULL;
        }
        else if (IS_SET(obj->value[2],STAND_AT))
        {
        act("You stand at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n stands at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],STAND_ON))
        {
        act("You stand on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n stands on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act("You stand in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n stands on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }

        ch->position = ch->fighting == NULL ? POS_STANDING : POS_FIGHTING;
        if (ch->bashed > 0)
        ch->bashed = 0;
        break;

    case POS_STANDING:
        if (ch->on == NULL)
        send_to_char("You are already standing.\n\r", ch);
        else
        {
        if (IS_SET(ch->on->value[2],STAND_AT))
        {
            act("You get off of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n gets off of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(ch->on->value[2],STAND_ON))
        {
            act("You get off of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n gets off of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
            act("You get out of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n gets out of $p.",ch, NULL, NULL,ch->on, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }

        ch->on = NULL;
        }
        break;

    case POS_FIGHTING:
        send_to_char("You are already fighting!\n\r", ch);
        break;
    }
}


/**
 * do_rest - Rest to recover faster
 *
 * Changes character position to resting. Can optionally specify
 * furniture to rest at/on/in. Wakes sleeping characters to resting.
 * Cannot rest while mounted or being ridden.
 *
 * @param ch        Character to rest
 * @param argument  Optional furniture name to rest on
 *
 * Triggers: TRIG_SIT (for furniture)
 */
void do_rest(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;
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

    if (ch->position == POS_FIGHTING)
    {
    send_to_char("You are already fighting!\n\r",ch);
    return;
    }

    /* okay, now that we know we can rest, find an object to rest on */
    if (argument[0] != '\0')
    {
    obj = get_obj_list(ch,argument,ch->in_room->contents);
    if (obj == NULL)
    {
        send_to_char("You don't see that here.\n\r",ch);
        return;
    }
    }
    else
    obj = ch->on;

    if (obj != NULL)
    {
        if (obj->item_type != ITEM_FURNITURE
        ||  (!IS_SET(obj->value[2],REST_ON)
        &&   !IS_SET(obj->value[2],REST_IN)
        &&   !IS_SET(obj->value[2],REST_AT)))
        {
        send_to_char("You can't rest on that.\n\r",ch);
        return;
        }

        if (obj != NULL && ch->on != obj && count_users(obj) >= obj->value[0])
        {
        act_new("There's no more room on $p.",ch,NULL,NULL, NULL, NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
        return;
        }

    ch->on = obj;
    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL);
    }

    switch (ch->position)
    {
    case POS_SLEEPING:
        if (IS_AFFECTED(ch,AFF_SLEEP))
        {
        send_to_char("You can't wake up!\n\r",ch);
        return;
        }

        if (obj == NULL)
        {
        send_to_char("You wake up and start resting.\n\r", ch);
        act ("$n wakes up and starts resting.",ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_AT))
        {
        act_new("You wake up and rest at $p.", ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
        act("$n wakes up and rests at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_ON))
        {
        act_new("You wake up and rest on $p.", ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
        act("$n wakes up and rests on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act_new("You wake up and rest in $p.", ch, NULL, NULL, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR,POS_SLEEPING,NULL);
        act("$n wakes up and rests in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        ch->position = POS_RESTING;
        break;

    case POS_RESTING:
        send_to_char("You are already resting.\n\r", ch);
        break;

    case POS_STANDING:
        if (obj == NULL)
        {
        send_to_char("You rest.\n\r", ch);
        act("$n sits down and rests.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_AT))
        {
        act("You sit down at $p and rest.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n sits down at $p and rests.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_ON))
        {
        act("You sit on $p and rest.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n sits on $p and rests.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act("You rest in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n rests in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        ch->position = POS_RESTING;
        break;

    case POS_SITTING:
        if (obj == NULL)
        {
        send_to_char("You rest.\n\r",ch);
        act("$n rests.",ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_AT))
        {
        act("You rest at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n rests at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],REST_ON))
        {
        act("You rest on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n rests on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act("You rest in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n rests in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        ch->position = POS_RESTING;
        break;
    }
}


/**
 * do_sit - Sit down
 *
 * Changes character position to sitting. Can optionally specify
 * furniture to sit at/on/in. Wakes sleeping characters to sitting.
 * Cannot sit while mounted or being ridden.
 *
 * @param ch        Character to sit
 * @param argument  Optional furniture name to sit on
 *
 * Triggers: TRIG_SIT (for furniture)
 */
void do_sit (CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;

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

    if (ch->position == POS_FIGHTING)
    {
    send_to_char("Maybe you should finish this fight first?\n\r",ch);
    return;
    }

    /* okay, now that we know we can sit, find an object to sit on */
    if (argument[0] != '\0')
    {
    if ((obj = get_obj_list(ch,argument,ch->in_room->contents)) == NULL)
    {
        send_to_char("You don't see that here.\n\r",ch);
        return;
    }
    }
    else
    obj = ch->on;

    if (obj != NULL)
    {
    if (obj->item_type != ITEM_FURNITURE
    ||  (!IS_SET(obj->value[2],SIT_ON)
    &&   !IS_SET(obj->value[2],SIT_IN)
    &&   !IS_SET(obj->value[2],SIT_AT)))
    {
        send_to_char("You can't sit on that.\n\r",ch);
        return;
    }

    if (obj != NULL && ch->on != obj && count_users(obj) >= obj->value[0])
    {
        act_new("There's no more room on $p.",ch,NULL,NULL, NULL, NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
        return;
    }

    ch->on = obj;
    }

    switch (ch->position)
    {
    case POS_SLEEPING:
        if (IS_AFFECTED(ch,AFF_SLEEP))
        {
        send_to_char("You can't wake up!\n\r",ch);
        return;
        }

            if (obj == NULL)
            {
                send_to_char("You wake and sit up.\n\r", ch);
                act("$n wakes and sits up.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
            else if (IS_SET(obj->value[2],SIT_AT))
            {
                act_new("You wake and sit at $p.",ch, NULL, NULL, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR,POS_DEAD,NULL);
                act("$n wakes and sits at $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_ROOM, NULL, NULL);
            }
            else if (IS_SET(obj->value[2],SIT_ON))
            {
                act_new("You wake and sit on $p.",ch, NULL, NULL, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR,POS_DEAD,NULL);
                act("$n wakes and sits at $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_ROOM, NULL, NULL);
            }
            else
            {
                act_new("You wake and sit in $p.",ch, NULL, NULL, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR,POS_DEAD,NULL);
                act("$n wakes and sits in $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_ROOM, NULL, NULL);
            }

        ch->position = POS_SITTING;
        break;
    case POS_RESTING:
        if (obj == NULL)
        send_to_char("You stop resting.\n\r",ch);
        else if (IS_SET(obj->value[2],SIT_AT))
        {
        act("You sit at $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR, NULL, NULL);
        act("$n sits at $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_ROOM, NULL, NULL);
        }

        else if (IS_SET(obj->value[2],SIT_ON))
        {
        act("You sit on $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR, NULL, NULL);
        act("$n sits on $p.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_ROOM, NULL, NULL);
        }
        ch->position = POS_SITTING;
        break;
    case POS_SITTING:
        send_to_char("You are already sitting down.\n\r",ch);
        break;
    case POS_STANDING:
        if (obj == NULL)
            {
        send_to_char("You sit down.\n\r",ch);
                act("$n sits down on the ground.",ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],SIT_AT))
        {
        act("You sit down at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n sits down at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],SIT_ON))
        {
        act("You sit on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n sits on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
        act("You sit down in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n sits down in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
            ch->position = POS_SITTING;
            break;
    }

    if (obj) p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL);
}


/**
 * do_sleep - Go to sleep for maximum recovery
 *
 * Changes character position to sleeping. Can optionally specify
 * furniture to sleep at/on/in. Cannot sleep while mounted, being
 * ridden, or fighting.
 *
 * @param ch        Character to sleep
 * @param argument  Optional furniture name to sleep on
 *
 * Triggers: TRIG_SIT (for furniture)
 */
void do_sleep(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj = NULL;

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

    switch (ch->position)
    {
    case POS_SLEEPING:
        send_to_char("You are already sleeping.\n\r", ch);
        break;

    case POS_RESTING:
    case POS_SITTING:
    case POS_STANDING:
        if (argument[0] == '\0' && ch->on == NULL)
        {
        send_to_char("You go to sleep.\n\r", ch);
        act("$n goes to sleep.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        ch->position = POS_SLEEPING;
        }
        else  /* find an object and sleep on it */
        {
        if (argument[0] == '\0')
            obj = ch->on;
        else
            obj = get_obj_list(ch, argument,  ch->in_room->contents);

        if (obj == NULL)
        {
            send_to_char("You don't see that here.\n\r",ch);
            return;
        }
        if (obj->item_type != ITEM_FURNITURE
        ||  (!IS_SET(obj->value[2],SLEEP_ON)
        &&   !IS_SET(obj->value[2],SLEEP_IN)
        &&	 !IS_SET(obj->value[2],SLEEP_AT)))
        {
            send_to_char("You can't sleep on that!\n\r",ch);
            return;
        }

        if (ch->on != obj && count_users(obj) >= obj->value[0])
        {
            act_new("There is no room on $p for you.", ch,NULL,NULL, NULL, NULL,obj,NULL,NULL,NULL,TO_CHAR,POS_DEAD,NULL);
            return;
        }

        ch->on = obj;
        p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SIT, NULL);
        if (IS_SET(obj->value[2],SLEEP_AT))
        {
            act("You go to sleep at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n goes to sleep at $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else if (IS_SET(obj->value[2],SLEEP_ON))
        {
            act("You go to sleep on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n goes to sleep on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        else
        {
            act("You go to sleep in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n goes to sleep in $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        }
        ch->position = POS_SLEEPING;
        }
        break;

    case POS_FIGHTING:
        send_to_char("You are already fighting!\n\r", ch);
        break;
    }
}


/**
 * do_wake - Wake up self or another character
 *
 * With no argument, wakes self (calls do_stand). With argument,
 * attempts to wake the named character. Cannot wake characters
 * affected by magical sleep or with PLR_NO_WAKE flag.
 *
 * @param ch        Character doing the waking
 * @param argument  Optional name of character to wake
 */
void do_wake(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    { do_function(ch, &do_stand, ""); return; }

    if (!IS_AWAKE(ch))
    { send_to_char("You are asleep yourself!\n\r",       ch); return; }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    { send_to_char("They aren't here.\n\r",              ch); return; }

    if (IS_AWAKE(victim))
    { act("$N is already awake.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL); return; }

    if (!IS_IMMORTAL(ch) && (IS_AFFECTED(victim, AFF_SLEEP) || (!IS_NPC(victim) && IS_SET(victim->act[1], PLR_NO_WAKE))))
    { act("You can't wake $M!",   ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);  return; }

    act_new("$n wakes you.", ch, victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TO_VICT,POS_SLEEPING,NULL);
    do_function(victim, &do_stand, "");
}


/**
 * do_sneak - Attempt to move silently
 *
 * Activates sneak mode using gsn_sneak skill. Sneaking characters
 * don't show leave/arrive messages when moving. For immortals,
 * can also be used as a teleport command with location argument.
 *
 * @param ch        Character attempting to sneak
 * @param argument  For immortals: optional location to teleport to
 */
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
    affect_strip(ch, gsn_sneak);

    if (IS_AFFECTED(ch,AFF_SNEAK))
    return;

memset(&af,0,sizeof(af));

    if (number_percent() < get_skill(ch,gsn_sneak))
    {
    check_improve(ch,gsn_sneak,true,3);
    af.where     = TO_AFFECTS;
    af.group     = AFFGROUP_PHYSICAL;
    af.type      = gsn_sneak;
    af.skill = skill_from_sn(af.type);
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
    check_improve(ch,gsn_sneak,false,3);
        send_to_char("You fail to move silently.\n\r", ch);
    }
}


/**
 * do_hide - Hide self or an object
 *
 * Multi-purpose hide command:
 * - No argument: Hide self in shadows (uses gsn_hide skill)
 * - "hide <obj>": Hide object in the room (sector-specific messages)
 * - "hide <obj> in <container>": Hide object inside container
 * - "hide <obj> on <victim>": Plant object on another character
 *
 * Hidden characters get AFF_HIDE. Hidden objects get ITEM_HIDDEN.
 *
 * @param ch        Character hiding
 * @param argument  Optional: object to hide, or "self"
 *
 * Triggers: TRIG_PREHIDE, TRIG_PREHIDE_IN, TRIG_HIDE, TRIG_HIDDEN
 */
void do_hide(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;

    if (check_social_status(ch))
        return;

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

            if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT)) {
                send_to_char("You can't let go of it.\n\r", ch);
                return;
            }

            if( p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREHIDE, NULL) )
                return;

            if( !str_cmp(arg2, "in") )
            {
                // We are trying to hide something inside an object
                OBJ_DATA *container;

                if( IS_NULLSTR(argument) )
                {
                    act("Hide it in what?", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if( (container = get_obj_inv(ch, argument, false)) == NULL )
                {
                    act("You don't have that item.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if (!can_put_obj(ch, obj, container, NULL, false))
                    return;

                if( p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREHIDE_IN, NULL) )
                    return;

                obj_from_char(obj);
                obj_to_obj(obj, container);

                p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_HIDE, NULL);
            }
            else if( !str_cmp(arg2, "on") )
            {
                CHAR_DATA *victim;

                int sneak1;
                int sneak2;

                if( IS_NULLSTR(argument) )
                {
                    act("Hide it on whom?", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if( (victim = get_char_room(ch, NULL, argument)) == NULL )
                {
                    act("They aren't here.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if( victim == ch )
                {
                    send_to_char("You would you hide that on yourself?", ch);
                    return;
                }

                if( (victim->carry_number + get_obj_number(obj)) > can_carry_n(victim))
                {
                    act("$N can't carry that.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if( (get_carry_weight(victim) + get_obj_weight(obj)) > can_carry_w(victim))
                {
                    act("$N can't carry that.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                if( p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREHIDE_IN, NULL) )
                    return;

                act("You deftly hide $p on $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                // Do a skill test
                sneak1 = get_skill(ch, gsn_sneak) * ch->tot_level;
                if( IS_REMORT(ch) ) sneak1 = 3 * sneak1 / 2;	// 50% boost
                if( IS_SAGE(ch) ) sneak1 = 3 * sneak1 / 2;		// 50% boost
                if( number_percent() < get_skill(ch, gsn_deception) ) sneak1 *= 2;

                sneak2 = get_skill(victim, gsn_sneak) * victim->tot_level;
                if( IS_REMORT(victim) ) sneak2 = 3 * sneak2 / 2;	// 50% boost
                if( IS_SAGE(victim) ) sneak2 = 3 * sneak2 / 2;		// 50% boost
                if( number_percent() < get_skill(victim, gsn_deception) ) sneak2 *= 2;

                // Check if victim is awake or if the victim is immortal and hider is not
                if( IS_AWAKE(victim) || (IS_IMMORTAL(victim) && !IS_IMMORTAL(ch)) )
                {
                    // Check if hider's sneak score is weaker and if the hider is a player or the victim is immortal
                    if( (sneak1 < sneak2) && (!IS_IMMORTAL(ch) || IS_IMMORTAL(victim)) )
                        act("$n hides something on you.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                }

                obj_from_char(obj);
                obj_to_char(obj,victim);

                p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_HIDE, NULL);
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

                act("You deftly hide $p $t.", ch, NULL, NULL, obj, NULL, buf2, NULL, TO_CHAR, NULL, NULL);
                for (others = ch->in_room->people;
                      others != NULL; others = others->next_in_room)
                {
                    if (((get_skill(ch, gsn_deception > 0) && number_percent() < get_skill(ch, gsn_deception)) ||
                        ( number_percent() < get_skill(ch, gsn_hide))) &&
                        ch != others &&
                        can_see_obj(others, obj))
                {
                    act("You notice $N hide $p $t.", others, ch, NULL, obj, NULL, buf2, NULL, TO_CHAR, NULL, NULL);
                }
                }
                obj_from_char(obj);
                obj_to_room(obj, ch->in_room);

                p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_HIDE, NULL);
            }
            SET_BIT(obj->extra[0], ITEM_HIDDEN);
            return;
        }
        else
        {
            act("You don't have that item.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

    if( p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREHIDE, NULL) )
        return;

    send_to_char("You attempt to hide.\n\r", ch);

    HIDE_STATE(ch, skill_table[gsn_hide].beats);
    return;
}


/**
 * hide_end - Complete the hide attempt after delay
 *
 * Called after HIDE_STATE delay expires. Rolls against gsn_hide skill
 * to determine success. Characters with gsn_deception may notice
 * the hiding attempt.
 *
 * @param ch  Character completing hide attempt
 *
 * Triggers: TRIG_HIDDEN (on success)
 */
void hide_end(CHAR_DATA *ch)
{
    CHAR_DATA *rch;

    if (number_percent() < get_skill(ch,gsn_hide))
    {
        SET_BIT(ch->affected_by[0], AFF_HIDE);
        for (rch = ch->in_room->people; rch != NULL; rch = rch->next_in_room)
        {
            if (get_skill(rch, gsn_deception) > 0)
            {
                if (number_percent() < get_skill(rch, gsn_deception))
                {
                    act("{D$n hides in the shadows.{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                    check_improve(rch, gsn_deception, true, 1);
                }
            }
        }

        send_to_char("You successfully hide in the shadows.\n\r{x", ch);
        check_improve(ch,gsn_hide,true,3);

        // Allow for other fun stuff to occur when you are fully hidden
        p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_HIDDEN, NULL);
    }
    else
    {
        send_to_char("You fail to hide in the shadows.\n\r", ch);
        check_improve(ch,gsn_hide,false,3);
    }
}

/**
 * do_visible - Remove all invisibility and stealth effects
 *
 * Strips all forms of invisibility: invis, mass invis, sneak,
 * improved invis, cloak of guile, and hide.
 *
 * @param ch        Character becoming visible
 * @param argument  Unused
 */
void do_visible(CHAR_DATA *ch, char *argument)
{
    affect_strip (ch, gsn_invis			);
    affect_strip (ch, gsn_mass_invis			);
    affect_strip (ch, gsn_sneak			);
    affect_strip (ch, skill_lookup("improved invisibility"));
    affect_strip (ch, skill_lookup("cloak of guile"));
    REMOVE_BIT   (ch->affected_by[0], AFF_HIDE		);
    REMOVE_BIT   (ch->affected_by[0], AFF_INVISIBLE	);
    REMOVE_BIT   (ch->affected_by[0], AFF_SNEAK		);
    REMOVE_BIT   (ch->affected_by[1], AFF2_IMPROVED_INVIS);
    REMOVE_BIT   (ch->affected_by[1], AFF2_CLOAK_OF_GUILE);
    send_to_char("You reveal yourself.\n\r", ch);
}

/**
 * do_recall - Teleport to recall point (low-level only)
 *
 * Prayers-based teleport to character's recall location. Only works
 * for players level 30 or below. Blocked by cursed status, ROOM_NO_RECALL,
 * AREA_NO_RECALL, or being in combat. Costs half movement points.
 * Also brings pet and mount.
 *
 * @param ch        Player attempting recall
 * @param argument  Unused
 *
 * Triggers: TRIG_PRERECALL (on self and room)
 */
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

    if(p_percent_trigger(ch, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL,NULL) ||
    p_percent_trigger(NULL, NULL, ch->in_room, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_PRERECALL,NULL))
    return;


    act("$n prays for transportation!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    if (!(location = get_recall_room(ch, false))) {
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
    act("{D$n disappears.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(ch);
    char_to_room(ch, location);
    act("{D$n appears in the room.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    do_function(ch, &do_look, "auto");


    if (ch->pet && !ch->pet->fighting) {
    ch->pet->move /= 2;
    act("{D$n disappears.{x", ch->pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(ch->pet);
    char_to_room(ch->pet, location);
    act("{D$n appears in the room.{x", ch->pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    do_function(ch->pet, &do_look, "auto");
    }
    if (ch->mount && !ch->mount->fighting) {
    ch->mount->move /= 2;
    act("{D$n disappears.{x", ch->mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    char_from_room(ch->mount);
    char_to_room(ch->mount, location);
    act("{D$n appears in the room.{x", ch->mount, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    do_function(ch->mount, &do_look, "auto");
    }
}

/**
 * do_fade - Dimensional fade movement (remort skill)
 *
 * Uses gsn_fade skill to phase through dimensions, moving multiple
 * rooms in the specified direction without triggering normal movement
 * checks. Blocked in AREA_NO_FADING areas, water, and social areas.
 * Cannot fade while fighting or pulling a cart.
 *
 * @param ch        Character using fade
 * @param argument  Direction to fade (n, e, s, w, etc.)
 */
void do_fade(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    int door;
    int counter;

    argument = one_argument(argument, arg);

    if (!IS_VALID(ch))
    {
        pbugf(LOG_ERROR, "invalid ch.");
        return;
    }

    if (get_skill(ch, gsn_fade) == 0)
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
    act("You can't fade while pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

    act("{W$n fades to a different dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_ROOM, NULL, NULL);
    act("{WYou fade to a different dimension.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[door], TO_CHAR, NULL, NULL);
    check_improve(ch,gsn_fade,true,1);
}


/**
 * fade_end - Complete the fade movement after initial delay
 *
 * Called after FADE_STATE delay expires. Moves character multiple
 * rooms based on skill level (3-5 rooms). Higher skill reduces
 * delay between fades.
 *
 * @param ch  Character completing fade movement
 */
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
        skill = get_skill(ch,gsn_fade);

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
            act("{W$n fades in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            ch->fade_dir = -1;	/* @@@NIB : 20071020 */
            ch->force_fading = 0;
            return;
        } else if (counter != 2)
            act("{W$n fades in then off to the $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dir_name[ch->fade_dir], TO_ROOM, NULL, NULL);
    }

    do_function(ch, &do_look, "auto");
    FADE_STATE(ch, beats);
    act("{W$n fades in.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}


/**
 * move_success - Attempt a single fade movement step
 *
 * Helper function for fade_end(). Moves character one room in
 * their fade_dir direction. Checks for valid exits, closed doors,
 * pass door ability, and room visibility.
 *
 * @param ch  Character being moved
 *
 * @return true if movement succeeded, false if blocked
 */
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
        act("Magical interference stops your ability to fade.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    if (ch->in_room && (ch->in_room->sector_type == SECT_WATER_NOSWIM ||
        ch->in_room->sector_type == SECT_WATER_SWIM)) {
        act("Magical interference stops your ability to fade.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return false;
    }

    if (!IS_NPC(ch) && (p_exit_trigger(ch, door, PRG_MPROG) || p_exit_trigger(ch, door, PRG_OPROG) || p_exit_trigger(ch, door, PRG_RPROG)))
        return false;

    if (!(pexit = in_room->exit[door])) {
        //Updated show_room_to_char to show_room -- Tieryo 08/18/2010
        show_room(ch,ch->in_room,false,false,false);
            act("\n\rYou can't go any further.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return false;
    }

    if (IS_SET(pexit->exit_info, EX_CLOSED) &&
        (!IS_AFFECTED(ch, AFF_PASS_DOOR) || IS_SET(pexit->exit_info,EX_NOPASS))) {
        //Updated show_room_to_char to show_room --Tieryo 08/18/2010
        show_room(ch,ch->in_room,false,false,false);
            act("\n\rYou can't go any further.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return false;
    }

    if(!(to_room = exit_destination(pexit))) {
        send_to_char ("Alas, you cannot go that way.\n\r", ch);
        return false;
    }

    if(!is_room_unlocked(ch, to_room) )
    {
        send_to_char("You cannot enter that place yet.\n\r", ch);
        return false;
    }

    if(!can_see_room (ch, to_room)) {
        send_to_char ("Alas, you cannot go that way.\n\r", ch);
        return false;
    }

    char_from_room(ch);
    /* VIZZWILDS */
    if (to_wilds)
        char_to_vroom (ch, to_wilds, to_vroom_x, to_vroom_y);
    else
        char_to_room(ch, to_room);

    return true;
}

/**
 * do_project - Cosmic projection for angel/demon remorts (INCOMPLETE)
 *
 * Work-in-progress feature for angel and demon remort characters.
 * Would allow projection to the mortal realm from afterlife states.
 * - Angels: Project to room 11051 (pillar)
 * - Demons: Project to room 11022 (crack in ground)
 * Both reset all affects and set stats to 50%.
 *
 * @param ch        Angel or demon character
 * @param argument  Unused
 *
 * @note COMMENTED OUT - incomplete implementation
 */
/*  Project for remorts - work in progress
void do_project(CHAR_DATA *ch, char *argument)
{
    if (!IS_DEMON(ch) && !IS_ANGEL(ch))
    {
    send_to_char("You attempt to project yourself.\n\r", ch);
    return;
    }

    if (IS_DEAD(ch) && (IS_ANGEL(ch) || IS_DEMON(ch)))
    {
    send_to_char("You lack the energy to project yourself.\n\r", ch);
    return;
    }

    if (IS_SET(ch->in_room->room_flag[0],ROOM_NO_RECALL) ||
    IS_AFFECTED(ch,AFF_CURSE)
    || IS_SET(ch->in_room->area->area_flags, AREA_NO_RECALL))
    {
    send_to_char("You cannot seem to project yourself.\n\r",ch);
    return;
    }

    if (IS_ANGEL(ch))
    {
        send_to_char("You focus your mind, and cosmically project yourself to the mortal realm.\n\r", ch);
        act("{W$n vanishes in a puff of smoke.{x", ch, NULL, NULL, TO_ROOM);

        // Reset affects
        while (ch->affected)
            affect_remove(ch, ch->affected);
        ch->affected_by[0] = ch->race ? ch->race->aff[0] : 0;
        ch->affected_by[1] = 0;

    char_from_room(ch);
        char_to_room(ch, get_room_index(11051));
        act("{W$n mysteriously walks out from behind a pillar.{x", ch, NULL, NULL, TO_ROOM);
        ch->position = POS_RESTING;
        ch->hit = ch->max_hit / 2;
        ch->mana = ch->max_mana / 2;
        ch->move = ch->max_move / 2;
    }

    if (IS_DEMON(ch))
    {
        send_to_char("You focus your mind, and cosmically project yourself to the mortal realm.\n\r", ch);
        act("{R$n vanishes in a puff of smoke.{x", ch, NULL, NULL, TO_ROOM);

    // Reset affects
        while (ch->affected)
            affect_remove(ch, ch->affected);
        ch->affected_by[0] = ch->race ? ch->race->aff[0] : 0;
        ch->affected_by[1] = 0;

    char_from_room(ch);
        char_to_room(ch, get_room_index(11022));
        send_to_char("You emerge from a large crack in the ground which closes up instantly.\n\r", ch);
    act("{RA large crack in the ground opens up and $n emerges from it's hot depths.{x\n\r", ch, NULL, NULL, TO_ROOM);
        ch->position = POS_RESTING;
        ch->hit = ch->max_hit / 2;
        ch->mana = ch->max_mana / 2;
        ch->move = ch->max_move / 2;
    }
} */

/**
 * do_bar - Bar a door or portal shut
 *
 * Uses gsn_bar skill to place a bar across a closed door or portal,
 * preventing it from being opened. Door must be closed and not
 * already barred or flagged EX_NOBAR. Updates both sides of room exits.
 *
 * @param ch        Character barring the door
 * @param argument  Direction or portal name to bar
 */
void do_bar(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char exit[MSL];
    OBJ_DATA *obj;
    int door;

    one_argument(argument, arg);

    if (get_skill(ch, gsn_bar) == 0)
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
                act("You can't find a way to bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
                return;
            }

            SET_BIT(obj->value[1],EX_BARRED);
            act("You bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            act("$n bars up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
            check_improve(ch, gsn_bar, true, 1);
            return;
        }

        act("You can't bar up the $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        return;
    }

    if ((door = find_door(ch, arg, true)) >= 0)
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
        act("You bar up the $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
        act("$n bars the $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM, NULL, NULL);
        check_improve(ch, gsn_bar, true, 1);

        /* bar the other side */
        if ((to_room   = pexit->u1.to_room) != NULL &&
            (pexit_rev = to_room->exit[rev_dir[door]]) != NULL &&
            pexit_rev->u1.to_room == ch->in_room)
        {
            SET_BIT(pexit_rev->exit_info, EX_BARRED);
        }
    }
}

/**
 * do_jam - Jam a lock (STUB - not implemented)
 *
 * Placeholder for lock jamming skill. Would set LOCK_JAMMED flag
 * to prevent the lock from being opened or locked.
 *
 * @param ch        Character attempting to jam
 * @param argument  Target lock
 *
 * @note NOT IMPLEMENTED - empty stub function
 */
void do_jam(CHAR_DATA *ch, char *argument)
{

}

/**
 * do_evasion - Activate evasion defensive stance
 *
 * Uses gsn_evasion skill to gain AFF2_EVASION effect with +3 DEX bonus.
 * Duration based on total level. Cannot use while mounted or already
 * affected.
 *
 * @param ch        Character using evasion
 * @param argument  Unused
 */
void do_evasion(CHAR_DATA *ch, char *argument)
{
    AFFECT_DATA af;

    if (get_skill(ch, gsn_evasion) == 0)
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
    if (number_percent() < get_skill(ch,gsn_evasion))
    {
    af.where     = TO_AFFECTS;
    af.group     = AFFGROUP_PHYSICAL;
    af.type      = gsn_evasion;
    af.skill = skill_from_sn(af.type);
    af.level     = ch->tot_level;
    af.duration  = ch->tot_level/3;
    af.location  = APPLY_DEX;
    af.modifier  = 3;
    af.bitvector = 0;
        af.bitvector2 = AFF2_EVASION;
        af.slot	= WEAR_NONE;
    affect_to_char(ch, &af);
        send_to_char("You shroud yourself in your cloak, prepared to be evasive.\n\r", ch);
    check_improve(ch,gsn_evasion,true,3);
    }
    else
    {
        send_to_char("You fail to be any more evasive.\n\r", ch);
    check_improve(ch,gsn_evasion,false,3);
    }
}


/**
 * do_warp - Placeholder command (joke/easter egg)
 *
 * @param ch        Character
 * @param argument  Unused
 */
void do_warp(CHAR_DATA *ch, char *argument)
{
    send_to_char("Warp speed! NOW!!!\n\r", ch);
    return;
}


/**
 * check_see_hidden - Alert if magic item detects hidden exit
 *
 * Called when entering a room. If character wears an ITEM_SEE_HIDDEN
 * item and the room has undiscovered hidden exits, the item vibrates
 * and hums to alert the player.
 *
 * @param ch  Character to check
 */
void check_see_hidden(CHAR_DATA *ch)
{
    OBJ_DATA *obj;

ITERATOR it;
iterator_start(&it, ch->lworn);
while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
    if (IS_SET(obj->extra[1], ITEM_SEE_HIDDEN)) {
        break;
    }
}
iterator_stop(&it);

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
                act("{Y$p{Y begins to vibrate and hum.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("{Y$n's $p{Y begins to vibrate and hum.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                return;
            }
        }
    }
}

/**
 * check_traps - Detect traps in room using gsn_detect_traps
 *
 * Called when entering a room. Checks visible exits for ROOM_DEATH_TRAP
 * destinations and visible objects for ITEM_TRAPPED flag. Success chance
 * based on gsn_detect_traps skill level.
 *
 * @param ch    Character to check traps for
 * @param show  If true, display warning messages
 */
void check_traps(CHAR_DATA *ch, bool show)
{
    EXIT_DATA *exit;
    OBJ_DATA *obj;
    int i;

    if (ch == NULL)
    {
        pbugf(LOG_ERROR, "checked traps for null ch!");
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
            number_percent() < get_skill(ch, gsn_detect_traps))
        {
            if( show )
                act("{RYou sense a strong feeling of danger coming from the $t.{x", ch, NULL, NULL, NULL, NULL, dir_name[i], NULL, TO_CHAR, NULL, NULL);

            if (number_percent() < 5)
                check_improve_show(ch, gsn_detect_traps, true, 5, show);
        }
    }

    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
    {
        if (!IS_SET(obj->extra[0], ITEM_HIDDEN) &&
            IS_SET(obj->extra[1], ITEM_TRAPPED) &&
            number_percent() < get_skill(ch, gsn_detect_traps))
        {
            if ( show )
                act("{RYou sense a strong feeling of danger coming from $p.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

            if (number_percent() < 5)
                check_improve_show(ch, gsn_detect_traps, true, 5, show);
        }
    }
}

/**
 * do_ambush - Set up an ambush to attack entering characters
 *
 * Uses gsn_ambush skill to set up an ambush that triggers when matching
 * characters enter the room. Can filter by PC/NPC/all and level range.
 * Syntax: ambush <pc|npc|all> <min level> <max level> <command>
 *         ambush stop
 *
 * The command is executed against the victim when ambush triggers.
 *
 * @param ch        Character setting up ambush
 * @param argument  Ambush parameters or "stop"
 */
void do_ambush(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char arg2[MSL];
    char arg3[MSL];
    AMBUSH_DATA *ambush;
    int min;
    int max;
    int type;

    if (get_skill(ch, gsn_ambush) == 0)
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
    act("You find a good place to hide and crouch down.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n finds a good place to hide and crouches down.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}

/**
 * do_pk - Toggle player-killer status
 *
 * Allows players to toggle PLR_PK flag at guildmasters for 5000 pneuma.
 * Requirements: level 31+ (or remort), at a guildmaster, not in PK church.
 * Sets personal_pk_question flag for confirmation prompt.
 *
 * @param ch        Player toggling PK status
 * @param argument  Unused
 */
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

    if (ch->church != NULL && ch->church->pk == true)
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
    ch->personal_pk_question = true;
    }
    else
    {
    send_to_char("{RAre you SURE you want toggle on PK? The cost is 5000 pneuma.{x\n\r", ch);
    ch->personal_pk_question = true;
    }
}

/**
 * do_knock - Knock on a closed door
 *
 * Knocks on a closed door, notifying characters on the other side.
 * The door must be closed.
 *
 * @param ch        Character knocking
 * @param argument  Direction of door to knock on
 *
 * Triggers: TRIG_KNOCK (this side), TRIG_KNOCKING (other side)
 */
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
    if((door = find_door(ch, arg, true)) >= 0) {
    ROOM_INDEX_DATA *to_room;
    EXIT_DATA *pexit;
    EXIT_DATA *pexit_rev;

    exit_name(ch->in_room, door, exit);

    pexit = ch->in_room->exit[door];
    if (!IS_SET(pexit->exit_info, EX_CLOSED)) {
        send_to_char("It is not closed.\n\r", ch);
        return;
    }

    act("$n knocks on $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM, NULL, NULL);
    act("You knock on $T.", ch, NULL, NULL, NULL, NULL, NULL, exit, TO_CHAR, NULL, NULL);
    p_direction_trigger(ch, ch->in_room, door, PRG_RPROG, TRIG_KNOCK);

    if ((to_room = pexit->u1.to_room) != NULL
    && (pexit_rev = to_room->exit[rev_dir[door]]) != NULL
    && pexit_rev->u1.to_room == ch->in_room) {
        if(to_room->people) {
            exit_name(to_room, rev_dir[door], exit);
            act("Knocking comes from $T.", to_room->people, NULL, NULL, NULL, NULL, NULL, exit, TO_ROOM, NULL, NULL);
        }
        p_direction_trigger(ch, to_room, rev_dir[door], PRG_RPROG, TRIG_KNOCKING);
    }
    }
}

/**
 * do_takeoff - Launch into flight using wings
 *
 * Allows winged characters or mounted winged creatures to take flight.
 * Handles both character-initiated flight and mount-initiated flight.
 *
 * Requirements and checks:
 * - Must have PART_WINGS (unless trigger handles it)
 * - Must not already be flying (AFF_FLYING or gsn_flight)
 * - Must have sufficient movement points (based on max_move/CON ratio)
 * - Carry weight must not exceed capacity (mortals only)
 * - Skill check against gsn_flight (characters only)
 *
 * If mounted, the mount takes flight:
 * - Uses mount's wings and stats for all checks
 * - Weight includes rider's weight
 *
 * On success, applies AFF_FLYING via gsn_flight affect with permanent
 * duration (-1). Characters improve their flight skill on success/failure.
 *
 * @param ch        Character or mount taking flight
 * @param argument  Unused
 *
 * Triggers: TRIG_TAKEOFF
 */
void do_takeoff(CHAR_DATA *ch, char *argument)
{
    int chance;
    int weight;
    AFFECT_DATA af;

    if(MOUNTED(ch)) {
        if(IS_AFFECTED(MOUNTED(ch), AFF_FLYING) || is_affected(MOUNTED(ch),gsn_flight)) {
            act("$N is already flying.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        if(!p_percent_trigger(MOUNTED(ch), NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TAKEOFF,NULL)) {
            if(!IS_SET(MOUNTED(ch)->parts,PART_WINGS)) {
                act("$N doesn't seem to be able to fly.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                return;
            }

            if(number_range(0,MOUNTED(ch)->max_move/get_curr_stat(MOUNTED(ch),STAT_CON)) > MOUNTED(ch)->move) {
                act("$N appears too exhausted to take flight.", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("$n spreads $s wings, taking a few strokes in the air before taking off.", MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        af.where     = TO_AFFECTS;
        af.group     = AFFGROUP_PHYSICAL;
        af.type      = gsn_flight;
    af.skill = skill_from_sn(af.type);
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
        if(IS_AFFECTED(ch, AFF_FLYING) || is_affected(ch,gsn_flight)) {
            send_to_char("You are already flying.\n\r", ch);
            return;
        }

        /* Trigger is responsible for messages! */
        if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TAKEOFF,NULL)) {
            chance = get_skill(ch,gsn_flight);

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
                act("$n flaps $s wings in effort to take off but fails to generate lift.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                check_improve(ch,gsn_flight,false,3);
                return;
            }

            send_to_char("You spread your wings, taking a few strokes in the air before taking off.\n\r", ch);
            act("$n spreads $s wings, taking a few strokes in the air before taking off.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        af.where     = TO_AFFECTS;
        af.group     = AFFGROUP_PHYSICAL;
        af.type      = gsn_flight;
    af.skill = skill_from_sn(af.type);
        af.level     = ch->tot_level;
        af.duration  = -1;
        af.location  = APPLY_NONE;
        af.modifier  = 0;
        af.bitvector = AFF_FLYING;
        af.bitvector2 = 0;
        af.custom_name = NULL;
        af.slot	= WEAR_NONE;
        affect_to_char(ch, &af);
        check_improve(ch,gsn_flight,true,3);
    }
}

/**
 * do_land - Descend from flight to the ground
 *
 * Ends flight for character or their mount by removing flight affects.
 * Provides different messages based on whether landing in water or on ground,
 * and whether the character used physical flight (wings) or magical flight.
 *
 * Landing behaviors:
 * - Physical flight (gsn_flight): "Diving down" messages
 * - Magical flight (gsn_fly): "Slowly descend" messages
 * - Water sectors: Descend to water
 * - Other sectors: Descend to ground
 *
 * If mounted, the mount lands rather than the rider.
 * Strips both gsn_flight and gsn_fly affects to ensure grounding.
 *
 * @param ch        Flying character or mount
 * @param argument  Unused
 *
 * Triggers: TRIG_LAND
 */
void do_land(CHAR_DATA *ch, char *argument)
{
    if(MOUNTED(ch)) {
        if(!IS_AFFECTED(MOUNTED(ch),AFF_FLYING) && !is_affected(MOUNTED(ch),gsn_flight)) {
            act("$N doesn't seem to be airborne.", ch, MOUNTED(ch),NULL,NULL,NULL,NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        if(!p_percent_trigger(MOUNTED(ch), NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LAND,NULL)) {
            if(is_affected(ch,gsn_flight)) {
                if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
                    ch->in_room->sector_type == SECT_WATER_SWIM ||
                    ch->in_room->sector_type == SECT_UNDERWATER ||
                    ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
                    act("Diving down, you descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("Diving down, $n descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                } else {
                    act("Diving down, you descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("Diving down, $n descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            } else {
                if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
                    ch->in_room->sector_type == SECT_WATER_SWIM ||
                    ch->in_room->sector_type == SECT_UNDERWATER ||
                    ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
                    act("You slowly descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n slowly descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                } else {
                    act("You slowly descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n slowly descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            }
        }

        affect_strip(MOUNTED(ch),gsn_flight);
        affect_strip(MOUNTED(ch),gsn_fly);
    } else {
        if(!IS_AFFECTED(ch, AFF_FLYING) && !is_affected(ch,gsn_flight)) {
            send_to_char("You don't seem to be airborne.\n\r", ch);
            return;
        }

        if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_LAND,NULL)) {
            if(is_affected(ch,gsn_flight)) {
                if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
                    ch->in_room->sector_type == SECT_WATER_SWIM ||
                    ch->in_room->sector_type == SECT_UNDERWATER ||
                    ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
                    act("Diving down, you descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("Diving down, $n descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                } else {
                    act("Diving down, you descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("Diving down, $n descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            } else {
                if(	ch->in_room->sector_type == SECT_WATER_NOSWIM ||
                    ch->in_room->sector_type == SECT_WATER_SWIM ||
                    ch->in_room->sector_type == SECT_UNDERWATER ||
                    ch->in_room->sector_type == SECT_DEEP_UNDERWATER) {
                    act("You slowly descend to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n slowly descends to the water below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                } else {
                    act("You slowly descend to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n slowly descends to the ground below.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            }
        }

        affect_strip(ch,gsn_flight);
        affect_strip(ch,gsn_fly);
    }
}
