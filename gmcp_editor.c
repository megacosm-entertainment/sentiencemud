/**
 * @file gmcp_editor.c
 * @brief GMCP Editor Protocol — Sentience.Editor.* message builders and handlers.
 *
 * Implements outgoing message builders, send helpers, entity ID formatting,
 * and incoming message dispatch for the OLC editor GMCP protocol.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "merc.h"
#include "gmcp_editor.h"
#include "gmcp_sentience.h"
#include "protocol.h"
#include "editors/common/olc_changeset.h"
#include "editors/common/olc_editor.h"
#include "editors/common/olc_staged.h"

/* =========================================================================
 * Entity ID Formatting
 * ========================================================================= */

const char *gmcp_editor_entity_id(int editor_type, WNUM_LOAD wnum)
{
    static char buf[64];
    const char *type_name;

    switch (editor_type) {
        case ED_ROOM:   type_name = "room"; break;
        case ED_MOBILE: type_name = "mob";  break;
        case ED_OBJECT: type_name = "obj";  break;
        case ED_AREA:   type_name = "area"; break;
        default:        type_name = "unknown"; break;
    }

    if (editor_type == ED_AREA)
        snprintf(buf, sizeof(buf), "%s:%ld", type_name, wnum.auid);
    else
        snprintf(buf, sizeof(buf), "%s:%ld#%ld", type_name, wnum.auid, wnum.vnum);

    return buf;
}

bool gmcp_editor_parse_entity_id(const char *entity_id,
    int *editor_type, WNUM_LOAD *wnum)
{
    if (!entity_id || !editor_type || !wnum) return false;

    char type_buf[16];
    long auid = 0, vnum = 0;

    /* Try "type:auid#vnum" format first */
    if (sscanf(entity_id, "%15[^:]:%ld#%ld", type_buf, &auid, &vnum) == 3) {
        /* got all three */
    } else if (sscanf(entity_id, "%15[^:]:%ld", type_buf, &auid) == 2) {
        vnum = 0;
    } else {
        return false;
    }

    if (!str_cmp(type_buf, "room"))      *editor_type = ED_ROOM;
    else if (!str_cmp(type_buf, "mob"))   *editor_type = ED_MOBILE;
    else if (!str_cmp(type_buf, "obj"))   *editor_type = ED_OBJECT;
    else if (!str_cmp(type_buf, "area"))  *editor_type = ED_AREA;
    else return false;

    wnum->auid = auid;
    wnum->vnum = vnum;
    return true;
}

/* =========================================================================
 * Outgoing Message Builders
 * ========================================================================= */

json_t *gmcp_editor_build_field(const char *entity_id, const char *field,
    json_t *value, const char *type_str, bool is_pending)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));
    json_object_set_new(msg, "field", json_string(field ? field : ""));
    if (value)
        json_object_set(msg, "value", value);  /* borrowed ref — caller owns value */
    else
        json_object_set_new(msg, "value", json_null());
    json_object_set_new(msg, "type", json_string(type_str ? type_str : "string"));
    json_object_set_new(msg, "is_pending", json_boolean(is_pending));
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

json_t *gmcp_editor_build_state(const char *entity_id,
    olc_changeset_t *cs, bool draft_restored)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));

    int count = cs ? olc_changeset_count(cs) : 0;
    json_object_set_new(msg, "pending_count", json_integer(count));
    json_object_set_new(msg, "draft_restored", json_boolean(draft_restored));

    json_t *changes = json_array();
    if (cs && cs->changes) {
        ITERATOR it;
        iterator_start(&it, cs->changes);
        olc_pending_change_t *change;
        while ((change = (olc_pending_change_t *)iterator_nextdata(&it)) != NULL) {
            json_t *entry = json_object();
            json_object_set_new(entry, "field",
                json_string(change->field_path ? change->field_path : ""));

            const char *type_name;
            switch (change->field_type) {
                case OLC_FIELD_STRING:      type_name = "string"; break;
                case OLC_FIELD_INT:         type_name = "int"; break;
                case OLC_FIELD_INT16:       type_name = "int16"; break;
                case OLC_FIELD_FLAGS:       type_name = "flags"; break;
                case OLC_FIELD_BOOL:        type_name = "bool"; break;
                case OLC_FIELD_WIDEVNUM:    type_name = "widevnum"; break;
                case OLC_FIELD_EXIT:        type_name = "exit"; break;
                case OLC_FIELD_EMBEDDED:    type_name = "embedded"; break;
                case OLC_FIELD_LIST_ADD:    type_name = "list_add"; break;
                case OLC_FIELD_LIST_REMOVE: type_name = "list_remove"; break;
                case OLC_FIELD_LIST_UPDATE: type_name = "list_update"; break;
                case OLC_FIELD_MULTILINE:   type_name = "multiline"; break;
                case OLC_FIELD_TYPE_DATA:   type_name = "type_data"; break;
                default:                    type_name = "unknown"; break;
            }
            json_object_set_new(entry, "type", json_string(type_name));

            if (change->new_value)
                json_object_set(entry, "value", change->new_value);
            else
                json_object_set_new(entry, "value", json_null());

            json_array_append_new(changes, entry);
        }
        iterator_stop(&it);
    }
    json_object_set_new(msg, "changes", changes);
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

json_t *gmcp_editor_build_close(const char *entity_id, const char *reason)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));
    json_object_set_new(msg, "reason", json_string(reason ? reason : "done"));
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

json_t *gmcp_editor_build_error(const char *entity_id, const char *field,
    const char *error_code, const char *message)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));
    if (field)
        json_object_set_new(msg, "field", json_string(field));
    json_object_set_new(msg, "error", json_string(error_code ? error_code : "unknown"));
    json_object_set_new(msg, "message", json_string(message ? message : ""));
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

json_t *gmcp_editor_build_commit_result(const char *entity_id,
    const char *status, int changes_applied)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));
    json_object_set_new(msg, "status", json_string(status ? status : "error"));
    json_object_set_new(msg, "changes_applied", json_integer(changes_applied));
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

json_t *gmcp_editor_build_group_commit_result(int group_id,
    json_t *results_array, const char *comment)
{
    json_t *msg = json_object();
    if (!msg) return NULL;

    json_object_set_new(msg, "group_id", json_integer(group_id));
    if (results_array)
        json_object_set(msg, "results", results_array);
    else
        json_object_set_new(msg, "results", json_array());
    if (comment)
        json_object_set_new(msg, "comment", json_string(comment));
    json_object_set_new(msg, "_v", json_integer(1));

    return msg;
}

/* =========================================================================
 * Send Helpers
 * ========================================================================= */

static bool can_send_gmcp(descriptor_t *d)
{
    return d && d->pProtocol && d->pProtocol->bGMCP;
}

void gmcp_editor_send_field(descriptor_t *d, const char *entity_id,
    const char *field, json_t *value, const char *type_str, bool is_pending)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_field(entity_id, field, value, type_str, is_pending);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.Field", msg);
}

void gmcp_editor_send_state(descriptor_t *d, const char *entity_id,
    olc_changeset_t *cs, bool draft_restored)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_state(entity_id, cs, draft_restored);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.State", msg);
}

void gmcp_editor_send_close(descriptor_t *d, const char *entity_id,
    const char *reason)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_close(entity_id, reason);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.Close", msg);
}

void gmcp_editor_send_error(descriptor_t *d, const char *entity_id,
    const char *field, const char *error_code, const char *message)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_error(entity_id, field, error_code, message);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.Error", msg);
}

void gmcp_editor_send_commit_result(descriptor_t *d, const char *entity_id,
    const char *status, int changes_applied)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_commit_result(entity_id, status, changes_applied);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.CommitResult", msg);
}

/* =========================================================================
 * StringEdit Message Builders
 * ========================================================================= */

json_t *gmcp_editor_build_string_open(const char *entity_id, const char *field,
    const char *current_value, int max_length, int session_id)
{
    char session_str[32];
    snprintf(session_str, sizeof(session_str), "se_%d", session_id);

    json_t *msg = json_object();
    json_object_set_new(msg, "session_id", json_string(session_str));
    json_object_set_new(msg, "entity_id", json_string(entity_id ? entity_id : ""));
    json_object_set_new(msg, "field", json_string(field ? field : ""));
    json_object_set_new(msg, "value", json_string(current_value ? current_value : ""));
    json_object_set_new(msg, "max_length", json_integer(max_length));
    return msg;
}

json_t *gmcp_editor_build_string_close(int session_id, const char *status)
{
    char session_str[32];
    snprintf(session_str, sizeof(session_str), "se_%d", session_id);

    json_t *msg = json_object();
    json_object_set_new(msg, "session_id", json_string(session_str));
    json_object_set_new(msg, "status", json_string(status ? status : "done"));
    return msg;
}

void gmcp_editor_send_string_open(descriptor_t *d, const char *entity_id,
    const char *field, const char *current_value, int max_length, int session_id)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_string_open(entity_id, field,
        current_value, max_length, session_id);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.StringEdit.Open", msg);
}

void gmcp_editor_send_string_close(descriptor_t *d, int session_id,
    const char *status)
{
    if (!can_send_gmcp(d)) return;
    json_t *msg = gmcp_editor_build_string_close(session_id, status);
    if (msg)
        sentience_send_package(d, "Sentience.Editor.StringEdit.Close", msg);
}

/* =========================================================================
 * Incoming Message Handlers
 * ========================================================================= */

/**
 * Validate a GMCP editor request: entity must match current editor session.
 * Returns the active changeset, or NULL (with error sent to client).
 */
static olc_changeset_t *validate_editor_request(descriptor_t *d,
    const char *entity_id, const char *field)
{
    if (!entity_id) {
        gmcp_editor_send_error(d, "", field ? field : "",
            "invalid_request", "Missing entity_id.");
        return NULL;
    }

    int editor_type;
    WNUM_LOAD wnum;
    if (!gmcp_editor_parse_entity_id(entity_id, &editor_type, &wnum)) {
        gmcp_editor_send_error(d, entity_id, field ? field : "",
            "invalid_entity", "Invalid entity ID format.");
        return NULL;
    }

    if (!d->olc_state) {
        gmcp_editor_send_error(d, entity_id, field ? field : "",
            "not_editing", "No editor session active.");
        return NULL;
    }

    olc_changeset_t *cs = olc_edit_state_find_changeset(
        d->olc_state, editor_type, wnum);
    if (!cs) {
        gmcp_editor_send_error(d, entity_id, field ? field : "",
            "not_editing", "Entity is not open for editing.");
        return NULL;
    }

    return cs;
}

/**
 * Handle Sentience.Editor.Set — client sets a field value.
 *
 * Payload: { "entity_id": "room:5#3001", "field": "name", "value": "..." }
 */
static void handle_editor_set(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));
    const char *field = json_string_value(json_object_get(payload, "field"));
    json_t *value = json_object_get(payload, "value");

    if (!field || !value) {
        gmcp_editor_send_error(d, entity_id ? entity_id : "",
            field ? field : "", "invalid_request", "Missing required fields.");
        return;
    }

    olc_changeset_t *cs = validate_editor_request(d, entity_id, field);
    if (!cs) return;

    CHAR_DATA *ch = d->character;
    if (!olc_check_staging_limits(ch, cs)) {
        gmcp_editor_send_error(d, entity_id, field,
            "limit_reached", "Too many pending changes.");
        return;
    }

    /* Determine field type from JSON value type */
    olc_field_type_t ftype = OLC_FIELD_STRING;
    if (json_is_integer(value))      ftype = OLC_FIELD_INT;
    else if (json_is_boolean(value)) ftype = OLC_FIELD_BOOL;

    /* Get current live value for old_value if this is the first edit */
    json_t *old_value = NULL;
    olc_pending_change_t *existing = olc_changeset_find_change(cs, field);
    if (!existing) {
        const olc_field_handler_t *handler = olc_find_field_handler(
            olc_find_editor_by_type(cs->editor_type)->field_handlers,
            field, ftype);
        if (handler && handler->serialize_fn) {
            old_value = handler->serialize_fn(d->pEdit, field);
        }
    }

    /* Stage the change (collapsing logic handles duplicates/no-ops) */
    olc_pending_change_t *result = olc_changeset_add_change(
        cs, field, ftype, old_value, value);
    if (old_value) json_decref(old_value);

    if (result) {
        /* Change staged — send confirmation with pending=true */
        gmcp_editor_send_field(d, entity_id, field, value, "string", true);
    } else {
        /* Collapsed to no-op (reverted to original) — send current live value */
        gmcp_editor_send_field(d, entity_id, field, json_null(), "string", false);
    }
}

/**
 * Handle Sentience.Editor.Commit — client requests commit of pending changes.
 *
 * Payload: { "entity_id": "room:5#3001", "comment": "optional" }
 */
static void handle_editor_commit(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));

    olc_changeset_t *cs = validate_editor_request(d, entity_id, NULL);
    if (!cs) return;

    if (olc_changeset_count(cs) == 0) {
        gmcp_editor_send_error(d, entity_id, NULL,
            "no_changes", "No pending changes to commit.");
        return;
    }

    const OLC_EDITOR_DEF *def = olc_find_editor_by_type(cs->editor_type);
    if (!def) {
        gmcp_editor_send_error(d, entity_id, NULL,
            "internal_error", "Editor definition not found.");
        return;
    }

    const char *error_field = NULL;
    int applied = olc_changeset_commit(cs, d->pEdit,
        def->field_handlers, &error_field);

    if (applied < 0) {
        gmcp_editor_send_error(d, entity_id, error_field,
            "commit_failed", "Error applying changes.");
        return;
    }

    /* Mark entity area as changed */
    if (def->get_area_fn && d->pEdit) {
        AREA_DATA *area = def->get_area_fn(d->pEdit);
        if (area)
            SET_BIT(area->area_flags, AREA_CHANGED);
    }

    if (def->editor_type == ED_ROOM
        || def->editor_type == ED_MOBILE
        || def->editor_type == ED_OBJECT) {
        fix_index_inheritance();
    }

    gmcp_editor_send_commit_result(d, entity_id, "success", applied);
}

/**
 * Handle Sentience.Editor.Revert — client requests revert.
 *
 * Payload: { "entity_id": "room:5#3001", "field": "name" }  (field is optional)
 */
static void handle_editor_revert(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));
    const char *field = json_string_value(json_object_get(payload, "field"));

    olc_changeset_t *cs = validate_editor_request(d, entity_id, field);
    if (!cs) return;

    if (field) {
        if (!olc_changeset_revert_field(cs, field)) {
            gmcp_editor_send_error(d, entity_id, field,
                "not_found", "No pending change for this field.");
            return;
        }
    } else {
        if (olc_changeset_count(cs) == 0) {
            gmcp_editor_send_error(d, entity_id, NULL,
                "no_changes", "No pending changes to revert.");
            return;
        }
        olc_changeset_revert(cs);
    }

    /* Send updated state */
    gmcp_editor_send_state(d, entity_id, cs, false);
}

/**
 * Handle Sentience.Editor.Request — client requests current state.
 *
 * Payload: { "entity_id": "room:5#3001" }
 */
static void handle_editor_request(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));

    olc_changeset_t *cs = validate_editor_request(d, entity_id, NULL);
    if (!cs) return;

    gmcp_editor_send_state(d, entity_id, cs, false);
}

/**
 * Parse session_id string "se_N" to integer N.
 * Returns -1 on invalid format.
 */
static int parse_session_id(const char *session_str)
{
    if (!session_str || strncmp(session_str, "se_", 3) != 0)
        return -1;
    char *endp;
    long val = strtol(session_str + 3, &endp, 10);
    if (*endp != '\0' || val < 0)
        return -1;
    return (int)val;
}

/**
 * Handle Sentience.Editor.StringEdit.Save — client submits edited text.
 *
 * Payload: { "session_id": "se_1", "value": "new text content" }
 */
static void handle_editor_string_save(descriptor_t *d, json_t *payload)
{
    const char *session_str = json_string_value(json_object_get(payload, "session_id"));
    const char *value = json_string_value(json_object_get(payload, "value"));

    int session_id = parse_session_id(session_str);
    if (session_id < 0 || !value) {
        gmcp_editor_send_error(d, "", NULL,
            "invalid_request", "Missing session_id or value.");
        return;
    }

    if (!d->olc_state) {
        gmcp_editor_send_error(d, "", NULL,
            "no_session", "No active editing state.");
        return;
    }

    olc_string_edit_session_t *session = olc_string_session_find(d->olc_state, session_id);
    if (!session) {
        gmcp_editor_send_error(d, "", NULL,
            "invalid_session", "String edit session not found.");
        return;
    }

    if (session->changeset) {
        /* Staged mode: store in changeset overlay */
        json_t *old_val = json_string(
            session->field_ptr && *session->field_ptr ? *session->field_ptr : "");
        json_t *new_val = json_string(value);
        olc_changeset_add_change(session->changeset, session->field_path,
            OLC_FIELD_MULTILINE, old_val, new_val);
        json_decref(old_val);
        json_decref(new_val);
    } else if (session->field_ptr) {
        /* Direct mode: write immediately */
        free_string(*session->field_ptr);
        *session->field_ptr = str_dup(value);
    }

    gmcp_editor_send_string_close(d, session_id, "saved");
    olc_string_session_remove(d->olc_state, session_id);
}

/**
 * Handle Sentience.Editor.StringEdit.Cancel — client cancels editing.
 *
 * Payload: { "session_id": "se_1" }
 */
static void handle_editor_string_cancel(descriptor_t *d, json_t *payload)
{
    const char *session_str = json_string_value(json_object_get(payload, "session_id"));

    int session_id = parse_session_id(session_str);
    if (session_id < 0) {
        gmcp_editor_send_error(d, "", NULL,
            "invalid_request", "Missing or invalid session_id.");
        return;
    }

    if (!d->olc_state) {
        gmcp_editor_send_error(d, "", NULL,
            "no_session", "No active editing state.");
        return;
    }

    olc_string_edit_session_t *session = olc_string_session_find(d->olc_state, session_id);
    if (!session) {
        gmcp_editor_send_error(d, "", NULL,
            "invalid_session", "String edit session not found.");
        return;
    }

    gmcp_editor_send_string_close(d, session_id, "cancelled");
    olc_string_session_remove(d->olc_state, session_id);
}

/**
 * Handle Sentience.Editor.Draft.Save — client saves current changeset as draft.
 *
 * Payload: { "entity_id": "room:5#3001" }
 */
static void handle_editor_draft_save(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));

    olc_changeset_t *cs = validate_editor_request(d, entity_id, NULL);
    if (!cs) return;

    if (olc_changeset_count(cs) == 0) {
        gmcp_editor_send_error(d, entity_id, NULL,
            "no_changes", "No pending changes to save.");
        return;
    }

    if (olc_draft_save(cs))
        gmcp_editor_send_state(d, entity_id, cs, false);
    else
        gmcp_editor_send_error(d, entity_id, NULL,
            "save_failed", "Failed to save draft.");
}

/**
 * Handle Sentience.Editor.Draft.Load — client loads a saved draft.
 *
 * Payload: { "entity_id": "room:5#3001" }
 */
static void handle_editor_draft_load(descriptor_t *d, json_t *payload)
{
    const char *entity_id = json_string_value(json_object_get(payload, "entity_id"));

    olc_changeset_t *cs = validate_editor_request(d, entity_id, NULL);
    if (!cs) return;

    CHAR_DATA *ch = d->character;
    const OLC_EDITOR_DEF *def = olc_find_editor_by_type(d->editor);
    if (!def || !d->olc_state) return;

    WNUM_LOAD wnum = olc_get_entity_wnum(def, d->pEdit);

    if (!olc_draft_exists(ch->name, def->editor_type, wnum)) {
        gmcp_editor_send_error(d, entity_id, NULL,
            "no_draft", "No saved draft found.");
        return;
    }

    olc_changeset_t *loaded = olc_draft_load(ch->name, def->editor_type, wnum);
    if (!loaded) {
        gmcp_editor_send_error(d, entity_id, NULL,
            "load_failed", "Failed to load draft (may be corrupt).");
        return;
    }

    /* Replace current changeset with loaded one */
    list_remlink(d->olc_state->active_changesets, cs, false);
    olc_changeset_destroy(cs);
    list_addlink(d->olc_state->active_changesets, loaded);

    gmcp_editor_send_state(d, entity_id, loaded, true);
}

void sentience_handle_editor(descriptor_t *d, int module, const char *json_str)
{
    if (!d || !d->character || IS_NPC(d->character))
        return;

    json_error_t error;
    json_t *payload = json_loads(json_str, 0, &error);
    if (!payload) {
        gmcp_editor_send_error(d, "", NULL,
            "parse_error", "Invalid JSON payload.");
        return;
    }

    switch (module) {
        case GMCP_SENTIENCE_EDITOR_SET:
            handle_editor_set(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_COMMIT:
            handle_editor_commit(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_REVERT:
            handle_editor_revert(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_REQUEST:
            handle_editor_request(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_STRING_SAVE:
            handle_editor_string_save(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_STRING_CANCEL:
            handle_editor_string_cancel(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_DRAFT_SAVE:
            handle_editor_draft_save(d, payload);
            break;
        case GMCP_SENTIENCE_EDITOR_DRAFT_LOAD:
            handle_editor_draft_load(d, payload);
            break;
    }

    json_decref(payload);
}
