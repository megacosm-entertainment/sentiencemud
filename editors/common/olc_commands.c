/**
 * @file olc_commands.c
 * @brief Common OLC Command Helper Implementations
 *
 * See olc_commands.h for API documentation.
 */

#include "olc_commands.h"
#include "olc_editor.h"
#include "olc_staged.h"
#include "../common.h"
#include "../../sentience_link.h"
#include "../../gmcp_editor.h"

#include <limits.h>

/* =========================================================================
 * Internals
 * ========================================================================= */

/**
 * Helper: send GMCP Editor.Field notification after staging a change.
 * @param cs      Active changeset (for entity_id)
 * @param ch      Character making the change
 * @param label   Field path
 * @param value   New value (json_t, borrowed ref) or NULL for no-op revert
 * @param type_str Field type string for GMCP
 * @param staged  true if change was staged, false if collapsed to no-op
 */
static void notify_field_change(olc_changeset_t *cs, CHAR_DATA *ch,
    const char *label, json_t *value, const char *type_str, bool staged)
{
    if (!ch || !ch->desc || !cs) return;
    const char *eid = gmcp_editor_entity_id(cs->editor_type, cs->entity_wnum);
    gmcp_editor_send_field(ch->desc, eid, label,
        staged ? value : json_null(), type_str, staged);
}

/**
 * Helper: call the record callback if non-NULL.
 */
static inline void cmd_record(olc_cmd_record_fn record_fn, void *ctx,
    CHAR_DATA *ch, const char *field, const char *old_val, const char *new_val)
{
    if (record_fn)
        record_fn(ctx, ch, field, old_val, new_val);
}

/* =========================================================================
 * olc_cmd_string
 * ========================================================================= */

bool olc_cmd_string(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, char **field_ptr, int str_flags,
    void *ctx, olc_cmd_record_fn record_fn)
{
    if (IS_NULLSTR(argument)) {
        if (syntax) {
            send_to_char(syntax, ch);
        } else if (str_flags & OLC_STR_CLEARABLE) {
            if (str_flags & OLC_STR_UTF8_RESTRICT) {
                send_to_char(formatf("Syntax: %s <text**>\n\r"
                                     "        %s clear\n\r"
                                     "** - <text> must conform to naming restrictions.\n\r", label, label), ch);
            } else {
                send_to_char(formatf("Syntax: %s <text>\n\r"
                                     "        %s clear\n\r", label, label), ch);
            }
        } else if (str_flags & OLC_STR_UTF8_RESTRICT) {
            send_to_char(formatf("Syntax: %s <text**>\n\r"
                                 "** - <text> must conform to naming restrictions.\n\r", label), ch);
        } else {
            send_to_char(formatf("Syntax: %s <text>\n\r", label), ch);
        }
        return false;
    }

    /* Handle "clear" */
    if ((str_flags & OLC_STR_CLEARABLE) && !str_cmp(argument, "clear")) {
        /* Staged mode: record clear as setting to empty string */
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_string(*field_ptr ? *field_ptr : "");
                json_t *new_val = json_string("");
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_STRING, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s cleared.\n\r", label);
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_string(""), "string", result != NULL);
                return result != NULL;
            }
        }
        cmd_record(record_fn, ctx, ch, label, *field_ptr, "(cleared)");
        free_string(*field_ptr);
        *field_ptr = (str_flags & OLC_STR_CLEAR_NULL) ? NULL : &str_empty[0];
        send_to_char(formatf("%s cleared.\n\r", label), ch);
        return true;
    }

    if ((str_flags & OLC_STR_UTF8_RESTRICT)) {

        // Validate the input can be a name
        if (!olc_validate_name(ch, argument))
            return false;

        // Name has been validated according to game settings.
    }

    /* Staged mode: store change in overlay instead of writing directly */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_string(*field_ptr ? *field_ptr : "");
                json_t *new_val = json_string(argument);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_STRING, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set.\n\r", label);
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_string(argument), "string", result != NULL);
                return result != NULL;
            }
        }
    }

    /* Set new value */
    cmd_record(record_fn, ctx, ch, label, *field_ptr, argument);
    free_string(*field_ptr);
    *field_ptr = str_dup(argument);
    send_to_char(formatf("%s set.\n\r", label), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_string_append
 * ========================================================================= */

bool olc_cmd_string_append(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, char **field_ptr,
    void *ctx, olc_cmd_record_fn record_fn)
{
    if (!IS_NULLSTR(argument)) {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s    (opens string editor)\n\r", label), ch);
        return false;
    }

    /* Non-blocking for WebSocket clients with GMCP support */
    if (ch->desc && is_websocket_connection(ch->desc)
        && ch->desc->pProtocol && ch->desc->pProtocol->bGMCP) {
        const OLC_EDITOR_DEF *def = olc_find_editor_by_type(ch->desc->editor);
        if (def) {
            if (!ch->desc->olc_state)
                ch->desc->olc_state = olc_edit_state_create();

            olc_changeset_t *cs = (def->change_mode == OLC_CHANGE_STAGED)
                ? olc_get_active_changeset(ch, def)
                : NULL;

            WNUM_LOAD wnum = olc_get_entity_wnum(def, ch->desc->pEdit);
            const char *entity_id = gmcp_editor_entity_id(def->editor_type, wnum);

            olc_string_edit_session_t *session = olc_string_session_create(
                ch->desc->olc_state, entity_id, label, field_ptr, cs);

            if (session) {
                gmcp_editor_send_string_open(ch->desc, entity_id,
                    label, field_ptr && *field_ptr ? *field_ptr : "",
                    4096, session->session_id);

                send_to_char("{GString editor opened in web client panel.{x\n\r", ch);
                cmd_record(record_fn, ctx, ch, label, "(editing)", "(editing)");
                return true;
            }
        }
    }

    /* Fallback: existing modal string_append() for telnet */
    cmd_record(record_fn, ctx, ch, label, "(edited)", "(edited)");
    string_append(ch, field_ptr);
    return true;
}

/* =========================================================================
 * olc_cmd_number
 * ========================================================================= */

bool olc_cmd_number(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int *field_ptr, int min_val, int max_val,
    void *ctx, olc_cmd_record_fn record_fn)
{
    int value;

    if (IS_NULLSTR(argument) || !is_number(argument)) {
        if (syntax) {
            send_to_char(syntax, ch);
        } else if (min_val != INT_MIN && max_val != INT_MAX) {
            send_to_char(formatf("Syntax: %s <number>  (%d to %d)\n\r",
                label, min_val, max_val), ch);
        } else {
            send_to_char(formatf("Syntax: %s <number>\n\r", label), ch);
        }
        return false;
    }

    value = atoi(argument);

    if (value < min_val || value > max_val) {
        send_to_char(formatf("%s must be between %d and %d.\n\r",
            label, min_val, max_val), ch);
        return false;
    }

    /* Staged mode: store change in overlay */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_integer(*field_ptr);
                json_t *new_val = json_integer(value);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_INT, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to %d.\n\r", label, value);
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(value), "int", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        formatf("%d", *field_ptr), formatf("%d", value));
    *field_ptr = value;
    send_to_char(formatf("%s set to %d.\n\r", label, value), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_number_i16
 * ========================================================================= */

bool olc_cmd_number_i16(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int16_t *field_ptr, int min_val, int max_val,
    void *ctx, olc_cmd_record_fn record_fn)
{
    int value;

    if (IS_NULLSTR(argument) || !is_number(argument)) {
        if (syntax) {
            send_to_char(syntax, ch);
        } else if (min_val != INT_MIN && max_val != INT_MAX) {
            send_to_char(formatf("Syntax: %s <number>  (%d to %d)\n\r",
                label, min_val, max_val), ch);
        } else {
            send_to_char(formatf("Syntax: %s <number>\n\r", label), ch);
        }
        return false;
    }

    value = atoi(argument);

    if (value < min_val || value > max_val) {
        send_to_char(formatf("%s must be between %d and %d.\n\r",
            label, min_val, max_val), ch);
        return false;
    }

    /* Staged mode: store change in overlay */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_integer((int)*field_ptr);
                json_t *new_val = json_integer(value);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_INT16, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to %d.\n\r", label, value);
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(value), "int16", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        formatf("%d", (int)*field_ptr), formatf("%d", value));
    *field_ptr = (int16_t)value;
    send_to_char(formatf("%s set to %d.\n\r", label, value), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_long
 * ========================================================================= */

bool olc_cmd_long(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *field_ptr, long min_val, long max_val,
    void *ctx, olc_cmd_record_fn record_fn)
{
    long value;

    if (IS_NULLSTR(argument) || !is_number(argument)) {
        if (syntax) {
            send_to_char(syntax, ch);
        } else if (min_val != LONG_MIN && max_val != LONG_MAX) {
            send_to_char(formatf("Syntax: %s <number>  (%ld to %ld)\n\r",
                label, min_val, max_val), ch);
        } else {
            send_to_char(formatf("Syntax: %s <number>\n\r", label), ch);
        }
        return false;
    }

    value = atol(argument);

    if (value < min_val || value > max_val) {
        send_to_char(formatf("%s must be between %ld and %ld.\n\r",
            label, min_val, max_val), ch);
        return false;
    }

    /* Staged mode: store change in overlay */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_integer(*field_ptr);
                json_t *new_val = json_integer(value);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_LONG, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to %ld.\n\r", label, value);
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(value), "long", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        formatf("%ld", *field_ptr), formatf("%ld", value));
    *field_ptr = value;
    send_to_char(formatf("%s set to %ld.\n\r", label, value), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_flag_toggle
 * ========================================================================= */

bool olc_cmd_flag_toggle(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, long *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn)
{
    long value;

    if (IS_NULLSTR(argument)) {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s <flag list>\n\r"
                                 "Type '? %s' for a list of flags.\n\r",
                                 label, label), ch);
        return false;
    }

    value = flag_value(flag_table, argument);
    if (value == NO_FLAG) {
        send_to_char("Invalid flag.  Type '? ",  ch);
        send_to_char(label, ch);
        send_to_char("' for a list.\n\r", ch);
        return false;
    }

    /* Staged mode: compute toggle against staged (or live) value */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                long current = olc_staged_flags(cs, label, *field_ptr);
                long toggled = current ^ value;
                json_t *old_val = json_integer(*field_ptr);
                json_t *new_val = json_integer(toggled);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_FLAGS, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s toggled. Current: %s\n\r",
                        label, flag_string(flag_table, toggled));
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(toggled), "flags", result != NULL);
                return result != NULL;
            }
        }
    }

    {
        char old_flags[MAX_STRING_LENGTH];
        snprintf(old_flags, sizeof(old_flags), "%s",
            flag_string(flag_table, *field_ptr));

        *field_ptr ^= value;

        cmd_record(record_fn, ctx, ch, label, old_flags,
            flag_string(flag_table, *field_ptr));
    }

    send_to_char(formatf("%s toggled. Current: %s\n\r",
        label, flag_string(flag_table, *field_ptr)), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_type_set
 * ========================================================================= */

bool olc_cmd_type_set(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn)
{
    int value;

    if (IS_NULLSTR(argument)) {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s <type>\n\r"
                                 "Type '? %s' for a list.\n\r",
                                 label, label), ch);
        return false;
    }

    value = flag_value(flag_table, argument);
    if (value == NO_FLAG) {
        send_to_char(formatf("Invalid %s.  Type '? %s' for a list.\n\r",
            label, label), ch);
        return false;
    }

    /* Staged mode */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_integer(*field_ptr);
                json_t *new_val = json_integer(value);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_INT, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to: %s\n\r",
                        label, flag_name(flag_table, value));
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(value), "int", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        flag_name(flag_table, *field_ptr),
        flag_name(flag_table, value));
    *field_ptr = value;
    send_to_char(formatf("%s set to: %s\n\r",
        label, flag_name(flag_table, *field_ptr)), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_type_set_i16
 * ========================================================================= */

bool olc_cmd_type_set_i16(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, int16_t *field_ptr, const struct flag_type *flag_table,
    void *ctx, olc_cmd_record_fn record_fn)
{
    int value;

    if (IS_NULLSTR(argument)) {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s <type>\n\r"
                                 "Type '? %s' for a list.\n\r",
                                 label, label), ch);
        return false;
    }

    value = flag_value(flag_table, argument);
    if (value == NO_FLAG) {
        send_to_char(formatf("Invalid %s.  Type '? %s' for a list.\n\r",
            label, label), ch);
        return false;
    }

    /* Staged mode */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_integer((int)*field_ptr);
                json_t *new_val = json_integer(value);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_INT16, old_val, new_val);
                json_decref(old_val);
                json_decref(new_val);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to: %s\n\r",
                        label, flag_name(flag_table, value));
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_integer(value), "int16", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        flag_name(flag_table, (int)*field_ptr),
        flag_name(flag_table, value));
    *field_ptr = (int16_t)value;
    send_to_char(formatf("%s set to: %s\n\r",
        label, flag_name(flag_table, (int)*field_ptr)), ch);
    return true;
}

/* =========================================================================
 * olc_cmd_bool
 * ========================================================================= */

bool olc_cmd_bool(CHAR_DATA *ch, char *argument, const char *label,
    const char *syntax, bool *field_ptr,
    void *ctx, olc_cmd_record_fn record_fn)
{
    bool new_val;

    if (IS_NULLSTR(argument)) {
        /* Toggle — use staged value if in staged mode for correct toggle */
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            new_val = !olc_staged_bool(cs, label, *field_ptr);
        } else {
            new_val = !(*field_ptr);
        }
    } else if (!str_cmp(argument, "yes") || !str_cmp(argument, "true")
            || !str_cmp(argument, "on")) {
        new_val = true;
    } else if (!str_cmp(argument, "no") || !str_cmp(argument, "false")
            || !str_cmp(argument, "off")) {
        new_val = false;
    } else {
        if (syntax)
            send_to_char(syntax, ch);
        else
            send_to_char(formatf("Syntax: %s [yes|no]\n\r"
                                 "        %s          (toggles)\n\r",
                                 label, label), ch);
        return false;
    }

    /* Staged mode: store change in overlay */
    {
        const OLC_EDITOR_DEF *edef = olc_find_editor_by_type(ch->desc->editor);
        if (edef && edef->change_mode == OLC_CHANGE_STAGED) {
            olc_changeset_t *cs = olc_get_active_changeset(ch, edef);
            if (cs) {
                if (!olc_check_staging_limits(ch, cs)) return false;
                json_t *old_val = json_boolean(*field_ptr);
                json_t *new_val_j = json_boolean(new_val);
                olc_pending_change_t *result = olc_changeset_add_change(
                    cs, label, OLC_FIELD_BOOL, old_val, new_val_j);
                json_decref(old_val);
                json_decref(new_val_j);
                if (result)
                    printf_to_char(ch, "{G[STAGED]{x %s set to %s.\n\r",
                        label, new_val ? "Yes" : "No");
                else
                    printf_to_char(ch, "%s reverted to original value.\n\r", label);
                notify_field_change(cs, ch, label, json_boolean(new_val), "bool", result != NULL);
                return result != NULL;
            }
        }
    }

    cmd_record(record_fn, ctx, ch, label,
        *field_ptr ? "Yes" : "No",
        new_val    ? "Yes" : "No");
    *field_ptr = new_val;
    send_to_char(formatf("%s set to %s.\n\r",
        label, *field_ptr ? "Yes" : "No"), ch);
    return true;
}
