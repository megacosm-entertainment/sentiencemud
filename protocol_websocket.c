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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "protocol_layer.h"
#include "protocol.h"
#include "merc.h"

/*
 * WebSocket protocol layer structure
 * Supports ANSI colors and GMCP, but no telnet-specific features
 */
typedef struct protocol_websocket {
    protocol_layer_t base;           // Must be first for casting
    bool color_enabled;              // Color support enabled
    bool xterm_256_enabled;          // 256-color support
    bool utf8_enabled;               // UTF-8 support
    bool gmcp_enabled;               // GMCP support
    char *client_name;               // Client name from Core.Hello
    char *client_version;            // Client version from Core.Hello
    bool supports_char;              // Client wants Char.* packages
    bool supports_room;              // Client wants Room.* packages
} protocol_websocket_t;

/*
 * Forward declarations
 */
static void websocket_process_input(protocol_layer_t *proto,
                                   const char *input, int input_len,
                                   char *out_buf, int out_size);
static const char* websocket_process_output(protocol_layer_t *proto,
                                           const char *output, int *out_len);
static void websocket_negotiate(protocol_layer_t *proto);
static void websocket_send_mxp_variable(protocol_layer_t *proto,
                                       const char *variable, const char *value,
                                       bool is_number);
static void websocket_set_echo(protocol_layer_t *proto, bool echo_on);
static void websocket_free(protocol_layer_t *proto);
static const char* websocket_get_protocol_name(protocol_layer_t *proto);
static protocol_capabilities_t websocket_get_capabilities(protocol_layer_t *proto);

/*
 * WebSocket virtual function table
 */
static protocol_layer_vtable_t websocket_vtable = {
    .process_input = websocket_process_input,
    .process_output = websocket_process_output,
    .negotiate = websocket_negotiate,
    .send_mxp_variable = websocket_send_mxp_variable,
    .set_echo = websocket_set_echo,
    .free = websocket_free,
    .get_protocol_name = websocket_get_protocol_name,
    .get_capabilities = websocket_get_capabilities
};

/*
 * Create a new WebSocket protocol layer
 */
protocol_layer_t* protocol_websocket_create(connection_t *conn, struct descriptor_data *desc)
{
    protocol_websocket_t *ws_proto;

    // Allocate protocol structure
    ws_proto = (protocol_websocket_t*)calloc(1, sizeof(protocol_websocket_t));
    if (!ws_proto) {
        log_string("protocol_websocket_create: Out of memory");
        return NULL;
    }

    // Initialize base protocol layer
    ws_proto->base.vtable = &websocket_vtable;
    ws_proto->base.connection = conn;
    ws_proto->base.descriptor = desc;
    ws_proto->base.proto_data = NULL;
    ws_proto->base.capabilities =
        PROTO_CAP_ANSI_COLOR |
        PROTO_CAP_XTERM_256 |
        PROTO_CAP_UTF8 |
        PROTO_CAP_GMCP;  // WebSocket supports GMCP but not telnet-specific MSDP

    // WebSocket UI clients can interpret native { color tokens directly.
    ws_proto->color_enabled = true;
    ws_proto->xterm_256_enabled = true;
    ws_proto->utf8_enabled = true;
    ws_proto->gmcp_enabled = true;
    ws_proto->client_name = NULL;
    ws_proto->client_version = NULL;
    ws_proto->supports_char = false;
    ws_proto->supports_room = false;

    return (protocol_layer_t*)ws_proto;
}

/*
 * Helper: Process incoming GMCP message from WebSocket client
 */
static void process_gmcp_input(protocol_websocket_t *ws_proto, const char *input, int input_len)
{
    char package[256];
    char *space_pos;

    // GMCP format: "Package.Message {json data}"
    // Extract package name
    space_pos = strchr(input, ' ');
    if (!space_pos) {
        // No JSON data, just package name
        snprintf(package, sizeof(package), "%.*s", input_len, input);
    } else {
        int pkg_len = space_pos - input;
        if (pkg_len >= sizeof(package))
            pkg_len = sizeof(package) - 1;
        memcpy(package, input, pkg_len);
        package[pkg_len] = '\0';
    }

    // Handle Core.Supports.Set - client tells us what packages they want
    if (strstr(package, "Core.Supports.Set") || strstr(package, "Core.Supports.Add")) {
        if (strstr(input, "Char")) {
            ws_proto->supports_char = true;
            log_string("WebSocket client supports Char.* GMCP packages");
        }
        if (strstr(input, "Room")) {
            ws_proto->supports_room = true;
            log_string("WebSocket client supports Room.* GMCP packages");
        }
        if (strstr(input, "Sentience")) {
            ws_proto->supports_char = true;
            ws_proto->supports_room = true;
            if (ws_proto->base.descriptor && ws_proto->base.descriptor->pProtocol) {
                ws_proto->base.descriptor->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true;
            }
            log_string("WebSocket client supports Sentience.* GMCP packages");
        }
    }
    // Handle Core.Hello from client
    else if (strstr(package, "Core.Hello")) {
        // Extract client name and version from JSON
        // Format: {"client":"ClientName","version":"1.0"}
        // For now, just log it
        log_stringf("WebSocket client sent Core.Hello: %s", input);

        // TODO: Parse JSON and store client_name, client_version
    }
}

/*
 * Process input from WebSocket connection
 * Check for GMCP messages, otherwise pass through as game commands
 */
static void websocket_process_input(protocol_layer_t *proto,
                                   const char *input, int input_len,
                                   char *out_buf, int out_size)
{
    protocol_websocket_t *ws_proto = (protocol_websocket_t*)proto;

    // Check if this is a GMCP message (starts with package name with dots)
    // GMCP messages look like: "Core.Hello {json}" or "Char.Vitals {}"
    if (ws_proto->gmcp_enabled && input_len > 5 && strchr(input, '.')) {
        // Check if it looks like a GMCP message (has a dot in first 30 chars)
        const char *dot = strchr(input, '.');
        if (dot && (dot - input) < 30 && (dot - input) > 0) {
            // Likely a GMCP message
            process_gmcp_input(ws_proto, input, input_len);
            // Don't pass GMCP messages to game logic
            out_buf[0] = '\0';
            return;
        }
    }

    // Regular game command - just copy input to output
    int copy_len = input_len < out_size - 1 ? input_len : out_size - 1;
    memcpy(out_buf, input, copy_len);
    out_buf[copy_len] = '\0';
}

/*
 * Process output to WebSocket connection
 * Preserves native { color tokens for browser-side rendering.
 */
static const char* websocket_process_output(protocol_layer_t *proto,
                                           const char *output, int *out_len)
{
    protocol_websocket_t *ws_proto = (protocol_websocket_t*)proto;
    static char result[MAX_OUTPUT_BUFFER + 1];
    const char color_char = COLOUR_CHAR;  // '{' by default
    int i = 0, j = 0;

    if (!output)
        return output;

    // WebSocket clients handle native { color tokens themselves.
    // Keep payload text as-is so browser clients can render using their own mapper.
    if (ws_proto->color_enabled) {
        if (out_len)
            *out_len = strlen(output);
        return output;
    }

    // If colors disabled, strip color codes
    // Simple strip - just skip color sequences
    while (output[j] != '\0' && i < MAX_OUTPUT_BUFFER) {
        if (output[j] == color_char) {
            j++; // Skip color char
            if (output[j] != '\0')
                j++; // Skip color code
        } else {
            result[i++] = output[j++];
        }
    }
    result[i] = '\0';
    if (out_len)
        *out_len = i;
    return result;
}

/*
 * Helper: Send raw GMCP message via WebSocket
 * Format: GMCP.Package.Message {json data}
 */
static void send_gmcp_message(protocol_layer_t *proto, const char *package, const char *json_data)
{
    char gmcp_msg[4096];
    int len;

    if (!proto->descriptor || !proto->connection)
        return;

    // Format: Package.Message {data}
    len = snprintf(gmcp_msg, sizeof(gmcp_msg), "%s %s", package, json_data);
    if (len < 0 || len >= sizeof(gmcp_msg)) {
        log_string("send_gmcp_message: Message too large");
        return;
    }

    // Send via write_to_buffer which will use WebSocket framing
    write_to_buffer(proto->descriptor, gmcp_msg, len);
}

/*
 * Perform WebSocket protocol negotiation
 * Send GMCP Core.Hello to advertise server capabilities
 */
static void websocket_negotiate(protocol_layer_t *proto)
{
    protocol_websocket_t *ws_proto = (protocol_websocket_t*)proto;
    char hello_msg[512];

    if (!ws_proto->gmcp_enabled)
        return;

    // Send Core.Hello with server info
    snprintf(hello_msg, sizeof(hello_msg),
             "{\"client\":\"%s\",\"version\":\"%s\"}",
             MUD_NAME,
             VERSION);

    send_gmcp_message(proto, "Core.Hello", hello_msg);

    /* WebSocket clients always get Sentience.* packages */
    if (proto->descriptor && proto->descriptor->pProtocol) {
        proto->descriptor->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true;
    }

    log_stringf("WebSocket GMCP negotiation started (fd %d)",
               proto->descriptor ? proto->descriptor->descriptor : -1);
}

/*
 * Send GMCP variable update
 * WebSocket uses JSON-based GMCP instead of telnet-based MSDP
 */
static void websocket_send_mxp_variable(protocol_layer_t *proto,
                                       const char *variable, const char *value,
                                       bool is_number)
{
    /* No-op: Sentience.* GMCP packages (sent via sentience_gmcp_update)
     * now handle all WebSocket GMCP data.  The legacy per-variable path
     * through this function is no longer used. */
    (void)proto;
    (void)variable;
    (void)value;
    (void)is_number;
}

/*
 * Set echo on/off (for password entry)
 * WebSocket doesn't have telnet echo negotiation, so this is a no-op
 */
static void websocket_set_echo(protocol_layer_t *proto, bool echo_on)
{
    // WebSocket clients handle echo locally
    // We could send a GMCP message to request echo off, but most
    // clients will just use HTML input type="password"
}

/*
 * Free WebSocket protocol layer
 */
static void websocket_free(protocol_layer_t *proto)
{
    protocol_websocket_t *ws_proto = (protocol_websocket_t*)proto;

    // Free GMCP client info
    if (ws_proto->client_name) {
        free(ws_proto->client_name);
        ws_proto->client_name = NULL;
    }
    if (ws_proto->client_version) {
        free(ws_proto->client_version);
        ws_proto->client_version = NULL;
    }

    // Free the structure
    free(ws_proto);
}

/*
 * Get protocol name
 */
static const char* websocket_get_protocol_name(protocol_layer_t *proto)
{
    return "WebSocket";
}

/*
 * Get protocol capabilities
 */
static protocol_capabilities_t websocket_get_capabilities(protocol_layer_t *proto)
{
    return proto->capabilities;
}
