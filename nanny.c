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


/* Account related functions */

// Everything starts here.
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
        write_to_buffer(d, "Illegal account name, try another.\n\rAccount: ", 0);
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
            
        sprintf(buf, "Wizlocked: %s tried to connect from %s.", argument, d->host);
        log_string(buf);
        wiznet(buf, NULL, NULL, WIZ_LOGINS, 0, 0);
        close_socket(d);
        return;
    }
    
    // Existing account
    if (found) {

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
    
    write_to_buffer(d, "\n\r", 2);

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
                d->connected = CON_GET_ACCOUNT_PASSWORD;
                return;
            } else {
                write_to_buffer(d, "Please confirm your email address: ", 0);
                d->connected = CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET;  // Changed to account-specific handler
                return;
            }
        }
    }

    // Check for reset code
    if (acct->reset_state == RESET_PENDING) {
        if (strcmp(argument, acct->reset_code) && 
            strcmp(sha256_crypt(argument), acct->passwd)) {
            write_to_buffer(d, "Wrong reset code.\n\r", 0);
            d->login_attempts++;
            return;
        }
        
        if ((current_time - acct->reset_time) > 86400) {
            if (game_settings.enable_email)
                write_to_buffer(d, "Reset code has expired. Please try resetting again.\n\r", 0);
            else
                write_to_buffer(d, "Reset code has expired. Please contact staff for assistance.\n\r", 0);
                
            acct->reset_state = NO_RESET;
            free_string(acct->reset_code);
            acct->reset_time = 0;
            save_account(acct);
            close_socket(d);
            return;
        }
        
        if (!str_cmp(argument, acct->reset_code)) {
            acct->reset_state = NO_RESET;
            free_string(acct->reset_code);
            acct->reset_code = str_dup("");
            acct->reset_time = 0;
            acct->old_passwd = str_dup(acct->passwd);
            
            write_to_buffer(d, "Reset code accepted. You are required to set a new password.\n\r", 0);

            d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
            return;
        }
    }

    // Normal password check
    if (strcmp(sha256_crypt(argument), acct->passwd)) {
        // Log bad password attempts
        sprintf(log_buf, "Denying access to account %s@%s (bad password).",
            acct->username, d->host);
        log_string(log_buf);
        
        if (game_settings.enable_email)
            write_to_buffer(d, "Wrong password. Please try again, or use 'resetpassword' to reset.\n\r", 0);
        else
            write_to_buffer(d, "Wrong password. Please try again or reach out to staff for assistance.\n\r", 0);
            
        d->login_attempts++;
        return;
    }

    // Handle MFA
    if (!IS_NULLSTR(acct->mfa_key) && acct->mfa_enabled) {
        ProtocolNoEcho(d,true);
        d->connected = CON_GET_ACCOUNT_MFA;
        return;
    }

    ProtocolNoEcho(d, false);

    // Log the successful connection
    sprintf(log_buf, "Account %s@%s has connected.", acct->username, d->host);
    log_string(log_buf);

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
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
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
    
    if (!acceptablePassword(d, argument))
        return;
        
    acct->passwd = str_dup(sha256_crypt(argument));
    acct->passwd_version = 1;
    
    write_to_buffer(d, "Please confirm password: ", 0);
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
    
    if (strcmp(sha256_crypt(argument), acct->passwd)) {
        write_to_buffer(d, "Passwords don't match.\n\r", 0);
        write_to_buffer(d, "Please enter a new password: ", 0);
        d->connected = CON_NEW_ACCOUNT_PASSWORD;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you.\n\r"
                "It will not be distributed to any third parties or abused in any way.\n\r", 0);
    write_to_buffer(d, "\n\rEnter your e-mail address: ", 0);
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
    if (!game_settings.require_email_verification) {
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
        write_to_buffer(d, "Invalid code. Try again: ", 0);
    }
}

// Verify current password before allowing a password change
// This leads to CON_CHANGE_ACCOUNT_PASSWORD.
void login_verify_account_password(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    write_to_buffer(d, "\n\r", 2);
    
    if (strcmp(sha256_crypt(argument), acct->passwd)) {
        write_to_buffer(d, "Incorrect password.\n\r", 0);
        ProtocolNoEcho(d, false);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
}

// User is verifying their MFA code for account settings.
// If the code is correct, we proceed to the MFA settings menu.
void login_account_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    if (check_account_mfa(acct, argument) || check_account_recovery_code(acct, argument)) {
        d->mfa_verified = true;
        display_account_mfa_menu(d, "");
    } else {
        write_to_buffer(d, "Invalid MFA or recovery code. Try again: ", 0);
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
            acct->mfa_enabled = false;
            save_account(acct);
            write_to_buffer(d, "MFA disabled for this character.\n\r", 0);
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
    ITERATOR it;
    ACCOUNT_CHARACTER *ch_entry;
    
    d->mfa_verified = false;

    // Create temporary arrays to store characters for sorting
    ACCOUNT_CHARACTER *staff_chars[10];
    ACCOUNT_CHARACTER *regular_chars[100];
    int staff_count = 0;
    int regular_count = 0;
    
    write_to_buffer(d, "\n\r{B=={W[ {YSENTIENCE ACCOUNT MENU {W]{B=={x\n\r\n\r", 0);
    
    sprintf(buf, "Account: {C%s{x\n\r", acct->username);
    write_to_buffer(d, buf, 0);
    
    if (game_settings.require_email_verification && !acct->email_verified && !IS_NULLSTR(acct->pending_email)) {
        sprintf(buf, "Email: {C%s{x (pending: {Y%s{x)\n\r\n\r", IS_NULLSTR(acct->email) ? "Not set" : acct->email, acct->pending_email);
    } else if (game_settings.require_email_verification && !acct->email_verified && IS_NULLSTR(acct->pending_email)) {
        sprintf(buf, "Email: {YPending verification{x\n\r\n\r");
    } else {
        sprintf(buf, "Email: {C%s{x\n\r\n\r", IS_NULLSTR(acct->email) ? "Not set" : acct->email);
    }
    write_to_buffer(d, buf, 0);
    
    // First pass: separate staff and regular characters
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
            staff_chars[staff_count++] = ch_entry;
        } else {
            regular_chars[regular_count++] = ch_entry;
        }
    }
    iterator_stop(&it);
    
    // Sort characters alphabetically in two buckets (staff and normal)
    for (int i = 0; i < staff_count - 1; i++) {
        for (int j = 0; j < staff_count - i - 1; j++) {
            if (strcasecmp(staff_chars[j]->name, staff_chars[j+1]->name) > 0) {
                ACCOUNT_CHARACTER *temp = staff_chars[j];
                staff_chars[j] = staff_chars[j+1];
                staff_chars[j+1] = temp;
            }
        }
    }
    
    for (int i = 0; i < regular_count - 1; i++) {
        for (int j = 0; j < regular_count - i - 1; j++) {
            if (strcasecmp(regular_chars[j]->name, regular_chars[j+1]->name) > 0) {
                ACCOUNT_CHARACTER *temp = regular_chars[j];
                regular_chars[j] = regular_chars[j+1];
                regular_chars[j+1] = temp;
            }
        }
    }
    
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
            live_ch->in_room->area ? live_ch->in_room->area->name : NULL,
            (live_ch->in_room->region && live_ch->in_room->region->name) ? live_ch->in_room->region->name : NULL
            );
        } else {
            loc_str = format_location_string(
                ch_entry->last_area, 
                ch_entry->last_region
            );
    }
            sprintf(name_buf, "{W%s{x", ch_entry->name);
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
                    live_ch->in_room->area ? live_ch->in_room->area->name : NULL,
                    (live_ch->in_room->region && live_ch->in_room->region->name) ? live_ch->in_room->region->name : NULL
                );
            } else {
                loc_str = format_location_string(
                    ch_entry->last_area, 
                    ch_entry->last_region
                );
            }
            
            sprintf(name_buf, "{W%s{x", ch_entry->name);
            
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
    
    write_to_buffer(d, "{GE{x) Change email address\n\r", 0);
    if (game_settings.require_email_verification && !acct->email_verified) {
    write_to_buffer(d, "{GV{x) Verify email address\n\r", 0);
    if (!IS_NULLSTR(acct->pending_email))
        write_to_buffer(d, "{GR{x) Resend verification email\n\r", 0);
    }
    write_to_buffer(d, "{GP{x) Change password\n\r", 0);
    
    write_to_buffer(d, "{GM{x) MFA settings\n\r", 0);
        
    write_to_buffer(d, "{GQ{x) Quit\n\r\n\r", 0);

}

// This handles the selections in the account menu.
// It can lead to a number of states:
// CON_CREATING_NEW_CHAR (creating a new character)
// CON_CREATING_NEW_STAFF_CHAR (creating a new staff character)
// CON_LINKING_CHARACTER (linking an existing character)
// CON_GET_ACCOUNT_EMAIL (changing email address)
// CON_GET_ACCOUNT_PASSWORD (changing password)
// CON_ACCOUNT_MFA_MENU (MFA settings)
// CON_VERIFY_ACCOUNT_EMAIL_CHANGE (verifying email address)
void login_account_menu(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    ACCOUNT_CHARACTER *ch_entry;
    int choice = 0;
    ITERATOR it;

    // Arrays to store sorted characters
    ACCOUNT_CHARACTER *staff_chars[100];
    ACCOUNT_CHARACTER *regular_chars[100];
    int staff_count = 0;
    int regular_count = 0;

    // Handle number choices (character selection)
    if (isdigit(argument[0])) {
        choice = atoi(argument);

        // First separate and count staff/regular characters
        iterator_start(&it, acct->characters);
        while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
            if (ch_entry->staff && ch_entry->staff_rank >= STAFF_IMMORTAL) {
                staff_chars[staff_count++] = ch_entry;
            } else {
                regular_chars[regular_count++] = ch_entry;
            }
        }
        iterator_stop(&it);
        
        // Sort staff characters alphabetically
        for (int i = 0; i < staff_count - 1; i++) {
            for (int j = 0; j < staff_count - i - 1; j++) {
                if (strcasecmp(staff_chars[j]->name, staff_chars[j+1]->name) > 0) {
                    ACCOUNT_CHARACTER *temp = staff_chars[j];
                    staff_chars[j] = staff_chars[j+1];
                    staff_chars[j+1] = temp;
                }
            }
        }
        
        // Sort regular characters alphabetically
        for (int i = 0; i < regular_count - 1; i++) {
            for (int j = 0; j < regular_count - i - 1; j++) {
                if (strcasecmp(regular_chars[j]->name, regular_chars[j+1]->name) > 0) {
                    ACCOUNT_CHARACTER *temp = regular_chars[j];
                    regular_chars[j] = regular_chars[j+1];
                    regular_chars[j+1] = temp;
                }
            }
        }
        
        int total_count = staff_count + regular_count;

        if (choice < 1 || choice > total_count) {
            write_to_buffer(d, "Invalid character selection.\n\r", 0);
            display_account_menu(d);
            return;
        }

        // Find the selected character
        if (choice <= staff_count) {
            ch_entry = staff_chars[choice - 1];
        } else {
            ch_entry = regular_chars[choice - staff_count - 1];
        }

        select_character(d, ch_entry);
        return;
    }

    // Handle letter choices (menu options)
    switch (toupper(argument[0])) {
        case 'C': // Create new character
            if (game_settings.require_email_verification && !IS_EMAIL_VERIFIED(acct)) {
                write_to_buffer(d, "\n\rYou must verify your email address before creating characters.\n\r", 0);
                write_to_buffer(d, "Select 'V' from the menu to verify your email address.\n\r", 0);
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
            write_to_buffer(d, "\n\rWhat will be your character's name? ", 0);
            d->connected = CON_CREATING_NEW_CHAR;
            return;

        case 'I': // Create new staff character
            if (!IS_SET(acct->acct_flags,ACCT_CAN_CREATE_STAFF)) {
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
            return;

        case 'L': // Link existing character
            if (!can_link_characters(acct)) {
                write_to_buffer(d, "Linking is not available for your account.\n\r", 0);
                display_account_menu(d);
                return;
            }
            write_to_buffer(d, "\n\rEnter the name of the character to link: ", 0);
            d->connected = CON_LINK_CHARACTER_NAME;
            return;

        case 'E': // Change email address
            write_to_buffer(d, "\n\rCurrent email: ", 0);
            write_to_buffer(d, IS_NULLSTR(acct->email) ? "Not set\n\r" : acct->email, 0);
            d->connected = CON_CHANGE_ACCOUNT_EMAIL;
            return;

        case 'P': // Change password
            write_to_buffer(d, "\n\rEnter your current password: ", 0);
            ProtocolNoEcho(d, true);
            d->connected = CON_VERIFY_ACCOUNT_PASSWORD;
            break;

        case 'M': // MFA settings
            display_account_mfa_menu(d, "");
            break;

        case 'Q': // Quit
            write_to_buffer(d, "\n\rThank you for playing Sentience!\n\r", 0);
            close_socket(d);
            break;
        case 'R':
            resend_account_verification_code(d);
            display_account_menu(d);
            return;
        case 'V':
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
            write_to_buffer(d, "Enter the code sent to your email: ", 0);
            d->connected = CON_VERIFY_ACCOUNT_EMAIL_CHANGE;
            return;

        default:
            write_to_buffer(d, "Invalid choice.\n\r", 0);
            display_account_menu(d);
            break;
    }
}

// This function displays the MFA menu for the account.
// It shows the current MFA status, options to set up or disable MFA,
// and options to view or email recovery codes.
// It is paired with login_account_mfa_menu to handle the actual selections.
void display_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];
    bool mfa_enabled = acct->mfa_enabled;
    bool mfa_pending = acct->mfa_pending;
    bool has_secret = !IS_NULLSTR(acct->mfa_pending_key) || !IS_NULLSTR(acct->mfa_key);
    bool has_recovery = has_recovery_codes((const char **)acct->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(acct->email);

    // If MFA is enabled, require code before allowing changes (except confirmation)
    if (acct->mfa_enabled && !acct->mfa_pending && !d->mfa_verified) {
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

    write_to_buffer(d, "\n\r{GB{x) Back to account menu\n\r", 0);


    d->connected = CON_ACCOUNT_MFA_MENU;
}

// This function handles the MFA menu selections.
// It allows the user to set up MFA, confirm setup, disable MFA,
// display or email QR codes, and manage recovery codes.
// It is paired with display_account_mfa_menu to show the menu.

void login_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument) {
    ACCOUNT_DATA *acct = d->account;
    bool mfa_enabled = acct->mfa_enabled;
    bool mfa_pending = acct->mfa_pending;
    bool has_secret = !IS_NULLSTR(acct->mfa_pending_key) || !IS_NULLSTR(acct->mfa_key);
    bool has_recovery = has_recovery_codes((const char **)acct->recovery_codes, MFA_RECOVERY_CODES);

    switch (toupper(argument[0])) {
        case 'S': // Set up MFA
            if (mfa_enabled || mfa_pending) {
                write_to_buffer(d, "MFA is already enabled or setup is in progress.\n\r", 0);
                display_account_mfa_menu(d, "");
                return;
            }
            {
                char key[MIL];
                char qr_url[MIL];
                char *secret = generate_totp_key(key, sizeof(key));
                free_string(acct->mfa_pending_key);
                acct->mfa_pending_key = str_dup(secret);
                acct->mfa_pending = true;
                generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, secret);
                display_qr_code(d, qr_url);
                write_to_buffer(d, "\n\rMFA setup started. Use your authenticator app to scan the QR code or enter the secret above.\n\r", 0);
                write_to_buffer(d, "You must confirm MFA setup before it is enabled.\n\r", 0);
                display_account_mfa_menu(d, "");
            }
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
            } else if (IS_NULLSTR(acct->email)) {
                write_to_buffer(d, "No email address set for this account.\n\r", 0);
            } else {
                send_qr_email_for_account(acct, acct->email, acct->mfa_pending_key ? acct->mfa_pending_key : acct->mfa_key);
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
                if (!IS_NULLSTR(acct->mfa_pending_key))
                    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, acct->mfa_pending_key);
                else
                    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, acct->mfa_key);
                display_qr_code(d, qr_url);
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
            } else if (IS_NULLSTR(acct->email)) {
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
        write_to_buffer(d, "Illegal name, try another.\n\rName: ", 0);
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

        if (IS_SET(ch->act[0], PLR_DENY))
        {
            sprintf(log_buf, "Denying access to %s@%s.", argument, d->host);
            log_string(log_buf);
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

                sprintf(buf, "The game is wizlocked, %s tried to connect from %s.", argument, d->host);
                log_string(buf);
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

void login_get_old_passwd(DESCRIPTOR_DATA *d, char *argument)
{
	CHAR_DATA *ch;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    write_to_buffer(d, "\n\r", 2);

    if (d->login_attempts >= game_settings.max_login_attempts)
    {
        write_to_buffer(d, "Too many login attempts. Goodbye.\n\r", 0);
        close_socket(d);
        return;
    }
    if (game_settings.enable_email)
    {
        if(!strcmp(argument, "resetpassword"))
        {
            if (ch->pcdata->reset_state == RESET_PENDING)
            {
                write_to_buffer(d, "You have already requested a password reset. Please enter the reset code.\n\r", 0);
                d->connected = CON_GET_OLD_PASSWORD;
                return;
            }
            else
            {
                if (ch->pcdata->email[0] == '\0')
                {
                    write_to_buffer(d, "You must have an email address set to reset your password. Please reach out to staff for a manual reset.\n\r", 0);
                    d->connected = CON_GET_OLD_PASSWORD;
                    return;
                }
                else
                {
                    write_to_buffer(d, "Please confirm your email address: \n\r", 0);
                    d->connected = CON_CONFIRM_EMAIL_FOR_RESET;
                    return;
                }
            }
        }
    }
            

    if (d->character->pcdata->reset_state == RESET_PENDING)
    {
        if (strcmp(argument, ch->pcdata->reset_code) && strcmp(sha256_crypt(argument), ch->pcdata->pwd) && strcmp(crypt(argument, ch->pcdata->pwd), ch->pcdata->pwd) && strcmp(argument, ch->pcdata->pwd))
        {
            write_to_buffer(d, "Wrong reset code.\n\r", 0);
            d->login_attempts++;
            d->connected = CON_GET_OLD_PASSWORD;
            return;
        }
        if ((current_time - d->character->pcdata->reset_time) > 86400)
        {
            if (game_settings.enable_email)
                write_to_buffer(d, "Reset code has expired. Please try resetting again.\n\r", 0);
            else
                write_to_buffer(d, "Reset code has expired. Please contact staff for a manual reset.\n\r", 0);
            d->character->pcdata->reset_state = NO_RESET;
            free_string(ch->pcdata->reset_code);
            ch->pcdata->reset_time = 0;
            save_char_obj(ch);
            close_socket(d);
            return;
        }
        if (!str_cmp(argument, ch->pcdata->reset_code))
        {
            ch->pcdata->reset_state = NO_RESET;
            free_string(ch->pcdata->reset_code);
            ch->pcdata->reset_code = str_dup("");
            ch->pcdata->reset_time = 0;
            save_char_obj(ch);
            write_to_buffer(d, "Reset code accepted. You are required to set a new password.\n\r Password: ", 0);
            ch->pcdata->old_pwd = str_dup(ch->pcdata->pwd);
            d->connected = CON_CHANGE_PASSWORD;
            return;
        }
    }

    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
    {
        /* Log bad password attempts */
        sprintf(log_buf, "Denying access to %s@%s (bad password).",
        ch->name, d->host);
        log_string(log_buf);
        wiznet(log_buf,NULL,NULL,WIZ_LOGINS,0,get_staff_rank(ch));
        if (game_settings.enable_email)
            write_to_buffer(d, "Wrong password. Please try again, or use 'resetpassword' to attempt a reset.\n\r", 0);
        else
            write_to_buffer(d, "Wrong password. Please try again or reach out to staff for assistance.\n\r", 0);
        d->login_attempts++;
        d->connected = CON_GET_OLD_PASSWORD;
        return;
    }

    if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled)
    {
        ProtocolNoEcho(d,true);
        d->connected = CON_GET_MFA;
        return;
    }


    //	write_to_buffer(d, echo_on_str, 0);
    ProtocolNoEcho(d,false);

    if (check_playing(d,ch->name))
        return;

    if (check_reconnect(d, ch->name, true))
        return;

    sprintf(log_buf, "%s@%s has connected.", ch->name, d->host);
    log_string(log_buf);

    ch->pcdata->old_pwd = str_dup(ch->pcdata->pwd);

    /* OLD character who doesn't have an email on file with us will be prompted for it here. */
    if (ch->pcdata->email == NULL) {
        write_to_buffer(d, "\n\rPlease enter a valid e-mail address at which we can reach you in case you lose your password.\n\r"
            "It will not be distributed to any third parties or abused in any way.\n\r", 0);
        d->connected = CON_GET_EMAIL;
        return;
    }
    if (ch->pcdata->need_change_pw == true || ch->pcdata->pwd_vers < 1) {
        send_to_char("\n\rYou are required to set a new password. Please do so now.\n\r",ch);
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }

    if (IS_IMMORTAL(ch))
    {
        send_to_char("{BWelcome, Immortal.{x\n\r\n\r", ch);
        do_function(ch, &do_imotd, "");
        if(IS_IMPLEMENTOR(ch)) {
            if(game_settings.wizlock) send_to_char("\n\r{b-{B==={C=={W[ {YWIZLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
            if(game_settings.new_char_lock || game_settings.new_acct_lock) send_to_char("\n\r{b-{B==={C=={W[ {GNEWLOCK ACTIVE{W ]{C=={B==={b-{x\n\r", ch);
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

void login_confirm_email_for_reset(DESCRIPTOR_DATA *d, char *argument)
{

	CHAR_DATA *ch;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
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
        d->connected = CON_CONFIRM_EMAIL_FOR_RESET;
        return;
    }
    else
    {
        if (strcmp(argument, ch->pcdata->email))
        {
            write_to_buffer(d, "Email address does not match. Please try again.\n\r", 0);
            d->login_attempts++;
            d->connected = CON_CONFIRM_EMAIL_FOR_RESET;
            return;
        }
        else
        {
            char tmp_reset_code[16];
            write_to_buffer(d, "Email address confirmed. A reset code will be sent to you for login.\n\r", 0);
            write_to_buffer(d, "Password or Reset Code: ", 0);
            ch->pcdata->reset_state = RESET_PENDING;
            generate_reset_code(tmp_reset_code, 15);
            ch->pcdata->reset_code = str_dup(tmp_reset_code);
            ch->pcdata->reset_time = current_time;
            save_char_obj(ch);

            sprintf(reset_subject, "Password Reset for %s", d->character->name);
            sprintf(reset_msg, "Your password reset code is: %s.\nPlease note that this code will expire after 24 hours.\n\r", d->character->pcdata->reset_code);

            send_email_async(d->character, d->character->pcdata->email, reset_subject, reset_msg, NULL, NULL);
            d->connected = CON_GET_OLD_PASSWORD;
            return;
        }
    }
}

void login_change_passwd_initial(DESCRIPTOR_DATA *d, char *argument)
{
	CHAR_DATA *ch;
	char *pwdnew;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    if (argument[0] == '\0')
    {
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }
    if (ch->pcdata->pwd_vers < 1) 
    {
        if (!strcmp(crypt(argument, ch->pcdata->old_pwd), ch->pcdata->old_pwd))
        {
            send_to_char("Password must be DIFFERENT from your current password!\n\rPassword: ", ch);
            d->connected = CON_CHANGE_PASSWORD;
            return;
        }
    }
    else
    {
        if (!strcmp(sha256_crypt(argument), ch->pcdata->old_pwd))
        {
            send_to_char("Password must be DIFFERENT from your current password!\n\rPassword: ", ch);
            d->connected = CON_CHANGE_PASSWORD;
            return;
        }
    }


    if (!acceptablePassword(d, argument))
        return;

    pwdnew = sha256_crypt(argument);

    free_string(ch->pcdata->pwd);
    ch->pcdata->pwd	= str_dup(pwdnew);

    ch->pcdata->need_change_pw = false;
    d->connected = CON_CHANGE_PASSWORD_CONFIRM;
}

void login_change_passwd_confirm(DESCRIPTOR_DATA *d, char *argument)
{

	CHAR_DATA *ch;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
    {
        write_to_buffer(d, "Passwords don't match.\n\rPassword: ", 0);
        d->connected = CON_CHANGE_PASSWORD;
        return;
    }

    send_to_char("\n\r\n\r{Y***{x {RThank you. Please remember to never give your password to anybody.{Y *** {x\n\r\n\r", ch);
    if (ch->pcdata->pwd_vers < 1){
        ch->pcdata->pwd_vers = 1;
    }
    save_char_obj(d->character);
//		write_to_buffer(d, echo_on_str, 0);
    ProtocolNoEcho(d,false);

    if (IS_IMMORTAL(ch))
    {
        do_function(ch, &do_imotd, "");
        d->connected = CON_READ_IMOTD;
    }
    else
    {
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
        write_to_buffer(d,"Reconnect attempt failed.\n\rName: ",0);
        if (d->character != NULL)
        {
            free_char(d->character);
            d->character = NULL;
        }
        d->connected = CON_GET_NAME;
        break;

    case 'n' : case 'N':
        write_to_buffer(d,"Name: ",0);
        if (d->character != NULL)
        {
            free_char(d->character);
            d->character = NULL;
        }
        d->connected = CON_GET_NAME;
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
            // Skip directly to ASCII color selection
            write_to_buffer(d, "\n\rWould you like ascii colour (Y/N)? ", 0);
            d->connected = CON_GET_ASCII;
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

void login_get_new_passwd(DESCRIPTOR_DATA *d, char *argument)
{
	CHAR_DATA *ch;
	char *pwdnew;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    write_to_buffer(d, "\n\r", 2);
    if (!acceptablePassword(d, argument))
        return;

    pwdnew = sha256_crypt(argument);

    free_string(ch->pcdata->pwd);
    ch->pcdata->pwd	= str_dup(pwdnew);
    ch->pcdata->pwd_vers = 1;
    write_to_buffer(d, "Please retype password: ", 0);
    d->connected = CON_CONFIRM_NEW_PASSWORD;

    ch->pcdata->need_change_pw = false;
}

void login_confirm_new_passwd(DESCRIPTOR_DATA *d, char *argument)
{
	CHAR_DATA *ch;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    write_to_buffer(d, "\n\r", 2);

    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
    {
        write_to_buffer(d, "Passwords don't match.\n\r\n\rRetype password: ", 0);
        d->connected = CON_GET_NEW_PASSWORD;
        return;
    }

//		write_to_buffer(d, echo_on_str, 0);
    ProtocolNoEcho(d,false);

    write_to_buffer(d,	"\n\rPlease enter a valid e-mail address at which we can reach you in case you lose your password.\n\r"
                        "It will not be distributed to any third parties or abused in any way.\n\r", 0);

    send_to_char("\n\rEnter your e-mail address: ", ch);

    d->connected = CON_GET_EMAIL;
}

void login_get_ascii(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
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

    send_to_char("\n\r{r-----{R======{D//// {WWelcome to the world of Sentience! {D\\\\{R======{r-----{x\n\r", ch);
    write_to_buffer(d, "\n\r", 2);

    // Set account details on character if coming from account creation flow
    if (d->connected == CON_CREATING_NEW_CHAR && d->account) {
        free_string(ch->pcdata->account_name);
        ch->pcdata->account_name = str_dup(d->account->username);
        ch->pcdata->account_id[0] = d->account->id[0];
        ch->pcdata->account_id[1] = d->account->id[1];
        
        // Since we're creating through an account, email is already set at account level
        // No need to collect it again at character level
        if (IS_NULLSTR(ch->pcdata->email)) {
            free_string(ch->pcdata->email);
            ch->pcdata->email = str_dup(d->account->email);
        }
    }

    wiznet("Newbie alert!  $N sighted.", ch, NULL, WIZ_NEWBIE, 0, 0);

    send_to_char("\n\r{YChoose an alignment ({GGood/Neutral/Evil{Y):{x ", ch);
    d->connected = CON_GET_ALIGNMENT;
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

void login_get_new_race(DESCRIPTOR_DATA *d, char *argument)
{

	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	char races[MSL];
	CHAR_DATA *ch;
	int i;
	RACE_DATA *race;
	HELP_DATA *help;

	while (ISSPACE(*argument))
		argument++;

	ch = d->character;
    one_argument(argument,arg);

    sprintf(races, "\n\r{YChoose your race");
    add_possible_races(ch, races);
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
            race = get_race_data(argument);
            if (IS_VALID(race) &&
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

    race = get_race_data(argument);
    if (!IS_VALID(race)) {
        send_to_char("There is no such race.\n\r", ch);
        send_to_char(races, ch);
        return;
    }

    if (!race->playable || race == gr_shaper) {
        send_to_char("That isn't a player race.\n\r", ch);
        send_to_char(races, ch);
        return;
    }

    if (race->remort) {
        send_to_char("You cannot choose that race.\n\r", ch);
        send_to_char(races, ch);
        return;
    }

#if 0
    // Disabled as players can freely choose their alignment (and can change it as well)
    //   Will inform players that choosing a race whose standard alignment differs from what
    //    they've selected will cause some issues they will likely have to overcome, due to
    //    npcs and factions not liking them.
    if ((ch->alignment == 0 && pc_race_table[race].alignment != ALIGN_NONE) ||
        (ch->alignment  < 0 && pc_race_table[race].alignment != ALIGN_EVIL) ||
        (ch->alignment  > 0 && pc_race_table[race].alignment != ALIGN_GOOD))
    {
        if (ch->alignment == 0)		send_to_char("That is not a neutral aligned race.\n\r", ch);
        else if (ch->alignment < 0) send_to_char("That is not an evil aligned race.\n\r", ch);
        else						send_to_char("That is not a good aligned race.\n\r", ch);

        send_to_char(races, ch);
        break;
    }
#endif
    ch->race = race;

    /* initialize stats */
    for (i = 0; i < MAX_STATS; i++) {
        ch->perm_stat[i] = race->stats[i];
        ch->dirty_stat[i] = true;
    }

    ch->act[0]		= (ch->act[0] | race->act[0]) & ~ACT_IS_NPC;
    ch->act[1]		= ch->act[1] | race->act[1];
    ch->affected_by[0] = ch->affected_by[0] | race->aff[0];
    ch->affected_by[1] = ch->affected_by[1] | race->aff[1];

    ch->imm_flags_perm = race->imm;
    ch->res_flags_perm = race->res;
    ch->vuln_flags_perm = race->vuln;
    /* 20203003 - Tieryo - Fixing racial affects */
    ch->affected_by_perm[0] = race->aff[0];
    ch->affected_by_perm[1] = race->aff[1];

    ch->imm_flags	= ch->imm_flags|race->imm;
    ch->res_flags	= ch->res_flags|race->res;
    ch->vuln_flags	= ch->vuln_flags|race->vuln;
    ch->form	= race->form;
    ch->parts	= race->parts;

    // TODO: change this to allow for groups as well
    ITERATOR skit;
    SKILL_DATA *skill;
    iterator_start(&skit, race->skills);
    while((skill = (SKILL_DATA *)iterator_nextdata(&skit)))
    {
        skill_add(ch, skill);
    }
    iterator_stop(&skit);

    // TODO: Add size selection *IF* the race has a range.
    ch->size = race->min_size;

    send_to_char("\n\r{YWhat is your sex (M/F)?{x ", ch);
    d->connected = CON_GET_NEW_SEX;
}

void login_get_new_sex(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch;
    int i;
    long vector, *field;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    switch (argument[0])
    {
    case 'm': case 'M': ch->sex = ch->pcdata->true_sex = SEX_MALE; break;
    case 'f': case 'F': ch->sex = ch->pcdata->true_sex = SEX_FEMALE; break;
    default:
        if (argument[0] != '\0')
            send_to_char("{xThat's not a sex.\n\r", ch);

        send_to_char("\n\r{YWhat is your sex (M/F)?{x ", ch);
        return;
    }

    SET_BIT(ch->act[0], PLR_NO_CHALLENGE);

    group_add(ch, "global skills", false);

    // Set them as an adventurer
    add_class_level(ch, gcl_adventurer, 1);
    ch->pcdata->current_class = get_class_level(ch, gcl_adventurer);

    /* Make it so no notes appear*/
    ch->pcdata->last_note = current_time;
    ch->pcdata->last_idea = current_time;
    ch->pcdata->last_penalty = current_time;
    ch->pcdata->last_news = current_time;
    ch->pcdata->last_changes = current_time;
    ch->pcdata->last_ready_check = 0;

    // Save new character to the account if applicable
    if (d->account) {
        account_add_character(d->account, ch);
        save_account(d->account);
    }
    
    send_to_char("\n\r{YPress ENTER to begin your journey, adventurer!{W\n\r", ch);

    /* Set up default toggles*/
    for (i = 0; pc_set_table[i].name != NULL; i++)
    {
        if (pc_set_table[i].default_state == SETTING_ON && get_staff_rank(ch) >= pc_set_table[i].min_rank)
        {
            if (pc_set_table[i].vector != 0)
            {
                vector = pc_set_table[i].vector;
                field = &ch->act[0];
            }
            else if (pc_set_table[i].vector2 != 0)
            {
                vector = pc_set_table[i].vector2;
                field = &ch->act[1];
            }
            else if (pc_set_table[i].vector_comm != 0)
            {
                vector = pc_set_table[i].vector_comm;
                field = &ch->comm;
            }
            else
                continue;

            if (pc_set_table[i].inverted)
            {
                REMOVE_BIT(*field, vector);
            }
            else
            {
                SET_BIT(*field, vector);
            }
        }
    }

    ch->tot_level = 0;

    d->connected = CON_READ_MOTD;
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
    extern bool fBootstrap;

    while (ISSPACE(*argument))
        argument++;

    ch = d->character;
    
    // First, make sure we have a valid character
    if (!ch) {
        bug("login_read_motd: null character", 0);
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
        
        // Free the temporary character
        free_char(ch);
        
        // Set as playing
        d->connected = CON_PLAYING;

        // Send reconnection message and place in room
        send_to_char("Reconnecting. Type replay to see missed tells.\n\r", existing);
        
        // Place character back in their room if they had one
        if (old_room) {
            char_to_room(existing, old_room);
            act("$n has reconnected.", existing, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        } else if (room_index_temple) {
            char_to_room(existing, room_index_temple);
            act("$n has reconnected.", existing, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        }
        
        // Log the reconnection
        sprintf(buf, "%s@%s reconnected.", existing->name, d->host);
        log_string(buf);
        wiznet("$N has relinked.", existing, NULL, WIZ_LINKS, 0, 0);
        
        // Update connection tracking
        connection_add(d);
        
        // Update protocol
        MXPSendTag(d, "<VERSION>");
        
        return;
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
    
    d->connected = CON_PLAYING;
    
    // Reset character stats
    reset_char(ch);

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
        
        // Handle bootstrapping if needed
        if (fBootstrap) {
            // Bootstrap code...
        } else {
            moved_to_room = true;
            // Safely place in starting room
            if (room_index_school) {
                char_to_room(ch, room_index_school);
                do_function(ch, &do_changes, "catchup");
                SET_BIT(ch->comm, COMM_NO_OOC);
                SET_BIT(ch->comm, COMM_NO_FLAMING);
                send_to_char("\n\r", ch);
                
                // Announce new player
                for (d2 = descriptor_list; d2 != NULL; d2 = d2->next) {
                    if (d2->connected == CON_PLAYING && d2->character != ch &&
                        !IS_SET(d2->character->comm, COMM_NOANNOUNCE)) {
                        act("{MThe Town Crier Announces 'All welcome $N, a new adventurer to Sentience!'{x",
                            d2->character, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
                    }
                }
                ch->tot_level = 1;
            }
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
                plogf("nanny.c, join_world(): Transferring char to VRoom");
                char_to_vroom(ch, ch->in_wilds, ch->at_wilds_x, ch->at_wilds_y);
            } else {
                plogf("nanny.c, join_world(): Previous VRoom invalid. Relocating to Temple");
                ch->in_wilds = NULL;
                ch->at_wilds_x = -1;
                ch->at_wilds_y = -1;
                if (room_index_temple)
                    char_to_room(ch, room_index_temple);
            }
        } else {
            if (IS_IMMORTAL(ch)) {
                if (room_index_chat)
                    char_to_room(ch, room_index_chat);
            } else {
                if (room_index_temple)
                    char_to_room(ch, room_index_temple);
            }
        }
    }

    // Announce entry to game
    act("$$n has entered the game.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    MXPSendTag(d, "<VERSION>");
    
    // Normal login notifications to other players
    for (d2 = descriptor_list; d2 != NULL; d2 = d2->next) {
        if (d2->connected == CON_PLAYING && !IS_IMMORTAL(d->character) &&
            d2->character != ch && IS_SET(d2->character->comm, COMM_NOTIFY))
            act("{B$$N has entered the game.{x", d2->character, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    }

    // Handle church-related logic
    if (ch->church != NULL) {
        if ((ch->alignment < 0 && ch->church->alignment == CHURCH_GOOD) ||
            (ch->alignment > 0 && ch->church->alignment == CHURCH_EVIL)) {
            act("{YAs you enter Sentience, you feel your church's faith has been changed.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
            act("{YYou feel your psychic link to $T being severed.{x", ch, NULL, NULL, NULL, NULL, NULL, ch->church->name, TO_CHAR);
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
    do_function(ch, &do_look, "auto");
    do_function(ch, &do_unread, "");
    
    // LOGIN TRIGGER
    script_login(ch);
}

void login_get_email(DESCRIPTOR_DATA *d, char *argument)
{
	CHAR_DATA *ch;


	while (ISSPACE(*argument))
		argument++;

	ch = d->character;

    if (argument[0] == '\0') {
        send_to_char("Enter your e-mail address: ", ch);
        return;
    }

    if (strlen(argument) < 5 || str_infix("@", argument)) {
        send_to_char("\n\rInvalid e-mail address. Enter your e-mail address: ", ch);
        return;
    }

    ch->pcdata->email = str_dup(argument);

    ch->pcdata->email_verified = false;
    free_string(ch->pcdata->email_verification_code);
    ch->pcdata->email_verification_code = str_dup("");
    ch->pcdata->email_verification_time = 0;

    /* New char, continue with the char creation process */
    if (ch->tot_level == 0) {
        write_to_buffer(d, "\n\rWould you like ascii colour (Y/N)? ", 0);
        d->connected = CON_GET_ASCII;
    } else { /* Old char, send them on their merry way */
        write_to_buffer(d, "\n\rYour e-mail address has been saved.\n\r\n\r[Hit Return to continue]\n\r", 0);
        if (IS_IMMORTAL(ch))
            d->connected = CON_READ_IMOTD;
        else
            d->connected = CON_READ_MOTD;
    }
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
        display_account_menu(d);
        return;
    }
    
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
        write_to_buffer(d, "Character name: ", 0);
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
        write_to_buffer(d, "Character name: ", 0);
        free_char(d->character);
        d->character = NULL;
        return;
    }
    
    // Initialize character with account password
    if (d->account && ch->pcdata) {
        if (ch->pcdata->pwd) {
            free_string(ch->pcdata->pwd);
        }
        ch->pcdata->pwd = str_dup(d->account->passwd);
        ch->pcdata->pwd_vers = d->account->passwd_version;
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
        display_account_menu(d);
        return;
    }
    
    // Check if the character is already linked to this account
    iterator_start(&it, acct->characters);
    while ((ch_entry = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!str_cmp(ch_entry->name, argument)) {
            write_to_buffer(d, "That character is already linked to your account.\n\r", 0);
            iterator_stop(&it);
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
        display_account_menu(d);
        return;
    }
    
    // Check if character is already linked to an account
    if (!IS_NULLSTR(ch->pcdata->account_name)) {
        if (str_cmp(ch->pcdata->account_name, acct->username)) {
            write_to_buffer(d, "That character is already linked to a different account.\n\r", 0);
            free_char(ch);
            display_account_menu(d);
            return;
        } else {
            write_to_buffer(d, "That character is already linked to your account.\n\r", 0);
            free_char(ch);
            display_account_menu(d);
            return;
        }
    }
    
    // Store character name temporarily for the linking process
    d->character = ch;
    
    write_to_buffer(d, "Enter the character's password: ", 0);
    ProtocolNoEcho(d, true);
    d->connected = CON_LINK_CHARACTER_PASSWORD;
}

void login_link_character_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    // Safety check - if character is NULL, return to account menu
    if (ch == NULL) {
        write_to_buffer(d, "Error with character data. Returning to account menu.\n\r", 0);
        display_account_menu(d);
        d->connected = CON_ACCOUNT_MENU;
        return;
    }
    
    write_to_buffer(d, "\n\r", 2);
    
    // Check password
    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd) && 
        strcmp(crypt(argument, ch->pcdata->pwd), ch->pcdata->pwd) && 
        strcmp(argument, ch->pcdata->pwd)) {
        
        write_to_buffer(d, "Incorrect password.\n\r", 0);
        ProtocolNoEcho(d, false);
        free_char(ch);
        d->character = NULL;
        display_account_menu(d);
        return;
    }
    
    // Check if character has MFA enabled - more thorough check
    if (ch->pcdata != NULL && 
        !IS_NULLSTR(ch->pcdata->mfa_key) && 
        ch->pcdata->mfa_enabled) {
        
        write_to_buffer(d, "This character has MFA enabled. Please enter the MFA code: ", 0);
        ProtocolNoEcho(d, true);
        d->connected = CON_LINK_CHARACTER_MFA;
        return;
    }
    
    // Link the character to the account
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
    
    // Save changes - THIS IS WHERE THE SEGFAULT HAPPENS
    // We need to save the character BEFORE freeing it
    
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
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

void display_character_menu(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    char buf[MAX_STRING_LENGTH];
    char label[50], value[100];
    d->mfa_verified = false;
    
    write_to_buffer(d, "\n\r{B=={W[ {YCHARACTER MENU {W]{B=={x\n\r\n\r", 0);
    
    // Format character information using pad_string for consistent alignment
    sprintf(label, "{CCharacter:{x");
    sprintf(value, "{W%s{x", ch->name);
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);

    CLASS_LEVEL *cl = get_class_level(ch, NULL);
    
    sprintf(label, "{CLevel:{x");
    sprintf(value, "{G%d (%d){x", 
            cl->level > 0 ? cl->level : ch->tot_level,
            ch->tot_level);
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);
    
    sprintf(label, "{CRace:{x");
    sprintf(value, "{G%s{x", ch->race ? ch->race->name : "Unknown");
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);
    
    sprintf(label, "{CClass:{x");
    sprintf(value, "{G%s{x", 
            (ch->pcdata && ch->pcdata->current_class && IS_VALID(ch->pcdata->current_class->clazz)) ? 
            ch->pcdata->current_class->clazz->name : "Adventurer");
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);

// Location information
if (!IS_NULLSTR(ch->pcdata->last_area) || (!IS_NULLSTR(ch->pcdata->last_region) && str_cmp(ch->pcdata->last_region, "default region"))) {
    sprintf(label, "{CLocation:{x");
    
    // Format the location string using our helper function
    const char *loc_str = format_location_string(
        ch->pcdata->last_area,
        ch->pcdata->last_region
    );
    sprintf(value, "{Y%s{x", loc_str);
    
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);
}
    
    // Created date
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
    
    // Display email if set
    if (!IS_NULLSTR(ch->pcdata->email)) {
if (game_settings.require_email_verification && !ch->pcdata->email_verified && !IS_NULLSTR(ch->pcdata->pending_email)) {
    sprintf(label, "{CEmail:{x");
    sprintf(value, "{C%s{x (pending: {Y%s{x)", IS_NULLSTR(ch->pcdata->email) ? "Not set" : ch->pcdata->email, ch->pcdata->pending_email);
    sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
    write_to_buffer(d, buf, 0);
} else if (game_settings.require_email_verification && !ch->pcdata->email_verified && IS_NULLSTR(ch->pcdata->pending_email)) {
    sprintf(label, "{CEmail:{x");
    sprintf(value, "{YPending verification{x");
    sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
    write_to_buffer(d, buf, 0);
} else if (!IS_NULLSTR(ch->pcdata->email)) {
    sprintf(label, "{CEmail:{x");
    sprintf(value, "{C%s{x", ch->pcdata->email);
    sprintf(buf, "%s%s %s\n\r", label, pad_string(label, 20, NULL, " "), value);
    write_to_buffer(d, buf, 0);
}
    }
    
    // Display last login if available
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

    // Display character-password status
    sprintf(label, "{CCharacter password:{x");
    sprintf(value, "%s", 
            ch->pcdata->account_pwd_override && ch->pcdata->pwd_vers == 1 ? 
            "{GSet{x" : "{RNot set{x");
    
    sprintf(buf, "%s%s %s\n\r", 
            label, 
            pad_string(label, 20, NULL, " "), 
            value);
    write_to_buffer(d, buf, 0);

    // Display MFA status if it's configured
    if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
        sprintf(label, "{CMFA Status:{x");
        sprintf(value, "%s", ch->pcdata->mfa_enabled ? "{GEnabled{x" : "{RDisabled{x");
        
        sprintf(buf, "%s%s %s\n\r", 
                label, 
                pad_string(label, 20, NULL, " "), 
                value);
        write_to_buffer(d, buf, 0);
    }
    
    // Add a divider using pad_string before menu options
    write_to_buffer(d, "\n\r", 0);
    sprintf(buf, "{C%s{x\n\r", pad_string("", 60, NULL, "="));
    write_to_buffer(d, buf, 0);
    
    write_to_buffer(d, "{GL{x) Log in with this character\n\r", 0);
    write_to_buffer(d, "{GP{x) Set character password\n\r", 0);
    
    write_to_buffer(d, "{GM{x) MFA settings\n\r", 0);
write_to_buffer(d, "{GE{x) Change character email address\n\r", 0);
if (game_settings.require_email_verification && !ch->pcdata->email_verified) {
    write_to_buffer(d, "{GV{x) Verify character email address\n\r", 0);
    if (!IS_NULLSTR(ch->pcdata->pending_email))
        write_to_buffer(d, "{GR{x) Resend verification email\n\r", 0);
}
    
    if (can_unlink_characters(d->account))
        write_to_buffer(d, "{GU{x) Unlink this character\n\r", 0);
    
    write_to_buffer(d, "{GD{x) Delete this character\n\r", 0);
    write_to_buffer(d, "{GB{x) Back to account menu\n\r\n\r", 0);

}



void login_character_password(DESCRIPTOR_DATA *d, char *argument)
{
    switch (toupper(argument[0])) {
        case 'Y':
            write_to_buffer(d, "\n\rEnter new character password: ", 0);
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
    
    write_to_buffer(d, "\n\r", 2);
    
    if (!acceptablePassword(d, argument))
        return;
        
    free_string(ch->pcdata->pwd);
    ch->pcdata->pwd = str_dup(sha256_crypt(argument));
    ch->pcdata->pwd_vers = 1;
    ch->pcdata->account_pwd_override = true;
    
    save_char_obj(ch);
    
    ProtocolNoEcho(d, false);
    write_to_buffer(d, "\n\rCharacter password set.\n\r", 0);
    
    display_character_menu(d);
    d->connected = CON_CHARACTER_MENU;
}

void login_character_mfa_toggle(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    switch (toupper(argument[0])) {
        case 'Y':
            ch->pcdata->mfa_enabled = !ch->pcdata->mfa_enabled;
            
            if (ch->pcdata->mfa_enabled)
                write_to_buffer(d, "\n\rMFA has been enabled for this character.\n\r", 0);
            else
                write_to_buffer(d, "\n\rMFA has been disabled for this character.\n\r", 0);
                
            save_char_obj(ch);
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
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

void setup_character_mfa(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    
    if (!ch || IS_NPC(ch))
        return;
        
    if (!setup_mfa_for_char(ch, !IS_NULLSTR(ch->pcdata->email))) {
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
    
    sprintf(buf, "Character '%s' has been deleted.\n\r", ch->name);
    write_to_buffer(d, buf, 0);
    
    // Remove from account
    account_remove_character(acct, ch->name);
    
    // TODO: Delete character file if desired
    // For now, just unlink from account
    
    // Clean up
    free_char(ch);
    d->character = NULL;
    
    // Return to account menu
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

// When selecting a character from the account menu, check for additional character security
void login_character_menu(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;

    switch (toupper(argument[0])) {
        case 'L': // Log in with this character
            if (check_playing(d, ch->name))
                return;
            if (check_reconnect(d, ch->name, true))
                return;
            if (IS_IMMORTAL(ch) && game_settings.require_2fa_staff &&
                (IS_NULLSTR(ch->pcdata->mfa_key) || !ch->pcdata->mfa_enabled) &&
                (IS_NULLSTR(d->account->mfa_key) || !d->account->mfa_enabled)) {
                write_to_buffer(d, "\n\r{RERROR: Staff characters require MFA to be enabled.{x\n\r", 0);
                write_to_buffer(d, "You must enable MFA on either your account or this character before logging in.\n\r", 0);
                display_character_menu(d);
                return;
            }
            if (ch->pcdata->account_pwd_override) {
                write_to_buffer(d, "\n\rThis character requires an additional password.\n\r", 0);
                write_to_buffer(d, "Enter character password: ", 0);
                ProtocolNoEcho(d, true);
                d->connected = CON_GET_CHAR_PASSWORD;
                return;
            }
            if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled) {
                write_to_buffer(d, "\n\rThis character has MFA enabled.\n\r", 0);
                ProtocolNoEcho(d, true);
                d->connected = CON_GET_CHAR_MFA;
                return;
            }
            if (IS_IMMORTAL(ch) && !IS_NULLSTR(d->account->mfa_key) &&
                d->account->mfa_enabled &&
                (IS_NULLSTR(ch->pcdata->mfa_key) || !ch->pcdata->mfa_enabled)) {
                write_to_buffer(d, "\n\rThis is a staff character. Account MFA verification required.\n\r", 0);
                d->connected = CON_GET_ACCOUNT_MFA_FOR_CHAR;
                return;
            }
            proceed_to_game(d);
            break;

        case 'P': // Set/change character password
            if (ch->pcdata->account_pwd_override) {
                write_to_buffer(d, "This character already has a unique password.\n\r", 0);
                write_to_buffer(d, "Do you want to change it? (Y/N): ", 0);
            } else {
                write_to_buffer(d, "Setting a character-specific password will require\n\r", 0);
                write_to_buffer(d, "an additional password when logging in as this character.\n\r", 0);
                write_to_buffer(d, "Do you want to set a password for this character? (Y/N): ", 0);
            }
            d->connected = CON_CHARACTER_PASSWORD;
            break;

        case 'M': // MFA settings
            display_character_mfa_menu(d, "");
            break;

        case 'U': // Unlink (if allowed)
            if (!can_unlink_characters(d->account)) {
                write_to_buffer(d, "Unlinking is not available for your account.\n\r", 0);
                display_character_menu(d);
                return;
            }
            // Implement unlink logic here (prompt for confirmation, etc.)
            write_to_buffer(d, "Unlinking not yet implemented.\n\r", 0);
            display_character_menu(d);
            break;

        case 'D': // Delete character
            if (IS_IMMORTAL(ch)) {
                write_to_buffer(d, "\n\r{RStaff characters cannot be deleted through the menu.{x\n\r", 0);
                write_to_buffer(d, "Please contact an administrator for assistance.\n\r", 0);
                display_character_menu(d);
                return;
            }
            if (ch->pcdata->account_pwd_override) {
                write_to_buffer(d, "\n\r{RThis character requires password verification before deletion.{x\n\r", 0);
                write_to_buffer(d, "Enter character password: ", 0);
                ProtocolNoEcho(d, true);
                d->connected = CON_VERIFY_DELETE_PASSWORD;
                return;
            }
            if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled) {
                write_to_buffer(d, "\n\r{RThis character has MFA enabled. Please authenticate:{x\n\r", 0);
                write_to_buffer(d, "Enter MFA code: ", 0);
                ProtocolNoEcho(d,true);
                d->connected = CON_VERIFY_DELETE_MFA;
                return;
            }
            write_to_buffer(d, "\n\r{RWARNING: This will permanently delete this character!{x\n\r", 0);
            write_to_buffer(d, "Type 'DELETE' to confirm: ", 0);
            d->connected = CON_CONFIRM_DELETE_CHARACTER;
            break;
case 'E': // Change email address
    write_to_buffer(d, "\n\rCurrent email: ", 0);
    write_to_buffer(d, IS_NULLSTR(ch->pcdata->email) ? "Not set\n\r" : ch->pcdata->email, 0);
    write_to_buffer(d, "\n\rEnter new email address: ", 0);
    d->connected = CON_CHANGE_CHARACTER_EMAIL;
    return;
case 'V':
    if (ch->pcdata->email_verified) {
        write_to_buffer(d, "\n\rYour character email is already verified.\n\r", 0);
        display_character_menu(d);
        return;
    }
    if (IS_NULLSTR(ch->pcdata->pending_email)) {
        write_to_buffer(d, "No pending email to verify. Change your email address first.\n\r", 0);
        display_character_menu(d);
        return;
    }
    write_to_buffer(d, "Enter the code sent to your email: ", 0);
    d->connected = CON_VERIFY_CHARACTER_EMAIL_CHANGE;
    return;
case 'R':
    resend_character_verification_code(d);
    display_character_menu(d);
    return;

        case 'B': // Back to account menu
            free_char(d->character);
            d->character = NULL;
            display_account_menu(d);
            d->connected = CON_ACCOUNT_MENU;
            break;

        default:
            write_to_buffer(d, "Invalid choice.\n\r", 0);
            display_character_menu(d);
            break;
    }
}

void login_get_account_mfa_for_char(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    CHAR_DATA *ch = d->character;
    
    if (!check_account_mfa(acct, argument) && !check_account_recovery_code(acct, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        // Return to character menu
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    // Check if this is a reconnection 
    if (d->reconnecting) {
        reconnect_char(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

// Add the new connection states for character-specific authentication
void login_get_char_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    write_to_buffer(d, "\n\r", 2);
    ProtocolNoEcho(d, false);
    
    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd) && 
        strcmp(crypt(argument, ch->pcdata->pwd), ch->pcdata->pwd) && 
        strcmp(argument, ch->pcdata->pwd)) {
        
        write_to_buffer(d, "Incorrect character password.\n\r", 0);
        // Return to character menu
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // If the character has MFA enabled, prompt for that next
    if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled) {
        ProtocolNoEcho(d, true);
        d->connected = CON_GET_CHAR_MFA;
        return;
    }
    
    // Check for d->reconnecting flag.
    if (d->reconnecting) {
        // This is a genuine reconnect - handle accordingly
        reconnect_char(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

void login_get_char_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    if (!check_char_mfa(ch, argument) && !check_recovery_code(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        // Return to character menu
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    ProtocolNoEcho(d, false);
    
    // Check for d->reconnecting flag.
    if (d->reconnecting) {
        // This is a genuine reconnect - handle accordingly
        reconnect_char(d);
    } else {
        // Normal login flow for a fresh connection
        proceed_to_game(d);
    }
}

// Helper function to proceed to the game after authentication
void proceed_to_game(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    
    // Add to loaded character lists
    if (!list_haslink(loaded_chars, ch))
        list_appendlink(loaded_chars, ch);
    
    // Mark that they're no longer reconnecting (clear any existing state)
    ch->timer = 0;
    
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
        
    if (!setup_mfa_for_account(d, !IS_NULLSTR(acct->email))) {
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

    if (!check_char_mfa(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code. MFA setup has been aborted.\n\r", 0);
        free_string(ch->pcdata->mfa_key);
        ch->pcdata->mfa_key = str_dup("");
        ch->pcdata->mfa_enabled = false;
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }

    ch->pcdata->mfa_enabled = true;
    save_char_obj(ch);

    write_to_buffer(d, "MFA has been successfully enabled for this character.\n\r", 0);

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
    if (check_account_mfa(acct, argument)) {
        // Promote the pending key!
        free_string(acct->mfa_key);
        acct->mfa_key = str_dup(acct->mfa_pending_key);
        acct->mfa_enabled = true;
        acct->mfa_pending = false;
        free_string(acct->mfa_pending_key);
        acct->mfa_pending_key = str_dup("");
        generate_recovery_codes(acct->recovery_codes, acct->recovery_used, MFA_RECOVERY_CODES);
        save_account(acct);

        d->mfa_verified = true;

        write_to_buffer(d, "MFA setup complete and enabled for your account.\n\r", 0);
        display_account_mfa_menu(d, "");
    } else {
        write_to_buffer(d, "Invalid MFA code. Please try again: ", 0);
    }
}

void login_get_account_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (!check_account_mfa(acct, argument) && !check_account_recovery_code(acct, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        d->login_attempts++;
        
        if (d->login_attempts >= game_settings.max_login_attempts) {
            write_to_buffer(d, "Too many login attempts. Goodbye.\n\r", 0);
            close_socket(d);
            return;
        }
        
        write_to_buffer(d, "Please enter your MFA code: ", 0);
        return;
    }
    
    // Log the successful connection
    ProtocolNoEcho(d, false);
    sprintf(log_buf, "Account %s@%s has connected.", acct->username, d->host);
    log_string(log_buf);

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
    
    if (argument[0] == '\0')
    {
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }
    
    // Check if the new password is the same as the old one
    if (!IS_NULLSTR(acct->old_passwd) && 
        !strcmp(sha256_crypt(argument), acct->old_passwd))
    {
        write_to_buffer(d, "Password must be DIFFERENT from your current password!\n\r", 0);
        write_to_buffer(d, "Password: ", 0);
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    if (!acceptablePassword(d, argument))
        return;

    free_string(acct->passwd);
    acct->passwd = str_dup(sha256_crypt(argument));
    write_to_buffer(d, "\n\rPlease retype new password: ", 0);

    d->connected = CON_CONFIRM_ACCOUNT_PASSWORD_CHANGE;
}

void login_confirm_account_password_change(DESCRIPTOR_DATA *d, char *argument)
{
    ACCOUNT_DATA *acct = d->account;
    
    if (strcmp(sha256_crypt(argument), acct->passwd))
    {
        write_to_buffer(d, "Passwords don't match.\n\rPassword: ", 0);
        d->connected = CON_CHANGE_ACCOUNT_PASSWORD;
        return;
    }

    write_to_buffer(d, "\n\r\n\r{Y***{x {RThank you. Please remember to never give your password to anybody.{Y *** {x\n\r\n\r", 0);
    
    // Update password version if needed
    if (acct->passwd_version < 1) {
        acct->passwd_version = 1;
    }
    
    // Free old password if it exists
    if (acct->old_passwd) {
        free_string(acct->old_passwd);
        acct->old_passwd = NULL;
    }
    
    save_account(acct);
    ProtocolNoEcho(d, false);

    // Return to account menu
    display_account_menu(d);
    d->connected = CON_ACCOUNT_MENU;
}

void login_verify_delete_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    write_to_buffer(d, "\n\r", 2);
    ProtocolNoEcho(d, false);
    
    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd) && 
        strcmp(crypt(argument, ch->pcdata->pwd), ch->pcdata->pwd) && 
        strcmp(argument, ch->pcdata->pwd)) {
        
        write_to_buffer(d, "Incorrect character password.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // If the character also has MFA, we need to verify that too
    if (!IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->mfa_enabled) {
        write_to_buffer(d, "\n\r{RThis character has MFA enabled. Please authenticate:{x\n\r", 0);
        write_to_buffer(d, "Enter MFA code: ", 0);
        ProtocolNoEcho(d,true);
        d->connected = CON_VERIFY_DELETE_MFA;
        return;
    }
    
    // If no MFA, proceed to confirmation
    write_to_buffer(d, "\n\r{RWARNING: This will permanently delete this character!{x\n\r", 0);
    write_to_buffer(d, "Type 'DELETE' to confirm: ", 0);
    d->connected = CON_CONFIRM_DELETE_CHARACTER;
}

void login_verify_delete_mfa(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    
    if (!check_char_mfa(ch, argument)) {
        write_to_buffer(d, "Invalid MFA code.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    
    // MFA verified, proceed to final confirmation
    write_to_buffer(d, "\n\r{RWARNING: This will permanently delete this character!{x\n\r", 0);
    write_to_buffer(d, "Type 'DELETE' to confirm: ", 0);
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
            
            // Try to load the character
            if (load_char_obj(&temp_d, ch_entry->name)) {
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
        write_to_buffer(d, "Staff character name: ", 0);
        return;
    }

    // Check if character exists
    if (player_exists(argument)) {
        write_to_buffer(d, "That character already exists. Please choose another name.\n\r", 0);
        write_to_buffer(d, "Staff character name: ", 0);
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

    write_to_buffer(d, "Should this staff character require individual MFA? (Y/N): ", 0);
    d->connected = CON_STAFF_MFA_PROMPT;
}

void login_staff_mfa_prompt(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    switch (toupper(argument[0])) {
        case 'Y':
            d->creating_staff_character = true;
            setup_character_mfa(d);
            break;
        case 'N':
            // If game requires unique passwords for staff, prompt for password
            if (game_settings.require_uniq_pass_staff) {
                write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
                ProtocolNoEcho(d, true);
                d->connected = CON_STAFF_PASSWORD;
            } else {
                // Use account password
                if (!ch->pcdata) ch->pcdata = new_pcdata();
                if (!d->account->passwd || IS_NULLSTR(d->account->passwd)) {
                    write_to_buffer(d, "Account password is not set. Cannot use as staff password.\n\r", 0);
                    write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
                    ProtocolNoEcho(d, true);
                    d->connected = CON_STAFF_PASSWORD;
                    break;
                }
                free_string(ch->pcdata->pwd);
                ch->pcdata->pwd = str_dup(d->account->passwd);
                ch->pcdata->pwd_vers = d->account->passwd_version;
                ch->pcdata->account_pwd_override = false;
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
    if (!acceptablePassword(d, argument))
        return;

    // Password must not match account password
    if (!strcmp(sha256_crypt(argument), d->account->passwd)) {
        write_to_buffer(d, "Password must be different from your account password.\n\r", 0);
        write_to_buffer(d, "Enter a unique password for this staff character: ", 0);
        return;
    }

    free_string(ch->pcdata->pwd);
    ch->pcdata->pwd = str_dup(sha256_crypt(argument));
    ch->pcdata->pwd_vers = 1;
    ch->pcdata->account_pwd_override = true;

    write_to_buffer(d, "Please retype password: ", 0);
    d->connected = CON_CONFIRM_STAFF_PASSWORD;
}

void login_confirm_staff_password(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    if (strcmp(sha256_crypt(argument), ch->pcdata->pwd)) {
        write_to_buffer(d, "Passwords don't match.\n\rEnter a unique password for this staff character: ", 0);
        d->connected = CON_STAFF_PASSWORD;
        return;
    }
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
    bool mfa_enabled = ch->pcdata->mfa_enabled;
    bool mfa_pending = ch->pcdata->mfa_pending;
    bool has_secret = !IS_NULLSTR(ch->pcdata->mfa_pending_key) || !IS_NULLSTR(ch->pcdata->mfa_key);
    bool has_recovery = has_recovery_codes((const char **)ch->pcdata->recovery_codes, MFA_RECOVERY_CODES);
    bool has_email = !IS_NULLSTR(ch->pcdata->email);
    char buf[MAX_STRING_LENGTH];

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
    switch (toupper(argument[0])) {
        case '1': // Start MFA setup
        {
            char key[MIL];
            char qr_url[MIL];
            char *secret = generate_totp_key(key, sizeof(key));
            free_string(ch->pcdata->mfa_pending_key);
            ch->pcdata->mfa_pending_key = str_dup(secret);
            ch->pcdata->mfa_pending = true;
            generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, secret);
            display_qr_code(d, qr_url);
            write_to_buffer(d, "\n\rMFA setup started. Use your authenticator app to scan the QR code or enter the secret above.\n\r", 0);
            write_to_buffer(d, "You must confirm MFA setup before it is enabled.\n\r", 0);
            display_character_mfa_menu(d, "");
        }
break;
        case '2': // Confirm MFA setup
            if (!ch->pcdata->mfa_pending) {
                write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
                display_character_mfa_menu(d, "");
                return;
            }
            d->connected = CON_CHARACTER_MFA_CONFIRM;
            break;
        case '3': // Email QR code
            if (IS_NULLSTR(ch->pcdata->email)) {
                write_to_buffer(d, "No email address set for this character.\n\r", 0);
            } else {
                // Email QR code (implement send_qr_email_for_char)
                send_qr_email_for_char(ch, ch->pcdata->email, ch->pcdata->mfa_pending_key ? ch->pcdata->mfa_pending_key : ch->pcdata->mfa_key);
                write_to_buffer(d, "QR code emailed.\n\r", 0);
            }
            display_character_mfa_menu(d, "");
            break;
        case '4': // Disable MFA
            write_to_buffer(d, "Are you sure you want to disable MFA? (Y/N): ", 0);
            d->connected = CON_CHARACTER_MFA_DISABLE_CONFIRM;
            break;
        case '5': // Show recovery codes
            display_recovery_codes(d, ch);
            display_character_mfa_menu(d, "");
            break;
        case '6': // Email recovery codes
            if (IS_NULLSTR(ch->pcdata->email)) {
                write_to_buffer(d, "No email address set for this character.\n\r", 0);
            } else {
                // Email recovery codes (implement send_recovery_codes_email_for_char)
                send_recovery_codes_email_for_char(ch, ch->pcdata->email);
                write_to_buffer(d, "Recovery codes emailed.\n\r", 0);
            }
            display_character_mfa_menu(d, "");
            break;
        case '7': // Regenerate recovery codes
            generate_recovery_codes(ch->pcdata->recovery_codes, ch->pcdata->recovery_used, MFA_RECOVERY_CODES);
            save_char_obj(ch);
            write_to_buffer(d, "Recovery codes regenerated.\n\r", 0);
            display_recovery_codes(d, ch);
            display_character_mfa_menu(d, "");
            break;
        case '8': // Display QR code in terminal
            {
                char qr_url[MIL];
                if (!IS_NULLSTR(ch->pcdata->mfa_pending_key))
                    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, ch->pcdata->mfa_pending_key);
                else if (!IS_NULLSTR(ch->pcdata->mfa_key))
                    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, ch->pcdata->mfa_key);
                else {
                    write_to_buffer(d, "No MFA secret available to display a QR code.\n\r", 0);
                    display_character_mfa_menu(d, "");
                    break;
                }
                display_qr_code(d, qr_url);
                display_character_mfa_menu(d, "");
                break;
            }
        case 'B':
            display_character_menu(d);
            d->connected = CON_CHARACTER_MENU;
            break;
        default:
            write_to_buffer(d, "Invalid choice.\n\r", 0);
            display_character_mfa_menu(d, "");
            break;
    }
}

void login_character_mfa_confirm(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    if (!ch->pcdata->mfa_pending || IS_NULLSTR(ch->pcdata->mfa_pending_key)) {
        write_to_buffer(d, "No MFA setup in progress.\n\r", 0);
        display_character_mfa_menu(d, "");
        return;
    }
    // Use the pending key for validation!
    if (!validate_totp_code(ch->pcdata->mfa_pending_key, argument)) {
        write_to_buffer(d, "Invalid MFA code. Please try again: ", 0);
        d->connected = CON_CHARACTER_MFA_CONFIRM;
        return;
    }
    // Promote pending key to active, enable MFA
    free_string(ch->pcdata->mfa_key);
    ch->pcdata->mfa_key = str_dup(ch->pcdata->mfa_pending_key);
    ch->pcdata->mfa_enabled = true;
    ch->pcdata->mfa_pending = false;
    free_string(ch->pcdata->mfa_pending_key);
    ch->pcdata->mfa_pending_key = str_dup("");
    // Generate recovery codes
    generate_recovery_codes(ch->pcdata->recovery_codes, ch->pcdata->recovery_used, MFA_RECOVERY_CODES);
    save_char_obj(ch);

    d->mfa_verified = true;

    write_to_buffer(d, "\n\rMFA enabled! Here are your recovery codes (save them!):\n\r", 0);
    display_recovery_codes(d, ch);
    display_character_mfa_menu(d, "");
}

void login_character_mfa_disable_confirm(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    switch (toupper(argument[0])) {
        case 'Y':
            ch->pcdata->mfa_enabled = false;
            save_char_obj(ch);
            write_to_buffer(d, "MFA disabled for this character.\n\r", 0);
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



void login_character_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument) {
    CHAR_DATA *ch = d->character;
    if (check_char_mfa(ch, argument) || check_recovery_code(ch, argument)) {
        d->mfa_verified = true;
        display_character_mfa_menu(d, "");
    } else {
        write_to_buffer(d, "Invalid MFA or recovery code. Try again: ", 0);
    }
}






// Helper function to format location string for display
char* format_location_string(const char* area_name, const char* region_name) {
    static char loc_buf[100];
    
    if (IS_NULLSTR(area_name) && IS_NULLSTR(region_name)) {
        strcpy(loc_buf, "(Unknown)");
        return loc_buf;
    }
    
    // If there's an area but no region, or if the region is "default region"
    if (!IS_NULLSTR(area_name) && (IS_NULLSTR(region_name) || !str_cmp(region_name, "default region"))) {
        sprintf(loc_buf, "%s", area_name);
        return loc_buf;
    }
    
    // If there's a region but no area
    if (IS_NULLSTR(area_name) && !IS_NULLSTR(region_name) && str_cmp(region_name, "default region")) {
        sprintf(loc_buf, "%s", region_name);
        return loc_buf;
    }
    
    // If both area and non-default region are present
    if (!IS_NULLSTR(area_name) && !IS_NULLSTR(region_name) && str_cmp(region_name, "default region")) {
        sprintf(loc_buf, "%s - %s", area_name, region_name);
        return loc_buf;
    }
    
    // Fallback
    strcpy(loc_buf, area_name ? area_name : "(Unknown)");
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
    if (IS_NULLSTR(ch->pcdata->pending_email)) {
        write_to_buffer(d, "No pending email to verify.\n\r", 0);
        return;
    }
    if ((current_time - ch->pcdata->email_verification_last_sent) < (15 * 60)) {
        write_to_buffer(d, "You must wait before resending the verification email.\n\r", 0);
        return;
    }
    char code[16];
    if (IS_NULLSTR(ch->pcdata->email_verification_code)) {
        generate_reset_code(code, 15);
        free_string(ch->pcdata->email_verification_code);
        ch->pcdata->email_verification_code = str_dup(code);
    }
    ch->pcdata->email_verification_time = current_time;
    ch->pcdata->email_verification_last_sent = current_time;
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", ch->pcdata->email_verification_code);
    send_email_async_ex(ch, ch->desc->account, ch->pcdata->pending_email, "Sentience: Verify Your New Character Email", body, NULL, NULL);
    save_char_obj(ch);
    write_to_buffer(d, "Verification email resent.\n\r", 0);
}

void login_verify_character_email_change(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;
    if (IS_NULLSTR(ch->pcdata->pending_email) || IS_NULLSTR(ch->pcdata->email_verification_code)) {
        write_to_buffer(d, "No email change is pending.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    // Expiry check (72 hours)
    if ((current_time - ch->pcdata->email_verification_time) > (72 * 3600)) {
        write_to_buffer(d, "Verification code expired. Please start the process again.\n\r", 0);
        free_string(ch->pcdata->pending_email);
        ch->pcdata->pending_email = str_dup("");
        free_string(ch->pcdata->email_verification_code);
        ch->pcdata->email_verification_code = str_dup("");
        ch->pcdata->email_verification_time = 0;
        save_char_obj(ch);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    if (!str_cmp(argument, ch->pcdata->email_verification_code)) {
        free_string(ch->pcdata->email);
        ch->pcdata->email = str_dup(ch->pcdata->pending_email);
        ch->pcdata->email_verified = true;
        free_string(ch->pcdata->pending_email);
        ch->pcdata->pending_email = str_dup("");
        free_string(ch->pcdata->email_verification_code);
        ch->pcdata->email_verification_code = str_dup("");
        ch->pcdata->email_verification_time = 0;
        ch->pcdata->email_verification_last_sent = 0;
        save_char_obj(ch);
        write_to_buffer(d, "\n\rYour character's email address has been updated and verified.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
    } else {
        write_to_buffer(d, "Invalid code. Try again: ", 0);
    }
}

void login_change_character_email(DESCRIPTOR_DATA *d, char *argument)
{
    CHAR_DATA *ch = d->character;

    if (argument[0] == '\0' || !strstr(argument, "@") || !strstr(argument, ".")) {
        write_to_buffer(d, "That's not a valid email address.\n\r", 0);
        write_to_buffer(d, "Enter your e-mail address (or 'cancel' to cancel): ", 0);
        return;
    }
    if (!str_cmp(argument, "cancel")) {
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    if (!game_settings.require_email_verification) {
        // Immediate update
        free_string(ch->pcdata->email);
        ch->pcdata->email = str_dup(argument);
        ch->pcdata->email_verified = true;
        write_to_buffer(d, "\n\rEmail address updated.\n\r", 0);
        save_char_obj(ch);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    // Verification required
    if (!IS_NULLSTR(ch->pcdata->pending_email) && !str_cmp(argument, ch->pcdata->pending_email)) {
        write_to_buffer(d, "That email is already pending verification.\n\r", 0);
        display_character_menu(d);
        d->connected = CON_CHARACTER_MENU;
        return;
    }
    free_string(ch->pcdata->pending_email);
    ch->pcdata->pending_email = str_dup(argument);
    ch->pcdata->email_verified = false;
    // Generate code and set time
    char code[16];
    generate_reset_code(code, 15);
    free_string(ch->pcdata->email_verification_code);
    ch->pcdata->email_verification_code = str_dup(code);
    ch->pcdata->email_verification_time = current_time;
    ch->pcdata->email_verification_last_sent = current_time;
    // Notify old email
    if (!IS_NULLSTR(ch->pcdata->email)) {
        send_email_async_ex(ch, d->account, ch->pcdata->email, "Sentience: Email Change Requested",
            "A request was made to change your character email. If this was not you, contact support.", NULL, NULL);
    }
    // Send code to new email
    char body[256];
    sprintf(body, "Your verification code is: %s\nThis code will expire in 72 hours.", code);
    send_email_async_ex(ch, NULL, ch->pcdata->pending_email, "Sentience: Verify Your New Email Address", body, NULL, NULL);
    save_char_obj(ch);
    write_to_buffer(d, "\n\rA verification code has been sent to your new email address.\n\r", 0);
    write_to_buffer(d, "Enter the code to confirm your new email: ", 0);
    d->connected = CON_VERIFY_CHARACTER_EMAIL_CHANGE;
}

bool is_reconnecting(CHAR_DATA *ch)
{
    if (!ch) return false;
    
    // Skip checking freshly created characters not yet in loaded_chars
    if (ch->tot_level == 0 && !ch->pcdata->last_login) 
        return false;
    
    // Debug output for tracing
    char debug_buf[MAX_STRING_LENGTH];
    sprintf(debug_buf, "[DEBUG] is_reconnecting checking: %s", ch->name);
    log_string(debug_buf);
    
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
            
            sprintf(debug_buf, "[DEBUG] Found reconnect match: %s (%p), desc: %s", 
                    real_existing->name, 
                    (void*)real_existing,
                    real_existing->desc ? "connected" : "linkdead");
            log_string(debug_buf);
            
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);
    
    // If we're here, we didn't find a matching character
    log_string("[DEBUG] No reconnect match found");
    return false;
}

void nanny(DESCRIPTOR_DATA *d, char *argument)
{


	while (ISSPACE(*argument))
		argument++;

	switch (d->connected) {
	default:
		bug("Nanny: bad d->connected %d.", d->connected);
		connection_remove(d);
		close_socket(d);
		return;

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

	case CON_GET_NEW_PASSWORD:
        login_get_new_passwd(d, argument);
		break;

	case CON_CONFIRM_NEW_PASSWORD:
        login_confirm_new_passwd(d, argument);
		break;

	case CON_GET_ASCII:
        login_get_ascii(d, argument);
		break;

	case CON_GET_ALIGNMENT:
        login_get_alignment(d, argument);
		break;

	case CON_GET_NEW_RACE:
        login_get_new_race(d, argument);
		break;

	case CON_GET_NEW_SEX:
        login_get_new_sex(d, argument);
		break;

#if 0
	case CON_GET_NEW_CLASS:
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
			break;
		}

		if (argument[0] == '\0') {
			send_to_char(classes, ch);
			break;
		}

		if ((iClass = class_lookup(argument)) == -1)
		{
			send_to_char("{xThat's not a class.\n\r", ch);
			send_to_char(classes, ch);
			break;
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
		break;

	case CON_GET_SUB_CLASS:
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

		sprintf(log_buf, "%s@%s new player.", ch->name, d->host);
		log_string(log_buf);

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
		ch->pcdata->last_ready_check = 0;

		send_to_char("\n\r{YPress ENTER to begin your journey, adventurer!{W\n\r", ch);
		buf[0] = '\0';

		/* Set up default toggles*/
		for (i = 0; pc_set_table[i].name != NULL; i++)
		{
			if (pc_set_table[i].default_state == SETTING_ON && ch->tot_level >= pc_set_table[i].min_level)
			{
				if (pc_set_table[i].vector != 0)
				{
					vector = pc_set_table[i].vector;
					field = &ch->act[0];
				}
				else if (pc_set_table[i].vector2 != 0)
				{
					vector = pc_set_table[i].vector2;
					field = &ch->act[1];
				}
				else if (pc_set_table[i].vector_comm != 0)
				{
					vector = pc_set_table[i].vector_comm;
					field = &ch->comm;
				}
				else
					continue;

				if (pc_set_table[i].inverted)
				{
					REMOVE_BIT(*field, vector);
				}
				else
				{
					SET_BIT(*field, vector);
				}
			}
		}

		ch->level     = 0;
		ch->tot_level = 0;

		SKILL_DATA *weapon_skill = NULL;

		/* Set up weapon skill*/
		switch (ch->pcdata->class_current)
		{
		case CLASS_MAGE:	weapon_skill = gsk_quarterstaff;	break;
		case CLASS_CLERIC:	weapon_skill = gsk_quarterstaff;	break;
		case CLASS_THIEF:	weapon_skill = gsk_dagger;		break;
		case CLASS_WARRIOR:	weapon_skill = gsk_sword;		break;
		default:
			bug("nanny: bad current class in weapon pick", 0);
			weapon_skill = gsk_sword;
			break;
		}

		SKILL_ENTRY *entry;
		TOKEN_DATA *token = NULL;
		if (weapon_skill->token)
			token = give_token(weapon_skill->token, ch, NULL, NULL);

		if (is_skill_spell(weapon_skill))
			entry = skill_entry_addskill(ch, weapon_skill, token, SKILLSRC_NORMAL, SKILL_AUTOMATIC);
		else
			entry = skill_entry_addspell(ch, weapon_skill, token, SKILLSRC_NORMAL, SKILL_AUTOMATIC);

		entry->rating = 50;
		d->connected = CON_READ_MOTD;
		break;
#endif

	case CON_READ_IMOTD:
        login_read_imotd(d, argument);
		break;

	case CON_READ_MOTD:
        login_read_motd(d, argument);
		break;

	/* Get the player's e-mail if it's not in the pfile already */
	case CON_GET_EMAIL:
        login_get_email(d, argument);
        break;

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
        
    case CON_CHARACTER_MFA_TOGGLE:
        login_character_mfa_toggle(d, argument);
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

	}
}