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

#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdbool.h>
#include <time.h>

/*
 * Forward declarations
 */
struct descriptor_data;
struct connection;
struct connection_vtable;

/*
 * Connection types
 */
typedef enum {
    CONN_TYPE_TCP,          // Plain TCP connection
    CONN_TYPE_TLS,          // TLS-encrypted TCP connection
    CONN_TYPE_WEBSOCKET_TLS // WebSocket over TLS (WSS only)
} connection_type_t;

/*
 * Connection state
 */
typedef enum {
    CONN_STATE_CONNECTING,     // Initial connection, handshake in progress
    CONN_STATE_CONNECTED,      // Fully connected and ready
    CONN_STATE_CLOSING,        // Graceful shutdown in progress
    CONN_STATE_CLOSED          // Connection closed
} connection_state_t;

/*
 * Virtual function table for connection operations
 * Each connection type implements these functions
 */
typedef struct connection_vtable {
    /*
     * Read data from the connection
     * Returns: true on success, false on error or connection closed
     * bytes_read: set to number of bytes actually read (0 on EAGAIN)
     */
    bool (*read)(struct connection *conn, char *buf, int size, int *bytes_read);

    /*
     * Write data to the connection
     * Returns: true on success, false on error or connection closed
     * bytes_written: set to number of bytes actually written (0 on EAGAIN)
     */
    bool (*write)(struct connection *conn, const char *buf, int size, int *bytes_written);

    /*
     * Process handshake for protocols that need it (TLS, WebSocket)
     * Returns: true if handshake complete, false if still in progress or error
     * Call this when the socket is readable/writable until it returns true
     */
    bool (*process_handshake)(struct connection *conn);

    /*
     * Close the connection gracefully
     * Performs protocol-specific shutdown (SSL shutdown, WebSocket close frame)
     */
    void (*close)(struct connection *conn);

    /*
     * Free connection resources
     * Called after close() to clean up memory
     */
    void (*free)(struct connection *conn);

    /*
     * Check if connection is secure (encrypted)
     */
    bool (*is_secure)(struct connection *conn);

    /*
     * Get protocol name for logging
     */
    const char* (*get_protocol_name)(struct connection *conn);
} connection_vtable_t;

/*
 * Base connection structure
 * All connection types embed this at the start of their structure
 */
typedef struct connection {
    connection_vtable_t *vtable;           // Virtual function table
    connection_type_t type;                // Connection type
    connection_state_t state;              // Current connection state
    int fd;                                // File descriptor
    struct descriptor_data *descriptor;    // Back-reference to game descriptor
    void *proto_data;                      // Protocol-specific data (SSL*, ws_state*, etc)
    time_t last_activity;                  // Last I/O activity timestamp
    bool handshake_in_progress;            // Handshake not yet complete
} connection_t;

/*
 * Connection factory functions
 */

// Create a plain TCP connection from an accepted socket
connection_t* connection_tcp_create(int fd, struct descriptor_data *desc);

// Create a TLS connection from an accepted socket
// Initiates SSL_accept() handshake
connection_t* connection_tls_create(int fd, struct descriptor_data *desc);

// Create a WebSocket+TLS connection from an accepted socket
// Performs SSL_accept() followed by WebSocket handshake
// Note: Plain WebSocket (WS) is not supported - only secure WebSocket (WSS)
connection_t* connection_websocket_tls_create(int fd, struct descriptor_data *desc);

/*
 * Helper functions
 */

// Set socket to non-blocking mode
bool connection_set_nonblocking(int fd);

// Get human-readable state name
const char* connection_state_name(connection_state_t state);

// Get human-readable type name
const char* connection_type_name(connection_type_t type);

/*
 * Inline wrapper functions for cleaner calling syntax
 */

static inline bool connection_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    return conn->vtable->read(conn, buf, size, bytes_read);
}

static inline bool connection_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    return conn->vtable->write(conn, buf, size, bytes_written);
}

static inline bool connection_process_handshake(connection_t *conn)
{
    return conn->vtable->process_handshake(conn);
}

static inline void connection_close(connection_t *conn)
{
    conn->vtable->close(conn);
}

static inline void connection_free(connection_t *conn)
{
    conn->vtable->free(conn);
}

static inline bool connection_is_secure(connection_t *conn)
{
    return conn->vtable->is_secure(conn);
}

static inline const char* connection_get_protocol_name(connection_t *conn)
{
    return conn->vtable->get_protocol_name(conn);
}

#endif // CONNECTION_H
