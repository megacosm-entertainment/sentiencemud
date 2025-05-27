#ifndef EDITOR_COMMON_H
#define EDITOR_COMMON_H

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

#endif // EDITOR_COMMON_H