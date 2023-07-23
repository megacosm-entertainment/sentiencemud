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

// NATURE
//  Anything that deals with the environment itself or draws from
//  the environment and isn't any other elemental force like lightning.

SPELL_FUNC(spell_call_familiar)
{
	CHAR_DATA *victim;
	ITERATOR it;
	int lvl;

	if(ch->num_grouped >= 9) {
		send_to_char("You have too many people in your group already.\n\r", ch);
		return FALSE;
	}

	iterator_start(&it, loaded_chars);
	while(( victim = (CHAR_DATA *)iterator_nextdata(&it)))
	{
		if (!IS_NPC(victim) || IS_SET(victim->act, ACT_PROTECTED) ||
			IS_SET(victim->act, ACT_SENTINEL) ||
			IS_SET(victim->act, ACT_PRACTICE) ||
			IS_SET(victim->act, ACT_TRAIN) ||
			IS_AFFECTED(victim,AFF_CHARM) ||
			victim->master ||
			victim->fighting)
			continue;

		// Only same alignment
		if (IS_GOOD(ch) && !IS_GOOD(victim)) continue;
		if (IS_EVIL(ch) && !IS_EVIL(victim)) continue;
		if (IS_NEUTRAL(ch) && !IS_NEUTRAL(victim)) continue;

		lvl = number_range((ch->tot_level / 2), (ch->tot_level - ch->tot_level / 10));

		if (!victim->in_room)
			continue;

		if (victim->tot_level >= (lvl-5) && victim->tot_level <= (lvl+5) &&
			number_percent() < 25) {

			// DON'T CREATE! TRANSFER THE MOB!

			add_follower(victim, ch, FALSE);
			if (!add_grouped(victim, ch, FALSE)) {
				stop_follower(victim,FALSE);
				continue;
			}

			break;
		}
	}
	iterator_stop(&it);

	if (victim) {
		if(victim->in_room != ch->in_room) {
			act("{WA brilliant light consumes $n before $e vanishes into thin air.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			char_from_room(victim);
			char_to_room(victim, ch->in_room);
			act("{W$N appears within a brilliant light next to $n.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{W$N appears within a brilliant light next to you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		} else {
			act("{YA faint glow pulses around $N momentarily before $E moves next to $n.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{YA faint glow pulses around $N momentarily before $E moves next to you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}

		SET_BIT(victim->affected_by, AFF_CHARM);
		return TRUE;
	}

	return FALSE;
}

SPELL_FUNC(spell_create_rose)
{
#if 0
	// TODO: Rework all of this spell... if we will keep it?
	// Disabling until this is reworked... likely need to create a wilderness stock reserved area
	OBJ_DATA *rose;
	int chance;
	long vnum;

	chance = number_percent();

	if (chance <= 5) vnum = number_range(100167, 100171);
	else if (chance > 5 && chance <= 10) vnum = number_range(100172, 100175);
	else if (chance > 10 && chance <= 35) vnum = number_range(100082, 100089);
	else vnum = number_range(100176, 100189);

	rose = create_object(get_obj_index(vnum), 0, TRUE);
	act("You have created $p!", ch, NULL, NULL, rose, NULL, NULL, NULL, TO_CHAR);
	act("$n has created $p!", ch, NULL, NULL, rose, NULL, NULL, NULL, TO_ROOM);
	if (ch->carry_number + 1 > can_carry_n(ch)) {
		obj_to_room(rose, ch->in_room);
		act("$p floats gently to the ground.", ch, NULL, NULL, rose, NULL, NULL, NULL, TO_ALL);
	} else
		obj_to_char(rose, ch);
#endif
	return TRUE;
}


SPELL_FUNC(spell_control_weather)
{
	char *target_name = (char *) vo;

	if (!target_name) {
		send_to_char ("Do you want it to get better or worse?\n\r", ch);
		return FALSE;
	}

	if (!str_prefix(target_name, "better")) {
		weather_info.change += dice(level / 3, 4);
		send_to_char("The sky shimmers blue briefly.\n\r", ch);
		return TRUE;
	} else if (!str_prefix(target_name, "worse")) {
		weather_info.change -= dice(level / 3, 4);
		send_to_char("The sky shimmers red briefly.\n\r", ch);
		return TRUE;
	}

	send_to_char ("Do you want it to get better or worse?\n\r", ch);
	return FALSE;
}

SPELL_FUNC(spell_eagle_eye)
{
	long bonus_view;
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	ROOM_INDEX_DATA *room = ch->in_room;

	if (IS_VALID(ship))
		room = obj_room(ship->ship);

	if (!IS_WILDERNESS(room)) {
		send_to_char("Eagles won't come near the city. You must be in the wilderness.\n\r", ch);
		return FALSE;
	}

	act("You momentarily see through the eyes of an eagle.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n's eyes glaze over momentarily.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	bonus_view = 20;

	show_map_to_char_wyx(room->wilds, room->x, room->y, ch, room->x, room->y, bonus_view * 2, bonus_view / 2, false);

	// Add an event, must be outside to see it
	if(IS_OUTSIDE(ch))
	{
		wait_function(ch->in_room, NULL, EVENT_ECHO, number_range(1,2), NULL, "An eagle can be seen flying high in the sky...\n\r");
	}
	return TRUE;
}

SPELL_FUNC(spell_ensnare)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (IS_AFFECTED(victim, AFF_WEB) ||
		IS_AFFECTED2(victim, AFF2_ENSNARE)) {
		act("$N is already entangled enough.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (saves_spell(level,victim,DAM_OTHER)) {
		send_to_char("Nothing happens.\n\r", ch);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.type = sn;
	af.location = APPLY_DEX;
	af.modifier = -3;
	af.level = level + 1;
	af.duration = 2 + (int) ch->tot_level/10; //1+level > 3 ? 3 : 1+level;
	af.bitvector = 0;
	af.bitvector2 = AFF2_ENSNARE;
	affect_to_char(victim, &af);
	act("{gThick vines sprout from the ground to clutch you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{gThick vines sprout from the ground to clutch $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{gThick vines sprout from the ground to clutch $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	return TRUE;
}


SPELL_FUNC(spell_master_weather)
{
	char *target_name = (char *) vo;

	if (!str_prefix(target_name, "clear")) {
		send_to_char("{YHuge gusts of winds mobilize in the sky as you summon wind elementals.{x\n\r", ch);
		act("{Y$n summons elementals which sweep the sky clear of clouds.",ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
		weather_info.sky = SKY_CLOUDLESS;
	} else if (!str_prefix(target_name, "cloudy")) {
		send_to_char("{YElemental forces start pulling clouds into the sky.{x\n\r", ch);
		act("{Y$n summons elemental forces which pull clouds into the sky.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		weather_info.sky = SKY_CLOUDY;
	} else if (!str_prefix(target_name, "rainy")) {
		send_to_char("{YThousands of tiny water elementals bring rain down from the heavens.{x\n\r", ch);
		act("{Y$n summons thousands of tiny water elementals to bring rain down from the heavens.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		weather_info.sky = SKY_RAINING;
	} else if (!str_prefix(target_name, "stormy")) {
		send_to_char("{YFlashes of lightning arc across the sky you summon fire elementals.{x\n\r", ch);
		act("{YFlashes of lightning arc across the sky as $n summons fire elementals.{x", ch,  NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		weather_info.sky = SKY_LIGHTNING;
	} else {
		send_to_char("You must specify rainy, stormy, cloudy, or clear.\n\r", ch);
		return FALSE;
	}

	return TRUE;
}

SPELL_FUNC(spell_vision)
{
	SHIP_DATA *ship = get_room_ship(ch->in_room);
	ROOM_INDEX_DATA *room = ch->in_room;
	long bonus_view;

	if (IS_VALID(ship))
		room = obj_room(ship->ship);

	if (!IS_WILDERNESS(room) && /*!ON_SHIP(ch) &&*/
		(!IS_SET(ch->in_room->room_flags, ROOM_VIEWWILDS) ||
		!IS_OUTSIDE(ch))) {
		act("You must be outdoors.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	act("$n blinks $s eyes.\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("You have a vision of your surrounding terrain.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	WILDS_DATA *wilds = room->wilds;
	if(IS_SET(ch->in_room->room_flags, ROOM_VIEWWILDS))
		wilds = room->viewwilds;

	bonus_view = 16;

	show_map_to_char_wyx(wilds, room->x, room->y, ch, room->x, room->y, bonus_view * 2, bonus_view / 2, false);

	return TRUE;
}

SPELL_FUNC(spell_web)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;

	memset(&af,0,sizeof(af));

	if (IS_AFFECTED(victim, AFF_WEB)) {
		act("$N is already entangled in webs.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (saves_spell(level,victim,DAM_OTHER)) {
		send_to_char("Nothing happens.\n\r", ch);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where     = TO_AFFECTS;
	af.group    = AFFGROUP_PHYSICAL;
	af.type      = sn;
	af.level     = level;
	af.location  = APPLY_HITROLL;
	af.modifier  = -4;
	af.duration  = 3;
	af.bitvector = AFF_WEB;
	af.bitvector2 = 0;
	affect_to_char(victim, &af);
	send_to_char("You are enmeshed in webs!\n\r", victim);
	act("$n engulfs $N in a thick web.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	if (ch != victim) act("You engulf $N in a thick web.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	return TRUE;
}

