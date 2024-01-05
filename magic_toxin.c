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


SPELL_FUNC(spell_fatigue)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (is_affected(victim, skill) || saves_spell(level, victim,DAM_OTHER))
		return FALSE;

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = URANGE(1, level / 2, 5);
	af.location = APPLY_MOVE;
	af.modifier = -1 * (level / 10);
	af.bitvector = 0;
	af.bitvector2 = AFF2_FATIGUE;
	affect_to_char(victim, &af);
	send_to_char("Your feel your stamina slip away.\n\r", victim);
	act("$n looks extremely fatigued.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	return TRUE;
}

SPELL_FUNC(spell_gas_breath)
{
	CHAR_DATA *vch;
	CHAR_DATA *vch_next;
	int dam;

	act("$n breathes out a cloud of poisonous gas!",ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
	act("You breath out a cloud of poisonous gas.",ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);

	dam = level * 15;
	if (IS_DRAGON(ch))
		dam += dam/4;

	poison_effect(ch->in_room,level,dam/10,TARGET_ROOM);

	for (vch = ch->in_room->people; vch != NULL; vch = vch_next) {
		vch_next = vch->next_in_room;

		if (is_safe_spell(ch,vch,TRUE) || is_same_group(ch, vch) || (IS_NPC(ch) && IS_NPC(vch) &&
			(ch->fighting == vch || vch->fighting == ch)))
			continue;

		vch->set_death_type = DEATHTYPE_BREATH;

		if (saves_spell(level,vch,DAM_POISON)) {
			poison_effect(vch,level/2,dam/4,TARGET_CHAR);
			damage(ch,vch,dam/2,skill,TYPE_UNDEFINED,DAM_POISON,TRUE);
		} else {
			poison_effect(vch,level,dam,TARGET_CHAR);
			damage(ch,vch,dam,skill,TYPE_UNDEFINED,DAM_POISON,TRUE);
		}
	}
	return TRUE;
}

SPELL_FUNC(spell_paralysis)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (!victim) {
		send_to_char("No such person in sight.\n\r", ch);
		return FALSE;
	}

	if (IS_AFFECTED2(victim, AFF2_PARALYSIS)) {
		act("$N is already paralyzed.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration  = level/4 + 3;
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_PARALYSIS;
	affect_to_char(victim, &af);
	return TRUE;
}


SPELL_FUNC(spell_plague)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (saves_spell(level,victim,DAM_DISEASE) || (IS_NPC(victim) && IS_SET(victim->act[0],ACT_UNDEAD))) {
		if (ch == victim)
			send_to_char("You feel momentarily ill, but it passes.\n\r",ch);
		else
			act("$N seems to be unaffected.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level * 3/4;
	af.duration = URANGE(1,level, 5);
	af.location = APPLY_STR;
	af.modifier = -5;
	af.bitvector = AFF_PLAGUE;
	af.bitvector2 = 0;
	affect_join(victim,&af);

	send_to_char("You scream in agony as plague sores erupt from your skin.\n\r",victim);
	act("$n screams in agony as plague sores erupt from $s skin.", victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	return TRUE;
}


SPELL_FUNC(spell_poison)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (target == TARGET_OBJ) {
		obj = (OBJ_DATA *) vo;

		if (IS_FOOD(obj) || IS_FLUID_CON(obj)) {
			if (IS_OBJ_STAT(obj,ITEM_BLESS) || IS_OBJ_STAT(obj,ITEM_BURN_PROOF)) {
				act("Your spell fails to corrupt $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				return FALSE;
			}

			int poison = URANGE(1, level / 3, 99);		// Never allow this kind of applied poison to be permanent.
			
			if (IS_FOOD(obj))
			{
				if (FOOD(obj)->poison > poison)
				{
					act("$p is already sufficiently poisoned.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
					return FALSE;
				}

				FOOD(obj)->poison = poison;
			}
			else
			{
				if (FLUID_CON(obj)->poison > poison)
				{
					act("$p is already sufficiently poisoned.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
					return FALSE;
				}

				FLUID_CON(obj)->poison = poison;
			}

			act("$p is infused with poisonous vapors.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
			return TRUE;
		}

		// TODO: Fix
#if 0
		if (IS_WEAPON(obj)) {
			if (IS_WEAPON_STAT(obj,WEAPON_FLAMING) ||
				IS_WEAPON_STAT(obj,WEAPON_FROST) ||
				IS_WEAPON_STAT(obj,WEAPON_VAMPIRIC) ||
				IS_WEAPON_STAT(obj,WEAPON_VORPAL) ||
				IS_WEAPON_STAT(obj,WEAPON_SHOCKING) ||
				IS_WEAPON_STAT(obj,WEAPON_ACIDIC) ||
				IS_WEAPON_STAT(obj,WEAPON_RESONATE) ||
				IS_WEAPON_STAT(obj,WEAPON_BLAZE) ||
				IS_WEAPON_STAT(obj,WEAPON_SUCKLE) ||
				IS_OBJ_STAT(obj,ITEM_BLESS) ||
				IS_OBJ_STAT(obj,ITEM_BURN_PROOF)) {
				act("You can't seem to envenom $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				return FALSE;
			}

			if (IS_WEAPON_STAT(obj,WEAPON_POISON)) {
				act("$p is already envenomed.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
				return FALSE;
			}

			af.slot	= WEAR_NONE;
			af.where = TO_WEAPON;
			af.group = AFFGROUP_WEAPON;
			af.skill = skill;
			af.level = level / 2;
			af.duration = URANGE(1,level/8, 5);
			af.location = 0;
			af.modifier = 0;
			af.bitvector = WEAPON_POISON;
			af.bitvector2 = 0;
			affect_to_obj(obj,&af);

			act("$p is coated with deadly venom.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
			return TRUE;
		}
#endif
		act("You can't poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		return FALSE;
	}

	victim = (CHAR_DATA *) vo;
	if (!victim) {
		char buf[MAX_STRING_LENGTH];
		sprintf(buf, "spell_poison: null victim!, ch %s", ch->name);
		bug(buf, 0);
		return FALSE;
	}

	if (saves_spell(level, victim,DAM_POISON)) {
		act("$n turns slightly green, but it passes.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		send_to_char("You feel momentarily ill, but it passes.\n\r",victim);
		return FALSE;
	}

	af.slot	= obj_wear_loc;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_MAGICAL;
	af.skill = skill;
	af.level = level;
	af.duration = URANGE(1,level,5);
	af.location = APPLY_STR;
	af.modifier = -2;
	af.bitvector = AFF_POISON;
	af.bitvector2 = 0;
	affect_join(victim, &af);
	send_to_char("You feel very sick.\n\r", victim);
	act("$n looks very ill.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);

	return TRUE;
}


SPELL_FUNC(spell_stinking_cloud)
{
	OBJ_DATA *cloud;
	OBJ_DATA *obj;

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
		if (obj->pIndexData == obj_index_stinking_cloud)
			return FALSE;

	cloud = create_object(obj_index_stinking_cloud, 0, TRUE);
	cloud->timer = 4;
	cloud->level = ch->tot_level;
	obj_to_room(cloud, ch->in_room);
	act("{gA thick hazy green fog erupts!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	return TRUE;
}

SPELL_FUNC(spell_toxic_fumes)
{
	CHAR_DATA *victim;

	victim = (CHAR_DATA *) vo;
	if (!victim) {
		char buf[MAX_STRING_LENGTH];
		sprintf(buf, "spell_toxic_fumes: null victim!, ch %s", ch->name);
		bug(buf, 0);
		return FALSE;
	}

	act("{gYou are enveloped by a toxic cloud.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{gA toxic cloud forms around $n.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	if (!affect_find(victim->affected,gsk_toxic_fumes))
		toxic_fumes_effect(victim,ch);
	return TRUE;
}


SPELL_FUNC(spell_toxin_neurotoxin)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (!victim) {
		send_to_char("No such person in sight.\n\r", ch);
		return FALSE;
	}

	if (IS_AFFECTED2(victim, AFF2_NEUROTOXIN)) {
		send_to_char("The poison has no further effect on you.\n\r", victim);
		return FALSE;
	}

	chance = get_curr_stat(victim, STAT_CON);
	chance += get_curr_stat(victim, STAT_DEX) / 2;

	if (number_percent() < chance) {
		act("{YYou resist the nerve-wracking toxins flowing through your body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n resists the nerve-wracking toxins flowing through $s body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_BIOLOGICAL;
	af.skill = gsk_neurotoxin;
	af.level = victim->bitten_level;
	af.duration = number_range(5, 10);
	af.location = APPLY_INT;
	af.modifier = -5;
	af.bitvector = 0;
	af.bitvector2 = AFF2_NEUROTOXIN;
	affect_to_char(victim, &af);
	return TRUE;
}


SPELL_FUNC(spell_toxin_paralysis)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (IS_AFFECTED2(victim, AFF2_PARALYSIS)) {
		send_to_char("The poison has no further effect on you.\n\r", victim);
		return FALSE;
	}

	chance = get_curr_stat(victim, STAT_CON);
	chance += get_curr_stat(victim, STAT_DEX) / 2;

	if (number_percent() < chance) {
		act("{YYou resist the paralyzing toxins flowing through your body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n resists the paralyzing toxins flowing through $s body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	act("{G$n begins to look pale and $s flesh blanches.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{GYou feel paralyzing toxins race through your body.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_BIOLOGICAL;
	af.skill = get_skill_data("paralysis");	// TODO: gsk_paralysis
	af.level = victim->bitten_level;
	af.duration = 0;
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = 0;
	af.bitvector2 = AFF2_PARALYSIS;
	affect_to_char(victim, &af);
	return TRUE;
}


SPELL_FUNC(spell_toxin_weakness)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;
	AFFECT_DATA af;
	//bool perm = FALSE;
	memset(&af,0,sizeof(af));

	if (!victim) {
		send_to_char("No such person in sight.\n\r", ch);
		return FALSE;
	}

	if (IS_AFFECTED(victim, AFF_WEAKEN) && IS_AFFECTED2(victim, AFF2_FATIGUE)) {
		send_to_char("The poison has no further effect on you.\n\r", victim);
		return FALSE;
	}

	chance = get_curr_stat(victim, STAT_CON);
	chance += get_curr_stat(victim, STAT_DEX) / 2;

	if (number_percent() < chance) {
		act("{YYou resist the debilitating toxins flowing through your body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n resists the debilitating toxins flowing through $s body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	if (!is_affected(victim, gsk_fatigue)) {
		af.slot	= WEAR_NONE;
		af.where = TO_AFFECTS;
		af.group = AFFGROUP_BIOLOGICAL;
		af.skill = gsk_fatigue;
		af.level = level;
		af.duration = URANGE(1, level / 2, 5);
		af.location = APPLY_MOVE;
		af.modifier = -1 * (victim->max_move / 5);
		af.bitvector = 0;
		af.bitvector2 = AFF2_FATIGUE;
		affect_to_char(victim, &af);
		send_to_char("Your feel your stamina slip away.\n\r", victim);
		act("$n looks extremely fatigued.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	}

	if (!is_affected(victim, gsk_weaken)) {
		af.slot	= WEAR_NONE;
		af.where = TO_AFFECTS;
		af.group = AFFGROUP_BIOLOGICAL;
		af.skill = gsk_weaken;
		af.level = level;
		af.duration = URANGE(1, level / 2, 5);
		af.location = APPLY_STR;
		af.modifier = -1 * (level / 13);
		af.bitvector = AFF_WEAKEN;
		af.bitvector2 = 0;
		affect_to_char(victim, &af);
		send_to_char("You feel your strength slip away.\n\r", victim);
		act("$n looks tired and weak.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	}

	return TRUE;
}


SPELL_FUNC(spell_toxin_venom)
{
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	int chance;
	AFFECT_DATA af;
	memset(&af,0,sizeof(af));

	if (!victim) {
		send_to_char("No such person in sight.\n\r", ch);
		return FALSE;
	}

	chance = get_curr_stat(victim, STAT_CON);
	chance += get_curr_stat(victim, STAT_DEX) / 2;

	if (number_percent() < chance) {
		act("{YYou resist the venomous toxins flowing through your body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n resists the venomous toxins flowing through $s body!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	if (IS_AFFECTED(victim, AFF_SLEEP) || victim->level > victim->bitten_level*2) {
		send_to_char("The poison has no effect on you.\n\r", victim);
		act("The poison has no effect on $n.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return FALSE;
	}

	if (victim->fighting) stop_fighting(victim, TRUE);

	af.slot	= WEAR_NONE;
	af.where = TO_AFFECTS;
	af.group = AFFGROUP_BIOLOGICAL;
	af.skill = gsk_sleep;
	af.level = level;
	af.duration = number_range(1, 5);
	af.location = APPLY_NONE;
	af.modifier = 0;
	af.bitvector = AFF_SLEEP;
	af.bitvector2 = 0;
	affect_join(victim, &af);

	if (IS_AWAKE(victim)) {
		send_to_char("{DYou black out and collapse as the poison claims you.{x\n\r", victim);
		act("{D$n blacks out from the poison and falls asleep.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		victim->position = POS_SLEEPING;
	}
	return TRUE;
}

SPELL_FUNC(spell_withering_cloud)
{
	OBJ_DATA *cloud;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	int dir = 0;
	bool exists = FALSE;

	if (!obj_index_withering_cloud) {
		bug("spell_withering_cloud: null obj_index!\n", 0);
		return FALSE;
	}

	act("{gA dark withering cloud descends!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{gYou form an acidic withering cloud.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
		if (obj->pIndexData == obj_index_withering_cloud) {
			exists = TRUE;
			break;
		}
	}

	if (!exists) {
		cloud = create_object(obj_index_withering_cloud, 0, TRUE);
		cloud->timer = 4;
		obj_to_room(cloud, ch->in_room);
	}

	echo_around(ch->in_room, "{gA dense withering cloud coils in from nearby.{x\n\r");

	for (dir = 0; dir < MAX_DIR; dir++)
		if (ch->in_room->exit[dir] && !IS_SET(ch->in_room->exit[dir]->exit_info, EX_CLOSED)) {
			if(!(room = exit_destination(ch->in_room->exit[dir]))) continue;

			for (obj = room->contents; obj; obj = obj->next_content) {
				if (obj->pIndexData == obj_index_withering_cloud) {
					exists = TRUE;
					break;
				}
			}

			if (!exists) {
				cloud = create_object(obj_index_withering_cloud, 0, TRUE);
				cloud->timer = 4;
				obj_to_room(cloud, room);
			}
		}

	return TRUE;
}
