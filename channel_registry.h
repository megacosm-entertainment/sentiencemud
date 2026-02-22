/***************************************************************************
 *  channel_registry.h — Channel definition registry with JSON persistence *
 *                                                                         *
 *  Central registry for all channel definitions, both built-in and        *
 *  dynamically created via cedit.  Owns CHANNEL_SCOPE and                 *
 *  CHANNEL_DEF_DATA; channel_service.h includes this header and aliases   *
 *  CHANNEL_DEFINITION → CHANNEL_DEF_DATA for backward compatibility.      *
 *                                                                         *
 *  Persistence: data/system/channels.json (not version controlled).       *
 ***************************************************************************/

#ifndef CHANNEL_REGISTRY_H
#define CHANNEL_REGISTRY_H

#include <stdbool.h>

/*=========================================================================*
 * Channel Scope                                                            *
 *=========================================================================*/

typedef enum channel_scope {
    CHANNEL_SCOPE_GLOBAL        = 0, /* Server-wide; all connected entities    */
    CHANNEL_SCOPE_AREA          = 1, /* All entities in the same area          */
    CHANNEL_SCOPE_REGION        = 2, /* All entities in the same region        */
    CHANNEL_SCOPE_ROOM_WV       = 3, /* All entities in the same room (Stg 4) */
    CHANNEL_SCOPE_DIRECT_ENTITY = 4, /* Point-to-point sender → entity         */
    CHANNEL_SCOPE_GROUP_ID      = 5, /* All members of a specific group        */
    CHANNEL_SCOPE_CHURCH_ID     = 6  /* All members of a specific church       */
} CHANNEL_SCOPE;

/*=========================================================================*
 * Route Targets (for multi-target scope channels)                         *
 *=========================================================================*/

#define CHANNEL_MAX_ROUTE_TARGETS 16

typedef struct channel_route_targets {
    int region_count;
    long region_uids[CHANNEL_MAX_ROUTE_TARGETS];

    int area_count;
    long area_uids[CHANNEL_MAX_ROUTE_TARGETS];

    int topic_group_count;
    const char *topic_groups[CHANNEL_MAX_ROUTE_TARGETS];
} CHANNEL_ROUTE_TARGETS;

/*=========================================================================*
 * Channel Definition                                                       *
 *                                                                         *
 * `persistent` = true  → server always subscribes to this channel's       *
 *                         topic(s) regardless of who is online.           *
 * `persistent` = false → subscribed dynamically based on active           *
 *                         participants in the channel's scope.             *
 *=========================================================================*/

typedef struct channel_def_data {
    char id[32];
    char name[64];
    char command[32];               /* player command that invokes the channel */
    CHANNEL_SCOPE scope;
    bool allow_player_flags;        /* honour per-player display flags         */
    bool persistent;                /* true = always-on; false = context-bound */

    /* Optional topic override.  Empty string = use scope-based default.
     *
     * Static topic (no variables):  "chan:global:gossip"
     * Template (with $variables):   "chan:group:$group_id1:$group_id2"
     *
     * Supported variables (resolved from the sender at publish time):
     *   $channel_id   — def->id
     *   $area_uid     — sender's area uid (or area->area_topic if set)
     *   $region_uid   — sender's region uid (or region->topic if set)
     *   $group_id1    — sender's group->id[0]
     *   $group_id2    — sender's group->id[1]
     *   $church_uid   — sender's church->uid
     *   $entity_id1   — sender->id[0]   (for DIRECT_ENTITY send side)
     *   $entity_id2   — sender->id[1]
     *
     * Variables that cannot be resolved (NULL context) expand to empty string.
     */
    char topic_pattern[128];

    CHANNEL_ROUTE_TARGETS route_targets;
} CHANNEL_DEF_DATA;

/*=========================================================================*
 * Registry API                                                             *
 *=========================================================================*/

/**
 * channel_registry_init - Seed the registry with built-in default channels.
 *
 * Safe to call multiple times; subsequent calls are no-ops.  Must be called
 * before any other registry function.
 *
 * @return true always
 */
bool channel_registry_init(void);

/**
 * channel_registry_load - Load channel definitions from a JSON file.
 *
 * Replaces the current registry contents (including defaults) with entries
 * from `filepath`.  If the file is missing or malformed the registry is left
 * unchanged and false is returned.
 *
 * @param filepath  Path to channels.json (e.g. "data/system/channels.json")
 * @return          true on success
 */
bool channel_registry_load(const char *filepath);

/**
 * channel_registry_save - Write current registry to a JSON file.
 *
 * @param filepath  Destination path (created or overwritten)
 * @return          true on success
 */
bool channel_registry_save(const char *filepath);

/**
 * channel_registry_find - Look up a channel definition by id.
 *
 * @param id  Channel identifier (e.g. "gossip")
 * @return    Pointer into the registry, or NULL if not found.
 *            Valid until the next upsert/remove call.
 */
const CHANNEL_DEF_DATA *channel_registry_find(const char *id);

/**
 * channel_registry_count - Number of definitions currently in the registry.
 */
int channel_registry_count(void);

/**
 * channel_registry_get - Indexed access into the registry.
 *
 * @param index  0-based index; must be < channel_registry_count()
 * @return       Pointer to definition, or NULL if out of range.
 */
const CHANNEL_DEF_DATA *channel_registry_get(int index);

/**
 * channel_registry_upsert - Add or overwrite a channel definition.
 *
 * If a definition with def->id already exists it is overwritten in-place;
 * otherwise a new entry is appended.
 *
 * @param def  Definition to insert or update (copied by value)
 * @return     true on success; false if the registry is full
 */
bool channel_registry_upsert(const CHANNEL_DEF_DATA *def);

/**
 * channel_registry_remove - Remove a channel definition by id.
 *
 * @return true if found and removed; false if not found
 */
bool channel_registry_remove(const char *id);

#endif /* CHANNEL_REGISTRY_H */
