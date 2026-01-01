/***************************************************************************
 *                                                                         *
 *    Account notes system - allows staff to add notes to accounts         *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <zlib.h>
/* VIZZWILDS - support for plogf() and printf_to_char() functions*/
#include <stdarg.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../strings.h"
#include "../merc.h"
#include "../interp.h"
#include "../recycle.h"
#include "../scripts.h"
#include "../tables.h"
#include "../wilds.h"
#include "../protocol.h"
#include "../olc.h"


// Forward declarations
void free_account_note(ACCOUNT_NOTE_DATA *note);
void string_append_note(CHAR_DATA *ch, char **pString);

/*
 * Free a single note
 */
void free_account_note(ACCOUNT_NOTE_DATA *note)
{
    if (!note)
        return;
        
    free_string(note->author);
    free_string(note->subject);
    free_string(note->text);
    free_mem(note, sizeof(ACCOUNT_NOTE_DATA));
}

/*
 * Free all notes for an account
 */
void free_all_account_notes(ACCOUNT_DATA *account)
{
    ACCOUNT_NOTE_DATA *note, *note_next;
    
    if (!account)
        return;
    
    for (note = account->staff_notes; note != NULL; note = note_next)
    {
        note_next = note->next;
        free_account_note(note);
    }
    
    account->staff_notes = NULL;
}

/*
 * Main command handler for account notes
 */
void do_accnote(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_DATA *account;
    ACCOUNT_NOTE_DATA *note, *prev;
    int count, note_num;
    bool loaded = false;
    
    // Only staff can use this command
    if (!IS_IMMORTAL(ch))
    {
        send_to_char("Huh?\n\r", ch);
        return;
    }
    
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    
    if (arg1[0] == '\0')
    {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  accnote list <account>     - List all notes on an account\n\r", ch);
        send_to_char("  accnote list player:<name> - List notes for player's account\n\r", ch);
        send_to_char("  accnote add <account> <subject> - Add a note to an account\n\r", ch);
        send_to_char("  accnote add player:<name> <subject> - Add note to player's account\n\r", ch);
        send_to_char("  accnote view <account> <#> - View a specific note\n\r", ch);
        send_to_char("  accnote remove <account> <#> - Remove a note\n\r", ch);
        return;
    }
    
    // LIST notes for an account
    if (!str_cmp(arg1, "list"))
    {
        if (arg2[0] == '\0')
        {
            send_to_char("List notes for which account or player:name?\n\r", ch);
            return;
        }
        
        account = get_account_by_identifier(arg2, &loaded);
        if (account == NULL)
        {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try using player:<name> to look up by character name.\n\r", ch);
            return;
        }
        
        send_to_char(formatf("Account notes for: %s\n\r", account->username), ch);
        send_to_char("---------------------------------------------\n\r", ch);
        
        count = 0;
        for (note = account->staff_notes; note != NULL; note = note->next)
        {
            count++;
            struct tm *note_time = localtime(&note->timestamp);
            sprintf(buf, "[%2d] %-20s %-12s %02d/%02d/%04d\n\r",
                count, note->subject, note->author,
                note_time->tm_mon + 1, note_time->tm_mday, note_time->tm_year + 1900);
            send_to_char(buf, ch);
        }
        
        if (count == 0)
            send_to_char("No notes found for this account.\n\r", ch);
            
        return;
    }
    
    // ADD a note to an account
    if (!str_cmp(arg1, "add"))
    {
        if (arg2[0] == '\0')
        {
            send_to_char("Add a note to which account or player:name?\n\r", ch);
            return;
        }
        
        account = get_account_by_identifier(arg2, &loaded);
        if (account == NULL)
        {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try using player:<name> to look up by character name.\n\r", ch);
            return;
        }

    if (loaded && loaded_accounts) {
        // First remove it if it exists (just to be safe)
        list_remlink(loaded_accounts, account, NULL);
        // Then add it back
        list_appendlink(loaded_accounts, account);
    }
        
        // Check if we have a subject (arg3 or combined arg3+argument)
        if (arg3[0] == '\0')
        {
            send_to_char("You must provide a subject for the note.\n\r", ch);
            return;
        }
        
        // Combine arg3 and any remaining arguments for the subject
        char subject[MAX_STRING_LENGTH];
        strcpy(subject, arg3);
        
        if (argument[0] != '\0')
        {
            strcat(subject, " ");
            strcat(subject, argument);
        }
        
        note = alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
        note->author = str_dup(ch->name);
        note->subject = str_dup(subject);
        note->text = str_dup("");
        note->timestamp = current_time;
        
        // Add at the beginning of the list
        note->next = account->staff_notes;
        account->staff_notes = note;
        
        send_to_char("Note added. Now enter the text of the note.\n\r", ch);
        send_to_char("Type @ on a line by itself when done.\n\r", ch);
        
        // Save the account info for string editing
        ch->desc->editor = ED_ACCNOTE;
        ch->desc->pString = &note->text;
        ch->desc->editor_ptr = account;  // Store account pointer for saving later
        
        string_append(ch, &note->text);
        return;
    }
    
    // VIEW a specific note
    if (!str_cmp(arg1, "view"))
    {
        if (arg2[0] == '\0')
        {
            send_to_char("View notes for which account or player:name?\n\r", ch);
            return;
        }
        
        account = get_account_by_identifier(arg2, &loaded);
        if (account == NULL)
        {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try using player:<name> to look up by character name.\n\r", ch);
            return;
        }
        
        if (arg3[0] == '\0' || !is_number(arg3))
        {
            send_to_char("View which note number?\n\r", ch);
            return;
        }
        
        note_num = atoi(arg3);
        count = 0;
        
        for (note = account->staff_notes; note != NULL; note = note->next)
        {
            count++;
            if (count == note_num)
                break;
        }
        
        if (note == NULL)
        {
            send_to_char("No such note found.\n\r", ch);
            return;
        }
        
        struct tm *note_time = localtime(&note->timestamp);
        send_to_char("----------------------------------------------------\n\r", ch);
        sprintf(buf, "Note: %d  Subject: %s\n\r", note_num, note->subject);
        send_to_char(buf, ch);
        sprintf(buf, "Author: %s  Date: %02d/%02d/%04d %02d:%02d\n\r", 
            note->author, note_time->tm_mon + 1, note_time->tm_mday, 
            note_time->tm_year + 1900, note_time->tm_hour, note_time->tm_min);
        send_to_char(buf, ch);
        send_to_char("----------------------------------------------------\n\r", ch);
        send_to_char(note->text, ch);
        send_to_char("\n\r----------------------------------------------------\n\r", ch);
        return;
    }
    
    // REMOVE a note
    if (!str_cmp(arg1, "remove"))
    {
        if (arg2[0] == '\0')
        {
            send_to_char("Remove a note from which account or player:name?\n\r", ch);
            return;
        }
        
        account = get_account_by_identifier(arg2, &loaded);
        if (account == NULL)
        {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try using player:<name> to look up by character name.\n\r", ch);
            return;
        }
        
        if (arg3[0] == '\0' || !is_number(arg3))
        {
            send_to_char("Remove which note number?\n\r", ch);
            return;
        }
        
        // Check for appropriate staff rank to remove notes
        if (get_staff_rank(ch) < STAFF_IMMORTAL)
        {
            send_to_char("You are not of sufficient rank to remove account notes.\n\r", ch);
            return;
        }
        
        note_num = atoi(arg3);
        
        if (note_num <= 0)
        {
            send_to_char("Note numbers start at 1.\n\r", ch);
            return;
        }
        
        count = 0;
        prev = NULL;
        
        for (note = account->staff_notes; note != NULL; note = note->next)
        {
            count++;
            if (count == note_num)
                break;
            prev = note;
        }
        
        if (note == NULL)
        {
            send_to_char("No such note found.\n\r", ch);
            return;
        }
        
        // Remove the note from the linked list
        if (prev == NULL)
            account->staff_notes = note->next;
        else
            prev->next = note->next;
            
        // Free the note
        free_account_note(note);
        
        send_to_char("Note removed.\n\r", ch);
        
        // Save the account 
        save_account(account);
        return;
    }
    
    // If we get here, invalid syntax
    send_to_char("Invalid accnote command. Type 'accnote' for help.\n\r", ch);
}
/*
 * Hook to add to the string editor - for saving after note text is added
 */
void string_end_accnote(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) {
        bug("string_end_accnote: NULL character or descriptor", 0);
        return;
    }
    
    ACCOUNT_DATA *account = (ACCOUNT_DATA *)ch->desc->editor_ptr;
    
    if (!account) {
        bug("string_end_accnote: NULL account", 0);
        return;
    }

    // First check if account is still valid
    bool found = false;
    if (loaded_accounts) {
        ITERATOR it;
        ACCOUNT_DATA *acc;
        iterator_start(&it, loaded_accounts);
        while ((acc = (ACCOUNT_DATA *)iterator_nextdata(&it))) {
            if (acc == account) {
                found = true;
                break;
            }
        }
        iterator_stop(&it);
    }

    // If not found, log it but continue (we'll save anyway)
    if (!found) {
        bug("string_end_accnote: account not in loaded_accounts list", 0);
        // Add it back to the list
        if (loaded_accounts)
            list_appendlink(loaded_accounts, account);
    }
    
    // Save account before doing anything else with the descriptor
    save_account(account);

    // Reset the descriptor's string editing state
    ch->desc->pString = NULL;
    ch->desc->editor = 0;

    // Clear the editor_ptr *after* saving and resetting the string editor
    ch->desc->editor_ptr = NULL;
    
    // Only now notify the player
    send_to_char("\n\rNote text saved.\n\r", ch);
}