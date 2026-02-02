/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvements copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefiting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	Sentience MUD improvements copyright (C) 2000-2026                *
*       Nibelung Enterprises                                              *
*       All Rights Reserved                                               *
***************************************************************************/

#ifndef JSON_CHAT_H
#define JSON_CHAT_H

#include <jansson.h>
#include "merc.h"

/* JSON serialization functions */
json_t *json_chat_room_serialize(CHAT_ROOM_DATA *chat);
json_t *json_chat_op_serialize(CHAT_OP_DATA *op);
json_t *json_chat_ban_serialize(CHAT_BAN_DATA *ban);

/* JSON deserialization functions */
CHAT_ROOM_DATA *json_chat_room_deserialize(json_t *json);
CHAT_OP_DATA *json_chat_op_deserialize(json_t *json);
CHAT_BAN_DATA *json_chat_ban_deserialize(json_t *json);

/* File I/O */
bool save_chat_rooms_json(void);
bool load_chat_rooms_json(void);

#endif /* JSON_CHAT_H */
