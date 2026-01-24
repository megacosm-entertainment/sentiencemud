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
 #include "secret.h"
 
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
 
 /*
 * Configure SSL context with appropriate settings
 */
bool configure_context(SSL_CTX *context)
{
    if (!context)
        return false;
        
    // Set the minimum TLS protocol version to TLS 1.2
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
    
    // Set up DH parameters using EVP interface
    EVP_PKEY *dhpkey = NULL;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);
    
    if (pctx) {
        OSSL_PARAM params[2];
        params[0] = OSSL_PARAM_construct_utf8_string("group", "ffdhe2048", 0);
        params[1] = OSSL_PARAM_construct_end();
        
        if (EVP_PKEY_paramgen_init(pctx) > 0 && 
            EVP_PKEY_CTX_set_params(pctx, params) > 0 &&
            EVP_PKEY_generate(pctx, &dhpkey) > 0) {
            
            // Transfer ownership of the EVP_PKEY to the SSL_CTX
            if (!SSL_CTX_set0_tmp_dh_pkey(context, dhpkey)) {
                EVP_PKEY_free(dhpkey);
                log_string("SSL error: Failed to set DH parameters");
            }
            // If successful, dhpkey is now owned by the context and shouldn't be freed
        }
        EVP_PKEY_CTX_free(pctx);
    }
    
    // Set up ECDH parameters
    SSL_CTX_set_ecdh_auto(context, 1);

    // Load certificate - try secrets (mount or env var) first, then file
    const char *ssl_cert_data = secret_get("SENTIENCE_SSL_CERT_DATA");
    const char *ssl_key_data = secret_get("SENTIENCE_SSL_KEY_DATA");

    if (ssl_cert_data && ssl_cert_data[0] != '\0') {
        // Load certificate chain from secrets (PEM format)
        // This handles full chains: server cert + intermediates + root
        const char *source = secret_using_mount() ? "secrets mount" : "environment variable";
        log_stringf("Override applied: SSL_CERT_DATA from %s", source);
        BIO *bio = BIO_new_mem_buf(ssl_cert_data, -1);
        if (!bio) {
            log_string("SSL error: Failed to create BIO for certificate data");
            return false;
        }

        // Read the first certificate (server certificate)
        X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        if (!cert) {
            BIO_free(bio);
            log_string("SSL error: Failed to parse server certificate from secrets");
            return false;
        }

        if (SSL_CTX_use_certificate(context, cert) <= 0) {
            X509_free(cert);
            BIO_free(bio);
            log_string("SSL error: Failed to use server certificate from secrets");
            return false;
        }
        X509_free(cert);

        // Read any additional certificates (intermediate/root CA certs) and add to chain
        int chain_count = 0;
        while ((cert = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
            // SSL_CTX_add_extra_chain_cert takes ownership of cert on success
            if (SSL_CTX_add_extra_chain_cert(context, cert) <= 0) {
                X509_free(cert);
                BIO_free(bio);
                log_string("SSL error: Failed to add chain certificate from secrets");
                return false;
            }
            chain_count++;
        }

        // Clear any "no more certificates" error from the loop
        ERR_clear_error();
        BIO_free(bio);

        if (chain_count > 0) {
            log_stringf("SSL: Loaded certificate chain with %d intermediate/CA cert(s) from secrets", chain_count);
        }
    } else {
        // Load certificate from file (original behavior)
        if (SSL_CTX_use_certificate_chain_file(context, game_settings.ssl_cert_path) <= 0) {
            log_string("SSL error: Failed to load certificate chain");
            return false;
        }
    }

    if (ssl_key_data && ssl_key_data[0] != '\0') {
        // Load private key from secrets (PEM format)
        const char *source = secret_using_mount() ? "secrets mount" : "environment variable";
        log_stringf("Override applied: SSL_KEY_DATA from %s", source);
        BIO *bio = BIO_new_mem_buf(ssl_key_data, -1);
        if (!bio) {
            log_string("SSL error: Failed to create BIO for private key data");
            return false;
        }

        EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_free(bio);

        if (!pkey) {
            log_string("SSL error: Failed to parse private key from secrets");
            return false;
        }

        if (SSL_CTX_use_PrivateKey(context, pkey) <= 0) {
            EVP_PKEY_free(pkey);
            log_string("SSL error: Failed to use private key from secrets");
            return false;
        }
        EVP_PKEY_free(pkey);
    } else {
        // Load private key from file (original behavior)
        if (SSL_CTX_use_PrivateKey_file(context, game_settings.ssl_key_path, SSL_FILETYPE_PEM) <= 0) {
            log_string("SSL error: Failed to load private key");
            return false;
        }
    }

    // Verify the private key matches the certificate
    if (!SSL_CTX_check_private_key(context)) {
        log_string("SSL error: Private key does not match certificate");
        return false;
    }
    
    // Set cipher list - enforce Perfect Forward Secrecy with modern AEAD ciphers
    // Prioritize TLS 1.3 ciphers, fall back to strong TLS 1.2 ciphersuites with PFS
    SSL_CTX_set_cipher_list(context,
        "TLS_AES_256_GCM_SHA384:"           // TLS 1.3
        "TLS_CHACHA20_POLY1305_SHA256:"     // TLS 1.3
        "TLS_AES_128_GCM_SHA256:"           // TLS 1.3
        "ECDHE-RSA-AES256-GCM-SHA384:"      // TLS 1.2 with PFS
        "ECDHE-RSA-AES128-GCM-SHA256:"      // TLS 1.2 with PFS
        "ECDHE-RSA-CHACHA20-POLY1305:"      // TLS 1.2 with PFS
        "DHE-RSA-AES256-GCM-SHA384:"        // TLS 1.2 with PFS
        "DHE-RSA-AES128-GCM-SHA256"         // TLS 1.2 with PFS
    );
    
    return true;
}

/*
 * Create a new SSL context with properly configured settings
 */
SSL_CTX *create_context(void)
{
    // Use TLS_server_method() for modern OpenSSL (1.1.0+)
    // This supports all TLS versions, controlled by min/max version settings
    SSL_CTX *new_ctx = SSL_CTX_new(TLS_server_method());

    if (!new_ctx) {
        log_string("SSL error: Failed to create SSL context");
        return NULL;
    }

    // Disable old, insecure protocols (redundant with min version, but explicit)
    SSL_CTX_set_options(new_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
    
    // Configure the context
    if (!configure_context(new_ctx)) {
        log_string("SSL error: Failed to configure SSL context");
        SSL_CTX_free(new_ctx);
        return NULL;
    }
    
    return new_ctx;
}