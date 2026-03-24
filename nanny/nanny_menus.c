#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../merc.h"
#include "../recycle.h"
#include "../protocol.h"
#include "nanny.h"
#include "nanny_utils.h"
#include "nanny_menus.h"
#include "../account/preferences.h"

/*
 * Nanny Menu System Implementation
 *
 * Each handler matches the exact behavior of the inline code that was
 * previously in login_account_menu() and login_character_menu().
 */

/*
 * ============================================================================
 * ACCOUNT MENU HANDLERS
 * ============================================================================
 */

/**
 * handle_account_create_character - Create new regular character (option 'C')
 *
 * Checks email verification, new character lock, and character limit
 * before transitioning to character creation flow.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_create_character(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (game_settings.require_email_verif && !IS_EMAIL_VERIFIED(acct)) {
        write_to_buffer(d, formatf("\n\r%s\n\r", localization_translate(d->lang, "error.msg.login.character.create.no_email")), 0);
        write_to_buffer(d, formatf("%s\n\r", localization_translate(d->lang, "error.msg.login.character.create.no_email")), 0);
        display_account_menu(d);
        return;
    }

    if (!account_has_immortal(acct) && game_settings.new_char_lock) {
        if (!IS_NULLSTR(game_settings.new_char_lock_msg))
            write_to_buffer(d, game_settings.new_char_lock_msg, 0);
        else
            write_to_buffer(d, "New characters are not being accepted at this time.\n\r", 0);
        display_account_menu(d);
        return;
    }

    {
        int max_chars = (acct->character_limit > 0) ? acct->character_limit : game_settings.max_characters;
        int nonstaff_count = account_count_nonstaff_characters(acct);
        if (max_chars > 0 && nonstaff_count >= max_chars) {
            write_to_buffer(d, "\n\r{RYou have reached the maximum number of characters for your account.{x\n\r", 0);
            write_to_buffer(d, "Delete an existing character or contact staff for assistance.\n\r", 0);
            display_account_menu(d);
            return;
        }
    }

    d->connected = CON_CREATING_NEW_CHAR;
}

/**
 * handle_account_create_staff - Create new staff character (option 'I')
 *
 * Checks staff creation permission and staff character limit.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_create_staff(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!IS_SET(acct->acct_flags, ACCT_CAN_CREATE_STAFF)) {
        write_to_buffer(d, "\n\r{RYou do not have permission to create staff characters.{x\n\r", 0);
        display_account_menu(d);
        return;
    }

    {
        int staff_limit = acct->staff_limit;
        int staff_count = account_count_staff_characters(acct);
        if (staff_limit > 0 && staff_count >= staff_limit) {
            write_to_buffer(d, "\n\r{RYou have reached your staff character limit for this account.{x\n\r", 0);
            display_account_menu(d);
            return;
        }
    }

    d->connected = CON_CREATING_NEW_STAFF_CHAR;
}

/**
 * handle_account_link_character - Link existing character to account (option 'L')
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_link_character(DESCRIPTOR_DATA *d)
{
    if (!can_link_characters(d->account)) {
        write_to_buffer(d, "Linking is not available for your account.\n\r", 0);
        display_account_menu(d);
        return;
    }

    d->connected = CON_LINK_CHARACTER_NAME;
}

/**
 * handle_account_change_email - Change account email (option 'E')
 *
 * Displays current email and transitions to email change flow.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_change_email(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    write_to_buffer(d, "\n\rCurrent email: ", 0);
    write_to_buffer(d, IS_NULLSTR(acct->email) ? "Not set\n\r" : acct->email, 0);
    d->connected = CON_CHANGE_ACCOUNT_EMAIL;
}

/**
 * handle_account_change_password - Change account password (option 'P')
 *
 * Disables echo and transitions to password verification flow.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_change_password(DESCRIPTOR_DATA *d)
{
    if (DEV_SKIP_PASSWORD) {
        write_to_buffer(d, "\n\rPasswords are disabled in development mode.\n\r", 0);
        display_account_menu(d);
        return;
    }

    ProtocolNoEcho(d, true);
    d->connected = CON_VERIFY_ACCOUNT_PASSWORD;
}

/**
 * handle_account_mfa_menu - Access account MFA settings (option 'M')
 *
 * Displays the MFA menu (which also sets the state to CON_ACCOUNT_MFA_MENU).
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_mfa_menu(DESCRIPTOR_DATA *d)
{
    if (DEV_SKIP_MFA) {
        write_to_buffer(d, "\n\rMFA is disabled in development mode.\n\r", 0);
        display_account_menu(d);
        return;
    }

    display_account_mfa_menu(d, "");
}

/**
 * handle_account_preferences - Access account preferences menu (option 'A')
 *
 * Displays the account preferences menu (which also sets the state to
 * CON_ACCOUNT_PREFS).
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_preferences(DESCRIPTOR_DATA *d)
{
    display_account_prefs_menu(d);
}

/**
 * handle_account_verify_email - Verify email address (option 'V')
 *
 * Checks email is enabled, not already verified, and has a pending email
 * before transitioning to the verification code entry flow.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_verify_email(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (acct->email_verified) {
        write_to_buffer(d, "\n\rYour email is already verified.\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (IS_NULLSTR(acct->pending_email)) {
        write_to_buffer(d, "No pending email to verify. Change your email address first.\n\r", 0);
        display_account_menu(d);
        return;
    }

    d->connected = CON_VERIFY_ACCOUNT_EMAIL_CHANGE;
}

/**
 * handle_account_resend_verification - Resend verification email (option 'R')
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_resend_verification(DESCRIPTOR_DATA *d)
{
    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\rEmail is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    resend_account_verification_code(d);
    display_account_menu(d);
}

/**
 * handle_account_shared_storage - Display shared storage info (option 'S')
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_shared_storage(DESCRIPTOR_DATA *d)
{
    if (!game_settings.vault_enabled) {
        write_to_buffer(d, "\n\rVault is not enabled.\n\r", 0);
        display_account_menu(d);
        return;
    }

    display_shared_storage_info(d);
}

/**
 * handle_account_logout - Disconnect from the server (option 'Q')
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_logout(DESCRIPTOR_DATA *d)
{
    write_to_buffer(d, "\n\rThank you for playing Sentience!\n\r", 0);
    close_socket(d);
}

/**
 * handle_account_default_character - Quick-login with default character (option 'Y')
 *
 * Finds the default character on the account and calls direct_login_character
 * to bypass the character menu.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_default_character(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *ch_entry;
    ITERATOR it;
    bool found = false;

    if (IS_NULLSTR(acct->default_character)) {
        write_to_buffer(d, "No default character has been set.\n\r", 0);
        display_account_menu(d);
        return;
    }

    /* Verify the default character exists on this account */
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ch_entry->name, acct->default_character)) {
            found = true;
            break;
        }
    }
    iterator_stop(&it);

    if (!found) {
        write_to_buffer(d, "Default character not found in your account.\n\r", 0);
        display_account_menu(d);
        return;
    }

    direct_login_character(d, acct->default_character);
}

/**
 * handle_account_recent_character - Quick-login with most recent character (option 'Z')
 *
 * Finds the most recently played character and calls direct_login_character.
 *
 * @param d  The descriptor at the account menu
 */
void handle_account_recent_character(DESCRIPTOR_DATA *d)
{
    ACCOUNT_CHARACTER *recent_char = find_most_recent_character(d->account);

    if (!recent_char) {
        write_to_buffer(d, "No eligible recent character found.\n\r", 0);
        display_account_menu(d);
        return;
    }

    direct_login_character(d, recent_char->name);
}

/**
 * handle_account_select_by_number - Select character by number from sorted list
 *
 * Uses the same sorted staff/regular ordering as display_account_menu.
 * Staff characters are numbered first, then regular characters.
 *
 * @param d       The descriptor at the account menu
 * @param choice  1-based character number from the displayed list
 */
void handle_account_select_by_number(DESCRIPTOR_DATA *d, int choice)
{
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *staff_chars[100];
    ACCOUNT_CHARACTER *regular_chars[100];
    int staff_count = 0;
    int regular_count = 0;
    ACCOUNT_CHARACTER *ch_entry;

    sort_account_characters(acct, staff_chars, &staff_count, regular_chars, &regular_count, 100);

    int total_count = staff_count + regular_count;

    if (choice < 1 || choice > total_count) {
        write_to_buffer(d, "Invalid character selection.\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (choice <= staff_count)
        ch_entry = staff_chars[choice - 1];
    else
        ch_entry = regular_chars[choice - staff_count - 1];

    select_character(d, ch_entry);
}

/**
 * handle_account_select_by_name - Select character by name prefix
 *
 * Searches sorted staff characters first, then regular characters,
 * using case-insensitive prefix matching.
 *
 * @param d     The descriptor at the account menu
 * @param name  The name prefix to search for
 */
void handle_account_select_by_name(DESCRIPTOR_DATA *d, const char *name)
{
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *staff_chars[100];
    ACCOUNT_CHARACTER *regular_chars[100];
    int staff_count = 0;
    int regular_count = 0;
    ACCOUNT_CHARACTER *found_entry = NULL;

    sort_account_characters(acct, staff_chars, &staff_count, regular_chars, &regular_count, 100);

    /* Search staff first */
    for (int i = 0; i < staff_count; i++) {
        if (!str_prefix(name, staff_chars[i]->name)) {
            found_entry = staff_chars[i];
            break;
        }
    }

    /* If not found, search regular */
    if (!found_entry) {
        for (int i = 0; i < regular_count; i++) {
            if (!str_prefix(name, regular_chars[i]->name)) {
                found_entry = regular_chars[i];
                break;
            }
        }
    }

    if (found_entry) {
        select_character(d, found_entry);
    } else {
        write_to_buffer(d, "No character by that name found.\n\r", 0);
        display_account_menu(d);
    }
}


/*
 * ============================================================================
 * CHARACTER MENU HANDLERS
 * ============================================================================
 */

/**
 * handle_character_login - Log in with selected character (option 'L')
 *
 * Handles the full character login flow including: deleted character check,
 * reconnection detection, staff password uniqueness enforcement, character
 * password/MFA verification, and staff MFA enforcement.
 *
 * @param d  The descriptor at the character menu (d->character must be loaded)
 */
void handle_character_login(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data;
    bool has_char_pwd;
    bool has_char_mfa;

    if (ch->deleted) {
        write_to_buffer(d, "This character is flagged for deletion. Please cancel deletion first.\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (check_playing(d, ch->name))
        return;

    bool is_reconnecting_attempt = check_reconnect(d, ch->name, true);
    if (is_reconnecting_attempt && d->connected != CON_CHARACTER_MENU)
        return;

    has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);

    if (!DEV_SKIP_PASSWORD) {
        /* Staff characters require unique password if no account password */
        if (IS_IMMORTAL(ch) && game_settings.require_uniq_pass_staff &&
            !has_char_pwd &&
            (IS_NULLSTR(acct->passwd) || acct->passwd_version == 0)) {
            write_to_buffer(d, "\n\r{RERROR: Staff characters require a unique password.{x\n\r", 0);
            write_to_buffer(d, "You must set a unique password for this character before logging in.\n\r", 0);
            display_character_menu(d);
            return;
        }

        if (has_char_pwd) {
            write_to_buffer(d, "\n\rThis character requires an additional password.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_GET_CHAR_PASSWORD;
            return;
        }
    }

    if (!DEV_SKIP_MFA) {
        if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff &&
            !has_char_mfa &&
            (IS_NULLSTR(acct->mfa_key))) {
            write_to_buffer(d, "\n\r{RERROR: Staff characters require MFA to be enabled.{x\n\r", 0);
            write_to_buffer(d, "You must enable MFA on either your account or this character before logging in.\n\r", 0);
            display_character_menu(d);
            return;
        }

        if (has_char_mfa) {
            write_to_buffer(d, "\n\rThis character has MFA enabled.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_GET_CHAR_MFA;
            return;
        }

        if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff &&
            !IS_NULLSTR(acct->mfa_key) && !has_char_mfa) {
            write_to_buffer(d, "\n\rThis is a staff character. Account MFA verification required.\n\r", 0);
            d->connected = CON_GET_ACCOUNT_MFA_FOR_CHAR;
            return;
        }
    }

    if (d->reconnecting && d->reconnect_ch) {
        complete_reconnect(d);
    } else {
        proceed_to_game(d);
    }
}

/**
 * handle_character_change_password - Set/change character password (option 'P')
 *
 * Shows context-appropriate prompt depending on whether a password is
 * already set, then transitions to the password setting flow.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_change_password(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (DEV_SKIP_PASSWORD) {
        write_to_buffer(d, "Character password setting is disabled in development mode.\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);

    if (has_char_pwd) {
        write_to_buffer(d, "This character already has a password set.\n\r", 0);
        write_to_buffer(d, "Do you want to change it? (Y/N): ", 0);
    } else {
        write_to_buffer(d, "Setting a character-specific password will require\n\r", 0);
        write_to_buffer(d, "an additional password when logging in as this character.\n\r", 0);
        write_to_buffer(d, "Do you want to set a password for this character? (Y/N): ", 0);
    }

    d->connected = CON_CHARACTER_PASSWORD;
}

/**
 * handle_character_clear_password - Clear character password (option 'X')
 *
 * Removes the character-specific password. Only available when a password
 * is currently set.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_clear_password(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (DEV_SKIP_PASSWORD) {
        write_to_buffer(d, "Character password setting is disabled in development mode.\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (IS_NULLSTR(acct_char->pwd)) {
        write_to_buffer(d, "This character does not have a password set.\n\r", 0);
        display_character_menu(d);
        return;
    }

    free_string(acct_char->pwd);
    acct_char->pwd = str_dup("");
    acct_char->pwd_vers = 0;
    save_account(acct);

    write_to_buffer(d, "Character password has been cleared.\n\r", 0);
    display_character_menu(d);
}

/**
 * handle_character_mfa_menu - Access character MFA settings (option 'M')
 *
 * Displays the character MFA menu (which also sets state).
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_mfa_menu(DESCRIPTOR_DATA *d)
{
    if (DEV_SKIP_MFA) {
        write_to_buffer(d, "MFA settings are disabled in development mode.\n\r", 0);
        display_character_menu(d);
        return;
    }

    display_character_mfa_menu(d, "");
}

/**
 * handle_character_change_email - Change character email (option 'E')
 *
 * Displays current character email and prompts for a new one.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_change_email(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\r{REmail is currently disabled on this server.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    write_to_buffer(d, "\n\rCurrent email: ", 0);
    write_to_buffer(d, IS_NULLSTR(acct_char->email) ? "Not set\n\r" : acct_char->email, 0);
    write_to_buffer(d, "\n\rEnter new email address: ", 0);
    d->connected = CON_CHANGE_CHARACTER_EMAIL;
}

/**
 * handle_character_verify_email - Verify character email (option 'V')
 *
 * Checks that email is enabled, not already verified, and has a pending
 * email before prompting for the verification code.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_verify_email(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\r{REmail is currently disabled on this server.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (acct_char->email_verified) {
        write_to_buffer(d, "\n\rYour character email is already verified.\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (IS_NULLSTR(acct_char->pending_email)) {
        write_to_buffer(d, "No pending email to verify. Change your email address first.\n\r", 0);
        display_character_menu(d);
        return;
    }

    write_to_buffer(d, "Enter the code sent to your email: ", 0);
    d->connected = CON_VERIFY_CHARACTER_EMAIL_CHANGE;
}

/**
 * handle_character_resend_verification - Resend character verification email (option 'R')
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_resend_verification(DESCRIPTOR_DATA *d)
{
    if (!game_settings.enable_email) {
        write_to_buffer(d, "\n\r{REmail is currently disabled on this server.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    resend_character_verification_code(d);
    display_character_menu(d);
}

/**
 * handle_character_unlink - Unlink character from account (option 'U')
 *
 * Handles the full unlink flow: permission check, staff rejection,
 * deleted character check, password/MFA verification gates, and
 * new standalone password prompt if no auth is set.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_unlink(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (!can_unlink_characters(acct)) {
        write_to_buffer(d, "Unlinking is not available for your account.\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (IS_IMMORTAL(ch)) {
        write_to_buffer(d, "\n\r{RStaff characters cannot be unlinked through the menu.{x\n\r", 0);
        write_to_buffer(d, "Please contact an administrator for assistance.\n\r", 0);
        display_character_menu(d);
        return;
    }

    if (ch->deleted) {
        write_to_buffer(d, "You cannot unlink a character that is flagged for deletion.\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    bool has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);

    if (has_char_pwd) {
        write_to_buffer(d, "This character requires password verification before unlinking.\n\r", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_VERIFY_UNLINK_PASSWORD;
        return;
    }

    if (has_char_mfa) {
        write_to_buffer(d, "This character has MFA enabled. ", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_VERIFY_UNLINK_MFA;
        return;
    }

    /* No password/MFA - prompt for a new standalone password */
    write_to_buffer(d, "Enter a new password for this character: ", 0);
    ProtocolNoEcho(d, true);
    d->connected = CON_SET_UNLINK_PASSWORD;
}

/**
 * handle_character_delete - Delete character (option 'D')
 *
 * Handles the full deletion flow: staff rejection, already-deleted states
 * (immediate delete if permitted, otherwise reject), password/MFA verification
 * gates, and the DELETE confirmation prompt.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_delete(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (IS_IMMORTAL(ch)) {
        write_to_buffer(d, "\n\r{RStaff characters cannot be deleted through the menu.{x\n\r", 0);
        write_to_buffer(d, "Please contact an administrator for assistance.\n\r", 0);
        display_character_menu(d);
        return;
    }

    /* Already flagged for deletion - check for immediate delete permission */
    if (ch->deleted && IS_SET(acct->acct_flags, ACCT_CAN_DELETE_IMMEDIATELY)) {
        write_to_buffer(d, "\n\r{RThis will permanently delete the character.{x\n\r", 0);
        write_to_buffer(d, "Type 'DELETE NOW' to confirm immediate deletion, or 'C' to cancel: ", 0);
        d->connected = CON_VERIFY_CHARACTER_DELETE;
        return;
    }

    if (ch->deleted) {
        write_to_buffer(d, "This character is already flagged for deletion.\n\r", 0);
        display_character_menu(d);
        return;
    }

    get_character_auth_data(ch, acct, &acct_char);
    if (!acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        return;
    }

    bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    bool has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);

    if (has_char_pwd) {
        write_to_buffer(d, "This character requires password verification before deletion.\n\r", 0);
        write_to_buffer(d, "Enter character password: ", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_VERIFY_DELETE_PASSWORD;
        return;
    }

    if (has_char_mfa) {
        write_to_buffer(d, "This character has MFA enabled. ", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_VERIFY_DELETE_MFA;
        return;
    }

    write_to_buffer(d, "\n\r{RWARNING: This will flag your character for deletion!{x\n\r", 0);
    write_to_buffer(d, "Type 'DELETE' to confirm or anything else to cancel: ", 0);
    d->connected = CON_CONFIRM_DELETE_CHARACTER;
}

/**
 * handle_character_cancel_delete - Cancel character deletion (option 'C')
 *
 * Clears deletion flags on both the character and account_character entry.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_cancel_delete(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    if (!ch->deleted) {
        write_to_buffer(d, "This character is not flagged for deletion.\n\r", 0);
        display_character_menu(d);
        return;
    }

    ch->deleted = false;
    ch->delete_time = 0;

    get_character_auth_data(ch, acct, &acct_char);
    if (acct_char) {
        acct_char->deleted = false;
        acct_char->delete_time = 0;
    }

    save_char_obj(ch);
    save_account(acct);

    write_to_buffer(d, "Character deletion canceled.\n\r", 0);
    display_character_menu(d);
}

/**
 * handle_character_set_default - Set character as default login (option 'Y')
 *
 * Uses set_default_character() to properly validate and store.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_set_default(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    if (set_default_character(acct, ch->name)) {
        write_to_buffer(d, "This character has been set as your default login character.\n\r", 0);
    } else {
        write_to_buffer(d, "Error setting default character.\n\r", 0);
    }

    display_character_menu(d);
}

/**
 * handle_character_reset_prefs - Reset settings to account defaults (option 'T')
 *
 * Shows a preview of all character-specific setting overrides and what
 * they would become after reset (account value or game default). Asks
 * for confirmation before applying.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_reset_prefs(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;

    if (!ch || ch->deleted) {
        write_to_buffer(d, "Cannot reset settings for a deleted character.\n\r", 0);
        display_character_menu(d);
        return;
    }

    int changes = pref_build_reset_preview(d);

    if (changes > 0) {
        write_to_buffer(d,
            "\n\rThis will clear all character-specific settings and apply\n\r"
            "your account preferences (or game defaults where not set).\n\r"
            "\n\r{YProceed with reset? (Y/N):{x ", 0);
        d->connected = CON_CONFIRM_RESET_PREFS;
    } else {
        display_character_menu(d);
    }
}

/**
 * handle_character_back_to_account - Return to account menu (option 'B')
 *
 * Frees the loaded character data and returns to the account menu.
 *
 * @param d  The descriptor at the character menu
 */
void handle_character_back_to_account(DESCRIPTOR_DATA *d)
{
    free_char(d->character);
    d->character = NULL;
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}
