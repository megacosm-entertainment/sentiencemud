/**
 * @file olc_commands.c
 * @brief Common OLC Command Helper Implementations
 *
 * See olc_commands.h for API documentation.
 */

#include "olc_commands.h"
#include "../common.h"

#include <limits.h>

/* =========================================================================
 * Internals
 * ========================================================================= */

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

    cmd_record(record_fn, ctx, ch, label,
        formatf("%d", (int)*field_ptr), formatf("%d", value));
    *field_ptr = (int16_t)value;
    send_to_char(formatf("%s set to %d.\n\r", label, value), ch);
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
        /* Toggle */
        new_val = !(*field_ptr);
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

    cmd_record(record_fn, ctx, ch, label,
        *field_ptr ? "Yes" : "No",
        new_val    ? "Yes" : "No");
    *field_ptr = new_val;
    send_to_char(formatf("%s set to %s.\n\r",
        label, *field_ptr ? "Yes" : "No"), ch);
    return true;
}
