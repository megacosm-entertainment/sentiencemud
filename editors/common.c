/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "../../merc.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../interp.h"
#include "../../scripts.h"
#include "../../wilds.h"

void process_olc_command(
    CHAR_DATA *ch,
    char *olc_argument, // This is the original argument string from the editor function
    const struct olc_cmd_type olc_table[],
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed)
) {
    char command[MAX_INPUT_LENGTH];
    char arg_for_interpret[MAX_STRING_LENGTH];
    char working_argument_buffer[MAX_STRING_LENGTH];
    char *current_command_args_ptr;
    void *pEdit = ch->desc->pEdit; // The actual data being edited
    int cmd_index;

    // Smash tildes in the input argument string. This modifies olc_argument directly.
    smash_tilde(olc_argument);

    // Copy the smashed argument for potential use by interpret()
    strcpy(arg_for_interpret, olc_argument);

    // Use a working buffer for one_argument, as it modifies the string it parses.
    strcpy(working_argument_buffer, olc_argument);
    current_command_args_ptr = working_argument_buffer;

    // Extract the first command word. current_command_args_ptr will point to the rest of the arguments.
    current_command_args_ptr = one_argument(current_command_args_ptr, command);

    // Handle "done" command
    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    // Update last OLC command timestamp
    if (ch->pcdata && ch->pcdata->immortal) {
        ch->pcdata->immortal->last_olc_command = current_time;
    }

    // If no command was entered, call the editor's show function
    if (command[0] == '\0') {
        if (show_func) {
            (*show_func)(ch, current_command_args_ptr); // Pass remaining arguments to show_func
        } else {
            send_to_char("No display function available for this editor.\n\r", ch);
        }
        return;
    }

    // Iterate through the OLC command table for the current editor
    for (cmd_index = 0; olc_table[cmd_index].name != NULL; cmd_index++) {
        if (!str_prefix(command, olc_table[cmd_index].name)) {
            // Execute the command function. It returns true if data was changed.
            if ((*olc_table[cmd_index].olc_fun)(ch, current_command_args_ptr)) {
                if (mark_changed_func) { // Check if a callback is provided
                    (*mark_changed_func)(ch, true); // Calls generic_olc_mark_changed
                }
            }
            }
            // Command was found and handled (or attempted), so return.
            return;
        }
    }

    // If command not found in OLC table, fall back to the main interpreter
    interpret(ch, arg_for_interpret);
}


void generic_olc_mark_changed(CHAR_DATA *ch, bool changed_status) {
    if (!changed_status || !ch || !ch->desc || !ch->desc->pEdit) {
        return;
    }

    void *pEdit = ch->desc->pEdit;

    switch (ch->desc->editor) {
        case ED_AREA:
            helper_mark_area_changed(pEdit);
            break;
        case ED_ROOM:
            helper_mark_room_changed(pEdit);
            break;
        case ED_OBJECT:
            helper_mark_object_changed(pEdit);
            break;
        case ED_MOBILE:
            helper_mark_mobile_changed(pEdit);
            break;
        case ED_TOKEN:
            helper_mark_token_changed(pEdit);
            break;
        case ED_WILDS:
            helper_mark_wilds_changed(pEdit);
            break;
        case ED_VLINK:
            helper_mark_vlink_changed(pEdit);
            break;
        case ED_PROJECT:
            helper_mark_project_changed(pEdit); // pEdit might be NULL or not directly used by this helper
            break;
        // Add other editor types as needed, dispatching to their specific helper
        default:
            bug("generic_olc_mark_changed: Unknown editor type %d for marking change.", ch->desc->editor);
            break;
    }
}