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

// Check if file is JSON format (vs old pfile format)
bool json_is_json_file(const char *filename);

/***************************************************************************
 * Section-Specific Functions (for lazy loading)                          *
 ***************************************************************************/

// Read only character section (lightweight metadata)
// Used for account menu display and cache warming
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
