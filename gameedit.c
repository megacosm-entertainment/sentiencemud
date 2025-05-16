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
        
/* If not a category, look for specific setting */
if (!found && gameedit_find_setting(ch, argument, &setting)) {
    char buf[MAX_STRING_LENGTH];
    char formatted_value[MAX_STRING_LENGTH];
    
    sprintf(buf, "{Y╔══════════════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║ %-78s ║{x\n\r", setting->name);
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠══════════════════════════╦═══════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    /* Category */
    sprintf(buf, "{Y║{x Category                {Y║{x %-57s {Y║{x\n\r", setting_category_names[setting->category]);
    add_buf(buffer, buf);
    
    /* Type */
    sprintf(buf, "{Y║{x Type                    {Y║{x %-57s {Y║{x\n\r", setting_type_names[setting->type]);
    add_buf(buffer, buf);
    
    /* OLC Settable */
    sprintf(buf, "{Y║{x OLC Settable            {Y║{x %-57s {Y║{x\n\r", setting->olc_settable ? "Yes" : "No");
    add_buf(buffer, buf);
    
    /* Requires Reboot */
    sprintf(buf, "{Y║{x Requires Reboot         {Y║{x %-57s {Y║{x\n\r", setting->requires_reboot ? "Yes" : "No");
    add_buf(buffer, buf);
    
    /* Sensitive */
    sprintf(buf, "{Y║{x Sensitive               {Y║{x %-57s {Y║{x\n\r", setting->sensitive ? "Yes" : "No");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠══════════════════════════╩═══════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    /* Description */
    char description_buf[MAX_STRING_LENGTH];
    strcpy(description_buf, setting->help);
    sprintf(buf, "{Y║{x Description: %-66s {Y║{x\n\r", description_buf);
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠════════════════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    /* Current Value */
    if (setting->sensitive && ch->pcdata->security < 10) {
        sprintf(formatted_value, "{D*****{x");
    } else {
        switch (setting->type) {
            case SETTING_TYPE_BOOL:
                sprintf(formatted_value, "%s", *(bool *)setting->ptr ? "{Gtrue{x" : "{Rfalse{x");
                break;
            case SETTING_TYPE_INT:
                sprintf(formatted_value, "{Y%d{x", *(int *)setting->ptr);
                break;
            case SETTING_TYPE_STRING:
                if (*(char **)setting->ptr && **(char **)setting->ptr)
                    sprintf(formatted_value, "{W%s{x", *(char **)setting->ptr);
                else
                    sprintf(formatted_value, "{D(empty){x");
                break;
            default:
                sprintf(formatted_value, "{D(unknown type){x");
                break;
        }
    }
    
    sprintf(buf, "{Y║{x Current Value: %-64s {Y║{x\n\r", formatted_value);
    add_buf(buffer, buf);
    
    /* Check if there's a pending change */
    ITERATOR it;
    GAME_SETTING_CHANGE *change;
    iterator_start(&it, pending_changes);
    while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
        if (change->setting == setting) {
            if (setting->sensitive && ch->pcdata->security < 10) {
                sprintf(buf, "{Y║{x Pending Value: {D*****{x%-60s {Y║{x\n\r", "");
            } else {
                sprintf(buf, "{Y║{x Pending Value: {Y%s{x%-60s {Y║{x\n\r", change->value, "");
            }
            add_buf(buffer, buf);
            break;
        }
    }
    iterator_stop(&it);
    
    /* Add help text for the setting type */
    sprintf(buf, "{Y╠════════════════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    if (setting->olc_settable) {
        switch (setting->type) {
            case SETTING_TYPE_BOOL:
                sprintf(buf, "{Y║{x Usage: {Wgameedit set %s [true|false|yes|no|on|off|1|0]{x%-26s {Y║{x\n\r", 
                    setting->name, "");
                break;
            case SETTING_TYPE_INT:
                sprintf(buf, "{Y║{x Usage: {Wgameedit set %s <number>{x%-49s {Y║{x\n\r", 
                    setting->name, "");
                break;
            case SETTING_TYPE_STRING:
                sprintf(buf, "{Y║{x Usage: {Wgameedit set %s <text>{x%-52s {Y║{x\n\r", 
                    setting->name, "");
                break;
        }
        add_buf(buffer, buf);
    }
    
    sprintf(buf, "{Y╚════════════════════════════════════════════════════════════════════════════════════════╝{x\n\r");
    add_buf(buffer, buf);
} 
else if (!found) {
    send_to_char("No such category or setting found.\n\r", ch);
    free_buf(buffer);
    return;
}
    }
    
/* Display any pending confirmation */
if (list_size(pending_changes) > 0) {
    char buf[MAX_STRING_LENGTH];
    
    sprintf(buf, "\n\r{Y╔══════════════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║ {RPending Changes{x%-66s {Y║{x\n\r", "");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠════════════════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║{x There are %d pending changes that need to be confirmed.%-28s {Y║{x\n\r", 
        list_size(pending_changes), "");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║{x Use '{Wgameedit confirm{x' to apply changes%-44s {Y║{x\n\r", "");
    add_buf(buffer, buf);
    
    if (requires_reboot()) {
        sprintf(buf, "{Y║{x {RWARNING: Some changes require a reboot to take effect{x%-27s {Y║{x\n\r", "");
        add_buf(buffer, buf);
        pending_reboot = true;
    }
    
    sprintf(buf, "{Y║{x Use '{Wgameedit revert{x' to discard all pending changes%-36s {Y║{x\n\r", "");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╚════════════════════════════════════════════════════════════════════════════════════════╝{x\n\r");
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

/*
 * Display a single setting
 */
/*
 * Display a single setting
 */
void gameedit_display_setting(BUFFER *buffer, CHAR_DATA *ch, const struct game_setting_type *setting)
{
    char buf[MAX_STRING_LENGTH];
    char value_buf[MAX_STRING_LENGTH];
    char type_buf[MAX_STRING_LENGTH];
    bool has_change = FALSE;
    char *change_value = NULL;
    
    /* Check if there's a pending change */
    if (pending_changes && list_size(pending_changes) > 0) {
        ITERATOR it;
        GAME_SETTING_CHANGE *change;
        iterator_start(&it, pending_changes);
        while ((change = (GAME_SETTING_CHANGE *)iterator_nextdata(&it))) {
            if (change->setting == setting) {
                has_change = TRUE;
                change_value = change->value;
                break;
            }
        }
        iterator_stop(&it);
    }
    
    /* Format the setting name with proper table structure */
    sprintf(buf, "{Y║{x  {C%s{x", setting->name);
    pad_string(buf, 24, NULL, NULL);
    add_buf(buffer, buf);
    
    /* Format the value based on type */
    if (setting->sensitive && ch->pcdata->security < 10) {
        sprintf(value_buf, "{D*****{x");
    }
    else {
        switch (setting->type) {
            case SETTING_TYPE_BOOL:
                sprintf(value_buf, "%s", *(bool *)setting->ptr ? "{Gtrue{x" : "{Rfalse{x");
                break;
            case SETTING_TYPE_INT:
                sprintf(value_buf, "{Y%d{x", *(int *)setting->ptr);
                break;
            case SETTING_TYPE_STRING:
                if (*(char **)setting->ptr && **(char **)setting->ptr)
                    sprintf(value_buf, "{W%s{x", *(char **)setting->ptr);
                else
                    sprintf(value_buf, "{D(empty){x");
                break;
            default:
                sprintf(value_buf, "{D(unknown){x");
                break;
        }
    }
    
    /* If there's a pending change, show it */
    if (has_change) {
        char old_value[MAX_STRING_LENGTH];
        strcpy(old_value, value_buf);
        
        if (setting->sensitive && ch->pcdata->security < 10) {
            sprintf(value_buf, "%s → {Y*****{x", old_value);
        } else {
            sprintf(value_buf, "%s → {Y%s{x", old_value, change_value);
        }
    }
    
    sprintf(buf, "{Y║{x  %s", value_buf);
    pad_string(buf, 38, NULL, NULL);
    add_buf(buffer, buf);
    
    /* Add type and flags in the last column */
    sprintf(type_buf, "{B%s{x", setting_type_names[setting->type]);
    
    if (setting->requires_reboot)
        strcat(type_buf, " {R*{x");
    
    if (setting->sensitive)
        strcat(type_buf, " {M§{x");
        
    if (!setting->olc_settable)
        strcat(type_buf, " {D✘{x");
    
    sprintf(buf, "{Y║{x  %s", type_buf);
    pad_string(buf, 21, NULL, NULL);
    strcat(buf, "{Y║{x\n\r");
    add_buf(buffer, buf);
}

/*
 * Display a category of settings
 */
/*
 * Display a category of settings
 */
void gameedit_display_category(BUFFER *buffer, CHAR_DATA *ch, int category)
{
    char buf[MAX_STRING_LENGTH];
    int i;
    bool found = FALSE;
    
    sprintf(buf, "\n\r{Y╔══════════════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║ %-78s ║{x\n\r", setting_category_names[category]);
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠════════════════════════╦═══════════════════════════════════════╦═══════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y║  %-21s ║  %-35s ║  %-16s ║{x\n\r", "Setting Name", "Value", "Type");
    add_buf(buffer, buf);
    
    sprintf(buf, "{Y╠════════════════════════╬═══════════════════════════════════════╬═══════════════════╣{x\n\r");
    add_buf(buffer, buf);
    
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        if (game_settings_table[i].category == category) {
            gameedit_display_setting(buffer, ch, &game_settings_table[i]);
            found = TRUE;
        }
    }
    
    if (!found) {
        sprintf(buf, "{Y║{x  %-76s  {Y║{x\n\r", "No settings in this category.");
        add_buf(buffer, buf);
    }
    
    sprintf(buf, "{Y╚════════════════════════╩═══════════════════════════════════════╩═══════════════════╝{x\n\r");
    add_buf(buffer, buf);
    
    /* Add a legend for the symbols */
    sprintf(buf, "  Legend: {R*{x Requires reboot   {M§{x Sensitive setting   {D✘{x Not settable via OLC\n\r\n\r");
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

