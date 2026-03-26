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
    int sn __attribute__((unused)) = skill->uid;
    OBJ_DATA *spring;

    OBJ_INDEX_DATA *spring_idx = get_reserved_obj_index("obj_spring");
    if (!spring_idx) {
        log_message(LOG_LEVEL_BUG, LOG_ERROR, "spell_create_spring: obj_spring prototype not found.");
        return false;
    }
    spring = create_object(spring_idx, 0, true);
    if (!spring) {
        return false;
    }
    spring->timer = level;
    obj_to_room(spring, ch->in_room);
    act("$p flows from the ground.", ch, NULL, NULL, spring, NULL, NULL, NULL, TO_ALL, NULL, NULL);
    return true;
}


SPELL_FUNC(spell_create_water)
{
    int sn __attribute__((unused)) = skill->uid;
    OBJ_DATA *obj = (OBJ_DATA *) vo;
    int water;

    if (obj->item_type != ITEM_DRINK_CON) {
        send_to_char("It is unable to hold water.\n\r", ch);
        return false;
    }

    if (FLUID_CON(obj)->liquid != LIQ_WATER && FLUID_CON(obj)->amount) {
        send_to_char("It contains some other liquid.\n\r", ch);
        return false;
    }

    water = FLUID_CON(obj)->capacity - FLUID_CON(obj)->amount;
    FLUID_CON(obj)->liquid = LIQ_WATER;
    FLUID_CON(obj)->amount += water;
    if (!is_name("water", obj->name)) {
        char buf[MAX_STRING_LENGTH];

        sprintf(buf, "%s water", obj->name);
        free_string(obj->name);
        obj->name = str_dup(buf);
    }
    act("$p is filled.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return true;
}
