#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../merc.h"
#include "../recycle.h"
#include "../protocol.h"

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

bool validate_password_strength_basic(const char *password, char *error_msg, int error_len)
{
    if (!password) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password cannot be empty");
        return false;
    }

    /* Minimum length */
    if (strlen(password) < 5) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password must be at least 5 characters");
        return false;
    }

    /* Maximum length */
    if (strlen(password) > 50) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password must be 50 characters or less");
        return false;
    }

    return true;
}

bool validate_account_name(const char *name)
{
    if (!name || !*name)
        return false;

    /* Use existing check_parse_name if available */
    return check_parse_name(name);
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
