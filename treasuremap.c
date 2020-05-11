/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "olc.h"
#include "wilds.h"

OBJ_DATA *create_wilderness_map(WILDS_DATA *pWilds, int vx, int vy, OBJ_DATA *scroll, int offset)
{
	int distance;
	AREA_DATA *bestArea = NULL;
	AREA_DATA *closestArea;
	int bestDistance = 200;

	if( !pWilds ) return NULL;
	if( !scroll ) return NULL;

	int w = get_squares_to_show_x(0) - 1;
	int h = get_squares_to_show_y(0) - 1;

	int wx = vx + number_range(-w, w);
	int wy = vy + number_range(-h, h);

	// OFFSET% chance the marker is off by +/-1
	if( number_percent() < offset )
	{
		vx += number_range(-1, 1);
		vy += number_range(-1, 1);
	}

	// Get closest area
	for (closestArea = area_first; closestArea != NULL; closestArea = closestArea->next)
	{
		if( !closestArea->open )
			continue;

		distance = (int) sqrt(
				( closestArea->x - vx ) * ( closestArea->x - vx ) +
				( closestArea->y - vy ) * ( closestArea->y - vy ) );

		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestArea = closestArea;
		}
	}

	if (scroll != NULL)
	{
		BUFFER *buffer = new_buf();

		get_wilds_mapstring(buffer, pWilds, wx, wy, vx, vy, 0, 0);

		if (scroll->full_description != NULL)
			free_string(scroll->full_description);

		if( bestArea )
		{
			add_buf(buffer, "\n\r{RX{x marks the spot! The location is near ");
			add_buf(buffer, bestArea->name);
			add_buf(buffer, ".\n\r");
		}
		else
		{
			add_buf(buffer, "\n\r{RX{x marks the spot!\n\r");
		}

		scroll->full_description = str_dup(buffer->string);

		free_buf(buffer);

		if( scroll->item_type == ITEM_MAP )
		{
			scroll->value[0] = wx;
			scroll->value[1] = wy;
		}
	}

	return scroll;
}

OBJ_DATA *create_treasure_map(WILDS_DATA *pWilds, AREA_DATA *pArea, OBJ_DATA *treasure)
{
	ROOM_INDEX_DATA *pRoom = NULL;
	OBJ_DATA *scroll;
	int vx, vy;

	if( !IS_VALID(treasure) ) return NULL;

	pRoom = obj_room(treasure);

	if( !pRoom || !IS_WILDERNESS(pRoom) || pRoom->wilds != pWilds || treasure->carried_by || treasure->in_obj)
	{
		if( treasure->carried_by )
			obj_from_char(treasure);
		else if( treasure->in_obj)
			obj_from_obj(treasure);
		else if( pRoom )
			obj_from_room(treasure);

		// Find location for map
		while(TRUE) {
			if( pArea && pArea->open )
			{
				vx = number_range(-50, 50);
				vy = number_range(-50, 50);

				if( (vx * vx + vy * vy) >= 40000 )
					continue;

				vx += pArea->x;
				vy += pArea->y;
			}
			else
			{
				vx = number_range(0, pWilds->map_size_x - 1);
				vy = number_range(0, pWilds->map_size_y - 1);
			}

			WILDS_TERRAIN *pTerrain = get_terrain_by_coors(pWilds, vx, vy);

			if( pTerrain != NULL && !pTerrain->nonroom &&
				pTerrain->template->sector_type != SECT_WATER_SWIM &&
				pTerrain->template->sector_type != SECT_WATER_NOSWIM)
			{
				break;
			}
		}

		if( !(pRoom = get_wilds_vroom(pWilds, vx, vy)) )
			pRoom = create_wilds_vroom(pWilds, vx, vy);

		obj_to_room(treasure, pRoom);
	}
	else
	{
		// In a Wilderess Room already
		vx = pRoom->x;
		vy = pRoom->y;
	}

	// create the scroll
	scroll = create_object(get_obj_index(OBJ_VNUM_TREASURE_MAP), 0, TRUE);

	return create_wilderness_map(pWilds, vx, vy, scroll, 5);
}

void do_spawntreasuremap(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	AREA_DATA *area = NULL;

	if( !IN_WILDERNESS(ch) )
	{
		send_to_char("You must be in the wilderness.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( arg[0] != '\0' )
	{
		for (area = area_first; area != NULL; area = area->next) {
			if (!is_number(arg) && !str_infix(argument, area->name)) {
				break;
			}
		}
	}

	bool generated = false;
	OBJ_DATA *treasure = NULL;
	if( argument[0] != '\0' )
	{
		treasure = get_obj_carry(ch, argument, ch);

		if( treasure == NULL )
		{
			send_to_char("You do not have that.\n\r", ch);
			return;
		}
	}
	else
	{
		// find treasure
		int i = number_range(0, MAX_TREASURES-1);

		// create object
		treasure = create_object(get_obj_index(treasure_table[i]), 0, TRUE);

		if( !treasure )
		{
			send_to_char("Try again next time.\n\r", ch);
			return;
		}

		generated = true;
	}

	OBJ_DATA *map = create_treasure_map(ch->in_room->wilds, area, treasure);

	if( map )
	{
		SET_BIT(treasure->extra2_flags, ITEM_BURIED);	// Bury the treasure, yar!

		obj_to_char(map, ch);
		act("You spawn $p out of thin air.", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR);
		act("$n spawns $p out of thin air.", ch, NULL, NULL, map, NULL, NULL, NULL, TO_ROOM);
		return;
	}
	else
	{
		if( generated )
		{
			if( treasure->in_room )
				extract_obj(treasure);
			else
			{
				list_remlink(loaded_objects, treasure);
				--treasure->pIndexData->count;
				free_obj(treasure);
			}
		}
		send_to_char("You seem to have misplaced the map.  Try again next time.\n\r", ch);
		return;
	}
}
