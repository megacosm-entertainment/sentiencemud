#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../merc.h"
#include "../recycle.h"
#include "nanny_utils.h"
#include "nanny_menus.h"

/*
 * Nanny Menu System Implementation
 *
 * This module contains the extracted handler functions from the mega-functions
 * login_account_menu() and login_character_menu().
 */

/*
 * ============================================================================
 * ACCOUNT MENU HANDLERS
 * ============================================================================
 */

/*
 * Handle: Create new regular character (option 'C')
 */
void handle_account_create_character(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    /* Check email verification requirement */
    if (game_settings.require_email_verif && !IS_EMAIL_VERIFIED(acct)) {
        write_to_buffer(d, "\n\rYou must verify your email address before creating characters.\n\r", 0);
        write_to_buffer(d, "Select 'V' from the menu to verify your email address.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Check new character lock */
    if (!account_has_immortal(acct) && game_settings.new_char_lock) {
        if (!IS_NULLSTR(game_settings.new_char_lock_msg))
            write_to_buffer(d, game_settings.new_char_lock_msg, 0);
        else
            write_to_buffer(d, "New characters are not being accepted at this time.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Check character limit */
    int max_chars = (acct->character_limit > 0) ? acct->character_limit : game_settings.max_characters;
    int nonstaff_count = account_count_nonstaff_characters(acct);

    if (max_chars > 0 && nonstaff_count >= max_chars) {
        write_to_buffer(d, "\n\r{RYou have reached the maximum number of characters for your account.{x\n\r", 0);
        write_to_buffer(d, "Delete an existing character or contact staff for assistance.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* All checks passed - start character creation */
    nanny_transition(d, CON_CREATING_NEW_CHAR);
}

/*
 * Handle: Create new staff character (option 'I')
 */
void handle_account_create_staff(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    /* Check permission */
    if (!IS_SET(acct->acct_flags, ACCT_CAN_CREATE_STAFF)) {
        write_to_buffer(d, "\n\r{RYou do not have permission to create staff characters.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Check staff limit */
    int staff_limit = acct->staff_limit;
    int staff_count = account_count_staff_characters(acct);

    if (staff_limit > 0 && staff_count >= staff_limit) {
        write_to_buffer(d, "\n\r{RYou have reached your staff character limit for this account.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* All checks passed - start staff character creation */
    nanny_transition(d, CON_CREATING_NEW_STAFF_CHAR);
}

/*
 * Handle: Link existing character (option 'L')
 */
void handle_account_link_character(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    if (!can_link_characters(acct)) {
        write_to_buffer(d, "Linking is not available for your account.\n\r", 0);
        display_account_menu(d);
        return;
    }

    nanny_transition(d, CON_LINK_CHARACTER_NAME);
}

/*
 * Handle: Change account email (option 'E')
 */
void handle_account_change_email(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    write_to_buffer(d, "\n\rCurrent email: ", 0);
    write_to_buffer(d, IS_NULLSTR(acct->email) ? "Not set\n\r" : acct->email, 0);
    nanny_transition(d, CON_CHANGE_ACCOUNT_EMAIL);
}

/*
 * Handle: Change account password (option 'P')
 */
void handle_account_change_password(DESCRIPTOR_DATA *d)
{
    if (should_skip_password()) {
        write_to_buffer(d, "\n\rPasswords are disabled in development mode.\n\r", 0);
        display_account_menu(d);
        return;
    }

    nanny_echo_off(d);
    nanny_transition(d, CON_VERIFY_ACCOUNT_PASSWORD);
}

/*
 * Handle: Account MFA menu (option 'M')
 */
void handle_account_mfa_menu(DESCRIPTOR_DATA *d)
{
    if (should_skip_mfa()) {
        write_to_buffer(d, "\n\rMFA is disabled in development mode.\n\r", 0);
        display_account_menu(d);
        return;
    }

    nanny_transition(d, CON_ACCOUNT_MFA_MENU);
}

/*
 * Handle: Verify email (option 'V')
 */
void handle_account_verify_email(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail verification is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (acct->email_verified) {
        write_to_buffer(d, "\n\rYour email is already verified.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Start email verification flow */
    nanny_transition(d, CON_VERIFY_ACCOUNT_EMAIL_CHANGE);
}

/*
 * Handle: Logout (option 'Q')
 */
void handle_account_logout(DESCRIPTOR_DATA *d)
{
    write_to_buffer(d, "\n\rGoodbye!\n\r", 0);
    close_socket(d);
}

/*
 * Handle: Select character by number
 * This is called when user enters a number to select a character
 */
void handle_account_select_character(DESCRIPTOR_DATA *d, int char_num)
{
    ACCOUNT_DATA *acct = d->account;
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    int count = 0;
    ACCOUNT_CHARACTER *selected = NULL;

    if (!acct) {
        nanny_send_error(d, "No account loaded.");
        return;
    }

    /* Find the character by number */
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        count++;
        if (count == char_num) {
            selected = ch_entry;
            break;
        }
    }
    iterator_stop(&it);

    if (!selected) {
        write_to_buffer(d, "\n\r{RInvalid character number.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Check if character is deleted */
    if (selected->deleted) {
        write_to_buffer(d, "\n\r{RThis character has been deleted.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Check if already online */
    if (is_character_online(selected->name)) {
        write_to_buffer(d, "\n\rThat character is already logged in.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Clear any existing character data */
    if (d->character) {
        free_char(d->character);
        d->character = NULL;
    }

    /* Load the character */
    if (!load_char_obj(d, selected->name)) {
        write_to_buffer(d, "\n\r{RFailed to load character. Please contact staff.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (!d->character) {
        write_to_buffer(d, "\n\r{RCharacter data failed to load. Please contact staff.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Transition to character menu */
    nanny_transition(d, CON_CHARACTER_MENU);
}

/*
 * ============================================================================
 * CHARACTER MENU HANDLERS
 * ============================================================================
 */

/*
 * Handle: Enter game (option 'E')
 */
void handle_character_enter_game(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;

    if (!ch) {
        nanny_send_error(d, "No character loaded.");
        nanny_return_to_account_menu(d);
        return;
    }

    /* Character will enter game - this is handled by the calling code */
    /* Just mark that we're ready to play */
    d->connected = CON_READ_MOTD;
}

/*
 * Handle: Change character password (option 'P')
 */
void handle_character_change_password(DESCRIPTOR_DATA *d)
{
    if (should_skip_password()) {
        write_to_buffer(d, "\n\rPasswords are disabled in development mode.\n\r", 0);
        display_character_menu(d);
        return;
    }

    nanny_echo_off(d);
    nanny_transition(d, CON_CHARACTER_PASSWORD);
}

/*
 * Handle: Character MFA menu (option 'M')
 */
void handle_character_mfa_menu(DESCRIPTOR_DATA *d)
{
    if (should_skip_mfa()) {
        write_to_buffer(d, "\n\rMFA is disabled in development mode.\n\r", 0);
        display_character_menu(d);
        return;
    }

    nanny_transition(d, CON_CHARACTER_MFA_MENU);
}

/*
 * Handle: Change character email (option 'A')
 */
void handle_character_change_email(DESCRIPTOR_DATA *d)
{
    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail is not enabled.\n\r", 0);
        display_character_menu(d);
        return;
    }

    nanny_transition(d, CON_CHANGE_CHARACTER_EMAIL);
}

/*
 * Handle: Delete character (option 'D')
 */
void handle_character_delete(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;

    if (!ch) {
        nanny_send_error(d, "No character loaded.");
        nanny_return_to_account_menu(d);
        return;
    }

    /* Staff characters require special handling */
    if (ch->pcdata && ch->pcdata->staff_rank >= STAFF_IMMORTAL) {
        write_to_buffer(d, "\n\r{RStaff characters cannot be deleted through this menu.{x\n\r", 0);
        write_to_buffer(d, "Please contact administration if you need to delete a staff character.\n\r", 0);
        display_character_menu(d);
        return;
    }

    nanny_transition(d, CON_CHARACTER_DELETE);
}

/*
 * Handle: Unlink character (option 'U')
 */
void handle_character_unlink(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    if (!ch || !acct) {
        nanny_send_error(d, "Character or account not loaded.");
        nanny_return_to_account_menu(d);
        return;
    }

    /* Staff characters cannot be unlinked */
    if (ch->pcdata && ch->pcdata->staff_rank >= STAFF_IMMORTAL) {
        write_to_buffer(d, "\n\r{RStaff characters cannot be unlinked.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    /* Check if unlinking is allowed */
    if (!can_link_characters(acct)) {
        write_to_buffer(d, "\n\rUnlinking is not available.\n\r", 0);
        display_character_menu(d);
        return;
    }

    nanny_transition(d, CON_VERIFY_UNLINK_PASSWORD);
}

/*
 * Handle: Return to account menu (option 'B')
 */
void handle_character_back_to_account(DESCRIPTOR_DATA *d)
{
    /* Clean up character and return to account menu */
    nanny_cleanup_character(d);
    nanny_return_to_account_menu(d);
}

/*
 * Handle: Set character as default (option 'S')
 */
void handle_character_set_default(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    if (!ch || !acct) {
        nanny_send_error(d, "Character or account not loaded.");
        return;
    }

    /* Set as default character */
    if (acct->default_character) {
        free_string(acct->default_character);
    }
    acct->default_character = str_dup(ch->name);
    save_account(acct);

    write_to_buffer(d, "\n\r{GDefault character set.{x\n\r", 0);
    display_character_menu(d);
}
