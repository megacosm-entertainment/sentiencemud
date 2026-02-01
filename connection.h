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
 * @file connection.h
 * @brief Connection abstraction layer for multi-protocol support
 *
 * Provides a polymorphic interface for handling different connection types:
 *   - Plain TCP (telnet)
 *   - TLS-encrypted TCP
 *   - WebSocket over TLS (WSS)
 *
 * Uses a virtual function table (vtable) pattern to allow protocol-specific
 * implementations while presenting a unified API to the rest of the codebase.
 *
 * Each connection type implements:
 *   - read(): Non-blocking data read
 *   - write(): Non-blocking data write
 *   - process_handshake(): Protocol handshake (TLS, WebSocket)
 *   - close(): Graceful connection shutdown
 *   - free(): Resource cleanup
 *   - is_secure(): Check if connection is encrypted
 *   - get_protocol_name(): Human-readable protocol name
 *
 * Inline wrapper functions (connection_read, connection_write, etc.) provide
 * convenient calling syntax without direct vtable access.
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdbool.h>
#include <time.h>

/**
 * @name Forward Declarations
 * @{
 */
struct descriptor_data;
struct connection;
struct connection_vtable;
/** @} */

/**
 * @enum connection_type_t
 * @brief Enumeration of supported connection protocols
 */
typedef enum {
    CONN_TYPE_TCP,          /**< Plain TCP connection (telnet) */
    CONN_TYPE_TLS,          /**< TLS-encrypted TCP connection */
    CONN_TYPE_WEBSOCKET_TLS /**< WebSocket over TLS (WSS only, no plain WS) */
} connection_type_t;

/**
 * @enum connection_state_t
 * @brief Connection lifecycle states
 */
typedef enum {
    CONN_STATE_CONNECTING,  /**< Initial connection, handshake in progress */
    CONN_STATE_CONNECTED,   /**< Fully connected and ready for I/O */
    CONN_STATE_CLOSING,     /**< Graceful shutdown in progress */
    CONN_STATE_CLOSED       /**< Connection closed, awaiting cleanup */
} connection_state_t;

/**
 * @struct connection_vtable
 * @brief Virtual function table for polymorphic connection operations
 *
 * Each connection type (TCP, TLS, WebSocket) implements these functions.
 * The vtable pattern allows the game loop to treat all connections uniformly
 * while delegating protocol-specific behavior to the implementations.
 */
typedef struct connection_vtable {
    /**
     * Read data from the connection (non-blocking)
     * @param conn        The connection to read from
     * @param buf         Buffer to store read data
     * @param size        Maximum bytes to read
     * @param bytes_read  Output: actual bytes read (0 on EAGAIN/EWOULDBLOCK)
     * @return            true on success, false on error or connection closed
     */
    bool (*read)(struct connection *conn, char *buf, int size, int *bytes_read);

    /**
     * Write data to the connection (non-blocking)
     * @param conn           The connection to write to
     * @param buf            Data to write
     * @param size           Bytes to write
     * @param bytes_written  Output: actual bytes written (0 on EAGAIN/EWOULDBLOCK)
     * @return               true on success, false on error or connection closed
     */
    bool (*write)(struct connection *conn, const char *buf, int size, int *bytes_written);

    /**
     * Process protocol handshake (TLS, WebSocket)
     * @param conn  The connection with pending handshake
     * @return      true if handshake complete, false if still in progress or error
     * @note        Call repeatedly when socket is readable/writable until returns true
     */
    bool (*process_handshake)(struct connection *conn);

    /**
     * Close connection gracefully
     * @param conn  The connection to close
     * @note        Performs protocol-specific shutdown (SSL_shutdown, WebSocket close frame)
     */
    void (*close)(struct connection *conn);

    /**
     * Free connection resources
     * @param conn  The connection to free
     * @note        Called after close() to release memory
     */
    void (*free)(struct connection *conn);

    /**
     * Check if connection is encrypted
     * @param conn  The connection to check
     * @return      true if TLS/WSS, false for plain TCP
     */
    bool (*is_secure)(struct connection *conn);

    /**
     * Get human-readable protocol name
     * @param conn  The connection to query
     * @return      Static string: "TCP", "TLS", or "WebSocket+TLS"
     */
    const char* (*get_protocol_name)(struct connection *conn);
} connection_vtable_t;

/**
 * @struct connection
 * @brief Base connection structure for all protocol types
 *
 * Represents a network connection independent of the underlying protocol.
 * All connection type implementations embed this structure and add their
 * protocol-specific data in the proto_data field.
 */
typedef struct connection {
    connection_vtable_t *vtable;        /**< Virtual function table for this type */
    connection_type_t type;             /**< Connection protocol type */
    connection_state_t state;           /**< Current lifecycle state */
    int fd;                             /**< Underlying socket file descriptor */
    struct descriptor_data *descriptor; /**< Back-reference to game descriptor */
    void *proto_data;                   /**< Protocol-specific data (SSL*, ws_state*, etc.) */
    time_t last_activity;               /**< Timestamp of last I/O activity */
    bool handshake_in_progress;         /**< True until handshake completes */
} connection_t;

/**
 * @name Factory Functions
 * Create connections of specific types from accepted sockets.
 * @{
 */

/**
 * connection_tcp_create - Create a plain TCP connection
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with connection
 * @return      New connection, or NULL on failure
 */
connection_t* connection_tcp_create(int fd, struct descriptor_data *desc);

/**
 * connection_tls_create - Create a TLS-encrypted connection
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with connection
 * @return      New connection, or NULL on failure
 * @note        Initiates SSL_accept() handshake (non-blocking)
 */
connection_t* connection_tls_create(int fd, struct descriptor_data *desc);

/**
 * connection_websocket_tls_create - Create a WebSocket over TLS connection
 * @param fd    Accepted socket file descriptor
 * @param desc  Game descriptor to associate with connection
 * @return      New connection, or NULL on failure
 * @note        Performs SSL handshake, then WebSocket upgrade handshake
 * @note        Plain WebSocket (WS) not supported - WSS only
 */
connection_t* connection_websocket_tls_create(int fd, struct descriptor_data *desc);

/** @} */

/**
 * @name Helper Functions
 * @{
 */

/**
 * connection_set_nonblocking - Set socket to non-blocking mode
 * @param fd  Socket file descriptor
 * @return    true on success, false on failure
 */
bool connection_set_nonblocking(int fd);

/**
 * connection_state_name - Get human-readable state name
 * @param state  Connection state enum value
 * @return       Static string describing the state
 */
const char* connection_state_name(connection_state_t state);

/**
 * connection_type_name - Get human-readable connection type name
 * @param type  Connection type enum value
 * @return      Static string describing the type
 */
const char* connection_type_name(connection_type_t type);

/** @} */

/**
 * @name Inline Wrapper Functions
 * Convenience functions that delegate to the vtable. These provide cleaner
 * calling syntax without requiring direct vtable access.
 * @{
 */

/** @brief Read data from connection - see connection_vtable::read */
static inline bool connection_read(connection_t *conn, char *buf, int size, int *bytes_read)
{
    return conn->vtable->read(conn, buf, size, bytes_read);
}

/** @brief Write data to connection - see connection_vtable::write */
static inline bool connection_write(connection_t *conn, const char *buf, int size, int *bytes_written)
{
    return conn->vtable->write(conn, buf, size, bytes_written);
}

/** @brief Process handshake - see connection_vtable::process_handshake */
static inline bool connection_process_handshake(connection_t *conn)
{
    return conn->vtable->process_handshake(conn);
}

/** @brief Close connection - see connection_vtable::close */
static inline void connection_close(connection_t *conn)
{
    conn->vtable->close(conn);
}

/** @brief Free connection - see connection_vtable::free */
static inline void connection_free(connection_t *conn)
{
    conn->vtable->free(conn);
}

/** @brief Check if secure - see connection_vtable::is_secure */
static inline bool connection_is_secure(connection_t *conn)
{
    return conn->vtable->is_secure(conn);
}

/** @brief Get protocol name - see connection_vtable::get_protocol_name */
static inline const char* connection_get_protocol_name(connection_t *conn)
{
    return conn->vtable->get_protocol_name(conn);
}

/** @} */

#endif // CONNECTION_H
