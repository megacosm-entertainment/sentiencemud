/***************************************************************************
 *  JSON Changesets - Serialization for game settings changeset history    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "json_changesets.h"

#define JSON_CHANGESETS_VERSION 1

/**
 * json_save_changesets - Save all changesets to a JSON file
 *
 * Serializes the global changesets array to JSON format. Each changeset
 * includes its id, author, timestamp, comment, and list of setting changes.
 *
 * @param path  File path to write
 * @return      true on success, false on failure
 */
bool json_save_changesets(const char *path)
{
    json_t *root;
    json_t *arr;
    int i;
    ITERATOR it;
    GAME_SETTING_CHANGE_HISTORY *history;

    root = json_object();
    if (!root)
        return false;

    json_object_set_new(root, "version", json_integer(JSON_CHANGESETS_VERSION));
    json_object_set_new(root, "next_changeset_id", json_integer(next_changeset_id));

    arr = json_array();

    for (i = 0; i < changeset_count; i++) {
        GAME_SETTINGS_CHANGESET *cs = changesets[i];
        json_t *cs_obj;
        json_t *changes_arr;

        if (!cs)
            continue;

        cs_obj = json_object();
        json_object_set_new(cs_obj, "id", json_integer(cs->id));
        json_object_set_new(cs_obj, "author", json_string(cs->author ? cs->author : ""));
        json_object_set_new(cs_obj, "timestamp", json_integer((json_int_t)cs->timestamp));
        json_object_set_new(cs_obj, "comment", json_string(cs->comment ? cs->comment : ""));

        changes_arr = json_array();

        iterator_start(&it, cs->changes);
        while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
            json_t *ch_obj = json_object();
            json_object_set_new(ch_obj, "setting", json_string(history->setting->name));
            json_object_set_new(ch_obj, "old_value", json_string(history->old_value ? history->old_value : ""));
            json_object_set_new(ch_obj, "new_value", json_string(history->new_value ? history->new_value : ""));
            json_array_append_new(changes_arr, ch_obj);
        }
        iterator_stop(&it);

        json_object_set_new(cs_obj, "changes", changes_arr);
        json_array_append_new(arr, cs_obj);
    }

    json_object_set_new(root, "changesets", arr);

    if (json_dump_file(root, path, JSON_INDENT(2) | JSON_SORT_KEYS) != 0) {
        pbugf(LOG_ERROR, "Cannot write changeset JSON file '%s'", path);
        json_decref(root);
        return false;
    }

    json_decref(root);
    plogf(LOG_INIT, "Game setting changeset history saved (for rollback/audit).");
    return true;
}

/**
 * json_load_changesets - Load changesets from a JSON file
 *
 * Deserializes changesets from JSON into the global changesets array.
 * Setting names are resolved against the game_settings_table; unrecognized
 * settings are skipped with a warning. Does NOT call free_all_changesets()
 * - the caller is responsible for that.
 *
 * @param path  File path to read
 * @return      true on success, false on failure
 */
bool json_load_changesets(const char *path)
{
    json_t *root;
    json_t *arr;
    json_t *cs_json;
    json_error_t error;
    size_t i;

    root = json_load_file(path, 0, &error);
    if (!root) {
        pbugf(LOG_ERROR, "Cannot parse changeset JSON '%s': %s (line %d)", path, error.text, error.line);
        return false;
    }

    next_changeset_id = (int)json_integer_value(json_object_get(root, "next_changeset_id"));

    arr = json_object_get(root, "changesets");
    if (!json_is_array(arr)) {
        pbugf(LOG_ERROR, "Changeset JSON missing 'changesets' array");
        json_decref(root);
        return false;
    }

    changeset_count = 0;

    json_array_foreach(arr, i, cs_json) {
        GAME_SETTINGS_CHANGESET *cs;
        json_t *changes_arr;
        json_t *ch_json;
        size_t j;
        const char *str;

        if (changeset_count >= MAX_CHANGESETS) {
            pwarnf(LOG_INIT, "Too many changesets in JSON, ignoring extras");
            break;
        }

        cs = alloc_mem(sizeof(GAME_SETTINGS_CHANGESET));
        if (!cs)
            break;

        cs->id = (int)json_integer_value(json_object_get(cs_json, "id"));

        str = json_string_value(json_object_get(cs_json, "author"));
        cs->author = str_dup(str ? str : "");

        cs->timestamp = (time_t)json_integer_value(json_object_get(cs_json, "timestamp"));

        str = json_string_value(json_object_get(cs_json, "comment"));
        cs->comment = str_dup(str ? str : "");

        cs->changes = list_create(false);

        changes_arr = json_object_get(cs_json, "changes");
        if (json_is_array(changes_arr)) {
            json_array_foreach(changes_arr, j, ch_json) {
                const char *setting_name = json_string_value(json_object_get(ch_json, "setting"));
                const char *old_val = json_string_value(json_object_get(ch_json, "old_value"));
                const char *new_val = json_string_value(json_object_get(ch_json, "new_value"));

                if (!setting_name || !*setting_name)
                    continue;

                const struct game_setting_type *setting = get_game_setting(setting_name);
                if (!setting) {
                    pwarnf(LOG_INIT, "Changeset #%d: setting '%s' not found, skipping", cs->id, setting_name);
                    continue;
                }

                GAME_SETTING_CHANGE_HISTORY *history = alloc_mem(sizeof(GAME_SETTING_CHANGE_HISTORY));
                if (!history)
                    continue;

                history->setting = setting;
                history->old_value = str_dup(old_val ? old_val : "");
                history->new_value = str_dup(new_val ? new_val : "");
                list_appendlink(cs->changes, history);
            }
        }

        changesets[changeset_count++] = cs;
    }

    for (int a = 0; a < changeset_count - 1; a++) {
        for (int b = a + 1; b < changeset_count; b++) {
            if (changesets[a] && changesets[b] &&
                changesets[a]->id > changesets[b]->id) {
                GAME_SETTINGS_CHANGESET *temp = changesets[a];
                changesets[a] = changesets[b];
                changesets[b] = temp;
            }
        }
    }

    plogf(LOG_INIT, "Changeset history loaded from JSON: %d records available for rollback/audit.", changeset_count);
    for (int k = 0; k < changeset_count; k++) {
        plogf(LOG_INIT, " - Changeset #%d by %s with %d changes",
            changesets[k]->id, changesets[k]->author,
            list_size(changesets[k]->changes));
    }

    json_decref(root);
    return true;
}
