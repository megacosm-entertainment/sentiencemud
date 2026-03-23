/***************************************************************************
 *  JSON Persistence Layer - Phase 1                                       *
 *                                                                          *
 *  Handles JSON serialization/deserialization for persistent world state: *
 *  - Rooms (static, wilderness, cloned)                                   *
 *  - Mobiles (NPCs)                                                       *
 *  - Objects                                                              *
 *                                                                          *
 *  This replaces the monolithic persist.dat with individual JSON files.   *
 ***************************************************************************/

#ifndef JSON_PERSIST_H
#define JSON_PERSIST_H

#include <jansson.h>
#include <stdbool.h>

/***************************************************************************
 * Version Constants                                                       *
 ***************************************************************************/

#define JSON_PERSIST_VERSION_OBJECT     1
#define JSON_PERSIST_VERSION_MOBILE     1
#define JSON_PERSIST_VERSION_ROOM       1

/***************************************************************************
 * Directory Structure                                                     *
 ***************************************************************************/

#define PERSIST_JSON_DIR        DATA_DIR "persist/"
#define PERSIST_JSON_ROOMS      PERSIST_JSON_DIR "rooms/"
#define PERSIST_JSON_MOBILES    PERSIST_JSON_DIR "mobiles/"
#define PERSIST_JSON_OBJECTS    PERSIST_JSON_DIR "objects/"

/***************************************************************************
 * Initialization                                                          *
 ***************************************************************************/

/* Create directory structure for JSON persistence */
bool json_persist_init(void);

/***************************************************************************
 * Object Serialization (Full State)                                       *
 ***************************************************************************/

/* Serialize an object to JSON (full state for persistence) */
json_t *json_persist_object_to_json(OBJ_DATA *obj);

/* Deserialize an object from JSON */
OBJ_DATA *json_persist_json_to_object(json_t *json);

/* Write a persistent object to its JSON file */
bool json_persist_save_object(OBJ_DATA *obj);

/* Load a persistent object from its JSON file */
OBJ_DATA *json_persist_load_object(unsigned long id0, unsigned long id1);

/* Get the file path for an object */
void json_persist_object_path(unsigned long id0, unsigned long id1, char *buf, size_t bufsize);

/***************************************************************************
 * Mobile Serialization (Full State)                                       *
 ***************************************************************************/

/* Serialize a mobile to JSON (full state for persistence) */
json_t *json_persist_mobile_to_json(CHAR_DATA *ch);

/* Deserialize a mobile from JSON */
CHAR_DATA *json_persist_json_to_mobile(json_t *json);

/* Write a persistent mobile to its JSON file */
bool json_persist_save_mobile(CHAR_DATA *ch);

/* Load a persistent mobile from its JSON file */
CHAR_DATA *json_persist_load_mobile(unsigned long id0, unsigned long id1);

/* Get the file path for a mobile */
void json_persist_mobile_path(unsigned long id0, unsigned long id1, char *buf, size_t bufsize);

/***************************************************************************
 * Room Serialization (Full State)                                         *
 ***************************************************************************/

/* Serialize a room to JSON (full state for persistence) */
json_t *json_persist_room_to_json(ROOM_INDEX_DATA *room);

/* Deserialize a room from JSON */
ROOM_INDEX_DATA *json_persist_json_to_room(json_t *json);

/* Write a persistent room to its JSON file */
bool json_persist_save_room(ROOM_INDEX_DATA *room);

/* Load a persistent room from its JSON file */
ROOM_INDEX_DATA *json_persist_load_room(const char *room_id);

/* Get the file path for a room */
void json_persist_room_path(ROOM_INDEX_DATA *room, char *buf, size_t bufsize);

/* Generate unique room ID string */
void json_persist_room_id(ROOM_INDEX_DATA *room, char *buf, size_t bufsize);

/***************************************************************************
 * Token Serialization                                                     *
 ***************************************************************************/

/* Serialize a token to JSON */
json_t *json_persist_token_to_json(TOKEN_DATA *token);

/* Deserialize a token from JSON */
TOKEN_DATA *json_persist_json_to_token(json_t *json);

/***************************************************************************
 * Script Data Serialization                                               *
 ***************************************************************************/

/* Serialize script variables to JSON */
json_t *json_persist_scriptdata_to_json(PROG_DATA *progs);

/* Deserialize script variables from JSON */
bool json_persist_json_to_scriptdata(json_t *json, PROG_DATA **progs);

/***************************************************************************
 * Affect Serialization                                                    *
 ***************************************************************************/

/* Serialize an affect to JSON */
json_t *json_persist_affect_to_json(AFFECT_DATA *paf);

/* Deserialize an affect from JSON */
AFFECT_DATA *json_persist_json_to_affect(json_t *json);

/***************************************************************************
 * Exit Serialization                                                      *
 ***************************************************************************/

/* Serialize an exit to JSON */
json_t *json_persist_exit_to_json(EXIT_DATA *pexit, int dir);

/* Deserialize an exit from JSON */
EXIT_DATA *json_persist_json_to_exit(json_t *json, ROOM_INDEX_DATA *room);

/***************************************************************************
 * Location Serialization                                                  *
 ***************************************************************************/

/* Serialize a location to JSON */
json_t *json_persist_location_to_json(LOCATION *loc);

/* Deserialize a location from JSON */
bool json_persist_json_to_location(json_t *json, LOCATION *loc);

/***************************************************************************
 * Lock Serialization                                                      *
 ***************************************************************************/

/* Serialize a lock to JSON */
json_t *json_persist_lock_to_json(LOCK_STATE *lock);

/* Deserialize a lock from JSON */
LOCK_STATE *json_persist_json_to_lock(json_t *json);

/***************************************************************************
 * Batch Operations                                                        *
 ***************************************************************************/

/* Save all persistent entities to JSON files */
bool json_persist_save_all(void);

/* Load all persistent entities from JSON files */
bool json_persist_load_all(void);

/***************************************************************************
 * Migration Utilities                                                     *
 ***************************************************************************/

/***************************************************************************
 * Phase 2: Background Worker                                              *
 ***************************************************************************/

/* Start the background persist worker thread */
bool json_persist_worker_start(void);

/* Stop the background persist worker thread */
void json_persist_worker_stop(void);

/* Check if background worker is running */
bool json_persist_worker_is_running(void);

/***************************************************************************
 * Phase 2: Redis-Cached Operations                                        *
 ***************************************************************************/

/* Save through Redis cache (async disk write via worker) */
bool json_persist_save_object_cached(OBJ_DATA *obj);
bool json_persist_save_mobile_cached(CHAR_DATA *ch);
bool json_persist_save_room_cached(ROOM_INDEX_DATA *room);

/* Load with Redis cache (cache-first, then disk) */
OBJ_DATA *json_persist_load_object_cached(unsigned long id0, unsigned long id1);
CHAR_DATA *json_persist_load_mobile_cached(unsigned long id0, unsigned long id1);
ROOM_INDEX_DATA *json_persist_load_room_cached(const char *room_id);

/* Warm Redis cache with already-loaded persist entities (call after redis_init) */
void json_persist_warm_cache(void);

#endif /* JSON_PERSIST_H */
