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
#include "protocol_layer.h"
#include "protocol.h"
#include "merc.h"

/*
 * Telnet protocol layer structure
 * Wraps the existing protocol.c implementation
 */
typedef struct protocol_telnet {
    protocol_layer_t base;    // Must be first for casting
    protocol_t *pProtocol;    // Existing KaVir protocol structure
} protocol_telnet_t;

/*
 * Forward declarations
 */
static void telnet_process_input(protocol_layer_t *proto,
                                const char *input, int input_len,
                                char *out_buf, int out_size);
static const char* telnet_process_output(protocol_layer_t *proto,
                                        const char *output, int *out_len);
static void telnet_negotiate(protocol_layer_t *proto);
static void telnet_send_mxp_variable(protocol_layer_t *proto,
                                    const char *variable, const char *value,
                                    bool is_number);
static void telnet_set_echo(protocol_layer_t *proto, bool echo_on);
static void telnet_free(protocol_layer_t *proto);
static const char* telnet_get_protocol_name(protocol_layer_t *proto);
static protocol_capabilities_t telnet_get_capabilities(protocol_layer_t *proto);

/*
 * Telnet virtual function table
 */
static protocol_layer_vtable_t telnet_vtable = {
    .process_input = telnet_process_input,
    .process_output = telnet_process_output,
    .negotiate = telnet_negotiate,
    .send_mxp_variable = telnet_send_mxp_variable,
    .set_echo = telnet_set_echo,
    .free = telnet_free,
    .get_protocol_name = telnet_get_protocol_name,
    .get_capabilities = telnet_get_capabilities
};

/*
 * Create a new telnet protocol layer
 */
protocol_layer_t* protocol_telnet_create(connection_t *conn, struct descriptor_data *desc)
{
    protocol_telnet_t *telnet_proto;
    protocol_t *pProtocol;

    // Allocate protocol structure
    telnet_proto = (protocol_telnet_t*)calloc(1, sizeof(protocol_telnet_t));
    if (!telnet_proto) {
        log_string("protocol_telnet_create: Out of memory");
        return NULL;
    }

    // Create KaVir protocol structure (from existing protocol.c)
    pProtocol = ProtocolCreate();
    if (!pProtocol) {
        log_string("protocol_telnet_create: ProtocolCreate() failed");
        free(telnet_proto);
        return NULL;
    }

    // Initialize base protocol layer
    telnet_proto->base.vtable = &telnet_vtable;
    telnet_proto->base.connection = conn;
    telnet_proto->base.descriptor = desc;
    telnet_proto->base.proto_data = pProtocol;
    telnet_proto->base.capabilities =
        PROTO_CAP_ANSI_COLOR |
        PROTO_CAP_XTERM_256 |
        PROTO_CAP_UTF8 |
        PROTO_CAP_MSDP |
        PROTO_CAP_GMCP |
        PROTO_CAP_MXP |
        PROTO_CAP_MCCP |
        PROTO_CAP_MSP |
        PROTO_CAP_TELNET_IAC;

    telnet_proto->pProtocol = pProtocol;

    return (protocol_layer_t*)telnet_proto;
}

/*
 * Process input from telnet connection
 * Strips IAC sequences and returns clean text
 */
static void telnet_process_input(protocol_layer_t *proto,
                                const char *input, int input_len,
                                char *out_buf, int out_size)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Use existing ProtocolInput from protocol.c
    // This handles telnet IAC sequences, MSDP, etc.
    ProtocolInput(proto->descriptor, (char*)input, input_len, out_buf);
}

/*
 * Process output to telnet connection
 * Applies ANSI colors, MXP, compression, etc.
 */
static const char* telnet_process_output(protocol_layer_t *proto,
                                        const char *output, int *out_len)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Use existing ProtocolOutput from protocol.c
    // This handles color codes, MXP, MSP, etc.
    return ProtocolOutput(proto->descriptor, output, out_len);
}

/*
 * Perform telnet protocol negotiation
 */
static void telnet_negotiate(protocol_layer_t *proto)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Use existing ProtocolNegotiate from protocol.c
    // This sends telnet IAC sequences to negotiate options
    ProtocolNegotiate(proto->descriptor);
}

/*
 * Send MSDP/GMCP variable update
 */
static void telnet_send_mxp_variable(protocol_layer_t *proto,
                                    const char *variable, const char *value,
                                    bool is_number)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Use existing MSDP functions from protocol.c
    if (is_number) {
        // For MSDP, we need to convert string back to int
        // This is a bit awkward - in the future we should pass int directly
        int int_value = atoi(value);
        // Look up variable enum by name (simplified - real implementation
        // would need a lookup table)
        // For now, just use MSDPSendPair which works for both
        MSDPSendPair(proto->descriptor, variable, value);
    } else {
        MSDPSendPair(proto->descriptor, variable, value);
    }
}

/*
 * Set echo on/off (for password entry)
 */
static void telnet_set_echo(protocol_layer_t *proto, bool echo_on)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Use existing ProtocolNoEcho from protocol.c
    ProtocolNoEcho(proto->descriptor, !echo_on);
}

/*
 * Free telnet protocol layer
 */
static void telnet_free(protocol_layer_t *proto)
{
    protocol_telnet_t *telnet_proto = (protocol_telnet_t*)proto;

    // Free KaVir protocol structure
    if (telnet_proto->pProtocol) {
        ProtocolDestroy(telnet_proto->pProtocol);
        telnet_proto->pProtocol = NULL;
    }

    // Free the structure
    free(telnet_proto);
}

/*
 * Get protocol name
 */
static const char* telnet_get_protocol_name(protocol_layer_t *proto)
{
    return "Telnet";
}

/*
 * Get protocol capabilities
 */
static protocol_capabilities_t telnet_get_capabilities(protocol_layer_t *proto)
{
    return proto->capabilities;
}
