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
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "connection.h"
#include "merc.h"

/*
 * Set a socket to non-blocking mode
 */
bool connection_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        log_stringf("connection_set_nonblocking: fcntl(F_GETFL) failed: %s", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_stringf("connection_set_nonblocking: fcntl(F_SETFL) failed: %s", strerror(errno));
        return false;
    }

    return true;
}

/*
 * Get human-readable connection state name
 */
const char* connection_state_name(connection_state_t state)
{
    switch (state) {
        case CONN_STATE_CONNECTING:  return "CONNECTING";
        case CONN_STATE_CONNECTED:   return "CONNECTED";
        case CONN_STATE_CLOSING:     return "CLOSING";
        case CONN_STATE_CLOSED:      return "CLOSED";
        default:                     return "UNKNOWN";
    }
}

/*
 * Get human-readable connection type name
 */
const char* connection_type_name(connection_type_t type)
{
    switch (type) {
        case CONN_TYPE_TCP:           return "TCP";
        case CONN_TYPE_TLS:           return "TLS";
        case CONN_TYPE_WEBSOCKET_TLS: return "WebSocket+TLS (WSS)";
        default:                      return "UNKNOWN";
    }
}
