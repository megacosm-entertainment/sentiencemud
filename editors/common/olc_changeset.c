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
    state->next_string_session_id = 1;

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
