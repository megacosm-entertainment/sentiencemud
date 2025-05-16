/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"
#include "scripts.h"


/* Editor functions */
GAMEEDIT(gameedit_show);
GAMEEDIT(gameedit_set);
GAMEEDIT(gameedit_confirm);
GAMEEDIT(gameedit_revert);



/* Utils */
bool gameedit_find_setting(CHAR_DATA *ch, char *name, const struct game_setting_type **setting);
void gameedit_display_setting(BUFFER *buffer, CHAR_DATA *ch, const struct game_setting_type *setting);
void gameedit_display_category(BUFFER *buffer, CHAR_DATA *ch, int category);
bool gameedit_set_value(CHAR_DATA *ch, const struct game_setting_type *setting, char *value);
bool requires_reboot(void);


/* Pending setting changes */
typedef struct game_setting_change {
    const struct game_setting_type *setting;
    char *value;
} GAME_SETTING_CHANGE;

/* Global variables */
LLIST *pending_changes = NULL; // List of GAME_SETTING_CHANGE objects
bool pending_reboot = false;   // Whether a reboot will be needed after confirmation

/* The main command */
void do_gameedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    
    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (command[0] == '\0') {
        gameedit_show(ch, argument);
        return;
    }

    /* Search for command in table */
    if (ch->pcdata->security < MIN_SECURITY_GAMEEDIT) {
        send_to_char("You do not have enough security to edit game settings.\n\r", ch);
        return;
    }

    /* Editor commands */
    if (!str_cmp(command, "show"))
        gameedit_show(ch, argument);
    else if (!str_cmp(command, "set"))
        gameedit_set(ch, argument);
    else if (!str_cmp(command, "confirm"))
        gameedit_confirm(ch, argument);
    else if (!str_cmp(command, "revert"))
        gameedit_revert(ch, argument);
    else {
        send_to_char("Syntax: gameedit show [category|setting]\n\r", ch);
        send_to_char("        gameedit set <setting> <value>\n\r", ch);
        send_to_char("        gameedit confirm\n\r", ch);
        send_to_char("        gameedit revert\n\r", ch);
    }

    return;
}

/*
 * Lists all settings or a specific category of settings
 */
GAMEEDIT(gameedit_show)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    bool found = false;
    int i;
    const struct game_setting_type *setting;

    buffer = new_buf();

    if (argument[0] == '\0') {
        /* Show all categories */
        for (i = 0; i < SETTING_CAT_MAX; i++) {
            gameedit_display_category(buffer, ch, i);
        }
    }
    else {
        /* Check if it's a category name */
        for (i = 0; i < SETTING_CAT_MAX; i++) {
            if (!str_prefix(argument, setting_category_names[i])) {
                gameedit_display_category(buffer, ch, i);
                found = true;
                break;
            }
        }
        /* If not a category, look for settings matching the prefix */
        if (!found) {
            int match_count = 0, last_match = -1;
            for (i = 0; game_settings_table[i].name != NULL; i++) {
                if (!str_prefix(argument, game_settings_table[i].name)) {
                    match_count++;
                    last_match = i;
                }
            }

if (match_count == 1) {
    setting = &game_settings_table[last_match];
    char formatted_value[MAX_STRING_LENGTH];
    char pending_value[MAX_STRING_LENGTH] = "";
    char desc_line[80];
    int desc_len;

// Top border
snprintf(buf, sizeof(buf), "{Y+----------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

// Setting name, centered
int name_len = strlen_no_colours(setting->name);
int name_pad = (78 - name_len) / 2;
snprintf(buf, sizeof(buf), "{Y|%*s%-*.*s%*s|{x\n\r", name_pad, "", name_len, name_len, setting->name, 78 - name_pad - name_len, "");
add_buf(buffer, buf);

// Info section (2 columns)
snprintf(buf, sizeof(buf), "{Y+--------------------+---------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

// Fixed width for both columns, with proper padding and truncation
snprintf(buf, sizeof(buf), "{Y| %-18.18s | %-55.55s |{x\n\r", "Category", setting_category_names[setting->category]);
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-18.18s | %-55.55s |{x\n\r", "Type", setting_type_names[setting->type]);
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-18.18s | %-55.55s |{x\n\r", "OLC Settable", setting->olc_settable ? "Yes" : "No");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-18.18s | %-55.55s |{x\n\r", "Requires Reboot", setting->requires_reboot ? "Yes" : "No");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-18.18s | %-55.55s |{x\n\r", "Sensitive", setting->sensitive ? "Yes" : "No");
add_buf(buffer, buf);

// Section divider
snprintf(buf, sizeof(buf), "{Y+--------------------+---------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

// Description (wrap to 76 chars per line)
const char *desc = setting->help;
while (*desc) {
    for (desc_len = 0; desc_len < 76 && desc[desc_len] && desc[desc_len] != '\n'; ++desc_len);
    strncpy(desc_line, desc, desc_len);
    desc_line[desc_len] = '\0';
    snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", desc_line);
    add_buf(buffer, buf);
    desc += desc_len;
    if (*desc == '\n') ++desc;
}

// Section divider
snprintf(buf, sizeof(buf), "{Y+----------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

// Current Value - ensure proper padding
if (setting->sensitive && ch->pcdata->security < 10) {
    snprintf(formatted_value, sizeof(formatted_value), "{D*****{x");
} else {
    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            snprintf(formatted_value, sizeof(formatted_value), "%s", *(bool *)setting->ptr ? "{Gtrue{x" : "{Rfalse{x");
            break;
        case SETTING_TYPE_INT:
            snprintf(formatted_value, sizeof(formatted_value), "{Y%d{x", *(int *)setting->ptr);
            break;
        case SETTING_TYPE_STRING:
            if (*(char **)setting->ptr && **(char **)setting->ptr)
                snprintf(formatted_value, sizeof(formatted_value), "{W%s{x", *(char **)setting->ptr);
            else
                snprintf(formatted_value, sizeof(formatted_value), "{D(empty){x");
            break;
        default:
            snprintf(formatted_value, sizeof(formatted_value), "{D(unknown type){x");
            break;
    }
}

// Calculate visible length for proper padding
int val_vis_len = strlen_no_colours(formatted_value);
int val_pad = 61 - val_vis_len;
if (val_pad < 0) val_pad = 0;
snprintf(buf, sizeof(buf), "{Y| Current Value: %-61.61s|{x\n\r", formatted_value);
add_buf(buffer, buf);

// Pending Value (if any)
ITERATOR it;
GAME_SETTING_CHANGE *change;
iterator_start(&it, pending_changes);
while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
    if (change->setting == setting) {
        if (setting->sensitive && ch->pcdata->security < 10) {
            snprintf(pending_value, sizeof(pending_value), "{D*****{x");
        } else {
            snprintf(pending_value, sizeof(pending_value), "{Y%s{x", change->value);
        }
        val_vis_len = strlen_no_colours(pending_value);
        val_pad = 61 - val_vis_len;
        if (val_pad < 0) val_pad = 0;
        snprintf(buf, sizeof(buf), "{Y| Pending Value: %-61.61s|{x\n\r", pending_value);
        add_buf(buffer, buf);
        break;
    }
}
iterator_stop(&it);

// Section divider
snprintf(buf, sizeof(buf), "{Y+----------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

// Usage/help - properly calculate padding for each type's format string
if (setting->olc_settable) {
    char usage[MAX_STRING_LENGTH];
    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s [true|false|yes|no|on|off|1|0]{x", setting->name);
            break;
        case SETTING_TYPE_INT:
            snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s <number>{x", setting->name);
            break;
        case SETTING_TYPE_STRING:
            snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s <text>{x", setting->name);
            break;
        default:
            snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s <value>{x", setting->name);
            break;
    }
    int usage_len = strlen_no_colours(usage);
    int usage_pad = 76 - usage_len;
    if (usage_pad < 0) usage_pad = 0;
    snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", usage);
    add_buf(buffer, buf);
}

// Bottom border
snprintf(buf, sizeof(buf), "{Y+----------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

    found = true;
} else if (match_count > 1) {
                // Show a table of all matches
                bool any = FALSE;
sprintf(buf, "{Y+------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);
sprintf(buf, "{Y| %-23s| %-38s| %-12s|{x\n\r", "Setting Name", "Value", "Type");
add_buf(buffer, buf);
sprintf(buf, "{Y+------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);

                for (i = 0; game_settings_table[i].name != NULL; i++) {
                    if (!str_prefix(argument, game_settings_table[i].name)) {
                        gameedit_display_setting(buffer, ch, &game_settings_table[i]);
                        any = TRUE;
                    }
                }

                if (!any) {
                    sprintf(buf, "{Y║ %-76s║{x\n\r", "No matching settings found.");
                    add_buf(buffer, buf);
                }

                // Table footer
sprintf(buf, "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);

                // Legend
sprintf(buf, "  Legend: {R*{x Requires reboot   {MS{x Sensitive setting   {DX{x Not settable via OLC\n\r\n\r");
add_buf(buffer, buf);

                found = true;
            }
        }

        /* If not found, try partial match for all settings */
        if (!found) {
            bool any = FALSE;
            // Table header
snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-23s | %-38s | %-11s |{x\n\r", "Setting Name", "Value", "Type");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);

            for (i = 0; game_settings_table[i].name != NULL; i++) {
                if (!str_prefix(argument, game_settings_table[i].name)) {
                    gameedit_display_setting(buffer, ch, &game_settings_table[i]);
                    any = TRUE;
                }
            }

            if (!any) {
                sprintf(buf, "{Y║ %-76s║{x\n\r", "No matching settings found.");
                add_buf(buffer, buf);
            }

            // Table footer
            sprintf(buf, "{Y╚════════════════════════╩═══════════════════════════════════════╩═════════════╝{x\n\r");
            add_buf(buffer, buf);

            // Legend
            sprintf(buf, "  Legend: {R*{x Requires reboot   {M§{x Sensitive setting   {D✘{x Not settable via OLC\n\r\n\r");
            add_buf(buffer, buf);
        }
    }

if (list_size(pending_changes) > 0) {
    sprintf(buf, "\n\r{Y+----------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    // Center title with proper padding
    sprintf(buf, "{Y| {RPending Changes{x%-60s |{x\n\r", "");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    // Ensure consistent padding for each line
    char msg[MAX_STRING_LENGTH];
    int msg_len, msg_pad;
    
    sprintf(msg, "There are %d pending changes that need to be confirmed.", list_size(pending_changes));
    msg_len = strlen(msg);
    msg_pad = 76 - msg_len;
    if (msg_pad < 0) msg_pad = 0;
    snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", msg);
    add_buf(buffer, buf);
    
    sprintf(msg, "Use '{Wgameedit confirm{x' to apply changes");
    msg_len = strlen_no_colours(msg);
    msg_pad = 76 - msg_len;
    if (msg_pad < 0) msg_pad = 0;
    snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", msg);
    add_buf(buffer, buf);
    
    if (requires_reboot()) {
        sprintf(msg, "{RWARNING: Some changes require a reboot to take effect.{x");
        msg_len = strlen_no_colours(msg);
        msg_pad = 76 - msg_len;
        if (msg_pad < 0) msg_pad = 0;
        snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", msg);
        add_buf(buffer, buf);
        pending_reboot = true;
    }
    
    sprintf(msg, "Use '{Wgameedit revert{x' to discard all pending changes");
    msg_len = strlen_no_colours(msg);
    msg_pad = 76 - msg_len;
    if (msg_pad < 0) msg_pad = 0;
    snprintf(buf, sizeof(buf), "{Y| %-76.76s |{x\n\r", msg);
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
}

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return FALSE;
}

/*
 * Set a setting value
 */
GAMEEDIT(gameedit_set)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    const struct game_setting_type *setting;
    
    argument = one_argument(argument, arg1);
    
    if (arg1[0] == '\0') {
        send_to_char("Syntax: gameedit set <setting> <value>\n\r", ch);
        return FALSE;
    }
    
    if (!gameedit_find_setting(ch, arg1, &setting)) {
        send_to_char("No such setting found.\n\r", ch);
        return FALSE;
    }
    
    /* Check if setting is OLC settable */
    if (!setting->olc_settable) {
        send_to_char("This setting cannot be changed through OLC.\n\r", ch);
        return FALSE;
    }
    
    /* Get the value argument */
    if (setting->type == SETTING_TYPE_BOOL) {
        /* For boolean values, allow omitting the value to toggle */
        if (argument[0] == '\0') {
            bool current = *(bool *)setting->ptr;
            sprintf(arg2, "%s", !current ? "true" : "false");
        } else {
            strcpy(arg2, argument);
        }
    } else {
        if (argument[0] == '\0') {
            send_to_char("Syntax: gameedit set <setting> <value>\n\r", ch);
            return FALSE;
        }
        strcpy(arg2, argument);
    }
    
    return gameedit_set_value(ch, setting, arg2);
}

/*
 * Confirm all pending changes
 */
GAMEEDIT(gameedit_confirm)
{
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    int count = 0;
    bool reboot_needed = false;
    
    if (list_size(pending_changes) == 0) {
        send_to_char("There are no pending changes to confirm.\n\r", ch);
        return FALSE;
    }
    
    /* Apply all pending changes */
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        count++;
        
        /* Apply the change based on the setting type */
        switch (change->setting->type) {
            case SETTING_TYPE_BOOL:
                *(bool *)change->setting->ptr = (!str_cmp(change->value, "true") || 
                                                !str_cmp(change->value, "yes") || 
                                                !str_cmp(change->value, "on") || 
                                                !str_cmp(change->value, "1"));
                break;
                
            case SETTING_TYPE_INT:
                *(int *)change->setting->ptr = atoi(change->value);
                break;
                
            case SETTING_TYPE_STRING:
                free_string(*(char **)change->setting->ptr);
                *(char **)change->setting->ptr = str_dup(change->value);
                break;
        }
        
        if (change->setting->requires_reboot)
            reboot_needed = true;
    }
    iterator_stop(&it);
    
    /* Clear the pending changes */
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        free_string(change->value);
        free_mem(change, sizeof(GAME_SETTING_CHANGE));
    }
    iterator_stop(&it);
    list_clear(pending_changes);
    
    send_to_char(formatf("Applied %d setting changes.\n\r", count), ch);
    
    /* Save the settings using the existing function */
    game_settings_write();
    send_to_char("Game settings saved.\n\r", ch);
    
    /* If a reboot is needed, prompt the user */
    if (reboot_needed) {
        send_to_char("{RChanged settings require a reboot to take full effect.{x\n\r", ch);
        send_to_char("Type '{Wreboot now{x' to reboot the server immediately.\n\r", ch);
    }
    
    pending_reboot = false;
    return TRUE;
}

/*
 * Revert all pending changes
 */
GAMEEDIT(gameedit_revert)
{
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    int count = 0;
    
    if (list_size(pending_changes) == 0) {
        send_to_char("There are no pending changes to revert.\n\r", ch);
        return FALSE;
    }
    
    /* Count and clear the pending changes */
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        count++;
        free_string(change->value);
        free_mem(change, sizeof(GAME_SETTING_CHANGE));
    }
    iterator_stop(&it);
    list_clear(pending_changes);
    
    send_to_char(formatf("Reverted %d pending setting changes.\n\r", count), ch);
    pending_reboot = false;
    return TRUE;
}

/*
 * Find a setting by name
 */
bool gameedit_find_setting(CHAR_DATA *ch, char *name, const struct game_setting_type **setting)
{
    int i;
    
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        if (!str_prefix(name, game_settings_table[i].name)) {
            *setting = &game_settings_table[i];
            return TRUE;
        }
    }
    
    return FALSE;
}

void gameedit_display_setting(BUFFER *buffer, CHAR_DATA *ch, const struct game_setting_type *setting)
{
    char buf[MAX_STRING_LENGTH];
    char value_str[MAX_STRING_LENGTH] = "";
    char type_str[MAX_STRING_LENGTH] = "";
    bool has_change = FALSE;
    
    // 1. FORMAT THE VALUE COLUMN
    if (setting->sensitive && ch->pcdata->security < 10) {
        strcpy(value_str, "{D*****{x");
    } else {
        switch (setting->type) {
            case SETTING_TYPE_BOOL:
                strcpy(value_str, *(bool *)setting->ptr ? "{Gtrue{x" : "{Rfalse{x");
                break;
            case SETTING_TYPE_INT:
                sprintf(value_str, "{Y%d{x", *(int *)setting->ptr);
                break;
            case SETTING_TYPE_STRING:
                if (*(char **)setting->ptr && **(char **)setting->ptr)
                    sprintf(value_str, "{W%s{x", *(char **)setting->ptr);
                else
                    strcpy(value_str, "{D(empty){x");
                break;
            default:
                strcpy(value_str, "{D(unknown){x");
                break;
        }
    }
    
    // Check for pending change
    if (pending_changes && list_size(pending_changes) > 0) {
        ITERATOR it;
        GAME_SETTING_CHANGE *change;
        iterator_start(&it, pending_changes);
        while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
            if (change->setting == setting) {
                has_change = TRUE;
                char old_value[MAX_STRING_LENGTH];
                strcpy(old_value, value_str);
                
                if (setting->sensitive && ch->pcdata->security < 10) {
                    sprintf(value_str, "%s → {Y*****{x", old_value);
                } else {
                    sprintf(value_str, "%s → {Y%s{x", old_value, change->value);
                }
                break;
            }
        }
        iterator_stop(&it);
    }
    
    // 2. FORMAT THE TYPE COLUMN WITH FLAGS
    const char *type_name = "";
    switch (setting->type) {
        case SETTING_TYPE_BOOL:   type_name = "Boolean"; break;
        case SETTING_TYPE_INT:    type_name = "Integer"; break;
        case SETTING_TYPE_STRING: type_name = "String";  break;
        default:                  type_name = "Unknown"; break;
    }
    
    // Add colored type name and flags
    sprintf(type_str, "{B%s{x", type_name);
    
    // Add flags at the end
    if (setting->requires_reboot) strcat(type_str, "*");
    if (setting->sensitive) strcat(type_str, "S");
    if (!setting->olc_settable) strcat(type_str, "X");
    
    // 3. HANDLE VALUE TRUNCATION (if needed)
    char clean_value[MAX_STRING_LENGTH];
    int max_vis_len = 36;
    int actual_vis_len = strlen_no_colours(value_str);
    
    if (actual_vis_len <= max_vis_len) {
        strcpy(clean_value, value_str);
        // Pad with spaces to exact width
        int pad = max_vis_len - actual_vis_len;
        while (pad-- > 0) strcat(clean_value, " ");
    } else {
        // Truncate to max_vis_len - 5 for ellipsis
        int trunc_len = colour_trunc_len(value_str, max_vis_len - 5);
        strncpy(clean_value, value_str, trunc_len);
        clean_value[trunc_len] = '\0';
        
        // Fix truncated color codes
        int i = strlen(clean_value) - 1;
        while (i >= 0 && clean_value[i] == '{') {
            clean_value[i] = '\0';
            i--;
        }
        
        strcat(clean_value, ". . .");
        
        // Pad with spaces to exact width
        int pad = max_vis_len - strlen_no_colours(clean_value);
        while (pad-- > 0) strcat(clean_value, " ");
    }
    
    // Replace newlines with spaces
    for (int k = 0; clean_value[k]; ++k) {
        if (clean_value[k] == '\n' || clean_value[k] == '\r')
            clean_value[k] = ' ';
    }
    
    // 4. OUTPUT THE ROW WITH EXACT WIDTHS
snprintf(buf, sizeof(buf), 
    "{Y| %-23.23s | %-35.35s | %-11.11s |{x\n\r",
    setting->name, clean_value, type_str);
    
    add_buf(buffer, buf);
}

/*
 * Display a category of settings
 */
void gameedit_display_category(BUFFER *buffer, CHAR_DATA *ch, int category)
{
    char buf[MAX_STRING_LENGTH];
    int i;
    bool found = FALSE;
    
snprintf(buf, sizeof(buf), "{Y+-----------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);
    

int cat_len = strlen_no_colours(setting_category_names[category]);
int cat_pad = (78 - cat_len) / 2;
snprintf(buf, sizeof(buf), "{Y|%*s%-*.*s%*s|{x\n\r", cat_pad, "", cat_len, cat_len, setting_category_names[category], 78 - cat_pad - cat_len, "");add_buf(buffer, buf);
    //sprintf(buf, "{Y║ %-76s ║{x\n\r", setting_category_names[category]);
    //add_buf(buffer, buf);
    
snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y| %-23s | %-35s | %-11s |{x\n\r", "Setting Name", "Value", "Type");
add_buf(buffer, buf);
snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);
    
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        if (game_settings_table[i].category == category) {
            gameedit_display_setting(buffer, ch, &game_settings_table[i]);
            found = TRUE;
        }
    }
    
    if (!found) {
sprintf(buf, "{Y|{x  %-76s  {Y|{x\n\r", "No settings in this category.");
add_buf(buffer, buf);
    }
    
sprintf(buf, "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
add_buf(buffer, buf);
    
    /* Add a legend for the symbols */
sprintf(buf, "  Legend: {R*{x Requires reboot   {MS{x Sensitive setting   {DX{x Not settable via OLC\n\r\n\r");
add_buf(buffer, buf);
}
/*
 * Set a setting value (adds to pending changes)
 */
bool gameedit_set_value(CHAR_DATA *ch, const struct game_setting_type *setting, char *value)
{
    GAME_SETTING_CHANGE *change;
    ITERATOR it;
    
    /* Validate the value based on the setting type */
    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            if (str_cmp(value, "true") && str_cmp(value, "false") &&
                str_cmp(value, "yes") && str_cmp(value, "no") &&
                str_cmp(value, "on") && str_cmp(value, "off") &&
                str_cmp(value, "1") && str_cmp(value, "0")) {
                send_to_char("Boolean settings must be true/false, yes/no, on/off, or 1/0.\n\r", ch);
                return FALSE;
            }
            break;
            
        case SETTING_TYPE_INT:
            if (!is_number(value)) {
                send_to_char("This setting requires a numeric value.\n\r", ch);
                return FALSE;
            }
            break;
            
        case SETTING_TYPE_STRING:
            /* No validation needed for string types */
            break;
    }
    
    /* Initialize pending changes list if needed */
    if (!pending_changes) {
        pending_changes = list_create(false);
    }
    
    /* Check if we already have a pending change for this setting */
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        if (change->setting == setting) {
            /* Update the existing change */
            free_string(change->value);
            change->value = str_dup(value);
            iterator_stop(&it);
            
            send_to_char("Setting updated in pending changes.\n\r", ch);
            return TRUE;
        }
    }
    iterator_stop(&it);
    
    /* Create a new pending change */
    change = (GAME_SETTING_CHANGE *)alloc_mem(sizeof(GAME_SETTING_CHANGE));
    change->setting = setting;
    change->value = str_dup(value);
    list_appendlink(pending_changes, change);
    
    send_to_char("Setting added to pending changes.\n\r", ch);
    
    if (setting->requires_reboot) {
        send_to_char("{RNote: This setting requires a reboot to take effect.{x\n\r", ch);
        pending_reboot = true;
    }
    
    return TRUE;
}

/*
 * Check if any pending changes require a reboot
 */
bool requires_reboot(void)
{
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    
    if (!pending_changes || list_size(pending_changes) == 0)
        return FALSE;
    
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        if (change->setting->requires_reboot) {
            iterator_stop(&it);
            return TRUE;
        }
    }
    iterator_stop(&it);
    
    return FALSE;
}
