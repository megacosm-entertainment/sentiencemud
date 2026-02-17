#include <stdio.h>
#include <string.h>

#include "merc.h"
#include "tables.h"
#include "sectors_runtime.h"
#include "io/json/json_sectors.h"

static bool sector_runtime_booted = false;
static SECTOR_RUNTIME_DATA sector_runtime[SECT_MAX];

extern int16_t movement_loss[SECT_MAX];

enum {
    SRCLASS_NONE = 0,
    SRCLASS_ABYSS,
    SRCLASS_AIR,
    SRCLASS_ARCTIC,
    SRCLASS_CITY,
    SRCLASS_DESERT,
    SRCLASS_DUNGEON,
    SRCLASS_FOREST,
    SRCLASS_HAZARDOUS,
    SRCLASS_HILLS,
    SRCLASS_JUNGLE,
    SRCLASS_MOUNTAINS,
    SRCLASS_NETHER,
    SRCLASS_PLAINS,
    SRCLASS_ROADS,
    SRCLASS_SUBARCTIC,
    SRCLASS_SWAMP,
    SRCLASS_UNDERGROUND,
    SRCLASS_VULCAN,
    SRCLASS_WATER
};

static const struct flag_type sector_runtime_classes[] = {
    { "none",         SRCLASS_NONE,        true },
    { "abyss",        SRCLASS_ABYSS,       true },
    { "air",          SRCLASS_AIR,         true },
    { "arctic",       SRCLASS_ARCTIC,      true },
    { "city",         SRCLASS_CITY,        true },
    { "desert",       SRCLASS_DESERT,      true },
    { "dungeon",      SRCLASS_DUNGEON,     true },
    { "forest",       SRCLASS_FOREST,      true },
    { "hazardous",    SRCLASS_HAZARDOUS,   true },
    { "hills",        SRCLASS_HILLS,       true },
    { "jungle",       SRCLASS_JUNGLE,      true },
    { "mountains",    SRCLASS_MOUNTAINS,   true },
    { "nether",       SRCLASS_NETHER,      true },
    { "plains",       SRCLASS_PLAINS,      true },
    { "roads",        SRCLASS_ROADS,       true },
    { "subarctic",    SRCLASS_SUBARCTIC,   true },
    { "swamp",        SRCLASS_SWAMP,       true },
    { "underground",  SRCLASS_UNDERGROUND, true },
    { "vulcan",       SRCLASS_VULCAN,      true },
    { "water",        SRCLASS_WATER,       true },
    { NULL,             0,                   false }
};

static const struct flag_type sector_runtime_flagbank[] = {
    { "aerial",       SECTOR_AERIAL,      true },
    { "briars",       SECTOR_BRIARS,      true },
    { "city_lights",  SECTOR_CITY_LIGHTS, true },
    { "crumbles",     SECTOR_CRUMBLES,    true },
    { "deep_water",   SECTOR_DEEP_WATER,  true },
    { "drain_mana",   SECTOR_DRAIN_MANA,  true },
    { "flame",        SECTOR_FLAME,       true },
    { "frozen",       SECTOR_FROZEN,      true },
    { "hard_magic",   SECTOR_HARD_MAGIC,  true },
    { "indoors",      SECTOR_INDOORS,     true },
    { "melts",        SECTOR_MELTS,       true },
    { "nature",       SECTOR_NATURE,      true },
    { "no_fade",      SECTOR_NO_FADE,     true },
    { "no_gate",      SECTOR_NO_GATE,     true },
    { "no_gohall",    SECTOR_NO_GOHALL,   true },
    { "no_hide_obj",  SECTOR_NO_HIDE_OBJ, true },
    { "no_magic",     SECTOR_NO_MAGIC,    true },
    { "no_soil",      SECTOR_NO_SOIL,     true },
    { "sleep_drain",  SECTOR_SLEEP_DRAIN, true },
    { "slow_magic",   SECTOR_SLOW_MAGIC,  true },
    { "toxic",        SECTOR_TOXIC,       true },
    { "underwater",   SECTOR_UNDERWATER,  true },
    { NULL,             0,                false }
};

static void sector_clear_hide_messages(SECTOR_RUNTIME_DATA *sector)
{
    int i;

    for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
        if (sector->hide_msgs[i])
            free_string(sector->hide_msgs[i]);
        sector->hide_msgs[i] = NULL;
    }
}

static void sector_clear_affinities(SECTOR_RUNTIME_DATA *sector)
{
    int i;

    for (i = 0; i < SECTOR_MAX_AFFINITIES; i++) {
        sector->affinities[i].catalyst = CATALYST_NONE;
        sector->affinities[i].value = 0;
    }
}

static void sector_set_default_features(int id, SECTOR_RUNTIME_DATA *sector)
{
    sector->sector_class = SRCLASS_NONE;
    sector->flags = 0;
    sector->soil = 0;

    switch (id) {
    case SECT_INSIDE:
        sector->sector_class = SRCLASS_CITY;
        SET_BIT(sector->flags, SECTOR_CITY_LIGHTS);
        SET_BIT(sector->flags, SECTOR_INDOORS);
        sector->soil = -20;
        break;

    case SECT_CITY:
        sector->sector_class = SRCLASS_CITY;
        SET_BIT(sector->flags, SECTOR_CITY_LIGHTS);
        sector->soil = -10;
        break;

    case SECT_FIELD:
        sector->sector_class = SRCLASS_PLAINS;
        SET_BIT(sector->flags, SECTOR_NATURE);
        sector->soil = 5;
        break;

    case SECT_FOREST:
        sector->sector_class = SRCLASS_FOREST;
        SET_BIT(sector->flags, SECTOR_NATURE);
        break;

    case SECT_HILLS:
        sector->sector_class = SRCLASS_HILLS;
        SET_BIT(sector->flags, SECTOR_NATURE);
        break;

    case SECT_MOUNTAIN:
        sector->sector_class = SRCLASS_MOUNTAINS;
        SET_BIT(sector->flags, SECTOR_NATURE);
        sector->soil = -10;
        break;

    case SECT_WATER_SWIM:
    case SECT_WATER_NOSWIM:
    case SECT_UNDERWATER:
    case SECT_DEEP_UNDERWATER:
        sector->sector_class = SRCLASS_WATER;
        SET_BIT(sector->flags, SECTOR_NO_SOIL);
        if (id == SECT_WATER_NOSWIM || id == SECT_DEEP_UNDERWATER)
            SET_BIT(sector->flags, SECTOR_DEEP_WATER);
        if (id == SECT_UNDERWATER || id == SECT_DEEP_UNDERWATER)
            SET_BIT(sector->flags, SECTOR_UNDERWATER);
        break;

    case SECT_AIR:
        sector->sector_class = SRCLASS_AIR;
        SET_BIT(sector->flags, SECTOR_AERIAL);
        SET_BIT(sector->flags, SECTOR_NO_SOIL);
        break;

    case SECT_DESERT:
        sector->sector_class = SRCLASS_DESERT;
        sector->soil = 20;
        break;

    case SECT_CAVE:
        sector->sector_class = SRCLASS_UNDERGROUND;
        break;

    case SECT_SWAMP:
        sector->sector_class = SRCLASS_SWAMP;
        break;

    case SECT_JUNGLE:
        sector->sector_class = SRCLASS_JUNGLE;
        break;

    case SECT_NETHERWORLD:
        sector->sector_class = SRCLASS_NETHER;
        SET_BIT(sector->flags, SECTOR_NO_SOIL);
        break;

    case SECT_BRAMBLE:
        sector->sector_class = SRCLASS_HAZARDOUS;
        SET_BIT(sector->flags, SECTOR_BRIARS);
        break;

    case SECT_TOXIC_BOG:
        sector->sector_class = SRCLASS_HAZARDOUS;
        SET_BIT(sector->flags, SECTOR_TOXIC);
        break;

    case SECT_ENCHANTED_FOREST:
        sector->sector_class = SRCLASS_FOREST;
        SET_BIT(sector->flags, SECTOR_NATURE);
        SET_BIT(sector->flags, SECTOR_CRUMBLES);
        SET_BIT(sector->flags, SECTOR_SLEEP_DRAIN);
        break;

    case SECT_CURSED_SANCTUM:
        sector->sector_class = SRCLASS_HAZARDOUS;
        SET_BIT(sector->flags, SECTOR_HARD_MAGIC);
        SET_BIT(sector->flags, SECTOR_SLOW_MAGIC);
        SET_BIT(sector->flags, SECTOR_DRAIN_MANA);
        break;

    case SECT_LAVA:
        sector->sector_class = SRCLASS_VULCAN;
        SET_BIT(sector->flags, SECTOR_FLAME);
        SET_BIT(sector->flags, SECTOR_MELTS);
        SET_BIT(sector->flags, SECTOR_NO_SOIL);
        break;
    }
}

static const char *sector_default_name(int id)
{
    int i;

    for (i = 0; sector_flags[i].name != NULL; i++) {
        if (sector_flags[i].bit == id)
            return sector_flags[i].name;
    }

    return "unknown";
}

static void sector_seed_defaults(void)
{
    int i;

    for (i = 0; i < SECT_MAX; i++) {
        sector_runtime[i].id = i;

        if (sector_runtime[i].name)
            free_string(sector_runtime[i].name);
        if (sector_runtime[i].description)
            free_string(sector_runtime[i].description);
        if (sector_runtime[i].comments)
            free_string(sector_runtime[i].comments);

        sector_runtime[i].name = str_dup(sector_default_name(i));
        sector_runtime[i].description = str_dup("");
        sector_runtime[i].move_cost = movement_loss[i];
        sector_runtime[i].heal_rate = 100;
        sector_runtime[i].mana_rate = 100;
        sector_runtime[i].move_rate = 100;
        sector_runtime[i].comments = str_dup("");
        sector_clear_hide_messages(&sector_runtime[i]);
        sector_clear_affinities(&sector_runtime[i]);
        sector_set_default_features(i, &sector_runtime[i]);

        if (i == SECT_INSIDE || i == SECT_CITY)
            sector_runtime[i].hide_msgs[0] = str_dup("in the corner");
        else if (i == SECT_FIELD)
            sector_runtime[i].hide_msgs[0] = str_dup("among the grasses");
        else if (i == SECT_FOREST)
            sector_runtime[i].hide_msgs[0] = str_dup("in the thick vegetation");
    }
}

bool save_sector_data(void)
{
    load_sector_data();
    return json_sector_data_save(sector_runtime, SECT_MAX);
}

void load_sector_data(void)
{
    int i;

    if (sector_runtime_booted)
        return;

    sector_runtime_booted = true;
    sector_seed_defaults();

    if (!json_sector_data_load(sector_runtime, SECT_MAX)) {
        if (!json_sector_data_bootstrap_if_missing(sector_runtime, SECT_MAX))
            log_string("load_sector_data: Failed to seed sectors.json from defaults.");
    }

    for (i = 0; i < SECT_MAX; i++)
        movement_loss[i] = sector_runtime[i].move_cost;
}

bool reload_sector_data(void)
{
    int i;

    load_sector_data();
    sector_seed_defaults();

    if (!json_sector_data_load(sector_runtime, SECT_MAX)) {
        for (i = 0; i < SECT_MAX; i++)
            movement_loss[i] = sector_runtime[i].move_cost;
        return false;
    }

    for (i = 0; i < SECT_MAX; i++)
        movement_loss[i] = sector_runtime[i].move_cost;

    return true;
}

int sector_count(void)
{
    return SECT_MAX;
}

int sector_type_sanitize(int index)
{
    if (index < 0 || index >= SECT_MAX)
        return SECT_INSIDE;

    return index;
}

int room_sector_type(const ROOM_INDEX_DATA *room)
{
    load_sector_data();

    if (!room)
        return SECT_INSIDE;

    if (!room->sector)
        return SECT_INSIDE;

    return sector_type_sanitize(room->sector->id);
}

int room_set_sector_type(ROOM_INDEX_DATA *room, int sector_type)
{
    int normalized = sector_type_sanitize(sector_type);

    load_sector_data();

    if (!room)
        return normalized;

    room->sector = &sector_runtime[normalized];
    return normalized;
}

bool room_in_sector(const ROOM_INDEX_DATA *room, int sector_type)
{
    return room_sector_type(room) == sector_type_sanitize(sector_type);
}

bool room_sector_has_flag(const ROOM_INDEX_DATA *room, long flag)
{
    if (!room)
        return false;

    return sector_has_flag(room_sector_type(room), flag);
}

int room_rs_sector_type(const ROOM_INDEX_DATA *room)
{
    load_sector_data();

    if (!room)
        return SECT_INSIDE;

    if (!room->rs_sector)
        return SECT_INSIDE;

    return sector_type_sanitize(room->rs_sector->id);
}

int room_set_rs_sector_type(ROOM_INDEX_DATA *room, int sector_type)
{
    int normalized = sector_type_sanitize(sector_type);

    load_sector_data();

    if (!room)
        return normalized;

    room->rs_sector = &sector_runtime[normalized];
    return normalized;
}

bool sector_has_flag(int index, long flag)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return false;

    return IS_SET(sector_runtime[index].flags, flag);
}

const char *sector_name(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return "unknown";

    return sector_runtime[index].name ? sector_runtime[index].name : "unknown";
}

int sector_move_cost(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 1;

    return sector_runtime[index].move_cost;
}

int sector_heal_rate(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 100;

    return sector_runtime[index].heal_rate;
}

int sector_mana_rate(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 100;

    return sector_runtime[index].mana_rate;
}

const char *sector_comments(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return "";

    return sector_runtime[index].comments ? sector_runtime[index].comments : "";
}

int sector_lookup(const char *name)
{
    int i;

    load_sector_data();

    if (IS_NULLSTR(name))
        return NO_FLAG;

    for (i = 0; i < SECT_MAX; i++) {
        if (!str_prefix((char *)name, sector_runtime[i].name))
            return i;
    }

    return flag_value(sector_flags, (char *)name);
}

bool sector_set_name(int index, const char *name)
{
    if (index < 0 || index >= SECT_MAX || IS_NULLSTR(name))
        return false;

    load_sector_data();
    free_string(sector_runtime[index].name);
    sector_runtime[index].name = str_dup(name);
    return true;
}

const char *sector_description(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return "";

    return sector_runtime[index].description ? sector_runtime[index].description : "";
}

bool sector_set_description(int index, const char *description)
{
    if (index < 0 || index >= SECT_MAX || !description)
        return false;

    load_sector_data();
    free_string(sector_runtime[index].description);
    sector_runtime[index].description = str_dup(description);
    return true;
}

bool sector_set_move_cost(int index, int move_cost)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();

    sector_runtime[index].move_cost = URANGE(1, move_cost, 200);
    movement_loss[index] = sector_runtime[index].move_cost;
    return true;
}

bool sector_set_heal_rate(int index, int heal_rate)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].heal_rate = URANGE(1, heal_rate, 1000);
    return true;
}

bool sector_set_mana_rate(int index, int mana_rate)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].mana_rate = URANGE(1, mana_rate, 1000);
    return true;
}

int sector_move_rate(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 100;

    return sector_runtime[index].move_rate;
}

bool sector_set_move_rate(int index, int move_rate)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].move_rate = URANGE(1, move_rate, 1000);
    return true;
}

bool sector_set_comments(int index, const char *comments)
{
    if (index < 0 || index >= SECT_MAX || !comments)
        return false;

    load_sector_data();
    free_string(sector_runtime[index].comments);
    sector_runtime[index].comments = str_dup(comments);
    return true;
}

int sector_class(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return SRCLASS_NONE;

    return sector_runtime[index].sector_class;
}

bool sector_set_class(int index, int sector_class)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    if (flag_name(sector_runtime_classes, sector_class) == NULL)
        return false;

    load_sector_data();
    sector_runtime[index].sector_class = sector_class;
    return true;
}

long sector_runtime_flags_value(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 0;

    return sector_runtime[index].flags;
}

bool sector_set_runtime_flags(int index, long flags)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].flags = flags;
    return true;
}

int sector_soil(int index)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 0;

    return sector_runtime[index].soil;
}

bool sector_set_soil(int index, int soil)
{
    if (index < 0 || index >= SECT_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].soil = URANGE(-100, soil, 100);
    return true;
}

int sector_hide_msg_count(int index)
{
    int i;
    int count = 0;

    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 0;

    for (i = 0; i < SECTOR_MAX_HIDE_MSGS; i++) {
        if (!IS_NULLSTR(sector_runtime[index].hide_msgs[i]))
            count++;
    }

    return count;
}

const char *sector_hide_msg(int index, int slot)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return "";
    if (slot < 0 || slot >= SECTOR_MAX_HIDE_MSGS)
        return "";

    return sector_runtime[index].hide_msgs[slot] ? sector_runtime[index].hide_msgs[slot] : "";
}

bool sector_set_hide_msg(int index, int slot, const char *msg)
{
    if (index < 0 || index >= SECT_MAX)
        return false;
    if (slot < 0 || slot >= SECTOR_MAX_HIDE_MSGS)
        return false;
    if (!msg)
        return false;

    load_sector_data();

    if (sector_runtime[index].hide_msgs[slot])
        free_string(sector_runtime[index].hide_msgs[slot]);

    if (IS_NULLSTR(msg))
        sector_runtime[index].hide_msgs[slot] = NULL;
    else
        sector_runtime[index].hide_msgs[slot] = str_dup(msg);

    return true;
}

int sector_affinity_catalyst(int index, int slot)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return CATALYST_NONE;
    if (slot < 0 || slot >= SECTOR_MAX_AFFINITIES)
        return CATALYST_NONE;

    return sector_runtime[index].affinities[slot].catalyst;
}

int sector_affinity_value(int index, int slot)
{
    load_sector_data();

    if (index < 0 || index >= SECT_MAX)
        return 0;
    if (slot < 0 || slot >= SECTOR_MAX_AFFINITIES)
        return 0;

    return sector_runtime[index].affinities[slot].value;
}

bool sector_set_affinity(int index, int slot, int catalyst, int value)
{
    if (index < 0 || index >= SECT_MAX)
        return false;
    if (slot < 0 || slot >= SECTOR_MAX_AFFINITIES)
        return false;
    if (catalyst <= CATALYST_NONE || catalyst >= CATALYST_MAX)
        return false;

    load_sector_data();
    sector_runtime[index].affinities[slot].catalyst = catalyst;
    sector_runtime[index].affinities[slot].value = URANGE(-1000, value, 1000);
    return true;
}

bool sector_clear_affinity(int index, int slot)
{
    if (index < 0 || index >= SECT_MAX)
        return false;
    if (slot < 0 || slot >= SECTOR_MAX_AFFINITIES)
        return false;

    load_sector_data();
    sector_runtime[index].affinities[slot].catalyst = CATALYST_NONE;
    sector_runtime[index].affinities[slot].value = 0;
    return true;
}

const struct flag_type *sector_class_table(void)
{
    return sector_runtime_classes;
}

const struct flag_type *sector_runtime_flag_table(void)
{
    return sector_runtime_flagbank;
}
