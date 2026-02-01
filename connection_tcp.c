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
 * @file connection_tcp.c
 * @brief Plain TCP connection implementation
 *
 * Implements the connection abstraction interface for unencrypted TCP
 * sockets (traditional telnet connections). This is the simplest connection
 * type with no handshake required - connections are ready immediately.
 *
 * Uses standard POSIX read()/write() system calls in non-blocking mode.
 * Handles EAGAIN/EWOULDBLOCK for non-blocking I/O and properly detects
 * connection closures and errors.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "connection.h"
#include "merc.h"

/**
 * @struct connection_tcp
 * @brief TCP connection implementation structure
 *
 * Plain unencrypted TCP socket. Has no additional state beyond the base
 * connection since TCP requires no protocol-specific data.
 */
typedef struct connection_tcp {
    connection_t base;      /**< Base connection (must be first for casting) */
} connection_tcp_t;

/* Forward declarations for vtable functions */
static bool tcp_read(connection_t *conn, char *buf, int size, int *bytes_read);
static bool tcp_write(connection_t *conn, const char *buf, int size, int *bytes_written);
static bool tcp_process_handshake(connection_t *conn);
static void tcp_close(connection_t *conn);
static void tcp_free(connection_t *conn);
static bool tcp_is_secure(connection_t *conn);
static const char* tcp_get_protocol_name(connection_t *conn);

/**
 * @brief Virtual function table for TCP connections
 */
static connection_vtable_t tcp_vtable = {
    .read = tcp_read,
    .write = tcp_write,
    .process_handshake = tcp_process_handshake,
    .close = tcp_close,
    .free = tcp_free,
    .is_secure = tcp_is_secure,
    .get_protocol_name = tcp_get_protocol_name
};

/**
 * connection_tcp_create - Create a new TCP connection
 *
 * Factory function to create a plain TCP connection from an accepted socket.
 * Sets socket to non-blocking mode. TCP connections require no handshake
 * and are ready for I/O immediately.
 *
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with this connection
 * @return      New connection_t pointer, or NULL on failure
 */
connection_t* connection_tcp_create(int fd, struct descriptor_data *desc)
{
    connection_tcp_t *tcp_conn;

    // Allocate connection structure
    tcp_conn = (connection_tcp_t*)calloc(1, sizeof(connection_tcp_t));
    if (!tcp_conn) {
        log_string("connection_tcp_create: Out of memory");
        return NULL;
    }

    // Initialize base connection
    tcp_conn->base.vtable = &tcp_vtable;
    tcp_conn->base.type = CONN_TYPE_TCP;
    tcp_conn->base.state = CONN_STATE_CONNECTED;  // TCP is ready immediately
    tcp_conn->base.fd = fd;
    tcp_conn->base.descriptor = desc;
    tcp_conn->base.proto_data = NULL;
    tcp_conn->base.last_activity = current_time;
    tcp_conn->base.handshake_in_progress = false;

    // Set socket to non-blocking
    if (!connection_set_nonblocking(fd)) {
        free(tcp_conn);
        return NULL;
    }

    return (connection_t*)tcp_conn;
}

/**
 * tcp_read - Read data from TCP socket
 *
 * Non-blocking read from the TCP socket. Returns immediately with
 * bytes_read=0 if no data is available (EAGAIN/EWOULDBLOCK).
 *
 * @param conn        The connection to read from
 * @param buf         Buffer to store read data
 * @param size        Maximum bytes to read
 * @param bytes_read  Output: number of bytes actually read
 * @return            true on success or would-block, false on error/disconnect
 */
static bool tcp_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    ssize_t nread;

    *bytes_read = 0;

    // Read from socket
    nread = read(conn->fd, buf, size);

    if (nread > 0) {
        // Successfully read data
        *bytes_read = nread;
        conn->last_activity = current_time;
        return true;
    } else if (nread == 0) {
        // Connection closed by peer
        log_stringf("tcp_read: Connection closed (fd %d)", conn->fd);
        return false;
    } else {
        // Error occurred
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available (non-blocking)
            return true;
        }

        // Real error
        log_stringf("tcp_read: read() failed on fd %d: %s", conn->fd, strerror(errno));
        return false;
    }
}

/**
 * tcp_write - Write data to TCP socket
 *
 * Non-blocking write to the TCP socket. Returns immediately with
 * bytes_written=0 if socket buffer is full (EAGAIN/EWOULDBLOCK).
 * Detects broken connections (EPIPE, ECONNRESET).
 *
 * @param conn           The connection to write to
 * @param buf            Data to write
 * @param size           Bytes to write
 * @param bytes_written  Output: number of bytes actually written
 * @return               true on success or would-block, false on error/disconnect
 */
static bool tcp_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    ssize_t nwritten;

    *bytes_written = 0;

    // Write to socket
    nwritten = write(conn->fd, buf, size);

    if (nwritten > 0) {
        // Successfully wrote data
        *bytes_written = nwritten;
        conn->last_activity = current_time;
        return true;
    } else if (nwritten == 0) {
        // Nothing written (shouldn't happen)
        return true;
    } else {
        // Error occurred
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Socket not ready for writing (non-blocking)
            return true;
        }

        if (errno == EPIPE || errno == ECONNRESET) {
            // Connection broken
            log_stringf("tcp_write: Connection broken (fd %d): %s", conn->fd, strerror(errno));
            return false;
        }

        // Real error
        log_stringf("tcp_write: write() failed on fd %d: %s", conn->fd, strerror(errno));
        return false;
    }
}

/**
 * tcp_process_handshake - Process handshake (no-op for TCP)
 *
 * TCP has no handshake - connections are ready immediately after accept().
 *
 * @param conn  The connection (unused)
 * @return      Always returns true (handshake complete)
 */
static bool tcp_process_handshake(connection_t *conn)
{
    // TCP connections are ready immediately
    return true;
}

/**
 * tcp_close - Close TCP connection gracefully
 *
 * Performs orderly shutdown of the TCP connection:
 *   1. Sets state to CLOSING
 *   2. Calls shutdown() to stop both read and write
 *   3. Closes the file descriptor
 *   4. Sets state to CLOSED
 *
 * @param conn  The connection to close
 */
static void tcp_close(connection_t *conn)
{
    if (conn->state == CONN_STATE_CLOSED)
        return;

    conn->state = CONN_STATE_CLOSING;

    // Gracefully shut down the socket
    shutdown(conn->fd, SHUT_RDWR);
    close(conn->fd);

    conn->state = CONN_STATE_CLOSED;
}

/**
 * tcp_free - Free TCP connection resources
 *
 * Ensures connection is closed, then frees the connection structure.
 *
 * @param conn  The connection to free
 */
static void tcp_free(connection_t *conn)
{
    connection_tcp_t *tcp_conn = (connection_tcp_t*)conn;

    // Ensure connection is closed
    if (conn->state != CONN_STATE_CLOSED)
        tcp_close(conn);

    // Free the structure
    free(tcp_conn);
}

/**
 * tcp_is_secure - Check if TCP connection is encrypted
 *
 * Plain TCP is not encrypted.
 *
 * @param conn  The connection to check
 * @return      Always returns false
 */
static bool tcp_is_secure(connection_t *conn)
{
    return false;
}

/**
 * tcp_get_protocol_name - Get protocol name for logging
 *
 * @param conn  The connection (unused)
 * @return      "TCP"
 */
static const char* tcp_get_protocol_name(connection_t *conn)
{
    return "TCP";
}
