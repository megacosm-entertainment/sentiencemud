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


SPELL_FUNC(spell_blindness)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;

	memset(&af,0,sizeof(af));

	if (IS_AFFECTED(victim, AFF_BLIND) || saves_spell(level, victim, DAM_OTHER)) {
		send_to_char("Nothing happens.\n\r", ch);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.location = APPLY_HITROLL;
	af.modifier = -4;
	af.duration = 1 + level/6;
	af.bitvector = AFF_BLIND;
	af.bitvector2 = 0;
	af.slot	= obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("You are blinded!\n\r", victim);
	act("$n appears to be blinded.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);

	return true;
}


SPELL_FUNC(spell_cause_light)
{
	damage(ch, (CHAR_DATA *) vo, dice(1, 8) + level / 3, sn,DAM_HARM,true);
	return true;
}


SPELL_FUNC(spell_cause_critical)
{
	damage(ch, (CHAR_DATA *) vo, dice(3, 8) + level - 6, sn,DAM_HARM,true);
	return true;
}


SPELL_FUNC(spell_cause_serious)
{
	damage(ch, (CHAR_DATA *) vo, dice(2, 8) + level / 2, sn,DAM_HARM,true);
	return true;
}


// What exactly is this bloody thing anyway?
SPELL_FUNC(spell_colour_spray)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;

	static const int16_t dam_each[] = {
		0,
		0,  0,  0,  0,  0,	 0,  0,  0,  0,  0,
		30, 35, 40, 45, 50,	55, 55, 55, 56, 57,
		58, 58, 59, 60, 61,	61, 62, 63, 64, 64,
		65, 66, 67, 67, 68,	69, 70, 70, 71, 72,
		73, 73, 74, 75, 76,	76, 77, 78, 79, 79
	};
	int dam;

	if (check_shield_block_projectile(ch, victim, "colour spray", NULL))
		return false;

	level = UMIN(level, sizeof(dam_each)/sizeof(dam_each[0]) - 1);
	level = UMAX(0, level);
	dam = number_range(dam_each[level] / 2,  dam_each[level] * 2);
	if (saves_spell(level, victim,DAM_LIGHT))
		dam /= 2;
	else
		spell_blindness(gsn_blindness,level/2,ch,(void *) victim,TARGET_CHAR, WEAR_NONE);

	damage(ch, victim, dam, sn, DAM_LIGHT,true);
	return true;
}




SPELL_FUNC(spell_cure_blindness)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;

	if (!IS_AFFECTED(victim, AFF_BLIND) && !is_affected(victim, skill_lookup("fire breath"))) {
		if (victim == ch)
			send_to_char("You aren't blind.\n\r",ch);
		else
			act("$N doesn't appear to be blinded.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return true;
	}

	if (get_profession(ch, CLASS_CLERIC) != -1)
		chance = get_skill(ch, sn);
	else
		chance = 75;

	if (IS_AFFECTED(victim, AFF_BLIND) && number_percent() < chance) {
		affect_strip(victim, skill_lookup("blindness"));
		affect_strip(victim, skill_lookup("fire breath"));
		REMOVE_BIT(victim->affected_by, AFF_BLIND);
		send_to_char(skill_table[skill_lookup("blindness")].msg_off, victim);
		send_to_char("\n\r", victim);
		act("$n is no longer blinded.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	}
	return true;
}


SPELL_FUNC(spell_cure_critical)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int heal;

	heal = dice(3, 8) + level*2 - 6;

	victim->hit = UMIN(victim->hit + heal, victim->max_hit);

	update_pos(victim);
	send_to_char("You feel better!\n\r", victim);
	if (ch != victim)
		act("$N looks much better.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return true;
}


SPELL_FUNC(spell_cure_disease)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;

	if (!IS_AFFECTED(victim, AFF_PLAGUE)) {
		if (victim == ch)
			send_to_char("You aren't ill.\n\r",ch);
		else
		act("$N doesn't appear to be diseased.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return false;
	}

	if (get_profession(ch, CLASS_CLERIC) != -1)
		chance = get_skill(ch, sn);
	else
		chance = 75;

	if (number_percent() < chance) {
		affect_strip(victim, gsn_plague);
		// @@@NIB : 20070127 : added in case the poison is from toxic fumes as well
		if (!IS_AFFECTED(victim, AFF_PLAGUE)) {
			send_to_char(skill_table[gsn_plague].msg_off, victim);
			send_to_char("\n\r", victim);
			act("$n looks relieved as $s sores vanish.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		} else if (victim == ch)
			send_to_char("A stronger illness infects you.\n\r",ch);
		else
			act("$N suffers from a stronger illness.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	}
	return true;
}


SPELL_FUNC(spell_cure_light)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int heal;

	heal = dice(1, 8) + level / 3;
	victim->hit = UMIN(victim->hit + heal, victim->max_hit);

	update_pos(victim);
	send_to_char("You feel better!\n\r", victim);
	if (ch != victim)
		act("$N looks slightly better.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return true;
}


SPELL_FUNC(spell_cure_poison)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;

	if (!IS_AFFECTED(victim, AFF_POISON)) {
		if (victim == ch)
			send_to_char("You aren't poisoned.\n\r",ch);
		else
			act("$N doesn't appear to be poisoned.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	if (get_profession(ch, CLASS_CLERIC) != -1)
		chance = get_skill(ch, sn);
	else
		chance = 75;

	if (number_percent() < chance) {
		affect_strip(victim, gsn_poison);
		// @@@NIB : 20070127 : added in case the poison is from toxic fumes as well
		if (!IS_AFFECTED(victim, AFF_POISON)) {
			send_to_char(skill_table[gsn_poison].msg_off, victim);
			send_to_char("\n\r", victim);
			act("$n looks much better.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		} else if (victim == ch)
			send_to_char("A deeper poison courses through your veins.\n\r",ch);
		else
			act("$N suffers from a deeper poison.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	}
	return true;
}


SPELL_FUNC(spell_cure_serious)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int heal;

	heal = dice(2, 8) + level /2 ;
	victim->hit = UMIN(victim->hit + heal, victim->max_hit);

	update_pos(victim);
	send_to_char("You feel better!\n\r", victim);
	if (ch != victim)
		act("$N looks moderately better.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return true;
}

SPELL_FUNC(spell_cure_toxic)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance, helped;
	AFFECT_DATA *paf, *tox, *next;

	if (!(tox = affect_find(victim->affected,gsn_toxic_fumes))) {
		if (victim == ch)
			send_to_char("You aren't suffering from toxic fumes.\n\r",ch);
		else
			act("$N doesn't appear to be suffering from toxic fumes.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	if (get_profession(ch, CLASS_CLERIC) != -1)
		chance = get_skill(ch, sn);
	else
		chance = 75;

	if (number_percent() < chance) {
		if(IS_IMMORTAL(ch) || (!IS_SET(victim->in_room->room2_flags, ROOM_TOXIC_BOG) &&
			(victim->in_room->sector_type != SECT_TOXIC_BOG))) {
			affect_strip(victim, gsn_toxic_fumes);
			send_to_char(skill_table[gsn_toxic_fumes].msg_off, victim);
			send_to_char("\n\r", victim);
			act("$n looks much better.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		} else {
			// This is where the spell is PERMANENT and cannot be removed here.
			helped = false;
			for(paf = tox;paf;paf = next) {
				next = paf->next;
				if (paf->type == gsn_toxic_fumes) {
					if(paf->duration > 0)
						paf->duration = -paf->duration;
					else if(!paf->duration)
						paf->duration = 0;
					chance = paf->duration;
					paf->duration += number_range(1,level/10);
					if(paf->duration >= 0) paf->duration = -1;

					if(chance != paf->duration)
						helped = true;
				}
			}
			if(helped)
				act("$n looks much better.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		}
		return true;
	}
	return false;
}


SPELL_FUNC(spell_harm)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int dam;

	dam = dice(UMIN(120, level), 8);//UMAX( 20, victim->hit - dice(1,4));
	if (saves_spell(level, victim,DAM_HARM))
		dam = UMIN(50, dam / 2);
	//dam = UMIN(100, dam);
	damage(ch, victim, dam, sn, DAM_HARM ,true);
	return true;
}

SPELL_FUNC(spell_haste)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (is_affected(victim, sn) || IS_AFFECTED(victim,AFF_HASTE)) {
		if (victim == ch)
			send_to_char("You can't move any faster!\n\r",ch);
		else
			act("$N is already moving as fast as $E can.", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	if (perm) af.duration = -1;
	else if (victim == ch) af.duration  = level/2;
	else af.duration  = level/4;
	af.location  = APPLY_DEX;
	af.modifier  = 1 + (level >= 18) + (level >= 25) + (level >= 32);
	af.bitvector = AFF_HASTE;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);

	send_to_char("You feel yourself moving more quickly.\n\r", victim);
	act("$n is moving more quickly.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);

	return true;
}


SPELL_FUNC(spell_heal)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int heal;

	// Out of combat
	if (!ch->fighting) {
		if (ch->tot_level < 30)
			heal = 100;
		else
			heal = (ch->max_hit * 16)/100;
	} else {
		if (ch->tot_level < 30)
			heal = 70;
		else
			heal = (ch->max_hit * 14)/100;
	}

	victim->hit = UMIN(victim->hit + heal, victim->max_hit);

	update_pos(victim);
	send_to_char("A warm feeling fills your body.\n\r", victim);

	if (ch != victim)
	act("A warm aura surrounds $N momentarily, then fades.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	return true;
}


SPELL_FUNC(spell_healing_aura)
{
	CHAR_DATA *gch = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(gch, sn)) {
		affect_strip(gch, sn);
	} else if (IS_AFFECTED2(gch, AFF2_HEALING_AURA)) {
		if (gch == ch)
			send_to_char("You are already surrounded by a healing aura.\n\r",ch);
		else
			act("$N is already surrounded by a healing aura.",ch,gch, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_DIVINE;
	af.type = sn;
	af.level = level;
	af.duration = perm?-1:6;
	af.location = 0;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_HEALING_AURA;
	af.slot = obj_wear_loc;
	affect_to_char(gch, &af);
	send_to_char("You feel a warm glow within you.\n\r", gch);
	if (ch != gch)
		act("$N is surrounded with a warm healing aura.",ch,gch, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	return true;
}


SPELL_FUNC(spell_infravision)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn)) {
		affect_strip(victim, sn);
	} else if (IS_AFFECTED(victim, AFF_INFRARED)) {
		if (victim == ch)
			send_to_char("You can already see in the dark.\n\r",ch);
		else
			act("$N already has infravision.\n\r",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}
	act("$n's eyes glow red.\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (2 * level);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = AFF_INFRARED;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("Your eyes glow red.\n\r", victim);
	return true;
}

SPELL_FUNC(spell_invis)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	/* object invisibility */
	if (target == TARGET_OBJ) {
		obj = (OBJ_DATA *) vo;

		if (IS_OBJ_STAT(obj,ITEM_INVIS)) {
			act("$p is already invisible.",ch, NULL, NULL,obj,NULL, NULL, NULL,TO_CHAR);
			return false;
		}

		af.where = TO_OBJECT;
		af.group = AFFGROUP_ENCHANT;
		af.type	= sn;
		af.level = level;
		af.duration = level + 12;
		af.location = APPLY_NONE;
		af.modifier = 0;
		af.bitvector = ITEM_INVIS;
		af.bitvector2 = 0;
		af.slot	= WEAR_NONE;
		affect_to_obj(obj,&af);

		act("$p fades out of sight.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
		return true;
	}

	/* character invisibility */
	victim = (CHAR_DATA *) vo;

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(victim, sn))
		affect_strip(victim, sn);
	else if (IS_AFFECTED(victim, AFF_INVISIBLE) || IS_AFFECTED2(victim, AFF2_IMPROVED_INVIS))
		return false;

	act("$n fades out of existence.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : (level + 12);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = AFF_INVISIBLE;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(victim, &af);
	send_to_char("You fade out of existence.\n\r", victim);
	return true;
}


SPELL_FUNC(spell_mass_healing)
{
	CHAR_DATA *gch;
	int heal_num, refresh_num;

	heal_num = skill_lookup("heal");
	refresh_num = skill_lookup("refresh");

	for (gch = ch->in_room->people; gch; gch = gch->next_in_room) {
		if (((IS_NPC(ch) && IS_NPC(gch)) || (!IS_NPC(ch) && !IS_NPC(gch))) && can_see(ch, gch)) {
			spell_heal(heal_num,level,ch,(void *) gch,TARGET_CHAR, WEAR_NONE);
			spell_refresh(refresh_num,level,ch,(void *) gch,TARGET_CHAR, WEAR_NONE);
		}
	}
	return true;
}


SPELL_FUNC(spell_mass_invis)
{
	AFFECT_DATA af;
	CHAR_DATA *gch;
	memset(&af,0,sizeof(af));

	for (gch = ch->in_room->people; gch; gch = gch->next_in_room) {
		if (IS_AFFECTED(gch, AFF_INVISIBLE))
			continue;

		act("$n slowly fades out of existence.", gch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("You slowly fade out of existence.\n\r", gch);

		af.where = TO_AFFECTS;
		af.group = AFFGROUP_MAGICAL;
		af.type = sn;
		af.level = level/2;
		af.duration = 24;
		af.location = APPLY_NONE;
		af.modifier = 0;
		af.bitvector = AFF_INVISIBLE;
		af.bitvector2 = 0;
		af.slot	= obj_wear_loc;
		affect_to_char(gch, &af);
	}
	return true;
}


SPELL_FUNC(spell_regeneration)
{
	CHAR_DATA *gch;
	AFFECT_DATA af;
	bool perm = false;
	memset(&af,0,sizeof(af));

	gch = (CHAR_DATA *) vo;

	if (level > MAGIC_WEAR_SPELL) {
		level -= MAGIC_WEAR_SPELL;
		perm = true;
	}

	if (perm && is_affected(gch, sn))
		affect_strip(gch, sn);
	else if (is_affected(gch,sn) || IS_AFFECTED(gch, AFF_REGENERATION) || IS_AFFECTED2(gch, AFF2_HEALING_AURA)) {
		if (gch == ch)
			send_to_char("You are already affected by regeneration.\n\r",ch);
		else
			act("$N is already regenerating.",ch,gch, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return false;
	}

	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.type = sn;
	af.level = level;
	af.duration = perm ? -1 : 6;
	af.location  = 0;
	af.modifier  = 0;
	af.bitvector = AFF_REGENERATION;
	af.bitvector2 = 0;
	af.slot = obj_wear_loc;
	affect_to_char(gch, &af);
	send_to_char("You feel yourself regenerating.\n\r", gch);

	if (ch != gch) act("$N glows warmly.",ch,gch, NULL, NULL, NULL, NULL, NULL,TO_CHAR);

	return true;
}
