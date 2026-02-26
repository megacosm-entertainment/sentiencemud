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

#include "protocol_layer.h"
#include "connection.h"
#include "merc.h"

/*
 * Determine appropriate protocol layer for a connection type
 */
protocol_layer_t* protocol_layer_create_for_connection(connection_t *conn, struct descriptor_data *desc)
{
    if (!conn || !desc) {
        log_string("protocol_layer_create_for_connection: NULL connection or descriptor");
        return NULL;
    }

    switch (conn->type) {
        case CONN_TYPE_TCP:
        case CONN_TYPE_TLS:
            // TCP/TLS connections use full telnet protocol
            return protocol_telnet_create(conn, desc);

        case CONN_TYPE_WEBSOCKET_TLS:
            // WebSocket connections use WebSocket protocol layer
            // (colors + GMCP, no telnet IAC)
            // Note: Only WSS (WebSocket over TLS) is supported
            return protocol_websocket_create(conn, desc);

        default:
            log_stringf("protocol_layer_create_for_connection: Unknown connection type %d",
                       conn->type);
            return NULL;
    }
}
