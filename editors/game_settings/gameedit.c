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
#include "../../merc.h"
#include "../../interp.h"
#include "../../tables.h"
#include "../../olc.h"
#include "../../recycle.h"
#include "../../scripts.h"


/* Editor functions */
GAMEEDIT(gameedit_show);
GAMEEDIT(gameedit_set);
GAMEEDIT(gameedit_confirm);
GAMEEDIT(gameedit_revert);
GAMEEDIT(gameedit_history);
GAMEEDIT(gameedit_view);
GAMEEDIT(gameedit_rollback);
GAMEEDIT(gameedit_comment);
GAMEEDIT(gameedit_pending);





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
int next_changeset_id = 1;           // Next ID to assign
int changeset_count = 0;             // Number of stored changesets
GAME_SETTINGS_CHANGESET *changesets[MAX_CHANGESETS]; // Array of changesets

/* Global variables */
extern LLIST *pending_changes; // List of GAME_SETTING_CHANGE objects
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
    else if (!str_cmp(command, "history"))
        gameedit_history(ch, argument);
    else if (!str_cmp(command, "view"))
        gameedit_view(ch, argument);
    else if (!str_cmp(command, "rollback"))
        gameedit_rollback(ch, argument);
    else if (!str_cmp(command, "comment"))
        gameedit_comment(ch, argument);
    else if (!str_cmp(command, "pending")) 
        gameedit_pending(ch, argument);
    else {
        send_to_char("Syntax: gameedit show [category|setting]\n\r", ch);
        send_to_char("        gameedit set <setting> <value>\n\r", ch);
        send_to_char("        gameedit confirm\n\r", ch);
        send_to_char("        gameedit revert\n\r", ch);
        send_to_char("        gameedit pending\n\r", ch);
        send_to_char("        gameedit history [limit]\n\r", ch);
        send_to_char("        gameedit view <changeset_id>\n\r", ch);
        send_to_char("        gameedit rollback <changeset_id>\n\r", ch);
        send_to_char("        gameedit comment <changeset_id>\n\r", ch);
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
        // Show all categories
        for (i = 0; i < SETTING_CAT_MAX; i++) {
            gameedit_display_category(buffer, ch, i);
        }
    } else {
        // Check if it's a category name
        for (i = 0; i < SETTING_CAT_MAX; i++) {
            if (!str_prefix(argument, setting_category_names[i])) {
                gameedit_display_category(buffer, ch, i);
                found = true;
                break;
            }
        }

        // If not a category, look for settings matching the prefix
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
    char desc_line[81];
    int desc_len;

    // Top border (80 columns)
    snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    // Setting name, centered (80 columns)
    int name_len = strlen_no_colours(setting->name);
    int name_pad = (78 - name_len) / 2;
    snprintf(buf, sizeof(buf), "{Y|%*s%-*.*s%*s|{x\n\r", name_pad, "", name_len, name_len, setting->name, 78 - name_pad - name_len, "");
    add_buf(buffer, buf);

    // Info section (2 columns, 20+59+1=80)
    snprintf(buf, sizeof(buf), "{Y+--------------------+---------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    snprintf(buf, sizeof(buf), "{Y| %-20.20s| %-55.55s|{x\n\r", "Category", setting_category_names[setting->category]);
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf), "{Y| %-20.20s| %-55.55s|{x\n\r", "Type", setting_type_names[setting->type]);
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf), "{Y| %-20.20s| %-55.55s|{x\n\r", "OLC Settable", setting->olc_settable ? "Yes" : "No");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf), "{Y| %-20.20s| %-55.55s|{x\n\r", "Requires Reboot", setting->requires_reboot ? "Yes" : "No");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf), "{Y| %-20.20s| %-55.55s|{x\n\r", "Sensitive", setting->sensitive ? "Yes" : "No");
    add_buf(buffer, buf);

    // Section divider (80 columns)
    snprintf(buf, sizeof(buf), "{Y+--------------------+---------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    // Description (wrap to 78 chars per line, 80 with borders)
    const char *desc = setting->help;
    while (*desc) {
        for (desc_len = 0; desc_len < 77 && desc[desc_len] && desc[desc_len] != '\n'; ++desc_len);
        strncpy(desc_line, desc, desc_len);
        desc_line[desc_len] = '\0';
        snprintf(buf, sizeof(buf), "{Y| %-77.77s|{x\n\r", desc_line);
        add_buf(buffer, buf);
        desc += desc_len;
        if (*desc == '\n') ++desc;
    }

    // Section divider (80 columns)
    snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    // Current Value (pad/truncate to 65 chars, 80 with border and label)
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
            case SETTING_TYPE_EXTSTR:
                if (*(char **)setting->ptr && **(char **)setting->ptr) {
                    char preview[200];
                    strncpy(preview, *(char **)setting->ptr, 180);
                    preview[180] = '\0';
                    // Replace newlines with spaces for display
                    for (int k = 0; preview[k]; k++)
                        if (preview[k] == '\n' || preview[k] == '\r')
                            preview[k] = ' ';
                    if (strlen(*(char **)setting->ptr) > 180)
                        strcat(preview, "...");
                    snprintf(formatted_value, sizeof(formatted_value), "{W%s{x", preview);
                } else
                    snprintf(formatted_value, sizeof(formatted_value), "{D(empty){x");
                break;
            case SETTING_TYPE_FLOAT:
                snprintf(formatted_value, sizeof(formatted_value), "{Y%.4f{x", *(float *)setting->ptr);
                break;
            default:
                snprintf(formatted_value, sizeof(formatted_value), "{D(unknown type){x");
                break;
        }
    }
    // Pad/truncate to 65 visible chars
    char display_value[256];
    int max_vis_len = 66;
    int vis_len = strlen_no_colours(formatted_value);
    if (vis_len > max_vis_len) {
        int trunc_len = colour_trunc_len(formatted_value, max_vis_len - 3);
        strncpy(display_value, formatted_value, trunc_len);
        display_value[trunc_len] = '\0';
        strcat(display_value, "{x...");
    } else {
        strcpy(display_value, formatted_value);
        int pad = max_vis_len - vis_len;
        while (pad-- > 0) strcat(display_value, " ");
    }
    for (int k = 0; display_value[k]; ++k)
        if (display_value[k] == '\n' || display_value[k] == '\r')
            display_value[k] = ' ';
    snprintf(buf, sizeof(buf), "{Y| Current Value: %-66.66s{Y|{x\n\r", display_value);
    add_buf(buffer, buf);

    // Pending Value (if any, pad/truncate to 65 chars)
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
            int vis_len = strlen_no_colours(pending_value);
            if (vis_len > max_vis_len) {
                int trunc_len = colour_trunc_len(pending_value, max_vis_len - 3);
                strncpy(display_value, pending_value, trunc_len);
                display_value[trunc_len] = '\0';
                strcat(display_value, "{x...");
            } else {
                strcpy(display_value, pending_value);
                int pad = max_vis_len - vis_len;
                while (pad-- > 0) strcat(display_value, " ");
            }
            for (int k = 0; display_value[k]; ++k)
                if (display_value[k] == '\n' || display_value[k] == '\r')
                    display_value[k] = ' ';
            snprintf(buf, sizeof(buf), "{Y| Pending Value: %-66.66s{Y|{x\n\r", display_value);
            add_buf(buffer, buf);
            break;
        }
    }
    iterator_stop(&it);

    // Section divider (80 columns)
    snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    // Usage/help (wrap to 78 chars, 80 with borders)
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
            case SETTING_TYPE_EXTSTR:
                snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s edit{x (opens string editor)", setting->name);
                break;
            case SETTING_TYPE_FLOAT:
                snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s <number.decimal>{x", setting->name);
                break;
            default:
                snprintf(usage, sizeof(usage), "Usage: {Wgameedit set %s <value>{x", setting->name);
                break;
        }
        int usage_len = strlen_no_colours(usage);
        int usage_pad = 81 - usage_len;
        if (usage_pad < 0) usage_pad = 0;
        snprintf(buf, sizeof(buf), "{Y| %-81.81s{Y|{x\n\r", usage);
        add_buf(buffer, buf);
    }

    // Bottom border (80 columns)
    snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);

    found = true;
}
        }

        // If still not found, try partial match for all settings
        if (!found) {
            bool any = FALSE;
            snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
            add_buf(buffer, buf);
            snprintf(buf, sizeof(buf), "{Y| %-23s | %-36s | %-10s |{x\n\r", "Setting Name", "Value", "Type");
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
                snprintf(buf, sizeof(buf), "{Y| %-75s {Y|{x\n\r", "No matching settings found.");
                add_buf(buffer, buf);
            }

            // Table footer
            snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
            add_buf(buffer, buf);

            // Legend
            snprintf(buf, sizeof(buf), "  Legend: {R*{x Requires reboot   {MS{x Sensitive setting   {DX{x Not settable via OLC\n\r\n\r");
            add_buf(buffer, buf);
        }
    }

    // Show pending changes if any
    if (list_size(pending_changes) > 0) {
snprintf(buf, sizeof(buf), "\n\r{Y+------------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

snprintf(buf, sizeof(buf), "{Y| {RPending Changes{x%-62s{Y|{x\n\r", "");
add_buf(buffer, buf);

snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);

char msg[MAX_STRING_LENGTH];
int msg_len, msg_pad;

snprintf(msg, sizeof(msg), "There are %d pending changes that need to be confirmed.", list_size(pending_changes));
msg_len = strlen_no_colours(msg);
msg_pad = 77 - msg_len;
if (msg_pad < 0) msg_pad = 0;
snprintf(buf, sizeof(buf), "{Y| %-*.*s|{x\n\r", 77, 77, msg);
add_buf(buffer, buf);

snprintf(msg, sizeof(msg), "Use '{Wgameedit confirm{Y' to apply changes");
msg_len = strlen_no_colours(msg);
msg_pad = 81 - msg_len;
if (msg_pad < 0) msg_pad = 0;
snprintf(buf, sizeof(buf), "{Y| %-*.*s|{x\n\r", 81, 81, msg);
add_buf(buffer, buf);

if (requires_reboot()) {
    snprintf(msg, sizeof(msg), "{RWARNING: Some changes require a reboot to take effect.{x");
    msg_len = strlen_no_colours(msg);
    msg_pad = 81 - msg_len;
    if (msg_pad < 0) msg_pad = 0;
    snprintf(buf, sizeof(buf), "{Y| %-*.*s{Y|{x\n\r", 81, 81, msg);
    add_buf(buffer, buf);
}

snprintf(msg, sizeof(msg), "Use '{Wgameedit revert{Y' to discard all pending changes");
msg_len = strlen_no_colours(msg);
msg_pad = 81 - msg_len;
if (msg_pad < 0) msg_pad = 0;
snprintf(buf, sizeof(buf), "{Y| %-*.*s{Y|{x\n\r", 81, 81, msg);
add_buf(buffer, buf);

snprintf(buf, sizeof(buf), "{Y+------------------------------------------------------------------------------+{x\n\r");
add_buf(buffer, buf);
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return FALSE;
}

// Modify the do_gameedit function (in the 'set' section) to handle EXTSTR edit case
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
    } 
    else if (setting->type == SETTING_TYPE_EXTSTR) {
        /* For extended strings, check if they want to edit */
        if (argument[0] == '\0' || !str_cmp(argument, "edit")) {
            strcpy(arg2, "edit");
        } else {
            strcpy(arg2, argument);
        }
    }
    else {
        if (argument[0] == '\0') {
            send_to_char("Syntax: gameedit set <setting> <value>\n\r", ch);
            return FALSE;
        }
        strcpy(arg2, argument);
    }
    
    return gameedit_set_value(ch, setting, arg2);
}

// Update the confirm function to handle the new types
GAMEEDIT(gameedit_confirm)
{
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    int count = 0;
    bool reboot_needed = false;
    char comment[MAX_STRING_LENGTH];
    
    if (list_size(pending_changes) == 0) {
        send_to_char("There are no pending changes to confirm.\n\r", ch);
        return FALSE;
    }
    
    /* Extract optional comment */
    if (argument[0] != '\0') {
        strcpy(comment, argument);
    } else {
        strcpy(comment, "No comment provided");
    }
    
    /* Create a new changeset */
    create_changeset(ch, comment);
    
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
            case SETTING_TYPE_EXTSTR:
                free_string(*(char **)change->setting->ptr);
                *(char **)change->setting->ptr = str_dup(change->value);
                break;
                
            case SETTING_TYPE_FLOAT:
                *(float *)change->setting->ptr = atof(change->value);
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
    
    /* Save the settings and changesets */
    game_settings_write();
    save_changesets();
    
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

bool gameedit_find_setting(CHAR_DATA *ch, char *name, const struct game_setting_type **setting)
{
    int i;
    
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        /* First try exact match */
        if (!str_cmp(name, game_settings_table[i].name)) {
            *setting = &game_settings_table[i];
            return TRUE;
        }
    }
    
    /* If exact match fails, try prefix match */
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        if (!str_prefix(name, game_settings_table[i].name)) {
            *setting = &game_settings_table[i];
            return TRUE;
        }
    }
    
    return FALSE;
}

/*
 * Find a setting by name
 */
void gameedit_display_setting(BUFFER *buffer, CHAR_DATA *ch, const struct game_setting_type *setting)
{
    char buf[MAX_STRING_LENGTH];
    char value_str[2048] = "";
    char type_str[32] = "";

    // Format value
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
            case SETTING_TYPE_EXTSTR:
                if (*(char **)setting->ptr && **(char **)setting->ptr) {
                    char preview[50];
                    strncpy(preview, *(char **)setting->ptr, 45);
                    preview[45] = '\0';
                    if (strlen(*(char **)setting->ptr) > 45)
                        strcat(preview, "...");
                    sprintf(value_str, "{W%s{x", preview);
                } else
                    strcpy(value_str, "{D(empty){x");
                break;
            case SETTING_TYPE_FLOAT:
                sprintf(value_str, "{Y%.4f{x", *(float *)setting->ptr);
                break;
            default:
                strcpy(value_str, "{D(unknown){x");
                break;
        }
    }

    // Pending change indicator
    if (pending_changes && list_size(pending_changes) > 0) {
        ITERATOR it;
        GAME_SETTING_CHANGE *change;
        iterator_start(&it, pending_changes);
        while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
            if (change->setting == setting) {
                char old_value[1024];
                strcpy(old_value, value_str);
                if (setting->sensitive && ch->pcdata->security < 10) {
                    snprintf(value_str, sizeof(value_str), "%s → {Y*****{x", old_value);
                } else {
                    snprintf(value_str, sizeof(value_str), "%s → {Y%s{x", old_value, change->value);
                }
                break;
            }
        }
        iterator_stop(&it);
    }

// Format type column with modifiers
const char *type_name = "";
switch (setting->type) {
    case SETTING_TYPE_BOOL:   type_name = "Boolean"; break;
    case SETTING_TYPE_INT:    type_name = "Integer"; break;
    case SETTING_TYPE_STRING: type_name = "String";  break;
    case SETTING_TYPE_EXTSTR: type_name = "ExtStr"; break;
    case SETTING_TYPE_FLOAT:  type_name = "Float";   break;
    default:                  type_name = "Unknown"; break;
}
char modifiers[16] = "";
if (setting->requires_reboot) strcat(modifiers, "{R*{x");
if (setting->sensitive) strcat(modifiers, "{MS{x");
if (!setting->olc_settable) strcat(modifiers, "{DX{x");

// Calculate visible lengths
int type_vis = strlen_no_colours(type_name);
int mod_vis = strlen_no_colours(modifiers);
int pad_middle = 14 - type_vis - mod_vis;
if (pad_middle < 0) pad_middle = 0;

// Build the type string: type name left, modifiers right
snprintf(type_str, sizeof(type_str), "{B%s{x%*s%s", type_name, pad_middle, "", modifiers);

// Truncate and pad value for display (39 visible chars for 80-column table)
char display_value[256];
int max_vis_len = 40;
int vis_len = strlen_no_colours(value_str);
if (vis_len > max_vis_len) {
    int trunc_len = colour_trunc_len(value_str, max_vis_len - 3);
    strncpy(display_value, value_str, trunc_len);
    display_value[trunc_len] = '\0';
    strcat(display_value, "{x...");
} else {
    strcpy(display_value, value_str);
    // Pad with spaces to align type column if needed
    int pad = max_vis_len - vis_len;
    while (pad-- > 0) strcat(display_value, " ");
}
for (int k = 0; display_value[k]; ++k)
{
    if (display_value[k] == '\n' || display_value[k] == '\r')
        display_value[k] = ' ';
}
    // Output the row (80 columns: 23 + 41 + 12 + separators)
    snprintf(buf, sizeof(buf),
        "{Y| %-23.23s {Y| %-39.39s {Y| %-14.14s {Y|{x\n\r",
        setting->name, display_value, type_str);
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

    // Table header
    snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
    add_buf(buffer, buf);

    // Centered category name
    int cat_len = strlen_no_colours(setting_category_names[category]);
    int cat_pad = (77 - cat_len) / 2;
    snprintf(buf, sizeof(buf), "{Y|%*s%-*.*s%*s|{x\n\r", cat_pad, "", cat_len, cat_len, setting_category_names[category], 77 - cat_pad - cat_len, "");
    add_buf(buffer, buf);

    snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
    add_buf(buffer, buf);
    snprintf(buf, sizeof(buf), "{Y| %-23s | %-36s | %-10s |{x\n\r", "Setting Name", "Value", "Type");
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
        snprintf(buf, sizeof(buf), "{Y| %-73s |{x\n\r", "No settings in this category.");
        add_buf(buffer, buf);
    }

    snprintf(buf, sizeof(buf), "{Y+-------------------------+--------------------------------------+------------+{x\n\r");
    add_buf(buffer, buf);

    // Legend
    snprintf(buf, sizeof(buf), "  Legend: {R*{x Requires reboot   {MS{x Sensitive setting   {DX{x Not settable via OLC\n\r\n\r");
    add_buf(buffer, buf);
}

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
            
        case SETTING_TYPE_FLOAT:
            {
                char *p;
                bool valid = TRUE;
                int decimal_points = 0;
                
                // Check if it's a valid float format (digits, maybe a decimal point, more digits)
                for (p = value; *p != '\0'; p++) {
                    if (*p == '.') {
                        decimal_points++;
                        if (decimal_points > 1) {
                            valid = FALSE;
                            break;
                        }
                    } else if (*p == '-' && p == value) {
                        // Negative sign allowed only at start
                        continue;
                    } else if (!isdigit(*p)) {
                        valid = FALSE;
                        break;
                    }
                }
                
                if (!valid) {
                    send_to_char("This setting requires a floating point number.\n\r", ch);
                    return FALSE;
                }
            }
            break;

        case SETTING_TYPE_EXTSTR:
            // For extended strings, we'll handle editing differently
            if (!str_cmp(value, "edit")) {
                if (!setting->olc_settable) {
                    send_to_char("This setting cannot be changed through OLC.\n\r", ch);
                    return FALSE;
                }
                
                // Set up the string editor
                ch->desc->pString = (char **)setting->ptr;
                ch->desc->editor = ED_GAMESETTING;
                ch->desc->editor_ptr = (void *)setting; // Store setting for later reference
                
                string_append(ch, (char **)setting->ptr);
                return TRUE;
            }
            // Otherwise, treat it like a regular string
            break;
            
        case SETTING_TYPE_STRING:
            /* No validation needed for string types */
            break;
    }
    
    /* Initialize pending changes list if needed */
    if (!pending_changes) {
        pending_changes = list_create(false);
    }
    
    // Handle special case for EXTSTR when not using 'edit'
    if (setting->type == SETTING_TYPE_EXTSTR && str_cmp(value, "edit")) {
        send_to_char("Extended string settings must be edited with 'gameedit set <setting> edit'.\n\r", ch);
        return FALSE;
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


/*
 * View changeset history
 */
GAMEEDIT(gameedit_history)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int i, limit;
    
    buffer = new_buf();
    
    /* Determine how many changesets to show */
    if (argument[0] != '\0' && is_number(argument))
        limit = UMIN(atoi(argument), changeset_count);
    else
        limit = changeset_count;
    
    /* Table header */
    sprintf(buf, "{Y+-------+--------------------+-----------------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WID{x    | {WAuthor{x            | {WChanges{x         | {WDate & Time{x                  |{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y+-------+--------------------+-----------------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    /* Show changesets from newest to oldest */
    for (i = changeset_count - 1; i >= 0 && i >= changeset_count - limit; i--) {
        GAME_SETTINGS_CHANGESET *changeset = changesets[i];
        char time_buf[64];
        struct tm *time_info;
        
        /* Format the timestamp */
        time_info = localtime(&changeset->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);
        
        sprintf(buf, "{Y| {W%-5d{x | %-18.18s | {W%-15d{x | %-29s |{x\n\r",
            changeset->id,
            changeset->author,
            list_size(changeset->changes),
            time_buf);
        add_buf(buffer, buf);
    }
    
    /* Table footer */
    sprintf(buf, "{Y+-------+--------------------+-----------------+-------------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    /* Instructions */
    sprintf(buf, "\n\rUse '{Wgameedit view <id>{x' to see details of a specific changeset.\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "Use '{Wgameedit comment <id>{x' to edit a changeset's comment.\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "Use '{Wgameedit rollback <id>{x' to revert to a previous changeset.\n\r");
    add_buf(buffer, buf);

    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return FALSE;
}

/*
 * View details of a specific changeset
 */
GAMEEDIT(gameedit_view)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int i, id;
    GAME_SETTINGS_CHANGESET *changeset = NULL;
    ITERATOR it;
    GAME_SETTING_CHANGE_HISTORY *history;
    
    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: gameedit view <changeset_id>\n\r", ch);
        return FALSE;
    }
    
    id = atoi(argument);
    
    /* Find the changeset with the given ID */
    for (i = 0; i < changeset_count; i++) {
        if (changesets[i]->id == id) {
            changeset = changesets[i];
            break;
        }
    }
    
    if (!changeset) {
        send_to_char("No changeset found with that ID.\n\r", ch);
        return FALSE;
    }
    
    buffer = new_buf();
    
    /* Changeset header */
    sprintf(buf, "{Y+----------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WChangeset #{x%-67d |{x\n\r", changeset->id);
    add_buf(buffer, buf);
    sprintf(buf, "{Y+----------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    /* Author and timestamp */
    char time_buf[64];
    struct tm *time_info = localtime(&changeset->timestamp);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);
    
    sprintf(buf, "{Y| {WAuthor:{x %-68s |{x\n\r", changeset->author);
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WDate:{x %-70s |{x\n\r", time_buf);
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WComment:{x %-67s |{x\n\r", changeset->comment);
    add_buf(buffer, buf);
    
    /* Change list header */
    sprintf(buf, "{Y+----------------------------------------------------------------------------+{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WChanges:{x %-68d |{x\n\r", list_size(changeset->changes));
    add_buf(buffer, buf);
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {WSetting{x              | {WOld Value{x              | {WNew Value{x              |{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    /* List each change */
    iterator_start(&it, changeset->changes);
    while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
        char old_display[MAX_INPUT_LENGTH];
        char new_display[MAX_INPUT_LENGTH];
        
        /* Format the values for display, handling sensitive settings */
        if (history->setting->sensitive && ch->pcdata->security < 10) {
            // Hide sensitive values for non-high-security staff
            strcpy(old_display, "{D*****{x");
            strcpy(new_display, "{D*****{x");
        } else {
            // Apply appropriate coloring based on value type
            switch (history->setting->type) {
                case SETTING_TYPE_BOOL:
                    if (!str_cmp(history->old_value, "true") || 
                        !str_cmp(history->old_value, "yes") || 
                        !str_cmp(history->old_value, "on") || 
                        !str_cmp(history->old_value, "1"))
                        sprintf(old_display, "{Gtrue{x");
                    else
                        sprintf(old_display, "{Rfalse{x");
                        
                    if (!str_cmp(history->new_value, "true") || 
                        !str_cmp(history->new_value, "yes") || 
                        !str_cmp(history->new_value, "on") || 
                        !str_cmp(history->new_value, "1"))
                        sprintf(new_display, "{Gtrue{x");
                    else
                        sprintf(new_display, "{Rfalse{x");
                    break;
                    
                case SETTING_TYPE_INT:
                    sprintf(old_display, "{Y%s{x", history->old_value);
                    sprintf(new_display, "{Y%s{x", history->new_value);
                    break;
                    
                case SETTING_TYPE_STRING:
                    if (history->old_value && history->old_value[0])
                        sprintf(old_display, "{W%s{x", history->old_value);
                    else
                        strcpy(old_display, "{D(empty){x");
                        
                    if (history->new_value && history->new_value[0])
                        sprintf(new_display, "{W%s{x", history->new_value);
                    else
                        strcpy(new_display, "{D(empty){x");
                    break;
                    
                default:
                    sprintf(old_display, "%s", history->old_value);
                    sprintf(new_display, "%s", history->new_value);
                    break;
            }
        }
        
        sprintf(buf, "{Y| {C%-22.22s{x | %-23.23s | %-23.23s |{x\n\r",
            history->setting->name, old_display, new_display);
        add_buf(buffer, buf);
    }
    iterator_stop(&it);
    
    /* Footer */
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);
    
    /* Instructions */
    sprintf(buf, "Use '{Wgameedit comment %d{x' to edit this changeset's comment.\n\r", changeset->id);
    add_buf(buffer, buf);
    sprintf(buf, "\n\rUse '{Wgameedit rollback %d{x' to revert to this changeset.\n\r", changeset->id);
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    
    return FALSE;
}

/*
 * Display all pending changes
 */
GAMEEDIT(gameedit_pending)
{
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    ITERATOR it;
    GAME_SETTING_CHANGE *change;

    buffer = new_buf();

    if (!pending_changes || list_size(pending_changes) == 0) {
        add_buf(buffer, "There are no pending game setting changes.\n\r");
        page_to_char(buf_string(buffer), ch);
        free_buf(buffer);
        return TRUE;
    }

    sprintf(buf, "{YPending Game Setting Changes (%d):{x\n\r", list_size(pending_changes));
    add_buf(buffer, buf);

    // Table header
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);
    sprintf(buf, "{Y| {W%-22s{x | {W%-23s{x | {W%-23s{x |{x\n\r", "Setting Name", "Current Value", "Pending Value");
    add_buf(buffer, buf);
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);

    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        const struct game_setting_type *setting = change->setting;
        char current_value_str[MAX_STRING_LENGTH];
        char pending_value_str[MAX_STRING_LENGTH];
        char display_current_val[MAX_STRING_LENGTH];
        char display_pending_val[MAX_STRING_LENGTH];
        int width_current, width_pending;
        const int max_vis_col_width = 23;

        // Format Current Value
        if (setting->sensitive && ch->pcdata->security < 10) {
            strcpy(current_value_str, "{D*****{x");
        } else {
            switch (setting->type) {
                case SETTING_TYPE_BOOL:
                    sprintf(current_value_str, "%s", *(bool *)setting->ptr ? "{Gtrue{x" : "{Rfalse{x");
                    break;
                case SETTING_TYPE_INT:
                    sprintf(current_value_str, "{Y%d{x", *(int *)setting->ptr);
                    break;
                case SETTING_TYPE_STRING:
                    if (*(char **)setting->ptr && **(char **)setting->ptr)
                        sprintf(current_value_str, "{W%s{x", *(char **)setting->ptr);
                    else
                        strcpy(current_value_str, "{D(empty){x");
                    break;
                default:
                    strcpy(current_value_str, "{D(unknown){x");
                    break;
            }
        }

        // Format Pending Value
        if (setting->sensitive && ch->pcdata->security < 10) {
            strcpy(pending_value_str, "{D*****{x");
        } else {
            switch (setting->type) {
                case SETTING_TYPE_BOOL:
                    if (!str_cmp(change->value, "true") || !str_cmp(change->value, "yes") || !str_cmp(change->value, "on") || !str_cmp(change->value, "1"))
                        sprintf(pending_value_str, "{G%s{x", change->value);
                    else
                        sprintf(pending_value_str, "{R%s{x", change->value);
                    break;
                case SETTING_TYPE_INT:
                     sprintf(pending_value_str, "{Y%s{x", change->value);
                    break;
                case SETTING_TYPE_STRING:
                    if (change->value && change->value[0])
                        sprintf(pending_value_str, "{W%s{x", change->value);
                    else
                        strcpy(pending_value_str, "{D(empty){x");
                    break;
                default:
                     sprintf(pending_value_str, "{M%s{x", change->value);
                    break;
            }
        }
        
        // Truncate current_value_str for display
        if (strlen_no_colours(current_value_str) > max_vis_col_width) {
            int trunc_len = colour_trunc_len(current_value_str, max_vis_col_width - 3);
            strncpy(display_current_val, current_value_str, trunc_len);
            display_current_val[trunc_len] = '\0';
            if (strlen(display_current_val) > 0 && display_current_val[strlen(display_current_val)-1] == '{') {
                 display_current_val[strlen(display_current_val)-1] = '\0'; // Avoid broken color code
            }
            strcat(display_current_val, "{x...");
        } else {
            strcpy(display_current_val, current_value_str);
        }
        for (int k = 0; display_current_val[k]; ++k) if (display_current_val[k] == '\n' || display_current_val[k] == '\r') display_current_val[k] = ' ';

        // Truncate pending_value_str for display
        if (strlen_no_colours(pending_value_str) > max_vis_col_width) {
            int trunc_len = colour_trunc_len(pending_value_str, max_vis_col_width - 3);
            strncpy(display_pending_val, pending_value_str, trunc_len);
            display_pending_val[trunc_len] = '\0';
            if (strlen(display_pending_val) > 0 && display_pending_val[strlen(display_pending_val)-1] == '{') {
                 display_pending_val[strlen(display_pending_val)-1] = '\0'; // Avoid broken color code
            }
            strcat(display_pending_val, "{x...");
        } else {
            strcpy(display_pending_val, pending_value_str);
        }
        for (int k = 0; display_pending_val[k]; ++k) if (display_pending_val[k] == '\n' || display_pending_val[k] == '\r') display_pending_val[k] = ' ';

        width_current = max_vis_col_width + (strlen(display_current_val) - strlen_no_colours(display_current_val));
        width_pending = max_vis_col_width + (strlen(display_pending_val) - strlen_no_colours(display_pending_val));

        sprintf(buf, "{Y| {C%-22.22s{x | %-*.30s | %-*.30s |{x\n\r",
                setting->name,
                width_current, display_current_val,
                width_pending, display_pending_val);
        add_buf(buffer, buf);
    }
    iterator_stop(&it);

    // Table footer
    sprintf(buf, "{Y+------------------------+-------------------------+-------------------------+{x\n\r");
    add_buf(buffer, buf);

    if (requires_reboot()) {
        sprintf(buf, "\n\r{RWARNING: Some pending changes require a reboot to take effect.{x\n\r");
        add_buf(buffer, buf);
    }
    
    sprintf(buf, "\n\rUse '{Wgameedit confirm{x' to apply these changes or '{Wgameedit revert{x' to discard them.\n\r");
    add_buf(buffer, buf);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return TRUE;
}

/*
 * Rollback to a previous changeset
 */
GAMEEDIT(gameedit_rollback)
{
    int i, id;
    char arg[MAX_INPUT_LENGTH];
    bool confirm = FALSE;
    GAME_SETTINGS_CHANGESET *changeset = NULL;
    ITERATOR it;
    GAME_SETTING_CHANGE_HISTORY *history;
    int count = 0;
    bool reboot_needed = false;
    
    argument = one_argument(argument, arg);
    
    if (arg[0] == '\0' || !is_number(arg)) {
        send_to_char("Syntax: gameedit rollback <changeset_id> [confirm]\n\r", ch);
        return FALSE;
    }
    
    id = atoi(arg);
    
    /* Check if 'confirm' was provided */
    if (argument[0] != '\0' && !str_cmp(argument, "confirm"))
        confirm = TRUE;
    
    /* Find the changeset with the given ID */
    for (i = 0; i < changeset_count; i++) {
        if (changesets[i]->id == id) {
            changeset = changesets[i];
            break;
        }
    }
    
    if (!changeset) {
        send_to_char("No changeset found with that ID.\n\r", ch);
        return FALSE;
    }
    
    /* Require confirmation before proceeding */
    if (!confirm) {
        char time_buf[64];
        struct tm *time_info = localtime(&changeset->timestamp);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);
        
        send_to_char(formatf("You are about to rollback to changeset #%d created by %s on %s.\n\r",
            changeset->id, changeset->author, time_buf), ch);
        send_to_char(formatf("This will revert %d settings.\n\r", list_size(changeset->changes)), ch);
        send_to_char("Type 'gameedit rollback <id> confirm' to proceed.\n\r", ch);
        return FALSE;
    }
    
    /* Apply the old values */
    iterator_start(&it, changeset->changes);
    while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
        count++;
        
        /* Apply the old value based on the setting type */
        switch (history->setting->type) {
            case SETTING_TYPE_BOOL:
                *(bool *)history->setting->ptr = (!str_cmp(history->old_value, "true") || 
                                                !str_cmp(history->old_value, "yes") || 
                                                !str_cmp(history->old_value, "on") || 
                                                !str_cmp(history->old_value, "1"));
                break;
                
            case SETTING_TYPE_INT:
                *(int *)history->setting->ptr = atoi(history->old_value);
                break;
                
            case SETTING_TYPE_STRING:
                free_string(*(char **)history->setting->ptr);
                *(char **)history->setting->ptr = str_dup(history->old_value);
                break;
        }
        
        if (history->setting->requires_reboot)
            reboot_needed = true;
    }
    iterator_stop(&it);
    
/* Create a new changeset for the rollback */
if (!pending_changes)
    pending_changes = list_create(false);

/* Add each change to the pending changes list */
iterator_start(&it, changeset->changes);
GAME_SETTING_CHANGE *change;
while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
    change = alloc_mem(sizeof(GAME_SETTING_CHANGE));
    change->setting = history->setting;
    
    // Store the current value as we're rolling back FROM this value TO the old value
    switch (history->setting->type) {
        case SETTING_TYPE_BOOL: {
//            bool current = *(bool *)history->setting->ptr;
            // We're rolling back TO the old_value
            change->value = str_dup(history->old_value);
            break;
        }
        
        case SETTING_TYPE_INT: {
//            int current = *(int *)history->setting->ptr;
            // We're rolling back TO the old_value
            change->value = str_dup(history->old_value);
            break;
        }
        
        case SETTING_TYPE_STRING:
            // We're rolling back TO the old_value
            change->value = str_dup(history->old_value);
            break;
    }
    
    list_appendlink(pending_changes, change);
}
iterator_stop(&it);

/* Create the rollback changeset and save */
char comment[MAX_STRING_LENGTH];
sprintf(comment, "Rollback to changeset #%d", changeset->id);
create_changeset(ch, comment);

/* Clear the pending changes AFTER saving the changeset */
iterator_start(&it, pending_changes);
while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
    if (change->value)
        free_string(change->value);
    free_mem(change, sizeof(GAME_SETTING_CHANGE));
}
iterator_stop(&it);
list_clear(pending_changes);

/* Save the settings and changesets */
game_settings_write();
save_changesets();
    
    send_to_char(formatf("Rolled back %d settings to changeset #%d.\n\r", count, changeset->id), ch);
    send_to_char("Game settings saved.\n\r", ch);
    
    /* If a reboot is needed, prompt the user */
    if (reboot_needed) {
        send_to_char("{RChanged settings require a reboot to take full effect.{x\n\r", ch);
        send_to_char("Type '{Wreboot now{x' to reboot the server immediately.\n\r", ch);
        pending_reboot = true;
    }
    
    return TRUE;
}

void create_changeset(CHAR_DATA *ch, char *comment)
{
    GAME_SETTINGS_CHANGESET *changeset;
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    int index;

    /* Don't create empty changesets */
    if (!pending_changes || list_size(pending_changes) == 0)
        return;
    
    /* Create a new changeset */
    changeset = alloc_mem(sizeof(GAME_SETTINGS_CHANGESET));
    changeset->id = next_changeset_id++;
    changeset->author = str_dup(ch->name);
    changeset->timestamp = current_time;
    changeset->comment = str_dup(comment ? comment : "No comment provided");
    changeset->changes = list_create(false);
    
    /* Add each pending change to the changeset history */
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        GAME_SETTING_CHANGE_HISTORY *history;
        
        history = alloc_mem(sizeof(GAME_SETTING_CHANGE_HISTORY));
        history->setting = change->setting;
        
// In the create_changeset function:
/* Store the old value */
    switch (change->setting->type) {
        case SETTING_TYPE_BOOL: {
            bool current_value = *(bool *)change->setting->ptr;
            history->old_value = str_dup(current_value ? "true" : "false");
            break;
        }
        
        case SETTING_TYPE_INT: {
            int current_value = *(int *)change->setting->ptr;
            char temp_buf[MAX_STRING_LENGTH];
            sprintf(temp_buf, "%d", current_value);
            history->old_value = str_dup(temp_buf);
            break;
        }
        
        case SETTING_TYPE_STRING:
        case SETTING_TYPE_EXTSTR: {
            char *current_value = *(char **)change->setting->ptr;
            history->old_value = str_dup(current_value ? current_value : "");
            break;
        }
        
        case SETTING_TYPE_FLOAT: {
            float current_value = *(float *)change->setting->ptr;
            char temp_buf[MAX_STRING_LENGTH];
            sprintf(temp_buf, "%.4f", current_value);
            history->old_value = str_dup(temp_buf);
            break;
        }
    }
        
        /* Store the new value */
        history->new_value = str_dup(change->value);
        
        list_appendlink(changeset->changes, history);
    }
    iterator_stop(&it);
    
    /* Make room for the new changeset if needed */
    if (changeset_count == MAX_CHANGESETS) {
        /* Free the oldest changeset to make room */
        free_changeset(changesets[0]);
        
        /* Shift all changesets down one position */
        for (index = 0; index < MAX_CHANGESETS - 1; index++) {
            changesets[index] = changesets[index + 1];
        }
        
        changeset_count--;
    }
    
    /* Add the new changeset */
    changesets[changeset_count++] = changeset;
    
    /* Log the changeset */
    log_string(formatf("Changeset #%d created by %s with %d changes", 
        changeset->id, changeset->author, list_size(changeset->changes)));
}

/*
 * Free a single changeset and its contents
 */
void free_changeset(GAME_SETTINGS_CHANGESET *changeset)
{
    ITERATOR it;
    GAME_SETTING_CHANGE_HISTORY *history;
    
    if (!changeset)
        return;
    
    free_string(changeset->author);
    free_string(changeset->comment);
    
    if (changeset->changes) {
        iterator_start(&it, changeset->changes);
        while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
            free_string(history->old_value);
            free_string(history->new_value);
            free_mem(history, sizeof(GAME_SETTING_CHANGE_HISTORY));
        }
        iterator_stop(&it);
        list_destroy(changeset->changes);
    }
    
    free_mem(changeset, sizeof(GAME_SETTINGS_CHANGESET));
}

/*
 * Free all changesets
 */
void free_all_changesets(void)
{
    int i;
    
    for (i = 0; i < changeset_count; i++) {
        free_changeset(changesets[i]);
        changesets[i] = NULL;
    }
    
    changeset_count = 0;
}

/*
 * Save changesets to a file
 */
void save_changesets(void)
{
    FILE *fp;
    int i;
    ITERATOR it;
    GAME_SETTING_CHANGE_HISTORY *history;
    
    if ((fp = fopen(CHANGESET_FILE, "w")) == NULL) {
        bug("save_changesets: Cannot open changeset file for writing", 0);
        return;
    }
    
    fprintf(fp, "#CHANGESETS %d %d\n", changeset_count, next_changeset_id);
    
    // Make sure we save ALL changesets in the array, in order by index
    for (i = 0; i < changeset_count; i++) {
        GAME_SETTINGS_CHANGESET *changeset = changesets[i];
        
        if (!changeset) {
            log_string(formatf("Warning: Null changeset at index %d", i));
            continue;
        }
        
        fprintf(fp, "#CHANGESET\n");
        fprintf(fp, "Id %d\n", changeset->id);
        fprintf(fp, "Author %s~\n", changeset->author);
        fprintf(fp, "Timestamp %ld\n", (long)changeset->timestamp);
        fprintf(fp, "Comment %s~\n", changeset->comment);
        
        iterator_start(&it, changeset->changes);
        while ((history = (GAME_SETTING_CHANGE_HISTORY *)iterator_nextdata(&it))) {
            // Make sure to write the setting name separately from the old and new values
            fprintf(fp, "Change %s %s~ %s~\n", 
                history->setting->name,
                history->old_value ? history->old_value : "",
                history->new_value ? history->new_value : "");
        }
        iterator_stop(&it);
        
        fprintf(fp, "#END\n");
    }
    
    fprintf(fp, "#END\n");
    fclose(fp);
    
    log_string("Game setting changesets saved.");
}

/*
 * Load changesets from a file
 */
/*
 * Load changesets from a file
 */
void load_changesets(void)
{
    FILE *fp;
    char *word;
    bool fMatch;
    GAME_SETTINGS_CHANGESET *changeset = NULL;
    char buf[MAX_STRING_LENGTH];
    
    /* First, free any existing changesets */
    free_all_changesets();
    
    if ((fp = fopen(CHANGESET_FILE, "r")) == NULL) {
        log_string("No changeset file found. Starting with empty history.");
        return;
    }
    
    log_string("Loading changesets from file...");
    
    word = fread_word(fp);
    if (!word || str_cmp(word, "#CHANGESETS")) {
        snprintf(buf, sizeof(buf), "load_changesets: Expected #CHANGESETS but got %s", word ? word : "NULL");
        bug(buf, 0);
        fclose(fp);
        return;
    }
    
    changeset_count = fread_number(fp);
    next_changeset_id = fread_number(fp);
    
    log_string(formatf("Found %d changesets, next ID: %d", changeset_count, next_changeset_id));
    
    changeset_count = 0; // Reset and count as we load
    
    for (;;) {
        if (feof(fp)) {
            log_string("Reached end of changeset file");
            break;
        }
        
        word = fread_word(fp);
        if (!word) {
            log_string("Error: fread_word returned NULL");
            break;
        }
        
        fMatch = FALSE;
        
        if (word[0] == '\0') {
            log_string("Error: Empty word read");
            break;
        }
        
        // If we're inside a changeset and see #END, it's the end of the current changeset
        if (!str_cmp(word, "#END") && changeset) {
            log_string(formatf("Finished changeset ID #%d", changeset->id));
            changeset = NULL; // Reset for next changeset
            fMatch = TRUE;
            continue;
        }
        
        // If we're not inside a changeset and see #END, it's the end of the file
        if (!str_cmp(word, "#END") && !changeset) {
            log_string("Found file-level #END marker");
            break;
        }
        
        // Start of a new changeset
        if (!str_cmp(word, "#CHANGESET")) {
            if (changeset_count >= MAX_CHANGESETS) {
                log_string("Warning: Too many changesets, ignoring extras");
                // Skip this changeset by reading until #END
                for (;;) {
                    word = fread_word(fp);
                    if (!word || !str_cmp(word, "#END") || feof(fp))
                        break;
                }
                continue;
            }
            
            changeset = alloc_mem(sizeof(GAME_SETTINGS_CHANGESET));
            if (!changeset) {
                log_string("Error: Failed to allocate memory for changeset");
                fclose(fp);
                return;
            }
            
            changeset->changes = list_create(false);
            changeset->author = str_dup("");  // Initialize with empty strings
            changeset->comment = str_dup("");
            
            changesets[changeset_count++] = changeset;
//            log_string(formatf("Processing changeset #%d", changeset_count));
            fMatch = TRUE;
            continue;
        }
        
        // If not working on a changeset, skip this line
        if (!changeset) {
            log_string(formatf("Warning: Found data outside of changeset block: '%s'", word));
            fread_to_eol(fp);
            continue;
        }
        
        switch (UPPER(word[0])) {
            case 'A':
                if (!str_cmp(word, "Author")) {
                    free_string(changeset->author);
                    changeset->author = fread_string(fp);
                    fMatch = TRUE;
                }
                break;
                
            case 'C':
                if (!str_cmp(word, "Comment")) {
                    free_string(changeset->comment);
                    changeset->comment = fread_string(fp);
                    fMatch = TRUE;
                }
                else if (!str_cmp(word, "Change")) {
                    char *setting_name = fread_word(fp);  // Use fread_word for the setting name
                    if (!setting_name) {
                        log_string("Error reading setting name");
                        fread_to_eol(fp);
                        continue;
                    }
                    
                    // Now read the old_value and new_value using fread_string
                    char *old_value = fread_string(fp);
                    char *new_value = fread_string(fp);
                    
//                    log_string(formatf("Loading change for setting: '%s'", setting_name));
                    
                    const struct game_setting_type *setting = NULL;
                    if (setting_name && *setting_name) {
                        bool found = gameedit_find_setting(NULL, setting_name, &setting);
//                        log_string(formatf("Setting lookup result: %s", found ? "FOUND" : "NOT FOUND"));
                        
                        if (found && setting) {
                            GAME_SETTING_CHANGE_HISTORY *history;
                            
                            history = alloc_mem(sizeof(GAME_SETTING_CHANGE_HISTORY));
                            if (history) {
                                history->setting = setting;
                                history->old_value = str_dup(old_value ? old_value : "");
                                history->new_value = str_dup(new_value ? new_value : "");
                                list_appendlink(changeset->changes, history);
                                log_string(formatf("Added change for setting: %s (old: %s, new: %s)", 
                                    setting_name, old_value ? old_value : "empty", 
                                    new_value ? new_value : "empty"));
                            }
                        } else {
                            log_string(formatf("WARNING: Setting '%s' not found in game_settings_table", 
                                setting_name));
                        }
                    }
                    
                    // Free strings as needed
                    if (old_value) free_string(old_value);
                    if (new_value) free_string(new_value);
                    
                    fMatch = TRUE;
                }
                break;
                
            case 'I':
                if (!str_cmp(word, "Id")) {
                    changeset->id = fread_number(fp);
                    fMatch = TRUE;
                }
                break;
                
            case 'T':
                if (!str_cmp(word, "Timestamp")) {
                    changeset->timestamp = fread_number(fp);
                    fMatch = TRUE;
                }
                break;
        }
        
        if (!fMatch) {
            log_string(formatf("Warning: Unrecognized keyword '%s' in changeset file", 
                word ? word : "NULL"));
            fread_to_eol(fp);
        }
    }

    // Sort changesets by ID
    log_string("Sorting changesets by ID...");
    for (int i = 0; i < changeset_count - 1; i++) {
        for (int j = i + 1; j < changeset_count; j++) {
            if (changesets[i] && changesets[j] && 
                changesets[i]->id > changesets[j]->id) {
                GAME_SETTINGS_CHANGESET *temp = changesets[i];
                changesets[i] = changesets[j];
                changesets[j] = temp;
            }
        }
    }
    
    // Verify all changesets loaded correctly
    log_string(formatf("Successfully loaded %d changesets.", changeset_count));
    for (int i = 0; i < changeset_count; i++) {
        log_string(formatf(" - Changeset #%d by %s with %d changes", 
            changesets[i]->id, changesets[i]->author, 
            list_size(changesets[i]->changes)));
    }
    
    fclose(fp);
}

GAMEEDIT(gameedit_comment)
{
    int i, id;
    GAME_SETTINGS_CHANGESET *changeset = NULL;
    
    if (argument[0] == '\0' || !is_number(argument)) {
        send_to_char("Syntax: gameedit comment <changeset_id>\n\r", ch);
        return FALSE;
    }
    
    id = atoi(argument);
    
    /* Find the changeset with the given ID */
    for (i = 0; i < changeset_count; i++) {
        if (changesets[i]->id == id) {
            changeset = changesets[i];
            break;
        }
    }
    
    if (!changeset) {
        send_to_char("No changeset found with that ID.\n\r", ch);
        return FALSE;
    }
    
    /* Only allow author or imps to edit comments */
    if (str_cmp(ch->name, changeset->author) && get_staff_rank(ch) < STAFF_IMPLEMENTOR) {
        send_to_char("You can only edit comments on changesets you created if you are not an IMP.\n\r", ch);
        return FALSE;
    }
    
    /* Start the string editor without saving immediately */
    ch->desc->pString = &changeset->comment;
    ch->desc->editor = ED_CHANGESET;
    
    string_append(ch, &changeset->comment);
    
    return TRUE;
}

// Add a handler for the string editor for EXTSTR type settings
void game_settings_string_edit(CHAR_DATA *ch)
{
    const struct game_setting_type *setting;
    GAME_SETTING_CHANGE *change;
    ITERATOR it;
    
    if (!ch->desc || !ch->desc->pString || !ch->desc->editor_ptr) {
        send_to_char("String editing error.\n\r", ch);
        return;
    }
    
    setting = (const struct game_setting_type *)ch->desc->editor_ptr;
    
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
            change->value = str_dup(*(char **)setting->ptr);
            iterator_stop(&it);
            
            send_to_char("Setting updated in pending changes.\n\r", ch);
            return;
        }
    }
    iterator_stop(&it);
    
    /* Create a new pending change */
    change = (GAME_SETTING_CHANGE *)alloc_mem(sizeof(GAME_SETTING_CHANGE));
    change->setting = setting;
    change->value = str_dup(*(char **)setting->ptr);
    list_appendlink(pending_changes, change);
    
    send_to_char("Setting added to pending changes.\n\r", ch);
    
    if (setting->requires_reboot) {
        send_to_char("{RNote: This setting requires a reboot to take effect.{x\n\r", ch);
        pending_reboot = true;
    }
}