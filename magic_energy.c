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

SPELL_FUNC(spell_cosmic_blast)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (victim != ch) {
		act("{M$n appears to power up, then unleashes a massive blast of energy upon $N!{x", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		act("{MYou unleash a cosmic blast upon $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	dam = dice(level,8);
	damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_ENERGY,true);

	spell_blindness(gsk_blindness, 3 * level / 4, ch, (void *) victim,TARGET_CHAR, WEAR_NONE);
	return true;
}


SPELL_FUNC(spell_energy_drain)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;
	int sk;
	int maxh, maxm;

	if (victim == ch) {
		damage(ch, ch, ch->max_hit/2, skill, TYPE_UNDEFINED, DAM_NEGATIVE, true);
		return true;
	}

	dam = dice((level * 2)/5 + 1, UMAX((level * 2)/7 - 1, 6));

	if (check_immune(victim, DAM_NEGATIVE) == IS_IMMUNE)
		dam = 0;

	if (dam < 1) {
		act("{YYou try to suck life from $N, but $E seems to be immune to your dark powers!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		send_to_char("You feel a brief chilly sensation.", victim);
		return true;
	}

	sk = get_skill(ch,skill);
	sk = sk * sk / 100;	// 75 -> 56.25
	maxh = (150+sk) * ch->max_hit / 200;
	maxm = (150+sk) * ch->max_move / 200;	// Ranging from 75% to 125%

	if(ch->hit < maxh) {
		ch->hit += dam/2;
		ch->hit = UMIN(maxh, ch->hit);
	}
	if(ch->move < maxm) {
		ch->move += dam/2;
		ch->move = UMIN(maxm, ch->move);
	}

	act("{YYou feel energized as you suck the life from $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$N keels over in pain as $n strips $S life force.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("You keel over in pain as $n strips your life force.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_NEGATIVE ,true);
	return true;
}

SPELL_FUNC(spell_energy_field)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (IS_AFFECTED2(victim, AFF2_ENERGY_FIELD)) {
		if (victim == ch)
			send_to_char("You are already protected by an energy field.\n\r",ch);
		else
			act("$N is already protected by an energy field.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : (level / 6 + 4);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_ENERGY_FIELD;
	affect_to_char(victim, &af);

	act("{MThe air around $n starts to hum softly.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{MThe air around you starts to hum softly.{x\n\r", victim);
	return true;
}


SPELL_FUNC(spell_shield)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, skill)) {
		affect_strip(victim, skill);
	} else if (is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You are already shielded from harm.\n\r",ch);
		else
			act("$N is already protected by a shield.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = perm ? -1 : (8 + level);
	af.location = APPLY_AC;
	af.modifier = -20;
	af.bitvector = 0;
	af.bitvector2 = 0;
	affect_to_char(victim, &af);
	act("{W$n is surrounded by a force shield.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{WYou are surrounded by a force shield.\n\r{x", victim);
	return true;
}
