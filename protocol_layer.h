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

#ifndef PROTOCOL_LAYER_H
#define PROTOCOL_LAYER_H

#include <stdbool.h>
#include "connection.h"

/*
 * Forward declarations
 */
struct descriptor_data;
struct protocol_layer;
struct protocol_layer_vtable;

/*
 * Protocol layer capabilities
 * These indicate what features a protocol layer supports
 */
typedef enum {
    PROTO_CAP_NONE          = 0,
    PROTO_CAP_ANSI_COLOR    = (1 << 0),  // ANSI color codes
    PROTO_CAP_XTERM_256     = (1 << 1),  // 256-color support
    PROTO_CAP_UTF8          = (1 << 2),  // UTF-8 unicode
    PROTO_CAP_MSDP          = (1 << 3),  // MSDP variables
    PROTO_CAP_GMCP          = (1 << 4),  // GMCP (JSON-based MSDP)
    PROTO_CAP_MXP           = (1 << 5),  // MXP (telnet only)
    PROTO_CAP_MCCP          = (1 << 6),  // Compression (telnet only)
    PROTO_CAP_MSP           = (1 << 7),  // Sound triggers (telnet only)
    PROTO_CAP_TELNET_IAC    = (1 << 8),  // Telnet IAC negotiation
} protocol_capabilities_t;

/*
 * Virtual function table for protocol operations
 * Each protocol type implements these functions
 */
typedef struct protocol_layer_vtable {
    /*
     * Process input from connection
     * Strips protocol-specific sequences (telnet IAC, etc.)
     * Returns: processed text for game logic
     * out_buf: buffer to store processed text
     * out_size: size of output buffer
     */
    void (*process_input)(struct protocol_layer *proto,
                         const char *input, int input_len,
                         char *out_buf, int out_size);

    /*
     * Process output to connection
     * Applies colors, MXP, MCCP, etc based on capabilities
     * Returns: processed text ready to send
     * out_len: set to length of processed output
     */
    const char* (*process_output)(struct protocol_layer *proto,
                                  const char *output, int *out_len);

    /*
     * Perform protocol-specific negotiation (telnet handshake, etc.)
     * Called when connection is established
     */
    void (*negotiate)(struct protocol_layer *proto);

    /*
     * Send MSDP/GMCP variable update
     * variable: variable name (e.g., "HEALTH")
     * value: variable value
     * is_number: true if value is numeric
     */
    void (*send_mxp_variable)(struct protocol_layer *proto,
                             const char *variable, const char *value,
                             bool is_number);

    /*
     * Set echo on/off (for password entry)
     */
    void (*set_echo)(struct protocol_layer *proto, bool echo_on);

    /*
     * Free protocol resources
     */
    void (*free)(struct protocol_layer *proto);

    /*
     * Get protocol name for logging
     */
    const char* (*get_protocol_name)(struct protocol_layer *proto);

    /*
     * Get protocol capabilities
     */
    protocol_capabilities_t (*get_capabilities)(struct protocol_layer *proto);
} protocol_layer_vtable_t;

/*
 * Base protocol layer structure
 * All protocol types embed this at the start of their structure
 */
typedef struct protocol_layer {
    protocol_layer_vtable_t *vtable;       // Virtual function table
    connection_t *connection;              // Associated connection
    struct descriptor_data *descriptor;    // Back-reference to descriptor
    void *proto_data;                      // Protocol-specific data (protocol_t*, etc)
    protocol_capabilities_t capabilities;  // Supported features
} protocol_layer_t;

/*
 * Protocol layer factory functions
 */

// Create a telnet protocol layer (full telnet with IAC, MCCP, MXP, etc.)
protocol_layer_t* protocol_telnet_create(connection_t *conn, struct descriptor_data *desc);

// Create a WebSocket protocol layer (colors + GMCP, no telnet-specific features)
protocol_layer_t* protocol_websocket_create(connection_t *conn, struct descriptor_data *desc);

// Create a plain text protocol layer (ANSI colors only, no negotiation)
protocol_layer_t* protocol_plain_create(connection_t *conn, struct descriptor_data *desc);

/*
 * Helper functions
 */

// Determine appropriate protocol for a connection type
protocol_layer_t* protocol_layer_create_for_connection(connection_t *conn, struct descriptor_data *desc);

/*
 * Inline wrapper functions for cleaner calling syntax
 */

static inline void protocol_process_input(protocol_layer_t *proto,
                                         const char *input, int input_len,
                                         char *out_buf, int out_size)
{
    proto->vtable->process_input(proto, input, input_len, out_buf, out_size);
}

static inline const char* protocol_process_output(protocol_layer_t *proto,
                                                  const char *output, int *out_len)
{
    return proto->vtable->process_output(proto, output, out_len);
}

static inline void protocol_negotiate(protocol_layer_t *proto)
{
    proto->vtable->negotiate(proto);
}

static inline void protocol_send_mxp_variable(protocol_layer_t *proto,
                                             const char *variable, const char *value,
                                             bool is_number)
{
    if (proto->vtable->send_mxp_variable)
        proto->vtable->send_mxp_variable(proto, variable, value, is_number);
}

static inline void protocol_set_echo(protocol_layer_t *proto, bool echo_on)
{
    if (proto->vtable->set_echo)
        proto->vtable->set_echo(proto, echo_on);
}

static inline void protocol_layer_free(protocol_layer_t *proto)
{
    proto->vtable->free(proto);
}

static inline const char* protocol_get_name(protocol_layer_t *proto)
{
    return proto->vtable->get_protocol_name(proto);
}

static inline protocol_capabilities_t protocol_get_capabilities(protocol_layer_t *proto)
{
    return proto->vtable->get_capabilities(proto);
}

// Check if protocol has a capability
static inline bool protocol_has_capability(protocol_layer_t *proto, protocol_capabilities_t cap)
{
    return (proto->capabilities & cap) != 0;
}

#endif // PROTOCOL_LAYER_H
