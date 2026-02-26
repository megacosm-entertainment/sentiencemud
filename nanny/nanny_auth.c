#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "../merc.h"
#include "../recycle.h"
#include "../account/auth.h"
#include "nanny_utils.h"

/*
 * Nanny Authentication Handlers
 *
 * This module consolidates all authentication handling logic for the nanny system,
 * eliminating the duplication of password/MFA/email verification patterns that
 * were scattered throughout nanny.c.
 */

/*
 * Password Check Types
 */
typedef enum {
    PW_CHECK_ACCOUNT,           /* Check account password */
    PW_CHECK_CHARACTER,         /* Check character password */
    PW_CHECK_ACCOUNT_OR_RESET,  /* Check account password or reset code */
    PW_SET_NEW,                 /* Setting new password */
    PW_CONFIRM_NEW              /* Confirming new password */
} password_check_type_t;

/*
 * Consolidated Password Handler
 * Handles all password input scenarios across the login system
 * Returns true if password check passed, false otherwise
 */
bool handle_password_input(DESCRIPTOR_DATA *d, char *argument, password_check_type_t type)
{
    if (!d || !argument)
        return false;

    ACCOUNT_DATA *acct = d->account;
    CHAR_DATA *ch = d->character;

    switch (type) {
        case PW_CHECK_ACCOUNT:
        case PW_CHECK_ACCOUNT_OR_RESET: {
            if (!acct) {
                nanny_send_error(d, "No account loaded.");
                return false;
            }

            /* Check for reset code first if reset is pending */
            if (type == PW_CHECK_ACCOUNT_OR_RESET && acct->reset_state == RESET_PENDING) {
                if (!IS_NULLSTR(acct->reset_code) && !strcmp(argument, acct->reset_code)) {
                    /* Check expiration */
                    if ((current_time - acct->reset_time) > 86400) { /* 24 hours */
                        nanny_send_error(d, "Reset code has expired.");
                        acct->reset_state = NO_RESET;
                        free_string(acct->reset_code);
                        acct->reset_code = str_dup("");
                        acct->reset_time = 0;
                        save_account(acct);
                        return false;
                    }
                    /* Reset code accepted - handled by caller */
                    return true;
                }
            }

            /* Verify account password using auth API */
            AUTH_DATA *auth = get_account_auth(acct);
            pwd_result_t pwd_result = verify_password(argument, auth);
            free_auth_data(auth);

            if (pwd_result != PWD_INVALID) {
                /* Mark for upgrade if using legacy password methods */
                if (pwd_result == PWD_VALID_PLAINTEXT) {
                    acct->passwd_version = 0; /* Force upgrade */
                }
                return true;
            }

            return false;
        }

        case PW_CHECK_CHARACTER: {
            if (!ch || !acct) {
                nanny_send_error(d, "Invalid character or account.");
                return false;
            }

            /* Verify character password using auth API */
            AUTH_DATA *auth = get_auth_data(ch, acct);
            pwd_result_t pwd_result = verify_password(argument, auth);
            free_auth_data(auth);

            return (pwd_result != PWD_INVALID);
        }

        case PW_SET_NEW: {
            /* Validate password strength */
            char error_msg[256];
            if (!validate_password_strength(argument, error_msg, sizeof(error_msg))) {
                nanny_send_error(d, error_msg);
                return false;
            }

            /* Store new password temporarily for confirmation */
            if (d->new_password_buffer) {
                free_string(d->new_password_buffer);
            }
            d->new_password_buffer = str_dup(argument);
            return true;
        }

        case PW_CONFIRM_NEW: {
            if (!d->new_password_buffer) {
                nanny_send_error(d, "No password to confirm.");
                return false;
            }

            if (strcmp(argument, d->new_password_buffer) != 0) {
                nanny_send_error(d, "Passwords don't match.");
                free_string(d->new_password_buffer);
                d->new_password_buffer = NULL;
                return false;
            }

            return true;
        }
    }

    return false;
}

/*
 * Consolidated MFA Handler
 * Handles all MFA code verification scenarios
 * Returns true if MFA code/recovery code was valid
 */
bool handle_mfa_input(DESCRIPTOR_DATA *d, char *argument, bool is_account, bool *used_recovery)
{
    if (!d || !argument)
        return false;

    if (used_recovery)
        *used_recovery = false;

    AUTH_DATA *auth = NULL;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;

    /* Get auth data based on context */
    if (is_account) {
        if (!acct) {
            nanny_send_error(d, "No account loaded.");
            return false;
        }
        auth = get_account_auth(acct);
    } else {
        /* Character-level MFA */
        CHAR_DATA *ch = d->character;
        if (!ch || !acct) {
            nanny_send_error(d, "Invalid character or account.");
            return false;
        }

        if (!get_character_auth_data(ch, acct, &acct_char) || !acct_char) {
            nanny_send_error(d, "Unable to find character authentication data.");
            return false;
        }

        auth = get_character_auth(acct_char);
    }

    bool valid = false;

    /* Try MFA code first */
    if (verify_mfa_code(argument, auth)) {
        valid = true;
    }
    /* Try recovery code if MFA failed */
    else if (verify_recovery_code(argument, auth)) {
        mark_recovery_code_used(argument, auth, acct, is_account ? NULL : acct_char);
        save_account(acct);
        valid = true;
        if (used_recovery)
            *used_recovery = true;
    }

    free_auth_data(auth);
    return valid;
}

/*
 * Email Validation Handler
 * Handles email input and validation
 */
bool handle_email_input(DESCRIPTOR_DATA *d, char *argument)
{
    if (!d || !argument)
        return false;

    /* Validate email format */
    if (!validate_email_format(argument)) {
        nanny_send_error(d, "Invalid email format. Must be name@domain.com");
        return false;
    }

    return true;
}

/*
 * Email Verification Code Handler
 * Handles email verification code checking
 */
bool handle_email_verification(DESCRIPTOR_DATA *d, char *argument, bool is_account)
{
    if (!d || !argument)
        return false;

    ACCOUNT_DATA *acct = d->account;
    if (!acct)
        return false;

    /* For now, check account-level email verification */
    /* Character-level email verification would be similar */
    if (IS_NULLSTR(acct->email_verification_code))
        return false;

    if (strcmp(argument, acct->email_verification_code) != 0)
        return false;

    /* Verification successful */
    if (!IS_NULLSTR(acct->pending_email)) {
        free_string(acct->email);
        acct->email = str_dup(acct->pending_email);
        free_string(acct->pending_email);
        acct->pending_email = str_dup("");
    }

    acct->email_verified = true;
    free_string(acct->email_verification_code);
    acct->email_verification_code = str_dup("");
    acct->email_verification_time = 0;

    save_account(acct);
    return true;
}

/*
 * Handle password reset code
 * Returns true if reset code was valid and not expired
 */
bool handle_reset_code(DESCRIPTOR_DATA *d, char *argument, bool is_account)
{
    if (!d || !argument)
        return false;

    ACCOUNT_DATA *acct = d->account;
    if (!acct)
        return false;

    /* Check reset code */
    if (IS_NULLSTR(acct->reset_code))
        return false;

    if (strcmp(argument, acct->reset_code) != 0)
        return false;

    /* Check expiration (24 hours) */
    if ((current_time - acct->reset_time) > 86400) {
        nanny_send_error(d, "Reset code has expired.");
        acct->reset_state = NO_RESET;
        free_string(acct->reset_code);
        acct->reset_code = str_dup("");
        acct->reset_time = 0;
        save_account(acct);
        return false;
    }

    return true;
}

/*
 * Complete password change
 * Applies the confirmed new password to account or character
 */
bool complete_password_change(DESCRIPTOR_DATA *d, bool is_account, bool char_override)
{
    if (!d || !d->new_password_buffer)
        return false;

    ACCOUNT_DATA *acct = d->account;
    if (!acct)
        return false;

    /* Hash the new password */
    char *hashed = hash_password(d->new_password_buffer);

    if (is_account) {
        /* Update account password */
        if (acct->passwd) {
            free_string(acct->old_passwd);
            acct->old_passwd = str_dup(acct->passwd);
        }
        free_string(acct->passwd);
        acct->passwd = str_dup(hashed);
        acct->passwd_version = 1; /* Current version */

        /* Clear any reset state */
        acct->reset_state = NO_RESET;
        if (acct->reset_code) {
            free_string(acct->reset_code);
            acct->reset_code = str_dup("");
        }
        acct->reset_time = 0;

        save_account(acct);
    } else {
        /* Update character password (override) */
        CHAR_DATA *ch = d->character;
        ACCOUNT_CHARACTER *acct_char = NULL;

        if (!ch || !get_character_auth_data(ch, acct, &acct_char) || !acct_char)
            return false;

        if (acct_char->pwd) {
            free_string(acct_char->old_pwd);
            acct_char->old_pwd = str_dup(acct_char->pwd);
        }
        free_string(acct_char->pwd);
        acct_char->pwd = str_dup(hashed);
        acct_char->pwd_vers = 1;

        save_account(acct);
    }

    /* Clean up temporary password */
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;

    return true;
}
