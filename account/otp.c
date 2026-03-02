 /***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Thanks to abaddon for proof-reading our comm.c and pointing out bugs.  *
 *  Any remaining bugs are, of course, our work, not his.  :)              *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*
 * This file contains all of the OS-dependent stuff:
 *   startup, signals, BSD sockets for tcp/ip, i/o, timing.
 *
 * The data flow for input is:
 *    Game_loop ---> Read_from_descriptor ---> Read
 *    Game_loop ---> Read_from_buffer
 *
 * The data flow for output is:
 *    Game_loop ---> Process_Output ---> Write_to_descriptor -> Write
 *
 * The OS-dependent functions are Read_from_descriptor and Write_to_descriptor.
 * -- Furey  26 Jan 1993
 */

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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <libpng/png.h>
/* VIZZWILDS - support for plogf() and printf_to_char() functions*/
#include <stdarg.h>
#include <cotp.h>
#include <qrencode.h>

#include "../strings.h"
#include "../merc.h"
#include "../interp.h"
#include "../recycle.h"
#include "../scripts.h"
#include "../tables.h"
#include "../wilds.h"
#include "../protocol.h"
#include "auth_sodium.h"
 
/*
 * Generate a new TOTP key for a user
 * This function works with either an account or character context
 * Returns the generated key
 */
char *generate_totp_key(char *buffer, size_t length)
{
    char random_string[MIL];
    cotp_error_t cotp_err;
    
    // Generate a random string
    generate_reset_code(random_string, 16);
    
    // Convert to Base32 for TOTP
    char *secret_key = base32_encode((uchar *)random_string, strlen(random_string)+1, &cotp_err);
    
    // Copy to the provided buffer
    if (buffer && length > 0) {
        strncpy(buffer, secret_key, length - 1);
        buffer[length - 1] = '\0';
    }
    
    free(secret_key);
    return buffer;
}

/*
 * Validate a TOTP code against a key
 * Returns true if the code is valid
 */
bool validate_totp_code(const char *key, const char *code)
{
    cotp_error_t err;
    time_t current_time = time(NULL);
    char current_totp[MIL];
    char previous_totp[MIL];
    char *current_code, *previous_code;
    bool has_secret_chars = false;

    if (!key || !code)
        return false;

    if (IS_NULLSTR(key) || IS_NULLSTR(code))
        return false;

    if (strlen(code) != 6)
        return false;

    for (const char *p = code; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return false;
    }
    
    // Check if the key is encrypted
    char *plaintext_key = NULL;
    bool is_encrypted = is_encrypted_key(key);
    
    if (is_encrypted) {
        // Decrypt the key before validation
        plaintext_key = decrypt_string_versioned(key);
        if (IS_NULLSTR(plaintext_key)) {
            if (plaintext_key)
                free_string(plaintext_key);
            return false;
        }
        key = plaintext_key;
    }

    if (!key || key[0] == '\0') {
        if (is_encrypted && plaintext_key)
            free_string(plaintext_key);
        return false;
    }

    // libcotp does not defensively handle bad secrets; reject malformed inputs here.
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        if (isspace(*p) || *p == '-')
            continue;

        if (!(isalpha(*p) || (*p >= '2' && *p <= '7') || *p == '=')) {
            if (is_encrypted && plaintext_key)
                free_string(plaintext_key);
            return false;
        }

        has_secret_chars = true;
    }

    if (!has_secret_chars) {
        if (is_encrypted && plaintext_key)
            free_string(plaintext_key);
        return false;
    }
    
    // Get current and previous tokens (30-second window)
    current_code = get_totp_at(key, current_time, 6, 30, SHA1, &err);
    previous_code = get_totp_at(key, current_time - 30, 6, 30, SHA1, &err);
    
    if (!current_code || !previous_code) {
        if (is_encrypted && plaintext_key)
            free_string(plaintext_key);
        return false;
    }
    
    // Store the tokens in our buffer
    snprintf(current_totp, sizeof(current_totp), "%s", current_code);
    snprintf(previous_totp, sizeof(previous_totp), "%s", previous_code);
    
    // Check if the provided code matches either the current or previous token
    bool valid = (!str_cmp(code, current_totp) || !str_cmp(code, previous_totp));
    
    // For even better user experience, also check for +30 seconds
    // (handling the case where user's clock is slightly ahead)
    if (!valid) {
        char next_totp[MIL];
        char *next_code = get_totp_at(key, current_time + 30, 6, 30, SHA1, &err);
        if (next_code) {
            snprintf(next_totp, sizeof(next_totp), "%s", next_code);
            valid = !str_cmp(code, next_totp);
        }
    }
    
    // Free decrypted key if we allocated it
    if (is_encrypted && plaintext_key)
        free_string(plaintext_key);
        
    return valid;
}

/**
 * migrate_otp_key - Migrate OTP key from v1 to v2 encryption
 *
 * Transparently upgrades OTP key from AES-CBC (v1) to authenticated
 * encryption (v2). Handles both account-level and character-level keys.
 *
 * @param acct       Account to migrate (required)
 * @param acct_char  Character to migrate (NULL for account-level)
 * @return           true if migration performed, false if not needed/error
 */
bool migrate_otp_key(ACCOUNT_DATA *acct, ACCOUNT_CHARACTER *acct_char)
{
    char **mfa_key_ptr;
    char **mfa_pending_ptr;
    const char *context;
    bool migrated = false;

    if (!acct) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "migrate_otp_key: NULL account");
        return false;
    }

    // Determine which keys to migrate (account or character)
    if (acct_char) {
        mfa_key_ptr = &acct_char->mfa_key;
        mfa_pending_ptr = &acct_char->mfa_pending_key;
        context = acct_char->name;
    } else {
        mfa_key_ptr = &acct->mfa_key;
        mfa_pending_ptr = &acct->mfa_pending_key;
        context = acct->username;
    }

    // Migrate active MFA key if needed
    if (!IS_NULLSTR(*mfa_key_ptr) && needs_encryption_upgrade(*mfa_key_ptr)) {
        char *plaintext = decrypt_string_versioned(*mfa_key_ptr);
        if (!IS_NULLSTR(plaintext)) {
            char *upgraded = encrypt_string_versioned(plaintext);
            if (upgraded) {
                free_string(*mfa_key_ptr);
                *mfa_key_ptr = str_dup(upgraded);
                free_string(upgraded);
                migrated = true;
                log_message_f(LOG_LEVEL_INFO, LOG_SECURITY,
                    "Migrated OTP key to v2 for %s", context);
            }
            free_string(plaintext);
        }
    }

    // Migrate pending MFA key if needed
    if (!IS_NULLSTR(*mfa_pending_ptr) && needs_encryption_upgrade(*mfa_pending_ptr)) {
        char *plaintext = decrypt_string_versioned(*mfa_pending_ptr);
        if (!IS_NULLSTR(plaintext)) {
            char *upgraded = encrypt_string_versioned(plaintext);
            if (upgraded) {
                free_string(*mfa_pending_ptr);
                *mfa_pending_ptr = str_dup(upgraded);
                free_string(upgraded);
                migrated = true;
                log_message_f(LOG_LEVEL_INFO, LOG_SECURITY,
                    "Migrated pending OTP key to v2 for %s", context);
            }
            free_string(plaintext);
        }
    }

    // Save account if any migration occurred
    if (migrated) {
        save_account(acct);
    }

    return migrated;
}

/*
 * Generate a QR code URL for TOTP setup
 * Works with either account or character
 */
char *generate_totp_qr_url(char *buffer, size_t length, const char *name, const char *key)
{
    // Create the otpauth URL
    snprintf(buffer, length,
        "otpauth://totp/Sentience:%s?secret=%s&Issuer=SentienceMUD&digits=6",
        name, key);
        
    return buffer;
}

/*
 * Display a QR code to a descriptor
 */
void display_qr_code(DESCRIPTOR_DATA *d, const char *url)
{
    QRcode *qrcode = QRcode_encodeString(url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) {
        write_to_buffer(d, "Error generating QR code.\n\r", 0);
        return;
    }
    
    // Display the QR code in the terminal
    for (int i = -1; i <= qrcode->width; i++) {
        for (int j = -1; j <= qrcode->width; j++) {
            if (i >= 0 && i < qrcode->width && j >= 0 && j < qrcode->width && 
                qrcode->data[i * qrcode->width + j] & 1) {
                write_to_buffer(d, "\033[40m  \033[0m", 0);
            } else {
                write_to_buffer(d, "\033[47m  \033[0m", 0);
            }
        }
        write_to_buffer(d, "\n\r", 0);
    }
    
    QRcode_free(qrcode);
}

/*
 * Save a QR code as a PNG file
 */
void save_qr_code_as_png(QRcode *qrcode, const char *filename, int scale)
{
    if (!qrcode)
        return;
        
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return;
        
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return;
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }
    
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }
    
    png_init_io(png_ptr, fp);
    
    int width = qrcode->width * scale;
    png_set_IHDR(png_ptr, info_ptr, width, width, 8, PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png_ptr, info_ptr);
    
    png_bytep *row_pointers = malloc(sizeof(png_bytep) * width);
    for (int i = 0; i < width; i++) {
        row_pointers[i] = malloc(width * 3);
    }
    
    for (int y = 0; y < qrcode->width; y++) {
        for (int x = 0; x < qrcode->width; x++) {
            unsigned char black = (qrcode->data[y * qrcode->width + x] & 1) ? 0 : 255;
            
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    int real_y = y * scale + sy;
                    int real_x = x * scale + sx;
                    row_pointers[real_y][real_x * 3] = black;
                    row_pointers[real_y][real_x * 3 + 1] = black;
                    row_pointers[real_y][real_x * 3 + 2] = black;
                }
            }
        }
    }
    
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    
    for (int i = 0; i < width; i++) {
        free(row_pointers[i]);
    }
    free(row_pointers);
    
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

/*
 * Initialize MFA for a character
 * Works in-game and returns true if successful
 */
bool setup_mfa_for_char(CHAR_DATA *ch, bool has_email)
{
    char key[MIL];
    char qr_url[MIL];
    ACCOUNT_DATA *acct = NULL;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = false;
    
    if (IS_NPC(ch))
        return false;

    // Generate a new key
    generate_totp_key(key, sizeof(key));

    // Check if we should use account_character data
    if (ch->desc && ch->desc->account) {
        acct = ch->desc->account;
        has_auth_data = get_character_auth_data(ch, acct, &acct_char);
        
        if (has_auth_data && acct_char) {
            // Store as pending key - requires confirmation before enabling
            // Encrypt the key before storing
            char *encrypted_key = encrypt_string_versioned(key);
            free_string(acct_char->mfa_pending_key);
            acct_char->mfa_pending_key = str_dup(encrypted_key);
            free_string(encrypted_key);
            save_account(acct);
        } else {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "setup_mfa_for_char: No account character entry found for %s", ch->name);
            return false;
        }
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "setup_mfa_for_char: No account found for %s", ch->name);
        return false;
    }

    // Generate QR code URL for display (using plaintext key)
    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, key);

    // Display setup information to the character
    if (ch->desc) {
        write_to_buffer(ch->desc, "\n\r{GMFA Setup Information{x\n\r", 0);
        write_to_buffer(ch->desc, "{C-------------------------------------{x\n\r", 0);
        
        // Display QR code in the terminal first
        write_to_buffer(ch->desc, "{WQR Code:{x\n\r", 0);
        display_qr_code(ch->desc, qr_url);
        write_to_buffer(ch->desc, "\n\r", 0);
        
        // Now display the secret key after the QR code (plaintext for user setup)
        write_to_buffer(ch->desc, "{WSecret Key: {G", 0);
        write_to_buffer(ch->desc, key, 0);
        write_to_buffer(ch->desc, "{x\n\r", 0);
        write_to_buffer(ch->desc, "{YPlease save this key in a secure location.{x\n\r\n\r", 0);
        
        write_to_buffer(ch->desc, "{YOnce you've set up your authenticator app, enter the code it generates to verify and enable MFA.{x\n\r", 0);
    } else {
        send_to_char("\n\r{GMFA Setup Information{x\n\r", ch);
        send_to_char("{C-------------------------------------{x\n\r", ch);
        send_to_char("{YYour MFA key has been generated.{x\n\r", ch);
        send_to_char("{WSecret Key: {G", ch);
        send_to_char(key, ch);
        send_to_char("{x\n\r", ch);
        send_to_char("{YPlease save this key in a secure location.{x\n\r\n\r", ch);
        send_to_char("{YOnce you've set up your authenticator app, enter the code to verify and enable MFA.{x\n\r", ch);
    }

    return true;
}

/*
 * Initialize MFA for an account
 * Works from the account menu and returns true if successful
 */
bool setup_mfa_for_account(DESCRIPTOR_DATA *d, bool has_email)
{
    ACCOUNT_DATA *acct;
    char key[MIL];
    char qr_url[MIL];
    
    if (!d || !(acct = d->account))
        return false;
        
    // Generate a new key
    generate_totp_key(key, sizeof(key));
    
    // Save key to account as pending - encrypt the key before storing
    char *encrypted_key = encrypt_string_versioned(key);
    free_string(acct->mfa_pending_key);
    acct->mfa_pending_key = str_dup(encrypted_key);
    free_string(encrypted_key);
    save_account(acct);
    
    // Generate QR code URL (using plaintext key)
    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, key);
    
    // Display MFA setup information
    write_to_buffer(d, "\n\r{GMFA Setup Information{x\n\r", 0);
    write_to_buffer(d, "{C-------------------------------------{x\n\r", 0);
    
    // First display QR code
    write_to_buffer(d, "{WQR Code:{x\n\r", 0);
    display_qr_code(d, qr_url);
    write_to_buffer(d, "\n\r", 0);
    
    // Then display the secret key after the QR code
    write_to_buffer(d, "{WSecret Key: {G", 0);
    write_to_buffer(d, key, 0);
    write_to_buffer(d, "{x\n\r", 0);
    write_to_buffer(d, "{YPlease save this key in a secure location.{x\n\r\n\r", 0);
    
    write_to_buffer(d, "{YOnce you've set up your authenticator app, enter the code it generates to verify and enable MFA.{x\n\r", 0);
    
    return true;
}

/*
 * Check if a TOTP code is valid for a character
 * Uses account_character data if available, falls back to pcdata
 */
bool check_char_mfa(CHAR_DATA *ch, const char *code)
{
    if (IS_NPC(ch))
        return false;
    
    ACCOUNT_DATA *acct = NULL;
    ACCOUNT_CHARACTER *acct_char = NULL;
    const char *encrypted_key = NULL;
    bool has_auth_data = false;
    
    // Try to get authentication data from account_character
    if (ch->desc && ch->desc->account) {
        acct = ch->desc->account;
        has_auth_data = get_character_auth_data(ch, acct, &acct_char);

        if (has_auth_data && acct_char) {
            // Migrate OTP key encryption if needed
            migrate_otp_key(acct, acct_char);


            // Use pending key if in setup mode, otherwise use active key
            encrypted_key = !IS_NULLSTR(acct_char->mfa_pending_key) ? 
                      acct_char->mfa_pending_key : acct_char->mfa_key;
            
            // Check for recovery code usage if we have an MFA key
            if (!IS_NULLSTR(encrypted_key)) {
                // Check TOTP code against the key (validate_totp_code handles decryption)
                bool valid = validate_totp_code(encrypted_key, code);
                
                // If code validates against pending key, activate it
                if (valid && !IS_NULLSTR(acct_char->mfa_pending_key)) {
                    free_string(acct_char->mfa_key);
                    acct_char->mfa_key = str_dup(acct_char->mfa_pending_key);
                    free_string(acct_char->mfa_pending_key);
                    acct_char->mfa_pending_key = str_dup("");
                    acct_char->mfa_enabled = true;
                    save_account(acct);
                }
                
                return valid;
            }
            
            // Check recovery codes as fallback
            for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
                if (!IS_NULLSTR(acct_char->recovery_codes[i]) &&
                    !acct_char->recovery_used[i] &&
                    verify_recovery_code_hash(acct_char->recovery_codes[i], code)) {
                    acct_char->recovery_used[i] = true;
                    save_account(acct);
                    return true;
                }
            }
        }
    }
    
    return false;
}

/**
 * check_account_mfa - Validate a TOTP code or recovery code for an account
 *
 * Checks the TOTP code against the account's active or pending MFA key.
 * If a pending key validates, it is activated (setup confirmation).
 * Falls back to recovery codes if TOTP validation fails.
 *
 * @param acct  Account to validate against
 * @param code  User-provided TOTP code or recovery code
 * @return      true if code is valid, false otherwise
 */
bool check_account_mfa(ACCOUNT_DATA *acct, const char *code) {
    const char *encrypted_key;

    if (!acct)
        return false;

    // Migrate OTP key encryption if needed
    migrate_otp_key(acct, NULL);

    // Use pending key if in setup mode, otherwise use active key
    encrypted_key = !IS_NULLSTR(acct->mfa_pending_key) ?
                    acct->mfa_pending_key : acct->mfa_key;

    if (!IS_NULLSTR(encrypted_key)) {
        // Check TOTP code against the key (validate_totp_code handles decryption)
        bool valid = validate_totp_code(encrypted_key, code);

        // If code validates against pending key, activate it
        if (valid && !IS_NULLSTR(acct->mfa_pending_key)) {
            free_string(acct->mfa_key);
            acct->mfa_key = str_dup(acct->mfa_pending_key);
            free_string(acct->mfa_pending_key);
            acct->mfa_pending_key = str_dup("");
            acct->mfa_enabled = true;
            acct->mfa_pending = false;
            save_account(acct);
        }

        if (valid)
            return true;
    }

    // Check recovery codes as fallback
    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        if (!IS_NULLSTR(acct->recovery_codes[i]) &&
            !acct->recovery_used[i] &&
            verify_recovery_code_hash(acct->recovery_codes[i], code)) {
            acct->recovery_used[i] = true;
            save_account(acct);
            return true;
        }
    }

    return false;
}

/*
 * Compatibility function
 */
bool check_mfa(CHAR_DATA *ch, char *argument)
{
    return check_char_mfa(ch, argument);
}

#include <unistd.h> // for unlink()

/*
 * Send QR code email for character
 * -- Updated to handle encrypted keys
 */
void send_qr_email_for_char(CHAR_DATA *ch, const char *email, const char *encrypted_secret) {
    ACCOUNT_DATA *acct = NULL;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = false;
    char qr_url[MIL], filename[256], subject[128], body[1024];
    
    // Get account data for email
    if (ch->desc && ch->desc->account) {
        acct = ch->desc->account;
        has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    }
    
    // If no account data, we can't send email
    if (!has_auth_data || !acct_char) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "send_qr_email_for_char: No account character entry found for %s", ch->name);
        return;
    }
    
    // Decrypt key if needed
    char *plaintext_secret;
    bool is_encrypted = is_encrypted_key(encrypted_secret);
    
    if (is_encrypted)
        plaintext_secret = decrypt_string_versioned(encrypted_secret);
    else
        plaintext_secret = str_dup(encrypted_secret);
    
    // Generate QR code
    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, plaintext_secret);

    QRcode *qrcode = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) {
        free_string(plaintext_secret);
        return;
    }
    
    snprintf(filename, sizeof(filename), "/tmp/%s-qrcode.png", ch->name);
    save_qr_code_as_png(qrcode, filename, 5);

    snprintf(subject, sizeof(subject), "Sentience MFA QR Code for %s", ch->name);
    snprintf(body, sizeof(body),
        "Scan the attached QR code or enter this secret in your authenticator app:\n"
        "Secret: %s\n\n"
        "If you did not request this, please contact staff.\n",
        plaintext_secret);

    // Use the email from account character data
    send_email_async_ex(ch, acct, acct_char->email, subject, body, filename, "image/png");

    QRcode_free(qrcode);
    free_string(plaintext_secret);
    delayed_unlink(filename);
}

void send_recovery_codes_email_for_char(CHAR_DATA *ch, const char *email) {
    ACCOUNT_DATA *acct = NULL;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_auth_data = false;
    char subject[128], body[1024];
    
    // Get account data for email and recovery codes
    if (ch->desc && ch->desc->account) {
        acct = ch->desc->account;
        has_auth_data = get_character_auth_data(ch, acct, &acct_char);
    }
    
    // If no account data, we can't send email
    if (!has_auth_data || !acct_char) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR, "send_recovery_codes_email_for_char: No account character entry found for %s", ch->name);
        return;
    }
    
    snprintf(subject, sizeof(subject), "Sentience MFA Recovery Codes for %s", ch->name);
    strcpy(body, "Your recovery codes (each can be used once):\n\n");
    
    // Use recovery codes from account character data
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (!IS_NULLSTR(acct_char->recovery_codes[i])) {
            strncat(body, acct_char->recovery_codes[i], sizeof(body) - strlen(body) - 1);
            if (acct_char->recovery_used[i])
                strncat(body, " (used)", sizeof(body) - strlen(body) - 1);
            strncat(body, "\n", sizeof(body) - strlen(body) - 1); // One code per line
        }
    }
    
    strncat(body, "\nKeep these codes safe. Each can be used only once.\n", sizeof(body) - strlen(body) - 1);
    
    // Use the email from account character data
    send_email_async_ex(ch, acct, acct_char->email, subject, body, NULL, NULL);
}

/*
 * Send QR code email for account
 * -- Updated to handle encrypted keys
 */
void send_qr_email_for_account(ACCOUNT_DATA *acct, const char *email, const char *encrypted_secret) {
    char qr_url[MIL], filename[256], subject[128], body[1024];
    
    // Decrypt key if needed
    char *plaintext_secret;
    bool is_encrypted = is_encrypted_key(encrypted_secret);
    
    if (is_encrypted)
        plaintext_secret = decrypt_string_versioned(encrypted_secret);
    else
        plaintext_secret = str_dup(encrypted_secret);
    
    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, plaintext_secret);

    QRcode *qrcode = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) {
        free_string(plaintext_secret);
        return;
    }
    
    snprintf(filename, sizeof(filename), "/tmp/%s-qrcode.png", acct->username);
    save_qr_code_as_png(qrcode, filename, 5);

    snprintf(subject, sizeof(subject), "Sentience MFA QR Code for Account %s", acct->username);
    snprintf(body, sizeof(body),
        "Scan the attached QR code or enter this secret in your authenticator app:\n"
        "Secret: %s\n\n"
        "If you did not request this, please contact staff.",
        plaintext_secret);

    send_email_async_ex(NULL, acct, (char *)email, subject, body, filename, "image/png");

    QRcode_free(qrcode);
    free_string(plaintext_secret);
    delayed_unlink(filename);
}

void send_recovery_codes_email_for_account(ACCOUNT_DATA *acct, const char *email) {
    char subject[128], body[1024];
    snprintf(subject, sizeof(subject), "Sentience MFA Recovery Codes for Account %s", acct->username);
    
    // Start with clear header
    strcpy(body, "Your recovery codes (each can be used once):");
    
    // Add TWO newlines after the header for visual separation
    strcat(body, "\n\n");
    
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (acct->recovery_codes[i]) {
            // Add each code on its own line
            strcat(body, acct->recovery_codes[i]);
            if (acct->recovery_used[i])
                strcat(body, " (used)");
            
            // Add a newline after EACH code
            strcat(body, "\n");
        }
    }
    
    // Add an empty line before the footer
    strcat(body, "\n");
    strcat(body, "Keep these codes safe. Each can be used only once.");
    
    send_email_async_ex(NULL, acct, (char *)email, subject, body, NULL, NULL);
}

/*
 * Display a plaintext MFA key to a descriptor or character
 * Safely decrypts an encrypted key before display
 */
void display_mfa_key(DESCRIPTOR_DATA *d, const char *encrypted_key)
{
    if (IS_NULLSTR(encrypted_key)) {
        if (d)
            write_to_buffer(d, "No MFA key is set.\n\r", 0);
        return;
    }

    // Check if the key needs decryption
    char *plaintext_key = NULL;
    bool is_encrypted = is_encrypted_key(encrypted_key);
    
    if (is_encrypted)
        plaintext_key = decrypt_string(encrypted_key);
    else
        plaintext_key = str_dup(encrypted_key);
    
    if (d) {
        write_to_buffer(d, "{WMFA Secret Key: {G", 0);
        write_to_buffer(d, plaintext_key, 0);
        write_to_buffer(d, "{x\n\r", 0);
        write_to_buffer(d, "{YMake sure to keep this key secure!{x\n\r", 0);
    }

    free_string(plaintext_key);
}

/*
 * Display MFA key for a character
 */
void display_acct_char_mfa_key(DESCRIPTOR_DATA *d, ACCOUNT_CHARACTER *acct_char)
{
    if (!d || !acct_char)
        return;
    
    const char *encrypted_key = !IS_NULLSTR(acct_char->mfa_key) ? 
                     acct_char->mfa_key : acct_char->mfa_pending_key;
    
    if (!IS_NULLSTR(encrypted_key)) {
        write_to_buffer(d, "\n\r{GMFA Key Information{x\n\r", 0);
        write_to_buffer(d, "{C-------------------------------------{x\n\r", 0);
        display_mfa_key(d, encrypted_key);
    } else {
        write_to_buffer(d, "No MFA key is set for this character.\n\r", 0);
    }
}

/*
 * Display MFA key for an account
 */
void display_account_mfa_key(DESCRIPTOR_DATA *d, ACCOUNT_DATA *acct)
{
    if (!d || !acct)
        return;
    
    const char *encrypted_key = !IS_NULLSTR(acct->mfa_key) ? 
                     acct->mfa_key : acct->mfa_pending_key;
    
    if (!IS_NULLSTR(encrypted_key)) {
        write_to_buffer(d, "\n\r{GMFA Key Information{x\n\r", 0);
        write_to_buffer(d, "{C-------------------------------------{x\n\r", 0);
        display_mfa_key(d, encrypted_key);
    } else {
        write_to_buffer(d, "No MFA key is set for this account.\n\r", 0);
    }
}