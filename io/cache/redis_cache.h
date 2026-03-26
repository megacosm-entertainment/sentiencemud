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
#include <jansson.h>
#include "../../merc.h"

/***************************************************************************
 * Configuration                                                           *
 * Redis settings are now managed through game_settings                    *
 * See merc.h GAME_SETTINGS_DATA for the configuration structure           *
 * Settings can be overridden with environment variables:                  *
 *   - SENTIENCE_REDIS_ENABLE                                              *
 *   - SENTIENCE_REDIS_HOST                                                *
 *   - SENTIENCE_REDIS_PORT                                                *
 *   - SENTIENCE_REDIS_PASSWORD                                            *
 *   - SENTIENCE_REDIS_TIMEOUT_SEC                                         *
 *   - SENTIENCE_REDIS_TIMEOUT_USEC                                        *
 ***************************************************************************/

// TTL values (in seconds)
#define REDIS_TTL_CHAR_INFO    (24 * 3600)  // 24 hours - rarely changes
#define REDIS_TTL_CHAR_DATA    (1 * 3600)   // 1 hour - active gameplay
#define REDIS_TTL_CHAR_FULL    (24 * 3600)  // 24 hours - full character JSON
#define REDIS_TTL_CHAR_ACTIVE  (30 * 60)    // 30 minutes - is logged in?
#define REDIS_TTL_WORLD_STATE  (7 * 24 * 3600)  // 7 days - persistent world
#define REDIS_TTL_AREA_FULL    (7 * 24 * 3600)  // 7 days - areas rarely change

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

// Create a new independent, fully-authenticated Redis connection.
// The caller owns the returned context and must redisFree() it when done.
// Returns NULL on failure. Safe to call from any thread.
redisContext *redis_new_context(void);

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
 * Full Character Caching (Phase 2 - RedisJSON)                           *
 ***************************************************************************/

// Cache full character JSON document
// Key: "char:{name}:full"
// Mode: Uses RedisJSON (JSON.SET) if available, falls back to string storage
// Returns: true if cached successfully
bool redis_cache_char_full(CHAR_DATA *ch, json_t *char_json);

// Retrieve full character JSON from cache
// Returns: json_t* if found (caller must json_decref), NULL if miss
// Mode: Uses RedisJSON (JSON.GET) if available, falls back to string parsing
json_t *redis_get_char_full(const char *name);

// Partial update functions (only work with RedisJSON)
// These return false if RedisJSON is not available
bool redis_update_char_gold(const char *name, long gold);
bool redis_update_char_exp(const char *name, long exp);
bool redis_update_char_position(const char *name, int room_vnum);

// Check if RedisJSON module is available
bool redis_has_json_module(void);

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

// Serialize CHAR_DATA to basic info structure
CHAR_INFO_CACHE *char_to_info_cache(CHAR_DATA *ch);

// Build Redis key
// Usage: redis_key("char", name, "info") -> "char:elzamine:info"
char *redis_key(const char *prefix, const char *name, const char *suffix);

/***************************************************************************
 * Account Caching (Phase 3)                                              *
 ***************************************************************************/

// Cache full account JSON document
// Key: "account:{name}:full"
// Returns: true if cached successfully
bool redis_cache_account_full(const char *account_name, json_t *account_json);

// Retrieve full account JSON from cache
// Returns: json_t* if found (caller must json_decref), NULL if miss
json_t *redis_get_account_full(const char *account_name);

/***************************************************************************
 * Area Caching                                                            *
 * Pattern: Boot loads from disk, then async warms Redis.                  *
 * Edits/saves update Redis immediately, queue async disk write.          *
 ***************************************************************************/

// Cache full area JSON
// Key: "area:{uid}:full"
// Returns: true if cached successfully
// filename: area filename (e.g., "midgaard.json") - .json suffix is stripped for key
bool redis_cache_area_full(const char *filename, const char *json_str);

// Retrieve full area JSON from cache
// Returns: allocated string if found (caller must free), NULL if miss
char *redis_get_area_full(const char *filename);

// Invalidate area cache entry
void redis_invalidate_area(const char *filename);

// Cache area and queue for async disk write
// This is the main entry point for area saves
// The filename is extracted from the JSON when writing to disk
bool redis_cache_area_state(const char *filename, const char *json_str);

// Async cache warming (call after boot, non-blocking)
// Queues areas for background caching
void redis_queue_area_cache_warm(const char *filename, const char *json_str);

// Process one area from the cache warm queue (call from game loop)
// Returns: true if processed an item, false if queue empty
bool redis_process_area_cache_warm(void);

// Get size of cache warm queue
long redis_area_cache_warm_queue_size(void);

/***************************************************************************
 * World State Persistence (Phase 2)                                       *
 ***************************************************************************/

// Low-level cache operations
bool redis_cache_persist_data(const char *key, const char *json_str);
char *redis_get_persist_data(const char *key);
bool redis_delete_persist_data(const char *key);

// Dirty queue operations (for async disk writes)
bool redis_queue_dirty_key(const char *key);
char *redis_pop_dirty_key(int timeout_sec);
long redis_dirty_queue_size(void);

// High-level entity caching (cache + queue dirty)
bool redis_cache_room_state(const char *room_id, const char *json_str);
bool redis_cache_mobile_state(unsigned long id0, unsigned long id1, const char *json_str);
bool redis_cache_object_state(unsigned long id0, unsigned long id1, const char *json_str);

// Cache retrieval
char *redis_get_room_state(const char *room_id);
char *redis_get_mobile_state(unsigned long id0, unsigned long id1);
char *redis_get_object_state(unsigned long id0, unsigned long id1);

/***************************************************************************
 * Leaderboard Operations (Sorted Sets)                                   *
 * Keys: "leaderboard:{board}" (e.g. "leaderboard:pkers")                *
 * No TTL - leaderboard data persists until explicitly removed             *
 ***************************************************************************/

// Update a player's score in a leaderboard (ZADD)
bool redis_leaderboard_update(const char *board_name, const char *player_name, double score);

// Retrieve top N entries, highest score first (ZREVRANGE WITHSCORES)
// names[] entries are strdup'd - caller must free each non-NULL entry
int redis_leaderboard_get_top(const char *board_name, int max_entries,
                              char **names, double *scores);

// Retrieve bottom N entries, lowest score first (ZRANGE WITHSCORES)
int redis_leaderboard_get_bottom(const char *board_name, int max_entries,
                                 char **names, double *scores);

// Remove a player from a specific leaderboard (ZREM)
bool redis_leaderboard_remove(const char *board_name, const char *player_name);

// Remove a player from ALL leaderboards and ratio data
void redis_leaderboard_remove_all(const char *player_name);

// Store kills/deaths for ratio computation (HSET leaderboard:ratio:data)
bool redis_leaderboard_set_ratio_data(const char *player_name,
                                       int total_kills, int total_deaths);

// Compute ratios from stored data, return sorted ascending (worst first)
// names[] entries are strdup'd - caller must free each non-NULL entry
int redis_leaderboard_get_ratio_data(int max_entries, char **names,
                                      double *scores, int min_total_fights);

// Get number of entries in a leaderboard (ZCARD)
long redis_leaderboard_count(const char *board_name);

/***************************************************************************
 * Audit Error Counters                                                    *
 **************************************************************************/

// Increment persistent count for an audit error signature
bool redis_audit_error_increment(const char *signature);

// Retrieve persistent count for an audit error signature
// Returns -1 on error/unavailable
long redis_audit_error_get_count(const char *signature);

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
