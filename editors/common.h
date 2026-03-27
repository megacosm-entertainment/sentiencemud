#ifndef __OLC_COMMON_H__
#define __OLC_COMMON_H__

#include "../merc.h" // For CHAR_DATA, DESCRIPTOR_DATA, etc.
#include "../olc.h"  // For olc_cmd_type, OLC_FUN

/*
 * ==========================================================================
 * OLC Editor Framework - Unified Tab and Display System
 * ==========================================================================
 */

// Maximum tabs any editor can have
#define OLC_MAX_TABS 10

// Default layout dimensions
#define OLC_DEFAULT_LABEL_WIDTH  16
#define OLC_MIN_LABEL_WIDTH      12
#define OLC_MAX_LABEL_WIDTH      24

/*
 * Tab definition for an editor
 */
typedef struct olc_tab_def {
    const char *name;        // Full tab name (e.g., "General", "Combat")
    const char *short_name;  // Short name for narrow displays (e.g., "Gen", "Cbt")
} OLC_TAB_DEF;

/*
 * Editor tab configuration - define one per editor type
 */
typedef struct olc_editor_tabs {
    int          tab_count;              // Number of tabs
    OLC_TAB_DEF  tabs[OLC_MAX_TABS];     // Tab definitions
} OLC_EDITOR_TABS;

/*
 * Layout context for rendering - tracks screen dimensions and state
 */
typedef struct olc_layout_ctx {
    CHAR_DATA   *ch;            // Character viewing
    BUFFER      *buffer;        // Output buffer
    int         screen_width;   // Detected screen width (80-160)
    int         current_tab;    // Current tab index
    int         label_width;    // Calculated label column width
    int         value_width;    // Calculated value column width
    struct olc_changeset *changeset;  // Active changeset for pending markers (may be NULL)
} OLC_LAYOUT_CTX;

/*
 * ==========================================================================
 * Layout Context Management
 * ==========================================================================
 */

// Create a new layout context with calculated dimensions
OLC_LAYOUT_CTX *olc_layout_new(CHAR_DATA *ch);

// Free layout context (also frees the buffer)
void olc_layout_free(OLC_LAYOUT_CTX *ctx);

// Initialize editor state (call when entering an editor)
// Sets up pEdit, editor type, and resets tab to 0
void olc_init_editor(CHAR_DATA *ch, int editor_type, void *pEdit);

/*
 * ==========================================================================
 * Tab System
 * ==========================================================================
 */

// Render tab bar at top of editor display
void olc_render_tabs(OLC_LAYOUT_CTX *ctx, const OLC_EDITOR_TABS *tabs);

// Process tab switch command - returns true if handled
// Handles "1", "2", "3" numeric and "tab <name>" commands
bool olc_tab_switch(CHAR_DATA *ch, const char *argument, const OLC_EDITOR_TABS *tabs);

/*
 * ==========================================================================
 * Field Renderers - Display various field types consistently
 * ==========================================================================
 */

// Simple text field with optional MXP command
void olc_render_string(OLC_LAYOUT_CTX *ctx, const char *label,
                       const char *command, const char *value);

// Integer/long value field
void olc_render_number(OLC_LAYOUT_CTX *ctx, const char *label,
                       const char *command, long value);

// Dice field (XdY+Z format)
void olc_render_dice(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, DICE_DATA *dice);

// Bit flags with MXP toggle support
void olc_render_flags(OLC_LAYOUT_CTX *ctx, const char *label,
                      const char *command, const struct flag_type *table,
                      long value);

// Type selection (radio-style from stat table)
void olc_render_type(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, const struct flag_type *table,
                     int value);

// Multi-line text/description field
void olc_render_text(OLC_LAYOUT_CTX *ctx, const char *label,
                     const char *command, const char *text);

// Section divider with title
void olc_render_section(OLC_LAYOUT_CTX *ctx, const char *title);

// Horizontal rule (no title)
void olc_render_hr(OLC_LAYOUT_CTX *ctx);

/*
 * ==========================================================================
 * Command Processing with Tab Support
 * ==========================================================================
 */

// Enhanced command processor that handles tab switching automatically
void olc_process_command_tabbed(
    CHAR_DATA *ch,
    char *argument,
    const struct olc_cmd_type olc_table[],
    const OLC_EDITOR_TABS *tabs,
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed)
);

/*
 * ==========================================================================
 * Legacy/Existing Functions
 * ==========================================================================
 */

// Generic OLC command processor
// ch: The character performing the action.
// olc_argument: The raw argument string passed to the editor function.
// olc_table: The command table specific to the current editor.
// show_func: The editor-specific function to display current status (e.g., aedit_show).
// mark_changed_func: A callback to mark the edited entity as changed.
//                    The void* pEdit parameter will be ch->desc->pEdit.
void process_olc_command(
    CHAR_DATA *ch,
    char *olc_argument,
    const struct olc_cmd_type olc_table[],
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed)
);

typedef struct olc_table_theme {
    const char *border;
    const char *title_text; // Color for the main table title
    const char *label_text;
    const char *value_text;
    const char *clickable_text; // For MXP clickable values
    const char *unset_text;     // For "(unset)" or similar
    const char *default_text;   // General reset color (e.g., "{x")
    // You could add more here, e.g., for alternating row colors, section headers etc.
} OLC_TABLE_THEME;

void olc_render_table_header(BUFFER *buffer, const char *title, const OLC_TABLE_THEME *theme);
void olc_render_field_row(CHAR_DATA *ch, BUFFER *buffer, const char *label, const char *value_str, const char *click_command, const OLC_TABLE_THEME *theme);
void olc_render_text_block_row(BUFFER *buffer, const char *label, const char *text, const OLC_TABLE_THEME *theme, CHAR_DATA *ch);
void olc_render_table_footer(BUFFER *buffer, const OLC_TABLE_THEME *theme);
void generic_olc_mark_changed(CHAR_DATA *ch, bool changed_status); // Assuming this is a general helper
void olc_select_tab(CHAR_DATA *ch, const char **tab_names, const char *argument, const char *tab_cmd);
void olc_render_tab_bar(CHAR_DATA *ch, BUFFER *buffer, const char **tab_names, const char *editor_olc_command);
void format_to_width(char *dest, const char *src, int width, bool pad_right);
const char *repeat_char(char c, int count);
const char *format_dice_string(DICE_DATA *dice);
const char *format_ac_string(MOB_INDEX_DATA *pMob);

// Screen dimension helpers
int get_olc_screen_width(CHAR_DATA *ch);
int get_olc_screen_height(CHAR_DATA *ch);

// Widevnum parsing helpers
AREA_DATA *olc_relative_widevnum_context(AREA_DATA *context_area, const char *argument);

// Script/prog display helpers
#define MAX_PROG_GROUP_TRIGGERS 64
#define MAX_PROG_GROUPS 32

typedef struct prog_group_entry {
    PROG_LIST *entry;   // The PROG_LIST from the slot
    int slot;           // Which TRIGSLOT it lives in
} PROG_GROUP_ENTRY;

typedef struct prog_group {
    long vnum;
    SCRIPT_DATA *script;
    PROG_GROUP_ENTRY triggers[MAX_PROG_GROUP_TRIGGERS];
    int trigger_count;
} PROG_GROUP;

int prog_build_groups(LLIST **progs, PROG_GROUP *groups, int max_groups, int type);
void olc_show_progs_grouped(BUFFER *buffer, LLIST **progs, int type, const char *title);
char *olc_show_script_status(SCRIPT_DATA *prog, int type);

// Flag display helpers (existing, used by olc_render_flags internally)
int olc_buffer_show_flags(CHAR_DATA *ch, BUFFER *buffer,
        const struct flag_type *flag_table,
        long value, char *command, char *heading,
        const char *colors);

int olc_buffer_show_flags_ex(CHAR_DATA *ch, BUFFER *buffer,
        const struct flag_type *flag_table,
        long value, char *command, char *heading,
        int max_width, int first_indent, int indent,
        const char *colors);

void olc_buffer_show_string(CHAR_DATA *ch, BUFFER *buffer, const char *value,
        char *command, char *heading, int indent, char *colors);

void olc_buffer_show_tabs(CHAR_DATA *ch, BUFFER *buffer, const char **tab_names);

bool olc_validate_name(CHAR_DATA *ch, const char *str);

#endif /* !def __OLC_COMMON_H__ */