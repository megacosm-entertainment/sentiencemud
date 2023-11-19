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
		return FALSE;
	}

	if (weather_info.sky < SKY_RAINING) {
		send_to_char("You need bad weather.\n\r", ch);
		return FALSE;
	}

	dam = dice(level/2, 8);

	send_to_char("{YYou bring lightning upon your foes!{x\n\r", ch);
	act("{Y$n calls lightning to strike $s foes!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (vch = ch->in_room->people; vch != NULL; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (!is_safe(ch, vch, FALSE)) {
			if (!check_spell_deflection(ch, vch, skill, NULL)) continue;

			if (vch != ch && (IS_NPC(ch) ? !IS_NPC(vch) : IS_NPC(vch)))
				damage(ch, vch, saves_spell(level,vch,DAM_LIGHTNING) ? dam / 2 : dam, skill, TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
		}
	}

	return TRUE;
}

// Special spell deflection handler for chain lightning.
// Instead of doing anything, it will just attempt to select another target who *doesn't* have the same affect.
// If it ends up being ch, it will check their chain lightning ability to see if they get struck, otherwise it returns null.
// Returns true if there was any interaction with the spell deflection aura... due to messages being generated.
bool deflect_chain_lightning(CHAR_DATA *ch, CHAR_DATA *victim, SKILL_DATA *skill, CHAR_DATA **target)
{
	CHAR_DATA *rch = NULL;
	AFFECT_DATA *af;
	int attempts;
	int lev;

	if (!IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION))
	{
		// No interaction
		*target = victim;
		return false;
	}

	// Find spell deflection
	for (af = victim->affected; af != NULL; af = af->next) {
		if (af->skill == gsk_spell_deflection)
			break;
	}

	if (af == NULL)
	{
		// No affect, just the bit, it has *no* power, just a message
		act("{MLightning strikes and penetrates your crimson aura!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{MLightning strikes and penetrates the crimson aura around $n!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		*target = victim;
		return true;
	}

	// Actuall affect, can do some testing...
	act("{MLightning strikes your crimson aura!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{MLightning strikes crimson aura around $n!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL,lev,0,0,0,0) )
	{
		// The lightning bolt penetrates the aura
		if (ch != NULL)	{
			if (ch == victim)
				send_to_char("The lightning bolt penetrates your protective crimson aura!\n\r", ch);
			else {
				act("Your lightning bolt penetrates $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n's lightning bolt penetrates your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$n's lightning bolt penetrates $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		}
		*target = victim;
		return true;
	}

	/* it bounces to a random person */
	if (skill->target != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++)
		{
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch != NULL && rch == ch) ||
				rch == victim ||
				IS_AFFECTED2(rch, AFF2_SPELL_DEFLECTION) ||		/* Don't pick someone who has it to prevent bouncing in this mess */
				((skill->target == TAR_CHAR_OFFENSIVE ||
				skill->target == TAR_OBJ_CHAR_OFF) &&
				ch != NULL && is_safe(ch, rch, FALSE)))
			{
				rch = NULL;
				continue;
			}
		}

	// Loses potency with time
	af->level -= 10;
	if (af->level <= 0) {
		send_to_char("{MThe crimson aura around you vanishes.{x\n\r", victim);
		act("{MThe crimson aura around $n vanishes.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		affect_remove(victim, af);
		*target = victim;
		return true;
	}

	if (rch != NULL) {
		if (ch != NULL) {
			act("{YYour lightning reflects off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's lightning reflects off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$n's lightning reflects off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
	} else {
		if (ch != NULL) {
			act("{YYour lightning reflects off, fizzling out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's  lightning reflects off, fizzling out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}
	*target = rch;
	return true;
}


bool __chain_lightning(CHAR_DATA *ch, int own_skill, int level, CHAR_DATA *last_vict)
{
	CHAR_DATA *tmp_vict,*next_vict;
	bool found;
	int dam;

		/* new targets */
	while (level > 0) {
		found = FALSE;
		for (tmp_vict = ch->in_room->people; tmp_vict != NULL; tmp_vict = next_vict) {
			next_vict = tmp_vict->next_in_room;
			if (!is_safe(ch,tmp_vict,FALSE) && can_see(ch,tmp_vict) && tmp_vict != last_vict) {
				if (tmp_vict == ch && own_skill > number_percent())
					continue;
				
				CHAR_DATA *rch = tmp_vict;
				bool deflection = deflect_chain_lightning(ch, tmp_vict, gsk_chain_lightning, &rch);
				if (!rch)
				{
					// No victim, the spell fizzled out
					// Return true because messages were done.
					return true;
				}

				if (check_shield_block_projectile(ch, rch, "arc of lightning", NULL)) {
					if (number_percent() < get_skill(rch, gsk_shield_block)/4) {
						act("The bolt arcs off $n's shield and fizzles out.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
						act("The bolt arcs off your shield and fizzles out.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						return true;
					} else
						continue;
				}

				found = TRUE;
				last_vict = rch;

				if (!deflection || rch != tmp_vict)
				{
					act("The bolt arcs to $n!",tmp_vict, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
					act("The bolt hits you!",tmp_vict, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
				}
				dam = dice(level,6);

				if (saves_spell(level,tmp_vict,DAM_LIGHTNING))
					dam /= 3;

				damage(ch,tmp_vict,dam,gsk_chain_lightning,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
				shock_effect(tmp_vict,level/2,dam,TARGET_CHAR);
				level -= 10;  /* decrement damage */
			}
		}   /* end target searching loop */

		// No target found, try to hit the originator
		if (!found)
		{
			if (!ch) return true;
			if (own_skill > number_percent()) return false;

			CHAR_DATA *rch = ch;
			bool deflection = deflect_chain_lightning(ch, ch, gsk_chain_lightning, &rch);
			if (!rch)
			{
				// No victim, the spell fizzled out
				// Return true because messages were done.
				return true;
			}

			if (check_shield_block_projectile(ch, rch, "arc of lightning", NULL)) {
				if (number_percent() < get_skill(rch, gsk_shield_block)/4) {
					act("The bolt arcs off $n's shield and fizzles out.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					act("The bolt arcs off your shield and fizzles out.", rch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					return true;
				}
			}

			if (last_vict == rch) {/* no double hits */
				act("The bolt seems to have fizzled out.",rch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
				act("The bolt grounds out through your body.",rch,NULL, NULL, NULL, NULL, NULL,NULL,TO_CHAR);
				return true;
			}

			last_vict = rch;
			if (!deflection || rch == ch)
			{
				act("The bolt arcs to $n...whoops!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
				send_to_char("You are struck by your own lightning!\n\r",ch);
			}
			else
			{
				act("The bolt arcs to $n!",rch, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
				act("The bolt hits you!",rch, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
			}
			dam = dice(level,6);
			if (saves_spell(level,ch,DAM_LIGHTNING))
				dam /= 3;
			damage(ch,rch,dam,gsk_chain_lightning,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
			shock_effect(rch,level/2,dam,TARGET_CHAR);
			level -= 4;  /* decrement damage */
		}
	}

	return true;
}

SPELL_FUNC(spell_chain_lightning)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *last_vict;
	int dam;

	/* first strike */
	act("A lightning bolt leaps from $n's hand and arcs to $N.", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("A lightning bolt leaps from your hand and arcs to $N.", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("A lightning bolt leaps from $n's hand and hits you!", ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	dam = dice(level,6);
	if (saves_spell(level,victim,DAM_LIGHTNING))
		dam /= 3;

	damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
	shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	last_vict = victim;
	level -= 4;   /* decrement damage */

	// get ch's chain lightning skill
	int own_skill = get_skill(ch, gsk_chain_lightning);

	if (!__chain_lightning(ch, own_skill, level, last_vict))
	{
		act("The lightning bolt fizzles out.", ch,NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	}

	return TRUE;
}

TOUCH_FUNC(touch_chain_lightning)
{
	// get ch's chain lightning skill
	int own_skill = get_skill(ch, gsk_chain_lightning);

	act("A lightning bolt leaps from $n's $p.", ch,NULL, NULL, tattoo, NULL, NULL, NULL, TO_ROOM);
	act("A lightning bolt leaps from $p.", ch,NULL, NULL, tattoo, NULL, NULL, NULL, TO_CHAR);

	if (!__chain_lightning(ch, own_skill, level, NULL))
	{
		act("The lightning bolt fizzles out.", ch,NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	}

	return TRUE;
}


ZAP_FUNC(zap_chain_lightning)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	CHAR_DATA *last_vict;
	int dam;

	/* first strike */
	act("A lightning bolt leaps from $n's $p and arcs to $N.", ch,victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
	act("A lightning bolt leaps from $p and arcs to $N.", ch,victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("A lightning bolt leaps from $n's $p and hits you!", ch,victim, NULL, obj, NULL, NULL, NULL, TO_VICT);

	dam = dice(level,6);
	if (saves_spell(level,victim,DAM_LIGHTNING))
		dam /= 3;

	damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
	shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	last_vict = victim;
	level -= 4;   /* decrement damage */

	// get ch's chain lightning skill
	int own_skill = get_skill(ch, gsk_chain_lightning);

	if (!__chain_lightning(ch, own_skill, level, last_vict))
	{
		act("The lightning bolt fizzles out.", ch,NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	}

	return TRUE;
}

SPELL_FUNC(spell_electrical_barrier)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, skill))
		affect_strip(victim, skill);
	else if (is_affected(victim, skill)) {
		if (victim == ch)
			send_to_char("You are already surrounded by an electrical barrier.\n\r",ch);
		else
			act("$N is already surrounded by an electrical barrier.", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
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
	return TRUE;
}


SPELL_FUNC(spell_lightning_breath)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	act("$n breathes a bolt of lightning at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("$n breathes a bolt of lightning at you!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("You breathe a bolt of lightning at $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	if (check_shield_block_projectile(ch, victim, "lightning bolt", NULL))
		return FALSE;

	dam = level * 16;
	if (IS_DRAGON(ch))
		dam += dam/4;

	victim->set_death_type = DEATHTYPE_BREATH;

	if (saves_spell(level,victim,DAM_LIGHTNING)) {
		shock_effect(victim,level/2,dam/8,TARGET_CHAR);
		damage(ch,victim,dam/2,skill,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
	} else {
		shock_effect(victim,level,dam/8,TARGET_CHAR);
		damage(ch,victim,dam,skill,TYPE_UNDEFINED,DAM_LIGHTNING,TRUE);
	}
	return TRUE;
}


SPELL_FUNC(spell_lightning_bolt)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (check_shield_block_projectile(ch, victim, "lightning bolt", NULL))
		return FALSE;

	level = UMAX(0, level);
	dam = dice(level, level/6);

	if (saves_spell(level, victim, DAM_LIGHTNING))
		dam /= 2;

	dam = UMIN(dam, 2500);

	damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_LIGHTNING ,TRUE);
	shock_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	return TRUE;
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

	damage(ch, victim, dam, skill, TYPE_UNDEFINED, DAM_LIGHTNING ,TRUE);
	return TRUE;
}
