/**
 * @file gmcp_editor.h
 * @brief GMCP Editor Protocol — Sentience.Editor.* messages.
 *
 * Handles bidirectional GMCP communication for OLC editors.
 * Server→client: Open, Field, State, Close, Error, CommitResult
 * Client→server: Set, Commit, Revert, Request, StringEdit.Save/Cancel, Draft.*
 *
 * @see docs/superpowers/specs/2026-03-26-olc-changeset-gmcp-design.md
 */

#ifndef __GMCP_EDITOR_H__
#define __GMCP_EDITOR_H__

#include "merc.h"
#include "editors/common/olc_changeset.h"
#include <jansson.h>

/* Forward declarations */
typedef struct olc_editor_def OLC_EDITOR_DEF;

/* =========================================================================
 * Outgoing Message Builders (Server → Client)
 *
 * Each builder returns a new json_t* that the caller must json_decref().
 * The send helpers handle decref via sentience_send_package().
 * ========================================================================= */

/** Build Sentience.Editor.Field JSON. */
json_t *gmcp_editor_build_field(const char *entity_id, const char *field,
    json_t *value, const char *type_str, bool is_pending);

/** Build Sentience.Editor.State JSON from a changeset. */
json_t *gmcp_editor_build_state(const char *entity_id,
    olc_changeset_t *cs, bool draft_restored);

/** Build Sentience.Editor.Close JSON. reason: "done", "forced", "disconnect" */
json_t *gmcp_editor_build_close(const char *entity_id, const char *reason);

/** Build Sentience.Editor.Error JSON. */
json_t *gmcp_editor_build_error(const char *entity_id, const char *field,
    const char *error_code, const char *message);

/** Build Sentience.Editor.CommitResult JSON (single entity). */
json_t *gmcp_editor_build_commit_result(const char *entity_id,
    const char *status, int changes_applied);

/** Build Sentience.Editor.CommitResult JSON (group commit). */
json_t *gmcp_editor_build_group_commit_result(int group_id,
    json_t *results_array, const char *comment);

/** Map ED_* constant to lowercase string for editor_type field. */
const char *gmcp_editor_type_name(int editor_type, const char *fallback_name);

/** Build Sentience.Editor.Open JSON with full schema and state. */
json_t *gmcp_editor_build_open(const char *entity_id, const char *editor_name,
    const char *editor_type, json_t *tabs, olc_changeset_t *cs,
    bool draft_restored);

/* =========================================================================
 * Send Helpers (combines build + sentience_send_package)
 * ========================================================================= */

/** Send Editor.Field to descriptor. */
void gmcp_editor_send_field(descriptor_t *d, const char *entity_id,
    const char *field, json_t *value, const char *type_str, bool is_pending);

/** Send Editor.State to descriptor. */
void gmcp_editor_send_state(descriptor_t *d, const char *entity_id,
    olc_changeset_t *cs, bool draft_restored);

/** Send Editor.Close to descriptor. */
void gmcp_editor_send_close(descriptor_t *d, const char *entity_id,
    const char *reason);

/** Send Editor.Error to descriptor. */
void gmcp_editor_send_error(descriptor_t *d, const char *entity_id,
    const char *field, const char *error_code, const char *message);

/** Send Editor.CommitResult to descriptor. */
void gmcp_editor_send_commit_result(descriptor_t *d, const char *entity_id,
    const char *status, int changes_applied);

/** Send Editor.Open to descriptor with full schema capture. */
void gmcp_editor_send_open(descriptor_t *d, const OLC_EDITOR_DEF *def,
    void *entity, const char *entity_id, olc_changeset_t *cs,
    bool draft_restored);

/* =========================================================================
 * StringEdit Messages (Server → Client)
 * ========================================================================= */

/** Build Sentience.Editor.StringEdit.Open JSON. */
json_t *gmcp_editor_build_string_open(const char *entity_id, const char *field,
    const char *current_value, int max_length, int session_id);

/** Build Sentience.Editor.StringEdit.Close JSON. */
json_t *gmcp_editor_build_string_close(int session_id, const char *status);

/** Send Editor.StringEdit.Open to descriptor. */
void gmcp_editor_send_string_open(descriptor_t *d, const char *entity_id,
    const char *field, const char *current_value, int max_length, int session_id);

/** Send Editor.StringEdit.Close to descriptor. */
void gmcp_editor_send_string_close(descriptor_t *d, int session_id,
    const char *status);

/* =========================================================================
 * Incoming Message Handler (Client → Server)
 * ========================================================================= */

/**
 * Dispatch incoming Sentience.Editor.* GMCP message.
 * Called from ParseGMCP() with the raw JSON payload string.
 */
void sentience_handle_editor(descriptor_t *d, int module, const char *json_str);

/* =========================================================================
 * Entity ID Formatting
 * ========================================================================= */

/** Format entity ID string: "type:auid#vnum" or "area:auid". Returns static buffer. */
const char *gmcp_editor_entity_id(int editor_type, WNUM_LOAD wnum);

/** Parse entity ID string back to editor_type and wnum. Returns true on success. */
bool gmcp_editor_parse_entity_id(const char *entity_id,
    int *editor_type, WNUM_LOAD *wnum);

#endif /* __GMCP_EDITOR_H__ */
