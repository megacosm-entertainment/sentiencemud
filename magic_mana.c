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


SPELL_FUNC(spell_cancellation)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	bool found = FALSE;
	int number_affects;
	AFFECT_DATA *af;

	level += 2;

	if ((!IS_NPC(ch) && IS_NPC(victim) &&
		!(IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim)) ||
		(IS_NPC(ch) && !IS_NPC(victim))) {
		send_to_char("You failed, try dispel magic.\n\r",ch);
		return FALSE;
	}

	act("{YA negating magical aura surrounds you.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{YA negating magical aura surrounds $n.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	number_affects = 0;
	for (af = victim->affected; af != NULL; af = af->next)
		if(af->group == AFFGROUP_MAGICAL && !af->custom_name) {
			if(check_dispel(ch, victim, af->type)) found = TRUE;
			number_affects++;
		}

	if (!number_affects) {
		if (ch != victim)
			act("$N has no magic affecting $M.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
			send_to_char("There is nothing affecting you.\n\r", ch);

		return FALSE;
	}

	return found;
}

SPELL_FUNC(spell_channel)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (victim->mana < 1) {
		act("{Y$N doesn't have any mana to drain.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	dam = victim->mana / 20;
	dam = UMIN(dam, 200);
	dam += dice(level/4, 8);
	if ((victim->mana - dam) < 0)
		dam = victim->mana;

	//dam = (dam * victim->mana)/victim->max_mana;
	victim->mana -= dam;

	// Steal some mana?
	ch->mana += dam;


	send_to_char("{YYou feel your mana channeled away!{x\n\r",victim);
	act("{YYou feel more powerful as you channel mana from $N!{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$N magically fades as his mana is partially removed!",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	return TRUE;
}

SPELL_FUNC(spell_counter_spell)
{
	int mana;
	CHAR_DATA *victim;

	victim = (CHAR_DATA *) vo;

	if (victim->cast <= 0)
		return FALSE;

	sn = victim->cast_sn;
	if (number_percent() < get_skill(ch, gsn_counterspell) && can_see(ch, victim)) {
		stop_casting(victim, FALSE);
		act("{YYour magic fizzles and backfires!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n's magic fizzles and backfires!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		mana = skill_table[sn].min_mana;

		target = TARGET_NONE;
		switch (skill_table[sn].target) {
		default:
			bug("Do_cast: bad target for sn %d.", sn);
			return TRUE;

		case TAR_IGNORE:
			return TRUE;
			break;

		case TAR_IGNORE_CHAR_DEF:
			return TRUE;
			break;

		case TAR_CHAR_OFFENSIVE:
			if (!IS_NPC(ch)) {
				if (is_safe(ch, victim, TRUE) && victim != ch &&
					!IS_SET(ch->in_room->room2_flags, ROOM_MULTIPLAY)) {
					send_to_char("Not on that target.\n\r",ch);
					return TRUE;
				}
			}

			if (IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim) {
				send_to_char("You can't do that on your own follower.\n\r", ch);
				return TRUE;
			}

			vo = (void *) victim;
			target = TARGET_CHAR;
			break;

		case TAR_CHAR_SELF:
			vo = (void *) victim;
			target = TARGET_CHAR;
			break;
		}

		victim->mana -= mana/3;
		ch->mana -= (mana * 2)/3;
		(*skill_table[sn].spell_fun)(sn, 3 * ch->tot_level/4, victim, vo, target, WEAR_NONE);
	} else
		stop_casting(victim, TRUE);

	return TRUE;
}


SPELL_FUNC(spell_discharge)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	AFFECT_DATA *paf;
	AFFECT_DATA *paf_next;
	int result;
	int number = 0;

	if (obj->wear_loc != -1) {
		send_to_char("The item must be carried to be discharged.\n\r",ch);
		return FALSE;
	}

	if (!obj->affected) {
		act("$p has no magical affects which you can strip.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (IS_SET(obj->extra[1], ITEM_NO_DISCHARGE)) {
		act("$p's magic cannot be removed from it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	for (paf = obj->affected; paf; paf = paf->next) number++;

	result = number_range(1, number);
	number = 0;
	for (paf = obj->affected; paf; paf = paf_next) {
		paf_next = paf->next;
		if (++number == result) {
			act("$p glows brightly, then fades.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
			affect_remove_obj(obj,paf);
			break;
		}
	}

	return TRUE;
}


SPELL_FUNC(spell_dispel_magic)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	bool found = FALSE;
	int number_affects;
	int old;
	AFFECT_DATA *af, *af_next;

	act("{YA negating aura of magic surrounds $n.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YA negating aura of magic surrounds you.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	number_affects = 0;
	for (af = victim->affected; af != NULL; af = af_next) {
		af_next = af->next;
		if((af->group == AFFGROUP_DIVINE || af->group == AFFGROUP_MAGICAL || af->group == AFFGROUP_MENTAL) &&
			!af->custom_name) {
			if(check_dispel(ch, victim, af->type)) found = TRUE;
			number_affects++;
		}
	}

	old = victim->tempstore[3];
	victim->tempstore[3] = found ? 1 : 0;
	if(!p_percent_trigger(victim,NULL,NULL,NULL,ch,NULL,NULL, NULL, NULL,TRIG_SPELL_DISPEL, NULL,0,0,0,0,0) && !number_affects) {
		act("$N has no magic affecting $M.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	found = victim->tempstore[3] ? TRUE : FALSE;
	victim->tempstore[3] = old;

	if(!found && number_affects > 0) {
		act("The magic on $N is too strong for you to dispel.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	return TRUE;
}

SPELL_FUNC(spell_dispel_room)
{
	OBJ_DATA *obj = NULL;
	EXIT_DATA *pexit = NULL;
	ROOM_INDEX_DATA *pRoom = NULL;
	ROOM_INDEX_DATA *rev_pRoom;
	int index;
	bool exists = FALSE;
	char buf[MAX_STRING_LENGTH];
	char buf2[MAX_STRING_LENGTH];

	act("{YThe air around you sparkles mysteriously.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{YThe air around you sparkles mysteriously.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	pRoom = ch->in_room;
	rev_pRoom = ch->in_room;

	// Dispel current room
	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content) {
		exists = FALSE;

		if (obj->item_type == ITEM_ROOM_FLAME) {
			sprintf(buf, "{DThe flames die down and disappear.{x\n\r");
			exists = TRUE;
		} else if (obj->item_type == ITEM_ROOM_DARKNESS) {
			sprintf(buf, "{YThe light returns.{x\n\r");
			exists = TRUE;
		} else if (obj->item_type == ITEM_ROOM_ROOMSHIELD) {
			sprintf(buf, "{YThe energy field shielding the room fades away.{x\n\r");
			exists = TRUE;
		} else if (obj->item_type == ITEM_STINKING_CLOUD || obj->item_type == ITEM_WITHERING_CLOUD) {
			sprintf(buf, "{gThe poisonous haze disappears.{x\n\r");
			exists = TRUE;
		}
		else if(IS_SET(obj->extra[2], ITEM_CAN_DISPEL)) {
			if(!saves_dispel(ch, NULL, obj->level))
			{
				if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SPELL_DISPEL, NULL,0,0,0,0,0))
				{
					obj_from_room(obj);
					extract_obj(obj);
				}
			}
		}


		if (exists && !saves_dispel(ch, NULL, obj->level)) {
			room_echo(pRoom, buf);
			obj_from_room(obj);
			extract_obj(obj);
		}
	}

	/* dispel surrounding rooms as well */
	for (index = 0; index < MAX_DIR; index++) {
		pexit = ch->in_room->exit[ index ];
		if (pexit && ((pRoom = pexit->u1.to_room)) &&
			!IS_SET(pexit->exit_info, EX_CLOSED)) {
			for (obj = pRoom->contents; obj; obj = obj->next_content)  {
				exists = FALSE;
				if (obj->item_type == ITEM_ROOM_FLAME) {
					sprintf(buf, "{DThe flames die down and disappear.{x\n\r");
					sprintf(buf2, "{YYou hear the hissing sound of dying flames from the %s.{x\n\r",	dir_name[ index ]);
					exists = TRUE;
				} else if (obj->item_type == ITEM_ROOM_DARKNESS) {
					sprintf(buf, "{YThe light returns.{x\n\r");
					sprintf(buf2, "{YThe room to the %s suddenly seems brighter.{x\n\r",	dir_name[ index ]);
					exists = TRUE;
				} else if (obj->item_type == ITEM_ROOM_ROOMSHIELD) {
					sprintf(buf, "{YThe energy field shielding the room fades away.{x\n\r");
					sprintf(buf2, "{YYou hear the crackling sound of negation from the %s.{x\n\r",	dir_name[ index ]);
					exists = TRUE;
				} else if (obj->item_type == ITEM_STINKING_CLOUD || obj->item_type == ITEM_WITHERING_CLOUD) {
					sprintf(buf, "{gThe poisonous haze disappears.{x\n\r");
					sprintf(buf2, "{YThe noxious fumes wafting in from the %s dissipate.{x\n\r",	dir_name[ index ]);
					exists = TRUE;
				}
				else if(IS_SET(obj->extra[2], ITEM_CAN_DISPEL)) {
					if(!saves_dispel(ch, NULL, obj->level))
					{
						if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SPELL_DISPEL, dir_name[ index ],0,0,0,0,0))
						{
							obj_from_room(obj);
							extract_obj(obj);
						}
					}
				}


				if (exists && !saves_dispel(ch, NULL, obj->level)) {
					room_echo(rev_pRoom, buf2);
					room_echo(pRoom, buf);
					extract_obj(obj);
				}
			}
		}
	}
	return TRUE;
}


SPELL_FUNC(spell_magic_missile)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	if (check_shield_block_projectile(ch, victim, "magic missile", NULL))
		return FALSE;

	dam = dice(level, level/9);

	damage(ch, victim, dam, sn, DAM_MAGIC ,TRUE);
	return TRUE;
}


SPELL_FUNC(spell_recharge)
{
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	int charges;

	if (obj->item_type != ITEM_WAND && obj->item_type != ITEM_STAFF &&  obj->item_type != ITEM_POTION) {
		send_to_char("That item does not carry charges.\n\r",ch);
		return FALSE;
	}

	if (obj->level > ch->tot_level) {
		act("$p's powers exceed yours.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	if (number_percent() > get_skill(ch, gsn_recharge)) {
		act("$p disappears as you screw up the spell.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n's $p disappears as $e screws up $s spell.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		extract_obj(obj);
	} else {
		if (obj->value[0] < 10) {
			act("$p's magical energies are too low to be recharged.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return FALSE;
		}

		switch (obj->item_type) {
		case ITEM_WAND:
		case ITEM_STAFF:
			obj->value[0] = (obj->value[0] * 9)/10;
			obj->value[2] = obj->value[1];
			break;

		case ITEM_POTION:
			if (get_profession(ch, SECOND_SUBCLASS_CLERIC) == CLASS_CLERIC_ALCHEMIST) {
				obj->value[0] = (obj->value[0] * 9)/10;

				if (get_skill(ch, gsn_brew) < 75)
					charges = 1;
				else if (get_skill(ch, gsn_brew) < 85)
					charges = 2;
				else
					charges = 3;

				// Throw in some randomness
				if (number_percent() < 20) {
					charges--;
					charges = UMAX(1, charges);
				}

				obj->value[5] = charges;
			} else {
				act("Only an alchemist can recharge magical potions.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				return FALSE;
			}

			break;
		}

		act("$p hisses with power as you breathe magical energy into it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	}
	return TRUE;
}

SPELL_FUNC(spell_refresh)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;

	victim->move = UMIN(victim->move + (victim->max_move/8), victim->max_move);

	if (victim->max_move == victim->move)
		send_to_char("You feel fully refreshed!\n\r",victim);
	else
		send_to_char("You feel less tired.\n\r", victim);
	if (ch != victim)
		act("$N looks less tired.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return TRUE;
}

SPELL_FUNC(spell_spell_deflection)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION)) {
		if (victim == ch)
			send_to_char("{MYou are already protected by spell deflection.{x\n\r",ch);
		else
			act("{M$N is already protected by spell deflection.{x",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : 4;
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_SPELL_DEFLECTION;
	affect_to_char(victim, &af);

	act("{MA dazzling crimson aura appears around $n.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{MA dazzling crimson aura appears around you.{x\n\r", victim);

	return TRUE;
}

SPELL_FUNC(spell_spell_shield)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = TRUE;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (IS_AFFECTED2(victim, AFF2_SPELL_SHIELD)) {
		if (victim == ch)
			send_to_char("You are already protected by a spell shield.\n\r",ch);
		else
			act("$N is already protected by a spell shield.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.slot = obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (level / 6);
	af.location  = 0;
	af.modifier  = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_SPELL_SHIELD;
	affect_to_char(victim, &af);

	act("{CA hazy blue sphere appears around $n.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{CA hazy blue sphere appears around you.{x\n\r", victim);

	return TRUE;
}


SPELL_FUNC(spell_spell_trap)
{
	OBJ_DATA *trap;

	for (trap = ch->in_room->contents; trap; trap = trap->next_content) {
		if (trap->item_type == ITEM_SPELL_TRAP) {
			act("{W$p shimmers briefly.{x",ch, NULL, NULL, trap, NULL, NULL, NULL, TO_ALL);
			trap->level += ch->level/2;
			trap->timer += number_range(0,(ch->level/30));
			return TRUE;
		}
	}

	trap = create_object(obj_index_spell_trap, level, TRUE);
	trap->timer = 4;
	trap->level = ch->tot_level;
	obj_to_room(trap, ch->in_room);

	act("{W$n forms a small glass orb in $s palm and places it on the ground.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{WYou form a small glass orb in your palm and place it on the ground.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return TRUE;
}
