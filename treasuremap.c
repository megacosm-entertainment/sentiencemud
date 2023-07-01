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

OBJ_DATA *create_wilderness_map(WILDS_DATA *pWilds, int vx, int vy, OBJ_DATA *scroll, int offset, char *marker)
{
	int distance;
	AREA_REGION *bestRegion = NULL;
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

	if( IS_NULLSTR(marker) )
		marker = "{RX{x";

	// Get closest area
	for (closestArea = area_first; closestArea != NULL; closestArea = closestArea->next)
	{
		if( !closestArea->open )
			continue;

		int distanceSq;
		AREA_REGION *region = get_closest_area_region(closestArea, vx, vy, &distanceSq, FALSE);

		if (distanceSq >= 0)
		{
			distance = (int)sqrt(distanceSq);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestRegion = region;
			}
		}

	}

	if (scroll != NULL)
	{
		BUFFER *buffer = new_buf();

		get_wilds_mapstring(buffer, pWilds, wx, wy, vx, vy, 0, 0, marker);

		if (scroll->full_description != NULL)
			free_string(scroll->full_description);

		if( IS_VALID(bestRegion) )
		{
			add_buf(buffer, "\n\r");
			add_buf(buffer, marker);
			add_buf(buffer, "{x marks the spot! The location is near {Y");
			add_buf(buffer, bestRegion->name);
			add_buf(buffer, "{x in {Y");
			add_buf(buffer, bestRegion->area->name);
			add_buf(buffer, "{x.\n\r");
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

inline static bool __treasure_area_region_valid(AREA_REGION *region, WILDS_DATA *wilds)
{
	if (region->x < 0 || region->y < 0) return false;

	for( int x = -20; x <= 20; x++ )
	{
		for( int y = -20; y <= 20; y++ )
		{
			int d = x * x + y * y;

			if( d >= 40000 ) continue;

			WILDS_TERRAIN *pTerrain = get_terrain_by_coors(wilds, region->x + x, region->x + y);

			if( pTerrain != NULL && !pTerrain->nonroom &&
				pTerrain->template->sector_type != SECT_WATER_SWIM &&
				pTerrain->template->sector_type != SECT_WATER_NOSWIM)
				return true;
		}
	}

	return false;
}

bool valid_area_for_treasure(AREA_DATA *pArea)
{
	if( !pArea ) return false;

	if( pArea->wilds_uid < 1 ) return false;

	WILDS_DATA *wilds = get_wilds_from_uid(NULL, pArea->wilds_uid);
	if( !wilds ) return false;

	if (__treasure_area_region_valid(&pArea->region, wilds))
		return true;

	ITERATOR it;
	AREA_REGION *region;
	iterator_start(&it, pArea->regions);
	while((region = (AREA_REGION *)iterator_nextdata(&it)))
	{
		if (__treasure_area_region_valid(region, wilds))
			break;
	}
	iterator_stop(&it);


	return region != NULL;
}

OBJ_DATA *create_treasure_map(WILDS_DATA *pWilds, AREA_DATA *pArea, OBJ_DATA *treasure)
{
	ROOM_INDEX_DATA *pRoom = NULL;
	OBJ_DATA *scroll;
	int vx, vy;

	if( !IS_VALID(treasure) ) return NULL;

	if( !pWilds ) return NULL;

	// Not in the same wilderness as the one specified
	if( pArea )
	{
		if( !valid_area_for_treasure(pArea) )
			return NULL;

		if( pArea->wilds_uid != pWilds->uid )
			return NULL;
	}

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
				int r = number_range(0, list_size(pArea->regions));
				AREA_REGION *region;

				if (r > 0)
					region = (AREA_REGION *)list_nthdata(pArea->regions, r);
				else
					region = &pArea->region;

				vx = number_range(-50, 50);
				vy = number_range(-50, 50);

				if( (vx * vx + vy * vy) >= 40000 )
					continue;

				vx += region->x;
				vy += region->y;
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
	scroll = create_object(obj_index_treasure_map, 0, TRUE);

	return create_wilderness_map(pWilds, vx, vy, scroll, 5, "{RX{x");
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
			if (!is_number(arg) && !str_infix(arg, area->name)) {
				break;
			}
		}
	}

	bool generated = false;
	OBJ_DATA *treasure = NULL;
	if( argument[0] != '\0' )
	{
		treasure = get_obj_here(ch, NULL, argument);

		if( treasure == NULL )
		{
			send_to_char("You do not have that.\n\r", ch);
			return;
		}
	}
	else
	{
		/*
		// TODO: Rando treasure map
		// Disabling this for now...
		// find treasure
		int i = number_range(0, MAX_TREASURES-1);

		// create object
		treasure = create_object(get_obj_index(treasure_table[i]), 0, TRUE);

		if( !treasure )
		{
			send_to_char("Try again next time.\n\r", ch);
			return;
		}
		*/

		generated = true;
		send_to_char("Try again next time.\n\r", ch);
		return;
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
