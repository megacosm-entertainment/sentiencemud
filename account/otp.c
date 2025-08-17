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
    
    // Get current and previous tokens (30-second window)
    current_code = get_totp_at(key, current_time, 6, 30, SHA1, &err);
    previous_code = get_totp_at(key, current_time - 30, 6, 30, SHA1, &err);
    
    if (!current_code || !previous_code) {
        return false;
    }
    
    // Store the tokens in our buffer
    sprintf(current_totp, "%s", current_code);
    sprintf(previous_totp, "%s", previous_code);
    
    
    // Check if the provided code matches either the current or previous token
    if (!str_cmp(code, current_totp) || !str_cmp(code, previous_totp))
        return true;
    
    // For even better user experience, also check for +30 seconds
    // (handling the case where user's clock is slightly ahead)
    char next_totp[MIL];
    char *next_code = get_totp_at(key, current_time + 30, 6, 30, SHA1, &err);
    if (next_code) {
        sprintf(next_totp, "%s", next_code);
        if (!str_cmp(code, next_totp))
            return true;
    }
        
    return false;
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
bool setup_mfa_for_char(CHAR_DATA *ch, bool send_email)
{
    char key[MIL];
    if (IS_NPC(ch))
        return false;

    // Generate a new key
    generate_totp_key(key, sizeof(key));

    // Save key to character
    free_string(ch->pcdata->mfa_key);
    ch->pcdata->mfa_key = str_dup(key);
    //ch->pcdata->qr_code_expiration = time(NULL) + 10 * 60;  // 10 minutes

    send_to_char("Your MFA key has been generated. Please save this key in a safe place.\n\r", ch);
    send_to_char("You will need this key to authenticate with MFA.\n\r", ch);
    send_to_char("Your key is: ", ch);
    send_to_char(ch->pcdata->mfa_key, ch);
    send_to_char("\n\r", ch);

    // Do NOT generate or display/email QR code here anymore!

    return true;
}

/*
 * Initialize MFA for an account
 * Works from the account menu and returns true if successful
 */
bool setup_mfa_for_account(DESCRIPTOR_DATA *d, bool send_email)
{
    ACCOUNT_DATA *acct;
    char key[MIL];
    
    if (!d || !(acct = d->account))
        return false;
        
    // Generate a new key
    generate_totp_key(key, sizeof(key));
    
    // Save key to account
    free_string(acct->mfa_key);
    acct->mfa_key = str_dup(key);
    //acct->qr_code_expiration = time(NULL) + 10 * 60;  // 10 minutes
    
    // Display key information
    write_to_buffer(d, "Your MFA key has been generated. Please save this key in a safe place.\n\r", 0);
    write_to_buffer(d, "You will need this key to authenticate with MFA.\n\r", 0);
    write_to_buffer(d, "Your key is: ", 0);
    write_to_buffer(d, acct->mfa_key, 0);
    write_to_buffer(d, "\n\r", 0);
    
    return true;
}

/*
 * Check if a TOTP code is valid for a character
 */
bool check_char_mfa(CHAR_DATA *ch, const char *code)
{
    if (IS_NPC(ch) || IS_NULLSTR(ch->pcdata->mfa_pending ? ch->pcdata->mfa_pending_key : ch->pcdata->mfa_key))
        return false;
        
    return validate_totp_code(ch->pcdata->mfa_key, code);
}

/*
 * Check if a TOTP code is valid for an account
 */
bool check_account_mfa(ACCOUNT_DATA *acct, const char *code) {
    const char *key = acct->mfa_pending ? acct->mfa_pending_key : acct->mfa_key;
    if (IS_NULLSTR(key)) return false;
    return validate_totp_code(key, code);
}

/*
 * In-game command to manage MFA
 */
void do_keygen(CHAR_DATA *ch, char *argument, const char *context)
{
    if (argument[0] == '\0') {
        send_to_char("Syntax: keygen <generate|clear|confirm>\n\r", ch);
        return;
    }

    if (IS_NPC(ch)) {
        send_to_char("NPCs cannot use MFA.\n\r", ch);
        return;
    }

    if (!strcmp(argument, "clear")) {
        if (IS_NULLSTR(ch->pcdata->mfa_key)) {
            send_to_char("You do not have an MFA key to clear.\n\r", ch);
            return;
        }
        free_string(ch->pcdata->mfa_key);
        ch->pcdata->mfa_key = str_dup("");
        ch->pcdata->mfa_enabled = false;
        //ch->pcdata->qr_code_expiration = 0;
        send_to_char("Your MFA key has been cleared.\n\r", ch);
        return;
    }
    else if (!strcmp(argument, "generate")) {
        if (IS_NULLSTR(ch->pcdata->mfa_key)) {
            setup_mfa_for_char(ch, true);
        }
        else {
            send_to_char("You already have an MFA key. If you would like to generate a new key, please run 'keygen clear'.\n\r", ch);
        }
    }
    else if (!str_prefix(argument, "confirm")) {
        if (IS_NULLSTR(ch->pcdata->mfa_key)) {
            send_to_char("You do not have an MFA key to validate.\n\r", ch);
            return;
        }
        


        // Prompt the user to enter the MFA code
        send_to_char("Please enter the code from your MFA app to authenticate your account.\n\r", ch);
        ch->pcdata->mfa_question = true;
        return;
    }
    else {
        send_to_char("Syntax: keygen <generate|clear|confirm>\n\r", ch);
        return;
    }
}

/*
 * Compatibility function
 */
void generate_key(CHAR_DATA *ch, char *key)
{
    cotp_error_t cotp_err;
    char *secret_key = base32_encode((uchar *)key, strlen(key)+1, &cotp_err);
    
    if (!IS_NULLSTR(ch->pcdata->mfa_key)) {
        free_string(ch->pcdata->mfa_key);
    }
    
    ch->pcdata->mfa_key = str_dup(secret_key);
    free(secret_key);
}

/*
 * Compatibility function
 */
bool check_mfa(CHAR_DATA *ch, char *argument)
{
    return check_char_mfa(ch, argument);
}

#include <unistd.h> // for unlink()

void send_qr_email_for_char(CHAR_DATA *ch, const char *email, const char *secret) {
    char qr_url[MIL], filename[256], subject[128], body[1024];
    generate_totp_qr_url(qr_url, sizeof(qr_url), ch->name, secret);

    QRcode *qrcode = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) return;
    snprintf(filename, sizeof(filename), "/tmp/%s-qrcode.png", ch->name);
    save_qr_code_as_png(qrcode, filename, 5);

    snprintf(subject, sizeof(subject), "Sentience MFA QR Code for %s", ch->name);
    snprintf(body, sizeof(body),
        "Dear %s,\n\n"
        "Scan the attached QR code or enter this secret in your authenticator app:\n\n"
        "Secret: %s\n\n"
        "If you did not request this, please contact staff.\n",
        ch->name, secret);

    // Use send_email_async_ex for better context
    send_email_async_ex(ch, NULL, (char *)email, subject, body, filename, "image/png");

    QRcode_free(qrcode);
    delayed_unlink(filename);
}

void send_recovery_codes_email_for_char(CHAR_DATA *ch, const char *email) {
    char subject[128], body[1024];
    snprintf(subject, sizeof(subject), "Sentience MFA Recovery Codes for %s", ch->name);
    strcpy(body, "Your recovery codes (each can be used once):\n\n");
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (ch->pcdata->recovery_codes[i]) {
            strncat(body, ch->pcdata->recovery_codes[i], sizeof(body) - strlen(body) - 1);
            if (ch->pcdata->recovery_used[i])
                strncat(body, " (used)", sizeof(body) - strlen(body) - 1);
            strncat(body, "\n", sizeof(body) - strlen(body) - 1); // One code per line
        }
    }
    strncat(body, "\nKeep these codes safe. Each can be used only once.\n", sizeof(body) - strlen(body) - 1);
    send_email_async_ex(ch, NULL, (char *)email, subject, body, NULL, NULL);
}

void send_qr_email_for_account(ACCOUNT_DATA *acct, const char *email, const char *secret) {
    char qr_url[MIL], filename[256], subject[128], body[1024];
    generate_totp_qr_url(qr_url, sizeof(qr_url), acct->username, secret);

    QRcode *qrcode = QRcode_encodeString(qr_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode) return;
    snprintf(filename, sizeof(filename), "/tmp/%s-qrcode.png", acct->username);
    save_qr_code_as_png(qrcode, filename, 5);

    snprintf(subject, sizeof(subject), "Sentience MFA QR Code for Account %s", acct->username);
    snprintf(body, sizeof(body),
        "Dear %s,\n\n"
        "Scan the attached QR code or enter this secret in your authenticator app:\n\n"
        "Secret: %s\n\n"
        "If you did not request this, please contact staff.\n",
        acct->username, secret);

    send_email_async_ex(NULL, acct, (char *)email, subject, body, filename, "image/png");

    QRcode_free(qrcode);
    delayed_unlink(filename);
}

void send_recovery_codes_email_for_account(ACCOUNT_DATA *acct, const char *email) {
    char subject[128], body[1024];
    snprintf(subject, sizeof(subject), "Sentience MFA Recovery Codes for Account %s", acct->username);
    strcpy(body, "Your recovery codes (each can be used once):\n\n");
    for (int i = 0; i < MFA_RECOVERY_CODES; ++i) {
        if (acct->recovery_codes[i]) {
            strncat(body, acct->recovery_codes[i], sizeof(body) - strlen(body) - 1);
            if (acct->recovery_used[i])
                strncat(body, " (used)", sizeof(body) - strlen(body) - 1);
            strncat(body, "\n", sizeof(body) - strlen(body) - 1); // One code per line
        }
    }
    strcat(body, "\nKeep these codes safe. Each can be used only once.\n");
    send_email_async_ex(NULL, acct, (char *)email, subject, body, NULL, NULL);
}