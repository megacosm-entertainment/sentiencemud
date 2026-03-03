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

/* Forward declarations for ship module subcommands */
static void do_ship_modules(CHAR_DATA *ch, char *argument);
static void do_ship_install(CHAR_DATA *ch, char *argument);
static void do_ship_uninstall(CHAR_DATA *ch, char *argument);
static void do_ship_repair(CHAR_DATA *ch, char *argument);
static void do_ship_status(CHAR_DATA *ch, char *argument);

/* Forward declarations for ship combat system */
static int ship_get_max_weapon_range(SHIP_DATA *ship);
static int ship_distance(SHIP_DATA *a, SHIP_DATA *b);
static void boat_fire_weapon(SHIP_DATA *attacker, SHIP_DATA *target, SHIP_MODULE *weapon);
static void ship_apply_threshold_effects(SHIP_DATA *ship);
static void ship_destruction_sequence(SHIP_DATA *ship);
static void ship_combat_rep_attack(CHAR_DATA *ch, SHIP_DATA *attacker, SHIP_DATA *target);
static void ship_combat_rep_sink(SHIP_DATA *destroyed);
static bool npc_ship_is_valid_target(SHIP_DATA *hunter, SHIP_DATA *candidate);
static void ship_coast_guard_alert(CHAR_DATA *ch, SHIP_DATA *attacker, SHIP_DATA *target);
static void ship_schedule_tick(SHIP_DATA *ship);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);
void steering_set_heading(SHIP_DATA *ship, int heading);
void steering_set_turning(SHIP_DATA *ship, char direction);
void steering_calc_heading(SHIP_DATA *ship);
void ship_stop(SHIP_DATA *ship);
void do_ship_speed( CHAR_DATA *ch, char *argument );

extern LLIST *loaded_instances;

/**
 * Global Variables - Ship System State
 *
 * @var ships_changed       Dirty flag indicating ship data needs saving
 * @var top_ship_index_vnum Highest allocated ship template VNUM (for new ship creation)
 * @var loaded_ships        List of all active SHIP_DATA instances in the game
 * @var loaded_waypoints    List of all WAYPOINT_DATA navigation points
 * @var loaded_waypoint_paths List of all WAYPOINT_PATH_DATA routes between waypoints
 */
bool ships_changed = false;
long top_ship_index_vnum = 0;

LLIST *loaded_ships;
LLIST *loaded_waypoints;
LLIST *loaded_waypoint_paths;
NPC_SHIP_DATA *npc_ship_list = NULL;

static bool ship_is_npc_autonomous_candidate(SHIP_DATA *ship)
{
    if (!IS_VALID(ship) || !IS_VALID(ship->ship))
        return false;

    if (ship->npc_autonomous)
        return true;

    if (ship->npc_ship)
        return true;

    if (IS_VALID(ship->owner) && IS_NPC(ship->owner))
        return true;

    return false;
}

static void ship_npc_set_seek_goal(SHIP_DATA *ship, WILDS_DATA *wilds, int x, int y)
{
    if (!ship || !wilds)
        return;

    ship->seek_point.wilds = wilds;
    ship->seek_point.w = wilds->uid;
    ship->seek_point.x = URANGE(0, x, wilds->map_size_x - 1);
    ship->seek_point.y = URANGE(0, y, wilds->map_size_y - 1);
}

static bool ship_npc_begin_route(SHIP_DATA *ship, SHIP_ROUTE *route)
{
    ITERATOR it;
    WAYPOINT_DATA *wp;

    if (!ship || !IS_VALID(route) || list_size(route->waypoints) < 1)
        return false;

    ship_cancel_route(ship);

    iterator_start(&it, route->waypoints);
    while ((wp = (WAYPOINT_DATA *)iterator_nextdata(&it)) != NULL)
    {
        WAYPOINT_DATA *copy = clone_waypoint(wp);
        list_appendlink(ship->route_waypoints, copy);
    }
    iterator_stop(&it);

    iterator_start(&ship->route_it, ship->route_waypoints);
    wp = (WAYPOINT_DATA *)iterator_nextdata(&ship->route_it);
    if (!wp)
    {
        ship_cancel_route(ship);
        return false;
    }

    ship_npc_set_seek_goal(ship, get_wilds_from_uid(NULL, wp->w), wp->x, wp->y);
    ship->seek_navigator = false;
    ship->current_waypoint = wp;
    ship->current_route = route;

    return (ship->seek_point.wilds != NULL);
}

static bool ship_npc_try_offpath_goal(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    int radius;
    int gx;
    int gy;

    if (!ship || !room || !room->wilds)
        return false;

    if (ship->npc_offpath_active || ship->npc_goal_cooldown > 0)
        return false;

    if (ship->current_route == NULL || ship->seek_point.wilds == NULL)
        return false;

    ship->npc_resume_point = ship->seek_point;

    radius = number_range(8, 28);
    gx = room->x + number_range(-radius, radius);
    gy = room->y + number_range(-radius, radius);

    gx = URANGE(0, gx, room->wilds->map_size_x - 1);
    gy = URANGE(0, gy, room->wilds->map_size_y - 1);

    if (gx == room->x && gy == room->y)
        return false;

    ship_npc_set_seek_goal(ship, room->wilds, gx, gy);
    ship->npc_offpath_active = true;
    ship->npc_goal_cooldown = number_range(60, 180);

    ship_echo(ship, "{YThe crew diverts toward a nearby objective off the main route.{x");
    return true;
}

static bool ship_npc_try_trade_goal(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    AREA_DATA *area;
    AREA_DATA *target = NULL;
    int seen = 0;

    if (!ship || !room || !room->wilds)
        return false;

    if (ship->npc_offpath_active || ship->npc_goal_cooldown > 0)
        return false;

    if (ship->current_route == NULL || ship->seek_point.wilds == NULL)
        return false;

    for (area = area_first; area != NULL; area = area->next)
    {
        if (area->wilds_uid != room->wilds->uid)
            continue;

        if (area->trade_list == NULL)
            continue;

        if (area->x < 0 || area->x >= room->wilds->map_size_x)
            continue;
        if (area->y < 0 || area->y >= room->wilds->map_size_y)
            continue;

        if (area->x == room->x && area->y == room->y)
            continue;

        seen++;
        if (number_range(1, seen) == 1)
            target = area;
    }

    if (!target)
        return false;

    ship->npc_resume_point = ship->seek_point;
    ship_npc_set_seek_goal(ship, room->wilds, target->x, target->y);
    ship->npc_offpath_active = true;
    ship->npc_goal_cooldown = number_range(120, 320);

    ship_echo(ship, "{CThe crew adjusts course for a profitable trade stop.{x");
    return true;
}

static bool ship_npc_try_patrol_goal(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    int radius;
    int gx;
    int gy;

    if (!ship || !room || !room->wilds)
        return false;

    if (ship->npc_offpath_active || ship->npc_goal_cooldown > 0)
        return false;

    if (ship->current_route == NULL || ship->seek_point.wilds == NULL)
        return false;

    ship->npc_resume_point = ship->seek_point;

    if (ship->npc_ship && ship->npc_ship->pShipData
        && ship->npc_ship->pShipData->original_x >= 0
        && ship->npc_ship->pShipData->original_y >= 0)
    {
        gx = ship->npc_ship->pShipData->original_x + number_range(-10, 10);
        gy = ship->npc_ship->pShipData->original_y + number_range(-10, 10);
    }
    else
    {
        radius = number_range(6, 20);
        gx = room->x + number_range(-radius, radius);
        gy = room->y + number_range(-radius, radius);
    }

    gx = URANGE(0, gx, room->wilds->map_size_x - 1);
    gy = URANGE(0, gy, room->wilds->map_size_y - 1);

    if (gx == room->x && gy == room->y)
        return false;

    ship_npc_set_seek_goal(ship, room->wilds, gx, gy);
    ship->npc_offpath_active = true;
    ship->npc_goal_cooldown = number_range(90, 220);

    ship_echo(ship, "{WThe crew diverts for a local patrol sweep.{x");
    return true;
}

static SHIP_DATA *ship_npc_find_escort_leader(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    ITERATOR it;
    SHIP_DATA *other;
    SHIP_DATA *best = NULL;
    int best_dist = 0;

    if (!ship || !room || !room->wilds)
        return NULL;

    iterator_start(&it, loaded_ships);
    while ((other = (SHIP_DATA *)iterator_nextdata(&it)) != NULL)
    {
        ROOM_INDEX_DATA *other_room;
        int dx;
        int dy;
        int dist_sq;

        if (!IS_VALID(other) || other == ship || !IS_VALID(other->ship))
            continue;

        other_room = obj_room(other->ship);
        if (!IS_WILDERNESS(other_room) || other_room->wilds != room->wilds)
            continue;

        if (!ship_is_npc_autonomous_candidate(other))
            continue;

        if (other->seek_point.wilds == NULL && other->current_route == NULL)
            continue;

        if (ship->npc_ship && ship->npc_ship->pShipData && other->npc_ship && other->npc_ship->pShipData)
        {
            if (ship->npc_ship->pShipData->npc_sub_type != other->npc_ship->pShipData->npc_sub_type)
                continue;
        }

        dx = other_room->x - room->x;
        dy = other_room->y - room->y;
        dist_sq = dx * dx + dy * dy;

        if (dist_sq > (40 * 40))
            continue;

        if (!best || dist_sq < best_dist)
        {
            best = other;
            best_dist = dist_sq;
        }
    }
    iterator_stop(&it);

    return best;
}

static bool ship_npc_try_escort_goal(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    SHIP_DATA *leader;
    ROOM_INDEX_DATA *leader_room;
    int gx;
    int gy;

    if (!ship || !room || !room->wilds)
        return false;

    if (ship->npc_offpath_active || ship->npc_goal_cooldown > 0)
        return false;

    if (ship->current_route == NULL || ship->seek_point.wilds == NULL)
        return false;

    leader = ship_npc_find_escort_leader(ship, room);
    if (!leader)
        return false;

    leader_room = obj_room(leader->ship);
    if (!IS_WILDERNESS(leader_room) || leader_room->wilds != room->wilds)
        return false;

    ship->npc_resume_point = ship->seek_point;

    gx = leader_room->x + number_range(-4, 4);
    gy = leader_room->y + number_range(-4, 4);
    gx = URANGE(0, gx, room->wilds->map_size_x - 1);
    gy = URANGE(0, gy, room->wilds->map_size_y - 1);

    if (gx == room->x && gy == room->y)
        return false;

    ship_npc_set_seek_goal(ship, room->wilds, gx, gy);
    ship->npc_offpath_active = true;
    ship->npc_goal_cooldown = number_range(40, 110);

    ship_echo(ship, "{WThe crew forms up and escorts a nearby vessel.{x");
    return true;
}

static int ship_npc_goal_profile(SHIP_DATA *ship)
{
    if (ship && ship->npc_ship && ship->npc_ship->pShipData)
    {
        switch (ship->npc_ship->pShipData->npc_sub_type)
        {
        case NPC_SHIP_SUB_TYPE_COAST_GUARD_ATHEMIA:
        case NPC_SHIP_SUB_TYPE_COAST_GUARD_SERALIA:
            return 4; /* escort/patrol */

        case NPC_SHIP_SUB_TYPE_LIGHT_TRADER:
        case NPC_SHIP_SUB_TYPE_MEDIUM_TRADER:
            return 2; /* trader */

        case NPC_SHIP_SUB_TYPE_TREASURE_BOAT:
            return 3; /* raider/hunter */

        default:
            break;
        }
    }

    return 0; /* generic */
}

static void ship_npc_try_weighted_goals(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    int profile;
    int roll;

    if (!ship || !room)
        return;

    if (ship->npc_offpath_active || ship->npc_goal_cooldown > 0)
        return;

    if (ship->current_route == NULL || ship->seek_point.wilds == NULL)
        return;

    profile = ship_npc_goal_profile(ship);

    roll = number_range(1, 1000);

    if (profile == 2)
    {
        if (roll <= 60 && ship_npc_try_trade_goal(ship, room))
            return;

        if (roll <= 90)
            (void)ship_npc_try_offpath_goal(ship, room);

        return;
    }

    if (profile == 1)
    {
        if (roll <= 55 && ship_npc_try_patrol_goal(ship, room))
            return;

        if (roll <= 70)
            (void)ship_npc_try_offpath_goal(ship, room);

        return;
    }

    if (profile == 4)
    {
        if (roll <= 65 && ship_npc_try_escort_goal(ship, room))
            return;

        if (roll <= 85 && ship_npc_try_patrol_goal(ship, room))
            return;

        if (roll <= 92)
            (void)ship_npc_try_offpath_goal(ship, room);

        return;
    }

    if (profile == 3)
    {
        if (roll <= 42)
            (void)ship_npc_try_offpath_goal(ship, room);

        return;
    }

    if (roll <= 18)
    {
        if (ship_npc_try_trade_goal(ship, room))
            return;
    }

    if (roll <= 30)
    {
        (void)ship_npc_try_offpath_goal(ship, room);
    }
}

static void ship_npc_autopilot_tick(SHIP_DATA *ship)
{
    ROOM_INDEX_DATA *room;
    SHIP_ROUTE *route;
    int route_count;
    int heading;

    if (!ship_is_npc_autonomous_candidate(ship))
        return;

    /* Transport ships are driven by ship_schedule_tick, not autopilot */
    if (IS_SET(ship->ship_flags, SHIP_TRANSPORT))
        return;

    room = obj_room(ship->ship);
    if (!IS_WILDERNESS(room))
        return;

    if (ship->npc_goal_cooldown > 0)
        ship->npc_goal_cooldown--;

    if (ship->seek_point.wilds == NULL)
    {
        route_count = list_size(ship->routes);
        if (route_count > 0)
        {
            int pick = number_range(1, route_count);
            route = (SHIP_ROUTE *)list_nthdata(ship->routes, pick);
            ship_npc_begin_route(ship, route);
        }

        if (ship->seek_point.wilds == NULL)
        {
            int radius = number_range(12, 36);
            int gx = room->x + number_range(-radius, radius);
            int gy = room->y + number_range(-radius, radius);

            ship_npc_set_seek_goal(ship, room->wilds, gx, gy);
            ship->seek_navigator = false;
            ship->current_waypoint = NULL;
            ship->current_route = NULL;
            ship->npc_goal_cooldown = number_range(40, 120);
        }
    }

    ship_npc_try_weighted_goals(ship, room);

    if (ship->ship_power > SHIP_SPEED_STOPPED)
    {
        if (ship->seek_point.wilds == NULL && ship->steering.turning_dir == 0
            && number_percent() <= 4)
        {
            heading = number_range(0, 359);
            steering_set_heading(ship, heading);
            steering_set_turning(ship, (number_percent() < 50) ? -1 : 1);
        }

        return;
    }

    if (ship->ship_type == SHIP_AIR_SHIP && ship->ship_power == SHIP_SPEED_LANDED)
        ship->ship_power = SHIP_SPEED_HALF_SPEED;
    else if (ship->ship_power <= SHIP_SPEED_STOPPED)
        ship->ship_power = SHIP_SPEED_FULL_SPEED;

    if (ship->steering.heading < 0)
    {
        heading = number_range(0, 359);
        ship->steering.heading = heading;
        ship->steering.heading_target = heading;
        steering_calc_heading(ship);
    }

    ship_set_move_steps(ship);
    if (ship->ship_move <= 0)
        ship->ship_move = UMAX(1, ship->index ? ship->index->move_delay : 1);
}

static int ship_weather_speed_penalty_percent(ROOM_INDEX_DATA *room, SHIP_DATA *ship)
{
    int storm_type;

    if (!room || !ship)
        return 0;

    if (!IS_WILDERNESS(room))
        return 0;

    storm_type = get_storm_for_room(room);

    if (ship->ship_type == SHIP_AIR_SHIP)
    {
        switch (storm_type)
        {
        default:
        case WEATHER_NONE: return 0;
        case WEATHER_RAIN_STORM: return 6;
        case WEATHER_SNOW_STORM: return 10;
        case WEATHER_LIGHTNING_STORM: return 24;
        case WEATHER_HURRICANE: return 42;
        case WEATHER_TORNADO: return 58;
        }
    }

    switch (storm_type)
    {
    default:
    case WEATHER_NONE: return 0;
    case WEATHER_RAIN_STORM: return 10;
    case WEATHER_SNOW_STORM: return 15;
    case WEATHER_LIGHTNING_STORM: return 20;
    case WEATHER_HURRICANE: return 40;
    case WEATHER_TORNADO: return 55;
    }
}

static void ship_weather_apply_drift(SHIP_DATA *ship, ROOM_INDEX_DATA *room)
{
    int storm_type;
    int chance;
    int max_drift;
    int delta;
    int heading;

    if (!ship || !room)
        return;

    if (!IS_WILDERNESS(room) || ship->steering.heading < 0)
        return;

    storm_type = get_storm_for_room(room);

    if (ship->ship_type == SHIP_AIR_SHIP)
    {
        switch (storm_type)
        {
        default:
        case WEATHER_NONE:
            return;
        case WEATHER_RAIN_STORM:
            chance = 4;
            max_drift = 14;
            break;
        case WEATHER_SNOW_STORM:
            chance = 7;
            max_drift = 22;
            break;
        case WEATHER_LIGHTNING_STORM:
            chance = 14;
            max_drift = 45;
            break;
        case WEATHER_HURRICANE:
            chance = 30;
            max_drift = 95;
            break;
        case WEATHER_TORNADO:
            chance = 45;
            max_drift = 140;
            break;
        }
    }
    else
    {
    switch (storm_type)
    {
    default:
    case WEATHER_NONE:
        return;
    case WEATHER_RAIN_STORM:
        chance = 3;
        max_drift = 15;
        break;
    case WEATHER_SNOW_STORM:
        chance = 5;
        max_drift = 20;
        break;
    case WEATHER_LIGHTNING_STORM:
        chance = 7;
        max_drift = 30;
        break;
    case WEATHER_HURRICANE:
        chance = 24;
        max_drift = 75;
        break;
    case WEATHER_TORNADO:
        chance = 35;
        max_drift = 110;
        break;
    }
    }

    if (number_percent() > chance)
        return;

    delta = number_range(-max_drift, max_drift);
    if (delta == 0)
        delta = (number_percent() < 50) ? -1 : 1;

    heading = ship->steering.heading + delta;
    while (heading < 0)
        heading += 360;
    while (heading >= 360)
        heading -= 360;

    ship->steering.heading = heading;
    ship->steering.heading_target = heading;
    steering_calc_heading(ship);

    if (ship->ship_type == SHIP_AIR_SHIP)
        ship_echo(ship, "{YTurbulence buffets the airship off course.{x");
    else
        ship_echo(ship, "{YCrosswinds push the vessel off course.{x");
}

/**
 * Crew Skill Indices
 *
 * Used to index into crew member skill ratings for improvement checks.
 * Each NPC crew member has ratings 1-99 in these skills.
 */
#define CREW_SKILL_SCOUTING		0   /**< Ability to spot hazards/enemies */
#define CREW_SKILL_GUNNING		1   /**< Accuracy with ship weapons */
#define CREW_SKILL_OARRING		2   /**< Rowing/propulsion efficiency */
#define CREW_SKILL_MECHANICS	3   /**< Ship repair and maintenance */
#define CREW_SKILL_NAVIGATION	4   /**< Waypoint accuracy and route planning */
#define CREW_SKILL_LEADERSHIP	5   /**< Crew morale and coordination */

/**
 * bearing_door - Maps ship heading to discrete compass direction
 *
 * Converts a heading angle (0-359) to one of 8 cardinal/ordinal directions.
 * Index is calculated as: 2 * heading / 45
 * This gives 16 segments, with adjacent pairs mapping to the same direction.
 *
 * Example: heading 45 -> index 2 -> DIR_NORTHEAST
 */
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

/**
 * crew_skill_improve - Attempt to improve an NPC crew member's skill
 *
 * Roll-based skill improvement for ship crew NPCs. Higher existing
 * skill makes improvement less likely (diminishing returns).
 * Skill must be between 1-99 to have any chance of improvement.
 *
 * @param ch     NPC crew member (must have ch->crew set)
 * @param skill  CREW_SKILL_* constant indicating which skill to improve
 */
void crew_skill_improve(CHAR_DATA *ch, int skill)
{
    SHIP_CREW_DATA *crew = ch->crew;
    int16_t *ptr;

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

/**
 * set_seek_point - Set a navigation target coordinate with skill-based accuracy
 *
 * Initializes a WILDS_COORD with target location. If navigator skill is
 * less than 100, the actual target may be offset randomly based on
 * skill difference (simulating navigation inaccuracy).
 *
 * @param coord  Output coordinate structure to populate
 * @param wilds  Wilderness to navigate in (or NULL to lookup by w)
 * @param w      Wilderness UID
 * @param x      Target X coordinate
 * @param y      Target Y coordinate
 * @param skill  Navigator's navigation skill (0-100)
 */
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

/**
 * ship_seek_point - Process ship navigation toward current waypoint
 *
 * Called during ship movement to check if the ship has reached its
 * current navigation target. If within ~2.44 blocks of target:
 * - Awards skill improvement to navigator (crew or player)
 * - Advances to next waypoint in route
 * - Stops ship if no more waypoints remain
 *
 * Also adjusts ship heading to point toward the seek point.
 *
 * @param ship  Ship data to process navigation for
 * @return      true to continue movement, false if ship should stop
 */
bool ship_seek_point(SHIP_DATA *ship)
{
    ROOM_INDEX_DATA *room = obj_room(ship->ship);
    if( IS_WILDERNESS(room) && ship->seek_point.wilds == room->wilds )
    {
        int distSq = (room->x - ship->seek_point.x) * (room->x - ship->seek_point.x) + (room->y - ship->seek_point.y) * (room->y - ship->seek_point.y);

        // Within 2.44 block radius of location
        if( distSq <= 6 )
        {
            if( ship->npc_offpath_active )
            {
                ship->npc_offpath_active = false;

                if (ship->npc_resume_point.wilds)
                {
                    ship->seek_point = ship->npc_resume_point;
                    memset(&ship->npc_resume_point, 0, sizeof(ship->npc_resume_point));
                    ship->npc_goal_cooldown = UMAX(ship->npc_goal_cooldown, number_range(80, 200));
                    ship_echo(ship, "{WThe crew completes the detour and resumes the plotted route.{x");
                    return true;
                }
            }

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
                check_improve(ship->owner, skill_resolve_gsn("navigation"), true, 10);
                skill = get_skill(ship->owner, skill_resolve_gsn("navigation"));
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

/**
 * steering_calc_heading - Calculate movement vectors from current heading
 *
 * Converts the ship's current heading angle (0-359 degrees) into
 * movement delta values for Bresenham-style line movement:
 * - dx, dy: Scaled direction components (±1000 range)
 * - ax, ay: Absolute values of dx, dy
 * - sx, sy: Sign values (-1, 0, or 1)
 * - compass: Nearest cardinal/ordinal direction (DIR_* constant)
 *
 * Also resets the movement accumulator.
 *
 * @param ship  Ship to calculate heading vectors for
 */
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

/**
 * steering_calc_forceheading - Force ship heading from raw direction vector
 *
 * Sets ship's movement vectors directly from dx/dy values and
 * back-calculates the heading angle. Used when external forces
 * (currents, wind) override normal steering.
 *
 * @param ship  Ship to update
 * @param dx    X component of forced direction
 * @param dy    Y component of forced direction
 */
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

/**
 * steering_set_heading - Set ship's target heading
 *
 * Sets the desired heading angle. If this is the first heading set
 * (heading was negative), also initializes current heading.
 * Ship will gradually turn toward target heading.
 *
 * @param ship     Ship to update
 * @param heading  Target heading in degrees (0-359, 0=North)
 */
void steering_set_heading(SHIP_DATA *ship, int heading)
{
    // Initialize it
    if( ship->steering.heading < 0 )
    {
        ship->steering.heading = heading;
    }

    ship->steering.heading_target = heading;
}

/**
 * steering_set_turning - Set ship's turning direction
 *
 * Controls which direction the ship turns to reach target heading.
 *
 * @param ship       Ship to update
 * @param direction  Turn direction: -1 = port (left), 1 = starboard (right)
 */
void steering_set_turning(SHIP_DATA *ship, char direction)
{
    ship->steering.turning_dir = direction;
}

/**
 * steering_update - Process gradual ship turning toward target heading
 *
 * Called each tick to incrementally adjust ship's heading toward its
 * target. Turning rate depends on ship's turning stat and current
 * power state (slower while stopped). When target heading is reached,
 * notifies passengers and stops turning.
 *
 * If ship has a seek point set, recalculates force heading to point
 * directly at destination after reaching target heading.
 *
 * @param ship  Ship to update steering for
 */
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

/**
 * steering_movement - Calculate next movement step based on heading
 *
 * Uses Bresenham-style line algorithm to determine the next grid
 * coordinate the ship should move to based on its current heading.
 * Handles diagonal movement by accumulating fractional movement.
 *
 * Validates destination is traversable (water for ships, anywhere
 * for airships).
 *
 * @param ship  Ship calculating movement for
 * @param x     Output: next X coordinate
 * @param y     Output: next Y coordinate
 * @param door  Output: direction constant for exit messages
 * @return      true if movement is valid, false if blocked/stopped
 */
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

/**
 * load_ship_index - Load a ship template definition from file
 *
 * Reads a ship index (template) from the ships data file. Ship indices
 * define the base stats for ship types: hull, armor, speed, cargo,
 * weapons, blueprints, etc. Individual ships reference these templates.
 *
 * File format: #SHIP vnum followed by key-value pairs, ending with #-SHIP
 *
 * @param fp  Open file positioned at #SHIP line
 * @return    New SHIP_INDEX_DATA populated from file
 */
SHIP_INDEX_DATA *load_ship_index(FILE *fp)
{
    SHIP_INDEX_DATA *ship;
    char *word;
    bool fMatch;

    ship = new_ship_index();
    ship->vnum = fread_number(fp);

    if( ship->vnum > top_ship_index_vnum)
        top_ship_index_vnum = ship->vnum;

    /* Assign ship to area based on vnum range */
    WNUM wnum;
    if (resolve_widevnum(ship->vnum, NULL, &wnum))
        ship->area = wnum.pArea;
    else
        ship->area = get_system_area_fallback();

    while (str_cmp((word = fread_word(fp)), "#-SHIP"))
    {
        fMatch = false;

        switch(word[0])
        {
        case 'A':
            KEY("Armor", ship->armor, fread_number(fp));
            break;

        case 'B':
            if( !str_cmp(word, "Blueprint") )
            {
                ship->blueprint_ref.vnum = fread_number(fp);
                ship->blueprint = NULL;  // Resolved in fix pass
                fMatch = true;
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

                fMatch = true;
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
            if( !str_cmp(word, "Hardpoint") )
            {
                SHIP_HARDPOINT_DEF *hp = new_ship_hardpoint_def();
                hp->slot_id = fread_number(fp);
                free_string(hp->name);
                hp->name = fread_string(fp);
                hp->type = fread_number(fp);
                hp->size = fread_number(fp);
                hp->domain_flags = fread_number(fp);
                hp->flags = fread_number(fp);
                list_appendlink(ship->hardpoints, hp);
                fMatch = true;
                break;
            }
            break;

        case 'K':
            if( !str_cmp(word, "Key") )
            {
                long *key_vnum = alloc_perm(sizeof(long));
                *key_vnum = fread_number(fp);
                list_appendlink(ship->special_keys, key_vnum);
                fMatch = true;
                break;
            }
            break;

        case 'M':
            KEY("MoveDelay", ship->move_delay, fread_number(fp));
            KEY("MoveSteps", ship->move_steps, fread_number(fp));
            KEY("ModuleWeight", ship->max_module_weight, fread_number(fp));
            break;

        case 'N':
            KEYS("Name", ship->name, fread_string(fp));
            break;

        case 'O':
            KEY("Oars", ship->oars, fread_number(fp));
            if (!str_cmp(word, "Object"))
            {
                ship->ship_object_ref.vnum = fread_number(fp);
                ship->ship_object = NULL;  // Resolved in fix pass
                fMatch = true;
                break;
            }
            break;

        case 'T':
            KEY("Turning", ship->turning, fread_number(fp));
            break;

        case 'W':
            KEY("Weight", ship->weight, fread_number(fp));
            break;
        }

        if (!fMatch) {
            pbugf(LOG_ERROR, "load_ship: no match for word %.50s", word);
        }

    }

    return ship;
}

/**
 * load_ships - Load all ship templates from ships.dat at boot time
 *
 * Reads SHIPS_FILE and populates ship_index_hash with all ship templates.
 * Updates top_ship_index_vnum for new ship template creation.
 */
void load_ships()
{
    char ships_path_buf[MAX_INPUT_LENGTH];
    const char *ships_path = resolve_game_path(SHIPS_FILE, ships_path_buf, sizeof(ships_path_buf));
    FILE *fp = fopen(ships_path, "r");
    if (fp == NULL)
    {
        pbugf(LOG_ERROR, "Couldn't read ships.dat (%s)", ships_path);
        return;
    }

    char *word;
    bool fMatch;

    top_ship_index_vnum = 0;

    while (str_cmp((word = fread_word(fp)), "#END"))
    {
        fMatch = false;

        if( !str_cmp(word, "#SHIP") )
        {
            SHIP_INDEX_DATA *ship = load_ship_index(fp);
            AREA_DATA *area = ship->area;
            
            if (area)
            {
                int iHash = ship->vnum % MAX_KEY_HASH;
                ship->next = area->ship_index_hash[iHash];
                area->ship_index_hash[iHash] = ship;
            }
            else
            {
                pbugf(LOG_ERROR, "load_ships: ship %ld has no area assigned", ship->vnum);
            }

            fMatch = true;
            continue;
        }

        if (!fMatch) {
            pbugf(LOG_ERROR, "load_ships: no match for word %.50s", word);
        }
    }

    fclose(fp);
}

/**
 * save_ship_index - Write a single ship template to file
 *
 * Serializes all properties of a SHIP_INDEX_DATA including name,
 * description, stats, blueprint reference, and special keys.
 *
 * @param fp    Open file to write to
 * @param ship  Ship template to save
 */
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

    if( ship->ship_object )
        fprintf(fp, "Object %ld\n", ship->ship_object->vnum);

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

    if( ship->max_module_weight > 0 )
        fprintf(fp, "ModuleWeight %d\n", ship->max_module_weight);

    iterator_start(&it, ship->special_keys);
    while( (obj = (OBJ_INDEX_DATA *)iterator_nextdata(&it)) )
    {
        if( obj->item_type == ITEM_KEY )
            fprintf(fp, "Key %ld\n", obj->vnum);
    }
    iterator_stop(&it);

    /* Save hardpoint definitions */
    {
        SHIP_HARDPOINT_DEF *hp;
        ITERATOR hit;
        iterator_start(&hit, ship->hardpoints);
        while( (hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&hit)) )
        {
            fprintf(fp, "Hardpoint %d %s~ %d %d %ld %ld\n",
                hp->slot_id, fix_string(hp->name),
                hp->type, hp->size, hp->domain_flags, hp->flags);
        }
        iterator_stop(&hit);
    }

    fprintf(fp, "#-SHIP\n\n");
}

/**
 * save_ships - Save all ship templates to disk
 *
 * Writes all ship templates from ship_index_hash to SHIPS_FILE.
 * Clears ships_changed flag on success.
 *
 * @return  true on success, false if file couldn't be opened
 */
bool save_ships()
{
    char ships_path_buf[MAX_INPUT_LENGTH];
    const char *ships_path = resolve_game_path(SHIPS_FILE, ships_path_buf, sizeof(ships_path_buf));
    FILE *fp = fopen(ships_path, "w");
    if (fp == NULL)
    {
        pbugf(LOG_ERROR, "Couldn't save ships.dat (%s)", ships_path);
        return false;
    }

    int iHash;
    AREA_DATA *area;

    for (area = area_first; area != NULL; area = area->next)
    {
        for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
        {
            for(SHIP_INDEX_DATA *ship = area->ship_index_hash[iHash]; ship; ship = ship->next)
            {
                save_ship_index(fp, ship);
            }
        }
    }

    fprintf(fp, "#END\n");
    fclose(fp);

    ships_changed = false;
    return true;
}

/**
 * get_ship_index - Look up a ship template by VNUM (searches all areas)
 *
 * Searches all area ship_index_hash tables for a template with the given VNUM.
 * Use this for global lookups (e.g., vnum command).
 *
 * @param vnum  VNUM to search for
 * @return      SHIP_INDEX_DATA if found, NULL otherwise
 */
SHIP_INDEX_DATA *get_ship_index(long vnum)
{
    AREA_DATA *area;
    int iHash;

    for (area = area_first; area != NULL; area = area->next)
    {
        iHash = vnum % MAX_KEY_HASH;
        for(SHIP_INDEX_DATA *ship = area->ship_index_hash[iHash]; ship; ship = ship->next)
        {
            if( ship->vnum == vnum )
                return ship;
        }
    }

    return NULL;
}

/**
 * get_ship_index_for_area - Look up a ship template within a specific area
 *
 * Searches only the specified area's ship_index_hash for the given VNUM.
 * Use this for list/edit commands that should be area-scoped.
 *
 * @param area  Area to search in
 * @param vnum  VNUM to search for
 * @return      SHIP_INDEX_DATA if found in area, NULL otherwise
 */
SHIP_INDEX_DATA *get_ship_index_for_area(AREA_DATA *area, long vnum)
{
    int iHash;

    if (!area)
        return NULL;

    iHash = vnum % MAX_KEY_HASH;
    for(SHIP_INDEX_DATA *ship = area->ship_index_hash[iHash]; ship; ship = ship->next)
    {
        if( ship->vnum == vnum )
            return ship;
    }

    return NULL;
}

/**
 * get_ship_module_index - Look up a module template by VNUM (searches all areas)
 *
 * @param vnum  VNUM to search for
 * @return      SHIP_MODULE_INDEX if found, NULL otherwise
 */
SHIP_MODULE_INDEX *get_ship_module_index(long vnum)
{
    AREA_DATA *area;
    int iHash = vnum % MAX_KEY_HASH;

    for (area = area_first; area != NULL; area = area->next)
    {
        for (SHIP_MODULE_INDEX *mod = area->ship_module_index_hash[iHash]; mod; mod = mod->next)
        {
            if (mod->vnum == vnum)
                return mod;
        }
    }

    return NULL;
}

/**
 * get_ship_module_index_for_area - Look up a module template within a specific area
 *
 * @param area  Area to search in
 * @param vnum  VNUM to search for
 * @return      SHIP_MODULE_INDEX if found in area, NULL otherwise
 */
SHIP_MODULE_INDEX *get_ship_module_index_for_area(AREA_DATA *area, long vnum)
{
    if (!area) return NULL;

    int iHash = vnum % MAX_KEY_HASH;
    for (SHIP_MODULE_INDEX *mod = area->ship_module_index_hash[iHash]; mod; mod = mod->next)
    {
        if (mod->vnum == vnum)
            return mod;
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////
//
// Ships (Runtime Instances)
//

/**
 * create_ship - Create a new runtime ship instance from template
 *
 * Instantiates a ship from a SHIP_INDEX_DATA template:
 * 1. Validates ship and object templates exist
 * 2. Creates physical ship object (ITEM_SHIP)
 * 3. Creates interior instance from blueprint
 * 4. Initializes hitpoints, steering, cargo, etc.
 * 5. Adds to loaded_ships list
 *
 * @param vnum  VNUM of ship template to instantiate
 * @return      New SHIP_DATA, or NULL on failure
 */
SHIP_DATA *create_ship(WNUM wnum)
{
    OBJ_DATA *obj;						// Physical ship object
    OBJ_INDEX_DATA *obj_index;			// Ship object index to create
    SHIP_DATA *ship;					// Runtime ship data
    SHIP_INDEX_DATA *ship_index = NULL;		// Ship index to create
    INSTANCE *instance;
    ITERATOR it;

    // Verify the ship index exists (try area-scoped first, then global)
    if( wnum.pArea )
    {
        ship_index = get_ship_index_for_area(wnum.pArea, wnum.vnum);
    }
    
    if( !ship_index )
    {
        ship_index = get_ship_index(wnum.vnum);
    }
    
    if( !ship_index )
        return NULL;

    // Resolve ship_object if not already resolved
    obj_index = ship_index->ship_object;
    if( !obj_index && ship_index->ship_object_ref.load.vnum > 0 )
    {
        // Try to resolve using the area UID first
        AREA_DATA *target_area = get_area_from_uid(ship_index->ship_object_ref.load.auid);
        
        if( target_area )
        {
            obj_index = get_obj_index(target_area, ship_index->ship_object_ref.load.vnum);
        }
        
        // If not found, search all areas (legacy support)
        if( !obj_index )
        {
            AREA_DATA *search_area;
            for (search_area = area_first; search_area != NULL; search_area = search_area->next)
            {
                obj_index = get_obj_index(search_area, ship_index->ship_object_ref.load.vnum);
                if (obj_index)
                    break;
            }
        }
        
        // Cache the resolved pointer
        if( obj_index )
        {
            ship_index->ship_object = obj_index;
        }
    }

    // Verify the object index exists and is a ship
    if( !obj_index )
    {
        pbugf(LOG_ERROR, "create_ship: ship %s has no valid ship_object (ref %lu#%ld)", 
            widevnum_string_wnum(wnum, NULL), 
            ship_index->ship_object_ref.load.auid,
            ship_index->ship_object_ref.load.vnum);
        return NULL;
    }

    if( obj_index->item_type != ITEM_SHIP )
    {
        pbugf(LOG_ERROR, "create_ship: attempting to use object (%ld) that is not a ship object for ship (%s)", obj_index->vnum, widevnum_string_wnum(wnum, NULL));
        return NULL;
    }

    ship = new_ship();
    if( !IS_VALID(ship) )
        return NULL;

    ship->index = ship_index;

    obj = create_object(obj_index, 0, false);
    if( !IS_VALID(obj) )
    {
        free_ship(ship);
        return NULL;
    }
    ship->ship = obj;
    obj->ship = ship;

    // Resolve blueprint if not already resolved
    BLUEPRINT *blueprint = ship_index->blueprint;
    if( !blueprint && ship_index->blueprint_ref.load.vnum > 0 )
    {
        // Try to resolve using the area UID first
        AREA_DATA *target_area = get_area_from_uid(ship_index->blueprint_ref.load.auid);
        
        if( target_area )
        {
            blueprint = get_blueprint_for_area(target_area, ship_index->blueprint_ref.load.vnum);
        }
        
        // If not found, search globally (legacy support)
        if( !blueprint )
        {
            blueprint = get_blueprint(ship_index->blueprint_ref.load.vnum);
        }
        
        // Cache the resolved pointer
        if( blueprint )
        {
            ship_index->blueprint = blueprint;
        }
    }

    if( !blueprint )
    {
        pbugf(LOG_ERROR, "create_ship: ship %s has no valid blueprint (ref %lu#%ld)", 
            widevnum_string_wnum(wnum, NULL), 
            ship_index->blueprint_ref.load.auid,
            ship_index->blueprint_ref.load.vnum);
        list_remlink(loaded_objects, obj, true);
        loaded_obj_hash_remove(obj);
        --obj->pIndexData->count;
        free_obj(obj);
        free_ship(ship);
        return NULL;
    }

    instance = create_instance(blueprint);
    if( !IS_VALID(instance) )
    {
        list_remlink(loaded_objects, obj, true);
        loaded_obj_hash_remove(obj);
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

    /* Recalculate module-derived stats (in case modules were loaded from save) */
    ship_recalc_modules(ship);

    list_appendlink(loaded_ships, ship);

    get_ship_id(ship);
    return ship;
}

/**
 * extract_ship - Remove and destroy a ship instance from the game
 *
 * Cleans up a runtime ship:
 * - Removes from owner's ship list
 * - Detaches from any docked ships
 * - Removes from loaded_ships list
 * - Extracts interior instance and physical object
 * - Frees memory
 *
 * @param ship  Ship to destroy
 */
void extract_ship(SHIP_DATA *ship)
{
    if( !IS_VALID(ship) ) return;

    if( IS_VALID(ship->owner) && !IS_NPC(ship->owner) )
    {
        list_remlink(ship->owner->pcdata->ships, ship, false);
    }

    detach_ships_ship(ship);

    list_remlink(loaded_ships, ship, false);

    extract_instance(ship->instance);
    extract_obj(ship->ship);

    free_ship(ship);
}

/**
 * ship_isowner_player - Check if a player owns a ship
 *
 * @param ship  Ship to check ownership of
 * @param ch    Character to check as owner
 * @return      true if ch owns ship, false otherwise
 */
bool ship_isowner_player(SHIP_DATA *ship, CHAR_DATA *ch)
{
    if( !IS_VALID(ship) ) return false;

    if( !IS_VALID(ch) ) return false;

    if( ship->owner == ch ) return true;	// If it's already assigned, short circuit

    //return ( (ship->owner_uid[0] == ch->id[0]) && (ship->owner_uid[1] == ch->id[1]) );
    return false;
}

/**
 * get_ship_waypoint - Find a waypoint by number or name
 *
 * Searches the ship's waypoint list by index (1-based) or name match.
 * Supports N.name syntax for matching Nth instance of a name.
 *
 * @param ship      Ship whose waypoints to search
 * @param argument  Waypoint number or name (possibly with N. prefix)
 * @param wilds     If non-NULL, only match waypoints in this wilderness
 * @return          Matching WAYPOINT_DATA or NULL
 */
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


/**
 * get_ship_route - Find a route by number or name
 *
 * Searches the ship's route list by index (1-based) or name match.
 * Supports N.name syntax for matching Nth instance of a name.
 *
 * @param ship      Ship whose routes to search
 * @param argument  Route number or name (possibly with N. prefix)
 * @return          Matching SHIP_ROUTE or NULL
 */
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

/**
 * ship_cancel_route - Cancel current route navigation
 *
 * Stops following the current route, clears waypoint iterator,
 * and resets seek point. Does not stop the ship's movement.
 *
 * @param ship  Ship to cancel route for
 */
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

/**
 * ship_stop - Bring ship to a complete halt
 *
 * Stops all ship movement: sail power, oar power, steering,
 * navigation route. Resets sextant and movement accumulators.
 * Also sets staggered wake fade times for visual trail effect.
 *
 * @param ship  Ship to stop
 */
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

/**
 * move_ship_success - Execute one step of ship movement
 *
 * Core ship movement function. Calculates next position based on
 * steering, validates terrain (water for ships, any for airships),
 * moves physical object, updates wake trail, and sends room messages.
 *
 * Stops ship if it runs aground on invalid terrain.
 *
 * @param ship  Ship to move
 * @return      true if movement succeeded, false if blocked
 */
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

    ship_weather_apply_drift(ship, in_room);

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

        if( (!room_in_sector(pTerrain->template, SECT_WATER_NOSWIM) &&
            !room_in_sector(pTerrain->template, SECT_WATER_SWIM)) || vlink )
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
    
    /* Sync ship instance entrance to new location */
    if (IS_VALID(ship->instance) && ship->instance->entrance && to_room->wilds) {
        ship->instance->entrance->wilds = to_room->wilds;
        ship->instance->entrance->x = to_room->x;
        ship->instance->entrance->y = to_room->y;
    }

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

/**
 * ship_set_move_steps - Calculate ship's movement steps per tick
 *
 * Determines how many grid squares the ship moves per movement tick
 * based on sail power, oar power (scaled by active rowers), and
 * the ship template's base move_steps. Clears sextant reading.
 *
 * @param ship  Ship to calculate movement for
 */
void ship_set_move_steps(SHIP_DATA *ship)
{
    if( ship->ship_power > SHIP_SPEED_STOPPED || ship->oar_power > SHIP_SPEED_STOPPED )
    {
        ROOM_INDEX_DATA *room;
        int penalty_percent;
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

        room = obj_room(ship->ship);
        penalty_percent = ship_weather_speed_penalty_percent(room, ship);
        if (penalty_percent > 0)
        {
            speed = UMAX(1, (speed * (100 - penalty_percent)) / 100);

            if (number_percent() <= UMIN(35, penalty_percent))
                ship_echo(ship, "{WHeavy weather slows the vessel.{x");
        }

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

/**
 * ship_move_update - Process ship movement for one movement tick
 *
 * Called when ship_move countdown reaches zero. If ship is moving:
 * - Updates steering (turning toward target heading)
 * - Executes move_steps worth of movement
 * - Performs auto-survey for navigation
 * If stopped but turning, processes stationary rotation.
 * Recalculates move_steps for next tick.
 *
 * @param ship  Ship to update movement for
 */
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

/**
 * ship_pulse_update - Per-pulse update for a single ship
 *
 * Called every game pulse. Handles:
 * - Movement countdown and triggering ship_move_update
 * - Wake trail expiration (fading previous positions)
 * - Exhausted oarsman recovery (resume rowing when stamina > 25%)
 *
 * @param ship  Ship to update
 */
void ship_pulse_update(SHIP_DATA *ship)
{
    if( !IS_VALID(ship) ) return;

    ship_npc_autopilot_tick(ship);

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

    /* Combat update — fire/reload cycle, chase, damage ticks */
    ship_combat_update(ship);
}

/**
 * ships_pulse_update - Global pulse update for all ships
 *
 * Called every game pulse. Iterates through loaded_ships and
 * calls ship_pulse_update on each active ship.
 */
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

/**
 * ship_tick_update - Per-tick update for a single ship
 *
 * Called every game tick. Handles longer-term ship events:
 * - Scuttling countdown (ship self-destructs when timer reaches 0)
 *
 * @param ship  Ship to update
 */
void ship_tick_update(SHIP_DATA *ship)
{
    // Update scuttling
    if( ship->scuttle_time > 0 && !IS_SET(ship->ship_flags, SHIP_SINKING) )
    {
        if(!--ship->scuttle_time)
        {
            boat_echo(ship, "{R** The vessel has been scuttled! **{x");
            ship_destruction_sequence(ship);
            return;
        }
    }

    /* NPC AI state machine update */
    if (ship->npc_ship && IS_SET(ship->ship_flags, SHIP_AUTONOMOUS_NPC))
    {
        npc_ship_state_update(ship);
    }
}

/**
 * ships_ticks_update - Global tick update for all ships
 *
 * Called every game tick. Iterates through loaded_ships and
 * calls ship_tick_update on each active ship.
 */
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

/**
 * ship_special_key_load - Load a special key tracking structure from file
 *
 * Reads key VNUM and list of key instance UIDs. Special keys track
 * which specific key objects have been used on ship locks.
 *
 * @param fp  Open file positioned at key vnum
 * @return    New SPECIAL_KEY_DATA with loaded data
 */
SPECIAL_KEY_DATA *ship_special_key_load(FILE *fp)
{
    SPECIAL_KEY_DATA *sk;
    char *word;
    bool fMatch;

    sk = new_special_key();
    {
        long auid = fread_number(fp);
        long vnum = fread_number(fp);
        sk->key_wnum.pArea = auid > 0 ? get_area_from_uid(auid) : NULL;
        sk->key_wnum.vnum = vnum;
    }

    while (str_cmp((word = fread_word(fp)), "#-SPECIALKEY"))
    {
        fMatch = false;

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
            pbugf(LOG_ERROR, "ship_special_key_load: no match for word %.50s", word);
        }
    }

    return sk;

}

/**
 * ship_route_load - Load a navigation route from file
 *
 * Reads route name and list of waypoint indices, resolving each
 * index to actual waypoints in the ship's waypoint list.
 *
 * @param fp    Open file positioned at route name
 * @param ship  Ship owning the waypoints (must have waypoints loaded first)
 * @return      New SHIP_ROUTE with loaded waypoints
 */
SHIP_ROUTE *ship_route_load(FILE *fp, SHIP_DATA *ship)
{
    SHIP_ROUTE *route;
    char *word;
    bool fMatch;

    route = new_ship_route();
    route->name = fread_string(fp);

    while (str_cmp((word = fread_word(fp)), "#-ROUTE"))
    {
        fMatch = false;

        switch(word[0])
        {
        case 'W':
            if( !str_cmp(word, "Waypoint") )
            {
                int index = fread_number(fp);

                WAYPOINT_DATA *wp = list_nthdata(ship->waypoints, index);

                list_appendlink(route->waypoints, wp);

                fMatch = true;
                break;
            }
            break;
        }

        if (!fMatch) {
            pbugf(LOG_ERROR, "ship_route_load: no match for word %.50s", word);
        }
    }

    return route;
}

/**
 * ship_load_find_crew - Find a crew member by UID during ship loading
 *
 * Searches the ship's crew list for a mobile with matching UID.
 * Used to resolve role references (navigator, oarsman, etc.) during load.
 *
 * @param ship  Ship whose crew to search
 * @param id1   First part of UID
 * @param id2   Second part of UID
 * @return      Matching CHAR_DATA or NULL
 */
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

/**
 * ship_load - Load a complete ship instance from file
 *
 * Deserializes a saved ship including:
 * - Ship index reference (template)
 * - Interior instance (rooms, exits)
 * - Physical ship object
 * - Crew members and their roles (navigator, oarsmen, etc.)
 * - Waypoints and routes
 * - Special keys
 * - Combat state, damage, flags
 *
 * @param fp  Open file positioned at ship index vnum
 * @return    Loaded SHIP_DATA, or NULL if index not found
 */
SHIP_DATA *ship_load(FILE *fp)
{
    SHIP_DATA *ship;
    SHIP_INDEX_DATA *index = NULL;
    char *word;
    bool fMatch;
    long vnum = fread_number(fp);
    AREA_DATA *area = NULL;

    ship = new_ship();

    while (str_cmp((word = fread_word(fp)), "#-SHIP"))
    {
        fMatch = false;

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

                fMatch = true;
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

                fMatch = true;
                break;
            }
            if( !str_cmp(word, "#OBJECT") )
            {
                OBJ_DATA *obj = persist_load_object(fp);


                if( IS_VALID(obj) ) {
                    ship->ship = obj;
                    obj->ship = ship;
                    if (obj->in_room) {
                        obj_to_room(obj, obj->in_room);
                    } else {
                        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "ship_load: loaded ship object vnum %ld, name '%s' with NULL in_room (not calling obj_to_room)", obj->pIndexData ? obj->pIndexData->vnum : -1L, obj->name ? obj->name : "(null)");
                    }
                }

                fMatch = true;
                break;
            }
            if( !str_cmp(word, "#ROUTE") )
            {
                SHIP_ROUTE *route = ship_route_load(fp, ship);

                list_appendlink(ship->routes, route);
                fMatch = true;
                break;
            }
            if( !str_cmp(word, "#SPECIALKEY") )
            {
                SPECIAL_KEY_DATA *sk = ship_special_key_load(fp);

                list_appendlink(ship->special_keys, sk);
                fMatch = true;
                break;
            }
            break;

        case 'A':
            if( !str_cmp(word, "AreaUid") )
            {
                long area_uid = fread_number(fp);
                area = get_area_from_uid(area_uid);
                fMatch = true;
                break;
            }
            KEY("Armor", ship->armor, fread_number(fp));
            KEY("AttackPos", ship->attack_position, fread_number(fp));
            break;

        case 'B':
            if( !str_cmp(word, "BoardedBy") )
            {
                ship->boarded_by_uid[0] = fread_number(fp);
                ship->boarded_by_uid[1] = fread_number(fp);
                fMatch = true;
                break;
            }
            break;

        case 'C':
            KEY("Cannons", ship->cannons, fread_number(fp));
            if( !str_cmp(word, "CharAttacked") )
            {
                ship->char_attacked_uid[0] = fread_number(fp);
                ship->char_attacked_uid[1] = fread_number(fp);
                fMatch = true;
                break;
            }
            if( !str_cmp(word, "Crew") )
            {
                ship->min_crew = fread_number(fp);
                ship->max_crew = fread_number(fp);
                fMatch = true;
                break;
            }
            break;

        case 'F':
            if( !str_cmp(word, "FirstMate") )
            {
                unsigned long id1 = fread_number(fp);
                unsigned long id2 = fread_number(fp);

                ship->first_mate = ship_load_find_crew(ship, id1, id2);

                fMatch = true;
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

                fMatch = true;
                break;
            }
            KEY("MoveSteps", ship->move_steps, fread_number(fp));
            break;

        case 'N':
            KEYS("Name", ship->ship_name, fread_string(fp));
            KEY("NpcAutonomous", ship->npc_autonomous, fread_number(fp));
            KEY("NpcGoalCooldown", ship->npc_goal_cooldown, fread_number(fp));
            KEY("NpcOffpath", ship->npc_offpath_active, fread_number(fp));
            if( !str_cmp(word, "Navigator") )
            {
                unsigned long id1 = fread_number(fp);
                unsigned long id2 = fread_number(fp);

                ship->navigator = ship_load_find_crew(ship, id1, id2);

                fMatch = true;
                break;
            }
            if( !str_cmp(word, "NpcResumePoint") )
            {
                long wuid = fread_number(fp);
                ship->npc_resume_point.wilds = get_wilds_from_uid(NULL, wuid);
                ship->npc_resume_point.w = wuid;
                ship->npc_resume_point.x = fread_number(fp);
                ship->npc_resume_point.y = fread_number(fp);

                if( !ship->npc_resume_point.wilds )
                {
                    memset(&ship->npc_resume_point, 0, sizeof(ship->npc_resume_point));
                }

                fMatch = true;
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

                fMatch = true;
                break;
            }
            if( !str_cmp(word, "Owner") )
            {
                ship->owner_uid[0] = fread_number(fp);
                ship->owner_uid[1] = fread_number(fp);
                fMatch = true;
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

                fMatch = true;
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

                fMatch = true;
                break;
            }
            if( !str_cmp(word, "ShipAttacked") )
            {
                ship->ship_attacked_uid[0] = fread_number(fp);
                ship->ship_attacked_uid[1] = fread_number(fp);
                fMatch = true;
                break;
            }
            if( !str_cmp(word, "ShipChased") )
            {
                ship->ship_chased_uid[0] = fread_number(fp);
                ship->ship_chased_uid[1] = fread_number(fp);
                fMatch = true;
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

                fMatch = true;
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
            pbugf(LOG_ERROR, "ship_load: no match for word %.50s", word);
        }

    }

    /* Resolve ship index - try area-scoped first, fall back to global */
    if (area) {
        index = get_ship_index_for_area(area, vnum);
    }
    if (!index) {
        index = get_ship_index(vnum);
    }

    if (!index) {
        log_stringf("ship_load: ship index %ld not found", vnum);
        extract_ship(ship);
        return NULL;
    }

    ship->index = index;

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

/**
 * save_ship_uid - Write a UID field to file if set
 *
 * Helper function to conditionally write UID references.
 * Only writes if at least one component is non-zero.
 *
 * @param fp     Open file to write to
 * @param field  Field name to use
 * @param uid    Two-part UID array
 */
void save_ship_uid(FILE *fp, char *field, unsigned long uid[2])
{
    if( uid[0] > 0 || uid[1] > 0 )
    {
        fprintf(fp, "%s %lu %lu\n", field, uid[0], uid[1]);
    }
}

/**
 * ship_special_key_save - Write a special key tracking structure to file
 *
 * Saves key VNUM and all associated key instance UIDs.
 *
 * @param fp  Open file to write to
 * @param sk  Special key data to save
 */
void ship_special_key_save(FILE *fp, SPECIAL_KEY_DATA *sk)
{
    ITERATOR it;
    LLIST_UID_DATA *luid;

    fprintf(fp, "#SPECIALKEY %ld %ld\n", sk->key_wnum.pArea ? sk->key_wnum.pArea->uid : 0, sk->key_wnum.vnum);

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

/**
 * ship_save - Write a complete ship instance to file
 *
 * Serializes entire ship state including:
 * - Template reference and basic stats (name, type, hp, armor)
 * - Owner UID, flags, movement state
 * - Steering data (heading, target, turning)
 * - Navigation (seek point, waypoints, routes)
 * - Physical object and interior instance
 * - Crew members and role assignments
 * - Special keys
 *
 * Note: Routes must be saved after waypoints since they reference
 * waypoints by index.
 *
 * @param fp    Open file to write to
 * @param ship  Ship to save
 * @return      true on success
 */
bool ship_save(FILE *fp, SHIP_DATA *ship)
{
    ITERATOR it, rit;
    SPECIAL_KEY_DATA *sk;
    CHAR_DATA *crew, *oarsman;

    AREA_DATA *ship_area = ship->index->area ? ship->index->area : get_system_area_fallback();
    fprintf(fp, "#SHIP %s\n", widevnum_string(ship_area, ship->index->vnum, NULL));

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

    fprintf(fp, "NpcAutonomous %d\n", ship->npc_autonomous ? 1 : 0);
    fprintf(fp, "NpcOffpath %d\n", ship->npc_offpath_active ? 1 : 0);
    fprintf(fp, "NpcGoalCooldown %d\n", ship->npc_goal_cooldown);

    if( ship->npc_resume_point.wilds != NULL )
    {
        fprintf(fp, "NpcResumePoint %ld %d %d\n",
            ship->npc_resume_point.wilds->uid,
            ship->npc_resume_point.x,
            ship->npc_resume_point.y);
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

/**
 * resolve_ships_player - Link ships to a player on login
 *
 * Called when a player enters the game. Scans all loaded ships
 * and sets owner/char_attacked pointers for any ships that have
 * matching owner_uid or char_attacked_uid.
 *
 * @param ch  Player character that just logged in
 */
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

/**
 * resolve_ships - Link ship-to-ship references after loading
 *
 * Called after all ships are loaded to resolve inter-ship UID
 * references to actual pointers (attacked, chased, boarded_by).
 */
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

/**
 * detach_ships_player - Unlink ships from a player on logout
 *
 * Called when a player leaves the game. Clears owner and
 * char_attacked pointers for any ships referencing this player.
 * The UIDs remain set for re-linking on next login.
 *
 * @param ch  Player character logging out
 */
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

/**
 * detach_ships_ship - Clear references to a ship being extracted
 *
 * Called when a ship is being destroyed. Clears any ship_attacked,
 * ship_chased, or boarded_by pointers from other ships that reference it.
 *
 * @param old_ship  Ship being extracted
 */
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


/**
 * get_ship_uids - Find a ship by two-part UID (wrapper)
 *
 * Convenience wrapper that takes UID parts as separate parameters.
 *
 * @param id1  First part of UID
 * @param id2  Second part of UID
 * @return     Matching ship or NULL
 */
SHIP_DATA *get_ship_uids(unsigned long id1, unsigned long id2)
{
    unsigned long id[2];
    id[0] = id1;
    id[1] = id2;

    return get_ship_uid(id);
}

/**
 * get_ship_uid - Find a ship by UID array
 *
 * Searches loaded_ships for a ship with matching UID.
 *
 * @param id  Two-element UID array to match
 * @return    Matching ship or NULL
 */
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

/**
 * get_ship_nearby - Find another player's ship in the same room by name
 *
 * Searches for a ship matching the name that is in the specified room
 * but NOT owned by the given character.
 *
 * @param name   Ship name to match
 * @param room   Room to search in
 * @param owner  Character to exclude (find ships NOT owned by this char)
 * @return       Matching ship or NULL
 */
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

/**
 * is_ship_safe - Check if a ship is in a safe/protected state
 *
 * A ship is safe if it has SHIP_PROTECTED flag or is in a ROOM_SAFE_HARBOR.
 * Used to prevent combat/attacks in safe areas.
 *
 * @param ch     Character attempting action (unused currently)
 * @param ship   Ship to check safety of
 * @param ship2  Other ship involved (or NULL for general safety check)
 * @return       true if ship is safe from attack
 */
bool is_ship_safe(CHAR_DATA *ch, SHIP_DATA *ship, SHIP_DATA *ship2)
{
    if( IS_SET(ship->ship_flags, SHIP_PROTECTED) )
        return true;

    if ( ship2 == NULL )
    {
        if( !ship->ship->in_room )
            return false;

        if( IS_SET(ship->ship->in_room->room_flag[1], ROOM_SAFE_HARBOR) )
            return true;

        return false;
    }


    return true;
}

/**
 * get_room_ship - Get the ship that owns a room
 *
 * Navigates from a room through its instance_section and instance
 * to find the owning ship (if room is part of a ship interior).
 *
 * @param room  Room to check
 * @return      Ship owning this room, or NULL
 */
SHIP_DATA *get_room_ship(ROOM_INDEX_DATA *room)
{
    if( !room ) return NULL;
    if( !IS_VALID(room->instance_section) ) return NULL;
    if( !IS_VALID(room->instance_section->instance) ) return NULL;
    if( !IS_VALID(room->instance_section->instance->ship) ) return NULL;

    return room->instance_section->instance->ship;
}

/**
 * ischar_onboard_ship - Check if a character is aboard a specific ship
 *
 * @param ch    Character to check
 * @param ship  Ship to check
 * @return      true if ch is in a room belonging to ship's interior
 */
bool ischar_onboard_ship(CHAR_DATA *ch, SHIP_DATA *ship)
{
    if( !IS_VALID(ch) || !ch->in_room ) return false;
    if( !IS_VALID(ship) ) return false;

    return get_room_ship(ch->in_room) == ship;
}

/**
 * get_ship_wildsicon - Get the map icon character for a ship
 *
 * Returns a colored 'O' representing the ship on wilderness maps:
 * - NPC ships: dark gray (or red if scuttling)
 * - Player ships: white (or bright red if scuttling)
 *
 * @param ship  Ship to get icon for
 * @param buf   Output buffer
 * @param len   Buffer size
 */
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

/**
 * get_ship_location - Format ship's location as a human-readable string
 *
 * Generates a location description for display to players:
 * - Immortals with holylight see exact coordinates/VNUMs
 * - Players see terrain type and nearest landmark/area
 * - Ship owners with sextant reading see South/East coordinates
 * - Region names used for ships far from any area
 *
 * @param ch    Character viewing the location (affects detail level)
 * @param ship  Ship to describe location of
 * @param buf   Output buffer
 * @param len   Buffer size
 */
void get_ship_location(CHAR_DATA *ch, SHIP_DATA *ship, char *buf, size_t len)
{
    ROOM_INDEX_DATA *room = obj_room(ship->ship);
    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
    {
        // Give exact location of ship
        if( IS_WILDERNESS(room) )
        {
            snprintf(buf, len, "{Y%s {x({W%ld{x) at ({C%ld{x,{C%ld{x)", room->wilds->name, room->wilds->uid, room->x, room->y);
        }
        else if( room->source )
        {
            // Only scripting should ever cause this situation
            snprintf(buf, len, "{Y%s {x({W%s{x):{C%lu{x:{C%lu{x", room->name, widevnum_string_room(room->source, NULL), room->id[0], room->id[1]);
        }
        else
        {
            snprintf(buf, len, "{Y%s {x({W%s{x)", room->name, widevnum_string_room(room, NULL));
        }
    }
    else
    {
        if( IS_WILDERNESS(room) )
        {
            WILDS_DATA *wilds = room->wilds;
            WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, room->x, room->y);

            int closestDistanceSq = 401;	// 20 distance radius
            AREA_DATA *closestArea = NULL;
            for( AREA_DATA *area = area_first; area; area = area->next )
            {
                if( area->wilds_uid == wilds->uid )
                {
                    int distanceSq =
                        (area->x - room->x) * (area->x - room->x) +
                        (area->y - room->y) * (area->y - room->y);

                    if( distanceSq < closestDistanceSq )
                    {
                        closestDistanceSq = distanceSq;
                        closestArea = area;
                    }
                }
            }

            if( closestArea != NULL )
            {
                if( ship_isowner_player(ship, ch) && ship->sextant_x >= 0 && ship->sextant_y >= 0 )
                {
                    snprintf(buf, len, "Anchored at {YSouth {W%d{x by {YEast {W%d{x near {Y%s{x", ship->sextant_y, ship->sextant_x, closestArea->name);
                }
                else
                {
                    snprintf(buf, len, "{Y%s{x near {Y%s{x", terrain->template->name, closestArea->name);
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

/**
 * ship_autosurvey - Trigger automatic survey for players on moving ship
 *
 * Called after ship movement. For each connected player aboard the ship
 * with PLR_AUTOSURVEY flag who is awake and can see outside (outdoor
 * room, helm, or view room), executes the survey command.
 *
 * @param ship  Ship that just moved
 */
void ship_autosurvey( SHIP_DATA *ship )
{
    DESCRIPTOR_DATA *d;

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
        CHAR_DATA *victim;

        victim = d->original ? d->original : d->character;

        if( d->connected == CON_PLAYING &&
            IS_SET(victim->act[1], PLR_AUTOSURVEY) &&
            victim->in_room != NULL &&
            ischar_onboard_ship(victim, ship) &&
            IS_AWAKE(victim) &&
            (IS_OUTSIDE(victim) ||
                IS_SET(victim->in_room->room_flag[0], ROOM_SHIP_HELM) ||
                IS_SET(victim->in_room->room_flag[0], ROOM_VIEWWILDS)) )
        {
            do_function(victim, &do_survey, "auto" );
        }
    }
}


/**
 * ship_echo - Send a message to all players aboard a ship
 *
 * Broadcasts str to every connected player who is in a room
 * belonging to the ship's interior instance.
 *
 * @param ship  Ship to broadcast to
 * @param str   Message to send (can contain act codes)
 */
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
            act(str, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
}

/**
 * boat_echo - Alias for ship_echo
 *
 * @param ship  Ship to broadcast to
 * @param str   Message string (supports color codes)
 */
void boat_echo(SHIP_DATA *ship, char *str)
{
    ship_echo(ship, str);
}

/**
 * ship_echoaround - Send a message to all players aboard except one
 *
 * Broadcasts str to every connected player aboard the ship,
 * excluding the specified character.
 *
 * @param ship  Ship to broadcast to
 * @param ch    Character to exclude from message
 * @param str   Message to send (ch available as $N in act codes)
 */
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
            act(str, victim, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
}


/**
 * ship_has_enough_crew - Check if ship has minimum crew for operation
 *
 * Checks whether the ship's crew count meets the min_crew requirement.
 *
 * @param ship  Ship to check
 * @return      true if sufficient crew, false otherwise
 */
bool ship_has_enough_crew( SHIP_DATA *ship )
{
    if (!IS_VALID(ship) || !ship->index)
        return false;

    if (ship->index->min_crew <= 0)
        return true;

    int crew_count = ship->crew ? list_size(ship->crew) : 0;
    return crew_count >= ship->index->min_crew;
}


/**
 * do_ships - Staff command to manage ship instances
 *
 * Provides administrative control over ships:
 * - ships list [player]: List all loaded ships (optionally filtered by owner)
 * - ships load [vnum] [owner] [name]: Create a new ship from template
 * - ships unload [#]: Destroy a ship instance
 *
 * Shows ship status including HP, armor, crew, movement state, etc.
 *
 * @param ch        Staff member using the command
 * @param argument  Subcommand and arguments
 */
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
            send_to_char("         ships owner <name|none>\n\r", ch);
            send_to_char("         ships ai <on|off>\n\r", ch);
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
            bool error = false;
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
                    error = true;
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
        else if( !str_prefix(arg, "owner") )
        {
            SHIP_DATA *room_ship = get_room_ship(ch->in_room);
            CHAR_DATA *owner = NULL;
            CHAR_DATA *old_owner;

            if( !IS_VALID(room_ship) )
            {
                send_to_char("You must be on a ship to set owner.\n\r", ch);
                return;
            }

            if( argument[0] == '\0' )
            {
                send_to_char("Syntax: ships owner <name|none>\n\r", ch);
                return;
            }

            if( str_cmp(argument, "none") )
            {
                owner = get_char_world(ch, argument);

                if( !owner )
                {
                    send_to_char("No such character found.\n\r", ch);
                    return;
                }
            }

            old_owner = room_ship->owner;
            room_ship->owner = owner;

            if( old_owner && !IS_NPC(old_owner) && old_owner->pcdata )
                list_remlink(old_owner->pcdata->ships, room_ship, false);

            if( owner )
            {
                room_ship->owner_uid[0] = owner->id[0];
                room_ship->owner_uid[1] = owner->id[1];
                room_ship->npc_autonomous = IS_NPC(owner) ? true : room_ship->npc_autonomous;

                if( !IS_NPC(owner) && owner->pcdata )
                {
                    if( !list_hasdata(owner->pcdata->ships, room_ship) )
                        list_appendlink(owner->pcdata->ships, room_ship);
                }

                send_to_char("Ship owner updated.\n\r", ch);
            }
            else
            {
                room_ship->owner_uid[0] = 0;
                room_ship->owner_uid[1] = 0;
                send_to_char("Ship owner cleared.\n\r", ch);
            }

            return;
        }
        else if( !str_prefix(arg, "ai") )
        {
            SHIP_DATA *room_ship = get_room_ship(ch->in_room);

            if( !IS_VALID(room_ship) )
            {
                send_to_char("You must be on a ship to toggle AI.\n\r", ch);
                return;
            }

            if( argument[0] == '\0' )
            {
                send_to_char("Syntax: ships ai <on|off>\n\r", ch);
                return;
            }

            if( !str_prefix(argument, "on") )
            {
                room_ship->npc_autonomous = true;
                send_to_char("Ship autonomous NPC control enabled.\n\r", ch);
                return;
            }

            if( !str_prefix(argument, "off") )
            {
                room_ship->npc_autonomous = false;
                room_ship->npc_offpath_active = false;
                room_ship->npc_goal_cooldown = 0;
                memset(&room_ship->npc_resume_point, 0, sizeof(room_ship->npc_resume_point));
                send_to_char("Ship autonomous NPC control disabled.\n\r", ch);
                return;
            }

            send_to_char("Syntax: ships ai <on|off>\n\r", ch);
            return;
        }
        else if( !str_prefix(arg, "load") )
        {
            char buf[2*MSL];
            char arg2[MIL];
            char arg3[MIL];
            WNUM wnum;

            argument = one_argument(argument, arg2);
            argument = one_argument(argument, arg3);

            // Parse WNUM - use NULL for default area so bare vnums search globally
            if( !parse_widevnum(arg2, NULL, &wnum) )
            {
                send_to_char("Invalid ship reference. Use vnum or area_uid#vnum format.\n\r", ch);
                return;
            }

            SHIP_INDEX_DATA *index = NULL;
            if( wnum.pArea )
            {
                index = get_ship_index_for_area(wnum.pArea, wnum.vnum);
            }
            
            if( !index )
            {
                index = get_ship_index(wnum.vnum);
            }

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

                if( !room_in_sector(ch->in_room, SECT_WATER_SWIM) &&
                    !room_in_sector(ch->in_room, SECT_WATER_NOSWIM) )
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

            CHAR_DATA *owner = get_char_world(ch, arg3);

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

            /* NPC ship auto-detection: if template has npc flag, do full NPC setup */
            if (IS_SET(index->flags, SHIP_AUTONOMOUS_NPC)) {
                NPC_SHIP_DATA *npc = new_npc_ship_data();
                npc->ship = ship;
                npc->state = NPC_SHIP_STATE_STOPPED;
                npc->trigger_char = NULL;
                npc->captain = NULL;

                ship->npc_ship = npc;
                ship->npc_autonomous = true;
                ship->owner = NULL;

                ship_populate_crew(ship);
                ship_auto_assign_crew(ship);

                /* Set initial heading and power */
                if (ship->ship_power <= SHIP_SPEED_STOPPED)
                    ship->ship_power = SHIP_SPEED_FULL_SPEED;
                if (ship->steering.heading < 0) {
                    int heading = number_range(0, 359);
                    ship->steering.heading = heading;
                    ship->steering.heading_target = heading;
                    steering_calc_heading(ship);
                }
                ship_set_move_steps(ship);

                npc->next = npc_ship_list;
                npc_ship_list = npc;

                send_to_char(formatf("NPC ship '%s' spawned (type: %s, crew: %d).\n\r",
                    index->name,
                    flag_string(npc_ship_types, index->npc_type),
                    ship->crew ? list_size(ship->crew) : 0), ch);
            } else {
                ship->owner = owner;
                if( owner )
                {
                    ship->owner_uid[0] = owner->id[0];
                    ship->owner_uid[1] = owner->id[1];
                    if( IS_NPC(owner) )
                        ship->npc_autonomous = true;
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

                act("$p splashes down after being christened '$T'.",ch, NULL, NULL,ship->ship, NULL, NULL,ship->ship_name,TO_ALL, NULL, NULL);
            }

            obj_to_room(ship->ship, ch->in_room);

            /* Sync ship instance entrance to ship object location */
            if (IS_VALID(ship->instance) && ship->instance->entrance && ch->in_room->wilds) {
                ship->instance->entrance->wilds = ch->in_room->wilds;
                ship->instance->entrance->x = ch->in_room->x;
                ship->instance->entrance->y = ch->in_room->y;
            }
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

/**
 * ship_can_issue_command - Check if character can issue ship commands
 *
 * Character must be at the ship's helm (ROOM_SHIP_HELM flag) to
 * issue navigation commands.
 *
 * @param ch    Character trying to issue command
 * @param ship  Ship (unused currently, could check roles)
 * @return      true if at helm, false otherwise
 */
bool ship_can_issue_command(CHAR_DATA *ch, SHIP_DATA *ship)
{
    if (!IS_SET(ch->in_room->room_flag[0], ROOM_SHIP_HELM))
    {
        return false;
    }

    return true;
}

/**
 * ship_dispatch_message - Send command feedback and notify ship owner
 *
 * Sends error/status message to the command issuer. If the issuer
 * is not the ship owner, also notifies the owner of the dispatched
 * command.
 *
 * @param ch       Character who issued command
 * @param ship     Ship the command was issued on
 * @param error    Message to display
 * @param command  Command that was executed (for owner notification)
 */
void ship_dispatch_message(CHAR_DATA *ch, SHIP_DATA *ship, char *error, char *command)
{
    send_to_char(error, ch);
    send_to_char("\n\r", ch);

    // This command was executed by someone other than the owner, tell the owner of the ship if they are online
    if( IS_VALID(ship->owner) && ship->owner != ch )
    {
        act("{YDispatched '{W$T{Y':{x\n\r$t", ship->owner, NULL, NULL, NULL, NULL, error, command, TO_CHAR, NULL, NULL);
    }
}


/**
 * do_ship_scuttle - Player command to destroy their ship
 *
 * Initiates ship scuttling (self-destruction). Ship will be destroyed
 * after scuttle_time ticks. Only works for ship owner or against
 * enemy ships not in safe harbor.
 *
 * @param ch        Character issuing scuttle command
 * @param argument  Unused
 */
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

    {
        ROOM_INDEX_DATA *plith_harbour = get_reserved_room_index("room_plith_harbour");
        ROOM_INDEX_DATA *southern_harbour = get_reserved_room_index("room_southern_harbour");
        ROOM_INDEX_DATA *northern_harbour = get_reserved_room_index("room_northern_harbour");

        if ((ship->ship->in_room == plith_harbour ||
            ship->ship->in_room == southern_harbour ||
            ship->ship->in_room == northern_harbour) &&
            ship->owner != ch)
    {
    boat_echo(ship, "{MThe flames are extinguished by a mysterious protective magic.{x");
    return;
    }
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


/**
 * do_ship_steer - Player command to control ship direction
 *
 * With no argument: reports current heading and turn status.
 * With direction: sets new heading or turning mode.
 *
 * Accepts:
 * - Cardinal directions: north, south, east, west, etc.
 * - Numeric degrees: 0-359 (0=North)
 * - Relative: port (turn left), starboard (turn right)
 *
 * Requires being at helm (ROOM_SHIP_HELM) or having a first mate
 * who will relay commands with a delay based on leadership skill.
 *
 * @param ch        Character issuing steer command
 * @param argument  Direction, degrees, or port/starboard
 */
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
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'steer $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

/**
 * do_ship_engines - Control airship engine throttle
 *
 * Airship-specific speed control. With no argument, reports current
 * engine status. Accepts speed values:
 * - stop: Cut engines to idle
 * - minimal: 1% power
 * - half: 50% power
 * - full: 100% power
 * - percentage: 0-100%
 * - explicit count: 1 to max move_steps
 *
 * @param ch        Character issuing command
 * @param argument  Speed setting
 */
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
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( ship->ship_type != SHIP_AIR_SHIP )
    {
        act("The vessel has no engines.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( arg[0] == '\0' )
    {
        if( ship->ship_power > SHIP_SPEED_STOPPED )
            sprintf(buf, "The vessel is currently running the engines at {W%d%%{x.", ship->ship_power);
        else
            sprintf(buf, "The engines are currently idling.");

        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    // First Mates can always execute orders on their ship as they were assigned that role
    if( ch != ship->first_mate )
    {
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'engines $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("You give the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for the furnace output to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
            act("You give the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
        act("You give the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        ship_echo(ship, "You feel the vessel slowing down.");
    }
    else if( ship->ship_power < speed )
    {
        act("You give the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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

/**
 * do_ship_sails - Control sailing ship speed via sails
 *
 * Sailing ship-specific speed control. With no argument, reports
 * current sail status. Accepts speed values:
 * - stop: Furl sails
 * - minimal: 1% sail
 * - half: 50% sail
 * - full: 100% sail (full canvas)
 * - percentage: 0-100%
 * - explicit count: 1 to max move_steps
 *
 * Requires ship to have a heading set first.
 *
 * @param ch        Character issuing command
 * @param argument  Speed setting
 */
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
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( ship->ship_type != SHIP_SAILING_BOAT )
    {
        act("The vessel has no sails.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( arg[0] == '\0' )
    {
        if( ship->ship_power > SHIP_SPEED_STOPPED )
            sprintf(buf, "The vessel is currently running sails at {W%d%%{x.", ship->ship_power);
        else
            sprintf(buf, "The sails are currently furled.");

        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    // First Mates can always execute orders on their ship as they were assigned that role
    if( ch != ship->first_mate )
    {
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'sails $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("You give the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for the sails to be lowered.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
            act("You give the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for full speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
        act("You give the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to reduce speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        ship_echo(ship, "You feel the vessel slowing down.");
    }
    else if( ship->ship_power < speed )
    {
        act("You give the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to increase speed.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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

/**
 * do_ship_speed - Display current ship speed status
 *
 * Reports the ship's current effective speed as a descriptive message
 * (stopped, minimal, half, full, etc.). This is a read-only status
 * command, not used to change speed.
 *
 * @param ch        Character checking speed
 * @param argument  Unused
 */
void do_ship_speed( CHAR_DATA *ch, char *argument )
{
    //char buf[MSL];
    char arg[MAX_INPUT_LENGTH];
    SHIP_DATA *ship;
    char cmd[MIL];

    sprintf(cmd, "ship speed %s", argument);

//	char *command = argument;
    argument = one_argument( argument, arg);

    ship = get_room_ship(ch->in_room);

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

/**
 * do_ship_aim - Aim ship cannons at a target (NYI)
 *
 * Placeholder for ship combat targeting system. Currently disabled
 * with #if 0 block. Would allow aiming cannons at other ships or
 * characters.
 *
 * @param ch        Character issuing aim command
 * @param argument  Target specification
 */
void do_ship_aim( CHAR_DATA *ch, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    SHIP_DATA *orig_ship;
    SHIP_DATA *target = NULL;

    argument = one_argument( argument, arg);

    if (!ON_SHIP(ch)) {
        send_to_char("You aren't even on a vessel.\n\r", ch);
        return;
    }

    orig_ship = get_room_ship(ch->in_room);
    if (!IS_VALID(orig_ship)) {
        send_to_char("You aren't even on a vessel.\n\r", ch);
        return;
    }

    if (!IS_SET(ch->in_room->room_flag[0], ROOM_SHIP_HELM)) {
        send_to_char("You must be at the helm of the vessel to order an attack.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(orig_ship, ch)) {
        send_to_char("You must be the owner to order an attack.\n\r", ch);
        return;
    }

    if (!ship_has_enough_crew(orig_ship)) {
        send_to_char("There isn't enough crew to order that command!\n\r", ch);
        return;
    }

    if (arg[0] == '\0') {
        send_to_char("Syntax: ship aim <target_ship>\n\r"
                     "        ship aim stop\n\r", ch);
        return;
    }

    /* Stop attacking */
    if (!str_prefix(arg, "stop")) {
        if (orig_ship->attack_position == SHIP_ATTACK_STOPPED &&
            !IS_VALID(orig_ship->ship_attacked)) {
            send_to_char("You aren't attacking anything.\n\r", ch);
            return;
        }

        act("You give the order to cease fire.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to cease fire.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        orig_ship->attack_position = SHIP_ATTACK_STOPPED;
        orig_ship->ship_attacked = NULL;
        orig_ship->char_attacked = NULL;
        return;
    }

    /* Must have at least one operational weapon module */
    int max_range = ship_get_max_weapon_range(orig_ship);
    if (max_range <= 0) {
        send_to_char("This vessel has no operational weapons.\n\r", ch);
        return;
    }

    /* Find target ship — search loaded_ships by name match */
    {
        ITERATOR it;
        SHIP_DATA *ship;
        iterator_start(&it, loaded_ships);
        while ((ship = (SHIP_DATA *)iterator_nextdata(&it))) {
            if (ship == orig_ship) continue;
            if (!IS_VALID(ship) || !IS_VALID(ship->ship) ||
                !ship->ship->in_room) continue;

            /* Match by ship name */
            if (ship->ship_name && !str_prefix(arg, ship->ship_name_plain ? ship->ship_name_plain : ship->ship_name)) {
                target = ship;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (!target) {
        send_to_char("That ship is not visible.\n\r", ch);
        return;
    }

    /* Range check */
    int dist = ship_distance(orig_ship, target);
    if (dist > max_range) {
        send_to_char(formatf("That ship is out of weapon range (%d tiles away, max range %d).\n\r",
            dist, max_range), ch);
        return;
    }

    /* Safe zone check */
    if (is_ship_safe(ch, orig_ship, target)) {
        return;
    }

    /* Engage! */
    act("You give the order to target $t!", ch, NULL, NULL, NULL, NULL, target->ship_name ? target->ship_name : "the enemy vessel", NULL, TO_CHAR, NULL, NULL);
    act("$n gives the order to target the enemy vessel!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    SHIP_ATTACK_STATE(ch, 8);

    orig_ship->attack_position = SHIP_ATTACK_LOADING;
    orig_ship->ship_attacked = target;
    orig_ship->char_attacked = NULL;

    /* Reputation: penalize for attacking a faction NPC ship */
    ship_combat_rep_attack(ch, orig_ship, target);

    /* Coast guard proximity: alert nearby coast guard ships */
    ship_coast_guard_alert(ch, orig_ship, target);

    /* Start all weapon modules reloading */
    if (orig_ship->modules) {
        ITERATOR it;
        SHIP_MODULE *mod;
        iterator_start(&it, orig_ship->modules);
        while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
            if (mod->index && mod->index->type == HARDPOINT_WEAPON &&
                mod->operational && mod->reload_countdown <= 0) {
                mod->reload_countdown = mod->index->reload_time;
            }
        }
        iterator_stop(&it);
    }

    /* Warn the target */
    boat_echo(target, formatf("{W%s is targeting your vessel!{x",
        orig_ship->ship_name ? orig_ship->ship_name : "An enemy vessel"));
}

/**
 * do_ship_navigate - Automated navigation command for ships
 *
 * Provides waypoint-based navigation:
 * - navigate goto <waypoint>: Sail to a named waypoint
 * - navigate seek <south> <east>: Sail to coordinates
 * - navigate plot <wp1> <wp2> ...: Set multi-waypoint route
 * - navigate cancel: Stop current navigation
 * - navigate route: List available routes
 * - navigate route go <route>: Follow a saved route
 *
 * Uses navigator crew skill or player navigation skill for accuracy.
 *
 * @param ch        Character issuing navigation command
 * @param argument  Subcommand and parameters
 */
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
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch))
    {
        act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
                    skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            skill = get_skill(ch, skill_resolve_gsn("navigation"));

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

/**
 * do_ship_oars - Control oar-based propulsion
 *
 * Commands assigned oarsmen to row, providing supplemental or
 * primary propulsion. Oarsmen drain stamina while rowing and
 * rest when exhausted. With no argument, reports oar status.
 *
 * - oars stop: Oarsmen stop rowing
 * - oars row: Oarsmen begin rowing at maximum power
 * - oars <percentage>: Set oar power level
 *
 * @param ch        Character issuing command
 * @param argument  Oar power setting
 */
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
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( ship->oars < 1 )
    {
        act("The vessel has no oars.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if( arg[0] == '\0' )
    {
        if( ship->oar_power > SHIP_SPEED_STOPPED )
            sprintf(buf, "The vessel is currently running at {W%d%%{x oar power.", ship->oar_power);
        else
            sprintf(buf, "The vessel is not using oars currently.");

        act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    // First Mates can always execute orders on their ship as they were assigned that role
    if( ch != ship->first_mate )
    {
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'sails $T'.", ch, NULL, NULL, NULL, NULL, NULL, command, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("You give the order for the oarsmen to cease.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for the oarsmen to cease.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
            act("You give the order for full oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gives the order for full oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
        act("You give the order to reduce oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to reduce oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        ship_echo(ship, "You feel the vessel slowing down.");
    }
    else if( ship->oar_power < speed )
    {
        act("You give the order to increase oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to increase oarring.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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


/**
 * do_ship_christen - Name a newly acquired ship
 *
 * Allows ship owner to give their ship a name. Can only be done once
 * (ship must not already have a name). Name is limited to 30 characters
 * excluding color codes. Updates the ship object's name, short_descr,
 * and description to include the christened name.
 *
 * @param ch        Ship owner christening the vessel
 * @param argument  Desired ship name (may include color codes)
 */
void do_ship_christen(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    SHIP_DATA *ship;

    ship = get_room_ship(ch->in_room);

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

    act("{Y$n christens the vessel '{x$T{Y'.{x", ch, NULL, NULL, NULL, NULL, NULL, ship->ship_name, TO_ROOM, NULL, NULL);
    act("{YYou christen the vessel '{x$T{Y'.{x", ch, NULL, NULL, NULL, NULL, NULL, ship->ship_name, TO_CHAR, NULL, NULL);
}

/**
 * do_ship_land - Land an airship
 *
 * Airship-specific command to descend and land on the ground.
 * Must be airborne and in a location suitable for landing.
 * Sets ship power to SHIP_SPEED_LANDED state.
 *
 * @param ch        Character issuing land command
 * @param argument  Unused
 */
void do_ship_land(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    SHIP_DATA *ship = get_room_ship(ch->in_room);

    if( !IS_VALID(ship) )
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    // First Mates can always execute orders on their ship as they were assigned that role
    if( ch != ship->first_mate )
    {
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'land'.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
    AREA_DATA *to_area = NULL;

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

            if( area->wilds_uid != uid || area->airship_land_wnum.vnum < 1 ) continue;

            int distanceSq = ( x - area->x ) * ( x - area->x ) + ( y - area->y ) * ( y - area->y );

            if( distanceSq < bestDistanceSq )
            {
                bestDistanceSq = distanceSq;
                to_area = area;
            }
        }

        if( to_area == NULL )
        {
            ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
            return;
        }

        AREA_DATA *land_area = to_area->airship_land_wnum.pArea;
        if (!land_area) {
            WNUM wnum;
            if (resolve_widevnum(to_area->airship_land_load.vnum, NULL, &wnum))
                land_area = wnum.pArea;
        }
        if (!land_area) land_area = get_system_area_fallback();
        to_room = get_room_index(land_area, to_area->airship_land_wnum.vnum);
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
            if( !room_in_sector(pTerrain->template, SECT_CITY) )
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
                if( area->wilds_uid != uid || area->airship_land_wnum.vnum < 1 ) continue;

                int distanceSq = ( x - area->x ) * ( x - area->x ) + ( y - area->y ) * ( y - area->y );

                if( distanceSq < bestDistanceSq )
                {
                    bestDistanceSq = distanceSq;
                    to_area = area;
                }
            }

            if( to_area == NULL )
            {
                ship_dispatch_message(ch, ship, "The vessel cannot land here.", "ship land");
                return;
            }

            AREA_DATA *land_area = to_area->airship_land_wnum.pArea;
            if (!land_area) {
                WNUM wnum;
                if (resolve_widevnum(to_area->airship_land_load.vnum, NULL, &wnum))
                    land_area = wnum.pArea;
            }
            if (!land_area) land_area = get_system_area_fallback();
            to_room = get_room_index(land_area, to_area->airship_land_wnum.vnum);
            if( !to_room )
            {
                ship_dispatch_message(ch, ship, "There is no safe place to land the ship here.", "ship land");
                return;
            }
        }
    }

    ship->ship_power = SHIP_SPEED_LANDED;

    if( to_room && to_area )
    {
        sprintf(buf, "{WThe vessel descends down to {Y%s{W below.{x", to_area->name);
        ship_echo(ship, buf);

        if( IS_NULLSTR(ship->ship_name) )
        {
            sprintf(buf, "{W%s %s descends from above to land in {Y%s{W.{x", get_article(ship->index->name, true), ship->index->name, to_area->name);
        }
        else
        {
            sprintf(buf, "{WThe %s '{x%s{W' descends from above to land in {Y%s{W.{x", ship->index->name, ship->ship_name, to_area->name);
        }
        room_echo(ship->ship->in_room, buf);

        obj_from_room(ship->ship);
        obj_to_room(ship->ship, to_room);
        
        /* Sync ship instance entrance to new location */
        if (IS_VALID(ship->instance) && ship->instance->entrance && to_room->wilds) {
            ship->instance->entrance->wilds = to_room->wilds;
            ship->instance->entrance->x = to_room->x;
            ship->instance->entrance->y = to_room->y;
        }
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

/**
 * do_ship_launch - Launch a landed airship into the air
 *
 * Airship-specific command to take off from the ground. Must be
 * in SHIP_SPEED_LANDED state and outdoors. Sets heading to north
 * if no heading was set and moves ship into wilderness airspace.
 *
 * @param ch        Character issuing launch command
 * @param argument  Unused
 */
void do_ship_launch(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    SHIP_DATA *ship = get_room_ship(ch->in_room);

    if( !IS_VALID(ship) )
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    // First Mates can always execute orders on their ship as they were assigned that role
    if( ch != ship->first_mate )
    {
        if (!IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)) && !ship_isowner_player(ship, ch))
        {
            act("This isn't your vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

                    act("You give the order to your first mate to 'launch'.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n gives an order to the first mate.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    if( IS_IMMORTAL(ch) && IS_SET(ch->act[0], PLR_HOLYLIGHT) )
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

            act("You must be at the helm of the vessel to steer.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        WILDS_DATA *wilds = get_wilds_from_uid(NULL, area->wilds_uid);

        if( !wilds || area->x < 0 || area->y < 0 )
        {
            ship_dispatch_message(ch, ship, "There is nowhere for the vessel to go.", "ship launch");
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
        
        /* Sync ship instance entrance to new location */
        if (IS_VALID(ship->instance) && ship->instance->entrance && room->wilds) {
            ship->instance->entrance->wilds = room->wilds;
            ship->instance->entrance->x = room->x;
            ship->instance->entrance->y = room->y;
        }

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

/**
 * do_ship_chase - Chase another ship
 *
 * Sets the ship to auto-pursue a target ship, matching heading.
 * Speed advantage determines if chaser closes or target escapes.
 * Disengages after target exceeds max weapon range × 3.
 *
 * @param ch        Character issuing chase command
 * @param argument  Target ship identifier or "stop"
 */
void do_ship_chase(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    SHIP_DATA *ship;

    argument = one_argument(argument, arg);

    ship = get_room_ship(ch->in_room);
    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    if (!IS_SET(ch->in_room->room_flag[0], ROOM_SHIP_HELM)) {
        send_to_char("You must be at the helm to give chase orders.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch)) {
        send_to_char("You must be the owner to give chase orders.\n\r", ch);
        return;
    }

    if (arg[0] == '\0') {
        if (IS_VALID(ship->ship_chased))
            send_to_char(formatf("Currently chasing: %s\n\r",
                ship->ship_chased->ship_name ? ship->ship_chased->ship_name : "unknown vessel"), ch);
        else
            send_to_char("Not currently chasing anyone.\n\r"
                         "Syntax: ship chase <target>\n\r"
                         "        ship chase stop\n\r", ch);
        return;
    }

    if (!str_prefix(arg, "stop")) {
        if (!IS_VALID(ship->ship_chased)) {
            send_to_char("You aren't chasing anyone.\n\r", ch);
            return;
        }
        ship->ship_chased = NULL;
        act("You give the order to break off pursuit.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives the order to break off pursuit.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    if (!ship_has_enough_crew(ship)) {
        send_to_char("There isn't enough crew to pursue!\n\r", ch);
        return;
    }

    /* Find target */
    SHIP_DATA *target = NULL;
    {
        ITERATOR it;
        SHIP_DATA *s;
        iterator_start(&it, loaded_ships);
        while ((s = (SHIP_DATA *)iterator_nextdata(&it))) {
            if (s == ship) continue;
            if (!IS_VALID(s) || !IS_VALID(s->ship) || !s->ship->in_room) continue;
            if (s->ship_name && !str_prefix(arg, s->ship_name_plain ? s->ship_name_plain : s->ship_name)) {
                target = s;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (!target) {
        send_to_char("That ship is not visible.\n\r", ch);
        return;
    }

    int dist = ship_distance(ship, target);
    int max_range = ship_get_max_weapon_range(ship);
    int chase_range = max_range > 0 ? max_range * 3 : 30;

    if (dist > chase_range) {
        send_to_char("That ship is too far away to chase.\n\r", ch);
        return;
    }

    ship->ship_chased = target;
    act("You give the order to pursue $t!", ch, NULL, NULL, NULL, NULL,
        target->ship_name ? target->ship_name : "the target vessel",
        NULL, TO_CHAR, NULL, NULL);
    act("$n gives the order to pursue an enemy vessel!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    boat_echo(target, formatf("{W%s is giving chase!{x",
        ship->ship_name ? ship->ship_name : "An enemy vessel"));
}

/**
 * do_ship_flag - Set the ship's flag design
 *
 * Allows ship owner to set or change the flag flying from the mast.
 * Flag text can include color codes.
 *
 * @param ch        Ship owner setting the flag
 * @param argument  Flag design/text
 */
void do_ship_flag(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship;

    ship = get_room_ship(ch->in_room);

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

/**
 * do_ship_list - List all ships owned by the player
 *
 * Displays a formatted list of all ships the player owns, showing
 * ship type, name, and current location. Can filter by name.
 *
 * @param ch        Player listing their ships
 * @param argument  Optional name filter
 */
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

/**
 * do_ship_waypoints - Manage ship navigation waypoints
 *
 * Allows ship owners to manage saved waypoints:
 * - (no args): List all waypoints for this ship
 * - add <name>: Save current location as a named waypoint
 * - remove <#>: Delete a waypoint by number
 * - rename <#> <name>: Rename an existing waypoint
 *
 * Waypoints can be used with ship navigate commands.
 *
 * @param ch        Ship owner managing waypoints
 * @param argument  Subcommand and parameters
 */
void do_ship_waypoints(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    char buf[MSL];
    char arg[MIL];

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        int x = -1, y = -1;

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
                (!room_in_sector(terrain->template, SECT_WATER_SWIM) &&
                 !room_in_sector(terrain->template, SECT_WATER_NOSWIM)) )
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            list_remlink(route->waypoints, wp, true);
        }
        iterator_stop(&rit);

        list_remnthlink(ship->waypoints, value, true);
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            act("{xThere are no waypoints on $p{x.", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
                    (!room_in_sector(terrain->template, SECT_WATER_SWIM) &&
                     !room_in_sector(terrain->template, SECT_WATER_NOSWIM)) )
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
    ITERATOR it;
    
    iterator_start(&it, ch->lcarrying);
    while ((map = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (map->item_type == ITEM_BLANK_SCROLL || map->pIndexData == get_reserved_obj_index("obj_blank_scroll"))
            break;
    }
    iterator_stop(&it);

    if (map == NULL)
    {
        send_to_char("You do not have a blank scroll.\n\r", ch);
        return;
    }

    extract_obj(map);
    map = create_object(get_reserved_obj_index("obj_nav_chart"), 0, false);
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

            if( map->item_type == ITEM_BLANK_SCROLL || map->pIndexData == get_reserved_obj_index("obj_blank_scroll") )
            {
                // Replace blank scroll with map object
                extract_obj(map);
                map = create_object(get_reserved_obj_index("obj_nav_chart"), 0, false);
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
                    act("{YThat coordinate is already on $p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("{x$N{Y jots something down onto {x$p{Y and hands it to {x$n{Y.{x", ch, ship->navigator, NULL, map, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
            act("{x$N{Y jots something down onto {x$p{Y and hands it to you.{x", ch, ship->navigator, NULL, map, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

            // Improve the navigator's navigation skill
            crew_skill_improve(ship->navigator, CREW_SKILL_NAVIGATION);
        }
        else
        {
            act("{x$n{Y jots something down onto {x$p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("{YYou jot down coordinates onto {x$p{Y.{x", ch, NULL, NULL, map, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        }
        return;
    }

    do_ship_waypoints(ch, "");
}

/**
 * do_ship_routes - Manage saved navigation routes
 *
 * Routes are ordered sequences of waypoints for automated travel:
 * - routes list: Show all saved routes
 * - routes create <wp1> <wp2> ... <wpN>: Create new route
 * - routes delete <#>: Remove a route
 * - routes rename <#> <name>: Rename a route
 * - routes add <#> <waypoint>: Append waypoint to route
 * - routes insert <#> <waypoint> <pos>: Insert waypoint at position
 * - routes remove <route#> <waypoint#>: Remove waypoint from route
 * - routes move <#from> <#to/up/down>: Reorder routes
 *
 * @param ch        Ship owner managing routes
 * @param argument  Subcommand and parameters
 */
void do_ship_routes(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    char buf[MSL];
    char arg[MIL];

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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

        list_remnthlink(ship->routes, value, true);
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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

        list_remnthlink(route->waypoints, windex, true);
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
            int skill = get_skill(ch, skill_resolve_gsn("navigation"));

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

/**
 * do_ship_keys - Manage special keys for ship access
 *
 * Displays and manages the special keys associated with the ship.
 * Special keys control access to locked ship areas/containers.
 * Lists key types and which specific key instances have been created.
 *
 * @param ch        Ship owner checking keys
 * @param argument  Unused
 */
void do_ship_keys(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    char buf[MSL];
    char arg[MIL];

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            OBJ_INDEX_DATA *key = sk->key_wnum.pArea ? get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum) : NULL;

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
        OBJ_INDEX_DATA *key_index = sk->key_wnum.pArea ? get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum) : NULL;

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

        OBJ_DATA *key = create_object(key_index, 0, true);
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
        act("{xA ship deckhand hands you $p{x.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{xA ship deckhand hands $n $p{x.", ch, NULL, NULL, key, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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

/**
 * crew_skill_rating - Format a crew skill as a visual bar
 *
 * Generates a colored star bar (0-10 stars) plus percentage display
 * for crew skill ratings.
 *
 * @param field   Skill name to display
 * @param rating  Skill value (0-100)
 * @param buf     Output buffer
 * @param len     Buffer length
 */
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

/**
 * do_ship_crew - Manage ship crew members and role assignments
 *
 * View and manage NPC crew:
 * - (no args): List all crew with current role assignments
 * - list: Same as no args
 * - view <#>: View detailed stats for a crew member
 * - assign <#> <role>: Assign crew to a role (firstmate, navigator, scout, oarsman)
 * - unassign <role>: Remove crew from a role
 *
 * Crew members have skills that improve with use.
 *
 * @param ch        Ship owner managing crew
 * @param argument  Subcommand and parameters
 */
void do_ship_crew(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    char buf[MSL];
    char arg[MIL];
    char arg2[MIL];

    if (!IS_VALID(ship))
    {
        act("You aren't even on a vessel.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
                     "         ship crew unassign <role>\n\r"
                     "         ship crew roster\n\r", ch);
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

    if( !str_prefix(arg, "roster") )
    {
        /* Full crew roster with module + role assignments */
        BUFFER *buffer = new_buf();

        add_buf(buffer, "{W=== Crew Roster ==={x\n\r\n\r");

        /* Traditional roles */
        add_buf(buffer, "{CTraditional Roles:{x\n\r");
        char buf[MSL];

        if (IS_VALID(ship->first_mate))
            sprintf(buf, "  {YFirst Mate:{x  %s\n\r", ship->first_mate->short_descr);
        else
            sprintf(buf, "  {YFirst Mate:{x  {D(vacant){x\n\r");
        add_buf(buffer, buf);

        if (IS_VALID(ship->navigator))
            sprintf(buf, "  {YNavigator:{x   %s\n\r", ship->navigator->short_descr);
        else
            sprintf(buf, "  {YNavigator:{x   {D(vacant){x\n\r");
        add_buf(buffer, buf);

        if (IS_VALID(ship->scout))
            sprintf(buf, "  {YScout:{x       %s\n\r", ship->scout->short_descr);
        else
            sprintf(buf, "  {YScout:{x       {D(vacant){x\n\r");
        add_buf(buffer, buf);

        int oarsmen_count = ship->oarsmen ? list_size(ship->oarsmen) : 0;
        sprintf(buf, "  {YOarsmen:{x     {W%d{x assigned\n\r", oarsmen_count);
        add_buf(buffer, buf);

        /* Module assignments */
        if (ship->modules && list_size(ship->modules) > 0) {
            add_buf(buffer, "\n\r{CModule Stations:{x\n\r");
            add_buf(buffer, "{C Slot  Module                     Operators   Status{x\n\r");
            add_buf(buffer, "{C==========================================================={x\n\r");

            ITERATOR it;
            SHIP_MODULE *mod;
            iterator_start(&it, ship->modules);
            while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
                if (!mod->index) continue;

                int assigned = mod->assigned_crew ? list_size(mod->assigned_crew) : 0;
                const char *status;
                if (!mod->active)
                    status = "{DDisabled{x";
                else if (mod->condition <= 0)
                    status = "{RDestroyed{x";
                else if (!mod->operational)
                    status = "{YOffline{x";
                else
                    status = "{GOnline{x";

                sprintf(buf, " {W%3d  {Y%-25s  {W%d{x/{W%d{x        %s\n\r",
                    mod->slot_id,
                    mod->index->name ? mod->index->name : "(unnamed)",
                    assigned, mod->index->operators,
                    status);
                add_buf(buffer, buf);

                /* List assigned crew */
                if (assigned > 0) {
                    ITERATOR crew_it;
                    CHAR_DATA *crew;
                    iterator_start(&crew_it, mod->assigned_crew);
                    while ((crew = (CHAR_DATA *)iterator_nextdata(&crew_it))) {
                        sprintf(buf, "        {C-{x %s\n\r", crew->short_descr);
                        add_buf(buffer, buf);
                    }
                    iterator_stop(&crew_it);
                }
            }
            iterator_stop(&it);
        }

        /* Unassigned crew */
        if (ship->crew && list_size(ship->crew) > 0) {
            int unassigned = 0;
            BUFFER *ubuf = new_buf();
            ITERATOR it;
            CHAR_DATA *crew;
            int idx = 0;

            iterator_start(&it, ship->crew);
            while ((crew = (CHAR_DATA *)iterator_nextdata(&it))) {
                idx++;
                bool has_role = false;

                if (ship->first_mate == crew || ship->navigator == crew ||
                    ship->scout == crew || list_hasdata(ship->oarsmen, crew))
                    has_role = true;

                if (!has_role && ship->modules) {
                    ITERATOR mod_it;
                    SHIP_MODULE *mod;
                    iterator_start(&mod_it, ship->modules);
                    while ((mod = (SHIP_MODULE *)iterator_nextdata(&mod_it))) {
                        if (mod->assigned_crew && list_hasdata(mod->assigned_crew, crew)) {
                            has_role = true;
                            break;
                        }
                    }
                    iterator_stop(&mod_it);
                }

                if (!has_role) {
                    sprintf(buf, "  {C%2d){x %s\n\r", idx, crew->short_descr);
                    add_buf(ubuf, buf);
                    unassigned++;
                }
            }
            iterator_stop(&it);

            if (unassigned > 0) {
                add_buf(buffer, formatf("\n\r{CUnassigned Crew ({W%d{C):{x\n\r", unassigned));
                add_buf(buffer, buf_string(ubuf));
            }
            free_buf(ubuf);
        }

        add_buf(buffer, "\n\r");
        page_to_char(buf_string(buffer), ch);
        free_buf(buffer);
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

        /* Show module assignments */
        if (ship->modules && list_size(ship->modules) > 0) {
            ITERATOR mod_it;
            SHIP_MODULE *mod;
            iterator_start(&mod_it, ship->modules);
            while ((mod = (SHIP_MODULE *)iterator_nextdata(&mod_it))) {
                if (mod->assigned_crew && list_hasdata(mod->assigned_crew, crew)) {
                    SHIP_HARDPOINT_DEF *hp = ship_get_hardpoint(ship->index, mod->slot_id);
                    sprintf(buf, "{CAssigned to module slot {W%d{C ({Y%s{C - %s).{x\n\r",
                        mod->slot_id,
                        hp && hp->name ? hp->name : "unnamed",
                        mod->index ? mod->index->name : "unknown");
                    send_to_char(buf, ch);
                }
            }
            iterator_stop(&mod_it);
        }

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
            list_remlink(ship->oarsmen, crew, false);
            send_to_char("{WOarsman unassigned due to crew removal.{x\n\r", ch);
        }

        /* Remove from any module assignments */
        if (ship->modules) {
            ITERATOR mod_it;
            SHIP_MODULE *mod;
            iterator_start(&mod_it, ship->modules);
            while ((mod = (SHIP_MODULE *)iterator_nextdata(&mod_it))) {
                if (mod->assigned_crew && list_hasdata(mod->assigned_crew, crew)) {
                    list_remlink(mod->assigned_crew, crew, false);
                    send_to_char(formatf("{WModule slot %d unassigned due to crew removal.{x\n\r",
                        mod->slot_id), ch);
                }
            }
            iterator_stop(&mod_it);
            ship_recalc_modules(ship);
        }

        list_remlink(ship->crew, crew, false);
        crew->belongs_to_ship = NULL;

        extract_char(crew, true);

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
            send_to_char("{YValid roles: {Wfirstmate{x, {Wnavigator{x, {Wscout{x, {Woarsman{x, {Wmodule <slot#>{x\n\r", ch);
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

        if( !str_prefix(argument, "module") )
        {
            char arg_slot[MIL];
            one_argument(argument + 6, arg_slot); /* skip "module" prefix match */

            /* Re-parse: argument after "module" should be slot# */
            char *mod_arg = argument;
            while (*mod_arg && !isspace(*mod_arg)) mod_arg++;
            while (*mod_arg && isspace(*mod_arg)) mod_arg++;

            if (mod_arg[0] == '\0' || !is_number(mod_arg)) {
                send_to_char("Syntax: ship crew assign <crew#> module <slot#>\n\r", ch);
                return;
            }

            int slot_id = atoi(mod_arg);
            SHIP_MODULE *mod = ship_get_module_in_slot(ship, slot_id);
            if (!mod || !mod->index) {
                send_to_char("No module is installed in that slot.\n\r", ch);
                return;
            }

            if (list_hasdata(mod->assigned_crew, crew)) {
                send_to_char("That crew member is already assigned to this module.\n\r", ch);
                return;
            }

            if (mod->index->operators > 0 &&
                list_size(mod->assigned_crew) >= mod->index->operators) {
                send_to_char(formatf("That module already has its full complement of %d operators.\n\r",
                    mod->index->operators), ch);
                return;
            }

            list_appendlink(mod->assigned_crew, crew);
            ship_recalc_modules(ship);

            SHIP_HARDPOINT_DEF *hp = ship_get_hardpoint(ship->index, slot_id);
            send_to_char(formatf("Crew member assigned to module slot %d (%s - %s).%s\n\r",
                slot_id,
                hp && hp->name ? hp->name : "unnamed",
                mod->index->name,
                mod->operational ? " {GModule is now operational.{x" : ""), ch);
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
            send_to_char("{YValid roles: {Wfirstmate{x, {Wnavigator{x, {Wscout{x, {Woarsman <#>{x, {Wmodule <slot#>{x\n\r", ch);
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

            list_remlink(ship->oarsmen, crew, false);
            send_to_char("Oarsman unassigned.\n\r", ch);
            return;
        }

        if( !str_prefix(arg2, "module") )
        {
            if (!is_number(argument)) {
                send_to_char("Syntax: ship crew unassign module <slot#> [crew#]\n\r", ch);
                return;
            }

            char arg3[MIL];
            argument = one_argument(argument, arg3);
            int slot_id = atoi(arg3);

            SHIP_MODULE *mod = ship_get_module_in_slot(ship, slot_id);
            if (!mod || !mod->index) {
                send_to_char("No module is installed in that slot.\n\r", ch);
                return;
            }

            if (list_size(mod->assigned_crew) == 0) {
                send_to_char("No crew is assigned to that module.\n\r", ch);
                return;
            }

            /* If crew# given, remove specific; otherwise remove all */
            if (is_number(argument)) {
                int crew_idx = atoi(argument);
                if (crew_idx < 1 || crew_idx > list_size(ship->crew)) {
                    send_to_char("That is not a valid crew member.\n\r", ch);
                    return;
                }
                CHAR_DATA *crew = (CHAR_DATA *)list_nthdata(ship->crew, crew_idx);
                if (!list_hasdata(mod->assigned_crew, crew)) {
                    send_to_char("That crew member is not assigned to this module.\n\r", ch);
                    return;
                }
                list_remlink(mod->assigned_crew, crew, false);
                send_to_char(formatf("Crew member unassigned from module slot %d.\n\r", slot_id), ch);
            } else {
                list_clear(mod->assigned_crew);
                send_to_char(formatf("All crew unassigned from module slot %d.\n\r", slot_id), ch);
            }

            ship_recalc_modules(ship);
            return;
        }

        do_ship_crew(ch, "unassign");
        return;
    }

    do_ship_crew(ch, "");
}

/**
 * do_ship - Main ship command dispatcher
 *
 * Entry point for all ship-related player commands. Dispatches to
 * the appropriate subcommand handler based on the first argument.
 *
 * Subcommands:
 * - aim: Target ship weapons (NYI)
 * - chase: Pursue another ship (NYI)
 * - christen: Name the ship
 * - crew: Manage crew assignments
 * - engines: Airship throttle control
 * - flag: Set ship flag design
 * - keys: Manage special keys
 * - land/launch: Airship vertical movement
 * - list: Show owned ships
 * - navigate: Automated navigation
 * - oars: Rowing control
 * - routes: Manage saved routes
 * - sails: Sailboat speed control
 * - scuttle: Destroy ship
 * - steer: Direction control
 * - waypoints: Manage navigation waypoints
 *
 * @param ch        Player issuing ship command
 * @param argument  Subcommand and parameters
 */
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
                     "         ship install <module_vnum> <slot#>\n\r"
                     "         ship keys[ <actions>]\n\r"
                     "         ship land {W(airship only){x\n\r"
                     "         ship launch {W(airship only){x\n\r"
                     "         ship list\n\r"
                     "         ship modules\n\r"
                     "         ship navigate[ <action>]\n\r"
                     "         ship oars[ <oars>]\n\r"
                     "         ship repair <slot#>\n\r"
                     "         ship routes[ <action>]\n\r"
                     "         ship sails[ <level>] {W(sailboat only){x\n\r"
                     "         ship scuttle\n\r"
                     "         ship status\n\r"
                     "         ship steer[ <heading>[ <turn direction>]]\n\r"
                     "         ship uninstall <slot#>\n\r"
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

    if( !str_prefix(arg, "install") )
    {
        do_ship_install(ch, argument);
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

    if( !str_prefix(arg, "modules") )
    {
        do_ship_modules(ch, argument);
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

    if( !str_prefix(arg, "repair") )
    {
        do_ship_repair(ch, argument);
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

    if( !str_prefix(arg, "status") )
    {
        do_ship_status(ch, argument);
        return;
    }

    if( !str_prefix(arg, "steer") )
    {
        do_ship_steer(ch, argument);
        return;
    }

    if( !str_prefix(arg, "uninstall") )
    {
        do_ship_uninstall(ch, argument);
        return;
    }

    if( !str_prefix(arg, "waypoints") )
    {
        do_ship_waypoints(ch, argument);
        return;
    }

    do_ship(ch, "");
}

/**
 * find_ship_uid - Find a ship by UID parts
 *
 * Searches loaded_ships for a ship matching the given UID.
 *
 * @param id1  First UID component
 * @param id2  Second UID component
 * @return     Matching ship or NULL
 */
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

/**
 * get_owned_ship - Find a ship owned by a character by name or number
 *
 * Searches for a ship matching the argument by name or index.
 * Supports N.name syntax for multiple ships with same name.
 *
 * @param ch        Character whose ships to search (unused for PCs)
 * @param argument  Ship name or number
 * @return          Matching ship or NULL
 */
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


/**
 * _is_terrain_land - Check if coordinates are land terrain
 *
 * Helper function to check if a wilderness coordinate is land
 * (not water).
 *
 * @param wilds  Wilderness to check
 * @param x      X coordinate
 * @param y      Y coordinate
 * @return       true if land, false if water/invalid
 */
bool _is_terrain_land(WILDS_DATA *wilds, int x, int y)
{
    if(!wilds) return false;

    if( x < 0 || x >= wilds->map_size_x ) return false;
    if( y < 0 || y >= wilds->map_size_y ) return false;

    WILDS_TERRAIN *terrain = get_terrain_by_coors(wilds, x, y);

    return ( terrain && !terrain->nonroom &&
        !room_in_sector(terrain->template, SECT_WATER_NOSWIM) &&
        !room_in_sector(terrain->template, SECT_WATER_SWIM));
}

/**
 * is_shipyard_valid - Check if an area can serve as a shipyard
 *
 * Verifies that the rectangular area contains at least one SAFE_HARBOR
 * water tile adjacent to land. Does not verify navigability to open water.
 *
 * @param wuid  Wilderness UID
 * @param x1    Min X coordinate of area
 * @param y1    Min Y coordinate of area
 * @param x2    Max X coordinate of area
 * @param y2    Max Y coordinate of area
 * @return      true if valid shipyard location exists
 */
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
                (room_in_sector(terrain->template, SECT_WATER_NOSWIM) ||
                 room_in_sector(terrain->template, SECT_WATER_SWIM)) &&
                IS_SET(terrain->template->room_flag[1], ROOM_SAFE_HARBOR) )
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


/**
 * get_shipyard_location - Find a random valid spawn point in a shipyard
 *
 * Picks a random SAFE_HARBOR water tile adjacent to land within
 * the shipyard bounds. Used for spawning newly built ships.
 *
 * @param wuid  Wilderness UID
 * @param x1    Min X of shipyard area
 * @param y1    Min Y of shipyard area
 * @param x2    Max X of shipyard area
 * @param y2    Max Y of shipyard area
 * @param x     Output: spawn X coordinate
 * @param y     Output: spawn Y coordinate
 * @return      true if valid location found, false otherwise
 */
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
            (room_in_sector(terrain->template, SECT_WATER_NOSWIM) ||
             room_in_sector(terrain->template, SECT_WATER_SWIM)) &&
            IS_SET(terrain->template->room_flag[1], ROOM_SAFE_HARBOR) )
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

/**
 * purchase_ship - Create and place a ship purchased from a shop
 *
 * Called when a player buys a ship from a shipyard shop. Creates
 * the ship, assigns ownership, adds to player's ship list, and
 * places it at a random valid location in the shop's shipyard area.
 *
 * @param ch     Player purchasing the ship
 * @param vnum   Ship template VNUM to create
 * @param shop   Shop with shipyard configuration
 * @return       New ship, or NULL on failure
 */
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

/**
 * ships_player_owned - Count ships owned by a player
 *
 * Returns the number of ships owned by the given player, optionally
 * filtered by ship template type.
 *
 * @param ch     Player to count ships for
 * @param index  Ship template to filter by (NULL for all ships)
 * @return       Number of matching ships
 */
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
// NPC Ships (placeholder section)
//

/////////////////////////////////////////////////////////////////
//
// Ship Edit (OLC Editor for Ship Templates)
//

/* shedit_table moved to editors/ships/shedit.c */

/**
 * can_edit_ships - Check if character has ship template editing permissions
 *
 * Requires security level 9 and maximum total level.
 *
 * @param ch  Character to check
 * @return    true if can edit ship templates
 */
bool can_edit_ships(CHAR_DATA *ch)
{
    return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}

/**
 * list_ship_indexes - Display all ship templates
 *
 * Shows a formatted list of all SHIP_INDEX_DATA entries with
 * VNUM, name, and ship class. Supports area filtering.
 *
 * @param ch        Staff member viewing list
 * @param argument  Optional area name
 */
void list_ship_indexes(CHAR_DATA *ch, char *argument)
{
    if( !can_edit_ships(ch) )
    {
        send_to_char("You do not have access to ships.\n\r", ch);
        return;
    }

    AREA_DATA *pArea = NULL;
    char arg[MAX_INPUT_LENGTH];
    
    argument = one_argument(argument, arg);
    
    if (arg[0] != '\0')
    {
        if ((pArea = find_area(arg)))
        {
            // Use specified area
        }
        else
        {
            send_to_char("No such area.\n\r", ch);
            return;
        }
    }
    else
    {
        pArea = ch->in_room->area;
    }

    if(!ch->lines)
        send_to_char("{RWARNING:{W Having scrolling off may limit how many ships you can see.{x\n\r", ch);

    int lines = 0;
    bool error = false;
    BUFFER *buffer = new_buf();
    char buf[MSL];
    int iHash;

    for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
    {
        for(SHIP_INDEX_DATA *ship = pArea->ship_index_hash[iHash]; ship; ship = ship->next)
        {
            sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s  {G%-16.16s{x \n\r",
                ship->vnum,
                ship->name,
                flag_string(ship_class_types, ship->ship_class));

            ++lines;
            if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
            {
                error = true;
                break;
            }
        }
        if (error) break;
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

/**
 * do_shlist - Staff command to list all ship templates
 *
 * @param ch        Staff member
 * @param argument  Filter (passed to list_ship_indexes)
 */
void do_shlist(CHAR_DATA *ch, char *argument)
{
    list_ship_indexes(ch, argument);
}

/* shedit() moved to editors/ships/shedit.c */
/* do_shedit() moved to editors/ships/shedit.c */
/* do_shshow() moved to editors/ships/shedit.c */

/* smedit() moved to editors/ships/smedit.c */
/* do_smedit() moved to editors/ships/smedit.c */


/***************************************************************************
 * Module System — Core Logic                                              *
 ***************************************************************************/

/**
 * ship_module_is_operational - Check if an installed module is operational
 *
 * A module is operational when:
 * - It is valid and active
 * - Its condition is above 0
 * - It has enough assigned crew meeting minimum skill thresholds
 *
 * Updates the module's cached 'operational' flag.
 *
 * @param mod   Module instance to check
 * @return      true if module is operational
 */
bool ship_module_is_operational(SHIP_MODULE *mod)
{
    if (!IS_VALID(mod) || !mod->index)
        return (mod->operational = false);

    if (!mod->active || mod->condition <= 0)
        return (mod->operational = false);

    /* Check crew count */
    int crew_count = mod->assigned_crew ? list_size(mod->assigned_crew) : 0;
    if (crew_count < mod->index->operators)
        return (mod->operational = false);

    /* Check crew skill thresholds */
    if (mod->index->operators > 0 && mod->assigned_crew) {
        ITERATOR it;
        CHAR_DATA *crew;
        iterator_start(&it, mod->assigned_crew);
        while ((crew = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (!crew->crew) {
                iterator_stop(&it);
                return (mod->operational = false);
            }

            if (mod->index->req_gunning > 0 &&
                crew->crew->gunning < mod->index->req_gunning) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
            if (mod->index->req_mechanics > 0 &&
                crew->crew->mechanics < mod->index->req_mechanics) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
            if (mod->index->req_scouting > 0 &&
                crew->crew->scouting < mod->index->req_scouting) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
            if (mod->index->req_navigation > 0 &&
                crew->crew->navigation < mod->index->req_navigation) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
            if (mod->index->req_oarring > 0 &&
                crew->crew->oarring < mod->index->req_oarring) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
            if (mod->index->req_leadership > 0 &&
                crew->crew->leadership < mod->index->req_leadership) {
                iterator_stop(&it);
                return (mod->operational = false);
            }
        }
        iterator_stop(&it);
    }

    return (mod->operational = true);
}

/**
 * ship_recalc_modules - Recalculate ship stats from base template + modules
 *
 * Resets runtime stats to base template values, then adds bonuses from
 * all operational modules. Called after install/remove/repair/crew changes.
 *
 * Affected stats: hit, armor, max_crew, total_module_weight.
 * Speed/turning/cargo are not runtime fields yet, so bonuses are tracked
 * but applied at point-of-use via ship_get_effective_* helpers.
 *
 * @param ship  Ship to recalculate
 */
void ship_recalc_modules(SHIP_DATA *ship)
{
    if (!IS_VALID(ship) || !ship->index)
        return;

    SHIP_INDEX_DATA *idx = ship->index;

    /* Reset to base template values */
    long base_hit = idx->hit;
    long base_armor = idx->armor;
    int base_max_crew = idx->max_crew;

    int total_weight = 0;

    /* First pass: update operational status for every module */
    if (ship->modules) {
        ITERATOR it;
        SHIP_MODULE *mod;
        iterator_start(&it, ship->modules);
        while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
            ship_module_is_operational(mod);
            if (mod->index)
                total_weight += mod->index->weight;
        }
        iterator_stop(&it);
    }

    ship->total_module_weight = total_weight;

    /* Second pass: accumulate bonuses from operational modules */
    if (ship->modules) {
        ITERATOR it;
        SHIP_MODULE *mod;
        iterator_start(&it, ship->modules);
        while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
            if (!mod->operational || !mod->index)
                continue;

            base_hit   += mod->index->hit_bonus;
            base_armor += mod->index->armor_bonus;
            base_max_crew += mod->index->crew_bonus;
        }
        iterator_stop(&it);
    }

    /* Clamp and apply */
    ship->hit = UMAX(1, base_hit);
    ship->armor = UMAX(0, base_armor);
    ship->max_crew = (int16_t)UMAX(0, base_max_crew);
}

/**
 * ship_get_effective_speed - Get ship speed including module bonuses
 *
 * Calculates the effective speed modifier from all operational modules.
 * Returns the total speed bonus percentage (positive = faster).
 *
 * @param ship  Ship to check
 * @return      Speed bonus percentage (e.g. 30 = +30% speed)
 */
int ship_get_effective_speed(SHIP_DATA *ship)
{
    int bonus = 0;

    if (!IS_VALID(ship) || !ship->modules)
        return 0;

    ITERATOR it;
    SHIP_MODULE *mod;
    iterator_start(&it, ship->modules);
    while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
        if (mod->operational && mod->index)
            bonus += mod->index->speed_bonus;
    }
    iterator_stop(&it);

    return bonus;
}

/**
 * ship_get_effective_turning - Get ship turning including module bonuses
 *
 * @param ship  Ship to check
 * @return      Turning bonus (added to base turning degrees)
 */
int ship_get_effective_turning(SHIP_DATA *ship)
{
    int bonus = 0;

    if (!IS_VALID(ship) || !ship->modules)
        return 0;

    ITERATOR it;
    SHIP_MODULE *mod;
    iterator_start(&it, ship->modules);
    while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
        if (mod->operational && mod->index)
            bonus += mod->index->turning_bonus;
    }
    iterator_stop(&it);

    return bonus;
}

/**
 * ship_get_module_in_slot - Find installed module in a hardpoint slot
 *
 * @param ship     Ship to search
 * @param slot_id  Hardpoint slot number
 * @return         Module in that slot, or NULL
 */
SHIP_MODULE *ship_get_module_in_slot(SHIP_DATA *ship, int slot_id)
{
    if (!IS_VALID(ship) || !ship->modules)
        return NULL;

    ITERATOR it;
    SHIP_MODULE *mod;
    iterator_start(&it, ship->modules);
    while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
        if (mod->slot_id == slot_id) {
            iterator_stop(&it);
            return mod;
        }
    }
    iterator_stop(&it);
    return NULL;
}

/**
 * ship_get_hardpoint - Find a hardpoint definition on a ship template
 *
 * @param idx      Ship index data
 * @param slot_id  Slot number to find
 * @return         Hardpoint definition, or NULL
 */
SHIP_HARDPOINT_DEF *ship_get_hardpoint(SHIP_INDEX_DATA *idx, int slot_id)
{
    if (!idx || !idx->hardpoints)
        return NULL;

    ITERATOR it;
    SHIP_HARDPOINT_DEF *hp;
    iterator_start(&it, idx->hardpoints);
    while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&it))) {
        if (hp->slot_id == slot_id) {
            iterator_stop(&it);
            return hp;
        }
    }
    iterator_stop(&it);
    return NULL;
}

/***************************************************************************
 * Player Ship Module Commands                                             *
 ***************************************************************************/

/**
 * do_ship_modules - Display installed modules and hardpoint status
 *
 * Shows a table of all hardpoint slots on the ship, what module (if any)
 * is installed in each, and the module's condition and operational status.
 *
 * @param ch        Player viewing
 * @param argument  Unused
 */
void do_ship_modules(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    BUFFER *buffer;

    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    if (!ship->index || !ship->index->hardpoints ||
        list_size(ship->index->hardpoints) == 0) {
        send_to_char("This vessel has no hardpoint slots.\n\r", ch);
        return;
    }

    buffer = new_buf();
    char buf[MSL];

    sprintf(buf, "{WModule Loadout for %s{x  (Weight: {Y%d{x/{W%d{x)\n\r",
        ship->ship_name ? ship->ship_name : ship->index->name,
        ship->total_module_weight, ship->index->max_module_weight);
    add_buf(buffer, buf);

    add_buf(buffer, "{C Slot  Hardpoint Name         Type        Size     Module                    Cond   Status{x\n\r");
    add_buf(buffer, "{C=============================================================================================={x\n\r");

    ITERATOR it;
    SHIP_HARDPOINT_DEF *hp;
    iterator_start(&it, ship->index->hardpoints);
    while ((hp = (SHIP_HARDPOINT_DEF *)iterator_nextdata(&it))) {
        SHIP_MODULE *mod = ship_get_module_in_slot(ship, hp->slot_id);

        const char *type_str = flag_string(hardpoint_types, hp->type);
        const char *size_str = flag_string(hardpoint_sizes, hp->size);

        if (mod && mod->index) {
            const char *status;
            if (!mod->active)
                status = "{DDisabled{x";
            else if (mod->condition <= 0)
                status = "{RDestroyed{x";
            else if (!mod->operational)
                status = "{YOffline{x";
            else
                status = "{GOnline{x";

            sprintf(buf, " {W%3d  {Y%-20s {C%-10s {M%-8s {G%-25s {W%3d%%  %s{x\n\r",
                hp->slot_id,
                hp->name ? hp->name : "(unnamed)",
                type_str, size_str,
                mod->index->name ? mod->index->name : "(unnamed)",
                mod->max_condition > 0 ? (mod->condition * 100) / mod->max_condition : 0,
                status);
        } else {
            sprintf(buf, " {W%3d  {Y%-20s {C%-10s {M%-8s {D%-25s  ---  ---{x\n\r",
                hp->slot_id,
                hp->name ? hp->name : "(unnamed)",
                type_str, size_str,
                IS_SET(hp->flags, HARDPOINT_REQUIRED) ? "(REQUIRED - empty)" : "(empty)");
        }
        add_buf(buffer, buf);
    }
    iterator_stop(&it);

    add_buf(buffer, "{C=============================================================================================={x\n\r");
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}

/**
 * do_ship_install - Install a module into a hardpoint slot
 *
 * Requires the ship to be in safe harbor. The module is specified by
 * its vnum and the target hardpoint slot number.
 *
 * Validates: type match, size compatibility, domain compatibility,
 * weight budget, and slot availability.
 *
 * Syntax: ship install <module_vnum> <slot#>
 *
 * @param ch        Ship owner
 * @param argument  "<module_vnum> <slot#>"
 */
void do_ship_install(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    char arg_vnum[MIL], arg_slot[MIL];

    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch)) {
        send_to_char("This isn't your vessel.\n\r", ch);
        return;
    }

    /* Must be in safe harbor */
    if (!IS_IMMORTAL(ch) && !is_ship_safe(ch, ship, NULL)) {
        send_to_char("You can only install modules while in safe harbor.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg_vnum);
    argument = one_argument(argument, arg_slot);

    if (arg_vnum[0] == '\0' || arg_slot[0] == '\0') {
        send_to_char("Syntax: ship install <module_vnum> <slot#>\n\r", ch);
        return;
    }

    /* Parse module vnum */
    WNUM mod_wnum;
    if (!parse_widevnum(arg_vnum, ch->in_room->area, &mod_wnum)) {
        send_to_char("Invalid module vnum format.\n\r", ch);
        return;
    }

    SHIP_MODULE_INDEX *mod_idx = get_ship_module_index_for_area(mod_wnum.pArea, mod_wnum.vnum);
    if (!mod_idx) {
        /* Try global search as fallback */
        mod_idx = get_ship_module_index(mod_wnum.vnum);
    }
    if (!mod_idx) {
        send_to_char("That module template does not exist.\n\r", ch);
        return;
    }

    if (!is_number(arg_slot)) {
        send_to_char("Slot must be a number.\n\r", ch);
        return;
    }

    int slot_id = atoi(arg_slot);
    SHIP_HARDPOINT_DEF *hp = ship_get_hardpoint(ship->index, slot_id);
    if (!hp) {
        send_to_char("That hardpoint slot does not exist on this vessel.\n\r", ch);
        return;
    }

    /* Type must match */
    if (mod_idx->type != hp->type) {
        send_to_char(formatf("Module type (%s) does not match hardpoint type (%s).\n\r",
            flag_string(hardpoint_types, mod_idx->type),
            flag_string(hardpoint_types, hp->type)), ch);
        return;
    }

    /* Size must fit */
    if (mod_idx->size > hp->size) {
        send_to_char(formatf("Module is too large (%s) for this slot (%s).\n\r",
            flag_string(hardpoint_sizes, mod_idx->size),
            flag_string(hardpoint_sizes, hp->size)), ch);
        return;
    }

    /* Domain compatibility */
    if (hp->domain_flags && mod_idx->domain_flags &&
        !(hp->domain_flags & mod_idx->domain_flags)) {
        send_to_char("This module is not compatible with this slot's domain.\n\r", ch);
        return;
    }

    /* Slot must be empty */
    if (ship_get_module_in_slot(ship, slot_id)) {
        send_to_char("That hardpoint slot already has a module installed. Uninstall it first.\n\r", ch);
        return;
    }

    /* Weight budget check */
    if (ship->total_module_weight + mod_idx->weight > ship->index->max_module_weight) {
        send_to_char(formatf("Not enough weight budget. Module: %d, Available: %d.\n\r",
            mod_idx->weight,
            ship->index->max_module_weight - ship->total_module_weight), ch);
        return;
    }

    /* Create and install the module */
    SHIP_MODULE *mod = new_ship_module();
    mod->index = mod_idx;
    mod->slot_id = (int16_t)slot_id;
    mod->condition = 100;
    mod->max_condition = 100;
    mod->active = true;

    /* Set initial ammo if weapon with ammo */
    if (mod_idx->ammo && IS_SET(mod_idx->flags, MODULE_REQUIRES_AMMO)) {
        mod->ammo_count = 0; /* Player must load ammo separately */
    }

    list_appendlink(ship->modules, mod);

    ship_recalc_modules(ship);

    send_to_char(formatf("{G%s{x installed in slot {W%d{x ({Y%s{x).\n\r",
        mod_idx->name, slot_id, hp->name ? hp->name : "unnamed"), ch);
}

/**
 * do_ship_uninstall - Remove a module from a hardpoint slot
 *
 * Requires safe harbor. Removes the module and recalculates stats.
 *
 * Syntax: ship uninstall <slot#>
 *
 * @param ch        Ship owner
 * @param argument  Slot number
 */
void do_ship_uninstall(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);

    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch)) {
        send_to_char("This isn't your vessel.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !is_ship_safe(ch, ship, NULL)) {
        send_to_char("You can only uninstall modules while in safe harbor.\n\r", ch);
        return;
    }

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: ship uninstall <slot#>\n\r", ch);
        return;
    }

    int slot_id = atoi(argument);
    SHIP_MODULE *mod = ship_get_module_in_slot(ship, slot_id);
    if (!mod) {
        send_to_char("No module is installed in that slot.\n\r", ch);
        return;
    }

    SHIP_HARDPOINT_DEF *hp = ship_get_hardpoint(ship->index, slot_id);

    const char *mod_name = (mod->index && mod->index->name) ? mod->index->name : "module";

    /* Remove crew assignments */
    if (mod->assigned_crew && list_size(mod->assigned_crew) > 0) {
        list_clear(mod->assigned_crew);
    }

    list_remlink(ship->modules, mod, true);

    ship_recalc_modules(ship);

    send_to_char(formatf("{Y%s{x removed from slot {W%d{x ({Y%s{x).\n\r",
        mod_name, slot_id,
        (hp && hp->name) ? hp->name : "unnamed"), ch);
}

/**
 * do_ship_repair - Repair a damaged module
 *
 * Requires a crew member with mechanics skill assigned to the module
 * (or any crew with mechanics if no one is assigned). Repairs a fixed
 * amount of condition per use. Cannot repair destroyed modules (condition 0)
 * without a repair station utility module.
 *
 * Syntax: ship repair <slot#>
 *
 * @param ch        Ship owner or crew manager
 * @param argument  Slot number
 */
void do_ship_repair(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);

    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    if (!IS_IMMORTAL(ch) && !ship_isowner_player(ship, ch)) {
        send_to_char("This isn't your vessel.\n\r", ch);
        return;
    }

    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: ship repair <slot#>\n\r", ch);
        return;
    }

    int slot_id = atoi(argument);
    SHIP_MODULE *mod = ship_get_module_in_slot(ship, slot_id);
    if (!mod || !mod->index) {
        send_to_char("No module is installed in that slot.\n\r", ch);
        return;
    }

    if (mod->condition >= mod->max_condition) {
        send_to_char("That module is already at full condition.\n\r", ch);
        return;
    }

    /* Find a mechanic crew member */
    bool has_mechanic = false;
    int best_mechanics = 0;

    if (ship->crew) {
        ITERATOR it;
        CHAR_DATA *crew;
        iterator_start(&it, ship->crew);
        while ((crew = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (crew->crew && crew->crew->mechanics > 0) {
                has_mechanic = true;
                if (crew->crew->mechanics > best_mechanics)
                    best_mechanics = crew->crew->mechanics;
            }
        }
        iterator_stop(&it);
    }

    if (!has_mechanic && !IS_IMMORTAL(ch)) {
        send_to_char("You need a crew member with mechanics skill to repair modules.\n\r", ch);
        return;
    }

    /* Repair amount: base 10 + mechanics skill */
    int repair_amount = 10 + best_mechanics * 2;
    mod->condition = UMIN(mod->max_condition, mod->condition + repair_amount);

    ship_recalc_modules(ship);

    send_to_char(formatf("{G%s{x repaired to {W%d%%{x condition.\n\r",
        mod->index->name,
        mod->max_condition > 0 ? (mod->condition * 100) / mod->max_condition : 0), ch);
}

/**
 * do_ship_status - Display comprehensive ship status
 *
 * Shows hull HP, armor, combat state, module summary, crew, and
 * weight/capacity information.
 *
 * @param ch        Player viewing
 * @param argument  Unused
 */
void do_ship_status(CHAR_DATA *ch, char *argument)
{
    SHIP_DATA *ship = get_room_ship(ch->in_room);
    BUFFER *buffer;

    if (!IS_VALID(ship)) {
        send_to_char("You aren't on a vessel.\n\r", ch);
        return;
    }

    buffer = new_buf();
    char buf[MSL];

    sprintf(buf, "\n\r{W=== Ship Status: %s ==={x\n\r\n\r",
        ship->ship_name ? ship->ship_name : ship->index->name);
    add_buf(buffer, buf);

    /* Hull & Defense */
    int hp_pct = (ship->index->hit > 0) ? (int)((ship->hit * 100) / ship->index->hit) : 100;
    const char *hp_color = (hp_pct > 75) ? "{G" : (hp_pct > 50) ? "{Y" : (hp_pct > 25) ? "{y" : "{R";
    sprintf(buf, "  {CHull:{x     %s%ld{x / {W%d{x  (%s%d%%{x)\n\r",
        hp_color, ship->hit, ship->index->hit, hp_color, hp_pct);
    add_buf(buffer, buf);

    sprintf(buf, "  {CArmor:{x    {W%ld{x (base: %d)\n\r", ship->armor, ship->index->armor);
    add_buf(buffer, buf);

    sprintf(buf, "  {CSpeed:{x    {W%d{x power (module bonus: {Y%+d%%{x)\n\r",
        ship->ship_power + ship->oar_power, ship_get_effective_speed(ship));
    add_buf(buffer, buf);

    sprintf(buf, "  {CTurning:{x  {W%d{x deg/step (module bonus: {Y%+d{x)\n\r",
        ship->index->turning, ship_get_effective_turning(ship));
    add_buf(buffer, buf);

    /* Crew */
    int crew_count = ship->crew ? list_size(ship->crew) : 0;
    sprintf(buf, "  {CCrew:{x     {W%d{x / %d-%d\n\r",
        crew_count, ship->min_crew, ship->max_crew);
    add_buf(buffer, buf);

    /* Module summary */
    if (ship->modules && list_size(ship->modules) > 0) {
        int total = list_size(ship->modules);
        int operational = 0;
        int weapons = 0;
        int weapons_ready = 0;

        ITERATOR it;
        SHIP_MODULE *mod;
        iterator_start(&it, ship->modules);
        while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
            if (mod->operational) operational++;
            if (mod->index && mod->index->type == HARDPOINT_WEAPON) {
                weapons++;
                if (mod->operational && mod->reload_countdown <= 0)
                    weapons_ready++;
            }
        }
        iterator_stop(&it);

        sprintf(buf, "  {CModules:{x  {W%d{x installed, {G%d{x operational (weight: {Y%d{x/{W%d{x)\n\r",
            total, operational, ship->total_module_weight,
            ship->index->max_module_weight);
        add_buf(buffer, buf);

        if (weapons > 0) {
            sprintf(buf, "  {CWeapons:{x {W%d{x mounted, {G%d{x ready to fire\n\r",
                weapons, weapons_ready);
            add_buf(buffer, buf);
        }
    } else {
        add_buf(buffer, "  {CModules:{x  None installed\n\r");
    }

    /* Combat state */
    if (IS_VALID(ship->ship_attacked)) {
        sprintf(buf, "  {RTarget:{x   %s\n\r",
            ship->ship_attacked->ship_name ? ship->ship_attacked->ship_name : "Unknown vessel");
        add_buf(buffer, buf);
    }

    if (IS_VALID(ship->ship_chased)) {
        sprintf(buf, "  {YChasing:{x  %s\n\r",
            ship->ship_chased->ship_name ? ship->ship_chased->ship_name : "Unknown vessel");
        add_buf(buffer, buf);
    }

    /* Ship flags */
    if (IS_SET(ship->ship_flags, SHIP_SINKING))
        add_buf(buffer, "  {R** VESSEL IS SINKING **{x\n\r");
    if (IS_SET(ship->ship_flags, SHIP_ON_FIRE))
        add_buf(buffer, "  {R** VESSEL IS ON FIRE **{x\n\r");
    if (IS_SET(ship->ship_flags, SHIP_DISABLED))
        add_buf(buffer, "  {R** VESSEL IS DISABLED **{x\n\r");

    add_buf(buffer, "\n\r");
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


/***************************************************************************
 * Ship Combat System                                                      *
 ***************************************************************************/

/**
 * ship_distance - Calculate distance between two ships in wilderness tiles
 *
 * Uses Euclidean distance between ship object positions in the wilderness.
 * Both ships must have valid positions for a meaningful result.
 *
 * @param a  First ship
 * @param b  Second ship
 * @return   Distance in tiles, or INT_MAX if positions invalid
 */
static int ship_distance(SHIP_DATA *a, SHIP_DATA *b)
{
    if (!IS_VALID(a) || !IS_VALID(b)) return INT_MAX;
    if (!a->ship || !a->ship->in_room || !b->ship || !b->ship->in_room)
        return INT_MAX;

    int dx = a->ship->in_room->x - b->ship->in_room->x;
    int dy = a->ship->in_room->y - b->ship->in_room->y;

    return (int)sqrt((double)(dx * dx + dy * dy));
}

/**
 * ship_get_max_weapon_range - Get the maximum weapon range across all operational weapons
 *
 * @param ship  Ship to check
 * @return      Maximum range in tiles, 0 if no operational weapons
 */
static int ship_get_max_weapon_range(SHIP_DATA *ship)
{
    if (!IS_VALID(ship) || !ship->modules) return 0;

    int max_range = 0;
    ITERATOR it;
    SHIP_MODULE *mod;

    iterator_start(&it, ship->modules);
    while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
        if (!mod->index || mod->index->type != HARDPOINT_WEAPON) continue;
        if (!mod->operational) continue;
        if (mod->index->range > max_range)
            max_range = mod->index->range;
    }
    iterator_stop(&it);

    return max_range;
}

/**
 * boat_damage - Apply damage to a ship with module-aware distribution
 *
 * Incoming damage is reduced by armor. Primary hull HP is reduced.
 * There is a chance for each hit to damage installed modules and crew.
 * Threshold effects trigger at 75%, 50%, and 25% hull HP.
 *
 * @param ship    Target ship
 * @param amount  Raw damage before armor
 * @param type    SHIP_DAMAGE_GRIND or SHIP_DAMAGE_FIRE
 */
void boat_damage(SHIP_DATA *ship, long amount, int type)
{
    if (!IS_VALID(ship) || !ship->index) return;

    /* Armor reduces damage (minimum 1) */
    long effective = amount - ship->armor;
    if (effective < 1) effective = 1;

    ship->hit -= effective;

    boat_echo(ship, formatf("{R** The vessel takes {W%ld{R damage! **{x", effective));

    /* Fire damage: ongoing DoT to crew */
    if (type == SHIP_DAMAGE_FIRE && !IS_SET(ship->ship_flags, SHIP_ON_FIRE)) {
        SET_BIT(ship->ship_flags, SHIP_ON_FIRE);
        boat_echo(ship, "{R** Flames erupt across the deck! **{x");
    }

    /* Random chance to damage an installed module */
    if (ship->modules && list_size(ship->modules) > 0 &&
        number_percent() <= SHIP_MODULE_HIT_CHANCE) {
        int mod_count = list_size(ship->modules);
        int target_idx = number_range(1, mod_count);
        SHIP_MODULE *mod = (SHIP_MODULE *)list_nthdata(ship->modules, target_idx);
        if (mod && mod->condition > 0) {
            int mod_dmg = UMAX(1, (int)(effective * number_range(10, 30) / 100));
            mod->condition -= mod_dmg;
            if (mod->condition <= 0) {
                mod->condition = 0;
                mod->operational = false;
                boat_echo(ship, formatf("{R** Module '%s' in slot %d has been destroyed! **{x",
                    mod->index ? mod->index->name : "unknown", mod->slot_id));
            } else {
                boat_echo(ship, formatf("{Y** Module '%s' in slot %d damaged! (%d%% condition) **{x",
                    mod->index ? mod->index->name : "unknown", mod->slot_id,
                    mod->max_condition > 0 ? (mod->condition * 100) / mod->max_condition : 0));
            }
            ship_recalc_modules(ship);
        }
    }

    /* Anti-crew weapons: chance to injure crew */
    /* (Handled per-weapon in boat_fire_weapon, but fire damage hurts all) */
    if (type == SHIP_DAMAGE_FIRE && ship->crew && list_size(ship->crew) > 0) {
        ITERATOR it;
        CHAR_DATA *crew;
        iterator_start(&it, ship->crew);
        while ((crew = (CHAR_DATA *)iterator_nextdata(&it))) {
            if (number_percent() <= 10) {
                int fire_dmg = number_range(10, 30);
                if (crew->hit > fire_dmg) {
                    crew->hit -= fire_dmg;
                } else {
                    /* Crew member killed by fire */
                    boat_echo(ship, formatf("{R** %s has been killed in the blaze! **{x",
                        crew->short_descr));
                    iterator_remcurrent(&it);
                    list_remlink(ship->crew, crew, false);
                    crew->belongs_to_ship = NULL;
                    extract_char(crew, true);
                }
            }
        }
        iterator_stop(&it);
    }

    /* Threshold effects */
    ship_apply_threshold_effects(ship);

    /* Check for destruction */
    if (ship->hit <= 0) {
        ship->hit = 0;
        if (!IS_SET(ship->ship_flags, SHIP_SINKING)) {
            SET_BIT(ship->ship_flags, SHIP_SINKING);
            ship->scuttle_time = SHIP_SINK_COUNTDOWN;

            if (ship->ship_type == SHIP_AIR_SHIP) {
                boat_echo(ship, "{R** THE AIRSHIP IS GOING DOWN! ABANDON SHIP! **{x");
            } else if (ship->ship_type == SHIP_LAND_VESSEL) {
                boat_echo(ship, "{R** THE VESSEL IS BREAKING APART! ABANDON SHIP! **{x");
            } else {
                boat_echo(ship, "{R** THE VESSEL IS SINKING! ABANDON SHIP! **{x");
            }
        }
    }
}

/**
 * ship_apply_threshold_effects - Apply damage threshold effects
 *
 * At various hull HP percentages, the ship suffers escalating penalties:
 * - 75%: cosmetic smoke/flooding
 * - 50%: propulsion damage, fires likely
 * - 25%: critical — crew casualties, module failures
 *
 * @param ship  Ship to check
 */
static void ship_apply_threshold_effects(SHIP_DATA *ship)
{
    if (!IS_VALID(ship) || !ship->index || ship->index->hit <= 0) return;

    int hp_pct = (int)((ship->hit * 100) / ship->index->hit);

    /* 25% — Critical */
    if (hp_pct <= SHIP_DAMAGE_THRESHOLD_CRIT) {
        SET_BIT(ship->ship_flags, SHIP_DISABLED);

        /* Random module failure */
        if (ship->modules && list_size(ship->modules) > 0 && number_percent() <= 30) {
            int idx = number_range(1, list_size(ship->modules));
            SHIP_MODULE *mod = (SHIP_MODULE *)list_nthdata(ship->modules, idx);
            if (mod && mod->operational) {
                mod->active = false;
                mod->operational = false;
                boat_echo(ship, formatf("{R** Critical failure: '%s' has gone offline! **{x",
                    mod->index ? mod->index->name : "a module"));
                ship_recalc_modules(ship);
            }
        }
    }
    /* 50% — Major */
    else if (hp_pct <= SHIP_DAMAGE_THRESHOLD_MAJOR) {
        /* Speed halved if not already disabled */
        if (!IS_SET(ship->ship_flags, SHIP_ON_FIRE) && number_percent() <= 20) {
            SET_BIT(ship->ship_flags, SHIP_ON_FIRE);
            boat_echo(ship, "{R** Fire breaks out from the damage! **{x");
        }
    }
    /* 75% — Minor */
    else if (hp_pct <= SHIP_DAMAGE_THRESHOLD_MINOR) {
        /* Cosmetic only — message on first crossing */
        if (ship->ship_type == SHIP_SAILING_BOAT) {
            boat_echo(ship, "{Y** Water seeps through cracks in the hull. **{x");
        } else if (ship->ship_type == SHIP_AIR_SHIP) {
            boat_echo(ship, "{Y** Smoke billows from the damaged hull. **{x");
        } else {
            boat_echo(ship, "{Y** The vessel groans under the strain. **{x");
        }
    }
}

/**
 * ship_combat_rep_attack - Apply reputation penalty when a player attacks a faction ship
 *
 * Called when a player initiates combat against an NPC ship that has a faction.
 * Applies a small negative reputation change to the attacking player against
 * the target ship's faction.  Uses the captain's MOB_REPUTATION_DATA if present
 * for the base point amount, otherwise a flat default penalty.
 *
 * @param ch        Player who initiated the attack
 * @param attacker  Ship the player commands
 * @param target    NPC ship being attacked
 */
static void ship_combat_rep_attack(CHAR_DATA *ch, SHIP_DATA *attacker, SHIP_DATA *target)
{
    (void)attacker;

    if (!ch || IS_NPC(ch))
        return;

    if (!IS_VALID(target) || !target->index || !target->index->faction)
        return;

    REPUTATION_INDEX_DATA *faction = target->index->faction;
    long penalty = -50;  /* Default small penalty */

    /* Check captain's mob_reputation entries for a specific point value */
    if (target->npc_ship && target->npc_ship->captain &&
        IS_NPC(target->npc_ship->captain)) {
        MOB_INDEX_DATA *cap_idx = target->npc_ship->captain->pIndexData;
        if (cap_idx) {
            for (MOB_REPUTATION_DATA *mr = cap_idx->mob_reputations; mr; mr = mr->next) {
                if (mr->reputation == faction) {
                    penalty = -(mr->points / 4);  /* Quarter of kill value for attack */
                    if (penalty > -10) penalty = -10;
                    break;
                }
            }
        }
    }

    gain_reputation(ch, faction, penalty, NULL, NULL, true);
    printf_to_char(ch, "{YYour reputation with {W%s{Y has decreased.{x\n\r", faction->name);
}

/**
 * ship_combat_rep_sink - Apply reputation changes when an NPC ship is destroyed
 *
 * Called during ship_destruction_sequence for NPC ships with factions.
 * Finds all player-owned ships that were attacking the destroyed ship and
 * applies a large negative reputation change against the victim's faction.
 * Also checks nearby observer ships — if a faction ship witnessed its enemy
 * destroyed, grant the attacker positive reputation with the observer's faction.
 *
 * @param destroyed  NPC ship that was just destroyed
 */
static void ship_combat_rep_sink(SHIP_DATA *destroyed)
{
    if (!IS_VALID(destroyed) || !destroyed->index)
        return;

    REPUTATION_INDEX_DATA *victim_faction = destroyed->index->faction;
    long sink_penalty = -200;  /* Default large penalty */

    /* Get specific penalty from captain's MOB_REPUTATION_DATA if available */
    if (victim_faction && destroyed->npc_ship && destroyed->npc_ship->captain &&
        IS_NPC(destroyed->npc_ship->captain)) {
        MOB_INDEX_DATA *cap_idx = destroyed->npc_ship->captain->pIndexData;
        if (cap_idx) {
            for (MOB_REPUTATION_DATA *mr = cap_idx->mob_reputations; mr; mr = mr->next) {
                if (mr->reputation == victim_faction) {
                    sink_penalty = -(mr->points);  /* Full kill value for sinking */
                    if (sink_penalty > -50) sink_penalty = -50;
                    break;
                }
            }
        }
    }

    /* Find all player-owned ships that were attacking the destroyed ship */
    ITERATOR it;
    SHIP_DATA *attacker;
    iterator_start(&it, loaded_ships);
    while ((attacker = (SHIP_DATA *)iterator_nextdata(&it))) {
        if (attacker->ship_attacked != destroyed)
            continue;

        CHAR_DATA *pc_owner = attacker->owner;
        if (!pc_owner || IS_NPC(pc_owner))
            continue;

        /* Apply negative rep with victim's faction */
        if (victim_faction) {
            gain_reputation(pc_owner, victim_faction, sink_penalty, NULL, NULL, true);
            printf_to_char(pc_owner, "{RYour reputation with {W%s{R has significantly decreased!{x\n\r",
                victim_faction->name);
        }

        /* Check if any nearby NPC ships with factions consider the destroyed
         * ship an enemy — if so, grant positive reputation with observers */
        ITERATOR obs_it;
        SHIP_DATA *observer;
        iterator_start(&obs_it, loaded_ships);
        while ((observer = (SHIP_DATA *)iterator_nextdata(&obs_it))) {
            if (observer == attacker || observer == destroyed)
                continue;
            if (!IS_VALID(observer) || !observer->index || !observer->index->faction)
                continue;
            if (observer->index->faction == victim_faction)
                continue;  /* Same faction as victim — no bonus */

            int dist = ship_distance(observer, destroyed);
            if (dist > SHIP_COMBAT_DETECTION_RANGE)
                continue;  /* Too far to witness */

            /* Observer's faction type determines if they consider the sunk ship an enemy */
            bool is_enemy = false;
            if (IS_SET(observer->index->flags, SHIP_AUTONOMOUS_NPC)) {
                /* Coast guard considers pirates enemies, pirates consider coast guard enemies, etc. */
                if (npc_ship_is_valid_target(observer, destroyed))
                    is_enemy = true;
            }

            if (is_enemy) {
                long bonus = UMAX(25, (-sink_penalty) / 4);
                gain_reputation(pc_owner, observer->index->faction, bonus, NULL, NULL, true);
                printf_to_char(pc_owner, "{GYour reputation with {W%s{G has increased.{x\n\r",
                    observer->index->faction->name);
            }
        }
        iterator_stop(&obs_it);
    }
    iterator_stop(&it);
}

/**
 * ship_coast_guard_alert - Alert nearby coast guard ships when a player attacks
 *
 * Scans for coast guard NPC ships within detection range.  If any are found
 * and the target is NOT a pirate or hostile ship, the coast guard:
 *   - Applies negative reputation to the attacking player
 *   - Begins targeting the attacker's ship (enters combat)
 *   - Announces the interception
 *
 * This is the primary mechanism for "pirate flagging" — players who attack
 * innocent ships in patrolled waters suffer coast guard pursuit.
 *
 * @param ch        Player who initiated the attack
 * @param attacker  Ship commanded by the player
 * @param target    Ship being attacked
 */
static void ship_coast_guard_alert(CHAR_DATA *ch, SHIP_DATA *attacker, SHIP_DATA *target)
{
    if (!ch || IS_NPC(ch))
        return;

    /* Only triggers when attacking a non-pirate NPC ship */
    if (!IS_VALID(target) || !target->index)
        return;
    if (IS_SET(target->index->flags, SHIP_AUTONOMOUS_NPC) &&
        target->index->npc_type == NPC_SHIP_PIRATE)
        return;  /* Attacking pirates doesn't alert coast guard */

    ITERATOR it;
    SHIP_DATA *guard;
    iterator_start(&it, loaded_ships);
    while ((guard = (SHIP_DATA *)iterator_nextdata(&it))) {
        if (guard == attacker || guard == target)
            continue;
        if (!IS_VALID(guard) || !guard->index)
            continue;
        if (!IS_SET(guard->index->flags, SHIP_AUTONOMOUS_NPC))
            continue;
        if (guard->index->npc_type != NPC_SHIP_COAST_GUARD)
            continue;

        int dist = ship_distance(guard, attacker);
        if (dist > SHIP_COMBAT_DETECTION_RANGE)
            continue;

        /* Coast guard detected the aggression */
        if (guard->index->faction) {
            long penalty = -100;
            gain_reputation(ch, guard->index->faction, penalty, NULL, NULL, true);
            printf_to_char(ch, "{R** A coast guard vessel has witnessed your aggression! "
                "Your reputation with {W%s{R has decreased! **{x\n\r",
                guard->index->faction->name);
        }

        /* Coast guard engages the attacker */
        if (!IS_VALID(guard->ship_attacked)) {
            guard->ship_attacked = attacker;
            guard->attack_position = SHIP_ATTACK_LOADING;

            /* Start weapons reloading */
            if (guard->modules) {
                ITERATOR mod_it;
                SHIP_MODULE *mod;
                iterator_start(&mod_it, guard->modules);
                while ((mod = (SHIP_MODULE *)iterator_nextdata(&mod_it))) {
                    if (mod->index && mod->index->type == HARDPOINT_WEAPON &&
                        mod->operational && mod->reload_countdown <= 0) {
                        mod->reload_countdown = mod->index->reload_time;
                    }
                }
                iterator_stop(&mod_it);
            }

            /* Update NPC state to attacking */
            if (guard->npc_ship)
                guard->npc_ship->state = NPC_SHIP_STATE_ATTACKING;

            boat_echo(guard, formatf("{W** Hostile vessel detected! Engaging %s! **{x",
                attacker->ship_name ? attacker->ship_name : "unknown vessel"));
            boat_echo(attacker, formatf("{R** %s is moving to intercept! **{x",
                guard->ship_name ? guard->ship_name : "A coast guard vessel"));
        }
    }
    iterator_stop(&it);
}

/**
 * ship_destruction_sequence - Handle final destruction of a sinking ship
 *
 * Called when scuttle_time reaches 0 on a sinking ship. Domain-appropriate
 * destruction: aquatic sinks, aerial crashes, terrestrial breaks down.
 * All aboard are ejected/damaged. The ship is extracted.
 *
 * @param ship  Ship being destroyed
 */
static void ship_destruction_sequence(SHIP_DATA *ship)
{
    if (!IS_VALID(ship)) return;

    if (ship->ship_type == SHIP_AIR_SHIP) {
        boat_echo(ship, "{R** With a thunderous crash, the airship plummets to the ground! **{x");
    } else if (ship->ship_type == SHIP_LAND_VESSEL) {
        boat_echo(ship, "{R** The land vessel collapses into wreckage! **{x");
    } else {
        boat_echo(ship, "{B** With a final gurgle, the vessel slips beneath the waves! **{x");
    }

    /* Apply reputation changes for sinking an NPC faction ship
     * (must happen before clearing combat references) */
    ship_combat_rep_sink(ship);

    /* Clear combat references to this ship */
    {
        ITERATOR it;
        SHIP_DATA *other;
        iterator_start(&it, loaded_ships);
        while ((other = (SHIP_DATA *)iterator_nextdata(&it))) {
            if (other->ship_attacked == ship) {
                other->ship_attacked = NULL;
                other->attack_position = SHIP_ATTACK_STOPPED;
            }
            if (other->ship_chased == ship)
                other->ship_chased = NULL;
            if (other->boarded_by == ship)
                other->boarded_by = NULL;
        }
        iterator_stop(&it);
    }

    extract_ship(ship);
}

/**
 * boat_fire_weapon - Fire a single weapon module at a target ship
 *
 * Calculates hit chance based on range, crew gunning skill, and base chance.
 * On hit, applies damage via boat_damage(). Consumes ammo if required.
 * Triggers crew skill improvement for gunning.
 *
 * @param attacker  Ship firing
 * @param target    Target ship
 * @param weapon    Weapon module to fire
 */
static void boat_fire_weapon(SHIP_DATA *attacker, SHIP_DATA *target, SHIP_MODULE *weapon)
{
    if (!IS_VALID(attacker) || !IS_VALID(target) || !weapon || !weapon->index)
        return;

    SHIP_MODULE_INDEX *wpn = weapon->index;

    /* Consume ammo if required */
    if (IS_SET(wpn->flags, MODULE_REQUIRES_AMMO)) {
        if (weapon->ammo_count < wpn->ammo_per_shot) {
            boat_echo(attacker, formatf("{Y** %s: Out of ammunition! **{x", wpn->name));
            return;
        }
        weapon->ammo_count -= wpn->ammo_per_shot;
    }

    /* Calculate hit chance */
    int dist = ship_distance(attacker, target);
    int half_range = UMAX(1, wpn->range / 2);
    int hit_chance = SHIP_COMBAT_HIT_BASE;

    /* Range penalty: -5% per tile beyond half max range */
    if (dist > half_range) {
        hit_chance -= (dist - half_range) * SHIP_COMBAT_RANGE_PENALTY;
    }

    /* Crew gunning skill bonus */
    if (weapon->assigned_crew && list_size(weapon->assigned_crew) > 0) {
        CHAR_DATA *gunner = (CHAR_DATA *)list_nthdata(weapon->assigned_crew, 1);
        if (gunner && gunner->crew) {
            hit_chance += gunner->crew->gunning * SHIP_COMBAT_SKILL_BONUS;
        }
    }

    /* Clamp */
    hit_chance = URANGE(5, hit_chance, 95);

    /* Roll */
    int roll = number_percent();
    if (roll > hit_chance) {
        boat_echo(attacker, formatf("{C%s fires — misses!{x", wpn->name));
        boat_echo(target, formatf("{CA volley from %s — splashes harmlessly!{x",
            attacker->ship_name ? attacker->ship_name : "an enemy vessel"));
    } else {
        /* Hit! Calculate damage */
        int damage = wpn->damage;

        /* Crew gunning skill adds minor damage bonus */
        if (weapon->assigned_crew && list_size(weapon->assigned_crew) > 0) {
            CHAR_DATA *gunner = (CHAR_DATA *)list_nthdata(weapon->assigned_crew, 1);
            if (gunner && gunner->crew) {
                damage += gunner->crew->gunning; /* +1 per skill point */
            }
        }

        /* AOE weapons do reduced damage to hull but hit more modules */
        bool is_aoe = IS_SET(wpn->weapon_flags, MODULE_AOE);

        int damage_type = IS_SET(wpn->weapon_flags, MODULE_FIRE_DAMAGE) ?
            SHIP_DAMAGE_FIRE : SHIP_DAMAGE_GRIND;

        boat_echo(attacker, formatf("{G%s fires — HIT! (%d damage){x", wpn->name, damage));
        boat_echo(target, formatf("{R** %s struck by %s from %s! **{x",
            target->ship_name ? target->ship_name : "Your vessel",
            wpn->name,
            attacker->ship_name ? attacker->ship_name : "an enemy vessel"));

        boat_damage(target, damage, damage_type);

        /* AOE: second module hit chance */
        if (is_aoe && target->modules && list_size(target->modules) > 0 &&
            number_percent() <= SHIP_MODULE_HIT_CHANCE * 2) {
            int idx = number_range(1, list_size(target->modules));
            SHIP_MODULE *hit_mod = (SHIP_MODULE *)list_nthdata(target->modules, idx);
            if (hit_mod && hit_mod->condition > 0) {
                int mod_dmg = UMAX(1, damage / 4);
                hit_mod->condition = UMAX(0, hit_mod->condition - mod_dmg);
                if (hit_mod->condition <= 0) {
                    hit_mod->operational = false;
                    boat_echo(target, formatf("{R** Module '%s' destroyed by blast! **{x",
                        hit_mod->index ? hit_mod->index->name : "unknown"));
                }
                ship_recalc_modules(target);
            }
        }

        /* Anti-crew weapon: chance to injure crew */
        if (IS_SET(wpn->weapon_flags, MODULE_ANTI_CREW) &&
            target->crew && list_size(target->crew) > 0) {
            ITERATOR crew_it;
            CHAR_DATA *crew;
            iterator_start(&crew_it, target->crew);
            while ((crew = (CHAR_DATA *)iterator_nextdata(&crew_it))) {
                if (number_percent() <= SHIP_CREW_HIT_CHANCE) {
                    int crew_dmg = number_range(20, 60);
                    if (crew->hit > crew_dmg) {
                        crew->hit -= crew_dmg;
                        boat_echo(target, formatf("{R** %s is wounded! **{x",
                            crew->short_descr));
                    } else {
                        boat_echo(target, formatf("{R** %s has been killed! **{x",
                            crew->short_descr));
                        iterator_remcurrent(&crew_it);
                        list_remlink(target->crew, crew, false);
                        crew->belongs_to_ship = NULL;
                        extract_char(crew, true);
                    }
                }
            }
            iterator_stop(&crew_it);
        }
    }

    /* Reset reload countdown */
    weapon->reload_countdown = wpn->reload_time;

    /* Crew skill improvement for all assigned crew (gunning) */
    if (weapon->assigned_crew) {
        ITERATOR crew_it;
        CHAR_DATA *crew;
        iterator_start(&crew_it, weapon->assigned_crew);
        while ((crew = (CHAR_DATA *)iterator_nextdata(&crew_it))) {
            crew_skill_improve(crew, CREW_SKILL_GUNNING);
        }
        iterator_stop(&crew_it);
    }
}

/**
 * ship_combat_update - Per-pulse combat update for a single ship
 *
 * Handles:
 * - Weapon reload countdown (each weapon counts down independently)
 * - Firing ready weapons at current target
 * - Auto-disengage if target out of range or destroyed
 * - Fire damage ticks
 * - Sinking countdown
 * - Chase heading updates
 *
 * @param ship  Ship to update
 */
void ship_combat_update(SHIP_DATA *ship)
{
    if (!IS_VALID(ship)) return;

    /* --- Sinking countdown --- */
    if (IS_SET(ship->ship_flags, SHIP_SINKING)) {
        if (ship->scuttle_time > 0) {
            ship->scuttle_time--;
            if (ship->scuttle_time <= 0) {
                ship_destruction_sequence(ship);
                return; /* Ship is gone */
            }
            if (ship->scuttle_time <= 3) {
                boat_echo(ship, formatf("{R** %d ticks until the vessel is lost! **{x",
                    ship->scuttle_time));
            }
        }
        return; /* No other combat actions while sinking */
    }

    /* --- Fire damage tick --- */
    if (IS_SET(ship->ship_flags, SHIP_ON_FIRE)) {
        /* Fire does 2-5% of max hull per tick */
        long fire_dmg = UMAX(1, ship->index->hit * number_range(2, 5) / 100);
        boat_damage(ship, fire_dmg, SHIP_DAMAGE_FIRE);

        /* Mechanics crew can extinguish — check for any crew with mechanics skill */
        bool has_mechanics = false;
        if (ship->modules) {
            ITERATOR it;
            SHIP_MODULE *mod;
            iterator_start(&it, ship->modules);
            while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
                if (mod->assigned_crew && list_size(mod->assigned_crew) > 0) {
                    ITERATOR crew_it;
                    CHAR_DATA *crew;
                    iterator_start(&crew_it, mod->assigned_crew);
                    while ((crew = (CHAR_DATA *)iterator_nextdata(&crew_it))) {
                        if (crew->crew && crew->crew->mechanics >= 3) {
                            has_mechanics = true;
                            crew_skill_improve(crew, CREW_SKILL_MECHANICS);
                            break;
                        }
                    }
                    iterator_stop(&crew_it);
                }
                if (has_mechanics) break;
            }
            iterator_stop(&it);
        }

        /* 25% chance per tick to extinguish if mechanics crew available */
        if (has_mechanics && number_percent() <= 25) {
            REMOVE_BIT(ship->ship_flags, SHIP_ON_FIRE);
            boat_echo(ship, "{G** The crew has extinguished the fires! **{x");
        }
    }

    /* --- Chase logic --- */
    if (IS_VALID(ship->ship_chased)) {
        /* Check if target is still valid and in range */
        int max_range = ship_get_max_weapon_range(ship);
        int chase_range = max_range > 0 ? max_range * 3 : 30;
        int dist = ship_distance(ship, ship->ship_chased);

        if (dist > chase_range || !IS_VALID(ship->ship_chased->ship) ||
            !ship->ship_chased->ship->in_room) {
            boat_echo(ship, "{Y** Target has escaped pursuit. **{x");
            ship->ship_chased = NULL;
        } else if (ship->ship && ship->ship->in_room &&
                   ship->ship_chased->ship && ship->ship_chased->ship->in_room) {
            /* Calculate heading toward target */
            int dx = ship->ship_chased->ship->in_room->x - ship->ship->in_room->x;
            int dy = ship->ship_chased->ship->in_room->y - ship->ship->in_room->y;

            if (dx != 0 || dy != 0) {
                int target_heading = (int)(atan2((double)dx, (double)-dy) * 180.0 / M_PI);
                if (target_heading < 0) target_heading += 360;
                steering_set_heading(ship, target_heading);
                steering_calc_heading(ship);
            }

            /* Auto speed: full speed in pursuit */
            if (ship->ship_power < SHIP_SPEED_FULL_SPEED)
                ship->ship_power = SHIP_SPEED_FULL_SPEED;
        }
    }

    /* --- Weapon fire/reload cycle --- */
    if (ship->attack_position == SHIP_ATTACK_STOPPED) return;
    if (!IS_VALID(ship->ship_attacked)) {
        /* Target no longer valid */
        ship->attack_position = SHIP_ATTACK_STOPPED;
        ship->ship_attacked = NULL;
        ship->char_attacked = NULL;
        return;
    }

    /* Check if target is still in range of any weapon */
    int dist = ship_distance(ship, ship->ship_attacked);
    int max_range = ship_get_max_weapon_range(ship);

    if (max_range <= 0 || dist > max_range) {
        boat_echo(ship, "{Y** Target is out of weapon range. Ceasing fire. **{x");
        ship->attack_position = SHIP_ATTACK_STOPPED;
        ship->ship_attacked = NULL;
        return;
    }

    /* Iterate weapon modules: count down reload, fire when ready */
    if (ship->modules) {
        ITERATOR it;
        SHIP_MODULE *mod;
        bool any_fired = false;

        iterator_start(&it, ship->modules);
        while ((mod = (SHIP_MODULE *)iterator_nextdata(&it))) {
            if (!mod->index || mod->index->type != HARDPOINT_WEAPON) continue;
            if (!mod->operational) continue;

            /* Check individual weapon range */
            if (dist > mod->index->range) continue;

            if (mod->reload_countdown > 0) {
                mod->reload_countdown--;

                /* Gunning skill can speed reload slightly */
                if (mod->assigned_crew && list_size(mod->assigned_crew) > 0) {
                    CHAR_DATA *gunner = (CHAR_DATA *)list_nthdata(mod->assigned_crew, 1);
                    if (gunner && gunner->crew && gunner->crew->gunning >= 5 &&
                        number_percent() <= 10) {
                        mod->reload_countdown--; /* Skilled gunners reload faster */
                    }
                }
            }

            if (mod->reload_countdown <= 0) {
                /* FIRE! */
                boat_fire_weapon(ship, ship->ship_attacked, mod);
                any_fired = true;
            }
        }
        iterator_stop(&it);

        /* Update attack phase display */
        if (any_fired) {
            ship->attack_position = SHIP_ATTACK_FIRED;
        } else {
            ship->attack_position = SHIP_ATTACK_LOADING;
        }
    }
}


/////////////////////////////////////////////////////////////////
//
// NPC Ship Creation & Management
//

/**
 * ship_find_helm_room - Find the helm room in a ship's blueprint instance
 *
 * Iterates through instance sections and their rooms looking for a room
 * with the ROOM_SHIP_HELM flag set. Returns the first matching room,
 * or the instance entrance as a fallback.
 *
 * @param ship  Ship to search
 * @return      Helm room, or instance entrance, or NULL
 */
static ROOM_INDEX_DATA *ship_find_helm_room(SHIP_DATA *ship)
{
    ITERATOR sec_it, room_it;
    INSTANCE_SECTION *section;
    ROOM_INDEX_DATA *room;

    if (!IS_VALID(ship) || !IS_VALID(ship->instance))
        return NULL;

    iterator_start(&sec_it, ship->instance->sections);
    while ((section = (INSTANCE_SECTION *)iterator_nextdata(&sec_it)) != NULL) {
        if (!section->rooms)
            continue;
        iterator_start(&room_it, section->rooms);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&room_it)) != NULL) {
            if (IS_SET(room->room_flag[0], ROOM_SHIP_HELM)) {
                iterator_stop(&room_it);
                iterator_stop(&sec_it);
                return room;
            }
        }
        iterator_stop(&room_it);
    }
    iterator_stop(&sec_it);

    return ship->instance->entrance;
}

/**
 * ship_get_random_interior_room - Get a random room inside a ship instance
 *
 * Collects all rooms from all instance sections and picks one at random.
 * Used for placing crew members in varied locations throughout the ship.
 *
 * @param ship  Ship to pick a room from
 * @return      Random interior room, or instance entrance, or NULL
 */
static ROOM_INDEX_DATA *ship_get_random_interior_room(SHIP_DATA *ship)
{
    ITERATOR sec_it, room_it;
    INSTANCE_SECTION *section;
    ROOM_INDEX_DATA *room;
    int total = 0;
    int pick;

    if (!IS_VALID(ship) || !IS_VALID(ship->instance))
        return NULL;

    /* Count total rooms */
    iterator_start(&sec_it, ship->instance->sections);
    while ((section = (INSTANCE_SECTION *)iterator_nextdata(&sec_it)) != NULL) {
        if (section->rooms)
            total += list_size(section->rooms);
    }
    iterator_stop(&sec_it);

    if (total <= 0)
        return ship->instance->entrance;

    pick = number_range(1, total);
    total = 0;

    iterator_start(&sec_it, ship->instance->sections);
    while ((section = (INSTANCE_SECTION *)iterator_nextdata(&sec_it)) != NULL) {
        if (!section->rooms)
            continue;
        iterator_start(&room_it, section->rooms);
        while ((room = (ROOM_INDEX_DATA *)iterator_nextdata(&room_it)) != NULL) {
            if (++total == pick) {
                iterator_stop(&room_it);
                iterator_stop(&sec_it);
                return room;
            }
        }
        iterator_stop(&room_it);
    }
    iterator_stop(&sec_it);

    return ship->instance->entrance;
}

/**
 * ship_populate_crew - Spawn crew mobs from template definitions onto a ship
 *
 * Creates mobile instances from the ship's SHIP_INDEX_DATA crew_defs list
 * and captain definition. The captain is placed at the helm room, crew
 * members are distributed across ship interior rooms. All spawned mobs
 * are added to ship->crew LLIST and have belongs_to_ship set.
 *
 * Works for both NPC and player-owned ships (e.g., hired crew from a template).
 *
 * @param ship  Ship to populate (must have valid index and instance)
 * @return      Number of crew members spawned (including captain)
 */
int ship_populate_crew(SHIP_DATA *ship)
{
    SHIP_INDEX_DATA *idx;
    ROOM_INDEX_DATA *helm_room;
    ITERATOR it;
    SHIP_CREW_DEF *cd;
    CHAR_DATA *mob;
    int spawned = 0;

    if (!IS_VALID(ship) || !ship->index)
        return 0;

    idx = ship->index;

    if (!IS_VALID(ship->instance))
        return 0;

    helm_room = ship_find_helm_room(ship);

    /* Spawn the captain at the helm */
    if (idx->captain) {
        mob = create_mobile(idx->captain, false);
        if (mob) {
            if (helm_room)
                char_to_room(mob, helm_room);
            else if (ship->instance->entrance)
                char_to_room(mob, ship->instance->entrance);

            list_appendlink(ship->crew, mob);
            mob->belongs_to_ship = ship;
            spawned++;

            /* If this ship has NPC_SHIP_DATA, reference the captain there too */
            if (ship->npc_ship)
                ship->npc_ship->captain = mob;
        }
    }

    /* Spawn crew from definitions */
    if (idx->crew_defs && list_size(idx->crew_defs) > 0) {
        iterator_start(&it, idx->crew_defs);
        while ((cd = (SHIP_CREW_DEF *)iterator_nextdata(&it)) != NULL) {
            if (!cd->mob)
                continue;

            for (int i = 0; i < cd->count; i++) {
                mob = create_mobile(cd->mob, false);
                if (!mob)
                    continue;

                ROOM_INDEX_DATA *target = ship_get_random_interior_room(ship);
                if (target)
                    char_to_room(mob, target);
                else if (ship->instance->entrance)
                    char_to_room(mob, ship->instance->entrance);

                list_appendlink(ship->crew, mob);
                mob->belongs_to_ship = ship;
                spawned++;
            }
        }
        iterator_stop(&it);
    }

    return spawned;
}

/**
 * ship_auto_assign_crew - Assign crew members to modules that need operators
 *
 * Iterates installed modules and assigns unassigned crew members to modules
 * missing operators. For NPC ships, crew are always considered qualified.
 * For player ships, this checks skill requirements against SHIP_CREW_DATA.
 *
 * Crew members can only be assigned to one module at a time. The captain
 * (from NPC_SHIP_DATA) is excluded from module assignment.
 *
 * @param ship  Ship whose crew to assign to modules
 */
void ship_auto_assign_crew(SHIP_DATA *ship)
{
    ITERATOR mod_it, crew_it;
    SHIP_MODULE *mod;
    CHAR_DATA *crew;

    if (!IS_VALID(ship))
        return;

    if (!ship->modules || !ship->crew)
        return;

    /* Build a simple list of unassigned crew members */
    /* A crew member is "unassigned" if they appear in no module's assigned_crew */
    iterator_start(&mod_it, ship->modules);
    while ((mod = (SHIP_MODULE *)iterator_nextdata(&mod_it)) != NULL) {
        if (!IS_VALID(mod) || !mod->index || !mod->active)
            continue;

        int needed = mod->index->operators;
        int have = mod->assigned_crew ? list_size(mod->assigned_crew) : 0;

        if (have >= needed)
            continue;

        /* Need (needed - have) more crew for this module */
        int slots_open = needed - have;

        iterator_start(&crew_it, ship->crew);
        while ((crew = (CHAR_DATA *)iterator_nextdata(&crew_it)) != NULL && slots_open > 0) {
            /* Skip the captain — they command, not operate */
            if (ship->npc_ship && ship->npc_ship->captain == crew)
                continue;

            /* Skip crew already assigned to a module */
            bool already_assigned = false;
            ITERATOR check_it;
            SHIP_MODULE *check_mod;
            iterator_start(&check_it, ship->modules);
            while ((check_mod = (SHIP_MODULE *)iterator_nextdata(&check_it)) != NULL) {
                if (check_mod->assigned_crew &&
                    list_hasdata(check_mod->assigned_crew, crew)) {
                    already_assigned = true;
                    break;
                }
            }
            iterator_stop(&check_it);

            if (already_assigned)
                continue;

            /* For NPC crew without SHIP_CREW_DATA, assume qualified */
            bool qualified = true;
            if (crew->crew && mod->index) {
                if (mod->index->req_gunning > 0 &&
                    crew->crew->gunning < mod->index->req_gunning)
                    qualified = false;
                if (mod->index->req_mechanics > 0 &&
                    crew->crew->mechanics < mod->index->req_mechanics)
                    qualified = false;
                if (mod->index->req_scouting > 0 &&
                    crew->crew->scouting < mod->index->req_scouting)
                    qualified = false;
                if (mod->index->req_navigation > 0 &&
                    crew->crew->navigation < mod->index->req_navigation)
                    qualified = false;
                if (mod->index->req_oarring > 0 &&
                    crew->crew->oarring < mod->index->req_oarring)
                    qualified = false;
                if (mod->index->req_leadership > 0 &&
                    crew->crew->leadership < mod->index->req_leadership)
                    qualified = false;
            }

            if (!qualified)
                continue;

            /* Assign this crew member to the module */
            if (!mod->assigned_crew)
                mod->assigned_crew = list_create(false);

            list_appendlink(mod->assigned_crew, crew);
            slots_open--;
        }
        iterator_stop(&crew_it);
    }
    iterator_stop(&mod_it);

    /* Recalculate operational status for all modules */
    ship_recalc_modules(ship);
}

/**
 * create_npc_sailing_boat - Create a fully-operational NPC ship from a template
 *
 * Creates a ship via create_ship(), spawns crew from template definitions,
 * attaches NPC_SHIP_DATA for AI behavior, and marks the ship as autonomous.
 * The resulting ship is ready to be placed in the world and will be driven
 * by the NPC autopilot tick.
 *
 * @param ship_index  Ship template to instantiate
 * @return            NPC_SHIP_DATA for the new ship, or NULL on failure
 */
NPC_SHIP_DATA *create_npc_sailing_boat(SHIP_INDEX_DATA *ship_index)
{
    SHIP_DATA *ship;
    NPC_SHIP_DATA *npc;
    WNUM wnum;

    if (!ship_index)
        return NULL;

    /* Build the WNUM from the ship index */
    wnum.pArea = ship_index->area;
    wnum.vnum = ship_index->vnum;

    ship = create_ship(wnum);
    if (!IS_VALID(ship))
        return NULL;

    /* Create and attach NPC runtime data */
    npc = new_npc_ship_data();
    npc->ship = ship;
    npc->state = NPC_SHIP_STATE_STOPPED;
    npc->trigger_char = NULL;
    npc->captain = NULL;

    ship->npc_ship = npc;
    ship->npc_autonomous = true;
    ship->owner = NULL;

    /* Copy NPC type from template */
    /* (npc_type on SHIP_INDEX_DATA drives AI behavior) */

    /* Populate crew from template definitions */
    ship_populate_crew(ship);

    /* Assign crew to installed modules */
    ship_auto_assign_crew(ship);

    /* Set initial ship power so autopilot can begin moving */
    if (ship->ship_power <= SHIP_SPEED_STOPPED)
        ship->ship_power = SHIP_SPEED_FULL_SPEED;

    /* Set initial heading */
    if (ship->steering.heading < 0) {
        int heading = number_range(0, 359);
        ship->steering.heading = heading;
        ship->steering.heading_target = heading;
        steering_calc_heading(ship);
    }

    ship_set_move_steps(ship);

    /* Add to the NPC ship linked list */
    npc->next = npc_ship_list;
    npc_ship_list = npc;

    return npc;
}

/**
 * npc_ship_is_valid_target - Check if a candidate ship is targetable by an NPC
 *
 * Applies type-based targeting rules:
 *   - PIRATE: targets player ships and traders, avoids other pirates
 *   - COAST_GUARD: targets pirates only
 *   - BOUNTY_HUNTER: targets trigger_char's ship, or pirates
 *   - TRADER: never initiates combat
 *   - ADVENTURER: targets pirates if provoked
 *   - AIR_SHIP: never initiates combat (transport only)
 *
 * Protected ships and fellow NPC ships of the same type are always skipped.
 *
 * @param hunter     NPC ship doing the scanning
 * @param candidate  Potential target ship
 * @return           true if candidate is a valid target
 */
static bool npc_ship_is_valid_target(SHIP_DATA *hunter, SHIP_DATA *candidate)
{
    int hunter_type;
    int candidate_type;

    if (!IS_VALID(hunter) || !IS_VALID(candidate))
        return false;

    /* Never target yourself */
    if (hunter == candidate)
        return false;

    /* Never target protected ships */
    if (IS_SET(candidate->ship_flags, SHIP_PROTECTED))
        return false;

    /* Never target sinking ships */
    if (IS_SET(candidate->ship_flags, SHIP_SINKING))
        return false;

    if (!hunter->index)
        return false;

    hunter_type = hunter->index->npc_type;

    /* Determine candidate's NPC type (-1 means player-owned) */
    candidate_type = -1;
    if (candidate->npc_ship && candidate->index &&
        IS_SET(candidate->ship_flags, SHIP_AUTONOMOUS_NPC)) {
        candidate_type = candidate->index->npc_type;
    }

    switch (hunter_type) {
        case NPC_SHIP_PIRATE:
            /* Pirates attack player ships and traders, leave other pirates alone */
            if (candidate_type == NPC_SHIP_PIRATE)
                return false;
            if (candidate_type == NPC_SHIP_COAST_GUARD)
                return (number_percent() < 20);  /* Rarely pick fights with coast guard */
            return true;  /* Attack players, traders, adventurers */

        case NPC_SHIP_COAST_GUARD:
            /* Coast guard only attacks pirates */
            return (candidate_type == NPC_SHIP_PIRATE);

        case NPC_SHIP_BOUNTY_HUNTER:
            /* Bounty hunters target trigger_char's ship first, then pirates */
            if (hunter->npc_ship && hunter->npc_ship->trigger_char) {
                if (IS_VALID(candidate->owner) &&
                    candidate->owner == hunter->npc_ship->trigger_char)
                    return true;
            }
            return (candidate_type == NPC_SHIP_PIRATE);

        case NPC_SHIP_TRADER:
            /* Traders never initiate combat */
            return false;

        case NPC_SHIP_ADVENTURER:
            /* Adventurers only attack pirates */
            return (candidate_type == NPC_SHIP_PIRATE);

        case NPC_SHIP_AIR_SHIP:
            /* Air ships never initiate combat */
            return false;

        default:
            return false;
    }
}

/**
 * npc_ship_scan_for_target - Scan nearby ships for a valid combat target
 *
 * Iterates loaded_ships looking for candidates within detection range.
 * Detection range is based on the ship's max weapon range, or a default
 * of 15 tiles. Only considers ships in the same wilderness.
 *
 * Returns the closest valid target, or NULL if none found.
 *
 * @param ship  NPC ship doing the scanning
 * @return      Best target ship, or NULL
 */
static SHIP_DATA *npc_ship_scan_for_target(SHIP_DATA *ship)
{
    ITERATOR it;
    SHIP_DATA *candidate;
    SHIP_DATA *best = NULL;
    int best_dist = INT_MAX;
    int detect_range;
    ROOM_INDEX_DATA *our_room;

    if (!IS_VALID(ship) || !IS_VALID(ship->ship))
        return NULL;

    our_room = ship->ship->in_room;
    if (!our_room || !IS_WILDERNESS(our_room))
        return NULL;

    detect_range = ship_get_max_weapon_range(ship);
    if (detect_range <= 0)
        detect_range = 15;
    else
        detect_range = UMAX(detect_range, 10);  /* Minimum 10 tile detection */

    iterator_start(&it, loaded_ships);
    while ((candidate = (SHIP_DATA *)iterator_nextdata(&it)) != NULL) {
        if (!IS_VALID(candidate->ship) || !candidate->ship->in_room)
            continue;

        /* Must be in the same wilderness */
        if (!IS_WILDERNESS(candidate->ship->in_room))
            continue;
        if (candidate->ship->in_room->wilds != our_room->wilds)
            continue;

        int dist = ship_distance(ship, candidate);
        if (dist > detect_range)
            continue;

        if (!npc_ship_is_valid_target(ship, candidate))
            continue;

        if (dist < best_dist) {
            best = candidate;
            best_dist = dist;
        }
    }
    iterator_stop(&it);

    return best;
}

/***************************************************************************
 * Transport Schedule System                                               *
 ***************************************************************************/

/**
 * ship_schedule_find_stop - Look up a stop by index in the schedule list
 *
 * @param ship   Ship index with schedule_stops list
 * @param idx    0-based index
 * @return       Pointer to stop, or NULL if out of range
 */
static SHIP_SCHEDULE_STOP *ship_schedule_find_stop(SHIP_INDEX_DATA *idx, int stop_idx)
{
    if (!idx || !idx->schedule_stops)
        return NULL;

    return (SHIP_SCHEDULE_STOP *)list_nthdata(idx->schedule_stops, stop_idx + 1);
}

/**
 * ship_schedule_create_dock_exit - Create a temporary exit from dock to ship
 *
 * Creates a one-way exit from the dock room (or wilderness vroom) into
 * the ship's instance entrance room upon docking.
 *
 * For DOCK_EXIT_ROOM: creates exit on dock_room in direction dock_exit_dir
 * For DOCK_EXIT_VLINK: handled externally (vlinks are managed differently)
 * For DOCK_EXIT_INSTANCE: syncs instance entrance to dock location
 *
 * @param ship   Ship data
 * @param stop   Schedule stop being docked at
 */
static void ship_schedule_create_dock_exit(SHIP_DATA *ship, SHIP_SCHEDULE_STOP *stop)
{
    if (!ship || !stop || stop->dock_exit_type == DOCK_EXIT_NONE)
        return;

    if (stop->dock_exit_dir < 0 || stop->dock_exit_dir >= MAX_DIR)
        return;

    ROOM_INDEX_DATA *entry_room = NULL;
    if (IS_VALID(ship->instance) && ship->instance->entrance)
        entry_room = ship->instance->entrance;

    if (!entry_room)
        return;

    switch (stop->dock_exit_type) {
        case DOCK_EXIT_ROOM: {
            /* Create a temporary exit from the dock room into the ship */
            ROOM_INDEX_DATA *dock = stop->dock_room;
            if (!dock)
                return;

            /* Don't overwrite an existing exit */
            if (dock->exit[stop->dock_exit_dir] != NULL)
                return;

            EXIT_DATA *ex = new_exit();
            ex->u1.to_room = entry_room;
            ex->from_room = dock;
            ex->orig_door = stop->dock_exit_dir;
            free_string(ex->keyword);
            ex->keyword = str_dup("gangway plank");
            free_string(ex->short_desc);
            ex->short_desc = str_dup("the gangway");

            dock->exit[stop->dock_exit_dir] = ex;
            ship->schedule_dock_exit = ex;
            ship->schedule_dock_from = dock;
            break;
        }

        case DOCK_EXIT_INSTANCE: {
            /* Sync instance entrance location to the stop */
            if (stop->location_type == STOP_LOC_ROOM && stop->dock_room) {
                /* For zone-room stops, set entrance environ to dock room */
                ship->instance->environ = stop->dock_room;
            } else if (stop->location_type == STOP_LOC_WILDERNESS) {
                WILDS_DATA *wilds = get_wilds_from_uid(NULL, stop->wilds_uid);
                if (wilds && ship->instance->entrance) {
                    ship->instance->entrance->wilds = wilds;
                    ship->instance->entrance->x = stop->loc_x;
                    ship->instance->entrance->y = stop->loc_y;
                }
            }
            break;
        }

        case DOCK_EXIT_VLINK:
            /* VLinks are static/persistent — not dynamically created here.
             * The schedule stop should align with an existing vlink at the
             * wilderness coords. Future: could create temporary vlinks. */
            break;
    }
}

/**
 * ship_schedule_remove_dock_exit - Remove the temporary dock exit
 *
 * Cleans up exits created by ship_schedule_create_dock_exit when the
 * ship departs from a stop.
 *
 * @param ship  Ship data with dock exit references
 */
static void ship_schedule_remove_dock_exit(SHIP_DATA *ship)
{
    if (!ship)
        return;

    if (ship->schedule_dock_exit && ship->schedule_dock_from) {
        int dir = ship->schedule_dock_exit->orig_door;
        if (dir >= 0 && dir < MAX_DIR
            && ship->schedule_dock_from->exit[dir] == ship->schedule_dock_exit) {
            free_exit(ship->schedule_dock_exit);
            ship->schedule_dock_from->exit[dir] = NULL;
        }
    }

    ship->schedule_dock_exit = NULL;
    ship->schedule_dock_from = NULL;
}

/**
 * ship_schedule_tick - Per-tick update for transport schedule ships
 *
 * Manages the schedule state machine:
 *
 * IDLE     -> Look at clock, pick first/next stop, begin traveling
 * TRAVELING -> Ship is navigating via seek_point; check if arrived
 * ARRIVING  -> Arrived at stop, create dock exits → DOCKED
 * DOCKED    -> Count down dwell timer or wait for depart_hour → DEPARTING
 * DEPARTING -> Remove dock exits, advance to next stop → TRAVELING
 *
 * Called from npc_ship_state_update when state is NPC_SHIP_STATE_TRANSPORT.
 *
 * @param ship  Ship with SHIP_TRANSPORT flag and schedule_stops
 */
static void ship_schedule_tick(SHIP_DATA *ship)
{
    SHIP_INDEX_DATA *idx;
    int num_stops;

    if (!ship || !ship->index)
        return;

    idx = ship->index;

    if (!idx->schedule_stops)
        return;

    num_stops = list_size(idx->schedule_stops);
    if (num_stops == 0)
        return;

    switch (ship->schedule_state) {
        case SCHEDULE_STATE_IDLE: {
            /* Initialize: pick the first stop and start traveling */
            ship->schedule_stop_idx = 0;
            ship->schedule_current_stop = ship_schedule_find_stop(idx, 0);
            if (!ship->schedule_current_stop) {
                return;  /* No valid stops */
            }
            ship->schedule_state = SCHEDULE_STATE_TRAVELING;

            /* Set navigation target */
            SHIP_SCHEDULE_STOP *stop = ship->schedule_current_stop;
            if (stop->location_type == STOP_LOC_WILDERNESS) {
                WILDS_DATA *wilds = get_wilds_from_uid(NULL, stop->wilds_uid);
                if (wilds) {
                    ship_npc_set_seek_goal(ship, wilds, stop->loc_x, stop->loc_y);
                    ship->seek_navigator = false;

                    if (ship->ship_power <= SHIP_SPEED_STOPPED) {
                        ship->ship_power = SHIP_SPEED_FULL_SPEED;
                        ship_set_move_steps(ship);
                        if (ship->ship_move <= 0)
                            ship->ship_move = UMAX(1, idx->move_delay);
                    }
                }
            }
            /* For STOP_LOC_ROOM: airship-style, would teleport or fly.
             * For now, room-based stops should also have wilds coords
             * as approach points, or be handled by landing logic. */
            break;
        }

        case SCHEDULE_STATE_TRAVELING: {
            /* Check if we've arrived at the target stop */
            ROOM_INDEX_DATA *room = ship->ship ? obj_room(ship->ship) : NULL;
            SHIP_SCHEDULE_STOP *stop = ship->schedule_current_stop;

            if (!room || !stop)
                break;

            if (stop->location_type == STOP_LOC_WILDERNESS && IS_WILDERNESS(room)) {
                int dx = room->x - stop->loc_x;
                int dy = room->y - stop->loc_y;
                int distSq = dx * dx + dy * dy;

                if (distSq <= 6) {
                    /* Arrived! */
                    ship->schedule_state = SCHEDULE_STATE_ARRIVING;
                }
            } else if (stop->location_type == STOP_LOC_ROOM && stop->dock_room) {
                /* For room-type stops on airships, check if we've stopped */
                /* (Airship landing is handled by direct placement) */
                ship->schedule_state = SCHEDULE_STATE_ARRIVING;
            }

            /* Ensure ship keeps moving toward target */
            if (ship->ship_power <= SHIP_SPEED_STOPPED
                && ship->schedule_state == SCHEDULE_STATE_TRAVELING) {
                if (ship->ship_type == SHIP_AIR_SHIP && ship->ship_power == SHIP_SPEED_LANDED)
                    ship->ship_power = SHIP_SPEED_HALF_SPEED;
                else
                    ship->ship_power = SHIP_SPEED_FULL_SPEED;

                ship_set_move_steps(ship);
                if (ship->ship_move <= 0)
                    ship->ship_move = UMAX(1, idx->move_delay);
            }
            break;
        }

        case SCHEDULE_STATE_ARRIVING: {
            /* Stop the ship and open dock exits */
            SHIP_SCHEDULE_STOP *stop = ship->schedule_current_stop;
            if (!stop)
                break;

            /* Stop ship at dock */
            if (ship->ship_type == SHIP_AIR_SHIP)
                ship->ship_power = SHIP_SPEED_LANDED;
            else
                ship->ship_power = SHIP_SPEED_STOPPED;

            ship->steering.turning_dir = 0;
            ship->move_steps = 0;
            ship->ship_move = 0;

            /* Clear navigation target */
            memset(&ship->seek_point, 0, sizeof(ship->seek_point));

            /* Create dock exits */
            ship_schedule_create_dock_exit(ship, stop);

            /* Initialize dwell timer */
            ship->schedule_dwell = stop->dwell_ticks;

            /* Announce arrival */
            if (stop->name[0] != '\0')
                boat_echo(ship, formatf("{YThe vessel has arrived at %s.{x", stop->name));
            else
                boat_echo(ship, "{YThe vessel has arrived at its destination.{x");

            ship->schedule_state = SCHEDULE_STATE_DOCKED;
            break;
        }

        case SCHEDULE_STATE_DOCKED: {
            SHIP_SCHEDULE_STOP *stop = ship->schedule_current_stop;
            if (!stop)
                break;

            /* Check departure conditions */
            bool should_depart = false;

            /* Hour-based departure */
            if (stop->depart_hour >= 0) {
                if (time_info.hour == stop->depart_hour)
                    should_depart = true;
            }

            /* Dwell-based departure */
            if (stop->dwell_ticks > 0) {
                ship->schedule_dwell--;
                if (ship->schedule_dwell <= 0)
                    should_depart = true;
            }

            if (should_depart)
                ship->schedule_state = SCHEDULE_STATE_DEPARTING;
            break;
        }

        case SCHEDULE_STATE_DEPARTING: {
            SHIP_SCHEDULE_STOP *stop = ship->schedule_current_stop;

            /* Announce departure */
            if (stop && stop->name[0] != '\0')
                boat_echo(ship, formatf("{YThe vessel departs from %s.{x", stop->name));
            else
                boat_echo(ship, "{YThe vessel departs.{x");

            /* Remove dock exits */
            ship_schedule_remove_dock_exit(ship);

            /* Advance to next stop */
            int next_idx = ship->schedule_stop_idx + 1;

            if (next_idx >= num_stops) {
                if (idx->schedule_loop) {
                    next_idx = 0;
                } else {
                    /* Ping-pong: reverse the schedule.
                     * For simplicity, wrap to last stop and go backward.
                     * We'll handle reverse by just resetting to 0 for now
                     * (a full reverse would require tracking direction). */
                    next_idx = 0;
                }
            }

            ship->schedule_stop_idx = next_idx;
            ship->schedule_current_stop = ship_schedule_find_stop(idx, next_idx);

            if (!ship->schedule_current_stop) {
                ship->schedule_state = SCHEDULE_STATE_IDLE;
                break;
            }

            /* Navigate to next stop */
            SHIP_SCHEDULE_STOP *next_stop = ship->schedule_current_stop;
            if (next_stop->location_type == STOP_LOC_WILDERNESS) {
                WILDS_DATA *wilds = get_wilds_from_uid(NULL, next_stop->wilds_uid);
                if (wilds) {
                    ship_npc_set_seek_goal(ship, wilds,
                        next_stop->loc_x, next_stop->loc_y);
                    ship->seek_navigator = false;
                }
            }

            /* Start moving */
            if (ship->ship_type == SHIP_AIR_SHIP)
                ship->ship_power = SHIP_SPEED_HALF_SPEED;
            else
                ship->ship_power = SHIP_SPEED_FULL_SPEED;

            ship_set_move_steps(ship);
            if (ship->ship_move <= 0)
                ship->ship_move = UMAX(1, idx->move_delay);

            if (ship->steering.heading < 0) {
                int heading = number_range(0, 359);
                ship->steering.heading = heading;
                ship->steering.heading_target = heading;
                steering_calc_heading(ship);
            }

            ship->schedule_state = SCHEDULE_STATE_TRAVELING;
            break;
        }
    }
}

/**
 * npc_ship_state_update - Per-ship NPC AI state machine tick
 *
 * Called once per tick for each NPC-flagged ship. Drives behavior
 * transitions based on the current state and NPC type:
 *
 * States: STOPPED -> SAILING -> CHASING -> ATTACKING -> FLEEING
 *
 * NPC type influences aggression and flee thresholds:
 *   - PIRATE: aggressive, attacks traders/weaker ships
 *   - COAST_GUARD: attacks pirates, protects traders
 *   - BOUNTY_HUNTER: attacks specific trigger targets
 *   - TRADER: non-aggressive, flees when attacked
 *   - ADVENTURER: mild aggression, explores
 *
 * @param ship  Ship to update (must have npc_ship attached)
 */
void npc_ship_state_update(SHIP_DATA *ship)
{
    NPC_SHIP_DATA *npc;
    int npc_type;

    if (!IS_VALID(ship) || !ship->npc_ship || !ship->index)
        return;

    npc = ship->npc_ship;
    npc_type = ship->index->npc_type;

    /* Transport ships use the schedule system exclusively */
    if (IS_SET(ship->ship_flags, SHIP_TRANSPORT)
        && ship->index->schedule_stops
        && list_size(ship->index->schedule_stops) > 0) {
        /* Auto-initialize transport state on first tick */
        if (npc->state != NPC_SHIP_STATE_TRANSPORT)
            npc->state = NPC_SHIP_STATE_TRANSPORT;
        ship_schedule_tick(ship);
        return;
    }

    /* Flee check: if health drops below threshold, enter flee state */
    if (ship->hit > 0 && ship->index->hit > 0) {
        int health_pct = (ship->hit * 100) / ship->index->hit;
        int flee_threshold;

        switch (npc_type) {
            case NPC_SHIP_TRADER:        flee_threshold = 60; break;
            case NPC_SHIP_ADVENTURER:    flee_threshold = 40; break;
            case NPC_SHIP_COAST_GUARD:   flee_threshold = 25; break;
            case NPC_SHIP_BOUNTY_HUNTER: flee_threshold = 20; break;
            case NPC_SHIP_PIRATE:        flee_threshold = 15; break;
            default:                     flee_threshold = 30; break;
        }

        if (health_pct <= flee_threshold && npc->state != NPC_SHIP_STATE_FLEEING) {
            npc->state = NPC_SHIP_STATE_FLEEING;
            npc->trigger_char = NULL;

            /* Break off combat */
            ship->ship_attacked = NULL;
            ship->char_attacked = NULL;

            /* Set random flee heading away from current direction */
            int flee_heading = (ship->steering.heading + 180 + number_range(-45, 45)) % 360;
            steering_set_heading(ship, flee_heading);

            if (ship->ship_power < SHIP_SPEED_FULL_SPEED)
                ship->ship_power = SHIP_SPEED_FULL_SPEED;

            return;
        }
    }

    switch (npc->state) {
        case NPC_SHIP_STATE_STOPPED:
            /* Transition to sailing — autopilot handles movement */
            npc->state = NPC_SHIP_STATE_SAILING;
            break;

        case NPC_SHIP_STATE_SAILING:
            /* Autopilot handles movement; scan for targets */
            if (ship->npc_goal_cooldown <= 0) {
                SHIP_DATA *target = npc_ship_scan_for_target(ship);
                if (target) {
                    /* Engage: begin chasing */
                    ship->ship_chased = target;
                    memcpy(ship->ship_chased_uid, target->id, sizeof(target->id));
                    npc->state = NPC_SHIP_STATE_CHASING;

                    /* Set seek point toward target */
                    if (target->ship && target->ship->in_room) {
                        ship_npc_set_seek_goal(ship,
                            target->ship->in_room->wilds,
                            target->ship->in_room->x,
                            target->ship->in_room->y);
                        ship->seek_navigator = false;
                    }
                }
            }
            break;

        case NPC_SHIP_STATE_CHASING:
            /* Pursuing a target — check if in weapon range to engage */
            if (!IS_VALID(ship->ship_chased)) {
                npc->state = NPC_SHIP_STATE_SAILING;
                ship->ship_chased = NULL;
            } else {
                int dist = ship_distance(ship, ship->ship_chased);
                int max_range = ship_get_max_weapon_range(ship);

                if (max_range > 0 && dist <= max_range) {
                    /* In range — open fire */
                    ship->ship_attacked = ship->ship_chased;
                    memcpy(ship->ship_attacked_uid, ship->ship_chased_uid,
                        sizeof(ship->ship_chased_uid));
                    ship->attack_position = SHIP_ATTACK_LOADING;
                    npc->state = NPC_SHIP_STATE_ATTACKING;

                    boat_echo(ship->ship_chased,
                        formatf("{R** %s opens fire on your vessel! **{x",
                            ship->ship_name ? ship->ship_name : "A ship"));
                } else if (ship->ship_chased->ship &&
                           ship->ship_chased->ship->in_room) {
                    /* Update seek point to follow moving target */
                    ship_npc_set_seek_goal(ship,
                        ship->ship_chased->ship->in_room->wilds,
                        ship->ship_chased->ship->in_room->x,
                        ship->ship_chased->ship->in_room->y);
                }
            }
            break;

        case NPC_SHIP_STATE_ATTACKING:
            /* In combat — handled by ship_combat_update */
            if (!IS_VALID(ship->ship_attacked)) {
                npc->state = NPC_SHIP_STATE_SAILING;
                ship->ship_attacked = NULL;
                ship->char_attacked = NULL;
            }
            break;

        case NPC_SHIP_STATE_FLEEING:
            /* Keep fleeing until health recovers or safe */
            if (ship->hit > 0 && ship->index->hit > 0) {
                int health_pct = (ship->hit * 100) / ship->index->hit;
                if (health_pct > 50) {
                    npc->state = NPC_SHIP_STATE_SAILING;
                }
            }
            break;

        case NPC_SHIP_STATE_BOARDING:
            /* Boarding actions — future implementation */
            break;
    }
}
