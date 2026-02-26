/**
 * @file olc_commands.h
 * @brief Common OLC Command Helpers
 *
 * Provides reusable helper functions for the most common OLC command patterns:
 * setting strings, numbers, flags, types, and booleans.  Each helper handles
 * argument validation, value assignment, user feedback, and optional change
 * history recording — reducing per-command boilerplate from ~15 lines to 1-3.
 *
 * History recording uses a generic callback so the helpers stay independent
 * of any particular editor's entity type.
 *
 * Every helper accepts both a `label` and a `syntax` parameter:
 *   - `label`  — used for feedback messages ("Name set.") and history field names.
 *   - `syntax` — if non-NULL, displayed verbatim on bad input instead of the
 *                auto-generated syntax line.  Pass NULL for reasonable defaults.
 *
 * @see editors/common/olc_editor.h   for the editor framework
 * @see editors/common/olc_display.h  for display/rendering helpers
 */

#ifndef __OLC_COMMANDS_H__
#define __OLC_COMMANDS_H__

#include "../../merc.h"
#include "../../tables.h"

/* =========================================================================
 * History Recording Callback
 * ========================================================================= */

/**
 * Generic history-recording callback for olc_cmd_* helpers.
 *
 * Each editor supplies a thin wrapper that forwards to its specific
 * record function (e.g., skedit_record, racedit_record) and handles
 * dirty-marking for JSON persistence.
 *
 * @param ctx       Editor-specific entity pointer (e.g., SKILL_DATA *)
 * @param ch        Character who made the change
 * @param field     Field name (e.g., "name", "level")
 * @param old_val   Previous value formatted as a display string
 * @param new_val   New value formatted as a display string
 *
 * Example wrapper in skedit.c:
 * @code
 * static void skedit_record_cb(void *ctx, CHAR_DATA *ch,
 *     const char *field, const char *old_val, const char *new_val) {
 *     skedit_record((SKILL_DATA *)ctx, ch, field, old_val, new_val);
 * }
 * @endcode
 */
typedef void (*olc_cmd_record_fn)(void *ctx, CHAR_DATA *ch,
    const char *field, const char *old_val, const char *new_val);

/* =========================================================================
 * String Command Flags
 * ========================================================================= */

/** Default behavior: set only, no clearing. */
#define OLC_STR_DEFAULT         0x00

/** Allow the user to type "clear" to empty the field. */
#define OLC_STR_CLEARABLE       0x01

/** On clear, set field to NULL instead of &str_empty[0]. */
#define OLC_STR_CLEAR_NULL      0x02

/* =========================================================================
 * Common Command Helpers
 * ========================================================================= */

/**
 * Set a string field.
 *
 * Validates input, calls free_string()/str_dup(), records history, and
 * sends feedback.  Supports an optional "clear" sub-command when
 * OLC_STR_CLEARABLE is set.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text
 * @param label       Field name for feedback/history (e.g., "name")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the char* field to modify
 * @param str_flags   OLC_STR_* flags (0 for simple set)
 * @param ctx         Entity pointer forwarded to record_fn (may be NULL)
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if the field was changed
 */
bool olc_cmd_string(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, char **field_ptr, int str_flags,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Open the multi-line string editor for a field.
 *
 * If @p argument is empty, opens string_append().  Records "(edited)"
 * in history when a recording callback is provided.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text (must be empty to open editor)
 * @param label       Field name for feedback/history (e.g., "description")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the char* field to edit
 * @param ctx         Entity pointer forwarded to record_fn
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if the editor was opened
 */
bool olc_cmd_string_append(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, char **field_ptr,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Set an integer field with range validation.
 *
 * Validates that @p argument is numeric and within [min_val, max_val],
 * assigns the value, records history, and sends feedback.
 *
 * Use INT_MIN / INT_MAX for open-ended ranges.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text
 * @param label       Field name for feedback/history (e.g., "level")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the int field to modify
 * @param min_val     Minimum acceptable value (inclusive)
 * @param max_val     Maximum acceptable value (inclusive)
 * @param ctx         Entity pointer forwarded to record_fn
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if the field was changed
 */
bool olc_cmd_number(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int *field_ptr, int min_val, int max_val,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Set a 16-bit integer field with range validation.
 * Same as olc_cmd_number() but for int16_t fields.
 */
bool olc_cmd_number_i16(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int16_t *field_ptr, int min_val, int max_val,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Toggle bitwise flags on a long field.
 *
 * Uses flag_value() to parse the argument, then XORs the result into
 * the field.  Records old/new flag strings in history.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text (space-separated flag names)
 * @param label       Field name for feedback/history (e.g., "flags")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the long field to modify
 * @param flag_table  Flag table for parsing and display
 * @param ctx         Entity pointer forwarded to record_fn
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if flags were toggled
 */
bool olc_cmd_flag_toggle(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Set a single-value type from a flag/stat table.
 *
 * Uses flag_value() to look up the argument, then assigns it directly
 * (no XOR).  Records old/new type names in history.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text (type name)
 * @param label       Field name for feedback/history (e.g., "type")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the int field to modify
 * @param flag_table  Stat/flag table for lookup and display
 * @param ctx         Entity pointer forwarded to record_fn
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if the type was changed
 */
bool olc_cmd_type_set(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Set a 16-bit type from a flag/stat table.
 * Same as olc_cmd_type_set() but for int16_t fields.
 */
bool olc_cmd_type_set_i16(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int16_t *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn);

/**
 * Set or toggle a boolean field.
 *
 * Behavior:
 *   - Empty argument → toggle the current value
 *   - "yes", "true", "on"   → set to true
 *   - "no",  "false", "off" → set to false
 *   - Anything else → show syntax error
 *
 * Records "Yes"/"No" in history.
 *
 * @param ch          Character issuing the command
 * @param argument    Raw argument text
 * @param label       Field name for feedback/history (e.g., "playable")
 * @param syntax      Custom syntax help (NULL = auto-generate)
 * @param field_ptr   Pointer to the bool field to modify
 * @param ctx         Entity pointer forwarded to record_fn
 * @param record_fn   History callback (NULL to skip recording)
 * @return true if the field was changed
 */
bool olc_cmd_bool(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, bool *field_ptr,
    void *ctx, olc_cmd_record_fn record_fn);

#endif /* !def __OLC_COMMANDS_H__ */
