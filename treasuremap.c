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

// Puts some treasure in the wilds, buries it and creates a map
OBJ_DATA* create_treasure_map(WILDS_DATA *pWilds)
{
	long vnum;
	int i;
	ROOM_INDEX_DATA *pRoom = NULL;
	char *map;
	OBJ_DATA *scroll;
	OBJ_DATA *treasure;
	AREA_DATA *pArea;
	AREA_DATA *closestArea;
	int distance;
	AREA_DATA *bestArea = NULL;
	int bestDistance = 200;

	// find treasure
	i = number_range(0, MAX_TREASURES-1);

	// create object
	treasure = create_object(get_obj_index(treasure_table[i]), 0, TRUE);

	// Find location for map
	int vx, vy;
	while(TRUE) {
		vx = number_range(0, pWilds->map_size_x - 1);
		vy = number_range(0, pWilds->map_size_y - 1);

		WILDS_TERRAIN *pTerrain = get_terrain_by_coords(pWilds, vx, vy);

		if( pTerrain != NULL && !pTerrain->nonroom &&
			pTerrain->template->sector_type != SECT_WATER_SWIM &&
			pTerrain->template->sector_type != SECT_WATER_NOSWIM)
		{
			break;
		}
	}

	if( !(pRoom = get_wilds_vroom(wilds, vx, vy)) )
		pRoom = create_wilds_vroom(wilds, vx, vy);



	int wx = vx + number_range(-7, 7);
	int wy = vy + number_range(-7, 7);

	// Get closest area
	for (closestArea = area_first; closestArea != NULL; closestArea = closestArea->next)
	{
		distance = (int) sqrt(
				( closestArea->x - vx ) * ( closestArea->x - vx ) +
				( closestArea->y - vy ) * ( closestArea->y - vy ) );

		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestArea = closestArea;
		}
	}

	// create the scroll
	scroll = create_object(get_obj_index(OBJ_VNUM_TREASURE_MAP), 0, TRUE);
	if (scroll != NULL)
	{
		BUFFER *buffer = new_buf();

		get_wilds_mapstring(buffer, pWilds, wx, wy, vx, vy);

		if (scroll->full_description != NULL)
			free_string(scroll->full_description);

		if( bestArea )
		{
			add_buf(buffer, "\n\r{RX{x marks the spot! The location be near ");
			add_buf(buffer, bestArea->name);
			add_buf(buffer, ".\n\r");
		}
		else
		{
			add_buf(buffer, "\n\r{RX{x marks the spot!\n\r");
		}

		scroll->full_description = str_dup(buffer->string);

		free_buf(buffer);
	}

	return scroll;
}

void do_spawntreasuremap(CHAR_DATA *ch, char *argument)
{
	if( !IN_WILDERNESS(ch) )
	{
		send_to_char("You must be in the wilderness.\n\r", ch);
		return;
	}

	OBJ_DATA *map = create_treasure_map(ch->in_room->wilds);

	if( map )
	{
		obj_to_char(map, ch);
		act("You spawn $p out of thin air.", ch, NULL, NULL, map, NULL, NULL, TO_CHAR);
		act("$n spawns $p out of thin air.", ch, NULL, NULL, map, NULL, NULL, TO_ROOM);
		return;
	}
	else
	{
		send_to_char("Try again next time.\n\r", ch);
		return;
	}
}
