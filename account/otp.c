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
 
void do_keygen(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    if (argument[0] == '\0')
    {
        send_to_char("Syntax: keygen\n\r", ch);
        return;
    }

    if (IS_NPC(ch))
    {
        send_to_char("NPCs cannot use MFA.\n\r", ch);
        return;
    }

    if (!strcmp(argument, "clear"))
    {
        if (IS_NULLSTR(ch->pcdata->mfa_key))
        {
            send_to_char("You do not have an MFA key to clear.\n\r", ch);
            return;
        }
        free_string(ch->pcdata->mfa_key);
        ch->pcdata->mfa_key = str_dup("");
        send_to_char("Your MFA key has been cleared.\n\r", ch);
        return;
    }
    else if (!strcmp(argument, "generate"))
    {
        if (IS_NULLSTR(ch->pcdata->mfa_key))
        {
            char random_string[MIL];
            char otp_url[MIL];
            char filename[256];
            generate_reset_code(random_string, 16);
            sprintf(buf, "%s", random_string);
            generate_key(ch, random_string);
            send_to_char("Your MFA key has been generated. Please save this key in a safe place.\n\r", ch);
            send_to_char("You will need this key to authenticate your account with MFA.\n\r", ch);
            send_to_char("Your key is: ", ch);
            send_to_char(ch->pcdata->mfa_key, ch);
            send_to_char("\n\r", ch);

            sprintf(otp_url, "otpauth://totp/Sentience:%s?secret=%s&Issuer=SentienceMUD&digits=6", ch->name, ch->pcdata->mfa_key);
            send_to_char("You can add this key to your MFA app by scanning the following QR code:\n\r", ch);

            QRcode *qrcode = QRcode_encodeString(otp_url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (qrcode) {
                ch->pcdata->qr_code_expiration = time(NULL) + 10 * 60; // 10 minutes
                for (int i = -1; i <= qrcode->width; i++) {
                    for (int j = -1; j <= qrcode->width; j++) {
                        if (i >= 0 && i < qrcode->width && j >= 0 && j < qrcode->width && qrcode->data[i * qrcode->width + j] & 1) {
                            send_to_char("\033[40m  \033[0m", ch);
                        } else {
                        send_to_char("\033[47m  \033[0m", ch);
                        }
                    }
                    send_to_char("\n\r", ch);
                }
                sprintf(filename, "/tmp/%s-qrcode.png", ch->name);
                save_qr_code_as_png(qrcode, filename, 5);

                char email_body[1024];
                sprintf(email_body, "Dear %s,\n\nYour MFA key has been generated. Please save this key in a safe place.\n\nYou will need this key to authenticate your account with MFA.\n\nYour key is: %s\n\nYou can add this key to your MFA app by scanning the following QR code:\n\n", ch->name, ch->pcdata->mfa_key);
                send_email_async(ch, ch->pcdata->email, "Sentience MUD MFA Key", email_body, filename, "image/png");
                QRcode_free(qrcode);
            }
            else {
                send_to_char("Error generating QR code.\n\r", ch);
            }
        }
        else
        {
            send_to_char("You already have an MFA key. If you would like to generate a new key, please run 'keygen clear'.\n\r", ch);
        }
    }
    else if (!str_prefix(argument, "confirm"))
    {
        if (IS_NULLSTR(ch->pcdata->mfa_key) && ch->pcdata->qr_code_expiration == 0)
        {
            send_to_char("You do not have an MFA key to validate.\n\r", ch);
            return;
        }
        // Check if the QR code has expired
        if (time(NULL) > ch->pcdata->qr_code_expiration) {
            send_to_char("The QR code has expired. Please generate a new one.\n\r", ch);
            free_string(ch->pcdata->mfa_key);
            ch->pcdata->mfa_key = str_dup("");
            ch->pcdata->qr_code_expiration = 0;

            return;
        }

        // Prompt the user to enter the MFA code
        send_to_char("Please enter the code from your MFA app to authenticate your account.\n\r", ch);
        ch->pcdata->mfa_question = true;
        return;

        // Wait for the user to enter the code (this part depends on your input handling system)
        // For example, you might have a separate function to handle user input
        // Here, we'll assume the code is provided in the `argument` variable
    }
    else
    {
        send_to_char("Syntax: keygen <generate|clear|confirm>\n\r", ch);
        return;
    }
}
 
 void generate_key(CHAR_DATA *ch, char *key)
 {
 //    char random_string[MIL];
 //    generate_reset_code(random_string, 64);
     cotp_error_t cotp_err;
 
     char *secret_key = base32_encode((uchar *)key, strlen(key)+1, &cotp_err);
     if (!IS_NULLSTR(ch->pcdata->mfa_key))
     {
         free_string(ch->pcdata->mfa_key);
         ch->pcdata->mfa_key = str_dup("");
     }
     ch->pcdata->mfa_key = str_dup(secret_key);
     free_string(secret_key);
 }
 
 bool check_mfa(CHAR_DATA *ch, char *argument)
 {
     char *provided_code = argument;
     cotp_error_t err;
     char buf[MSL];
 
     char current_totp[MIL];
     char previous_totp[MIL];
     current_totp[0] = '\0';
     previous_totp[0] = '\0';
 
     time_t current_time = time(NULL);
 
 
     
     sprintf(current_totp, "%s", get_totp_at(ch->pcdata->mfa_key, current_time, 6, 30, SHA1, &err));
     sprintf(previous_totp, "%s", get_totp_at(ch->pcdata->mfa_key, current_time - 30, 6, 30, SHA1, &err));
 
     if (!str_cmp(provided_code, current_totp) || !str_cmp(provided_code, previous_totp))
     {
         sprintf(buf, "You have successfully authenticated your account with MFA.\n\r");
         return true;
     }
     else
     {
         sprintf(buf, "The code you provided is incorrect. Please try again.\n\r");
         send_to_char(buf,ch);
         return false;
     }
     return false;
 }