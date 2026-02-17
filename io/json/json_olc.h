/***************************************************************************
 *  JSON OLC History — Per-type file persistence for OLC change history    *
 *                                                                         *
 *  Manages per-entity audit trails stored in per-type files:              *
 *    data/history/skills.json, data/history/races.json, etc.              *
 *                                                                         *
 *  Each file contains a keyed object where keys are entity identifiers    *
 *  and values are arrays of change entries. History is loaded on demand   *
 *  and flushed on entity save + periodic tick + shutdown.                 *
 ***************************************************************************/

#ifndef JSON_OLC_H
#define JSON_OLC_H

#include <jansson.h>
#include "../../editors/common/olc_editor.h"

/***************************************************************************
 * History Types                                                           *
 *                                                                         *
 * Each OLC entity type that supports history gets a constant here.        *
 * Used to identify which per-type file to read/write.                     *
 ***************************************************************************/

#define OLC_HIST_SKILL      0
#define OLC_HIST_GROUP      1
#define OLC_HIST_SONG       2
#define OLC_HIST_RACE       3
#define OLC_HIST_CLASS      4
#define OLC_HIST_TRAIT      5
#define OLC_HIST_AREA_EDITOR 6
#define OLC_HIST_MAX        7

/***************************************************************************
 * Per-Entry Serialization                                                 *
 ***************************************************************************/

/**
 * Serialize a single entity's change history to a JSON array.
 *
 * @param history  History to serialize (returns json_null() if NULL/empty)
 * @return json_t* that the caller owns
 */
json_t *olc_history_to_json(const OLC_CHANGE_HISTORY *history);

/**
 * Deserialize a JSON array back into an OLC_CHANGE_HISTORY.
 *
 * @param arr  JSON array of history entry objects (safe to pass NULL)
 * @return Newly allocated history, or NULL if arr is NULL/empty/invalid
 */
OLC_CHANGE_HISTORY *olc_history_from_json(json_t *arr);

/***************************************************************************
 * Per-Type File Management                                                *
 ***************************************************************************/

/**
 * Load an entity's history from its type file.
 * Returns a newly allocated history, or NULL if no history on disk.
 *
 * @param hist_type  OLC_HIST_* constant
 * @param entity_id  Entity identifier (skill name, race id, etc.)
 * @return History, or NULL
 */
OLC_CHANGE_HISTORY *olc_history_load(int hist_type, const char *entity_id);

/**
 * Mark an entity's history as dirty so it will be flushed on next save.
 * Call this after olc_history_record() if the history should persist.
 *
 * @param hist_type   OLC_HIST_* constant
 * @param entity_id   Entity identifier
 * @param history     Current in-memory history to flush
 */
void olc_history_mark_dirty(int hist_type, const char *entity_id,
                            OLC_CHANGE_HISTORY *history);

/**
 * Flush a single entity's dirty history to its type file immediately.
 * Called from editor save commands.
 *
 * @param hist_type   OLC_HIST_* constant
 * @param entity_id   Entity identifier
 * @param history     Current in-memory history to write
 */
void olc_history_flush(int hist_type, const char *entity_id,
                       const OLC_CHANGE_HISTORY *history);

/**
 * Flush ALL dirty histories across all types to disk.
 * Called from periodic update tick and shutdown.
 */
void olc_history_flush_all(void);

/**
 * Remove an entity's history from the type file (e.g., on entity delete).
 *
 * @param hist_type   OLC_HIST_* constant
 * @param entity_id   Entity identifier to remove
 */
void olc_history_remove(int hist_type, const char *entity_id);

#endif /* JSON_OLC_H */

