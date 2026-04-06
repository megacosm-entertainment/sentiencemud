/**
 * @file olc_changeset.c
 * @brief Implementation of OLC changeset CRUD operations
 */

#include <sys/types.h>
#include <string.h>

#include "../../merc.h"
#include "olc_changeset.h"

/*
 * Internal helper: create a pending change record.
 * Increfs JSON values so the changeset owns its own references.
 */
olc_pending_change_t *olc_pending_change_create(const char *field_path,
                                                 olc_field_type_t field_type,
                                                 json_t *old_value, json_t *new_value)
{
    olc_pending_change_t *change = alloc_mem(sizeof(*change));

    change->field_path = str_dup(field_path);
    change->field_type = field_type;
    change->old_value  = old_value ? json_incref(old_value) : NULL;
    change->new_value  = new_value ? json_incref(new_value) : NULL;

    return change;
}

/*
 * Internal helper: destroy a pending change and release all resources.
 */
void olc_pending_change_destroy(olc_pending_change_t *change)
{
    if (!change)
        return;

    free_string(change->field_path);

    if (change->old_value)
        json_decref(change->old_value);
    if (change->new_value)
        json_decref(change->new_value);

    free_mem(change, sizeof(*change));
}

/*
 * Create a new changeset for the given entity.
 */
olc_changeset_t *olc_changeset_create(int editor_type, WNUM_LOAD entity_wnum,
                                       const char *entity_label, const char *author)
{
    olc_changeset_t *cs = alloc_mem(sizeof(*cs));

    cs->editor_type  = editor_type;
    cs->entity_wnum  = entity_wnum;
    cs->entity_label = str_dup(entity_label ? entity_label : "");
    cs->author       = str_dup(author ? author : "");
    cs->changes      = list_create(false);
    cs->created_at   = current_time;
    cs->updated_at   = current_time;
    cs->is_dirty     = false;

    return cs;
}

/*
 * Destroy a changeset and all its pending changes.
 */
void olc_changeset_destroy(olc_changeset_t *cs)
{
    if (!cs)
        return;

    olc_changeset_clear(cs);
    list_destroy(cs->changes);
    free_string(cs->entity_label);
    free_string(cs->author);
    free_mem(cs, sizeof(*cs));
}

/*
 * Find an existing change for the given field path.
 */
olc_pending_change_t *olc_changeset_find_change(olc_changeset_t *cs, const char *field_path)
{
    if (!cs || !field_path)
        return NULL;

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        if (strcmp(change->field_path, field_path) == 0) {
            iterator_stop(&it);
            return change;
        }
    }
    iterator_stop(&it);

    return NULL;
}

static bool is_list_operation(olc_field_type_t type)
{
    return type == OLC_FIELD_LIST_ADD
        || type == OLC_FIELD_LIST_REMOVE
        || type == OLC_FIELD_LIST_UPDATE;
}

static bool collapse_list_operation(olc_changeset_t *cs,
    olc_pending_change_t *existing, olc_field_type_t new_type,
    json_t *old_value, json_t *new_value, olc_pending_change_t **out)
{
    *out = NULL;

    /* ADD + REMOVE → no-op */
    if (existing->field_type == OLC_FIELD_LIST_ADD && new_type == OLC_FIELD_LIST_REMOVE) {
        olc_changeset_remove_change(cs, existing->field_path);
        return true;
    }

    /* ADD + UPDATE → ADD with updated new_value */
    if (existing->field_type == OLC_FIELD_LIST_ADD && new_type == OLC_FIELD_LIST_UPDATE) {
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* REMOVE + ADD → UPDATE */
    if (existing->field_type == OLC_FIELD_LIST_REMOVE && new_type == OLC_FIELD_LIST_ADD) {
        existing->field_type = OLC_FIELD_LIST_UPDATE;
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* UPDATE + UPDATE → keep original old_value, update new_value */
    if (existing->field_type == OLC_FIELD_LIST_UPDATE && new_type == OLC_FIELD_LIST_UPDATE) {
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    /* UPDATE + REMOVE → REMOVE with original old_value */
    if (existing->field_type == OLC_FIELD_LIST_UPDATE && new_type == OLC_FIELD_LIST_REMOVE) {
        existing->field_type = OLC_FIELD_LIST_REMOVE;
        if (existing->new_value) json_decref(existing->new_value);
        existing->new_value = NULL;
        cs->updated_at = current_time;
        cs->is_dirty = true;
        *out = existing;
        return true;
    }

    return false;
}

/*
 * Add or update a change in the changeset.
 *
 * No-op detection:
 *   - If old_value == new_value, any existing change is removed and NULL returned.
 *   - If updating an existing change and the new value matches the original old_value,
 *     the change collapses to a no-op and is removed.
 *
 * Returns the change record on success, or NULL on no-op / safety limit.
 */
olc_pending_change_t *olc_changeset_add_change(olc_changeset_t *cs, const char *field_path,
                                                olc_field_type_t field_type,
                                                json_t *old_value, json_t *new_value)
{
    if (!cs || !field_path)
        return NULL;

    /* No-op: old and new are identical */
    if (json_equal(old_value, new_value)) {
        olc_changeset_remove_change(cs, field_path);
        return NULL;
    }

    /* Check for existing change on this field */
    olc_pending_change_t *existing = olc_changeset_find_change(cs, field_path);

    if (existing) {
        /* List operation collapsing */
        if (is_list_operation(existing->field_type) || is_list_operation(field_type)) {
            olc_pending_change_t *result;
            if (collapse_list_operation(cs, existing, field_type, old_value, new_value, &result))
                return result;
        }

        /* Update in place: keep original old_value, replace new_value */
        if (existing->new_value)
            json_decref(existing->new_value);
        existing->new_value = new_value ? json_incref(new_value) : NULL;
        existing->field_type = field_type;

        /* Collapse check: if new now equals original old, it's a no-op */
        if (json_equal(existing->old_value, existing->new_value)) {
            olc_changeset_remove_change(cs, field_path);
            return NULL;
        }

        cs->updated_at = current_time;
        cs->is_dirty = true;
        return existing;
    }

    /* Safety limit */
    if (list_size(cs->changes) >= OLC_MAX_PENDING_PER_ENTITY)
        return NULL;

    /* Create new change */
    olc_pending_change_t *change = olc_pending_change_create(field_path, field_type,
                                                              old_value, new_value);
    list_addlink(cs->changes, change);

    cs->updated_at = current_time;
    cs->is_dirty = true;
    return change;
}

/*
 * Remove a change for the given field path.
 * Returns true if a change was found and removed.
 */
bool olc_changeset_remove_change(olc_changeset_t *cs, const char *field_path)
{
    if (!cs || !field_path)
        return false;

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        if (strcmp(change->field_path, field_path) == 0) {
            iterator_remcurrent(&it);
            iterator_stop(&it);
            olc_pending_change_destroy(change);
            cs->updated_at = current_time;
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

/**
 * Get the next available sequence number for a path prefix.
 * Scans changes matching "{prefix}:" and returns max + 1.
 * E.g., for prefix "affects/add" with existing changes "affects/add:0"
 * and "affects/add:2", returns 3.
 */
int olc_changeset_next_seq(olc_changeset_t *cs, const char *prefix)
{
    if (!cs || !prefix)
        return 0;

    int max_seq = -1;
    size_t prefix_len = strlen(prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        if (strncmp(change->field_path, prefix, prefix_len) == 0
            && change->field_path[prefix_len] == ':') {
            int seq = atoi(change->field_path + prefix_len + 1);
            if (seq > max_seq)
                max_seq = seq;
        }
    }
    iterator_stop(&it);

    return max_seq + 1;
}

/**
 * Remove all changes whose field_path starts with the given prefix followed
 * by '/'. Also removes an exact match on the prefix itself.
 * Returns the number of changes removed.
 */
int olc_changeset_revert_prefix(olc_changeset_t *cs, const char *prefix)
{
    if (!cs || !prefix)
        return 0;

    int removed = 0;
    size_t prefix_len = strlen(prefix);

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = iterator_nextdata(&it)) != NULL) {
        bool match = (strcmp(change->field_path, prefix) == 0)
            || (strncmp(change->field_path, prefix, prefix_len) == 0
                && change->field_path[prefix_len] == '/');
        if (match) {
            iterator_remcurrent(&it);
            olc_pending_change_destroy(change);
            removed++;
        }
    }
    iterator_stop(&it);

    if (removed > 0)
        cs->is_dirty = true;

    return removed;
}

/*
 * Remove and destroy all changes in the changeset.
 */
void olc_changeset_clear(olc_changeset_t *cs)
{
    if (!cs)
        return;

    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        iterator_remcurrent(&it);
        olc_pending_change_destroy(change);
    }
    iterator_stop(&it);

    cs->updated_at = current_time;
    cs->is_dirty = false;
}

/*
 * Return the number of pending changes.
 */
int olc_changeset_count(olc_changeset_t *cs)
{
    if (!cs)
        return 0;

    return list_size(cs->changes);
}

/*
 * Create a new per-builder edit state.
 */
olc_edit_state_t *olc_edit_state_create(void)
{
    olc_edit_state_t *state = alloc_mem(sizeof(*state));

    state->active_changesets    = list_create(false);
    state->string_edit_sessions = list_create(false);
    state->next_string_session_id = 0;

    return state;
}

/*
 * Destroy an edit state and all its changesets.
 */
void olc_edit_state_destroy(olc_edit_state_t *state)
{
    if (!state)
        return;

    ITERATOR it;
    iterator_start(&it, state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = (olc_changeset_t *)iterator_nextdata(&it)) != NULL) {
        iterator_remcurrent(&it);
        olc_changeset_destroy(cs);
    }
    iterator_stop(&it);

    iterator_start(&it, state->string_edit_sessions);
    olc_string_edit_session_t *session;
    while ((session = (olc_string_edit_session_t *)iterator_nextdata(&it)) != NULL) {
        iterator_remcurrent(&it);
        olc_string_session_destroy(session);
    }
    iterator_stop(&it);

    list_destroy(state->active_changesets);
    list_destroy(state->string_edit_sessions);
    free_mem(state, sizeof(*state));
}

/*
 * Find a changeset by editor type and entity wnum.
 */
olc_changeset_t *olc_edit_state_find_changeset(olc_edit_state_t *state,
                                                int editor_type, WNUM_LOAD entity_wnum)
{
    if (!state)
        return NULL;

    ITERATOR it;
    iterator_start(&it, state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = (olc_changeset_t *)iterator_nextdata(&it)) != NULL) {
        if (cs->editor_type == editor_type
            && cs->entity_wnum.auid == entity_wnum.auid
            && cs->entity_wnum.vnum == entity_wnum.vnum) {
            iterator_stop(&it);
            return cs;
        }
    }
    iterator_stop(&it);

    return NULL;
}

/*
 * Sum the total number of pending changes across all active changesets.
 */
int olc_edit_state_total_pending(olc_edit_state_t *state)
{
    if (!state)
        return 0;

    int total = 0;
    ITERATOR it;
    iterator_start(&it, state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = (olc_changeset_t *)iterator_nextdata(&it)) != NULL) {
        total += olc_changeset_count(cs);
    }
    iterator_stop(&it);

    return total;
}

/* =========================================================================
 * String Edit Sessions
 * ========================================================================= */

olc_string_edit_session_t *olc_string_session_create(olc_edit_state_t *state,
    const char *entity_id, const char *field_path,
    char **field_ptr, olc_changeset_t *changeset)
{
    if (!state) return NULL;

    olc_string_edit_session_t *session = alloc_mem(sizeof(*session));
    session->session_id = ++state->next_string_session_id;
    session->entity_id = str_dup(entity_id ? entity_id : "");
    session->field_path = str_dup(field_path ? field_path : "");
    session->field_ptr = field_ptr;
    session->changeset = changeset;

    list_addlink(state->string_edit_sessions, session);
    return session;
}

void olc_string_session_destroy(olc_string_edit_session_t *session)
{
    if (!session) return;
    if (session->entity_id) free_string(session->entity_id);
    if (session->field_path) free_string(session->field_path);
    free_mem(session, sizeof(*session));
}

olc_string_edit_session_t *olc_string_session_find(olc_edit_state_t *state,
    int session_id)
{
    if (!state || !state->string_edit_sessions) return NULL;

    ITERATOR it;
    iterator_start(&it, state->string_edit_sessions);
    olc_string_edit_session_t *session;
    while ((session = (olc_string_edit_session_t *)iterator_nextdata(&it)) != NULL) {
        if (session->session_id == session_id) {
            iterator_stop(&it);
            return session;
        }
    }
    iterator_stop(&it);
    return NULL;
}

void olc_string_session_remove(olc_edit_state_t *state, int session_id)
{
    if (!state || !state->string_edit_sessions) return;

    olc_string_edit_session_t *session = olc_string_session_find(state, session_id);
    if (!session) return;

    list_remlink(state->string_edit_sessions, session, false);
    olc_string_session_destroy(session);
}

/* =========================================================================
 * Serialization
 * ========================================================================= */

json_t *olc_changeset_serialize(olc_changeset_t *cs)
{
    if (!cs) return NULL;

    json_t *root = json_object();
    json_object_set_new(root, "editor_type", json_integer(cs->editor_type));
    json_object_set_new(root, "entity_wnum_auid", json_integer(cs->entity_wnum.auid));
    json_object_set_new(root, "entity_wnum_vnum", json_integer(cs->entity_wnum.vnum));
    json_object_set_new(root, "entity_label",
        json_string(cs->entity_label ? cs->entity_label : ""));
    json_object_set_new(root, "author",
        json_string(cs->author ? cs->author : ""));
    json_object_set_new(root, "created_at", json_integer((json_int_t)cs->created_at));

    json_t *changes_arr = json_array();
    ITERATOR it;
    iterator_start(&it, cs->changes);
    olc_pending_change_t *change;
    while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
        json_t *entry = json_object();
        json_object_set_new(entry, "field_path",
            json_string(change->field_path ? change->field_path : ""));
        json_object_set_new(entry, "field_type", json_integer(change->field_type));
        if (change->old_value)
            json_object_set(entry, "old_value", change->old_value);
        if (change->new_value)
            json_object_set(entry, "new_value", change->new_value);
        json_array_append_new(changes_arr, entry);
    }
    iterator_stop(&it);
    json_object_set_new(root, "changes", changes_arr);

    return root;
}

olc_changeset_t *olc_changeset_deserialize(json_t *json)
{
    if (!json || !json_is_object(json)) return NULL;

    json_t *j_type = json_object_get(json, "editor_type");
    json_t *j_auid = json_object_get(json, "entity_wnum_auid");
    json_t *j_vnum = json_object_get(json, "entity_wnum_vnum");
    json_t *j_label = json_object_get(json, "entity_label");
    json_t *j_author = json_object_get(json, "author");

    if (!json_is_integer(j_type) || !json_is_integer(j_auid)
        || !json_is_integer(j_vnum) || !json_is_string(j_label)
        || !json_is_string(j_author))
        return NULL;

    WNUM_LOAD wnum = {
        .auid = json_integer_value(j_auid),
        .vnum = json_integer_value(j_vnum)
    };

    olc_changeset_t *cs = olc_changeset_create(
        (int)json_integer_value(j_type), wnum,
        json_string_value(j_label), json_string_value(j_author));
    if (!cs) return NULL;

    json_t *j_created = json_object_get(json, "created_at");
    if (json_is_integer(j_created))
        cs->created_at = (time_t)json_integer_value(j_created);

    json_t *changes_arr = json_object_get(json, "changes");
    if (json_is_array(changes_arr)) {
        size_t idx;
        json_t *entry;
        json_array_foreach(changes_arr, idx, entry) {
            json_t *j_fp = json_object_get(entry, "field_path");
            json_t *j_ft = json_object_get(entry, "field_type");
            if (!json_is_string(j_fp) || !json_is_integer(j_ft))
                continue;

            json_t *old_v = json_object_get(entry, "old_value");
            json_t *new_v = json_object_get(entry, "new_value");

            olc_changeset_add_change(cs, json_string_value(j_fp),
                (olc_field_type_t)json_integer_value(j_ft),
                old_v, new_v);
        }
    }

    cs->is_dirty = false;
    return cs;
}

/* =========================================================================
 * Draft Persistence
 * ========================================================================= */

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static void ensure_draft_dir(const char *author)
{
    char path[256];
    snprintf(path, sizeof(path), "%sdrafts", DATA_DIR);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%sdrafts/%s", DATA_DIR, author);
    mkdir(path, 0755);
}

static const char *olc_draft_path(const char *author, int editor_type,
    WNUM_LOAD wnum)
{
    static char path[256];
    snprintf(path, sizeof(path), "%sdrafts/%s/%d_%ld_%ld.json",
        DATA_DIR, author, editor_type, wnum.auid, wnum.vnum);
    return path;
}

bool olc_draft_save(olc_changeset_t *cs)
{
    if (!cs || !cs->author) return false;

    json_t *json = olc_changeset_serialize(cs);
    if (!json) return false;

    /* Check size limit */
    char *dump = json_dumps(json, JSON_COMPACT);
    if (dump) {
        size_t len = strlen(dump);
        free(dump);
        if (len > OLC_MAX_DRAFT_SIZE) {
            json_decref(json);
            return false;
        }
    }

    ensure_draft_dir(cs->author);
    const char *path = olc_draft_path(cs->author, cs->editor_type,
        cs->entity_wnum);

    /* Write atomically: temp file + rename */
    char tmp_path[270];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    int rc = json_dump_file(json, tmp_path, JSON_INDENT(2));
    json_decref(json);

    if (rc != 0) {
        unlink(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }

    cs->is_dirty = false;
    return true;
}

olc_changeset_t *olc_draft_load(const char *author, int editor_type,
    WNUM_LOAD entity_wnum)
{
    if (!author) return NULL;

    const char *path = olc_draft_path(author, editor_type, entity_wnum);

    json_error_t error;
    json_t *json = json_load_file(path, 0, &error);
    if (!json) return NULL;

    olc_changeset_t *cs = olc_changeset_deserialize(json);
    json_decref(json);
    return cs;
}

bool olc_draft_discard(const char *author, int editor_type,
    WNUM_LOAD entity_wnum)
{
    if (!author) return false;
    const char *path = olc_draft_path(author, editor_type, entity_wnum);
    return (unlink(path) == 0);
}

bool olc_draft_exists(const char *author, int editor_type,
    WNUM_LOAD entity_wnum)
{
    if (!author) return false;
    const char *path = olc_draft_path(author, editor_type, entity_wnum);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

void olc_draft_auto_save(olc_edit_state_t *state)
{
    if (!state || !state->active_changesets) return;

    ITERATOR it;
    iterator_start(&it, state->active_changesets);
    olc_changeset_t *cs;
    while ((cs = (olc_changeset_t *)iterator_nextdata(&it)) != NULL) {
        if (olc_changeset_count(cs) > 0)
            olc_draft_save(cs);
    }
    iterator_stop(&it);
}
