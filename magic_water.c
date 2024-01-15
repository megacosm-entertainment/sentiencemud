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

	spring = create_object(obj_index_spring, 0, true);
	spring->timer = level;
	obj_to_room(spring, ch->in_room);
	act("$p flows from the ground.", ch, NULL, NULL, spring, NULL, NULL, NULL, TO_ALL);
	return true;
}


SPELL_FUNC(spell_create_water)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;

	if (!IS_FLUID_CON(obj)) {
		send_to_char("It is unable to hold water.\n\r", ch);
		return false;
	}

	if (FLUID_CON(obj)->liquid && FLUID_CON(obj)->liquid != liquid_water && FLUID_CON(obj)->amount) {
		send_to_char("It contains some other liquid.\n\r", ch);
		return false;
	}

	FLUID_CON(obj)->liquid = liquid_water;
	FLUID_CON(obj)->amount = FLUID_CON(obj)->capacity;

	// Add "water" to the object's name
	if (!is_name("water", obj->name)) {
		char buf[MAX_STRING_LENGTH];

		sprintf(buf, "%s water", obj->name);
		free_string(obj->name);
		obj->name = str_dup(buf);
	}

	act("$p is filled.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_FLUID_FILLED, NULL,CONTEXT_FLUID_CON,0,0,0,0);
	return true;
}
