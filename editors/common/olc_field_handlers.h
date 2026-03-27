/**
 * @file olc_field_handlers.h
 * @brief Field handler registration and dispatch for OLC changeset commit.
 *
 * Simple scalar fields (string, int, bool, flags) use generic handlers
 * provided by the framework. Complex fields (exits, lists, embedded structs)
 * use editor-specific handlers registered in OLC_EDITOR_DEF.
 */

#ifndef __OLC_FIELD_HANDLERS_H__
#define __OLC_FIELD_HANDLERS_H__

#include "olc_changeset.h"

/** Maximum registered field handlers per editor */
#define OLC_MAX_FIELD_HANDLERS  64

typedef struct olc_field_handler {
    const char         *field_path;     /**< field pattern, e.g. "exits/STAR" for wildcard */
    olc_field_type_t    type;
    json_t *(*serialize_fn)(void *entity, const char *field_path);
    bool (*apply_fn)(void *entity, olc_pending_change_t *change);
    const char *(*display_fn)(json_t *value);
} olc_field_handler_t;

const olc_field_handler_t *olc_find_field_handler(
    const olc_field_handler_t *handlers,
    const char *field_path, olc_field_type_t type);

/* Generic apply functions for scalar types */
bool olc_apply_generic_string(char **field_ptr, olc_pending_change_t *change);
bool olc_apply_generic_int(int *field_ptr, olc_pending_change_t *change);
bool olc_apply_generic_int16(int16_t *field_ptr, olc_pending_change_t *change);
bool olc_apply_generic_bool(bool *field_ptr, olc_pending_change_t *change);
bool olc_apply_generic_flags(long *field_ptr, olc_pending_change_t *change);

/* Commit/revert API */
int olc_changeset_commit(olc_changeset_t *cs, void *entity,
    const olc_field_handler_t *handlers, const char **error_field);
void olc_changeset_revert(olc_changeset_t *cs);
bool olc_changeset_revert_field(olc_changeset_t *cs, const char *field_path);

#endif /* !def __OLC_FIELD_HANDLERS_H__ */
