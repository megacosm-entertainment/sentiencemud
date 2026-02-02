/***************************************************************************
 *  JSON Instance Format - Instance/Ship/Dungeon Serialization             *
 *                                                                          *
 *  This file contains serialization for runtime instance data:            *
 *  - Instances (standalone and object-owned)                              *
 *  - Ships (contain an instance)                                          *
 *  - Dungeons (contain multiple instances)                                *
 *  - Uses WNUM format for area-scoped references                          *
 *                                                                          *
 *  Storage model:                                                          *
 *  - Ships:     persist/ships/ship_<uid>.json (self-contained)            *
 *  - Dungeons:  persist/dungeons/dungeon_<uid>.json (self-contained)      *
 *  - Instances: persist/instances/instance_<uid>.json (self-contained)    *
 *                                                                          *
 *  Each persisted file contains complete room state including all NPCs    *
 *  and objects, allowing full restoration of instance contents.           *
 ***************************************************************************/

#ifndef JSON_INSTANCE_H
#define JSON_INSTANCE_H

#include <jansson.h>
#include "merc.h"

/***************************************************************************
 * File Paths                                                              *
 ***************************************************************************/

#define INSTANCES_FILE_JSON     "data/world/instances.json"
#define INSTANCES_FILE_DAT      "data/world/instances.dat"

/* Individual persist directories */
#define PERSIST_SHIPS_DIR       "persist/ships/"
#define PERSIST_DUNGEONS_DIR    "persist/dungeons/"
#define PERSIST_INSTANCES_DIR   "persist/instances/"

/***************************************************************************
 * Instance Section Serialization                                          *
 *                                                                          *
 * Instance sections contain dynamically cloned rooms from blueprints.     *
 * Serialization captures the complete room state including contents.      *
 ***************************************************************************/

json_t *instance_section_to_json(INSTANCE_SECTION *section);
INSTANCE_SECTION *json_to_instance_section(json_t *json);

/***************************************************************************
 * Instance Serialization                                                  *
 *                                                                          *
 * Instances are complete procedural areas generated from blueprints.      *
 * They can be standalone, owned by ships, or owned by dungeons.           *
 ***************************************************************************/

json_t *instance_to_json(INSTANCE *instance);
INSTANCE *json_to_instance(json_t *json);

/***************************************************************************
 * Ship Serialization                                                      *
 *                                                                          *
 * Ships are mobile player-owned vehicles that contain an instance.        *
 * Includes crew, navigation waypoints, and physical position data.        *
 ***************************************************************************/

json_t *ship_to_json(SHIP_DATA *ship);
SHIP_DATA *json_to_ship(json_t *json);

/***************************************************************************
 * Dungeon Serialization                                                   *
 *                                                                          *
 * Dungeons are multi-floor instanced areas with multiple instances.       *
 * Each floor is a separate instance with linked entry/exit points.        *
 ***************************************************************************/

json_t *dungeon_to_json(DUNGEON *dungeon);
DUNGEON *json_to_dungeon(json_t *json);

/***************************************************************************
 * Load/Save Functions                                                     *
 ***************************************************************************/

/* Main batch functions - iterate all entities */
bool json_save_instances(void);
bool json_load_instances(void);

/* Individual persist functions - save single entity to file */
bool json_persist_save_ship(SHIP_DATA *ship);
SHIP_DATA *json_persist_load_ship(const char *filename);

bool json_persist_save_dungeon(DUNGEON *dungeon);
DUNGEON *json_persist_load_dungeon(const char *filename);

bool json_persist_save_instance(INSTANCE *instance);
INSTANCE *json_persist_load_instance(const char *filename);

/* Directory scanning - load all entities from persist directories */
int json_persist_load_all_ships(void);
int json_persist_load_all_dungeons(void);
int json_persist_load_all_instances(void);

/* Legacy .dat format support for migration */
bool legacy_load_instances_dat(void);

#endif /* JSON_INSTANCE_H */
