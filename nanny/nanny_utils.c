#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../merc.h"
#include "../recycle.h"
#include "../protocol.h"
#include "nanny.h"

/*
 * Nanny Utility Functions
 *
 * This module contains common utility functions used throughout the nanny
 * (login/character creation) system to reduce code duplication.
 */

/*
 * Echo Management
 * Consolidates the 57+ ProtocolNoEcho calls scattered throughout nanny.c
 */

void nanny_echo_off(DESCRIPTOR_DATA *d)
{
    if (!d)
        return;
    ProtocolNoEcho(d, true);
}

void nanny_echo_on(DESCRIPTOR_DATA *d)
{
    if (!d)
        return;
    ProtocolNoEcho(d, false);
}

/*
 * State Transition Helpers
 * Consolidates the 221+ state transition assignments
 */

void nanny_transition(DESCRIPTOR_DATA *d, int new_state)
{
    if (!d)
        return;
    d->connected = new_state;
}

void nanny_transition_with_prompt(DESCRIPTOR_DATA *d, int new_state, const char *prompt)
{
    if (!d)
        return;

    d->connected = new_state;

    if (prompt)
        write_to_buffer(d, prompt, 0);
}

void nanny_transition_with_echo(DESCRIPTOR_DATA *d, int new_state, bool echo_on)
{
    if (!d)
        return;

    d->connected = new_state;
    ProtocolNoEcho(d, !echo_on);
}

/*
 * Common transition patterns
 */

void nanny_return_to_account_menu(DESCRIPTOR_DATA *d)
{
    if (!d)
        return;

    nanny_echo_on(d);
    d->connected = CON_ACCOUNT_MENU;
    display_account_menu(d);
}

void nanny_return_to_character_menu(DESCRIPTOR_DATA *d)
{
    if (!d)
        return;

    nanny_echo_on(d);
    d->connected = CON_CHARACTER_MENU;
    display_character_menu(d);
}

/*
 * Input Validation Helpers
 */

bool validate_email_format(const char *email)
{
    if (!email || !*email)
        return false;

    /* Basic email validation - must have @ and . */
    const char *at = strchr(email, '@');
    if (!at)
        return false;

    const char *dot = strchr(at, '.');
    if (!dot)
        return false;

    /* @ must come before . */
    if (at >= dot)
        return false;

    /* Must have at least one char before @, between @ and ., and after . */
    if (at == email || at + 1 == dot || *(dot + 1) == '\0')
        return false;

    /* Length check */
    if (strlen(email) > 100)
        return false;

    return true;
}


bool validate_account_name(const char *name)
{
    if (!name || !*name)
        return false;

    /* Use existing check_parse_name if available */
    return check_parse_name((char *)name);
}

/*
 * Login Attempt Tracking
 */

bool check_login_attempts(DESCRIPTOR_DATA *d, int max_attempts)
{
    if (!d)
        return false;

    if (d->login_attempts >= max_attempts) {
        write_to_buffer(d, "Too many login attempts. Goodbye.\n\r", 0);
        close_socket(d);
        return false;
    }

    return true;
}

void increment_login_attempts(DESCRIPTOR_DATA *d)
{
    if (d)
        d->login_attempts++;
}

void reset_login_attempts(DESCRIPTOR_DATA *d)
{
    if (d)
        d->login_attempts = 0;
}

/*
 * Common Error Handlers
 */

void nanny_error_invalid_input(DESCRIPTOR_DATA *d, const char *prompt)
{
    if (!d)
        return;

    write_to_buffer(d, "Invalid input.\n\r", 0);
    if (prompt)
        write_to_buffer(d, prompt, 0);
}

void nanny_error_return_to_menu(DESCRIPTOR_DATA *d, const char *error, bool account_menu)
{
    if (!d)
        return;

    if (error)
        write_to_buffer(d, error, 0);

    nanny_echo_on(d);

    if (account_menu) {
        d->connected = CON_ACCOUNT_MENU;
        display_account_menu(d);
    } else {
        d->connected = CON_CHARACTER_MENU;
        display_character_menu(d);
    }
}

/*
 * Character Cleanup Helpers
 */

void nanny_cleanup_character(DESCRIPTOR_DATA *d)
{
    if (!d)
        return;

    if (d->character) {
        free_char(d->character);
        d->character = NULL;
    }
}

/*
 * Rate Limiting / Timing Helpers
 */

bool check_rate_limit(DESCRIPTOR_DATA *d, const char *operation, time_t min_interval)
{
    /* This is a placeholder - actual implementation would track
     * per-descriptor operation timestamps */
    return true;
}

/*
 * Utility for checking dev mode bypasses
 */

bool should_skip_password(void)
{
    return (game_settings.dev_server && !game_settings.enable_passwd);
}

bool should_skip_mfa(void)
{
    return (game_settings.dev_server && !game_settings.enable_mfa);
}

/*
 * String Utilities for Nanny
 */

void nanny_send_prompt(DESCRIPTOR_DATA *d, const char *prompt)
{
    if (d && prompt)
        write_to_buffer(d, prompt, 0);
}

void nanny_send_message(DESCRIPTOR_DATA *d, const char *message)
{
    if (d && message)
        write_to_buffer(d, message, 0);
}

void nanny_send_error(DESCRIPTOR_DATA *d, const char *error)
{
    if (d && error) {
        write_to_buffer(d, "{R", 0);
        write_to_buffer(d, error, 0);
        write_to_buffer(d, "{x\n\r", 0);
    }
}

void nanny_send_success(DESCRIPTOR_DATA *d, const char *message)
{
    if (d && message) {
        write_to_buffer(d, "{G", 0);
        write_to_buffer(d, message, 0);
        write_to_buffer(d, "{x\n\r", 0);
    }
}

/**
 * direct_login_character - Load and enter game with a character, bypassing character menu
 *
 * Handles the full flow of loading a character by name from the account
 * menu, checking authentication (password/MFA), handling reconnection,
 * and proceeding to the game. Used by both the 'Y' (default character)
 * and 'Z' (most recent character) quick-login options.
 *
 * @param d             The descriptor attempting login
 * @param char_name     The name of the character to load
 */
void direct_login_character(DESCRIPTOR_DATA *d, const char *char_name)
{
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data;
    bool has_char_pwd;
    bool has_char_mfa;

    if (!acct || IS_NULLSTR(char_name)) {
        write_to_buffer(d, "Error: invalid character.\n\r", 0);
        display_account_menu(d);
        return;
    }

    // Check if already online
    if (is_character_online(char_name)) {
        write_to_buffer(d, "That character is already logged in.\n\r", 0);
        display_account_menu(d);
        return;
    }

    // Clear any existing character data
    if (d->character) {
        free_char(d->character);
        d->character = NULL;
    }

    // Reset reconnection flags
    d->reconnect_ch = NULL;
    d->reconnecting = false;

    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "Direct login: Loading character");

    // Load the character
    if (!load_char_obj(d, char_name)) {
        write_to_buffer(d, "Error loading character.\n\r", 0);
        display_account_menu(d);
        return;
    }

    if (!d->character) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "Direct login: Character loaded but d->character is NULL");
        write_to_buffer(d, "Error loading character data.\n\r", 0);
        display_account_menu(d);
        return;
    }

    // Get account character data
    has_auth_data = get_character_auth_data(d->character, acct, &acct_char);

    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "Error with character authentication data.\n\r", 0);
        free_char(d->character);
        d->character = NULL;
        display_account_menu(d);
        return;
    }

    has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);

    // Check for reconnection
    if (check_reconnect(d, d->character->name, false)) {
        if (d->connected != CON_ACCOUNT_MENU) {
            return;
        }
    }

    // Check password if needed
    if (!DEV_SKIP_PASSWORD && has_char_pwd) {
        write_to_buffer(d, "\n\rThis character requires an additional password.\n\r", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_GET_CHAR_PASSWORD;
        return;
    }

    // Check MFA if needed
    if (!DEV_SKIP_MFA && has_char_mfa) {
        write_to_buffer(d, "\n\rThis character has MFA enabled.\n\r", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_GET_CHAR_MFA;
        return;
    }

    // If no auth needed, proceed to game
    if (d->reconnecting && d->reconnect_ch) {
        complete_reconnect(d);
    } else {
        proceed_to_game(d);
    }
}

/**
 * sort_account_characters - Sort account characters into staff and regular buckets
 *
 * Separates an account's characters into staff (immortal+) and regular
 * arrays, sorted alphabetically by name. Used by display_account_menu
 * and login_account_menu to generate consistent numbered/lettered lists.
 *
 * @param acct           The account whose characters to sort
 * @param staff_chars    Output array for staff characters (caller-provided, must hold >=max elements)
 * @param staff_count    Output: number of staff characters found
 * @param regular_chars  Output array for regular characters (caller-provided, must hold >=max elements)
 * @param regular_count  Output: number of regular characters found
 * @param max            Maximum number of characters per array
 */
void sort_account_characters(ACCOUNT_DATA *acct,
    ACCOUNT_CHARACTER **staff_chars, int *staff_count,
    ACCOUNT_CHARACTER **regular_chars, int *regular_count,
    int max)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    *staff_count = 0;
    *regular_count = 0;

    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            if (*staff_count < max)
                staff_chars[(*staff_count)++] = ch_entry;
        } else {
            if (*regular_count < max)
                regular_chars[(*regular_count)++] = ch_entry;
        }
    }
    iterator_stop(&it);

    // Sort staff alphabetically
    for (int i = 0; i < *staff_count - 1; i++) {
        for (int j = 0; j < *staff_count - i - 1; j++) {
            if (strcasecmp(staff_chars[j]->name, staff_chars[j+1]->name) > 0) {
                ACCOUNT_CHARACTER *temp = staff_chars[j];
                staff_chars[j] = staff_chars[j+1];
                staff_chars[j+1] = temp;
            }
        }
    }

    // Sort regular alphabetically
    for (int i = 0; i < *regular_count - 1; i++) {
        for (int j = 0; j < *regular_count - i - 1; j++) {
            if (strcasecmp(regular_chars[j]->name, regular_chars[j+1]->name) > 0) {
                ACCOUNT_CHARACTER *temp = regular_chars[j];
                regular_chars[j] = regular_chars[j+1];
                regular_chars[j+1] = temp;
            }
        }
    }
}
