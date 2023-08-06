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


SPELL_FUNC(spell_gate)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	bool gate_pet;
	int distance, catalyst;

	if (!victim) {
		send_to_char("Nobody like that is around.\n\r", ch);
		return FALSE;
	}

	if (!can_gate(ch, victim))
		return FALSE;

	// getdistance... ln(distance)+1
	distance = 1;

	catalyst = has_catalyst(ch,NULL,CATALYST_ASTRAL,CATALYST_INVENTORY,1,CATALYST_MAXSTRENGTH);
	if(catalyst >= 0 && catalyst < distance) {
		send_to_char("You appear to be missing a required astral catalyst.\n\r", ch);
		return TRUE;
	}

	catalyst = use_catalyst(ch,NULL,CATALYST_ASTRAL,CATALYST_INVENTORY,distance,1,CATALYST_MAXSTRENGTH,TRUE);

	if (ch->pet && ch->in_room == ch->pet->in_room)
		gate_pet = TRUE;
	else
		gate_pet = FALSE;

	act("{M$n steps through a gate and vanishes.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	send_to_char("{MYou step through a gate and vanish.\n\r{x",ch);
	char_from_room(ch);
	char_to_room(ch,victim->in_room);

	act("{M$n has arrived through a gate.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	do_function(ch, &do_look, "auto");

	if (gate_pet) {
		act("{M$n steps through a gate and vanishes.{x",ch->pet, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
		send_to_char("{MYou step through a gate and vanish.{x\n\r",ch->pet);
		char_from_room(ch->pet);
		char_to_room(ch->pet,victim->in_room);
		act("{M$n has arrived through a gate.{x",ch->pet,NULL, NULL, NULL, NULL, NULL,NULL,TO_ROOM);
		do_function(ch->pet, &do_look, "auto");
	}

	if(MOUNTED(ch)) {
		act("{M$n steps through a gate and vanishes.{x",MOUNTED(ch),NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("{MYou step through a gate and vanish.{x\n\r",MOUNTED(ch));

		act("{M$n has arrived through a gate.{x",MOUNTED(ch),NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		do_look(MOUNTED(ch),"auto");
	}

	return TRUE;
}

SPELL_FUNC(spell_maze)
{
	int skill;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *stone;
	ROOM_INDEX_DATA *room;
	AREA_DATA *area;

	victim = (CHAR_DATA *) vo;

	stone = get_warp_stone(ch);
	if (stone) {
		act("You draw upon the power of $p.",ch, NULL, NULL,stone,NULL, NULL, NULL,TO_CHAR);
		act("$p flares brightly and vanishes!",ch, NULL, NULL,stone,NULL, NULL, NULL,TO_CHAR);
		extract_obj(stone);
	} else {
		send_to_char("You appear to be missing a required reagent.\n\r", ch);
		return FALSE;
	}

	if (saves_spell(level, victim, DAM_NONE)) {
		send_to_char("Nothing happens.\n\r", ch);
		return FALSE;
	}

	skill = get_skill(ch, gsn_maze);
	if (!(area = area_geldoff_maze) || (number_percent() >= skill)) {
		send_to_char("Your mind seems to have gotten lost in its own maze...\n\r", ch);
		ch->daze += 10 - number_range(0, skill/10);
		return FALSE;
	}

	while(!(room = get_room_index(area, number_range(1, area->top_room))));

	if (victim->fighting) stop_fighting(victim, TRUE);

	act("{WA phantasmal maze encapsulates $n!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{WA phantasmal maze appears about you!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	act("{M$n has been banished!!!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	if (victim != ch) act("{M$N has banished you!!!{x", victim, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	act("{D$n disappears in a puff of smoke!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{DYou disappear in a puff of smoke!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	victim->maze_time_left = 3;

	char_from_room(victim);
	char_to_room(victim, room);

	do_function(victim, &do_look, "");
	return TRUE;
}


SPELL_FUNC(spell_nexus)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	OBJ_DATA *portal;
	ROOM_INDEX_DATA *to_room, *from_room;
	int distance, catalyst;

	from_room = ch->in_room;

	if (!victim) {
		send_to_char("Nobody like that is around.\n\r", ch);
		return FALSE;
	}

	if (!can_gate(ch, victim))
		return FALSE;

	if (victim->in_room == ch->in_room ) {
		send_to_char("What would be the point?\n\r", ch);
		return FALSE;
	}


	to_room = victim->in_room;

	// getdistance... ln(distance)+1
	distance = 1;

	catalyst = has_catalyst(ch,NULL,CATALYST_ASTRAL,CATALYST_INVENTORY,1,CATALYST_MAXSTRENGTH);
	if(catalyst >= 0 && catalyst < distance) {
		send_to_char("You appear to be missing a required astral catalyst.\n\r", ch);
		return TRUE;
	}

	catalyst = use_catalyst(ch,NULL,CATALYST_ASTRAL,CATALYST_INVENTORY,distance,1,CATALYST_MAXSTRENGTH,TRUE);

	/* portal one */
	portal = create_object(obj_index_portal,0, TRUE);
	if (!IS_PORTAL(portal))
	{
		free_obj(portal);
		send_to_char("A portal tried to form but something went wrong.\n\r", ch);
		return TRUE;
	}

	portal->timer = 1 + level / 10;

	if( to_room->wilds && IS_SET(to_room->room2_flags, ROOM_VIRTUAL_ROOM) )
	{
		PORTAL(portal)->type = GATETYPE_WILDS;
		PORTAL(portal)->params[0] = to_room->wilds->uid;
		PORTAL(portal)->params[1] = to_room->x;
		PORTAL(portal)->params[2] = to_room->y;
		PORTAL(portal)->params[3] = 0;
		PORTAL(portal)->params[4] = 0;
	}
	else
	{
		PORTAL(portal)->type = GATETYPE_NORMAL;
		PORTAL(portal)->params[0] = to_room->area->uid;
		PORTAL(portal)->params[1] = to_room->vnum;
		PORTAL(portal)->params[2] = to_room->id[0];	// If this is a clone room, these will be set
		PORTAL(portal)->params[3] = to_room->id[1];	// otherwise, they will be 0,0
		PORTAL(portal)->params[4] = 0;
	}

	obj_to_room(portal,from_room);

	act("{B$p rises up from the ground.{x",ch, NULL, NULL,portal,NULL, NULL, NULL,TO_ROOM);
	act("{B$p rises up before you.{x",ch, NULL, NULL,portal,NULL, NULL, NULL,TO_CHAR);

	if (to_room != from_room) {

		/* portal two */
		portal = create_object(obj_index_portal,0, TRUE);
		portal->timer = 1 + level/10;

		if( from_room->wilds && IS_SET(from_room->room2_flags, ROOM_VIRTUAL_ROOM) )
		{
			PORTAL(portal)->type = GATETYPE_WILDS;
			PORTAL(portal)->params[0] = from_room->wilds->uid;
			PORTAL(portal)->params[1] = from_room->x;
			PORTAL(portal)->params[2] = from_room->y;
			PORTAL(portal)->params[3] = 0;
			PORTAL(portal)->params[4] = 0;
		}
		else
		{
			PORTAL(portal)->type = GATETYPE_NORMAL;
			PORTAL(portal)->params[0] = from_room->area->uid;
			PORTAL(portal)->params[1] = from_room->vnum;
			PORTAL(portal)->params[2] = from_room->id[0];	// If this is a clone room, these will be set
			PORTAL(portal)->params[3] = from_room->id[1];	// otherwise, they will be 0,0
			PORTAL(portal)->params[4] = 0;
		}

		obj_to_room(portal,to_room);

		act("{B$p rises from the ground.{x",to_room->people, NULL, NULL,portal, NULL, NULL,NULL,TO_ALL);
	}

/*
	if (IS_MSP(ch)) send_to_char(sound_table[ SOUND_TELEPORT ].tag, ch);

	if (IS_MSP(victim)) send_to_char(sound_table[ SOUND_TELEPORT ].tag, victim);
*/
	return TRUE;
}

SPELL_FUNC(spell_reflection)
{
	CHAR_DATA *reflection;
	char buf[MAX_STRING_LENGTH];

	if (!mob_index_reflection) {
		bug("spell_reflection: get_mob_index was null!\n\r",0);
		return FALSE;
	}

	reflection = create_mobile(mob_index_reflection, FALSE);

	free_string(reflection->short_descr);
	reflection->short_descr = str_dup(buf);

	sprintf(buf, "A transparent projection of %s is here.", ch->name);
	char_to_room(reflection, ch->in_room);

	if (IS_SET(ch->act[0], PLR_COLOUR))
		SET_BIT(reflection->act[0], PLR_COLOUR);

	ch->desc->character = reflection;
	ch->desc->original = ch;
	reflection->desc = ch->desc;
	ch->desc = NULL;

	reflection->hit = 1;
	reflection->mana = 1;
	reflection->move = 1;
	reflection->max_hit = 1;
	reflection->max_mana = 1;
	reflection->max_move = 1;

	reflection->comm = ch->comm;
	reflection->lines = ch->lines;

	send_to_char("{WYou feel different as you assume the form of your shadow.{x\n\r", reflection);
	send_to_char("Type 'return' to return to your normal body.\n\r", ch);

	act("The shadow of $n bends and warps, then detaches itself from $s body!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return TRUE;
}

SPELL_FUNC(spell_summon)
{
	CHAR_DATA *victim;

	victim = (CHAR_DATA *) vo;

	if (!victim) {
		send_to_char("Nobody like that is around.\n\r", ch);
		return FALSE;
	}

	if (IS_SET(victim->act[0], PLR_NOSUMMON)) {
		act("$N isn't allowing summons.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (victim->fighting || victim->maze_time_left > 0) {
		send_to_char("Nothing happens.\n\r", ch);
		return FALSE;
	}

	//
	// Room
	//
	//Added area_no_recall check to go with corresponding area flag - Areo 08-10-2006
	if (IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) ||
		IS_SET(victim->in_room->room_flags, ROOM_NOMAGIC) ||
		IS_SET(victim->in_room->room_flags, ROOM_SAFE) ||
		IS_SET(victim->in_room->area->area_flags, AREA_NO_RECALL)) {
		send_to_char("Your target is in a magically protected room.\n\r", ch);
		return FALSE;
	}

	if (IS_SET(ch->in_room->room_flags, ROOM_PK) || IS_SET(ch->in_room->room_flags, ROOM_CHAOTIC) || IS_SET(ch->in_room->room_flags, ROOM_ARENA)) {
		send_to_char("You can't summon players into this room.\n\r", ch);
		return FALSE;
	}

	if (!can_gate(ch, victim))
		return FALSE;

	if (victim->tot_level < ch->tot_level - 20 && !is_pk(victim)) {
		if (is_pk_safe_range(ch->in_room, 5, -1) > -1) {
			act("You have to be at least 5 rooms away from a PK area to summon $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return FALSE;
		}
	}

	if (victim->pulled_cart) {
		act("$N must first drop what $E is pulling.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	act("{R$n disappears suddenly.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	char_from_room(victim);
	char_to_room(victim, ch->in_room);
	act("{R$n arrives suddenly.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{M$n has summoned you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL,   TO_VICT);
	do_function(victim, &do_look, "auto");
	return TRUE;
}
