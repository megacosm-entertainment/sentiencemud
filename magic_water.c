/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "wilds.h"

SPELL_FUNC(spell_create_spring)
{
	OBJ_DATA *spring;

	spring = create_object(get_obj_index(OBJ_VNUM_SPRING), 0, true);
	spring->timer = level;
	obj_to_room(spring, ch->in_room);
	act("$p flows from the ground.", ch, NULL, NULL, spring, NULL, NULL, NULL, TO_ALL);
	return true;
}


SPELL_FUNC(spell_create_water)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	int water;

	if (obj->item_type != ITEM_DRINK_CON) {
		send_to_char("It is unable to hold water.\n\r", ch);
		return false;
	}

	if (obj->value[2] != LIQ_WATER && obj->value[1]) {
		send_to_char("It contains some other liquid.\n\r", ch);
		return false;
	}

	water = obj->value[0] - obj->value[1];
	obj->value[2] = LIQ_WATER;
	obj->value[1] += water;
	if (!is_name("water", obj->name)) {
		char buf[MAX_STRING_LENGTH];

		sprintf(buf, "%s water", obj->name);
		free_string(obj->name);
		obj->name = str_dup(buf);
	}
	act("$p is filled.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return true;
}
