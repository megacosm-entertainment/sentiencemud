#ifndef NANNY_AUTH_H
#define NANNY_AUTH_H

#include "../merc.h"

/*
 * Nanny Authentication Handlers
 *
 * Consolidated authentication handling for the nanny (login) system.
 * Eliminates duplication of password/MFA/email verification patterns.
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
 * Password Input Handler
 * Handles all password input scenarios across the login system
 * Returns true if password check passed, false otherwise
 */
bool handle_password_input(DESCRIPTOR_DATA *d, char *argument, password_check_type_t type);

/*
 * MFA Input Handler
 * Handles all MFA code verification scenarios
 * Returns true if MFA code/recovery code was valid
 * Sets *used_recovery to true if a recovery code was used (optional parameter)
 */
bool handle_mfa_input(DESCRIPTOR_DATA *d, char *argument, bool is_account, bool *used_recovery);

/*
 * Email Validation Handler
 * Handles email input and validation
 * Returns true if email format is valid
 */
bool handle_email_input(DESCRIPTOR_DATA *d, char *argument);

/*
 * Email Verification Code Handler
 * Handles email verification code checking
 * Returns true if verification code was valid and email was updated
 */
bool handle_email_verification(DESCRIPTOR_DATA *d, char *argument, bool is_account);

/*
 * Password Reset Code Handler
 * Returns true if reset code was valid and not expired
 */
bool handle_reset_code(DESCRIPTOR_DATA *d, char *argument, bool is_account);

/*
 * Complete Password Change
 * Applies the confirmed new password to account or character
 * Returns true if password was successfully updated
 */
bool complete_password_change(DESCRIPTOR_DATA *d, bool is_account, bool char_override);

#endif /* NANNY_AUTH_H */
