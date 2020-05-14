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
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <math.h>
#include "merc.h"
#include "recycle.h"
#include "olc.h"
#include "tables.h"
#include "scripts.h"
#include "wilds.h"
#include "interp.h"

INSTANCE *instance_load(FILE *fp);
void update_instance(INSTANCE *instance);
void reset_instance(INSTANCE *instance);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);
void steering_set_heading(SHIP_DATA *ship, int heading);
void steering_set_turning(SHIP_DATA *ship, char direction);
void ship_stop(SHIP_DATA *ship);

extern LLIST *loaded_instances;

bool ships_changed = false;
long top_ship_index_vnum = 0;

LLIST *loaded_ships;
LLIST *loaded_waypoints;
LLIST *loaded_waypoint_paths;


// Measured in 2 * ship->steering.heading / 45
int bearing_door[] = {
	DIR_NORTH,				// 0
	DIR_NORTHEAST,			// 1
	DIR_NORTHEAST,			// 2
	DIR_EAST,				// 3
	DIR_EAST,				// 4
	DIR_SOUTHEAST,			// 5
	DIR_SOUTHEAST,			// 6
	DIR_SOUTH,				// 7
	DIR_SOUTH,				// 8
	DIR_SOUTHWEST,			// 9
	DIR_SOUTHWEST,			// 10
	DIR_WEST,				// 11
	DIR_WEST,				// 12
	DIR_NORTHWEST,			// 13
	DIR_NORTHWEST,			// 14
	DIR_NORTH,				// 15
};

/////////////////////////////////////////////////////////////////
//
// Navigation
//

bool ship_seek_point(SHIP_DATA *ship)
{
	ROOM_INDEX_DATA *room = obj_room(ship->ship);
	if( IS_WILDERNESS(room) && ship->seek_point.wilds == room->wilds )
	{
		if( room->x == ship->seek_point.x &&
			room->y == ship->seek_point.y )
		{
			// TODO: Is there another waypoint?

			ship_echo(ship, "{WThe vessel has reached its destination.{x");
			ship_stop(ship);
			return false;	// Return false to indicate stop movement
		}


		int dx = ship->seek_point.x - room->x;
		int dy = room->y - ship->seek_point.y;

		int heading = (int)(180 * atan2(dx, dy) / 3.14159);
		if( heading < -180 ) heading += 360;

		if( abs(heading - ship->steering.heading_target) > 5)
		{
			int delta = heading - ship->steering.heading;
			if( delta > 180 ) delta -= 360;
			else if( delta <= -180 ) delta += 360;

			char turning_dir;

			if( delta == 180 )
				turning_dir = (number_percent() < 50) ? -1 : 1;
			else
				turning_dir = (delta < 0) ? -1 : 1;

			steering_set_heading(ship, heading);
			steering_set_turning(ship, turning_dir);
		}
	}

	return true;
}



/////////////////////////////////////////////////////////////////
//
// Steering
//

void steering_calc_heading(SHIP_DATA *ship)
{
	ship->steering.dx = (int)(1000 * sin(3.14159 * ship->steering.heading / 180));
	ship->steering.dy = -(int)(1000 * cos(3.14159 * ship->steering.heading / 180));

	ship->steering.ax = abs(ship->steering.dx);
	ship->steering.ay = abs(ship->steering.dy);

	ship->steering.sx = (ship->steering.dx > 0) ? 1 : ((ship->steering.dx < 0) ? -1 : 0);
	ship->steering.sy = (ship->steering.dy > 0) ? 1 : ((ship->steering.dy < 0) ? -1 : 0);

	ship->steering.compass = bearing_door[2 * ship->steering.heading / 45];

	ship->steering.move = 0;
}

void steering_set_heading(SHIP_DATA *ship, int heading)
{
	// Initialize it
	if( ship->steering.heading < 0 )
	{
		ship->steering.heading = heading;
	}

	ship->steering.heading_target = heading;
}

void steering_set_turning(SHIP_DATA *ship, char direction)
{
	ship->steering.turning_dir = direction;
}

void steering_update(SHIP_DATA *ship)
{
	if( ship->steering.turning_dir )
	{
		int power = ship->index->turning;
		if( ship->speed == SHIP_SPEED_STOPPED )
		{
			// Cut the turning power while motionless
			power /= 2;
			power = UMAX(1, power);
		}


		if( ship->steering.heading_target < 0 )
		{
			// Simply turning, rather than turning to a direction
			ship->steering.heading += ship->steering.turning_dir * power;
			if( ship->steering.heading < 0 )
				ship->steering.heading += 360;
			else if( ship->steering.heading >= 360 )
				ship->steering.heading -= 360;
		}
		else
		{
			int delta = abs(ship->steering.heading - ship->steering.heading_target);
			if( delta > 180 ) delta = 360 - delta;

			if( delta <= power )
			{
				ship->steering.heading = ship->steering.heading_target;

				// Stationary targeted turning
				//  Ship has reading its desired heading
				if( ship->speed == SHIP_SPEED_STOPPED )
				{
					char buf[MSL];
					char arg[MIL];
					switch(ship->steering.heading)
					{
					case 0:		strcpy(arg, "to the north"); break;
					case 45:	strcpy(arg, "to the northeast"); break;
					case 90:	strcpy(arg, "to the east"); break;
					case 135:	strcpy(arg, "to the southeast"); break;
					case 180:	strcpy(arg, "to the south"); break;
					case 225:	strcpy(arg, "to the southwest"); break;
					case 270:	strcpy(arg, "to the west"); break;
					case 315:	strcpy(arg, "to the northwest"); break;
					default:
						sprintf(arg, "toward %d degrees", ship->steering.heading);
					}

					sprintf(buf, "{WThe vessel is now heading %s.{x", arg);
					ship_echo(ship, buf);
				}

				ship->steering.turning_dir = 0;
			}
			else
			{
				ship->steering.heading += ship->steering.turning_dir * power;
				if( ship->steering.heading < 0 )
					ship->steering.heading += 360;
				else if( ship->steering.heading >= 360 )
					ship->steering.heading -= 360;
			}
		}

		steering_calc_heading(ship);
	}
}

bool steering_movement(SHIP_DATA *ship, int *x, int *y, int *door)
{
	static int compasses[] = {
		DIR_NORTHWEST, DIR_NORTH, DIR_NORTHEAST,
		DIR_WEST, -1, DIR_EAST,
		DIR_SOUTHWEST, DIR_SOUTH, DIR_SOUTHEAST
	};

	// Allow for steering updates while stopped
	if( ship->speed <= SHIP_SPEED_STOPPED ) return false;

	ROOM_INDEX_DATA *room = obj_room(ship->ship);

	if( !room || !room->wilds ) return false;

	if( !ship_seek_point(ship) ) return false;

	// No heading, no movement!
	if( !ship->steering.ax && !ship->steering.ay ) return false;

	int _x = room->x;
	int _y = room->y;
	int _d;

	if( ship->steering.ay > ship->steering.ax )
	{
		// more North/South
		_y += ship->steering.sy;
		_d = 4 + 3 * ship->steering.sy;	// Will result in 1(North) or 7(South)

		ship->steering.move += ship->steering.ax;
		if( ship->steering.move >= ship->steering.ay )
		{
			ship->steering.move -= ship->steering.ay;

			_x += ship->steering.sx;
			_d += ship->steering.sx;	// Shift to 0/6(West corner) or 2/8(East corner)
		}
	}
	else
	{
		_x += ship->steering.sx;
		_d = 4 + ship->steering.sx;		// Will result in 3(West) or 5(East)

		ship->steering.move += ship->steering.ay;
		if( ship->steering.move >= ship->steering.ax )
		{
			ship->steering.move -= ship->steering.ax;

			_y += ship->steering.sy;
			_d += ship->steering.sy * 3;	// Shift to 0/2(North corner) or 6/8(South corner)
		}
	}

	// Only airships can fly over whereever
	if( ship->ship_type != SHIP_AIR_SHIP &&
		!check_for_bad_room(room->wilds,_x,_y) ) return false;

	*x = _x;
	*y = _y;
	*door = compasses[_d];
	return true;
}

/////////////////////////////////////////////////////////////////
//
// Ship Types
//

SHIP_INDEX_DATA *load_ship_index(FILE *fp)
{
	SHIP_INDEX_DATA *ship;
	char *word;
	bool fMatch;

	ship = new_ship_index();
	ship->vnum = fread_number(fp);

	if( ship->vnum > top_ship_index_vnum)
		top_ship_index_vnum = ship->vnum;

	while (str_cmp((word = fread_word(fp)), "#-SHIP"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case 'A':
			KEY("Armor", ship->armor, fread_number(fp));
			break;

		case 'B':
			if( !str_cmp(word, "Blueprint") )
			{
				long vnum = fread_number(fp);
				BLUEPRINT *bp = get_blueprint(vnum);

				if( bp )
				{
					ship->blueprint = bp;
				}

				fMatch = TRUE;
				break;
			}
			break;

		case 'C':
			KEY("Capacity", ship->capacity, fread_number(fp));
			KEY("Class", ship->ship_class, fread_number(fp));
			if( !str_cmp(word, "Crew") )
			{
				ship->min_crew = fread_number(fp);
				ship->max_crew = fread_number(fp);

				fMatch = TRUE;
				break;
			}
			break;

		case 'D':
			KEYS("Description", ship->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", ship->flags, fread_number(fp));
			break;

		case 'G':
			KEY("Guns", ship->guns, fread_number(fp));
			break;

		case 'H':
			KEY("Hit", ship->hit, fread_number(fp));
			break;

		case 'K':
			if( !str_cmp(word, "Key") )
			{
				long key_vnum = fread_number(fp);

				OBJ_INDEX_DATA *key = get_obj_index(key_vnum);
				if( key )
				{
					list_appendlink(ship->special_keys, key);
				}

				fMatch = TRUE;
				break;
			}
			break;

		case 'M':
			KEY("MoveDelay", ship->move_delay, fread_number(fp));
			KEY("MoveSteps", ship->move_steps, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", ship->name, fread_string(fp));
			break;

		case 'O':
			KEY("Object", ship->ship_object, fread_number(fp));
			break;

		case 'T':
			KEY("Turning", ship->turning, fread_number(fp));
			break;

		case 'W':
			KEY("Weight", ship->weight, fread_number(fp));
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_ship: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	return ship;
}

void load_ships()
{
	FILE *fp = fopen(SHIPS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't read ships.dat", 0);
		return;
	}

	char *word;
	bool fMatch;

	top_ship_index_vnum = 0;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if( !str_cmp(word, "#SHIP") )
		{
			SHIP_INDEX_DATA *ship = load_ship_index(fp);
			int iHash = ship->vnum % MAX_KEY_HASH;

			ship->next = ship_index_hash[iHash];
			ship_index_hash[iHash] = ship;

			fMatch = TRUE;
			continue;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_ships: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	fclose(fp);
}

void save_ship_index(FILE *fp, SHIP_INDEX_DATA *ship)
{
	ITERATOR it;
	OBJ_INDEX_DATA *obj;

	fprintf(fp, "#SHIP %ld\n", ship->vnum);

	fprintf(fp, "Name %s~\n", fix_string(ship->name));
	fprintf(fp, "Description %s~\n", fix_string(ship->description));
	fprintf(fp, "Class %d\n", ship->ship_class);
	fprintf(fp, "Flags %d\n", ship->flags);

	if( IS_VALID(ship->blueprint) )
		fprintf(fp, "Blueprint %ld\n", ship->blueprint->vnum);

	if( ship->ship_object > 0 )
		fprintf(fp, "Object %ld\n", ship->ship_object);

	fprintf(fp, "Hit %d\n", ship->hit);
	fprintf(fp, "Guns %d\n", ship->guns);
	fprintf(fp, "Crew %d %d\n", ship->min_crew, ship->max_crew);
	fprintf(fp, "MoveDelay %d\n", ship->move_delay);
	fprintf(fp, "MoveSteps %d\n", ship->move_steps);
	fprintf(fp, "Turning %d\n", ship->turning);
	fprintf(fp, "Weight %d\n", ship->weight);
	fprintf(fp, "Capacity %d\n", ship->capacity);
	fprintf(fp, "Armor %d\n", ship->armor);

	iterator_start(&it, ship->special_keys);
	while( (obj = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
	{
		if( obj->item_type == ITEM_KEY )
			fprintf(fp, "Key %ld\n", obj->vnum);
	}
	iterator_stop(&it);

	fprintf(fp, "#-SHIP\n\n");
}

bool save_ships()
{
	FILE *fp = fopen(SHIPS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save ships.dat", 0);
		return FALSE;
	}

	int iHash;

	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = ship_index_hash[iHash]; ship; ship = ship->next)
		{
			save_ship_index(fp, ship);
		}
	}

	fprintf(fp, "#END\n");
	fclose(fp);

	ships_changed = false;
	return true;
}

SHIP_INDEX_DATA *get_ship_index(long vnum)
{
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = ship_index_hash[iHash]; ship; ship = ship->next)
		{
			if( ship->vnum == vnum )
				return ship;
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////
//
// Ships
//

SHIP_DATA *create_ship(long vnum)
{
	OBJ_DATA *obj;						// Physical ship object
	OBJ_INDEX_DATA *obj_index;			// Ship object index to create
	SHIP_DATA *ship;					// Runtime ship data
	SHIP_INDEX_DATA *ship_index;		// Ship index to create
	INSTANCE *instance;
	ITERATOR it;

	// Verify the ship index exists
	if( !(ship_index = get_ship_index(vnum)) )
		return NULL;

	// Verify the object index exists and is a ship
	if( !(obj_index = get_obj_index(ship_index->ship_object)) )
		return NULL;

	if( obj_index->item_type != ITEM_SHIP )
	{
		char buf[MSL];
		sprintf(buf, "create_ship: attempting to use object (%ld) that is not a ship object for ship (%ld)", obj_index->vnum, ship_index->vnum);
		bug(buf, 0);
		return NULL;
	}

	ship = new_ship();
	if( !IS_VALID(ship) )
		return NULL;

	ship->index = ship_index;

	obj = create_object(obj_index, 0, FALSE);
	if( !IS_VALID(obj) )
	{
		free_ship(ship);
		return NULL;
	}
	ship->ship = obj;
	obj->ship = ship;

	instance = create_instance(ship_index->blueprint);
	if( !IS_VALID(instance) )
	{
		list_remlink(loaded_objects, obj);
		--obj->pIndexData->count;
		free_obj(obj);
		free_ship(ship);
		return NULL;
	}

	if( list_size(ship_index->special_keys) > 0 )
	{
		iterator_start(&it, ship_index->special_keys);
		OBJ_INDEX_DATA *key;
		while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
		{
			SPECIAL_KEY_DATA *sk = new_special_key();

			sk->key_vnum = key->vnum;
			list_appendlink(ship->special_keys, sk);
		}
		iterator_stop(&it);
		instance_apply_specialkeys(instance, ship->special_keys);
	}

	ship->instance = instance;
	instance->ship = ship;

	list_appendlink(loaded_instances, instance);

	ship->ship_type = ship_index->ship_class;
	ship->hit = ship_index->hit;
	ship->ship_flags = ship_index->flags;
	ship->armor = ship_index->armor;

	// Build cannons

	list_appendlink(loaded_ships, ship);

	get_ship_id(ship);
	return ship;
}

void extract_ship(SHIP_DATA *ship)
{
	if( !IS_VALID(ship) ) return;

	detach_ships_ship(ship);

	list_remlink(loaded_ships, ship);

	extract_instance(ship->instance);
	extract_obj(ship->ship);

	free_ship(ship);
}

bool ship_isowner_player(SHIP_DATA *ship, CHAR_DATA *ch)
{
	if( !IS_VALID(ship) ) return false;

	if( !IS_VALID(ch) ) return false;

	return ( (ship->owner_uid[0] == ch->id[0]) && (ship->owner_uid[1] == ch->id[1]) );
}

void ship_stop(SHIP_DATA *ship)
{
	ship->speed = SHIP_SPEED_STOPPED;
	ship->move_steps = 0;
	ship->steering.move = 0;
	ship->steering.turning_dir = 0;
	ship->ship_move = 0;
	ship->last_times[0] = current_time + 15;
	ship->last_times[1] = current_time + 10;
	ship->last_times[2] = current_time + 5;
	memset(&ship->seek_point, 0, sizeof(ship->seek_point));
}

bool move_ship_success(SHIP_DATA *ship)
{
	char buf[MSL];
	ROOM_INDEX_DATA *in_room = NULL;
	ROOM_INDEX_DATA *to_room = NULL;
	OBJ_DATA *obj;
	int door;
	int x;
	int y;

	obj = ship->ship;
	in_room = ship->ship->in_room;

	if( !in_room || !in_room->wilds )
		return false;

	if( !steering_movement(ship, &x, &y, &door) ) return false;

	if (x < 0 || x >= in_room->wilds->map_size_x ) return false;
	if (y < 0 || y >= in_room->wilds->map_size_y ) return false;

	if ( ship->ship_type != SHIP_AIR_SHIP )
	{
		WILDS_TERRAIN *pTerrain = get_terrain_by_coors(in_room->wilds, x, y);

		// Invalid terrain
		if( !pTerrain || pTerrain->nonroom )
			return false;

		EXIT_DATA *pexit = in_room->exit[door];
		bool vlink = (pexit && IS_SET(pexit->exit_info, EX_VLINK));

		if( (pTerrain->template->sector_type != SECT_WATER_NOSWIM &&
			pTerrain->template->sector_type != SECT_WATER_SWIM) || vlink )
		{
			ship_echo(ship, "The vessel has run aground.");
			ship_stop(ship);
			return false;
		}
	}

	to_room = get_wilds_vroom(in_room->wilds, x, y);
	if(!to_room)
		to_room = create_wilds_vroom(in_room->wilds,x, y);

	// TODO: Handle Weather

	// Save the wake of non-floating ships
	if ( ship->ship_type != SHIP_AIR_SHIP )
	{
		ship->last_coords[2] = ship->last_coords[1];
		ship->last_coords[1] = ship->last_coords[0];
		ship->last_coords[0].wilds = in_room->wilds;
		ship->last_coords[0].w = in_room->wilds->uid;
		ship->last_coords[0].x = in_room->x;
		ship->last_coords[0].y = in_room->y;
		ship->last_times[2] = current_time + 5;
		ship->last_times[1] = 0;
		ship->last_times[0] = 0;
	}

	switch(ship->ship_type)
	{
	case SHIP_AIR_SHIP:
		sprintf(buf, "{W%s flies away to the %s.{x\n\r", capitalize(obj->short_descr), dir_name[door]);
		break;

	default:
		sprintf(buf, "{W%s sails away to the %s.{x\n\r", capitalize(obj->short_descr), dir_name[door]);
		break;
	}
	room_echo(in_room, buf);

	obj_from_room(obj);
	obj_to_room(obj, to_room);

	switch(ship->ship_type)
	{
	case SHIP_AIR_SHIP:
		sprintf(buf, "{W%s flies in from the %s.{x\n\r", capitalize(obj->short_descr), dir_name[door]);
		break;

	default:
		sprintf(buf, "{W%s sails in from the %s.{x\n\r", capitalize(obj->short_descr), dir_name[door]);
		break;
	}
	room_echo(to_room, buf);

	return true;
}

double ln2_50 = log(2) / 50.0;	// I want 'a' to satisfy exp(a * 50) = 2

void ship_set_move_steps(SHIP_DATA *ship)
{
	if( ship->speed > SHIP_SPEED_STOPPED )
	{
		int speed = ship->speed;

		// TODO: Add some kind of modifiers for speed
		// - encumberance (cargo weight)
		// - damaged propulsion
		// - wind?
		// - relic modifier

		ship->move_steps = speed * ship->index->move_steps / 100;
		ship->move_steps = UMAX(1, ship->move_steps);
	}
	else
	{
		ship->move_steps = 0;
	}
	ship->ship_move = ship->index->move_delay;
}

void ship_move_update(SHIP_DATA *ship)
{
	if( ship->speed > SHIP_SPEED_STOPPED )
	{
		steering_update(ship);

		for( int i = 0; i < ship->move_steps; i++)
		{
			if( !move_ship_success(ship) ) return;
		}

		ship_autosurvey(ship);
	}
	else if( !ship->steering.turning_dir )
	{
		ship_echo(ship, "The vessel has stopped.");
		ship_stop(ship);
		return;
	}
	else
	{
		steering_update(ship);	// Stationary turning

		if( !ship->steering.turning_dir ) return;
	}

	ship_set_move_steps(ship);
}

void ship_pulse_update(SHIP_DATA *ship)
{
	if( !IS_VALID(ship) ) return;

	if( ship->ship_move > 0 )
	{
		if( !--ship->ship_move )
		{
			ship_move_update(ship);
		}
	}

	// Update the wake
	for( int i = 0; i < 3; i++ )
	{
		if( ship->last_times[i] > 0 && ship->last_times[i] < current_time )
		{
			ship->last_coords[i].wilds = NULL;
			ship->last_coords[i].x = 0;
			ship->last_coords[i].y = 0;
			ship->last_times[i] = 0;

		}
	}
}

// Called on the pulse
void ships_pulse_update()
{
	ITERATOR it;
	SHIP_DATA *ship;

	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		ship_pulse_update(ship);
	}
	iterator_stop(&it);
}

void ship_tick_update(SHIP_DATA *ship)
{
	// Update scuttling
	if( ship->scuttle_time > 0 )
	{
		if(!--ship->scuttle_time)
		{
			ship_echo(ship, "{R[INSERT SCUTTLE MESSAGE]{x");
			extract_ship(ship);
			return;
		}
	}
}

// Called on the tick
void ships_ticks_update()
{
	ITERATOR it;
	SHIP_DATA *ship;

	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		ship_tick_update(ship);
	}
	iterator_stop(&it);
}

SPECIAL_KEY_DATA *ship_special_key_load(FILE *fp)
{
	SPECIAL_KEY_DATA *sk;
	char *word;
	bool fMatch;

	sk = new_special_key();
	sk->key_vnum = fread_number(fp);

	while (str_cmp((word = fread_word(fp)), "#-SPECIALKEY"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case 'K':
			if( !str_cmp(word, "Key") )
			{
				LLIST_UID_DATA *luid = new_list_uid_data();
				luid->id[0] = fread_number(fp);
				luid->id[1] = fread_number(fp);

				list_appendlink(sk->list, luid);
			}
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "ship_special_key_load: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return sk;

}

INSTANCE *instance_load(FILE *fp);
OBJ_DATA *persist_load_object(FILE *fp);
SHIP_DATA *ship_load(FILE *fp)
{
	SHIP_DATA *ship;
	SHIP_INDEX_DATA *index;
	char *word;
	bool fMatch;

	index = get_ship_index(fread_number(fp));
	if( !index ) return NULL;

	ship = new_ship();
	ship->index = index;

	while (str_cmp((word = fread_word(fp)), "#-SHIP"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#INSTANCE") )
			{
				INSTANCE *instance = instance_load(fp);

				if( IS_VALID(instance) )
				{
					ship->instance = instance;
					ship->instance->ship = ship;
				}

				fMatch = TRUE;
				break;
			}
			if( !str_cmp(word, "#OBJECT") )
			{
				OBJ_DATA *obj = persist_load_object(fp);

				if( IS_VALID(obj) ) {
					ship->ship = obj;
					obj->ship = ship;
					obj_to_room(obj, obj->in_room);
				}

				fMatch = TRUE;
				break;
			}
			if( !str_cmp(word, "#SPECIALKEY") )
			{
				SPECIAL_KEY_DATA *sk = ship_special_key_load(fp);

				list_appendlink(ship->special_keys, sk);
				fMatch = TRUE;
				break;
			}
			break;

		case 'A':
			KEY("Armor", ship->armor, fread_number(fp));
			KEY("AttackPos", ship->attack_position, fread_number(fp));
			break;

		case 'B':
			if( !str_cmp(word, "BoardedBy") )
			{
				ship->boarded_by_uid[0] = fread_number(fp);
				ship->boarded_by_uid[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			break;

		case 'C':
			KEY("Cannons", ship->cannons, fread_number(fp));
			if( !str_cmp(word, "CharAttacked") )
			{
				ship->char_attacked_uid[0] = fread_number(fp);
				ship->char_attacked_uid[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			if( !str_cmp(word, "Crew") )
			{
				ship->min_crew = fread_number(fp);
				ship->max_crew = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			break;

		case 'F':
			KEYS("Flag", ship->flag, fread_string(fp));
			break;

		case 'H':
			KEY("Hit", ship->hit, fread_number(fp));
			break;

		case 'M':
			KEY("MoveSteps", ship->move_steps, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", ship->ship_name, fread_string(fp));
			break;

		case 'O':
			if( !str_cmp(word, "Owner") )
			{
				ship->owner_uid[0] = fread_number(fp);
				ship->owner_uid[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			break;

		case 'P':
			KEY("PK", ship->pk, true);
			break;

		case 'S':
			KEY("ScuttleTime", ship->scuttle_time, fread_number(fp));
			if(!str_cmp(word, "SeekPoint") )
			{
				long wuid = fread_number(fp);
				ship->seek_point.wilds = get_wilds_from_uid(NULL, wuid);
				ship->seek_point.w = wuid;
				ship->seek_point.x = fread_number(fp);
				ship->seek_point.y = fread_number(fp);

				if( !ship->seek_point.wilds )
				{
					memset(&ship->seek_point, 0, sizeof(ship->seek_point));
				}

				fMatch = TRUE;
				break;
			}
			if( !str_cmp(word, "ShipAttacked") )
			{
				ship->ship_attacked_uid[0] = fread_number(fp);
				ship->ship_attacked_uid[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			if( !str_cmp(word, "ShipChased") )
			{
				ship->ship_chased_uid[0] = fread_number(fp);
				ship->ship_chased_uid[1] = fread_number(fp);
				fMatch = TRUE;
				break;
			}
			KEY("ShipFlags", ship->ship_flags, fread_number(fp));
			KEY("ShipMove", ship->ship_move, fread_number(fp));
			KEY("Speed", ship->speed, fread_number(fp));
			if(!str_cmp(word, "Steering") )
			{
				ship->steering.heading = fread_number(fp);
				ship->steering.heading_target = fread_number(fp);
				ship->steering.turning_dir = (char)fread_number(fp);

				steering_calc_heading(ship);

				ship->steering.move = fread_number(fp);

				fMatch = TRUE;
				break;
			}
			break;

		case 'U':
			if( !str_cmp(word, "Uid") )
			{
				ship->id[0] = fread_number(fp);
				ship->id[1] = fread_number(fp);
			}
			break;

		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "ship_load: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	if( !IS_VALID(ship->ship) || !IS_VALID(ship->instance) )
	{
		extract_ship(ship);
		return NULL;
	}

	get_ship_id(ship);
	return ship;
}

void save_ship_uid(FILE *fp, char *field, unsigned long uid[2])
{
	if( uid[0] > 0 || uid[1] > 0 )
	{
		fprintf(fp, "%s %lu %lu\n", field, uid[0], uid[1]);
	}
}

void ship_special_key_save(FILE *fp, SPECIAL_KEY_DATA *sk)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;

	fprintf(fp, "#SPECIALKEY %ld\n", sk->key_vnum);

	iterator_start(&it, sk->list);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "Key %lu %lu\n", luid->id[0], luid->id[1]);

	}
	iterator_stop(&it);

	fprintf(fp, "#-SPECIALKEY\n");
}

void persist_save_object(FILE *fp, OBJ_DATA *obj, bool multiple);
bool ship_save(FILE *fp, SHIP_DATA *ship)
{
	ITERATOR it;
	SPECIAL_KEY_DATA *sk;

	fprintf(fp, "#SHIP %ld\n", ship->index->vnum);

	save_ship_uid(fp, "Uid", ship->id);

	fprintf(fp, "Name %s~\n", fix_string(ship->ship_name));

	if( ship->owner_uid[0] > 0 || ship->owner_uid[1] > 0 )
	{
		fprintf(fp, "Owner %lu %lu\n", ship->owner_uid[0], ship->owner_uid[1]);
	}

	fprintf(fp, "Flag %s~\n", fix_string(ship->flag));

	fprintf(fp, "Speed %d\n", ship->speed);
	fprintf(fp, "Hit %ld\n", ship->hit);
	fprintf(fp, "Armor %ld\n", ship->armor);

	fprintf(fp, "ShipFlags %d\n", ship->ship_flags);
	fprintf(fp, "Cannons %d\n", ship->cannons);
	fprintf(fp, "Crew %d %d\n", ship->min_crew, ship->max_crew);

	if( ship->ship_move > 0 )
	{
		fprintf(fp, "ShipMove %d\n", ship->ship_move);
	}

	if( ship->move_steps > 0 )
	{
		fprintf(fp, "MoveSteps %d\n", ship->move_steps);
	}

	fprintf(fp, "Steering %d %d %d %d\n",
		ship->steering.heading,
		ship->steering.heading_target,
		ship->steering.turning_dir,
		ship->steering.move);

	if( ship->seek_point.wilds != NULL )
	{
		fprintf(fp, "SeekPoint %ld %d %d\n",
			ship->seek_point.wilds->uid,
			ship->seek_point.x,
			ship->seek_point.y);
	}

	if( ship->pk )
	{
		fprintf(fp, "PK\n");
	}

	// TODO: Iterate over crew

	persist_save_object(fp, ship->ship, false);

	instance_save(fp, ship->instance);

	iterator_start(&it, ship->special_keys);
	while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&it)) )
	{
		ship_special_key_save(fp, sk);
	}
	iterator_stop(&it);

	fprintf(fp, "AttackPos %d\n", ship->attack_position);

	save_ship_uid(fp, "ShipAttacked", ship->ship_attacked_uid);
	save_ship_uid(fp, "CharAttacked", ship->char_attacked_uid);

	save_ship_uid(fp, "ShipChased", ship->ship_chased_uid);
	save_ship_uid(fp, "BoardedBy", ship->boarded_by_uid);

	fprintf(fp, "ScuttleTime %d\n", ship->scuttle_time);

	// TODO: Save proper destination
	// TODO: Save waypoints
	// TODO: Save current waypoint index
	// TODO: Save cannon

	fprintf(fp, "#-SHIP\n");
	return true;
}

void resolve_ships_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	SHIP_DATA *ship;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( uid_match(ship->owner_uid,ch->id) )
		{
			ship->owner = ch;
		}

		if( uid_match(ship->char_attacked_uid,ch->id) )
		{
			ship->char_attacked = ch;
		}
	}
	iterator_stop(&it);
}

void resolve_ships(void)
{
	ITERATOR it;
	SHIP_DATA *ship;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		ship->ship_attacked = get_ship_uid(ship->ship_attacked_uid);
		ship->ship_chased = get_ship_uid(ship->ship_chased_uid);
		ship->boarded_by = get_ship_uid(ship->boarded_by_uid);
	}
	iterator_stop(&it);
}

void detach_ships_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	SHIP_DATA *ship;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( ship->owner == ch ) ship->owner = NULL;
		if( ship->char_attacked == ch ) ship->char_attacked = NULL;
	}
	iterator_stop(&it);
}

void detach_ships_ship(SHIP_DATA *old_ship)
{
	ITERATOR it;
	SHIP_DATA *ship;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( ship->ship_attacked == old_ship ) ship->ship_attacked = NULL;
		if( ship->ship_chased == old_ship ) ship->ship_chased = NULL;
		if( ship->boarded_by == old_ship ) ship->boarded_by = NULL;
	}
	iterator_stop(&it);
}


SHIP_DATA *get_ship_uids(unsigned long id1, unsigned long id2)
{
	unsigned long id[2];
	id[0] = id1;
	id[1] = id2;

	return get_ship_uid(id);
}

SHIP_DATA *get_ship_uid(unsigned long id[2])
{
	ITERATOR it;
	SHIP_DATA *ship = NULL;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( uid_match(ship->id,id) )
		{
			break;
		}
	}
	iterator_stop(&it);

	return ship;
}

SHIP_DATA *get_ship_nearby(char *name, ROOM_INDEX_DATA *room, CHAR_DATA *owner)
{
	ITERATOR it;
	SHIP_DATA *ship = NULL;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( is_name(ship->ship_name, name) &&
			ship->ship->in_room == room &&
			ship->owner != owner )
			break;
	}
	iterator_stop(&it);

	return ship;
}

bool is_ship_safe(CHAR_DATA *ch, SHIP_DATA *ship, SHIP_DATA *ship2)
{
	if( IS_SET(ship->ship_flags, SHIP_PROTECTED) )
		return true;

	if ( ship2 == NULL )
	{
		if( !ship->ship->in_room )
			return false;

		if( IS_SET(ship->ship->in_room->room2_flags, ROOM_SAFE_HARBOR) )
			return true;

		return false;
	}


	return true;
}

SHIP_DATA *get_room_ship(ROOM_INDEX_DATA *room)
{
	if( !room ) return NULL;
	if( !IS_VALID(room->instance_section) ) return NULL;
	if( !IS_VALID(room->instance_section->instance) ) return NULL;
	if( !IS_VALID(room->instance_section->instance->ship) ) return NULL;

	return room->instance_section->instance->ship;
}

bool ischar_onboard_ship(CHAR_DATA *ch, SHIP_DATA *ship)
{
	if( !IS_VALID(ch) || !ch->in_room ) return false;
	if( !IS_VALID(ship) ) return false;

	return get_room_ship(ch->in_room) == ship;
}

void get_ship_wildsicon(SHIP_DATA *ship, char *buf, size_t len)
{
	if( IS_NPC_SHIP(ship) )
	{
		if( ship->scuttle_time > 0 )
			strncpy(buf, "{rO{x", len);
		else
			strncpy(buf, "{DO{x", len);
	}
	else
	{
		if( ship->scuttle_time > 0 )
			strncpy(buf, "{RO{x", len);
		else
			strncpy(buf, "{WO{x", len);
	}

	buf[len] = '\0';
}

void ship_autosurvey( SHIP_DATA *ship )
{
	DESCRIPTOR_DATA *d;

	for ( d = descriptor_list; d != NULL; d = d->next )
	{
		CHAR_DATA *victim;

		victim = d->original ? d->original : d->character;

		if( d->connected == CON_PLAYING &&
			IS_SET(victim->act2, PLR_AUTOSURVEY) &&
			victim->in_room != NULL &&
			ischar_onboard_ship(victim, ship) &&
			IS_AWAKE(victim) &&
			(IS_OUTSIDE(victim) ||
				IS_SET(victim->in_room->room_flags, ROOM_SHIP_HELM) ||
				IS_SET(victim->in_room->room_flags, ROOM_VIEWWILDS)) )
		{
			do_function(victim, &do_survey, "auto" );
		}
	}
}


void ship_echo( SHIP_DATA *ship, char *str )
{
	DESCRIPTOR_DATA *d;

	for ( d = descriptor_list; d != NULL; d = d->next )
	{
		CHAR_DATA *victim;

		victim = d->original ? d->original : d->character;

		if( d->connected == CON_PLAYING &&
			victim->in_room != NULL &&
			ischar_onboard_ship(victim, ship) )
			act(str, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
}

void ship_echoaround( SHIP_DATA *ship, CHAR_DATA *ch, char *str )
{
	DESCRIPTOR_DATA *d;

	for ( d = descriptor_list; d != NULL; d = d->next )
	{
		CHAR_DATA *victim;

		victim = d->original ? d->original : d->character;

		if( d->connected == CON_PLAYING &&
			victim != ch &&
			victim->in_room != NULL &&
			ischar_onboard_ship(victim, ship) )
			act(str, victim, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
}


bool ship_has_enough_crew( SHIP_DATA *ship )
{
	// TODO: Implement crew

	return true;
}


void do_ships(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	SHIP_DATA *ship;

	if( IS_IMMORTAL(ch) )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ships list[ player]\n\r", ch);
			send_to_char("         ships load [vnum] [owner] [name]\n\r", ch);
			// TODO: NPC ship handling
			send_to_char("         ships unload [#]\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg);

		if( !str_prefix(arg, "list") )
		{
			CHAR_DATA *owner = get_player(argument);

			if(!ch->lines)
				send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

			int lines = 0;
			bool error = FALSE;
			BUFFER *buffer = new_buf();
			ITERATOR it;
			char buf[MSL];

			iterator_start(&it, loaded_ships);
			while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
			{
				if( owner && ship->owner != owner )
					continue;

				char dir[20];
				if( ship->steering.heading < 0 )
				{
					dir[0] = '\0';
				}
				else
				{
					switch(ship->steering.heading)
					{
					case 0:		strcpy(dir, "North"); break;
					case 45:	strcpy(dir, "Northeast"); break;
					case 90:	strcpy(dir, "East"); break;
					case 135:	strcpy(dir, "Southeast"); break;
					case 180:	strcpy(dir, "South"); break;
					case 225:	strcpy(dir, "Southwest"); break;
					case 270:	strcpy(dir, "West"); break;
					case 315:	strcpy(dir, "Northwest"); break;
					default:
						sprintf(dir, "%d",ship->steering.heading);
						break;
					}
				}

				int snwidth = get_colour_width(ship->ship_name) + 30;


				sprintf(buf, "{W%4d{x)  {G%8ld  {x%-*.*s   {x%-20.20s{x  %d %d %d %d %d %d %d %d %d %s\n\r",
					++lines,
					ship->index->vnum,
					snwidth, snwidth, ship->ship_name,
					ship->owner ? ship->owner->name : "{DNone",
					ship->speed, ship->move_steps, ship->ship_move,
					ship->steering.heading, ship->steering.heading_target, ship->steering.turning_dir,
					ship->steering.dx, ship->steering.dy, ship->steering.move, dir);

				if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
				{
					error = TRUE;
					break;
				}
			}
			iterator_stop(&it);


			if( error )
			{
				send_to_char("Too many ships to list.  Please shorten!\n\r", ch);
			}
			else
			{
				if( !lines )
				{
					add_buf( buffer, "No ships to display.\n\r" );
				}
				else
				{
					// Header
					send_to_char("{Y      [  Vnum  ] [             Name             ]  Owner{x\n\r", ch);
					send_to_char("{Y========================================================================{x\n\r", ch);
				}

				page_to_char(buffer->string, ch);
			}
			free_buf(buffer);
		}
		else if( !str_prefix(arg, "load") )
		{
			char buf[2*MSL];
			char arg2[MIL];
			char arg3[MIL];
			long vnum;

			argument = one_argument(argument, arg2);
			argument = one_argument(argument, arg3);

			if( !is_number(arg2) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return;
			}

			vnum = atol(arg2);

			SHIP_INDEX_DATA *index = get_ship_index(vnum);

			if( !index )
			{
				send_to_char("That ship does not exist.\n\r", ch);
				return;
			}

			if( index->ship_class == SHIP_SAILING_BOAT )
			{
				if( !IS_WILDERNESS(ch->in_room) )
				{
					send_to_char("Must be in the wilderness.\n\r", ch);
					return;
				}

				if( ch->in_room->sector_type != SECT_WATER_SWIM &&
					ch->in_room->sector_type != SECT_WATER_NOSWIM )
				{
					send_to_char("Must be in the water.\n\r", ch);
					return;
				}
			}
			else if( index->ship_class == SHIP_AIR_SHIP )
			{
				if( !IS_OUTSIDE(ch) )
				{
					send_to_char("Must be outside.\n\r", ch);
					return;
				}
			}

			CHAR_DATA *owner = get_player(arg3);

			if( owner )
			{
				sprintf(buf, "Ship Owner: %s\n\r", owner->name);
			}
			else
			{
				sprintf(buf, "Ship Owner: '%s' not found\n\r", arg3);
			}
			send_to_char(buf, ch);

			ship = create_ship(vnum);

			if( !IS_VALID(ship) )
			{
				send_to_char("Failed to create ship.\n\r", ch);
				return;
			}

			ship->owner = owner;
			if( owner )
			{
				ship->owner_uid[0] = owner->id[0];
				ship->owner_uid[1] = owner->id[1];
			}

			free_string(ship->ship_name);
			ship->ship_name = str_dup(argument);

			// Install ship_name
			char *plaintext = nocolour(ship->ship_name);
			free_string(ship->ship->name);
			sprintf(buf, ship->ship->pIndexData->name, plaintext);
			ship->ship->name = str_dup(buf);
			free_string(plaintext);

			free_string(ship->ship->short_descr);
			sprintf(buf, ship->ship->pIndexData->short_descr, ship->ship_name);
			ship->ship->short_descr = str_dup(buf);

			free_string(ship->ship->description);
			sprintf(buf, ship->ship->pIndexData->description, ship->ship_name);
			ship->ship->description = str_dup(buf);

			obj_to_room(ship->ship, ch->in_room);
			act("$p splashes down after being christened '$T'.",ch, NULL, NULL,ship->ship, NULL, NULL,ship->ship_name,TO_ALL);
		}
		else if( !str_prefix(arg, "unload") )
		{
			// Require that the ship being unloaded is not a special ship:
			//   Endeavour
			//   Goblin Airship
			//


		}

		return;
	}
	else
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ships list\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg);

		return;
	}


	do_ships(ch, "");
}

bool ship_can_issue_command(CHAR_DATA *ch, SHIP_DATA *ship)
{
	// Search crew for FIRST MATE

	if (!IS_SET(ch->in_room->room_flags, ROOM_SHIP_HELM))
	{
		return false;
	}

	return true;
}



void do_ship_scuttle( CHAR_DATA *ch, char *argument)
{
//	ROOM_INDEX_DATA *location;
//    OBJ_DATA *ship_obj;
    SHIP_DATA *ship;
//	char buf[MSL];

	ship = get_room_ship(ch->in_room);

    if( !IS_VALID(ship) )
	{
		send_to_char("You are not on a ship.\n\r", ch);
		return;
	}

	if( ship->scuttle_time > 0 )
	{
		send_to_char("The vessel has already been scuttled!\n\r", ch);
		return;
	}

	// is NPC ship
	if ( IS_NPC_SHIP(ship) )
	{
		// NYI
		send_to_char("Not yet implemented.\n\r", ch);
		return;
	}
	else if( ship->owner != ch )
	{
		if( is_ship_safe(ch, ship, NULL) )
		{
			send_to_char("The vessel is in safe harbor.\n\r", ch);
			return;
		}

		// Check to see if the owner of the ship is onboard
	}

	send_to_char("You douse the vessel with fuel and ignite it!\n\r", ch);
	ship_echoaround(ship, ch, "$N douses the vessel with fuel and ignites it!");

	ship->scuttle_time = 5;

	// TODO: STOP BOARDING
#if 0
	if ( ship->boarded_by != NULL )
	{
		ship->boarded_by->ship_attacked = NULL;
		ship->boarded_by->ship_chased = NULL;
		ship->boarded_by->destination = NULL;
	}
	ship->ship_attacked = NULL;
	ship->ship_chased = NULL;
	ship->destination = NULL;

	stop_boarding(ship);
#endif


#if 0

	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *temp_char;
	SHIP_DATA *ship;

	/* Is player on a boat? */
	if ( ( ship = ch->in_room->ship ) == NULL )
	{
	send_to_char("You aren't on a vessel.\n\r", ch);
	return;
	}

	if ( ship->scuttle_time > 0 )
	{
	send_to_char("The vessel has already been scuttled!\n\r", ch);
	return;
	}

	if ( is_boat_safe( ch, ship, ship ) )
	{
	send_to_char("The vessel is docked in a protected area.\n\r", ch);
	return;
	}

	/* Is player on a boarded boat? */
	if ( IS_NPC_SHIP(ship) )
	{
	/* Is captain on the ship or dead */
	if ( ship->npc_ship->captain != NULL )
	{
	send_to_char("You can't scuttle this vessel, the captain is not dead yet!\n\r", ch);
	return;
	}
	}
	else
	{
	for ( temp_char = ship->crew_list; temp_char != NULL; temp_char = temp_char->next_in_crew)
	{
	if ( temp_char == ship->owner )
	{
	break;
	}
	}

	if ( temp_char != NULL )
	{
	send_to_char("The captain is still on the vessel!\n\r", ch);
	return;
	}
	}

	sprintf(buf, "%s douses the vessel with fuel and ignites it!", ch->name);
	boat_echo(ship, buf);

	if ( ( ship->ship->in_room->vnum == ROOM_VNUM_SEA_PLITH_HARBOUR ||
	ship->ship->in_room->vnum == ROOM_VNUM_SEA_SOUTHERN_HARBOUR ||
	ship->ship->in_room->vnum == ROOM_VNUM_SEA_NORTHERN_HARBOUR ) &&
	ship->owner != ch )
	{
	boat_echo(ship, "{MThe flames are extinguished by a mysterious protective magic.{x");
	return;
	}

	ship->scuttle_time = 5;

	if ( ship->boarded_by != NULL )
	{
	ship->boarded_by->ship_attacked = NULL;
	ship->boarded_by->ship_chased = NULL;
	ship->boarded_by->destination = NULL;
	}
	ship->ship_attacked = NULL;
	ship->ship_chased = NULL;
	ship->destination = NULL;

	stop_boarding(ship);
#endif
}


void do_ship_steer( CHAR_DATA *ch, char *argument )
{
	char buf[MSL];
	char arg[MIL];
	char arg2[MIL];
	SHIP_DATA *ship;
	int heading;

	argument = one_argument( argument, arg);
	argument = one_argument( argument, arg2);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( !ship_can_issue_command(ch, ship) )
	{
		act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		act("The wheel is magically locked. This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (arg[0] == '\0')
	{
		// For now.. tell the bearing
		//  - Maybe adding a compass item?

		if( ship->speed > SHIP_SPEED_STOPPED )
		{
			switch(ship->steering.heading)
			{
			case 0:		strcpy(arg, "heading to the north"); break;
			case 45:	strcpy(arg, "heading to the northeast"); break;
			case 90:	strcpy(arg, "heading to the east"); break;
			case 135:	strcpy(arg, "heading to the southeast"); break;
			case 180:	strcpy(arg, "heading to the south"); break;
			case 225:	strcpy(arg, "heading to the southwest"); break;
			case 270:	strcpy(arg, "heading to the west"); break;
			case 315:	strcpy(arg, "heading to the northwest"); break;
			default:
				sprintf(arg, "heading toward %d degrees", ship->steering.heading);
			}

			if( ship->steering.turning_dir < 0 )
				strcpy(arg2, ", and is turning to port");
			else if( ship->steering.turning_dir > 0 )
				strcpy(arg2, ", and is turning to starboard");
			else
				arg2[0] = '\0';
		}
		else
		{
			switch(ship->steering.heading)
			{
			case 0:		strcpy(arg, "stationary pointing to the north"); break;
			case 45:	strcpy(arg, "stationary pointing to the northeast"); break;
			case 90:	strcpy(arg, "stationary pointing to the east"); break;
			case 135:	strcpy(arg, "stationary pointing to the southeast"); break;
			case 180:	strcpy(arg, "stationary pointing to the south"); break;
			case 225:	strcpy(arg, "stationary pointing to the southwest"); break;
			case 270:	strcpy(arg, "stationary pointing to the west"); break;
			case 315:	strcpy(arg, "stationary pointing to the northwest"); break;
			default:
				sprintf(arg, "stationary pointing toward %d degrees", ship->steering.heading);
			}

			if( ship->steering.turning_dir < 0 )
				strcpy(arg2, ", but is turning to port");
			else if( ship->steering.turning_dir > 0 )
				strcpy(arg2, ", but is turning to starboard");
			else
				arg2[0] = '\0';
		}

		sprintf(buf, "The vessel is %s%s.\n\r", arg, arg2);
		send_to_char(buf, ch);
		//send_to_char("Steer which way?\n\r", ch);
		return;
	}

    if ( !ship_has_enough_crew( ch->in_room->ship ) )
    {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
    }

	if( ship->ship_type == SHIP_AIR_SHIP && ship->speed == SHIP_SPEED_LANDED )
	{
		send_to_char( "The vessel needs to be airborne first.\n\r  Try 'ship launch' to go airborne.\n\r", ch );
		return;
	}

	if( is_number(arg) )
	{
		heading = atoi(arg);

		if( heading < 0 || heading >= 360 )
		{
			send_to_char("That isn't a valid direction.\n\r", ch);
			return;
		}
	}
	else if( !str_prefix(arg, "port") )
	{
		if( ship->steering.turning_dir != -1 )
		{
			ship->steering.turning_dir = -1;
			ship->steering.heading_target = -1;	// Aimless

			ship_echo(ship, "{WThe vessel is now turning to port.");

			// Allow stationary turning
			if( ship->speed == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
			{
				ship->ship_move = ship->index->move_delay;
			}

		}
		else
		{
			send_to_char("The vessel is already turning to port.\n\r", ch);
		}
		return;
	}
	else if( !str_prefix(arg, "starboard") )
	{
		if( ship->steering.turning_dir != 1 )
		{
			ship->steering.turning_dir = 1;
			ship->steering.heading_target = -1;	// Aimless

			ship_echo(ship, "{WThe vessel is now turning to starboard.");

			// Allow stationary turning
			if( ship->speed == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
			{
				ship->ship_move = ship->index->move_delay;
			}
		}
		else
		{
			send_to_char("The vessel is already turning to starboard.\n\r", ch);
		}
		return;
	}
	else if( !str_prefix(arg, "ahead") || !str_prefix(arg, "straight") )
	{
		if( ship->steering.turning_dir != 0 )
		{
			ship->steering.turning_dir = 0;
			ship->steering.heading_target = ship->steering.heading;

			ship_echo(ship, "{WThe vessel is now heading straight ahead.");
		}
		else
		{
			send_to_char("The vessel is already heading straight ahead.\n\r", ch);
		}
		return;
	}
	else
	{
		heading = parse_direction(arg);

		switch(heading)
		{
		case DIR_NORTH:				heading = 0; break;
		case DIR_NORTHEAST:			heading = 45; break;
		case DIR_EAST:				heading = 90; break;
		case DIR_SOUTHEAST:			heading = 135; break;
		case DIR_SOUTH:				heading = 180; break;
		case DIR_SOUTHWEST:			heading = 225; break;
		case DIR_WEST:				heading = 270; break;
		case DIR_NORTHWEST:			heading = 315; break;
		default:
			send_to_char("That isn't a valid direction.\n\r", ch);
			return;
		}
	}

	switch(heading)
	{
	case 0:		strcpy(arg, "to the north"); break;
	case 45:	strcpy(arg, "to the northeast"); break;
	case 90:	strcpy(arg, "to the east"); break;
	case 135:	strcpy(arg, "to the southeast"); break;
	case 180:	strcpy(arg, "to the south"); break;
	case 225:	strcpy(arg, "to the southwest"); break;
	case 270:	strcpy(arg, "to the west"); break;
	case 315:	strcpy(arg, "to the northwest"); break;
	default:
		sprintf(arg, "toward %d degrees", heading);
	}

	char turning_dir = 0;

	if( arg2[0] != '\0' )
	{
		if( !str_prefix(arg2, "port") )
			turning_dir = -1;
		else if(!str_prefix(arg2, "starboard") )
			turning_dir = 1;
	}

	int delta = heading - ship->steering.heading;
	if( delta > 180 ) delta -= 360;
	else if( delta <= -180 ) delta += 360;

	if( !turning_dir )
	{
		// Going in opposite direction? Need to specify which way to turn
		if( delta == 180 )	// Delta should never be -180, so only need to check this
		{
			send_to_char("Turning direction ambiguous.\n\rPlease specify whether to go 'port' or 'starboard'.\n\r", ch);
			return;
		}

		turning_dir = (delta < 0) ? -1 : 1;
	}

	steering_set_heading(ship, heading);
	steering_set_turning(ship, turning_dir);

	// TODO: Cancel waypoint
	// TODO: Cancel chasing

	sprintf(buf, "{WThe vessel is now turning %s.{x", arg);
	ship_echo(ship, buf);

	// Allow stationary turning
	if( ship->speed == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
	{
		ship->ship_move = ship->index->move_delay;
	}
}


void do_ship_speed( CHAR_DATA *ch, char *argument )
{
	char arg[MAX_INPUT_LENGTH];
	SHIP_DATA *ship;

	argument = one_argument( argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( arg[0] == '\0' )
	{
		int speed;

		if( ship->index->move_steps > 0 )
		{
			speed = 100 * ship->move_steps / ship->index->move_steps;
			speed = UMAX(0, speed);
		}
		else
			speed = -1;

		if( speed < 0 )
		{
			send_to_char("The vessel is unable to move.\n\r", ch);
		}
		else if( speed == SHIP_SPEED_STOPPED )
		{
			send_to_char("The vessel is stopped.\n\r", ch);
		}
		else if( speed > SHIP_SPEED_FULL_SPEED )
		{
			send_to_char("The vessel is going beyond its maximum speed.\n\r", ch);
		}
		else if( speed == SHIP_SPEED_FULL_SPEED )
		{
			send_to_char("The vessel is going at full speed.\n\r", ch);
		}
		else if( speed >= 90 )
		{
			send_to_char("The vessel is nearly going full speed.\n\r", ch);
		}
		else if( speed > 55 )
		{
			send_to_char("The vessel is going over half speed.\n\r", ch);
		}
		else if( speed >= 45 && ship->speed <= 55 )
		{
			send_to_char("The vessel is going about half speed.\n\r", ch);
		}
		else if( speed > 10 )
		{
			send_to_char("The vessel is going below half speed.\n\r", ch);
		}
		else
		{
			send_to_char("The vessel is going minimal speed.\n\r", ch);
		}


		return;
	}

	if( !ship_can_issue_command(ch, ship) )
	{
		act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		act("The wheel is magically locked. This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if ( !ship_has_enough_crew( ship ) ) {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
	}

	if ( ship->index->move_steps < 1 )
	{
		send_to_char( "The vessel doesn't seem to have enough power to move!\n\r", ch );
		return;
	}

	if( ship->ship_type == SHIP_AIR_SHIP && ship->speed == SHIP_SPEED_LANDED )
	{
		send_to_char( "The vessel needs to be airborne first.\n\r  Try 'ship launch' to go airborne.\n\r", ch );
		return;
	}

	int speed = -1;
	if ( is_percent(arg) )
	{
		speed = atoi(arg);
	}
	else if ( is_number(arg) )
	{
		speed = atoi(arg);

		if( speed < 1 || speed > ship->index->move_steps )
		{
			char buf[MSL];
			sprintf(buf, "Explicit distance values must be from 1 to %d\n\r", ship->index->move_steps);
			send_to_char(buf, ch);
			return;
		}

		speed = 100 * speed / ship->index->move_steps;
	}
	else if(!str_prefix(arg, "stop"))
	{
		speed = SHIP_SPEED_STOPPED;
	}
	else if(!str_prefix(arg, "minimal"))
	{
		speed = 1;
	}
	else if(!str_prefix(arg, "half"))
	{
		speed = SHIP_SPEED_HALF_SPEED;
	}
	else if(!str_prefix(arg, "full"))
	{
		speed = SHIP_SPEED_FULL_SPEED;
	}

	if( speed < 0 || speed > 100 )
	{
		send_to_char("You may stop the vessel, or order half, full, some percentage speed or specific distance count.\n\r", ch);
		return;
	}

	if( speed == SHIP_SPEED_STOPPED )
	{
		if( ship->speed > SHIP_SPEED_STOPPED )
		{
			switch(ship->ship_type)
			{
			case SHIP_AIR_SHIP:
				act("You give the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n gives the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				break;

			default:
				act("You give the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n gives the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				break;
			}
			ship->speed = speed;
			ship_stop(ship);

			// TODO: Cancel waypoint
			// TODO: Cancel chasing
		}
		else
		{
			send_to_char("Ship is already stopped.\n\r", ch);
		}
		return;
	}

	if( speed == SHIP_SPEED_FULL_SPEED )
	{
		if( ship->speed < SHIP_SPEED_FULL_SPEED )
		{
			act("You give the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->speed = speed;
			ship_set_move_steps(ship);
		}
		else
		{
			send_to_char("Ship is already going at full speed.\n\r", ch);
		}
		return;
	}

	if( ship->speed > speed )
	{
		act("You give the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else if( ship->speed < speed )
	{
		act("You give the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	else
	{
		send_to_char("Ship is already going at that speed.\n\r", ch);
		return;
	}

	ship->speed = URANGE(1,speed,100);
	ship_set_move_steps(ship);

    ship_autosurvey(ship);
}

void do_ship_aim( CHAR_DATA *ch, char *argument )
{
	send_to_char("Not yet implemented.\n\r", ch);

#if 0
	char arg[MAX_INPUT_LENGTH];
	char buf[MAX_STRING_LENGTH];
	ROOM_INDEX_DATA *orig;
	SHIP_DATA *orig_ship;
	SHIP_DATA *ship;
	SHIP_DATA *attack;
	CHAR_DATA *victim;
	int x, y;

	argument = one_argument( argument, arg);

	if (!ON_SHIP(ch))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_SET(ch->in_room->room_flags, ROOM_SHIP_HELM))
	{
		act("You must be at the helm of the vessel to order an attack.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && str_cmp(ch->name, ch->in_room->ship->owner_name))
	{
		act("You must be the owner to order an attack.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if ( !ship_has_enough_crew( ch->in_room->ship ) ) {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
	}

	if ( !str_prefix( arg, "stop" ) )
	{
		act("You give the order to cease the attack.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to cease the attack.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		ch->in_room->ship->speed = SHIP_SPEED_STOPPED;
		return;
	}

	orig_ship = ch->in_room->ship;

	orig = ch->in_room;
	char_from_room(ch);
	char_to_room(ch, orig->ship->ship->in_room);

	show_map_to_char(ch, ch, ch->wildview_bonus_x, ch->wildview_bonus_y,FALSE);

	x = get_squares_to_show_x(ch->wildview_bonus_x);
	y = get_squares_to_show_y(ch->wildview_bonus_y);

	attack = NULL;

	victim = get_char_world( ch, arg);

	if (!(victim != NULL && IN_WILDERNESS(victim) && !is_safe(ch, victim, TRUE) &&
				(victim->in_room->x < ch->in_room->x + x &&
				 victim->in_room->x > ch->in_room->x - x)
				&& (victim->in_room->y < ch->in_room->y + y &&
					victim->in_room->y > ch->in_room->y - y)))
	{
		victim = NULL;
		/* Check for sailing ships */
		for ( ship = ((AREA_DATA *) get_sailing_boat_area())->ship_list;
				ship != NULL;
				ship = ship->next)

			if ( orig_ship != ship
					&& (ship->ship->in_room->x < ch->in_room->x + x &&
						ship->ship->in_room->x > ch->in_room->x - x)
					&& (ship->ship->in_room->y < ch->in_room->y + y &&
						ship->ship->in_room->y > ch->in_room->y - y) &&
					(!str_prefix( ship->owner_name, arg)
					 || !str_prefix( ship->ship_name, arg)))
			{
				attack = ship;
			}
	}

	if (attack == NULL && victim == NULL)
	{
		send_to_char("That person or ship is not in range.\n\r", ch);
		char_from_room(ch);
		char_to_room(ch, orig);
		return;
	}

	/* Make sure the enemey ship isn't in a safe zone */
	if ( attack != NULL && is_boat_safe( ch, orig_ship, attack ) )
	{
		char_from_room(ch);
		char_to_room(ch, orig);
		return;
	}

	char_from_room(ch);
	char_to_room(ch, orig);

	act("You give the order to fire the cannons.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n gives the order to fire the cannons.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	SHIP_ATTACK_STATE(ch, 8);

	ch->in_room->ship->attack_position = SHIP_ATTACK_LOADING;
	ch->in_room->ship->ship_attacked = attack;
	ch->in_room->ship->char_attacked = victim;

	sprintf(buf, "{W%s's sailing boat is turning to aim at you!{x", ch->in_room->ship->owner_name);

	if (attack != NULL)
	{
		boat_echo(attack, buf);

		/*  If you are in range of coast guard or attacking coast guard then pirate */
		if (IS_NPC_SHIP(attack) && attack->npc_ship->pShipData->npc_sub_type == NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA) {
			/* set_pirate_status(ch, CONT_SERALIA, ch->tot_level * 1000); */
		}
		else
			if (IS_NPC_SHIP(attack) && attack->npc_ship->pShipData->npc_sub_type == NPC_SHIP_SUB_TYPE_COAST_GUARD_ATHEMIA) {
			/*	set_pirate_status(ch, CONT_ATHEMIA, ch->tot_level * 1000); */
			}
			else {
				NPC_SHIP_DATA *npc_ship;
				int distance = 0;

				for (npc_ship = npc_ship_list; npc_ship != NULL; npc_ship = npc_ship->next)
				{
					if (npc_ship->pShipData->npc_sub_type == NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA ||
							npc_ship->pShipData->npc_sub_type == NPC_SHIP_SUB_TYPE_COAST_GUARD_ATHEMIA) {

						/* get distance between coast guard and ship to attack */
						distance = (int) sqrt( 					\
								( npc_ship->ship->ship->in_room->x - ch->in_room->ship->ship->in_room->x ) *	\
								( npc_ship->ship->ship->in_room->x - ch->in_room->ship->ship->in_room->x ) +	\
								( npc_ship->ship->ship->in_room->y - ch->in_room->ship->ship->in_room->y ) *	\
								( npc_ship->ship->ship->in_room->y - ch->in_room->ship->ship->in_room->y ) );

						if (distance < 6) {
							break;
						}
					}
				}
/*
				 coast guard ship saw attack
				if (npc_ship != NULL) {
					set_pirate_status(ch, npc_ship->pShipData->npc_sub_type == NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA ? CONT_SERALIA : CONT_ATHEMIA, ch->tot_level * 1000);

				}*/
		}

	}
	else
	{
		act(buf, victim, NULL, NULL, TO_CHAR);
	}
#else
	send_to_char("Not implemented yet.\n\r", ch);
#endif
}

void do_ship_navigate(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg[MIL];
	SHIP_DATA *ship;
	ROOM_INDEX_DATA *room;
	WILDS_DATA *wilds;

	argument = one_argument(argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( !ship_can_issue_command(ch, ship) )
	{
		act("You must be at the helm of the vessel to navigate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		act("The wheel is magically locked. This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if ( !ship_has_enough_crew( ship ) ) {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
	}

	// TODO: Check for navigator or player "navigation" skill

	if( arg[0] == '\0' )
	{
		send_to_char("Navigate where?\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "seek") )
	{
		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Seek what point?\n\r"
						 "Syntax: navigate seek [south] [east]\n\r", ch);
			return;
		}

		room = obj_room(ship->ship);
		if( !IS_WILDERNESS(room) )
		{
			act("The vessel is not in the wilderness.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		wilds = room->wilds;

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) || !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}
		int south = atoi(arg2);
		int east = atoi(argument);

		if( east < 0 || east >= wilds->map_size_x )
		{
			sprintf(buf, "East coordinate is out of bounds.  Range: 0 to %d\n\r", wilds->map_size_x - 1);
			send_to_char(buf, ch);
			return;
		}

		if( south < 0 || south >= wilds->map_size_y )
		{
			sprintf(buf, "South coordinate is out of bounds.  Range: 0 to %d\n\r", wilds->map_size_y - 1);
			send_to_char(buf, ch);
			return;
		}

		ship->seek_point.wilds = wilds;
		ship->seek_point.w = wilds->uid;
		ship->seek_point.x = east;
		ship->seek_point.y = south;

		send_to_char("{WSeek point set.{x\n\r", ch);
		return;
	}

	do_ship_navigate(ch, "");
}

void do_ship_christen(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	SHIP_DATA *ship;

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	if( !IS_NULLSTR(ship->ship_name) )
	{
		send_to_char("The vessel already has a name!\n\r", ch);
		return;
	}

	free_string(ship->ship_name);
	ship->ship_name = str_dup(argument);

	// Install ship_name
	char *plaintext = nocolour(ship->ship_name);
	free_string(ship->ship->name);
	sprintf(buf, ship->ship->pIndexData->name, plaintext);
	ship->ship->name = str_dup(buf);
	free_string(plaintext);

	free_string(ship->ship->short_descr);
	sprintf(buf, ship->ship->pIndexData->short_descr, ship->ship_name);
	ship->ship->short_descr = str_dup(buf);

	free_string(ship->ship->description);
	sprintf(buf, ship->ship->pIndexData->description, ship->ship_name);
	ship->ship->description = str_dup(buf);

	act("{Y$n christens the vessel '{x$T{Y'.{x", ch, NULL, NULL, NULL, NULL, NULL, ship->ship_name, TO_ROOM);
	act("{YYou christen the vessel '{x$T{Y'.{x", ch, NULL, NULL, NULL, NULL, NULL, ship->ship_name, TO_CHAR);
}

void do_ship_land(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);

	if( !IS_VALID(ship) )
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->ship_type != SHIP_AIR_SHIP )
	{
		send_to_char("Land?  The vessel can only be in water.\n\r", ch);
		return;
	}

	if( !ship_can_issue_command(ch, ship) )
	{
		act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		act("The wheel is magically locked. This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if ( !ship_has_enough_crew( ship ) ) {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
	}

	if( ship->speed > SHIP_SPEED_STOPPED )
	{
		send_to_char("Please stop the vessel first, lest you crash.\n\r", ch);
		return;
	}

	if( ship->speed == SHIP_SPEED_LANDED )
	{
		send_to_char("The vessel has already landed.\n\r", ch);
		return;
	}

	WILDS_TERRAIN *pTerrain = get_terrain_by_coors(ship->ship->in_room->wilds, ship->ship->in_room->x, ship->ship->in_room->y);

	ROOM_INDEX_DATA *to_room = NULL;
	AREA_DATA *bestArea = NULL;
	if( !pTerrain || pTerrain->nonroom )
	{
		// Indicative of the terrain being part of some city region on the wilds
		if( pTerrain->template->sector_type != SECT_CITY )
		{
			send_to_char("The vessel cannot land here.\n\r", ch);
			return;
		}

		int bestDistanceSq = 400;	// 20 distance radius

		long uid = ship->ship->in_room->wilds->uid;
		int x = ship->ship->in_room->x;
		int y = ship->ship->in_room->y;

		for( AREA_DATA *area = area_first; area; area = area->next )
		{
			if( area->wilds_uid != uid || area->airship_land_spot < 1 ) continue;

			int distanceSq = ( x - area->x ) * ( x - area->x ) + ( y - area->y ) * ( y - area->y );

			if( distanceSq < bestDistanceSq )
			{
				bestDistanceSq = distanceSq;
				bestArea = area;
			}
		}

		if( bestArea == NULL )
		{
			send_to_char("The vessel cannot land here.\n\r", ch);
			return;
		}

		to_room = get_room_index(bestArea->airship_land_spot);
		if( !to_room )
		{
			send_to_char("There is no safe place to land the ship here.\n\r", ch);
			return;
		}
	}



	ship->speed = SHIP_SPEED_LANDED;

	char buf[MSL];
	if( to_room && bestArea )
	{
		sprintf(buf, "{WThe vessel descends down to {Y%s{W below.{x", bestArea->name);
		ship_echo(ship, buf);

		if( IS_NULLSTR(ship->ship_name) )
		{
			sprintf(buf, "{W%s %s descends from above to land in {Y%s{W.{x", get_article(ship->index->name, true), ship->index->name, bestArea->name);
		}
		else
		{
			sprintf(buf, "{WThe %s '{x%s{W' descends from above to land in {Y%s{W.{x", ship->index->name, ship->ship_name, bestArea->name);
		}
		room_echo(ship->ship->in_room, buf);

		obj_from_room(ship->ship);
		obj_to_room(ship->ship, to_room);
	}
	else
	{
		ship_echo(ship, "{WThe vessel descends to the ground below.{x");
	}

	if( IS_NULLSTR(ship->ship_name) )
	{
		sprintf(buf, "{W%s %s descends from above to land.{x", get_article(ship->index->name, true), ship->index->name);
	}
	else
	{
		sprintf(buf, "{WThe %s '{x%s{W' descends from above to land.{x", ship->index->name, ship->ship_name);
	}
	room_echo(ship->ship->in_room, buf);

}

void do_ship_launch(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);

	if( !IS_VALID(ship) )
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->ship_type != SHIP_AIR_SHIP )
	{
		send_to_char("The vessel must remain in the water.\n\r", ch);
		return;
	}

	if( !ship_can_issue_command(ch, ship) )
	{
		act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		act("The wheel is magically locked. This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if ( !ship_has_enough_crew( ship ) ) {
		send_to_char( "There isn't enough crew to order that command!\n\r", ch );
		return;
	}

	if( ship->speed >= SHIP_SPEED_STOPPED )
	{
		send_to_char("The vessel is already airborne.\n\r", ch);
		return;
	}

	ship->speed = SHIP_SPEED_STOPPED;
	ship_echo(ship, "{WThe vessel groans a bit before taking flight.{x");

	char buf[MSL];
	if( !ship->ship->in_room->wilds )
	{
		// In a fixed area
		AREA_DATA *area = ship->ship->in_room->area;
		WILDS_DATA *wilds = get_wilds_from_uid(NULL, area->wilds_uid);

		if( !wilds || area->x < 0 || area->y < 0 )
		{
			send_to_char("There is nowhere for the vessel to go.\n\r", ch);
			return;
		}

		ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, area->x, area->y);
		if( !room )
			room = create_wilds_vroom(wilds, area->x, area->y);


		if( IS_NULLSTR(ship->ship_name) )
		{
			sprintf(buf, "{W%s %s groans a bit before taking flight.{x", get_article(ship->index->name, true), ship->index->name);
		}
		else
		{
			sprintf(buf, "{WThe %s '{x%s{W' groans a bit before taking flight.{x", ship->index->name, ship->ship_name);
		}
		room_echo(ship->ship->in_room, buf);

		obj_from_room(ship->ship);
		obj_to_room(ship->ship, room);

		if( IS_NULLSTR(ship->ship_name) )
		{
			sprintf(buf, "{W%s %s soars into the air from %s below.{x", get_article(ship->index->name, true), ship->index->name, area->name);
		}
		else
		{
			sprintf(buf, "{WThe %s '{x%s{W' soars into the air from %s below.{x", ship->index->name, ship->ship_name, area->name);
		}
		room_echo(ship->ship->in_room, buf);
	}
	else
	{
		if( IS_NULLSTR(ship->ship_name) )
		{
			sprintf(buf, "{W%s %s groans a bit before taking flight.{x", get_article(ship->index->name, true), ship->index->name);
		}
		else
		{
			sprintf(buf, "{WThe %s '{x%s{W' groans a bit before taking flight.{x", ship->index->name, ship->ship_name);
		}
		room_echo(ship->ship->in_room, buf);
	}

}

void do_ship_chase(CHAR_DATA *ch, char *argument)
{
	send_to_char("Not yet implemented.\n\r", ch);
}

void do_ship_flag(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship;

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	if( argument[0] == '\0' )
	{
		send_to_char("Please specify a flag.\n\r", ch);
		return;
	}

	char buf[MIL];
	if( IS_NULLSTR(ship->flag) )
	{
		sprintf(buf, "{WThe flag adorned with '{x%s{W' is hoisted up the mast.{x", argument);
	}
	else
	{
		sprintf(buf, "{WThe flag is lowered before a new flag adorned with '{x%s{W' is hoisted back up the mast.{x", argument);
	}

	ship_echo(ship, buf);
	free_string(ship->flag);
	ship->flag = str_dup(argument);
}

void do_ship_list(CHAR_DATA *ch, char *argument)
{
	send_to_char("Not yet implemented.\n\r", ch);
}

void do_ship(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];

	if( IS_NPC(ch) ) return;

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' )
	{
		send_to_char("Syntax:  ship aim <ship>\n\r"
					 "         ship chase <ship>\n\r"
					 "         ship christen <name>\n\r"
					 "         ship flag[ <flag>]\n\r"
					 "         ship land         (airship only)\n\r"
					 "         ship launch       (airship only)\n\r"
					 "         ship list\n\r"
					 "         ship navigate[ <action>]\n\r"
					 "         ship scuttle\n\r"
					 "         ship speed[ <speed>]\n\r"
					 "         ship steer[ <heading>[ <turn direction>]]\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "aim") )
	{
		do_ship_aim(ch, argument);
		return;
	}

	if( !str_prefix(arg, "chase") )
	{
		do_ship_chase(ch, argument);
		return;
	}

	if( !str_prefix(arg, "christen") )
	{
		do_ship_christen(ch, argument);
		return;
	}

	if( !str_prefix(arg, "flag") )
	{
		do_ship_flag(ch, argument);
		return;
	}

	if( !str_prefix(arg, "land") )
	{
		do_ship_land(ch, argument);
		return;
	}

	if( !str_prefix(arg, "launch") )
	{
		do_ship_launch(ch, argument);
		return;
	}

	if( !str_prefix(arg, "list") )
	{
		do_ship_list(ch, argument);
		return;
	}

	if( !str_prefix(arg, "navigate") )
	{
		do_ship_navigate(ch, argument);
		return;
	}

	if( !str_prefix(arg, "scuttle") )
	{
		do_ship_scuttle(ch, argument);
		return;
	}

	if( !str_prefix(arg, "speed") )
	{
		do_ship_speed(ch, argument);
		return;
	}

	if( !str_prefix(arg, "steer") )
	{
		do_ship_steer(ch, argument);
		return;
	}

	do_ship(ch, "");
}


bool _is_terrain_land(WILDS_DATA *wilds, int x, int y)
{
	if(!wilds) return false;

	if( x < 0 || x >= wilds->map_size_x ) return false;
	if( y < 0 || y >= wilds->map_size_y ) return false;

	WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, x, y);

	return ( terrain && !terrain->nonroom &&
		terrain->template->sector_type != SECT_WATER_NOSWIM &&
		terrain->template->sector_type != SECT_WATER_SWIM);
}

// Not very smart, just checks whether there is a SAFE_HARBOR water room next to land
//  Does not check whether that room has access to the edge
bool is_shipyard_valid(long wuid, int x1, int y1, int x2, int y2)
{
	WILDS_DATA *wilds = get_wilds_from_uid(NULL, wuid);

	if(!wilds) return false;

	if( x1 < 0 || x1 >= wilds->map_size_x ) return false;
	if( x2 < 0 || x2 >= wilds->map_size_x ) return false;
	if( y1 < 0 || y1 >= wilds->map_size_y ) return false;
	if( y2 < 0 || y2 >= wilds->map_size_y ) return false;

	for( int y = y1; y <= y2; y++ )
	{
		for( int x = x1; x <= x2; x++ )
		{
			WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, x, y);

			if( terrain && !terrain->nonroom &&
				(terrain->template->sector_type == SECT_WATER_NOSWIM ||
				 terrain->template->sector_type == SECT_WATER_SWIM) &&
				IS_SET(terrain->template->room2_flags, ROOM_SAFE_HARBOR) )
			{

				if( _is_terrain_land(wilds, x-1,y) ||
					_is_terrain_land(wilds, x+1,y) ||
					_is_terrain_land(wilds, x,y-1) ||
					_is_terrain_land(wilds, x,y+1) )
					return true;
			}
		}
	}

	return false;
}


bool get_shipyard_location(long wuid, int x1, int y1, int x2, int y2, int *x, int *y)
{
	// Verify the shipyard is valid still
	if( !is_shipyard_valid(wuid, x1, y1, x2, y2) ) return false;

	WILDS_DATA *wilds = get_wilds_from_uid(NULL, wuid);

	if(!wilds) return false;

	while(true) {
		int _x = number_range(x1, x2);
		int _y = number_range(y1, y2);

		WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, _x, _y);

		if( terrain && !terrain->nonroom &&
			(terrain->template->sector_type == SECT_WATER_NOSWIM ||
			 terrain->template->sector_type == SECT_WATER_SWIM) &&
			IS_SET(terrain->template->room2_flags, ROOM_SAFE_HARBOR) )
		{
			if( _is_terrain_land(wilds, _x-1,_y) ||
				_is_terrain_land(wilds, _x+1,_y) ||
				_is_terrain_land(wilds, _x,_y-1) ||
				_is_terrain_land(wilds, _x,_y+1) )
			{
				*x = _x;
				*y = _y;

				return true;
			}
		}
	}
}

SHIP_DATA *purchase_ship(CHAR_DATA *ch, long vnum, SHOP_DATA *shop)
{
	char buf[MSL];
	WILDS_DATA *wilds = get_wilds_from_uid(NULL, shop->shipyard);

	if(!wilds) return NULL;

	int x, y;
	if( !get_shipyard_location(shop->shipyard,
			shop->shipyard_region[0][0],
			shop->shipyard_region[0][1],
			shop->shipyard_region[1][0],
			shop->shipyard_region[1][1], &x, &y) )
	{
		return NULL;
	}

	SHIP_DATA *ship = create_ship(vnum);

	if( !IS_VALID(ship) )
	{
		return NULL;
	}

	ship->owner = ch;
	ship->owner_uid[0] = ch->id[0];
	ship->owner_uid[1] = ch->id[1];

	if( ship->ship_type == SHIP_AIR_SHIP )
		ship->speed = SHIP_SPEED_LANDED;

	ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, x, y);
	if( !room )
		room = create_wilds_vroom(wilds, x, y);

	free_string(ship->ship->name);
	sprintf(buf, ship->ship->pIndexData->name, ch->name);
	ship->ship->name = str_dup(buf);

	free_string(ship->ship->short_descr);
	sprintf(buf, "%s %s", get_article(ship->index->name, false), ship->index->name);
	ship->ship->short_descr = str_dup(buf);

	// Long description is never seen

	obj_to_room(ship->ship, room);
	return ship;
}

int ships_player_owned(CHAR_DATA *ch, SHIP_INDEX_DATA *index)
{
	ITERATOR it;
	SHIP_DATA *ship;

	int count = 0;
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( (!index || ship->index == index) &&
			ship_isowner_player(ship, ch) )
		{
			++count;
		}
	}
	iterator_stop(&it);

	return count;
}

/////////////////////////////////////////////////////////////////
//
// NPC Ships
//

/////////////////////////////////////////////////////////////////
//
// SHip Edit
//

const struct olc_cmd_type shedit_table[] =
{
	{ "?",					show_help			},
	{ "armor",				shedit_armor		},
	{ "blueprint",			shedit_blueprint	},
	{ "capacity",			shedit_capacity		},
	{ "class",				shedit_class		},
	{ "commands",			show_commands		},
	{ "create",				shedit_create		},
	{ "crew",				shedit_crew			},
	{ "desc",				shedit_desc			},
	{ "flags",				shedit_flags		},
	{ "guns",				shedit_guns			},
	{ "hit",				shedit_hit			},
	{ "keys",				shedit_keys			},
	{ "list",				shedit_list			},
	{ "move",				shedit_move			},
	{ "name",				shedit_name			},
	{ "object",				shedit_object		},
	{ "show",				shedit_show			},
	{ "turning",			shedit_turning		},
	{ "weight",				shedit_weight		},
	{ NULL,					0,					}
};

bool can_edit_ships(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}

void list_ship_indexes(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_ships(ch) )
	{
		send_to_char("You do not have access to ships.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_ship_index_vnum; vnum++)
	{
		SHIP_INDEX_DATA *ship = get_ship_index(vnum);

		if( ship )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s  {G%-16.16s{x \n\r",
				vnum,
				ship->name,
				flag_string(ship_class_types, ship->ship_class));

			++lines;
			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = TRUE;
				break;
			}
		}
	}

	if( error )
	{
		send_to_char("Too many ships to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No ships to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ] [   Ship Class   ]{x\n\r", ch);
			send_to_char("{Y=========================================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
	}
	free_buf(buffer);
}

void do_shlist(CHAR_DATA *ch, char *argument)
{
	list_ship_indexes(ch, argument);
}

void shedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_ships(ch))
	{
		send_to_char("SHEdit:  Insufficient security to edit ships - action logged.\n\r", ch);
		edit_done(ch);
		return;
	}

	if (!str_cmp(command, "done"))
	{
		edit_done(ch);
		return;
	}

    ch->pcdata->immortal->last_olc_command = current_time;

	if (command[0] == '\0')
	{
		shedit_show(ch, argument);
		return;
	}

	for (cmd = 0; shedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, shedit_table[cmd].name))
		{
			if ((*shedit_table[cmd].olc_fun) (ch, argument))
			{
				ships_changed = true;
			}

			return;
		}
	}

	interpret(ch, arg);
}

void do_shedit(CHAR_DATA *ch, char *argument)
{
	SHIP_INDEX_DATA *ship = NULL;
	int value;
	char arg[MAX_STRING_LENGTH];

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument,arg);

	if (!can_edit_ships(ch))
	{
		send_to_char("SHEdit : Insufficient security to edit ships - action logged.\n\r", ch);
		return;
	}

	if (is_number(arg))
	{
		value = atoi(arg);
		if ( !(ship = get_ship_index(value)) )
		{
			send_to_char("That ship vnum does not exist.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)ship;
		ch->desc->editor = ED_SHIP;
		return;
	}
	else if (!str_cmp(arg, "create"))
	{
		if( shedit_create(ch, argument) )
		{
			ch->pcdata->immortal->last_olc_command = current_time;
			ships_changed = true;
			ch->desc->editor = ED_SHIP;
		}
		return;
	}

	send_to_char("Syntax: shedit <vnum>\n\r"
				 "        shedit create <vnum>\n\r", ch);
}

void do_shshow(CHAR_DATA *ch, char *argument)
{
	SHIP_INDEX_DATA *ship;
	void *old_edit;
	long value;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  shshow <vnum>\n\r", ch);
		return;
	}

	if (!is_number(argument))
	{
		send_to_char("Vnum must be a number.\n\r", ch);
		return;
	}

	value = atol(argument);
	if (!(ship = get_ship_index(value)))
	{
		send_to_char("That ship does not exist.\n\r", ch);
		return;
	}

	old_edit = ch->desc->pEdit;
	ch->desc->pEdit = (void *) ship;

	shedit_show(ch, argument);
	ch->desc->pEdit = old_edit;
	return;
}

SHEDIT( shedit_list )
{
	list_ship_indexes(ch, argument);
	return FALSE;
}

SHEDIT( shedit_show )
{
	SHIP_INDEX_DATA *ship;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_SHIP(ch, ship);

	buffer = new_buf();

	add_buf(buffer, "{x");

	sprintf(buf, "Name:        [%5ld] %s{x\n\r", ship->vnum, ship->name);
	add_buf(buffer, buf);

	sprintf(buf, "Ship Class:  %s{x\n\r", flag_string(ship_class_types, ship->ship_class));
	add_buf(buffer, buf);

	sprintf(buf, "Flags:       [%s]\n\r", flag_string(ship_flags, ship->flags));
	add_buf(buffer, buf);

	if( IS_VALID(ship->blueprint) )
		sprintf(buf, "Blueprint:   [%5ld] %s{x\n\r", ship->blueprint->vnum, ship->blueprint->name);
	else
		sprintf(buf, "Blueprint:   {Dunassigned{x\n\r");
	add_buf(buffer, buf);

	OBJ_INDEX_DATA *obj = get_obj_index(ship->ship_object);
	if( obj )
		sprintf(buf, "Ship Object: [%5ld] %s{x\n\r", obj->vnum, obj->short_descr);
	else
		sprintf(buf, "Ship Object: {Dunassigned{x\n\r");
	add_buf(buffer, buf);

	sprintf(buf, "Hit Points:  [%5d]{x\n\r", ship->hit);
	add_buf(buffer, buf);

	sprintf(buf, "Max Guns:    [%5d]{x\n\r", ship->guns);
	add_buf(buffer, buf);

	sprintf(buf, "Min Crew:    [%5d]{x\n\r", ship->min_crew);
	add_buf(buffer, buf);

	sprintf(buf, "Max Crew:    [%5d]{x\n\r", ship->max_crew);
	add_buf(buffer, buf);

	sprintf(buf, "Move Delay:  [%5d]{x\n\r", ship->move_delay);
	add_buf(buffer, buf);

	sprintf(buf, "Move Steps:  [%5d]{x\n\r", ship->move_steps);
	add_buf(buffer, buf);

	sprintf(buf, "Max Turning: %d degrees{x\n\r", ship->turning);
	add_buf(buffer, buf);

	sprintf(buf, "Max Weight:  [%5d]{x\n\r", ship->weight);
	add_buf(buffer, buf);

	sprintf(buf, "Capacity:    [%5d]{x\n\r", ship->capacity);
	add_buf(buffer, buf);

	sprintf(buf, "Base Armor:  [%5d]{x\n\r", ship->armor);
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, fix_string(ship->description));
	add_buf(buffer, "\n\r\n\r");

	add_buf(buffer, "Special Keys:\n\r");
	if( list_size(ship->special_keys) > 0 )
	{
		ITERATOR it;
		OBJ_INDEX_DATA *key;
		int count = 0;

		add_buf(buffer, "    [  Vnum  ]  Name\n\r");
		add_buf(buffer, "==============================================\n\r");

		iterator_start(&it, ship->special_keys);
		while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
		{
			char key_color = 'Y';

			if( key->item_type != ITEM_KEY )
			{
				key_color = 'R';
			}


			sprintf(buf, "{W%3d  {G%8ld  {%c%s{x\n\r", ++count, key->vnum, key_color, key->short_descr);
			add_buf(buffer, buf);
		}
		iterator_stop(&it);

		add_buf(buffer, "==============================================\n\r");
		add_buf(buffer, "{RRED{x = not a key.\n\r");
	}
	else
	{
		add_buf(buffer, "  None\n\r");
	}

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return FALSE;
}

SHEDIT( shedit_create )
{
	SHIP_INDEX_DATA *ship;
	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_ship_index_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_ship_index_vnum; last_vnum++)
		{
			if( !get_ship_index(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}
	else if( get_ship_index(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return FALSE;
	}

	ship = new_ship_index();
	ship->vnum = value;

	iHash							= ship->vnum % MAX_KEY_HASH;
	ship->next						= ship_index_hash[iHash];
	ship_index_hash[iHash]			= ship;
	ch->desc->pEdit					= (void *)ship;

	if( ship->vnum > top_ship_index_vnum)
		top_ship_index_vnum = ship->vnum;

    return TRUE;
}

SHEDIT( shedit_name )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	smash_tilde(argument);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  name [name]\n\r", ch);
		return FALSE;
	}

	free_string(ship->name);
	ship->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_desc )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] != '\0' )
	{
		send_to_char("Syntax:  desc\n\r", ch);
		return FALSE;
	}

	string_append(ch, &ship->description);
	return TRUE;
}

SHEDIT( shedit_class )
{
	SHIP_INDEX_DATA *ship;
	int value;

	EDIT_SHIP(ch, ship);

	value = flag_value(ship_class_types, argument);
	if( value == NO_FLAG )
	{
		send_to_char("Syntax:  class [ship class]\n\r", ch);
		send_to_char("See '? shipclass' for list of classes.\n\r\n\r", ch);
		show_help(ch, "shipclass");
		return FALSE;
	}

	ship->ship_class = value;
	send_to_char("Ship Class changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_flags)
{
	SHIP_INDEX_DATA *ship;
	int value;

	EDIT_SHIP(ch, ship);

	value = flag_value(ship_flags, argument);
	if( value == NO_FLAG )
	{
		send_to_char("Syntax:  flags [flags]\n\r", ch);
		send_to_char("See '? ship' for list of flags.\n\r\n\r", ch);
		show_help(ch, "ship");
		return FALSE;
	}

	ship->flags ^= value;
	send_to_char("Ship flags changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_blueprint )
{
	SHIP_INDEX_DATA *ship;
	BLUEPRINT *bp;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  blueprint [vnum]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	if( !(bp = get_blueprint(atol(argument))) )
	{
		send_to_char("Blueprint does not exist.\n\r", ch);
		return FALSE;
	}

	if( bp->mode == BLUEPRINT_MODE_STATIC )
	{

		// Verify the blueprint has certain features
		// * Has an entry room
		if( bp->static_entry_section < 1 || bp->static_entry_link < 1)
		{
			send_to_char("Blueprint requires an entry point for boarding purposes.\n\r", ch);
			return FALSE;
		}

		// * Room with HELM
		// * Room with VIEWWILDS (optional)
		bool helm = false, viewwilds = false;
		ITERATOR sit;

		// Check special rooms
		BLUEPRINT_SPECIAL_ROOM *special_room;
		iterator_start(&sit, bp->special_rooms);
		while( (special_room = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			ROOM_INDEX_DATA *room = get_room_index(special_room->vnum);

			if( room )
			{
				if( IS_SET(room->room_flags, ROOM_SHIP_HELM) )
				{
					helm = true;
				}

				if( IS_SET(room->room_flags, ROOM_VIEWWILDS) )
				{
					viewwilds = true;
				}
			}
		}
		iterator_stop(&sit);

		if( !helm || !viewwilds )
		{
			BLUEPRINT_SECTION *section;
			iterator_start(&sit, bp->sections);
			while( (section = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
			{
				for( long vnum = section->lower_vnum; vnum <= section->upper_vnum; vnum++)
				{
					ROOM_INDEX_DATA *room = get_room_index(vnum);

					if( room )
					{
						if( IS_SET(room->room_flags, ROOM_SHIP_HELM) )
						{
							helm = true;
						}

						if( IS_SET(room->room_flags, ROOM_VIEWWILDS) )
						{
							viewwilds = true;
						}
					}
				}
			}
			iterator_stop(&sit);
		}


		if( !helm )
		{
			send_to_char("Blueprint requires at least one room with the 'helm' flag set, for controlling the ship.\n\r", ch);
			return FALSE;
		}

		if( !viewwilds )
		{
			// Not a deal breaker, just warn about it being missing
			send_to_char("{YWARNING: {xBlueprint missing a room with 'viewwilds' to serve as a crow's nest. Might want to add one.\n\r", ch);
		}

	}
	else
	{
		send_to_char("Only static blueprints supported.\n\r", ch);
		return FALSE;
	}



	ship->blueprint = bp;
	send_to_char("Ship blueprint changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_object )
{
	SHIP_INDEX_DATA *ship;
	OBJ_INDEX_DATA *obj;
	long vnum;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  object [vnum]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	vnum = atol(argument);
	obj = get_obj_index(vnum);
	if( !obj )
	{
		send_to_char("That object does not exist.\n\r", ch);
		return FALSE;
	}

	if( obj->item_type != ITEM_SHIP )
	{
		send_to_char("Object is not a ship.\n\r", ch);
		return FALSE;
	}

	ship->ship_object = vnum;
	send_to_char("Ship object set.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_hit )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  hit [points]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 1 || value > SHIP_MAX_HIT )
	{
		send_to_char("Hit points must be in the range of 1 to " __STR(SHIP_MAX_HIT) ".\n\r", ch);
		return FALSE;
	}

	ship->hit = value;
	send_to_char("Ship hit points changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_turning )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  turning [degrees]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 1 || value > 60 )
	{
		send_to_char("Turning power must be in the range of 1 to 60 degrees.\n\r", ch);
		return FALSE;
	}

	ship->turning = value;
	send_to_char("Turning power changed.\n\r", ch);
	return TRUE;
}


SHEDIT( shedit_guns )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  guns [count]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_GUNS )
	{
		send_to_char("Gun allowance must be in the range of 0 to " __STR(SHIP_MAX_GUNS) ".\n\r", ch);
		return FALSE;
	}

	ship->guns = value;
	send_to_char("Ship gun allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_crew )
{
	SHIP_INDEX_DATA *ship;
	char arg[MIL];

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  crew [min] [max]\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !is_number(arg) ||  !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int min_crew = atoi(arg);
	int max_crew = atoi(argument);

	if( max_crew < min_crew )
	{
		int value = min_crew;
		min_crew = max_crew;
		max_crew = value;
	}

	if( min_crew < 0 || min_crew > SHIP_MAX_CREW )
	{
		send_to_char("Minimum crew allowance must be in the range of 0 to " __STR(SHIP_MAX_CREW) ".\n\r", ch);
		return FALSE;
	}

	if( max_crew < 0 || max_crew > SHIP_MAX_CREW )
	{
		send_to_char("Maximum crew allowance must be in the range of 0 to " __STR(SHIP_MAX_CREW) ".\n\r", ch);
		return FALSE;
	}

	ship->min_crew = min_crew;
	ship->max_crew = max_crew;
	send_to_char("Ship crew allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_move )
{
	SHIP_INDEX_DATA *ship;
	char arg[MIL];

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  move [delay] [steps]\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !is_number(arg) || !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int delay = atoi(arg);
	int steps = atoi(argument);

	if( delay < SHIP_MIN_DELAY )
	{
		send_to_char("Move delay must be at least " __STR(SHIP_MIN_DELAY) ".\n\r", ch);
		return FALSE;
	}

	if( steps < SHIP_MIN_STEPS )
	{
		send_to_char("Move steps must be at least " __STR(SHIP_MIN_STEPS) ".\n\r", ch);
		return FALSE;
	}

	ship->move_steps = steps;
	ship->move_delay = delay;
	send_to_char("Ship movement changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_weight )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  weight [weight]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_WEIGHT )
	{
		send_to_char("Weight allowance must be in the range of 0 to " __STR(SHIP_MAX_WEIGHT) ".\n\r", ch);
		return FALSE;
	}

	ship->weight = value;
	send_to_char("Ship weight allowance changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_capacity )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  capacity [count]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_CAPACITY )
	{
		send_to_char("Ship capacity must be in the range of 0 to " __STR(SHIP_MAX_CAPACITY) ".\n\r", ch);
		return FALSE;
	}

	ship->capacity = value;
	send_to_char("Ship capacity changed.\n\r", ch);
	return TRUE;
}

SHEDIT( shedit_armor)
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  armor [rating]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 || value > SHIP_MAX_ARMOR )
	{
		send_to_char("Ship base armor must be in the range of 0 to " __STR(SHIP_MAX_ARMOR) ".\n\r", ch);
		return FALSE;
	}

	ship->armor = value;
	send_to_char("Ship base armor changed.\n\r", ch);
	return TRUE;
}


SHEDIT( shedit_keys )
{
	SHIP_INDEX_DATA *ship;
	char arg[MIL];

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  keys list\n\r", ch);
		send_to_char("Syntax:  keys add <vnum>\n\r", ch);
		send_to_char("Syntax:  keys remove <#>\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !str_cmp(arg, "list") )
	{
		if( list_size(ship->special_keys) > 0 )
		{
			ITERATOR it;
			OBJ_INDEX_DATA *key;
			BUFFER *buffer = new_buf();
			char buf[MSL];
			int count = 0;

			add_buf(buffer, "    [  Vnum  ]  Name\n\r");
			add_buf(buffer, "==============================================\n\r");

			iterator_start(&it, ship->special_keys);
			while( (key = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
			{
				char key_color = 'Y';

				if( key->item_type != ITEM_KEY )
				{
					key_color = 'R';
				}

				sprintf(buf, "{W%3d  {G%8ld  {%c%s{x\n\r", ++count, key->vnum, key_color, key->short_descr);
				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			add_buf(buffer, "==============================================\n\r");
			add_buf(buffer, "{RRED{x = not a key.\n\r");

			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
		}
		else
		{
			send_to_char("No special keys to display.\n\r", ch);
		}

		return FALSE;
	}

	if( !str_cmp(arg, "add") )
	{
		OBJ_INDEX_DATA *key;
		long vnum;

		if( !is_number(argument) )
		{
			send_to_char("That is not a number,\n\r", ch);
			return FALSE;
		}

		vnum = atol(argument);
		if( !(key = get_obj_index(vnum)) )
		{
			send_to_char("That object does not exist.\n\r", ch);
			return FALSE;
		}

		if( key->item_type != ITEM_KEY )
		{
			send_to_char("That is not a key.\n\r", ch);
			return FALSE;
		}

		if( list_hasdata(ship->special_keys, key) )
		{
			send_to_char("That key is already in the list.\n\r", ch);
			return FALSE;
		}

		list_appendlink(ship->special_keys, key);
		send_to_char("Key added.\n\r", ch);
		return TRUE;
	}

	if( !str_cmp(arg, "remove") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number,\n\r", ch);
			return FALSE;
		}

		int value = atoi(argument);
		if( value < 0 || value > list_size(ship->special_keys) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return FALSE;
		}

		list_remnthlink(ship->special_keys, value);
		send_to_char("Key removed.\n\r", ch);
		return TRUE;
	}

	shedit_keys(ch, "");
	return FALSE;

}


/////////////////////////////////////////////////////////////////
//
// NPC SHip Edit
//
