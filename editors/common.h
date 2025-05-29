#ifndef __OLC_COMMON_H__
#define __OLC_COMMON_H__

#include "../merc.h" // For CHAR_DATA, DESCRIPTOR_DATA, etc.
#include "../olc.h"  // For olc_cmd_type, OLC_FUN

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

#endif /* !def __OLC_COMMON_H__ */