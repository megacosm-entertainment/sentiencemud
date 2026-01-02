/***************************************************************************
 *  Redis Cache Module - Character Data Caching                            *
 *                                                                         *
 *  Provides high-performance caching layer for character data using Redis *
 *  Supports lazy loading, TTL-based expiration, and async writes         *
 *                                                                         *
 *  Architecture:                                                          *
 *    Game Memory <-> Redis Cache <-> JSON Files (disk)                   *
 *                                                                         *
 *  Future: World state persistence (NPCs, rooms, objects)                *
 ***************************************************************************/

#ifndef REDIS_CACHE_H
#define REDIS_CACHE_H

#include <hiredis/hiredis.h>
#include "merc.h"

/***************************************************************************
 * Configuration                                                           *
 ***************************************************************************/

#define REDIS_HOST "127.0.0.1"
#define REDIS_PORT 6379
#define REDIS_TIMEOUT_SEC 1
#define REDIS_TIMEOUT_USEC 500000

// TTL values (in seconds)
#define REDIS_TTL_CHAR_INFO    (24 * 3600)  // 24 hours - rarely changes
#define REDIS_TTL_CHAR_DATA    (1 * 3600)   // 1 hour - active gameplay
#define REDIS_TTL_CHAR_ACTIVE  (30 * 60)    // 30 minutes - is logged in?
#define REDIS_TTL_WORLD_STATE  (7 * 24 * 3600)  // 7 days - persistent world

/***************************************************************************
 * Data Structures                                                         *
 ***************************************************************************/

// Lightweight character info for account menu display
typedef struct char_info_cache {
    char *name;
    int level;
    int tot_level;
    int remorts;
    char *race;
    char **classes;  // Array of class names
    int num_classes;
    char *title;
    long last_played;
    bool is_active;  // Currently logged in?

    // Quick stats for display
    int health_pct;  // current / max * 100
    int mana_pct;
    long gold;
    long experience;
} CHAR_INFO_CACHE;

/***************************************************************************
 * Connection Management                                                   *
 ***************************************************************************/

// Initialize Redis connection pool
bool redis_init(void);

// Shutdown Redis connections
void redis_shutdown(void);

// Get a Redis connection (thread-safe if needed later)
redisContext *redis_get_connection(void);

// Return connection to pool
void redis_release_connection(redisContext *c);

// Test if Redis is available
bool redis_is_available(void);

/***************************************************************************
 * Character Info Caching (Phase 1)                                       *
 ***************************************************************************/

// Cache character basic info for account menu
// Key: "char:{name}:info"
// Returns: true if cached successfully
bool redis_cache_char_info(CHAR_DATA *ch);

// Retrieve character info from cache
// Returns: CHAR_INFO_CACHE* if found, NULL if miss
// Caller must free returned structure with free_char_info_cache()
CHAR_INFO_CACHE *redis_get_char_info(const char *name);

// Free a CHAR_INFO_CACHE structure
void free_char_info_cache(CHAR_INFO_CACHE *info);

// Mark character as active (logged in)
// Key: "char:{name}:active"
bool redis_set_char_active(const char *name, bool active);

// Check if character is active
bool redis_is_char_active(const char *name);

// Invalidate all cached data for a character
void redis_invalidate_char(const char *name);

// Warm cache by loading recently active characters
void redis_warm_cache(int max_chars);

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

// Serialize CHAR_DATA to basic info structure
CHAR_INFO_CACHE *char_to_info_cache(CHAR_DATA *ch);

// Build Redis key
// Usage: redis_key("char", name, "info") -> "char:elzamine:info"
char *redis_key(const char *prefix, const char *name, const char *suffix);

/***************************************************************************
 * Future: Full Character Data Caching (Phase 4)                          *
 ***************************************************************************/

// These will be implemented in later phases
#ifdef REDIS_FUTURE_FEATURES
bool redis_cache_char_inventory(CHAR_DATA *ch);
bool redis_cache_char_locker(CHAR_DATA *ch);
bool redis_cache_char_equipment(CHAR_DATA *ch);
char *redis_get_char_inventory(const char *name);
char *redis_get_char_locker(const char *name);
#endif

/***************************************************************************
 * Future: World State Persistence                                        *
 ***************************************************************************/

// These will be implemented for world state persistence
#ifdef REDIS_WORLD_STATE
bool redis_cache_mob_state(CHAR_DATA *mob);
bool redis_cache_room_state(ROOM_INDEX_DATA *room);
bool redis_cache_obj_state(OBJ_DATA *obj);
void redis_restore_world_state(void);
void redis_save_world_state(void);
#endif

/***************************************************************************
 * Diagnostics & Monitoring                                               *
 ***************************************************************************/

// Get cache statistics
typedef struct redis_stats {
    long hits;
    long misses;
    long sets;
    long deletes;
    long errors;
    long keys_stored;
    long memory_used;
} REDIS_STATS;

REDIS_STATS *redis_get_stats(void);
void redis_print_stats(CHAR_DATA *ch);  // For admin command

#endif /* REDIS_CACHE_H */
