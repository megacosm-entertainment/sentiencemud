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
 * Incoming Message Handler (stub — full implementation in Task 9)
 * ========================================================================= */

void sentience_handle_editor(descriptor_t *d, int module, const char *json_str)
{
    /* Incoming GMCP editor messages will be implemented in Task 9.
     * For now, just validate basic prerequisites. */
    if (!d || !d->character || IS_NPC(d->character))
        return;

    (void)module;
    (void)json_str;
}
