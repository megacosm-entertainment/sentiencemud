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
 
 /*
  * Socket and TCP/IP stuff.
  */
 #include <fcntl.h>
 #include <netdb.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include "telnet.h"
 
 
 // Global SSL context
 SSL_CTX *ctx;
 
 // Function to initialize SSL library
 void init_openssl_library(void)
 {
     SSL_load_error_strings();
     OpenSSL_add_ssl_algorithms();
 }
 
 // Function to create a new SSL context
 SSL_CTX* create_context(void)
 {
     const SSL_METHOD *method;
     SSL_CTX *ctx;
 
     method = SSLv23_server_method();
 
     ctx = SSL_CTX_new(method);
     if (!ctx) {
         perror("Unable to create SSL context");
         ERR_print_errors_fp(stderr);
         exit(EXIT_FAILURE);
     }
 
     return ctx;
 }
 
 // Function to configure SSL context
 void configure_context(SSL_CTX *ctx)
 {
     SSL_CTX_set_ecdh_auto(ctx, 1);
 
     /* Set the key and cert */
     if (SSL_CTX_use_certificate_file(ctx, game_settings.ssl_cert_path, SSL_FILETYPE_PEM) <= 0) {
         ERR_print_errors_fp(stderr);
         exit(EXIT_FAILURE);
     }
 
     if (SSL_CTX_use_PrivateKey_file(ctx, game_settings.ssl_key_path, SSL_FILETYPE_PEM) <= 0 ) {
         ERR_print_errors_fp(stderr);
         exit(EXIT_FAILURE);
     }
 }