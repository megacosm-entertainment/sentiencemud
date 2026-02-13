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

#include "strings.h"
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "scripts.h"
#include "tables.h"
#include "wilds.h"
#include "protocol.h"
#include "account/auth.h"
#include "account/penalty.h"
#include "account/preferences.h"
#include "account/unlock.h"
#include "io/json/json_char.h"
#include "nanny/nanny_utils.h"
#include "nanny/nanny_menus.h"
#include "class_data.h"
#include "skill_group.h"
#include "traits.h"

/*
 * NANNY SYSTEM - LOGIN AND CHARACTER CREATION
 *
 * This file handles all connection states for login and character creation.
 * Current size: 6,447 lines, 92 functions
 *
 * REFACTORING STATUS:
 * ✅ Auth API consolidated (account/auth.c) - 16+ password checks → 1 function
 * ✅ Utilities extracted (nanny/nanny_utils.c) - Common helpers
 * ✅ Menu handlers created (nanny/nanny_menus.c) - Account/character menus
 * ✅ Auth handlers created (nanny/nanny_auth.c) - Password/MFA/email handling
 * ⏸️ File split deferred - This file should eventually be split into:
 *     - nanny/nanny_account.c (~600 lines) - Account operations
 *     - nanny/nanny_character.c (~700 lines) - Character operations
 *     - nanny/nanny_creation.c (~800 lines) - Character creation flow
 *     - nanny.c (~800 lines) - Main dispatcher
 *
 * For now, keeping this monolithic to avoid breaking working code.
 * Split incrementally as functions are touched.
 */

/* Account related functions */
// This leads to CON_GET_ACOCOUNT_PASSWORD (existing)
// or CON_CONFIRM_ACCOUNT_NAME (new).
void login_get_account(DESCRIPTOR_DATA *d, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    bool found;

    if (!argument[0]) {
        close_socket(d);
        return;
    }

    // Remove illegal characters
    argument[0] = UPPER(argument[0]);
    if (!check_parse_name(argument)) {
        write_to_buffer(d, "Illegal account name, try another.\n\r ", 0);
        return;
    }

    // Create account structure for descriptor
    if (d->account != NULL) {
        free_account(d->account);
        d->account = NULL;
    }
    
    // Try to load the account
    found = load_account(d, argument);
    
    // Check if they're banned
    if (check_ban(d->host, BAN_PERMIT)) {
        write_to_buffer(d, "Your site has been banned from Sentience.\n\r", 0);
        close_socket(d);
        return;
    }
    
    // Check wizlock if they don't have any immortal characters
    if (game_settings.wizlock && !account_has_immortal(d->account)) {
        if (!IS_NULLSTR(game_settings.wizlock_msg))
            write_to_buffer(d, game_settings.wizlock_msg, 0);
        else
            write_to_buffer(d, "The game is wizlocked.\n\r", 0);
            
        log_message_f(LOG_LEVEL_INFO, LOG_SECURITY, "Wizlocked: %s tried to connect from %s.", argument, d->host);
        sprintf(buf, "Wizlocked: %s tried to connect from %s.", argument, d->host);
        wiznet(buf, NULL, NULL, WIZ_LOGINS, 0, 0);
        close_socket(d);
        return;
    }
    
    // Existing account
    if (found) {
        if (DEV_SKIP_PASSWORD) {
            // Log and proceed as if password was accepted
            ProtocolNoEcho(d, false);
            log_message_f(LOG_LEVEL_INFO, LOG_SECURITY, "Account %s@%s has connected (dev server, password skipped).", d->account->username, d->host);

            if (DEV_SKIP_MFA) {
                d->connected = CON_ACCOUNT_MENU;
                display_account_menu(d);
                return;
            }

            // Check for MFA
            if (!IS_NULLSTR(d->account->mfa_key) && d->account->mfa_enabled && !DEV_SKIP_MFA) {
                ProtocolNoEcho(d, true);
                d->connected = CON_GET_ACCOUNT_MFA;
                return;
            }

            // Proceed to account menu
            ProtocolNoEcho(d, false);
            display_account_menu(d);
            d->connected = CON_ACCOUNT_MENU;
            return;
        }

        ProtocolNoEcho(d, true);
        d->connected = CON_GET_ACCOUNT_PASSWORD;
        return;
    }
    // New account
    else {
        // Check newlock
        if (game_settings.new_acct_lock) {
            if (game_settings.new_acct_lock_msg && game_settings.new_acct_lock_msg[0])
                write_to_buffer(d, game_settings.new_acct_lock_msg, 0);
            else
                write_to_buffer(d, "New accounts are not being accepted at this time.\n\r", 0);
                
            write_to_buffer(d, "The game is newlocked.\n\r", 0);
            close_socket(d);
            return;
        }

        if (check_ban(d->host, BAN_NEWBIES)) {
            write_to_buffer(d, "New accounts are not allowed from your site.\n\r", 0);
            close_socket(d);
            return;
        }

        sprintf(buf, "\n\rCreate a new account named %s (Y/N)? ", argument);
        write_to_buffer(d, buf, 0);
        d->connected = CON_CONFIRM_ACCOUNT_NAME;
        return;
    }
}

// We have an existing account. Go ahead and get their password.
// If they have a reset code, we need to handle that first.
// This can lead into a number of states:
// CON_GET_ACCOUNT_MFA (if they have MFA enabled)
// CON_ACCOUNT_MENU (if everything is good and they can access the account menu)
// CON_GET_ACCOUNT_PASSWORD (if they need to enter their password again)
// CON_GET_ACCOUNT_EMAIL (if they need to set their email address)
// CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET (if they need to confirm their email address for reset)
// CON_CHANGE_ACCOUNT_PASSWORD (if they need to change their password after reset)
void login_get_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    bool password_ok = false;
    
    write_to_buffer(d, "\n\r", 2);

    /*
    if (d->login_attempts == 0 && acct->reset_state == NO_RESET)
    {
        write_to_buffer(d, "Password: ", 0);
    }
*/
    if (d->login_attempts >= game_settings.max_login_attempts) {
        write_to_buffer(d, "Too many login attempts. Goodbye.\n\r", 0);
        close_socket(d);
        return;
    }

    // Handle password reset request
    if (game_settings.enable_email && !strcmp(argument, "resetpassword")) {
        if (acct->reset_state == RESET_PENDING) {
            write_to_buffer(d, "Reset is already pending. Check your email for the code.\n\r", 0);
            write_to_buffer(d, "Password or Reset Code: ", 0);
            return;
        } else {
            if (IS_NULLSTR(acct->email)) {
                write_to_buffer(d, "You must have an email address set to reset your password.\n\r", 0);
                write_to_buffer(d, "Please reach out to staff for assistance.\n\r", 0);
                // d->connected = CON_GET_ACCOUNT_PASSWORD; // Already in this state
                return;
            } else {
                d->connected = CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET;
                return;
            }
        }
    }

    // Check for reset code or password
    if (acct->reset_state == RESET_PENDING) {
        // Try reset code first
        if (!IS_NULLSTR(acct->reset_code) && !strcmp(argument, acct->reset_code)) {
            if ((current_time - acct->reset_time) > 86400) { // 24 hours
                if (game_settings.enable_email)
                    write_to_buffer(d, "Reset code has expired. Please try resetting again.\n\r", 0);
                else
                    write_to_buffer(d, "Reset code has expired. Please contact staff for assistance.\n\r", 0);
                
                acct->reset_state = NO_RESET;
                free_string(acct->reset_code);
                acct->reset_code = str_dup(""); // Ensure it's not NULL
                acct->reset_time = 0;
                save_account(acct);
                close_socket(d); // Or return to account name prompt
                return;
            }
            // Reset code accepted
            acct->reset_state = NO_RESET;
            free_string(acct->reset_code);
            acct->reset_code = str_dup("");
            acct->reset_time = 0;
            if (acct->old_passwd) free_string(acct->old_passwd);
            acct->old_passwd = str_dup(acct->passwd);
            
            write_to_buffer(d, "Reset code accepted. You are required to set a new password.\n\r", 0);
            d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
            return;
        }
        // If not reset code, try password using auth API
        AUTH_DATA *auth = get_account_auth(acct);
        pwd_result_t pwd_result = verify_password(argument, auth);
        free_auth_data(auth);

        if (pwd_result != PWD_INVALID) {
            password_ok = true;
            // Automatically migrate legacy passwords to Argon2id
            if (pwd_result != PWD_VALID_ARGON2ID) {
                migrate_password_on_login(NULL, acct, argument, pwd_result);
            }
        }

        if (!password_ok) { // If neither reset code nor password matched
            write_to_buffer(d, "Wrong password or reset code.\n\r", 0);
            d->login_attempts++;
            write_to_buffer(d, "Password or Reset Code: ", 0);
            return;
        }
        // If password_ok is true here, it means they entered their current password
        // while a reset was pending. We can treat this as a successful login.
        // The reset state will be cleared upon successful login further down.

    } else { // Normal password check (no reset pending)
        // Use auth API for password verification
        AUTH_DATA *auth = get_account_auth(acct);
        pwd_result_t pwd_result = verify_password(argument, auth);
        free_auth_data(auth);

        if (pwd_result != PWD_INVALID) {
            password_ok = true;
            // Automatically migrate legacy passwords to Argon2id
            if (pwd_result != PWD_VALID_ARGON2ID) {
                migrate_password_on_login(NULL, acct, argument, pwd_result);
            }
        }
    }

    if (!password_ok) {
        // Log bad password attempts
        log_message_f(LOG_LEVEL_WARN, LOG_SECURITY, "Denying access to account %s@%s (bad password).",
            acct->username, d->host);
        
        if (game_settings.enable_email)
            write_to_buffer(d, "Wrong password. Please try again, or use 'resetpassword' to reset.\n\r", 0);
        else
            write_to_buffer(d, "Wrong password. Please try again or reach out to staff for assistance.\n\r", 0);
        
        d->login_attempts++;
        return;
    }

    // Password is OK, proceed with login
    d->login_attempts = 0; // Reset attempts on success

    // Clear any pending password reset state as they've successfully logged in
    if (acct->reset_state != NO_RESET) {
        acct->reset_state = NO_RESET;
        if (acct->reset_code) free_string(acct->reset_code);
        acct->reset_code = str_dup("");
        acct->reset_time = 0;
        // old_passwd might have been set if they used a reset code, clear it if not needed
        // Or keep it if a forced password change is next
    }


    // Handle MFA
    if (!IS_NULLSTR(acct->mfa_key) && !DEV_SKIP_MFA) {
        ProtocolNoEcho(d,true);
        d->connected = CON_GET_ACCOUNT_MFA;
        return;
    }

    ProtocolNoEcho(d, false);

    // Log the successful connection
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Account %s@%s has connected.", acct->username, d->host);

    // Check if they need to provide email
    if (IS_NULLSTR(acct->email) && game_settings.enable_email) { // Added game_settings.enable_email check
        write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you.\n\r"
            "It will not be distributed to any third parties or abused in any way.\n\r", 0);
        d->connected = CON_GET_ACCOUNT_EMAIL;
        return;
    }

    // Password update needed?
    if (acct->passwd_version < 1 && !DEV_SKIP_PASSWORD) {
        write_to_buffer(d, "\n\rYou are required to set a new password. Please do so now.\n\r", 0);
        if (acct->old_passwd) free_string(acct->old_passwd);
        acct->old_passwd = str_dup(acct->passwd);
        ProtocolNoEcho(d, true);
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    // Add to loaded_accounts if not already present
    acct->refcount++;
    if (!list_haslink(loaded_accounts, acct))
        list_addlink(loaded_accounts, acct);  

    if (acct->last_login_host) free_string(acct->last_login_host);
    acct->last_login_host = str_dup(d->host);
    acct->last_login = current_time;
    save_account(acct); // Save account on successful login

    // Check for account-level deny penalty
    if (has_penalty(acct, PENALTY_DENY, NULL)) {
        log_message_f(LOG_LEVEL_WARN, LOG_SECURITY, "Denying account %s@%s (account penalty).", acct->username, d->host);
        write_to_buffer(d, "This account has been denied access.\n\r", 0);
        close_socket(d);
        return;
    }

    // Show the account menu
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

// User is confirming account name for a new account.
// This leads to CON_NEW_ACCOUNT_PASSWORD.
// If they say no, we go back to CON_GET_ACCOUNT_NAME.
void login_confirm_account_name(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    switch(toupper(argument[0])) {
    case 'Y':
        ProtocolNoEcho(d, true);
        d->connected = CON_NEW_ACCOUNT_PASSWORD;
        break;
        
    case 'N':
        free_account(acct);
        d->account = NULL;
        d->connected = CON_GET_ACCOUNT_NAME;
    if (!IS_NULLSTR(game_settings.login_string))
    {
        write_to_buffer(d, game_settings.login_string, 0);
        write_to_buffer(d, "\n\r", 0);
    }
    else
    {
        write_to_buffer(d, "\n\rBy what name do you wish to be known? ", 0);
    }
        break;
        
    default:
        write_to_buffer(d, "Please answer (Y/N): ", 0);
        break;
    }
}

// We've prompted for a new password. If it is acceptable, prompt for confirmation.
// If not, we prompt again.
// Leads to CON_CONFIRM_ACCOUNT_PASSWORD.
void login_new_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    write_to_buffer(d, "\n\r", 2);
    
    // Check if password meets minimum requirements
    if (!acceptablePassword(d, argument)) {
        // acceptablePassword should send its own error and re-prompt or set state.
        return;
    }
    
    // NEW CHECK: Validate uniqueness across staff characters
    if (game_settings.require_uniq_pass_staff && 
        !validate_password_uniqueness(acct, argument, false, NULL, false)) {
        write_to_buffer(d, "This password matches one of your staff character passwords.\n\r", 0);
        write_to_buffer(d, "Account passwords must be unique from staff character passwords. Please try again.\n\r", 0);
        d->connected = CON_NEW_ACCOUNT_PASSWORD;
        return;
    }
        
    // Store password for confirmation
    if (d->new_password_buffer) {
        free_string(d->new_password_buffer);
    }
    d->new_password_buffer = str_dup(argument);
    
    d->connected = CON_CONFIRM_ACCOUNT_PASSWORD;
}

// User is confirming their password for a new account.
// If it matches, we proceed to get their email address.
// If not, we prompt again.
// This leads to CON_NEW_ACCOUNT_EMAIL.
void login_confirm_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    write_to_buffer(d, "\n\r", 2);
    
    if (!d->new_password_buffer) {
        write_to_buffer(d, "An error occurred. Please try setting your password again.\n\r", 0);
        // Go back to password entry
        if (acct) { // If account context exists
             d->connected = CON_NEW_ACCOUNT_PASSWORD;
        } else { // Should not happen if acct is always present here
            close_socket(d); // Or back to account name
        }
        return;
    }
    
    if (strcmp(argument, d->new_password_buffer) != 0) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        d->connected = CON_NEW_ACCOUNT_PASSWORD;
        return;
    }
    
    // Passwords match, now set it encrypted
    if (!set_encrypted_password(&acct->passwd, &acct->passwd_version, d->new_password_buffer)) {
        write_to_buffer(d, "Error setting password. Please try again or contact staff.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        d->connected = CON_NEW_ACCOUNT_PASSWORD; // Go back to re-enter
        return;
    }
    
    // Password successfully set and hashed
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;
    ProtocolNoEcho(d, false);
    
    write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you.\n\r"
                "It will not be distributed to any third parties or abused in any way.\n\r", 0);
    d->connected = CON_GET_ACCOUNT_EMAIL;
}

// User is setting their email address for a new account.
// If the email is valid, we save the account and show the account menu.
// If not, we prompt again.
// This leads to CON_ACCOUNT_MENU.
void login_get_account_email(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (argument[0] == '\0' || !strstr(argument, "@") || !strstr(argument, ".")) {
        write_to_buffer(d, "That's not a valid email address.\n\r", 0);
        return;
    }
    
    free_string(acct->email);
    acct->email = str_dup(argument);

    free_string(acct->creation_host);
    acct->creation_host = str_dup(d->host);
    
    // Save the account
    save_account(acct);

    // Add to loaded_accounts if not already present
    acct->refcount++;

    if (!list_haslink(loaded_accounts, acct))
        list_addlink(loaded_accounts, acct);

    // Always show the account menu after email is provided, whether this is a new account or not
    write_to_buffer(d, "\n\rAccount successfully created! You can now create characters and manage your account.\n\r", 0);
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

// Handle changing account email
// This leads to CON_VERIFY_ACCOUNT_EMAIL_CHANGE if verification is needed.
// Otherwise, it goes to CON_ACCOUNT_MENU.
// If the email is invalid, we prompt again.
// If the user cancels, we go back to the account menu.
// If the email is already pending verification, we notify the user and go back to the account menu.
// If the email is valid and verification is not required, we update the email immediately.
// If the email is valid and verification is required, we send a verification code to the new email.
// The user must then enter the code to confirm the change.
void login_change_account_email(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;

    if (argument[0] == '\0' || !strstr(argument, "@") || !strstr(argument, ".")) {
        write_to_buffer(d, "That's not a valid email address.\n\r", 0);
        write_to_buffer(d, "Enter your e-mail address (or 'cancel' to cancel): ", 0);
        return;
    }
    if (!str_cmp(argument, "cancel")) {
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    if (!game_settings.require_email_verif) {
        // Immediate update
        free_string(acct->email);
        acct->email = str_dup(argument);
        acct->email_verified = true;
        write_to_buffer(d, "\n\rEmail address updated.\n\r", 0);
        save_account(acct);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    // Verification required
    if (!IS_NULLSTR(acct->pending_email) && !str_cmp(argument, acct->pending_email)) {
        write_to_buffer(d, "That email is already pending verification.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    free_string(acct->pending_email);
    acct->pending_email = str_dup(argument);
    acct->email_verified = false;

if (!str_cmp(argument, acct->email)) {
    // Email is unchanged, just mark as verified and skip verification
    acct->email_verified = true;
    free_string(acct->pending_email);
    acct->pending_email = str_dup("");
    free_string(acct->email_verification_code);
    acct->email_verification_code = str_dup("");
    acct->email_verification_time = 0;
    acct->email_verification_last_sent = 0;
    write_to_buffer(d, "\n\rEmail address is unchanged and now verified.\n\r", 0);
    save_account(acct);
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
    return;
}
    // Generate code and set time
    char code[16];
    generate_reset_code(code, 15);
    free_string(acct->email_verification_code);
    acct->email_verification_code = str_dup(code);
    acct->email_verification_time = current_time;
    acct->email_verification_last_sent = current_time;
    // Notify old email
    if (!IS_NULLSTR(acct->email)) {
        send_email_async(NULL, acct->email, "Sentience: Email Change Requested",
            "A request was made to change your account email. If this was not you, contact support.", NULL, NULL);
    }
    // Send code to new email
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", code);
    send_email_async(NULL, acct->pending_email, "Sentience: Verify Your New Email Address", body, NULL, NULL);
    save_account(acct);
    write_to_buffer(d, "\n\rA verification code has been sent to your new email address.\n\r", 0);
    d->connected = CON_VERIFY_ACCOUNT_EMAIL_CHANGE;
}

// User is verifying their email address change.
// If the code is correct, we update the email and show the account menu.
// If the code is incorrect, we prompt again.
// If the code is expired, we notify the user and go back to the account menu.
void login_verify_account_email_change(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    if (IS_NULLSTR(acct->pending_email) || IS_NULLSTR(acct->email_verification_code)) {
        write_to_buffer(d, "No email change is pending.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    // Expiry check (72 hours)
    if ((current_time - acct->email_verification_time) > (72 * 3600)) {
        write_to_buffer(d, "Verification code expired. Please start the process again.\n\r", 0);
        free_string(acct->pending_email);
        acct->pending_email = str_dup("");
        free_string(acct->email_verification_code);
        acct->email_verification_code = str_dup("");
        acct->email_verification_time = 0;
        save_account(acct);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rEmail verification cancelled.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    if (!str_cmp(argument, acct->email_verification_code)) {
        free_string(acct->email);
        acct->email = str_dup(acct->pending_email);
        acct->email_verified = true;
        free_string(acct->pending_email);
        acct->pending_email = str_dup("");
        free_string(acct->email_verification_code);
        acct->email_verification_code = str_dup("");
        acct->email_verification_time = 0;
        acct->email_verification_last_sent = 0;
        save_account(acct);
        write_to_buffer(d, "\n\rYour email address has been updated and verified.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
    } else {
        write_to_buffer(d, "Invalid code. Try again (or 'cancel' to cancel): ", 0);
    }
}

// Verify current password before allowing a password change
// This leads to CON_CHANGE_ACCOUNT_PASSWORD.
void login_verify_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;

    write_to_buffer(d, "\n\r", 2);

    // Use auth API for password verification
    AUTH_DATA *auth = get_account_auth(acct);
    pwd_result_t pwd_result = verify_password(argument, auth);
    free_auth_data(auth);

    if (pwd_result == PWD_INVALID) {
        write_to_buffer(d, "Incorrect password.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    // Migrate legacy passwords to Argon2id before changing password
    if (pwd_result != PWD_VALID_ARGON2ID) {
        migrate_password_on_login(NULL, acct, argument, pwd_result);
    }

    d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
}

// User is verifying their MFA code for account settings.
// If the code is correct, we proceed to the MFA settings menu.
void login_account_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    AUTH_DATA *auth = get_account_auth(acct);
    bool valid = false;

    // Try MFA code first
    if (verify_mfa_code(argument, auth)) {
        valid = true;
    }
    // Try recovery code if MFA failed
    else if (verify_recovery_code(argument, auth)) {
        mark_recovery_code_used(argument, auth, acct, NULL);
        save_account(acct);
        valid = true;
        write_to_buffer(d, "\n\r{YRecovery code accepted. This code cannot be used again.{x\n\r", 0);
    }

    free_auth_data(auth);

    if (!str_cmp(argument, "cancel")) {
        write_to_buffer(d, "\n\rReturning to account menu.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    if (valid) {
        d->mfa_verified = true;
        display_account_mfa_menu(d, "");
        d->connected = CON_ACCOUNT_MFA_MENU;
    } else {
        write_to_buffer(d, "Invalid MFA or recovery code. Try again (or 'cancel'): ", 0);
    }
}

// User is confirming MFA disable.
// If they confirm, we disable MFA and show the account menu.
// If they cancel, we go back to the account menu.
// If they enter an invalid response, we prompt again.
void login_account_mfa_disable_confirm(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    switch (toupper(argument[0])) {
        case 'Y':
            // Clear MFA key
            if (acct->mfa_key != NULL)
                free_string(acct->mfa_key);
            acct->mfa_key = str_dup("");
            
            // Clear any pending MFA key
            if (acct->mfa_pending_key != NULL)
                free_string(acct->mfa_pending_key);
            acct->mfa_pending_key = str_dup("");

            acct->mfa_enabled = false;
            acct->mfa_pending = false;

            // Clear recovery codes
            for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                if (acct->recovery_codes[i] != NULL)
                    free_string(acct->recovery_codes[i]);
                acct->recovery_codes[i] = str_dup("");
                acct->recovery_used[i] = false;
            }
            
            save_account(acct);
            write_to_buffer(d, "MFA disabled and all MFA data cleared for this account.\n\r", 0);
            display_account_mfa_menu(d, "");
            break;
        case 'N':
            display_account_mfa_menu(d, "");
            break;
        default:
            write_to_buffer(d, "Please answer Y or N: ", 0);
            break;
    }
}

// Resending the account verification code.
// This is used when the user has requested a new email address and needs to verify it.
// If the email is already pending verification, we notify the user and go back to the account menu.
// If the email is valid and verification is not required, we update the email immediately.
void resend_account_verification_code(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    if (IS_NULLSTR(acct->pending_email)) {
        write_to_buffer(d, "No pending email to verify.\n\r", 0);
        return;
    }
    if ((current_time - acct->email_verification_last_sent) < (15 * 60)) {
        write_to_buffer(d, "You must wait before resending the verification email.\n\r", 0);
        return;
    }
    char code[16];
    if (IS_NULLSTR(acct->email_verification_code)) {
        generate_reset_code(code, 15);
        free_string(acct->email_verification_code);
        acct->email_verification_code = str_dup(code);
    }
    acct->email_verification_time = current_time;
    acct->email_verification_last_sent = current_time;
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", acct->email_verification_code);
    send_email_async_ex(NULL, acct, acct->pending_email, "Sentience: Verify Your New Email Address", body, NULL, NULL);
    save_account(acct);
    write_to_buffer(d, "Verification email resent.\n\r", 0);
}

void display_shared_storage_info(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];

    write_to_buffer(d, "\n\r{B=={W[ {YSHARED STORAGE INFO {W]{B=={x\n\r\n\r", 0);

    // Vault info
    if (game_settings.vault_enabled) {
        sprintf(buf, "Vault is {G%s{x\n\r", game_settings.vault_enabled ? "ENABLED" : "DISABLED");
        write_to_buffer(d, buf, 0);

        if (game_settings.vault_rent) {
            time_t now = current_time;
            int days_left = 0;
            if (acct->vault_rent > now)
                days_left = (acct->vault_rent - now) / 86400;
            sprintf(buf, "Vault rented until: {Y%s{x (%d days left)\n\r",
                acct->vault_rent > 0 ? ctime(&acct->vault_rent) : "Not rented",
                days_left);
            write_to_buffer(d, buf, 0);
            sprintf(buf, "Vault rent cost: {G%d{x per %d days\n\r",
                game_settings.vault_rent_cost, game_settings.vault_rent_time);
            write_to_buffer(d, buf, 0);
        }

        sprintf(buf, "Vault item limit: {G%d{x\n\r", game_settings.max_vault_items);
        write_to_buffer(d, buf, 0);
        sprintf(buf, "Vault weight limit: {G%d{x\n\r", game_settings.max_vault_weight);
        write_to_buffer(d, buf, 0);

        // List items
        write_to_buffer(d, "\n\r{CVault Items:{x\n\r", 0);
        OBJ_DATA *obj;
        int count = 0;
        for (obj = acct->vault_items; obj; obj = obj->next_content) {
            sprintf(buf, "  {G%-30s{x (weight: %d)\n\r", obj->short_descr, obj->weight);
            write_to_buffer(d, buf, 0);
            count++;
        }
        if (count == 0)
            write_to_buffer(d, "  {RNo items in vault.{x\n\r", 0);
    } else {
        write_to_buffer(d, "Vault storage is currently disabled.\n\r", 0);
    }

    write_to_buffer(d, "\n\r{GB{x) Back to account menu\n\r", 0);
    d->connected = CON_ACCOUNT_MENU;
}



// We're displaying the account menu here. 
// This is paired with login_account_menu to handle the actual selections.

void display_account_menu(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];
    char name_buf[50];
    char rank_buf[50];
    char level_buf[50];
    char race_buf[50];
    char class_buf[50];
    ACCOUNT_CHARACTER *ch_entry;
    char *default_char = IS_NULLSTR(acct->default_character) ? NULL : acct->default_character;
    ACCOUNT_CHARACTER *recent_char = find_most_recent_character(acct);
    
    d->mfa_verified = false;

    // Create temporary arrays to store characters for sorting
    ACCOUNT_CHARACTER *staff_chars[10];
    ACCOUNT_CHARACTER *regular_chars[100];
    int staff_count = 0;
    int regular_count = 0;

    if (!acct) {
        write_to_buffer(d, "\n\r{RERROR: Account data missing. Please reconnect or contact staff.{x\n\r", 0);
        close_socket(d);
        return;
    }
    
    write_to_buffer(d, "\n\r{B=={W[ {YSENTIENCE ACCOUNT MENU {W]{B=={x\n\r\n\r", 0);
    
    sprintf(buf, "Account: {C%s{x\n\r", acct->username);
    write_to_buffer(d, buf, 0);
    if (game_settings.enable_email){
    if (game_settings.require_email_verif && !acct->email_verified && !IS_NULLSTR(acct->pending_email)) {
        sprintf(buf, "Email: {C%s{x (pending: {Y%s{x)\n\r\n\r", IS_NULLSTR(acct->email) ? "Not set" : acct->email, acct->pending_email);
    } else if (game_settings.require_email_verif && !acct->email_verified && IS_NULLSTR(acct->pending_email)) {
        sprintf(buf, "Email: {YPending verification{x\n\r\n\r");
    } else {
        sprintf(buf, "Email: {C%s{x\n\r\n\r", IS_NULLSTR(acct->email) ? "Not set" : acct->email);
    }
    write_to_buffer(d, buf, 0);
}

    /* Account status summary line */
    {
        int active_penalties = 0, active_bonuses = 0;
        PENALTY_DATA *pen;
        BONUS_DATA *bon;

        for (pen = acct->penalties; pen; pen = pen->next)
            if (!is_penalty_expired(pen)) active_penalties++;
        for (bon = acct->bonuses; bon; bon = bon->next)
            if (!is_bonus_expired(bon)) active_bonuses++;

        int unlocks = account_race_unlock_count(acct);

        /* Build compact status line */
        char status[MAX_STRING_LENGTH];
        status[0] = '\0';

        if (active_penalties > 0) {
            sprintf(buf, "{R%d penalt%s{x", active_penalties,
                active_penalties == 1 ? "y" : "ies");
            strcat(status, buf);
        }
        if (active_bonuses > 0) {
            if (status[0] != '\0') strcat(status, " | ");
            sprintf(buf, "{G%d bonus%s{x", active_bonuses,
                active_bonuses == 1 ? "" : "es");
            strcat(status, buf);
        }
        if (unlocks > 0) {
            if (status[0] != '\0') strcat(status, " | ");
            sprintf(buf, "{C%d race unlock%s{x", unlocks,
                unlocks == 1 ? "" : "s");
            strcat(status, buf);
        }

        /* Staff: show note summary */
        if (d->character && IS_IMMORTAL(d->character)) {
            int acct_notes = account_note_count(acct->staff_notes);
            if (acct_notes > 0) {
                if (status[0] != '\0') strcat(status, " | ");
                sprintf(buf, "{Y%d staff note%s{x", acct_notes,
                    acct_notes == 1 ? "" : "s");
                strcat(status, buf);
            }
        }

        if (status[0] != '\0') {
            write_to_buffer(d, "Status: ", 0);
            write_to_buffer(d, status, 0);
            write_to_buffer(d, "\n\r", 0);
        }
    }
    
    // Separate and sort characters into staff/regular buckets
    sort_account_characters(acct, staff_chars, &staff_count, regular_chars, &regular_count, 100);
    
    // Display staff characters first
    if (staff_count > 0) {
        write_to_buffer(d, "{B=={W[ {YSTAFF CHARACTERS {W]{B=={x\n\r", 0);
        
        sprintf(buf, "{D%-4s %-16s %-15s %-30s{x\n\r", 
                "Num", "Name", "Rank", "Location");
        write_to_buffer(d, buf, 0);


        sprintf(buf, "{D%s{x\n\r", pad_string("", 70, NULL, "-"));
        write_to_buffer(d, buf, 0);
        
        for (int i = 0; i < staff_count; i++) {
            ch_entry = staff_chars[i];
            const char *staff_rank_str = flag_string(staff_ranks, ch_entry->staff_rank);
            
        CHAR_DATA *live_ch = get_char_world(NULL, ch_entry->name);
        const char *loc_str;
        if (live_ch && live_ch->in_room) {
            loc_str = format_location_string(
            live_ch->in_room ? live_ch->in_room : NULL
            );
        } else {
            loc_str = str_dup(
                ch_entry->last_area
            );
    }
            sprintf(name_buf, "{W%s%s{x", ch_entry->name, ch_entry->deleted ? " {R(D){X" : "");
            sprintf(rank_buf, "{R%s{x", staff_rank_str ? capitalize(staff_rank_str) : "IMM");
            
            sprintf(buf, "{G[%2d]{x %s%s %s%s {Y%s{x\n\r",
                    i + 1,
                    name_buf,
                    pad_string(name_buf, 16, NULL, " "),
                    rank_buf,
                    pad_string(rank_buf, 15, NULL, " "),
                    loc_str);
            write_to_buffer(d, buf, 0);
        }
    }
    
    // Display regular characters
    if (regular_count > 0) {
        write_to_buffer(d, "\n\r{B=={W[ {YREGULAR CHARACTERS {W]{B=={x\n\r", 0);
        
        sprintf(buf, "{D%-4s %-16s %-7s %-12s %-12s %-25s{x\n\r", 
                "Num", "Name", "Level", "Race", "Class", "Location");
        write_to_buffer(d, buf, 0);

        
        sprintf(buf, "{D%s{x\n\r", pad_string("", 90, NULL, "-"));
        write_to_buffer(d, buf, 0);
        
        for (int i = 0; i < regular_count; i++) {
            ch_entry = regular_chars[i];
            
            CHAR_DATA *live_ch = get_char_world(NULL, ch_entry->name);
            const char *loc_str;
            if (live_ch && live_ch->in_room) {
                loc_str = format_location_string(
                    live_ch->in_room ? live_ch->in_room : NULL
                );
            } else {
                loc_str = str_dup(
                    ch_entry->last_area
                );
            }
            
            sprintf(name_buf, "{W%s%s{x", ch_entry->name, ch_entry->deleted ? " {R(D){X" : "");
            
            sprintf(level_buf, "{G%d(%d){x", 
                    ch_entry->current_level > 0 ? ch_entry->current_level : ch_entry->tot_level,
                    ch_entry->tot_level);
            
            sprintf(race_buf, "{W%s{x", 
                    ch_entry->race_name ? capitalize(ch_entry->race_name) : "Unknown");
            sprintf(class_buf, "{W%s{x", 
                    ch_entry->class_name ? capitalize(ch_entry->class_name) : "Adventurer");
            
            sprintf(buf, "{G[%2d]{x %s%s %s%s %s%s %s%s {Y%s{x\n\r",
                    i + staff_count + 1,
                    name_buf,
                    pad_string(name_buf, 16, NULL, " "),
                    level_buf,
                    pad_string(level_buf, 7, NULL, " "),
                    race_buf,
                    pad_string(race_buf, 12, NULL, " "),
                    class_buf,
                    pad_string(class_buf, 12, NULL, " "),
                    loc_str);
            write_to_buffer(d, buf, 0);
        }
    }
    
    if (staff_count == 0 && regular_count == 0) {
        write_to_buffer(d, "   {RNo characters found.{x\n\r", 0);
    }
    
    // Display active account penalties
    if (acct->penalties) {
        write_to_buffer(d, "\n\r{RActive Penalties:{x\n\r", 0);
        PENALTY_DATA *pen;
        for (pen = acct->penalties; pen; pen = pen->next) {
            if (is_penalty_expired(pen))
                continue;
            char dur[64];
            if (pen->expires_at == 0)
                sprintf(dur, "{RPermanent{x");
            else {
                penalty_format_duration(pen->expires_at - current_time, dur, sizeof(dur));
            }
            if (pen->scope == PENALTY_SCOPE_CHARACTER && !IS_NULLSTR(pen->target_name)) {
                sprintf(buf, "   {R*{x %-14s ({Y%s{x) - %s\n\r",
                    penalty_type_name(pen->type), pen->target_name, dur);
            } else {
                sprintf(buf, "   {R*{x %-14s ({Caccount{x) - %s\n\r",
                    penalty_type_name(pen->type), dur);
            }
            write_to_buffer(d, buf, 0);
        }
    }

    // Display active account bonuses
    if (acct->bonuses) {
        write_to_buffer(d, "\n\r{GActive Bonuses:{x\n\r", 0);
        BONUS_DATA *bon;
        for (bon = acct->bonuses; bon; bon = bon->next) {
            if (is_bonus_expired(bon))
                continue;
            char dur[64];
            if (bon->expires_at == 0)
                sprintf(dur, "{GPermanent{x");
            else {
                penalty_format_duration(bon->expires_at - current_time, dur, sizeof(dur));
            }
            char scope_str[64];
            if (bon->scope == BONUS_SCOPE_UNASSIGNED)
                sprintf(scope_str, "{Yunassigned{x");
            else if (bon->scope == BONUS_SCOPE_CHARACTER && !IS_NULLSTR(bon->target_name))
                sprintf(scope_str, "{Y%s{x", bon->target_name);
            else
                sprintf(scope_str, "{Caccount{x");
            sprintf(buf, "   {G+{x %-10s %+d%% (%s) - %s\n\r",
                bonus_type_name(bon->type), bon->value, scope_str, dur);
            write_to_buffer(d, buf, 0);
        }
    }

    // Display menu options with divider
    write_to_buffer(d, "\n\r", 0);
    sprintf(buf, "{C%s{x\n\r", pad_string("", 60, NULL, "="));
    write_to_buffer(d, buf, 0);

    int total_count = staff_count + regular_count;
    if (total_count == 1) {
        write_to_buffer(d, "{G1{x) Select character\n\r", 0);
    } else if (total_count > 1) {
        sprintf(buf, "{G1-%d{x) Select a character\n\r", total_count);
        write_to_buffer(d, buf, 0);
    }

    bool can_create_char = true;
    if (!account_has_immortal(acct)) {
        if (game_settings.new_char_lock)
            can_create_char = false;
    }
    if (can_create_char) {
        write_to_buffer(d, "{GC{x) Create a new character\n\r", 0);
    }

    if (acct->acct_flags & IS_SET(acct->acct_flags,ACCT_CAN_CREATE_STAFF)) {
        int staff_limit = acct->staff_limit;
        int staff_count = account_count_staff_characters(acct);
        if (staff_limit == 0 || staff_count < staff_limit) {
            write_to_buffer(d, "{GI{x) Create a new staff character\n\r", 0);
        }
    }
    
    if (can_link_characters(acct)) {
        write_to_buffer(d, "{GL{x) Link existing character\n\r", 0);
    }

    if (game_settings.vault_enabled) {
        write_to_buffer(d, "{GS{x) Shared Storage (Vault) Info\n\r", 0);
    }
    if (game_settings.enable_email){

    
    write_to_buffer(d, "{GE{x) Change email address\n\r", 0);
    if (game_settings.require_email_verif && !acct->email_verified) {
    write_to_buffer(d, "{GV{x) Verify email address\n\r", 0);
    if (!IS_NULLSTR(acct->pending_email))
        write_to_buffer(d, "{GR{x) Resend verification email\n\r", 0);
    }
}
    if (!DEV_SKIP_PASSWORD)
    write_to_buffer(d, "{GP{x) Change password\n\r", 0);
    
    if (!DEV_SKIP_MFA)
    write_to_buffer(d, "{GM{x) MFA settings\n\r", 0);

    write_to_buffer(d, "{GA{x) Account preferences\n\r", 0);

    if (default_char != NULL) {
        sprintf(buf, "{GY{x) Log in with default character ({C%s{x)\n\r", default_char);
        write_to_buffer(d, buf, 0);
    }

    if (recent_char != NULL && !is_character_online(recent_char->name)) {
        sprintf(buf, "{GZ{x) Log in with last played character ({C%s{x)\n\r", recent_char->name);
        write_to_buffer(d, buf, 0);
    }
        
    write_to_buffer(d, "{GQ{x) Quit\n\r\n\r", 0);

}

// This handles the selections in the account menu.
// Dispatches to handler functions in nanny_menus.c.
void login_account_menu(DESCRIPTOR_DATA *d, char *argument)
{
    char arg[MAX_INPUT_LENGTH];

    /* Handle single-letter menu options */
    if (argument[0] != '\0' && argument[1] == '\0' && !isdigit(argument[0])) {
        switch (toupper(argument[0])) {
            case 'C': handle_account_create_character(d); return;
            case 'I': handle_account_create_staff(d);     return;
            case 'L': handle_account_link_character(d);    return;
            case 'E': handle_account_change_email(d);      return;
            case 'P': handle_account_change_password(d);   return;
            case 'M': handle_account_mfa_menu(d);          return;
            case 'A': handle_account_preferences(d);     return;
            case 'Q': handle_account_logout(d);            return;
            case 'R': handle_account_resend_verification(d); return;
            case 'S': handle_account_shared_storage(d);    return;
            case 'V': handle_account_verify_email(d);      return;
            case 'Y': handle_account_default_character(d); return;
            case 'Z': handle_account_recent_character(d);  return;
            default:
                write_to_buffer(d, "Invalid choice.\n\r", 0);
                display_account_menu(d);
                return;
        }
    }

    /* Handle numeric or alpha character selection */
    if (isdigit(argument[0])) {
        handle_account_select_by_number(d, atoi(argument));
        return;
    }

    if (isalpha(argument[0])) {
        one_argument(argument, arg);
        handle_account_select_by_name(d, arg);
        return;
    }

    /* Nothing matched */
    write_to_buffer(d, "Invalid choice.\n\r", 0);
    display_account_menu(d);
}


// This function displays the MFA menu for the account.
// It shows the current MFA status, options to set up or disable MFA,
// and options to view or email recovery codes.
// It is paired with login_account_mfa_menu to handle the actual selections.
void display_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];
    bool mfa_enabled = !IS_NULLSTR(acct->mfa_key);
    bool mfa_pending = !IS_NULLSTR(acct->mfa_pending_key);
    bool has_secret = mfa_enabled || mfa_pending;
    bool has_recovery = has_recovery_codes((const char **)acct->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(acct->email);

    // If MFA is enabled, require code before allowing changes (except confirmation)
    if (mfa_enabled && !mfa_pending && !d->mfa_verified) {
        d->connected = CON_ACCOUNT_MFA_VERIFY_FOR_SETTINGS;
        return;
    }

    write_to_buffer(d, "\n\r{B=={W[ {YMFA SETTINGS {W]{B=={x\n\r", 0);
    sprintf(buf, "MFA is currently: %s\n\r",
        mfa_enabled ? "{GENABLED{x" : (mfa_pending ? "{YSETUP IN PROGRESS{x" : "{ROFF{x"));
    write_to_buffer(d, buf, 0);

    // --- Setup Section ---
    if (!mfa_enabled && !mfa_pending)
        write_to_buffer(d, "\n\r{CSetup:{x\n\r{GS{x) Set up MFA\n\r", 0);
    else if (mfa_pending)
        write_to_buffer(d, "\n\r{CSetup:{x\n\r{GC{x) Confirm MFA setup\n\r", 0);

    // --- QR Code Section ---
    bool show_qr_section = has_secret;
    if (show_qr_section) {
        write_to_buffer(d, "\n\r{CQR Code:{x\n\r", 0);
        if (has_secret)
            write_to_buffer(d, "{GD{x) Display QR code in terminal\n\r", 0);
        if (has_secret && has_email)
            write_to_buffer(d, "{GQ{x) Email QR code\n\r", 0);
    }

    // --- Recovery Codes Section ---
    bool show_recovery_section = (mfa_enabled || mfa_pending) && has_recovery;
    if (show_recovery_section) {
        write_to_buffer(d, "\n\r{CRecovery Codes:{x\n\r", 0);
        write_to_buffer(d, "{GV{x) Show recovery codes\n\r", 0);
        if (has_email)
            write_to_buffer(d, "{GE{x) Email recovery codes\n\r", 0);
    }

    // --- Actions Section ---
    bool show_actions = (mfa_enabled || mfa_pending);
    if (show_actions) {
        write_to_buffer(d, "\n\r{CActions:{x\n\r", 0);
        if (mfa_enabled || mfa_pending)
            write_to_buffer(d, "{GX{x) Disable MFA\n\r", 0);
        // Only show generate/regenerate if not pending
        if (mfa_enabled) {
            write_to_buffer(d, has_recovery ? "{GG{x) Regenerate recovery codes\n\r" : "{GG{x) Generate recovery codes\n\r", 0);
        }
    }

    write_to_buffer(d, "\n\r{GB{x) Back to account menu\n\r", 0);
    d->connected = CON_ACCOUNT_MFA_MENU;
}

// This function handles the MFA menu selections.
// It allows the user to set up MFA, confirm setup, disable MFA,
// display or email QR codes, and manage recovery codes.
// It is paired with display_account_mfa_menu to show the menu.

void login_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    bool mfa_enabled = !IS_NULLSTR(acct->mfa_key);
    bool mfa_pending = !IS_NULLSTR(acct->mfa_pending_key);
    bool has_secret = mfa_enabled || mfa_pending;
    bool has_recovery = has_recovery_codes((const char **)acct->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(acct->email);

    switch (toupper(argument[0])) {
        case 'S': // Set up MFA
            if (mfa_enabled || mfa_pending) {
                write_to_buffer(d, "MFA is already enabled or setup is in progress.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            
            // Call setup_mfa_for_char instead of manually creating keys
            if (!setup_mfa_for_account(d, false)) {
                write_to_buffer(d, "Error setting up MFA. Please try again.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            
            // Display the character MFA menu again with updated status
            display_account_mfa_menu(d, "");
            
            break;
        case 'C': // Confirm MFA setup
            if (!mfa_pending) {
                write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            d->connected = CON_ACCOUNT_MFA_CONFIRM;
            break;
        case 'Q': // Email QR code
            if (!has_secret) {
                write_to_buffer(d, "No MFA secret available to email a QR code.\n\r", 0);
            } else if (!has_email) {
                write_to_buffer(d, "No email address set for this account.\n\r", 0);
            } else {
                send_qr_email_for_account(acct, acct->email, 
                    mfa_pending ? acct->mfa_pending_key : acct->mfa_key);
                write_to_buffer(d, "QR code emailed.\n\r", 0);
            }
            display_account_mfa_menu(d, "");
            break;
        case 'D': // Display QR code in terminal
            if (!has_secret) {
                write_to_buffer(d, "No MFA secret available to display a QR code.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            } else {
                char qr_url[MIL];
                if (mfa_pending)
                    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, acct->mfa_pending_key);
                else
                    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, acct->mfa_key);
                display_qr_code(d, qr_url);
                display_account_mfa_key(d, acct);
            }
            display_account_mfa_menu(d, "");
            break;
        case 'X': // Disable MFA
            if (!mfa_enabled && !mfa_pending) {
                write_to_buffer(d, "MFA is not enabled or pending for this account.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            write_to_buffer(d, "Are you sure you want to disable MFA? (Y/N): ", 0);
            d->connected = CON_ACCOUNT_MFA_DISABLE_CONFIRM;
            break;
        case 'V': // Show recovery codes
            if (!(mfa_enabled || mfa_pending) || !has_recovery) {
                write_to_buffer(d, "No recovery codes available to display.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            display_account_recovery_codes(d, acct);
            display_account_mfa_menu(d, "");
            break;
        case 'E': // Email recovery codes
            if (!(mfa_enabled || mfa_pending) || !has_recovery) {
                write_to_buffer(d, "No recovery codes available to email.\n\r", 0);
            } else if (!has_email) {
                write_to_buffer(d, "No email address set for this account.\n\r", 0);
            } else {
                send_recovery_codes_email_for_account(acct, acct->email);
                write_to_buffer(d, "Recovery codes emailed.\n\r", 0);
            }
            display_account_mfa_menu(d, "");
            break;
        case 'G': // Generate/Regenerate recovery codes
            if (!(mfa_enabled || mfa_pending)) {
                write_to_buffer(d, "MFA must be enabled or pending to generate recovery codes.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            generate_recovery_codes(acct->recovery_codes, acct->recovery_used, MFA_RECOVERY_CODES);
            save_account(acct);
            write_to_buffer(d, has_recovery ? "Recovery codes regenerated.\n\r" : "Recovery codes generated.\n\r", 0);
            display_account_recovery_codes(d, acct);
            display_account_mfa_menu(d, "");
            break;
        case 'B': // Back
            display_account_menu(d);
            d->connected = CON_ACCOUNT_MENU;
            break;
        default:
            write_to_buffer(d, "Invalid or unavailable choice.\n\r", 0);
            display_account_mfa_menu(d, "");
            break;
    }
}


/***************************************************************************
 * Account Preferences Menu                                                *
 ***************************************************************************/

/**
 * get_default_bool - Get the effective default value for a toggle setting
 *
 * Returns the game's source-of-truth default for a setting. Checks
 * game_settings.pref_defaults first (managed via 'prefadmin defaults'),
 * then falls back to the factory default from pc_set_table if no
 * explicit entry exists.
 *
 * @param name           The setting name
 * @param table_default  Factory default from pc_set_table (SETTING_ON/OFF)
 * @return               true if the default is ON
 */
static bool get_default_bool(const char *name, int table_default)
{
    /* Check game_settings overrides first */
    PREF_ENTRY *gp = pref_find(game_settings.pref_defaults, name);
    if (gp && gp->type == PREF_TYPE_BOOL)
        return gp->val.b;

    return (table_default == SETTING_ON);
}

/* Channel names and their pref keys for account-level display/toggle.
 * Channels are stored as "channel_<name>" boolean prefs where true = ON.
 * The canonical table is acct_channel_defaults in preferences.c;
 * this alias provides local convenience. */
#define acct_channel_table acct_channel_defaults

/**
 * display_account_prefs_menu - Show account preferences with toggle options
 *
 * Displays all player-level toggle settings, channel settings, prompt,
 * scroll, and wimpy with their current effective value at the account
 * level. Settings with an account override show (acct), otherwise (def)
 * for game default.
 *
 * @param d  The descriptor at the account menu
 */
void display_account_prefs_menu(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];

    write_to_buffer(d, "\n\r{B=={W[ {YACCOUNT PREFERENCES {W]{B=={x\n\r", 0);
    write_to_buffer(d, "{DSet defaults for all your characters. Character-specific\n\r"
                       "overrides (set in-game via 'prefs') take priority.{x\n\r", 0);

    /* --- Toggle Settings --- */
    write_to_buffer(d, "\n\r{Y--- Toggle Settings ---{x\n\r", 0);
    write_to_buffer(d, "{D  Setting          Value    Source{x\n\r", 0);
    write_to_buffer(d, "{D──────────────────────────────────────────{x\n\r", 0);

    for (int i = 0; pc_set_table[i].name; i++) {
        if (pc_set_table[i].min_rank > STAFF_PLAYER)
            continue;

        PREF_ENTRY *acct_pref = pref_find(acct->preferences, pc_set_table[i].name);
        bool is_on;
        const char *source_tag;

        if (acct_pref && acct_pref->type == PREF_TYPE_BOOL) {
            is_on = acct_pref->val.b;
            source_tag = "{C(acct){x";
        } else {
            is_on = get_default_bool(pc_set_table[i].name,
                                     pc_set_table[i].default_state);
            source_tag = "{D(def){x ";
        }

        sprintf(buf, "  %-16s %s    %s\n\r",
                pc_set_table[i].name,
                is_on ? "{WON{x " : "{DOFF{x",
                source_tag);
        write_to_buffer(d, buf, 0);
    }

    /* Wimpy */
    {
        PREF_ENTRY *wp = pref_find(acct->preferences, "wimpy");
        if (wp && wp->type == PREF_TYPE_INT) {
            sprintf(buf, "  %-16s {W%-5d{x  {C(acct){x\n\r", "wimpy", wp->val.i);
        } else {
            PREF_ENTRY *gp = pref_find(game_settings.pref_defaults, "wimpy");
            int def_wimpy = (gp && gp->type == PREF_TYPE_INT) ? gp->val.i : 0;
            sprintf(buf, "  %-16s {W%-5d{x  {D(def){x\n\r", "wimpy", def_wimpy);
        }
        write_to_buffer(d, buf, 0);
    }

    /* Scroll (lines per page) */
    {
        PREF_ENTRY *sp = pref_find(acct->preferences, "scroll");
        if (sp && sp->type == PREF_TYPE_INT) {
            if (sp->val.i == 0)
                sprintf(buf, "  %-16s {DOFF{x    {C(acct){x\n\r", "scroll");
            else
                sprintf(buf, "  %-16s {W%-5d{x  {C(acct){x\n\r", "scroll", sp->val.i);
        } else {
            PREF_ENTRY *gp = pref_find(game_settings.pref_defaults, "scroll");
            int def_scroll = (gp && gp->type == PREF_TYPE_INT) ? gp->val.i : 0;
            if (def_scroll == 0)
                sprintf(buf, "  %-16s {DOFF{x    {D(def){x\n\r", "scroll");
            else
                sprintf(buf, "  %-16s {W%-5d{x  {D(def){x\n\r", "scroll", def_scroll);
        }
        write_to_buffer(d, buf, 0);
    }

    /* --- Channel Settings --- */
    write_to_buffer(d, "\n\r{Y--- Channel Settings ---{x\n\r", 0);
    write_to_buffer(d, "{D  Channel          Status   Source{x\n\r", 0);
    write_to_buffer(d, "{D──────────────────────────────────────────{x\n\r", 0);

    for (int i = 0; acct_channel_table[i].name; i++) {
        PREF_ENTRY *cp = pref_find(acct->preferences, acct_channel_table[i].pref_key);
        bool is_on;
        const char *source_tag;

        if (cp && cp->type == PREF_TYPE_BOOL) {
            is_on = cp->val.b;
            source_tag = "{C(acct){x";
        } else {
            /* Default: all channels ON */
            is_on = true;
            source_tag = "{D(def){x ";
        }

        sprintf(buf, "  %-16s %s    %s\n\r",
                acct_channel_table[i].name,
                is_on ? "{WON{x " : "{DOFF{x",
                source_tag);
        write_to_buffer(d, buf, 0);
    }

    /* --- Prompt --- */
    write_to_buffer(d, "\n\r{Y--- Prompt ---{x\n\r", 0);
    {
        PREF_ENTRY *pp = pref_find(acct->preferences, "prompt");
        if (pp && pp->type == PREF_TYPE_STRING && !IS_NULLSTR(pp->val.str)) {
            sprintf(buf, "  Prompt: {W%s{x  {C(acct){x\n\r", pp->val.str);
        } else {
            sprintf(buf, "  Prompt: {W(game default){x  {D(def){x\n\r");
        }
        write_to_buffer(d, buf, 0);
    }

    /* Summary */
    int acct_prefs = acct->preferences ? pref_count(acct->preferences) : 0;
    write_to_buffer(d, "\n\r", 0);
    if (acct_prefs > 0) {
        sprintf(buf, "{D%d account preference%s set.{x\n\r",
                acct_prefs, acct_prefs == 1 ? "" : "s");
        write_to_buffer(d, buf, 0);
    }

    write_to_buffer(d, "\n\r{DType a setting or channel name to toggle it.{x\n\r", 0);
    write_to_buffer(d, "{DType 'wimpy <value>' to set wimpy.{x\n\r", 0);
    write_to_buffer(d, "{DType 'scroll <lines>' to set scroll (0 = off, 10-100).{x\n\r", 0);
    write_to_buffer(d, "{DType 'prompt <string>' to set prompt, 'prompt clear' to reset.{x\n\r", 0);
    write_to_buffer(d, "{GR{x) Reset all account preferences\n\r", 0);
    write_to_buffer(d, "{GB{x) Back to account menu\n\r\n\r", 0);

    d->connected = CON_ACCOUNT_PREFS;
}

/**
 * login_account_prefs_menu - Handle input at the account preferences menu
 *
 * Processes setting toggles, channel toggles, wimpy, scroll, prompt
 * changes, reset, and back navigation.
 *
 * @param d         The descriptor
 * @param argument  User input
 */
void login_account_prefs_menu(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (IS_NULLSTR(arg)) {
        display_account_prefs_menu(d);
        return;
    }

    /* Back to account menu */
    if (toupper(arg[0]) == 'B' && arg[1] == '\0') {
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    /* Reset all account preferences */
    if (toupper(arg[0]) == 'R' && arg[1] == '\0') {
        int count = acct->preferences ? pref_count(acct->preferences) : 0;
        if (count == 0) {
            write_to_buffer(d, "\n\rYou have no account preferences to reset.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        /* Free all preferences */
        while (acct->preferences) {
            PREF_ENTRY *next = acct->preferences->next;
            if (acct->preferences->key && acct->preferences->key != str_empty)
                free_string(acct->preferences->key);
            if (acct->preferences->type == PREF_TYPE_STRING
                && acct->preferences->val.str
                && acct->preferences->val.str != str_empty)
                free_string(acct->preferences->val.str);
            free(acct->preferences);
            acct->preferences = next;
        }

        save_account(acct);
        sprintf(buf, "\n\rCleared %d account preference%s. All settings reverted to game defaults.\n\r",
                count, count == 1 ? "" : "s");
        write_to_buffer(d, buf, 0);
        display_account_prefs_menu(d);
        return;
    }

    /* Wimpy setting */
    if (!str_prefix(arg, "wimpy")) {
        char val_arg[MAX_INPUT_LENGTH];
        one_argument(argument, val_arg);

        if (IS_NULLSTR(val_arg)) {
            write_to_buffer(d, "\n\rSyntax: wimpy <value>\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        if (!is_number(val_arg)) {
            write_to_buffer(d, "\n\rWimpy must be a number.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        int val = atoi(val_arg);
        if (val < 0 || val > 1000) {
            write_to_buffer(d, "\n\rWimpy must be between 0 and 1000.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        pref_set_int(&acct->preferences, PREF_CAT_TOGGLE, "wimpy", val);
        save_account(acct);
        sprintf(buf, "\n\rAccount wimpy set to {W%d{x.\n\r", val);
        write_to_buffer(d, buf, 0);
        display_account_prefs_menu(d);
        return;
    }

    /* Scroll setting */
    if (!str_prefix(arg, "scroll")) {
        char val_arg[MAX_INPUT_LENGTH];
        one_argument(argument, val_arg);

        if (IS_NULLSTR(val_arg)) {
            write_to_buffer(d, "\n\rSyntax: scroll <lines>  (0 = off, 10-100)\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        if (!is_number(val_arg)) {
            write_to_buffer(d, "\n\rScroll must be a number.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        int val = atoi(val_arg);
        if (val != 0 && (val < 10 || val > 100)) {
            write_to_buffer(d, "\n\rScroll must be 0 (off) or between 10 and 100.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        pref_set_int(&acct->preferences, PREF_CAT_DISPLAY, "scroll", val);
        save_account(acct);
        if (val == 0)
            write_to_buffer(d, "\n\rAccount scroll paging {DOFF{x.\n\r", 0);
        else {
            sprintf(buf, "\n\rAccount scroll set to {W%d{x lines.\n\r", val);
            write_to_buffer(d, buf, 0);
        }
        display_account_prefs_menu(d);
        return;
    }

    /* Prompt setting */
    if (!str_prefix(arg, "prompt")) {
        if (IS_NULLSTR(argument)) {
            write_to_buffer(d, "\n\rSyntax: prompt <string>  or  prompt clear\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        if (!str_cmp(argument, "clear") || !str_cmp(argument, "reset")) {
            pref_remove(&acct->preferences, "prompt");
            save_account(acct);
            write_to_buffer(d, "\n\rAccount prompt cleared. Game default will be used.\n\r", 0);
            display_account_prefs_menu(d);
            return;
        }

        pref_set_string(&acct->preferences, PREF_CAT_PROMPT, "prompt", argument);
        save_account(acct);
        sprintf(buf, "\n\rAccount prompt set to: {W%s{x\n\r", argument);
        write_to_buffer(d, buf, 0);
        display_account_prefs_menu(d);
        return;
    }

    /* Toggle a setting from pc_set_table */
    for (int i = 0; pc_set_table[i].name; i++) {
        if (pc_set_table[i].min_rank > STAFF_PLAYER)
            continue;

        if (!str_prefix(arg, pc_set_table[i].name)) {
            PREF_ENTRY *acct_pref = pref_find(acct->preferences, pc_set_table[i].name);
            bool current_val;

            if (acct_pref && acct_pref->type == PREF_TYPE_BOOL) {
                current_val = acct_pref->val.b;
            } else {
                current_val = get_default_bool(pc_set_table[i].name,
                                               pc_set_table[i].default_state);
            }

            bool new_val = !current_val;
            pref_set_bool(&acct->preferences, PREF_CAT_TOGGLE,
                          pc_set_table[i].name, new_val);
            save_account(acct);

            sprintf(buf, "\n\r%s is now %s{x for your account.\n\r",
                    pc_set_table[i].name,
                    new_val ? "{WON" : "{DOFF");
            write_to_buffer(d, buf, 0);
            display_account_prefs_menu(d);
            return;
        }
    }

    /* Toggle a channel */
    for (int i = 0; acct_channel_table[i].name; i++) {
        if (!str_prefix(arg, acct_channel_table[i].name)) {
            PREF_ENTRY *cp = pref_find(acct->preferences,
                                       acct_channel_table[i].pref_key);
            bool current_val = (cp && cp->type == PREF_TYPE_BOOL)
                               ? cp->val.b : true; /* default: ON */

            bool new_val = !current_val;
            pref_set_bool(&acct->preferences, PREF_CAT_CHANNEL,
                          acct_channel_table[i].pref_key, new_val);
            save_account(acct);

            sprintf(buf, "\n\r%s channel is now %s{x for your account.\n\r",
                    acct_channel_table[i].name,
                    new_val ? "{WON" : "{DOFF");
            write_to_buffer(d, buf, 0);
            display_account_prefs_menu(d);
            return;
        }
    }

    /* No match */
    write_to_buffer(d, "\n\rUnknown setting. Type a setting or channel name to toggle it.\n\r", 0);
    display_account_prefs_menu(d);
}

void login_get_name(DESCRIPTOR_DATA *d, char *argument)
{

    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *ch;
    bool fOld;


    while (ISSPACE(*argument))
        argument++;

    ch = d->character;

    if (!argument[0]) {
        close_socket(d);
        return;
    }

    argument[0] = UPPER(argument[0]);
    if (!check_parse_name(argument)) {
        write_to_buffer(d, "Illegal name, try another.\n\r", 0);
        return;
    }

    // Temporarily disabling for reconnect crash.
    // Check if the player is already playing
    /*
    if ((ch = find_existing_player(argument)) != NULL)
    {
        fOld = true;
        d->character = ch;
    }
    else
    {*/
        fOld = load_char_obj(d, argument);

        ch = d->character;

        if (IS_SET(ch->act[0], PLR_DENY)
            || (d->account && has_penalty(d->account, PENALTY_DENY, ch->name)))
        {
            log_message_f(LOG_LEVEL_WARN, LOG_SECURITY, "Denying access to %s@%s.", argument, d->host);
            write_to_buffer(d, "You are denied access.\n\r", 0);
            close_socket(d);
            return;
        }

        if (check_ban(d->host,BAN_PERMIT))
        {
            write_to_buffer(d,"Your site has been banned from Sentience.\n\r",0);
            close_socket(d);
            return;
        }

        // Adding back in for reconnect crash
        if (check_reconnect(d, argument, false))
            fOld = true;
        else
        {
            if (game_settings.wizlock && !IS_IMMORTAL(ch))
            {
                if (!IS_NULLSTR(game_settings.wizlock_msg))
                    write_to_buffer(d, game_settings.wizlock_msg, 0);
                else
                    write_to_buffer(d, "The game is wizlocked.\n\r", 0);

                log_message_f(LOG_LEVEL_INFO, LOG_SECURITY, "The game is wizlocked, %s tried to connect from %s.", argument, d->host);
                sprintf(buf, "Wizlocked: %s tried to connect from %s.", argument, d->host);
                wiznet(buf, ch, NULL, WIZ_LOGINS, 0, 0);
                close_socket(d);
                return;
            }
        }
    

    /* Old player */
    if (fOld)
    {
        if (d->character->pcdata->reset_state == RESET_PENDING)
            write_to_buffer(d, "Password or Reset Code: ", 0);
        else
            write_to_buffer(d, "Password: ", 0);
        ProtocolNoEcho(d,true);
        d->connected = CON_GET_OLD_PASSWORD;

#if 0
        if (port != PORT_SYN) {
            write_to_buffer(d, "Password: ", 0);
            ProtocolNoEcho(d,true);
            d->connected = CON_GET_OLD_PASSWORD;
        }

        /* Syn - placed here for ease of testing so that I don't have to spam through
            pw entry/motd's every single time I boot the game. DEBUG is a definition
            as a safeguard just in case someone runs it on PORT_SYN for whatever reason. */
        else if (DEBUG == true)
        {
            write_to_buffer(d, "Welcome back, Master.\n\r", 0);
            if (check_playing(d,ch->name))
                return;

            if (check_reconnect(d,ch->name,true))
                return;

            reset_char(ch);

            list_appendlink(loaded_chars, ch);
            list_appendlink(loaded_players, ch);

            char_to_room(ch, ch->in_room);
            d->connected = CON_PLAYING;
            do_function(d->character, &do_look, "");
        }

#endif
        return;
    }
    else
    {
        /* New player */
        /* Todo: Update this for account system */
        if (game_settings.new_acct_lock || game_settings.new_char_lock)
        {
            if (game_settings.new_acct_lock)
                write_to_buffer(d, game_settings.new_acct_lock_msg, 0);
            else if (game_settings.new_char_lock)
                write_to_buffer(d, game_settings.new_char_lock_msg, 0);
            else
                write_to_buffer(d, "New characters are not allowed.\n\r", 0);
            write_to_buffer(d, "The game is newlocked.\n\r", 0);
            close_socket(d);
            return;
        }

        if (check_ban(d->host,BAN_NEWBIES))
        {
            write_to_buffer(d, "New players are not allowed from your site.\n\r",0);
            close_socket(d);
            return;
        }

        if(game_settings.dev_server) 
        {
            game_settings.new_char_lock = true;	
            game_settings.new_acct_lock = true;
        }

        sprintf(buf, "\n\rDo you want to create a character named %s (Y/N)? ", argument);
        write_to_buffer(d, buf, 0);
        d->connected = CON_CONFIRM_NEW_NAME;
        return;
    }

}

void login_get_mfa(DESCRIPTOR_DATA *d, char *argument)
{

    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    if (check_mfa(ch, argument))
    {

        if (check_playing(d,ch->name))
        return;

        if (check_reconnect(d, ch->name, true))
        return;
        
        if (IS_IMMORTAL(ch))
        {
            send_to_char("\n\r{BWelcome, Immortal.{x\n\r\n\r", ch);
            do_function(ch, &do_imotd, "");
            if(IS_IMPLEMENTOR(ch)) 
            {
                if(game_settings.wizlock) 
                    send_to_char("\n\r{b-{B==={C=={W[ {YWIZLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
                if(game_settings.new_acct_lock || game_settings.new_char_lock) 
                    send_to_char("\n\r{b-{B==={C=={W[ {GNEWLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
            }
            send_to_char("\n\r{WCurrent active projects:{x\n\r", ch);
            do_function(ch, &do_project, "list open");
            send_to_char("[Hit Return to continue]\n\r", ch);
            d->connected = CON_READ_IMOTD;
        }
        else
        {
            do_function(ch, &do_motd, "");
            d->connected = CON_READ_MOTD;
        }
    }
    else
    {
        if (d->login_attempts > 2)
        {
            write_to_buffer(d, "Too many attempts. Please try again later.\n\r", 0);
            close_socket(d);
            return;
        }
        d->login_attempts++;
        ProtocolNoEcho(d,true);
        d->connected = CON_GET_MFA;
        return;
    }
}


void login_change_passwd_initial(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    const char *old_pwd = NULL;

    while (ISSPACE(*argument))
        argument++;

    if (argument[0] == '\0') {
        // Re-prompt without error if input is empty
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }
    
    // Get the appropriate old password for comparison
    if (has_auth_data) {
        old_pwd = acct_char->pwd;
    } else {
        old_pwd = ch->pcdata->old_pwd;
    }
    
    // Check if the new password is the same as the old one
    if (!IS_NULLSTR(old_pwd)) {
        password_check_status old_match_status = check_encrypted_password(argument, old_pwd, PWD_VER_CRYPT_SYSTEM); 
        if (old_match_status == PWD_CHECK_FAIL && strlen(old_pwd) == 64) { // If not crypt, and looks like sha256
            old_match_status = check_encrypted_password(argument, old_pwd, PWD_VER_SHA256_CUSTOM);
        }
        if (old_match_status == PWD_CHECK_FAIL && old_pwd[0] != '$') { // If not crypt/sha256, try plaintext
             old_match_status = check_encrypted_password(argument, old_pwd, PWD_VER_PLAINTEXT);
        }

        if (old_match_status != PWD_CHECK_FAIL) {
            send_to_char("New password must be DIFFERENT from your current password!\n\r", ch);
            d->connected = CON_CHANGE_PASSWORD;
            return;
        }
    }

    if (!acceptablePassword(d, argument)) {
        // acceptablePassword should re-prompt or set state
        return;
    }

    // Store the first password attempt in the descriptor's buffer
    if (d->new_password_buffer) {
        free_string(d->new_password_buffer);
    }
    d->new_password_buffer = str_dup(argument);

    d->connected = CON_CHANGE_PASSWORD_CONFIRM;
}

void login_change_passwd_confirm(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);

    while (ISSPACE(*argument))
        argument++;

    if (!d->new_password_buffer) {
        write_to_buffer(d, "An error occurred. Please try setting your password again.\n\r", 0);
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }

    if (strcmp(argument, d->new_password_buffer) != 0) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }

    // Passwords match, set it encrypted
    if (has_auth_data) {
        // Set password on account_character
        if (!set_encrypted_password(&acct_char->pwd, &acct_char->pwd_vers, d->new_password_buffer)) {
            write_to_buffer(d, "Error setting new password. Please try again or contact staff.\n\r", 0);
            free_string(d->new_password_buffer);
            d->new_password_buffer = NULL;
            d->connected = CON_CHANGE_PASSWORD;
            return;
        }
        save_account(acct);
    }
    
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;

    send_to_char("\n\r\n\r{Y***{x {RThank you. Please remember to never give your password to anybody.{Y *** {x\n\r\n\r", ch);
    
    ch->pcdata->need_change_pw = false; // Password successfully changed

    // Clear old_pwd if it was being held for this change
    if (ch->pcdata->old_pwd) {
        free_string(ch->pcdata->old_pwd);
        ch->pcdata->old_pwd = NULL;
    }

    ProtocolNoEcho(d, false);

    if (IS_IMMORTAL(ch)) {
        do_function(ch, &do_imotd, "");
        d->connected = CON_READ_IMOTD;
    } else {
        do_function(ch, &do_motd, "");
        d->connected = CON_READ_MOTD;
    }
    return;
}

void login_break_connect(DESCRIPTOR_DATA *d, char *argument)
{
    DESCRIPTOR_DATA *d_old, *d_next;
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    
    // Safety check - we should always have an account at this point
    // If not, log it as a bug and fall back to legacy behavior
    if (!d->account) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "login_break_connect: Called without an account context");
    }
    
    switch(*argument)
    {
    case 'y' : case 'Y':
        for (d_old = descriptor_list; d_old != NULL; d_old = d_next)
        {
            d_next = d_old->next;
            if (d_old == d || d_old->character == NULL)
                continue;

            if (str_cmp(ch->name,d_old->original ?
                d_old->original->name : d_old->character->name))
            continue;

            close_socket(d_old);
        }
        
        if (check_reconnect(d,ch->name,true))
            return;
            
        write_to_buffer(d,"Reconnect attempt failed.\n\r",0);
        if (d->character != NULL)
        {
            free_char(d->character);
            d->character = NULL;
        }
        
        // Always return to account menu since we should have an account
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        break;

    case 'n' : case 'N':
        // Free the character
        if (d->character != NULL)
        {
            free_char(d->character);
            d->character = NULL;
        }
        
        // Always return to account menu since we should have an account
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        break;

    default:
        write_to_buffer(d,"Please type Y or N? ",0);
        break;
    }
}

void login_confirm_new_name(DESCRIPTOR_DATA *d, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    switch (*argument)
    {
    case 'y': case 'Y':
        // Check if we're creating a character from an account
        if (d->account) {
            /* If the account has a colour preference set, skip the prompt
             * and apply it directly — the defaults system will handle it
             * in finalize_new_character anyway. */
            if (pref_get_source(d->account, NULL, "colour") != PREF_SOURCE_DEFAULT) {
                if (pref_get_bool(d->account, NULL, "colour", false))
                    SET_BIT(ch->act[0], PLR_COLOUR);
                login_get_ascii(d, "skip");
            } else {
                write_to_buffer(d, "\n\rWould you like ascii colour (Y/N)? ", 0);
                d->connected = CON_GET_ASCII;
            }
        } else {
            // Original behavior for direct character creation
            ProtocolNoEcho(d, true);
            sprintf(buf, "\n\rEnter a password for %s: ", ch->name);
            write_to_buffer(d, buf, 0);
            d->connected = CON_GET_NEW_PASSWORD;
        }
        break;

case 'n': case 'N':
    if (d->account) {
        free_char(d->character);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
    } else {
        write_to_buffer(d, "Name: ", 0);
        free_char(d->character);
        d->character = NULL;
        d->connected = CON_GET_NAME;
    }
    break;

default:
    if (!str_cmp(argument, "cancel")) {
        if (d->account) {
            free_char(d->character);
            d->character = NULL;
            display_account_menu(d);
            d->connected = CON_ACCOUNT_MENU;
        } else {
            write_to_buffer(d, "Name: ", 0);
            free_char(d->character);
            d->character = NULL;
            d->connected = CON_GET_NAME;
        }
        break;
    }
    write_to_buffer(d, "Please type yes, no, or cancel: ", 0);
    break;
    }
}

void login_get_ascii(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;

    /* "skip" means colour was already set by account prefs — skip the question */
    if (str_cmp(argument, "skip")) {
        switch (argument[0])
        {
        case 'y': case 'Y':
            SET_BIT(ch->act[0], PLR_COLOUR);
            break;
        case 'n': case 'N':
            break;
        default:
            write_to_buffer(d, "Yes/No.\n\rWould you like ascii colour? ", 0);
            return;
        }
    }

    send_to_char("\n\r{r-----{R======{D//// {WWelcome to the world of Sentience! {D\\\\{R======{r-----{x\n\r", ch);
    write_to_buffer(d, "\n\r", 2);

    // Set account details on character if coming from account creation flow
    if (d->connected == CON_CREATING_NEW_CHAR && d->account) {
        free_string(ch->pcdata->account_name);
        ch->pcdata->account_name = str_dup(d->account->username);
        ch->pcdata->account_id[0] = d->account->id[0];
        ch->pcdata->account_id[1] = d->account->id[1];
        
    }

    wiznet("Newbie alert!  $N sighted.", ch, NULL, WIZ_NEWBIE, 0, 0);

    /* Show available races (starting + account-unlocked) */
    send_to_char("\n\r{YThe following races are available to you:{x\n\r", ch);

    bool has_unlocked = false;
    for (RACE_DATA *r = race_list; r; r = r->next) {
        if (!race_available_for_creation(r, d->account))
            continue;

        char rbuf[MSL];
        const char *tag = "";
        if (!r->starting && d->account && account_has_race_unlock(d->account, r->id)) {
            tag = " {Y(unlocked){x";
            has_unlocked = true;
        }
        if (!IS_NULLSTR(r->summary)) {
            sprintf(rbuf, "{G%-12s{B - %s%s\n\r", capitalize(r->name), r->summary, tag);
        } else if (!IS_NULLSTR(r->description)) {
            /* Trim trailing newlines from description for one-line display */
            char desc_buf[MSL];
            strncpy(desc_buf, r->description, sizeof(desc_buf) - 1);
            desc_buf[sizeof(desc_buf) - 1] = '\0';
            char *p = desc_buf + strlen(desc_buf) - 1;
            while (p >= desc_buf && (*p == '\n' || *p == '\r'))
                *p-- = '\0';
            sprintf(rbuf, "{G%-12s{B - %s%s\n\r", capitalize(r->name), desc_buf, tag);
        } else {
            sprintf(rbuf, "{G%-12s{B - A playable race.%s\n\r", capitalize(r->name), tag);
        }
        send_to_char(rbuf, ch);
    }

    if (has_unlocked) {
        send_to_char("\n\r{DRaces marked (unlocked) were earned through gameplay.{x\n\r", ch);
    }

    send_to_char("\n\r{xYou will now be asked which race you would like your character to\n\r", ch);
    send_to_char("{xbelong to. Each race has its advantages and disadvantages. You can\n\r", ch);
    send_to_char("{xinspect each of the races by typing \"help <race>\". To get a summary\n\r", ch);
    send_to_char("{xof all the races, type \"help\".\n\r", ch);

    send_to_char("\n\r{YChoose your race (type \"help <race>\" for more information):{x ", ch);
    d->connected = CON_GET_NEW_RACE;
}

void login_get_alignment(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    switch (argument[0])
    {
    case 'g' : case 'G' : ch->alignment = 750;  break;
    case 'n' : case 'N' : ch->alignment = 0;	break;
    case 'e' : case 'E' : ch->alignment = -750; break;
    default:
        if (argument[0] != '\0')
            write_to_buffer(d,"That's not a valid alignment.\n\r",0);

        send_to_char("\n\r{YChoose an alignment ({GGood/Neutral/Evil{Y):{x ", ch);
        return;
    }

    /* Evil*/
    // Align checks out
    if (ch->alignment < 0)
    {
        send_to_char("\n\r{xYou have chosen to be {REvil{x.\n\r\n\r", ch);

        send_to_char("{YThe following races are available to you: \n\r", ch);
        send_to_char("{GDrow        {B - Dark elves who are masters of tact and dexterity.\n\r", ch);
        send_to_char("{GVampire     {B - The walking dead. Lots of extra skills but lots of vulnerabilities.\n\r", ch); //-- Disabled by Gairun 20111219
        send_to_char("{GSith        {B - Half man, half snake. Natural hunt, toxins, and a nasty tail.\n\r", ch);
        send_to_char("{GMinotaur    {B - The ultimate warrior. Very strong, tough and has a thick warm coat, but vulnerable to fire. \n\r", ch);
    }

    /* Good*/
    if (ch->alignment > 0)
    {
        send_to_char("\n\r{xYou have chosen to be {WGood{x.\n\r\n\r", ch);

        send_to_char("{YThe following races are available to you:\n\r", ch);
        send_to_char("{GDraconian   {B - Dragon/human cross. Can fly and breathe fire, frost, acid, gas, or lightning.\n\r", ch);
        send_to_char("{GSlayer      {B - Ancient holy fighters who can shapeshift into beasts. 25%% extra damage against evil.\n\r", ch); //-- Disabled by Gairun 20111219
        send_to_char("{GTitan       {B - Very strong, have an extra attack, vulnerable to lightning.\n\r", ch);
        send_to_char("{GElf         {B - Very high stats, resistant to magic and fast mana regen.\n\r", ch);
    }

    /* Neutral*/
    if (ch->alignment == 0)
    {
        send_to_char("\n\r{xYou have chosen to be {GNeutral{x.\n\r\n\r", ch);
        send_to_char("{GDwarf       {B - Hardy, great at combat, and masters of craftwork.\n\r", ch);
        send_to_char("{GHuman       {B - Average stats, no particular strengths or vunerabilities.\n\r", ch);
        send_to_char("{GLich        {B - Undead lords of magic - many magical powers but physically weak.\n\r", ch); //-- Disabled by Gairun 20111219
//		    send_to_char("{DPraxis      {D - Coming soon!\n\r",ch);
    }

    send_to_char("\n\r{xYou will now be asked which race you would like your character to\n\r", ch);
    send_to_char("{xbelong to. Each race has its advantages and disadvantages. You can\n\r", ch);
    send_to_char("{xinspect each of the races by typing \"help <race>\". To get a summary\n\r", ch);
    send_to_char("{xof all the races, type \"help\".\n\r", ch);

    send_to_char("\n\r{YChoose your race (type \"help <race>\" for more information):{x ", ch);
    d->connected = CON_GET_NEW_RACE;
}

static void apply_race_properties(CHAR_DATA *ch);

void login_get_new_race(DESCRIPTOR_DATA *d, char *argument)
{

    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char races[MSL];
    CHAR_DATA *ch;
    RACE_DATA *race;
    HELP_DATA *help;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    one_argument(argument,arg);

        sprintf(races, "\n\r{YChoose your race");
        add_possible_races(d->account, races);
        strcat(races, "{Y:{x ");

        if (!strcmp(arg,"help"))
        {
            argument = one_argument(argument,arg);
            if (argument[0] == '\0' || !str_prefix(argument, "races"))
            {
                send_to_char("{b++++++{B------{C++++++ {WRACES SUMMARY {C++++++{B------{b++++++{x\n\r\n\r", ch);
                if ((help = lookup_help_exact("races grid", 0, topHelpCat)) != NULL)
                    send_to_char(help->text, ch);
            }
            else
            {
                if ((race = race_lookup(argument)) != NULL &&
                    (help = lookup_help_exact(race->name, 0, topHelpCat)) != NULL)
                {
                    sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
                    send_to_char(buf, ch);
                    send_to_char(help->text, ch);
                }
                else
                    send_to_char("That's not a race.\n\r", ch);
            }

            send_to_char(races, ch);
            return;
        }

        if (arg[0] == '\0') {
            send_to_char(races, ch);
            return;
        }

        if ((race = race_lookup(argument)) == NULL) {
            send_to_char("There is no such race.\n\r", ch);
            send_to_char(races, ch);
            return;
        }

        if (!race->playable || !str_cmp(race->id, "shaper")) {
            send_to_char("That isn't a player race.\n\r", ch);
            send_to_char(races, ch);
            return;
        }

        if (!race_available_for_creation(race, d->account)) {
            send_to_char("That race is not available to you.\n\r", ch);
            send_to_char(races, ch);
            return;
        }

ch->race = race;

        /* For remort/path races: set original race */
        if (race_is_remort(race)) {
            if (race_is_path(race)) {
                /* Path race — need to prompt for original race */
                send_to_char("\n\r{YAs a path race, you must choose your original race.{x\n\r", ch);
                send_to_char("{YType 'list' to see available races, or enter a race name:{x ", ch);
                d->connected = CON_GET_ORIGIN_RACE;
                return;
            } else {
                /* Standard remort race — auto-set orace from prerequisite */
                RACE_DATA *prereq = race_get_prerequisite(race);
                if (prereq)
                    ch->orace = prereq;
            }
        }

        /* Apply race properties (stats, flags, skills, size, alignment) */
        apply_race_properties(ch);

        send_to_char("\n\r{YIs your body {wmasculine{Y, {Wfeminine{y, {Wneutral{Y, or {Wother{Y?{x ", ch);
        d->connected = CON_GET_NEW_BODY_TYPE;
        return;

}

/**
 * apply_race_properties - Apply race properties to a new character
 *
 * Sets alignment, stats, affects, immunities, resistances, vulnerabilities,
 * form, parts, size, and racial skills from the character's current race.
 * For characters with an orace (original race), applies overlay semantics.
 *
 * @param ch  Character to apply race properties to
 */
static void apply_race_properties(CHAR_DATA *ch)
{
    RACE_DATA *race = ch->race;
    int i;

    if (!race)
        return;

    /* Set alignment from race's default (full -1000..1000 range) */
    ch->alignment = URANGE(-1000, race->default_alignment, 1000);

    /* Initialize stats from race */
    for (i = 0; i < MAX_STATS; i++) {
        ch->perm_stat[i] = race->stats[i];
        ch->dirty_stat[i] = true;
    }

    /* Apply flags — overlay with orace if present */
    if (ch->orace) {
        ch->affected_by_perm[0] = ch->orace->aff[0] | race->aff[0];
        ch->affected_by_perm[1] = ch->orace->aff[1] | race->aff[1];
        ch->imm_flags_perm = ch->orace->imm | race->imm;
        ch->res_flags_perm  = race->res;
        ch->vuln_flags_perm = race->vuln;
    } else {
        ch->affected_by_perm[0] = race->aff[0];
        ch->affected_by_perm[1] = race->aff[1];
        ch->imm_flags_perm = race->imm;
        ch->res_flags_perm = race->res;
        ch->vuln_flags_perm = race->vuln;
    }

    ch->act[1]         = ch->act[1] | race->act[1];
    ch->affected_by[0] = ch->affected_by[0] | ch->affected_by_perm[0];
    ch->imm_flags  = ch->imm_flags | ch->imm_flags_perm;
    ch->res_flags  = ch->res_flags | ch->res_flags_perm;
    ch->vuln_flags = ch->vuln_flags | ch->vuln_flags_perm;
    ch->form  = race->form;
    ch->parts = race->parts;
    ch->size  = race->min_size;

    /* Add skills from race (and orace in overlay) */
    if (race->skills) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, race->skills);
        while ((skill_name = (char *)iterator_nextdata(&it)))
            group_add(ch, skill_name, false);
        iterator_stop(&it);
    }

    if (ch->orace && ch->orace->skills) {
        ITERATOR it;
        char *skill_name;
        iterator_start(&it, ch->orace->skills);
        while ((skill_name = (char *)iterator_nextdata(&it)))
            group_add(ch, skill_name, false);
        iterator_stop(&it);
    }
}

/**
 * login_get_origin_race - Handle origin race selection for path races
 *
 * During character creation, if a player chose a path race (lich, vampire,
 * slayer, etc.), they must select what their character's original race was
 * before the transformation. Only non-remort, playable races are valid.
 */
void login_get_origin_race(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    char arg[MAX_INPUT_LENGTH];
    RACE_DATA *orace;

    while (ISSPACE(*argument))
        argument++;

    one_argument(argument, arg);

    if (arg[0] == '\0' || !str_cmp(arg, "list")) {
        RACE_DATA *r;
        send_to_char("{YAvailable original races:{x\n\r", ch);
        for (r = race_list; r; r = r->next) {
            if (r->playable && !race_is_remort(r))
                printf_to_char(ch, "  %s\n\r", r->name);
        }
        send_to_char("\n\r{YChoose your original race:{x ", ch);
        return;
    }

    orace = race_lookup(arg);
    if (!orace || !orace->playable || race_is_remort(orace)) {
        send_to_char("That is not a valid original race. Type 'list' to see choices.\n\r", ch);
        return;
    }

    ch->orace = orace;
    printf_to_char(ch, "\n\r{YYour original race before becoming %s was %s.{x\n\r",
                   ch->race->name, orace->name);

    /* Now apply race properties with overlay semantics */
    apply_race_properties(ch);

    send_to_char("\n\r{YIs your body {wmasculine{Y, {Wfeminine{y, {Wneutral{Y, or {Wother{Y?{x ", ch);
    d->connected = CON_GET_NEW_BODY_TYPE;
}

static void finalize_new_character(DESCRIPTOR_DATA *d);

/* DEPRECATED: login_get_new_sex is no longer part of the normal character
 * creation flow. Body type selection (CON_GET_NEW_BODY_TYPE) now handles
 * sex/pronoun assignment. Retained for the dispatch table only. */
void login_get_new_sex(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    switch (argument[0])
    {
    case 'm': case 'M': ch->sex = ch->pcdata->true_sex = SEX_MALE; break;
    case 'f': case 'F': ch->sex = ch->pcdata->true_sex = SEX_FEMALE; break;
    case 'n': case 'N': ch->sex = ch->pcdata->true_sex = SEX_NEUTRAL; break;
    default:
        if (argument[0] != '\0')
            send_to_char("{xThat's not a sex.\n\r", ch);

        send_to_char("\n\r{YWhat is your sex (M/F/N)?{x ", ch);
        return;
    }

    /* Skip class selection — assign defaults and finalize */
    finalize_new_character(d);
}


void login_read_imotd(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;


    while (ISSPACE(*argument))
        argument++;

    ch = d->character;

    write_to_buffer(d,"\n\r",2);
    do_function(ch, &do_motd, "");
    d->connected = CON_READ_MOTD;
}

void login_read_motd(DESCRIPTOR_DATA *d, char *argument)
{
    DESCRIPTOR_DATA *d2;
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *ch;
    long playernum;
    extern char str_boot_time[MAX_INPUT_LENGTH];

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    
    // First, make sure we have a valid character
    if (!ch) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "login_read_motd: null character");
        close_socket(d);
        return;
    }
    
    // Look for existing characters with this name that might be linkdead
    CHAR_DATA *existing = NULL;
    ITERATOR it;
    
    iterator_start(&it, loaded_chars);
    while ((existing = (CHAR_DATA *)iterator_nextdata(&it))) {
        if (!IS_NPC(existing) && 
            existing != ch && 
            !str_cmp(ch->name, existing->name)) {
            // Found an already-loaded character with this name
            break;
        }
    }
    iterator_stop(&it);
    
    // If we found an existing instance of this character already in the game
    if (existing && existing != ch) {
        // Check if the existing character is truly "active" in the game
        // A character that is mid-extraction (after quit) will have in_room == NULL
        // Only reconnect to characters that are still in a room (true linkdeath scenario)
        if (existing->in_room == NULL) {
            // Existing character is stale (mid-extraction or already extracted but not yet
            // removed from loaded_chars). Use the freshly loaded character from disk/cache
            // which has the correct saved state.
            log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                "login_read_motd: Found stale character %s in loaded_chars (no room), using freshly loaded data",
                existing->name ? existing->name : "(unknown)");

            // Remove the stale character from loaded_chars and free it
            list_remlink(loaded_chars, existing, false);
            free_char(existing);
            existing = NULL;
            // Fall through to normal login with the freshly loaded 'ch'
        } else {
            // True linkdeath scenario - existing character is still in a room
            // Use the existing character to preserve any unsaved in-game state

            // LOG: Inventory counts before reconnect
            int existing_inv = existing->lcarrying ? list_size(existing->lcarrying) : 0;
            int temp_inv = ch->lcarrying ? list_size(ch->lcarrying) : 0;
            log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "RECONNECT: existing=%s has %d items, temp has %d items",
                       existing->name ? existing->name : "(unknown)", existing_inv, temp_inv);

            // If the existing character has a descriptor, disconnect them
            if (existing->desc) {
                write_to_buffer(existing->desc, "\n\rSomeone else is logging in with your character.\n\r", 0);
                close_socket(existing->desc);
                existing->desc = NULL;
            }

            // Save the room reference for later, but remove character from it FIRST
            ROOM_INDEX_DATA *old_room = existing->in_room;
            if (existing->in_room) {
                char_from_room(existing);
            }

            // Transfer the descriptor to the existing character
            existing->desc = d;
            d->character = existing;

            // Free the temporary character (freshly loaded one)
            free_char(ch);

            // LOG: Check inventory count after freeing temp character
            int existing_inv_after = existing->lcarrying ? list_size(existing->lcarrying) : 0;
            log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "RECONNECT: After free_char, existing=%s now has %d items (was %d)",
                       existing->name ? existing->name : "(unknown)", existing_inv_after, existing_inv);

            // Set as playing
            d->connected = CON_PLAYING;

            // Send reconnection message and place in room
            send_to_char("Reconnecting. Type replay to see missed tells.\n\r", existing);

            // Place character back in their room
            if (old_room) {
                char_to_room(existing, old_room);
                act("$n has reconnected.", existing, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }

            // Log the reconnection
            log_message_f(LOG_LEVEL_INFO, LOG_INFO, "%s@%s reconnected.", existing->name, d->host);
            wiznet("$N has relinked.", existing, NULL, WIZ_LINKS, 0, 0);

            // Update connection tracking
            connection_add(d);

            // Update protocol
            MXPSendTag(d, "<VERSION>");

            return;
        }
    }

    // This is a normal login, not a reconnect
    // Continue with regular login process
    
    // Password cleanup for normal login
    if (ch->pcdata) {
        if (ch->pcdata->old_pwd != NULL) {
            free_string(ch->pcdata->old_pwd);
            ch->pcdata->old_pwd = NULL;
        }

        if (ch->pcdata->reset_code != NULL) {
            free_string(ch->pcdata->reset_code);
            ch->pcdata->reset_code = NULL;
        }

        if (ch->pcdata->reset_time != 0) {
            ch->pcdata->reset_time = 0;
        }

        if (ch->pcdata->reset_state != 0) {
            ch->pcdata->reset_state = 0;
        }
    }
    
    // Add to loaded characters list
    if (!list_haslink(loaded_chars, ch))
        list_appendlink(loaded_chars, ch);
    
    // Migrate legacy penalty flags to account penalty system
    if (d->account) {
        migrate_legacy_penalties(d->account, ch);
    }

    d->connected = CON_PLAYING;
    
    // Reset character stats
    reset_char(ch);

    /* Rebuild skill source metadata from class rewards.
     * This ensures dimming/availability works correctly for characters
     * loaded from JSON files that predate the multi-source system,
     * and picks up any cross-class reward changes made offline. */
    rebuild_skill_sources(ch);

    /* Apply account preferences on login.
     * This ensures account-level settings (and updated game defaults)
     * take effect each login, unless the character has an explicit
     * override saved in their own preferences list. */
    if (d->account) {
        pref_apply_to_character(d->account, ch,
                                ch->pcdata ? ch->pcdata->preferences : NULL);
    }

    // Show server stats
    playernum = 0;
    for (d2 = descriptor_list; d2 != NULL; d2 = d2->next) {
        if (d2->connected == CON_PLAYING && d2 != d &&
            can_see(d->character, d2->character))
            playernum++;
    }

    sprintf(buf, "{MThe current system time is {x%s{x\r", ctime(&current_time));
    send_to_char(buf, ch);

    sprintf(buf, "{MLast reboot was at {x%s{x\r", str_boot_time);
    send_to_char(buf, ch);

    sprintf(buf, "{MThere are currently {W%ld{M players online.{x\n\r", playernum);
    send_to_char(buf, ch);

    bool moved_to_room = false;

    // Handle new character setup
    if (ch->tot_level == 0) {
        ch->exp = 0;
        ch->hit = ch->max_hit;
        ch->mana = ch->max_mana;
        ch->move = ch->max_move;
        ch->train = 3;
        ch->practice = 5;
        set_title(ch, "");
        advance_level(ch, true);  /* silent level 1 stat gains */
        ch->hit = ch->max_hit;
        ch->mana = ch->max_mana;
        ch->move = ch->max_move;

            moved_to_room = true;
            // Safely place in starting room
            if (get_reserved_room_index("room_begin_new_character")) {
                char_to_room(ch, get_reserved_room_index("room_begin_new_character"));
                do_function(ch, &do_changes, "catchup");
                send_to_char("\n\r", ch);
                
                // Announce new player
                for (d2 = descriptor_list; d2 != NULL; d2 = d2->next) {
                    if (d2->connected == CON_PLAYING && d2->character != ch &&
                        !IS_SET(d2->character->comm, COMM_NOANNOUNCE)) {
                        act("{MThe Town Crier Announces 'All welcome $N, a new adventurer to Sentience!'{x",
                            d2->character, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    }
                }
                ch->tot_level = 1;
            
            }
    }

    // Fix variable and dungeon references
    variable_dynamic_fix_mobile(ch);
    resolve_dungeons_player(ch);
    resolve_instances_player(ch);
    resolve_ships_player(ch);

    // Handle room placement only if not already placed
    if (!moved_to_room) {
        if (ch->in_room != NULL) {
            char_to_room(ch, ch->in_room);
        } else if (ch->in_wilds != NULL) {
            if (check_for_bad_room(ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y)) {
                plogf(LOG_INFO, "Transferring %s to VRoom", ch->name);
                char_to_vroom(ch, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
            } else {
                ROOM_INDEX_DATA *default_recall = get_reserved_room_index("room_default_recall");
                perrf(LOG_INFO, "Previous VRoom invalid. Relocating %s to default room (%ld - %s)", ch->name,
                    default_recall ? default_recall->vnum : 0,
                    default_recall ? default_recall->name : "Unknown");
                ch->in_wilds = NULL;
                ch->at_wilds_x = -1;
                ch->at_wilds_y = -1;
                if (default_recall)
                    char_to_room(ch, default_recall);
            }
        } else {
            if (IS_IMMORTAL(ch)) {
                if (get_reserved_room_index("room_chat_lobby"))
                    char_to_room(ch, get_reserved_room_index("room_chat_lobby"));
            } else {
                if (get_reserved_room_index("room_default_recall"))
                    char_to_room(ch, get_reserved_room_index("room_default_recall"));
            }
        }
    }

    // Announce entry to game
    act("$$n has entered the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    MXPSendTag(d, "<VERSION>");
    
    // Normal login notifications to other players
    for (d2 = descriptor_list; d2 != NULL; d2 = d2->next) {
        if (d2->connected == CON_PLAYING && !IS_IMMORTAL(d->character) &&
            d2->character != ch && IS_SET(d2->character->comm, COMM_NOTIFY))
            act("{B$$N has entered the game.{x", d2->character, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }

    // Handle church-related logic
    if (ch->church != NULL) {
        if ((ch->alignment < 0 && ch->church->alignment == CHURCH_GOOD) ||
            (ch->alignment > 0 && ch->church->alignment == CHURCH_EVIL)) {
            act("{YAs you enter Sentience, you feel your church's faith has been changed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("{YYou feel your psychic link to $T being severed.{x", ch, NULL, NULL, NULL, NULL, NULL, ch->church->name, TO_CHAR, NULL, NULL);
            remove_member(ch->church_member);
            ch->church = NULL;
        } else {
            // Send a message to the church
            sprintf(buf, "{Y[%s has entered the game.]{x\n\r", ch->name);
            church_echo(ch->church, buf);
            list_addlink(ch->church->online_players, ch);
        }
    }

    // Account linkage for new characters
    if (d->account) {
        if (ch->tot_level == 0 || IS_NULLSTR(ch->pcdata->account_name)) {
            free_string(ch->pcdata->account_name);
            ch->pcdata->account_name = str_dup(d->account->username);
            ch->pcdata->account_id[0] = d->account->id[0];
            ch->pcdata->account_id[1] = d->account->id[1];
            
            // Update account's character list
            account_add_character(d->account, ch);
            save_account(d->account);
        }
    }
    
    // Add connection to tracking list
    connection_add(d);
    
    // Final login processing
    wiznet("$N has entered the game.", d->character, NULL, WIZ_LOGINS, 0, 0);
    notify_staff_of_notes(d);
    do_function(ch, &do_look, "auto");
    do_function(ch, &do_unread, "");
    
    // Push stats to leaderboards on login
    leaderboard_on_login(ch);

    // LOGIN TRIGGER
    script_login(ch);
}

// Update the select_character function
void select_character(DESCRIPTOR_DATA *d, ACCOUNT_CHARACTER *ch_entry)
{
    char buf[MAX_STRING_LENGTH];
    bool found;

    // Load the character
    found = load_char_obj(d, ch_entry->name);

    if (!found) {
        sprintf(buf, "Character '%s' could not be loaded.\n\r", ch_entry->name);
        write_to_buffer(d, buf, 0);
        d->connected = CON_ACCOUNT_MENU;
        display_account_menu(d);
        return;
    }

    // NOTE: Character is NOT added to loaded_chars here with lazy loading
    // It will be added in proceed_to_game() AFTER remaining data (tokens, inventory, etc.) is loaded
    // This prevents mobile_update() from processing partially-loaded characters with uninitialized tokens

    // Show character menu
    display_character_menu(d);
    d->connected = CON_CHARACTER_MENU;
}


void login_creating_new_char(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;
    char buf[MAX_STRING_LENGTH];
    
    argument[0] = UPPER(argument[0]);
    if (!check_parse_name(argument)) {
        write_to_buffer(d, "Illegal character name, try another.\n\r", 0);
        return;
    }
    
    // If we previously attempted to create a character, free it first
    if (d->character != NULL) {
        free_char(d->character);
        d->character = NULL;
    }
    
    // Create a new character 
    d->character = new_char();
    ch = d->character;
    ch->desc = d;
    
    // Ensure pcdata is initialized
    if (ch->pcdata == NULL) {
        ch->pcdata = new_pcdata();
    }
    
    // Set up the character with the name
    free_string(ch->name);
    ch->name = str_dup(argument);
    
    // Check if a character with this name already exists on disk
    if (player_exists(argument)) {
        write_to_buffer(d, "That character already exists. Please choose another name.\n\r", 0);
        free_char(d->character);
        d->character = NULL;
        return;
    }
    
    sprintf(buf, "\n\rDo you want to create a character named %s (Y/N)? ", ch->name);
    write_to_buffer(d, buf, 0);
    d->connected = CON_CONFIRM_NEW_NAME;
}


void login_link_character_name(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;
    ACCOUNT_DATA *acct = d->account;
    bool found = false;
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    
    if (!argument[0]) {
        write_to_buffer(d, "Linking canceled.\n\r", 0);
        d->connected = CON_ACCOUNT_MENU;
        display_account_menu(d);
        return;
    }
    
    // Check if the character is already linked to this account
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ch_entry->name, argument)) {
            write_to_buffer(d, "That character is already linked to your account.\n\r", 0);
            iterator_stop(&it);
            d->connected = CON_ACCOUNT_MENU;
            display_account_menu(d);
            return;
        }
    }
    iterator_stop(&it);
    
    // Check if the character exists
    DESCRIPTOR_DATA temp_d;
    memset(&temp_d, 0, sizeof(temp_d));
    
    found = load_char_obj(&temp_d, argument);
    ch = temp_d.character;
    
    if (!found) {
        write_to_buffer(d, "Character not found.\n\r", 0);
        d->connected = CON_ACCOUNT_MENU;
        display_account_menu(d);
        return;
    }
    
    // Check if character is already linked to an account
    if (!IS_NULLSTR(ch->pcdata->account_name)) {
        // If already linked to THIS account but missing from the character list,
        // automatically add it to the list without requiring password
        if (!str_cmp(ch->pcdata->account_name, acct->username) ||
            (ch->pcdata->account_id[0] == acct->id[0] && 
             ch->pcdata->account_id[1] == acct->id[1])) {
            
            char buf[MAX_STRING_LENGTH];
            sprintf(buf, "Character '%s' was already linked to your account. Fixing character list.\n\r", ch->name);
            write_to_buffer(d, buf, 0);
            
            // Add to account's character list
            account_add_character(acct, ch);
            save_account(acct);
            
            free_char(ch);
            d->connected = CON_ACCOUNT_MENU;
            display_account_menu(d);
            return;
        }
        // Linked to a different account - deny
        else {
            write_to_buffer(d, "That character is already linked to a different account.\n\r", 0);
            free_char(ch);
            d->connected = CON_ACCOUNT_MENU;
            display_account_menu(d);
            return;
        }
    }

    // If the character is not already linked, but its email matches the verified email on the account, go ahead and link them.
    if (!IS_NULLSTR(ch->pcdata->email)){
        if (!str_cmp(ch->pcdata->email, acct->email)) {
            account_add_character(acct, ch);
            save_account(acct);
            free_char(ch);
            d->connected = CON_ACCOUNT_MENU;
            display_account_menu(d);
            return;
        }
    }
    
    // Store character name temporarily for the linking process
    d->character = ch;
    
    ProtocolNoEcho(d, true);
    d->connected = CON_LINK_CHARACTER_PASSWORD;
}

void login_link_character_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    bool password_ok = false;
    
    // Safety check - if character is NULL or pcdata is NULL, return to account menu
    if (ch == NULL || ch->pcdata == NULL) {
        write_to_buffer(d, "Error with character data. Returning to account menu.\n\r", 0);
        if (d->character) free_char(d->character); // Clean up if ch exists
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    write_to_buffer(d, "\n\r", 2);

    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "Character link cancelled.\n\r", 0);
        ProtocolNoEcho(d, false);
        if (d->character) free_char(d->character);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    // Use auth API for character password verification (from PC_DATA for unlinked chars)
    AUTH_DATA *auth = get_auth_data(ch, d->account);
    pwd_result_t pwd_result = verify_password(argument, auth);
    free_auth_data(auth);

    if (pwd_result != PWD_INVALID) {
        password_ok = true;
    }
    
    if (!password_ok) {
        write_to_buffer(d, "Incorrect password.\n\r", 0);
        ProtocolNoEcho(d, false);
        free_char(ch); // ch was allocated by load_char_obj in login_link_character_name
        d->character = NULL;
        d->connected = CON_ACCOUNT_MENU;
        display_account_menu(d);
        return;
    }
    
    // Password is OK
    // Check if character has MFA enabled
    if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled && !DEV_SKIP_MFA) {
        write_to_buffer(d, "This character has MFA enabled. Please enter the MFA code: ", 0);
        // ProtocolNoEcho is already true from the password prompt
        d->connected = CON_LINK_CHARACTER_MFA;
        return;
    }
    
    // No MFA or MFA skipped, link the character to the account
    complete_character_link(d);
}

void login_link_character_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    // Safety check - if character is NULL, return to account menu
    if (ch == NULL) {
        write_to_buffer(d, "Error with character data. Returning to account menu.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    if (!check_mfa(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        ProtocolNoEcho(d, false);
        
        // Free character and set to NULL before returning to menu
        free_char(ch);
        d->character = NULL;
        
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Link the character to the account
    complete_character_link(d);
}

void complete_character_link(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];
    
    ProtocolNoEcho(d, false);
    
    // Update character's account reference
    free_string(ch->pcdata->account_name);
    ch->pcdata->account_name = str_dup(acct->username);
    ch->pcdata->account_id[0] = acct->id[0];
    ch->pcdata->account_id[1] = acct->id[1];
    
    // Add character to account
    account_add_character(acct, ch);
    
    // Create a clone of the descriptor to use for saving
    DESCRIPTOR_DATA temp_d;
    memcpy(&temp_d, d, sizeof(DESCRIPTOR_DATA));
    ch->desc = &temp_d;  // Attach the temporary descriptor for saving
    
    save_char_obj(ch);   // Now save with the temporary descriptor
    save_account(acct);
    
    ch->desc = d;        // Restore original descriptor reference
    
    sprintf(buf, "Character '%s' successfully linked to your account.\n\r", ch->name);
    write_to_buffer(d, buf, 0);
    
    // Clean up
    free_char(ch);
    d->character = NULL;
    
    // Return to account menu
    d->connected = CON_ACCOUNT_MENU;
    display_account_menu(d);
}

void display_character_menu(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    char buf[MAX_STRING_LENGTH];
    char label[50], value[100];
    d->mfa_verified = false;
    
    // Get account character data once at the beginning
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, d->account, &acct_char);
    
    // Verify we have what we need
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character data in your account.{x\n\r", 0);
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "display_character_menu: Missing account character data");
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    write_to_buffer(d, "\n\r{B=={W[ {YCHARACTER MENU {W]{B=={x\n\r\n\r", 0);
    
    // Format character information using pad_string for consistent alignment
    sprintf(label, "{CCharacter:{x");
    if (!IS_NULLSTR(ch->pcdata->title))
    {
        sprintf(value, "{W{+%s %s{x", ch->name, ch->pcdata->title);
    }
    else
    sprintf(value, "{W{+%s{x", ch->name);
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);

    // Display deletion status if character is flagged for deletion
    if (ch->deleted && ch->delete_time > 0) {
        char del_buf[64];
        time_t del_time = ch->delete_time + (game_settings.character_delete_delay_days * 86400);
        strftime(del_buf, sizeof(del_buf), "%Y-%m-%d %H:%M", localtime(&del_time));
        sprintf(buf, "{R[PENDING DELETION: will be deleted on %s]{x\n\r", del_buf);
        write_to_buffer(d, buf, 0);
    }
    
    // Display level information
    sprintf(label, "{CLevel:{x");
    sprintf(value, "{G%d (%d){x", 
            ch->level > 0 ? ch->level : ch->tot_level,
            ch->tot_level);
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);
    
    // Display immortal/mortal-specific information
    if (IS_IMMORTAL(ch)) {
        // Immortal rank
        sprintf(label, "{CImmortal Rank:{x");
        sprintf(value, "{R{+%s{x", flag_string(staff_ranks, get_staff_rank(ch)));
        sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
        write_to_buffer(d, buf, 0);

        // Display duties if assigned
        if (ch->pcdata->immortal && ch->pcdata->immortal->duties) {
            sprintf(label, "{CDuties:{x");
            sprintf(value, "{W%s{x", flag_string(immortal_flags, ch->pcdata->immortal->duties));
            sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
            write_to_buffer(d, buf, 0);
        }
    } else {
        // Mortal's Race and Class
        sprintf(label, "{CRace:{x");
        sprintf(value, "{G%s{x", ch->race ? ch->race->name : "Unknown");
        sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
        write_to_buffer(d, buf, 0);

        sprintf(label, "{CClass:{x");
        sprintf(value, "{G%s{x", 
            (ch->pcdata && ch->pcdata->sub_class_current) ? 
            sub_class_table[ch->pcdata->sub_class_current].name[ch->sex] : "Adventurer");
        sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
        write_to_buffer(d, buf, 0);
    }

    // Location information
    if (!IS_NULLSTR(ch->pcdata->last_area)) {
        sprintf(label, "{CLocation:{x");
        
        // Format the location string using our helper function
        const char *loc_str = format_location_string(ch->in_room);
        sprintf(value, "{Y%s{x", loc_str);
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }
    
    // Creation date
    sprintf(label, "{CCreated:{x");
    sprintf(value, "{G%s{x", ch->pcdata->creation_date ? ctime(&ch->pcdata->creation_date) : "Unknown");
    // Remove newline from ctime result
    if (value[strlen(value)-2] == '\r' || value[strlen(value)-1] == '\n')
        value[strlen(value)-2] = '\0';
    
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);
    
    // Display email information from account_character data
    if (game_settings.enable_email) {
        sprintf(label, "{CEmail:{x");
        
        if (game_settings.require_email_verif && !acct_char->email_verified && !IS_NULLSTR(acct_char->pending_email)) {
            sprintf(value, "{C%s{x (pending: {Y%s{x)", 
                    IS_NULLSTR(acct_char->email) ? "Not set" : acct_char->email, 
                    acct_char->pending_email);
        } else if (game_settings.require_email_verif && !acct_char->email_verified && IS_NULLSTR(acct_char->pending_email)) {
            sprintf(value, "{YPending verification{x");
        } else if (!IS_NULLSTR(acct_char->email)) {
            sprintf(value, "{C%s{x", acct_char->email);
        } else {
            sprintf(value, "{RNot set{x");
        }
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }
    
    // Last login time
    if (ch->pcdata->last_login > 0) {
        sprintf(label, "{CLast login:{x");
        sprintf(value, "{G%s{x", ctime(&ch->pcdata->last_login));
        // Remove newline from ctime result
        if (value[strlen(value)-2] == '\r' || value[strlen(value)-1] == '\n')
            value[strlen(value)-2] = '\0';
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }

    // Character password status from account_character data
    if (!DEV_SKIP_PASSWORD) {
        sprintf(label, "{CCharacter password:{x");
        sprintf(value, !IS_NULLSTR(acct_char->pwd) ? "{GSet{x" : "{RNot set{x");
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }

    // MFA status from account_character data
    if (!DEV_SKIP_MFA) {
        bool mfa_key_set = !IS_NULLSTR(acct_char->mfa_key);
        bool mfa_pending = !IS_NULLSTR(acct_char->mfa_pending_key);
        
        sprintf(label, "{CMFA Status:{x");
        if (mfa_key_set) {
            sprintf(value, "{GEnabled{x");
        } else if (mfa_pending) {
            sprintf(value, "{YSetup in Progress{x");
        } else {
            sprintf(value, "{RNot Set{X");
        }
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }

    // Display character statistics for non-immortals
    if (!IS_IMMORTAL(ch)) {
        write_to_buffer(d, "\n\r{CCharacter Statistics:{x\n\r", 0);
        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);
        write_to_buffer(d, "{D|{x {CGold{x                {D|{x {CSilver{x        {D|{x {CBank Balance{x        {D|{x {CDeity Points{x  {D|{x\n\r", 0);
        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);

        sprintf(buf, "{D|{x %-20ld{D|{x %-14ld{D|{x %-20ld{D|{x %-14ld{D|{x\n\r",
            ch->gold,
            ch->silver,
            ch->pcdata ? ch->pcdata->bankbalance : 0,
            ch->pcdata ? ch->deitypoints : 0);
        write_to_buffer(d, buf, 0);

        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);
        write_to_buffer(d, "{D|{x {CPneuma{x              {D|{x {CQuest Points{x  {D|{x {CQuests Completed{x    {D|{x {CDeath Count{x   {D|{x\n\r", 0);
        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);

        sprintf(buf, "{D|{x %-20ld{D|{x %-14d{D|{x %-20ld{D|{x %-14d{D|{x\n\r",
            ch->pcdata ? ch->pneuma : 0,
            ch->pcdata ? ch->questpoints : 0,
            ch->pcdata ? ch->pcdata->quests_completed : 0,
            ch->pcdata ? ch->deaths : 0);
        write_to_buffer(d, buf, 0);

        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);
        write_to_buffer(d, "{D|{x {CArena Wins{x          {D|{x {CPK Kills{x      {D|{x {CCPK Kills{x           {D|{x {CMonster Kills{x {D|{x\n\r", 0);
        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);

        sprintf(buf, "{D|{x %-20d{D|{x %-14d{D|{x %-20d{D|{x %-14ld{D|{x\n\r",
            ch->pcdata ? ch->arena_kills : 0,
            ch->pcdata ? ch->player_kills : 0,
            ch->pcdata ? ch->cpk_kills : 0,
            ch->pcdata ? ch->monster_kills : 0);
        write_to_buffer(d, buf, 0);

        write_to_buffer(d, "{D+---------------------+---------------+---------------------+---------------+{x\n\r", 0);
    }

    // Display character-specific penalties
    if (d->account && d->account->penalties) {
        bool header_shown = false;
        PENALTY_DATA *pen;
        for (pen = d->account->penalties; pen; pen = pen->next) {
            if (is_penalty_expired(pen))
                continue;
            // Show account-scope penalties and character-scope matching this char
            if (pen->scope == PENALTY_SCOPE_CHARACTER
                && (IS_NULLSTR(pen->target_name) || str_cmp(pen->target_name, ch->name)))
                continue;
            if (!header_shown) {
                write_to_buffer(d, "\n\r{RActive Penalties:{x\n\r", 0);
                header_shown = true;
            }
            char dur[64];
            if (pen->expires_at == 0)
                sprintf(dur, "{RPermanent{x");
            else
                penalty_format_duration(pen->expires_at - current_time, dur, sizeof(dur));
            sprintf(buf, "   {R*{x %-14s (%s) - %s\n\r",
                penalty_type_name(pen->type),
                pen->scope == PENALTY_SCOPE_ACCOUNT ? "{Caccount{x" : "{Ycharacter{x",
                dur);
            write_to_buffer(d, buf, 0);
        }
    }

    // Display character-specific bonuses
    if (d->account && d->account->bonuses) {
        bool header_shown = false;
        BONUS_DATA *bon;
        for (bon = d->account->bonuses; bon; bon = bon->next) {
            if (is_bonus_expired(bon))
                continue;
            // Show account-scope, unassigned, and character-scope matching this char
            if (bon->scope == BONUS_SCOPE_CHARACTER
                && (IS_NULLSTR(bon->target_name) || str_cmp(bon->target_name, ch->name)))
                continue;
            if (!header_shown) {
                write_to_buffer(d, "\n\r{GActive Bonuses:{x\n\r", 0);
                header_shown = true;
            }
            char dur[64];
            if (bon->expires_at == 0)
                sprintf(dur, "{GPermanent{x");
            else
                penalty_format_duration(bon->expires_at - current_time, dur, sizeof(dur));
            char scope_str[64];
            if (bon->scope == BONUS_SCOPE_UNASSIGNED)
                sprintf(scope_str, "{Yunassigned{x");
            else if (bon->scope == BONUS_SCOPE_CHARACTER)
                sprintf(scope_str, "{Ycharacter{x");
            else
                sprintf(scope_str, "{Caccount{x");
            sprintf(buf, "   {G+{x %-10s %+d%% (%s) - %s\n\r",
                bonus_type_name(bon->type), bon->value, scope_str, dur);
            write_to_buffer(d, buf, 0);
        }
    }

    /* Staff: show character note count */
    if (d->character && IS_IMMORTAL(d->character) && acct_char->staff_notes) {
        int cnotes = account_note_count(acct_char->staff_notes);
        int warnings = account_note_count_by_category(acct_char->staff_notes, NOTE_CAT_WARNING);
        int punishments = account_note_count_by_category(acct_char->staff_notes, NOTE_CAT_PUNISHMENT);
        sprintf(buf, "\n\r{YStaff Notes:{x %d total", cnotes);
        if (warnings > 0) {
            char tmp[32];
            sprintf(tmp, ", {Y%d warning%s{x", warnings, warnings == 1 ? "" : "s");
            strcat(buf, tmp);
        }
        if (punishments > 0) {
            char tmp[32];
            sprintf(tmp, ", {R%d punishment%s{x", punishments, punishments == 1 ? "" : "s");
            strcat(buf, tmp);
        }
        strcat(buf, "\n\r");
        write_to_buffer(d, buf, 0);
    }

    // Menu options divider
    write_to_buffer(d, "\n\r", 0);
    sprintf(buf, "{C%s{x\n\r", pad_string("", 60, NULL, "="));
    write_to_buffer(d, buf, 0);
    
    // Standard menu options
    write_to_buffer(d, "{GL{x) Log in with this character\n\r", 0);
    
    // Password option
    if (!DEV_SKIP_PASSWORD) {
        if (!IS_NULLSTR(acct_char->pwd)) {
            write_to_buffer(d, "{GP{x) Change character password\n\r", 0);
        } else {
            write_to_buffer(d, "{GP{x) Set character password\n\r", 0);
        }
    }
    
    // MFA option
    if (!DEV_SKIP_MFA)
        write_to_buffer(d, "{GM{x) MFA settings\n\r", 0);
    
    // Email options based on account_character data
    if (game_settings.enable_email) {
        if (IS_NULLSTR(acct_char->email) && IS_NULLSTR(acct_char->pending_email))
            write_to_buffer(d, "{GE{x) Set character email address\n\r", 0);
        else
            write_to_buffer(d, "{GE{x) Change character email address\n\r", 0);
            
        if (game_settings.require_email_verif && !acct_char->email_verified && !IS_NULLSTR(acct_char->pending_email)) {
            write_to_buffer(d, "{GV{x) Verify character email address\n\r", 0);
            write_to_buffer(d, "{GR{x) Resend verification email\n\r", 0);
        }
    }
    
    // Other menu options
    if (can_unlink_characters(d->account) && !IS_IMMORTAL(ch) && !ch->deleted)
        write_to_buffer(d, "{GU{x) Unlink this character\n\r", 0);
    
    if (ch->deleted && IS_SET(d->account->acct_flags, ACCT_CAN_DELETE_IMMEDIATELY))
        write_to_buffer(d, "{GD{x) Delete this character immediately\n\r", 0);
    else if (!ch->deleted)
        write_to_buffer(d, "{GD{x) Delete this character\n\r", 0);
        
    if (ch->deleted)
        write_to_buffer(d, "{GC{x) Cancel deletion\n\r", 0);

    write_to_buffer(d, "{GY{x) Set as default character\n\r", 0);

    if (!ch->deleted)
        write_to_buffer(d, "{GT{x) Reset settings to account defaults\n\r", 0);

    write_to_buffer(d, "{GB{x) Back to account menu\n\r\n\r", 0);
}



void login_character_password(DESCRIPTOR_DATA *d, char *argument)
{
    switch (toupper(argument[0])) {
        case 'Y':
            ProtocolNoEcho(d, true);
            d->connected = CON_CONFIRM_CHARACTER_PASSWORD;
            break;
            
        case 'N':
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
            break;
            
        default:
            write_to_buffer(d, "Please answer Yes or No: ", 0);
            break;
    }
}

void login_confirm_character_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    write_to_buffer(d, "\n\r", 2);
    
    if (!d->new_password_buffer) {
        write_to_buffer(d, "An error occurred. Please try setting your password again.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (strcmp(argument, d->new_password_buffer) != 0) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // NEW CHECK: Validate staff password uniqueness for character passwords
    if (IS_IMMORTAL(ch) && game_settings.require_uniq_pass_staff && 
        !validate_password_uniqueness(acct, d->new_password_buffer, true, ch->name, true)) {
        write_to_buffer(d, "Staff character passwords must be unique.\n\r", 0);
        write_to_buffer(d, "This password matches either your account password or another character's password.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // Also check for non-staff characters matching staff passwords
    if (!IS_IMMORTAL(ch) && game_settings.require_uniq_pass_staff && 
        !validate_password_uniqueness(acct, d->new_password_buffer, true, ch->name, false)) {
        write_to_buffer(d, "This password matches one of your staff character passwords.\n\r", 0);
        write_to_buffer(d, "Please choose a different password.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Set password on account_character if available
    if (has_auth_data) {
        if (!set_encrypted_password(&acct_char->pwd, &acct_char->pwd_vers, d->new_password_buffer)) {
            write_to_buffer(d, "Error setting character password. Please try again or contact staff.\n\r", 0);
        } else {
            save_account(acct);
            write_to_buffer(d, "\n\rCharacter password set.\n\r", 0);
        }
    }
    
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;
    
    ProtocolNoEcho(d, false);
    display_character_menu(d);
    d->connected = CON_CHARACTER_MENU;
}

void login_character_mfa_disable_confirm(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    switch (toupper(argument[0])) {
        case 'Y':
            // Clear MFA keys
            if (acct_char->mfa_key != NULL)
                free_string(acct_char->mfa_key);
            acct_char->mfa_key = str_dup("");

            if (acct_char->mfa_pending_key != NULL)
                free_string(acct_char->mfa_pending_key);
            acct_char->mfa_pending_key = str_dup("");

            acct_char->mfa_enabled = false;

            // Clear recovery codes
            for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                if (acct_char->recovery_codes[i] != NULL)
                    free_string(acct_char->recovery_codes[i]);
                acct_char->recovery_codes[i] = str_dup("");
                acct_char->recovery_used[i] = false;
            }
            
            save_account(acct);
            write_to_buffer(d, "MFA disabled and all MFA data cleared for this character.\n\r", 0);
            display_character_mfa_menu(d, "");
            break;
            
        case 'N':
            display_character_mfa_menu(d, "");
            break;
            
        default:
            write_to_buffer(d, "Please answer Y or N: ", 0);
            break;
    }
}

void setup_character_mfa(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    
    if (!ch || IS_NPC(ch))
        return;
    
    // Don't pass email presence as a requirement - MFA should work without email
    if (!setup_mfa_for_char(ch, false)) {
        write_to_buffer(d, "Error setting up MFA. Please try again.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Have user verify the MFA
    d->connected = CON_CHARACTER_MFA_VERIFY;
}

void login_confirm_delete_character(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];

    if (strcmp(argument, "DELETE")) {
        write_to_buffer(d, "Character deletion canceled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // Soft delete: set flag and timer on both char and account character
    ch->deleted = true;
    ch->delete_time = current_time;
    // Update account character entry
    ACCOUNT_CHARACTER *ach;
    ITERATOR it;
    iterator_start(&it, acct->characters);
    while ((ach = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ach->name, ch->name)) {
            ach->deleted = true;
            ach->delete_time = ch->delete_time;
            break;
        }
    }
    iterator_stop(&it);
    save_char_obj(ch);
    save_account(acct);

    sprintf(buf, "Character '%s' is now flagged for deletion. You may cancel this within %d days.\n\r",
            ch->name, game_settings.character_delete_delay_days);
    write_to_buffer(d, buf, 0);

    free_char(ch);
    d->character = NULL;
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

// When selecting a character from the account menu, check for additional character security
void login_character_menu(DESCRIPTOR_DATA *d, char *argument)
{
    /* Dispatches to handler functions in nanny_menus.c */
    switch (toupper(argument[0])) {
        case 'L': handle_character_login(d);              break;
        case 'P': handle_character_change_password(d);    break;
        case 'X': handle_character_clear_password(d);     break;
        case 'M': handle_character_mfa_menu(d);           break;
        case 'E': handle_character_change_email(d);       break;
        case 'V': handle_character_verify_email(d);       break;
        case 'R': handle_character_resend_verification(d); break;
        case 'U': handle_character_unlink(d);             break;
        case 'D': handle_character_delete(d);             break;
        case 'C': handle_character_cancel_delete(d);      break;
        case 'Y': handle_character_set_default(d);        break;
        case 'T': handle_character_reset_prefs(d);        break;
        case 'B': handle_character_back_to_account(d);    break;
        default:
            write_to_buffer(d, "Invalid choice.\n\r", 0);
            display_character_menu(d);
            break;
    }
}

/**
 * login_confirm_reset_prefs - Handle Y/N confirmation for setting reset
 *
 * If the user confirms, clears all character overrides, resets to game
 * defaults, and applies account preferences. The character is then saved.
 *
 * @param d         The descriptor
 * @param argument  User input (Y or N)
 */
void login_confirm_reset_prefs(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    switch (toupper(argument[0])) {
    case 'Y':
        pref_reset_to_defaults(acct, ch);
        save_char_obj(ch);
        write_to_buffer(d,
            "\n\r{GSettings have been reset to defaults and account preferences applied.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        break;

    case 'N':
        write_to_buffer(d, "\n\rReset cancelled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        break;

    default:
        write_to_buffer(d, "Please enter Y or N: ", 0);
        break;
    }
}

void login_verify_unlink_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    password_check_status status;
    
    // Get auth data from account_character - this should always succeed for linked characters
    if (!get_character_auth_data(ch, acct, &acct_char)) {
        // If we're trying to unlink, but can't find the account_character data, something's wrong
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "login_verify_unlink_password: Called on character with no account data");
        ProtocolNoEcho(d, false);
        write_to_buffer(d, "Error verifying character. Please contact staff.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rUnlink cancelled.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verify the password using account_character data
    status = check_encrypted_password(argument, acct_char->pwd, acct_char->pwd_vers);

    if (status == PWD_CHECK_FAIL) {
        ProtocolNoEcho(d, false);
        write_to_buffer(d, "Incorrect character password.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // If MFA is set, require it next
    if (acct_char->mfa_enabled && !DEV_SKIP_MFA) {
        ProtocolNoEcho(d, true); // Ensure echo is off for MFA
        write_to_buffer(d, "This character has MFA enabled. Please enter the MFA code: ", 0);
        d->connected = CON_VERIFY_UNLINK_MFA;
        return;
    }

    // Otherwise, prompt for new password for the unlinked character
    ProtocolNoEcho(d, false); // Turn echo off for new password entry
    d->connected = CON_SET_UNLINK_PASSWORD;
}

void login_verify_unlink_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rUnlink cancelled.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    if (!check_char_mfa(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code (or 'cancel' to cancel): ", 0);
        return;
    }
    // MFA verified, prompt for new password
    ProtocolNoEcho(d, false);
    write_to_buffer(d, "Enter a new password for this character (unlinking will remove all account ties): ", 0);
    ProtocolNoEcho(d, true);
    d->connected = CON_SET_UNLINK_PASSWORD;
}

void login_set_unlink_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;

    if (!acceptablePassword(d, argument)) {
        // acceptablePassword should re-prompt or set state
        return;
    }

    // Set new password using the secure method
    if (!set_encrypted_password(&ch->pcdata->pwd, &ch->pcdata->pwd_vers, argument)) {
        ProtocolNoEcho(d, false);
        write_to_buffer(d, "Error setting new password for unlinked character. Unlinking aborted.\n\r", 0);
        // Revert to character menu, character is not unlinked.
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    ch->pcdata->account_pwd_override = false; // No longer an override, it's THE password.

    // Remove account linkage
    free_string(ch->pcdata->account_name);
    ch->pcdata->account_name = str_dup("");
    ch->pcdata->account_id[0] = 0;
    ch->pcdata->account_id[1] = 0;

    // Remove from account's character list
    account_remove_character(d->account, ch->name);
    save_account(d->account);
    save_char_obj(ch);

    ProtocolNoEcho(d, false);
    write_to_buffer(d, "Character has been unlinked from your account. You may now log in with the new password.\n\r", 0);
    
    // d->character still points to the char struct that was loaded.
    // It should be freed as it's no longer the active char for this descriptor.
    free_char(ch); 
    d->character = NULL;

    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

void login_verify_character_delete(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    if (!str_cmp(argument, "C")) {
        write_to_buffer(d, "Character deletion canceled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    if (!str_cmp(argument, "DELETE NOW") && ch->deleted && IS_SET(acct->acct_flags, ACCT_CAN_DELETE_IMMEDIATELY)) {
        // Immediate deletion: remove from account and delete pfile
        account_remove_character(acct, ch->name);
        save_account(acct);
        delete_character(ch); // Implement this to remove the pfile from disk
        write_to_buffer(d, "Character has been permanently deleted.\n\r", 0);
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    write_to_buffer(d, "Type 'DELETE NOW' to confirm immediate deletion, or 'C' to cancel: ", 0);
}

void login_get_account_mfa_for_char(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
//    CHAR_DATA *ch = d->character;
    
    if (!check_account_mfa(acct, argument) && !check_account_recovery_code(acct, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        // Return to character menu
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    // Check if this is a reconnection 
    if (d->reconnecting && d->reconnect_ch) {
        complete_reconnect(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

// Add the new connection states for character-specific authentication
void login_get_char_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    password_check_status status;
    
    write_to_buffer(d, "\n\r", 2);
    
    // Get auth data from account_character
    if (!get_character_auth_data(ch, acct, &acct_char)) {
        // This should not happen with properly linked characters
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "login_get_char_password called with no account character data");
        ProtocolNoEcho(d, false);
        write_to_buffer(d, "Error with character authentication. Please contact staff.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verify the password against the account_character data
    status = check_encrypted_password(argument, acct_char->pwd, acct_char->pwd_vers);

    if (status == PWD_CHECK_FAIL) {
        ProtocolNoEcho(d, false);
        write_to_buffer(d, "Incorrect character password.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // Check if MFA is required
    if (acct_char->mfa_key && !DEV_SKIP_MFA) {
        ProtocolNoEcho(d, true);
        d->connected = CON_GET_CHAR_MFA;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    // Check if this is a reconnection 
    if (d->reconnecting && d->reconnect_ch) {
        complete_reconnect(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

void login_get_char_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    bool mfa_valid = false;
    
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verify MFA using auth API
    AUTH_DATA *auth = get_character_auth(acct_char);

    // Try MFA code first
    if (verify_mfa_code(argument, auth)) {
        mfa_valid = true;
    }
    // Try recovery code if MFA failed
    else if (verify_recovery_code(argument, auth)) {
        mark_recovery_code_used(argument, auth, acct, acct_char);
        save_account(acct);
        mfa_valid = true;
        write_to_buffer(d, "\n\r{YRecovery code accepted. This code cannot be used again.{x\n\r", 0);
    }

    free_auth_data(auth);
    
    if (!mfa_valid) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        // Return to character menu
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    // Check for d->reconnecting flag.
    if (d->reconnecting && d->reconnect_ch) {
        // This is a genuine reconnect - handle accordingly
        complete_reconnect(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

void proceed_to_game(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;

    // Validation check
    if (!ch) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "proceed_to_game called with NULL character");
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: %s preparing to enter game", ch->name);

    // Load remaining character data if not fully loaded (inventory, equipment, skills, affects)
    if (ch->pcdata && !ch->pcdata->fully_loaded) {
        bool load_success = false;

        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: Loading remaining data for %s", ch->name);

        // Try Redis first for faster load
        json_t *cached_json = redis_get_char_full(ch->name);
        if (cached_json) {
            log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: Loading remaining data for %s from Redis cache", ch->name);
            load_success = json_read_char_remaining_from_json(ch, cached_json);
            json_decref(cached_json);
            if (!load_success) {
                log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: Redis load failed for %s, falling back to disk", ch->name);
            }
        }

        // Fall back to disk if Redis didn't work
        if (!load_success) {
            char strsave[MAX_INPUT_LENGTH];
            sprintf(strsave, "%s%c/%s", PLAYER_DIR, tolower(ch->name[0]), capitalize(ch->name));
            load_success = json_read_char_remaining(ch, strsave);
        }

        if (!load_success) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "Failed to load remaining data for %s", ch->name);
            write_to_buffer(d, "\n\r{RError loading character data. Please try again or contact staff.{x\n\r", 0);
            free_char(ch);
            d->character = NULL;
            display_account_menu(d);
            d->connected = CON_ACCOUNT_MENU;
            return;
        }

        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: Successfully loaded remaining data for %s", ch->name);
    }

    if (!list_haslink(loaded_chars, ch)) {
        log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "proceed_to_game: Adding to loaded_chars");
        list_appendlink(loaded_chars, ch);
    }
    /*
    if (!IS_NPC(ch) && !list_haslink(loaded_players, ch)) {
        log_string("proceed_to_game: Adding to loaded_players");
        list_appendlink(loaded_players, ch);
    }
        */
    
    // Reset inactivity timer
    ch->timer = 0;
    
    // Make sure character's descriptor is correctly set
    ch->desc = d;
    
    // Normal login path for fresh connections
    if (IS_IMMORTAL(ch)) {
        send_to_char("{BWelcome, Immortal.{x\n\r\n\r", ch);
        do_function(ch, &do_imotd, "");
        d->connected = CON_READ_IMOTD; 
    } else {
        do_function(ch, &do_motd, "");
        d->connected = CON_READ_MOTD;
    }
}

void setup_account_mfa(DESCRIPTOR_DATA *d)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (!acct)
        return;
        
    if (!setup_mfa_for_account(d, false)) {
        write_to_buffer(d, "Error setting up MFA. Please try again.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Have user verify the MFA
    d->connected = CON_ACCOUNT_MFA_VERIFY;
}

void login_character_mfa_verify(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (IS_NULLSTR(acct_char->mfa_pending_key)) {
        write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // Verify using the pending key via auth API
    AUTH_DATA *auth = get_character_auth(acct_char);
    bool valid = verify_mfa_code(argument, auth);
    free_auth_data(auth);

    if (!valid) {
        write_to_buffer(d, "Invalid MFA code. MFA setup has been aborted.\n\r", 0);

        // Clear the pending key
        free_string(acct_char->mfa_pending_key);
        acct_char->mfa_pending_key = str_dup("");
        save_account(acct);

        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // Setup successful, enable MFA
    free_string(acct_char->mfa_key);
    acct_char->mfa_key = str_dup(acct_char->mfa_pending_key);
    acct_char->mfa_enabled = true;
    
    // Clear pending key
    free_string(acct_char->mfa_pending_key);
    acct_char->mfa_pending_key = str_dup("");
    
    // Generate recovery codes
    generate_recovery_codes(acct_char->recovery_codes, acct_char->recovery_used, MFA_RECOVERY_CODES);
    save_account(acct);

    write_to_buffer(d, "MFA has been successfully enabled for this character.\n\r", 0);
    write_to_buffer(d, "\n\rHere are your recovery codes (save them!):\n\r", 0);
    display_recovery_codes(d, acct_char);

    if (d->creating_staff_character) {
        d->creating_staff_character = false; // Clear flag
        finalize_staff_character_creation(d);
        return;
    }

    display_character_menu(d);
    d->connected = CON_CHARACTER_MENU;
}

void login_account_mfa_confirm(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    
    // Verify we have a pending key to confirm
    if (IS_NULLSTR(acct->mfa_pending_key)) {
        write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
        display_account_mfa_menu(d, "");
        return;
    }
    
    // Validate with the pending key via auth API
    AUTH_DATA *auth = get_account_auth(acct);
    bool valid = verify_mfa_code(argument, auth);
    free_auth_data(auth);

    if (!str_cmp(argument, "cancel")) {
        write_to_buffer(d, "\n\rMFA setup cancelled.\n\r", 0);
        free_string(acct->mfa_pending_key);
        acct->mfa_pending_key = str_dup("");
        save_account(acct);
        display_account_mfa_menu(d, "");
        d->connected = CON_ACCOUNT_MFA_MENU;
        return;
    }

    if (!valid) {
        write_to_buffer(d, "Invalid MFA code. Please try again (or 'cancel'): ", 0);
        return;
    }
    
    // Promote the pending key to active
    free_string(acct->mfa_key);
    acct->mfa_key = str_dup(acct->mfa_pending_key);
    
    // Clear the pending key
    free_string(acct->mfa_pending_key);
    acct->mfa_pending_key = str_dup("");
    
    // Generate recovery codes if they don't exist
    generate_recovery_codes(acct->recovery_codes, acct->recovery_used, MFA_RECOVERY_CODES);
    save_account(acct);

    d->mfa_verified = true;

    write_to_buffer(d, "MFA setup complete and enabled for your account.\n\r", 0);
    display_account_recovery_codes(d, acct);
    display_account_mfa_menu(d, "");
}

void login_get_account_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    AUTH_DATA *auth = get_account_auth(acct);
    bool valid_code = false;
    bool used_recovery = false;

    // Try MFA code first
    if (verify_mfa_code(argument, auth)) {
        valid_code = true;
    }
    // Try recovery code if MFA failed
    else if (verify_recovery_code(argument, auth)) {
        mark_recovery_code_used(argument, auth, acct, NULL);
        save_account(acct);
        valid_code = true;
        used_recovery = true;
    }

    free_auth_data(auth);
    
    if (!valid_code) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        d->login_attempts++;
        
        if (d->login_attempts >= game_settings.max_login_attempts) {
            write_to_buffer(d, "Too many login attempts. Goodbye.\n\r", 0);
            close_socket(d);
            return;
        }
        
        return;
    }
    
    // If using a recovery code, inform the user
    if (used_recovery) {
        write_to_buffer(d, "\n\r{YRecovery code accepted. This code cannot be used again.{x\n\r", 0);
    }
    
    // Log the successful connection
    ProtocolNoEcho(d, false);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Account %s@%s has connected.", acct->username, d->host);

    // Check if they need to provide email
    if (IS_NULLSTR(acct->email)) {
        write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you.\n\r"
            "It will not be distributed to any third parties or abused in any way.\n\r", 0);
        write_to_buffer(d, "\n\rEnter your e-mail address: ", 0);
        d->connected = CON_GET_ACCOUNT_EMAIL;
        return;
    }

    // Password update needed?
    if (acct->passwd_version < 1) {
        write_to_buffer(d, "\n\rYou are required to set a new password. Please do so now.\n\r", 0);
        write_to_buffer(d, "Password: ", 0);
        acct->old_passwd = str_dup(acct->passwd);
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }

    // Add to loaded_accounts if not already present
    acct->refcount++;
    if (!list_haslink(loaded_accounts, acct))
        list_addlink(loaded_accounts, acct);

    free_string(acct->last_login_host);
    acct->last_login_host = str_dup(d->host);
    acct->last_login = current_time;

    // Show the account menu
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

void login_confirm_account_email_for_reset(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    char reset_msg[MSL], reset_subject[MSL];
    
    if(d->login_attempts > 2)
    {
        write_to_buffer(d, "Too many attempts. Please try again later.\n\r", 0);
        close_socket(d);
        return;
    }
    
    if (argument[0] == '\0')
    {
        write_to_buffer(d, "Invalid email address. Please try again.\n\r", 0);
        d->login_attempts++;
        d->connected = CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET;
        return;
    }
    else
    {
        if (strcmp(argument, acct->email))
        {
            write_to_buffer(d, "Email address does not match. Please try again.\n\r", 0);
            d->login_attempts++;
            d->connected = CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET;
            return;
        }
        else
        {
            char tmp_reset_code[16];
            write_to_buffer(d, "Email address confirmed. A reset code will be sent to you for login.\n\r", 0);
            write_to_buffer(d, "Password or Reset Code: ", 0);
            acct->reset_state = RESET_PENDING;
            generate_reset_code(tmp_reset_code, 15);
            free_string(acct->reset_code);
            acct->reset_code = str_dup(tmp_reset_code);
            acct->reset_time = current_time;
            save_account(acct);

            sprintf(reset_subject, "Password Reset for Account: %s", acct->username);
            sprintf(reset_msg, "Your password reset code is: %s.\nPlease note that this code will expire after 24 hours.\n\r", acct->reset_code);

            send_email_async_ex(NULL, acct, acct->email, reset_subject, reset_msg, NULL, NULL);
            d->connected = CON_GET_ACCOUNT_PASSWORD;
            return;
        }
    }
}

void login_change_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (argument[0] == '\0' || !str_cmp(argument, "cancel"))
    {
        write_to_buffer(d, "\n\rPassword change cancelled.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Check if the new password is the same as the old one
    if (!IS_NULLSTR(acct->old_passwd)) {
        password_check_status old_match_status = check_encrypted_password(argument, acct->old_passwd, PWD_VER_CRYPT_SYSTEM);
        if (old_match_status != PWD_CHECK_FAIL) {
            write_to_buffer(d, "New password must be DIFFERENT from your current password!\n\r", 0);
            d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
            return;
        }
    }

    // NEW CHECK: Validate uniqueness across staff characters
    if (game_settings.require_uniq_pass_staff && 
        !validate_password_uniqueness(acct, argument, false, NULL, false)) {
        write_to_buffer(d, "This password matches one of your staff character passwords.\n\r", 0);
        write_to_buffer(d, "Account passwords must be unique from staff character passwords. Please try again.\n\r", 0);
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    if (!acceptablePassword(d, argument)) {
        // acceptablePassword should re-prompt or set state
        return;
    }

    if (d->new_password_buffer) free_string(d->new_password_buffer);
    d->new_password_buffer = str_dup(argument);
    
    d->connected = CON_CONFIRM_ACCOUNT_PASSWORD_CHANGE;
}

void login_confirm_account_password_change(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rPassword change cancelled.\n\r", 0);
        if (d->new_password_buffer) {
            free_string(d->new_password_buffer);
            d->new_password_buffer = NULL;
        }
        ProtocolNoEcho(d, false);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    if (!d->new_password_buffer) {
        write_to_buffer(d, "An error occurred. Please try changing password again.\n\r", 0);
        // Go back to password entry
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    if (strcmp(argument, d->new_password_buffer) != 0) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    // Passwords match, set the new encrypted password
    if (!set_encrypted_password(&acct->passwd, &acct->passwd_version, d->new_password_buffer)) {
        write_to_buffer(d, "Error setting new password. Please try again or contact staff.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }
    
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;

    write_to_buffer(d, "\n\r\n\r{Y***{x {RPassword successfully changed. Please remember to never give your password to anybody.{Y *** {x\n\r\n\r", 0);
    
    // passwd_version is already updated by set_encrypted_password.
    
    // Free old_passwd if it was being held
    if (acct->old_passwd) {
        free_string(acct->old_passwd);
        acct->old_passwd = NULL; 
    }
    
    save_account(acct); // Save immediately after successful password change
    ProtocolNoEcho(d, false);

    // Return to account menu
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}
void login_verify_delete_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    password_check_status status;
    
    write_to_buffer(d, "\n\r", 2);
    ProtocolNoEcho(d, false);

    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "Deletion cancelled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verify the password using account_character data
    status = check_encrypted_password(argument, acct_char->pwd, acct_char->pwd_vers);

    if (status == PWD_CHECK_FAIL) {
        write_to_buffer(d, "Incorrect character password.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    // If an old password format was used, upgrade it
    if (status == PWD_CHECK_SUCCESS_SHA256_CUSTOM || status == PWD_CHECK_SUCCESS_PLAINTEXT) {
        if (!set_encrypted_password(&acct_char->pwd, &acct_char->pwd_vers, argument)) {
            // Continue with deletion, but log the failure to upgrade
            log_message(LOG_LEVEL_WARN, LOG_WARN, "Failed to upgrade password format during delete verification");
        } else {
            save_account(acct); // Save the upgraded password
        }
    }
    
    // If the character also has MFA, we need to verify that too
    if (!IS_NULLSTR(acct_char->mfa_key)) {
        write_to_buffer(d, "\n\r{RThis character has MFA enabled.{x\n\r", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_VERIFY_DELETE_MFA;
        return;
    }
    
    // If no MFA, proceed to confirmation
    write_to_buffer(d, "\n\r{RWARNING: This will flag your character for deletion!{x\n\r", 0);
    d->connected = CON_CONFIRM_DELETE_CHARACTER;
}

void login_verify_delete_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;

    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rDeletion cancelled.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (!check_char_mfa(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code (or 'cancel' to cancel): ", 0);
        return;
    }
    
    // MFA verified, proceed to final confirmation
    write_to_buffer(d, "\n\r{RWARNING: This will permanently delete this character!{x\n\r", 0);
    d->connected = CON_CONFIRM_DELETE_CHARACTER;
}

bool account_has_immortal(ACCOUNT_DATA *acct)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    bool has_immortal = false;
    DESCRIPTOR_DATA temp_d;
    
    // If the account can create staff, treat as staff-capable
    if (IS_SET(acct->acct_flags,ACCT_CAN_CREATE_STAFF))
        return true;

    if (!acct || !acct->characters)
        return false;
    
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        // Check if the character is marked as staff and has appropriate staff rank
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            has_immortal = true;
            break;
        }
        
        // If staff status isn't confirmed, temporarily load the character to check
        if (!has_immortal && ch_entry->name && *ch_entry->name) {
            // Initialize a temporary descriptor
            memset(&temp_d, 0, sizeof(temp_d));

            // Try to load the character (basic load only - don't need inventory for this check)
            if (load_char_obj_basic(&temp_d, ch_entry->name)) {
                // Check if they're immortal
                if (IS_IMMORTAL(temp_d.character)) {
                    has_immortal = true;
                    
                    // Update the account character entry with the staff rank
                    ch_entry->staff = true;
                    ch_entry->staff_rank = get_staff_rank(temp_d.character);
                } else {
                    // Make sure non-immortal characters are correctly marked
                    ch_entry->staff = false;
                }
                
                // Free the temporary character
                free_char(temp_d.character);
                temp_d.character = NULL;
            }
            
            if (has_immortal)
                break;
        }
    }
    iterator_stop(&it);
    
    // If we made any changes to the account character entries, save the account
    if (has_immortal) {
        save_account(acct);
    }
    
    return has_immortal;
}

void login_creating_new_staff_char(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;
    //char buf[MAX_STRING_LENGTH];

    argument[0] = UPPER(argument[0]);
    if (!check_parse_name(argument)) {
        write_to_buffer(d, "Illegal character name, try another.\n\r", 0);
        return;
    }

    // Check if character exists
    if (player_exists(argument)) {
        write_to_buffer(d, "That character already exists. Please choose another name.\n\r", 0);
        return;
    }

    // Free any previous character
    if (d->character != NULL) {
        free_char(d->character);
        d->character = NULL;
    }

    d->character = new_char();
    ch = d->character;
    ch->desc = d;
    if (ch->pcdata == NULL)
        ch->pcdata = new_pcdata();

    free_string(ch->name);
    ch->name = str_dup(argument);

    // Mark as staff
    ch->pcdata->staff_rank = STAFF_IMMORTAL;
    ch->pcdata->creation_date = current_time;

    write_to_buffer(d, "Enter an email address for this staff character: ", 0);
    d->connected = CON_GET_STAFF_EMAIL;
}

void login_get_staff_email(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;

    if (argument[0] == '\0' || !strstr(argument, "@") || !strstr(argument, ".")) {
        write_to_buffer(d, "That's not a valid email address.\n\r", 0);
        write_to_buffer(d, "Enter an email address for this staff character: ", 0);
        return;
    }

    free_string(ch->pcdata->email);
    ch->pcdata->email = str_dup(argument);

    write_to_buffer(d, "Should this staff character have individual MFA? (Y/N): ", 0);
    d->connected = CON_STAFF_MFA_PROMPT;
}

void login_staff_mfa_prompt(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    
    switch (toupper(argument[0])) {
        case 'Y':
            d->creating_staff_character = true;
            // First link the character to the account so account_character data exists
            free_string(ch->pcdata->account_name);
            ch->pcdata->account_name = str_dup(acct->username);
            ch->pcdata->account_id[0] = acct->id[0];
            ch->pcdata->account_id[1] = acct->id[1];
            
            // Add character to account to create account_character entry
            account_add_character(acct, ch);
            save_account(acct);
            
            // Now set up MFA - this will use account_character data instead of pcdata
            setup_character_mfa(d);
            break;
            
        case 'N':
            // If game requires unique passwords for staff, prompt for password
            if (game_settings.require_uniq_pass_staff) {
                write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
                ProtocolNoEcho(d, true);
                d->connected = CON_STAFF_PASSWORD;
            } else {
                // First link to account
                free_string(ch->pcdata->account_name);
                ch->pcdata->account_name = str_dup(acct->username);
                ch->pcdata->account_id[0] = acct->id[0];
                ch->pcdata->account_id[1] = acct->id[1];
                
                // Use account password - no character-specific password needed
                if (!acct->passwd || IS_NULLSTR(acct->passwd)) {
                    write_to_buffer(d, "Account password is not set. Cannot use as staff password.\n\r", 0);
                    write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
                    ProtocolNoEcho(d, true);
                    d->connected = CON_STAFF_PASSWORD;
                    break;
                }
                
                // Add character to account without setting character-specific password
                account_add_character(acct, ch);
                save_account(acct);
                
                // Finalize
                finalize_staff_character_creation(d);
            }
            break;
            
        default:
            write_to_buffer(d, "Please answer Y or N: ", 0);
            break;
    }
}

void login_staff_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    
    if (!acceptablePassword(d, argument)) {
        // acceptablePassword should handle re-prompting
        return;
    }

    // NEW CHECK: Validate staff password uniqueness
    if (game_settings.require_uniq_pass_staff && 
        !validate_password_uniqueness(acct, argument, true, ch->name, true)) {
        write_to_buffer(d, "Staff character passwords must be unique.\n\r", 0);
        write_to_buffer(d, "This password matches either your account password or another character's password.\n\r", 0);
        return;
    }

    // First link the character to the account so account_character data exists
    free_string(ch->pcdata->account_name);
    ch->pcdata->account_name = str_dup(acct->username);
    ch->pcdata->account_id[0] = acct->id[0];
    ch->pcdata->account_id[1] = acct->id[1];
    
    // Add character to account to create account_character entry
    account_add_character(acct, ch);
    
    // Store the temporary password in the descriptor for confirmation
    if (d->new_password_buffer) {
        free_string(d->new_password_buffer);
    }
    d->new_password_buffer = str_dup(argument);
    
    d->connected = CON_CONFIRM_STAFF_PASSWORD;
}

void login_confirm_staff_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    
    if (!d->new_password_buffer) {
        write_to_buffer(d, "An error occurred. Please try setting the password again.\n\r", 0);
        // Go back to password entry
        d->connected = CON_STAFF_PASSWORD;
        return;
    }
    
    if (strcmp(argument, d->new_password_buffer) != 0) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
        d->connected = CON_STAFF_PASSWORD;
        return;
    }
    
    // Get the account_character that was created
    if (!get_character_auth_data(ch, acct, &acct_char)) {
        // This should not happen if account_add_character was called properly
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "login_confirm_staff_password called with no account character data");
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        write_to_buffer(d, "Error creating staff character. Please try again.\n\r", 0);
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Set the password on the account_character
    if (!set_encrypted_password(&acct_char->pwd, &acct_char->pwd_vers, d->new_password_buffer)) {
        free_string(d->new_password_buffer);
        d->new_password_buffer = NULL;
        write_to_buffer(d, "Error setting staff character password. Please try again.\n\r", 0);
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Clean up the temporary password buffer
    free_string(d->new_password_buffer);
    d->new_password_buffer = NULL;
    
    // Save the account with the new password
    save_account(acct);
    
    // Complete the staff character creation
    finalize_staff_character_creation(d);
}

void finalize_staff_character_creation(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;

    // Link to account
    free_string(ch->pcdata->account_name);
    ch->pcdata->account_name = str_dup(acct->username);
    ch->pcdata->account_id[0] = acct->id[0];
    ch->pcdata->account_id[1] = acct->id[1];

    // Mark as staff
    ch->pcdata->staff_rank = STAFF_IMMORTAL;

    // Add to account
    account_add_character(acct, ch);
    save_account(acct);
    save_char_obj(ch);

    write_to_buffer(d, "\n\rStaff character created successfully!\n\r", 0);
    free_char(ch);
    d->character = NULL;
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

void display_character_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    char buf[MAX_STRING_LENGTH];
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Get MFA status from account_character data - use key presence instead of flag
    bool mfa_enabled = !IS_NULLSTR(acct_char->mfa_key);
    bool mfa_pending = !IS_NULLSTR(acct_char->mfa_pending_key);
    bool has_secret = mfa_enabled || mfa_pending;
    bool has_recovery = has_recovery_codes((const char **)acct_char->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(acct_char->email);

    // If MFA is enabled, require code before allowing changes (except confirmation)
    if (mfa_enabled && !mfa_pending && !d->mfa_verified) {
        d->connected = CON_CHARACTER_MFA_VERIFY_FOR_SETTINGS;
        return;
    }

    write_to_buffer(d, "\n\r{B=={W[ {YMFA SETTINGS {W]{B=={x\n\r", 0);
    sprintf(buf, "MFA is currently: %s\n\r",
        mfa_enabled ? "{GENABLED{x" : (mfa_pending ? "{YSETUP IN PROGRESS{x" : "{ROFF{x"));
    write_to_buffer(d, buf, 0);

    // --- Setup Section ---
    if (!mfa_enabled && !mfa_pending) {
        write_to_buffer(d, "\n\r{CSetup:{x\n\r{GS{x) Set up MFA\n\r", 0);
    } else if (mfa_pending) {
        write_to_buffer(d, "\n\r{CSetup:{x\n\r{GC{x) Confirm MFA setup\n\r", 0);
    }

    // --- QR Code Section ---
    bool show_qr_section = (has_secret && has_email) || has_secret;
    if (show_qr_section) {
        write_to_buffer(d, "\n\r{CQR Code:{x\n\r", 0);
        if (has_secret)
            write_to_buffer(d, "{GD{x) Display QR code in terminal\n\r", 0);
        if (has_secret && has_email)
            write_to_buffer(d, "{GQ{x) Email QR code\n\r", 0);
    }

    // --- Recovery Codes Section ---
    bool show_recovery_section = (mfa_enabled || mfa_pending) && has_recovery;
    if (show_recovery_section) {
        write_to_buffer(d, "\n\r{CRecovery Codes:{x\n\r", 0);
        write_to_buffer(d, "{GV{x) Show recovery codes\n\r", 0);
        if (has_email)
            write_to_buffer(d, "{GE{x) Email recovery codes\n\r", 0);
    }

    // --- Actions Section ---
    bool show_actions = (mfa_enabled || mfa_pending);
    if (show_actions) {
        write_to_buffer(d, "\n\r{CActions:{x\n\r", 0);
        if (mfa_enabled || mfa_pending)
            write_to_buffer(d, "{GX{x) Disable MFA\n\r", 0);
        // Only show generate/regenerate if not pending
        if (mfa_enabled) {
            write_to_buffer(d, has_recovery ? "{GG{x) Regenerate recovery codes\n\r" : "{GG{x) Generate recovery codes\n\r", 0);
        }
    }

    write_to_buffer(d, "\n\r{GB{x) Back to character menu\n\r", 0);

    d->connected = CON_CHARACTER_MFA_MENU;
}

void login_character_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Get MFA status from account_character data
    bool mfa_enabled = !IS_NULLSTR(acct_char->mfa_key);
    bool mfa_pending = !IS_NULLSTR(acct_char->mfa_pending_key);
    bool has_secret = mfa_pending || mfa_enabled;
    bool has_recovery = has_recovery_codes((const char **)acct_char->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(acct_char->email);

    switch (toupper(argument[0])) {
        case 'S': // Set up MFA
            if (mfa_enabled || mfa_pending) {
                write_to_buffer(d, "MFA is already enabled or setup is in progress.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            
            // Call setup_mfa_for_char instead of manually creating keys
            if (!setup_mfa_for_char(ch, false)) {
                write_to_buffer(d, "Error setting up MFA. Please try again.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            
            // Display the character MFA menu again with updated status
            display_character_mfa_menu(d, "");
            break;
            
        case 'C': // Confirm MFA setup
            if (!mfa_pending) {
                write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            d->connected = CON_CHARACTER_MFA_CONFIRM;
            break;
            
        case 'Q': // Email QR code
            if (!has_secret) {
                write_to_buffer(d, "No MFA secret available to email a QR code.\n\r", 0);
            } else if (!has_email) {
                write_to_buffer(d, "No email address set for this character.\n\r", 0);
            } else {
                send_qr_email_for_char(ch, acct_char->email, 
                    acct_char->mfa_pending_key ? acct_char->mfa_pending_key : acct_char->mfa_key);
                write_to_buffer(d, "QR code emailed.\n\r", 0);
            }
            display_character_mfa_menu(d, "");
            break;
            
        case 'D': // Display QR code in terminal
            if (!has_secret) {
                write_to_buffer(d, "No MFA secret available to display a QR code.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            } else {
                char qr_url[MIL];
                if (!IS_NULLSTR(acct_char->mfa_pending_key))
                    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, acct_char->mfa_pending_key);
                else
                    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, acct_char->mfa_key);
                display_qr_code(d, qr_url);
                display_acct_char_mfa_key(d, acct_char);
            }
            display_character_mfa_menu(d, "");
            break;
            
        case 'X': // Disable MFA
            if (!mfa_enabled && !mfa_pending) {
                write_to_buffer(d, "MFA is not enabled or pending for this character.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            write_to_buffer(d, "Are you sure you want to disable MFA? (Y/N): ", 0);
            d->connected = CON_CHARACTER_MFA_DISABLE_CONFIRM;
            break;
            
        case 'V': // Show recovery codes
            if (!(mfa_enabled || mfa_pending) || !has_recovery) {
                write_to_buffer(d, "No recovery codes available to display.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            display_recovery_codes(d, acct_char);
            display_character_mfa_menu(d, "");
            break;
            
        case 'E': // Email recovery codes
            if (!(mfa_enabled || mfa_pending) || !has_recovery) {
                write_to_buffer(d, "No recovery codes available to email.\n\r", 0);
            } else if (!has_email) {
                write_to_buffer(d, "No email address set for this character.\n\r", 0);
            } else {
                send_recovery_codes_email_for_char(ch, acct_char->email);
                write_to_buffer(d, "Recovery codes emailed.\n\r", 0);
            }
            display_character_mfa_menu(d, "");
            break;
            
        case 'G': // Generate/Regenerate recovery codes
            if (!(mfa_enabled || mfa_pending)) {
                write_to_buffer(d, "MFA must be enabled or pending to generate recovery codes.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            generate_recovery_codes(acct_char->recovery_codes, acct_char->recovery_used, MFA_RECOVERY_CODES);
            save_account(acct);
            write_to_buffer(d, has_recovery ? "Recovery codes regenerated.\n\r" : "Recovery codes generated.\n\r", 0);
            display_recovery_codes(d, acct_char);
            display_character_mfa_menu(d, "");
            break;
            
        case 'B': // Back
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
            break;
            
        default:
            write_to_buffer(d, "Invalid or unavailable choice.\n\r", 0);
            display_character_mfa_menu(d, "");
            break;
    }
}

void login_character_mfa_confirm(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    if (!has_auth_data || !acct_char || IS_NULLSTR(acct_char->mfa_pending_key)) {
        write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
        display_character_mfa_menu(d, "");
        return;
    }
    
    // Use the pending key for validation via auth API
    AUTH_DATA *auth = get_character_auth(acct_char);
    bool valid = verify_mfa_code(argument, auth);
    free_auth_data(auth);

    if (!str_cmp(argument, "cancel")) {
        write_to_buffer(d, "\n\rMFA setup cancelled.\n\r", 0);
        if (acct_char) {
            free_string(acct_char->mfa_pending_key);
            acct_char->mfa_pending_key = str_dup("");
            save_account(acct);
        }
        display_character_mfa_menu(d, "");
        d->connected = CON_CHARACTER_MFA_MENU;
        return;
    }

    if (!valid) {
        write_to_buffer(d, "Invalid MFA code. Please try again (or 'cancel'): ", 0);
        d->connected = CON_CHARACTER_MFA_CONFIRM;
        return;
    }
    
    // Promote pending key to active, no need to set mfa_enabled flag
    free_string(acct_char->mfa_key);
    acct_char->mfa_key = str_dup(acct_char->mfa_pending_key);
    
    // No need to clear a pending flag, just clear the pending key
    free_string(acct_char->mfa_pending_key);
    acct_char->mfa_pending_key = str_dup("");
    
    // Generate recovery codes
    generate_recovery_codes(acct_char->recovery_codes, acct_char->recovery_used, MFA_RECOVERY_CODES);
    save_account(acct);

    d->mfa_verified = true;

    write_to_buffer(d, "\n\rMFA enabled! Here are your recovery codes (save them!):\n\r", 0);
    display_recovery_codes(d, acct_char);
    display_character_mfa_menu(d, "");
}





void login_character_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool mfa_valid = false;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verify MFA using auth API
    AUTH_DATA *auth = get_character_auth(acct_char);

    // Try MFA code first
    if (verify_mfa_code(argument, auth)) {
        mfa_valid = true;
    }
    // Try recovery code if MFA failed
    else if (verify_recovery_code(argument, auth)) {
        mark_recovery_code_used(argument, auth, acct, acct_char);
        save_account(acct);
        mfa_valid = true;
        write_to_buffer(d, "\n\r{YRecovery code accepted. This code cannot be used again.{x\n\r", 0);
    }

    free_auth_data(auth);
    
    if (!str_cmp(argument, "cancel")) {
        write_to_buffer(d, "\n\rReturning to character menu.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    if (mfa_valid) {
        d->mfa_verified = true;
        display_character_mfa_menu(d, "");
    } else {
        write_to_buffer(d, "Invalid MFA or recovery code. Try again (or 'cancel'): ", 0);
    }
}


/**
 * finalize_new_character - Assign default class and finalize new character creation
 *
 * Replaces the interactive class/subclass selection flow. Assigns the default
 * CLASS_DATA, sets up starting skills/groups from that class, applies
 * preferences, initializes traits, and advances to CON_READ_MOTD.
 * Starting weapon is handled by a post-creation script.
 *
 * Legacy class fields (class_current, sub_class_current, etc.) are left at
 * their pcdata defaults (-1 / 0). Code that reads them must gracefully handle
 * those values until they are removed in Phase 9.
 *
 * @param d  Descriptor of the character being created
 */
static void finalize_new_character(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    CLASS_DATA *new_class;

    log_message_f(LOG_LEVEL_INFO, LOG_INFO, "%s@%s new player.", ch->name, d->host);

    SET_BIT(ch->act[0], PLR_NO_CHALLENGE);

    /* Assign default class from the new class system at level 1 */
    new_class = class_get_default();
    if (new_class) {
        add_class_level(ch, new_class, 1);
        ch->pcdata->current_class = get_class_level(ch, new_class);
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "nanny: assigned default class '%s' to %s",
            new_class->name, ch->name);

        /* Add skill groups defined by the class */
        group_add(ch, "global skills", false);
        if (new_class->groups) {
            ITERATOR it;
            SKILL_GROUP *sg;
            iterator_start(&it, new_class->groups);
            while ((sg = (SKILL_GROUP *)iterator_nextdata(&it)))
                group_add(ch, sg->name, false);
            iterator_stop(&it);
        }
    } else {
        log_message(LOG_LEVEL_BUG, LOG_ERROR,
            "nanny: no default class found — new character has no class!");
        group_add(ch, "global skills", false);
    }

    /* Make it so no notes appear */
    ch->pcdata->last_note    = current_time;
    ch->pcdata->last_idea    = current_time;
    ch->pcdata->last_penalty = current_time;
    ch->pcdata->last_news    = current_time;
    ch->pcdata->last_changes = current_time;

    send_to_char("\n\r{YPress ENTER to begin your journey, adventurer!{W\n\r", ch);

    /* Apply game defaults from pc_set_table, then account prefs on top */
    pref_apply_game_defaults(ch);
    if (d->account)
        pref_apply_to_character(d->account, ch, ch->pcdata->preferences);

    ch->level     = 1;
    ch->tot_level = 0;  /* stays 0 so first-login setup block fires */

    /* Initialize personal trait values for the new character */
    char_init_traits(ch);

    /* Save new character to the account if applicable */
    if (d->account) {
        account_add_character(d->account, ch);
        save_account(d->account);
    }

    d->connected = CON_READ_MOTD;
}


/* DEPRECATED: login_get_new_class and login_get_sub_class are no longer part of
 * the normal character creation flow. Default class is now assigned automatically
 * by finalize_new_character(). These functions are retained for reference only. */
void login_get_new_class(DESCRIPTOR_DATA *d, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char classes[MSL];
    CHAR_DATA *ch;
    int iClass;
    HELP_DATA *help;
    ch = d->character;

        sprintf(classes, "\n\r{YChoose your class {B[{Cmage cleric thief warrior{B]{Y:{x ");
        if (!str_prefix("help", argument))
        {
            argument = one_argument(argument,arg);
            if (argument[0] == '\0')
            {
                send_to_char("{b++++++{B------{C++++++ {WCLASSES AND SUBCLASSES{C++++++{B------{b++++++{x\n\r\n\r", ch);
                if ((help = lookup_help_exact("classes professions", 0, topHelpCat)) != NULL)
                    send_to_char(help->text, ch);
            }
            else
            {
                if ((iClass = class_lookup(argument)) != -1 &&
                    (help = lookup_help_exact(class_table[iClass].name, 0, topHelpCat)) != NULL)
                {
                    sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
                    send_to_char(buf, ch);
                    send_to_char(help->text, ch);
                }
                else
                    send_to_char("That's not a class.\n\r", ch);
            }

            send_to_char(classes, ch);
        }

        if (argument[0] == '\0') {
            send_to_char(classes, ch);
            return;
        }

        if ((iClass = class_lookup(argument)) == -1)
        {
            send_to_char("{xThat's not a class.\n\r", ch);
            send_to_char(classes, ch);
            return;
        }

        ch->pcdata->class_current = iClass;

        switch(iClass)
        {
        case CLASS_MAGE:
            ch->pcdata->class_mage = CLASS_MAGE;
            ch->pcdata->class_cleric = -1;
            ch->pcdata->class_thief = -1;
            ch->pcdata->class_warrior = -1;
            break;
        case CLASS_CLERIC:
            ch->pcdata->class_mage = -1;
            ch->pcdata->class_cleric = CLASS_CLERIC;
            ch->pcdata->class_thief = -1;
            ch->pcdata->class_warrior = -1;
            break;
        case CLASS_THIEF:
            ch->pcdata->class_mage = -1;
            ch->pcdata->class_cleric = -1;
            ch->pcdata->class_thief = CLASS_THIEF;
            ch->pcdata->class_warrior = -1;
            break;
        case CLASS_WARRIOR:
            ch->pcdata->class_mage = -1;
            ch->pcdata->class_cleric = -1;
            ch->pcdata->class_thief = -1;
            ch->pcdata->class_warrior = CLASS_WARRIOR;
            break;
        }

        send_to_char("\n\r{xFor each class there are a possible of three subclasses. Each subclass\n\r", ch);
        send_to_char("is for a particular alignment. A good aligned person may choose from\n\r", ch);
        send_to_char("the neutral or good subclasses. An evil aligned person may choose\n\r", ch);
        send_to_char("from either evil, or neutral subclasses. A neutral aligned person\n\r", ch);
        send_to_char("however may choose from either good, neutral or evil aligned subclasses.\n\r", ch);
        send_to_char("For help on a specific subclass, type help <subclass name>.\n\r\n\r", ch);

        strcpy(buf, "{YSelect the subclass you would like to begin with ");
        add_possible_subclasses(ch, buf);
        strcat(buf, "{Y:{x ");

        send_to_char(buf, ch);
        d->connected = CON_GET_SUB_CLASS;
    
}

void login_get_sub_class(DESCRIPTOR_DATA *d, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char subclasses[MSL];
    CHAR_DATA *ch;
    int iClass,weapon;
    HELP_DATA *help;
    ch = d->character;
        sprintf(subclasses, "\n\r{YChoose your subclass ");
        add_possible_subclasses(ch, subclasses);
        strcat(subclasses, "{Y:{x ");

        if (!str_prefix("help", argument))
        {
            argument = one_argument(argument,arg);
            if (argument[0] == '\0' || !str_prefix(argument, "subclasses") || !str_prefix(argument, "classes"))
            {
                send_to_char("{b++++++{B------{C++++++ {WCLASSES AND SUBCLASSES {C++++++{B------{b++++++{x\n\r\n\r", ch);
                if ((help = lookup_help_exact("classes professions", 0, topHelpCat)) != NULL)
                    send_to_char(help->text, ch);
            }
            else
            {
                sprintf(buf, "%s", argument);
                for (iClass = 0; iClass < MAX_SUB_CLASS; iClass++)
                {
                    if (!str_prefix(buf, sub_class_table[iClass].name[ch->sex]) &&
                        !sub_class_table[iClass].remort)
                        break;
                }

                if (iClass == MAX_SUB_CLASS)
                    send_to_char("That's not a subclass.\n\r", ch);
                else
                {
                    /* Kind of a hack for now*/
                    if (!str_cmp(sub_class_table[iClass].name[ch->sex], "witch") ||
                        !str_cmp(sub_class_table[iClass].name[ch->sex], "warlock"))
                        sprintf(buf, "Warlock Witch");
                    else if (!str_cmp(sub_class_table[iClass].name[ch->sex], "sorcerer") ||
                            !str_cmp(sub_class_table[iClass].name[ch->sex], "sorceress"))
                        sprintf(buf, "Sorcerer Sorceress");
                    else
                        sprintf(buf, sub_class_table[iClass].name[ch->sex]);

                    if ((help = lookup_help_exact(buf, 0, topHelpCat)) != NULL)
                    {
                        sprintf(buf, "{b++++++{B------{C++++++ {W%s {C++++++{B------{b++++++{x\n\r\n\r", help->keyword);
                        send_to_char(buf, ch);
                        send_to_char(help->text, ch);
                    }
                }
            }

            send_to_char(subclasses, ch);
            return;
        }

        if (argument[0] == '\0') {
            send_to_char(subclasses, ch);
            return;
        }

        iClass = sub_class_lookup(ch, argument);
        if (iClass == -1)
        {
            send_to_char("{xThat's not a subclass you can choose.\n\r", ch);
            send_to_char(subclasses, ch);
            return;
        }

        ch->pcdata->sub_class_current = iClass;

        if (ch->pcdata->class_mage != -1)
            ch->pcdata->sub_class_mage = iClass;
        else if (ch->pcdata->class_cleric != -1)
            ch->pcdata->sub_class_cleric = iClass;
        else if (ch->pcdata->class_thief != -1)
            ch->pcdata->sub_class_thief = iClass;
        else if (ch->pcdata->class_warrior != -1)
            ch->pcdata->sub_class_warrior = iClass;

        log_message_f(LOG_LEVEL_INFO, LOG_INFO, "%s@%s new player.", ch->name, d->host);

        SET_BIT(ch->act[0], PLR_NO_CHALLENGE);

        group_add(ch,"global skills",false);
        group_add(ch,class_table[ch->pcdata->class_current].base_group,false);
        group_add(ch, sub_class_table[ch->pcdata->sub_class_current].default_group, false);

        /* Make it so no notes appear*/
        ch->pcdata->last_note = current_time;
        ch->pcdata->last_idea = current_time;
        ch->pcdata->last_penalty = current_time;
        ch->pcdata->last_news = current_time;
        ch->pcdata->last_changes = current_time;

        send_to_char("\n\r{YPress ENTER to begin your journey, adventurer!{W\n\r", ch);
        buf[0] = '\0';

        /* Apply game defaults from pc_set_table, then account prefs on top */
        pref_apply_game_defaults(ch);
        if (d->account)
            pref_apply_to_character(d->account, ch, ch->pcdata->preferences);

        ch->level     = 0;
        ch->tot_level = 0;

        /* Set up weapon skill*/
        switch (ch->pcdata->class_current)
        {
        case CLASS_MAGE:	weapon = skill_resolve_gsn("quarterstaff");	break;
        case CLASS_CLERIC:	weapon = skill_resolve_gsn("quarterstaff");	break;
        case CLASS_THIEF:	weapon = skill_resolve_gsn("dagger");		break;
        case CLASS_WARRIOR:	weapon = skill_resolve_gsn("sword");		break;
        default:
            log_message(LOG_LEVEL_BUG, LOG_ERROR, "nanny: bad current class in weapon pick");
            weapon = skill_resolve_gsn("sword");
            break;
        }

        ch->pcdata->learned[weapon] = 50;

    /* Assign default class(es) from the new class system */
    {
        /* Try to find the selected sub_class in the new CLASS_DATA system */
        CLASS_DATA *new_class = class_from_legacy(
            ch->pcdata->class_current, ch->pcdata->sub_class_current);

        if (!new_class)
            new_class = class_get_default();

        if (new_class) {
            add_class_level(ch, new_class, 0);
            ch->pcdata->current_class = get_class_level(ch, new_class);
            log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                "nanny: assigned class '%s' to %s",
                new_class->name, ch->name);
        }
    }

    /* Initialize personal trait values for the new character */
    char_init_traits(ch);

    ch->level = 0;

    ch->tot_level = 0;

        // Save new character to the account if applicable
    if (d->account) {
        account_add_character(d->account, ch);
        save_account(d->account);
    }

    d->connected = CON_READ_MOTD;

}


char* format_location_string(ROOM_INDEX_DATA *room) {
    static char loc_buf[100];
    AREA_DATA *area;

    if (room == NULL) {
        strcpy(loc_buf, "(Invalid Room Vnum)");
        return loc_buf;
    }
    
    area = room->area;
    if (area == NULL) {
        strcpy(loc_buf, "(Unknown Area)");
        return loc_buf;
    }
    
    if (area->name != NULL && area->name[0] != '\0') {
        strncpy(loc_buf, area->name, sizeof(loc_buf) - 1);
        loc_buf[sizeof(loc_buf) - 1] = '\0'; // Ensure null termination
    } else {
        strcpy(loc_buf, "(Unnamed Area)");
    }
    
    return loc_buf;
}

bool can_link_characters(ACCOUNT_DATA *acct) {
    if (!acct) return false;
    if (game_settings.allow_link_all || IS_SET(acct->acct_flags, ACCT_CAN_LINK)) {
        int max_chars = (acct->character_limit > 0) ? acct->character_limit : game_settings.max_characters;
        int total_chars = account_count_nonstaff_characters(acct);
        if (max_chars == 0 || total_chars < max_chars)
            return true;
    }
    return false;
}

bool can_unlink_characters(ACCOUNT_DATA *acct) {
    if (!acct) return false;
    return (game_settings.allow_unlink_all || IS_SET(acct->acct_flags, ACCT_CAN_UNLINK));
}

void resend_character_verification_code(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (IS_NULLSTR(acct_char->pending_email)) {
        write_to_buffer(d, "No pending email to verify.\n\r", 0);
        return;
    }
    
    if ((current_time - acct_char->email_verification_last_sent) < (15 * 60)) {
        write_to_buffer(d, "You must wait before resending the verification email.\n\r", 0);
        return;
    }
    
    char code[16];
    if (IS_NULLSTR(acct_char->email_verification_code)) {
        generate_reset_code(code, 15);
        free_string(acct_char->email_verification_code);
        acct_char->email_verification_code = str_dup(code);
    }
    
    acct_char->email_verification_time = current_time;
    acct_char->email_verification_last_sent = current_time;
    
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", acct_char->email_verification_code);
    send_email_async_ex(ch, acct, acct_char->pending_email, "Sentience: Verify Your Character Email Address", body, NULL, NULL);
    save_account(acct);
    
    write_to_buffer(d, "Verification email resent.\n\r", 0);
}

void login_verify_character_email_change(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (IS_NULLSTR(acct_char->pending_email) || IS_NULLSTR(acct_char->email_verification_code)) {
        write_to_buffer(d, "No email change is pending.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Expiry check (72 hours)
    if ((current_time - acct_char->email_verification_time) > (72 * 3600)) {
        write_to_buffer(d, "Verification code expired. Please start the process again.\n\r", 0);
        
        free_string(acct_char->pending_email);
        acct_char->pending_email = str_dup("");
        free_string(acct_char->email_verification_code);
        acct_char->email_verification_code = str_dup("");
        acct_char->email_verification_time = 0;
        save_account(acct);
        
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (!str_cmp(argument, acct_char->email_verification_code)) {
        free_string(acct_char->email);
        acct_char->email = str_dup(acct_char->pending_email);
        acct_char->email_verified = true;
        free_string(acct_char->pending_email);
        acct_char->pending_email = str_dup("");
        free_string(acct_char->email_verification_code);
        acct_char->email_verification_code = str_dup("");
        acct_char->email_verification_time = 0;
        acct_char->email_verification_last_sent = 0;
        save_account(acct);
        
        write_to_buffer(d, "\n\rYour character's email address has been updated and verified.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
    } else if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rEmail verification cancelled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
    } else {
        write_to_buffer(d, "Invalid code. Try again (or 'cancel' to cancel): ", 0);
    }
}

void login_change_character_email(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = get_character_auth_data(ch, acct, &acct_char);

    if (!str_cmp(argument, "cancel") || argument[0] == '\0') {
        write_to_buffer(d, "\n\rEmail change cancelled.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    if (!strstr(argument, "@") || !strstr(argument, ".")) {
        write_to_buffer(d, "That's not a valid email address.\n\r", 0);
        write_to_buffer(d, "Enter your e-mail address (or 'cancel' to cancel): ", 0);
        return;
    }
    
    // If we don't have valid auth data, this is an error
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character authentication data.{x\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    if (!game_settings.require_email_verif) {
        // Immediate update
        free_string(acct_char->email);
        acct_char->email = str_dup(argument);
        acct_char->email_verified = true;
        save_account(acct);
        
        write_to_buffer(d, "\n\rEmail address updated.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Verification required - check if already pending
    if (!IS_NULLSTR(acct_char->pending_email) && !str_cmp(argument, acct_char->pending_email)) {
        write_to_buffer(d, "That email is already pending verification.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Check if unchanged
    if (!str_cmp(argument, acct_char->email)) {
        // Email is unchanged, just mark as verified and skip verification
        acct_char->email_verified = true;
        free_string(acct_char->pending_email);
        acct_char->pending_email = str_dup("");
        free_string(acct_char->email_verification_code);
        acct_char->email_verification_code = str_dup("");
        acct_char->email_verification_time = 0;
        acct_char->email_verification_last_sent = 0;
        write_to_buffer(d, "\n\rEmail address is unchanged and now verified.\n\r", 0);
        save_account(acct);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // Set up pending verification
    free_string(acct_char->pending_email);
    acct_char->pending_email = str_dup(argument);
    acct_char->email_verified = false;
    
    // Generate code and set time
    char code[16];
    generate_reset_code(code, 15);
    free_string(acct_char->email_verification_code);
    acct_char->email_verification_code = str_dup(code);
    acct_char->email_verification_time = current_time;
    acct_char->email_verification_last_sent = current_time;
    
    // Notify old email
    if (!IS_NULLSTR(acct_char->email)) {
        send_email_async_ex(ch, acct, acct_char->email, "Sentience: Email Change Requested",
            "A request was made to change your character email. If this was not you, contact support.", NULL, NULL);
    }
    
    // Send code to new email
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", code);
    send_email_async_ex(ch, acct, argument, "Sentience: Verify Your New Email Address", body, NULL, NULL);
    save_account(acct);
    
    write_to_buffer(d, "\n\rA verification code has been sent to your new email address.\n\r", 0);
    d->connected = CON_VERIFY_CHARACTER_EMAIL_CHANGE;
}

bool is_reconnecting(CHAR_DATA *ch)
{
    if (!ch) return false;
    
    // Skip checking freshly created characters not yet in loaded_chars
    if (ch->tot_level == 0 && !ch->pcdata->last_login) 
        return false;
    
    // Debug output for tracing
    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "[DEBUG] is_reconnecting checking: %s", ch->name);
    
    // Direct scan of loaded_chars for more reliable results
    CHAR_DATA *real_existing = NULL;
    ITERATOR it;
    
    iterator_start(&it, loaded_chars);
    while ((real_existing = (CHAR_DATA *)iterator_nextdata(&it))) {
        // Look for a character that:
        // 1. Is not an NPC
        // 2. Has the same name as our character
        // 3. Is *not* our current character instance
        // 4. Is already properly established in the game (has been saved before)
        if (!IS_NPC(real_existing) && 
            real_existing != ch && 
            !str_cmp(ch->name, real_existing->name) &&
            real_existing->in_room != NULL) {
            
            log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "[DEBUG] Found reconnect match: %s (%p), desc: %s",
                    real_existing->name,
                    (void*)real_existing,
                    real_existing->desc ? "connected" : "linkdead");

            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);
    
    // If we're here, we didn't find a matching character
    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "[DEBUG] No reconnect match found");
    return false;
}

/**
 * Gets the authentication data for a character, either from the account_character
 * if linked, or from pcdata if standalone.
 * @param ch The character to get authentication data for
 * @param acct The account the character is linked to (or NULL if unlinked)
 * @param acct_char Output pointer to store found account_character
 * @return True if auth data was found from account, false if using character data
 */
bool get_character_auth_data(CHAR_DATA *ch, ACCOUNT_DATA *acct, ACCOUNT_CHARACTER **acct_char)
{
    if (!ch || !ch->pcdata)
        return false;
    
    // If not linked to an account, use character data
    if (IS_NULLSTR(ch->pcdata->account_name) || !acct)
        return false;

    // Find the account character entry
    ITERATOR it;
    ACCOUNT_CHARACTER *ac;
    
    iterator_start(&it, acct->characters);
    while ((ac = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ac->name, ch->name)) {
            if (acct_char)
                *acct_char = ac;
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);
    
    // Not found in account
    return false;
}

void login_get_char_body_type(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    body_type_t chosen_body_type = BODY_TYPE_NEUTRAL;
    bool body_type_chosen = false;
    int i;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }

    if (argument[0] == '\0') {
        write_to_buffer(d, "Please choose a body type.\n\r", 0);
        
        return;
    }

    for (i = 0; i < BODY_TYPE_MAX; i++) {
        if (!str_prefix(argument, body_type_info[i].name)) {
            chosen_body_type = (body_type_t)i;
            body_type_chosen = true;
            break;
        }
        if (strlen(argument) == 1 && LOWER(argument[0]) == body_type_info[i].name[0]) {
            chosen_body_type = (body_type_t)i;
            body_type_chosen = true;
            break;
        }
    }

    if (!body_type_chosen) {
        write_to_buffer(d, "That's not a valid body type. Options are:\n\r", 0);
        for (i = 0; i < BODY_TYPE_MAX; i++) {
            sprintf(buf, "  %s\n\r", body_type_info[i].name);
            write_to_buffer(d, buf, 0);
        }
        write_to_buffer(d, "What body type do you want? ", 0);
        return;
    }
    ch->body_type = chosen_body_type;
    reset_pronouns_to_body_type(ch, ch->body_type);

    sprintf(buf, "\n\rYou have chosen the '%s' body type.\n\r", get_body_type_name(ch));
    write_to_buffer(d, buf, 0);
    write_to_buffer(d, "The default pronouns for this body type are:\n\r", 0);

    display_pronoun_examples(ch,
                             get_he_she(ch),
                             get_him_her(ch),
                             get_his_her(ch),
                             get_his_hers(ch),
                             get_himself_herself(ch),
                             ch->verb_preference);

    write_to_buffer(d, "\n\rAre these pronouns okay? (Yes/No)\n\r", 0);
    write_to_buffer(d, "If you choose No, you will be guided to set custom pronouns.\n\r> ", 0);
    d->connected = CON_CONFIRM_DEFAULT_PRONOUNS;
}

void login_char_get_default_pronouns(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }

    if (LOWER(argument[0]) == 'y') {
        write_to_buffer(d, "\n\rDefault pronouns accepted.\n\r", 0);
        finalize_new_character(d);
        return;
    } else if (LOWER(argument[0]) == 'n') {
        write_to_buffer(d, "\n\rOkay, let's set your custom pronouns.\n\r", 0);
        
        d->connected = CON_SET_CUSTOM_PRONOUN_SUBJ;
        
        write_to_buffer(d, "Enter your subjective pronoun (e.g., he, she, they, ze): ", 0);
    } else {
        write_to_buffer(d, "Please answer Yes or No.\n\rAre the default pronouns okay? (Yes/No)\n\r> ", 0);
        
    }
}

void login_char_set_custom_pronoun_subj(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }
    if (argument[0] == '\0' || strlen(argument) > (MIL / 4)) { 
        write_to_buffer(d, "Invalid input. Please enter your subjective pronoun (e.g., he, she, they, ze): ", 0);
        return;
    }

    free_string(ch->pronoun_he_she);
    ch->pronoun_he_she = str_dup(argument);
    sprintf(buf, "Subjective pronoun set to: %s\n\r", ch->pronoun_he_she);
    write_to_buffer(d, buf, 0);

    d->connected = CON_SET_CUSTOM_PRONOUN_OBJ;
    write_to_buffer(d, "Enter your objective pronoun (e.g., him, her, them, zir): ", 0);
}

void login_char_set_custom_pronoun_obj(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }

    if (argument[0] == '\0' || strlen(argument) > (MIL / 4)) { // Basic validation
        write_to_buffer(d, "Invalid input. Please enter your objective pronoun (e.g., him, her, them, zir): ", 0);
        return;
    }

    free_string(ch->pronoun_him_her);
    ch->pronoun_him_her = str_dup(argument);
    sprintf(buf, "Objective pronoun set to: %s\n\r", ch->pronoun_him_her);
    write_to_buffer(d, buf, 0);

    d->connected = CON_SET_CUSTOM_PRONOUN_POSS_ADJ;
    write_to_buffer(d, "Enter your possessive adjective pronoun (e.g., his, her, their, zir): ", 0);
}

void login_char_set_custom_pronoun_poss_adj(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }
    if (argument[0] == '\0' || strlen(argument) > (MIL / 4)) {
        write_to_buffer(d, "Invalid input. Please enter your possessive adjective pronoun (e.g., his, her, their, zir): ", 0);
        return;
    }

    free_string(ch->pronoun_his_her);
    ch->pronoun_his_her = str_dup(argument);
    sprintf(buf, "Possessive adjective pronoun set to: %s\n\r", ch->pronoun_his_her);
    write_to_buffer(d, buf, 0);

    d->connected = CON_SET_CUSTOM_PRONOUN_POSS_PRON;
    write_to_buffer(d, "Enter your possessive pronoun (e.g., his, hers, theirs, zirs): ", 0);
}

void login_char_set_custom_pronoun_poss_pron(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }
    if (argument[0] == '\0' || strlen(argument) > (MIL / 4)) {
        write_to_buffer(d, "Invalid input. Please enter your possessive pronoun (e.g., his, hers, theirs, zirs): ", 0);
        return;
    }

    free_string(ch->pronoun_his_hers);
    ch->pronoun_his_hers = str_dup(argument);
    sprintf(buf, "Possessive pronoun set to: %s\n\r", ch->pronoun_his_hers);
    write_to_buffer(d, buf, 0);

    d->connected = CON_SET_CUSTOM_PRONOUN_REFL;
    write_to_buffer(d, "Enter your reflexive pronoun (e.g., himself, herself, themself, zirself): ", 0);
}

void login_char_set_custom_pronoun_refl(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }
    if (argument[0] == '\0' || strlen(argument) > (MIL / 4)) {
        write_to_buffer(d, "Invalid input. Please enter your reflexive pronoun (e.g., himself, herself, themself, zirself): ", 0);
        return;
    }

    free_string(ch->pronoun_himself_herself);
    ch->pronoun_himself_herself = str_dup(argument);
    sprintf(buf, "Reflexive pronoun set to: %s\n\r", ch->pronoun_himself_herself);
    write_to_buffer(d, buf, 0);

    d->connected = CON_SET_CUSTOM_VERB_PREF;
    write_to_buffer(d, "Enter your verb preference (singular/plural/default): ", 0);
}

void login_char_set_custom_verb_pref(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }

    if (argument[0] == '\0') {
        write_to_buffer(d, "Invalid input. Please enter your verb preference (singular/plural/default): ", 0);
        return;
    }

    if (!str_prefix(argument, "singular")) {
        ch->verb_preference = VERB_FORM_SINGULAR;
        write_to_buffer(d, "Verb preference set to: singular.\n\r", 0);
    } else if (!str_prefix(argument, "plural")) {
        ch->verb_preference = VERB_FORM_PLURAL;
        write_to_buffer(d, "Verb preference set to: plural.\n\r", 0);
    } else if (!str_prefix(argument, "default")) {
        ch->verb_preference = VERB_FORM_DEFAULT;
        write_to_buffer(d, "Verb preference set to: default.\n\r", 0);
    } else {
        write_to_buffer(d, "Invalid choice. Please enter 'singular', 'plural', or 'default'.\n\r> ", 0);
        return;
    }

    write_to_buffer(d, "\n\rCustom pronouns and verb preference set. Here are your examples:\n\r", 0);
    display_pronoun_examples(ch,
                             get_he_she(ch),
                             get_him_her(ch),
                             get_his_her(ch),
                             get_his_hers(ch),
                             get_himself_herself(ch),
                             ch->verb_preference);

    d->connected = CON_SET_CUSTOM_PRONOUNS_CONFIRM;
    return;
}

void login_char_set_custom_pronouns_confirm(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    char buf[MSL];

    if (!ch) {
        write_to_buffer(d, "Error: No character found.\n\r", 0);
        close_socket(d);
        return;
    }

    if (LOWER(argument[0]) == 'y') {
        write_to_buffer(d, "\n\rCustom pronoun settings confirmed.\n\r", 0);
        finalize_new_character(d);
        return;
    } else if (LOWER(argument[0]) == 'n' || !str_cmp(argument, "back")) {
        write_to_buffer(d, "\n\rOkay, let's review the pronoun options for your chosen body type.\n\r", 0);
        
        reset_pronouns_to_body_type(ch, ch->body_type);


        sprintf(buf, "You have chosen the '%s' body type.\n\r", get_body_type_name(ch));
        write_to_buffer(d, buf, 0);
        write_to_buffer(d, "The default pronouns for this body type are:\n\r", 0);

        display_pronoun_examples(ch,
                                 get_he_she(ch),
                                 get_him_her(ch),
                                 get_his_her(ch),
                                 get_his_hers(ch),
                                 get_himself_herself(ch),
                                 ch->verb_preference);

        write_to_buffer(d, "\n\rAre these pronouns okay? (Yes/No)\n\r", 0);
        write_to_buffer(d, "If you choose No, you will be guided to set custom pronouns again.\n\r> ", 0);
        d->connected = CON_CONFIRM_DEFAULT_PRONOUNS;

    } else {
        write_to_buffer(d, "Please answer Yes, No, or Back.\n\rAre these settings correct? (Yes/No/Back)\n\r> ", 0);
    }
}

/**
 * Find the most recently played character for an account
 * @param acct The account to check
 * @return The most recently played character or NULL if none found
 */
ACCOUNT_CHARACTER *find_most_recent_character(ACCOUNT_DATA *acct)
{
    ACCOUNT_CHARACTER *most_recent = NULL;
    time_t most_recent_time = 0;
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    
    if (!acct || !acct->characters)
        return NULL;
    
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        // Skip deleted characters and those currently online
        if (ch_entry->deleted || is_character_online(ch_entry->name))
            continue;
        
        // If this character was logged in more recently, update our tracking
        if (ch_entry->last_login > most_recent_time) {
            most_recent = ch_entry;
            most_recent_time = ch_entry->last_login;
        }
    }
    iterator_stop(&it);
    
    return most_recent;
}

/**
 * Check if a character is currently online
 * @param name The character name to check
 * @return true if the character is online, false otherwise
 */
bool is_character_online(const char *name)
{
    DESCRIPTOR_DATA *d;
    
    for (d = descriptor_list; d != NULL; d = d->next) {
        if (d->character && !IS_NPC(d->character) && 
            d->connected == CON_PLAYING && 
            !str_cmp(d->character->name, name)) {
            return true;
        }
    }
    
    return false;
}

/**
 * Set a character as the default for an account
 * @param acct The account to update
 * @param char_name The character name to set as default
 * @return true if successful, false if the character wasn't found
 */
bool set_default_character(ACCOUNT_DATA *acct, const char *char_name)
{
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    bool found = false;
    
    if (!acct || !acct->characters || IS_NULLSTR(char_name))
        return false;
    
    // First verify the character exists in this account
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ch_entry->name, char_name)) {
            found = true;
            break;
        }
    }
    iterator_stop(&it);
    
    if (!found)
        return false;
    
    // Update the default character
    if (acct->default_character)
        free_string(acct->default_character);
    
    acct->default_character = str_dup(char_name);
    save_account(acct);
    
    return true;
}

void process_direct_login(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = false;
    
    // Add debug logging
    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Character %s",
            ch ? ch->name : "NULL");

    // Verify character exists
    if (!ch) {
        write_to_buffer(d, "\n\r{RERROR: Character not loaded properly.{x\n\r", 0);
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "process_direct_login: Character is NULL");
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }

    // Get authentication data - critical step
    has_auth_data = get_character_auth_data(ch, acct, &acct_char);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: auth_data=%d, acct_char=%s",
            has_auth_data, acct_char ? "found" : "NULL");

    // Verify we have what we need
    if (!has_auth_data || !acct_char) {
        write_to_buffer(d, "\n\r{RERROR: Unable to find character data in your account.{x\n\r", 0);
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "process_direct_login: Missing account character data");
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    // Check if character is flagged for deletion
    if (ch->deleted) {
        write_to_buffer(d, "This character is flagged for deletion. Please cancel deletion first.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
            
    if (check_playing(d, ch->name))
        return;
    
    // Check for reconnection first - this must happen before authentication
    bool is_reconnecting_attempt = check_reconnect(d, ch->name, false);

    log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: reconnect check=%d, connected=%d",
            is_reconnecting_attempt, d->connected);

    // If check_reconnect changed our connection state, return and let that handle it
    if (is_reconnecting_attempt && d->connected != CON_ACCOUNT_MENU) {
        log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Letting reconnect handler take over");
        return;
    }
    
    // Get authentication status from account_character data
    bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    bool has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);
    
    // Check password requirements
    if (!DEV_SKIP_PASSWORD) {
        if (IS_IMMORTAL(ch) && game_settings.require_uniq_pass_staff && 
            !has_char_pwd &&
            (IS_NULLSTR(acct->passwd) || acct->passwd_version == 0)) {
            write_to_buffer(d, "\n\r{RERROR: Staff characters require a unique password.{x\n\r", 0);
            write_to_buffer(d, "You must set a unique password for this character before logging in.\n\r", 0);
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
            return;
        }
        
        if (has_char_pwd) {
            log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Character requires password");
            write_to_buffer(d, "\n\rThis character requires an additional password.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_GET_CHAR_PASSWORD;
            return;
        }
    }

    // Check MFA requirements
    if (!DEV_SKIP_MFA) {
        if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff &&
            !has_char_mfa &&
            (IS_NULLSTR(acct->mfa_key))) {
            write_to_buffer(d, "\n\r{RERROR: Staff characters require MFA to be enabled.{x\n\r", 0);
            write_to_buffer(d, "You must enable MFA on either your account or this character before logging in.\n\r", 0);
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
            return;
        }

        if (has_char_mfa) {
            log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Character requires MFA");
            write_to_buffer(d, "\n\rThis character has MFA enabled.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_GET_CHAR_MFA;
            return;
        }

        if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff &&
            !IS_NULLSTR(acct->mfa_key) && !has_char_mfa) {
            log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Staff character requires account MFA");
            write_to_buffer(d, "\n\rThis is a staff character. Account MFA verification required.\n\r", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_GET_ACCOUNT_MFA_FOR_CHAR;
            return;
        }
    }

    // Now handle reconnection if needed - only if no authentication was required
    if (is_reconnecting_attempt) {
        log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Processing reconnection now");
        // Need to call with true to actually complete the reconnect
        check_reconnect(d, ch->name, true);
        return;
    }

    // No authentication needed, proceed directly to game
    log_message(LOG_LEVEL_DEBUG, LOG_DEBUG, "process_direct_login: Proceeding to game");
    proceed_to_game(d);
}

void nanny(DESCRIPTOR_DATA *d, char *argument)
{


    while (ISSPACE(*argument))
        argument++;

    switch (d->connected) {
    default:
        log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Nanny: bad d->connected %d.", d->connected);
        connection_remove(d);
        close_socket(d);
        return;
/*
    case CON_GET_NAME:
        login_get_name(d, argument);
        break;

    case CON_GET_OLD_PASSWORD:
        login_get_old_passwd(d, argument);
        break;

    case CON_GET_MFA:
        login_get_mfa(d, argument);
        break;

    case CON_CONFIRM_EMAIL_FOR_RESET:
        login_confirm_email_for_reset(d, argument);
        break;
        */

    case CON_CHANGE_PASSWORD:
        login_change_passwd_initial(d, argument);
        break;

    case CON_CHANGE_PASSWORD_CONFIRM:
        login_change_passwd_confirm(d, argument);
        break;

    case CON_BREAK_CONNECT:
        login_break_connect(d, argument);
        break;

    case CON_CONFIRM_NEW_NAME:
        login_confirm_new_name(d, argument);
        break;
/*
    case CON_GET_NEW_PASSWORD:
        login_get_new_passwd(d, argument);
        break;

    case CON_CONFIRM_NEW_PASSWORD:
        login_confirm_new_passwd(d, argument);
        break;
*/
    case CON_GET_ASCII:
        login_get_ascii(d, argument);
        break;

    case CON_GET_ALIGNMENT:
        login_get_alignment(d, argument);
        break;

    case CON_GET_NEW_RACE:
        login_get_new_race(d, argument);
        break;

    case CON_GET_ORIGIN_RACE:
        login_get_origin_race(d, argument);
        break;

    case CON_GET_NEW_SEX:
        login_get_new_sex(d, argument);
        break;

    case CON_GET_NEW_BODY_TYPE:
        login_get_char_body_type(d, argument);
        break;
    case CON_CONFIRM_DEFAULT_PRONOUNS:
        login_char_get_default_pronouns(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUN_SUBJ:
        login_char_set_custom_pronoun_subj(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUN_OBJ:
        login_char_set_custom_pronoun_obj(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUN_POSS_ADJ:
        login_char_set_custom_pronoun_poss_adj(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUN_POSS_PRON:
        login_char_set_custom_pronoun_poss_pron(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUN_REFL:
        login_char_set_custom_pronoun_refl(d, argument);
        break;
    case CON_SET_CUSTOM_VERB_PREF:
        login_char_set_custom_verb_pref(d, argument);
        break;
    case CON_SET_CUSTOM_PRONOUNS_CONFIRM:
        login_char_set_custom_pronouns_confirm(d, argument);
        break;
    case CON_GET_NEW_CLASS:
        login_get_new_class(d, argument);
        break;
    case CON_GET_SUB_CLASS:
        login_get_sub_class(d, argument);
        break;


    case CON_READ_IMOTD:
        login_read_imotd(d, argument);
        break;

    case CON_READ_MOTD:
        login_read_motd(d, argument);
        break;

    /* Get the player's e-mail if it's not in the pfile already */
    /*
    case CON_GET_EMAIL:
        login_get_email(d, argument);
        break;
*/
    case CON_GET_ACCOUNT_NAME:
        login_get_account(d, argument);
        break;

    case CON_GET_ACCOUNT_PASSWORD:
        login_get_account_password(d, argument);
        break;
    case CON_NEW_ACCOUNT_PASSWORD:
        login_new_account_password(d, argument);
        break;
    case CON_CONFIRM_ACCOUNT_PASSWORD:
        login_confirm_account_password(d, argument);
        break;
    case CON_GET_ACCOUNT_EMAIL:
        login_get_account_email(d, argument);
        break;
    case CON_ACCOUNT_MENU:
        login_account_menu(d, argument);
        break;
    case CON_CREATING_NEW_CHAR:
        login_creating_new_char(d, argument);
        break;
    //case CON_SELECT_CHARACTER:
      //  select_character(d, argument);
        //break;
    case CON_CONFIRM_ACCOUNT_NAME:
        login_confirm_account_name(d, argument);
        break;

        case CON_LINK_CHARACTER_NAME:
        login_link_character_name(d, argument);
        break;
        
    case CON_LINK_CHARACTER_PASSWORD:
        login_link_character_password(d, argument);
        break;
        
    case CON_LINK_CHARACTER_MFA:
        login_link_character_mfa(d, argument);
        break;
    
       case CON_CHARACTER_MENU:
        login_character_menu(d, argument);
        break;
        
    case CON_CHARACTER_PASSWORD:
        login_character_password(d, argument);
        break;
        
    case CON_CONFIRM_CHARACTER_PASSWORD:
        login_confirm_character_password(d, argument);
        break;
        
    case CON_CONFIRM_DELETE_CHARACTER:
        login_confirm_delete_character(d, argument);
        break;

    case CON_GET_CHAR_PASSWORD:
        login_get_char_password(d, argument);
        break;
        
    case CON_GET_CHAR_MFA:
        login_get_char_mfa(d, argument);
        break;
    
    case CON_CHARACTER_MFA_VERIFY:
        login_character_mfa_verify(d, argument);
        break;
    case CON_ACCOUNT_MFA_VERIFY:
        login_account_mfa_confirm(d, argument);
        break;

    case CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET:
        login_confirm_account_email_for_reset(d, argument);
        break;
        
    case CON_CHANGE_ACCOUNT_PASSWORD:
        login_change_account_password(d, argument);
        break;
        
    case CON_CONFIRM_ACCOUNT_PASSWORD_CHANGE:
        login_confirm_account_password_change(d, argument);
        break;

    case CON_CHANGE_ACCOUNT_EMAIL:
        login_change_account_email(d, argument);
        break;
        
    case CON_VERIFY_ACCOUNT_PASSWORD:
        login_verify_account_password(d, argument);
        break;
        
    case CON_ACCOUNT_MFA_MENU:
        login_account_mfa_menu(d, argument);
        break;

    case CON_ACCOUNT_PREFS:
        login_account_prefs_menu(d, argument);
        break;

    case CON_GET_ACCOUNT_MFA_FOR_CHAR:
        login_get_account_mfa_for_char(d, argument);
        break;
        
    case CON_VERIFY_DELETE_PASSWORD:
        login_verify_delete_password(d, argument);
        break;
        
    case CON_VERIFY_DELETE_MFA:
        login_verify_delete_mfa(d, argument);
        break;
    
    case CON_GET_ACCOUNT_MFA:
        login_get_account_mfa(d, argument);
        break;

    case CON_CREATING_NEW_STAFF_CHAR:
        login_creating_new_staff_char(d, argument);
        break;
    case CON_GET_STAFF_EMAIL:
        login_get_staff_email(d, argument);
        break;
    case CON_STAFF_MFA_PROMPT:
        login_staff_mfa_prompt(d, argument);
        break;
    case CON_STAFF_PASSWORD:
        login_staff_password(d, argument);
        break;
    case CON_CONFIRM_STAFF_PASSWORD:
        login_confirm_staff_password(d, argument);
        break;

    case CON_CHARACTER_MFA_MENU:
        login_character_mfa_menu(d, argument);
        break;
    case CON_CHARACTER_MFA_CONFIRM:
        login_character_mfa_confirm(d, argument);
        break;
    case CON_CHARACTER_MFA_DISABLE_CONFIRM:
        login_character_mfa_disable_confirm(d, argument);
        break;
    case CON_CHARACTER_MFA_VERIFY_FOR_SETTINGS:
        login_character_mfa_verify_for_settings(d, argument);
        break;
    case CON_ACCOUNT_MFA_DISABLE_CONFIRM:
        login_account_mfa_disable_confirm(d, argument);
        break;
    case CON_ACCOUNT_MFA_VERIFY_FOR_SETTINGS:
        login_account_mfa_verify_for_settings(d, argument);
        break;
    case CON_ACCOUNT_MFA_CONFIRM:
        login_account_mfa_confirm(d, argument);
        break;
    case CON_VERIFY_ACCOUNT_EMAIL_CHANGE:
        login_verify_account_email_change(d, argument);
        break;
    case CON_CHANGE_CHARACTER_EMAIL:
        login_change_character_email(d, argument);
        break;
    case CON_VERIFY_CHARACTER_EMAIL_CHANGE:
        login_verify_character_email_change(d, argument);
        break;
/*
    case CON_CHARACTER_DELETE:
        login_character_delete(d, argument);
        break;
*/
        case CON_VERIFY_CHARACTER_DELETE:
        login_verify_character_delete(d, argument);
        break;
case CON_VERIFY_UNLINK_PASSWORD:
    login_verify_unlink_password(d, argument);
    break;
case CON_VERIFY_UNLINK_MFA:
    login_verify_unlink_mfa(d, argument);
    break;
case CON_SET_UNLINK_PASSWORD:
    login_set_unlink_password(d, argument);
    break;

case CON_CONFIRM_RESET_PREFS:
    login_confirm_reset_prefs(d, argument);
    break;


    }
}