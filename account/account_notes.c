/***************************************************************************
 *                                                                         *
 *    Account/character notes system - staff notes on accounts and chars   *
 *                                                                         *
 *    Supports categorized notes (info, warning, punishment, reward),      *
 *    note editing, and both account-level and character-level notes.      *
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
#include "penalty.h"

/* Externs */
extern void save_account(ACCOUNT_DATA *account);
extern ACCOUNT_DATA *get_account_by_identifier(const char *arg, bool *loaded);
extern LLIST *loaded_accounts;

/***************************************************************************
 * Note Category Helpers                                                   *
 ***************************************************************************/

static const struct {
    const char *name;
    const char *color;
    const char *symbol;
} note_categories[] = {
    { "info",       "{C", "{C*{x" },  /* NOTE_CAT_INFO       */
    { "warning",    "{Y", "{Y!{x" },  /* NOTE_CAT_WARNING    */
    { "punishment", "{R", "{R-{x" },  /* NOTE_CAT_PUNISHMENT */
    { "reward",     "{G", "{G+{x" },  /* NOTE_CAT_REWARD     */
};

/**
 * note_category_name - Get the display name for a note category
 *
 * @param category  NOTE_CAT_* constant
 * @return          Category name string, or "unknown"
 */
const char *note_category_name(int category)
{
    if (category >= 0 && category < NOTE_CAT_MAX)
        return note_categories[category].name;
    return "unknown";
}

/**
 * note_category_lookup - Look up a category by name prefix
 *
 * @param name  Category name (prefix match supported)
 * @return      NOTE_CAT_* constant, or -1 if not found
 */
int note_category_lookup(const char *name)
{
    int i;
    if (!name || !name[0])
        return -1;

    for (i = 0; i < NOTE_CAT_MAX; i++) {
        if (!str_prefix(name, note_categories[i].name))
            return i;
    }
    return -1;
}

static const char *note_category_color(int category)
{
    if (category >= 0 && category < NOTE_CAT_MAX)
        return note_categories[category].color;
    return "{w";
}

static const char *note_category_symbol(int category)
{
    if (category >= 0 && category < NOTE_CAT_MAX)
        return note_categories[category].symbol;
    return " ";
}

/***************************************************************************
 * Note Memory Management                                                  *
 ***************************************************************************/

/**
 * free_account_note - Free a single note and all its strings
 *
 * @param note  The note to free
 */
void free_account_note(ACCOUNT_NOTE_DATA *note)
{
    if (!note)
        return;

    free_string(note->author);
    free_string(note->subject);
    free_string(note->text);
    if (note->penalty_ref)
        free_string(note->penalty_ref);
    free_mem(note, sizeof(ACCOUNT_NOTE_DATA));
}

/**
 * free_all_account_notes - Free all notes on an account
 *
 * @param account  The account whose notes to free
 */
void free_all_account_notes(ACCOUNT_DATA *account)
{
    ACCOUNT_NOTE_DATA *note, *note_next;

    if (!account)
        return;

    for (note = account->staff_notes; note != NULL; note = note_next) {
        note_next = note->next;
        free_account_note(note);
    }

    account->staff_notes = NULL;
}

/***************************************************************************
 * Note Counting                                                           *
 ***************************************************************************/

/**
 * account_note_count - Count total notes in a linked list
 *
 * @param notes  Head of note linked list
 * @return       Number of notes
 */
int account_note_count(ACCOUNT_NOTE_DATA *notes)
{
    int count = 0;
    ACCOUNT_NOTE_DATA *note;
    for (note = notes; note; note = note->next)
        count++;
    return count;
}

/**
 * account_note_count_by_category - Count notes of a specific category
 *
 * @param notes     Head of note linked list
 * @param category  NOTE_CAT_* constant
 * @return          Number of matching notes
 */
int account_note_count_by_category(ACCOUNT_NOTE_DATA *notes, int category)
{
    int count = 0;
    ACCOUNT_NOTE_DATA *note;
    for (note = notes; note; note = note->next) {
        if (note->category == category)
            count++;
    }
    return count;
}

/***************************************************************************
 * Shared Note Display/Operation Helpers                                   *
 ***************************************************************************/

/**
 * get_note_by_number - Find a note by its 1-based number in the list
 *
 * @param head     Head of note linked list
 * @param num      1-based note number
 * @param prev_out If non-NULL, receives the previous note (for unlinking)
 * @return         The note, or NULL if not found
 */
static ACCOUNT_NOTE_DATA *get_note_by_number(ACCOUNT_NOTE_DATA *head, int num,
                                              ACCOUNT_NOTE_DATA **prev_out)
{
    ACCOUNT_NOTE_DATA *note, *prev = NULL;
    int count = 0;

    for (note = head; note; note = note->next) {
        count++;
        if (count == num) {
            if (prev_out)
                *prev_out = prev;
            return note;
        }
        prev = note;
    }
    return NULL;
}

/**
 * display_note_list - Show a list header and all notes
 *
 * @param ch        Staff character viewing
 * @param notes     Head of note linked list
 * @param label     Description label (e.g. "Account: foo")
 */
static void display_note_list(CHAR_DATA *ch, ACCOUNT_NOTE_DATA *notes,
                              const char *label)
{
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_NOTE_DATA *note;
    int count = 0;

    send_to_char(formatf("{wNotes for: {C%s{x\n\r", label), ch);
    send_to_char("{D------------------------------------------------------{x\n\r", ch);
    send_to_char("{D  #  Cat          Subject              Author       Date{x\n\r", ch);
    send_to_char("{D------------------------------------------------------{x\n\r", ch);

    for (note = notes; note; note = note->next) {
        count++;
        struct tm *t = localtime(&note->timestamp);
        sprintf(buf, " %s {w%2d{x  %s%-11s{x %-20s %-12s %02d/%02d/%04d\n\r",
            note_category_symbol(note->category),
            count,
            note_category_color(note->category),
            note_category_name(note->category),
            note->subject,
            note->author,
            t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
        send_to_char(buf, ch);
    }

    if (count == 0)
        send_to_char("  No notes found.\n\r", ch);

    send_to_char("{D------------------------------------------------------{x\n\r", ch);
}

/**
 * display_note_detail - Show full details of a single note
 *
 * @param ch    Staff character viewing
 * @param note  The note to display
 * @param num   The note number (for display)
 */
static void display_note_detail(CHAR_DATA *ch, ACCOUNT_NOTE_DATA *note, int num)
{
    char buf[MAX_STRING_LENGTH];
    struct tm *t = localtime(&note->timestamp);

    send_to_char("{D------------------------------------------------------{x\n\r", ch);
    sprintf(buf, "Note: {w%d{x   Category: %s%s{x\n\r",
        num, note_category_color(note->category),
        capitalize(note_category_name(note->category)));
    send_to_char(buf, ch);
    sprintf(buf, "Subject: {W%s{x\n\r", note->subject);
    send_to_char(buf, ch);
    sprintf(buf, "Author: {C%s{x   Date: %02d/%02d/%04d %02d:%02d\n\r",
        note->author, t->tm_mon + 1, t->tm_mday,
        t->tm_year + 1900, t->tm_hour, t->tm_min);
    send_to_char(buf, ch);
    if (!IS_NULLSTR(note->penalty_ref)) {
        sprintf(buf, "Linked penalty: {Y%s{x\n\r", note->penalty_ref);
        send_to_char(buf, ch);
    }
    send_to_char("{D------------------------------------------------------{x\n\r", ch);
    send_to_char(note->text, ch);
    send_to_char("\n\r{D------------------------------------------------------{x\n\r", ch);
}

/**
 * ensure_loaded_account - Ensure account is in loaded_accounts for saving
 *
 * @param account  The account to ensure is tracked
 * @param loaded   Whether it was freshly loaded by get_account_by_identifier
 */
static void ensure_loaded_account(ACCOUNT_DATA *account, bool loaded)
{
    if (loaded && loaded_accounts) {
        list_remlink(loaded_accounts, account, NULL);
        list_appendlink(loaded_accounts, account);
    }
}

/***************************************************************************
 * do_accnote - Staff command for account-level notes                      *
 ***************************************************************************/

/**
 * do_accnote - Manage staff notes on player accounts
 *
 * Supports list, add, view, edit, remove subcommands with note categories.
 *
 * Syntax:
 *   accnote list <account|player:name>
 *   accnote add <account|player:name> [category] <subject>
 *   accnote view <account|player:name> <#>
 *   accnote edit <account|player:name> <#>
 *   accnote remove <account|player:name> <#>
 *
 * @param ch        The staff character
 * @param argument  Command arguments
 */
void do_accnote(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    ACCOUNT_DATA *account;
    ACCOUNT_NOTE_DATA *note, *prev;
    int note_num;
    bool loaded = false;

    if (!IS_IMMORTAL(ch)) {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  accnote list <account|player:name>\n\r", ch);
        send_to_char("  accnote add <account|player:name> [category] <subject>\n\r", ch);
        send_to_char("  accnote view <account|player:name> <#>\n\r", ch);
        send_to_char("  accnote edit <account|player:name> <#>\n\r", ch);
        send_to_char("  accnote remove <account|player:name> <#>\n\r", ch);
        send_to_char("\n\rCategories: info, warning, punishment, reward\n\r", ch);
        return;
    }

    /* ---- LIST ---- */
    if (!str_cmp(arg1, "list")) {
        if (arg2[0] == '\0') {
            send_to_char("List notes for which account?\n\r", ch);
            return;
        }

        account = get_account_by_identifier(arg2, &loaded);
        if (!account) {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try player:<name>.\n\r", ch);
            return;
        }

        display_note_list(ch, account->staff_notes, account->username);
        return;
    }

    /* ---- ADD ---- */
    if (!str_cmp(arg1, "add")) {
        if (arg2[0] == '\0') {
            send_to_char("Add a note to which account?\n\r", ch);
            return;
        }

        account = get_account_by_identifier(arg2, &loaded);
        if (!account) {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try player:<name>.\n\r", ch);
            return;
        }

        ensure_loaded_account(account, loaded);

        /* Parse optional category and subject */
        argument = one_argument(argument, arg3);
        int category = note_category_lookup(arg3);
        char subject[MAX_STRING_LENGTH];

        if (category >= 0) {
            /* arg3 was a category, rest is subject */
            if (argument[0] == '\0') {
                send_to_char("You must provide a subject for the note.\n\r", ch);
                return;
            }
            strcpy(subject, argument);
        } else {
            /* arg3 is part of the subject, default category */
            if (arg3[0] == '\0') {
                send_to_char("You must provide a subject for the note.\n\r", ch);
                return;
            }
            category = NOTE_CAT_INFO;
            strcpy(subject, arg3);
            if (argument[0] != '\0') {
                strcat(subject, " ");
                strcat(subject, argument);
            }
        }

        note = alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
        note->author = str_dup(ch->name);
        note->subject = str_dup(subject);
        note->text = str_dup("");
        note->timestamp = current_time;
        note->category = category;
        note->penalty_ref = NULL;
        note->next = account->staff_notes;
        account->staff_notes = note;

        send_to_char(formatf("Note added (%s%s{x). Enter the text of the note.\n\r",
            note_category_color(category), note_category_name(category)), ch);
        send_to_char("Type @ on a line by itself when done.\n\r", ch);

        ch->desc->editor = ED_ACCNOTE;
        ch->desc->pString = &note->text;
        ch->desc->editor_ptr = account;
        string_append(ch, &note->text);
        return;
    }

    /* ---- VIEW ---- */
    if (!str_cmp(arg1, "view")) {
        if (arg2[0] == '\0') {
            send_to_char("View notes for which account?\n\r", ch);
            return;
        }

        account = get_account_by_identifier(arg2, &loaded);
        if (!account) {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try player:<name>.\n\r", ch);
            return;
        }

        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("View which note number?\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        note = get_note_by_number(account->staff_notes, note_num, NULL);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        display_note_detail(ch, note, note_num);
        return;
    }

    /* ---- EDIT ---- */
    if (!str_cmp(arg1, "edit")) {
        if (arg2[0] == '\0') {
            send_to_char("Edit a note on which account?\n\r", ch);
            return;
        }

        account = get_account_by_identifier(arg2, &loaded);
        if (!account) {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try player:<name>.\n\r", ch);
            return;
        }

        ensure_loaded_account(account, loaded);

        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("Edit which note number?\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        note = get_note_by_number(account->staff_notes, note_num, NULL);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        send_to_char(formatf("Editing note %d: %s\n\r", note_num, note->subject), ch);

        ch->desc->editor = ED_ACCNOTE;
        ch->desc->pString = &note->text;
        ch->desc->editor_ptr = account;
        string_append(ch, &note->text);
        return;
    }

    /* ---- REMOVE ---- */
    if (!str_cmp(arg1, "remove")) {
        if (arg2[0] == '\0') {
            send_to_char("Remove a note from which account?\n\r", ch);
            return;
        }

        account = get_account_by_identifier(arg2, &loaded);
        if (!account) {
            if (!strncmp(arg2, "player:", 7))
                send_to_char("Player not found or has no account.\n\r", ch);
            else
                send_to_char("Account not found. Try player:<name>.\n\r", ch);
            return;
        }

        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("Remove which note number?\n\r", ch);
            return;
        }

        if (get_staff_rank(ch) < STAFF_IMMORTAL) {
            send_to_char("You are not of sufficient rank to remove account notes.\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        if (note_num <= 0) {
            send_to_char("Note numbers start at 1.\n\r", ch);
            return;
        }

        note = get_note_by_number(account->staff_notes, note_num, &prev);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        if (prev == NULL)
            account->staff_notes = note->next;
        else
            prev->next = note->next;

        free_account_note(note);
        send_to_char("Note removed.\n\r", ch);
        save_account(account);
        return;
    }

    send_to_char("Invalid accnote command. Type 'accnote' for help.\n\r", ch);
}

/***************************************************************************
 * do_charnote - Staff command for character-level notes                   *
 ***************************************************************************/

/**
 * find_account_character - Look up a character entry in an account by name
 *
 * @param account  The account to search
 * @param name     Character name to find
 * @return         The ACCOUNT_CHARACTER entry, or NULL
 */
static ACCOUNT_CHARACTER *find_account_character(ACCOUNT_DATA *account,
                                                  const char *name)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *ac;

    if (!account || !account->characters)
        return NULL;

    iterator_start(&it, account->characters);
    while ((ac = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (ac->name && !str_cmp(ac->name, name)) {
            iterator_stop(&it);
            return ac;
        }
    }
    iterator_stop(&it);
    return NULL;
}

/**
 * do_charnote - Manage staff notes on individual characters
 *
 * Notes are stored at the ACCOUNT_CHARACTER level in the account data,
 * persisted in the account JSON file.
 *
 * Syntax:
 *   charnote list <character>
 *   charnote add <character> [category] <subject>
 *   charnote view <character> <#>
 *   charnote edit <character> <#>
 *   charnote remove <character> <#>
 *
 * @param ch        The staff character
 * @param argument  Command arguments
 */
void do_charnote(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    ACCOUNT_DATA *account;
    ACCOUNT_CHARACTER *target;
    ACCOUNT_NOTE_DATA *note, *prev;
    int note_num;
    bool loaded = false;

    if (!IS_IMMORTAL(ch)) {
        send_to_char("Huh?\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  charnote list <character>\n\r", ch);
        send_to_char("  charnote add <character> [category] <subject>\n\r", ch);
        send_to_char("  charnote view <character> <#>\n\r", ch);
        send_to_char("  charnote edit <character> <#>\n\r", ch);
        send_to_char("  charnote remove <character> <#>\n\r", ch);
        send_to_char("\n\rCategories: info, warning, punishment, reward\n\r", ch);
        return;
    }

    if (arg2[0] == '\0') {
        send_to_char("Which character?\n\r", ch);
        return;
    }

    /* Look up via player:name to find the account, then the character */
    char lookup[MAX_INPUT_LENGTH + 16];
    snprintf(lookup, sizeof(lookup), "player:%s", arg2);
    account = get_account_by_identifier(lookup, &loaded);
    if (!account) {
        send_to_char("Character not found or has no account.\n\r", ch);
        return;
    }

    target = find_account_character(account, arg2);
    if (!target) {
        send_to_char(formatf("Character '%s' not found in account '%s'.\n\r",
            arg2, account->username), ch);
        return;
    }

    /* ---- LIST ---- */
    if (!str_cmp(arg1, "list")) {
        char label[MAX_INPUT_LENGTH];
        sprintf(label, "%s (account: %s)", target->name, account->username);
        display_note_list(ch, target->staff_notes, label);
        return;
    }

    /* ---- ADD ---- */
    if (!str_cmp(arg1, "add")) {
        ensure_loaded_account(account, loaded);

        argument = one_argument(argument, arg3);
        int category = note_category_lookup(arg3);
        char subject[MAX_STRING_LENGTH];

        if (category >= 0) {
            if (argument[0] == '\0') {
                send_to_char("You must provide a subject for the note.\n\r", ch);
                return;
            }
            strcpy(subject, argument);
        } else {
            if (arg3[0] == '\0') {
                send_to_char("You must provide a subject for the note.\n\r", ch);
                return;
            }
            category = NOTE_CAT_INFO;
            strcpy(subject, arg3);
            if (argument[0] != '\0') {
                strcat(subject, " ");
                strcat(subject, argument);
            }
        }

        note = alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
        note->author = str_dup(ch->name);
        note->subject = str_dup(subject);
        note->text = str_dup("");
        note->timestamp = current_time;
        note->category = category;
        note->penalty_ref = NULL;
        note->next = target->staff_notes;
        target->staff_notes = note;

        send_to_char(formatf("Note added to %s (%s%s{x). Enter the text.\n\r",
            target->name, note_category_color(category),
            note_category_name(category)), ch);
        send_to_char("Type @ on a line by itself when done.\n\r", ch);

        ch->desc->editor = ED_CHARNOTE;
        ch->desc->pString = &note->text;
        ch->desc->editor_ptr = account;
        string_append(ch, &note->text);
        return;
    }

    /* ---- VIEW ---- */
    if (!str_cmp(arg1, "view")) {
        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("View which note number?\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        note = get_note_by_number(target->staff_notes, note_num, NULL);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        display_note_detail(ch, note, note_num);
        return;
    }

    /* ---- EDIT ---- */
    if (!str_cmp(arg1, "edit")) {
        ensure_loaded_account(account, loaded);

        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("Edit which note number?\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        note = get_note_by_number(target->staff_notes, note_num, NULL);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        send_to_char(formatf("Editing note %d on %s: %s\n\r",
            note_num, target->name, note->subject), ch);

        ch->desc->editor = ED_CHARNOTE;
        ch->desc->pString = &note->text;
        ch->desc->editor_ptr = account;
        string_append(ch, &note->text);
        return;
    }

    /* ---- REMOVE ---- */
    if (!str_cmp(arg1, "remove")) {
        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0' || !is_number(arg3)) {
            send_to_char("Remove which note number?\n\r", ch);
            return;
        }

        if (get_staff_rank(ch) < STAFF_IMMORTAL) {
            send_to_char("You are not of sufficient rank to remove notes.\n\r", ch);
            return;
        }

        note_num = atoi(arg3);
        if (note_num <= 0) {
            send_to_char("Note numbers start at 1.\n\r", ch);
            return;
        }

        note = get_note_by_number(target->staff_notes, note_num, &prev);
        if (!note) {
            send_to_char("No such note found.\n\r", ch);
            return;
        }

        if (prev == NULL)
            target->staff_notes = note->next;
        else
            prev->next = note->next;

        free_account_note(note);
        send_to_char("Note removed.\n\r", ch);
        save_account(account);
        return;
    }

    send_to_char("Invalid charnote command. Type 'charnote' for help.\n\r", ch);
}

/***************************************************************************
 * String Editor Hooks                                                     *
 ***************************************************************************/

/**
 * string_end_accnote - Called when string editor finishes for account notes
 *
 * Saves the account and clears editor state.
 *
 * @param ch  The staff character who was editing
 */
void string_end_accnote(CHAR_DATA *ch)
{
    if (!ch || !ch->desc) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR,
            "string_end_accnote: NULL character or descriptor");
        return;
    }

    ACCOUNT_DATA *account = (ACCOUNT_DATA *)ch->desc->editor_ptr;

    if (!account) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR,
            "string_end_accnote: NULL account");
        return;
    }

    /* Ensure account is in loaded list for saving */
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
    if (!found && loaded_accounts)
        list_appendlink(loaded_accounts, account);

    save_account(account);

    ch->desc->pString = NULL;
    ch->desc->editor = 0;
    ch->desc->editor_ptr = NULL;

    send_to_char("\n\rNote text saved.\n\r", ch);
}

/**
 * string_end_charnote - Called when string editor finishes for character notes
 *
 * Identical to string_end_accnote since char notes are stored in the
 * account JSON — just saves the account.
 *
 * @param ch  The staff character who was editing
 */
void string_end_charnote(CHAR_DATA *ch)
{
    /* Character notes live in the account JSON, same save path */
    string_end_accnote(ch);
}

/***************************************************************************
 * Login Notification                                                      *
 ***************************************************************************/

/**
 * notify_staff_of_notes - Alert online staff when a noted player enters
 *
 * Called during proceed_to_game(). Sends a wiznet-style notification
 * to online staff about any warning/punishment notes on the player's
 * account or character.
 *
 * @param d  The descriptor of the player entering the game
 */
void notify_staff_of_notes(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch;
    char buf[MAX_STRING_LENGTH];
    int acct_warnings = 0, acct_punishments = 0;
    int char_warnings = 0, char_punishments = 0;
    ACCOUNT_NOTE_DATA *note;
    ACCOUNT_CHARACTER *ac;

    if (!d || !d->character || !d->account)
        return;

    ch = d->character;

    /* Count account-level notes of concern */
    for (note = d->account->staff_notes; note; note = note->next) {
        if (note->category == NOTE_CAT_WARNING)
            acct_warnings++;
        else if (note->category == NOTE_CAT_PUNISHMENT)
            acct_punishments++;
    }

    /* Find character entry and count character-level notes */
    ac = find_account_character(d->account, ch->name);
    if (ac) {
        for (note = ac->staff_notes; note; note = note->next) {
            if (note->category == NOTE_CAT_WARNING)
                char_warnings++;
            else if (note->category == NOTE_CAT_PUNISHMENT)
                char_punishments++;
        }
    }

    int total = acct_warnings + acct_punishments + char_warnings + char_punishments;
    if (total == 0)
        return;

    /* Build notification message */
    sprintf(buf, "{Y[NOTE]{x $N has staff notes:");
    if (acct_warnings > 0 || acct_punishments > 0) {
        char acct_buf[100];
        sprintf(acct_buf, " acct[{Y%dW{x/{R%dP{x]",
            acct_warnings, acct_punishments);
        strcat(buf, acct_buf);
    }
    if (char_warnings > 0 || char_punishments > 0) {
        char char_buf[100];
        sprintf(char_buf, " char[{Y%dW{x/{R%dP{x]",
            char_warnings, char_punishments);
        strcat(buf, char_buf);
    }

    wiznet(buf, ch, NULL, WIZ_LOGINS, 0, 0);
}

/***************************************************************************
 * Player-Visible Account Standing                                         *
 ***************************************************************************/

/**
 * do_standing - Show a player their own account standing
 *
 * Displays active penalties, bonuses, and any player-visible notes.
 * For staff, shows additional detail.
 *
 * @param ch        The character requesting standing info
 * @param argument  Unused
 */
void do_standing(CHAR_DATA *ch, char *argument)
{
    DESCRIPTOR_DATA *d;
    ACCOUNT_DATA *account;
    char buf[MAX_STRING_LENGTH];

    if (IS_NPC(ch)) {
        send_to_char("NPCs don't have accounts.\n\r", ch);
        return;
    }

    d = ch->desc;
    if (!d || !d->account) {
        send_to_char("No account information available.\n\r", ch);
        return;
    }

    account = d->account;

    send_to_char("{B=={W[ {YAccount Standing {W]{B=={x\n\r", ch);
    sprintf(buf, "Account: {C%s{x\n\r\n\r", account->username);
    send_to_char(buf, ch);

    /* Active Penalties */
    bool has_penalties = false;
    PENALTY_DATA *pen;
    for (pen = account->penalties; pen; pen = pen->next) {
        if (is_penalty_expired(pen))
            continue;
        if (!has_penalties) {
            send_to_char("{RActive Penalties:{x\n\r", ch);
            has_penalties = true;
        }
        char dur[64];
        if (pen->expires_at == 0)
            sprintf(dur, "{RPermanent{x");
        else
            penalty_format_duration(pen->expires_at - current_time, dur, sizeof(dur));

        if (pen->scope == PENALTY_SCOPE_CHARACTER && !IS_NULLSTR(pen->target_name)) {
            sprintf(buf, "   {R*{x %-14s ({Y%s{x) - %s\n\r",
                penalty_type_name(pen->type), pen->target_name, dur);
        } else {
            sprintf(buf, "   {R*{x %-14s - %s\n\r",
                penalty_type_name(pen->type), dur);
        }
        send_to_char(buf, ch);
    }

    /* Active Bonuses */
    bool has_bonuses = false;
    BONUS_DATA *bon;
    for (bon = account->bonuses; bon; bon = bon->next) {
        if (is_bonus_expired(bon))
            continue;
        if (!has_bonuses) {
            if (has_penalties)
                send_to_char("\n\r", ch);
            send_to_char("{GActive Bonuses:{x\n\r", ch);
            has_bonuses = true;
        }
        char dur[64];
        if (bon->expires_at == 0)
            sprintf(dur, "{GPermanent{x");
        else
            penalty_format_duration(bon->expires_at - current_time, dur, sizeof(dur));

        char scope_str[64];
        if (bon->scope == BONUS_SCOPE_CHARACTER && !IS_NULLSTR(bon->target_name))
            sprintf(scope_str, "({Y%s{x) ", bon->target_name);
        else
            scope_str[0] = '\0';

        sprintf(buf, "   {G+{x %-10s %+d%% %s- %s\n\r",
            bonus_type_name(bon->type), bon->value, scope_str, dur);
        send_to_char(buf, ch);
    }

    if (!has_penalties && !has_bonuses)
        send_to_char("   {wNo active penalties or bonuses.{x\n\r", ch);

    /* Player-visible notes (info and reward only) */
    bool has_notes = false;
    ACCOUNT_NOTE_DATA *note;
    for (note = account->staff_notes; note; note = note->next) {
        if (note->category == NOTE_CAT_INFO || note->category == NOTE_CAT_REWARD) {
            if (!has_notes) {
                send_to_char("\n\r{CAccount Notes:{x\n\r", ch);
                has_notes = true;
            }
            struct tm *t = localtime(&note->timestamp);
            sprintf(buf, "   %s %s%-11s{x %-20s %02d/%02d/%04d\n\r",
                note_category_symbol(note->category),
                note_category_color(note->category),
                note_category_name(note->category),
                note->subject,
                t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
            send_to_char(buf, ch);
        }
    }

    send_to_char("\n\r{B========================={x\n\r", ch);
}
