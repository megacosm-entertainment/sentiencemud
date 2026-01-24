/***************************************************************************
 *  JSON Character Format - Single File with Sections                      *
 *                                                                         *
 *  Maintains single-file structure like current pfiles, but in JSON      *
 *  Supports lazy loading by parsing only needed sections                 *
 ***************************************************************************/

#ifndef JSON_CHAR_H
#define JSON_CHAR_H

#include "merc.h"
#include "redis_cache.h"
#include <jansson.h>

/***************************************************************************
 * File Structure                                                          *
 ***************************************************************************/

// Single JSON file: characters/{letter}/{Name}.json
// Contains sections:
//   - metadata: Version, timestamps, account info
//   - character: Basic character data (name, level, race, class, stats, vitals)
//   - inventory: Character's carrying inventory
//   - locker: Character's locker items
//   - equipment: Worn items
//   - skills: Skill percentages
//   - affects: Active affects
//   - variables: Script variables
//   - etc.

/***************************************************************************
 * Core Functions                                                          *
 ***************************************************************************/

// Write full character to JSON file
// Returns: true on success, false on failure
bool json_write_char(CHAR_DATA *ch, const char *filename);

// Read full character from JSON file
// Returns: true on success, false on failure
bool json_read_char(CHAR_DATA *ch, const char *filename);

// Read basic character data WITHOUT inventory/equipment/skills/affects
// Used for character menu - defers heavy data loading until game entry
// Returns: true on success, false on failure
bool json_read_char_basic(CHAR_DATA *ch, const char *filename);

// Load remaining character data (inventory/equipment/skills/affects)
// Called when character enters game after json_read_char_basic()
// Returns: true on success, false on failure
bool json_read_char_remaining(CHAR_DATA *ch, const char *filename);

// Check if file is JSON format (vs old pfile format)
bool json_is_json_file(const char *filename);

/***************************************************************************
 * Direct JSON Object Functions (for Redis cache)                          *
 * These accept a pre-parsed json_t* instead of a filename                 *
 ***************************************************************************/

// Read full character from JSON object (for Redis cache)
bool json_read_char_from_json(CHAR_DATA *ch, json_t *root);

// Read basic character data from JSON object (for Redis cache)
bool json_read_char_basic_from_json(CHAR_DATA *ch, json_t *root);

// Load remaining character data from JSON object (for Redis cache)
bool json_read_char_remaining_from_json(CHAR_DATA *ch, json_t *root);

/***************************************************************************
 * Section-Specific Functions (for lazy loading)                          *
 ***************************************************************************/

// Read only lightweight character info (for account menu display)
// Does NOT load inventory, equipment, skills, affects, etc.
// Returns CHAR_INFO_CACHE* that must be freed with free_char_info_cache()
// This is the preferred way to get character info for display purposes
CHAR_INFO_CACHE *json_read_char_info_lightweight(const char *filename);

// Deprecated: Read only character section (lightweight metadata)
// Used for account menu display and cache warming
// Use json_read_char_info_lightweight() instead
bool json_read_char_info(CHAR_DATA *ch, const char *filename);

// Read inventory section
bool json_read_char_inventory(CHAR_DATA *ch, json_t *root);

// Read locker section
bool json_read_char_locker(CHAR_DATA *ch, json_t *root);

// Read equipment section
bool json_read_char_equipment(CHAR_DATA *ch, json_t *root);

/***************************************************************************
 * Serialization Helpers                                                   *
 ***************************************************************************/

// Convert CHAR_DATA to JSON object (full character)
json_t *char_to_json(CHAR_DATA *ch);

// Convert CHAR_INFO_CACHE to JSON object (lightweight metadata only)
json_t *char_info_to_json(CHAR_INFO_CACHE *info);

// Parse JSON object to CHAR_INFO_CACHE
CHAR_INFO_CACHE *json_to_char_info(json_t *json);

// Object serialization (shared with account system for vault items)
json_t *obj_to_json(OBJ_DATA *obj, int nest_level);
OBJ_DATA *json_to_obj(json_t *json_obj, CHAR_DATA *ch);

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

// Get character file path (same for JSON and old pfile - format auto-detected)
// Example: characters/e/Elzamine
void json_get_char_path(const char *char_name, char *path_buf, size_t buf_size);

// Get old pfile path (same as json_get_char_path - unified filename)
// Example: characters/e/Elzamine
void json_get_pfile_path(const char *char_name, char *path_buf, size_t buf_size);

// Get backup path for old pfile before migration
// Example: characters/e.old/Elzamine
void json_get_backup_path(const char *char_name, char *path_buf, size_t buf_size);

// Create directory structure if needed
bool json_ensure_char_dir(const char *char_name);

#endif /* JSON_CHAR_H */
