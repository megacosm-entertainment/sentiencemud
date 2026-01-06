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

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include "connection.h"
#include "merc.h"

/*
 * External SSL context (from comm.c)
 */
extern SSL_CTX *ctx;
extern int ssl_errors_since_reset;
extern time_t last_ssl_error;

/*
 * TLS connection structure
 */
typedef struct connection_tls {
    connection_t base;      // Must be first for casting
    SSL *ssl;               // OpenSSL connection object
} connection_tls_t;

/*
 * Forward declarations
 */
static bool tls_read(connection_t *conn, char *buf, int size, int *bytes_read);
static bool tls_write(connection_t *conn, const char *buf, int size, int *bytes_written);
static bool tls_process_handshake(connection_t *conn);
static void tls_close(connection_t *conn);
static void tls_free(connection_t *conn);
static bool tls_is_secure(connection_t *conn);
static const char* tls_get_protocol_name(connection_t *conn);

/*
 * Helper function to log SSL errors
 */
static void log_ssl_error(const char *context)
{
    BIO *bio = BIO_new(BIO_s_mem());
    if (bio) {
        ERR_print_errors(bio);

        char *buf;
        size_t len = BIO_get_mem_data(bio, &buf);

        if (len > 0) {
            char *error_str = (char*)malloc(len + 1);
            if (error_str) {
                memcpy(error_str, buf, len);
                error_str[len] = '\0';
                log_stringf("%s: %s", context, error_str);
                free(error_str);
            }
        }

        BIO_free(bio);
    }

    // Update circuit breaker
    ssl_errors_since_reset++;
    last_ssl_error = current_time;
}

/*
 * TLS virtual function table
 */
static connection_vtable_t tls_vtable = {
    .read = tls_read,
    .write = tls_write,
    .process_handshake = tls_process_handshake,
    .close = tls_close,
    .free = tls_free,
    .is_secure = tls_is_secure,
    .get_protocol_name = tls_get_protocol_name
};

/*
 * Create a new TLS connection
 */
connection_t* connection_tls_create(int fd, struct descriptor_data *desc)
{
    connection_tls_t *tls_conn;
    SSL *ssl;
    int ret;

    // Allocate connection structure
    tls_conn = (connection_tls_t*)calloc(1, sizeof(connection_tls_t));
    if (!tls_conn) {
        log_string("connection_tls_create: Out of memory");
        return NULL;
    }

    // Set socket to non-blocking
    if (!connection_set_nonblocking(fd)) {
        free(tls_conn);
        return NULL;
    }

    // Create SSL object
    ssl = SSL_new(ctx);
    if (!ssl) {
        log_ssl_error("connection_tls_create: SSL_new() failed");
        free(tls_conn);
        return NULL;
    }

    // Bind SSL to socket
    if (!SSL_set_fd(ssl, fd)) {
        log_ssl_error("connection_tls_create: SSL_set_fd() failed");
        SSL_free(ssl);
        free(tls_conn);
        return NULL;
    }

    // Initialize base connection
    tls_conn->base.vtable = &tls_vtable;
    tls_conn->base.type = CONN_TYPE_TLS;
    tls_conn->base.state = CONN_STATE_CONNECTING;  // Handshake needed
    tls_conn->base.fd = fd;
    tls_conn->base.descriptor = desc;
    tls_conn->base.proto_data = ssl;
    tls_conn->base.last_activity = current_time;
    tls_conn->base.handshake_in_progress = true;
    tls_conn->ssl = ssl;

    // Start handshake (non-blocking, may need multiple calls)
    // Note: For non-blocking sockets, SSL_accept() almost always needs multiple calls
    ret = SSL_accept(ssl);
    if (ret == 1) {
        // Handshake completed immediately (rare for non-blocking sockets)
        tls_conn->base.state = CONN_STATE_CONNECTED;
        tls_conn->base.handshake_in_progress = false;
        log_stringf("TLS handshake completed immediately (fd %d)", fd);
    } else {
        // Handshake not complete - this is normal for non-blocking sockets
        // Clear any SSL errors from the error queue (they'll be checked again in process_handshake)
        // "unexpected eof" and similar errors are common on initial accept with non-blocking I/O
        ERR_clear_error();

        // Handshake will continue asynchronously in game loop
        // If it's a real error, tls_process_handshake() will catch it after the timeout
    }

    return (connection_t*)tls_conn;
}

/*
 * Read from TLS connection
 */
static bool tls_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    connection_tls_t *tls_conn = (connection_tls_t*)conn;
    int nread;

    *bytes_read = 0;

    // Read from SSL
    nread = SSL_read(tls_conn->ssl, buf, size);

    if (nread > 0) {
        // Successfully read data
        *bytes_read = nread;
        conn->last_activity = current_time;
        return true;
    } else {
        int err = SSL_get_error(tls_conn->ssl, nread);

        switch (err) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // No data available yet (non-blocking)
                return true;

            case SSL_ERROR_ZERO_RETURN:
                // Clean SSL shutdown
                log_stringf("tls_read: SSL connection closed gracefully (fd %d)", conn->fd);
                return false;

            case SSL_ERROR_SYSCALL:
                if (errno == 0) {
                    // EOF
                    log_stringf("tls_read: Connection closed (fd %d)", conn->fd);
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    log_stringf("tls_read: Connection broken (fd %d): %s", conn->fd, strerror(errno));
                } else {
                    log_stringf("tls_read: SSL_ERROR_SYSCALL (fd %d): %s", conn->fd, strerror(errno));
                }
                return false;

            default:
                // SSL error
                log_ssl_error("tls_read: SSL_read() failed");
                return false;
        }
    }
}

/*
 * Write to TLS connection
 */
static bool tls_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    connection_tls_t *tls_conn = (connection_tls_t*)conn;
    int nwritten;

    *bytes_written = 0;

    // Write to SSL
    nwritten = SSL_write(tls_conn->ssl, buf, size);

    if (nwritten > 0) {
        // Successfully wrote data
        *bytes_written = nwritten;
        conn->last_activity = current_time;
        return true;
    } else {
        int err = SSL_get_error(tls_conn->ssl, nwritten);

        switch (err) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Socket not ready yet (non-blocking)
                return true;

            case SSL_ERROR_ZERO_RETURN:
                // Clean SSL shutdown
                log_stringf("tls_write: SSL connection closed gracefully (fd %d)", conn->fd);
                return false;

            case SSL_ERROR_SYSCALL:
                if (errno == 0) {
                    // EOF without errno - connection closed
                    log_stringf("tls_write: Connection closed (fd %d)", conn->fd);
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    log_stringf("tls_write: Connection broken (fd %d): %s", conn->fd, strerror(errno));
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Would block - return true to try again later
                    *bytes_written = 0;
                    return true;
                } else {
                    log_stringf("tls_write: SSL_ERROR_SYSCALL (fd %d): %s", conn->fd, strerror(errno));
                }
                return false;

            default:
                // SSL error
                log_ssl_error("tls_write: SSL_write() failed");
                return false;
        }
    }
}

/*
 * Process TLS handshake
 */
static bool tls_process_handshake(connection_t *conn)
{
    connection_tls_t *tls_conn = (connection_tls_t*)conn;
    int ret;

    if (!conn->handshake_in_progress)
        return true;  // Already complete

    // Continue handshake
    ret = SSL_accept(tls_conn->ssl);

    if (ret == 1) {
        // Handshake complete
        conn->state = CONN_STATE_CONNECTED;
        conn->handshake_in_progress = false;
        conn->last_activity = current_time;
        log_stringf("TLS handshake completed (fd %d)", conn->fd);
        return true;
    } else {
        int err = SSL_get_error(tls_conn->ssl, ret);

        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            // Still in progress - need more data
            return false;
        }

        if (err == SSL_ERROR_SYSCALL) {
            // Check errno to distinguish real errors from normal non-blocking behavior
            if (errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-blocking socket waiting for data - not an error
                return false;
            }
            // Real system error - fall through to error handling
        }

        // Real handshake error - mark connection as failed
        conn->state = CONN_STATE_CLOSED;
        log_ssl_error("tls_process_handshake: SSL_accept() failed");
        return false;
    }
}

/*
 * Close TLS connection
 */
static void tls_close(connection_t *conn)
{
    connection_tls_t *tls_conn = (connection_tls_t*)conn;

    if (conn->state == CONN_STATE_CLOSED)
        return;

    conn->state = CONN_STATE_CLOSING;

    // Only perform SSL shutdown if handshake completed
    if (!conn->handshake_in_progress && tls_conn->ssl) {
        // Check if socket is writable before attempting shutdown
        fd_set writefds;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout

        FD_ZERO(&writefds);
        FD_SET(conn->fd, &writefds);

        if (select(conn->fd + 1, NULL, &writefds, NULL, &timeout) > 0) {
            // Perform proper SSL shutdown
            int ret = SSL_shutdown(tls_conn->ssl);
            if (ret == 0) {
                // First call succeeded, complete bidirectional shutdown
                SSL_shutdown(tls_conn->ssl);
            }
        }
    }

    // Free SSL object
    if (tls_conn->ssl) {
        SSL_free(tls_conn->ssl);
        tls_conn->ssl = NULL;
        conn->proto_data = NULL;
    }

    // Close socket
    shutdown(conn->fd, SHUT_RDWR);
    close(conn->fd);

    conn->state = CONN_STATE_CLOSED;
}

/*
 * Free TLS connection
 */
static void tls_free(connection_t *conn)
{
    connection_tls_t *tls_conn = (connection_tls_t*)conn;

    // Ensure connection is closed
    if (conn->state != CONN_STATE_CLOSED)
        tls_close(conn);

    // Free the structure
    free(tls_conn);
}

/*
 * Check if TLS is secure (it is)
 */
static bool tls_is_secure(connection_t *conn)
{
    return true;
}

/*
 * Get protocol name
 */
static const char* tls_get_protocol_name(connection_t *conn)
{
    return "TLS";
}
