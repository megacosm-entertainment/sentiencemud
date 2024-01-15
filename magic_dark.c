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

SPELL_FUNC(spell_dark_shroud)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	// Is an obj
	if (level > 9000) {
		level -= 9000;
		perm = true;
	}

	if (perm && is_affected(victim, skill))
		affect_strip(victim, skill);
	else if (IS_AFFECTED2(victim, AFF2_DARK_SHROUD)) {
		if (victim == ch)
			send_to_char("You are already surrounded by darkness.\n\r",ch);
		else
			act("$N is already surrounded by darkness.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	if(!perm) {
		if(use_catalyst(ch,NULL,CATALYST_DARKNESS,CATALYST_INVENTORY,1,true) < 1) {
			send_to_char("You appear to be missing a darkness catalyst.\n\r",ch);
			return false;
		}
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : (3 + level/5);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_DARK_SHROUD;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);

	if(room_is_dark(ch->in_room) || is_darked(ch->in_room))
		act("{DA dark shroud pulls $n into the darkness.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	else
		act("{D$n is surrounded by a shroud of darkness.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DYou are surrounded by a shroud of darkness.{x\n\r", victim);
	return true;
}

SPELL_FUNC(spell_momentary_darkness)
{
	OBJ_DATA *darkness;
	CHAR_DATA *rch;
	int catalyst;

	catalyst = has_catalyst(ch,NULL,CATALYST_DARKNESS,CATALYST_CARRY);
	if(!catalyst) {
		send_to_char("You appear to be missing a required darkness catalyst.\n\r", ch);
		return false;
	}
	catalyst = use_catalyst(ch,NULL,CATALYST_DARKNESS,CATALYST_CARRY,5,true);

	for (darkness = ch->in_room->contents; darkness; darkness = darkness->next_content) {
		if (darkness->item_type == ITEM_ROOM_DARKNESS) {
			act("{DThe darkness seems to grow stronger...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
			darkness->timer += 3 + catalyst;
			return true;
		}
	}

	if (!obj_index_room_darkness) {
		bug("spell_momentary_darkness: get_obj_index was null!\n\r", 0);
		return false;
	}

	darkness = create_object(obj_index_room_darkness, 0, true);
	darkness->timer = 3 + catalyst;
	darkness->level = ch->tot_level;

	obj_to_room(darkness, ch->in_room);

	act("{DAn intense darkness shrouds the surrounding area!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);

	// Stop fights in the room
	for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
		if (rch->fighting) stop_fighting(rch, true);
		if (IS_NPC(rch) && IS_SET(rch->act[0],ACT_AGGRESSIVE))
			REMOVE_BIT(rch->act[0],ACT_AGGRESSIVE);
		// TODO: Remove AFF2_AGGRESSIVE
	}
	return true;
}
