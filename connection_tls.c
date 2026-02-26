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

/**
 * @file connection_tls.c
 * @brief TLS-encrypted connection implementation using OpenSSL
 *
 * Implements the connection abstraction interface for TLS-encrypted
 * connections. Uses OpenSSL's SSL_* API for encryption/decryption.
 *
 * Key features:
 *   - Non-blocking TLS handshake (SSL_accept may need multiple calls)
 *   - Graceful SSL shutdown with bidirectional close
 *   - Circuit breaker integration for SSL error tracking
 *   - Proper handling of WANT_READ/WANT_WRITE for non-blocking I/O
 *
 * Uses the global SSL_CTX from comm.c for creating SSL connections.
 */

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

/**
 * @name External References
 * Global SSL context and error tracking from comm.c
 * @{
 */
extern SSL_CTX *ctx;                  /**< Global SSL context for new connections */
extern int ssl_errors_since_reset;    /**< Error counter for circuit breaker */
extern time_t last_ssl_error;         /**< Timestamp of last SSL error */
/** @} */

/**
 * @struct connection_tls
 * @brief TLS connection implementation structure
 *
 * Extends base connection with OpenSSL SSL object for encryption.
 */
typedef struct connection_tls {
    connection_t base;  /**< Base connection (must be first for casting) */
    SSL *ssl;           /**< OpenSSL connection object */
} connection_tls_t;

/* Forward declarations for vtable functions */
static bool tls_read(connection_t *conn, char *buf, int size, int *bytes_read);
static bool tls_write(connection_t *conn, const char *buf, int size, int *bytes_written);
static bool tls_process_handshake(connection_t *conn);
static void tls_close(connection_t *conn);
static void tls_free(connection_t *conn);
static bool tls_is_secure(connection_t *conn);
static const char* tls_get_protocol_name(connection_t *conn);

/**
 * log_ssl_error - Log OpenSSL error messages with context
 *
 * Extracts error messages from OpenSSL's error queue using BIO and
 * logs them with the provided context string. Also updates the SSL
 * circuit breaker counters.
 *
 * @param context  Descriptive context string for the error
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

/**
 * @brief Virtual function table for TLS connections
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

/**
 * connection_tls_create - Create a new TLS connection
 *
 * Factory function to create a TLS-encrypted connection from an accepted
 * socket. Creates an SSL object from the global context and initiates
 * the TLS handshake (non-blocking).
 *
 * The handshake typically requires multiple calls to tls_process_handshake()
 * to complete. Connection state starts as CONN_STATE_CONNECTING until
 * handshake completes.
 *
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with this connection
 * @return      New connection_t pointer, or NULL on failure
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

/**
 * tls_read - Read decrypted data from TLS connection
 *
 * Reads data through the SSL layer, automatically decrypting it.
 * Handles SSL-specific return codes:
 *   - SSL_ERROR_WANT_READ/WRITE: Non-blocking, try again later
 *   - SSL_ERROR_ZERO_RETURN: Clean SSL shutdown by peer
 *   - SSL_ERROR_SYSCALL: System error, check errno
 *
 * @param conn        The TLS connection to read from
 * @param buf         Buffer to store decrypted data
 * @param size        Maximum bytes to read
 * @param bytes_read  Output: actual bytes read (0 on would-block)
 * @return            true on success or would-block, false on error/disconnect
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

/**
 * tls_write - Write data to TLS connection with encryption
 *
 * Writes data through the SSL layer, automatically encrypting it.
 * Handles SSL-specific return codes similarly to tls_read().
 *
 * @param conn           The TLS connection to write to
 * @param buf            Data to encrypt and send
 * @param size           Bytes to write
 * @param bytes_written  Output: actual bytes written (0 on would-block)
 * @return               true on success or would-block, false on error/disconnect
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

/**
 * tls_process_handshake - Continue TLS handshake processing
 *
 * Called repeatedly when the socket is readable/writable until handshake
 * completes. TLS handshakes require multiple round-trips, so this function
 * typically needs to be called several times.
 *
 * Returns true when handshake is complete (connection ready for I/O).
 * Returns false if still in progress or if a fatal error occurred.
 *
 * @param conn  The TLS connection with pending handshake
 * @return      true if handshake complete, false if in progress or error
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

/**
 * tls_close - Close TLS connection with proper SSL shutdown
 *
 * Performs graceful TLS connection closure:
 *   1. Skip SSL shutdown if handshake never completed
 *   2. Wait briefly for socket to be writable
 *   3. Perform bidirectional SSL_shutdown() (send close_notify)
 *   4. Free SSL object
 *   5. Close underlying socket
 *
 * The socket writability check prevents blocking on dead connections.
 *
 * @param conn  The TLS connection to close
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

/**
 * tls_free - Free TLS connection resources
 *
 * Ensures connection is closed (SSL freed, socket closed), then frees
 * the connection structure itself.
 *
 * @param conn  The TLS connection to free
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

/**
 * tls_is_secure - Check if TLS connection is encrypted
 *
 * TLS connections are always encrypted.
 *
 * @param conn  The connection to check
 * @return      Always returns true
 */
static bool tls_is_secure(connection_t *conn)
{
    return true;
}

/**
 * tls_get_protocol_name - Get protocol name for logging
 *
 * @param conn  The connection (unused)
 * @return      "TLS"
 */
static const char* tls_get_protocol_name(connection_t *conn)
{
    return "TLS";
}
