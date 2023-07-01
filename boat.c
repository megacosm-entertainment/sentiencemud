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
void do_ship_speed( CHAR_DATA *ch, char *argument );

extern LLIST *loaded_instances;

LLIST *loaded_ships;
LLIST *loaded_waypoints;
LLIST *loaded_waypoint_paths;

#define CREW_SKILL_SCOUTING		0
#define CREW_SKILL_GUNNING		1
#define CREW_SKILL_OARRING		2
#define CREW_SKILL_MECHANICS	3
#define CREW_SKILL_NAVIGATION	4
#define CREW_SKILL_LEADERSHIP	5


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
// Crew
//

void crew_skill_improve(CHAR_DATA *ch, int skill)
{
	SHIP_CREW_DATA *crew = ch->crew;
	sh_int *ptr;

	switch(skill)
	{
	case CREW_SKILL_SCOUTING:	ptr = &crew->scouting; break;
	case CREW_SKILL_GUNNING:	ptr = &crew->gunning; break;
	case CREW_SKILL_OARRING:	ptr = &crew->oarring; break;
	case CREW_SKILL_MECHANICS:	ptr = &crew->mechanics; break;
	case CREW_SKILL_NAVIGATION:	ptr = &crew->navigation; break;
	case CREW_SKILL_LEADERSHIP:	ptr = &crew->leadership; break;
	default:
		return;
	}

	int rating = *ptr;

	if( rating < 1 || rating > 99 ) return;

	// Massage rating for improvement

	rating = URANGE(2, 100 - rating, 25);

	if( number_percent() < rating )
	{
		*ptr += 1;

		// add a trigger to the crew member?
	}
}

/////////////////////////////////////////////////////////////////
//
// Navigation
//

void set_seek_point(WILDS_COORD *coord, WILDS_DATA *wilds, long w, int x, int y, int skill)
{
	if( !wilds ) wilds = get_wilds_from_uid(NULL, w);

	coord->wilds = wilds;
	coord->w = w;
	coord->x = x;
	coord->y = y;

	int delta = number_percent() - skill;
	if( delta > 0 )
	{
		delta = (delta + 9) / 10;		// every 10% off the skill this will add 1 to fudge range.

		coord->x += number_range(-delta,delta);
		coord->x = URANGE(0, coord->x, coord->wilds->map_size_x - 1);

		coord->y += number_range(-delta,delta);
		coord->y = URANGE(0, coord->y, coord->wilds->map_size_y - 1);
	}
}

bool ship_seek_point(SHIP_DATA *ship)
{
	ROOM_INDEX_DATA *room = obj_room(ship->ship);
	if( IS_WILDERNESS(room) && ship->seek_point.wilds == room->wilds )
	{
		int distSq = (room->x - ship->seek_point.x) * (room->x - ship->seek_point.x) + (room->y - ship->seek_point.y) * (room->y - ship->seek_point.y);

		// Within 2.44 block radius of location
		if( distSq <= 6 )
		{
			int skill = 0;
			if( ship->seek_navigator )
			{
				if( IS_VALID(ship->navigator) && ship->navigator->crew && ship->navigator->crew->navigation > 0 )
				{
					SHIP_DATA *nav_ship = get_room_ship(ship->navigator->in_room);

					if( ship == nav_ship )
					{
						crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
					}

					skill = ship->navigator->crew->navigation;
				}
			}
			else if( IS_VALID(ship->owner) && !IS_NPC(ship->owner) )
			{
				check_improve(ship->owner, gsn_navigation, TRUE, 10);
				skill = get_skill(ship->owner, gsn_navigation);
			}

			WAYPOINT_DATA *wp = (WAYPOINT_DATA *)iterator_nextdata(&ship->route_it);
			if( wp )
			{
//				char buf[MSL];
//				sprintf(buf, "{WNext Waypoint:{x {YS{x%d {YE{x%d", wp->y, wp->x);
//				ship_echo(ship, buf);
				set_seek_point(&ship->seek_point, NULL, wp->w, wp->x, wp->y, skill);

				ship->current_waypoint = wp;
			}
			else
			{
				ship_echo(ship, "{WThe vessel has reached its destination.{x");
				ship_stop(ship);
				return false;	// Return false to indicate stop movement
			}
		}


		int dx = ship->seek_point.x - room->x;
		int dy = ship->seek_point.y - room->y;

		int heading = (int)(180 * atan2(dx, -dy) / 3.14159);
		if( heading < 0 ) heading += 360;

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

void steering_calc_forceheading(SHIP_DATA *ship, int dx, int dy)
{
	ship->steering.dx = dx;
	ship->steering.dy = dy;

	ship->steering.ax = abs(ship->steering.dx);
	ship->steering.ay = abs(ship->steering.dy);

	ship->steering.sx = (ship->steering.dx > 0) ? 1 : ((ship->steering.dx < 0) ? -1 : 0);
	ship->steering.sy = (ship->steering.dy > 0) ? 1 : ((ship->steering.dy < 0) ? -1 : 0);

	ship->steering.compass = bearing_door[2 * ship->steering.heading / 45];

	ship->steering.move = 0;

	int heading = (int)(180 * atan2(dx, -dy) / 3.14159 + 0.5);
	if( heading < 0 ) heading += 360;

	ship->steering.heading = heading;
	ship->steering.heading_target = heading;
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
	if( ship->ship_power < SHIP_SPEED_STOPPED )
		return;

	if( ship->steering.turning_dir )
	{
		int power = ship->index->turning;

		if( ship->ship_power == SHIP_SPEED_STOPPED )
		{
			if( ship->oar_power == SHIP_SPEED_STOPPED )
			{
				// Cut the turning power while motionless
				power /= 2;
			}
			else
			{
				power = 3 * power / 4;
			}

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
				if( ship->ship_power == SHIP_SPEED_STOPPED )
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

				if( ship->seek_point.wilds != NULL )
				{
					ROOM_INDEX_DATA *room = obj_room(ship->ship);

					steering_calc_forceheading(ship,
						ship->seek_point.x - room->x,
						ship->seek_point.y - room->y);

					return;
				}
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
	if( ship->ship_power <= SHIP_SPEED_STOPPED ) return false;

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

SHIP_INDEX_DATA *read_ship_index(FILE *fp, AREA_DATA *area)
{
	SHIP_INDEX_DATA *ship;
	char *word;
	bool fMatch;

	ship = new_ship_index();
	ship->vnum = fread_number(fp);
	ship->area = area;

	if (ship->vnum > area->top_ship_vnum)
		area->top_ship_vnum = ship->vnum;

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
				ship->blueprint_wnum = fread_widevnum(fp, area->uid);
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

				// Objects in the area should already be loaded
				OBJ_INDEX_DATA *key = get_obj_index(area, key_vnum);
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
			KEY("Oars", ship->oars, fread_number(fp));
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

void fix_ship_index(SHIP_INDEX_DATA *ship)
{
	ship->blueprint = get_blueprint_auid(ship->blueprint_wnum.auid, ship->blueprint_wnum.vnum);
}

void fix_ship_indexes()
{
	AREA_DATA *area;
	SHIP_INDEX_DATA *ship;
	int iHash;

	for(area = area_first; area; area = area->next)
	{
		for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
		{
			for(ship = area->ship_index_hash[iHash]; ship; ship = ship->next)
				fix_ship_index(ship);
		}
	}
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
	fprintf(fp, "Oars %d\n", ship->oars);
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

void save_ships(FILE *fp, AREA_DATA *area)
{
	int iHash;

	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = area->ship_index_hash[iHash]; ship; ship = ship->next)
		{
			save_ship_index(fp, ship);
		}
	}
}

SHIP_INDEX_DATA *get_ship_index_wnum(WNUM wnum)
{
	return get_ship_index(wnum.pArea, wnum.vnum);
}

SHIP_INDEX_DATA *get_ship_index_auid(long auid, long vnum)
{
	return get_ship_index(get_area_from_uid(auid), vnum);
}

SHIP_INDEX_DATA *get_ship_index(AREA_DATA *pArea, long vnum)
{
	if(!pArea) return NULL;

	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(SHIP_INDEX_DATA *ship = pArea->ship_index_hash[iHash]; ship; ship = ship->next)
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

SHIP_DATA *create_ship(WNUM wnum)
{
	OBJ_DATA *obj;						// Physical ship object
	OBJ_INDEX_DATA *obj_index;			// Ship object index to create
	SHIP_DATA *ship;					// Runtime ship data
	SHIP_INDEX_DATA *ship_index;		// Ship index to create
	INSTANCE *instance;
	ITERATOR it;

	// Verify the ship index exists
	if( !(ship_index = get_ship_index(wnum.pArea, wnum.vnum)) )
		return NULL;

	// Verify the object index exists and is a ship
	if( !(obj_index = get_obj_index(ship_index->area, ship_index->ship_object)) )
		return NULL;

	if( obj_index->item_type != ITEM_SHIP )
	{
		char buf[MSL];
		sprintf(buf, "create_ship: attempting to use object (%ld#%ld) that is not a ship object for ship (%ld#%ld)", obj_index->area->uid, obj_index->vnum, ship_index->area->uid, ship_index->vnum);
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

			sk->key_wnum.pArea = key->area;
			sk->key_wnum.vnum = key->vnum;
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
	ship->min_crew = ship_index->min_crew;
	ship->max_crew = ship_index->max_crew;

	// Build cannons

	list_appendlink(loaded_ships, ship);

	get_ship_id(ship);
	return ship;
}

void extract_ship(SHIP_DATA *ship)
{
	if( !IS_VALID(ship) ) return;

	if( IS_VALID(ship->owner) && !IS_NPC(ship->owner) )
	{
		list_remlink(ship->owner->pcdata->ships, ship);
	}

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

	if( ship->owner == ch ) return true;	// If it's already assigned, short circuit

	//return ( (ship->owner_uid[0] == ch->id[0]) && (ship->owner_uid[1] == ch->id[1]) );
	return false;
}

WAYPOINT_DATA *get_ship_waypoint(SHIP_DATA *ship, char *argument, WILDS_DATA *wilds)
{
	if( is_number(argument) )
	{
		int value = atoi(argument);

		if( value < 1 || value > list_size(ship->waypoints) )
			return NULL;

		return (WAYPOINT_DATA *)list_nthdata(ship->waypoints, value);
	}
	else
	{
		char arg[MIL];
		int number;

		number = number_argument(argument, arg);
		if( number < 1 ) return NULL;

		ITERATOR it;
		WAYPOINT_DATA *wp;

		iterator_start(&it, ship->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
		{
			if( (!wilds || (wp->w == wilds->uid)) && is_name(arg, wp->name) )
			{
				if( !--number )
					break;
			}
		}
		iterator_stop(&it);

		return wp;
	}
}


SHIP_ROUTE *get_ship_route(SHIP_DATA *ship, char *argument)
{
	if( is_number(argument) )
	{
		int value = atoi(argument);

		if( value < 1 || value > list_size(ship->routes) )
			return NULL;

		return (SHIP_ROUTE *)list_nthdata(ship->routes, value);
	}
	else
	{
		char arg[MIL];
		int number;

		number = number_argument(argument, arg);
		if( number < 1 ) return NULL;

		ITERATOR it;
		SHIP_ROUTE *route;

		iterator_start(&it, ship->routes);
		while( (route = (SHIP_ROUTE *)iterator_nextdata(&it)) )
		{
			if( is_name(arg, route->name) )
			{
				if( !--number )
					break;
			}
		}
		iterator_stop(&it);

		return route;
	}
}

void ship_cancel_route(SHIP_DATA *ship)
{
	if( list_size(ship->route_waypoints) > 0 )
	{
		iterator_stop(&ship->route_it);
		list_clear(ship->route_waypoints);
	}

	ship->current_waypoint = NULL;
	ship->current_route = NULL;

	memset(&ship->seek_point, 0, sizeof(ship->seek_point));
}

void ship_stop(SHIP_DATA *ship)
{
	ship->ship_power = SHIP_SPEED_STOPPED;
	ship->oar_power = SHIP_SPEED_STOPPED;
	ship->sextant_x = -1;
	ship->sextant_y = -1;
	ship->move_steps = 0;
	ship->steering.move = 0;
	ship->steering.turning_dir = 0;
	ship->ship_move = 0;
	ship_cancel_route(ship);
	ship->last_times[0] = current_time + 15;
	ship->last_times[1] = current_time + 10;
	ship->last_times[2] = current_time + 5;
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

	if( ship->oar_power > 0 && list_size(ship->oarsmen) > 0 )
	{
		ITERATOR oit;
		CHAR_DATA *oarsman;

		iterator_start(&oit, ship->oarsmen);
		while( (oarsman = (CHAR_DATA *)iterator_nextdata(&oit)) )
		{
			// Only allow oarsmen that are not exhausted
			if( oarsman->position == POS_STANDING )
			{
				crew_skill_improve(oarsman, CREW_SKILL_OARRING);

				// This will still allow Master oarsmen to get exhausted
				if( number_range(0, 124) >= oarsman->crew->oarring )
				{
					int nor = 125 - oarsman->crew->oarring;

					oarsman->move -= nor;

					oarsman->move = UMAX(0, oarsman->move);
					if (oarsman->move <= 0)
						oarsman->position = POS_RESTING;
				}
			}
		}
		iterator_stop(&oit);
	}

	return true;
}

void ship_set_move_steps(SHIP_DATA *ship)
{
	if( ship->ship_power > SHIP_SPEED_STOPPED || ship->oar_power > SHIP_SPEED_STOPPED )
	{
		int speed = ship->ship_power;

		if( ship->oar_power > 0 && list_size(ship->oarsmen) > 0 )
		{
			ITERATOR oit;
			CHAR_DATA *oarsman;

			iterator_start(&oit, ship->oarsmen);
			while( (oarsman = (CHAR_DATA *)iterator_nextdata(&oit)) )
			{
				if( oarsman->position == POS_STANDING )
				{
					// 1-10% for every oarsman not currently exhausted
					speed += (ship->oar_power + 9) / 10;
				}
			}
			iterator_stop(&oit);
		}

		// TODO: Add some kind of modifiers for speed
		// - encumberance (cargo weight)
		// - damaged propulsion
		// - wind?
		// - relic modifier

		ship->move_steps = speed * ship->index->move_steps / 100;
		ship->move_steps = UMAX(1, ship->move_steps);

		// Clear last sextant reading
		ship->sextant_x = -1;
		ship->sextant_y = -1;
	}
	else
	{
		ship->move_steps = 0;
	}
	ship->ship_move = ship->index->move_delay;
}

void ship_move_update(SHIP_DATA *ship)
{
	if( ship->ship_power > SHIP_SPEED_STOPPED )
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

	ITERATOR it;
	CHAR_DATA *oarsman;

	iterator_start(&it, ship->oarsmen);
	while( (oarsman = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		if( oarsman->max_move > 0 && oarsman->position == POS_RESTING )
		{
			int percent = 100 * oarsman->move / oarsman->max_move;

			if( percent > 25 )
			{
				// Return oarsman back to usable position
				oarsman->position = POS_STANDING;
			}
		}
	}
	iterator_stop(&it);
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
	WNUM_LOAD wnum = fread_widevnum(fp, 0);
	sk->key_wnum.pArea = get_area_from_uid(wnum.auid);
	sk->key_wnum.vnum = wnum.vnum;

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

SHIP_ROUTE *ship_route_load(FILE *fp, SHIP_DATA *ship)
{
	SHIP_ROUTE *route;
	char *word;
	bool fMatch;

	route = new_ship_route();
	route->name = fread_string(fp);

	while (str_cmp((word = fread_word(fp)), "#-ROUTE"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case 'W':
			if( !str_cmp(word, "Waypoint") )
			{
				int index = fread_number(fp);

				WAYPOINT_DATA *wp = list_nthdata(ship->waypoints, index);

				list_appendlink(route->waypoints, wp);

				fMatch = TRUE;
				break;
			}
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "ship_route_load: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return route;
}

CHAR_DATA *ship_load_find_crew(SHIP_DATA *ship, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	CHAR_DATA *crew;

	iterator_start(&it, ship->crew);
	while( (crew = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		if( crew->id[0] == id1 && crew->id[1] == id2 )
			break;
	}
	iterator_stop(&it);

	return crew;
}

INSTANCE *instance_load(FILE *fp);
OBJ_DATA *persist_load_object(FILE *fp);
CHAR_DATA *persist_load_mobile(FILE *fp);
CHAR_DATA *instance_find_mobile(INSTANCE *instance, unsigned long id1, unsigned long id2);

SHIP_DATA *ship_load(FILE *fp)
{
	SHIP_DATA *ship;
	SHIP_INDEX_DATA *index;
	char *word;
	bool fMatch;

	WNUM_LOAD wnum = fread_widevnum(fp, 0);
	AREA_DATA *area = get_area_from_uid(wnum.auid);

	index = get_ship_index(area, wnum.vnum);
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
			if( !str_cmp(word, "#MOBILE") )
			{
				CHAR_DATA *crew = persist_load_mobile(fp);

				if( IS_VALID(crew) )
				{
					list_appendlink(ship->crew, crew);
					char_to_room(crew, crew->in_room);
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
			if( !str_cmp(word, "#ROUTE") )
			{
				SHIP_ROUTE *route = ship_route_load(fp, ship);

				list_appendlink(ship->routes, route);
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
			if( !str_cmp(word, "FirstMate") )
			{
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				ship->first_mate = ship_load_find_crew(ship, id1, id2);

				fMatch = TRUE;
				break;
			}
			KEYS("Flag", ship->flag, fread_string(fp));
			break;

		case 'H':
			KEY("Hit", ship->hit, fread_number(fp));
			break;

		case 'M':
			if( !str_cmp(word, "MapWaypoint") )
			{
				WAYPOINT_DATA *wp = new_waypoint();

				wp->w = fread_number(fp);
				wp->x = fread_number(fp);
				wp->y = fread_number(fp);
				wp->name = fread_string(fp);

				list_appendlink(ship->waypoints, wp);

				fMatch = TRUE;
				break;
			}
			KEY("MoveSteps", ship->move_steps, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", ship->ship_name, fread_string(fp));
			if( !str_cmp(word, "Navigator") )
			{
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				ship->navigator = ship_load_find_crew(ship, id1, id2);

				fMatch = TRUE;
				break;
			}
			break;

		case 'O':
			KEY("Oars", ship->oars, fread_number(fp));
			if( !str_cmp(word, "Oarsman") )
			{
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				CHAR_DATA *oarsman = ship_load_find_crew(ship, id1, id2);

				if( IS_VALID(oarsman) )
				{
					list_appendlink(ship->oarsmen, oarsman);
				}

				fMatch = TRUE;
				break;
			}
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
			if( !str_cmp(word, "Scout") )
			{
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				ship->scout = ship_load_find_crew(ship, id1, id2);

				fMatch = TRUE;
				break;
			}
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
			KEY("ShipType", ship->ship_type, fread_number(fp));
			KEY("Speed", ship->ship_power, fread_number(fp));
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

	ship->ship_name_plain = nocolour(ship->ship_name);
	if( ship->min_crew <= 0 )
		ship->min_crew = ship->index->min_crew;

	if( ship->max_crew <= 0 )
		ship->max_crew = ship->index->max_crew;

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

	fprintf(fp, "#SPECIALKEY %ld#%ld\n", sk->key_wnum.pArea->uid, sk->key_wnum.vnum);

	iterator_start(&it, sk->list);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "Key %lu %lu\n", luid->id[0], luid->id[1]);

	}
	iterator_stop(&it);

	fprintf(fp, "#-SPECIALKEY\n");
}

void persist_save_object(FILE *fp, OBJ_DATA *obj, bool multiple);
void persist_save_mobile(FILE *fp, CHAR_DATA *ch);
bool ship_save(FILE *fp, SHIP_DATA *ship)
{
	ITERATOR it, rit;
	SPECIAL_KEY_DATA *sk;
	CHAR_DATA *crew, *oarsman;

	fprintf(fp, "#SHIP %ld#%ld\n", ship->index->area->uid, ship->index->vnum);

	save_ship_uid(fp, "Uid", ship->id);

	fprintf(fp, "Name %s~\n", fix_string(ship->ship_name));
	fprintf(fp, "ShipType %d\n", ship->ship_type);

	save_ship_uid(fp, "Owner", ship->owner_uid);

	fprintf(fp, "Flag %s~\n", fix_string(ship->flag));

	fprintf(fp, "Speed %d\n", ship->ship_power);
	fprintf(fp, "Hit %ld\n", ship->hit);
	fprintf(fp, "Armor %ld\n", ship->armor);

	fprintf(fp, "ShipFlags %d\n", ship->ship_flags);
	fprintf(fp, "Cannons %d\n", ship->cannons);
	fprintf(fp, "Crew %d %d\n", ship->min_crew, ship->max_crew);
	fprintf(fp, "Oars %d\n", ship->oars);

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

	WAYPOINT_DATA *wp;
	iterator_start(&it, ship->waypoints);
	while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "MapWaypoint %lu %d %d %s~\n", wp->w, wp->x, wp->y, fix_string(wp->name));
	}
	iterator_stop(&it);

	// Routes MUST be AFTER Waypoints
	SHIP_ROUTE *route;
	iterator_start(&rit, ship->routes);
	while( (route = (SHIP_ROUTE *)iterator_nextdata(&rit)) )
	{
		fprintf(fp, "#ROUTE %s~\n", fix_string(route->name));
		iterator_start(&it, route->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
		{
			int index = list_getindex(ship->waypoints, wp);

			if( index > 0 )
			{
				fprintf(fp, "Waypoint %d\n", index);
			}
		}
		iterator_stop(&it);
		fprintf(fp, "#-ROUTE\n");
	}
	iterator_stop(&rit);

	if( ship->pk )
	{
		fprintf(fp, "PK\n");
	}

	persist_save_object(fp, ship->ship, false);

	instance_save(fp, ship->instance);

	iterator_start(&it, ship->crew);
	while( (crew = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		persist_save_mobile(fp, crew);
	}
	iterator_stop(&it);

	if( IS_VALID(ship->first_mate) )
	{
		fprintf(fp, "FirstMate %lu %lu\n", ship->first_mate->id[0], ship->first_mate->id[1]);
	}

	if( IS_VALID(ship->navigator) )
	{
		fprintf(fp, "Navigator %lu %lu\n", ship->navigator->id[0], ship->navigator->id[1]);
	}

	if( IS_VALID(ship->scout) )
	{
		fprintf(fp, "Scout %lu %lu\n", ship->scout->id[0], ship->scout->id[1]);
	}

	iterator_start(&it, ship->oarsmen);
	while( (oarsman = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "Oarsman %lu %lu\n", oarsman->id[0], oarsman->id[1]);
	}
	iterator_stop(&it);

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
			strncpy(buf, "{rO", len);
		else
			strncpy(buf, "{DO", len);
	}
	else
	{
		if( ship->scuttle_time > 0 )
			strncpy(buf, "{RO", len);
		else
			strncpy(buf, "{WO", len);
	}

	buf[len] = '\0';
}

void get_ship_location(CHAR_DATA *ch, SHIP_DATA *ship, char *buf, size_t len)
{
	ROOM_INDEX_DATA *room = obj_room(ship->ship);
	if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
	{
		// Give exact location of ship
		if( IS_WILDERNESS(room) )
		{
			snprintf(buf, len, "{Y%s {x({W%ld{x) at ({C%ld{x,{C%ld{x)", room->wilds->name, room->wilds->uid, room->x, room->y);
		}
		else if( room->source )
		{
			// Only scripting should ever cause this situation
			snprintf(buf, len, "{Y%s {x({W%ld{x):{C%lu{x:{C%lu{x", room->name, room->source->vnum, room->id[0], room->id[1]);
		}
		else
		{
			snprintf(buf, len, "{Y%s {x({W%ld{x)", room->name, room->vnum);
		}
	}
	else
	{
		if( IS_WILDERNESS(room) )
		{
			WILDS_DATA *wilds = room->wilds;
			WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, room->x, room->y);

			int closestDistanceSq = 401;	// 20 distance radius
			AREA_REGION *closestRegion = NULL;
			for( AREA_DATA *area = area_first; area; area = area->next )
			{
				if( area->wilds_uid == wilds->uid )
				{
					int distanceSq;
					AREA_REGION *region = get_closest_area_region(area, room->x, room->y, &distanceSq, FALSE);

					if( distanceSq < closestDistanceSq )
					{
						closestDistanceSq = distanceSq;
						closestRegion = region;
					}
				}
			}

			if( closestRegion != NULL )
			{
				if( ship_isowner_player(ship, ch) && ship->sextant_x >= 0 && ship->sextant_y >= 0 )
				{
					snprintf(buf, len, "Anchored at {YSouth {W%d{x by {YEast {W%d{x near {Y%s{x in {Y%s{x", ship->sextant_y, ship->sextant_x, closestRegion->name, closestRegion->area->name);
				}
				else
				{
					snprintf(buf, len, "{Y%s{x near {Y%s{x in {Y%s{x", terrain->template->name, closestRegion->name, closestRegion->area->name);
				}
			}
			else
			{
				int region = get_region(room);

				if( ship_isowner_player(ship, ch) && ship->sextant_x >= 0 && ship->sextant_y >= 0 )
				{
					char loc[51];

					switch(region)
					{
					case REGION_FIRST_CONTINENT:	strncpy(loc, "near {YSeralia{x", 50); break;
					case REGION_SECOND_CONTINENT:	strncpy(loc, "near {YAthemia{x", 50); break;
					case REGION_THIRD_CONTINENT:	strncpy(loc, "near {YNaranda{x", 50); break;
					case REGION_FOURTH_CONTINENT:	strncpy(loc, "near {YHeletane{x", 50); break;
					case REGION_MORDRAKE_ISLAND:	strncpy(loc, "near {YMordrake's Island{x", 50); break;
					case REGION_TEMPLE_ISLAND:		strncpy(loc, "near {YVarkhan's Island{x", 50); break;
					case REGION_ARENA_ISLAND:		strncpy(loc, "near the {YArena Island{x", 50); break;
					case REGION_DRAGON_ISLAND:		strncpy(loc, "near the {YDragon Isle{x", 50); break;
					case REGION_UNDERSEA:			strncpy(loc, "near the {YUndersea{x", 50); break;
					case REGION_NORTH_POLE:			strncpy(loc, "near the {YNorthern Pole{x", 50); break;
					case REGION_SOUTH_POLE:			strncpy(loc, "near the {YSouthern Pole{x", 50); break;
					case REGION_NORTHERN_OCEAN:		strncpy(loc, "in the {YNorthern Equidorian Ocean{x", 50); break;
					case REGION_WESTERN_OCEAN:		strncpy(loc, "in the {YWestern Equidorian Ocean{x", 50); break;
					case REGION_CENTRAL_OCEAN:		strncpy(loc, "in the {YCentral Equidorian Ocean{x", 50); break;
					case REGION_EASTERN_OCEAN:		strncpy(loc, "in the {YEastern Equidorian Ocean{x", 50); break;
					case REGION_SOUTHERN_OCEAN:		strncpy(loc, "in the {YSouthern Equidorian Ocean{x", 50); break;
					default:						snprintf(loc, 50, "in {Y%s{x", wilds->name); break;
					}

					snprintf(buf, len, "Anchored at {YSouth {W%d{x by {YEast {W%d{x %s", ship->sextant_y, ship->sextant_x, loc);
				}
				else
				{
					switch(region)
					{
					case REGION_FIRST_CONTINENT:	strncpy(buf, "Near {YSeralia{x", len); break;
					case REGION_SECOND_CONTINENT:	strncpy(buf, "Near {YAthemia{x", len); break;
					case REGION_THIRD_CONTINENT:	strncpy(buf, "Near {YNaranda{x", len); break;
					case REGION_FOURTH_CONTINENT:	strncpy(buf, "Near {YHeletane{x", len); break;
					case REGION_MORDRAKE_ISLAND:	strncpy(buf, "Near {YMordrake's Island{x", len); break;
					case REGION_TEMPLE_ISLAND:		strncpy(buf, "Near {YVarkhan's Island{x", len); break;
					case REGION_ARENA_ISLAND:		strncpy(buf, "Near {YArena Island{x", len); break;
					case REGION_DRAGON_ISLAND:		strncpy(buf, "Near {YDragon Isle{x", len); break;
					case REGION_UNDERSEA:			strncpy(buf, "Near {YUndersea{x", len); break;
					case REGION_NORTH_POLE:			strncpy(buf, "Near the {YNorthern Pole{x", len); break;
					case REGION_SOUTH_POLE:			strncpy(buf, "Near the {YSouthern Pole{x", len); break;
					case REGION_NORTHERN_OCEAN:		strncpy(buf, "In the {YNorthern Equidorian Ocean{x", len); break;
					case REGION_WESTERN_OCEAN:		strncpy(buf, "In the {YWestern Equidorian Ocean{x", len); break;
					case REGION_CENTRAL_OCEAN:		strncpy(buf, "In the {YCentral Equidorian Ocean{x", len); break;
					case REGION_EASTERN_OCEAN:		strncpy(buf, "In the {YEastern Equidorian Ocean{x", len); break;
					case REGION_SOUTHERN_OCEAN:		strncpy(buf, "In the {YSouthern Equidorian Ocean{x", len); break;
					default:						snprintf(buf, len, "In {Y%s{x", wilds->name); break;
					}
				}
			}

		}
		else if( room->source )
		{
			ROOM_INDEX_DATA *environ = get_environment(room);
			snprintf(buf, len, "{Y%s{x located within {Y%s{x in {Y%s{x", room->name, environ->name, environ->area->name);
		}
		else
		{
			snprintf(buf, len, "{Y%s{x in {Y%s{x", room->name, room->area->name);
		}
	}
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
					ship->ship_power, ship->move_steps, ship->ship_move,
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
			WNUM wnum;

			argument = one_argument(argument, arg2);
			argument = one_argument(argument, arg3);

			if( !parse_widevnum(arg2, ch->in_room->area, &wnum) )
			{
				send_to_char("Please specify a widevnum.\n\r", ch);
				return;
			}

			SHIP_INDEX_DATA *index = get_ship_index(wnum.pArea, wnum.vnum);

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

			ship = create_ship(wnum);

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
			ship->ship_name_plain = nocolour(ship->ship_name);

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
	if (!IS_SET(ch->in_room->room_flags, ROOM_SHIP_HELM))
	{
		return false;
	}

	return true;
}

void ship_dispatch_message(CHAR_DATA *ch, SHIP_DATA *ship, char *error, char *command)
{
	send_to_char(error, ch);
	send_to_char("\n\r", ch);

	// This command was executed by someone other than the owner, tell the owner of the ship if they are online
	if( IS_VALID(ship->owner) && ship->owner != ch )
	{
		act("{YDispatched '{W$T{Y':{x\n\r$t", ship->owner, NULL, NULL, NULL, NULL, error, command, TO_CHAR);
	}
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
	char cmd[MIL];

	sprintf(cmd, "ship steer %s", argument);

	char *command = argument;
	argument = one_argument( argument, arg);
	argument = one_argument( argument, arg2);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (arg[0] == '\0')
	{
		// For now.. tell the bearing
		//  - Maybe adding a compass item?

		if( ship->ship_power > SHIP_SPEED_STOPPED )
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
			case -1:	strcpy(arg, "stationary"); break;
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

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'steer $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_steer, command);
					}
					else
						do_ship_steer(ship->first_mate, command);

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

    if ( !ship_has_enough_crew( ch->in_room->ship ) )
    {
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", cmd);
		return;
    }

	if( ship->ship_type == SHIP_AIR_SHIP && ship->ship_power == SHIP_SPEED_LANDED )
	{
		ship_dispatch_message(ch, ship, "The vessel needs to be airborne first.\n\r  Try 'ship launch' to go airborne.", cmd);
		return;
	}

	if( is_number(arg) )
	{
		heading = atoi(arg);

		if( heading < 0 || heading >= 360 )
		{
			ship_dispatch_message(ch, ship, "That isn't a valid direction.", cmd);
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
			if( ship->ship_power == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
			{
				ship->ship_move = ship->index->move_delay;
			}

		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already turning to port.", cmd);
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
			if( ship->ship_power == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
			{
				ship->ship_move = ship->index->move_delay;
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already turning to starboard.", cmd);
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
			ship_dispatch_message(ch, ship, "The vessel is already heading straight ahead.", cmd);
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
			ship_dispatch_message(ch, ship, "That isn't a valid direction.", cmd);
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
			ship_dispatch_message(ch, ship, "Turning direction ambiguous.\n\rPlease specify whether to go 'port' or 'starboard'.", cmd);
			return;
		}

		turning_dir = (delta < 0) ? -1 : 1;
	}

	steering_set_heading(ship, heading);
	steering_set_turning(ship, turning_dir);
	ship_cancel_route(ship);

	// TODO: Cancel chasing

	sprintf(buf, "{WThe vessel is now turning %s.{x", arg);
	ship_echo(ship, buf);

	// Allow stationary turning
	if( ship->ship_power == SHIP_SPEED_STOPPED && ship->ship_move <= 0)
	{
		ship->ship_move = ship->index->move_delay;
	}

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
	}
}

void do_ship_engines( CHAR_DATA *ch, char *argument )
{
	char buf[MSL];
	char arg[MAX_INPUT_LENGTH];
	SHIP_DATA *ship;
	char cmd[MIL];

	sprintf(cmd, "ship engines %s", argument);

	char *command = argument;
	argument = one_argument( argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->ship_type != SHIP_AIR_SHIP )
	{
		act("The vessel has no engines.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( arg[0] == '\0' )
	{
		if( ship->ship_power > SHIP_SPEED_STOPPED )
			sprintf(buf, "The vessel is currently running the engines at {W%d%%{x.", ship->ship_power);
		else
			sprintf(buf, "The engines are currently idling.");

		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'engines $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_engines, command);
					}
					else
						do_ship_speed(ship->first_mate, command);

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if ( !ship_has_enough_crew( ship ) )
	{
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", cmd);
		return;
	}

	if ( ship->index->move_steps < 1 )
	{
		ship_dispatch_message(ch, ship, "The vessel doesn't seem to have enough power to move!", cmd);
		return;
	}

	if( ship->ship_power == SHIP_SPEED_LANDED )
	{
		ship_dispatch_message(ch, ship, "The vessel needs to be airborne first.\n\r  Try 'ship launch' to go airborne.", cmd);
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
			sprintf(buf, "Explicit distance values must be from 1 to %d", ship->index->move_steps);
			ship_dispatch_message(ch, ship, buf, cmd);
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
		ship_dispatch_message(ch, ship, "You may stop the vessel, or order half, full, some percentage speed or specific distance count.", cmd);
		return;
	}

	if( speed == SHIP_SPEED_STOPPED )
	{
		if( ship->ship_power > SHIP_SPEED_STOPPED )
		{
			act("You give the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->ship_power = speed;

			if( ship->oar_power > SHIP_SPEED_STOPPED )
			{
				ship_echo(ship, "You feel the vessel slowing down as the furnace output drops to minimal.");
				ship_set_move_steps(ship);
			}
			else
			{
				ship_echo(ship, "You feel the vessel stopping as the furnace output drops to minimal.");
				ship_stop(ship);
			}
			// TODO: Cancel chasing

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already stopped.", cmd);
		}
		return;
	}

	if( speed == SHIP_SPEED_FULL_SPEED )
	{
		if( ship->ship_power < SHIP_SPEED_FULL_SPEED )
		{
			act("You give the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->ship_power = speed;

			ship_echo(ship, "You feel the vessel gaining speed.");
			ship_set_move_steps(ship);

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already going at full speed.", cmd);
		}
		return;
	}

	if( ship->ship_power > speed )
	{
		act("You give the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel slowing down.");
	}
	else if( ship->ship_power < speed )
	{
		act("You give the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel gaining speed.");
	}
	else
	{
		ship_dispatch_message(ch, ship, "The vessel is already going at that speed.", cmd);
		return;
	}

	ship->ship_power = URANGE(1,speed,100);
	ship_set_move_steps(ship);

    ship_autosurvey(ship);

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
	}
}

void do_ship_sails( CHAR_DATA *ch, char *argument )
{
	char buf[MSL];
	char arg[MAX_INPUT_LENGTH];
	SHIP_DATA *ship;
	char cmd[MIL];

	sprintf(cmd, "ship speed %s", argument);

	char *command = argument;
	argument = one_argument( argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->ship_type != SHIP_SAILING_BOAT )
	{
		act("The vessel has no sails.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( arg[0] == '\0' )
	{
		if( ship->ship_power > SHIP_SPEED_STOPPED )
			sprintf(buf, "The vessel is currently running sails at {W%d%%{x.", ship->ship_power);
		else
			sprintf(buf, "The sails are currently furled.");

		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'sails $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_sails, command);
					}
					else
						do_ship_speed(ship->first_mate, command);

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if ( !ship_has_enough_crew( ship ) )
	{
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", cmd);
		return;
	}

	if ( ship->index->move_steps < 1 )
	{
		ship_dispatch_message(ch, ship, "The vessel doesn't seem to have enough power to move!", cmd);
		return;
	}

	if ( ship->steering.heading < 0 )
	{
		ship_dispatch_message(ch, ship, "The vessel needs a heading first.  Please steer the vessel into a direction.", cmd);
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
			sprintf(buf, "Explicit distance values must be from 1 to %d", ship->index->move_steps);
			ship_dispatch_message(ch, ship, buf, cmd);
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
		ship_dispatch_message(ch, ship, "You may stop the vessel, or order half, full, some percentage speed or specific distance count.", cmd);
		return;
	}

	if( speed == SHIP_SPEED_STOPPED )
	{
		if( ship->ship_power > SHIP_SPEED_STOPPED )
		{
			act("You give the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->ship_power = speed;

			if( ship->oar_power > SHIP_SPEED_STOPPED )
			{
				ship_echo(ship, "You feel the vessel slowing down as the sails are lowered.");
				ship_set_move_steps(ship);
			}
			else
			{
				ship_echo(ship, "You feel the vessel stopping as the sails are lowered.");
				ship_stop(ship);
			}


			// TODO: Cancel chasing

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already stopped.", cmd);
		}
		return;
	}

	if( speed == SHIP_SPEED_FULL_SPEED )
	{
		if( ship->ship_power < SHIP_SPEED_FULL_SPEED )
		{
			act("You give the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->ship_power = speed;

			ship_echo(ship, "You feel the vessel gaining speed.");
			ship_set_move_steps(ship);

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The vessel is already going at full speed.", cmd);
		}
		return;
	}

	if( ship->ship_power > speed )
	{
		act("You give the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel slowing down.");
	}
	else if( ship->ship_power < speed )
	{
		act("You give the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel gaining speed.");
	}
	else
	{
		ship_dispatch_message(ch, ship, "The vessel is already going at that speed.", cmd);
		return;
	}

	ship->ship_power = URANGE(1,speed,100);
	ship_set_move_steps(ship);

    ship_autosurvey(ship);

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
	}
}

void do_ship_speed( CHAR_DATA *ch, char *argument )
{
	//char buf[MSL];
	char arg[MAX_INPUT_LENGTH];
	SHIP_DATA *ship;
	char cmd[MIL];

	sprintf(cmd, "ship speed %s", argument);

	//char *command = argument;
	argument = one_argument( argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->index->move_steps < 1 )
	{
		send_to_char("The vessel is unable to move.\n\r", ch);
		return;
	}

	int speed = 100 * ship->move_steps / ship->index->move_steps;
	speed = UMAX(0, speed);

	if( speed == SHIP_SPEED_STOPPED )
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
	else if( speed >= 45 && ship->ship_power <= 55 )
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

void do_ship_aim( CHAR_DATA *ch, char *argument )
{
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
		ch->in_room->ship->ship_power = SHIP_SPEED_STOPPED;
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
	int skill = 0;

	argument = one_argument(argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( arg[0] == '\0' )
	{
		send_to_char("Syntax:  ship navigate goto <waypoint>\n\r", ch);
		send_to_char("         ship navigate seek <south> <east>\n\r", ch);
		send_to_char("         ship navigate plot <waypoint1> <waypoint2> ... <waypointN>\n\r", ch);
		send_to_char("         ship navigate cancel\n\r", ch);
		send_to_char("         ship navigate route\n\r", ch);
		send_to_char("         ship navigate route go <route>\n\r", ch);
		return;
	}

	bool use_navigator = false;

	if( !str_prefix(arg, "cancel") )
	{
		if( ship->seek_point.wilds != NULL )
		{
			ship_cancel_route(ship);
			send_to_char("Current route canceled.\n\r", ch);
		}
		else
		{
			send_to_char("The vessel has no active destination.\n\r", ch);
		}
		return;
	}

	if( !str_prefix(arg, "route") )
	{
		if( argument[0] == '\0' )
		{
			if( list_size(ship->route_waypoints) > 0 )
			{
				int stop = 0;
				ITERATOR it;
				WAYPOINT_DATA *wp;

				if( ship->current_route != NULL )
				{
					sprintf(buf, "{CCurrent Route {Y%s{C:{x\n\r", ship->current_route->name);
					send_to_char(buf, ch);
				}
				else if( ship->seek_point.wilds != NULL )
				{
					sprintf(buf, "{CCurrent Route in {Y%s{C:{x\n\r", ship->seek_point.wilds->name);
					send_to_char(buf, ch);
				}
				else
					send_to_char("{CCurrent Route:{x\n\r", ch);

				send_to_char("{C====================================={x\n\r", ch);
				send_to_char("{CStop  South   East{x\n\r", ch);
				iterator_start(&it, ship->route_waypoints);
				while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
				{
					char col = (ship->current_waypoint == wp) ? 'C' : 'c';
					char mark = (ship->current_waypoint == wp) ? '*' : ' ';

					sprintf(buf, "{%c%3d{Y%c  {G%5d  %5d  {Y%s{x\n\r", col, ++stop, mark, wp->y, wp->x, wp->name);
					send_to_char(buf, ch);
				}
				iterator_stop(&it);
				send_to_char("{C====================================={x\n\r", ch);
			}
			else if( ship->seek_point.wilds != NULL )
			{
				sprintf(buf, "{CCurrent Destination in {Y%s{C:{x\n\r", ship->seek_point.wilds->name);
				send_to_char(buf, ch);

				send_to_char("{C====================================={x\n\r", ch);
				send_to_char("{CStop  South   East{x\n\r", ch);
				sprintf(buf, "{C  1   {G%5d  %5d{x\n\r", ship->seek_point.y, ship->seek_point.x);
				send_to_char(buf, ch);

			}
			else
			{
				send_to_char("The vessel has no active destination.\n\r", ch);
			}
		}
		else
		{
			char arg2[MIL];

			argument = one_argument(argument, arg2);

			if( !str_prefix(arg2, "go") )
			{
				// Is navigator here?
				if( IS_VALID(ship->navigator) &&
					ship->navigator->crew &&
					ship->navigator->crew->navigation > 0 &&
					ship->navigator->in_room == ch->in_room)
				{
					use_navigator = true;
					skill = ship->navigator->crew->navigation;
				}
				else
				{
					skill = get_skill(ch, gsn_navigation);

					if( skill < 1)
					{
						send_to_char("You need a Navigator present or {Wnavigation{x skill to perform navigation.\n\r", ch);
						return;
					}
				}

				SHIP_ROUTE *route = get_ship_route(ship, argument);

				if( !IS_VALID(route) )
				{
					send_to_char("No such route.\n\r", ch);
					return;
				}

				if( IS_NULLSTR(route->name) )
				{
					send_to_char("Please name the route first.\n\r", ch);
					return;
				}

				if( list_size(route->waypoints) < 1 )
				{
					sprintf(buf, "Route '%s' has no waypoints assigned.\n\r", route->name);
					send_to_char(buf, ch);
					return;
				}

				ship_cancel_route(ship);

				ITERATOR it;
				WAYPOINT_DATA *wp;
				iterator_start(&it, route->waypoints);
				while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
				{
					wp = clone_waypoint(wp);

					list_appendlink(ship->route_waypoints, wp);
				}
				iterator_stop(&it);

				iterator_start(&ship->route_it, ship->route_waypoints);

				wp = (WAYPOINT_DATA *)iterator_nextdata(&ship->route_it);

				set_seek_point(&ship->seek_point, NULL, wp->w, wp->x, wp->y, skill);
				ship->seek_navigator = use_navigator;
				ship->current_waypoint = wp;
				ship->current_route = route;

				sprintf(buf, "Route {Y%s{x started.\n\r", route->name);
				send_to_char(buf, ch);

				if( use_navigator )
				{
					crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
				}

				return;
			}
		}

		return;
	}

	if( !str_prefix(arg, "goto") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Goto where?\n\r"
						 "Syntax: navigate goto <#.named waypoint>\n\r", ch);
			return;
		}

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
			skill = ship->navigator->crew->navigation;
		}
		else
		{
			skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to perform navigation.\n\r", ch);
				return;
			}
		}


		WILDS_DATA *wilds = NULL;
		room = obj_room(ship->ship);
		if( IS_WILDERNESS(room) )
			wilds = room->wilds;
		else if( room->area->wilds_uid > 0 )
			wilds = get_wilds_from_uid(NULL, room->area->wilds_uid);

		if( !wilds )
		{
			send_to_char("Cannot find a way to get there.\n\r", ch);
			return;
		}

		WAYPOINT_DATA *wp = get_ship_waypoint(ship, argument, wilds);
		if( !wp )
		{
			send_to_char("Cannot find a way to get there.\n\r", ch);
			return;
		}

		ship_cancel_route(ship);

		set_seek_point(&ship->seek_point, wilds, wilds->uid, wp->x, wp->y, skill);
		ship->seek_navigator = use_navigator;

		/*

		AREA_DATA *area;
		for(area = area_first; area; area = area->next)
		{
			if( area->wilds_uid == wilds->uid &&
				(area->x >= 0 && area->x < wilds->map_size_x) &&
				(area->y >= 0 && area->y < wilds->map_size_y) &&
				!str_infix(argument, area->name) )
			{
				break;
			}
		}

		if( !area )
		{
			send_to_char("No such place exists.\n\r", ch);
			return;
		}


		ship->seek_point.wilds = wilds;
		ship->seek_point.w = wilds->uid;
		ship->seek_point.x = area->x;
		ship->seek_point.y = area->y;
		*/

		send_to_char("{WLocation set.{x\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "seek") )
	{
		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
			skill = ship->navigator->crew->navigation;
		}
		else
		{
			skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to perform navigation.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Seek what point?\n\r"
						 "Syntax: ship navigate seek [south] [east]\n\r", ch);
			return;
		}

		WILDS_DATA *wilds = NULL;
		room = obj_room(ship->ship);
		if( IS_WILDERNESS(room) )
			wilds = room->wilds;
		else if( room->area->wilds_uid > 0 )
			wilds = get_wilds_from_uid(NULL, room->area->wilds_uid);

		if( !wilds )
		{
			send_to_char("Cannot find a way to get there.\n\r", ch);
			return;
		}

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

		ship_cancel_route(ship);

		set_seek_point(&ship->seek_point, wilds, wilds->uid, east, south, skill);
		ship->seek_navigator = use_navigator;

		send_to_char("{WSeek point set.{x\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "plot") )
	{
		char arg[MIL];

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
			skill = ship->navigator->crew->navigation;
		}
		else
		{
			skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to perform navigation.\n\r", ch);
				return;
			}
		}

		if( argument[0] == '\0' )
		{
			send_to_char("Plot what route?\n\r"
						 "Syntax: ship navigate plot [waypoint1] [waypoint2] ...  [waypointN]\n\r", ch);
			return;
		}

		WILDS_DATA *wilds = NULL;
		room = obj_room(ship->ship);
		if( IS_WILDERNESS(room) )
			wilds = room->wilds;
		else if( room->area->wilds_uid > 0 )
			wilds = get_wilds_from_uid(NULL, room->area->wilds_uid);

		if( !wilds )
		{
			send_to_char("Cannot find a way to get there.\n\r", ch);
			return;
		}

		ship_cancel_route(ship);
		WAYPOINT_DATA *wp;

		while(argument[0] != '\0')
		{
			argument = one_argument(argument, arg);

			wp = get_ship_waypoint(ship, arg, NULL);

			if( !wp )
			{
				ship_cancel_route(ship);
				sprintf(buf, "No such waypoint '%s'.\n\r", arg);
				send_to_char(buf, ch);
				return;
			}

			if( wp->w != wilds->uid )
			{
				ship_cancel_route(ship);
				sprintf(buf, "Invalid waypoint '%s'.  Must be in the current wilderness.\n\r", arg);
				send_to_char(buf, ch);
				return;
			}

			wp = clone_waypoint(wp);

			list_appendlink(ship->route_waypoints, wp);
		}

		iterator_start(&ship->route_it, ship->route_waypoints);

		wp = (WAYPOINT_DATA *)iterator_nextdata(&ship->route_it);

		set_seek_point(&ship->seek_point, NULL, wp->w, wp->x, wp->y, skill);
		ship->seek_navigator = use_navigator;
		ship->current_waypoint = wp;

		send_to_char("{WCourse plotted.{x\n\r", ch);
		return;
	}

	do_ship_navigate(ch, "");
}

void do_ship_oars( CHAR_DATA *ch, char *argument )
{
	char buf[MSL];
	char arg[MAX_INPUT_LENGTH];
	SHIP_DATA *ship;
	char cmd[MIL];

	sprintf(cmd, "ship oars %s", argument);

	char *command = argument;
	argument = one_argument( argument, arg);

	ship = get_room_ship(ch->in_room);

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( ship->oars < 1 )
	{
		act("The vessel has no oars.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if( arg[0] == '\0' )
	{
		if( ship->oar_power > SHIP_SPEED_STOPPED )
			sprintf(buf, "The vessel is currently running at {W%d%%{x oar power.", ship->oar_power);
		else
			sprintf(buf, "The vessel is not using oars currently.");

		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'sails $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_sails, command);
					}
					else
						do_ship_speed(ship->first_mate, command);

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if ( !ship_has_enough_crew( ship ) )
	{
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", cmd);
		return;
	}

	if ( ship->index->move_steps < 1 )
	{
		ship_dispatch_message(ch, ship, "The vessel doesn't seem to have enough power to move!", cmd);
		return;
	}

	if ( ship->steering.heading < 0 )
	{
		ship_dispatch_message(ch, ship, "The vessel needs a heading first.  Please steer the vessel into a direction.", cmd);
		return;
	}

	int speed = -1;
	if ( is_percent(arg) )
	{
		speed = atoi(arg);
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
		ship_dispatch_message(ch, ship, "You may stop the vessel, or order half, full, some percentage speed.", cmd);
		return;
	}

	if( speed == SHIP_SPEED_STOPPED )
	{
		if( ship->oar_power > SHIP_SPEED_STOPPED )
		{
			act("You give the order for the oarsmen to cease.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for the oarsmen to cease.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->oar_power = speed;

			if( ship->ship_power > SHIP_SPEED_STOPPED )
			{
				ship_echo(ship, "You feel the vessel slowing down as the oarsmen cease their oarring.");
				ship_set_move_steps(ship);
			}
			else
			{
				ship_echo(ship, "You feel the vessel stopping as the oarsmen cease their oarring.");
				ship_stop(ship);
			}
			// TODO: Cancel chasing

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The oarsmen are already stopped.", cmd);
		}
		return;
	}

	if( speed == SHIP_SPEED_FULL_SPEED )
	{
		if( ship->oar_power < SHIP_SPEED_FULL_SPEED )
		{
			act("You give the order for full oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n gives the order for full oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			ship->oar_power = speed;

			ship_echo(ship, "You feel the vessel gaining speed.");
			ship_set_move_steps(ship);

			if( ch == ship->first_mate )
			{
				crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
			}
		}
		else
		{
			ship_dispatch_message(ch, ship, "The oarsmen are already going at full speed.", cmd);
		}
		return;
	}

	if( ship->oar_power > speed )
	{
		act("You give the order to reduce oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to reduce oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel slowing down.");
	}
	else if( ship->oar_power < speed )
	{
		act("You give the order to increase oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n gives the order to increase oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		ship_echo(ship, "You feel the vessel gaining speed.");
	}
	else
	{
		ship_dispatch_message(ch, ship, "The oarsmen are already going that rate.", cmd);
		return;
	}

	ship->oar_power = URANGE(1,speed,100);
	ship_set_move_steps(ship);

    ship_autosurvey(ship);

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
	}
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

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	if( !IS_NULLSTR(ship->ship_name) )
	{
		send_to_char("The vessel already has a name!\n\r", ch);
		return;
	}

	char *plaintext = nocolour(argument);

	if( strlen(plaintext) > 30 )
	{
		send_to_char("Please limit ship names to atmost 30 characters, minus color codes.\n\r", ch);
		free_string(plaintext);
		return;
	}

	free_string(ship->ship_name);
	ship->ship_name = str_dup(argument);
	ship->ship_name_plain = nocolour(ship->ship_name);

	// Install ship_name
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
	char buf[MSL];
	SHIP_DATA *ship = get_room_ship(ch->in_room);

	if( !IS_VALID(ship) )
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'land'.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_land, "");
					}
					else
						do_ship_land(ship->first_mate, "");

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if( ship->ship_type != SHIP_AIR_SHIP )
	{
		ship_dispatch_message(ch, ship, "Land?  The vessel can only be in water.", "ship land");
		return;
	}

	if ( !ship_has_enough_crew( ship ) )
	{
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", "ship land");
		return;
	}

	if( ship->ship_power > SHIP_SPEED_STOPPED )
	{
		ship_dispatch_message(ch, ship, "Please stop the vessel first, lest you crash.", "ship land");
		return;
	}

	if( ship->ship_power == SHIP_SPEED_LANDED )
	{
		ship_dispatch_message(ch, ship, "The vessel has already landed.", "ship land");
		return;
	}

	ROOM_INDEX_DATA *to_room = NULL;
	AREA_REGION *to_region = NULL;
	//AREA_DATA *to_area = NULL;

	if( argument[0] != '\0' )
	{
		// Target a nearby area
		int bestDistanceSq = 100;	// 10 distance radius

		long uid = ship->ship->in_room->wilds->uid;
		int x = ship->ship->in_room->x;
		int y = ship->ship->in_room->y;

		for( AREA_DATA *area = area_first; area; area = area->next )
		{
			if( str_infix(argument, area->name) ) continue;

			if( area->wilds_uid != uid ) continue;

			int distanceSq;
			AREA_REGION *region = get_closest_area_region(area, x, y, &distanceSq, TRUE);

			if( distanceSq >= 0 && distanceSq < bestDistanceSq )
			{
				bestDistanceSq = distanceSq;
				to_region = region;
			}
		}

		if( to_region == NULL )
		{
			ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
			return;
		}

		to_room = get_room_index(to_region->area, to_region->airship_land_spot);
		if( !to_room )
		{
			ship_dispatch_message(ch, ship, "There is no safe place to land the ship here.", "ship land");
			return;
		}
	}
	else
	{
		WILDS_TERRAIN *pTerrain = get_terrain_by_coors(ship->ship->in_room->wilds, ship->ship->in_room->x, ship->ship->in_room->y);

		if( !pTerrain )
		{
			ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
			return;
		}

		if( pTerrain->nonroom )
		{
			// Indicative of the terrain being part of some city region on the wilds
			if( pTerrain->template->sector_type != SECT_CITY )
			{
				ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
				return;
			}

			int bestDistanceSq = 400;	// 20 distance radius

			long uid = ship->ship->in_room->wilds->uid;
			int x = ship->ship->in_room->x;
			int y = ship->ship->in_room->y;

			for( AREA_DATA *area = area_first; area; area = area->next )
			{
				if( area->wilds_uid != uid ) continue;

				int distanceSq;
				AREA_REGION *region = get_closest_area_region(area, x, y, &distanceSq, TRUE);

				if( distanceSq >= 0 && distanceSq < bestDistanceSq )
				{
					bestDistanceSq = distanceSq;
					to_region = region;
				}
			}

			if( to_region == NULL )
			{
				ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
				return;
			}

			to_room = get_room_index(to_region->area, to_region->airship_land_spot);
			if( !to_room )
			{
				ship_dispatch_message(ch, ship, "There is no safe place to land the ship here.", "ship land");
				return;
			}
		}
	}

	ship->ship_power = SHIP_SPEED_LANDED;

	if( to_room && IS_VALID(to_region) )
	{
		sprintf(buf, "{WThe vessel descends down to {Y%s{W in {Y%s{W below.{x", to_region->name, to_region->area->name);
		ship_echo(ship, buf);

		if( IS_NULLSTR(ship->ship_name) )
		{
			sprintf(buf, "{W%s %s descends from above to land in {Y%s{W in {Y%s{W.{x", get_article(ship->index->name, true), ship->index->name, to_region->name, to_region->area->name);
		}
		else
		{
			sprintf(buf, "{WThe %s '{x%s{W' descends from above to land in {Y%s{W in {Y%s{W.{x", ship->index->name, ship->ship_name, to_region->name, to_region->area->name);
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

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
	}
}

void do_ship_launch(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	SHIP_DATA *ship = get_room_ship(ch->in_room);

	if( !IS_VALID(ship) )
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// First Mates can always execute orders on their ship as they were assigned that role
	if( ch != ship->first_mate )
	{
		if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act2, PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
		{
			act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if( !ship_can_issue_command(ch, ship) )
		{
			if( IS_VALID(ship->first_mate) && ship->first_mate->crew && ship->first_mate->crew->leadership > 0 )
			{
				SHIP_DATA *fm_ship = get_room_ship(ship->first_mate->in_room);

				if( ship != fm_ship )
				{
					send_to_char("{RYour first mate is not aboard this vessel.{x\n\r", ch);
				}
				else
				{
					int delay = (75 - ship->first_mate->crew->leadership) / 15;

					act("You give the order to your first mate to 'launch'.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					if( IS_IMMORTAL(ch) && IS_SET(ch->act, PLR_HOLYLIGHT) )
					{
						sprintf(buf, "{MFirst Mate Delay: {W%d{x\n\r", delay);
						send_to_char(buf, ch);
					}

					if( delay > 0 )
					{
						wait_function(ship->first_mate, NULL, EVENT_FUNCTION, delay - 1, do_ship_launch, "");
					}
					else
						do_ship_launch(ship->first_mate, "");

					return;
				}
			}

			act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if( ship->ship_type != SHIP_AIR_SHIP )
	{
		ship_dispatch_message(ch, ship, "The vessel must remain in the water.", "ship launch");
		return;
	}

	if ( !ship_has_enough_crew( ship ) )
	{
		ship_dispatch_message(ch, ship, "There isn't enough crew to order that command!", "ship launch");
		return;
	}

	if( ship->ship_power >= SHIP_SPEED_STOPPED )
	{
		ship_dispatch_message(ch, ship, "The vessel is already airborne.", "ship launch");
		return;
	}

	if( !ship->ship->in_room->wilds )
	{
		// In a fixed area
		AREA_DATA *area = ship->ship->in_room->area;
		AREA_REGION *aregion = get_room_region(ship->ship->in_room);
		WILDS_DATA *wilds = get_wilds_from_uid(NULL, area->wilds_uid);

		if( !wilds || !IS_VALID(aregion) || aregion->x < 0 || aregion->y < 0 )
		{
			ship_dispatch_message(ch, ship, "There is nowhere for the vessel to go.", "ship launch");
			return;
		}

		ROOM_INDEX_DATA *room = get_wilds_vroom(wilds, aregion->x, aregion->y);
		if( !room )
			room = create_wilds_vroom(wilds, aregion->x, aregion->y);


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

	ship->ship_power = SHIP_SPEED_STOPPED;
	ship_echo(ship, "{WThe vessel groans a bit before taking flight.{x");

	if( ch == ship->first_mate )
	{
		crew_skill_improve(ch, CREW_SKILL_LEADERSHIP);
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

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
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
	BUFFER *buffer;
	char buf[MSL];
	ITERATOR it;
	SHIP_DATA *ship;
	int lines = 0;
	bool error = false;

	if( IS_NPC(ch) ) return;

	buffer = new_buf();

	iterator_start(&it, ch->pcdata->ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		// Filter by name if specified
		if( argument[0] && is_name(argument, ship->ship->name) ) continue;

		int twidth = get_colour_width(ship->index->name) + 18;		// Type
		int nwidth = get_colour_width(ship->ship_name) + 30;		// Name
		// Location?

		char loc[MIL];
		get_ship_location(ch, ship, loc, MIL-1);

		sprintf(buf, "{W%3d{C)  {G%-*.*s   {x%-*.*s   {x%s{x\n\r", ++lines,
			twidth, twidth, ship->index->name,
			nwidth, nwidth, ship->ship_name,
			loc);

		if( !add_buf(buffer, buf) || (!ch->lines && strlen(buffer->string) > MSL) )
		{
			error = true;
			break;
		}
	}
	iterator_stop(&it);

	if( error )
	{
		send_to_char("Too many ships to list.  Please shorten,\n\r", ch);
	}
	else if(!lines)
	{
		send_to_char("You do not own any ships.\n\r", ch);
	}
	else
	{
		send_to_char("{C     [{B       Type       {C] [{B             Name             {C] [{B           Location           {C]{x\n\r", ch);
		send_to_char("{C============================================================================================{x\n\r", ch);

		if(ch->lines > 0)
		{
			page_to_char(buffer->string, ch);
		}
		else
		{
			send_to_char(buffer->string, ch);
		}
	}

	free_buf(buffer);
}

void do_ship_waypoints(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	char buf[MSL];
	char arg[MIL];

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	ROOM_INDEX_DATA *room = obj_room(ship->ship);

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' )
	{
		send_to_char("Syntax:  ship waypoints list[ <filter> ... <filter>]\n\r"
					 "         ship waypoints list filters\n\r"
					 "         ship waypoints add here[ <name>]\n\r"
					 "         ship waypoints add <south> <east>[ <name>]\n\r"
					 "         ship waypoints delete <#>\n\r"
					 "         ship waypoints rename <#> <name>\n\r"
					 "         ship waypoints move <#from> <#to>\n\r"
					 "         ship waypoints move <#from> up\n\r"
					 "         ship waypoints move <#from> down\n\r"
					 "         ship waypoints load <map>\n\r"
					 "         ship waypoints save <#>[ <map>]\n\r", ch);

		return;
	}

	if( !str_prefix(arg, "list") )
	{
		char name[MIL];
		char map_name[MIL];
		int s1, e1;
		int s2, e2;

		bool has_bb = false;
		bool has_map = false;
		bool has_name = false;

		if( argument[0] != '\0' )
		{
			if( !str_prefix(argument, "filters") )
			{
				send_to_char("Waypoint Filters:\n\r", ch);
				send_to_char("  {Ycoords {Wsouth1 east1 south2 east2{x - filter by bounding box\n\r", ch);
				send_to_char("  {Ymap {Wname                        {x - filter by map name\n\r", ch);
				send_to_char("  {Yname {Wname                       {x - filter by waypoint name\n\r", ch);
				send_to_char("  {Yname {W-                          {x - only include waypoints with no name\n\r", ch);
				send_to_char("\n\r{WFilters can be combined.\n\r", ch);
				return;
			}

			while( argument[0] != '\0' )
			{
				char filter[MIL];

				argument = one_argument(argument, filter);

				if( !str_prefix(filter, "coords") )
				{
					char argS1[MIL];
					char argE1[MIL];
					char argS2[MIL];
					char argE2[MIL];

					argument = one_argument(argument, argS1);
					argument = one_argument(argument, argE1);
					argument = one_argument(argument, argS2);
					argument = one_argument(argument, argE2);

					if( !is_number(argS1) || !is_number(argE1) || !is_number(argS2) || !is_number(argE2) )
					{
						send_to_char("Coords argument not a number.\n\r", ch);
						return;
					}

					s1 = atoi(argS1);
					e1 = atoi(argE1);
					s2 = atoi(argS2);
					e2 = atoi(argE2);

					if( s1 > s2 )
					{
						int temp = s1;
						s1 = s2;
						s2 = temp;
					}

					if( e1 > e2 )
					{
						int temp = e1;
						e1 = e2;
						e2 = temp;
					}

					has_bb = true;
				}
				else if( !str_prefix(filter, "map") )
				{
					argument = one_argument(argument, map_name);

					if( map_name[0] == '\0' )
					{
						send_to_char("Missing map name.\n\r", ch);
						return;
					}

					has_map = true;
				}
				else if( !str_prefix(filter, "name") )
				{
					argument = one_argument(argument, name);

					if( name[0] == '\0' )
					{
						send_to_char("Waypoint name.\n\r", ch);
						return;
					}

					if( name[0] == '-' )
					{
						name[0] = '\0';
					}

					has_name = true;
				}


			}
		}


		if (list_size(ship->waypoints) > 0)
		{
			int cnt = 0;
			int shown = 0;
			ITERATOR wit;
			WAYPOINT_DATA *wp;
			WILDS_DATA *wilds;

			BUFFER *buffer = new_buf();

			add_buf(buffer, "{BCartographer Waypoints:{x\n\r\n\r");
			add_buf(buffer, "{B     [     Wilderness     ] [ South ] [  East ] [        Name        ]{x\n\r");
			add_buf(buffer, "{B======================================================================={x\n\r");

			iterator_start(&wit, ship->waypoints);
			while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
			{
				++cnt;

				wilds = get_wilds_from_uid(NULL, wp->w);

				if( has_bb )
				{
					if( wp->y < s1 || wp->y > s2 ) continue;
					if( wp->x < e1 || wp->x > e2 ) continue;
				}

				if( has_name )
				{
					if( name[0] == '\0' )
					{
						if( wp->name[0] != '\0' ) continue;
					}
					else if(!is_name(name, wp->name) )
					{
						continue;
					}
				}

				if( has_map )
				{
					if( !wilds || !is_name(map_name, wilds->name) ) continue;
				}

				char *wname = wilds ? wilds->name : "{D(null){x";

				int wwidth = get_colour_width(wname) + 20;

				sprintf(buf, "{B%3d{b)  {W%-*.*s    {G%5d     %5d    {Y%s{x\n\r",
					cnt,
					wwidth, wwidth, wname,
					wp->y, wp->x, wp->name);

				add_buf(buffer, buf);
				shown++;
			}

			iterator_stop(&wit);

			add_buf(buffer, "\n\r");

			if( shown > 0 )
			{
				page_to_char(buffer->string, ch);
			}
			else if( cnt > 0 )
			{
				send_to_char("No waypoints in filter.\n\r", ch);
			}
			else
			{
				send_to_char("No waypoints to display.\n\r", ch);
			}

			free_buf(buffer);
		}
		else
			send_to_char("No waypoints to display.\n\r", ch);

		return;
	}

	bool use_navigator = false;

	if( !str_prefix(arg, "add") )
	{
		char arg2[MIL];
		char arg3[MIL];
		int x, y;

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		argument = one_argument(argument, arg2);

		if( is_number(arg2) )
		{
			argument = one_argument(argument, arg3);

			if( !is_number(arg3) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return;
			}

			y = atoi(arg2);
			x = atoi(arg3);

			if( use_navigator )
			{
				int skill = ship->navigator->crew->navigation;

				if( IS_WILDERNESS(room) )
				{
					// Navigator can screw up the waypoint location
					if( number_percent() > skill )
					{
						y += number_range(-1, 1);
						y = URANGE(0, y, room->wilds->map_size_y - 1);

						x += number_range(-1, 1);
						x = URANGE(0, x, room->wilds->map_size_x - 1);
					}
				}
			}

		}
		else if( !str_prefix(arg2, "here") )
		{
			if( use_navigator )
			{
				int skill = ship->navigator->crew->navigation;

				if( IS_WILDERNESS(room) )
				{
					y = room->y;
					x = room->x;

					if( number_percent() > skill )
					{
						y += number_range(-30, 30);
						y = URANGE(0, y, room->wilds->map_size_y - 1);

						x += number_range(-30, 30);
						x = URANGE(0, x, room->wilds->map_size_x - 1);
					}
				}
			}
			else
			{
				if( ship->sextant_x < 0 || ship->sextant_y < 0 )
				{
					send_to_char("You have no idea where you are.  Please look at a sextant first.\n\r", ch);
					return;
				}

				y = ship->sextant_y;
				x = ship->sextant_x;
			}
		}
		else
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		if( !IS_WILDERNESS(room) )
		{
			send_to_char("The vessel is not in the wilderness.\n\r", ch);
			return;
		}

		WILDS_DATA *wilds = room->wilds;

		if( y < 0 || y >= wilds->map_size_y )
		{
			sprintf(buf, "South coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_y - 1);
			send_to_char(buf, ch);
			return;
		}

		if( x < 0 || x >= wilds->map_size_x )
		{
			sprintf(buf, "East coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_x - 1);
			send_to_char(buf, ch);
			return;
		}

		// Only Airships can specify waypoints over any terrain
		if( ship->ship_type != SHIP_AIR_SHIP )
		{
			WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, x, y);
			if( !terrain || terrain->nonroom ||
				(terrain->template->sector_type == SECT_WATER_SWIM &&
				 terrain->template->sector_type == SECT_WATER_NOSWIM) )
			{
				send_to_char("You can only specify locations over water.\n\r", ch);
				return;
			}
		}

		smash_tilde(argument);

		WAYPOINT_DATA *wp;
		ITERATOR wit;

		iterator_start(&wit, ship->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
		{
			if( wp->w == wilds->uid && wp->x == x && wp->y == y )
			{
				break;
			}
		}
		iterator_stop(&wit);

		if( wp == NULL )
		{
			wp = new_waypoint();

			free_string(wp->name);
			wp->name = nocolour(argument);
			wp->w = room->wilds->uid;
			wp->x = x;
			wp->y = y;

			list_appendlink(ship->waypoints, wp);
			send_to_char("Waypoint added.\n\r", ch);
		}
		else
		{
			send_to_char("Location already in waypoint list.\n\r", ch);
		}

		if( use_navigator )
		{
			// Improve the navigator's navigation skill
			crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
		}
		return;
	}

	if( !str_prefix(arg, "delete") )
	{
		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(argument);
		if( value < 1 || value > list_size(ship->waypoints) )
		{
			send_to_char("No such waypoint.\n\r", ch);
			return;
		}

		WAYPOINT_DATA *wp = (WAYPOINT_DATA *)list_nthdata(ship->waypoints, value);
		ITERATOR rit;
		SHIP_ROUTE *route;
		iterator_start(&rit, ship->routes);
		while( (route = (SHIP_ROUTE *)iterator_nextdata(&rit)) )
		{
			// Remove waypoint from route (if it is in there)
			list_remlink(route->waypoints, wp);
		}
		iterator_stop(&rit);

		list_remnthlink(ship->waypoints, value);
		send_to_char("Waypoint deleted.\n\r", ch);

		if( use_navigator )
		{
			// Improve the navigator's navigation skill
			crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
		}
		return;
	}

	if( !str_prefix(arg, "rename") )
	{
		char arg2[MIL];

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(arg2);
		if( value < 1 || value > list_size(ship->waypoints) )
		{
			send_to_char("No such waypoint.\n\r", ch);
			return;
		}

		WAYPOINT_DATA *wp = (WAYPOINT_DATA *)list_nthdata(ship->waypoints, value);

		smash_tilde(argument);
		free_string(wp->name);
		wp->name = nocolour(argument);

		send_to_char("Waypoint renamed.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "move") )
	{
		char arg2[MIL];

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int total = list_size(ship->waypoints);

		int from = atoi(arg2);
		if( from < 1 || from > total )
		{
			send_to_char("Source position out of range.\n\r", ch);
			return;
		}

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ship waypoints move <#from> <#to>\n\r", ch);
			send_to_char("         ship waypoints move <#from> up\n\r", ch);
			send_to_char("         ship waypoints move <#from> down\n\r", ch);
			return;
		}

		if( is_number(argument) )
		{
			int to = atoi(argument);

			if( to < 1 || to > total )
			{
				send_to_char("Target position out of range.\n\r", ch);
				return;
			}

			if( to == from )
			{
				if( number_percent() < 2 )
				{
					send_to_char("What would be the {W(way){xpoint?\n\r", ch);
					send_to_char("\n\r ...Get it?\n\r", ch);
					send_to_char("   ...You're trying to move a waypoint to the same location?\n\r", ch);
					send_to_char("\n\r{Y[ sad server noises ]{x\n\r", ch);
				}
				else
				{
					send_to_char("What would be the point?\n\r", ch);
				}
				return;
			}

			list_movelink(ship->waypoints, from, to);
			send_to_char("Waypoint moved.\n\r", ch);
			return;
		}
		else if( !str_prefix(argument, "up") )
		{
			if( from > 1 )
			{
				list_movelink(ship->waypoints, from, from - 1);
				send_to_char("Waypoint moved.\n\r", ch);
			}
			else
			{
				send_to_char("Waypoint is already at the top of the list.\n\r", ch);
			}
			return;
		}
		else if( !str_prefix(argument, "down") )
		{
			if( from < total )
			{
				list_movelink(ship->waypoints, from, from + 1);
				send_to_char("Waypoint moved.\n\r", ch);
			}
			else
			{
				send_to_char("Waypoint is already at the bottom of the list.\n\r", ch);
			}
			return;
		}

		do_ship_waypoints(ch, "move");
		return;
	}

	if( !str_prefix(arg, "load") )
	{
		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		OBJ_DATA *map = get_obj_carry(ch, argument, ch);
		if( !IS_VALID(map) )
		{
			send_to_char("You don't have that.\n\r", ch);
			return;
		}

		if( map->item_type != ITEM_MAP )
		{
			send_to_char("That is not a map.\n\r", ch);
			return;
		}

		if( list_size(map->waypoints) < 1 )
		{
			act("{xThere are no waypoints on $p{x.", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		int copied = 0;
		int updated = 0;
		ITERATOR wit;
		WAYPOINT_DATA *wp;
		iterator_start(&wit, map->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
		{
			WILDS_DATA *wilds = get_wilds_from_uid(NULL, wp->w);
			if( !wilds ) continue;
			if( ship->ship_type != SHIP_AIR_SHIP )
			{
				WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, wp->x, wp->y);
				if( !terrain || terrain->nonroom ||
					(terrain->template->sector_type == SECT_WATER_SWIM &&
					 terrain->template->sector_type == SECT_WATER_NOSWIM) )
				{
					continue;
				}
			}

			ITERATOR it;
			WAYPOINT_DATA *ws;
			iterator_start(&it, ship->waypoints);
			while( (ws = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
			{
				if( ws->w == wp->w && ws->x == wp->x && ws->y == wp->y )
				{
					break;
				}
			}
			iterator_stop(&it);

			if( ws )
			{
				// Update the name
				free_string(ws->name);
				ws->name = str_dup(wp->name);
				updated++;
			}
			else
			{
				ws = clone_waypoint(wp);
				list_appendlink(ship->waypoints, ws);
				copied++;
			}
		}
		iterator_stop(&wit);

		if( copied > 0 )
		{
			if(updated > 0)
			{
				sprintf(buf, "{W%d{x copied.  {W%d{x updated.\n\r", copied, updated);
			}
			else
			{
				sprintf(buf, "{W%d{x copied.\n\r", copied);
			}
		}
		else if(updated > 0)
		{
			sprintf(buf, "{W%d{x updated.\n\r", updated);
		}
		else
		{
			sprintf(buf, "No waypoints were copied nor updated.\n\r");
		}

		send_to_char(buf, ch);

		if( use_navigator )
		{
			// Improve the navigator's navigation skill
			crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
		}
		return;
	}

	if( !str_prefix(arg, "save") )
	{
		char arg2[MIL];
		OBJ_DATA *map;

		// Is navigator here?
		if( IS_VALID(ship->navigator) &&
			ship->navigator->crew &&
			ship->navigator->crew->navigation > 0 &&
			ship->navigator->in_room == ch->in_room)
		{
			use_navigator = true;
		}
		else
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage waypoints.\n\r", ch);
				return;
			}
		}

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(arg2);

		if( value < 1 || value > list_size(ship->waypoints) )
		{
			send_to_char("No such waypoint.\n\r", ch);
			return;
		}

		WAYPOINT_DATA *wp = (WAYPOINT_DATA *)list_nthdata(ship->waypoints, value);

		if( IS_NULLSTR(argument) )
		{
			map = NULL;
			for (map = ch->carrying; map != NULL; map = map->next_content) {
				if (map->item_type == ITEM_BLANK_SCROLL || map->pIndexData == obj_index_blank_scroll)
					break;
			}

			if (map == NULL)
			{
				send_to_char("You do not have a blank scroll.\n\r", ch);
				return;
			}

			extract_obj(map);
			map = create_object(obj_index_navigational_chart, 0, FALSE);
			obj_to_char(map, ch);
		}
		else
		{
			map = get_obj_carry(ch, argument, ch);
			if( !IS_VALID(map) )
			{
				send_to_char("You don't have that.\n\r", ch);
				return;
			}

			if( map->item_type == ITEM_BLANK_SCROLL || map->pIndexData == obj_index_blank_scroll )
			{
				// Replace blank scroll with map object
				extract_obj(map);
				map = create_object(obj_index_navigational_chart, 0, FALSE);
				obj_to_char(map, ch);
			}
			else if( map->item_type != ITEM_MAP )
			{
				send_to_char("That is not a map nor a blank scroll.\n\r", ch);
				return;
			}

			if( map->waypoints )
			{
				ITERATOR it;
				WAYPOINT_DATA *wm;
				iterator_start(&it, map->waypoints);
				while( (wm = (WAYPOINT_DATA *)iterator_nextdata(&it)) )
				{
					if( wp->w == wm->w && wp->x == wm->x && wp->y == wm->y )
						break;
				}
				iterator_stop(&it);

				if( wm )
				{
					act("{YThat coordinate is already on $p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR);
					return;
				}
			}

		}

		if( !map->waypoints )
		{
			map->waypoints = new_waypoints_list();
		}

		wp = clone_waypoint(wp);

		list_appendlink(map->waypoints, wp);

		if( use_navigator )
		{
		    act("{x$N{Y jots something down onto {x$p{Y and hands it to {x$n{Y.{x", ch, ship->navigator, NULL, map, NULL, NULL, NULL, TO_NOTVICT);
		    act("{x$N{Y jots something down onto {x$p{Y and hands it to you.{x", ch, ship->navigator, NULL, map, NULL, NULL, NULL, TO_CHAR);

			// Improve the navigator's navigation skill
			crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
		}
		else
		{
		    act("{x$n{Y jots something down onto {x$p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_ROOM);
		    act("{YYou jot down coordinates onto {x$p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR);
		}
		return;
	}

	do_ship_waypoints(ch, "");
}

void do_ship_routes(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	char buf[MSL];
	char arg[MIL];

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' )
	{
		send_to_char("Syntax:  ship routes list\n\r"
					 "         ship routes create <waypoint1> <waypoint2> ... <waypointN>\n\r"
					 "         ship routes delete <#>\n\r"
					 "         ship routes rename <#> <name>\n\r"
					 "         ship routes add <#> <waypoint>\n\r"
					 "         ship routes insert <#> <waypoint> <pos#>\n\r"
					 "         ship routes remove <route#> <waypoint#>\n\r"
					 "         ship routes move <#from> <#to>\n\r"
					 "         ship routes move <#from> up\n\r"
					 "         ship routes move <#from> down\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "list") )
	{
		if( list_size(ship->routes) > 0 )
		{
			int cnt = 0;
			ITERATOR it, wit;
			SHIP_ROUTE *route;
			WAYPOINT_DATA *wp;

			BUFFER *buffer = new_buf();

			add_buf(buffer,"{C                Route Name            Waypoints{x\n\r");
			add_buf(buffer,"{C======================================================================{x\n\r");

			iterator_start(&it, ship->routes);
			while( (route = (SHIP_ROUTE *)iterator_nextdata(&it)) )
			{
				if( list_size(route->waypoints) > 0 )
				{
					int j = 0;
					int w = 83;

					j = sprintf(buf, "{C%3d)  {Y%-30.30s{x ", ++cnt, route->name);

					iterator_start(&wit, route->waypoints);
					while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
					{
						if(j > w)
						{
							buf[j] = '\0';
							add_buf(buffer, buf);
							add_buf(buffer,"\n\r");

							j = sprintf(buf, "%-37.37s", " ");
							w = 77;
						}

						if( wp->name[0] != '\0' )
						{
							j += sprintf(buf + j, " {Y%s{x,", wp->name);
							w += 4;
						}
						else
						{
							j += sprintf(buf + j, " {W({C%d{W,{C%d{W){x,", wp->y, wp->x);
							w += 12;
						}
					}
					iterator_stop(&wit);

					if( j > 0 )
					{
						buf[j-1] = '\0';
						add_buf(buffer, buf);
						add_buf(buffer,"\n\r");
					}
				}
				else
				{
					sprintf(buf, "{C%3d)  {Y%-30.30s{x  {Dempty{x\n\r", ++cnt, route->name);
					add_buf(buffer, buf);
				}
			}
			iterator_stop(&it);

			add_buf(buffer,"{C======================================================================{x\n\r");

			page_to_char(buffer->string, ch);

			free_buf(buffer);
		}
		else
			send_to_char("No routes to display.\n\r", ch);

		return;
	}

	if( !str_prefix(arg, "create") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg[MIL];
		SHIP_ROUTE *route = new_ship_route();

		WAYPOINT_DATA *wp;
		long uid = 0;

		while(argument[0] != '\0')
		{
			argument = one_argument(argument, arg);

			wp = get_ship_waypoint(ship, arg, NULL);

			if( !wp )
			{
				free_ship_route(route);
				sprintf(buf, "No such waypoint '%s'.\n\r", arg);
				send_to_char(buf, ch);
				return;
			}

			if( uid && wp->w != uid )
			{
				free_ship_route(route);
				sprintf(buf, "Invalid waypoint '%s'.  All waypoints in a route must be in the same wilderness.\n\r", arg);
				send_to_char(buf, ch);
				return;
			}

			list_appendlink(route->waypoints, wp);	// NOT a clone
		}

		list_appendlink(ship->routes, route);
		send_to_char("Route added.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "delete") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(argument);
		if( value < 1 || value > list_size(ship->routes) )
		{
			send_to_char("No such route.\n\r", ch);
			return;
		}

		list_remnthlink(ship->routes, value);
		send_to_char("Route deleted.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "rename") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(arg2);
		if( value < 1 || value > list_size(ship->routes) )
		{
			send_to_char("No such route.\n\r", ch);
			return;
		}

		SHIP_ROUTE *route = (SHIP_ROUTE *)list_nthdata(ship->routes, value);

		smash_tilde(argument);
		free_string(route->name);
		route->name = nocolour(argument);

		send_to_char("Route renamed.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "add") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(arg2);
		if( value < 1 || value > list_size(ship->routes) )
		{
			send_to_char("No such route.\n\r", ch);
			return;
		}

		SHIP_ROUTE *route = (SHIP_ROUTE *)list_nthdata(ship->routes, value);

		WAYPOINT_DATA *first = (WAYPOINT_DATA *)list_nthdata(route->waypoints, 1);

		WAYPOINT_DATA *wp = get_ship_waypoint(ship, argument, NULL);

		if( wp->w != first->w )
		{
			sprintf(buf, "Invalid waypoint '%s'.  All waypoints in a route must be in the same wilderness.\n\r", argument);
			send_to_char(buf, ch);
			return;
		}

		list_appendlink(route->waypoints, wp);
		send_to_char("Waypoint added to Route.\n\r", ch);
		return;
	}


	if( !str_prefix(arg, "insert") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];
		char arg3[MIL];

		argument = one_argument(argument, arg2);
		argument = one_argument(argument, arg3);

		if( !is_number(arg2) || !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int value = atoi(arg2);
		if( value < 1 || value > list_size(ship->routes) )
		{
			send_to_char("No such route.\n\r", ch);
			return;
		}

		SHIP_ROUTE *route = (SHIP_ROUTE *)list_nthdata(ship->routes, value);

		int index = atoi(argument);
		int total = list_size(route->waypoints);
		if( index < 1 || index > (total + 1) )
		{
			send_to_char("Insert position out of range.\n\r", ch);
			return;
		}

		WAYPOINT_DATA *wp = get_ship_waypoint(ship, arg3, NULL);

		if( total > 0 )
		{
			WAYPOINT_DATA *first = (WAYPOINT_DATA *)list_nthdata(route->waypoints, 1);

			if( wp->w != first->w )
			{
				sprintf(buf, "Invalid waypoint '%s'.  All waypoints in a route must be in the same wilderness.\n\r", argument);
				send_to_char(buf, ch);
				return;
			}
		}

		list_insertlink(route->waypoints, wp, index);
		send_to_char("Waypoint added to Route.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "remove") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) || !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int rindex = atoi(arg2);
		if( rindex < 1 || rindex > list_size(ship->routes) )
		{
			send_to_char("No such route.\n\r", ch);
			return;
		}

		SHIP_ROUTE *route = (SHIP_ROUTE *)list_nthdata(ship->routes, rindex);

		int windex = atoi(argument);
		if( windex < 1 || windex > list_size(route->waypoints) )
		{
			send_to_char("No such waypoint in route.\n\r", ch);
			return;
		}

		list_remnthlink(route->waypoints, windex);
		send_to_char("Waypoint deleted from route.\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "move") )
	{
		// Is navigator here?
		if( !IS_VALID(ship->navigator) ||
			!ship->navigator->crew ||
			ship->navigator->crew->navigation < 1 ||
			ship->navigator->in_room != ch->in_room)
		{
			int skill = get_skill(ch, gsn_navigation);

			if( skill < 1)
			{
				send_to_char("You need a Navigator present or {Wnavigation{x skill to manage routes.\n\r", ch);
				return;
			}
		}

		char arg2[MIL];

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int total = list_size(ship->waypoints);

		int from = atoi(arg2);
		if( from < 1 || from > total )
		{
			send_to_char("Source position out of range.\n\r", ch);
			return;
		}

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  ship routes move <#from> <#to>\n\r", ch);
			send_to_char("         ship routes move <#from> up\n\r", ch);
			send_to_char("         ship routes move <#from> down\n\r", ch);
			return;
		}

		if( is_number(argument) )
		{
			int to = atoi(argument);

			if( to < 1 || to > total )
			{
				send_to_char("Target position out of range.\n\r", ch);
				return;
			}

			if( to == from )
			{
				send_to_char("What would be the point?\n\r", ch);
				return;
			}

			list_movelink(ship->routes, from, to);
			send_to_char("Route moved.\n\r", ch);
			return;
		}
		else if( !str_prefix(argument, "up") )
		{
			if( from > 1 )
			{
				list_movelink(ship->routes, from, from - 1);
				send_to_char("Route moved.\n\r", ch);
			}
			else
			{
				send_to_char("Route is already at the top of the list.\n\r", ch);
			}
			return;
		}
		else if( !str_prefix(argument, "down") )
		{
			if( from < total )
			{
				list_movelink(ship->routes, from, from + 1);
				send_to_char("Route moved.\n\r", ch);
			}
			else
			{
				send_to_char("Route is already at the bottom of the list.\n\r", ch);
			}
			return;
		}

		do_ship_waypoints(ch, "move");
		return;
	}

	do_ship_routes(ch, "");
}

void do_ship_keys(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	char buf[MSL];
	char arg[MIL];

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	if (list_size(ship->special_keys) < 1 )
	{
		send_to_char("The vessel has no special keys.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' )
	{
		send_to_char("Syntax:  ship keys list       - list available keys for the ship\n\r"
					 "         ship keys load <#>   - load a copy of the specified key\n\r"
					 "         ship keys purge <#>  - clears out the current list of allowed loaded keys\n\r", ch);
		return;
	}

	if( !str_prefix(arg, "list") )
	{
		ITERATOR it;
		SPECIAL_KEY_DATA *sk;
		bool error = false;
		int lines = 0;

		//char *name;
		//if( IS_NULLSTR(ship->ship_name) )
		//	name = ship->index->name;
		//else
		//	name = ship->ship_name;

		BUFFER *buffer = new_buf();

		iterator_start(&it, ship->special_keys);
		while( (sk = (SPECIAL_KEY_DATA *)iterator_nextdata(&it)) )
		{
			OBJ_INDEX_DATA *key = get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum);

			if( key && key->item_type == ITEM_KEY )
				strncpy(arg, key->short_descr, MIL-1);
			else
				strcpy(arg, "{D-invalid key-{x");

			int nwidth = get_colour_width(arg) + 30;

			sprintf(buf, "{Y%2d) {x%-*.*s {W%d{x\n\r", ++lines, nwidth, nwidth, arg, list_size(sk->list));
			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buffer->string) > MSL) )
			{
				error = true;
				break;
			}
		}
		iterator_stop(&it);

		if( error )
		{
			send_to_char("Too many special keys to display.  Please enable scrolling.\n\r", ch);
		}
		else if(!lines)
		{
			send_to_char("No special keys to display.\n\r", ch);
		}
		else
		{
			send_to_char("{Y    [          Key Name          ] [ Keys Issued ]{x\n\r", ch);
			send_to_char("{Y==================================================={x\n\r", ch);

			page_to_char(buffer->string, ch);
		}

		free_buf(buffer);
		return;
	}

	if( !str_prefix(arg, "load") )
	{
		int index;

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		index = atoi(argument);
		if( index < 1 || index > list_size(ship->special_keys))
		{
			send_to_char("That is not a valid key.\n\r", ch);
			return;
		}

		SPECIAL_KEY_DATA *sk = (SPECIAL_KEY_DATA *)list_nthdata(ship->special_keys, index);
		OBJ_INDEX_DATA *key_index = get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum);

		if( !key_index || key_index->item_type != ITEM_KEY )
		{
			send_to_char("Something is wrong with that key.  Please inform an immortal or file a bug report.\n\r", ch);
			return;
		}

		// Inventory management checks
		if( (ch->carry_number + 1) > can_carry_n(ch) )
		{
			send_to_char("Your hands are full.\n\r", ch);
			return;
		}

		if(get_carry_weight(ch) + key_index->weight > can_carry_w(ch))
		{
			send_to_char("You can't carry that much more weight.\n\r", ch);
			return;
		}

		LLIST_UID_DATA *luid = new_list_uid_data();
		if( !luid )
		{
			send_to_char("Could not generate the key.  Please inform an immortal or file a bug report.\n\r", ch);
			return;
		}

		OBJ_DATA *key = create_object(key_index, 0, TRUE);
		if( !IS_VALID(key) )
		{
			send_to_char("Could not generate the key.  Please inform an immortal or file a bug report.\n\r", ch);
			free_list_uid_data(luid);
			return;
		}

		char *descr;	// str_dup will be called for each
		if( !IS_NULLSTR(ship->ship_name) )
		{
			char *name = nocolour(ship->ship_name);
			sprintf(buf, "%s %s", key->name, name);
			free_string(key->name);
			key->name = str_dup(buf);
			free_string(name);

			sprintf(buf, "%s of '{x%s{x'", key->short_descr, name);
			free_string(key->short_descr);
			key->short_descr = str_dup(buf);

			sprintf(buf, "with '{x%s{x' etched on the shaft", ship->ship_name);
			descr = string_replace(key->description, "%NAME%", buf);
		}
		else
		{
			descr = string_replace(key->description, "%NAME%", "");
		}


		free_string(key->description);
		key->description = descr;


		luid->ptr = key;
		luid->id[0] = key->id[0];
		luid->id[1] = key->id[1];
		list_appendlink(sk->list, luid);

		obj_to_char(key, ch);
		act("{xA ship deckhand hands you $p{x.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_CHAR);
		act("{xA ship deckhand hands $n $p{x.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_ROOM);
		return;
	}

	if( !str_prefix(arg, "purge") )
	{
		int index;

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		index = atoi(argument);
		if( index < 1 || index > list_size(ship->special_keys))
		{
			send_to_char("That is not a valid key.\n\r", ch);
			return;
		}

		SPECIAL_KEY_DATA *sk = (SPECIAL_KEY_DATA *)list_nthdata(ship->special_keys, index);

		list_clear(sk->list);

		send_to_char("{YAll authorized access for this key has been purged.{x\n\r", ch);
		send_to_char("Previously loaded copies are hereby void and useless.\n\r", ch);
		return;
	}

	do_ship_keys(ch, "");
}

static void crew_skill_rating(char *field, int rating, char *buf, size_t len)
{
	static char *ratings[11] = {
		"          ",
		"{b*         ",
		"{b**        ",
		"{b***       ",
		"{b***{B*      ",
		"{b***{B**     ",
		"{b***{B***    ",
		"{b***{B***{C*   ",
		"{b***{B***{C**  ",
		"{b***{B***{C*** ",
		"{b***{B***{C***{W*"
	};

	int rating10 = rating / 10;

	rating10 = URANGE(0, rating10, 10);

	snprintf(buf, len, "{C%-15.15s: {x[ %s {x] {W%d%%{x\n\r", field, ratings[rating10], rating);
}

void do_ship_crew(CHAR_DATA *ch, char *argument)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	char buf[MSL];
	char arg[MIL];
	char arg2[MIL];

	if (!IS_VALID(ship))
	{
		act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
	{
		send_to_char("This isn't your vessel.\n\r", ch);
		return;
	}

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  ship crew list\n\r"
					 "         ship crew info <#>\n\r"
					 "         ship crew remove <#>\n\r"
					 "         ship crew assign <#> <role>\n\r"
					 "         ship crew unassign <role>\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		if( list_size(ship->crew) > 0 )
		{
			ITERATOR it;
			CHAR_DATA *crew;
			BUFFER *buffer = new_buf();
			char buf[MSL];
			int lines = 0;
			bool error = false;

			add_buf(buffer,"{C    [            Name            ] [ Hiring ] [F] [N] [S] [O]{x\n\r");
			add_buf(buffer,"{C=============================================================={x\n\r");

			iterator_start(&it, ship->crew);
			while( (crew = (CHAR_DATA *)iterator_nextdata(&it)) )
			{
				int swidth = get_colour_width(crew->short_descr) + 30;

				char hired[MIL];

				if( crew->hired_to > 0 )
				{
					int delta = (int) ((crew->hired_to - current_time) / 60);

					if( delta < 1 )
						strcpy(hired, "<{W1{xh");
					else if( delta < 24 )
						sprintf(hired, "{W%d{xh", delta);
					else
						sprintf(hired, "{W%d{xd", delta / 24);
				}
				else
					strcpy(hired, "{D--{x");

				int hwidth = get_colour_width(hired) + 10;

				sprintf(buf, "{C%2d){x {Y%-*.*s {x%-*.*s %s %s %s %s{x\n\r", ++lines,
					swidth, swidth, crew->short_descr,
					hwidth, hwidth, hired,
					(ship->first_mate == crew) ? "{W Y " : "{D N ",
					(ship->navigator == crew) ? "{W Y " : "{D N ",
					(ship->scout == crew) ? "{W Y " : "{D N ",
					(list_hasdata(ship->oarsmen, crew)) ? "{W Y " : "{D N ");

				if( !add_buf(buffer, buf) || (!ch->lines && strlen(buffer->string) > MSL) )
				{
					error = true;
					break;
				}
			}
			iterator_stop(&it);

			if( error )
			{
				send_to_char("Too much crew to display.  Please change enable scrolling.\n\r", ch);
			}
			else if( !lines )
			{
				send_to_char("No crew to display.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}

			free_buf(buffer);
		}
		else
		{
			send_to_char("No crew to display.\n\r", ch);
		}

		return;
	}

	if( !str_prefix(arg, "info") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is number a number.\n\r", ch);
			return;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(ship->crew) )
		{
			send_to_char("That is not a valid crew member.\n\r", ch);
			return;
		}

		CHAR_DATA *crew = (CHAR_DATA *)list_nthdata(ship->crew, index);

		if( !crew->crew )
		{
			send_to_char("{WCrew member is missing crew data.  Please report to an immortal.\n\r", ch);
			return;
		}

		sprintf(buf, "{CInfo for Crew Member {W%d{x\n\r", index);
		send_to_char(buf, ch);

		sprintf(buf, "{CName:{x        %s\n\r", crew->short_descr);
		send_to_char(buf, ch);

		if( crew->hired_to > 0 )
		{
		sprintf(buf, "{CHired until:{x %s\n\r", (char *) ctime(&crew->hired_to));
		send_to_char(buf, ch);
		}

		send_to_char("{CSkill Ratings:{x\n\r", ch);
		send_to_char("{C-----------------------------{x\n\r",ch);

		crew_skill_rating("Scouting", crew->crew->scouting, buf, MSL-1);
		send_to_char(buf, ch);

		crew_skill_rating("Gunning", crew->crew->gunning, buf, MSL-1);
		send_to_char(buf, ch);

		crew_skill_rating("Oarring", crew->crew->oarring, buf, MSL-1);
		send_to_char(buf, ch);

		crew_skill_rating("Mechanics", crew->crew->mechanics, buf, MSL-1);
		send_to_char(buf, ch);

		crew_skill_rating("Navigating", crew->crew->navigation, buf, MSL-1);
		send_to_char(buf, ch);

		crew_skill_rating("Leadership", crew->crew->leadership, buf, MSL-1);
		send_to_char(buf, ch);

		send_to_char("{C-----------------------------{x\n\r",ch);
		send_to_char("\n\r", ch);

		if( ship->first_mate == crew )
			send_to_char("{CAssigned as {WFIRST MATE{C.{x\n\r", ch);

		if( ship->navigator == crew )
			send_to_char("{CAssigned as {WNAVIGATOR{C.{x\n\r", ch);

		if( ship->scout == crew )
			send_to_char("{CAssigned as {WSCOUT{C.{x\n\r", ch);

		if( list_hasdata(ship->oarsmen, crew) )
			send_to_char("{CAssigned as an {WOARSMAN{C.{x\n\r", ch);

		return;
	}

	if( !str_prefix(arg, "remove") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(ship->crew) )
		{
			send_to_char("That is not a valid crew member.\n\r", ch);
			return;
		}

		CHAR_DATA *crew = (CHAR_DATA *)list_nthdata(ship->crew, index);

		if( ship->first_mate == crew )
		{
			ship->first_mate = NULL;
			send_to_char("{WFirst Mate unassigned due to crew removal.{x\n\r", ch);
		}

		if( ship->navigator == crew )
		{
			ship->navigator = NULL;
			send_to_char("{WNavigator unassigned due to crew removal.{x\n\r", ch);
		}

		if( ship->scout == crew )
		{
			ship->scout = NULL;
			send_to_char("{WScout unassigned due to crew removal.{x\n\r", ch);
		}

		if( list_hasdata(ship->oarsmen, crew) )
		{
			list_remlink(ship->oarsmen, crew);
			send_to_char("{WOarsman unassigned due to crew removal.{x\n\r", ch);
		}

		list_remlink(ship->crew, crew);
		crew->belongs_to_ship = NULL;

		extract_char(crew, TRUE);

		send_to_char("Crew member removed.\n\r", ch);
		return;
	}


	if( !str_prefix(arg, "assign") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Assign which crew to what role?\n\r", ch);
			send_to_char("{YValid roles: {Wfirstmate{x, {Wnavigator{x, {Wscout{x, {Woarsman{x\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg2);

		if( !is_number(arg2) )
		{
			send_to_char("That is number a number.\n\r", ch);
			return;
		}

		int index = atoi(arg2);

		if( index < 1 || index > list_size(ship->crew) )
		{
			send_to_char("That is not a valid crew member.\n\r", ch);
			return;
		}

		CHAR_DATA *crew = (CHAR_DATA *)list_nthdata(ship->crew, index);

		if( !crew->crew )
		{
			send_to_char("{WCrew member is missing crew data.  Please report to an immortal.\n\r", ch);
			return;
		}

		if( argument[0] == '\0' )
		{
			send_to_char("Assign crew member to what role?\n\r", ch);
			send_to_char("{YValid roles: {Wfirstmate{x, {Wnavigator{x, {Wscout{x, {Woarsman{x\n\r", ch);
			send_to_char("{YNavigators and scouts are mutually exclusive.{x\n\r", ch);
			send_to_char("{YOarsmen and all other roles are mutually exclusive.{x\n\r", ch);
			return;
		}

		if( !str_prefix(argument, "firstmate") )
		{
			if( crew->crew->leadership < 1 )
			{
				send_to_char("That crew member lacks any '{WLeadership{x' ability.\n\r", ch);
				return;
			}

			if( list_hasdata(ship->oarsmen, crew) )
			{
				send_to_char("That crew member is already assigned as an {WOarsman{x.\n\r", ch);
				return;
			}

			ship->first_mate = crew;
			send_to_char("First Mate assigned.\n\r", ch);
			return;
		}

		if( !str_prefix(argument, "navigator") )
		{
			if( crew->crew->navigation < 1 )
			{
				send_to_char("That crew member lacks any '{WNavigation{x' ability.\n\r", ch);
				return;
			}

			if( ship->scout == crew )
			{
				send_to_char("That crew member is already assigned as the {WScout{x.\n\r", ch);
				return;
			}

			if( list_hasdata(ship->oarsmen, crew) )
			{
				send_to_char("That crew member is already assigned as an {WOarsman{x.\n\r", ch);
				return;
			}

			ship->navigator = crew;
			send_to_char("Navigator assigned.\n\r", ch);

			// Place navigator at the helm?
			return;
		}

		if( !str_prefix(argument, "oarsman") || !str_prefix(argument, "oarsmen") )
		{
			if( crew->crew->oarring < 1 )
			{
				send_to_char("That crew member lacks any '{WOarring{x' ability.\n\r", ch);
				return;
			}

			if( crew->max_move < 1 )
			{
				send_to_char("That crew member lacks the stamina to work the oars.\n\r", ch);
				return;
			}

			if( ship->first_mate == crew )
			{
				send_to_char("That crew member is already assigned as the {WFirst Mate{x.\n\r", ch);
				return;
			}

			if( ship->navigator == crew )
			{
				send_to_char("That crew member is already assigned as the {WNavigator{x.\n\r", ch);
				return;
			}

			if( ship->scout == crew )
			{
				send_to_char("That crew member is already assigned as the {WScout{x.\n\r", ch);
				return;
			}

			if( list_hasdata(ship->oarsmen, crew) )
			{
				send_to_char("That crew member is already assigned as an {WOarsman{x.\n\r", ch);
				return;
			}

			list_appendlink(ship->oarsmen, crew);
			send_to_char("Oarsman assigned.\n\r", ch);
			return;
		}

		if( !str_prefix(argument, "scout") )
		{
			if( crew->crew->scouting < 1 )
			{
				send_to_char("That crew member lacks any '{WScouting{x' ability.\n\r", ch);
				return;
			}

			if( ship->navigator == crew )
			{
				send_to_char("That crew member is already assigned as the {WNavigator{x.\n\r", ch);
				return;
			}

			if( list_hasdata(ship->oarsmen, crew) )
			{
				send_to_char("That crew member is already assigned as an {WOarsman{x.\n\r", ch);
				return;
			}

			ship->scout = crew;
			send_to_char("Scout assigned.\n\r", ch);
			// Place scout in the bird's nest, if available
			return;
		}


		do_ship_crew(ch, "assign");
		return;
	}

	if( !str_prefix(arg, "unassign") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Remove what role?\n\r", ch);
			send_to_char("{YValid roles: {Wfirstmate{x, {Wnavigator{x, {Wscout{x, {Woarsman <#>{x\n\r", ch);
			return;
		}

		argument = one_argument(argument, arg2);

		if( !str_prefix(arg2, "firstmate") )
		{
			if( !IS_VALID(ship->first_mate) )
			{
				send_to_char("No crew has been assigned as First Mate.\n\r", ch);
				return;
			}

			ship->first_mate = NULL;
			send_to_char("First Mate unassigned.\n\r", ch);
			return;
		}

		if( !str_prefix(arg2, "navigator") )
		{
			if( !IS_VALID(ship->navigator) )
			{
				send_to_char("No crew has been assigned as Navigator.\n\r", ch);
				return;
			}

			ship->navigator = NULL;
			send_to_char("Navigator unassigned.\n\r", ch);
			return;
		}

		if( !str_prefix(arg2, "scout") )
		{
			if( !IS_VALID(ship->scout) )
			{
				send_to_char("No crew has been assigned as Scout.\n\r", ch);
				return;
			}

			ship->scout = NULL;
			send_to_char("Scout unassigned.\n\r", ch);
			return;
		}

		if( !str_prefix(arg2, "oarsman") || !str_prefix(arg2, "oarsmen") )
		{
			if( !is_number(argument) )
			{
				send_to_char("That is number a number.\n\r", ch);
				return;
			}

			int index = atoi(argument);

			if( index < 1 || index > list_size(ship->crew) )
			{
				send_to_char("That is not a valid crew member.\n\r", ch);
				return;
			}

			CHAR_DATA *crew = (CHAR_DATA *)list_nthdata(ship->crew, index);

			if( !list_hasdata(ship->oarsmen, crew) )
			{
				send_to_char("That crew member is not an Oarsman.\n\r", ch);
				return;
			}

			list_remlink(ship->oarsmen, crew);
			send_to_char("Oarsman unassigned.\n\r", ch);
			return;
		}

		do_ship_crew(ch, "unassign");
		return;
	}

	do_ship_crew(ch, "");
}

void do_ship(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];

	if( IS_NPC(ch) ) return;

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' )
	{
		send_to_char("{xSyntax:  ship aim <ship>\n\r"
					 "         ship chase <ship>\n\r"
					 "         ship christen <name>\n\r"
					 "         ship crew[ <actions>]\n\r"
					 "         ship engines[ <level>] {W(airship only){x\n\r"
					 "         ship flag[ <flag>]\n\r"
					 "         ship keys[ <actions>]\n\r"
					 "         ship land {W(airship only){x\n\r"
					 "         ship launch {W(airship only){x\n\r"
					 "         ship list\n\r"
					 "         ship navigate[ <action>]\n\r"
					 "         ship oars[ <oars>]\n\r"
					 "         ship routes[ <action>]\n\r"
					 "         ship sails[ <level>] {W(sailboat only){x\n\r"
					 "         ship scuttle\n\r"
					 "         ship steer[ <heading>[ <turn direction>]]\n\r"
					 "         ship waypoints[ <action>]\n\r", ch);

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

	if( !str_prefix(arg, "crew") )
	{
		do_ship_crew(ch, argument);
		return;
	}

	if( !str_prefix(arg, "engines") )
	{
		// AIRSHIP only
		do_ship_engines(ch, argument);
		return;
	}

	if( !str_prefix(arg, "flag") )
	{
		do_ship_flag(ch, argument);
		return;
	}

	if( !str_prefix(arg, "keys") )
	{
		do_ship_keys(ch, argument);
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

	if( !str_prefix(arg, "oars") )
	{
		do_ship_oars(ch, argument);
		return;
	}

	if( !str_prefix(arg, "routes") )
	{
		do_ship_routes(ch, argument);
		return;
	}

	if( !str_prefix(arg, "sails") )
	{
		// SAILBOAT only
		do_ship_sails(ch, argument);
		return;
	}

	if( !str_prefix(arg, "scuttle") )
	{
		do_ship_scuttle(ch, argument);
		return;
	}

	if( !str_prefix(arg, "steer") )
	{
		do_ship_steer(ch, argument);
		return;
	}

	if( !str_prefix(arg, "waypoints") )
	{
		do_ship_waypoints(ch, argument);
		return;
	}

	do_ship(ch, "");
}

SHIP_DATA *find_ship_uid(unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	SHIP_DATA *ship;

	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( ship->id[0] == id1 && ship->id[1] == id2 )
			break;
	}
	iterator_stop(&it);

	return ship;
}

SHIP_DATA *get_owned_ship(CHAR_DATA *ch, char *argument)
{
	ITERATOR it;
	SHIP_DATA *ship = NULL;
	char arg[MIL];
	int number;

	if( is_number(argument) )
	{
		number = atoi(argument);
		arg[0] = '\0';
	}
	else
	{
		number = number_argument(argument, arg);
	}

	if( number < 1 ) return NULL;

	if( IS_NPC(ch) )
	{
		// Handle NPC ships
	}
	else
	{
		iterator_start(&it, loaded_ships);
		while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
		{
			if( is_name(arg, ship->ship_name_plain) )
			{
				if( !--number )
					break;
			}
		}
		iterator_stop(&it);
	}

	return ship;
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

SHIP_DATA *purchase_ship(CHAR_DATA *ch, WNUM wnum, SHOP_DATA *shop)
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

	SHIP_DATA *ship = create_ship(wnum);

	if( !IS_VALID(ship) )
	{
		return NULL;
	}

	ship->owner = ch;
	ship->owner_uid[0] = ch->id[0];
	ship->owner_uid[1] = ch->id[1];

	if( !IS_NPC(ch) )
	{
		list_appendlink(ch->pcdata->ships, ship);
	}

	if( ship->ship_type == SHIP_AIR_SHIP )
		ship->ship_power = SHIP_SPEED_LANDED;

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
	{ "oars",				shedit_oars			},
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
	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

	AREA_DATA *area = ch->in_room->area;
	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= area->top_ship_vnum; vnum++)
	{
		SHIP_INDEX_DATA *ship = get_ship_index(area, vnum);

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
			send_to_char("Ship indexes in current area.\n\r", ch);
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
	SHIP_INDEX_DATA *ship;
	AREA_DATA *area;

	EDIT_SHIP(ch, ship);
	area = ship->area;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

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
				SET_BIT(area->area_flags, AREA_CHANGED);
			}

			return;
		}
	}

	interpret(ch, arg);
}

void do_shedit(CHAR_DATA *ch, char *argument)
{
	SHIP_INDEX_DATA *ship = NULL;
	WNUM wnum;
	char arg[MAX_STRING_LENGTH];

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument,arg);

	if (parse_widevnum(arg, ch->in_room->area, &wnum))
	{
		if (!IS_BUILDER(ch, wnum.pArea))
		{
			send_to_char("ShEdit:  widevnum in an area you cannot build in.\n\r", ch);
			return;
		}

		if ( !(ship = get_ship_index(wnum.pArea, wnum.vnum)) )
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
			ch->desc->editor = ED_SHIP;

			EDIT_SHIP(ch, ship);
			SET_BIT(ship->area->area_flags, AREA_CHANGED);
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
	WNUM wnum;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  shshow <widevnum>\n\r", ch);
		return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		send_to_char("Please specify a widevnum.\n\r", ch);
		return;
	}

	if (!(ship = get_ship_index(wnum.pArea, wnum.vnum)))
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

	OBJ_INDEX_DATA *obj = get_obj_index(ship->area, ship->ship_object);
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

	sprintf(buf, "Oars:        [%5d]{x\n\r", ship->oars);
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
	WNUM wnum;
	int  iHash;
	AREA_DATA *pArea = ch->in_room->area;

	if (argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
	{
		long last_vnum = 0;
		long value = pArea->top_ship_vnum + 1;
		for(last_vnum = 1; last_vnum <= pArea->top_ship_vnum; last_vnum++)
		{
			if( !get_ship_index(pArea, last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}

		wnum.pArea = pArea;
		wnum.vnum = value;
	}

	if( get_ship_index(wnum.pArea, wnum.vnum) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return FALSE;
	}

	ship = new_ship_index();
	ship->vnum = wnum.vnum;
	ship->area = wnum.pArea;

	iHash							= ship->vnum % MAX_KEY_HASH;
	ship->next						= pArea->ship_index_hash[iHash];
	pArea->ship_index_hash[iHash]			= ship;
	ch->desc->pEdit					= (void *)ship;

	if( ship->vnum > pArea->top_ship_vnum)
		pArea->top_ship_vnum = ship->vnum;

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

BLUEPRINT_LAYOUT_SECTION_DATA *blueprint_get_nth_section(BLUEPRINT *bp, int section_no, BLUEPRINT_LAYOUT_SECTION_DATA **in_group);

SHEDIT( shedit_blueprint )
{
	SHIP_INDEX_DATA *ship;
	BLUEPRINT *bp;
	WNUM wnum;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  blueprint [widevnum]\n\r", ch);
		return FALSE;
	}

	if( !parse_widevnum(argument, ch->in_room->area, &wnum) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	if( !(bp = get_blueprint(wnum.pArea, wnum.vnum)) )
	{
		send_to_char("Blueprint does not exist.\n\r", ch);
		return FALSE;
	}

	// Verify the blueprint has certain features
	// * Has an entry room
	if( list_size(bp->entrances))
	{
		send_to_char("Blueprint requires an entry point for boarding purposes.\n\r", ch);
		return FALSE;
	}

	// Only allow static blueprints
	if (!is_blueprint_static(bp))
	{
		send_to_char("Only static blueprints maybe be used.\n\r", ch);
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
		ROOM_INDEX_DATA *room = blueprint_get_special_room(bp, special_room);

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
				ROOM_INDEX_DATA *room = get_room_index(bp->area, vnum);

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
	obj = get_obj_index(ship->area, vnum);
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


SHEDIT( shedit_oars )
{
	SHIP_INDEX_DATA *ship;

	EDIT_SHIP(ch, ship);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  oars [number]\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	int value = atoi(argument);
	if( value < 0 )
	{
		send_to_char("Number of Oar positions must be non-negative.\n\r", ch);
		return FALSE;
	}

	ship->oars = value;
	send_to_char("Oar positions changed.\n\r", ch);
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
		send_to_char("Syntax:  keys add <widevnum>\n\r", ch);
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
		WNUM wnum;

		if( !parse_widevnum(argument, ch->in_room->area, &wnum) )
		{
			send_to_char("Please specify a widevnum.\n\r", ch);
			return FALSE;
		}

		if( !(key = get_obj_index(wnum.pArea, wnum.vnum)) )
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
