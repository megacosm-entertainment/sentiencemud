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

/** Returns raw JSON value if pending, otherwise NULL. */
json_t *olc_staged_json(olc_changeset_t *cs, const char *field);

/** Check if a field has a pending change. */
bool olc_is_field_staged(olc_changeset_t *cs, const char *field);

/* =========================================================================
 * Display Formatting
 * ========================================================================= */

/** Pending marker prefix: "{Y*{x " if field is staged, "" if not. */
const char *olc_staged_marker(olc_changeset_t *cs, const char *field);

#endif /* !def __OLC_STAGED_H__ */
