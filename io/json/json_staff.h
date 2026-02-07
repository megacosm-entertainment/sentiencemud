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

#ifndef JSON_STAFF_H
#define JSON_STAFF_H

#include <jansson.h>
#include "../../merc.h"

/* JSON serialization functions */
json_t *json_immortal_serialize(IMMORTAL_DATA *immortal);

/* JSON deserialization functions */
IMMORTAL_DATA *json_immortal_deserialize(json_t *json);

/* File I/O */
bool json_save_staff(const char *path);
bool json_load_staff(const char *path);

#endif /* JSON_STAFF_H */
