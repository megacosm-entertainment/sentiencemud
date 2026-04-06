/**
 * @file olc_commit_history.h
 * @brief Commit-level history for OLC staged editors.
 *
 * Tracks committed changesets as revertable records. Each entity has its own
 * history, capped at a configurable limit (game setting: olc_history_limit).
 * History is lazy-loaded from disk, cached in Redis.
 */

#ifndef __OLC_COMMIT_HISTORY_H__
#define __OLC_COMMIT_HISTORY_H__

#include "olc_changeset.h"

/* Default history limit if game setting is 0 or unset */
#define OLC_DEFAULT_HISTORY_LIMIT  20

/**
 * A single field change within a committed record.
 * Mirrors olc_pending_change_t but is immutable once archived.
 */
typedef struct olc_committed_change {
    char            *field_path;
    olc_field_type_t field_type;
    json_t          *old_value;
    json_t          *new_value;
} olc_committed_change_t;

/**
 * A single commit record — an archived changeset.
 */
typedef struct olc_commit_record {
    int              id;             /* Sequential per-entity, starts at 1 */
    int              group_id;       /* Shared across entities in group commit; 0 = solo */
    char            *author;
    char            *comment;        /* Optional commit message */
    time_t           timestamp;
    LLIST           *changes;        /* List of olc_committed_change_t* */
} olc_commit_record_t;

/**
 * Per-entity commit history.
 */
typedef struct olc_commit_history {
    int              editor_type;    /* ED_* constant */
    WNUM_LOAD        entity_wnum;
    LLIST           *records;        /* List of olc_commit_record_t*, newest first */
    int              next_id;        /* Next sequential ID to assign */
    int              max_records;    /* From game setting, cached at creation */
    bool             is_dirty;       /* Needs disk save */
} olc_commit_history_t;

/* --- Global state --- */
extern int olc_next_group_id;        /* Global group ID counter */

/* --- Committed change lifecycle --- */
olc_committed_change_t *olc_committed_change_create(const char *field_path,
    olc_field_type_t type, json_t *old_value, json_t *new_value);
void olc_committed_change_destroy(olc_committed_change_t *change);

/* --- Commit record lifecycle --- */
olc_commit_record_t *olc_commit_record_create(const char *author, int group_id);
void olc_commit_record_destroy(olc_commit_record_t *record);

/* --- History lifecycle --- */
olc_commit_history_t *olc_commit_history_create(int editor_type, WNUM_LOAD wnum);
void olc_commit_history_destroy(olc_commit_history_t *history);

/* --- History operations --- */

/** Archive a changeset into a new commit record. Returns the new record, or NULL on error. */
olc_commit_record_t *olc_commit_history_archive(olc_commit_history_t *history,
    olc_changeset_t *cs, int group_id, const char *comment);

/** Find a record by ID. Returns NULL if not found. */
olc_commit_record_t *olc_commit_history_find(olc_commit_history_t *history, int id);

/** Get the number of records. */
int olc_commit_history_count(olc_commit_history_t *history);

/** Get the effective max records limit (from game settings or default). */
int olc_commit_history_get_limit(void);

/* --- Serialization --- */
json_t *olc_commit_history_serialize(olc_commit_history_t *history);
olc_commit_history_t *olc_commit_history_deserialize(json_t *json);
json_t *olc_commit_record_serialize(olc_commit_record_t *record);

/* --- Persistence (disk + Redis cache) --- */

/** Load history for an entity. Checks Redis first, then disk. Returns NULL if none. */
olc_commit_history_t *olc_commit_history_load(int editor_type, WNUM_LOAD wnum);

/** Save history to disk and update Redis cache. Returns true on success. */
bool olc_commit_history_save(olc_commit_history_t *history);

/** Delete history from disk and Redis. */
void olc_commit_history_delete(int editor_type, WNUM_LOAD wnum);

/** Save all dirty histories. Called on area save / shutdown. */
void olc_commit_history_save_all_dirty(void);

/** Get or load history for an entity (lazy load). */
olc_commit_history_t *olc_commit_history_get_or_load(int editor_type, WNUM_LOAD wnum);

#endif /* __OLC_COMMIT_HISTORY_H__ */
