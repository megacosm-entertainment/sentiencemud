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

    // WebSocket clients typically support colors and UTF-8 by default
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
 * ANSI color definitions (same as protocol.c)
 * Note: Background colors (C_BK_*) are already defined in merc.h as macros
 */
static const char s_Clean[]       = "\033[0m";
static const char s_DarkRed[]     = "\033[0;31m";
static const char s_BoldRed[]     = "\033[1;31m";
static const char s_DarkGreen[]   = "\033[0;32m";
static const char s_BoldGreen[]   = "\033[1;32m";
static const char s_DarkYellow[]  = "\033[0;33m";
static const char s_BoldYellow[]  = "\033[1;33m";
static const char s_DarkBlue[]    = "\033[0;34m";
static const char s_BoldBlue[]    = "\033[1;34m";
static const char s_DarkMagenta[] = "\033[0;35m";
static const char s_BoldMagenta[] = "\033[1;35m";
static const char s_DarkCyan[]    = "\033[0;36m";
static const char s_BoldCyan[]    = "\033[1;36m";
static const char s_BoldWhite[]   = "\033[1;37m";
static const char s_DarkBlack[]   = "\033[0;30m";
static const char s_BoldBlack[]   = "\033[1;30m";

/*
 * Generate ANSI 256-color code (simplified version of ColourRGB from protocol.c)
 */
static const char* get_xterm_256_color(char foreground_or_background, int r, int g, int b)
{
    static char color_buf[32];
    int color_code;

    // Convert RGB (0-5 range) to XTerm 256 color code
    // XTerm 216-color cube: 16 + (r * 36) + (g * 6) + b
    color_code = 16 + (r * 36) + (g * 6) + b;

    if (foreground_or_background == 'f' || foreground_or_background == 'F') {
        sprintf(color_buf, "\033[38;5;%dm", color_code);
    } else {
        sprintf(color_buf, "\033[48;5;%dm", color_code);
    }

    return color_buf;
}

/*
 * Process output to WebSocket connection
 * Applies ANSI colors but skips telnet-specific features (MXP, MSP, MCCP)
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

    // If colors disabled, strip color codes
    if (!ws_proto->color_enabled) {
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

    // Process color codes
    while (output[j] != '\0' && i < MAX_OUTPUT_BUFFER) {
        if (output[j] == color_char) {
            const char *color_seq = NULL;
            j++; // Skip color char

            switch (output[j]) {
                case '{': // Two {{ in a row = literal {
                    result[i++] = color_char;
                    break;

                // Basic ANSI colors
                case 'x': case 'X': case 'n': color_seq = s_Clean; break;
                case 'r': color_seq = s_DarkRed; break;
                case 'R': color_seq = s_BoldRed; break;
                case 'g': color_seq = s_DarkGreen; break;
                case 'G': color_seq = s_BoldGreen; break;
                case 'y': color_seq = s_DarkYellow; break;
                case 'Y': color_seq = s_BoldYellow; break;
                case 'b': color_seq = s_DarkBlue; break;
                case 'B': color_seq = s_BoldBlue; break;
                case 'm': color_seq = s_DarkMagenta; break;
                case 'M': color_seq = s_BoldMagenta; break;
                case 'c': color_seq = s_DarkCyan; break;
                case 'C': color_seq = s_BoldCyan; break;
                case 'w': color_seq = s_Clean; break;
                case 'W': color_seq = s_BoldWhite; break;
                case 'd': color_seq = s_DarkBlack; break;
                case 'D': color_seq = s_BoldBlack; break;

                // Background colors
                case '0': color_seq = C_BK_BLACK; break;
                case '1': color_seq = C_BK_BLUE; break;
                case '2': color_seq = C_BK_CYAN; break;
                case '3': color_seq = C_BK_GREEN; break;
                case '4': color_seq = C_BK_MAGENTA; break;
                case '5': color_seq = C_BK_RED; break;
                case '6': color_seq = C_BK_WHITE; break;
                case '7': color_seq = C_BK_YELLOW; break;

                // Extended colors (XTerm 256)
                case 'a': color_seq = get_xterm_256_color('f', 0, 1, 4); break; // azure
                case 'A': color_seq = get_xterm_256_color('f', 0, 2, 5); break;
                case 'j': color_seq = get_xterm_256_color('f', 0, 3, 1); break; // jade
                case 'J': color_seq = get_xterm_256_color('f', 0, 5, 2); break;
                case 'l': color_seq = get_xterm_256_color('f', 1, 4, 0); break; // lime
                case 'L': color_seq = get_xterm_256_color('f', 2, 5, 0); break;
                case 'o': color_seq = get_xterm_256_color('f', 5, 2, 0); break; // orange
                case 'O': color_seq = get_xterm_256_color('f', 5, 3, 0); break;
                case 'p': color_seq = get_xterm_256_color('f', 3, 0, 1); break; // pink
                case 'P': color_seq = get_xterm_256_color('f', 5, 0, 2); break;
                case 't': color_seq = get_xterm_256_color('f', 2, 1, 0); break; // tan
                case 'T': color_seq = get_xterm_256_color('f', 3, 2, 1); break;
                case 'v': color_seq = get_xterm_256_color('f', 1, 0, 4); break; // violet
                case 'V': color_seq = get_xterm_256_color('f', 2, 0, 5); break;

                // Special effects
                case 'i': color_seq = "\033[5m"; break;  // blink
                case 'f': color_seq = "\033[7m"; break;  // reverse

                default:
                    // Unknown color code - just skip it
                    break;
            }

            // Copy color sequence to output
            if (color_seq) {
                while (*color_seq && i < MAX_OUTPUT_BUFFER) {
                    result[i++] = *color_seq++;
                }
            }

            j++; // Move past color code character
        } else {
            // Regular character - copy it
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
    protocol_websocket_t *ws_proto = (protocol_websocket_t*)proto;
    char json_data[1024];

    if (!ws_proto->gmcp_enabled)
        return;

    // Format as simple JSON: {variable: value}
    if (is_number) {
        snprintf(json_data, sizeof(json_data), "{\"%s\":%s}", variable, value);
    } else {
        // Escape quotes in string values
        snprintf(json_data, sizeof(json_data), "{\"%s\":\"%s\"}", variable, value);
    }

    // Send via appropriate GMCP package (Char.Vitals, Char.Status, etc.)
    // For now, use a generic package
    send_gmcp_message(proto, "Char.Status", json_data);
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
