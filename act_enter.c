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
*	    Gabrielle Taylor (gtaylor@efn.org)				   *
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "interp.h"
#include "wilds.h"

void dungeon_check_commence(DUNGEON *dng, CHAR_DATA *ch);

void do_disembark( CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;
    OBJ_DATA *ship_obj;
    SHIP_DATA *ship;

    if ( ch->fighting != NULL )
    {
		act("You can't disembark while fighting.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
    }

	ship = get_room_ship(ch->in_room);

    if( !IS_VALID(ship) )
	{
		send_to_char("You are not on a ship.\n\r", ch);
		return;
	}

    if ( ship->ship_type == SHIP_AIR_SHIP )
    {
		if ( ship->ship_power > SHIP_SPEED_STOPPED )
		{
			act( "The doors of the airship are locked!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
			return;
		}

		if ( ship->ship_power == SHIP_SPEED_STOPPED && !mobile_is_flying(ch) )
		{
			act( "You need to be flying to disembark a flying airship.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
			return;
		}
    }

    location = ship->ship->in_room;
    ship_obj = ship->ship;

    char_from_room(ch);
    char_to_room(ch, location);

    if( get_colour_width(ship_obj->short_descr) > 0 )
    {
	    act("{WYou disembark from {x$p{W.{x", ch, NULL, NULL, ship_obj, NULL, NULL, NULL, TO_CHAR);
    	act("{W$n disembarks from {x$p{W.{x", ch, NULL, NULL, ship_obj, NULL, NULL, NULL, TO_ROOM);
	}
	else
	{
	    act("{WYou disembark from $p.{x", ch, NULL, NULL, ship_obj, NULL, NULL, NULL, TO_CHAR);
    	act("{W$n disembarks from $p.{x", ch, NULL, NULL, ship_obj, NULL, NULL, NULL, TO_ROOM);
	}

	move_cart(ch,location,TRUE);
}

ROOM_INDEX_DATA *get_portal_destination(CHAR_DATA *ch, OBJ_DATA *portal, bool allow_random)
{
	ROOM_INDEX_DATA *old_room = obj_room(portal);
	DUNGEON *in_dungeon = get_room_dungeon(old_room);
	INSTANCE *in_instance = get_room_instance(old_room);
	ROOM_INDEX_DATA *location = NULL;
	PORTAL_DATA *_portal = PORTAL(portal);

	// Value 3: Portal type
	switch(_portal->type)
	{
		case GATETYPE_ENVIRONMENT:
			// No values
			location = get_environment(old_room);
			break;

		case GATETYPE_NORMAL:
			if (IS_SET(_portal->flags,GATE_BUGGY) && (number_percent() < 5))
			{
				if (allow_random)
					location = get_random_room( ch, 0 );
			}
			else
			{
				// Param 0: AUID
				// Param 1: VNUM
				// Param 2: UID 0
				// Param 3: UID 1
				// If p2 and p3 are both 0, it will reference a static room
				location = get_room_index_auid(_portal->params[0], _portal->params[1]);

				// Check if this portal points to a clone room, if so, find it
				if( location != NULL && (_portal->params[2] > 0 || _portal->params[3] > 0)) {
					location = get_clone_room(location, (unsigned long)_portal->params[2], (unsigned long)_portal->params[3]);
				}
			}
			break;
		
		case GATETYPE_WILDS:
			// Param 0: WUID
			// Param 1: X
			// Param 2: Y
			{
				WILDS_DATA *wilds = get_wilds_from_uid(NULL,_portal->params[0]);
				location = get_wilds_vroom(wilds,_portal->params[1],_portal->params[2]);
				if(!location)
					location = create_wilds_vroom(wilds,_portal->params[1],_portal->params[2]);
			}
			break;

		case GATETYPE_WILDSRANDOM:
			// Param 0: WUID
			// Param 1: Min X (defaults to 0)
			// Param 2: Min Y (defaults to 0)
			// Param 3: Max X (defaults to map_size_x - 1)
			// Param 4: Max Y (defaults to map_size_y - 1)
			// Ranges will be clamped to the actual map size
			if (allow_random)
			{
				WILDS_DATA *wilds = get_wilds_from_uid(NULL, _portal->params[0]);
				if (wilds)
				{
					int x = number_range(_portal->params[1], _portal->params[3]);
					int y = number_range(_portal->params[2], _portal->params[4]);

					// Clamp to the current size of the map (as it can change after the portal was created)
					x = URANGE(0, x, wilds->map_size_x - 1);
					y = URANGE(0, y, wilds->map_size_y - 1);

					location = get_wilds_vroom(wilds,x,y);
					if(!location)
						location = create_wilds_vroom(wilds,x,y);
				}
			}
			break;

		case GATETYPE_RANDOM:
			if (allow_random)
				location = get_random_room( ch, 0 );
			break;

		case GATETYPE_AREARANDOM:
			// Param 0: AUID (0 = area of current room, ignored in wilds)
			if (allow_random)
			{
				ROOM_INDEX_DATA *here;

				here = obj_room(portal);

				if(here) {
					if(_portal->params[0] > 0)
					{
						location = get_random_room_area(ch, get_area_from_uid(_portal->params[0]));
					}
					else if(here->wilds)
					{
						int x,y;
						x = number_range(0,here->wilds->map_size_x-1);
						y = number_range(0,here->wilds->map_size_y-1);
						location = get_wilds_vroom(here->wilds,x,y);
						if(!location)
							location = create_wilds_vroom(here->wilds,x,y);
					}
					else
						location = get_random_room_area(ch, here->area);
				}
			}
			break;

		case GATETYPE_REGIONRECALL:
			// Param 0: AUID (0 = area of current room, does not work in wilds)
			// Param 1: Region index (0 = default region)
			{
				AREA_DATA *area = NULL;
				ROOM_INDEX_DATA *here;

				// Get the area
				if (_portal->params[0] > 0)
				{
					area = get_area_from_uid(_portal->params[0]);
				}
				else
				{
					// Current area if not in wilderness
					here = obj_room(portal);

					if (here && !here->wilds)
						area = here->area;
				}

				if (area)
				{
					AREA_REGION *region;
					if (_portal->params[1] > 0)
					{
						region = (AREA_REGION *)list_nthdata(area->regions, _portal->params[1]);
					}
					else
					{
						// Default region
						region = &area->region;
					}

					if (IS_VALID(region))
					{
						location = location_to_room(&region->recall);
					}
				}
			}
			break;

		case GATETYPE_AREARECALL:
			// Param 0: AUID (0 = area of current room, does not work in wilds)
			{
				AREA_DATA *area = NULL;

				if (_portal->params[0] > 0)
					area = get_area_from_uid(_portal->params[0]);
				else
				{
					ROOM_INDEX_DATA *here = obj_room(portal);

					if (here && !here->wilds)
						area = here->area;
				}
					
				if (area)
					location = location_to_room(&area->region.recall);
			}
			break;

		case GATETYPE_REGIONRANDOM:
			// Param 0: AUID (0 = current area, does not work in wilds)
			// Param 1: Region Index
			if (allow_random)
			{
				AREA_DATA *area = NULL;

				if (_portal->params[0] > 0)
					area = get_area_from_uid(_portal->params[0]);
				else
				{
					ROOM_INDEX_DATA *here = obj_room(portal);

					if (here && !here->wilds)
						area = here->area;
				}

				if (area)
				{
					AREA_REGION *region = NULL;
					if (_portal->params[1] > 0)
						region = (AREA_REGION *)list_nthdata(area->regions, _portal->params[1]);
					else
						region = &area->region;

					if (IS_VALID(region))
					{
						location = get_random_room_region(ch, region);
					}
				}
			}
			break;
		
		case GATETYPE_SECTIONRANDOM:
			// Param 0: Section index (0 = current section, <0 = ordinal index, >0 = generated index)
			if( allow_random && IS_VALID(old_room->instance_section) )
			{
				INSTANCE_SECTION *section = old_room->instance_section;

				if (_portal->params[0] != 0)
				{
					section = instance_get_section(section->instance, _portal->params[0]);
				}

				location = section_random_room(ch, section );
			}
			break;
		
		case GATETYPE_INSTANCERANDOM:
			if( allow_random && IS_VALID(old_room->instance_section) )
			{
				if( IS_VALID(old_room->instance_section->instance) )
				{
					location = instance_random_room(ch, old_room->instance_section->instance );
				}
				else
				{
					location = section_random_room(ch, old_room->instance_section );
				}
			}
			break;

		case GATETYPE_DUNGEONRANDOM:
			if( allow_random && IS_VALID(old_room->instance_section) )
			{
				if( IS_VALID(old_room->instance_section->instance) )
				{
					if( IS_VALID(old_room->instance_section->instance->dungeon) )
					{
						location = dungeon_random_room(ch, old_room->instance_section->instance->dungeon );
					}
					else
					{
						location = instance_random_room(ch, old_room->instance_section->instance );
					}
				}
				else
				{
					location = section_random_room(ch, old_room->instance_section );
				}
			}
			break;

		case GATETYPE_INSTANCE:
			// Param 0: Vnum (Area is the PORTAL's area)
			// Param 1: Upper UID
			// Param 2: Lower UID
			// Param 3: Special Room (if 0, go to default entrance)
			if (_portal->params[0] > 0)
			{
				INSTANCE *instance = NULL;
				if (_portal->params[1] > 0 || _portal->params[2] > 0)
				{
					instance = find_instance(_portal->params[1], _portal->params[2]);
				}

				// Was it not found or undefined?
				if (!IS_VALID(instance) || instance->blueprint->area != portal->pIndexData->area || instance->blueprint->vnum != _portal->params[0])
				{
					// [Re]spawn the instance
					BLUEPRINT *bp = get_blueprint(portal->pIndexData->area, _portal->params[0]);

					instance = create_instance(bp);
				}

				if (!IS_VALID(instance))
					break;

				_portal->params[1] = instance->uid[0];
				_portal->params[2] = instance->uid[7];
				
				// Get the special room
				if (_portal->params[3] > 0)
					location = get_instance_special_room(instance, _portal->params[3]);
				else
					location = instance->entrance;
			}
			break;

		case GATETYPE_DUNGEON:
			// Param 0: Vnum (Area is the PORTAL's area)
			// Param 1: Floor (this takes precendent)
			// Param 2: Entry Room (only looked at if v6 is 0)
			// if Param 1 and 2 are both less than 1, need to go to the default entrance
			if (_portal->params[0] > 0)
			{
				if (_portal->params[1] > 0)
					location = spawn_dungeon_player_floor(ch, portal->pIndexData->area, _portal->params[0], _portal->params[1]);
				else if (_portal->params[2] > 0)
					location = spawn_dungeon_player_special_room(ch, portal->pIndexData->area, _portal->params[0], _portal->params[2], NULL);
				else // Current default is assumed to be floor 1, but that needs to be changed to get the named special room.
					location = spawn_dungeon_player_floor(ch, portal->pIndexData->area, _portal->params[0], 1);
			}
			break;

		case GATETYPE_DUNGEONFLOOR:
			// Must be inside a dungeon for it to work
			// Param 0: Floor (0 = check Previous and Next Floor flags)
			if (IS_VALID(in_dungeon))
			{
				int floor = _portal->params[0];

				if( floor < 1 )
				{
					if( IS_SET(portal->value[1], EX_PREVFLOOR) )
						floor = in_instance->floor - 1;
					else if( IS_SET(portal->value[1], EX_NEXTFLOOR) )
						floor = in_instance->floor + 1;
					else
						floor = 0;
				}

				if( floor > 0 )
				{
					INSTANCE *instance = (INSTANCE *)list_nthdata(in_dungeon->floors, floor);

					if( IS_VALID(instance) )
						location = instance->entrance;
				}
			}
			break;

		case GATETYPE_DUNGEON_FLOOR_SPECIAL:
			// Must be inside a dungeon for it to work
			// Param 0: Floor (>0:generated index, <0:ordinal index, 0:current)
			// Param 1: Special room index (<1: invalid)
			if (IS_VALID(in_dungeon) && _portal->params[1] > 0)
			{
				INSTANCE *instance = in_instance;
				if (_portal->params[0] != 0)
				{
					instance = dungeon_get_instance_level(in_dungeon, _portal->params[0]);
					if (!IS_VALID(instance))
						break;
				}

				location = get_instance_special_room(instance, _portal->params[1]);
			}
			break;

		case GATETYPE_DUNGEON_RANDOM_FLOOR:
			if (allow_random && IS_VALID(in_dungeon))
			{
				int floors = list_size(in_dungeon->floors);
				int min_f = (_portal->params[0] > 0) ? _portal->params[0] : 1;
				int max_f = (_portal->params[1] > 0) ? _portal->params[1] : floors;

				min_f = UMAX(min_f, 1);
				max_f = UMIN(max_f, floors);

				if (min_f <= max_f)
				{
					int floor = number_range(min_f, max_f);

					INSTANCE *instance = (INSTANCE *)list_nthdata(in_dungeon->floors, floor);

					if( IS_VALID(instance) )
						location = instance->entrance;
				}
			}
			break;
	}

	return location;
}

void do_enter( CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location = NULL;

    if ( ch->fighting != NULL )
	return;

    if (argument[0] != '\0')
    {
	ROOM_INDEX_DATA *old_room;
	OBJ_DATA *portal;
	CHAR_DATA *fch, *fch_next;

	old_room = ch->in_room;

	portal = get_obj_list( ch, argument,  ch->in_room->contents );

	if (portal == NULL)
	{
	    int i;
	    ROOM_INDEX_DATA *r2;
	    for ( i = 0; i < MAX_DIR; i++ ) {
		if ( ch->in_room->exit[ i ] != NULL && (r2 = ch->in_room->exit[ i ]->u1.to_room)) {
		    portal = get_obj_list( ch, argument,  r2->contents );
		    if ( portal != NULL && portal->item_type == ITEM_SHIP ) {
			break;
		    }
		}
	    }

	    if ( portal == NULL ) {
		send_to_char("You don't see that here.\n\r",ch);
		return;
	    }
	}

	// TODO: Add a flag to portals for blocking or allowing carts and/or relics to automate this
/* - Temporary allowance of relics through portals, until ships are fixed. -- Areo
if (PULLING_CART(ch) && portal->item_type != ITEM_SHIP)
	{
	    send_to_char("You must drop what you are currently pulling.\n\r", ch);
	    return;
	}
*/

	if (portal->item_type == ITEM_SHIP)
	{
		// TODO: Why?
	    if (MOUNTED(ch))
	    {
			act("You can't board this vessel while mounted.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
	    }

	    if( portal->ship->ship_type == SHIP_AIR_SHIP )
	    {
	    	if( !mobile_is_flying(ch) && portal->ship->ship_power != SHIP_SPEED_LANDED )
	    	{
				act("You need to be flying to board a flying airship.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return;
			}
		}

		if (portal->ship->ship_power > SHIP_SPEED_STOPPED)
		{
			act("You can't board a moving vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

	    location = portal->ship->instance->entrance;

	    if ( location )
	    {
			/* CHAR_DATA *pMob; */
			act("{WYou board {x$p{W.{x\n\r", ch, NULL, NULL, portal, NULL, NULL, NULL, TO_CHAR);
			act("{W$n boards {x$p{W.{x\n\r", ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);

			move_cart(ch,location,TRUE);

			char_from_room(ch);
			char_to_room(ch, location);

			act("{W$n boards {x$p{W.{x", ch, NULL, NULL, portal, NULL, NULL, NULL, TO_ROOM);

			do_function(ch, &do_look, "auto");
			return;
	    }

		send_to_char("You could not board the ship.\n\r", ch);
		return;
	}

	if (!IS_PORTAL(portal) || IS_SET(PORTAL(portal)->exit,EX_CLOSED))
	{
	    send_to_char("You can't seem to find a way in.\n\r",ch);
	    return;
	}

 	/* @@@NIB : 20070126 : Changed the polarity of nocurse
 		It had a NOT.  But that's backwards to the name of
 		the flag.*/
 	if (IS_SET(PORTAL(portal)->flags,GATE_NOCURSE)
  		&&  (IS_AFFECTED(ch,AFF_CURSE)))
	    /*
	       ||   IS_SET(old_room->room_flags,ROOM_NO_RECALL)))
	     */
	{
	    send_to_char("Something prevents you from leaving...\n\r",ch);
	    return;
	}

	DUNGEON *in_dungeon = get_room_dungeon(old_room);
	INSTANCE *in_instance = get_room_instance(old_room);

	location = get_portal_destination(ch, portal, TRUE);

  	if (!location || location == old_room || !can_see_room(ch,location) ||
  		(!IS_SET(portal->value[2],GATE_NOPRIVACY) && room_is_private(location, ch))) {
	    act("$p doesn't seem to go anywhere.",ch, NULL, NULL,portal,NULL, NULL, NULL,TO_CHAR);
	    return;
	}

	if(!is_room_unlocked(ch, location) )
	{
		int ret = p_percent2_trigger(location->area, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREENTER, NULL,0,0,0,0,0);

		if( ret < 1 )
		{
			send_to_char("You cannot enter that place yet.\n\r", ch);
			return;
		}
		else if( ret == 1 )
		{
			return;
		}
	}

	if(p_percent_trigger(NULL, NULL, location, NULL, ch, NULL, NULL,portal, NULL,TRIG_PREENTER, "portal",0,0,0,0,0))
		return;

	if(p_percent_trigger(NULL, portal, NULL, NULL, ch, NULL, NULL,NULL, NULL,TRIG_PREENTER, NULL,0,0,0,0,0))
		return;

 	/* @@@NIB : 20070126 : added the check */
 	if(!IS_SET(PORTAL(portal)->flags,GATE_SILENTENTRY))
  		act("$n steps into $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM);

	if (IS_SET(PORTAL(portal)->flags,GATE_NORMAL_EXIT))
	    act("{YYou enter $p.{x",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_CHAR);
	else if (!IS_SET(PORTAL(portal)->flags, GATE_RANDOM))
	    act("{YYou walk through $p and find yourself in $T.{x",
		    ch, NULL, NULL,portal, NULL, NULL,location->name,TO_CHAR);
	else
	    act("{YYou walk through $p and find yourself somewhere else...{x",
		    ch, NULL, NULL,portal, NULL, NULL,location->name,TO_CHAR);

	move_cart(ch,location,TRUE);

	DUNGEON *to_dungeon = get_room_dungeon(location);
	INSTANCE *to_instance = get_room_instance(location);

	char_from_room(ch);
	char_to_room(ch, location);

	// Entering a dungeon
	if (IS_VALID(to_dungeon) && !IS_VALID(in_dungeon))
	{
		// Save location
		location_from_room(&ch->before_dungeon, old_room);
	}

        /* Let portals cast spells */
	obj_apply_spells(ch, portal, ch, NULL, PORTAL(portal)->spells, TRIG_APPLY_AFFECT);
		/*
	{
		SPELL_DATA *spell;

		for (spell = portal->spells; spell; spell = spell->next)
			obj_cast_spell(spell->sn, spell->level, ch, ch, NULL);
	}
	*/

	if (IS_SET(PORTAL(portal)->flags,GATE_GOWITH)) /* take the gate along */
	{
	    obj_from_room(portal);
	    obj_to_room(portal,location);
	}

	/* @@@NIB : 20070127 : strip off if portal is nosneak
			Right now, it does not mix with "sneak". */
	if(IS_SET(PORTAL(portal)->flags,GATE_NOSNEAK)) {
		affect_strip(ch, gsn_sneak);
		REMOVE_BIT(ch->affected_by[0], AFF_SNEAK);

	/* @@@NIB : 20070127 : if portal is sneak, attempt autosneak IF they can do it!
			Maybe in the future if permitted, this can do it regardless of
			whether they can do it or not.  For now, normal rules for "sneak"
			apply.  If they are already sneaking, that's a different story.
			Improvement is not done here and if you fail, it doesn't say
			anything. */
	} else if(IS_SET(PORTAL(portal)->flags,GATE_SNEAK)) {
		if(!MOUNTED(ch) && !ch->fighting && !IS_AFFECTED(ch,AFF_SNEAK) &&
			(number_percent() < get_skill(ch,gsn_sneak))) {
			AFFECT_DATA af;
			memset(&af,0,sizeof(af));
			af.where     = TO_AFFECTS;
			af.group     = AFFGROUP_PHYSICAL;
			af.type      = gsn_sneak;
			af.level     = ch->level;
			af.duration  = ch->level;
			af.location  = APPLY_NONE;
 			af.modifier  = 0;
			af.bitvector = AFF_SNEAK;
			af.bitvector2 = 0;
			af.slot	= WEAR_NONE;
			affect_to_char(ch, &af);
			send_to_char("You assume a sneaking posture.\n\r", ch);
		}
	}

	/* @@@NIB : 20070126 : added the check */
	if(!IS_SET(PORTAL(portal)->flags,GATE_SILENTEXIT)) {

		if( IS_VALID(in_dungeon) && !IS_VALID(to_dungeon) )
		{
			WNUM wnum;
			wnum.pArea = in_dungeon->index->area;
			wnum.vnum = in_dungeon->index->vnum;
			OBJ_DATA *dp = get_room_dungeon_portal(location, wnum);

			if( IS_VALID(dp) )
			{
				if( !IS_NULLSTR(in_dungeon->index->zone_out_portal) )
				{
					act(in_dungeon->index->zone_out_portal, ch, NULL, NULL, dp, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					act("$n has arrived through $p.",ch, NULL, NULL,dp, NULL, NULL,NULL,TO_ROOM);
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
					act(in_dungeon->index->zone_out, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				else
					act("{W$n materializes.{x", ch,NULL,NULL,NULL,NULL, NULL, NULL, TO_ROOM);
			}
		}
		else if (IS_SET(PORTAL(portal)->flags,GATE_NORMAL_EXIT))
  		    act("$n has arrived.",ch, NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
  		else
  		    act("$n has arrived through $p.",ch, NULL, NULL,portal, NULL, NULL,NULL,TO_ROOM);
	}

	if(!IS_NPC(ch) && IS_SET(PORTAL(portal)->flags,GATE_FORCE_BRIEF)) {
		bool was_brief = IS_SET(ch->comm, COMM_BRIEF) && TRUE;

		SET_BIT(ch->comm, COMM_BRIEF);
		do_function(ch, &do_look, "auto");

		if( !was_brief )
			REMOVE_BIT(ch->comm, COMM_BRIEF);
	} else {
		do_function(ch, &do_look, "auto");
	}


	if (IS_VALID(to_dungeon))
	{
		dungeon_check_commence(to_dungeon, ch);
	}


	/* charges */
	if (PORTAL(portal)->charges > 0)
	{
	    PORTAL(portal)->charges--;
	    if (PORTAL(portal)->charges == 0)
			PORTAL(portal)->charges = -1;
	}

	if(p_percent_trigger(NULL, portal, NULL, NULL, ch, NULL, NULL,NULL, NULL,TRIG_ENTRY, NULL,0,0,0,0,0))
		return;

	/* protect against circular follows */
	if (old_room == location)
	    return;

	for ( fch = old_room->people; fch != NULL; fch = fch_next )
	{
	    fch_next = fch->next_in_room;

	    if (portal == NULL || portal->value[0] == -1)
		/* no following through dead portals */
		continue;

	    if ( fch->master == ch && IS_AFFECTED(fch,AFF_CHARM)
		    &&   fch->position < POS_STANDING)
		do_function(fch, &do_stand, "");

	    if ( fch->master == ch && fch->position == POS_STANDING)
	    {
		act( "You follow $N.", fch, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		do_function(fch, &do_enter, argument);
	    }
	}

	if (portal != NULL && PORTAL(portal)->charges == -1)
	{
	    act("$p fades out of existence.",ch, NULL, NULL,portal,NULL, NULL, NULL,TO_CHAR);
	    if (ch->in_room == old_room)
			act("$p fades out of existence.",ch, NULL, NULL,portal,NULL, NULL, NULL,TO_ROOM);
	    else if (old_room->people != NULL)
	    {
			act("$p fades out of existence.", old_room->people, NULL, NULL,portal,NULL, NULL, NULL,TO_CHAR);
			act("$p fades out of existence.", old_room->people, NULL, NULL,portal,NULL, NULL, NULL,TO_ROOM);
	    }
	    extract_obj(portal);
	}

	/*
	 * If someone is following the char, these triggers get activated
	 * for the followers before the char, but it's safer this way...
	 */

	if( IS_VALID(to_dungeon) && (in_dungeon != to_dungeon) )
	{
		p_percent2_trigger(NULL, NULL, to_dungeon, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL,0,0,0,0,0);
	}

	if( to_instance != in_instance )
	{
		p_percent2_trigger(NULL, to_instance, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ENTRY, NULL,0,0,0,0,0);
	}

	p_percent_trigger( ch, NULL, NULL, NULL,NULL, NULL, NULL, NULL, NULL, TRIG_ENTRY , NULL,0,0,0,0,0);

	if ( !IS_NPC( ch ) ) {
	    p_greet_trigger( ch, PRG_MPROG ,0,0,0,0,0);
	    p_greet_trigger( ch, PRG_OPROG ,0,0,0,0,0);
	    p_greet_trigger( ch, PRG_RPROG ,0,0,0,0,0);
	}
	return;
    }

    send_to_char("Nope, can't do it.\n\r",ch);
}
