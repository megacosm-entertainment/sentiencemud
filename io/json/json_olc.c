/***************************************************************************
 *  JSON OLC History — Per-type file persistence for OLC change history    *
 *                                                                         *
 *  Serializes/deserializes OLC_CHANGE_HISTORY to/from JSON arrays.        *
 *  Each entry stores: author, ISO-8601 timestamp, sequential id,          *
 *  field name, old value, and new value.                                  *
 *                                                                         *
 *  History is stored in per-type files (data/history/skills.json, etc.)   *
 *  with async dirty tracking. Changes accumulate in memory and are        *
 *  flushed to disk on entity save, periodic update tick, or shutdown.     *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <jansson.h>

#include "../../merc.h"
#include "../../log.h"
#include "../../editors/common/olc_editor.h"
#include "json_olc.h"
#include "json_common.h"

/***************************************************************************
 * Constants                                                               *
 ***************************************************************************/

#define HISTORY_DIR         "data/history"
#define HISTORY_FORMAT      "olc_history"
#define HISTORY_VERSION     1

/** File paths indexed by OLC_HIST_* constants. */
static const char *hist_files[OLC_HIST_MAX] = {
    HISTORY_DIR "/skills.json",     /* OLC_HIST_SKILL */
    HISTORY_DIR "/groups.json",     /* OLC_HIST_GROUP */
    HISTORY_DIR "/songs.json",      /* OLC_HIST_SONG  */
    HISTORY_DIR "/races.json",      /* OLC_HIST_RACE  */
    HISTORY_DIR "/classes.json",    /* OLC_HIST_CLASS */
    HISTORY_DIR "/traits.json",     /* OLC_HIST_TRAIT */
};

static const char *hist_type_names[OLC_HIST_MAX] = {
    "skills", "groups", "songs", "races", "classes", "traits"
};

/***************************************************************************
 * Dirty Tracking                                                          *
 *                                                                         *
 * A simple linked list of entities whose history has been modified since  *
 * last flush. On flush, we group by type, load each type file, merge      *
 * dirty entries, and write back.                                          *
 ***************************************************************************/

typedef struct olc_history_dirty {
    int                         hist_type;
    char                       *entity_id;
    OLC_CHANGE_HISTORY         *history;
    struct olc_history_dirty   *next;
} OLC_HISTORY_DIRTY;

static OLC_HISTORY_DIRTY *dirty_list = NULL;

/**
 * Find an existing dirty entry for a type+entity pair.
 */
static OLC_HISTORY_DIRTY *dirty_find(int hist_type, const char *entity_id)
{
    OLC_HISTORY_DIRTY *d;

    for (d = dirty_list; d; d = d->next) {
        if (d->hist_type == hist_type && !str_cmp(d->entity_id, entity_id))
            return d;
    }
    return NULL;
}

/**
 * Remove a specific dirty entry from the list and free it.
 */
static void dirty_remove(int hist_type, const char *entity_id)
{
    OLC_HISTORY_DIRTY *d, *prev = NULL;

    for (d = dirty_list; d; prev = d, d = d->next) {
        if (d->hist_type == hist_type && !str_cmp(d->entity_id, entity_id)) {
            if (prev)
                prev->next = d->next;
            else
                dirty_list = d->next;
            free_string(d->entity_id);
            free(d);
            return;
        }
    }
}

/**
 * Free the entire dirty list.
 */
static void dirty_clear(void)
{
    OLC_HISTORY_DIRTY *d, *next;

    for (d = dirty_list; d; d = next) {
        next = d->next;
        free_string(d->entity_id);
        free(d);
    }
    dirty_list = NULL;
}

/***************************************************************************
 * File Helpers                                                            *
 ***************************************************************************/

/** Ensure data/history/ directory exists. */
static void ensure_history_dir(void)
{
    mkdir(HISTORY_DIR, 0755);
}

/**
 * Load an entire type's history file.
 * Returns the root JSON object (caller must json_decref), or NULL.
 */
static json_t *hist_file_open(int hist_type)
{
    json_error_t error;

    if (hist_type < 0 || hist_type >= OLC_HIST_MAX)
        return NULL;

    return json_load_file(hist_files[hist_type], 0, &error);
}

/**
 * Get the "entries" object from a type file root, or NULL.
 */
static json_t *hist_file_entries(json_t *root)
{
    json_t *entries;

    if (!root || !json_is_object(root))
        return NULL;

    entries = json_object_get(root, "entries");
    if (!entries || !json_is_object(entries))
        return NULL;

    return entries;
}

/**
 * Build a new root object for a type file.
 * The returned object has refcount 1 and contains an empty "entries" object.
 */
static json_t *hist_file_new(void)
{
    json_t *root = json_object();
    json_object_set_new(root, "_format", json_string(HISTORY_FORMAT));
    json_object_set_new(root, "_version", json_integer(HISTORY_VERSION));
    json_object_set_new(root, "entries", json_object());
    return root;
}

/**
 * Write a type file root to disk. Consumes the root reference.
 */
static bool hist_file_write(int hist_type, json_t *root)
{
    ensure_history_dir();
    return json_file_save(root, hist_files[hist_type],
                          hist_type_names[hist_type],
                          JSON_INDENT(2) | JSON_SORT_KEYS);
}


/***************************************************************************
 * Serialize — Entry Level                                                 *
 ***************************************************************************/

/**
 * olc_history_to_json - Convert an OLC_CHANGE_HISTORY to a JSON array.
 *
 * Returns json_null() if history is NULL or has no entries, so callers
 * can unconditionally attach the result to their parent object.
 */
json_t *olc_history_to_json(const OLC_CHANGE_HISTORY *history)
{
    json_t *arr;
    ITERATOR it;
    OLC_CHANGE_ENTRY *entry;
    char time_buf[64];
    struct tm *tm_info;

    if (!history || !history->entries || list_size(history->entries) == 0)
        return json_null();

    arr = json_array();

    iterator_start(&it, history->entries);
    while ((entry = (OLC_CHANGE_ENTRY *)iterator_nextdata(&it))) {
        json_t *obj = json_object();

        json_object_set_new(obj, "id",        json_integer(entry->id));
        json_object_set_new(obj, "author",    json_string_safe(entry->author));
        json_object_set_new(obj, "field",     json_string_safe(entry->field));
        json_object_set_new(obj, "old_value", json_string_safe(entry->old_value));
        json_object_set_new(obj, "new_value", json_string_safe(entry->new_value));

        /* ISO-8601 timestamp */
        tm_info = localtime(&entry->timestamp);
        if (tm_info) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm_info);
            json_object_set_new(obj, "timestamp", json_string(time_buf));
        } else {
            json_object_set_new(obj, "timestamp", json_integer((json_int_t)entry->timestamp));
        }

        json_array_append_new(arr, obj);
    }
    iterator_stop(&it);

    return arr;
}


/***************************************************************************
 * Deserialize — Entry Level                                               *
 ***************************************************************************/

/**
 * olc_history_from_json - Rebuild an OLC_CHANGE_HISTORY from a JSON array.
 *
 * Handles both ISO-8601 string timestamps and raw integer timestamps
 * for backward compatibility.
 */
OLC_CHANGE_HISTORY *olc_history_from_json(json_t *arr)
{
    OLC_CHANGE_HISTORY *history;
    size_t index;
    json_t *obj;

    if (!arr || !json_is_array(arr) || json_array_size(arr) == 0)
        return NULL;

    history = olc_history_new();

    json_array_foreach(arr, index, obj) {
        OLC_CHANGE_ENTRY *entry;

        if (!json_is_object(obj))
            continue;

        entry = calloc(1, sizeof(OLC_CHANGE_ENTRY));
        if (!entry)
            continue;

        entry->id        = (int)json_get_int(obj, "id", 0);
        entry->author    = str_dup(json_get_string(obj, "author", "unknown"));
        entry->field     = str_dup(json_get_string(obj, "field", ""));
        entry->old_value = str_dup(json_get_string(obj, "old_value", ""));
        entry->new_value = str_dup(json_get_string(obj, "new_value", ""));

        /* Parse timestamp — accept ISO-8601 string or raw integer */
        {
            json_t *ts = json_object_get(obj, "timestamp");
            if (json_is_string(ts)) {
                struct tm tm_val;
                memset(&tm_val, 0, sizeof(tm_val));
                if (strptime(json_string_value(ts), "%Y-%m-%dT%H:%M:%S", &tm_val))
                    entry->timestamp = mktime(&tm_val);
                else
                    entry->timestamp = time(NULL);
            } else if (json_is_integer(ts)) {
                entry->timestamp = (time_t)json_integer_value(ts);
            } else {
                entry->timestamp = time(NULL);
            }
        }

        /* Track highest ID for next_id */
        if (entry->id >= history->next_id)
            history->next_id = entry->id + 1;

        list_appendlink(history->entries, entry);
    }

    return history;
}


/***************************************************************************
 * Per-Type File Operations                                                *
 ***************************************************************************/

/**
 * olc_history_load - Load an entity's history from its type file.
 *
 * Opens the type file, extracts the entity's entry array, and
 * deserializes it. Returns NULL if no history found on disk.
 */
OLC_CHANGE_HISTORY *olc_history_load(int hist_type, const char *entity_id)
{
    json_t *root;
    json_t *entries;
    json_t *entity_arr;
    OLC_CHANGE_HISTORY *history;

    if (hist_type < 0 || hist_type >= OLC_HIST_MAX || IS_NULLSTR(entity_id))
        return NULL;

    root = hist_file_open(hist_type);
    if (!root)
        return NULL;

    entries = hist_file_entries(root);
    if (!entries) {
        json_decref(root);
        return NULL;
    }

    entity_arr = json_object_get(entries, entity_id);
    if (!entity_arr || !json_is_array(entity_arr)) {
        json_decref(root);
        return NULL;
    }

    history = olc_history_from_json(entity_arr);
    json_decref(root);
    return history;
}


/**
 * olc_history_mark_dirty - Mark an entity's history for deferred flush.
 *
 * Call this after olc_history_record() to ensure the change persists
 * on the next flush (periodic tick, entity save, or shutdown).
 */
void olc_history_mark_dirty(int hist_type, const char *entity_id,
                            OLC_CHANGE_HISTORY *history)
{
    OLC_HISTORY_DIRTY *d;

    if (hist_type < 0 || hist_type >= OLC_HIST_MAX
        || IS_NULLSTR(entity_id) || !history)
        return;

    /* Update existing entry if found */
    d = dirty_find(hist_type, entity_id);
    if (d) {
        d->history = history;
        return;
    }

    /* Create new dirty entry */
    d = calloc(1, sizeof(OLC_HISTORY_DIRTY));
    if (!d) return;

    d->hist_type = hist_type;
    d->entity_id = str_dup(entity_id);
    d->history   = history;
    d->next      = dirty_list;
    dirty_list   = d;
}


/**
 * olc_history_flush - Immediately write one entity's history to its type file.
 *
 * Loads the type file, replaces the entity's entry, and writes back.
 * Also removes the entity from the dirty list.
 */
void olc_history_flush(int hist_type, const char *entity_id,
                       const OLC_CHANGE_HISTORY *history)
{
    json_t *root;
    json_t *entries;
    json_t *arr;

    if (hist_type < 0 || hist_type >= OLC_HIST_MAX || IS_NULLSTR(entity_id))
        return;

    /* Load existing file or create new */
    root = hist_file_open(hist_type);
    if (!root)
        root = hist_file_new();

    entries = hist_file_entries(root);
    if (!entries) {
        /* Root exists but malformed — rebuild */
        json_object_set_new(root, "entries", json_object());
        entries = json_object_get(root, "entries");
    }

    /* Serialize and set entry */
    arr = olc_history_to_json(history);
    if (json_is_array(arr))
        json_object_set_new(entries, entity_id, arr);
    else {
        json_decref(arr);
        json_object_del(entries, entity_id);
    }

    hist_file_write(hist_type, root);  /* Consumes root ref */
    dirty_remove(hist_type, entity_id);
}


/**
 * olc_history_flush_all - Flush all dirty histories to disk.
 *
 * Groups dirty entries by type, loads each type file once, merges all
 * dirty entries for that type, and writes back. Called from the periodic
 * update tick and on shutdown.
 */
void olc_history_flush_all(void)
{
    int type;
    OLC_HISTORY_DIRTY *d;
    bool has_dirty[OLC_HIST_MAX];

    if (!dirty_list)
        return;

    /* Identify which types have dirty entries */
    for (type = 0; type < OLC_HIST_MAX; type++)
        has_dirty[type] = false;

    for (d = dirty_list; d; d = d->next)
        if (d->hist_type >= 0 && d->hist_type < OLC_HIST_MAX)
            has_dirty[d->hist_type] = true;

    /* Process each dirty type */
    for (type = 0; type < OLC_HIST_MAX; type++) {
        json_t *root;
        json_t *entries;

        if (!has_dirty[type])
            continue;

        /* Load or create the type file */
        root = hist_file_open(type);
        if (!root)
            root = hist_file_new();

        entries = hist_file_entries(root);
        if (!entries) {
            json_object_set_new(root, "entries", json_object());
            entries = json_object_get(root, "entries");
        }

        /* Merge all dirty entries for this type */
        for (d = dirty_list; d; d = d->next) {
            json_t *arr;

            if (d->hist_type != type)
                continue;

            arr = olc_history_to_json(d->history);
            if (json_is_array(arr))
                json_object_set_new(entries, d->entity_id, arr);
            else {
                json_decref(arr);
                json_object_del(entries, d->entity_id);
            }
        }

        hist_file_write(type, root);  /* Consumes root ref */
    }

    dirty_clear();
}


/**
 * olc_history_remove - Remove an entity's history from its type file.
 *
 * Used when an entity is deleted. Also clears any pending dirty entry.
 */
void olc_history_remove(int hist_type, const char *entity_id)
{
    json_t *root;
    json_t *entries;

    if (hist_type < 0 || hist_type >= OLC_HIST_MAX || IS_NULLSTR(entity_id))
        return;

    /* Remove from dirty list */
    dirty_remove(hist_type, entity_id);

    /* Remove from file */
    root = hist_file_open(hist_type);
    if (!root)
        return;

    entries = hist_file_entries(root);
    if (!entries) {
        json_decref(root);
        return;
    }

    if (json_object_get(entries, entity_id)) {
        json_object_del(entries, entity_id);
        hist_file_write(hist_type, root);  /* Consumes root ref */
    } else {
        json_decref(root);
    }
}
