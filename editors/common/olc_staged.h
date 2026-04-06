/**
 * @file olc_staged.h
 * @brief Preview helpers and staging utilities for OLC changeset mode.
 *
 * Provides functions to read the "effective" value of a field:
 * the staged value if a pending change exists, otherwise the live value.
 */

#ifndef __OLC_STAGED_H__
#define __OLC_STAGED_H__

#include "olc_changeset.h"

/* =========================================================================
 * Preview Helpers
 * ========================================================================= */

/** Returns staged string if pending, otherwise live value. */
const char *olc_staged_string(olc_changeset_t *cs, const char *field, const char *live);

/** Returns staged int if pending, otherwise live value. */
int olc_staged_int(olc_changeset_t *cs, const char *field, int live);

/** Returns staged flags if pending, otherwise live value. */
long olc_staged_flags(olc_changeset_t *cs, const char *field, long live);

/** Returns staged bool if pending, otherwise live value. */
bool olc_staged_bool(olc_changeset_t *cs, const char *field, bool live);

/** Returns staged long if pending, otherwise live value. */
long olc_staged_long(olc_changeset_t *cs, const char *field, long live);

/** Returns raw JSON value if pending, otherwise NULL. */
json_t *olc_staged_json(olc_changeset_t *cs, const char *field);

/** Check if a field has a pending change. */
bool olc_is_field_staged(olc_changeset_t *cs, const char *field);

/* =========================================================================
 * Display Formatting
 * ========================================================================= */

/** Pending marker prefix: "{Y*{x " if field is staged, "" if not. */
const char *olc_staged_marker(olc_changeset_t *cs, const char *field);

/* =========================================================================
 * Active Changeset Lookup
 * ========================================================================= */

struct olc_editor_def;  /* forward declaration to avoid circular include */

/**
 * Get the active changeset for the current editor session.
 * Returns NULL if not in staged mode or no changeset found.
 */
olc_changeset_t *olc_get_active_changeset(CHAR_DATA *ch,
    const struct olc_editor_def *def);

/**
 * Check staging safety limits. Returns true if OK, false if limit hit
 * (and sends error message to ch).
 */
bool olc_check_staging_limits(CHAR_DATA *ch, olc_changeset_t *cs);

/* =========================================================================
 * Staged Editor Commands
 * ========================================================================= */

/** Handle 'commit [comment]' — apply all pending, save entity, record history. */
void olc_staged_cmd_commit(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit, char *argument);

/** Handle 'commit group [comment]' — commit all open staged editors atomically. */
void olc_staged_cmd_commit_group(CHAR_DATA *ch, char *argument);

/** Handle 'revert [field]' — discard pending changes. */
void olc_staged_cmd_revert(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit, char *argument);

/** Handle 'pending' — show table of pending changes. */
void olc_staged_cmd_pending(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit);

/** Handle 'savedraft' — save current changeset to disk. */
void olc_staged_cmd_savedraft(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit);

/** Handle 'loaddraft' — load a saved draft from disk. */
void olc_staged_cmd_loaddraft(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit);

/** Handle 'discarddraft' — remove a saved draft from disk. */
void olc_staged_cmd_discarddraft(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit);

/** Handle 'history [id|revert <id> [confirm]]' — view or revert past commits. */
void olc_staged_cmd_history(CHAR_DATA *ch, const struct olc_editor_def *def,
    void *pEdit, char *argument);

/* Embedded struct snapshot helpers */
json_t *olc_staged_embedded(olc_changeset_t *cs, const char *struct_name);
bool    olc_staged_embedded_set(olc_changeset_t *cs, const char *struct_name,
                                 const char *key, json_t *value);

/* Flag value helper */
long olc_staged_flags_or(olc_changeset_t *cs, const char *field, long live_value);

#endif /* !def __OLC_STAGED_H__ */
