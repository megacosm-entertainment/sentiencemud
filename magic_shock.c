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


SPELL_FUNC(spell_call_lightning)
{
	CHAR_DATA *vch;
	CHAR_DATA *vch_next;
	int dam;

	if (!IS_OUTSIDE(ch)) {
		send_to_char("You must be outdoors.\n\r", ch);
		return false;
	}

	if (weather_info.sky < SKY_RAINING) {
		send_to_char("You need bad weather.\n\r", ch);
		return false;
	}

	dam = dice(level/2, 8);

	send_to_char("{YYou bring lightning upon your foes!{x\n\r", ch);
	act("{Y$n calls lightning to strike $s foes!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (vch = ch->in_room->people; vch != NULL; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (!is_safe(ch, vch, false)) {
			if (!check_spell_deflection(ch, vch, sn)) continue;

			if (vch != ch && (IS_NPC(ch) ? !IS_NPC(vch) : IS_NPC(vch)))
				damage(ch, vch, saves_spell(level,vch,DAM_LIGHTNING) ? dam / 2 : dam, sn,DAM_LIGHTNING,true);
		}
	}

	return true;
}


SPELL_FUNC(spell_chain_lightning)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *tmp_vict,*last_vict,*next_vict;
	bool found;
	int dam;

	/* first strike */
	act("A lightning bolt leaps from $n's hand and arcs to $N.", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("A lightning bolt leaps from your hand and arcs to $N.", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("A lightning bolt leaps from $n's hand and hits you!", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	dam = dice(level,6);
	if (saves_spell(level,victim,DAM_LIGHTNING))
		dam /= 3;

	damage(ch,victim,dam,sn,DAM_LIGHTNING,true);
	shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	last_vict = victim;
	level -= 4;   /* decrement damage */

	/* new targets */
	while (level > 0) {
		found = false;
		for (tmp_vict = ch->in_room->people; tmp_vict != NULL; tmp_vict = next_vict) {
			next_vict = tmp_vict->next_in_room;
			if (!is_safe(ch,tmp_vict,false) && can_see(ch,tmp_vict) && tmp_vict != last_vict) {
				if (!check_spell_deflection(ch, tmp_vict, sn))
					continue;

				if (check_shield_block_projectile(ch, tmp_vict, "arc of lightning", NULL)) {
					if (number_percent() < get_skill(tmp_vict, gsn_shield_block)/4) {
						act("The bolt arcs off $n's shield and fizzles out.", tmp_vict, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
						act("The bolt arcs off your shield and fizzles out.", tmp_vict, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						level = 0;
						break;
					} else
						continue;
				}

				found = true;
				last_vict = tmp_vict;

				act("The bolt arcs to $n!",tmp_vict, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
				act("The bolt hits you!",tmp_vict, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
				dam = dice(level,6);

				if (saves_spell(level,tmp_vict,DAM_LIGHTNING))
					dam /= 3;

				damage(ch,tmp_vict,dam,sn,DAM_LIGHTNING,true);
				shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
				level -= 10;  /* decrement damage */
			}
		}   /* end target searching loop */

		if (!found) {/* no target found, hit the caster */
			if (!ch) return true;

			if (!check_spell_deflection(ch, ch, sn))
				return true;

			if (last_vict == ch) {/* no double hits */
				act("The bolt seems to have fizzled out.",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
				act("The bolt grounds out through your body.", ch,NULL, NULL, NULL, NULL, NULL,NULL,TO_CHAR);
				return true;
			}

			last_vict = ch;
			act("The bolt arcs to $n...whoops!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			send_to_char("You are struck by your own lightning!\n\r",ch);
			dam = dice(level,6);
			if (saves_spell(level,ch,DAM_LIGHTNING))
				dam /= 3;
			damage(ch,ch,dam,sn,DAM_LIGHTNING,true);
			shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
			level -= 4;  /* decrement damage */
			if (!ch) return true;
		}
	}

	return true;
}

SPELL_FUNC(spell_electrical_barrier)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn))
		affect_strip(victim, sn);
	else if (is_affected(victim, sn)) {
		if (victim == ch)
			send_to_char("You are already surrounded by an electrical barrier.\n\r",ch);
		else
			act("$N is already surrounded by an electrical barrier.", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (level / 3);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_ELECTRICAL_BARRIER;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	act("{WCrackling blue arcs of electricity whip up and around $n forming a hazy barrier.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{WYou are surrounded by an electrical barrier.\n\r{x", victim);
	return true;
}


SPELL_FUNC(spell_lightning_breath)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("$n breathes a bolt of lightning at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("$n breathes a bolt of lightning at you!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("You breathe a bolt of lightning at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	if (check_shield_block_projectile(ch, victim, "lightning bolt", NULL))
		return false;

	dam = level * 16;
	if (IS_DRAGON(ch))
		dam += dam/4;

	victim->set_death_type = DEATHTYPE_BREATH;

	if (saves_spell(level,victim,DAM_LIGHTNING)) {
		shock_effect(victim,level/2,dam/8,TARGET_CHAR);
		damage(ch,victim,dam/2,sn,DAM_LIGHTNING,true);
	} else {
		shock_effect(victim,level,dam/8,TARGET_CHAR);
		damage(ch,victim,dam,sn,DAM_LIGHTNING,true);
	}
	return true;
}


SPELL_FUNC(spell_lightning_bolt)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (check_shield_block_projectile(ch, victim, "lightning bolt", NULL))
		return false;

	level = UMAX(0, level);
	dam = dice(level, level/6);

	if (saves_spell(level, victim, DAM_LIGHTNING))
		dam /= 2;

	dam = UMIN(dam, 2500);

	damage(ch, victim, dam, sn, DAM_LIGHTNING ,true);
	shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	return true;
}


SPELL_FUNC(spell_shocking_grasp)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	level = UMAX(0, level);
	dam = dice(level, level/8);

	dam += dice(2, level/10);

	if (saves_spell(level, victim,DAM_LIGHTNING))
		dam /= 2;

	dam = UMIN(dam, 2500);

	damage(ch, victim, dam, sn, DAM_LIGHTNING ,true);
	return true;
}
