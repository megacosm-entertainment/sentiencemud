/**
 * @file olc_commit_history.c
 * @brief Commit-level history for OLC staged editors — implementation.
 */

#include "olc_commit_history.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../log.h"
#include "../../io/cache/redis_cache.h"
#include <string.h>
#include <sys/stat.h>

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
