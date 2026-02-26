/**
 * @file olc_display.h
 * @brief Common display and rendering utilities for OLC editors
 *
 * Provides unified rendering functions for displaying editor content:
 *   - Tabular data (key-value fields with consistent alignment)
 *   - Lists and collections (numbered, bulleted, etc.)
 *   - Headers and sections
 *   - Flag displays with MXP toggle support
 *   - Inline tables for compact multi-column data
 *
 * All renderers are theme-aware and use the OLC_LAYOUT_CTX for
 * screen-width-responsive output. They work with both the new
 * OLC_EDITOR_DEF framework and legacy editors during migration.
 *
 * @see editors/common/olc_editor.h for the editor framework
 * @see editors/common.h for legacy display functions
 */

#ifndef __OLC_DISPLAY_H__
#define __OLC_DISPLAY_H__

#include "../../merc.h"
#include "../../olc.h"
#include "../common.h"
#include "olc_editor.h"

/* =========================================================================
 * Extended Layout Context
 * ========================================================================= */

/**
 * Create a themed layout context.
 * Like olc_layout_new() but attaches a theme for color-aware rendering.
 *
 * @param ch    Character viewing the output
 * @param theme Theme to use (NULL for default)
 * @return Layout context (must be freed with olc_layout_free)
 */
OLC_LAYOUT_CTX *olc_display_new(CHAR_DATA *ch, const OLC_EDITOR_THEME *theme);

/* =========================================================================
 * Editor Header / Footer
 * ========================================================================= */

/**
 * Render the standard editor header.
 * Shows the editor name, entity identifier, and tab bar (if applicable).
 *
 * Example output:
 *   ============= MEdit [#3001] Biff the Gateguard =============
 *    __________   _________   __________
 *   /1 General\__/2 Combat\__/3 Scripts\_
 *
 * @param ctx           Layout context
 * @param editor_name   Editor display name (e.g., "MEdit")
 * @param entity_name   Entity name/identifier (e.g., "Biff the Gateguard")
 * @param entity_id     Entity ID string (e.g., "#3001" or "UID 5")
 * @param tabs          Tab definitions (NULL for no tabs)
 * @param theme         Color theme
 */
void olc_display_header(OLC_LAYOUT_CTX *ctx, const char *editor_name,
                        const char *entity_name, const char *entity_id,
                        const OLC_EDITOR_DEF *def);

/**
 * Render a standard footer / bottom separator.
 *
 * @param ctx   Layout context
 * @param theme Color theme
 */
void olc_display_footer(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme);

/* =========================================================================
 * Field Renderers - Theme-aware versions
 * ========================================================================= */

/**
 * Render a string field with theme colors.
 * Shows "(unset)" in the unset color if value is NULL/empty.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command for clicking (NULL = read-only)
 * @param value     Field value (NULL/empty = shows "(unset)")
 */
void olc_display_string(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                        const char *label, const char *command,
                        const char *value);

/**
 * Render a number field with theme colors.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command (NULL = read-only)
 * @param value     Numeric value
 */
void olc_display_number(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                        const char *label, const char *command, long value);

/**
 * Render a dice field (XdY+Z) with theme colors.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command (NULL = read-only)
 * @param dice      Dice data (NULL = shows "(unset)")
 */
void olc_display_dice(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command, DICE_DATA *dice);

/**
 * Render a boolean/yes-no field with theme colors.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command (NULL = read-only)
 * @param value     Boolean value
 */
void olc_display_bool(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command, bool value);

/**
 * Render a vnum field with optional entity name lookup.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command (NULL = read-only)
 * @param vnum      Vnum value (0 = shows "(none)")
 * @param name      Entity name at this vnum (NULL = no name shown)
 */
void olc_display_vnum(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      long vnum, const char *name);

/**
 * Render a percentage field (0-100 or 0-1000).
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command (NULL = read-only)
 * @param value     Percentage value
 * @param scale     Divisor to convert to percentage (1 = already %, 10 = tenths)
 */
void olc_display_percent(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         const char *label, const char *command,
                         int value, int scale);

/**
 * Render flags/bits with theme-aware colors and MXP toggles.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP toggle command prefix
 * @param table     Flag table
 * @param value     Current flag value
 */
void olc_display_flags(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                       const char *label, const char *command,
                       const struct flag_type *table, long value);

/**
 * Render a type/stat selection with theme-aware colors.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label
 * @param command   MXP command
 * @param table     Type table
 * @param value     Current value
 */
void olc_display_type(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      const struct flag_type *table, int value);

/**
 * Render a multi-line text/description field.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label     Field label (NULL = no label line)
 * @param command   MXP command to edit (NULL = read-only)
 * @param text      Text content (NULL/empty = shows "(unset)")
 */
void olc_display_text(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label, const char *command,
                      const char *text);

/**
 * Render a two-column pair of values on one line.
 * Useful for things like "Min: 5   Max: 10" or "Level: 50   Align: -1000"
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param label1    First field label
 * @param cmd1      First field MXP command (NULL = read-only)
 * @param value1    First field value string
 * @param label2    Second field label
 * @param cmd2      Second field MXP command (NULL = read-only)
 * @param value2    Second field value string
 */
void olc_display_pair(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *label1, const char *cmd1, const char *value1,
                      const char *label2, const char *cmd2, const char *value2);

/* =========================================================================
 * Section Dividers
 * ========================================================================= */

/**
 * Render a titled section divider.
 * Example: "--- Combat Properties ---"
 *
 * @param ctx   Layout context
 * @param theme Color theme
 * @param title Section title
 */
void olc_display_section(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         const char *title);

/**
 * Render a horizontal rule without title.
 *
 * @param ctx   Layout context
 * @param theme Color theme
 */
void olc_display_hr(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme);

/**
 * Add a blank line to the output.
 *
 * @param ctx   Layout context
 */
void olc_display_blank(OLC_LAYOUT_CTX *ctx);

/* =========================================================================
 * List / Table Renderers
 * ========================================================================= */

/**
 * Column definition for inline table rendering.
 */
typedef struct olc_table_col {
    const char  *header;        /**< Column header text */
    int         width;          /**< Column width in characters (0 = auto) */
    bool        right_align;    /**< Right-align values in this column */
} OLC_TABLE_COL;

/**
 * Begin rendering an inline data table.
 * Call olc_display_table_row() for each row, then olc_display_table_end().
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param title     Table title (NULL = no title)
 * @param cols      Column definitions array
 * @param num_cols  Number of columns
 */
void olc_display_table_begin(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                             const char *title,
                             const OLC_TABLE_COL *cols, int num_cols);

/**
 * Render one row of an inline data table.
 * The values array must have the same number of entries as cols in _begin.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param values    Array of string values (one per column)
 * @param num_cols  Number of columns (must match _begin)
 * @param highlight Whether to highlight this row
 */
void olc_display_table_row(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                           const char **values, int num_cols, bool highlight);

/**
 * End an inline data table (renders bottom border).
 *
 * @param ctx   Layout context
 * @param theme Color theme
 */
void olc_display_table_end(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme);

/**
 * Render a numbered list with optional MXP delete commands.
 *
 * @param ctx           Layout context
 * @param theme         Color theme
 * @param title         List title/heading
 * @param items         Array of item display strings
 * @param count         Number of items
 * @param del_command   Delete command prefix (e.g., "deltrait") for MXP, NULL to omit
 */
void olc_display_list(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      const char *title, const char **items, int count,
                      const char *del_command);

/**
 * Render a simple key-value summary line (not indented like field renderers).
 * Good for compact info at the top of an editor.
 *
 * Example: "Area: Midgaard (#1)    Vnum: 3001    Level: 50"
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param fmt       Format string (uses color codes)
 * @param ...       Format arguments
 */
void olc_display_infof(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                       const char *fmt, ...);

/* =========================================================================
 * Script/Prog Display
 * ========================================================================= */

/**
 * Render a grouped script/program listing.
 * Shows scripts grouped by vnum with their triggers underneath.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param progs     Program list array (by trigger slot)
 * @param type      Script type (PRG_MPROG, PRG_OPROG, etc.)
 * @param title     Section title
 * @param add_cmd   MXP command to add a script (NULL = no add link)
 * @param del_cmd   MXP command prefix to delete (NULL = no delete link)
 */
void olc_display_scripts(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                         LLIST **progs, int type, const char *title,
                         const char *add_cmd, const char *del_cmd);

/* =========================================================================
 * Variable Display
 * ========================================================================= */

/**
 * Render index variables in a standard format.
 *
 * @param ctx       Layout context
 * @param theme     Color theme
 * @param var_list  Variable list
 * @param set_cmd   MXP command prefix for setting vars
 * @param clear_cmd MXP command prefix for clearing vars
 */
void olc_display_vars(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_THEME *theme,
                      pVARIABLE var_list, const char *set_cmd,
                      const char *clear_cmd);

/* =========================================================================
 * Common Editor List Functions
 * ========================================================================= */

/**
 * Options for entity list display.
 */
typedef struct olc_list_opts {
    const char  *title;         /**< List title */
    const char  *edit_command;  /**< Command to edit an entry (MXP) */
    int         page_size;      /**< Items per page (0 = all) */
    int         page;           /**< Current page (0-based) */
    bool        show_area;      /**< Show area column */
    bool        show_level;     /**< Show level column */
    /**
     * Custom filter callback. Return true to include the entity.
     * @param entity    Entity to check
     * @param filter    Filter argument string
     * @return true if entity should be included
     */
    bool        (*filter_fn)(void *entity, const char *filter);
    const char  *filter;        /**< Filter string passed to filter_fn */
} OLC_LIST_OPTS;

/**
 * Generic entity list renderer.
 * Displays a paginated, filterable list of entities in tabular format.
 *
 * @param ch        Character viewing the list
 * @param theme     Color theme
 * @param entities  Array of entity pointers
 * @param count     Number of entities
 * @param opts      Display options
 * @param format_fn Callback to format one entity into columns
 *                  (values array must have same count as columns)
 */
void olc_display_entity_list(CHAR_DATA *ch, const OLC_EDITOR_THEME *theme,
                             void **entities, int count,
                             const OLC_LIST_OPTS *opts,
                             void (*format_fn)(void *entity, const char **values,
                                              int max_cols));

#endif /* !def __OLC_DISPLAY_H__ */
