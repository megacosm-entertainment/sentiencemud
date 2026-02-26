/***************************************************************************
 *  JSON Common Utilities                                                  *
 *                                                                         *
 *  Shared helper functions for JSON serialization/deserialization used    *
 *  across all io/json/ modules. Consolidates duplicated patterns into    *
 *  a single reusable API.                                                 *
 *                                                                         *
 *  Includes:                                                              *
 *  - Flag/enum <-> JSON array conversion                                  *
 *  - Safe string/int/bool getters with defaults                           *
 *  - WNUM string serialization                                            *
 *  - JSON file load/save with error handling                              *
 *  - JSON format detection                                                *
 *  - Directory creation helpers                                           *
 *  - Null-safe string macro                                               *
 ***************************************************************************/

#ifndef JSON_COMMON_H
#define JSON_COMMON_H

#include <jansson.h>
#include <stdbool.h>

/* Forward declarations - avoid pulling in merc.h */
struct flag_type;

/***************************************************************************
 * Macros                                                                  *
 ***************************************************************************/

/**
 * json_string_safe - Null-safe wrapper for json_string()
 *
 * Handles NULL strings by substituting "".
 * Usage: json_object_set_new(obj, "name", json_string_safe(thing->name));
 */
#define json_string_safe(s)  json_string((s) ? (s) : "")

/**
 * JSON_APPEND_LINK - Append an item to a singly-linked list during
 *                    deserialization from a JSON array.
 *
 * @param head  Pointer to the list head
 * @param last  Pointer to the current tail
 * @param item  The new item (must have a ->next member)
 */
#define JSON_APPEND_LINK(head, last, item) do { \
    (item)->next = NULL;                        \
    if (!(head)) (head) = (item);               \
    else (last)->next = (item);                 \
    (last) = (item);                            \
} while(0)

/***************************************************************************
 * Flag / Enum Conversion                                                  *
 ***************************************************************************/

/**
 * json_flags_serialize - Convert a bitfield to a JSON array of flag names
 *
 * Handles both bitmask flags (multiple bits set) and stat-style tables
 * (exact value match).
 *
 * @param bits        The bitfield value to serialize
 * @param flag_table  Table of flag name/bit pairs (NULL-terminated)
 * @return            New JSON array (caller owns reference)
 */
json_t *json_flags_serialize(long bits, const struct flag_type *flag_table);

/**
 * json_flags_deserialize - Convert a JSON array of flag names to a bitfield
 *
 * @param array       JSON array of flag name strings
 * @param flag_table  Table of flag name/bit pairs (NULL-terminated)
 * @return            Combined bitfield value
 */
long json_flags_deserialize(json_t *array, const struct flag_type *flag_table);

/**
 * json_enum_serialize - Write an enum value as its string name
 *
 * Falls back to integer if no matching entry found.
 *
 * @param obj         JSON object to write to
 * @param key         JSON key name
 * @param val         Enum value
 * @param flag_table  Table mapping values to names
 */
void json_enum_serialize(json_t *obj, const char *key, long val,
                         const struct flag_type *flag_table);

/**
 * json_enum_deserialize - Read an enum from JSON, accepting both string and
 *                         integer formats (backward-compatible)
 *
 * @param obj         JSON object to read from
 * @param key         JSON key name
 * @param flag_table  Table mapping names to values
 * @return            Enum value, or 0 if not found
 */
long json_enum_deserialize(json_t *obj, const char *key,
                           const struct flag_type *flag_table);

/***************************************************************************
 * Value Getters with Defaults                                             *
 ***************************************************************************/

/**
 * json_get_string - Get a string from a JSON object, with default
 *
 * @param obj          JSON object
 * @param key          Key to look up
 * @param default_val  Value to return if key is missing or not a string
 * @return             String value (borrowed reference, do NOT free)
 */
const char *json_get_string(json_t *obj, const char *key, const char *default_val);

/**
 * json_get_int - Get an integer from a JSON object, with default
 *
 * @param obj          JSON object
 * @param key          Key to look up
 * @param default_val  Value to return if key is missing or not an integer
 * @return             Integer value
 */
long json_get_int(json_t *obj, const char *key, long default_val);

/**
 * json_get_bool - Get a boolean from a JSON object, with default
 *
 * @param obj          JSON object
 * @param key          Key to look up
 * @param default_val  Value to return if key is missing or not a boolean
 * @return             Boolean value
 */
bool json_get_bool(json_t *obj, const char *key, bool default_val);

/***************************************************************************
 * WNUM String Conversion                                                  *
 ***************************************************************************/

/**
 * json_wnum_serialize - Serialize area_uid + vnum as a JSON string
 *
 * Produces "area_uid#vnum" when area_uid > 0, or bare "vnum" otherwise.
 *
 * @param area_uid  Area UID (0 = no area qualifier)
 * @param vnum      Virtual number
 * @return          New JSON string (caller owns reference)
 */
json_t *json_wnum_serialize(long area_uid, long vnum);

/**
 * json_wnum_deserialize - Parse an "area_uid#vnum" or "vnum" JSON string
 *
 * @param json      JSON string value to parse
 * @param area_uid  [out] Parsed area UID (set to 0 if bare vnum)
 * @param vnum      [out] Parsed vnum (set to 0 on error)
 */
void json_wnum_deserialize(json_t *json, long *area_uid, long *vnum);

/***************************************************************************
 * File I/O Helpers                                                        *
 ***************************************************************************/

/**
 * json_file_is_json - Detect whether a file starts with '{' (JSON format)
 *
 * @param filename  Path to the file
 * @return          true if file exists and starts with '{'
 */
bool json_file_is_json(const char *filename);

/**
 * json_file_save - Write a JSON object to a file with error handling
 *
 * Calls json_dump_file() with the given flags, logs errors via
 * log_stringf(), and always calls json_decref(root).
 *
 * @param root     JSON root object (reference consumed — do NOT decref after)
 * @param path     Destination file path
 * @param context  Descriptive label for error messages (e.g. "json_save_bans")
 * @param flags    Jansson dump flags (e.g. JSON_INDENT(2) | JSON_PRESERVE_ORDER)
 * @return         true on success, false on write failure
 */
bool json_file_save(json_t *root, const char *path, const char *context, int flags);

/**
 * json_file_load - Load a JSON file and optionally extract a named array
 *
 * Returns the parsed root object. If array_key is non-NULL, validates that
 * the key exists and is a JSON array, setting *out_array to it.
 *
 * Caller must json_decref(root) when finished.
 *
 * @param path       Source file path
 * @param array_key  Key of the expected array (NULL to skip validation)
 * @param out_array  [out] Pointer to the array within root (borrowed ref)
 * @param context    Descriptive label for error messages
 * @return           Root JSON object, or NULL on error
 */
json_t *json_file_load(const char *path, const char *array_key,
                       json_t **out_array, const char *context);

/***************************************************************************
 * Directory Helpers                                                       *
 ***************************************************************************/

/**
 * json_ensure_dir - Create the first-letter subdirectory under base_dir
 *
 * Builds path "base_dir/tolower(name[0])" and calls mkdir().
 * Silently succeeds if the directory already exists.
 *
 * @param base_dir  Base directory (e.g. PLAYER_DIR, ACCOUNT_DIR)
 * @param name      Name whose first letter determines the subdirectory
 * @return          true (always succeeds or directory already exists)
 */
bool json_ensure_dir(const char *base_dir, const char *name);

/***************************************************************************
 * Backward Compatibility Aliases                                          *
 *                                                                         *
 * These preserve the old function names so callers can migrate gradually.  *
 * New code should use the json_common names above.                        *
 ***************************************************************************/

/* json_area.h used these names */
#define flags_to_json_array(flags, table)    json_flags_serialize((flags), (table))
#define json_array_to_flags(array, table)    json_flags_deserialize((array), (table))
#define json_get_string_default(obj, key, d) json_get_string((obj), (key), (d))
#define json_get_int_default(obj, key, d)    json_get_int((obj), (key), (d))
#define json_get_bool_default(obj, key, d)   json_get_bool((obj), (key), (d))

#endif /* JSON_COMMON_H */
