/**
 * @file olc_display.c
 * @brief Common display and rendering utilities for OLC editors
 *
 * Implements theme-aware rendering functions for all editor display needs.
 * These functions build on the existing OLC_LAYOUT_CTX from common.h
 * and add theme support plus additional renderers.
 *
 * @see olc_display.h for API documentation
 */

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../common.h"
#include "olc_editor.h"
#include "olc_display.h"
#include "olc_staged.h"

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * Check if MXP is available and enabled for a character.
 */
static bool display_use_mxp(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) return false;
    return isMXP(ch->desc) && IS_SET(ch->comm, COMM_MXP);
}

/**
 * Format a label with optional MXP click command.
 * Result is written to dest buffer. Safe against static buffer reuse.
 */
static void display_format_label(CHAR_DATA *ch, const char *command,
                                 const char *label,
                                 char *dest, size_t dest_size)
{
    if (!command || command[0] == '\0' || !display_use_mxp(ch)) {
        strncpy(dest, label ? label : "", dest_size - 1);
        dest[dest_size - 1] = '\0';
        return;
    }

    const char *result = MXPCreateSend(ch->desc, command, label);
    strncpy(dest, result, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* =========================================================================
 * Extended Layout Context
 * ========================================================================= */

OLC_LAYOUT_CTX *olc_display_new(CHAR_DATA *ch, const OLC_EDITOR_THEME *theme)
{
    /* Use the existing layout constructor - theme is passed to render calls */
    return olc_layout_new(ch);
}

/* =========================================================================
 * Editor Header / Footer
 * ========================================================================= */

void olc_display_header(OLC_LAYOUT_CTX *ctx, const char *editor_name,
                        const char *entity_name, const char *entity_id,
                        const OLC_EDITOR_DEF *def)
{
    char buf[MSL];
    const OLC_EDITOR_THEME *theme;
    int title_len, pad;

    if (!ctx || !ctx->buffer) return;

    theme = olc_get_theme(def);

    /* Build the title string */
    if (entity_id && entity_name) {
        snprintf(buf, sizeof(buf), "%s [%s] %s", editor_name, entity_id, entity_name);
    } else if (entity_name) {
        snprintf(buf, sizeof(buf), "%s - %s", editor_name, entity_name);
    } else if (entity_id) {
        snprintf(buf, sizeof(buf), "%s [%s]", editor_name, entity_id);
    } else {
        snprintf(buf, sizeof(buf), "%s", editor_name);
    }

    title_len = strlen(buf);
    pad = (ctx->screen_width - title_len - 4) / 2;
    if (pad < 3) pad = 3;

    /* Render: === Title === */
    add_buf(ctx->buffer, theme->border);
    for (int i = 0; i < pad; i++) add_buf(ctx->buffer, "=");
    add_buf(ctx->buffer, " ");
    add_buf(ctx->buffer, theme->header);
    add_buf(ctx->buffer, buf);
    add_buf(ctx->buffer, " ");
    add_buf(ctx->buffer, theme->border);
    for (int i = 0; i < pad; i++) add_buf(ctx->buffer, "=");
    add_buf(ctx->buffer, "{x\n\r");

    /* Render tabs if the editor has them */
    if (def && def->tabs.count > 0 && !olc_show_all_tabs_mode(ctx->ch)) {
        /* Build an OLC_EDITOR_TABS compatible struct for the existing renderer */
        OLC_EDITOR_TABS compat_tabs;
        compat_tabs.tab_count = def->tabs.count;
        for (int i = 0; i < def->tabs.count && i < OLC_MAX_TABS; i++) {
            compat_tabs.tabs[i].name = def->tabs.tabs[i].name;
            compat_tabs.tabs[i].short_name = def->tabs.tabs[i].short_name;
        }
        olc_render_tabs(ctx, &compat_tabs);
    }
}

void olc_display_footer(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme)
{
    char buf[MSL];
    int dash_count;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    dash_count = ctx->screen_width - 4;
    if (dash_count < 10) dash_count = 10;
    if (dash_count > (int)sizeof(buf) - 10) dash_count = sizeof(buf) - 10;

    add_buf(ctx->buffer, theme->border);
    for (int i = 0; i < dash_count; i++) add_buf(ctx->buffer, "=");
    add_buf(ctx->buffer, "{x\n\r");
}

/* =========================================================================
 * Field Renderers - Theme-aware
 * ========================================================================= */

void olc_display_string(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                        const char *label, const char *command,
                        const char *value)
{
    char buf[MSL];
    char label_buf[MIL];
    int pad;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    /* Pending marker if field is staged */
    const char *marker = "";
    if (ctx->changeset && command && olc_is_field_staged(ctx->changeset, command))
        marker = "{Y*{x";

    display_format_label(ctx->ch, command, label, label_buf, sizeof(label_buf));

    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    if (IS_NULLSTR(value)) {
        snprintf(buf, sizeof(buf), "%s%s%s%*s %s(unset){x\n\r",
                 marker, theme->label, label_buf, pad, "", theme->unset);
    } else {
        snprintf(buf, sizeof(buf), "%s%s%s%*s %s%s{x\n\r",
                 marker, theme->label, label_buf, pad, "", theme->value, value);
    }
    add_buf(ctx->buffer, buf);
}

void olc_display_number(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                        const char *label, const char *command, long value)
{
    long display_value = value;
    if (ctx && ctx->changeset && command)
        display_value = olc_staged_flags(ctx->changeset, command, value);
    char val_buf[32];
    snprintf(val_buf, sizeof(val_buf), "%ld", display_value);
    olc_display_string(ctx, theme, label, command, val_buf);
}

void olc_display_dice(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command, DICE_DATA *dice)
{
    if (!dice) {
        olc_display_string(ctx, theme, label, command, NULL);
        return;
    }
    char val_buf[64];
    snprintf(val_buf, sizeof(val_buf), "%dd%d+%d", dice->number, dice->size, dice->bonus);
    olc_display_string(ctx, theme, label, command, val_buf);
}

void olc_display_bool(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command, bool value)
{
    char buf[MSL];
    char label_buf[MIL];
    int pad;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    bool display_value = value;
    if (ctx->changeset && command)
        display_value = olc_staged_bool(ctx->changeset, command, value);

    const char *marker = "";
    if (ctx->changeset && command && olc_is_field_staged(ctx->changeset, command))
        marker = "{Y*{x";

    display_format_label(ctx->ch, command, label, label_buf, sizeof(label_buf));

    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    snprintf(buf, sizeof(buf), "%s%s%s%*s %s%s{x\n\r",
             marker, theme->label, label_buf, pad, "",
             display_value ? "{G" : theme->unset,
             display_value ? "Yes" : "No");
    add_buf(ctx->buffer, buf);
}

void olc_display_vnum(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      long vnum, const char *name)
{
    char val_buf[MIL];

    if (vnum <= 0) {
        olc_display_string(ctx, theme, label, command, NULL);
        return;
    }

    if (name && name[0] != '\0') {
        snprintf(val_buf, sizeof(val_buf), "%ld (%s)", vnum, name);
    } else {
        snprintf(val_buf, sizeof(val_buf), "%ld", vnum);
    }
    olc_display_string(ctx, theme, label, command, val_buf);
}

void olc_display_widevnum(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                          const char *label, const char *command,
                          const char *wnum_str, const char *name)
{
    char val_buf[MIL];

    if (!wnum_str || wnum_str[0] == '\0') {
        olc_display_string(ctx, theme, label, command, NULL);
        return;
    }

    if (name && name[0] != '\0') {
        snprintf(val_buf, sizeof(val_buf), "%s (%s)", wnum_str, name);
    } else {
        snprintf(val_buf, sizeof(val_buf), "%s", wnum_str);
    }
    olc_display_string(ctx, theme, label, command, val_buf);
}

void olc_display_percent(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         const char *label, const char *command,
                         int value, int scale)
{
    char val_buf[32];
    if (scale > 1) {
        snprintf(val_buf, sizeof(val_buf), "%d.%d%%", value / scale, value % scale);
    } else {
        snprintf(val_buf, sizeof(val_buf), "%d%%", value);
    }
    olc_display_string(ctx, theme, label, command, val_buf);
}

void olc_display_flags(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                       const char *label, const char *command,
                       const struct flag_type *table, long value)
{
    if (!ctx || !ctx->buffer || !table) return;
    if (!theme) theme = &olc_theme_default;

    long display_value = value;
    if (ctx->changeset && command)
        display_value = olc_staged_flags(ctx->changeset, command, value);

    /* Prefix marker if staged */
    if (ctx->changeset && command && olc_is_field_staged(ctx->changeset, command))
        add_buf(ctx->buffer, "{Y*{x");

    olc_buffer_show_flags_ex(ctx->ch, ctx->buffer, table, display_value,
        (char *)(command ? command : ""), (char *)label,
        ctx->screen_width - 3, ctx->label_width, 5,
        theme->flag_colors);
}

void olc_display_type(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      const struct flag_type *table, int value)
{
    char buf[MSL];
    char label_buf[MIL];
    const char *type_name = "unknown";
    int pad;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    int display_value = value;
    if (ctx->changeset && command)
        display_value = olc_staged_int(ctx->changeset, command, value);

    if (table)
        type_name = flag_name(table, display_value);

    const char *marker = "";
    if (ctx->changeset && command && olc_is_field_staged(ctx->changeset, command))
        marker = "{Y*{x";

    display_format_label(ctx->ch, command, label, label_buf, sizeof(label_buf));

    pad = ctx->label_width - strlen_no_colours(label);
    if (pad < 0) pad = 0;

    snprintf(buf, sizeof(buf), "%s%s%s%*s %s(%s){x\n\r",
             marker, theme->label, label_buf, pad, "", theme->value, type_name);
    add_buf(ctx->buffer, buf);
}

void olc_display_text(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      const char *text)
{
    char buf[MSL];
    char label_buf[MIL];

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    if (label && label[0] != '\0') {
        display_format_label(ctx->ch, command, label, label_buf, sizeof(label_buf));
        snprintf(buf, sizeof(buf), "%s%s{x\n\r", theme->label, label_buf);
        add_buf(ctx->buffer, buf);
    }

    if (IS_NULLSTR(text)) {
        snprintf(buf, sizeof(buf), "   %s(unset){x\n\r", theme->unset);
        add_buf(ctx->buffer, buf);
    } else {
        /* Only indent single-line values; multi-line code blocks (label=NULL)
         * manage their own indentation and shouldn't get a leading prefix. */
        if (label && label[0] != '\0') {
            add_buf(ctx->buffer, "   ");
        }
        add_buf(ctx->buffer, text);
        int len = strlen(text);
        if (len > 0 && text[len - 1] != '\n' && text[len - 1] != '\r') {
            add_buf(ctx->buffer, "\n\r");
        }
    }
}

void olc_display_pair(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label1, const char *cmd1, const char *value1,
                      const char *label2, const char *cmd2, const char *value2)
{
    char buf[MSL];
    char lbl1[MIL], lbl2[MIL];
    int pad1, half_width;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    half_width = (ctx->screen_width - 4) / 2;
    if (half_width < 30) half_width = 30;

    display_format_label(ctx->ch, cmd1, label1, lbl1, sizeof(lbl1));
    display_format_label(ctx->ch, cmd2, label2, lbl2, sizeof(lbl2));

    pad1 = ctx->label_width - strlen_no_colours(label1);
    if (pad1 < 0) pad1 = 0;

    /* Build first half */
    int pos = snprintf(buf, sizeof(buf), "%s%s%*s %s%s{x",
                       theme->label, lbl1, pad1, "",
                       IS_NULLSTR(value1) ? theme->unset : theme->value,
                       IS_NULLSTR(value1) ? "(unset)" : value1);

    /* Pad to half width (approximate - color codes make exact alignment tricky) */
    int visible1 = strlen_no_colours(buf);
    int gap = half_width - visible1;
    if (gap < 2) gap = 2;
    for (int i = 0; i < gap && pos < (int)sizeof(buf) - 1; i++)
        buf[pos++] = ' ';

    /* Build second half */
    int pad2 = ctx->label_width - strlen_no_colours(label2);
    if (pad2 < 0) pad2 = 0;

    snprintf(buf + pos, sizeof(buf) - pos, "%s%s%*s %s%s{x\n\r",
             theme->label, lbl2, pad2, "",
             IS_NULLSTR(value2) ? theme->unset : theme->value,
             IS_NULLSTR(value2) ? "(unset)" : value2);

    add_buf(ctx->buffer, buf);
}

/* =========================================================================
 * Section Dividers
 * ========================================================================= */

void olc_display_section(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         const char *title)
{
    char buf[MSL];
    int title_len, dash_count, pos;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    add_buf(ctx->buffer, "\n\r");

    if (title && title[0] != '\0') {
        title_len = strlen(title);
        dash_count = (ctx->screen_width - title_len - 6) / 2;
        if (dash_count < 3) dash_count = 3;

        pos = 0;
        pos += sprintf(buf + pos, "%s", theme->section);
        memset(buf + pos, '-', dash_count);
        pos += dash_count;
        pos += sprintf(buf + pos, " %s%s%s ", theme->section_text, title, theme->section);
        memset(buf + pos, '-', dash_count);
        pos += dash_count;
        pos += sprintf(buf + pos, "{x\n\r");
        buf[pos] = '\0';

        add_buf(ctx->buffer, buf);
    }
}

void olc_display_hr(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme)
{
    char buf[MSL];
    int dash_count;

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    dash_count = ctx->screen_width - 4;
    if (dash_count < 10) dash_count = 10;
    if (dash_count > (int)sizeof(buf) - 10) dash_count = sizeof(buf) - 10;

    int pos = sprintf(buf, "%s", theme->section);
    memset(buf + pos, '-', dash_count);
    pos += dash_count;
    sprintf(buf + pos, "{x\n\r");

    add_buf(ctx->buffer, buf);
}

void olc_display_blank(OLC_LAYOUT_CTX *ctx)
{
    if (!ctx || !ctx->buffer) return;
    add_buf(ctx->buffer, "\n\r");
}

/* =========================================================================
 * List / Table Renderers
 * ========================================================================= */

void olc_display_table_begin(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                             const char *title,
                             const OLC_TABLE_COL *cols, int num_cols)
{
    char buf[MSL];
    int pos;

    if (!ctx || !ctx->buffer || !cols) return;
    if (!theme) theme = &olc_theme_default;

    /* Title line */
    if (title && title[0] != '\0') {
        olc_display_section(ctx, theme, title);
    }

    /* Header row */
    pos = 0;
    pos += sprintf(buf + pos, "%s", theme->border);
    for (int i = 0; i < num_cols; i++) {
        int w = cols[i].width > 0 ? cols[i].width : 15;
        if (cols[i].right_align) {
            pos += sprintf(buf + pos, "%s%*s%s",
                           theme->label, w, cols[i].header ? cols[i].header : "",
                           i < num_cols - 1 ? " " : "");
        } else {
            pos += sprintf(buf + pos, "%s%-*s%s",
                           theme->label, w, cols[i].header ? cols[i].header : "",
                           i < num_cols - 1 ? " " : "");
        }
    }
    pos += sprintf(buf + pos, "{x\n\r");
    add_buf(ctx->buffer, buf);

    /* Separator */
    pos = 0;
    pos += sprintf(buf + pos, "%s", theme->border);
    for (int i = 0; i < num_cols; i++) {
        int w = cols[i].width > 0 ? cols[i].width : 15;
        memset(buf + pos, '-', w);
        pos += w;
        if (i < num_cols - 1) buf[pos++] = ' ';
    }
    pos += sprintf(buf + pos, "{x\n\r");
    add_buf(ctx->buffer, buf);
}

void olc_display_table_row(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                           const char **values, int num_cols, bool highlight)
{
    /* We need the column definitions here but they aren't passed.
     * For now, use default widths. Users should use the col widths they defined. */
    char buf[MSL];
    int pos = 0;

    if (!ctx || !ctx->buffer || !values) return;
    if (!theme) theme = &olc_theme_default;

    const char *val_color = highlight ? theme->clickable : theme->value;

    for (int i = 0; i < num_cols; i++) {
        const char *val = values[i] ? values[i] : "";
        pos += sprintf(buf + pos, "%s%s%s",
                       val_color, val,
                       i < num_cols - 1 ? " " : "");
    }
    pos += sprintf(buf + pos, "{x\n\r");
    add_buf(ctx->buffer, buf);
}

void olc_display_table_end(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme)
{
    if (!ctx || !ctx->buffer) return;
    /* Just a blank line after the table */
    add_buf(ctx->buffer, "\n\r");
}

void olc_display_list(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *title, const char **items, int count,
                      const char *del_command)
{
    char buf[MSL];

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    if (title && title[0] != '\0') {
        olc_display_section(ctx, theme, title);
    }

    if (count == 0) {
        snprintf(buf, sizeof(buf), "  %s(none){x\n\r", theme->unset);
        add_buf(ctx->buffer, buf);
        return;
    }

    for (int i = 0; i < count; i++) {
        const char *item = items[i] ? items[i] : "(null)";

        if (del_command && display_use_mxp(ctx->ch)) {
            char del_cmd[MIL];
            char del_text[MIL];
            snprintf(del_cmd, sizeof(del_cmd), "%s %d", del_command, i + 1);
            snprintf(del_text, sizeof(del_text), "{R[X]{x");
            const char *mxp = MXPCreateSend(ctx->ch->desc, del_cmd, del_text);
            /* Copy to avoid static buffer reuse */
            char mxp_copy[MIL];
            strncpy(mxp_copy, mxp, sizeof(mxp_copy) - 1);
            mxp_copy[sizeof(mxp_copy) - 1] = '\0';

            snprintf(buf, sizeof(buf), "  %s%s[%s%2d%s] %s%s{x\n\r",
                     mxp_copy, theme->border,
                     theme->label, i + 1, theme->border,
                     theme->value, item);
        } else {
            snprintf(buf, sizeof(buf), "  %s[%s%2d%s] %s%s{x\n\r",
                     theme->border, theme->label, i + 1, theme->border,
                     theme->value, item);
        }
        add_buf(ctx->buffer, buf);
    }
}

void olc_display_infof(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                       const char *fmt, ...)
{
    char buf[MSL];
    va_list args;

    if (!ctx || !ctx->buffer || !fmt) return;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 4, fmt, args);
    va_end(args);

    strcat(buf, "\n\r");
    add_buf(ctx->buffer, buf);
}

/* =========================================================================
 * Script/Prog Display
 * ========================================================================= */

void olc_display_scripts(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         LLIST **progs, int type, const char *title,
                         const char *add_cmd, const char *del_cmd)
{
    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;
    if (!progs) return;

    /* Use the existing grouped display function which writes to a buffer */
    olc_show_progs_grouped(ctx->buffer, progs, type, title);

    /* If MXP and add command provided, show an add link */
    if (add_cmd && display_use_mxp(ctx->ch)) {
        const char *mxp = MXPCreateSend(ctx->ch->desc, add_cmd, "{G[+ Add Script]{x");
        char mxp_copy[MIL];
        strncpy(mxp_copy, mxp, sizeof(mxp_copy) - 1);
        mxp_copy[sizeof(mxp_copy) - 1] = '\0';
        add_buf(ctx->buffer, "  ");
        add_buf(ctx->buffer, mxp_copy);
        add_buf(ctx->buffer, "\n\r");
    }
}

/* =========================================================================
 * Variable Display
 * ========================================================================= */

void olc_display_vars(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      pVARIABLE var_list, const char *set_cmd,
                      const char *clear_cmd)
{
    char buf[MSL];

    if (!ctx || !ctx->buffer) return;
    if (!theme) theme = &olc_theme_default;

    olc_display_section(ctx, theme, "Variables");

    if (!var_list) {
        snprintf(buf, sizeof(buf), "  %s(none){x\n\r", theme->unset);
        add_buf(ctx->buffer, buf);
        return;
    }

    /* Use the existing variable display function */
    olc_show_index_vars(ctx->buffer, var_list);
}

/* =========================================================================
 * Entity List Display
 * ========================================================================= */

void olc_display_entity_list(CHAR_DATA *ch, const OLC_EDITOR_THEME *theme,
                             void **entities, int count,
                             const OLC_LIST_OPTS *opts,
                             void (*format_fn)(void *entity, const char **values,
                                              int max_cols))
{
    BUFFER *buffer;
    char buf[MSL];
    int start, end;
    int shown = 0;

    if (!ch || !entities || count == 0) {
        send_to_char("No entries found.\n\r", ch);
        return;
    }
    if (!theme) theme = &olc_theme_default;

    buffer = new_buf();

    /* Title */
    if (opts && opts->title) {
        snprintf(buf, sizeof(buf), "\n\r%s%s{x\n\r", theme->header, opts->title);
        add_buf(buffer, buf);
    }

    /* Calculate pagination */
    if (opts && opts->page_size > 0) {
        start = opts->page * opts->page_size;
        end = start + opts->page_size;
        if (end > count) end = count;
    } else {
        start = 0;
        end = count;
    }

    /* Render entries */
    #define MAX_LIST_COLS 8
    const char *values[MAX_LIST_COLS];

    for (int i = start; i < end; i++) {
        if (!entities[i]) continue;

        /* Apply filter if present */
        if (opts && opts->filter_fn && opts->filter) {
            if (!opts->filter_fn(entities[i], opts->filter))
                continue;
        }

        /* Format this entity */
        memset(values, 0, sizeof(values));
        if (format_fn) {
            format_fn(entities[i], values, MAX_LIST_COLS);
        }

        /* Output formatted values */
        snprintf(buf, sizeof(buf), "  %s[%s%3d%s] ",
                 theme->border, theme->label, i + 1, theme->border);
        add_buf(buffer, buf);

        for (int c = 0; c < MAX_LIST_COLS && values[c]; c++) {
            snprintf(buf, sizeof(buf), "%s%s ", theme->value, values[c]);
            add_buf(buffer, buf);
        }
        add_buf(buffer, "{x\n\r");
        shown++;
    }

    /* Footer with count */
    snprintf(buf, sizeof(buf), "\n\r%sShowing %d of %d entries.{x\n\r",
             theme->unset, shown, count);
    add_buf(buffer, buf);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}
