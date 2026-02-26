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
#include "skill_data.h"
#include "wilds.h"
#include "wilderness_state.h"

static int weather_pick_storm_type(int region);
static AREA_REGION *weather_find_area_region_at(WILDS_DATA *pWilds, int x, int y);
static int weather_apply_severity_bias(int storm_type, int severity_bias, int region);
static int weather_target_storm_count(WILDS_DATA *pWilds);
static void weather_update_storms_for_wilds(WILDS_DATA *pWilds);
static void weather_update_wilderness_storms(void);
STORM_DATA* create_storm(AREA_DATA *pArea, int storm_type, int x, int y, int radius, float dx, float dy, int speed, int life);
void remove_storm(AREA_DATA *pArea, STORM_DATA *storm);

/* Send a weather echo only to awake characters who are outdoors. */
static void weather_gecho(const char *message)
{
    DESCRIPTOR_DATA *d;
    for (d = descriptor_list; d != NULL; d = d->next)
    {
        CHAR_DATA *ch;
        if (!d->character || d->connected != CON_PLAYING)
            continue;
        ch = d->original ? d->original : d->character;
        if (!ch->in_room || !IS_AWAKE(ch) || !IS_OUTSIDE(ch))
            continue;
        send_to_char(message, ch);
    }
}

static int weather_clamp_mmhg(int mmhg)
{
    if (mmhg < 960)
        return 960;
    if (mmhg > 1040)
        return 1040;
    return mmhg;
}

static int weather_base_storm_type(void)
{
    if (weather_info.sky == SKY_LIGHTNING)
        return WEATHER_LIGHTNING_STORM;

    if (weather_info.sky == SKY_RAINING)
    {
        if (time_info.month == 0 || time_info.month == 1 || time_info.month == 11)
            return WEATHER_SNOW_STORM;
        return WEATHER_RAIN_STORM;
    }

    return WEATHER_NONE;
}

static bool weather_region_is_ocean(int region)
{
    switch (region)
    {
    case REGION_NORTHERN_OCEAN:
    case REGION_WESTERN_OCEAN:
    case REGION_CENTRAL_OCEAN:
    case REGION_EASTERN_OCEAN:
    case REGION_SOUTHERN_OCEAN:
        return true;
    default:
        return false;
    }
}

static bool weather_region_is_polar(int region)
{
    return region == REGION_NORTH_POLE || region == REGION_SOUTH_POLE;
}

static AREA_REGION *weather_find_area_region_at(WILDS_DATA *pWilds, int x, int y)
{
    ROOM_INDEX_DATA *room;
    AREA_REGION *region;

    if (!pWilds || !pWilds->pArea)
        return NULL;

    room = get_wilds_vroom(pWilds, x, y);
    region = get_room_region(room);

    if (region)
        return region;

    return &pWilds->pArea->region;
}

static int weather_apply_severity_bias(int storm_type, int severity_bias, int region)
{
    int roll;
    int bias;

    if (storm_type <= WEATHER_NONE || severity_bias == 0)
        return storm_type;

    if (severity_bias > 0)
    {
        roll = number_percent();

        if (storm_type == WEATHER_RAIN_STORM && roll <= severity_bias)
            return WEATHER_LIGHTNING_STORM;

        if (storm_type == WEATHER_SNOW_STORM && !weather_region_is_polar(region)
            && roll <= (severity_bias / 2))
            return WEATHER_LIGHTNING_STORM;

        if (storm_type == WEATHER_LIGHTNING_STORM && roll <= (severity_bias / 2))
            return WEATHER_HURRICANE;

        if (storm_type == WEATHER_HURRICANE && roll <= (severity_bias / 3))
            return WEATHER_TORNADO;

        return storm_type;
    }

    bias = -severity_bias;
    roll = number_percent();

    if (storm_type == WEATHER_TORNADO && roll <= bias)
        return WEATHER_HURRICANE;

    if (storm_type == WEATHER_HURRICANE && roll <= bias)
        return WEATHER_LIGHTNING_STORM;

    if (storm_type == WEATHER_LIGHTNING_STORM && roll <= bias)
    {
        if (weather_region_is_polar(region) || time_info.month == 0 || time_info.month == 1 || time_info.month == 11)
            return WEATHER_SNOW_STORM;
        return WEATHER_RAIN_STORM;
    }

    return storm_type;
}

static int weather_pick_storm_type(int region)
{
    bool oceanic = weather_region_is_ocean(region);
    bool polar = weather_region_is_polar(region);

    switch (weather_info.sky)
    {
    default:
    case SKY_CLOUDLESS:
        return WEATHER_NONE;

    case SKY_CLOUDY:
        if (polar)
            return (number_percent() < 45) ? WEATHER_SNOW_STORM : WEATHER_NONE;
        return (number_percent() < (oceanic ? 80 : 60)) ? WEATHER_RAIN_STORM : WEATHER_NONE;

    case SKY_RAINING:
        if (polar || time_info.month == 0 || time_info.month == 1 || time_info.month == 11)
            return (number_percent() < 80) ? WEATHER_SNOW_STORM : WEATHER_LIGHTNING_STORM;
        if (oceanic)
            return (number_percent() < 85) ? WEATHER_RAIN_STORM : WEATHER_LIGHTNING_STORM;
        return (number_percent() < 70) ? WEATHER_RAIN_STORM : WEATHER_LIGHTNING_STORM;

    case SKY_LIGHTNING:
        if (polar)
            return (number_percent() < 70) ? WEATHER_SNOW_STORM : WEATHER_LIGHTNING_STORM;

        if (oceanic)
        {
            int roll = number_percent();
            if (roll <= 45)
                return WEATHER_HURRICANE;
            if (roll <= 90)
                return WEATHER_LIGHTNING_STORM;
            return WEATHER_TORNADO;
        }

        if (number_percent() < 65)
            return WEATHER_LIGHTNING_STORM;
        if (number_percent() < 85)
            return WEATHER_HURRICANE;
        return WEATHER_TORNADO;
    }
}

static int weather_target_storm_count_for_region(WILDS_DATA *pWilds, int target)
{
    int region;

    if (!pWilds)
        return target;

    region = pWilds->defaultRegion;

    if (weather_region_is_ocean(region))
        target = (target * 125) / 100;
    else if (weather_region_is_polar(region))
        target = (target * 120) / 100;
    else if (region == REGION_UNDERSEA)
        target = (target * 70) / 100;
    else if (region == REGION_MORDRAKE_ISLAND || region == REGION_TEMPLE_ISLAND
        || region == REGION_ARENA_ISLAND || region == REGION_DRAGON_ISLAND)
        target = (target * 110) / 100;

    if (pWilds->pArea)
    {
        int density_percent = URANGE(25, pWilds->pArea->region.weather_density_percent, 300);
        target = (target * density_percent) / 100;
    }

    return URANGE(0, target, 96);
}

static int weather_target_storm_count(WILDS_DATA *pWilds)
{
    long tiles;
    int base;

    if (!pWilds)
        return 0;

    tiles = (long)pWilds->map_size_x * (long)pWilds->map_size_y;
    base = (int)URANGE(1, tiles / 120000L, 12);

    switch (weather_info.sky)
    {
    default:
    case SKY_CLOUDLESS: return 0;
    case SKY_CLOUDY: return UMAX(1, base);
    case SKY_RAINING: return UMAX(2, base * 2);
    case SKY_LIGHTNING: return UMAX(3, base * 3);
    }
}

static void weather_update_storms_for_wilds(WILDS_DATA *pWilds)
{
    AREA_DATA *pArea;
    STORM_DATA *storm;
    STORM_DATA *storm_next;
    int storms = 0;
    int target;
    bool storms_changed = false;

    if (!pWilds || !pWilds->pArea)
        return;

    pArea = pWilds->pArea;

    for (storm = pArea->storm; storm != NULL; storm = storm_next)
    {
        storm_next = storm->next;
        storms++;
        storms_changed = true;

        storm->life--;

        if (number_percent() < 20)
            storm->counter += number_range(-35, 35);
        else
            storm->counter += number_range(-5, 10);

        storm->counter %= 360;
        if (storm->counter < 0)
            storm->counter += 360;

        storm->dx = cos(storm->counter * 3.1415926 / 180.0);
        storm->dy = sin(storm->counter * 3.1415926 / 180.0);

        storm->x += (int)round(storm->dx * storm->speed);
        storm->y += (int)round(storm->dy * storm->speed);

        if (storm->x < 0 || storm->y < 0
            || storm->x >= pWilds->map_size_x || storm->y >= pWilds->map_size_y
            || storm->life <= 0)
        {
            remove_storm(pArea, storm);
            free_storm_data(storm);
            storms--;
        }
    }

    target = weather_target_storm_count(pWilds);
    target = weather_target_storm_count_for_region(pWilds, target);
    {
        int attempts = 0;
        int max_attempts = UMAX(24, target * 10);

        while (storms < target && attempts < max_attempts)
    {
        int x;
        int y;
        int region;
        AREA_REGION *area_region;
        int density_percent;
        int life_percent;
        int severity_bias;
        int radius;
        int speed;
        int life;
        int angle;
        int storm_type;
        float dx;
        float dy;

        attempts++;

        x = number_range(0, UMAX(0, pWilds->map_size_x - 1));
        y = number_range(0, UMAX(0, pWilds->map_size_y - 1));
        region = get_wilds_effective_region(pWilds, x, y);
        if (region == REGION_UNKNOWN)
            region = pWilds->defaultRegion;

        area_region = weather_find_area_region_at(pWilds, x, y);
        if (!area_region)
            area_region = &pArea->region;

        density_percent = URANGE(25, area_region->weather_density_percent, 300);
        life_percent = URANGE(25, area_region->weather_life_percent, 300);
        severity_bias = URANGE(-100, area_region->weather_severity_bias, 100);

        if (density_percent < 100 && number_percent() > density_percent)
            continue;

        storm_type = weather_pick_storm_type(region);
        storm_type = weather_apply_severity_bias(storm_type, severity_bias, region);
        if (storm_type == WEATHER_NONE)
            continue;

        angle = number_range(0, 359);
        dx = cos(angle * 3.1415926 / 180.0);
        dy = sin(angle * 3.1415926 / 180.0);

        life = number_range(20, 90);
        speed = number_range(1, 3);

        switch (storm_type)
        {
        default:
        case WEATHER_RAIN_STORM: radius = number_range(18, 40); break;
        case WEATHER_LIGHTNING_STORM: radius = number_range(14, 28); speed = number_range(2, 4); break;
        case WEATHER_SNOW_STORM: radius = number_range(12, 24); break;
        case WEATHER_HURRICANE: radius = number_range(10, 18); speed = number_range(3, 5); break;
        case WEATHER_TORNADO: radius = number_range(7, 12); speed = number_range(3, 6); life = number_range(12, 40); break;
        }

        if (weather_region_is_ocean(region))
        {
            if (storm_type == WEATHER_RAIN_STORM || storm_type == WEATHER_LIGHTNING_STORM || storm_type == WEATHER_HURRICANE)
            {
                radius = (radius * 12) / 10;
                life = (life * 13) / 10;
            }

            if (storm_type == WEATHER_LIGHTNING_STORM || storm_type == WEATHER_HURRICANE)
                speed++;
        }
        else if (weather_region_is_polar(region))
        {
            if (storm_type == WEATHER_RAIN_STORM && number_percent() < 80)
                storm_type = WEATHER_SNOW_STORM;

            life = (life * 12) / 10;
            speed = UMAX(1, speed - 1);
        }
        else if (region == REGION_UNDERSEA)
        {
            if (storm_type == WEATHER_TORNADO)
                storm_type = WEATHER_HURRICANE;

            radius = (radius * 9) / 10;
            life = (life * 8) / 10;
        }

        life = (life * life_percent) / 100;

        if (severity_bias != 0)
        {
            radius += severity_bias / 10;
            speed += severity_bias / 25;
        }

        if (density_percent > 100 && number_percent() <= UMIN(75, density_percent - 100))
            radius = (radius * 11) / 10;

        radius = URANGE(5, radius, 80);
        speed = URANGE(1, speed, 8);
        life = URANGE(8, life, 220);

        create_storm(pArea, storm_type, x, y, radius, dx, dy, speed, life);
        storms++;
        storms_changed = true;
    }
    }

    if (storms_changed)
        wilderness_state_mark_dirty(pWilds, "weather_storm_tick");
}

static void weather_update_wilderness_storms(void)
{
    AREA_DATA *area;
    WILDS_DATA *pWilds;

    for (area = area_first; area; area = area->next)
    {
        for (pWilds = area->wilds; pWilds; pWilds = pWilds->next)
            weather_update_storms_for_wilds(pWilds);
    }
}

void update_weather(void)
{
    int diff;
    int old_sky;

    if (time_info.month >= 9 && time_info.month <= 16)
        diff = (weather_info.mmhg > 985 ? -2 : 2);
    else
        diff = (weather_info.mmhg > 1015 ? -2 : 2);

    weather_info.change += diff * number_range(1, 4)
        + number_range(2, 8)
        - number_range(2, 8);

    weather_info.change = URANGE(-12, weather_info.change, 12);

    weather_info.mmhg += weather_info.change;
    weather_info.mmhg = weather_clamp_mmhg(weather_info.mmhg);

    old_sky = weather_info.sky;

    if (weather_info.mmhg <= 980)
        weather_info.sky = SKY_LIGHTNING;
    else if (weather_info.mmhg <= 1000)
        weather_info.sky = SKY_RAINING;
    else if (weather_info.mmhg <= 1020)
        weather_info.sky = SKY_CLOUDY;
    else
        weather_info.sky = SKY_CLOUDLESS;

    if (game_settings.weather_storms_enabled)
        weather_update_wilderness_storms();
    update_weather_for_chars();

    if (old_sky == weather_info.sky)
        return;

    switch (weather_info.sky)
    {
    default:
    case SKY_CLOUDLESS:
        weather_gecho("{YThe clouds part and the sky clears.{x\n\r");
        break;
    case SKY_CLOUDY:
        weather_gecho("{WClouds gather and dim the sky.{x\n\r");
        break;
    case SKY_RAINING:
        if (time_info.month == 0 || time_info.month == 1 || time_info.month == 11)
            weather_gecho("{WCold winds gather and snow begins to fall.{x\n\r");
        else
            weather_gecho("{CThe clouds open and rain begins to fall.{x\n\r");
        break;
    case SKY_LIGHTNING:
        weather_gecho("{YLightning crackles across the sky as the storm intensifies.{x\n\r");
        break;
    }
}

int weather_movement_modifier(ROOM_INDEX_DATA *from_room, ROOM_INDEX_DATA *to_room, int base_move)
{
    int storm_type;
    int extra;
    bool overland;

    if (base_move < 1)
        return 0;

    overland = (from_room && IS_WILDERNESS(from_room)) || (to_room && IS_WILDERNESS(to_room));
    if (!overland)
        return 0;

    storm_type = weather_base_storm_type();
    switch (storm_type)
    {
    default:
    case WEATHER_NONE:
        return 0;

    case WEATHER_RAIN_STORM:
    case WEATHER_SNOW_STORM:
        extra = UMAX(1, base_move / 4);
        break;

    case WEATHER_LIGHTNING_STORM:
        extra = UMAX(1, base_move / 2);
        break;
    }

    if ((from_room && room_in_sector(from_room, SECT_WATER_SWIM))
        || (from_room && room_in_sector(from_room, SECT_WATER_NOSWIM))
        || (to_room && room_in_sector(to_room, SECT_WATER_SWIM))
        || (to_room && room_in_sector(to_room, SECT_WATER_NOSWIM)))
    {
        extra += UMAX(1, extra / 2);
    }

    return extra;
}

/**
 * With storms we only get the most important storm. If a tornado and a rain storm are over a city,
 * then only the tornado will have affect. Simplifies having to worry about different
 * combinations of storms hitting an area.
 *
 *
 * Basically wilderness has a list of storms, you can check whether an area is in the wilds as it will
 * have x and y area coords set. The actual storms are added to wilderness area, and affected_by_storm and
 * storm_close are set on areas if their x and y coords fall in the radius of the storms.
 *
 * I have lots of code duplication with working out which is the most important storm. This is due to
 * being too lazy to come up with something better.
 */

// Creates a new storm
STORM_DATA* create_storm(AREA_DATA *pArea, int storm_type, int x, int y, int radius, float dx, float dy, int speed, int life) {
  STORM_DATA *storm = NULL;

  // create new storm structure
  storm = new_storm_data();

  // Add storm to area
  storm->next  = pArea->storm;
  pArea->storm = storm;

  // Set parameters
  storm->storm_type = storm_type;
  storm->x = x;
  storm->y = y;
  storm->radius = radius;

  // Direction and speed
  storm->dx = dx;
  storm->dy = dy;
  storm->counter = number_range(0, 360);
  storm->speed = speed;
  storm->life = life;

  plogf(LOG_INFO, "Create storm type %d at %d %d of radius %d in direction %f %f at speed %d and life %d",
     storm_type, x, y, radius, dx, dy, speed, life);

  return storm;
}

// Remove the storm
void remove_storm(AREA_DATA *pArea, STORM_DATA *storm) {
    if (storm == pArea->storm)
    {
        pArea->storm = storm->next;
    }
    else
    {
        STORM_DATA *prev;

        for (prev = pArea->storm; prev != NULL;
                prev = prev->next)
        {
            if (prev->next == storm)
            {
                prev->next = storm->next;
                break;
            }
        }
    }
    storm->next     = NULL;
}

static const char *weather_storm_type_name(int storm_type)
{
    switch (storm_type)
    {
    case WEATHER_RAIN_STORM: return "rain";
    case WEATHER_LIGHTNING_STORM: return "lightning";
    case WEATHER_SNOW_STORM: return "snow";
    case WEATHER_HURRICANE: return "hurricane";
    case WEATHER_TORNADO: return "tornado";
    default: return "unknown";
    }
}

static int weather_storm_type_from_name(const char *name)
{
    if (IS_NULLSTR(name))
        return WEATHER_NONE;

    if (!str_prefix(name, "rain"))
        return WEATHER_RAIN_STORM;
    if (!str_prefix(name, "lightning") || !str_prefix(name, "storm"))
        return WEATHER_LIGHTNING_STORM;
    if (!str_prefix(name, "snow"))
        return WEATHER_SNOW_STORM;
    if (!str_prefix(name, "hurricane"))
        return WEATHER_HURRICANE;
    if (!str_prefix(name, "tornado"))
        return WEATHER_TORNADO;

    return WEATHER_NONE;
}

static bool weather_get_storm_context(CHAR_DATA *ch, ROOM_INDEX_DATA **room_out, AREA_DATA **area_out, WILDS_DATA **wilds_out)
{
    ROOM_INDEX_DATA *pRoom = NULL;
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;

    if (!ch || !ch->in_room)
        return false;

    if (ON_SHIP(ch) && ch->in_room->ship && ch->in_room->ship->ship && ch->in_room->ship->ship->in_room)
        pRoom = ch->in_room->ship->ship->in_room;
    else
        pRoom = ch->in_room;

    if (!pRoom || !pRoom->wilds || !pRoom->area)
        return false;

    pArea = pRoom->area;
    pWilds = pRoom->wilds;

    if (!pArea || !pWilds)
        return false;

    if (room_out)
        *room_out = pRoom;
    if (area_out)
        *area_out = pArea;
    if (wilds_out)
        *wilds_out = pWilds;

    return true;
}

static STORM_DATA *weather_get_storm_by_index(AREA_DATA *pArea, int index)
{
    STORM_DATA *storm;
    int current = 1;

    if (!pArea || index < 1)
        return NULL;

    for (storm = pArea->storm; storm; storm = storm->next)
    {
        if (current == index)
            return storm;
        current++;
    }

    return NULL;
}

bool weather_handle_storm_command(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom;
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    char subcmd[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    char arg5[MAX_INPUT_LENGTH];
    char arg6[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if (!IS_IMMORTAL(ch))
    {
        send_to_char("Only immortals can use weather storm controls.\n\r", ch);
        return true;
    }

    if (!weather_get_storm_context(ch, &pRoom, &pArea, &pWilds))
    {
        send_to_char("Weather storm controls require wilderness context (in wilds or on a wilderness ship).\n\r", ch);
        return true;
    }

    argument = one_argument(argument, subcmd);

    if (IS_NULLSTR(subcmd) || !str_prefix(subcmd, "help"))
    {
        send_to_char("Syntax: weather storm list\n\r", ch);
        send_to_char("        weather storm spawn <type> [x y [radius [speed [life [angle]]]]]\n\r", ch);
        send_to_char("        weather storm move <index> <x> <y>\n\r", ch);
        send_to_char("        weather storm set <index> <type|radius|speed|life|counter|angle> <value>\n\r", ch);
        send_to_char("        weather storm delete <index>\n\r", ch);
        send_to_char("        weather storm clear\n\r", ch);
        send_to_char("Types: rain lightning snow hurricane tornado\n\r", ch);
        return true;
    }

    if (!str_prefix(subcmd, "list"))
    {
        STORM_DATA *storm;
        int index = 1;

        sprintf(buf, "Active storms for wilds '%s' (uid=%ld):\n\r", pWilds->name, pWilds->uid);
        send_to_char(buf, ch);

        for (storm = pArea->storm; storm; storm = storm->next)
        {
            sprintf(buf,
                "  [%d] type=%s(%d) pos=%d,%d radius=%d speed=%d life=%d dir=(%.2f,%.2f) counter=%d\n\r",
                index,
                weather_storm_type_name(storm->storm_type),
                storm->storm_type,
                storm->x,
                storm->y,
                storm->radius,
                storm->speed,
                storm->life,
                storm->dx,
                storm->dy,
                storm->counter);
            send_to_char(buf, ch);
            index++;
        }

        if (index == 1)
            send_to_char("  (none)\n\r", ch);

        return true;
    }

    if (!str_prefix(subcmd, "spawn") || !str_prefix(subcmd, "create"))
    {
        int storm_type;
        int x;
        int y;
        int radius = 20;
        int speed = 2;
        int life = 60;
        int angle = number_range(0, 359);
        float dx;
        float dy;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);
        argument = one_argument(argument, arg4);
        argument = one_argument(argument, arg5);
        argument = one_argument(argument, arg6);

        storm_type = weather_storm_type_from_name(arg1);
        if (storm_type == WEATHER_NONE)
        {
            send_to_char("Specify storm type: rain lightning snow hurricane tornado\n\r", ch);
            return true;
        }

        x = pRoom->x;
        y = pRoom->y;

        if (!IS_NULLSTR(arg2) && !IS_NULLSTR(arg3))
        {
            if (!is_number(arg2) || !is_number(arg3))
            {
                send_to_char("Coordinates must be numeric.\n\r", ch);
                return true;
            }

            x = atoi(arg2);
            y = atoi(arg3);
        }

        if (!IS_NULLSTR(arg4))
        {
            if (!is_number(arg4))
            {
                send_to_char("Radius must be numeric.\n\r", ch);
                return true;
            }
            radius = atoi(arg4);
        }

        if (!IS_NULLSTR(arg5))
        {
            if (!is_number(arg5))
            {
                send_to_char("Speed must be numeric.\n\r", ch);
                return true;
            }
            speed = atoi(arg5);
        }

        if (!IS_NULLSTR(arg6))
        {
            if (!is_number(arg6))
            {
                send_to_char("Life must be numeric.\n\r", ch);
                return true;
            }
            life = atoi(arg6);

            if (!IS_NULLSTR(argument))
            {
                char arg7[MAX_INPUT_LENGTH];
                one_argument(argument, arg7);
                if (is_number(arg7))
                    angle = atoi(arg7);
            }
        }

        x = URANGE(0, x, UMAX(0, pWilds->map_size_x - 1));
        y = URANGE(0, y, UMAX(0, pWilds->map_size_y - 1));
        radius = URANGE(2, radius, 120);
        speed = URANGE(1, speed, 12);
        life = URANGE(1, life, 1000);
        angle %= 360;
        if (angle < 0)
            angle += 360;

        dx = cos(angle * 3.1415926 / 180.0);
        dy = sin(angle * 3.1415926 / 180.0);

        create_storm(pArea, storm_type, x, y, radius, dx, dy, speed, life);
        wilderness_state_mark_dirty(pWilds, "weather_storm_spawn");

        sprintf(buf,
            "Spawned %s storm at %d,%d radius=%d speed=%d life=%d angle=%d.\n\r",
            weather_storm_type_name(storm_type), x, y, radius, speed, life, angle);
        send_to_char(buf, ch);
        return true;
    }

    if (!str_prefix(subcmd, "move"))
    {
        STORM_DATA *storm;
        int index;
        int x;
        int y;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || !is_number(arg2) || !is_number(arg3))
        {
            send_to_char("Syntax: weather storm move <index> <x> <y>\n\r", ch);
            return true;
        }

        index = atoi(arg1);
        x = atoi(arg2);
        y = atoi(arg3);

        storm = weather_get_storm_by_index(pArea, index);
        if (!storm)
        {
            send_to_char("No storm with that index.\n\r", ch);
            return true;
        }

        storm->x = URANGE(0, x, UMAX(0, pWilds->map_size_x - 1));
        storm->y = URANGE(0, y, UMAX(0, pWilds->map_size_y - 1));

        wilderness_state_mark_dirty(pWilds, "weather_storm_move");
        sprintf(buf, "Moved storm [%d] to %d,%d.\n\r", index, storm->x, storm->y);
        send_to_char(buf, ch);
        return true;
    }

    if (!str_prefix(subcmd, "set"))
    {
        STORM_DATA *storm;
        int index;
        int value;

        argument = one_argument(argument, arg1);
        argument = one_argument(argument, arg2);
        argument = one_argument(argument, arg3);

        if (!is_number(arg1) || IS_NULLSTR(arg2) || IS_NULLSTR(arg3) || !is_number(arg3))
        {
            send_to_char("Syntax: weather storm set <index> <type|radius|speed|life|counter|angle> <value>\n\r", ch);
            return true;
        }

        index = atoi(arg1);
        value = atoi(arg3);
        storm = weather_get_storm_by_index(pArea, index);
        if (!storm)
        {
            send_to_char("No storm with that index.\n\r", ch);
            return true;
        }

        if (!str_prefix(arg2, "type"))
        {
            storm->storm_type = URANGE(WEATHER_RAIN_STORM, value, WEATHER_TORNADO);
        }
        else if (!str_prefix(arg2, "radius"))
        {
            storm->radius = URANGE(1, value, 200);
        }
        else if (!str_prefix(arg2, "speed"))
        {
            storm->speed = URANGE(0, value, 20);
        }
        else if (!str_prefix(arg2, "life"))
        {
            storm->life = URANGE(0, value, 5000);
        }
        else if (!str_prefix(arg2, "counter"))
        {
            storm->counter = value;
        }
        else if (!str_prefix(arg2, "angle"))
        {
            int angle = value % 360;
            if (angle < 0)
                angle += 360;
            storm->counter = angle;
            storm->dx = cos(angle * 3.1415926 / 180.0);
            storm->dy = sin(angle * 3.1415926 / 180.0);
        }
        else
        {
            send_to_char("Unknown field. Use: type radius speed life counter angle\n\r", ch);
            return true;
        }

        wilderness_state_mark_dirty(pWilds, "weather_storm_set");
        sprintf(buf, "Updated storm [%d] field '%s'.\n\r", index, arg2);
        send_to_char(buf, ch);
        return true;
    }

    if (!str_prefix(subcmd, "delete") || !str_prefix(subcmd, "remove"))
    {
        STORM_DATA *storm;
        int index;

        argument = one_argument(argument, arg1);
        if (!is_number(arg1))
        {
            send_to_char("Syntax: weather storm delete <index>\n\r", ch);
            return true;
        }

        index = atoi(arg1);
        storm = weather_get_storm_by_index(pArea, index);
        if (!storm)
        {
            send_to_char("No storm with that index.\n\r", ch);
            return true;
        }

        remove_storm(pArea, storm);
        free_storm_data(storm);
        wilderness_state_mark_dirty(pWilds, "weather_storm_delete");
        sprintf(buf, "Deleted storm [%d].\n\r", index);
        send_to_char(buf, ch);
        return true;
    }

    if (!str_prefix(subcmd, "clear"))
    {
        STORM_DATA *storm;
        STORM_DATA *storm_next;
        int count = 0;

        for (storm = pArea->storm; storm; storm = storm_next)
        {
            storm_next = storm->next;
            remove_storm(pArea, storm);
            free_storm_data(storm);
            count++;
        }

        wilderness_state_mark_dirty(pWilds, "weather_storm_clear");
        sprintf(buf, "Cleared %d storm(s).\n\r", count);
        send_to_char(buf, ch);
        return true;
    }

    send_to_char("Unknown weather storm subcommand. Use: list spawn move set delete clear\n\r", ch);
    return true;
}

bool weather_show_forecast(CHAR_DATA *ch)
{
    STORM_DATA *storm;
    ROOM_INDEX_DATA *pRoom = NULL;
    AREA_DATA *pArea;
    char buf[MAX_STRING_LENGTH];
    int x, y;
    int squares_to_show_x;
    int squares_to_show_y;
    int chx, chy;
    bool found_any_storm = false;

    if (!ch || !ch->in_room)
        return false;

    if (ON_SHIP(ch) && ch->in_room->ship && ch->in_room->ship->ship && ch->in_room->ship->ship->in_room)
        pRoom = ch->in_room->ship->ship->in_room;
    else if (IN_WILDERNESS(ch))
        pRoom = ch->in_room;
    else
        return false;

    if (!pRoom || !pRoom->wilds)
        return false;

    pArea = pRoom->area;
    chx = pRoom->x;
    chy = pRoom->y;

    squares_to_show_x = 18;
    squares_to_show_y = 9;

    buf[0] = '\0';

    for (y = 0; y < squares_to_show_y * 2; y++)
    {
        for (x = 0; x < squares_to_show_x * 2; x++)
        {
            int lx = chx - squares_to_show_x + x;
            int ly = chy - squares_to_show_y + y;
            bool found_rain_storm = false;
            bool found_lightning_storm = false;
            bool found_snow_storm = false;
            bool found_hurricane = false;
            bool found_tornado = false;

            for (storm = pArea->storm; storm != NULL; storm = storm->next)
            {
                long dx = (long)storm->x - (long)lx;
                long dy = (long)storm->y - (long)ly;
                long dist2 = dx * dx + dy * dy;
                long radius2 = (long)storm->radius * (long)storm->radius;

                if (dist2 > radius2)
                    continue;

                found_any_storm = true;
                switch (storm->storm_type)
                {
                case WEATHER_RAIN_STORM: found_rain_storm = true; break;
                case WEATHER_LIGHTNING_STORM: found_lightning_storm = true; break;
                case WEATHER_SNOW_STORM: found_snow_storm = true; break;
                case WEATHER_HURRICANE: found_hurricane = true; break;
                case WEATHER_TORNADO: found_tornado = true; break;
                default: break;
                }
            }

            if (x == squares_to_show_x && y == squares_to_show_y)
                strcat(buf, "{M@{x");
            else if (found_tornado)
                strcat(buf, "{GT{x");
            else if (found_hurricane)
                strcat(buf, "{RH{x");
            else if (found_snow_storm)
                strcat(buf, "{WS{x");
            else if (found_lightning_storm)
                strcat(buf, "{YL{x");
            else if (found_rain_storm)
                strcat(buf, "{BR{x");
            else
                strcat(buf, ".");
        }
        strcat(buf, "\n\r");
    }

    if (!found_any_storm)
        strcat(buf, "{DNo major storm fronts are currently nearby.{x\n\r");

    strcat(buf,
        "{GT{x - Tornado   {RH{x - Hurricane   {WS{x - Snow Storm   {YL{x - Lightning Storm\n\r"
        "{BR{x - Rain Storm\n\r");

    send_to_char(buf, ch);
    return true;
}

// handles weather update for wilderness and netherworld
/*void update_weather() {
  AREA_DATA *pArea = NULL;
  AREA_DATA *pArea2 = NULL;
  STORM_DATA *storm = NULL;
  STORM_DATA *storm_next = NULL;
  char buf[MSL];
  int storms = 0;
  int min_storms = 30;

  log_string("Updating weather...");

  // get wilderness area
  if ((pArea = find_area("Wilderness")) == NULL)
      return;

  // check number of storms on map
  for (storm = pArea->storm; storm != NULL; storm = storm_next) {
    storm_next = storm->next;

    // increase count of storms
    storms++;

    // decrease life of storm
    storm->life--;

    // update the storm (direction etc)
    storm->x += storm->dx * storm->speed;
    storm->y += storm->dy * storm->speed;

    // increase counter
    if (number_percent() < 10) {
      storm->counter += number_range(-180, 180);
    }
    else {
      storm->counter++;
    }

    // Once it hits 360 back to 0 again
    storm->counter = storm->counter % 360;

    // update directions
    //storm->dx = cos( storm->counter * 3.1415926 / 180 );
    //storm->dy = sin( storm->counter * 3.1415926 / 180 );


    if (storm->x < 0 || storm->y < 0 || storm->life < 0) {
      remove_storm(pArea, storm);
    }


  //sprintf(buf, "Updated storm type %d at %d %d of radius %d in direction %f %f at speed %d and life %d",
   //  storm->storm_type, storm->x, storm->y, storm->radius, storm->dx, storm->dy, storm->speed, storm->life);
  //log_string(buf);

    // iterate through all areas, if they are in the wilderness show warning of storm
    for (pArea2 = area_first; pArea2 != NULL; pArea2 = pArea2->next) {
      pArea2->affected_by_storm = NULL;
      pArea2->storm_close = NULL;

      if (pArea2->x > 0 && pArea2->y > 0) {
         long distance;

                 distance = (int) sqrt( 					\
                                    ( storm->x - pArea2->x ) *	\
                                    ( storm->x - pArea2->x ) +	\
                                    ( storm->y - pArea2->y ) *	\
                                    ( storm->y - pArea2->y ) );

         if (distance < storm->radius) {
             if ((storm->storm_type == WEATHER_TORNADO && pArea2->affected_by_storm != NULL && pArea2->affected_by_storm->storm_type == WEATHER_HURRICANE) ||
              (storm->storm_type == WEATHER_HURRICANE && pArea2->affected_by_storm != NULL && pArea2->affected_by_storm->storm_type == WEATHER_SNOW_STORM) ||
              (storm->storm_type == WEATHER_SNOW_STORM && pArea2->affected_by_storm != NULL && pArea2->affected_by_storm->storm_type == WEATHER_LIGHTNING_STORM) ||
              (storm->storm_type == WEATHER_LIGHTNING_STORM && pArea2->affected_by_storm != NULL && pArea2->affected_by_storm->storm_type == WEATHER_RAIN_STORM)) {
               pArea2->affected_by_storm = storm;
                         }
             else {
               pArea2->affected_by_storm = storm;
             }
         }
         else
         if (distance < storm->radius + 10) {
           if (pArea2->storm_close != NULL) {
             if ((storm->storm_type == WEATHER_TORNADO && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_HURRICANE) ||
              (storm->storm_type == WEATHER_HURRICANE && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_SNOW_STORM) ||
              (storm->storm_type == WEATHER_SNOW_STORM && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_LIGHTNING_STORM) ||
              (storm->storm_type == WEATHER_LIGHTNING_STORM && pArea2->storm_close != NULL && pArea2->storm_close->storm_type == WEATHER_RAIN_STORM)) {
               pArea2->storm_close = storm;
                         }
             else {
               pArea2->storm_close = storm;
             }
           }
         }

         if (pArea2->affected_by_storm != NULL) {
         sprintf(buf, "%s's is affected by type %d at %d, %d",
              pArea2->name, pArea2->affected_by_storm->storm_type, pArea2->affected_by_storm->x, pArea2->affected_by_storm->y);
         log_string(buf);
                 }

         if (pArea2->storm_close != NULL) {
         sprintf(buf, "%s's closest storm is storm type %d at %d, %d.",
              pArea2->name,
              pArea2->storm_close->storm_type,
              pArea2->storm_close->x,
              pArea2->storm_close->y);
         log_string(buf);
         }
      }

    }
  }

  // if storms < 30 create some more
  while (storms < min_storms) {
    int x, y, radius, speed, life;
    float dx, dy;
    int angle = 0;
    int storm_type;

    // increase storms counter as we are making another
    storms++;

    x = number_range(0, pArea->map_size_x);
    y = number_range(0, pArea->map_size_y);

    angle = number_range(0, 360);
    dx = cos(angle * 3.1415926 / 180);
    dy = sin(angle * 3.1415926 / 180);

    life = number_range(20, 100);
    speed = number_range(2, 5);

    storm_type = number_range(1, 5);
    switch(storm_type) {
      case WEATHER_RAIN_STORM:

           radius = number_range(22, 55);

           if (number_percent() < 40) {
                        create_storm(pArea, WEATHER_LIGHTNING_STORM, x, y, radius-4, dx, dy, speed, life);
                     }
           if (number_percent() < 20) {
                        create_storm(pArea, WEATHER_SNOW_STORM, x, y, radius-7, dx, dy, speed, life);
           }
           if (number_percent() < 15) {
                        create_storm(pArea, WEATHER_HURRICANE, x, y, radius-9, dx, dy, speed, life);
           }
           break;
      case WEATHER_LIGHTNING_STORM:

           radius = number_range(20, 25);
           break;
      case WEATHER_SNOW_STORM:

           radius = number_range(15, 20 );
           break;
      case WEATHER_HURRICANE:

           radius = number_range(15, 20);
                 speed = number_range(4, 7);
           break;
      case WEATHER_TORNADO:

           radius = number_range(10, 15);
                 speed = number_range(4, 8);
           break;
      default:
           radius = 10;
    }

   // create storm
   create_storm(pArea, storm_type, x, y, radius, dx, dy, speed, life);
  }
}

// Commenting this out, since Whisp's weather system doesn't work yet -- Areo
void do_weather(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    STORM_DATA *storm = NULL;
    char buf[MAX_STRING_LENGTH];
    int x, y;
        int squares_to_show_x = 0;
        int squares_to_show_y = 0;
    int chx, chy;
    ROOM_INDEX_DATA *pRoom;

        squares_to_show_x = get_squares_to_show_x(4);
        squares_to_show_y = get_squares_to_show_y(4);

    // Get area
    pArea = ch->in_room->area;

    buf[0] = '\0';

    if (ON_SHIP(ch)) {
      pRoom = ch->in_room->ship->ship->in_room;
      pArea = pRoom->area;
      chx = pRoom->x;
      chy = pRoom->y;
    }
    else
    if (IN_WILDERNESS(ch)) {
      chx = ch->in_room->x;
      chy = ch->in_room->y;
    }
    else
    if (IS_OUTSIDE(ch) && pArea->x > 0 && pArea->y > 0) {
      chx = pArea->x;
      chy = pArea->y;
    }
    else {
            send_to_char("You can't see the weather here.\n\r", ch);
            return;
    }

    if (IN_NETHERWORLD(ch)) {
    send_to_char("Thick rolling clouds tumble and turn. Lightning crashes to the ground all around you.\n\r", ch);
    return;
    }

    // Run through storms in area
    for (y = 0; y < squares_to_show_y*2; y++) {
        for (x = 0; x < squares_to_show_x*2; x++) {
          int lx, ly;
                    bool found_rain_storm = false;
                    bool found_lightning_storm = false;
                    bool found_snow_storm = false;
                    bool found_hurricane = false;
                    bool found_tornado = false;

          lx = chx - squares_to_show_x + x;
          ly = chy - squares_to_show_y + y;

          for (storm = pArea->storm; storm != NULL; storm = storm->next) {
               double field = 0;
                         double radius = 0;

                         radius = (double)1 / (double)(2*storm->radius * storm->radius);

                             field =  (double) 1 /
                                                (double)( ( storm->x - lx ) *	\
                                                ( storm->x - lx ) +	\
                                                ( storm->y - ly ) *	\
                                                ( storm->y - ly ) );

                if ( field >= radius ) {
                    switch(storm->storm_type) {
                                            case WEATHER_RAIN_STORM:
                                                     found_rain_storm = true;
                                                     break;
                                            case WEATHER_LIGHTNING_STORM:
                                                      found_lightning_storm = true;
                                                     break;
                                            case WEATHER_SNOW_STORM:
                                                      found_snow_storm = true;
                                                     break;
                                            case WEATHER_HURRICANE:
                                                      found_hurricane = true;
                                                     break;
                                            case WEATHER_TORNADO:
                                                      found_tornado = true;
                                                     break;
                                         }
                 }
                 //sprintf(buf2, "radius was %f, field was %f", radius, field);
                 //log_string(buf2);
                        }

            if (x == squares_to_show_x && y == squares_to_show_y) {
                strcat(buf, "{M@{x");
            }
            else
            if (found_tornado) {
                                strcat(buf, "{GT{x");
            }
            else
            if (found_hurricane) {
                                strcat(buf, "{RH{x");
            }
            else
            if (found_snow_storm) {
                                strcat(buf, "{WS{x");
            }
            else
            if (found_lightning_storm) {
                                strcat(buf, "{YL{x");
            }
            else
            if (found_rain_storm) {
                                strcat(buf, "{BR{x");
            }
            else {
               strcat(buf, ".");
            }
        }
        strcat(buf, "\n\r");
    }

    strcat(buf, "{GT{x - Tornado   {RH{x - Hurricane   {WS{x - Snow Storm   {YL{x - Lightning Storm\n\r"
                "{BR{x - Rain Storm\n\r");
    strcat(buf, "\0");

    send_to_char(buf, ch);
    return;
}*/

int get_storm_for_room(ROOM_INDEX_DATA *pRoom)
{
    AREA_DATA *pArea;
    AREA_DATA *pWilderness;
    STORM_DATA *storm = NULL;
    int lx, ly;
    int storm_type = WEATHER_NONE;
        bool found_rain_storm = false;
        bool found_lightning_storm = false;
        bool found_snow_storm = false;
        bool found_hurricane = false;
        bool found_tornado = false;

    // Get area
    pArea = pRoom->area;
    pWilderness = find_area("Wilderness");

    lx = pRoom->x;
    ly = pRoom->y;

    // Check if room is in an area affected by a storm
    if (pWilderness != NULL && str_cmp(pArea->name, pWilderness->name)) {

        if (pRoom->area->affected_by_storm != NULL) {
            storm_type = pRoom->area->affected_by_storm->storm_type;
        }
        // Close storms
        else
            if (pRoom->area->storm_close != NULL) {
                storm_type = pRoom->area->storm_close->storm_type;
            }
    }
    else {

        // Run through all storms in the wilderness and see which is affecting room
        for (storm = pRoom->area->storm; storm != NULL; storm = storm->next) {
            int distance;

            distance = (int) sqrt( 					\
                    ( storm->x - lx ) *	\
                    ( storm->x - lx ) +	\
                    ( storm->y - ly ) *	\
                    ( storm->y - ly ) );

         if (distance < storm->radius) {
                    switch(storm->storm_type) {
                                case WEATHER_RAIN_STORM:
                                         found_rain_storm = true;
                                         break;
                                case WEATHER_LIGHTNING_STORM:
                                         found_lightning_storm = true;
                                         break;
                                case WEATHER_SNOW_STORM:
                                         found_snow_storm = true;
                                         break;
                                case WEATHER_HURRICANE:
                                         found_hurricane = true;
                                         break;
                                case WEATHER_TORNADO:
                                         found_tornado = true;
                                         break;
                            }
             }
      }

            if (found_tornado) {
                 storm_type = WEATHER_TORNADO;
            }
            else
            if (found_hurricane) {
                 storm_type = WEATHER_HURRICANE;
            }
            else
            if (found_snow_storm) {
                 storm_type = WEATHER_SNOW_STORM;
            }
            else
            if (found_lightning_storm) {
                 storm_type = WEATHER_LIGHTNING_STORM;
            }
            else
            if (found_rain_storm) {
                 storm_type = WEATHER_RAIN_STORM;
            }
            else {
          // Check if any storms are close
                    for (storm = pRoom->area->storm; storm != NULL; storm = storm->next) {
                         int distance;

                         distance = (int) sqrt( 					\
                                            ( storm->x - lx ) *	\
                                            ( storm->x - lx ) +	\
                                            ( storm->y - ly ) *	\
                                            ( storm->y - ly ) );

                         if (distance < storm->radius + 10) {
                            switch(storm->storm_type) {
                                        case WEATHER_RAIN_STORM:
                                                 found_rain_storm = true;
                                                 break;
                                        case WEATHER_LIGHTNING_STORM:
                                                 found_lightning_storm = true;
                                                 break;
                                        case WEATHER_SNOW_STORM:
                                                 found_snow_storm = true;
                                                 break;
                                        case WEATHER_HURRICANE:
                                                 found_hurricane = true;
                                                 break;
                                        case WEATHER_TORNADO:
                                                 found_tornado = true;
                                                 break;
                                    }
                         }
         }
                if (found_tornado) {
                     storm_type = WEATHER_TORNADO;
                }
                else
                if (found_hurricane) {
                     storm_type = WEATHER_HURRICANE;
                }
                else
                if (found_snow_storm) {
                     storm_type = WEATHER_SNOW_STORM;
                }
                else
                if (found_lightning_storm) {
                     storm_type = WEATHER_LIGHTNING_STORM;
                }
                else
                if (found_rain_storm) {
                     storm_type = WEATHER_RAIN_STORM;
                }
              else {
           storm_type = WEATHER_NONE;
        }
            }
        }
    if (storm_type == WEATHER_NONE)
        storm_type = weather_base_storm_type();

    return storm_type;
}

// Affect chars caught in storm.
void storm_affect_char args((CHAR_DATA *ch, int storm_type)) {
  ROOM_INDEX_DATA *pRoom;
  long index = 0;
  AREA_DATA *pArea;
        int room_sector = room_sector_type(ch->in_room);
  AFFECT_DATA af;
  memset(&af,0,sizeof(af));

  switch(storm_type) {

    // Rain
    case WEATHER_RAIN_STORM:


      // Dont show too much information about it raining
      if (number_percent() < 33) {
      // Show that is is raining
            switch(number_range(0, 5)) {
                case 0:
                        send_to_char("{CThe rain falls harder causing the puddles to splash everywhere.{x\n\r", ch);
                        break;
                case 1:
                        send_to_char("{CThe rain soften to a quiet eerie whisper.{x\n\r", ch);
                        break;
                case 2:
                        send_to_char("{CRain from the clouds above you saturate the surrounding area.{x\n\r", ch);
                        break;
                case 3:
                        send_to_char("{CYou are saturated with water from the rain.{x\n\r", ch);
                        break;
                case 4:
                        send_to_char("{CRain falls from above splashing as it hits the ground.{x\n\r", ch);
                        break;
        default:
                    switch(room_sector) {
                        case SECT_CITY:
                            send_to_char("{CRain makes a melodic drumbeat as it falls on the rooftops.{x\r\n", ch);
                            break;
                        case SECT_FOREST:
                            send_to_char("{CThe raindrops falling on the forest around you makes a soothing sound.{x\r\n", ch);
                            break;
                        case SECT_FIELD:
                            send_to_char("{CThe thirsty fields drink up the rain as it begins falling.{x\r\n", ch);
                            break;
                        case SECT_MOUNTAIN:
              send_to_char("{CThe rain trickles down into the mountain valleys.{x\r\n", ch);
                            break;
                        case SECT_DOCK:
              send_to_char("{CThe rain beats chaotically on the wooden boards holding the pier.{x\r\n", ch);
                            break;
                        case SECT_DESERT:
              send_to_char("{CThe desert enjoys its rare taste of rain as it spatters into the sand.{x\r\n", ch);
                            break;
                        case SECT_WATER_NOSWIM:
              send_to_char("{CThe rain beats chaotic on the surface.{x\r\n", ch);
                            break;
                        default:
              send_to_char("{CRain pours mercilessly down upon you.{x\r\n", ch);
                    }
          }
      }
      break;

    // Snow
    case WEATHER_SNOW_STORM:

      // Dont show too much information about it raining
      if (number_percent() < 33) {
      // Show that is is snowing
            switch(number_range(0, 8)) {
                case 0:
                        send_to_char("{WSnow flakes fall gently to the ground.{x\n\r", ch);
                        break;
                case 1:
                        send_to_char("{WSnow litters the ground as it pours down from the clouds.{x\n\r", ch);
                        break;
                case 2:
                        send_to_char("{WThe snowfall quickens and the temperature drops.{x\n\r", ch);
                        break;
                case 3:
                        send_to_char("{WCold snow powers to the ground from the clouds above.{x\n\r", ch);
                        break;
                case 4:
                        send_to_char("{WThe temperature drops as snow falls from the sky.{x\n\r", ch);
                        break;
        case 5:
        case 6:
        case 7:
            cold_effect(ch, 1, dice(4,8), TARGET_CHAR);
          break;
        default:
                    switch(room_sector) {
                        case SECT_CITY:
                            send_to_char("{WBig fluffy flakes of snow collect in the city streets.{x\r\n", ch);
                            break;
                        case SECT_FOREST:
                            send_to_char("{WFlakes of snow fall softly, escaping the canopy of leaves above.{x\r\n", ch);
                            break;
                        case SECT_FIELD:
                            send_to_char("{WSnowflakes collect in the field, freezing the undergrowth.{x\r\n", ch);
                            break;
                        case SECT_MOUNTAIN:
              send_to_char("{WThe thick snow obscures the nearby mountains.{x\r\n", ch);
                            break;
                        case SECT_DOCK:
              send_to_char("{WThe rain beats chaotically on the wooden boards holding the pier.{x\r\n", ch);
                            break;
                        case SECT_DESERT:
              send_to_char("{WThe desert enjoys its rare taste of rain as it spatters into the sand.{x\r\n", ch);
                            break;
                        case SECT_WATER_NOSWIM:
              send_to_char("{WThe snowflakes melt instantly as they touch the water.{x\r\n", ch);
                            break;
                        default:
              send_to_char("{WBig, fat, fluffy flakes fall from a heavy-laden sky.{x\n\r", ch);
                    }
            }
            break;
        }

    // Lightning
    case WEATHER_LIGHTNING_STORM:

      // Dont show too much information about lightning storm
      if (number_percent() < 33) {

      // Show that there is a lightning storm
            switch(number_range(0, 7)) {
                case 0:
                        send_to_char("A billow of thunder explodes around you.\n\r", ch);
                        break;
                case 1:
                            send_to_char("{YFlashes of lightning streak across the sky.{x\n\r", ch);
                        break;
                case 2:
            if (!room_in_sector(ch->in_room, SECT_WATER_SWIM) &&
                !room_in_sector(ch->in_room, SECT_WATER_NOSWIM)) {
                            send_to_char("The low rumble of thunder shakes the ground.\n\r", ch);
            }
                        break;
                case 3:
                        send_to_char("{YBrilliant flashes light up the clouds above.{x\n\r", ch);
                        break;
                case 4:
            if (!room_in_sector(ch->in_room, SECT_WATER_SWIM) &&
                !room_in_sector(ch->in_room, SECT_WATER_NOSWIM)) {
                            send_to_char("{YLightning crashes to the ground next to you!{x\n\r", ch);
            }
                        break;
                case 5:
                        if (number_percent() < 30 && !IS_SET(ch->in_room->room_flag[0], ROOM_SAFE) && ch->fighting == NULL)
                        {
                            act("{YZAAAAAAAAAAAAAAP! You are struck by a bolt from the sky...{x\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                            damage(ch, ch, number_range(500,30000), 0, DAM_LIGHTNING, false);
                        }
                        break;
        default:
                    switch(room_sector) {
                        case SECT_CITY:
                            send_to_char("{BThunder echos off the building walls.\r\n{x", ch);
                            break;
                        case SECT_FOREST:
                            send_to_char("{YLightning flashes above the canopy of the forest.\r\n{x", ch);
                            break;
                        case SECT_FIELD:
                            send_to_char("{YForks of lightning streaks down across the fields.\r\n{x", ch);
                            break;
                        case SECT_MOUNTAIN:
              send_to_char("{BThunder echos off the surrounding mountains.\r\n{x", ch);
              break;
                        case SECT_HILLS:
              send_to_char("{BThunder echos off the surrounding hills.\r\n{x", ch);
              break;
                        case SECT_TUNDRA:
              send_to_char("{YForks of lightning strike the tundra.\r\n{x", ch);
              break;
                        case SECT_AIR:
              send_to_char("{YLightning forks in all directions!\r\n{x", ch);
              break;
                        case SECT_DOCK:
              send_to_char("{CThe rain beats chaotically on the wooden boards holding the pier.\r\n{x", ch);
                            break;
                        case SECT_DESERT:
              send_to_char("{YLightning crackles across the open desert.\r\n{x", ch);
                            break;
                        case SECT_WATER_NOSWIM:
              send_to_char("Lightning sparks across the open water.\r\n", ch);
                            break;
                        case SECT_NETHERWORLD:
              send_to_char("{YLightning chaotically forks between the dead ground and toxic sky.\n\r", ch);
                            break;
                        default:
              send_to_char("{YLightning chaotically forks in the sky.\n\r", ch);
                    }
            }
      }
      break;

    // Hurricane
    case WEATHER_HURRICANE:

      // Show that there is a hurricane
            switch(number_range(0, 3)) {
                case 0:
                        send_to_char("The wind threatens to carry you away. Now would be an excellent time to get indoors!\n\r", ch);
                        break;
                case 1:
                        send_to_char("The wind whips around you angrily!\n\r", ch);
                        break;
                case 2:
            send_to_char("The wind screams around you with incredible force!\n\r", ch);
                        break;
                case 3:
            send_to_char("The wind howls fiercely as it torments all in its path!\n\r", ch);
                        break;
                case 4:
            send_to_char("The hurricane relentlessly destroys everything around you!\n\r", ch);
                        break;
                case 5:
                  send_to_char("Something caught in the wind hits you hard!{x\n\r", ch);
            send_to_char("Out of nowhere something slams solidly into you.\n\r", ch);
            act("{R$n is struck by an object caught in the storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                      damage(ch, ch, 30000, 0, DAM_NONE, false);
                        break;
        default:
                switch(room_sector) {
                        case SECT_CITY:
              if (number_percent() < 50)
                            send_to_char("A begger is caught by the wind and carried off!\r\n", ch);
                            else
                          send_to_char("Some fencing and a small chicken flies past you!\n\r", ch);
                            break;
                        case SECT_FOREST:
              if (number_percent() < 50) {
                                send_to_char("A tree is ripped from the grounds and carried off in the wind!\r\n", ch);
                          }
              else {
                              send_to_char("Trees around you begin to snap from the powerful winds!\n\r", ch);
                            }
                            break;
                        case SECT_FIELD:
              if (number_percent() < 50)
                            send_to_char("A powerfull wind roars across the fields.\r\n", ch);
              else
                          send_to_char("A cow go's shooting past you, taken by the hurricane winds!\n\r", ch);
                            break;
                        case SECT_MOUNTAIN:
              send_to_char("A mountain goat is carried away by the winds!\r\n", ch);
                            break;
                        case SECT_DOCK:
              send_to_char("The rough waters smash up against the pier!\r\n", ch);
                            break;
                        case SECT_DESERT:
              send_to_char("A sand storm forms from the powerful winds!\r\n", ch);
                            af.slot	= WEAR_NONE;
                            af.where	= TO_AFFECTS;
                            af.group	= AFFGROUP_PHYSICAL;
                            SKILL_DATA *sk_blind = skill_find("blindness");
                            af.type 	= skill_sn(sk_blind);
    af.skill = sk_blind;
                            af.level 	= 10;
                            af.duration	= 3;
                            af.location	= APPLY_HITROLL;
                            af.modifier	= -4;
                            af.bitvector 	= AFF_BLIND;
                            af.bitvector2 = 0;

                            affect_to_char(ch,&af);
              send_to_char("{DYou are blinded by the sand!{x\n\r", ch);
              act("$n is blinded by the blowing sand!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                            break;
                        case SECT_WATER_NOSWIM:
              send_to_char("The seas swell turning the waves into mountains!\r\n", ch);
                            break;
                        default:
              send_to_char("The powerful winds of the hurricane threaten to carry you away!\n\r", ch);
                    }
            }
      break;

    // Tornado
    case WEATHER_TORNADO:

      // Show that there is a tornado
            switch(number_range(0, 8)) {
        default:
                switch(room_sector) {
                        case SECT_CITY:
              if (number_percent() < 20)
                            send_to_char("An old man is caught by the wind and consumed by the tornado!\r\n", ch);
                            else
              if (number_percent() < 20)
                          send_to_char("The buildings groan as they are slowly disected by the tornado!\n\r", ch);
                            else
              if (number_percent() < 20)
              send_to_char("The litter in the city streets disappears into the tornado!\n\r", ch);
                            else
              if (number_percent() < 20)
              send_to_char("The stray cat whimpers as a the tonado consumes it!\n\r", ch);
                            break;
                        case SECT_FOREST:
              if (number_percent() < 50) {
                                send_to_char("A tree is ripped from the grounds and carried off in the wind!\r\n", ch);
                          }
              else {
                              send_to_char("Trees around you begin to snap from the powerful winds!\n\r", ch);
                            }
                            break;
                        case SECT_FIELD:
              if (number_percent() < 50)
                            send_to_char("The giant spiralling tornado sucks plants and crops as it devestates the fields.\r\n", ch);
              else
                          send_to_char("A cow shoots past you, taken by the tornado!\n\r", ch);
                            break;
                        case SECT_MOUNTAIN:
              send_to_char("The stones and rocks of the mountain spiral into the tornado!\r\n", ch);
                            break;
                        case SECT_DOCK:
              send_to_char("The rough waters smash up against the pier!\r\n", ch);
                            break;
                        case SECT_DESERT:
              send_to_char("A sand storm forms from the powerful winds!\r\n", ch);
                            af.slot	= WEAR_NONE;
                            af.where	= TO_AFFECTS;
                            af.group	= AFFGROUP_PHYSICAL;
                            af.type 	= skill_lookup("blindness");
                            af.level 	= 10;
                            af.duration	= 3;
                            af.skill = skill_find_uid(af.type);
                            af.location	= APPLY_HITROLL;
                            af.modifier	= -4;
                            af.bitvector 	= AFF_BLIND;
                            af.bitvector2 = 0;

                            affect_to_char(ch,&af);
              send_to_char("{DYou are blinded by the sand!{x\n\r", ch);
              act("$n is blinded by the blowing sand!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                            break;
                        case SECT_WATER_NOSWIM:
              send_to_char("The seas swell turning the waves into mountains!\r\n", ch);
                            break;
                }
                case 0:
                        send_to_char("The monstrous swirling storm brings devestation as it rips all that stand in front!\n\r", ch);
                        break;
                case 1:
            if (!room_in_sector(ch->in_room, SECT_WATER_SWIM) &&
                !room_in_sector(ch->in_room, SECT_WATER_NOSWIM)) {
                            send_to_char("The ground beneath you groans as the tornado tugs at its core.\n\r", ch);
                        }
                        break;
                case 2:
                        send_to_char("You struggle to save yourself from the tornado!\n\r", ch);
                        break;
                case 3:
            if (!room_in_sector(ch->in_room, SECT_WATER_SWIM) &&
                !room_in_sector(ch->in_room, SECT_WATER_NOSWIM)) {
                        send_to_char("Objects around you are torn from the earth and fed to the mighty tornado!\n\r", ch);
                        }
                        break;
                case 4:
                  send_to_char("The sky groans loudly as the clouds above swirl chaotically.\n\r", ch);
                        break;
                case 5:
                  send_to_char("{RYou are sucked up into the tornado!{x\n\r", ch);
            send_to_char("Out of nowhere something slams solidly into you.\n\r", ch);
            act("{R$n is struck by an object caught in the storm!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                      damage(ch, ch, 30000, 0, DAM_NONE, false);
                        break;
                case 6:
            pArea = find_area("Wilderness");
            index = (long)((long)number_range(0, pArea->map_size_y) * pArea->map_size_x + (long)number_range(0, pArea->map_size_x) + pArea->min_vnum + WILDERNESS_VNUM_OFFSET);

                  send_to_char("{RYou are sucked up into the tornado!{x\n\r", ch);

            act("{R$n is sucked up into the tornado!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

            AREA_DATA *tornado_area = NULL;
            WNUM wnum;
            if (resolve_widevnum(index, NULL, &wnum))
                tornado_area = wnum.pArea;
            if (!tornado_area) tornado_area = get_system_area_fallback();
            if ((pRoom = get_room_index(tornado_area, index)) != NULL) {
                char_from_room(ch);
                char_to_room(ch, pRoom);
                          do_function(ch, &do_look, "auto");
                        }

            if (MOUNTED(ch) && !IS_AFFECTED(MOUNTED(ch), AFF_FLYING)) {
              send_to_char("You are thrown wildly around in circles and find yourself plummeting to the ground!\n\r", ch);
                    send_to_char("{RYou hit the ground with a loud thump!!!{x\n\r", ch);
                           damage(ch, ch, 30000, 0, DAM_NONE, false);
            }
            else {
              send_to_char("You are thrown ferociously out of the tornado.\n\r", ch);
              send_to_char("{RYou hit the ground hard, you manage to fly up enough to soften your landing.\n\r", ch);
                           damage(ch, ch, number_range(10,ch->max_hit), 0, DAM_NONE, false);
            }
                        break;
            }
      break;
  }
}

// Affect chars who see storm in background.
void storm_affect_char_background args((CHAR_DATA *ch, int storm_type)) {

  switch(storm_type) {

    // None
    case WEATHER_NONE:
      switch(number_range(0,6)) {
         default:
            break;
         case 0:
            switch(weather_info.sunlight) {
              case SUN_RISE:
              case SUN_LIGHT:
                switch(number_range(0,3)) {
                  case 0:
                      send_to_char("The cloudless sky bathes you in sunshine.\n\r", ch);
                      break;
                  case 1:
                      send_to_char("The warm sunshine beams brightly around you.\n\r", ch);
                      break;
                  case 2:
                      send_to_char("The bright sunshine warms you.\n\r", ch);
                      break;
                  case 3:
                      send_to_char("The warm sun beams down through a cloudless blue sky.\n\r", ch);
                      break;
                                }
                break;
              case SUN_SET:
              case SUN_DARK:
                switch(number_range(0,3)) {
                  case 0:
                      send_to_char("The stars twinkle in the cloudless sky.\n\r", ch);
                      break;
                  case 1:
                      send_to_char("A shooting star snakes across the cloudless sky.\n\r", ch);
                      break;
                  case 2:
                      send_to_char("The land is illuminated by the moon.\n\r", ch);
                      break;
                  case 3:
                      send_to_char("The stars above you twinkle in the clear sky.\n\r", ch);
                      break;
                                }
                break;
                        }
      }
    break;

    // Rain
    case WEATHER_RAIN_STORM:
      switch(number_range(0,3)) {
        case 0:
            send_to_char("Dark rain clouds gather in the distance.\n\r", ch);
          break;
        case 1:
            send_to_char("A hazy mist of rain clouds loom in the distance.\n\r", ch);
          break;
      }
      break;

    // Snow
    case WEATHER_SNOW_STORM:
      switch(number_range(0,3)) {
        case 0:
            send_to_char("The temperature begins to cool as snow clouds loom.\n\r", ch);
          break;
        case 1:
            send_to_char("Gusts of wind blow snowflakes in.\n\r", ch);
          break;
      }
      break;

    // Lightning
    case WEATHER_LIGHTNING_STORM:
      switch(number_range(0,3)) {
        case 0:
            send_to_char("Thunder rolls in from the distant storm.\n\r", ch);
          break;
        case 1:
            send_to_char("{YFlashes of lightning appear in the distance.{x\n\r", ch);
          break;
      }
      break;

    // Hurricane
    case WEATHER_HURRICANE:
      switch(number_range(0,3)) {
        case 0:
            send_to_char("Their is a sudden calming stillness in the air.\n\r", ch);
          break;
        case 1:
            send_to_char("In the distance you see the foreboding signs of a hurricane!\n\r", ch);
          break;
      }
      break;

    // Tornado
    case WEATHER_TORNADO:
      switch(number_range(0,3)) {
        case 0:
            send_to_char("You see a large column of swiling wind causing devestation nearby!\n\r", ch);
          break;
        case 1:
            send_to_char("The defeaning roar of a tornado can be heard nearby!\n\r", ch);
          break;
      }
      break;
  }
}

void update_weather_for_chars(void)
{
    DESCRIPTOR_DATA *d;

    if (!game_settings.weather_ambience_enabled)
        return;

    for (d = descriptor_list; d != NULL; d = d->next)
    {
        CHAR_DATA *ch;
        int storm_type;
        int room_sector;

        if (!d->character || d->connected != CON_PLAYING)
            continue;

        ch = d->character;

        if (!ch->in_room || !IS_AWAKE(ch) || !IS_OUTSIDE(ch))
            continue;

        if (IN_NETHERWORLD(ch))
            storm_type = WEATHER_LIGHTNING_STORM;
        else if (IN_EDEN(ch))
            storm_type = WEATHER_NONE;
        else
            storm_type = get_storm_for_room(ch->in_room);

        if (number_percent() < 12)
            storm_affect_char_background(ch, storm_type);

        if (IS_IMMORTAL(ch))
            continue;

        if (storm_type == WEATHER_SNOW_STORM && number_percent() < 8)
        {
            cold_effect(ch, 1, dice(1, 6), TARGET_CHAR);
            if (number_percent() < 35)
                send_to_char("{WThe bitter air cuts through your clothing.{x\n\r", ch);
        }

        room_sector = room_sector_type(ch->in_room);
        if ((storm_type == WEATHER_SNOW_STORM || storm_type == WEATHER_HURRICANE)
            && (room_sector == SECT_ICE || room_sector == SECT_SNOW)
            && !IS_AFFECTED(ch, AFF_FLYING)
            && ch->fighting == NULL
            && ch->position > POS_RESTING
            && number_percent() < 4)
        {
            send_to_char("{WYou slip on the slick ground and fall!{x\n\r", ch);
            act("{W$n slips on the slick ground and falls!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            ch->position = POS_RESTING;
            WAIT_STATE(ch, PULSE_VIOLENCE);
        }
    }
}
