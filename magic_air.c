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

SPELL_FUNC(spell_air_pocket)
{
	return FALSE;
}

SPELL_FUNC(spell_faerie_fog)
{
	CHAR_DATA *ich;

	act("$n conjures a cloud of purple smoke.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("You conjure a cloud of purple smoke.\n\r", ch);

	for (ich = ch->in_room->people; ich; ich = ich->next_in_room) {
		if (ich->invis_level > 0)
			continue;

		if (ich == ch || saves_spell(level, ich,DAM_OTHER))
			continue;

		// Umm, O.o  Wouldn't this expose something that's hidden?
		if (!check_spell_deflection(ch, ich, skill, NULL))
			continue;

		affect_strip(ich, gsk_invis);
		affect_strip(ich, gsk_mass_invis);
		affect_strip(ich, gsk_sneak);

		REMOVE_BIT(ich->affected_by[0], AFF_HIDE);
		REMOVE_BIT(ich->affected_by[0], AFF_INVISIBLE);
		REMOVE_BIT(ich->affected_by[0], AFF_SNEAK);

		act("$n is revealed!", ich, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("You are revealed!\n\r", ich);
	}
	return TRUE;
}

SPELL_FUNC(spell_fly)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (IS_AFFECTED(victim, AFF_FLYING)) {
		if (victim == ch)
			send_to_char("You are already airborne.\n\r",ch);
		else
			act("$N doesn't need your help to fly.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : (level + 3);
	af.location = 0;
	af.modifier = 0;
	af.bitvector = AFF_FLYING;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);

	send_to_char("Your feet rise off the ground.\n\r", victim);
	act("$n's feet rise off the ground.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	return TRUE;
}


SPELL_FUNC(spell_underwater_breathing)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;

	memset(&af,0,sizeof(af));

	if(level > MAGIC_SCRIPT_SPELL)
		level -= MAGIC_SCRIPT_SPELL;
	else if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, skill))
		affect_strip(victim, skill);
	else if (is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You can already breath underwater.\n\r",ch);
		else
			act("$N can already breath underwater.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : 35;
	af.modifier = 0;
	af.location = 0;
	af.bitvector = AFF_SWIM;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("You feel a strange sensation as gills sprout behind your ears.\n\r", victim);
	if (ch != victim) act("Gills sprout from behind $N's ears.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	return TRUE;
}

SPELL_FUNC(spell_wind_of_confusion)
{
	CHAR_DATA *vch;

	send_to_char("{MYou summon forth a howling wind!{x\n\r", ch);
	act("{RA howling chaotic wind of confusion whips around you!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (vch = ch->in_room->people; vch; vch = vch->next_in_room) {
		if (!is_same_group(ch, vch) && ch->fighting && is_same_group(ch->fighting, vch)) {
			if (!check_spell_deflection(ch, vch, skill, NULL))
				continue;

			if (saves_spell(level, vch, DAM_NONE)) {
				send_to_char("You are unaffected by the spell.\n\r", vch);
				continue;
			}

			if (vch->master) {
				stop_follower(vch,TRUE);
				die_follower(vch);
			}
		}
	}
	return TRUE;
}
