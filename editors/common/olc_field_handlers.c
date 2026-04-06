/**
 * @file olc_field_handlers.c
 * @brief Field handler lookup, generic scalar apply, and commit/revert.
 */

#include <sys/types.h>
#include <string.h>

#include "../../merc.h"
#include "../../log.h"
#include "olc_changeset.h"
#include "olc_field_handlers.h"

/*
 * Check if a handler pattern matches a given field path.
 *
 * Supports exact match, single-level wildcard (slash-star), and
 * multi-level wildcard (slash-star-star).
 */
static bool field_path_matches(const char *pattern, const char *path)
{
    if (!pattern || !path)
        return false;

    /* Exact match */
    if (strcmp(pattern, path) == 0)
        return true;

    size_t plen = strlen(pattern);

    /* Multi-level wildcard: pattern ends with slash-star-star */
    if (plen >= 3 && pattern[plen - 1] == '*' && pattern[plen - 2] == '*'
        && pattern[plen - 3] == '/') {
        size_t prefix_len = plen - 2; /* include the '/' */
        if (strncmp(pattern, path, prefix_len) == 0 && path[prefix_len] != '\0')
            return true;
    }

    /* Single-level wildcard: pattern ends with slash-star */
    if (plen >= 2 && pattern[plen - 1] == '*' && pattern[plen - 2] == '/') {
        size_t prefix_len = plen - 1; /* include the '/' */
        if (strncmp(pattern, path, prefix_len) == 0 && path[prefix_len] != '\0') {
            if (strchr(path + prefix_len, '/') == NULL)
                return true;
        }
    }

    return false;
}

/*
 * Find the best field handler for a given field path and type.
 *
 * Searches a NULL-terminated handler array. Exact matches take priority
 * over wildcard matches. Returns NULL if no handler matches.
 */
const olc_field_handler_t *olc_find_field_handler(
    const olc_field_handler_t *handlers,
    const char *field_path, olc_field_type_t type)
{
    if (!handlers || !field_path)
        return NULL;

    const olc_field_handler_t *wildcard_match = NULL;

    for (int i = 0; handlers[i].field_path != NULL; i++) {
        if (strcmp(handlers[i].field_path, field_path) == 0) {
            /* Exact match — return immediately */
            return &handlers[i];
        }

        if (!wildcard_match && field_path_matches(handlers[i].field_path, field_path)) {
            wildcard_match = &handlers[i];
        }
    }

    return wildcard_match;
}

/*
 * Generic string apply: reads json string from change->new_value,
 * frees old string at *field_ptr, and replaces with str_dup of new value.
 */
bool olc_apply_generic_string(char **field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    const char *val = json_string_value(change->new_value);
    if (!val)
        return false;

    free_string(*field_ptr);
    *field_ptr = str_dup(val);
    return true;
}

/*
 * Generic int apply: reads json integer from change->new_value,
 * writes to *field_ptr as int.
 */
bool olc_apply_generic_int(int *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    if (!json_is_integer(change->new_value))
        return false;

    *field_ptr = (int)json_integer_value(change->new_value);
    return true;
}

/*
 * Generic int16 apply: reads json integer from change->new_value,
 * writes to *field_ptr as int16_t.
 */
bool olc_apply_generic_int16(int16_t *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    if (!json_is_integer(change->new_value))
        return false;

    *field_ptr = (int16_t)json_integer_value(change->new_value);
    return true;
}

/*
 * Generic bool apply: reads json boolean from change->new_value,
 * writes to *field_ptr.
 */
bool olc_apply_generic_bool(bool *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    if (!json_is_boolean(change->new_value))
        return false;

    *field_ptr = json_is_true(change->new_value);
    return true;
}

/*
 * Generic flags apply: reads json integer from change->new_value,
 * writes to *field_ptr as long.
 */
bool olc_apply_generic_flags(long *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    if (!json_is_integer(change->new_value))
        return false;

    *field_ptr = (long)json_integer_value(change->new_value);
    return true;
}

/*
 * Generic long apply: reads json integer from change->new_value,
 * writes to *field_ptr as long.
 */
bool olc_apply_generic_long(long *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value)
        return false;

    if (!json_is_integer(change->new_value))
        return false;

    *field_ptr = (long)json_integer_value(change->new_value);
    return true;
}

/*
 * Generic dice apply: reads JSON object with "number", "size", "bonus"
 * from change->new_value, writes to DICE_DATA fields.
 */
bool olc_apply_generic_dice(DICE_DATA *field_ptr, olc_pending_change_t *change)
{
    if (!field_ptr || !change || !change->new_value) return false;
    if (!json_is_object(change->new_value)) return false;

    json_t *jnum = json_object_get(change->new_value, "number");
    json_t *jsiz = json_object_get(change->new_value, "size");
    json_t *jbon = json_object_get(change->new_value, "bonus");

    if (!json_is_integer(jnum) || !json_is_integer(jsiz) || !json_is_integer(jbon))
        return false;

    field_ptr->number = (int)json_integer_value(jnum);
    field_ptr->size   = (int)json_integer_value(jsiz);
    field_ptr->bonus  = (int)json_integer_value(jbon);
    return true;
}

/**
 * Compare function for list operation ordering.
 * Removes (rm:N paths) sort before adds (add:N paths).
 * Within removes, sort by descending index.
 * Within adds, sort by ascending sequence.
 * Non-list changes sort to the front (applied first, preserving order).
 */
static int list_op_sort_compare(const void *a, const void *b)
{
    const olc_pending_change_t *ca = *(const olc_pending_change_t **)a;
    const olc_pending_change_t *cb = *(const olc_pending_change_t **)b;

    bool a_is_rm  = (strstr(ca->field_path, "/rm:") != NULL);
    bool b_is_rm  = (strstr(cb->field_path, "/rm:") != NULL);
    bool a_is_add = (strstr(ca->field_path, "/add:") != NULL);
    bool b_is_add = (strstr(cb->field_path, "/add:") != NULL);
    bool a_is_list = a_is_rm || a_is_add;
    bool b_is_list = b_is_rm || b_is_add;

    /* Non-list changes come first (preserve insertion order) */
    if (!a_is_list && !b_is_list) return 0;
    if (!a_is_list) return -1;
    if (!b_is_list) return 1;

    /* Removes come before adds */
    if (a_is_rm && b_is_add) return -1;
    if (a_is_add && b_is_rm) return 1;

    /* Within removes, sort by descending target index */
    if (a_is_rm && b_is_rm) {
        int a_target = (int)json_integer_value(
            json_object_get(ca->new_value, "index"));
        int b_target = (int)json_integer_value(
            json_object_get(cb->new_value, "index"));
        return b_target - a_target; /* descending */
    }

    /* Within adds, sort by ascending sequence */
    if (a_is_add && b_is_add) {
        const char *a_seq = strstr(ca->field_path, "/add:") + 5;
        const char *b_seq = strstr(cb->field_path, "/add:") + 5;
        return atoi(a_seq) - atoi(b_seq);
    }

    return 0;
}

/*
 * Commit all pending changes in a changeset to a live entity.
 *
 * Iterates changes, looks up handler for each, calls apply_fn.
 * If handler has NULL apply_fn, logs a warning and skips.
 * List operations are sorted: removes (descending index) before adds (ascending).
 * On success, clears the changeset and returns number of changes applied.
 * On error, returns -1 and sets *error_field if provided.
 */
int olc_changeset_commit(olc_changeset_t *cs, void *entity,
    const olc_field_handler_t *handlers, const char **error_field)
{
    if (!cs || !entity) {
        if (error_field) *error_field = NULL;
        return -1;
    }

    int applied = 0;
    int change_count = list_size(cs->changes);

    if (change_count > 0) {
        olc_pending_change_t **sorted = alloca(change_count * sizeof(*sorted));
        int idx = 0;
        ITERATOR it;
        iterator_start(&it, cs->changes);
        olc_pending_change_t *change;
        while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL)
            sorted[idx++] = change;
        iterator_stop(&it);

        qsort(sorted, change_count, sizeof(*sorted), list_op_sort_compare);

        for (int i = 0; i < change_count; i++) {
            change = sorted[i];
            const olc_field_handler_t *handler = olc_find_field_handler(
                handlers, change->field_path, change->field_type);

            if (!handler) {
                if (error_field) *error_field = change->field_path;
                return -1;
            }

            if (!handler->apply_fn) {
                log_message_f(LOG_LEVEL_WARN, "olc",
                    "Field handler for '%s' has no apply_fn, skipping",
                    change->field_path);
                continue;
            }

            if (!handler->apply_fn(entity, change)) {
                if (error_field) *error_field = change->field_path;
                return -1;
            }

            applied++;
        }
    }

    olc_changeset_clear(cs);
    return applied;
}

/*
 * Revert all pending changes by clearing the changeset.
 */
void olc_changeset_revert(olc_changeset_t *cs)
{
    olc_changeset_clear(cs);
}

/*
 * Revert a single field by removing its pending change.
 */
bool olc_changeset_revert_field(olc_changeset_t *cs, const char *field_path)
{
    return olc_changeset_remove_change(cs, field_path);
}
