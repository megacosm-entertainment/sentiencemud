/**
 * @file olc_commit_history.c
 * @brief Commit-level history for OLC staged editors — implementation.
 */

#include "olc_commit_history.h"
#include "../../merc.h"
#include "../../olc.h"
#include "../../tables.h"
#include "../../log.h"
#include "../../io/cache/redis_cache.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int olc_next_group_id = 1;

/* In-memory cache of loaded histories — avoids repeated disk/Redis reads */
static LLIST *loaded_histories = NULL;

static void ensure_loaded_histories(void)
{
    if (!loaded_histories)
        loaded_histories = list_create(false);
}

/* --- Committed change lifecycle --- */

olc_committed_change_t *olc_committed_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value)
{
    olc_committed_change_t *change = alloc_mem(sizeof(*change));
    change->field_path = str_dup(field_path ? field_path : "");
    change->field_type = type;
    change->old_value = old_value ? json_incref(old_value) : NULL;
    change->new_value = new_value ? json_incref(new_value) : NULL;
    return change;
}

void olc_committed_change_destroy(olc_committed_change_t *change)
{
    if (!change) return;
    free_string(change->field_path);
    if (change->old_value) json_decref(change->old_value);
    if (change->new_value) json_decref(change->new_value);
    free_mem(change, sizeof(*change));
}

/* --- Commit record lifecycle --- */

olc_commit_record_t *olc_commit_record_create(const char *author, int group_id)
{
    olc_commit_record_t *record = alloc_mem(sizeof(*record));
    record->id = 0;
    record->group_id = group_id;
    record->author = str_dup(author ? author : "");
    record->comment = str_dup("");
    record->timestamp = time(NULL);
    record->changes = list_create(false);
    return record;
}

void olc_commit_record_destroy(olc_commit_record_t *record)
{
    if (!record) return;
    free_string(record->author);
    free_string(record->comment);
    if (record->changes) {
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL)
            olc_committed_change_destroy(change);
        iterator_stop(&it);
        list_destroy(record->changes);
    }
    free_mem(record, sizeof(*record));
}

/* --- History lifecycle --- */

olc_commit_history_t *olc_commit_history_create(int editor_type, WNUM_LOAD wnum)
{
    olc_commit_history_t *history = alloc_mem(sizeof(*history));
    history->editor_type = editor_type;
    history->entity_wnum = wnum;
    history->records = list_create(false);
    history->next_id = 1;
    history->max_records = olc_commit_history_get_limit();
    history->is_dirty = false;
    return history;
}

void olc_commit_history_destroy(olc_commit_history_t *history)
{
    if (!history) return;
    if (history->records) {
        ITERATOR it;
        iterator_start(&it, history->records);
        olc_commit_record_t *record;
        while ((record = iterator_nextdata(&it)) != NULL)
            olc_commit_record_destroy(record);
        iterator_stop(&it);
        list_destroy(history->records);
    }
    free_mem(history, sizeof(*history));
}

int olc_commit_history_get_limit(void)
{
    int limit = game_settings.olc_history_limit;
    return (limit > 0) ? limit : OLC_DEFAULT_HISTORY_LIMIT;
}

/* --- History operations --- */

olc_commit_record_t *olc_commit_history_archive(olc_commit_history_t *history,
    olc_changeset_t *cs, int group_id, const char *comment)
{
    if (!history || !cs) return NULL;
    if (olc_changeset_count(cs) == 0) return NULL;

    olc_commit_record_t *record = olc_commit_record_create(cs->author, group_id);
    record->id = history->next_id++;

    if (comment && comment[0] != '\0') {
        free_string(record->comment);
        record->comment = str_dup(comment);
    }

    /* Copy changes from changeset into committed record */
    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *pending;
    while ((pending = iterator_nextdata(&it)) != NULL) {
        olc_committed_change_t *committed = olc_committed_change_create(
            pending->field_path, pending->field_type,
            pending->old_value, pending->new_value);
        list_addlink(record->changes, committed);
    }
    iterator_stop(&it);

    /* Prepend (newest first) */
    list_addlink(history->records, record);

    /* Evict oldest if over limit */
    int limit = history->max_records;
    while (limit > 0 && list_size(history->records) > limit) {
        /* Remove from tail (oldest) */
        olc_commit_record_t *oldest = list_nthdata(history->records,
            list_size(history->records));
        if (oldest) {
            list_remlink(history->records, oldest, false);
            olc_commit_record_destroy(oldest);
        }
    }

    history->is_dirty = true;
    return record;
}

olc_commit_record_t *olc_commit_history_find(olc_commit_history_t *history, int id)
{
    if (!history || !history->records) return NULL;

    ITERATOR it;
    iterator_start(&it, history->records);
    olc_commit_record_t *record;
    while ((record = iterator_nextdata(&it)) != NULL) {
        if (record->id == id) {
            iterator_stop(&it);
            return record;
        }
    }
    iterator_stop(&it);
    return NULL;
}

int olc_commit_history_count(olc_commit_history_t *history)
{
    return (history && history->records) ? list_size(history->records) : 0;
}

/* --- Serialization --- */

static json_t *committed_change_serialize(olc_committed_change_t *change)
{
    json_t *obj = json_object();
    json_object_set_new(obj, "field_path", json_string(change->field_path ? change->field_path : ""));
    json_object_set_new(obj, "field_type", json_integer(change->field_type));
    if (change->old_value)
        json_object_set(obj, "old_value", change->old_value);
    if (change->new_value)
        json_object_set(obj, "new_value", change->new_value);
    return obj;
}

static olc_committed_change_t *committed_change_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_fp = json_object_get(json, "field_path");
    json_t *j_ft = json_object_get(json, "field_type");
    if (!json_is_string(j_fp) || !json_is_integer(j_ft))
        return NULL;

    return olc_committed_change_create(
        json_string_value(j_fp),
        (olc_field_type_t)json_integer_value(j_ft),
        json_object_get(json, "old_value"),
        json_object_get(json, "new_value"));
}

json_t *olc_commit_record_serialize(olc_commit_record_t *record)
{
    if (!record) return NULL;

    json_t *obj = json_object();
    json_object_set_new(obj, "id", json_integer(record->id));
    json_object_set_new(obj, "group_id", json_integer(record->group_id));
    json_object_set_new(obj, "author", json_string(record->author ? record->author : ""));
    json_object_set_new(obj, "comment", json_string(record->comment ? record->comment : ""));
    json_object_set_new(obj, "timestamp", json_integer((json_int_t)record->timestamp));

    json_t *changes_arr = json_array();
    if (record->changes) {
        ITERATOR it;
        iterator_start(&it, record->changes);
        olc_committed_change_t *change;
        while ((change = iterator_nextdata(&it)) != NULL)
            json_array_append_new(changes_arr, committed_change_serialize(change));
        iterator_stop(&it);
    }
    json_object_set_new(obj, "changes", changes_arr);
    return obj;
}

static olc_commit_record_t *commit_record_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_id = json_object_get(json, "id");
    json_t *j_group = json_object_get(json, "group_id");
    json_t *j_author = json_object_get(json, "author");
    if (!json_is_integer(j_id) || !json_is_integer(j_group) || !json_is_string(j_author))
        return NULL;

    olc_commit_record_t *record = olc_commit_record_create(
        json_string_value(j_author), (int)json_integer_value(j_group));
    record->id = (int)json_integer_value(j_id);

    json_t *j_comment = json_object_get(json, "comment");
    if (json_is_string(j_comment)) {
        free_string(record->comment);
        record->comment = str_dup(json_string_value(j_comment));
    }

    json_t *j_ts = json_object_get(json, "timestamp");
    if (json_is_integer(j_ts))
        record->timestamp = (time_t)json_integer_value(j_ts);

    json_t *changes_arr = json_object_get(json, "changes");
    if (json_is_array(changes_arr)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(changes_arr, idx, entry) {
            olc_committed_change_t *change = committed_change_deserialize(entry);
            if (change)
                list_appendlink(record->changes, change);
        }
    }

    return record;
}

json_t *olc_commit_history_serialize(olc_commit_history_t *history)
{
    if (!history) return NULL;

    json_t *root = json_object();
    json_object_set_new(root, "editor_type", json_integer(history->editor_type));
    json_object_set_new(root, "entity_wnum_auid", json_integer(history->entity_wnum.auid));
    json_object_set_new(root, "entity_wnum_vnum", json_integer(history->entity_wnum.vnum));
    json_object_set_new(root, "next_id", json_integer(history->next_id));

    json_t *records_arr = json_array();
    if (history->records) {
        ITERATOR it;
        iterator_start(&it, history->records);
        olc_commit_record_t *record;
        while ((record = iterator_nextdata(&it)) != NULL)
            json_array_append_new(records_arr, olc_commit_record_serialize(record));
        iterator_stop(&it);
    }
    json_object_set_new(root, "records", records_arr);
    return root;
}

olc_commit_history_t *olc_commit_history_deserialize(json_t *json)
{
    if (!json_is_object(json)) return NULL;

    json_t *j_type = json_object_get(json, "editor_type");
    json_t *j_auid = json_object_get(json, "entity_wnum_auid");
    json_t *j_vnum = json_object_get(json, "entity_wnum_vnum");
    if (!json_is_integer(j_type) || !json_is_integer(j_auid) || !json_is_integer(j_vnum))
        return NULL;

    WNUM_LOAD wnum = {
        .auid = json_integer_value(j_auid),
        .vnum = json_integer_value(j_vnum)
    };
    olc_commit_history_t *history = olc_commit_history_create(
        (int)json_integer_value(j_type), wnum);

    json_t *j_next = json_object_get(json, "next_id");
    if (json_is_integer(j_next))
        history->next_id = (int)json_integer_value(j_next);

    json_t *records_arr = json_object_get(json, "records");
    if (json_is_array(records_arr)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(records_arr, idx, entry) {
            olc_commit_record_t *record = commit_record_deserialize(entry);
            if (record)
                list_appendlink(history->records, record);
        }
    }

    history->is_dirty = false;
    return history;
}

/* --- Persistence helpers --- */

static const char *editor_type_dir_name(int editor_type)
{
    switch (editor_type) {
        case ED_AREA:   return "area";
        case ED_ROOM:   return "room";
        case ED_OBJECT: return "object";
        case ED_MOBILE: return "mobile";
        default:        return "unknown";
    }
}

static void ensure_history_dir(int editor_type)
{
    char path[256];
    snprintf(path, sizeof(path), "%shistory", DATA_DIR);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%shistory/%s", DATA_DIR, editor_type_dir_name(editor_type));
    mkdir(path, 0755);
}

static const char *history_disk_path(int editor_type, WNUM_LOAD wnum)
{
    static char path[256];
    snprintf(path, sizeof(path), "%shistory/%s/%ld_%ld.json",
        DATA_DIR, editor_type_dir_name(editor_type), wnum.auid, wnum.vnum);
    return path;
}

static const char *history_redis_key(int editor_type, WNUM_LOAD wnum)
{
    static char key[128];
    snprintf(key, sizeof(key), "olc:history:%s:%ld:%ld",
        editor_type_dir_name(editor_type), wnum.auid, wnum.vnum);
    return key;
}

/* --- Disk I/O --- */

static bool history_save_to_disk(olc_commit_history_t *history)
{
    if (!history) return false;

    ensure_history_dir(history->editor_type);

    json_t *json = olc_commit_history_serialize(history);
    if (!json) return false;

    const char *path = history_disk_path(history->editor_type, history->entity_wnum);
    int result = json_dump_file(json, path, JSON_INDENT(2) | JSON_SORT_KEYS);
    json_decref(json);

    return (result == 0);
}

static olc_commit_history_t *history_load_from_disk(int editor_type, WNUM_LOAD wnum)
{
    const char *path = history_disk_path(editor_type, wnum);

    json_error_t error;
    json_t *json = json_load_file(path, 0, &error);
    if (!json) return NULL;

    olc_commit_history_t *history = olc_commit_history_deserialize(json);
    json_decref(json);
    return history;
}

/* --- Redis cache --- */

static void history_cache_to_redis(olc_commit_history_t *history)
{
    if (!history || !redis_is_available()) return;

    json_t *json = olc_commit_history_serialize(history);
    if (!json) return;

    char *json_str = json_dumps(json, JSON_COMPACT);
    json_decref(json);
    if (!json_str) return;

    redis_cache_persist_data(
        history_redis_key(history->editor_type, history->entity_wnum),
        json_str);
    free(json_str);
}

static olc_commit_history_t *history_load_from_redis(int editor_type, WNUM_LOAD wnum)
{
    if (!redis_is_available()) return NULL;

    char *json_str = redis_get_persist_data(history_redis_key(editor_type, wnum));
    if (!json_str) return NULL;

    json_error_t error;
    json_t *json = json_loads(json_str, 0, &error);
    free(json_str);
    if (!json) return NULL;

    olc_commit_history_t *history = olc_commit_history_deserialize(json);
    json_decref(json);
    return history;
}

/* --- Public persistence API --- */

olc_commit_history_t *olc_commit_history_load(int editor_type, WNUM_LOAD wnum)
{
    /* Try Redis first */
    olc_commit_history_t *history = history_load_from_redis(editor_type, wnum);
    if (history) return history;

    /* Fall back to disk */
    history = history_load_from_disk(editor_type, wnum);
    if (history) {
        /* Populate Redis cache */
        history_cache_to_redis(history);
    }
    return history;
}

bool olc_commit_history_save(olc_commit_history_t *history)
{
    if (!history) return false;

    bool ok = history_save_to_disk(history);
    if (ok) {
        history_cache_to_redis(history);
        history->is_dirty = false;
    }
    return ok;
}

void olc_commit_history_delete(int editor_type, WNUM_LOAD wnum)
{
    const char *path = history_disk_path(editor_type, wnum);
    unlink(path);

    if (redis_is_available())
        redis_delete_persist_data(history_redis_key(editor_type, wnum));
}

/* --- In-memory cache and lazy loading --- */

olc_commit_history_t *olc_commit_history_get_or_load(int editor_type, WNUM_LOAD wnum)
{
    ensure_loaded_histories();

    /* Check in-memory cache */
    ITERATOR it;
    iterator_start(&it, loaded_histories);
    olc_commit_history_t *history;
    while ((history = iterator_nextdata(&it)) != NULL) {
        if (history->editor_type == editor_type
            && history->entity_wnum.auid == wnum.auid
            && history->entity_wnum.vnum == wnum.vnum) {
            iterator_stop(&it);
            return history;
        }
    }
    iterator_stop(&it);

    /* Try loading from Redis/disk */
    history = olc_commit_history_load(editor_type, wnum);
    if (!history) {
        /* Create fresh */
        history = olc_commit_history_create(editor_type, wnum);
    }

    list_addlink(loaded_histories, history);
    return history;
}

void olc_commit_history_save_all_dirty(void)
{
    if (!loaded_histories) return;

    ITERATOR it;
    iterator_start(&it, loaded_histories);
    olc_commit_history_t *history;
    while ((history = iterator_nextdata(&it)) != NULL) {
        if (history->is_dirty)
            olc_commit_history_save(history);
    }
    iterator_stop(&it);
}
