/**
 * @file olc_changeset.h
 * @brief Core data structures and CRUD operations for OLC changeset tracking
 *
 * Changesets track pending field-level edits made by builders in OLC editors.
 * Each changeset represents modifications to a single entity (room, mobile, etc.)
 * and contains a list of individual field changes with old/new value pairs.
 */

#ifndef __OLC_CHANGESET_H__
#define __OLC_CHANGESET_H__

#include "../../merc.h"
#include <jansson.h>

/* Safety limits */
#define OLC_MAX_PENDING_PER_ENTITY  100
#define OLC_MAX_PENDING_PER_BUILDER 500
#define OLC_MAX_DRAFT_SIZE          (64 * 1024)

/**
 * Field types that can be tracked in a changeset.
 */
typedef enum {
    OLC_FIELD_STRING,
    OLC_FIELD_INT,
    OLC_FIELD_INT16,
    OLC_FIELD_BOOL,
    OLC_FIELD_FLAGS,
    OLC_FIELD_WIDEVNUM,
    OLC_FIELD_EXIT,
    OLC_FIELD_EMBEDDED,
    OLC_FIELD_LIST_ADD,
    OLC_FIELD_LIST_REMOVE,
    OLC_FIELD_LIST_UPDATE,
    OLC_FIELD_MULTILINE,
    OLC_FIELD_TYPE_DATA
} olc_field_type_t;

/**
 * A single pending field change within a changeset.
 */
typedef struct olc_pending_change {
    char            *field_path;
    olc_field_type_t field_type;
    json_t          *old_value;
    json_t          *new_value;
} olc_pending_change_t;

/**
 * A changeset tracking all pending changes to a single entity.
 */
typedef struct olc_changeset {
    int              editor_type;
    WNUM_LOAD        entity_wnum;
    char            *entity_label;
    char            *author;
    LLIST           *changes;
    time_t           created_at;
    time_t           updated_at;
    bool             is_dirty;
} olc_changeset_t;

/**
 * A group of committed changesets with metadata.
 */
typedef struct olc_changeset_group {
    int              group_id;
    char            *comment;
    char            *author;
    time_t           committed_at;
    LLIST           *changesets;
} olc_changeset_group_t;

/**
 * Per-builder edit state tracking active changesets and string edit sessions.
 */
typedef struct olc_edit_state {
    LLIST           *active_changesets;
    LLIST           *string_edit_sessions;
    int              next_string_session_id;
} olc_edit_state_t;

/* Lifecycle API */
olc_changeset_t        *olc_changeset_create(int editor_type, WNUM_LOAD entity_wnum,
                                              const char *entity_label, const char *author);
void                    olc_changeset_destroy(olc_changeset_t *cs);

/* Change CRUD */
olc_pending_change_t   *olc_changeset_add_change(olc_changeset_t *cs, const char *field_path,
                                                  olc_field_type_t field_type,
                                                  json_t *old_value, json_t *new_value);
olc_pending_change_t   *olc_changeset_find_change(olc_changeset_t *cs, const char *field_path);
bool                    olc_changeset_remove_change(olc_changeset_t *cs, const char *field_path);
void                    olc_changeset_clear(olc_changeset_t *cs);
int                     olc_changeset_count(olc_changeset_t *cs);

/* Edit state API */
olc_edit_state_t       *olc_edit_state_create(void);
void                    olc_edit_state_destroy(olc_edit_state_t *state);
olc_changeset_t        *olc_edit_state_find_changeset(olc_edit_state_t *state,
                                                       int editor_type, WNUM_LOAD entity_wnum);
int                     olc_edit_state_total_pending(olc_edit_state_t *state);

/* Internal helpers */
olc_pending_change_t   *olc_pending_change_create(const char *field_path,
                                                   olc_field_type_t field_type,
                                                   json_t *old_value, json_t *new_value);
void                    olc_pending_change_destroy(olc_pending_change_t *change);

#endif /* __OLC_CHANGESET_H__ */
