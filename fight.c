/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1998 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*		Russ Taylor (rtaylor@hypercube.org)				   *
*		Gabrielle Taylor (gtaylor@hypercube.org)			   *
*		Brian Moore (zump@rom.org)					   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

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
#include <math.h>
#include "merc.h"
#include "interp.h"
#include "tables.h"
#include "magic.h"
#include "recycle.h"
#include "scripts.h"
#include "wilds.h"

#define MAX_BACKSTAB_DAMAGE 15000
#define MAX_FLEE_ATTEMPTS 10

void char_id(CHAR_DATA *ch, long *id)
{
	id[0] = ch->id[0];
	id[1] = ch->id[1];
}

bool is_combatant_valid(CHAR_DATA *ch, long id1, long id2)
{
	return IS_VALID(ch) && (ch->id[0] == id1) && (ch->id[1] == id2);
}

bool is_char_stillvalid(CHAR_DATA *ch, long *id)
{
	return IS_VALID(ch) && (ch->id[0] == id[0]) && (ch->id[1] == id[1]);
}

/*
 * Control the fights going on.
 * Called periodically by update_handler.
 */
void violence_update(void)
{
	CHAR_DATA *ch;
	CHAR_DATA *victim;
	OBJ_DATA *obj, *obj_next;
	char buf[MSL];
	ITERATOR ait;
	long aid[2], vid[2];

	// MK 100316 - Handle all combatants that are fighting to fire PREROUND before any combat is done each round.
	iterator_start(&ait, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&ait)))
	{
		if( !IS_VALID(ch) || ch->in_room == NULL) continue;

		if ((victim = ch->fighting) == NULL || !IS_VALID(victim))
			continue;

		if (!IS_AWAKE(ch) || ch->in_room != victim->in_room)
			continue;

		p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_PREROUND, NULL,0,0,0,0,0);
	}
	iterator_stop(&ait);


	iterator_start(&ait, loaded_chars);
	while(( ch = (CHAR_DATA *)iterator_nextdata(&ait)))
	{
		if( !IS_VALID(ch) ) continue;

		if (ch->in_room == NULL)
		{
			sprintf(buf, "violence_update: ch->in_room was null! %s (%ld)",
				IS_NPC(ch) ? ch->short_descr : ch->name,
				IS_NPC(ch) ? ch->pIndexData->vnum : 0);
			bug(buf, 0);
			continue;
		}

		// Regeneration code put here as it is a good pause
		if (IS_AFFECTED2(ch, AFF2_HEALING_AURA))
		{
			int heal = 0;

			heal = dice(4, 8);
			ch->hit = UMIN(ch->hit + heal, ch->max_hit);
			update_pos(ch);

			if (number_percent() == 1) {
				act("{BThe healing aura around you shimmers.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("{BThe healing aura around $n shimmers.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
		}

		// Minotaur (and other) regeneration
		if (IS_AFFECTED(ch, AFF_REGENERATION))
		{
			int heal = 0;

			heal = dice(4, 8) + ch->tot_level/6;
			ch->hit = UMIN(ch->hit + heal, ch->max_hit);

			// For slayers and werewolves, regenerate mana too:
			if (IS_SHIFTED(ch)) {
				heal = dice(4, 8) + ch->tot_level/10;
				ch->mana = UMIN(ch->mana + heal, ch->max_mana);

				heal = dice(4, 8) + ch->tot_level/8;
				ch->move = UMIN(ch->move + heal, ch->max_move);
			}
		}

		// Athletics - fast move regen
		if (get_skill(ch, gsn_athletics) > 0)
		{
			int move_gain = 0;

			move_gain = dice(4, 8) * (get_skill(ch, gsn_athletics)/100);
			ch->move = UMIN(ch->move + move_gain, ch->max_move);
			if (number_percent() == 1)
			check_improve(ch, gsn_athletics, TRUE, 50);
		}

		if ((victim = ch->fighting) == NULL || ch->in_room == NULL)
			continue;

		aid[0] = ch->id[0]; aid[1] = ch->id[1];
		vid[0] = victim->id[0]; vid[1] = victim->id[1];

		if (IS_AWAKE(ch) && ch->in_room == victim->in_room)
			multi_hit(ch, victim, TYPE_UNDEFINED);

		if(!is_combatant_valid(victim, vid[0], vid[1])) continue;

		if (ch->in_room != victim->in_room) {
			stop_fighting(ch, TRUE);
			continue;
		}


		if(!is_combatant_valid(ch, aid[0], aid[1])) continue;

		check_assist(ch,victim);

		p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_FIGHT, NULL,0,0,0,0,0);
		p_hprct_trigger(ch, victim,0,0,0,0,0);

		for (obj = ch->carrying; obj; obj = obj_next)
		{
			obj_next = obj->next_content;

			if (obj->wear_loc != WEAR_NONE)
			p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_FIGHT, NULL,0,0,0,0,0);
		}

		p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, victim, NULL, NULL, NULL, TRIG_FIGHT, NULL,0,0,0,0,0);
	}
	iterator_stop(&ait);

}


void check_assist(CHAR_DATA *ch, CHAR_DATA *victim)
{
	CHAR_DATA *rch, *rch_next;

	if(!IS_VALID(ch) || !IS_VALID(victim) || ch->in_room == NULL || victim->in_room == NULL) return;

	for (rch = ch->in_room->people; rch != NULL; rch = rch_next)
	{
	rch_next = rch->next_in_room;

	// Syn - can_see added so blinded characters can't assist.
	if (IS_AWAKE(rch) && rch->fighting == NULL && can_see(rch,victim))
	{
		// quick check for ASSIST_PLAYER
		if (!IS_NPC(ch) && IS_NPC(rch)
		&& IS_SET(rch->off_flags,ASSIST_PLAYERS)
		&&  rch->tot_level + 6 > victim->tot_level)
		{
		do_function(rch, &do_emote, "screams and attacks!");
		multi_hit(rch,victim,TYPE_UNDEFINED);
		continue;
		}

			// Don't include mounts in the fight except for crusader
		if (IS_NPC(rch) && IS_SET(rch->act[0], ACT_MOUNT))
		{
		if (rch->rider != NULL
		&&  !IS_NPC(rch->rider)
		&&  rch->rider->pcdata->second_sub_class_warrior != CLASS_WARRIOR_CRUSADER)
			continue;
		}

		// PCs next
		if (!IS_NPC(ch) || IS_AFFECTED(ch,AFF_CHARM))
		{
		if (is_same_group(ch,rch)
		&& !is_safe(rch, victim, TRUE))
			multi_hit (rch,victim,TYPE_UNDEFINED);

		continue;
		}

		// now check the NPC cases
 		if (IS_NPC(ch) && !IS_AFFECTED(ch,AFF_CHARM))
		{
		if ((IS_NPC(rch) && IS_SET(rch->off_flags, ASSIST_NPC) && IS_NPC(ch))
		|| (IS_NPC(rch) && IS_SET(rch->off_flags,ASSIST_ALL))

		//||   (IS_NPC(rch) && rch->group && rch->group == ch->group)

		||   (IS_NPC(rch) && rch->race == ch->race
		   && IS_SET(rch->off_flags,ASSIST_RACE))

		||   (IS_NPC(rch) && IS_SET(rch->off_flags,ASSIST_ALIGN)
		   &&   ((IS_GOOD(rch)    && IS_GOOD(ch))
			 ||  (IS_EVIL(rch)    && IS_EVIL(ch))
			 ||  (IS_NEUTRAL(rch) && IS_NEUTRAL(ch))))

		||   (rch->pIndexData == ch->pIndexData
		   && IS_SET(rch->off_flags,ASSIST_VNUM)))

	   	{ /* Syn - none of this shit is necesarry since we know "ch"
			 is the person being defended. The "For" loop below
			 was originally checking EVERY char in the char list, possibly
			 eating CPU time. I have left this code, albeit commented out,
			 in case something goes dodgy.
			CHAR_DATA *vch;
			CHAR_DATA *target;
			int number; */

			if (number_bits(1) == 0)
			continue;

			/*
			target = NULL;
			number = 0;
			for (vch = ch->in_room->people; vch; vch = vch->next_in_room)
			{
			if (can_see(rch,vch)
			&&  is_same_group(vch,victim)
			&&  number_range(0,number) == 0)
			{
				target = vch;
				number++;
			}
			}*/

			do_function(rch, &do_emote, "screams and attacks!");
			set_fighting(rch, victim);
			multi_hit(rch,victim,TYPE_UNDEFINED);
		}
		}
	}
	}
}

bool select_weapon(CHAR_DATA *ch)
{
	int chance;

	// HAS TWO WEAPONS
	if(get_eq_char(ch, WEAR_WIELD) && get_eq_char(ch, WEAR_SECONDARY)) {
		chance = get_skill(ch,gsn_dual) / 2;

		return number_percent() < chance;
	}

	return FALSE;
}

/*
 * Do one group of attacks.
 */
void multi_hit(CHAR_DATA *ch, CHAR_DATA *victim, int dt)
{
	int chance, rend;
	bool hand;
	long aid[2], vid[2];

	aid[0] = ch->id[0]; aid[1] = ch->id[1];
	vid[0] = victim->id[0]; vid[1] = victim->id[1];

	// Evasion skill: allows you to avoid attacks
	if (!IS_NPC(victim) && get_skill(victim,gsn_evasion) > 0 &&
		IS_AFFECTED2(victim, AFF2_EVASION) && number_percent() < (get_skill(victim, gsn_evasion) / 5)) {
		act("{Y$N gracefully evades your attack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{GYou gracefully evade $n's attack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$N gracefully evades $n's attack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		check_improve(victim, gsn_evasion, TRUE, 8);
		return;
	} else
		check_improve(victim, gsn_evasion, FALSE, 8);

	/* decrement the wait */
	if (!ch->desc) {
		ch->wait = UMAX(0,ch->wait - PULSE_VIOLENCE);
		ch->daze = UMAX(0,ch->daze - PULSE_VIOLENCE);
	}

	if (ch->position < POS_RESTING)
		return;

	if (ch->move <= 0) {
		send_to_char("You are too exhausted to fight!\n\r", ch);
		act("$n pants and sweats, too exhausted to fight!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
	}

	/* AO 092516 start a fight. This is to make sure defenses trigger combat. */
        if (ch->fighting != victim)
		set_fighting(ch,victim);

	if (IS_NPC(ch)) {
		mob_hit(ch, victim, dt);
		return;
	}


	//send_to_char("Normal", ch);
	hand = select_weapon(ch);
	one_hit(ch, victim, dt, hand);
	if(!is_combatant_valid(ch, aid[0], aid[1])) return;

	if(hand) check_improve(ch,gsn_dual,TRUE,5);

	if (IS_AFFECTED(ch, AFF_SLOW)) return;

	if (ch->fighting != victim) return;

	if(!is_combatant_valid(victim, vid[0], vid[1])) return;

	if (IS_AFFECTED(ch,AFF_HASTE) && dt != gsn_backstab) {
		hand = select_weapon(ch);
		one_hit(ch, victim, dt, hand);
		if(!is_combatant_valid(ch, aid[0], aid[1])) return;
		if(hand) check_improve(ch,gsn_dual,TRUE,5);
	}

	if(!is_combatant_valid(victim, vid[0], vid[1])) return;

	if (ch->fighting != victim || dt == gsn_backstab || dt == gsn_circle)
		return;

	chance = (get_skill(ch,gsn_second_attack)*3)/2;

	if (IS_AFFECTED2(ch,AFF2_WARCRY)) chance = chance * 3/2;

	if (number_percent() < chance) {
		hand = select_weapon(ch);
		if (one_hit(ch, victim, dt, hand)) {
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch,gsn_second_attack,TRUE,2);
			if(hand) check_improve(ch,gsn_dual,TRUE,5);
		}

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (ch->fighting != victim)
			return;
	}

	chance = get_skill(ch,gsn_third_attack)/2;

	if (IS_AFFECTED2(ch,AFF2_WARCRY)) chance = (chance * 3)/2;

	if (number_percent() < chance) {
		hand = select_weapon(ch);
		if (one_hit(ch, victim, dt, hand)) {
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch,gsn_third_attack,TRUE,2);
			if(hand) check_improve(ch,gsn_dual,TRUE,5);
		}

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (ch->fighting != victim)
			return;
	}

	/* Slayers and Werewolves get lots of hits */
	if (IS_SHIFTED(ch)) {
		if (number_percent() < get_skill(ch, gsn_shift)) {
			one_hit(ch, victim, dt, FALSE);
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch, gsn_shift, TRUE, 2);
		} else
			check_improve(ch, gsn_shift, FALSE, 2);

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (number_percent() < get_skill(ch, gsn_shift)) {
			one_hit(ch, victim, dt, FALSE);
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch, gsn_shift, TRUE, 2);
		} else
			check_improve(ch, gsn_shift, FALSE, 2);

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (number_percent() < get_skill(ch, gsn_shift)) {
			one_hit(ch, victim, dt, FALSE);
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch, gsn_shift, TRUE, 2);
		} else
			check_improve(ch, gsn_shift, FALSE, 2);

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;
	}

	// better chance of 4th attack for marauder->destroyer
	if (get_profession(ch, SUBCLASS_WARRIOR) == CLASS_WARRIOR_MARAUDER && get_profession(ch, SECOND_SUBCLASS_WARRIOR) == CLASS_WARRIOR_DESTROYER)
		chance = get_skill(ch, gsn_fourth_attack)/2;
	else
		chance = get_skill(ch, gsn_fourth_attack)/3;

	if (IS_AFFECTED2(ch,AFF2_WARCRY)) chance = (chance * 3)/2;

//	if (IS_AFFECTED(ch, AFF_SLOW)) chance = 0;	// This is moot as AFF_SLOW exits out earlier

	if (number_percent() < chance) {
		hand = select_weapon(ch);
		if (one_hit(ch, victim, dt, hand)) {
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch,gsn_fourth_attack,TRUE,2);
			if(hand) check_improve(ch,gsn_dual,TRUE,5);
		}

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (ch->fighting != victim)
			return;
	}

	chance = get_skill(ch, gsn_titanic_attack)/3;

//	if (IS_AFFECTED(ch, AFF_SLOW)) chance /= 2;	// Again, moot due to earlier exit

	if (number_percent() < chance) {
		hand = select_weapon(ch);
		if (one_hit(ch, victim, dt, hand)) {
			if(!is_combatant_valid(ch, aid[0], aid[1])) return;
			check_improve(ch,gsn_titanic_attack,TRUE,2);
			if(hand) check_improve(ch,gsn_dual,TRUE,5);
		}

		if(!is_combatant_valid(victim, vid[0], vid[1])) return;

		if (ch->fighting != victim)
			return;
	}

	chance = get_skill(ch,gsn_dual);
	if (get_eq_char(ch, WEAR_SECONDARY) && chance > 0) {
		if (number_percent() < (2*chance/3 + 33)) {
			if (one_hit(ch, victim, dt, select_weapon(ch))) {
				if(!is_combatant_valid(ch, aid[0], aid[1])) return;
				check_improve(ch,gsn_dual,TRUE,3);
			}

			if(!is_combatant_valid(victim, vid[0], vid[1])) return;

			if (ch->fighting != victim)
				return;
		}
	}

	// rending
	rend = get_skill(ch,gsn_rending);
	if (rend > 0 && !get_eq_char(ch, WEAR_WIELD) && victim->position >= POS_FIGHTING &&
		!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_REND,"pretest",0,0,0,0,0) &&
		!p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_REND,"pretest",0,0,0,0,0)) {
		int dam;

		if (number_percent() <= rend/4) {
			if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_REND,"message_pass",0,0,0,0,0)) {
				switch(number_range(1,3)) {
				case 1:
					act("{GYour sharp claws dig into $N's flesh and $E shrieks in pain!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{Y$n digs $s sharp claws into your flesh!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					break;
				case 2:
					act("{GYour raking claws cascade across $N's flesh, tearing it apart!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{Y$n's sharp claws tear your tender flesh apart!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					break;
				case 3:
					act("{GYour jagged, bony knuckles rend $N's face!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{Y$n's strikes you across the face with $s jagged bony knuckles!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					break;
				}
			}

			victim->hit_damage = URANGE(10, dice(ch->tot_level/2, 8), ch->tot_level);

			if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_REND,"damage",0,0,0,0,0)) {
				dam = victim->hit_damage;
				victim->hit_damage = 0;

				if(dam > 0) damage(ch, victim, dam, gsn_rending, DAM_SLASH, FALSE);
			} else
				victim->hit_damage = 0;
		}
	}
}


void mob_hit(CHAR_DATA *ch, CHAR_DATA *victim, int dt)
{
	int attacks;
	long aid[2];
	CHAR_DATA *vch;
	CHAR_DATA *vch_next;

	if (!IS_VALID(ch) || !ch->in_room) return;

	// No attacks ... all is scripted
	if (ch->pIndexData->attacks < 0)
		return;

	// Allows builders to set a number of attacks for their mobs. Default is 0 (normal calculations)
	if (!(attacks = ch->pIndexData->attacks))
		attacks = 1;
	else if (number_percent() < 20)
	// This is kind of a hack to thrown in here to ensure the NPC doesn't ALWAYS, 100% of the time get
	// their exact number of attacks - obviously a better way to do it would be by the NPCs fighting
	// skills - for now we will leave it semi-random.
		attacks = number_range(UMAX(attacks/2, 1), attacks);

	// +1 Haste
	if (IS_AFFECTED(ch,AFF_HASTE))
		attacks++;

	if (IS_AFFECTED(ch,AFF_SLOW))	// Mimic what is done by players
		attacks = 1;

	if (ch->fighting != victim || dt == gsn_backstab || dt == gsn_circle)
		attacks = 1;

	aid[0] = ch->id[0];	aid[1] = ch->id[1];

	for (attacks = UMAX(1, attacks); attacks != 0; attacks--) {
		one_hit(ch, victim, dt, FALSE);
		if( !is_combatant_valid(ch, aid[0], aid[1]) ) return;
		// Area attack also hits all others in the room who are fighting the mob
		if (IS_SET(ch->off_flags, OFF_AREA_ATTACK) && number_percent() <= 75)
			for (vch = ch->in_room->people; vch != NULL; vch = vch_next) {
				vch_next = vch->next_in_room;
				if (vch != victim && vch->fighting == ch) {
					one_hit(ch, vch, dt, FALSE);
					if( !is_combatant_valid(ch, aid[0], aid[1]) ) return;
				}
			}
	}

	if( !is_combatant_valid(ch, aid[0], aid[1]) ) return;

	if (ch->wait > 0)
		return;

	// Skills for the mob (specified in offensive flags)
	switch(number_range(0,8)) {
	case 0: if (IS_SET(ch->off_flags,OFF_BASH)) do_function(ch, &do_bash, ""); break;
	case 1: if (IS_SET(ch->off_flags,OFF_BERSERK) && !IS_AFFECTED(ch,AFF_BERSERK)) do_function(ch, &do_berserk, ""); break;
	case 2: if (IS_SET(ch->off_flags,OFF_DISARM) || get_weapon_sn(ch) != gsn_hand_to_hand) do_function(ch, &do_disarm, ""); break;
	case 3: if (IS_SET(ch->off_flags,OFF_KICK)) do_function(ch, &do_kick, ""); break;
	case 4: if (IS_SET(ch->off_flags,OFF_KICK_DIRT)) do_function(ch, &do_dirt, ""); break;
	case 5: break;
	case 6: break;
	case 7: break;
	case 8: if (IS_SET(ch->off_flags,OFF_BACKSTAB)) do_function(ch, &do_backstab, "");
	break;
	}
}


/*
 * Hit one guy once.
 * Returns FALSE if no hit is executed so people don't get
 * better at skills while they are casting, etc.
 */
bool one_hit(CHAR_DATA *ch, CHAR_DATA *victim, int dt, bool secondary)
{
	OBJ_DATA *wield;
	OBJ_DATA *wield2;
	int victim_ac;
	int dam;
	int diceroll;
	int sn,skill;
	int dam_type;
	int style_num;
	int style_chance;
	bool result;
	int extra_hitroll;
	int i;
	//char buf[MSL];

	sn = -1;

	if (victim == ch || !ch || !victim || !ch->in_room || !victim->in_room) return FALSE;

	if (victim->position == POS_DEAD || ch->in_room != victim->in_room) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	if (ch->cast > 0 || ch->music > 0 || ch->brew > 0 || ch->scribe > 0 ||
		ch->recite > 0 || ch->ranged > 0 || ch->bind > 0 || ch->reverie > 0 ||
		ch->trance > 0 || ch->ranged > 0 || ch->inking > 0 || ch->script_wait > 0) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	// If the attacker of one_hit is not fighting and is not what the attacker is fighting
	if( (!ch->fighting || !victim->fighting) && !set_fighting(ch, victim)) {

		// If for some reason, fighting could not be established, bail out.
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	if (!secondary) {
		wield = get_eq_char(ch, WEAR_WIELD);
		wield2 = get_eq_char(ch, WEAR_SECONDARY);
	} else {
		wield = get_eq_char(ch, WEAR_SECONDARY);
		wield2 = get_eq_char(ch, WEAR_WIELD);
	}


	if (dt == TYPE_UNDEFINED) {
		dt = TYPE_HIT;
		if (wield && wield->item_type == ITEM_WEAPON)
			dt += wield->value[3];
		else
			dt += ch->dam_type;
	}

	if (dt < TYPE_HIT)
		if (wield)
			dam_type = attack_table[wield->value[3]].damage;
		else
			dam_type = attack_table[ch->dam_type].damage;
	else
		dam_type = attack_table[dt - TYPE_HIT].damage;

	if (dam_type == -1)
		dam_type = DAM_BASH;

	/* get the weapon skill */
	if (wield)
		sn = get_weapon_sn(ch);
	else
		sn = gsn_hand_to_hand;

	// If wielding a weapon, find fighting style.
	if (wield) {
		if (wield->value[0] == WEAPON_SPEAR && (style_chance = get_skill(ch, gsn_wilderness_spear_style)) > 0) {
			style_num = style_chance / 5;
			check_improve(ch, gsn_wilderness_spear_style, TRUE, 8);
		} else if (wield && wield2 &&
			(wield->value[0] == WEAPON_SWORD || wield2->value[0] == WEAPON_SWORD) &&
			(wield->value[0] == WEAPON_DAGGER || wield2->value[0] == WEAPON_DAGGER) &&
			(style_chance = get_skill(ch, gsn_sword_and_dagger_style)) > 0) {
			style_num = style_chance / 5;
			check_improve(ch, gsn_sword_and_dagger_style, TRUE, 8);
		} else if (get_eq_char(ch, WEAR_SHIELD)) {
			style_num = get_skill(ch, gsn_shield_weapon_style) / 5;
			check_improve(ch,gsn_shield_weapon_style,TRUE,8);
		} else if (IS_WEAPON_STAT(wield, WEAPON_TWO_HANDS)) {
			style_num = get_skill(ch, gsn_two_handed_style) / 5;
			check_improve(ch,gsn_two_handed_style,TRUE,8);
		} else if (!wield2) {
			style_num = get_skill(ch, gsn_single_style) / 5;
			check_improve(ch,gsn_single_style,TRUE,8);
		} else
			style_num = 20;
	} else
		style_num = 20;

	// Do combat styles
	ch->tempstore[0] = style_num;
	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_COMBAT_STYLE, NULL,0,0,0,0,0))
		style_num = ch->tempstore[0];

	skill = style_num + (5 * get_weapon_skill(ch,sn))/6;
	if (MOUNTED(ch) && (style_chance = get_skill(ch, gsn_mount_and_weapon_style)) > 0) {
		skill += style_chance/5;
		check_improve(ch, gsn_mount_and_weapon_style, TRUE, 8);
	}

	extra_hitroll = 0;
	if (ch->leader && get_skill(ch->leader, gsn_leadership) > number_percent()) {
		extra_hitroll += 10;
		check_improve(ch->leader, gsn_leadership, TRUE, 6);
	}

	if (ch->leader && get_skill(ch->leader, gsn_warcry) > number_percent())
		extra_hitroll += 10;

	// Find which type of AC to apply
	switch(dam_type) {
	case(DAM_PIERCE):victim_ac = GET_AC(victim,AC_PIERCE);	break;
	case(DAM_BASH):	 victim_ac = GET_AC(victim,AC_BASH);	break;
	case(DAM_SLASH): victim_ac = GET_AC(victim,AC_SLASH);	break;
	default:	 victim_ac = GET_AC(victim,AC_EXOTIC);	break;
	}

	if (!can_see(ch, victim))
		victim_ac -= victim_ac/10;

	if (victim->position < POS_FIGHTING)
		victim_ac -= victim_ac/8;

	if (victim->position < POS_RESTING)
		victim_ac -= victim_ac/2;

	// The moment of excitement!
	while ((diceroll = number_bits(5)) >= 20);

	// Miss.
	// AO 010217 50% chance of missing when blind.
	if ((!diceroll || (IS_AFFECTED(ch, AFF_BLIND) && number_percent() < 50)) && (ch->tot_level > LEVEL_NEWBIE || number_percent() > 33)) {
		victim->set_death_type = DEATHTYPE_ALIVE;

		dam_message(ch, victim, -1, dt, FALSE);	// Show misses, skip all the damage BS as it's irrelevant here
		//damage(ch, victim, -1, dt, dam_type, TRUE);

		if (!IS_NPC(ch)) deduct_move(ch, 1);

		if( !victim->fighting && !ch->fighting )
		{
		}

		tail_chain();
		return TRUE;
	}

	// Hit, calc damage.
	if (IS_NPC(ch)) {
		if (!wield)
			dam = dice_roll(&ch->damage);
		else {
			dam = dice(wield->value[1], wield->value[2]) * skill/100;

			// weapon sharpness
			if (IS_WEAPON_STAT(wield,WEAPON_SHARP) && !IS_WEAPON_STAT(wield,WEAPON_DULL)) {
				int percent;

				if ((percent = number_percent()) <= (skill / 8))
				dam = dam * (100 + percent) / 50;
			}

			// weapon dullness
			if (!IS_WEAPON_STAT(wield,WEAPON_SHARP) && IS_WEAPON_STAT(wield,WEAPON_DULL)) {
				int percent;

				if ((percent = number_percent()) >= (skill / 8))
				dam = dam * (200 - percent) / 200;
			}

			dam += dice_roll(&ch->damage);
		}
	} else {
		if (sn != -1) check_improve(ch, sn, TRUE, 6);

		// Weapon damage
		if (wield) {
			dam = dice(wield->value[1], wield->value[2]) * skill/100;

			// weapon sharpness
			if (IS_WEAPON_STAT(wield,WEAPON_SHARP) && !IS_WEAPON_STAT(wield,WEAPON_DULL)) {
				int percent;

				if ((percent = number_percent()) <= (skill / 8))
				dam = dam * (100 + percent) / 50;
			}

			// weapon dullness
			if (!IS_WEAPON_STAT(wield,WEAPON_SHARP) && IS_WEAPON_STAT(wield,WEAPON_DULL)) {
				int percent;

				if ((percent = number_percent()) >= (skill / 8))
				dam = dam * (200 - percent) / 200;
			}

		// Hand-to-hand.
		} else {
			int num, type;
			if (!IS_SHIFTED(ch)) {
				num = ((ch->tot_level/12 + get_curr_stat(ch, STAT_STR)/5)*skill)/100;
				type = ((get_curr_stat(ch, STAT_STR)/8)*skill*(ch->tot_level/12))/100 + 5;
				dam = dice(num, type);
			} else {
				num = wepHitDiceTable[(int)(((ch->tot_level)*2)/5.5)].num;
				type = wepHitDiceTable[(int)(((ch->tot_level)*2)/5.5)].type;
				dam = dice(num, type) * skill/100;
			}

			// Martial arts improves hand-to-hand skill
			if ((style_chance = get_skill(ch, gsn_martial_arts)) > 0) {
				dam += dam * style_chance / 200;
				check_improve(ch,gsn_martial_arts,TRUE,6);
			}
		}
	}

	//
	// Bonuses.
	//

	// Slayers get 25% extra damage against evil
	if (IS_SLAYER(ch) && (victim->alignment < 0 || IS_CHURCH_EVIL(victim)))
		dam += (IS_NPC(victim)) ? (dam / 4) : (dam / 10);

	// Crusaders get 25% extra damage with exotic weapons
	if (get_profession(ch, SECOND_SUBCLASS_WARRIOR) == CLASS_WARRIOR_CRUSADER && wield && wield->value[0] == WEAPON_EXOTIC)
		dam += (IS_NPC(victim)) ? (dam / 4) : (dam / 10);

	// Extra damage relic
	if (ch->church && objindex_in_treasure_room(ch->church, obj_index_relic_extra_damage)) {
		dam += (IS_NPC(victim)) ? (dam / 5) : (dam / 10); // 20% extra damage
	}

	// Silver, wood, and iron vulns
	if (wield && ((IS_SET(victim->vuln_flags, VULN_SILVER) && !str_cmp(wield->material, "silver")) ||
		(IS_SET(victim->vuln_flags, VULN_WOOD) && !str_cmp(wield->material, "wood")) ||
		(IS_SET(victim->vuln_flags, VULN_IRON) && !str_cmp(wield->material, "iron"))))
		dam += (IS_NPC(victim)) ? (dam / 4) : (dam / 10);

	// Holy wrath skill
	if ((style_chance = get_skill(ch,gsn_holy_wrath)) > 0 && victim->alignment < 0) {
		if (number_percent() <= (style_chance / 3)) {
			check_improve(ch,gsn_holy_wrath, TRUE,1);
			dam += dam/10;
			send_to_char("{W({C+{W){x ", ch); // Syn: less spammy
		}
	}

	// Enhanced damage
	if ((style_chance = get_skill(ch,gsn_enhanced_damage)) > 0) {
		diceroll = number_percent();
		if (diceroll <= style_chance) {
			check_improve(ch,gsn_enhanced_damage,TRUE,1);
			dam += 2 * (dam * diceroll/300);
		}
	}

	// Boost backstab; highwayman gets it slightly better
	if (get_profession(ch, SECOND_SUBCLASS_THIEF) == CLASS_THIEF_HIGHWAYMAN) {
		if (dt == gsn_backstab && wield)
			dam *= 30;
	} else if (dt == gsn_backstab && wield)
		dam *= 25;

	if (dt == gsn_circle && wield)
		dam *= 2;

	if (GET_DAMROLL(ch) < 0)
		dam -= 40 * log(-GET_DAMROLL(ch))/100;
	else if (GET_DAMROLL(ch) > 0) // If nothing, do nothing
		dam += 40*log(GET_DAMROLL(ch)) * UMIN(100,skill) /100;


	// mobs wielding a weapon get the weapon damage + their damdice/2
	if (IS_NPC(ch))
		dam += dice_roll(&ch->pIndexData->damage) / 2;

	// apply armour class
	dam += victim_ac/10;


	// Damage boost for globals
	// Disabling boost for NPCs, this is player only - Tieryo
	if (!IS_NPC(ch)){
	if (boost_table[BOOST_DAMAGE].boost != 100) dam = (dam * boost_table[BOOST_DAMAGE].boost)/100;
};
	// deduct movement
	if (!IS_NPC(ch)) deduct_move(ch, 1);

	if (dam <= 0) dam = 1;

	victim->hit_damage = dam;
	victim->hit_type = dam_type;

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, wield, NULL, TRIG_HIT, NULL,0,0,0,0,0)) {
		tail_chain();
		return TRUE;
	}

	dam = victim->hit_damage;
	victim->hit_damage = 0;
	victim->hit_type = TYPE_UNDEFINED;


	// Check defenses against weapon attacks
	if (dt >= TYPE_HIT && ch != victim && ch->ranged <= 0) {
#define NDEFENSES	8
		bool defenses[NDEFENSES];
		int slot_defense;
		int nd;
		for(slot_defense = 0; slot_defense < NDEFENSES; slot_defense++) defenses[slot_defense] = TRUE;

		slot_defense = -1;
		//send_to_char("Defense Check\n\r", victim);

		for(nd = 0; nd < NDEFENSES; nd++) {
			int step = number_range(1,NDEFENSES);

			for(;step > 0;) {
				++slot_defense;
				if(slot_defense >= NDEFENSES) slot_defense = 0;

				if(defenses[slot_defense])
					step--;
			}

			defenses[slot_defense] = FALSE;

			switch(slot_defense) {
			case 0:
				//send_to_char("Acrobatics\n\r", victim);
				if (check_acro(ch, victim, wield)) return FALSE;
				break;

			case 1:
				//send_to_char("Catch\n\r", victim);
				if (check_catch(ch, victim, wield)) return FALSE;
				break;

			case 2:
				//send_to_char("Wilderness Spear\n\r", victim);
				if (check_spear_block(ch, victim, wield)) return FALSE;
				break;

			case 3:
				//send_to_char("Parry\n\r", victim);
				if (check_parry(ch, victim, wield)) return FALSE;
				break;

			case 4:
				//send_to_char("Dodge\n\r", victim);
				if (check_dodge(ch, victim, wield)) return FALSE;
				break;

			case 5:
				//send_to_char("Shield Block\n\r", victim);
				if (check_shield_block(ch,victim, wield)) return FALSE;
				break;

			case 6:
				//send_to_char("Swerve\n\r", victim);
				if (check_speed_swerve(ch,victim, wield)) return FALSE;
				break;

			case 7:
				//send_to_char("Custom\n\r", victim);
				if(p_percent_trigger( victim, NULL, NULL, NULL, ch, NULL, NULL, wield, NULL, TRIG_DEFENSE, NULL,0,0,0,0,0)) return FALSE;
				break;
			}

		}


	}


	result = damage_new(ch, victim, wield, dam, dt, dam_type, TRUE);
	// Lich crippling touch skill
	if ((style_chance = get_skill(ch,gsn_crippling_touch)) > 0 && wield == NULL && dam > 0 &&
		!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_CRIPPLE,"pretest",0,0,0,0,0) &&
		!p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_CRIPPLE,"pretest",0,0,0,0,0)) {
		diceroll = number_percent();
		if (diceroll <= (style_chance/10)) {
			int number_taken;

			number_taken = UMIN(10, (victim->max_hit/10 + victim->max_mana/10)/4);

			victim->hit_damage = number_taken;

			if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_CRIPPLE,"damage",0,0,0,0,0) && victim->hit_damage > 0) {
				number_taken = victim->hit_damage;

				ch->hit += number_taken;
				ch->mana += number_taken;
				if (ch->hit > ch->max_hit) ch->hit = ch->max_hit;
				if (ch->mana > ch->max_mana) ch->mana = ch->max_mana;

				victim->hit -= number_taken;
				victim->mana -= number_taken;

				if (victim->hit < 1) victim->hit = 1;
				if (victim->mana < 1) victim->mana = 1;

				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_CRIPPLE,"message_pass",0,0,0,0,0)) {
					act("{gYour crippling touch sucks life energy from $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{R$n's crippling touch sucks life energy from you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{R$n's crippling touch sucks life energy from $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				}
				check_improve(ch,gsn_crippling_touch, TRUE,1);
			}
			victim->hit_damage = 0;
		} else {
			p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_CRIPPLE,"message_fail",0,0,0,0,0);
			check_improve(ch,gsn_crippling_touch, FALSE, 1);
		}
	}

	// Special weapon flags. "Holy" flag exploits enemy's vulns.
	if (result && wield) {
		int dam;

		// Poison
		if (ch->fighting == victim && number_percent() < 5 && (IS_WEAPON_STAT(wield,WEAPON_POISON)
			|| (IS_OBJ_STAT(wield, ITEM_HOLY) && check_immune(victim, DAM_POISON) == IS_VULNERABLE))) {
			int level;
			AFFECT_DATA *poison, af;

			if (!(poison = affect_find(wield->affected,gsn_poison)))
				level = wield->level;
			else
				level = poison->level;

			if (!saves_spell(level / 2,victim,DAM_POISON)) {
				send_to_char("{gYou feel poison coursing through your veins.{x\n\r",victim);
				act("{g$n is poisoned by the venom on $p{G.{x",victim, NULL, NULL,wield, NULL, NULL,NULL,TO_ROOM);
				memset(&af,0,sizeof(af));
				af.where     = TO_AFFECTS;
				af.group     = AFFGROUP_BIOLOGICAL;
				af.type      = gsn_poison;
				af.level     = level * 3/4;
				af.duration  = URANGE(1,level / 2, 5);
				af.location  = APPLY_STR;
				af.modifier  = -1;
				af.bitvector = AFF_POISON;
				af.bitvector2 = 0;
				af.slot	= WEAR_NONE;
				affect_join(victim, &af);
			}

			// weaken the poison if it's temporary
			if (poison) {
				poison->level = UMAX(0,poison->level - 2);
				poison->duration = UMAX(0,poison->duration - 1);

				if (!poison->level || !poison->duration)
					act("The poison on $p has worn off.",ch, NULL, NULL,wield, NULL, NULL,NULL,TO_CHAR);
			}
		}

		// Vorpal
		if (ch->fighting == victim && number_range(1,1000) == 1 && IS_WEAPON_STAT(wield,WEAPON_VORPAL)) {
			act("{Y$p{Y explodes into action!{x", victim, NULL, NULL, wield, NULL, NULL, NULL, TO_ROOM);
			act("{Y$p{Y explodes into action!{x", victim, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR);

			for (i = 0; i < 3 ; i++) {
				dam = dice(wield->value[1], wield->value[2]);
				damage(ch,victim,dam,0,DAM_VORPAL,FALSE);
			}
		}

		// Vampiric
		if (ch->fighting == victim && number_percent() < 10 &&
			(IS_WEAPON_STAT(wield,WEAPON_VAMPIRIC) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_NEGATIVE) == IS_VULNERABLE))) {
			dam = number_range(1, wield->level / 5 + 1);
			act("{Y$p{Y draws life from $n.{x",victim, NULL, NULL,wield, NULL, NULL,NULL,TO_ROOM);
			act("{YYou feel $p{Y drawing your life away.{x", victim, NULL, NULL,wield, NULL, NULL,NULL,TO_CHAR);
			damage(ch,victim,dam,0,DAM_NEGATIVE,FALSE);
			ch->hit += dam/2;
		}

		// Suckle
		if (ch->fighting == victim && number_percent() < 5 &&
			(IS_WEAPON_STAT(wield,WEAPON_SUCKLE) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_NEGATIVE) == IS_VULNERABLE))) {
			dam = number_range(1, wield->level / 5 + 1);
			dam = UMIN(victim->mana,dam);
			if(dam > 2) {
				act("{B$p{B draws magical energy from $n.{x",victim, NULL, NULL,wield, NULL, NULL,NULL,TO_ROOM);
				act("{BYou feel $p{B drawing your magical energy away.{x", victim, NULL, NULL,wield, NULL, NULL,NULL,TO_CHAR);
				victim->mana -= dam;
				ch->mana += dam/2;
			}
		}

		// Flaming
		if (ch->fighting == victim && number_percent() < 3 &&
			(IS_WEAPON_STAT(wield,WEAPON_FLAMING) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_FIRE) == IS_VULNERABLE))) {
			dam = number_range(1,wield->level / 4 + 1);
			act("{R$p{R bursts into flames!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_CHAR);
			act("{R$p{R bursts into flames!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_VICT);
			act("{R$n's $p{R bursts into flames!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_NOTVICT);
			fire_effect((void *) victim,wield->level/2,dam,TARGET_CHAR);
			damage(ch,victim,dam,0,DAM_FIRE,FALSE);
		}

		// Frost
		if (ch->fighting == victim && (number_percent() < 10 &&
			(IS_WEAPON_STAT(wield,WEAPON_FROST) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_COLD) == IS_VULNERABLE)))) {
			dam = number_range(1,wield->level / 6 + 2);
			act("{B$p{B surrounds $n with ice!{x",victim, NULL, NULL,wield, NULL, NULL,NULL,TO_ROOM);
			act("{BThe cold touch of $p{B surrounds you with ice.{x", victim, NULL, NULL,wield, NULL, NULL,NULL,TO_CHAR);
			cold_effect(victim,wield->level/2,dam,TARGET_CHAR);
			damage(ch,victim,dam,0,DAM_COLD,FALSE);
		}

		// Shocking
		if (ch->fighting == victim && number_percent() < 10 &&
			(IS_WEAPON_STAT(wield,WEAPON_SHOCKING) || (IS_OBJ_STAT(wield, ITEM_HOLY)
				&& check_immune(victim, DAM_LIGHTNING) == IS_VULNERABLE))) {
			dam = number_range(1,wield->level/5 + 2);
			act("{YElectricity arcs across $p{Y towards $n!{x", victim, NULL, NULL, wield, NULL, NULL, NULL, TO_ROOM);
			act("{YYou are shocked by $p{Y!{x",victim, NULL, NULL,wield, NULL, NULL,NULL,TO_CHAR);
			shock_effect(victim,wield->level/2,dam,TARGET_CHAR);
			damage(ch,victim,dam,0,DAM_LIGHTNING,FALSE);
		}

		// Acidic
		if (ch->fighting == victim && number_percent() < 3 &&
			(IS_WEAPON_STAT(wield,WEAPON_ACIDIC) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_ACID) == IS_VULNERABLE))) {
			dam = number_range(1,wield->level / 4 + 1);
			act("{G$p{G covers you in acid!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_CHAR);
			act("{G$p{G covers $N in acid!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_VICT);
			act("{G$n's $p{G covers $N in acid!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_NOTVICT);
			acid_effect((void *) victim,wield->level/2,dam,TARGET_CHAR);
			damage(ch,victim,dam,0,DAM_ACID,FALSE);
		}

		// Resonate
		if (ch->fighting == victim && number_percent() < 10 &&
			(IS_WEAPON_STAT(wield,WEAPON_RESONATE) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_SOUND) == IS_VULNERABLE))) {
			dam = number_range(1,wield->level / 4 + 1);
			act("{C$p{C resonates a harmonic pulse!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_CHAR);
			act("{C$p{C resonates a harmonic pulse!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_VICT);
			act("{C$n's $p{C resonates a harmonic pulse!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_NOTVICT);
			damage(ch,victim,dam,0,DAM_SOUND,FALSE);
		}

		// Blaze
		if (ch->fighting == victim && number_percent() < 10 &&
			(IS_WEAPON_STAT(wield,WEAPON_BLAZE) || (IS_OBJ_STAT(wield, ITEM_HOLY) &&
				check_immune(victim, DAM_LIGHT) == IS_VULNERABLE))) {
			dam = number_range(1,wield->level / 4 + 1);
			act("{W$p{W blazes forth a blinding light!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_CHAR);
			if(!IS_AFFECTED(victim, AFF_BLIND))
				act("{W$p{W blazes forth a blinding light!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_VICT);
			act("{W$n's $p{W blazes forth a blinding light!{x", ch, victim, NULL, wield, NULL, NULL, NULL, TO_NOTVICT);
			if (number_percent() < 25 && !IS_AFFECTED(victim, AFF_BLIND) &&
				!saves_spell(wield->level, victim, DAM_OTHER)) {
				AFFECT_DATA af;
				memset(&af,0,sizeof(af));
				af.where     = TO_AFFECTS;
				af.group     = AFFGROUP_PHYSICAL;
				af.type      = gsn_blindness;
				af.level     = wield->level/2;
				af.location  = APPLY_HITROLL;
				af.modifier  = -4;
				af.duration  = 1 + wield->level/12;
				af.bitvector = AFF_BLIND;
				af.bitvector2 = 0;
				af.slot	= WEAR_NONE;
				affect_to_char(victim, &af);
				send_to_char("You are blinded!\n\r", victim);
				act("$n appears to be blinded.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			}
			damage(ch,victim,dam,0,DAM_LIGHT,FALSE);
		}
	}

	tail_chain();
	return TRUE;
}

bool damage_new(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *weapon, int dam, int dt, int dam_type, bool show)
{
	OBJ_DATA *corpse;
	OBJ_DATA *vObj;
	OBJ_DATA *obj;
	char buf[MAX_STRING_LENGTH];
	int corpse_type = RAWKILL_NORMAL;
	bool immune;
	bool kill_in_room = FALSE;
	long vid[2], cid[2];

	/* 03272020 - RH - Quick and dirty fix to bail from this function if mob has begun dying and is no longer in any room */
	if (victim->in_room == NULL){
		return FALSE;
	}

	if (victim->in_damage_function) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	if (dam < 0) {
		sprintf(buf, "damage start: negative dam, ch %s, damage %d, dt %d, dam_type %d, victim %s", HANDLE(ch), dam, dt, dam_type, HANDLE(victim));
		log_string(buf);
	}

	if (IS_NPC(victim) && IS_SET(victim->act[0], ACT_PROTECTED)) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	if (IS_AFFECTED2(victim,AFF2_PROTECTED)) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	if (victim->position == POS_DEAD) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		return FALSE;
	}

	vid[0] = victim->id[0];
	vid[1] = victim->id[1];

	cid[0] = ch->id[0];
	cid[1] = ch->id[1];

	victim->in_damage_function = TRUE;

	// sneaking doesn't wear off for rogue->ninja
	if (IS_AFFECTED(ch, AFF_SNEAK) && !(get_profession(ch, SECOND_SUBCLASS_THIEF) == CLASS_THIEF_NINJA &&
		get_profession(ch, SUBCLASS_THIEF) == CLASS_THIEF_ROGUE))
		affect_strip(ch, gsn_sneak);

	if (IS_AFFECTED2(ch, AFF2_EVASION))
		affect_strip(ch, gsn_evasion);

	// Stop up people with cheating weapons
	if (dam > 30000 && dt >= TYPE_HIT) {
		sprintf(buf, "damage: more than 30000 points(%d) from %s", dam, IS_NPC(ch) ? ch->short_descr : ch->name);
		bug(buf, 0);

		dam = 30000;
		if (!IS_IMMORTAL(ch)) {
			send_to_char("You really shouldn't cheat.\n\r", ch);

			if ((obj = get_eq_char(ch, WEAR_WIELD)))
				extract_obj(obj);
		}
	}

	// Argggh, damage reduction
	if (ch && victim) {

		if (dam < 0) {
			sprintf(buf, "damage right before reduction: negative dam, ch %s, damage %d, dt %d, dam_type %d, victim %s", HANDLE(ch), dam, dt, dam_type, HANDLE(victim));
			log_string(buf);
		}

		// PC vs NPC or NPC vs NPC
		if (IS_NPC(ch) || IS_NPC(victim)) {
			if (dam > 35) dam = (dam - 35)/3 + 35;
			if (dam > 80) dam = (dam - 80)/2 + 80;

		// PK damage
		} else {
			if (dam > 35) dam = (dam - 35)*3/4 + 35;
			if (dam > 80) dam = (dam - 80)*3/4 + 80;

			if (ch->tot_level > victim->tot_level)
				dam += (victim->tot_level - ch->tot_level);

			if (get_player_classnth(ch) == get_player_classnth(victim) &&
				abs(victim->tot_level - ch->tot_level) < 20)
				dam = dam * 6/5;

			if (dt == gsn_backstab) dam = dam * 3/2;
		}

		if (dam < 0) {
			sprintf(buf, "damage right after reduction: negative dam, ch %s, damage %d, dt %d, dam_type %d, victim %s", HANDLE(ch), dam, dt, dam_type, HANDLE(victim));
			log_string(buf);
		}
	}

	// Noobs get hurt less.
	if (victim->tot_level < 30 && !IS_REMORT(victim))
		dam /= 2;

	// Noobs in newbie-land get hurt EVEN less
	if (IS_SET(victim->in_room->area->area_flags, AREA_NEWBIE) && !IS_NPC(victim))
		dam /= 4;

	// Certain attacks are forbidden.
	if (victim != ch) {
		// Is victim safe from ch?
		if (!(!IS_NPC(ch) && IS_IMMORTAL(ch)) &&
			(is_safe(ch, victim, TRUE) || IS_SET(ch->in_room->room_flags, ROOM_SAFE) || IS_SET(victim->in_room->room_flags, ROOM_SAFE))) {
			victim->set_death_type = DEATHTYPE_ALIVE;
			victim->in_damage_function = FALSE;
			return FALSE;
		}

		if (ch->in_room == victim->in_room) {
			if (victim->position > POS_STUNNED) {
				if (!victim->fighting) {
					set_fighting(victim, ch);
					p_percent_trigger(victim, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_KILL, NULL,0,0,0,0,0);
				}

				if (victim->timer <= 4 && !IS_OBJCASTER(victim) && !IS_OBJCASTER(ch))
					victim->position = POS_FIGHTING;
			}

			if (victim->position > POS_STUNNED) {
				if (!ch->fighting)
					set_fighting(ch, victim);
			}
		}
	}

	// strip invis
	if (IS_AFFECTED(ch, AFF_INVISIBLE)) {
		affect_strip(ch, gsn_invis);
		affect_strip(ch, gsn_mass_invis);
		REMOVE_BIT(ch->affected_by[0], AFF_INVISIBLE);
		act("{C$n fades into existence.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	// Damage modifiers

	// Sanctuary = NOPE
	//if (dam > 1 && IS_AFFECTED(victim, AFF_SANCTUARY))
	//	dam /= 2;

	// Energy field reduces damage from energy/lightning by 10%
	if (dam > 1 && (dam_type == DAM_LIGHTNING || dam_type == DAM_ENERGY) &&
		IS_AFFECTED2(victim, AFF2_ENERGY_FIELD))
		dam = dam * 9/10;

	// Avatar shield reduces damage from evil by 10%
	if (dam > 1 && IS_AFFECTED2(victim, AFF2_AVATAR_SHIELD) && IS_EVIL(ch))
		dam = dam * 9/10;

	// Light shroud reduces damage from undead by 10%
	if (dam > 1 && IS_UNDEAD(ch) && IS_AFFECTED2(victim, AFF2_LIGHT_SHROUD))
		dam = dam * 9/10;

	immune = FALSE;

	// Dead people can't get hurt except by a higher level imm
	if ((IS_DEAD(victim) && !victim->fighting) && !(IS_IMMORTAL(ch) && ch->tot_level > victim->tot_level)) {
		victim->set_death_type = DEATHTYPE_ALIVE;
		victim->in_damage_function = FALSE;
		return FALSE;
	}

	// Armour and weapons decay with use
	for (vObj = victim->carrying; vObj; vObj = vObj->next_content)
		if (vObj->wear_loc != WEAR_NONE && (!IS_SET(vObj->extra[0], ITEM_BLESS || number_percent() < 33))) {
			switch(vObj->fragility) {
			case OBJ_FRAGILE_SOLID:  break;
			case OBJ_FRAGILE_STRONG:
				if (number_range(0,9999) <= 2)
					vObj->condition--;
				break;
			case OBJ_FRAGILE_NORMAL:
				if (number_range(0,9999) <= 5)
					vObj->condition--;
				break;
			case OBJ_FRAGILE_WEAK:
				if (number_range(0,9999) <= 20)
					vObj->condition--;
				break;
			default: break;
			}

			if (vObj->condition <= 0) {
				if (vObj->item_type == ITEM_WEAPON) {
					unequip_char(victim, vObj, TRUE);
					act("{y$n's $p breaks in two with a loud snap!{x", victim, NULL, NULL, vObj, NULL, NULL, NULL, TO_ROOM);
					act("{y$p splits in two with a loud snap!{x", victim, NULL, NULL, vObj, NULL, NULL, NULL, TO_CHAR);
					vObj->condition = 0;
				} else {
					extract_obj(vObj);
					act("{y$n's $p falls into pieces!{x", victim, NULL, NULL, vObj, NULL, NULL, NULL, TO_ROOM);
					act("{y$p breaks apart and crumbles!{x", victim, NULL, NULL, vObj, NULL, NULL, NULL, TO_CHAR);
				}
			}
		}

	// Apply immunity/resistant/vuln
	switch(check_immune(victim,dam_type)) {
	case(IS_IMMUNE): immune = TRUE; break;
	case(IS_RESISTANT): dam = dam * 3/4; break;		// Reduces damage by 25%
	case(IS_VULNERABLE): dam = dam * 6/5; break;		// Boosts damage by 20%
	}

	// If already immune, skip damage trigger
	if( !immune )
	{
		// @@@NIB - DAMAGE TRIGGER... This needs to be resticted
		//  This has a damage at the deepest level, the last stop before it is be applied to the victim
		victim->hit_damage = dam;
		victim->hit_type = dt;
		victim->hit_class = dam_type;

		// If the trigger returns non-zero, it will silence default messages, but not further processing of damage.
		if(p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, weapon, NULL, TRIG_DAMAGE, show?"visible":"hidden",0,0,0,0,0))
			show = FALSE;

		// Only allow reduction in damage
		if( victim->hit_damage < dam )
			dam = victim->hit_damage;

		if( dam < 0 ) dam = 0;
	}

	// READ-ONLY access to damage and damage type
	victim->hit_damage = dam;
	victim->hit_type = dt;
	victim->hit_class = dam_type;
	p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, weapon, NULL, TRIG_CHECK_DAMAGE, show?"visible":"hidden",0,0,0,0,0);

	// Reset the damage down to zero if immune
	if( immune ) dam = 0;
	victim->hit_damage = dam;


	if (show) dam_message(ch, victim, dam, dt, immune);

	if (dam <= 0) {
		victim->set_death_type = DEATHTYPE_ALIVE;

		// do a message for no damage
		victim->in_damage_function = FALSE;
		tail_chain();
		return FALSE;
	}

	// Barrier spells bounce back damage
	if (!ch->in_damage_function && dam > 0 && ch != victim && dam_type != DAM_POISON && ch->in_room == victim->in_room) {
		AFFECT_DATA *af;
		int level, bdam;

		if (IS_AFFECTED2(victim, AFF2_ELECTRICAL_BARRIER) && number_percent() < 10) {
			for (af = victim->affected; af && (af->type != gsn_electrical_barrier); af = af->next);

			level = af ? af->level : 0;
			if(level > ch->tot_level)
				level = (level + ch->tot_level + 1) / 2;
			else
				level = ch->tot_level;

			if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"electrical test",0,0,0,0,0)) {

				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"electrical message",0,0,0,0,0)) {
					act("{CElectricity arcs off $N and strikes you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{CElectricity arcs off you and strikes $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{CElectricity arcs off $N and strikes $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				}

				victim->hit_damage = dam/5 + level/2;
				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"electrical damage",0,0,0,0,0) && victim->hit_damage > 0) {
					bdam = victim->hit_damage;
					victim->hit_damage = 0;
					damage(victim, ch, bdam,gsn_electrical_barrier,DAM_LIGHTNING, TRUE);
				} else
					victim->hit_damage = 0;

				if(is_combatant_valid(victim, vid[0], vid[1]) && is_combatant_valid(ch, cid[0], cid[1]))
					p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"electrical post",0,0,0,0,0);
			}
		}

		if (IS_AFFECTED2(victim, AFF2_FIRE_BARRIER) && number_percent() < 10) {
			for (af = victim->affected; af && (af->type != gsn_fire_barrier); af = af->next);

			level = af ? af->level : 0;
			if(level > ch->tot_level)
				level = (level + ch->tot_level + 1) / 2;
			else
				level = ch->tot_level;

			if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"fire test",0,0,0,0,0)) {

				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"fire message",0,0,0,0,0)) {
					act("{RFlames bounce off the barrier surrounding $N and strike you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{RFlames bounce off the barrier surrounding you and strike $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{RFlames bounce off the barrier surrounding $N and strike $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				}

				victim->hit_damage = dam/5 + level/2;
				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"fire damage",0,0,0,0,0) && victim->hit_damage > 0) {
					bdam = victim->hit_damage;
					victim->hit_damage = 0;
					damage(victim, ch, bdam,gsn_fire_barrier,DAM_FIRE, TRUE);
				} else
					victim->hit_damage = 0;

				if(is_combatant_valid(victim, vid[0], vid[1]) && is_combatant_valid(ch, cid[0], cid[1]))
					p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"fire post",0,0,0,0,0);
			}
		}

		if (IS_AFFECTED2(victim, AFF2_FROST_BARRIER) && number_percent() < 10) {
			for (af = victim->affected; af && (af->type != gsn_frost_barrier); af = af->next);

			level = af ? af->level : 0;
			if(level > ch->tot_level)
				level = (level + ch->tot_level + 1) / 2;
			else
				level = ch->tot_level;

			if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"frost test",0,0,0,0,0)) {

				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"frost message",0,0,0,0,0)) {
					act("{BA wave of frost bounces off the barrier around $N and strikes you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{BA wave of frost bounces off the barrier around you and strikes $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{BA wave of frost bounces off the barrier around $N and strikes $n!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				}

				victim->hit_damage = dam/5 + level/2;
				if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"frost damage",0,0,0,0,0) && victim->hit_damage > 0) {
					bdam = victim->hit_damage;
					victim->hit_damage = 0;
					damage(victim, ch, bdam,gsn_frost_barrier,DAM_COLD, TRUE);
				} else
					victim->hit_damage = 0;

				if(is_combatant_valid(victim, vid[0], vid[1]) && is_combatant_valid(ch, cid[0], cid[1]))
					p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BARRIER,"frost post",0,0,0,0,0);
			}
		}
	}

	// Hurt the victim and update position.
	if (dam < 0) {
		sprintf(buf, "damage right before hurting: negative dam, ch %s, damage %d, dt %d, dam_type %d, victim %s", HANDLE(ch), dam, dt, dam_type, HANDLE(victim));
		log_string(buf);
	}

	victim->hit -= dam;

	// Check for death protections
	// Reckonings disable ALL death protections
	if(victim->hit < 1 && (pre_reckoning > 0 || reckoning_timer == 0)) {

		if (p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DEATH_PROTECTION, NULL,0,0,0,0,0)) {
			if( victim->hit < 1) victim->hit = 1;	// Allows for the script to heal
		} else if (p_percent_trigger(NULL, NULL, victim->in_room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DEATH_PROTECTION, NULL,0,0,0,0,0)) {
			if( victim->hit < 1) victim->hit = 1;

		// Protect imms
		} else if (!IS_NPC(victim) && IS_IMMORTAL(victim))
			victim->hit = 1;
	}

	update_pos(victim);

	switch(victim->position) {
	case POS_MORTAL:
		act("{R$n is mortally wounded, and will die soon, if not aided.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{RYou are mortally wounded, and will die soon, if not aided.\n\r{x", victim);
		break;

	case POS_INCAP:
		act("{R$n is incapacitated and will slowly die, if not aided.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{RYou are incapacitated and will slowly die, if not aided.\n\r{x", victim);
		break;

	case POS_STUNNED:
		act("{R$n is stunned, but will probably recover.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{RYou are stunned, but will probably recover.\n\r{x", victim);
		break;

	case POS_DEAD:
		act("{R$n is DEAD!!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("{RYou have been KILLED!!\n\r\n\r{x", victim);
		break;

	default:
		if (dam > victim->max_hit / 4) send_to_char("{RThat really did HURT!\n\r{x", victim);
		if (victim->hit < victim->max_hit / 4) send_to_char("{RYou sure are BLEEDING!\n\r{x", victim);
		break;
	}

	// This is to stop fights for the sleep spell and very wounded people (POS < sleeping).
	if (!IS_AWAKE(victim))
		stop_fighting(victim, FALSE);

	// Gain experience if victim is killed.
	if (victim->position == POS_DEAD) {
		victim->death_type = victim->set_death_type;
		if(victim->death_type == DEATHTYPE_ALIVE)
			victim->death_type = DEATHTYPE_ATTACK;
		victim->set_death_type = DEATHTYPE_ALIVE;

		if (ch != victim) {
			group_gain(ch, victim, 100);	// Full experience
			if( ch->fighting == victim )
				stop_fighting(ch, FALSE);
		}

		// If invasion mob then check if quest point is earned
		if (!IS_NPC(ch) && IS_NPC(victim) && IS_SET(victim->act[1], ACT2_INVASION_MOB) && number_percent() < 5) {
			send_to_char("{WYou have been awarded a quest point!{x\n\r", ch);
			ch->questpoints++;
		}

		// Send to logs
		if (!ch->in_wilds)
			sprintf(log_buf, "%s killed %s at %s (%ld)", (IS_NPC(ch) ? ch->short_descr : ch->name),
				HANDLE(victim), ch->in_room->name, ch->in_room->vnum);
		else
			sprintf(log_buf, "%s killed %s in wilds '%s', at %s (%ld, %ld)", (IS_NPC(ch) ? ch->short_descr : ch->name),
				HANDLE(victim), ch->in_wilds->name, ch->in_room->name, ch->in_room->x, ch->in_room->y);

		log_string(log_buf);

		// Send to wiznet
		// Show character names, even if shaped/shifted. -- Areo
		if (!ch->in_wilds)
			sprintf(log_buf, "%s killed %s at %s [room %ld]", (IS_NPC(ch) ? ch->short_descr : ch->name),
				(IS_NPC(victim) ? victim->short_descr : victim->name),
				ch->in_room->name, ch->in_room->vnum);
		else
			sprintf(log_buf, "%s killed %s in wilds '%s', at %s (%ld, %ld)",
				(IS_NPC(ch) ? ch->short_descr : ch->name),
				(IS_NPC(victim) ? victim->short_descr : victim->name),
				ch->in_wilds->name, ch->in_room->name, ch->in_room->x, ch->in_room->y);

		wiznet(log_buf,NULL,NULL,(IS_NPC(victim))?WIZ_MOBDEATHS:WIZ_DEATHS,0,0);

		// Death trigger for mobs
		{
			ROOM_INDEX_DATA *here = victim->in_room;
			victim->position = POS_STANDING;
			if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0))
				p_percent_trigger(NULL, NULL, here, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0);
		}

		// Count up PKs/monster kills/arena wins
		if (!IS_NPC(ch) && !IS_NPC(victim) && ch != victim)
			player_kill(ch, victim);
		else if (!IS_NPC(ch) && IS_NPC(victim))
			ch->monster_kills++;

		kill_in_room = (victim->in_room == ch->in_room);

		corpse_type = damage_to_corpse(dam_type);

		// Do the actual killing.
		if (dam_type == DAM_VORPAL || !victim->has_head || !IS_SET(victim->parts,PART_HEAD))
			corpse = raw_kill(victim, FALSE, TRUE, corpse_type);
		else
			corpse = raw_kill(victim, TRUE, TRUE, corpse_type);

		// Check if slain victim was part of a quest. Checks if your horse got the kill, too.
		if (!IS_NPC(ch)) {
			check_quest_slay_mob(ch, victim, true);
			check_invasion_quest_slay_mob(ch, victim);
		}

		// Hahahaha
		if (RIDDEN(ch)) check_quest_slay_mob(RIDDEN(ch), victim, true);

		// Autoloot
		if (!IS_NPC(ch) && kill_in_room && (corpse != NULL) &&
			corpse->item_type == ITEM_CORPSE_NPC && can_see_obj(ch,corpse)) {

			if (IS_SET(ch->act[0], PLR_AUTOLOOT) && corpse)
			{
				loot_corpse(ch, corpse);
			}

			// Pick up remaining gold
			if (IS_SET(ch->act[0], PLR_AUTOGOLD) && corpse && obj_has_money(ch, corpse))
				get_money_from_obj(ch, corpse);

			// Autosac corpses
			if (IS_SET(ch->act[0], PLR_AUTOSAC) && corpse) {
				 // Don't autosac corpse w/ treasure OR corpses from ranged attacks
				if ((corpse->contains && !IS_SET(ch->act[1], PLR_SACRIFICE_ALL)) || corpse->in_room != ch->in_room) {
					victim->in_damage_function = FALSE;
					return TRUE;
				} else
					sacrifice_obj(ch, corpse, "corpse");
			}
		}

		// The victim is already gone, this is just a reactive action after the kill is complete
		if(victim != ch && IS_VALID(ch) && ch->in_room) {
			bool found = FALSE;
			CHAR_DATA *foe;

			for(foe = ch->in_room->people; foe; foe = foe->next_in_room) {
				if( foe->fighting == ch ) {
					found = TRUE;
					break;
				}
			}

			if(!found) p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_AFTERKILL, NULL,0,0,0,0,0);
		}
		victim->in_damage_function = FALSE;
		return TRUE;
	}

	victim->set_death_type = DEATHTYPE_ALIVE;
	victim->in_damage_function = FALSE;

	if (victim == ch) {
		return TRUE;
	}


	// Take care of link dead newbs.
	if (!IS_NPC(victim) && victim->desc == NULL && victim->tot_level < 31 && !number_range(0, victim->wait)) {
		do_function(victim, &do_recall, "");
		return TRUE;
	}

	/*
	 * mobs automatically flee in terror if there is too large a level difference
	 * */
	if (IS_NPC(victim) && !IS_NPC(ch) && abs(ch->tot_level - victim->tot_level > 90) && number_percent() < 20) {
		char buf[MAX_STRING_LENGTH];
		sprintf(buf, "%s balks with fear at the sight of your approach!\n\r", victim->short_descr);
		send_to_char(buf,ch);
		do_function (victim, &do_flee, "");
	}

	// Wimpy - Mobiles
	if (IS_NPC(victim) && dam > 0 && victim->wait < PULSE_VIOLENCE / 2 &&
		((IS_SET(victim->act[0], ACT_WIMPY)  /* && !number_bits(2) */ && victim->hit < victim->max_hit / 3) ||
		(IS_AFFECTED(victim, AFF_CHARM) && victim->master && victim->master->in_room != victim->in_room)))
		do_function(victim, &do_flee, NULL);

	// Wimpy - Players
	if (!IS_NPC(victim) && victim->hit > 0 && victim->hit <= victim->wimpy &&
		victim->wait < PULSE_VIOLENCE / 2 && victim->paralyzed <= 0 && !IS_AFFECTED2(victim, AFF2_PARALYSIS) && !IS_AFFECTED2(victim, AFF2_IMMOBILE))
		do_function (victim, &do_flee, NULL);

	/* if fled successfully, return false so it doesn't keep position at fighting AO 092516 */
	if (victim->in_room != ch->in_room)
		return FALSE;

	tail_chain();
	return TRUE;
}

/* TRUE = show messages; FALSE = keep it quiet */
bool is_safe(CHAR_DATA *ch, CHAR_DATA *victim, bool show)
{
	CHAR_DATA *fch;

	if (victim->in_room == NULL || ch->in_room == NULL)
	return TRUE;

	// checks for kill stealing here now
	if (!IS_NPC(ch) && victim->fighting && !is_same_group(ch, victim->fighting)
	&& victim != ch && !IS_SET(ch->in_room->room2_flags, ROOM_MULTIPLAY)) {

	if (show)
		send_to_char("Kill stealing is not allowed.\n\r", ch);

	return TRUE;
	}

	// Can't kill anyone in social
	if (!str_cmp(victim->in_room->name, "Elysium")
	||   IS_SOCIAL(ch) || IS_SOCIAL(victim))
	return TRUE;

	// Immortals can attack anybody, if they have HOLYAURA on
	if (!IS_NPC(ch) && IS_IMMORTAL(ch) && IS_SET(ch->act[1],PLR_HOLYAURA))
	return FALSE;

	// Can't kill anyone's ridden mount unless you can kill them
	if (RIDDEN(victim) != NULL && is_safe(ch, RIDDEN(victim), FALSE))
	{
	if (show)
		act("$n is being ridden.", victim, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	return TRUE;
	}

	// Can always attack the person you're fighting
	if (victim->fighting == ch || victim == ch)
	return FALSE;

	if (IS_SET(victim->in_room->room_flags, ROOM_SAFE))
	{
	if (show)
		send_to_char("This room is sanctioned by the gods.\n\r", ch);

	return TRUE;
	}

	// You can't kill your own master
	if (IS_AFFECTED(ch,AFF_CHARM) && ch->master == victim)
	{
	if (show)
		act("You cannot harm your master.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);

	return TRUE;
	}

	// You can't kill someone that is protected by some affect
	if (IS_AFFECTED2(victim,AFF2_PROTECTED))
	{
	if (show) act("$N is protected by magic.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);

	return TRUE;
	}

	if(p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_PREKILL, NULL,0,0,0,0,0))
		return TRUE;

	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_PREKILL, NULL,0,0,0,0,0))
		return TRUE;

	// NPC victim
	if (IS_NPC(victim))
	{
	/* Syn- only mobs with act flag "protected" are automatically
	   protected by the gods now. All mobs such as guildmasters,
	   churchmasters, healers, bankers, etc, have been set to
	   protected if they weren't at the time of this change. */
	if (IS_SET(victim->act[0], ACT_PROTECTED))
	{
		if (show)
		act("$N is protected by the gods.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
	}

	// Shopkeepers are protected
	if (victim->shop != NULL)
	{
		if (show)
		act("You can't do that in $N's store!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
	}

	// You can't harm your own mount. this is mainly for room attacks.
	if (victim == MOUNTED(ch))
  		return TRUE;

	// NPC attacking NPC
	if (!IS_NPC(ch))
	{
		// Can't attack pets
		if (IS_SET(victim->act[0],ACT_PET))
		{
		if (show)
			act("But $N looks so cute and cuddly...", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
		}

		// Can't attack charmed characters unless you own them
		if (IS_AFFECTED(victim,AFF_CHARM) && ch != victim->master)
		{
		if (show)
			act("You can only attack $N if you are $S owner.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
		}
	}
	}
	// PC victim
	else
	{
	// Dead people cannot be attacked
	if (IS_DEAD(victim))
	{
		if (show)
		act("$N's shade is immortal.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
	}

	// NPC attacking PC
	if (IS_NPC(ch))
	{
		if (IS_AFFECTED(ch,AFF_CHARM)
		&&  ch->master != NULL
		&&  ch->master->fighting != victim)
		{
		if (show)
			send_to_char("Players are your friends!\n\r",ch);

		return TRUE;
		}
	}
	else // PC attacking PC
	{
		// If you're pulling a relic anyone can PK you
		if (victim->pulled_cart != NULL
		&&  is_relic(victim->pulled_cart->pIndexData))
		//&&  ch->church != NULL)
		return FALSE;

		// Also if any of your formation members are pulling relics, you are PKable
		for (fch = victim->in_room->people; fch != NULL; fch = fch->next_in_room)
		{
		if (fch->pulled_cart != NULL
		&&   is_relic(fch->pulled_cart->pIndexData)
		&&   is_same_group(fch, victim))
		//&&   ch->church != NULL)
			return FALSE;
		}

		// Check for autowar
		if (ch->in_war && victim->in_war)
		{
		// Genocide war?
		if (auto_war->war_type == AUTO_WAR_GENOCIDE)
		{
			if (ch->race == victim->race)
			{
			if (show)
				act("A magical power prevents your body from harming $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

			return TRUE;
			}
			else
			return FALSE;
		}
		else
		// Jihad war?
		if (auto_war->war_type == AUTO_WAR_JIHAD)
		{
			if (ch->alignment < 0
			&& (victim->alignment < 0 || IS_CHURCH_EVIL(victim)))
			{
			if (show)
				act("A magical power prevents you from harming $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

			return TRUE;
			}
			else
			if (ch->alignment > 0
			&& (victim->alignment > 0 || IS_CHURCH_GOOD(victim)))
			{
			if (show)
				act("A magical power prevents you from harming $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

			return TRUE;
			}
			else
			return FALSE;
		}
		}

		// PK rooms. Be sure that BOTH players are in a PK room for ranged attacks!
		if ((IS_SET(ch->in_room->room_flags, ROOM_PK)
			 || IS_SET(ch->in_room->room_flags, ROOM_ARENA))
		&&  (IS_SET(victim->in_room->room_flags, ROOM_PK)
			 || IS_SET(victim->in_room->room_flags, ROOM_ARENA)))
		return FALSE;

		if (IS_SET(victim->act[0],PLR_BOTTER))
		return FALSE;

		// Is it the reckoning?
		if (pre_reckoning == 0 && reckoning_timer > 0)
		{
			if (ch->tot_level <= 30 || victim->tot_level <= 30)
			{
				if (show)
					send_to_char("Only players level 31 and above are affected by 'The Reckoning'.\n\r",ch);

				return TRUE;
			}

			if (IS_SET(ch->act[1], PLR_NORECKONING))
			{
				if (show)
					act("$N has opted out of 'The Reckoning'.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

				return TRUE;
			}

			return FALSE;
		}


		if (!is_pk(ch))
		{
		if (show)
			send_to_char("You must be PK to participate in PK.\n\r", ch);

		return TRUE;
		}

		if (!is_pk(victim))
		{
		if (show)
			act("$N must be PK to participate in PK.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		return TRUE;
		}

		if (is_pk(ch) && is_pk(victim))
		return FALSE;
	}
	}

	return FALSE;
}


// Try to get rid of this function, it isn't very useful
bool is_safe_spell(CHAR_DATA *ch, CHAR_DATA *victim, bool area)
{
	if (victim->in_room == NULL || ch->in_room == NULL)
		return TRUE;

	if (victim == ch && area)
	return TRUE;

	if (victim->fighting == ch || victim == ch)
	return FALSE;

	/* This just prevents imms from testing spells. What's the harm
	   of casting spells on imms, they can't die anyway?!?! Syn
	if (IS_IMMORTAL(ch) && ch->level > LEVEL_IMMORTAL && !area)
	return FALSE;
	*/

	/* safe room? */
	if (IS_SET(victim->in_room->room_flags,ROOM_SAFE))
		return TRUE;

	/* killing mobiles */
	if (IS_NPC(victim))
	{
	if (victim->shop != NULL)
		return TRUE;

	/* no killing healers, trainers, etc */
	if (IS_SET(victim->act[0],ACT_TRAIN)
	||  IS_SET(victim->act[0],ACT_PRACTICE)
	||  IS_SET(victim->act[0],ACT_IS_HEALER)
	||  IS_SET(victim->act[0],ACT_IS_CHANGER))
		return TRUE;

	if (victim == MOUNTED(ch))
  		return TRUE;

	if (!IS_NPC(ch))
	{
		/* no pets */
		if (IS_SET(victim->act[0],ACT_PET))
	   	return TRUE;

		/* no charmed creatures unless owner */
		if (IS_AFFECTED(victim,AFF_CHARM) && (area || ch != victim->master))
		return TRUE;

		/* legal kill? -- cannot hit mob fighting non-group member */
		if (victim->fighting != NULL && !is_same_group(ch,victim->fighting))
		return TRUE;
	}
	else
	{
		/* area effect spells do not hit other mobs */
		if (area && !is_same_group(victim,ch->fighting))
		return TRUE;
	}
	}
	/* killing players */
	else
	{
	/*if (area && IS_IMMORTAL(victim) && victim->level > LEVEL_IMMORTAL)
		return TRUE; */

	/* NPC doing the killing */
	if (IS_NPC(ch))
	{
		/* charmed mobs and pets cannot attack players while owned */
		if (IS_AFFECTED(ch,AFF_CHARM) && ch->master != NULL
		&&  ch->master->fighting != victim)
		return TRUE;

		/* legal kill? -- mobs only hit players grouped with opponent*/
		if (ch->fighting != NULL && !is_same_group(ch->fighting,victim))
		return TRUE;
	}

	/* player doing the killing */
	else
	{
		if (is_pk(victim))
		return FALSE;
	}

	}
	return FALSE;
}


// Acrobatics
bool check_acro(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim) || MOUNTED(victim))
		return FALSE;

	chance = get_skill(victim,gsn_acro) / 5;
	if (chance == 0)
		return FALSE;

	chance += get_curr_stat(victim, STAT_DEX) / 3;
	chance -= get_curr_stat(ch, STAT_DEX) / 4;

	if (!can_see(victim,ch))
		chance /= 2;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
		chance = (chance * 3)/2;

	if (IS_NPC(ch))
		chance = (chance * 5)/3;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
		chance = (chance * 4/3);

	if (IS_NPC(ch) && !IS_NPC(victim) && number_percent () >= chance)
		return FALSE;

	if (number_percent() >= chance + UMIN(UMIN(victim->tot_level, MAX_MOB_LEVEL) - UMIN(ch->tot_level, MAX_MOB_LEVEL), 15))
		return FALSE;

	act("{GYou nimbly backflip away from $n's attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{Y$N nimbly backflips away from your attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	for (gch = ch->in_room->people; gch; gch = gch->next_in_room) {
		if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM))) {
			sprintf(buf, "{Y%s nimbly backflips out of %s's attack.{x", pers(victim, gch),  pers(ch, gch));
			act(buf, gch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}
	}

	check_improve(victim,gsn_acro,TRUE,2);
	return TRUE;
}


// Monk catch
bool check_catch(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
	int result;
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim) || IS_NPC(victim))
		return FALSE;

	chance = get_skill(victim,gsn_catch) / 3;
	chance += get_curr_stat(victim, STAT_DEX) / 4;
	chance -= get_curr_stat(ch, STAT_STR) / 5;
	chance -= get_curr_stat(ch, STAT_DEX) / 6;

	if (IS_NPC(ch))
		chance = (chance * 5)/3;

	if (chance == 0)
		return FALSE;

	if (get_weapon_sn(victim) != gsn_hand_to_hand)
		return FALSE;

	if (!can_see(ch,victim))
		chance /= 2;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
		chance = (chance * 4/3);

	if (IS_NPC(ch) && !IS_NPC(victim)) {
		if ((result = number_percent()) >= chance)
			return FALSE;
	}

	if ((result = number_percent()) >= chance + UMIN(UMIN(victim->tot_level, MAX_MOB_LEVEL) - UMIN(ch->tot_level, MAX_MOB_LEVEL), 15))
		return FALSE;

	if(p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_WEAPON_CAUGHT,"pretest",0,0,0,0,0))
		return FALSE;

	if(!p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_WEAPON_CAUGHT,"message",0,0,0,0,0)) {
		act("{GYou catch $n's attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{Y$N catches your attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		for (gch = ch->in_room->people; gch; gch = gch->next_in_room) {
			if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM))) {
				sprintf(buf, "{YWith great skill, %s catches %s's attack.{x", pers(victim, gch),  pers(ch, gch));
				act(buf, gch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
		}
	}

	check_improve(victim,gsn_catch,TRUE,4);
	return TRUE;
}


// Parry
bool check_parry(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance, skill;
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;
	OBJ_DATA *weapon;
	OBJ_DATA *weapon2;
	bool s_d = FALSE;

	if (!IS_AWAKE(victim))
		return FALSE;

	chance = get_skill(victim,gsn_parry) / 3;

	if (!chance) return FALSE;

	if (!wield) return FALSE;

	weapon = get_eq_char(ch, WEAR_WIELD);
	weapon2 = get_eq_char(ch, WEAR_SECONDARY);

	if(!weapon && !weapon2)
		return FALSE;

	skill = get_skill(ch,gsn_dual);

	if(!weapon) {
		weapon = weapon2;
		weapon2 = NULL;
	} else if(weapon2 && number_percent() < (skill / 2)) {
		weapon = weapon2;
		weapon2 = NULL;
		check_improve(victim,gsn_dual,TRUE,4);
	}

	if (IS_NPC(ch))
		chance = (chance * 5)/3;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
		chance = (chance * 4/3);

	chance += get_curr_stat(victim, STAT_DEX) / 3;
	chance -= get_curr_stat(ch, STAT_DEX) / 4;

	chance += get_weapon_skill(victim, get_weapon_sn(victim)) / 4;
	chance -= get_weapon_skill(ch, get_weapon_sn(ch)) / 4;

	chance -= abs(wield->weight / 5 - weapon->weight / 5);

	if (!can_see(ch,victim))
		chance /= 2;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
		chance = (chance * 3)/2;

	// cant parry a whip
	if (weapon && ((!weapon2 && weapon->value[0] == WEAPON_WHIP) ||
		(weapon2 && weapon->value[0] == WEAPON_WHIP && weapon2->value[0] == WEAPON_WHIP))) {
		return FALSE;
	}

	// secondary wielding another weapon gives you half the chance
	if (weapon && weapon2 && weapon->value[0] == WEAPON_WHIP && weapon2->value[0] != WEAPON_WHIP) {
		chance = chance / 2;
	}

	// Ninja get better parry if wielding a sword and dagger
	if (get_profession(victim, SECOND_SUBCLASS_THIEF) == CLASS_THIEF_NINJA) {

		if (weapon && weapon2 &&
			((weapon->value[0] == WEAPON_SWORD && weapon2->value[0] == WEAPON_DAGGER) || (weapon->value[0] == WEAPON_DAGGER && weapon2->value[0] == WEAPON_SWORD))) {
			skill = get_skill(victim, gsn_sword_and_dagger_style);
			if(number_percent() < skill) {
				chance = (chance * 3)/2;
				s_d = TRUE;
			}
		}
	}

	// Gladiator->destroyer also have better parry
	if (get_profession(victim, SUBCLASS_WARRIOR) == CLASS_WARRIOR_GLADIATOR &&
		get_profession(victim, SECOND_SUBCLASS_WARRIOR) == CLASS_WARRIOR_DESTROYER)
		chance = (chance * 3)/2;

	ch->skill_chance = chance;
	if(p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, weapon, NULL, TRIG_WEAPON_PARRIED,"pretest",0,0,0,0,0))
		return FALSE;
	chance = ch->skill_chance;

	if (IS_NPC(ch) && !IS_NPC(victim) && number_percent() >= chance)
		return FALSE;

	if (number_percent() >= (chance + UMIN(UMIN(victim->tot_level,MAX_MOB_LEVEL) - UMIN(ch->tot_level, MAX_MOB_LEVEL), 15)))
		return FALSE;

	if(!p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, weapon, NULL, TRIG_WEAPON_PARRIED,"message",0,0,0,0,0)) {

		act("{GYou parry $n's attack.{x",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{Y$N parries your attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room) {
			if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM))) {
				sprintf(buf, "{Y%s parries %s's attack.{x", pers(victim, gch),  pers(ch, gch));
				act(buf, gch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
		}
	}

	if(s_d) check_improve(victim,gsn_sword_and_dagger_style,TRUE,4);
	check_improve(victim,gsn_parry,TRUE,4);
	return TRUE;
}


// Shield block for projectiles (fireballs, arrows, etc.)
bool check_shield_block_projectile(CHAR_DATA *ch, CHAR_DATA *victim, char *attack, OBJ_DATA *projectile)
{
	int chance;
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;
	OBJ_DATA *shield;

	if (ch == victim || !IS_AWAKE(victim))
		return FALSE;

	chance = get_skill(victim,gsn_shield_block) / 4;
	shield = get_eq_char(victim, WEAR_SHIELD);
	if (chance == 0 || shield == NULL)
	return FALSE;

	if (IS_OBJ_STAT(shield, ITEM_HOLY))
	chance *= 2;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
	chance = (chance * 4/3);

	if (number_percent () >= chance)
	return FALSE;

	sprintf(buf, "{GYou block $n's %s with your shield.{x", attack);
	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	sprintf(buf, "{Y$N blocks your %s with $S shield.{x", attack);
	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if ((gch != ch
		  && gch != victim)
		  && (is_same_group(gch, ch)
			  || is_same_group(gch, victim)
			  || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
		{
		act("{Y$n shield blocks $N's $t.{x", victim, ch, gch, NULL, NULL, attack, NULL, TO_THIRD);
	}
	}

	// The shield decays from this
	shield = get_eq_char(victim, WEAR_SHIELD);
	if (!IS_SET(shield->extra[0], ITEM_BLESS) || number_percent() < 66)
	switch(shield->fragility)
	{
	case OBJ_FRAGILE_SOLID:
		break;
	case OBJ_FRAGILE_STRONG:
		if (number_percent() <= 1)
		if (number_percent() <= 5)
			shield->condition--;
		break;
	case OBJ_FRAGILE_NORMAL:
		if (number_percent() <= 2)
		if (number_percent() <= 10)
			shield->condition--;
		break;
	case OBJ_FRAGILE_WEAK:
		if (number_percent() <= 2)
		shield->condition--;
		break;
	}

	check_improve(victim,gsn_shield_block,TRUE,3);
	// Make sure we initiate combat
	if (ch->in_room == victim->in_room && (victim->fighting == NULL || ch->fighting == NULL))
	set_fighting(ch, victim);

	return TRUE;
}


// Shield block
bool check_shield_block(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
	OBJ_DATA *shield;
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim))
		return FALSE;

	if (!(shield = get_eq_char(victim, WEAR_SHIELD)))
		return FALSE;

	chance = get_skill(victim,gsn_shield_block) / 4;

	chance += get_curr_stat(victim, STAT_DEX) / 4;
	chance += get_curr_stat(victim, STAT_STR) / 4;
	chance -= get_curr_stat(ch, STAT_DEX) / 4;
	chance -= get_curr_stat(ch, STAT_STR) / 4;

	chance -= shield->weight / 10;

	if (IS_NPC(ch))
		chance = (chance * 5)/3;

	if (chance == 0)
		return FALSE;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
		chance = (chance * 6)/5;

	if (IS_OBJ_STAT(shield, ITEM_HOLY))
		chance = (chance * 6)/5;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
		chance = (chance * 4)/3;

	ch->skill_chance = chance;
	if(p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, shield, NULL, TRIG_WEAPON_BLOCKED,"pretest",0,0,0,0,0))
		return FALSE;
	chance = ch->skill_chance;


	if (IS_NPC(ch) && !IS_NPC(victim) && number_percent() >= chance)
		return FALSE;

	if (number_percent() >= chance + UMIN( UMIN(victim->tot_level, MAX_MOB_LEVEL) - UMIN(ch->tot_level, MAX_MOB_LEVEL), 15))
		return FALSE;


	if(!p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, shield, NULL, TRIG_WEAPON_BLOCKED,"message",0,0,0,0,0)) {

		act("{GYou block $n's attack with your shield.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{Y$N blocks your attack with $S shield.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		for (gch = ch->in_room->people; gch; gch = gch->next_in_room) {
			if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM))) {
				act("{Y$n shield blocks $N's attack.{x", victim, ch, gch, NULL, NULL, NULL, NULL, TO_THIRD);
			}
		}
	}

	if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_COUNTER,"pretest",0,0,0,0,0) &&
		!p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_COUNTER,"pretest",0,0,0,0,0) &&
		number_percent() < chance/2) {

		if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_COUNTER,"message",0,0,0,0,0)) {
			act("{GYou take advantage and counterattack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$N counters your blocked attack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

			for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room) {
				if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM))) {
					act("{Y$n counters $N's attack!{x", victim, ch, gch, NULL, NULL, NULL, NULL, TO_THIRD);
				}
			}
		}

		one_hit(victim, ch, TYPE_UNDEFINED, FALSE);
	}

	// shield decays with use
	if (!(shield = get_eq_char(victim, WEAR_SHIELD))) {
		sprintf(buf, "check_shield_block: shield was null before doing decay, victim %s, char %s", HANDLE(victim), HANDLE(ch));
		bug(buf, 0);
		return TRUE;
	}

	if (!IS_SET(shield->extra[0], ITEM_BLESS) || number_percent() < 66)
	switch(shield->fragility) {
	case OBJ_FRAGILE_SOLID:		break;
	case OBJ_FRAGILE_STRONG:	if (number_range(0,9999) < 12) shield->condition--; break;
	case OBJ_FRAGILE_NORMAL:	if (number_range(0,9999) < 33) shield->condition--; break;
	case OBJ_FRAGILE_WEAK:		if (number_range(0,9999) < 300) shield->condition--; break;
	}

	check_improve(victim,gsn_shield_block,TRUE,3);
	return TRUE;
}


// Speed swerve
bool check_speed_swerve(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
//    char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim))
	return FALSE;

	if (MOUNTED(victim))
	return FALSE;

	chance = get_skill(victim,gsn_swerve)/6;

	if (chance == 0)
	return FALSE;

	chance += get_curr_stat(victim, STAT_DEX);
	chance -= get_curr_stat(victim, STAT_DEX) / 2;

	if (IS_NPC(ch))
	chance = (chance * 5)/3;

	if (!can_see(victim,ch))
	chance /= 2;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
		chance = (chance * 3)/2;

	if (IS_NPC(ch) && !IS_NPC(victim)
	&&   number_percent() >= chance)
	return FALSE;

	if (number_percent() >= chance + UMIN( UMIN(victim->tot_level, MAX_MOB_LEVEL) - UMIN(ch->tot_level, MAX_MOB_LEVEL), 15))
		return FALSE;

	act("{GWith amazing agility you swerve $n's attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
	act("{Y$N speed swerves your attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if ((gch != ch && gch != victim) && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
	{
		act("{Y$n speed swerves $N's attack.{x", victim, ch, gch, NULL, NULL, NULL, NULL, TO_THIRD);
	}
	}

	check_improve(victim,gsn_swerve,TRUE,5);
	return TRUE;
}


// Dodge
bool check_dodge(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
	int roll;
//    char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim))
	return FALSE;

	if (MOUNTED(victim))
	return FALSE;

	chance = get_skill(victim,gsn_dodge) / 5;
	if (chance == 0)
	return FALSE;

	if (!can_see(victim,ch))
	chance /= 2;

	chance += get_curr_stat(victim, STAT_DEX) / 3;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
	chance = (chance * 3)/2;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
	chance = (chance * 4/3);

	roll = number_percent();
	if (roll >= chance)
	return FALSE;

	act("{GYou dodge $n's attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{Y$N dodges your attack.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if ((gch != ch && gch != victim)
		  && (is_same_group(gch, ch)
		   || is_same_group(gch, victim)
		   || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
	{
		act("{Y$n dodges $N's attack.{x", victim, ch, gch, NULL, NULL, NULL, NULL, TO_THIRD);
	}
	}

	check_improve(victim,gsn_dodge,TRUE,5);
	return TRUE;
}


// Wilderness spear block for rangers
bool check_spear_block(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield)
{
	int chance;
//    char buf[MAX_STRING_LENGTH];
	OBJ_DATA *spear;
	CHAR_DATA *gch;

	if (!IS_AWAKE(victim))
	return FALSE;

	if (!(chance = get_skill(victim, gsn_wilderness_spear_style)))
	return FALSE;

	chance /= 10;

	// attacker must wield a weapon
	if (get_eq_char(ch, WEAR_WIELD) == NULL)
	return FALSE;

	// defender must spear a spear
	spear = get_eq_char(victim, WEAR_WIELD);
	if (spear == NULL || spear->value[0] != WEAPON_SPEAR)
	{
	spear = get_eq_char(victim, WEAR_SECONDARY);

	if (spear == NULL || spear->value[0] != WEAPON_SPEAR)
		return FALSE;
	}

	if (IS_NPC(ch))
	chance = (chance * 5)/3;

	chance += get_curr_stat(victim, STAT_DEX) / 5;
	chance -= get_curr_stat(ch, STAT_DEX) / 4;

	chance += get_weapon_skill(victim, get_weapon_sn(victim)) / 7;
	chance -= get_weapon_skill(ch, get_weapon_sn(ch)) / 8;

	if (!can_see(ch,victim))
	chance /= 2;

	if (IS_AFFECTED2(victim, AFF2_WARCRY))
	chance = (chance * 3)/2;

	/* shifted players have more of a chance due to their eXtreeeem fighting skills */
	if (IS_SHIFTED(victim) && !IS_SHIFTED(ch))
	chance = (chance * 4/3);

	if (number_percent() >= chance)
	return FALSE;

	act("{GYou skillfully block $n's attack with $p.{x",  ch, victim, NULL, spear, NULL, NULL, NULL, TO_VICT);
	act("{Y$N skillfully blocks your attack with $p.{x", ch, victim, NULL, spear, NULL, NULL, NULL, TO_CHAR);

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
	if ((gch != ch && gch != victim)
		  && (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
		{
		act("{Y$n skillfully blocks $N's attack with $p.{x", victim, ch, gch, spear, NULL, NULL, NULL, TO_THIRD);
	}
	}

	check_improve(victim,gsn_wilderness_spear_style,TRUE,4);
	return TRUE;
}


// Update position of a victim.
void update_pos(CHAR_DATA *victim)
{
	if (victim->on != NULL && victim->position >= POS_FIGHTING)
	victim->on = NULL;

	if (victim->hit > 0)
	{
		if (victim->position <= POS_STUNNED)
		victim->position = POS_STANDING;

		// Be sure to take sleep spell off of them too
	if (is_affected(victim, gsn_sleep))
		affect_strip(victim, gsn_sleep);
	return;
	}

	/* TODO: Revisit
	if ((IS_NPC(victim) || (!IS_NPC(victim) && IS_DEAD(victim))) && victim->hit < 1)
	{
	victim->position = POS_DEAD;
	return;
	}

	if (victim->hit <= -11)
	{
	victim->position = POS_DEAD;
	return;
	}

		 if (victim->hit <= -6) victim->position = POS_MORTAL;
	else if (victim->hit <= -3) victim->position = POS_INCAP;
	else                          victim->position = POS_STUNNED;
	*/

	if (victim->hit <= 0)
		victim->position = POS_DEAD;
}

bool can_start_combat(CHAR_DATA *ch)
{
	if( ch->in_room == NULL ) return FALSE;

	// Fix for do_opcast. Make sure the dummy mob is not attacked.
	if( IS_NPC(ch) && ch->pIndexData->vnum == MOB_VNUM_OBJCASTER) return FALSE;

	if (IS_NPC(ch) && IS_SET(ch->act[0], ACT_MOUNT) && MOUNTED(ch))
	{
		if (IS_NPC(MOUNTED(ch)) || get_profession(MOUNTED(ch), SECOND_SUBCLASS_WARRIOR) != CLASS_WARRIOR_CRUSADER)
			return FALSE;
	}

	return TRUE;
}

void enter_combat(CHAR_DATA *ch, CHAR_DATA *victim, bool silent)
{
	if (IS_AFFECTED(ch, AFF_SLEEP))
	{
		affect_strip(ch, gsn_sleep);
		// Fix their affects?
	}

	// If victim is pulling a relic make the attacker PK for a little
	// while... this way victim can fight back against kill/flee attacks
	// which make attacker impervious to damage
	if (is_pulling_relic(victim))
		set_pk_timer(ch, victim, PULSE_VIOLENCE * 4);

	ch->fighting = victim;
	ch->position = POS_FIGHTING;

	// Make sure they aren't fading
	if (ch->fade > 0)
	{
		ch->fade = 0;
		ch->fade_dir = -1;	//@@@NIB : 20071020
		ch->force_fading = 0;
		if(!silent) {
			act("$n fades back into this dimension.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("You fade back into this dimension.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		}
	}

	/*
	// have aggro NPCs hunt their attackers
	if (IS_NPC(ch)
	&&  IS_SET(ch->act[0], ACT_AGGRESSIVE)
	&&  !IS_SET(ch->act[0], ACT_SENTINEL)
	&&  ch->tot_level < 200)
	hunt_char(ch, victim);
	*/

	if(!silent)
		act("$N begins attacking $n!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
}

// Start a fight.
bool set_fighting(CHAR_DATA *ch, CHAR_DATA *victim)
{
	OBJ_DATA *obj;

	if( !can_start_combat(ch) ) return FALSE;

	// They were already fighting the victim, just resume combat without any of the precombat stuff
	if( ch->fighting == victim )
	{
		if( victim->fighting == NULL ) {
//			send_to_char("Resuming combat...\n\r", victim);
			enter_combat(victim, ch, FALSE);
		}
		return TRUE;
	}

	if( ch->fighting != NULL ) return FALSE;

	if( !can_start_combat(victim) ) return FALSE;

	enter_combat(ch, victim, FALSE);

	// Wilderness spear style kicks in here too
	// MK 100316 - Changed so that the victim must be standing to do this defense.
	if (victim->position == POS_STANDING && get_skill(victim, gsn_wilderness_spear_style) > 0)
	{
		bool found = FALSE;
		int chance;

		if ((obj = get_eq_char(victim, WEAR_WIELD)) != NULL &&
			obj->item_type == ITEM_WEAPON && obj->value[0] == WEAPON_SPEAR)
			found = TRUE;
		else if ((obj = get_eq_char(victim, WEAR_SECONDARY)) != NULL &&
			obj->item_type == ITEM_WEAPON && obj->value[0] == WEAPON_SPEAR)
			found = TRUE;

		chance = victim->tot_level - ch->tot_level + get_skill(victim, gsn_wilderness_spear_style);

		if (found && number_percent() < chance)
		{
			act("{Y$N skillfully blocks your attack with $p!{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("{Y$N skillfully blocks $n's attack with $p!{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
			act("{GYou skillfully block $n's attack with $p!{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
			DAZE_STATE(ch, 12);
		}
	}

	/* AO 092516 Make sure to initiate combat on a successful defense. Hackish but oh well. */
	//damage_new(ch,victim,NULL,0,0,0,FALSE);	// MK couldn't you just... set the victim's fighting if it's null?
	if( victim->fighting == NULL )
		enter_combat(victim, ch, FALSE);

	p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_START_COMBAT, NULL,0,0,0,0,0);
	p_percent_trigger(ch, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_START_COMBAT, NULL,0,0,0,0,0);

	return TRUE;
}


// Stop a holdup.
void stop_holdup(CHAR_DATA *ch)
{
	if (ch->heldup != NULL)
	{
	ch->heldup->position = POS_STANDING;
		ch->heldup = NULL;
	}
}


// Stop a fight.
void stop_fighting(CHAR_DATA *ch, bool fBoth)
{
	CHAR_DATA *fch;

	if( (fch = ch->fighting) == NULL ) return;

	if(fBoth) {
		if (fch->reverie == -1)
			fch->reverie = 0;

		fch->fighting = NULL;
		fch->position = IS_NPC(fch) ? fch->default_pos : POS_STANDING;
		update_pos(fch);
	}

	if (ch->reverie == -1)
		ch->reverie = 0;

	ch->fighting = NULL;
	ch->position = IS_NPC(ch) ? ch->default_pos : POS_STANDING;
	update_pos(ch);

	return;
}

void set_corpse_data(OBJ_DATA *corpse, int corpse_type)
{
	char buf[MAX_STRING_LENGTH];
	char *name;
	char *short_desc;
	int min,max;

	if(corpse->item_type == ITEM_CORPSE_NPC) {
		MOB_INDEX_DATA *mob = get_mob_index_wnum(corpse->orig_wnum);

		// Check if the corpse has owner name/short information
		if( IS_NULLSTR(corpse->owner_name) )
			name = mob ? mob->short_descr : "someone";
		else
			name = corpse->owner_name;

		if( IS_NULLSTR(corpse->owner_short) )
			short_desc = name;
		else
			short_desc = corpse->owner_short;
		min = corpse_info_table[corpse_type].decay_npctimer_min;
		max = corpse_info_table[corpse_type].decay_npctimer_max;
	} else if(corpse->item_type == ITEM_CORPSE_PC) {
		name = corpse->owner;
		short_desc = corpse->owner;
		min = corpse_info_table[corpse_type].decay_pctimer_min;
		max = corpse_info_table[corpse_type].decay_pctimer_max;
	} else return;

	sprintf(buf, corpse_info_table[corpse_type].name, name);
	free_string(corpse->name);
	corpse->name = str_dup(buf);

	if(min < 0) min *= -corpse->level;
	if(max < 0) max *= -corpse->level;
	corpse->timer = number_range(min, max);
	corpse->timer = UMAX(corpse->timer, 1);	// Must have some decay time on it.
	CORPSE_RESURRECT(corpse) = corpse_info_table[corpse_type].resurrect_chance;
	CORPSE_ANIMATE(corpse) = corpse_info_table[corpse_type].animation_chance;

	if(corpse_info_table[corpse_type].headless) {
//		SET_BIT(corpse->extra[0], ITEM_NOSKULL);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_HEAD);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_BRAINS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EAR);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EYE);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_LONG_TONGUE);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EYESTALKS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_FANGS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_HORNS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_TUSKS);
	}

	if(corpse_info_table[corpse_type].lost_bodyparts)
		REMOVE_BIT(CORPSE_PARTS(corpse),corpse_info_table[corpse_type].lost_bodyparts);

	if (IS_SET(CORPSE_PARTS(corpse),PART_HEAD)) {
		sprintf(buf, corpse_info_table[corpse_type].short_descr, short_desc);
		free_string(corpse->short_descr);
		corpse->short_descr = str_dup(buf);

		sprintf(buf, corpse_info_table[corpse_type].long_descr, short_desc);
		free_string(corpse->description);
		corpse->description = str_dup(buf);

		sprintf(buf, corpse_info_table[corpse_type].full_descr, short_desc);
		free_string(corpse->full_description);
		corpse->full_description = str_dup(buf);
	} else {
		sprintf(buf, corpse_info_table[corpse_type].short_headless, short_desc);
		free_string(corpse->short_descr);
		corpse->short_descr = str_dup(buf);

		sprintf(buf, corpse_info_table[corpse_type].long_headless, short_desc);
		free_string(corpse->description);
		corpse->description = str_dup(buf);

		sprintf(buf, corpse_info_table[corpse_type].full_headless, short_desc);
		free_string(corpse->full_description);
		corpse->full_description = str_dup(buf);
	}

	CORPSE_TYPE(corpse) = corpse_type;
}

int blend_corpsetypes (int t1, int t2)
{
	int i;

	for(i=0;corpse_blending[i].type1 != RAWKILL_ANY || corpse_blending[i].type2 != RAWKILL_ANY;i++) {
		if((corpse_blending[i].type1 == RAWKILL_ANY || corpse_blending[i].type1 == t1) &&
			(corpse_blending[i].type2 == RAWKILL_ANY || corpse_blending[i].type2 == t2)) {
			if(corpse_blending[i].result == RAWKILL_TYPE1) return t1;
			if(corpse_blending[i].result == RAWKILL_TYPE2) return t2;
			return corpse_blending[i].result;
		}

		if(corpse_blending[i].dual &&
			(corpse_blending[i].type1 == RAWKILL_ANY || corpse_blending[i].type1 == t2) &&
			(corpse_blending[i].type2 == RAWKILL_ANY || corpse_blending[i].type2 == t1)) {
			if(corpse_blending[i].result == RAWKILL_TYPE1) return t1;
			if(corpse_blending[i].result == RAWKILL_TYPE2) return t2;
			return corpse_blending[i].result;
		}
	}

	return t1;
}

/*
 * Make a corpse out of a character.
 * Has_head: Make a corpse with flag beheaded if FALSE
 */
OBJ_DATA *make_corpse(CHAR_DATA *ch, bool has_head, int corpse_type, bool messages)
{
	OBJ_DATA *corpse;
	OBJ_DATA *obj;
	OBJ_DATA *obj_next;
	OBJ_DATA *pneuma;
	OBJ_INDEX_DATA *obj_index;
	char *name;
	char *short_desc;

	corpse_type = blend_corpsetypes(ch->corpse_type,corpse_type);

	if(corpse_type < RAWKILL_NORMAL) return NULL;

	// NPCs
	if (IS_NPC(ch))
	{
		name = ch->name;
		short_desc = ch->short_descr;

		obj_index = (ch->corpse_wnum.auid > 0 && ch->corpse_wnum.vnum > 0) ? get_obj_index_auid(ch->corpse_wnum.auid, ch->corpse_wnum.vnum) : NULL;

		if(!obj_index || obj_index->item_type != ITEM_CORPSE_NPC)
			obj_index = obj_index_corpse_npc;

		corpse = create_object(obj_index, 0, TRUE);
		// [3,6]
		corpse->orig_wnum.pArea = ch->pIndexData->area;
		corpse->orig_wnum.vnum = ch->pIndexData->vnum;

		if (!IS_IMMORTAL(ch) && ch->gold > 0)
		{
			obj_to_obj(create_money(ch->gold, ch->silver), corpse);

			ch->gold = 0;
			ch->silver = 0;
		}

		corpse->cost = 0;

		if (IS_SET(ch->act[1], ACT2_NO_RESURRECT))
		{
			SET_BIT(corpse->extra[1], ITEM_NO_RESURRECT);
			SET_BIT(corpse->extra[2], ITEM_NO_ANIMATE);
		}
	} else { // PCs
		name		= ch->name;
		short_desc	= ch->name;
		corpse		= create_object(obj_index_corpse_pc, 0, TRUE);
		// [25,40]

		// If the reckoning, put some pneuma in the corpse
		if (pre_reckoning == 0 && reckoning_timer > 0)
		{
			int pneuma_num = number_range(1, UMAX(ch->tot_level/4, 5));
			int count;

			for (count = 0; count < pneuma_num; count++)
			{
			pneuma = create_object(obj_index_bottled_soul, 0, TRUE);
			obj_to_obj(pneuma, corpse);
			}
		}

		corpse->owner = str_dup(ch->name);

		corpse->cost = 0;
		if (IS_SET(ch->act[0], PLR_NO_RESURRECT))
		{
			SET_BIT(corpse->extra[1], ITEM_NO_RESURRECT);
			SET_BIT(corpse->extra[2], ITEM_NO_ANIMATE);
		}

		if (IS_IMMORTAL(ch))
			SET_BIT(CORPSE_FLAGS(corpse), CORPSE_IMMORTAL);
		else {
			if(IS_SET(ch->in_room->room_flags, ROOM_CHAOTIC))
				SET_BIT(CORPSE_FLAGS(corpse), CORPSE_CHAOTICDEATH);
			if(is_room_pk(ch->in_room, TRUE) || is_pk(ch))
				SET_BIT(CORPSE_FLAGS(corpse), CORPSE_PKDEATH);
			if (ch->gold > 1 || ch->silver > 1)
			{
				obj_to_obj(create_money(ch->gold/2, ch->silver/2), corpse);
				ch->gold -= ch->gold/2;
				ch->silver -= ch->silver/2;
			}
		}


		// This is so we only resurrect the latest PC corpse
		ch->pcdata->corpse = corpse;
	}

	// Propagate INSTANCE status from mob to corpse
	if( IS_SET(ch->act[1], ACT2_INSTANCE_MOB) )
		SET_BIT(corpse->extra[2], ITEM_INSTANCE_OBJ);

	corpse->owner_name = str_dup(name);
	corpse->owner_short = str_dup(short_desc);

	corpse->level = ch->tot_level;
	CORPSE_PARTS(corpse) = ch->parts;

	CORPSE_TYPE(corpse) = corpse_type;

	if(corpse_info_table[corpse_type].headless || !IS_SET(ch->parts,PART_HEAD)) {
//		SET_BIT(corpse->extra[0], ITEM_NOSKULL);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_HEAD);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_BRAINS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EAR);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EYE);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_LONG_TONGUE);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_EYESTALKS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_FANGS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_HORNS);
		REMOVE_BIT(CORPSE_PARTS(corpse),PART_TUSKS);
	}

	set_corpse_data(corpse, corpse_type);

	for (obj = ch->carrying; obj != NULL; obj = obj_next) {
		obj_next = obj->next_content;
		if (IS_SET(obj->extra[0],ITEM_ROT_DEATH) && !IS_NPC(ch))
			extract_obj(obj);
	}

	// 20070521 : NIB : If a PC and a chaotic Death, mark the corpse as a chaotic death
	if(!IS_NPC(ch) && !IS_DEAD(ch) && !IS_IMMORTAL(ch))
	{
 		if(IS_SET(ch->in_room->room_flags,ROOM_CHAOTIC))
			SET_BIT(CORPSE_FLAGS(corpse),CORPSE_CHAOTICDEATH);
		if(is_room_pk(ch->in_room,FALSE) || is_pk(ch))
			SET_BIT(CORPSE_FLAGS(corpse),CORPSE_PKDEATH);
		if(IS_SET(ch->in_room->room_flags, ROOM_ARENA))
			SET_BIT(CORPSE_FLAGS(corpse),CORPSE_ARENADEATH);
	}

	// NPC death and CPK death for PCs
	// Don't leave no_loot items in player corpses, just like no_uncurse -- Areo
	if (IS_NPC(ch)
	|| (!IS_NPC(ch) && !IS_DEAD(ch) && IS_SET(ch->in_room->room_flags,ROOM_CHAOTIC)))
	for (obj = ch->carrying; obj != NULL; obj = obj_next)
	{
		obj_next = obj->next_content;

		if( !IS_SET(obj->extra[2], ITEM_ALWAYS_LOOT) && !IS_SET(obj->extra[2], ITEM_FORCE_LOOT) ) {
			if ((IS_SET(obj->extra[0], ITEM_NOUNCURSE) && !IS_NPC(ch)) ||
				(IS_SET(obj->extra[1], ITEM_NO_LOOT) && !IS_NPC(ch)))
				continue;
		}

		obj_from_char(obj);

		// If dealing with an npc, treat no_loot items just like inventory items.
		if (/*IS_SET(obj->extra[0], ITEM_INVENTORY) ||*/
			(!IS_SET(obj->extra[2], ITEM_ALWAYS_LOOT) && !IS_SET(obj->extra[2], ITEM_FORCE_LOOT) && IS_SET(obj->extra[1], ITEM_NO_LOOT) && IS_NPC(ch)))
			extract_obj(obj);
		else
		{
			REMOVE_BIT(obj->extra[2], ITEM_FORCE_LOOT);
			obj_to_obj(obj, corpse);
		}
	}

	obj_to_room(corpse, ch->in_room);

	if(messages) {
	MOBtrigger = FALSE;
	if(corpse_info_table[corpse_type].victim_message)
		act(corpse_info_table[corpse_type].victim_message, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	if(corpse_info_table[corpse_type].room_message)
		act(corpse_info_table[corpse_type].room_message, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	MOBtrigger = TRUE;
	}

	return corpse;
}

// Death cry sequence
void death_cry( CHAR_DATA *ch, bool has_head, bool messages )
{
	ROOM_INDEX_DATA *was_in_room;
	char *msg;
	int door;
	OBJ_INDEX_DATA *body_part = NULL;
	OBJ_INDEX_DATA *head_type = NULL;  // Either normal head or pirate head
	long parts;
	int head_time = number_range(4, 7); // amount of time for a head to last

	msg = "{RYou hear $n's death cry.{x";
	parts = ch->parts;

	if ( IS_NPC(ch) && IS_INVASION_LEADER(ch)) {
		head_type = obj_index_invasion_leader_head;
		head_time = 0; // head should last indefinitely
	} else {
		head_type = obj_index_severed_head;
	}

	if ( !has_head && IS_SET(parts,PART_HEAD)) {
		REMOVE_BIT(parts,PART_HEAD);
		REMOVE_BIT(parts,PART_BRAINS);
		REMOVE_BIT(parts,PART_EAR);
		REMOVE_BIT(parts,PART_EYE);
		REMOVE_BIT(parts,PART_LONG_TONGUE);
		REMOVE_BIT(parts,PART_EYESTALKS);
		REMOVE_BIT(parts,PART_FANGS);
		REMOVE_BIT(parts,PART_HORNS);
		REMOVE_BIT(parts,PART_TUSKS);

		switch ( number_range(0,3)) {
		case 0: msg  = "{R$n's headless body hits the ground ... DEAD.{x";
			body_part = head_type;
			break;
		case 1:
			if (!ch->material) {
				msg  = "{RBlood spurts from $n's neck as $s head drops to the ground.{x";
				body_part = head_type;
				break;
			}
		case 2: msg = "{R$n's head drops to the ground with a loud thud.{x";
			body_part = head_type;
			break;
		case 3: msg  = "{R$n's severed head plops on the ground.{x";
			body_part = head_type;
			break;
		}
	} else
		switch ( number_bits(4)) {
		case 0: msg = "{R$n hits the ground ... DEAD.{x"; break;
		case 1: if (!ch->material) { msg  = "{R$n splatters blood on your armour.{x"; break; }
		case 2:
			if (IS_SET(ch->parts,PART_GUTS)) {
				msg = "{R$n spills $s guts all over the floor.{x";
				body_part = obj_index_guts;
				REMOVE_BIT(parts,PART_GUTS);
			}
			break;
		case 3:
			if (IS_SET(ch->parts,PART_HEAD)) {
				msg  = "{R$n's severed head plops on the ground.{x";
				body_part = head_type;
				REMOVE_BIT(parts,PART_HEAD);
				REMOVE_BIT(parts,PART_BRAINS);
				REMOVE_BIT(parts,PART_EAR);
				REMOVE_BIT(parts,PART_EYE);
				REMOVE_BIT(parts,PART_LONG_TONGUE);
				REMOVE_BIT(parts,PART_EYESTALKS);
				REMOVE_BIT(parts,PART_FANGS);
				REMOVE_BIT(parts,PART_HORNS);
				REMOVE_BIT(parts,PART_TUSKS);
			}
			break;
		case  4:
			if (IS_SET(ch->parts,PART_HEART)) {
				msg  = "{R$n's heart is torn from $s chest.{x";
				body_part = obj_index_torn_heart;
				REMOVE_BIT(parts,PART_HEART);
			}
			break;
		case  5:
			if (IS_SET(ch->parts,PART_ARMS)) {
				msg  = "{R$n's arm is sliced from $s dead body.{x";
				body_part = obj_index_sliced_arm;
			}
			break;
		case  6:
			if (IS_SET(ch->parts,PART_LEGS)) {
				msg  = "{R$n's leg is sliced from $s dead body.{x";
				body_part = obj_index_sliced_leg;
			}
			break;
		case 7:
			if (IS_SET(ch->parts,PART_BRAINS)) {
				msg = "{R$n's head is shattered, and $s brains splash all over you.{x";
				body_part = obj_index_brains;
				REMOVE_BIT(parts,PART_BRAINS);
				if(number_percent() < 50) { REMOVE_BIT(parts,PART_EAR); }
				if(number_percent() < 50) {
					REMOVE_BIT(parts,PART_EYE);
					REMOVE_BIT(parts,PART_EYESTALKS);
				}
				if(number_percent() < 50) {
					REMOVE_BIT(parts,PART_LONG_TONGUE);
					REMOVE_BIT(parts,PART_FANGS);
					REMOVE_BIT(parts,PART_TUSKS);
				}
				if(number_percent() < 50) { REMOVE_BIT(parts,PART_HORNS); }
			}
		}

	ch->parts = parts;

	if(messages) act(msg, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	// Make body parts
	if (body_part) {
		char buf[MAX_STRING_LENGTH];
		char buf2[2*MAX_STRING_LENGTH];
		OBJ_DATA *obj;
		char *name;

		name		= IS_NPC(ch) ? ch->short_descr : ch->name;
		obj		= create_object(body_part, 0, TRUE);
		obj->level = ch->tot_level;
		obj->timer	= head_time;

		if( obj->item_type == ITEM_BODY_PART )
		{
			obj->value[1] = ch->race;
			if( !IS_NPC(ch) || ch->persist )
			{
				obj->value[2] = ch->id[0];
				obj->value[3] = ch->id[1];
			}
			else
			{
				// Remove any previously stored ID.
				obj->value[2] = 0;
				obj->value[3] = 0;
			}
		}

//		obj->pirate_reputation = get_rating( ch->ships_destroyed );

		sprintf(buf, obj->short_descr, name);
		free_string(obj->short_descr);
		obj->short_descr = str_dup(buf);

		if (str_cmp(obj->full_description, "(no full description)"))
			sprintf(buf, obj->full_description, name);
		else
			sprintf(buf, obj->description, name);
		free_string(obj->full_description);
		obj->full_description = str_dup(buf);

		sprintf(buf, obj->description, name);
		sprintf(buf2, "{r%s{x", buf);
		free_string(obj->description);
		obj->description = str_dup(buf2);

		if (IS_FOOD(obj)) {
			if (IS_SET(ch->form,FORM_POISON))
			{
				FOOD(obj)->poison = URANGE(1, ch->tot_level / 3, 99);
			}
			else if (!IS_SET(ch->form, FORM_EDIBLE))
			{
				obj->item_type = ITEM_TRASH;
				free_food_data(FOOD(obj));
				FOOD(obj) = NULL;
			}
		}

		obj_to_room(obj, ch->in_room);
	}

	if(messages) {
		if (IS_NPC(ch))
			msg = "{RYou hear something's death cry.{x";
		else
			msg = "{RYou hear someone's death cry.{x";

		was_in_room = ch->in_room;

		if (!was_in_room) {
			bug("death_cry, was_in_room was NULL!!",0);
			return;
		}

		for (door = 0; door <= 9; door++) {
			EXIT_DATA *pexit;

			if ((pexit = was_in_room->exit[door]) &&
				pexit->u1.to_room && pexit->u1.to_room != was_in_room) {
				ch->in_room = pexit->u1.to_room;
				act(msg, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
		}

		ch->in_room = was_in_room;
	}
}

int damage_to_corpse(int dam_type)
{
	int w, n;

	if(!dam_to_corpse[dam_type][0]) return RAWKILL_NORMAL;

	w = number_range(1,dam_to_corpse[dam_type][0]);

	for(n = 1; n < 11; n+=2) {
		if(!dam_to_corpse[dam_type][n]) break;

		if(w <= dam_to_corpse[dam_type][n]) return dam_to_corpse[dam_type][n+1];

		w -= dam_to_corpse[dam_type][n];
	}

	return RAWKILL_NORMAL;

}

/*
// 0 = SAFE
// 1 = NORMAL
// 2 = PK/ARENA
// 3 = CPK
// 4 = CPK + PK/ARENA
int room_pkness(ROOM_INDEX_DATA)
{
	int pk = 1;

	if (IS_SET(victim->in_room->room_flags, ROOM_PK) || IS_SET(victim->in_room->room_flags, ROOM_ARENA)) pk += 1;
	if (IS_SET(victim->in_room->room_flags, ROOM_CHAOTIC)) pk += 2;
	if (IS_SET(victim->in_room->room_flags, ROOM_SAFE)) pk = 0;

	return pk;
}


bool visit_func_deathsight (ROOM_INDEX_DATA *room, void *argv[], int argc, int depth, int door)
{
	int pk;

	pk =


	CHAR_DATA *vch, *vch_next, *ch;
	int level, max_depth;
	AFFECT_DATA af;

	memset(&af,0,sizeof(af));

	ch = (CHAR_DATA *)argv[0];
	max_depth = (int)argv[2];
	level = (int)argv[1] * (depth + 1) / (max_depth + 1);

	if(door < MAX_DIR) {
		act("{WA blinding light blasts in from nearby.{x", room->people, NULL, NULL, TO_ALL);
	} else {
		act("{W$n forces the light to explode outward in a blinding flash.{x", ch, NULL, NULL, TO_ROOM);
		send_to_char("{WYou force the light to explode outward in a blinding flash.{x\n\r", ch);
	}

	af.where     = TO_AFFECTS;
	af.group    = AFFGROUP_MAGICAL;
	af.type      = gsn_blindness;
	af.level     = level;
	af.location  = APPLY_HITROLL;
	af.modifier  = -4;
	af.duration  = 1 + level/6; //1+level > 3 ? 3 : 1+level;
	af.bitvector = AFF_BLIND;
	af.bitvector2 = 0;

	for (vch = room->people; vch != NULL; vch = vch_next) {
		vch_next = vch->next_in_room;
		if (!is_safe(ch, vch, FALSE) && vch != ch) {
			if(!IS_AFFECTED(vch, AFF_BLIND) && number_range(0,(int)argv[1]-1) < level && !saves_spell(level, vch, DAM_LIGHT)) {
				affect_to_char(vch, &af);
				send_to_char("You are blinded!\n\r", vch);
				act("$n appears to be blinded.",vch,NULL,NULL,TO_ROOM);
			}
			if(IS_VAMPIRE(vch) && !IS_IMMORTAL(vch))
				damage_vampires(vch,dice(level,5));
		}
	}

	return TRUE;
}


void death_sight_check(CHAR_DATA *ch, CHAR_DATA *victim)
{
	void *argv[8];
	int pk,skill,depth;

	if (IS_SET(victim->in_room->room_flags, ROOM_SAFE)) return;

	pk = room_pkness(victim->in_room);
	skill = get_skill(ch,gsn_deathsight);
	skill = URANGE(0,skill,100);
	depth = ((skill + 9) / 10);

	argv[0] = (void *)pk;				// PKness of the target room, paths to the room must be at least this and non-safe
	argv[1] = (void *)depth;			// Maximum number of rooms, round UP to the nearest 10% multiple
	argv[2] = (void *)(((skill + 9) % 10) + 1);	// the chance that it will DO the Nth room X/10
	argv[3] = (void *)-1;				// Used to set the direction on the
	argv[4] = (void *)(victim->in_room)		// Room where the victim is located
	argv[5] = (void *)FALSE;			// Whether the target was within range
	argv[6] = (void *)FALSE;			// Whether a lock was made on the exact room
}

*/

void death_sight_echo(CHAR_DATA *victim)
{
	DESCRIPTOR_DATA *d;
	CHAR_DATA *ch;

	for (d = descriptor_list; d != NULL; d = d->next) {
		ch = d->original ? d->original : d->character;

		if (d->connected == CON_PLAYING && ch != victim && IS_AFFECTED2(ch,AFF2_DEATHSIGHT)) {
//			if(ch->in_room && victim->in_room && ch->in_room->area == victim->in_room->area)
//				death_sight_check(ch,victim);
//			else
				act_new("{D$N lost $S life.{x", ch, victim,NULL,NULL,NULL,NULL,NULL, TO_CHAR,POS_DEAD,NULL);
		}
	}
}

OBJ_DATA *raw_kill(CHAR_DATA *victim, bool has_head, bool messages, int corpse_type)
{
	CHAR_DATA *temp;
	char buf[MAX_STRING_LENGTH];
	bool arena = FALSE;
	OBJ_DATA *obj, *corpse = NULL;
	LOCATION recall;
	ROOM_INDEX_DATA *recall_room;
	TOKEN_DATA *token, *token_next;
//    long repop_room = 0;

	sprintf(buf,"raw_kill(%s:%lu:%lu,%s,%s,%d)",
		(char*)((IS_NPC(victim) || victim->morphed) ? victim->short_descr : capitalize(victim->name)),
		victim->id[0],victim->id[1],
		(has_head?"HEAD":"HEADLESS"),
		(messages?"MESSAGES":"SILENT"),
		corpse_type);
	wiznet(buf,NULL,NULL,WIZ_DEATHS,0,MAX_LEVEL);

	/* If someone has died then unbanish them */
	victim->maze_time_left = 0;

	/* Save what the victim was wearing */
	save_last_wear(victim);

	/* If someone was in an auto-war, remove them */
	char_from_team(victim);

	/* Make sure their mount doesn't go with them */
	if (MOUNTED(victim)) {
		CHAR_DATA *mount = MOUNTED(victim);

		p_percent_trigger(mount, NULL, NULL, NULL, victim, NULL, NULL, NULL, NULL, TRIG_FORCEDISMOUNT, NULL,0,0,0,0,0);

		if(messages) {
			act("Your lifeless corpse falls off $N.", victim, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n's lifeless corpse falls off $N.", victim, mount, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("$n's lifeless corpse falls off of you.", victim, mount, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		}

		victim->riding = FALSE;
		mount->riding = FALSE;
		mount->rider = NULL;
		victim->mount = NULL;

		stop_grouped(mount);
	}

	/* remove any PURGE_DEATH tokens on the character */
	for (token = victim->tokens; token != NULL; token = token_next) {
		token_next = token->next;

		if (IS_SET(token->flags, TOKEN_PURGE_DEATH)) {
			p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL,0,0,0,0,0);

			sprintf(buf, "char update: token %s(%ld) char %s(%ld) was purged on death",
				token->name, token->pIndexData->vnum, HANDLE(victim), IS_NPC(victim) ? victim->pIndexData->vnum :
				0);
			log_string(buf);
			token_from_char(token);
			free_token(token);
		}
	}

	if (!IS_NPC(victim) && (IS_SET(victim->in_room->room_flags, ROOM_ARENA) || (pre_reckoning == 0 && reckoning_timer > 0)))
		arena = TRUE;

	/* if something catastrophic has happened bail out */
	if (victim->in_room == NULL)
	{
		sprintf(buf, "raw_kill: NO IN_ROOM ON CHAR %s(%ld)",
			victim->name, IS_NPC(victim) ? victim->pIndexData->vnum : 0);
		bug(buf, 0);
		extract_char(victim, FALSE);
		return NULL;
	}
	stop_fighting(victim, TRUE);
	stop_casting(victim, FALSE);
	stop_music(victim, FALSE);
	script_end_failure(victim, FALSE);
	interrupt_script(victim, TRUE);

	if (victim->master != NULL)
		stop_follower(victim,TRUE);

	die_follower(victim);
	if (victim->pulled_cart != NULL) {
		if(messages) {
			act("You stop pulling $p.", victim, NULL, NULL, victim->pulled_cart, NULL, NULL, NULL, TO_CHAR);
			act("$n stops pulling $p.", victim, NULL, NULL, victim->pulled_cart, NULL, NULL, NULL, TO_ROOM);
		}
		victim->pulled_cart = NULL;
	}

	/* take their stuff off them while dead */
	for (obj = victim->carrying; obj != NULL; obj = obj->next_content) {
		if (obj->wear_loc != WEAR_NONE &&
			WEAR_UNEQUIP_DEATH(obj->wear_loc) &&
			!IS_SET(obj->extra[2], ITEM_KEEP_EQUIPPED))
			unequip_char(victim, obj, TRUE);
	}

	// If switched, switch them back then kill them
	if (IS_SWITCHED(victim))
	{
		temp = victim->desc->character;
		char_from_room(victim->desc->original);
		char_to_room(victim->desc->original, temp->in_room);
		temp = victim->desc->original;
		extract_char(victim, TRUE);
		victim = temp;
	}

	/* morph them back before dying */
	if (IS_MORPHED(victim))
		do_function(victim, &do_shape, "");

	if (IS_SHIFTED(victim))
		do_function(victim, &do_shift, "");


	// Check to see if they are in a dungeon
	DUNGEON *dungeon = get_room_dungeon(victim->in_room);
	if (!IS_NPC(victim) && IS_VALID(dungeon) && dungeon->index->death_release != DEATH_RELEASE_NORMAL)
	{
		// Dungeons treat death differently for players
		victim->can_release = TRUE;
		arena = FALSE;		// Cancel out the arena
		recall_room = NULL;
	}
	else
	{
		// Standard death
		get_room_recall(victim->in_room, &recall);
		//recall = victim->in_room->area->recall;

		if (!IS_NPC(victim) && location_isset(&victim->recall))
		{
			recall = victim->recall;
			location_clear(&victim->recall);
		}

		// Just in case...
		if (!(recall_room = location_to_room(&recall)))
		{
			sprintf(buf, "raw_kill: recall room for %s(%ld) in_room %s(%ld) was NULL.",
				HANDLE(victim), IS_NPC(victim) ? victim->pIndexData->vnum : 0,
				victim->in_room->name, victim->in_room->vnum);
			bug(buf, 0);

			recall_room = room_index_temple;
		}
		location_from_room(&victim->recall,recall_room);

		/* quick repop for n00bs. */
		// TODO: Change this to an area flag?
		if ((victim->tot_level < 10) && !IS_REMORT(victim) && (!IS_NPC(victim)))
		{
			send_to_char("\n\r{yYou wake up in a dazed state... maybe you weren't dead after all.\n\r{x", victim);
			send_to_char("You notice that you are safe and healthy once again.\n\r", victim);

			char_from_room(victim);
			char_to_room(victim,room_index_newbie_death);

			victim->position = POS_RESTING;
			victim->dead = FALSE;

			victim->hit  = victim->max_hit;
			victim->mana = victim->max_mana;
			victim->move = victim->max_move;
			return NULL;
		}

	}

	if (!victim->has_head)
	 	victim->has_head = TRUE;

	death_cry(victim, has_head, messages);

	if ((!IS_NPC(victim) && IS_DEAD(victim)) || (IS_NPC(victim) && IS_SET(victim->act[1], ACT2_DROP_EQ)))
		corpse_type = RAWKILL_NOCORPSE;

	if (corpse_type > RAWKILL_NOCORPSE)
		corpse = make_corpse(victim, has_head, corpse_type,messages);

	if (IS_NPC(victim) && (IS_SET(victim->act[1], ACT2_DROP_EQ) || (corpse_type == RAWKILL_NOCORPSE))) {
		OBJ_DATA *obj_next;

		for (obj = victim->carrying; obj != NULL; obj = obj_next)
		{
			obj_next = obj->next_content;

			if(messages) act("$n drops $p.", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			obj_from_char(obj);
			obj_to_room(obj, victim->in_room);
		}
	}

	while (victim->affected)
		affect_remove(victim, victim->affected);

	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_STRIPAFFECT, NULL,0,0,0,0,0);

	victim->affected_by[0]	= race_table[victim->race].aff;
	victim->affected_by[1] = 0;

	victim->paroxysm = 0;

	victim->bitten_level = 0;
	victim->bitten_type = 0;
	victim->bitten = 0;

	// reset challenged to avoid exploits
	victim->challenged = NULL;

	if (IS_NPC(victim))
	{
		victim->pIndexData->killed++;

		// Has a different use for npcs than for players
		p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, corpse, NULL, TRIG_AFTERDEATH, NULL,0,0,0,0,0);

		extract_char(victim, TRUE);
		return corpse;
	}

	if (!IS_REMORT(victim))
		send_to_char("{WYour disembodied soul rises from your mutilated corpse.{x\n\r", victim);

	/* Inform church members */
	if (victim->church != NULL)
	{
		sprintf(buf, "{Y[%s has been KILLED at %s!!!]{X\n\r", victim->name, victim->in_room->name);
		msg_church_members(victim->church, buf);
	}

	if (!IS_NPC(victim))
		death_sight_echo(victim);

	if (!IS_REMORT(victim) && !arena)
		death_mob_echo(victim);

	// Instant repop for arena and reckoning deaths.
	if (arena)
	{
		if (!(pre_reckoning == 0 && reckoning_timer > 0))
		{
			if (!str_cmp(victim->in_room->area->name, "Plith"))
			{
			send_to_char("{YYou have been defeated in the arena!{x\n\r\n\r"
				"{GThe guards drag your body back to a safe place "
				"where you awake from your slumber.\n\r{x", victim);
			}
		}
		else
			send_to_char("\n\r{YThe daemonic forces of the Reckoning breathe new life into your shattered body.\n\r", victim);

		victim->hit = victim->max_hit/2;
		victim->mana = victim->max_mana/2;
		victim->move = victim->max_move/2;

		char_from_room(victim);
		char_to_room(victim, recall_room);

		victim->position = POS_RESTING;

		test_for_end_of_war();
		return corpse;
	}

	if(victim->manastore > 0)
		victim->manastore = victim->manastore / 2;	// They automatically lose half of it just from dying
	else
		victim->manastore = 0;

	// When a player is flagged where they can release, they are dead but will not go to the death plane.
	// As they can only release if they are
	if (!victim->can_release)
	{
		if (IS_DEAD(victim))
			victim->time_left_death += MINS_PER_DEATH + 1;
		else
			victim->time_left_death = MINS_PER_DEATH + 1;

		victim->hit = victim->max_hit;
		victim->mana = victim->max_mana;
		victim->move = victim->max_move;
		char_from_room(victim);
		char_to_room(victim, room_index_death);
	}

	victim->dead = TRUE;
	victim->deaths++;



	for (obj = victim->carrying; obj != NULL; obj = obj->next_content)
	SET_BIT(obj->extra[1], ITEM_UNSEEN);
	/*
	If you want people to carry eq when dead, put it here
	and DON'T FORGET TO TAKE IT OFF THEM WHEN THEY ARE BROUGHT
	BACK TO LIFE
	*/

	victim->position = POS_STANDING;

	spell_fly(gsn_fly, victim->tot_level, victim, victim, TARGET_CHAR, WEAR_NONE);
	spell_detect_invis(gsn_detect_invis, victim->tot_level, victim, victim, TARGET_CHAR, WEAR_NONE);
	spell_detect_hidden(gsn_detect_hidden, victim->tot_level, victim, victim, TARGET_CHAR, WEAR_NONE);
	spell_infravision(gsn_infravision, victim->tot_level, victim, victim, TARGET_CHAR, WEAR_NONE);

	// Do anything that might be required AFTER you truly die, such as expire any affects
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_AFTERDEATH, NULL,0,0,0,0,0);

	return corpse;
}


// Death mob echo when someone dies (for show)
void death_mob_echo(CHAR_DATA *victim)
{
	CHAR_DATA *death_mob;
	int rand = number_percent();

	if (rand < 20)
	{
	act("{DA skeletal hand punches its way up from the ground beneath.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DA skeletal hand punches its way up from the ground beneath.{x\n\r", victim);
	act("Death stumbles out of a shallow grave and shakes off the dirt.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Death stumbles out of a shallow grave and shakes off the dirt.\n\r", victim);
	act("{C'Well that entrance was more graceful than some of my others i guess...' says Death.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{C'Well that entrance was more graceful than some of my others i guess...' says Death.{x\n\r", victim);
	}
	else if (rand < 40)
	{
	act("{DDeath plummets from the sky above, landing with a loud thud.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DDeath plummets from the sky above, landing with a loud thud.{x\n\r", victim);
	act("Death peels himself off the ground and snaps his arm back into place.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Death peels himself off the ground and snaps his arm back into place.\n\r", victim);
	act("{C'Mental Note:' says Death. 'Lets not try that entrance again... EVER.'{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{C'Mental Note:' says Death. 'Lets not try that entrance again... EVER.'{x\n\r", victim);
	}
	else if (rand < 60)
	{
	act("{DDarkness collects around the corpse, revealing Death.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DDarkness collects around the corpse, revealing Death.{x\n\r", victim);
	act("{C'Now you've gotta admit. That entrance has an element of style about it.' says Death.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{C'Now you've gotta admit. That entrance has an element of style about it.' says Death.{x\n\r", victim);
	act("Death wipes his hands on his cloak.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Death wipes his hands on his cloak.\n\r", victim);
	}
	else
	{
	act("{DDeath arrives out of nowhere.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{DDeath arrives out of nowhere.{x\n\r", victim);
	act("{C'This really is pointless, how many times more am I going to have to take you back?' says Death.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{C'This really is pointless, how many times more am I going to have to take you back?' says Death.{x\n\r", victim);
	}

	death_mob = create_mobile(mob_index_death, FALSE);
	char_to_room(death_mob, victim->in_room);

	act("Death taps $n's corpse three times with his scythe.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("Death taps your corpse three times with his scythe.\n\r", victim);

	death_mob->tempstore[0] = 1;
	p_percent_trigger(death_mob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);
}


// Gain exp from a victim, splits amongst group
void group_gain(CHAR_DATA *ch, CHAR_DATA *victim, int percent)
{
	char buf[MAX_STRING_LENGTH];
	CHAR_DATA *gch;
	int xp;
	int members;
	int group_levels;

	// If is an NPC that can't level, verify this mob is grouped with a player in the room.
	if (IS_NPC(ch) && !IS_SET(ch->act[1], ACT2_CANLEVEL))
	{
		CHAR_DATA *pch;
		for(pch = ch->in_room->people; pch; pch = pch->next_in_room)
		{
			if (!IS_NPC(pch) && is_same_group(ch, pch))
				break;
		}

		if (!pch) return;
	}

	// No experience on npc victims if disabled.
	if( IS_NPC(victim) && IS_SET(victim->act[1], ACT2_NO_XP))
		return;

	// Check here just to make sure
	if (ch->in_room == NULL) {
		bug("group_gain: ch with null in_room", 0);
		return;
	}

	members = 0;
	group_levels = 0;
	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
		/* If this char is in the same form */
		if (is_same_group(gch, ch))
		{
			members++;
			if (IS_NPC(gch) && IS_SET(gch->act[0], ACT_MOUNT))
				continue;
			else
				group_levels += gch->tot_level;
		}
	}

	if (members == 0)
	{
		bug("Group_gain: 0 members.", members);
		members = 1;
		group_levels = ch->tot_level ;
	}

	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
		OBJ_DATA *obj;
		OBJ_DATA *obj_next;

		if (!is_same_group(gch, ch) || IS_NPC(gch))
			continue;

		xp = xp_compute(gch, victim, group_levels);	// This is computed so that objects can get experience

		xp = percent * xp / 100;

		if (!(IS_IMMORTAL(gch) || gch->tot_level == 120))
		{
			int pc_xp = xp;
			/* Check for reckoning boost in addition to experience boost. If reckoning is active, do a flat 2x experience -- Areo */
			// Reckoning now has an intensity which affects the experience boost, from +10% to +200%
			//  Players with NORECKONING turned on will not get reckoning boosts
			if (boost_table[BOOST_EXPERIENCE].boost != 100 || boost_table[BOOST_RECKONING].boost != 100)
			{
				if (boost_table[BOOST_RECKONING].boost != 100 && !IS_SET(ch->act[1], PLR_NORECKONING))
				{
					sprintf(buf, "{W%d%% experience!{x\n\r", boost_table[BOOST_RECKONING].boost);
					send_to_char(buf,gch);
					pc_xp = (xp * boost_table[BOOST_RECKONING].boost)/100;
				}
				else if (boost_table[BOOST_EXPERIENCE].boost != 100)
				{
					sprintf(buf, "{W%d%% experience!{x\n\r", boost_table[BOOST_EXPERIENCE].boost);
					send_to_char(buf, gch);
					pc_xp = (xp * boost_table[BOOST_EXPERIENCE].boost)/100;
				}
			}

			if (ch->leader != NULL && get_skill(ch->leader, gsn_leadership) < number_percent()) {
				pc_xp = 105 * pc_xp / 100;
			}

			if (ch->xpboost > 0)
			{
				pc_xp = (100 + ch->xpboost) * pc_xp / 100;
			}
			sprintf(buf, "{BYou receive {C%d {Bexperience points.\n\r{x", pc_xp);
			send_to_char(buf, gch);
			gain_exp(gch, pc_xp);
		}

		for (obj = gch->carrying; obj != NULL; obj = obj_next)
		{
			obj_next = obj->next_content;
			if (obj->wear_loc == WEAR_NONE)
				continue;

			obj->tempstore[0] = xp;
			obj->tempstore[1] = OBJ_XPGAIN_GROUP;	// 1
			p_percent_trigger(NULL, obj, NULL, NULL, gch, NULL, NULL, NULL, NULL, TRIG_XPGAIN, NULL,0,0,0,0,0);


			if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL)    && IS_EVIL(gch)   )
				||   (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD)    && IS_GOOD(gch)   )
				||   (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(gch)))
			{
				act("You are zapped by $p.", gch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("$n is zapped by $p.",   gch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				obj_from_char(obj);
				obj_to_room(obj, gch->in_room);
			}

		}
	}
}


// Compute exp for a kill.
int xp_compute(CHAR_DATA *gch, CHAR_DATA *victim, int total_levels)
{
	int xp;
	int base_exp;
	int multiplier;
	char buf[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	int bonus_xp = 0;
	int gch_tot_level;
	int diff_level;

	multiplier = victim->tot_level;
	if (victim->tot_level < 30)			base_exp = 100;
	else if (victim->tot_level < 60)	base_exp = 120;
	else if (victim->tot_level < 90)	base_exp = 140;
	else								base_exp = 150;

	gch_tot_level = UMIN(120, gch->tot_level);	// Clamp the level
	diff_level = gch_tot_level - victim->tot_level;

	// adjust exp based on level difference
	base_exp -= ((diff_level));
	if (victim->tot_level > gch_tot_level)
		base_exp += ((-diff_level)) / 2;

	xp = base_exp * multiplier;

	xp = (int) ((float) xp * (float) ((float) gch_tot_level / (float) total_levels));

	// HACK! stop people from leveling in plith.
	// TODO: Add area flag to do this
	if (diff_level > 25 && !str_cmp(gch->in_room->area->name, "Plith"))
		xp = (int) xp / diff_level;

	// Nothing for killing pets
	if (IS_SET(victim->act[0],ACT_PET))
	{
		sprintf(buf, "xp_compute: ch %s, victim %s with ACT_PET", gch->name,
			IS_NPC(victim) ? victim->short_descr : victim->name);
		log_string(buf);
		xp = 0;
	}

	if (IS_SET(victim->act[0], ACT_ANIMATED))
		xp = 0;


	bonus_xp = 0;
	// Extra xp relic
	if (gch->church != NULL && objindex_in_treasure_room(gch->church, obj_index_relic_extra_xp))
		bonus_xp += 10;

	// Leveling rewards for churches
	if ((is_good_church(gch) && victim->alignment < -300) ||
		(is_evil_church(gch) && victim->alignment > 300)) {
		// good exp rewards for PKills combat
		if (!IS_NPC(gch) && !IS_NPC(victim))
			xp *= 5;
		else {
			if (gch->church->size == CHURCH_SIZE_BAND)
				bonus_xp += 5;
			else if (gch->church->size == CHURCH_SIZE_CULT)
				bonus_xp += 10;
			else if (gch->church->size == CHURCH_SIZE_ORDER)
				bonus_xp += 20;
			else if (gch->church->size == CHURCH_SIZE_CHURCH)
				bonus_xp += 25;
		}
	}

	gch->tempstore[0] = 0;
	p_percent_trigger(gch,NULL, NULL, NULL, gch, NULL, NULL, NULL, NULL, TRIG_XPCOMPUTE,NULL,0,0,0,0,0);
	bonus_xp += gch->tempstore[0];
	for (obj = gch->carrying; obj != NULL; obj = obj->next_content) {
		if( obj->wear_loc != WEAR_NONE ) {
			obj->tempstore[0] = 0;
			p_percent_trigger(NULL, obj, NULL, NULL, gch, NULL, NULL, NULL, NULL, TRIG_XPCOMPUTE, NULL,0,0,0,0,0);
			bonus_xp += obj->tempstore[0];
		}
	}
	gch->in_room->tempstore[0] = 0;
	p_percent_trigger(NULL, NULL, gch->in_room, NULL, gch, NULL, NULL, NULL, NULL, TRIG_XPCOMPUTE, NULL,0,0,0,0,0);
	bonus_xp += gch->in_room->tempstore[0];

	if( xp > 0 && bonus_xp > 0 ) {
		if(!(IS_IMMORTAL(gch) || gch->tot_level == 120))
			printf_to_char(gch, "{W%d%% more experience!{x", bonus_xp);
		xp = (100 + bonus_xp) * xp / 100;
	}
	// kind of a hack but oh well
	xp = UMAX(xp, 0);

	return xp;
}


void dam_message(CHAR_DATA *ch, CHAR_DATA *victim, int dam,int dt,bool immune)
{
	char buf1[256], buf2[256], buf3[256];// tmp[25];
	const char *vs;
	const char *vp;
	const char *attack;
	CHAR_DATA *gch;
	char punct;
	float percent;
//	char msg[MSL];

//	sprintf(msg, "dam_message: '%s' vs '%s', dam = %d, dt = %d, immune = %s",
//		(ch ? ch->name : "(null)"),
//		(victim ? victim->name: "(null"),
//		dam, dt, (immune ? "TRUE" : "FALSE"));
//	wiznet(msg,NULL,NULL,WIZ_TESTING,0,0);

	if (ch == NULL || victim == NULL)
		return;

	percent = (float) dam / victim->hit;
	percent *= 100;


	if (dam <   0) { vs = "miss";	vp = "misses";		}
	else if (dam == 0) { vs = "do nothing to";	vp = "does nothing to";		}
	else if (percent <=  .5) { vs = "scratch";	vp = "scratches";	}
	else if (percent <=  .8) { vs = "graze";	vp = "grazes";		}
	else if (percent <=   1) { vs = "hit";	vp = "hits";		}
	else if (percent <=   2) { vs = "injure";	vp = "injures";		}
	else if (percent <=   3) { vs = "wound";	vp = "wounds";		}
	else if (percent <=   5) { vs = "maul";       vp = "mauls";		}
	else if (percent <=   6) { vs = "{cdecimate{x";	vp = "decimates";	}
	else if (percent <=   8) { vs = "{gdevastate{x";	vp = "devastates";	}
	else if (percent <=  10) { vs = "{gmaim{x";	vp = "maims";		}
	else if (percent <=  15) { vs = "{BM{bU{BT{bI{BL{bA{BT{bE{x";	vp = "{BM{bU{BT{bI{BL{bA{BT{bE{BS{x";	}
	else if (percent <=  18) { vs = "{BD{CI{BS{CE{BM{CB{BO{CW{BE{CL{x";	vp = "{BD{CI{BS{CE{BM{CB{BO{CW{BE{CL{BS{x";	}
	else if (percent <=  20) { vs = "{BD{CI{WSMEMB{CE{BR{x";	vp = "{BD{CI{WSMEMBE{CR{BS{x";	}
	else if (percent <=  22) { vs = "{RMASSACRE{x";	vp = "{RMASSACRES{x";	}
	else if (percent <=  25) { vs = "{RM{rA{RN{rG{RL{rE{x";	vp = "{RM{rA{RN{rG{RL{rE{RS{x";		}
	else if (percent <=  28) { vs = "{r**{R* {BDEMOLISH {R*{r**{x";
		vp = "{r**{R* {BDEMOLISHES {R*{r**{x";			}
	else if (percent <=  30) { vs = "{r**{R* {CDESTROY {R*{r**{x";
		vp = "{r**{R* {CDESTROYS {R*{r**{x";			}
	else if (percent <=  32)  { vs = "{r=={R= {WOBLITERATE {R={r=={x";
		vp = "{r=={R= {WOBLITERATES {R={r=={x";		}
	else if (percent <=  35)  { vs = "{c>>{C> {WANNIHILATE {C<{c<<{x";
		vp = "{c>>{C> {WANNIHILATES {C<{c<<{x";		}
	else if (percent <=  40)  { vs = "{D<{w<{W< {RERADICATE {W>{w>{D>{x";
		vp = "{D<{w<{W< {RERADICATES {W>{w>{D>{x";			}
	else                   { vs = "do {YU{GN{CS{WP{BE{MA{GK{CA{RB{YL{WE {Gt{Ch{Mi{Bn{Wg{Ys{G to{x";
		vp = "does {YU{GN{CS{WP{BE{MA{GK{CA{RB{YL{WE {Gt{Ch{Mi{Bn{Wg{Ys{G to{x";		}

	punct   = (percent <= 8) ? '.' : '!';

	if (dt == TYPE_HIT)
	{
	if (ch  == victim)
	{
		sprintf(buf1, "{Y{+$n %s {Y$melf%c{x",vp,punct);
		sprintf(buf2, "{RYou %s {Ryourself%c{x",vs,punct);
	}
	else
	{
		sprintf(buf1, "{Y{+$n %s {Y$N%c{x",  vp, punct);
		if (!str_cmp(vs, "miss"))
		sprintf(buf2, "{YYou %s {Y$N%c{x", vs, punct);
			else {
		sprintf(buf2, "%s{GYou %s {G$N%c{x",
				boost_table[BOOST_DAMAGE].boost != 100 ? "{R(+) {G" : "",
				vs,
				punct);
		}
		sprintf(buf3, "{Y{+$n %s {Yyou%c{x", vp, punct);
	}
	}
	else
	{
	if (dt >= 0 && dt < MAX_SKILL)
		attack	= skill_table[dt].noun_damage;
	else if (dt >= TYPE_HIT
	&& dt < TYPE_HIT + MAX_DAMAGE_MESSAGE)
		attack	= attack_table[dt - TYPE_HIT].noun;
	else
	{
		bug("Dam_message: bad dt %d.", dt);
		dt  = TYPE_HIT;
		attack  = attack_table[0].name;
	}

	// Immune to the attack
	if (immune)
	{
		if (ch == victim)
		{
		sprintf(buf1,"{C$n is unaffected by $s own %s.{c",attack);
		sprintf(buf2,"{WLuckily, you are immune to that.{c");
		}
		else
		{
			sprintf(buf1,"{C$N is unaffected by $n's %s{C!{x",attack);
			sprintf(buf2,"{C$N is unaffected by your %s{C!{x",attack);
			sprintf(buf3,"{C$n's %s is powerless against you.{x",attack);
		}
	}
	else // not immune
	{
		if (ch == victim)
		{
		sprintf(buf1, "{Y$n's %s {Y%s {Y$m%c{x",attack,vp,punct);
		sprintf(buf2, "{YYour %s {Y%s {Yyou%c{x",attack,vp,punct);
		}
		else
		{
			sprintf(buf1, "{Y$n's %s {Y%s {Y$N%c{x",  attack, vp, punct);
			sprintf(buf2, "{G%sYour %s {G%s {G$N%c{x",
				boost_table[BOOST_DAMAGE].boost != 100 ? "{R(+){G " : "",
				attack,
				vp,
				punct);
			sprintf(buf3, "{Y$n's %s {Y%s {Yyou%c{x", attack, vp, punct);
		}
	}
	}

	if( dam > 0 ) {
#ifdef DEBUG_ALLOW_SHOW_DAMAGE
		if (IS_SET(ch->act[0], PLR_SHOWDAMAGE)) {
			char dambuf[MSL];

			sprintf(dambuf, " {r({R%d{r){x", dam);
			strcat(buf2, dambuf);
		}

		if (IS_SET(victim->act[0], PLR_SHOWDAMAGE)) {
			char dambuf[MSL];

			sprintf(dambuf, " {r({R%d{r){x", dam);
			strcat(buf3, dambuf);
		}
#else
		if (IS_SET(ch->act[0], PLR_SHOWDAMAGE) && (IS_IMMORTAL(ch) || is_test_port)) {
			char dambuf[MSL];

			sprintf(dambuf, " {r({R%d{r){x", dam);
			strcat(buf2, dambuf);
		}

		if (IS_SET(victim->act[0], PLR_SHOWDAMAGE) && (IS_IMMORTAL(ch) || is_test_port)) {
			char dambuf[MSL];

			sprintf(dambuf, " {r({R%d{r){x", dam);
			strcat(buf3, dambuf);
		}
#endif
	}

	// Show damage to group members, char and others.
	if (ch == victim)
	{
	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
		if ((gch != ch && gch != victim)
		&& (is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
		act(buf1, ch, victim, gch, NULL, NULL, NULL, NULL, TO_THIRD);
	}

	act(buf2,ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
	}
	else
	{
	if (ch->in_room != NULL)
	for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
	{
		if ((gch != ch && gch != victim) &&
		(is_same_group(gch, ch) || is_same_group(gch, victim) || !IS_SET(gch->comm, COMM_NOBATTLESPAM)))
		act(buf1, ch, victim, gch, NULL, NULL, NULL, NULL, TO_THIRD);
	}

	act(buf2, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act(buf3, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	}
}


OBJ_DATA *disarm(CHAR_DATA *ch, CHAR_DATA *victim)
{
	OBJ_DATA *obj;

	if (!(obj = get_eq_char(victim, WEAR_WIELD)))
		return NULL;

	if (IS_OBJ_STAT(obj,ITEM_NOREMOVE) || IS_AFFECTED(victim, AFF_DEATH_GRIP)) {
		act("{R$S weapon won't budge!{x",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		act("{R$n tries to disarm you, but your weapon won't budge!{x", ch,victim, NULL, NULL, NULL, NULL, NULL,TO_VICT);
		act("{R$n tries to disarm $N, but fails.{x",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT);
		return NULL;
	}

	act("{R$n DISARMS you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{RYou disarm $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$n disarms $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

	unequip_char(victim, obj, FALSE);

	return obj;
}


void do_circle(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	int riding, circle, decept;

	if (!(circle = get_skill(ch,gsn_circle))) {
		send_to_char("Circle? What's that?\n\r",ch);
		return;
	}

	if (!(victim = ch->fighting)) {
		send_to_char("You aren't fighting anyone.\n\r", ch);
		return;
	}

	if (!(obj = get_eq_char(ch, WEAR_WIELD))) {
		send_to_char("You need to wield a primary weapon to circle.\n\r", ch);
		return;
	}

	if (MOUNTED(ch)) {
		act("You can't sneak around while riding $N!", ch, MOUNTED(ch), NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (victim->position > POS_SLEEPING && (victim->hit < (IS_NPC(victim) ? victim->max_hit/4 : victim->max_hit/2))) {
		act("$N is hurt and suspicious... you can't sneak around.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!can_see(ch, victim)) {
		send_to_char("You stumble blindly about, unable to find your opponent.\n\r", ch);
		return;
	}

	if (ch->move < 50) {
		send_to_char("You are too exhausted.\n\r", ch);
		return;
	}

	// Riding skill can fool circles
	if (MOUNTED(victim) && (riding = get_skill(victim,gsn_riding)) > 0) {
		CHAR_DATA *mount;
		int chance;
		int dam;

		chance = riding / 3;
		chance += get_curr_stat(victim, STAT_DEX) / 4;
		mount = MOUNTED(victim);

		if (number_percent() < chance) {
			dam = UMIN(ch->hit/6, 3000);
			dam *= riding / 100;
			act("{R$n notices $N circling around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n notices $N circling around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{R$n notices you circling around and kicks you with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			damage(mount, ch, dam, gsn_kick, DAM_BASH, TRUE);
			check_improve(victim, gsn_riding, TRUE, 10);
			return;
		}
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_CIRCLE,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_CIRCLE,"pretest",0,0,0,0,0))
		return;

	WAIT_STATE(ch, skill_table[gsn_circle].beats);
	deduct_move(ch, 50);

	decept = get_skill(victim, gsn_deception);

	if (number_percent() < circle && number_percent() > (decept / 10)) {
		act("{Y$n circles around behind you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{YYou circle around $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n circles around behind $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		one_hit(ch, victim, gsn_circle, FALSE);
		p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_CIRCLE,"message_pass",0,0,0,0,0);
		check_improve(ch,gsn_circle,TRUE,1);
	} else {
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_CIRCLE,"message_fail",0,0,0,0,0)) {
			act("{Y$n tries to circle around you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$N circles with you.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n tries to circle around $N.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			damage(ch, victim, 0, gsn_circle,DAM_NONE,TRUE);
		}
		check_improve(ch,gsn_circle,FALSE,1);

		if (decept > 0) check_improve(victim, gsn_deception, TRUE, 6);
	}
}


// Dwarven berserk
void do_berserk(CHAR_DATA *ch, char *argument)
{
	int chance;

	if (!(chance = get_skill(ch,gsn_berserk))) {
		send_to_char("You turn red in the face, but nothing happens.\n\r", ch);
		return;
	}

	if (IS_AFFECTED(ch,AFF_BERSERK)) {
		send_to_char("You get a little madder.\n\r", ch);
		return;
	}

	if (IS_AFFECTED(ch,AFF_CALM)) {
		send_to_char("You're feeling too mellow to berserk.\n\r",ch);
		return;
	}

	if (ch->mana < 50) {
		send_to_char("You can't gather enough energy.\n\r",ch);
		return;
	}

	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_BERSERK,"pretest",0,0,0,0,0))
		return;

	// Modifiers
	if (ch->position == POS_FIGHTING)
		chance += 10;

	ch->skill_chance = chance;
	p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_BERSERK,"chance",0,0,0,0,0);
	chance = ch->skill_chance;

	if (number_percent() < chance) {
		AFFECT_DATA af;
		memset(&af,0,sizeof(af));
		WAIT_STATE(ch,PULSE_VIOLENCE);
		ch->mana -= UMAX(ch->mana/6, 1);
		ch->move -= UMAX(ch->move/4, 1);

		// heal a little damage
		ch->hit += ch->tot_level * 2;
		ch->hit = UMIN(ch->hit,ch->max_hit);

		if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_BERSERK,"message_pass",0,0,0,0,0)) {
			send_to_char("{RYour pulse races as you are consumed by rage!{x\n\r",ch);
			act("{R$n gets a wild look in $s eyes.{x",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		}
		check_improve(ch,gsn_berserk,TRUE,2);

		af.where	= TO_AFFECTS;
		af.group     = AFFGROUP_METARACIAL;
		af.type		= gsn_berserk;
		af.level	= ch->tot_level;
		af.duration	= 5;
		af.modifier	= UMAX(1,ch->tot_level/10);
		af.bitvector 	= AFF_BERSERK;
		af.bitvector2 = 0;
		af.slot	= WEAR_NONE;

		af.location	= APPLY_HITROLL;
		affect_to_char(ch,&af);

		af.location	= APPLY_DAMROLL;
		affect_to_char(ch,&af);

		af.modifier	= UMAX(10,10 * (ch->tot_level/5));
		af.location	= APPLY_AC;
		affect_to_char(ch,&af);

		af.modifier     = (ch->max_hit * 1/6);
		af.location     = APPLY_HIT;
		affect_to_char(ch, &af);
	} else {
		WAIT_STATE(ch, PULSE_VIOLENCE);
		ch->mana -= 25;
		ch->move -= UMAX(ch->move/5, 1);

		if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_BERSERK,"message_fail",0,0,0,0,0))
			send_to_char("Your pulse speeds up, but nothing happens.\n\r",ch);
		check_improve(ch,gsn_berserk,FALSE,2);
	}
}


// Minotaur charge: bashes everyone in the room
void do_charge(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *rch, *rch_next;

	one_argument(argument,arg);

	if (!IS_NPC(ch) && get_skill(ch,gsn_charge) == 0)
	{
		send_to_char("You can't charge anything but drinks to your bar tab.\n\r",ch);
		return;
	}

	if (is_dead(ch))
		return;

	send_to_char("{YYou charge into the fray!{x\n\r", ch);
	act("{Y$n charges into the fray!{x\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	WAIT_STATE(ch, skill_table[gsn_charge].beats);

	// modifiers
	for (rch = ch->in_room->people; rch != NULL; rch = rch_next)
	{
		rch_next = rch->next_in_room;

		if (rch != ch &&
			!is_safe(ch, rch, FALSE) &&
			!is_same_group(ch, rch) &&
			can_see(ch, rch))
		{
			int count = 1;
			char name[80];
			char name_cnt[100];
			CHAR_DATA *rch_dup;

			// Necesarry for more than one NPC of a certain vnum in room.
			for (rch_dup = ch->in_room->people; rch_dup != NULL; rch_dup = rch_dup->next_in_room)
			{
				if (rch_dup == rch)
					break;

				if (!str_cmp(rch_dup->name, rch->name))
					count++;
			}

			one_argument(rch->name, name);

			sprintf(name_cnt, "%d.%s", count, name);

			do_bash(ch, name_cnt);
		}
	}

	check_improve(ch,gsn_charge,TRUE,1);
}


void do_intimidate(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	int chance;

	argument = one_argument(argument, arg);

	if (is_dead(ch))
		return;

	if (!(chance = get_skill(ch,gsn_intimidate))) {
		send_to_char("You attempt to look nasty.\n\r",ch);
		return;
	}

	if (!arg[0]) {
		if (!(victim = ch->fighting)) {
			send_to_char("But you aren't fighting anyone!\n\r",ch);
			return;
		}
	} else if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r",ch);
		return;
	}

	if (victim->position < POS_FIGHTING) {
		act("You'll have to let $M get back up first.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		return;
	}

	if (victim == ch) {
		send_to_char("You scare yourself.\n\r",ch);
		return;
	}

	if (is_safe(ch,victim, TRUE))
		return;

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_INTIMIDATE, "pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_INTIMIDATE, "pretest",0,0,0,0,0))
		return;

	//chance /= 3;
	chance += ch->tot_level - victim->tot_level;

	chance += (get_curr_stat(ch, STAT_STR) - get_curr_stat(victim, STAT_STR));

	chance += (ch->size - victim->size) * ((ch->size < victim->size)?15:10);

	act("$n looms over you!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("You loom over $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n looms over $N!",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

	if (number_percent() > chance) {
		if (!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_INTIMIDATE, "attack_fail",0,0,0,0,0) && IS_NPC(victim)) {
			act("$N laughs at you mercilessly.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$N laughs at $n mercilessly.",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT);
		}

		WAIT_STATE(ch,skill_table[gsn_intimidate].beats);
		return;
	}

	if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_INTIMIDATE, "attack_pass",0,0,0,0,0)) {
		act("{R$n cowers with fear and attempts to flee!{x", victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		act("{RYou are overcome with fear and panic!{x", victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	}
	check_improve(ch,gsn_intimidate,TRUE,1);
	do_function(victim, &do_flee, "anyway");
	DAZE_STATE(victim, 3 * PULSE_VIOLENCE);
	WAIT_STATE(ch,skill_table[gsn_intimidate].beats);
}


void do_bash(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	int chance, riding;
	int door;
	int dam;
	int ret;

	if (is_dead(ch))
	return;

	/* Syn - checks mobs just like players now. */
	if ((chance = get_skill(ch, gsn_bash)) < 1) {
		send_to_char("Bashing? What's that?\n\r",ch);
		return;
	}

	riding = get_skill(ch, gsn_riding);

	// Syn - added ifcheck until we add movement dice to mobs.
	if (!IS_NPC(ch) && ch->move < 50) {
		send_to_char("You're too exhausted to bash anything.\n\r", ch);
		return;
	}

	if (IS_NPC(ch)) // for mounts
		chance = (RIDDEN(ch)) ? get_skill(RIDDEN(ch), gsn_riding) : 80;

	if (MOUNTED(ch) && riding < 1) {
		send_to_char("You can't bash while riding!\n\r", ch);
		return;
	}

	// Bash a portal
	if ((obj = get_obj_here(ch, NULL, argument)) != NULL)
	{
		if (obj->item_type == ITEM_PORTAL)
		{
			if (!IS_SET(obj->value[1],EX_ISDOOR))
			{
				send_to_char("You can't do that.\n\r",ch);
				return;
			}

			if (!IS_SET(obj->value[1],EX_CLOSED))
			{
				act("$n attempts to bash down the already open $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				send_to_char("It's already open.\n\r", ch);
				return;
			}

			chance = get_skill(ch, gsn_bash) / 2 + 15;

			switch (ch->size) {
			case SIZE_TINY: chance = 0; break;
			case SIZE_SMALL: chance /= 2; break;
			case SIZE_MEDIUM: break;
			case SIZE_LARGE: chance *= .2; break;
			case SIZE_HUGE: chance *= 2; break;
			case SIZE_GIANT: chance *= 5; break;
			}

			chance += (get_curr_stat(ch, STAT_STR)) / 4;

			if (IS_AFFECTED2(ch, AFF2_WARCRY)) chance = chance * 3/2;

			act("You charge into $p at full speed!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("$n charges into $p at full speed!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

			dam = number_range(2, 15 + 2 * ch->size + chance/20 + ch->tot_level * 3);
			dam = UMIN(ch->hit, dam);

			damage(ch, ch, dam, gsn_bash, DAM_BASH, FALSE);
			deduct_move(ch, 75);

			if (IS_SET(obj->value[1], EX_NOBASH) ||
				(!obj->lock && !obj->lock->pick_chance && IS_SET(obj->value[1], EX_NOPASS)))
			{
				chance = chance / 5;	// Only 1/5 the original chance to break off any bars on the door

				if (IS_SET(obj->value[1], EX_BARRED) && number_percent() < chance)
				{
					REMOVE_BIT(obj->value[1], EX_BARRED);
					act("You rebound off $p, managing to break off the bars holding the $p closed, before flying backwards!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					act("$n rebounds off $p and flies backwards, breaking the bars on the $p in the process!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				}
				else
				{
					act("You rebound off $p and fly backwards!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					act("$n rebounds off $p and flies backwards!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				}


				ch->position = POS_RESTING;
				ch->bashed = number_range(2, 4);
				return;
			}

			if (number_percent() < chance) {
				if( obj->lock )
				{
					REMOVE_BIT(obj->lock->flags, LOCK_LOCKED);
				}

				REMOVE_BIT(obj->value[1], (EX_CLOSED|EX_BARRED));
				SET_BIT(obj->value[1], EX_BROKEN);

				act("$p explodes, sending pieces everywhere!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("$p explodes, sending pieces everywhere!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				check_improve(ch,gsn_bash,TRUE,5);
			}

			return;
		}
	}


	// Bash a door
	if ((door = find_door(ch, argument, FALSE)) >= 0) {
		ROOM_INDEX_DATA *to_room;
		EXIT_DATA *pexit;
		EXIT_DATA *pexit_rev;

		pexit = ch->in_room->exit[door];
		if (!IS_SET(pexit->exit_info, EX_CLOSED)) {
			act("$n attempts to bash down the already open $d.", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);
			send_to_char("It's already open.\n\r", ch);
			return;
		}

		chance = get_skill(ch, gsn_bash) / 2 + 15;

		switch (ch->size) {
		case SIZE_TINY: chance = 0; break;
		case SIZE_SMALL: chance /= 2; break;
		case SIZE_MEDIUM: break;
		case SIZE_LARGE: chance *= .2; break;
		case SIZE_HUGE: chance *= 2; break;
		case SIZE_GIANT: chance *= 5; break;
		}

		chance += (get_curr_stat(ch, STAT_STR)) / 4;

		if (IS_AFFECTED2(ch, AFF2_WARCRY)) chance = chance * 3/2;

		act("You charge into the $d at full speed!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_CHAR);
		act("$n charges into the $d at full speed!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);

		if (IS_SET(pexit->exit_info, EX_HIDDEN)) REMOVE_BIT(pexit->exit_info, EX_HIDDEN);

		dam = number_range(2, 15 + 2 * ch->size + chance/20 + ch->tot_level * 3);
		dam = UMIN(ch->hit, dam);

		damage(ch, ch, dam, gsn_bash, DAM_BASH, FALSE);
		deduct_move(ch, 75);

		if (IS_SET(pexit->exit_info, EX_NOBASH) ||
			(!pexit->door.lock.pick_chance && IS_SET(pexit->exit_info, EX_NOPASS)))
		{

			chance = chance / 5;	// Only 1/5 the original chance to break off any bars on the door

			if (IS_SET(pexit->exit_info, EX_BARRED) && number_percent() < chance)
			{
				REMOVE_BIT(pexit->exit_info , EX_BARRED);
				act("You rebound off the $d, managing to break off the bars holding the $p closed, before flying backwards!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_CHAR);
				act("$n rebounds off the $d and flies backwards, breaking the bars on the $p in the process!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);
			}
			else
			{
				act("You rebound off the $d and fly backwards!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_CHAR);
				act("$n rebounds off the $d and flies backwards!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);
			}


			ch->position = POS_RESTING;
			ch->bashed = number_range(2, 4);
			return;
		}

		if (number_percent() < chance) {
			if (IS_SET(pexit->door.lock.flags, LOCK_LOCKED)) REMOVE_BIT(pexit->door.lock.flags, LOCK_LOCKED);
			if (IS_SET(pexit->exit_info, EX_CLOSED)) REMOVE_BIT(pexit->exit_info , EX_CLOSED);
			if (IS_SET(pexit->exit_info, EX_BARRED)) REMOVE_BIT(pexit->exit_info , EX_BARRED);

			SET_BIT(pexit->exit_info, EX_BROKEN);

			if ((to_room = pexit->u1.to_room) && (pexit_rev = to_room->exit[rev_dir[door]]) &&
				pexit_rev->u1.to_room == ch->in_room) {

				if (IS_SET(pexit_rev->door.lock.flags , LOCK_LOCKED)) REMOVE_BIT(pexit->door.lock.flags , LOCK_LOCKED);
				if (IS_SET(pexit_rev->exit_info , EX_CLOSED)) REMOVE_BIT(pexit_rev->exit_info, EX_CLOSED);
				if (IS_SET(pexit_rev->exit_info , EX_BARRED)) REMOVE_BIT(pexit_rev->exit_info, EX_BARRED);

				SET_BIT(pexit_rev->exit_info, EX_BROKEN);

				act("The $d explodes from outwards!", to_room->people, NULL, NULL, NULL, NULL, NULL, pexit_rev->keyword, TO_ALL);
			}

			act("The $d explodes, sending pieces everywhere!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_CHAR);
			act("The $d explodes, sending pieces everywhere!", ch, NULL, NULL, NULL, NULL, NULL, pexit->keyword, TO_ROOM);
			check_improve(ch,gsn_bash,TRUE,5);
		}

		check_improve(ch,gsn_bash,FALSE,5);
		return;
	}

	// Bash a victim
	if (IS_NULLSTR(argument)) {
		victim = ch->fighting;
		if (!victim) {
			send_to_char("You aren't fighting anyone.\n\r" , ch);
			return;
		}
	} else {
		victim = get_char_room(ch, NULL, argument);
		if (!victim) {
			send_to_char("They aren't anywhere around here.\n\r", ch);
			return;
		}

		if (victim == ch) {
			send_to_char("What would be the point of that?\n\r", ch);
			return;
		}
	}

	if (is_safe(ch, victim, TRUE))
		return;

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BASH, "pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BASH, "pretest",0,0,0,0,0))
		return;

	chance += (get_curr_stat(ch, STAT_STR)) / 4;
	chance += (get_curr_stat(ch, STAT_DEX)) / 5;
	chance -= (get_curr_stat(victim, STAT_DEX)) / 5;

	// If mounted, you can charge on your mount.
	if (MOUNTED(ch)) {
		CHAR_DATA *mount;

		mount = MOUNTED(ch);

		if (mount == victim) {
			act("You can't bash $N while you are riding $M!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if (mount->position <= POS_RESTING) {
			send_to_char("Your mount is in no shape to bash.\n\r", ch);
			return;
		}

		if(p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BASH, "premount",0,0,0,0,0)) return;

		WAIT_STATE(ch, skill_table[gsn_bash].beats);
		act("You dig your heels into $N's flanks and charge!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n digs $s heels into $N's flanks and charges!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		switch (mount->size) {
		case SIZE_TINY: chance = 0; break;
		case SIZE_SMALL: chance /= 2; break;
		case SIZE_MEDIUM: break;
		case SIZE_LARGE: chance *= 1.2; break;
		case SIZE_HUGE: chance *= 2; break;
		case SIZE_GIANT: chance *= 5; break;
		}

		switch (victim->size) {
		case SIZE_TINY: chance *= 5; break;
		case SIZE_SMALL: chance *= 2; break;
		case SIZE_MEDIUM: break;
		case SIZE_LARGE: chance /= 1.2; break;
		case SIZE_HUGE: chance /= 2; break;
		case SIZE_GIANT: chance /= 5; break;
		}

		if (number_percent() < chance) {
			dam = (get_skill(ch, gsn_bash) + get_skill(ch, gsn_riding)) / 2;
			if (MOUNTED(ch)->size > victim->size) dam *= (MOUNTED(ch)->size - victim->size);
			if (MOUNTED(ch)->size < victim->size) dam /= (victim->size - MOUNTED(ch)->size);

			if (ch->tot_level > victim->tot_level) dam += ch->tot_level - victim->tot_level;
			if (ch->tot_level < victim->tot_level) dam -= victim->tot_level - ch->tot_level;

			dam = UMAX(dam, 1);

			victim->hit_damage = dam;
			p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BASH, "damage",0,0,0,0,0);
			dam = victim->hit_damage;
			victim->hit_damage = 0;

			if(dam > 0) damage(ch, victim, dam, gsn_bash, DAM_BASH, TRUE);

			ret = p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BASH, "message_pass_mount",0,0,0,0,0);
			if(ret < 0) ret = 0;
			if(!(ret & 2)) {
				victim->position = POS_RESTING;
				victim->bashed   = 10 * (30 - get_curr_stat(victim, STAT_DEX))/10 + number_range(1, 5);
			}
			check_improve(ch, gsn_riding, TRUE, 1);
			check_improve(ch, gsn_bash, TRUE, 5);
		} else {
			ret = p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BASH, "message_fail_mount",0,0,0,0,0);
			if(ret < 0) ret = 0;
			if(!(ret & 1)) {
				act("$N falls flat on $S face!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$N falls flat on $S face!", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			}
			if(!(ret & 2)) {
				mount->position = POS_RESTING;
				mount->bashed   = (int) 10 * (30 - get_curr_stat(mount, STAT_DEX)) / 10 + number_range(1, 5);
			}
			check_improve(ch, gsn_bash, FALSE, 5);
		}

		deduct_move(mount, 75);
		return;
	}


	// Non mounted section here
	switch (ch->size) {
	case SIZE_TINY: chance = 0; break;
	case SIZE_SMALL: chance /= 2; break;
	case SIZE_MEDIUM: break;
	case SIZE_LARGE: chance *= 1.2; break;
	case SIZE_HUGE: chance *= 2; break;
	case SIZE_GIANT: chance *= 5; break;
	}

	switch (victim->size) {
	case SIZE_TINY: chance *= 5; break;
	case SIZE_SMALL: chance *= 2; break;
	case SIZE_MEDIUM: break;
	case SIZE_LARGE: chance /= 1.2; break;
	case SIZE_HUGE: chance /= 2; break;
	case SIZE_GIANT: chance /= 5; break;
	}

	// Succeeded bash!
	if (number_percent() < chance) {
		act("You slam into $N...", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n slams into you...", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$n slams into $N...", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

		dam = (ch->tot_level + get_skill(ch, gsn_bash))*7*log10(ch->tot_level);
		if (ch->size > victim->size) dam *= (ch->size - victim->size);
		if (ch->size < victim->size) dam /= (victim->size - ch->size);

		/* AO 092516 stupid
		if (ch->tot_level > victim->tot_level) dam += ch->tot_level - victim->tot_level;
		if (ch->tot_level < victim->tot_level) dam -= victim->tot_level - ch->tot_level;
		*/

		if (get_skill(ch, gsn_martial_arts) > 0) {
			dam += (dam * get_skill(ch, gsn_martial_arts))/100;
			check_improve(ch,gsn_martial_arts,TRUE,6);
		}

		if (IS_MINOTAUR(ch)) dam *= 2;

		dam = UMAX(dam, 1);

		victim->hit_damage = dam;
		p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BASH, "damage",0,0,0,0,0);
		dam = victim->hit_damage;
		victim->hit_damage = 0;

		if(dam > 0) damage(ch, victim, dam, gsn_bash, DAM_BASH, TRUE);

		ret = p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BASH, "message_pass",0,0,0,0,0);
		if (ret < 0) ret = 0;
		if (!(ret & 1)) {
			act("You send $N sprawling!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n sends you sprawling!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("$n sends $M sprawling!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
		if (!(ret & 2)) {
			victim->position = POS_RESTING;
			victim->bashed = 10 * (30 - get_curr_stat(victim, STAT_DEX))/10 + number_range(1, 5);
		}
		check_improve(ch, gsn_bash, TRUE, 5);
	} else { // Failed
		ret = p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BASH, "message_fail",0,0,0,0,0);
		if (ret < 0) ret = 0;
		if (!(ret & 1)) {
			act("You fall flat on your face!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n falls flat on $s face.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("You evade $n's bash, causing $m to fall flat on $s face.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		}
		if (!(ret & 2)) {
			ch->position = POS_RESTING;
			ch->bashed = (int) 10 * (30 - get_curr_stat(ch, STAT_DEX)) / 10 + number_range(1, 5);
		}

		check_improve(ch,gsn_bash,FALSE,5);
	}

	WAIT_STATE(ch,skill_table[gsn_bash].beats * 3/2);
	deduct_move(ch, 75);
}


void do_bite(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	AFFECT_DATA af;
	CHAR_DATA *victim;
	int chance;
	int liquid;
	int amount, dam;
	int i;

	argument = one_argument(argument,arg);
	argument = one_argument(argument,arg2);

	if (is_dead(ch)) return;

	if (!(chance = get_skill(ch,gsn_bite))) {
		send_to_char("Your mouth isn't really made for biting.\n\r",ch);
		return;
	}

	if (!IS_SET(ch->parts,PART_FANGS)) {
		send_to_char("You lack the fangs for biting.\n\r",ch);
		return;
	}

	if (!arg[0]) {
		victim = ch->fighting;
		if (!victim || !can_see(ch,victim)) {
			if (IS_VAMPIRE(ch)) {
				send_to_char("You show your teeth!\n\r",ch);
				act("$n growls and bares $s fangs!",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			} else {
				send_to_char("You hiss and show your fangs!\n\r", ch);
				act("$n hisses menacingly and shows $s fangs!",ch, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
			}
			return;
		}
	} else if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r",ch);
		return;
	}

	if (victim == ch) {
		send_to_char("You try to bite yourself, but fail.\n\r",ch);
		return;
	}

	if (is_safe(ch,victim, TRUE))
		return;

	// Add in ability to modify chances?

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BITE,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BITE,"pretest",0,0,0,0,0))
		return;

	// stats
	chance += get_curr_stat(ch,STAT_STR);
	chance -= (get_curr_stat(victim,STAT_DEX) * 4)/3;

	// speed
	if (IS_AFFECTED(ch,AFF_HASTE)) chance += 10;
	if (IS_AFFECTED(victim,AFF_HASTE)) chance -= 15;

	// level
	chance += (ch->tot_level - victim->tot_level);

	if (IS_AFFECTED2(victim, AFF2_STONE_SKIN)) chance /= 2;

	memset(&af,0,sizeof(af));

	if (number_percent() < chance) {	// Success
		DAZE_STATE(victim, 2 * PULSE_VIOLENCE);
		WAIT_STATE(ch,skill_table[gsn_bite].beats);

		// Vamp bite
		if (IS_VAMPIRE(ch)) {
			act("{R$n leaps for your neck sinking $s fangs in deep!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{RYou leap for $N's neck sinking your fangs in deep!{x",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
			act("{R$n takes a bite out of $N's neck.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

			check_improve(ch,gsn_bite,TRUE,1);

			if (!IS_NPC(ch) &&  !IS_IMMORTAL(ch) &&  ch->pcdata->condition[COND_FULL] > 45) {
				send_to_char("You're too full to drink more.\n\r",ch);
				return;
			}

			victim->hit_damage = dam = dice(ch->tot_level, 12);

			if(p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BITE,"damage vampire",0,0,0,0,0))
				return;

			dam = victim->hit_damage;
			victim->hit_damage = 0;

			if(dam > 0) {
				// do some damage
				damage(ch,victim,dam,gsn_bite,DAM_BITE,FALSE);

				if (number_percent() < 15) {
					act("{RYour unholy bite causes $N to go into a paroxysm!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{YEvil toxins from $n's bite race through your body, triggering a paroxysm!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{YYou thrash about wildly, unable to control yourself!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					WAIT_STATE(victim, 1*PULSE_VIOLENCE);
					PAROXYSM_STATE(victim, URANGE(1, ch->tot_level/5, 25));
				}

				liquid = liq_lookup("blood"); // Value for blood
				amount = liq_table[liquid].liq_affect[4] * 3;
				gain_condition(ch, COND_FULL, amount * 8 / 4);
				gain_condition(ch, COND_THIRST, amount * 8 / 10);
				gain_condition(ch, COND_HUNGER, amount * 8 / 2);

				if (!IS_NPC(ch) && ch->pcdata->condition[COND_FULL] > 40) send_to_char("{GYou are full.{x\n\r", ch);
				if (!IS_NPC(ch) && ch->pcdata->condition[COND_THIRST] > 40) send_to_char("{GYour thirst is quenched.{x\n\r", ch);
			}
		} else if (IS_SITH(ch)) {	// sith bite
			act("{R$n bites you with $s fangs!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{RYou bite $N with your fangs!{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n bites $N with $s fangs!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

			check_improve(ch,gsn_bite,TRUE,1);

			// Use a toxin
			if (arg2[0]) {
				/*
				if (IS_IMMORTAL(victim)) {
					act("{R$N grabs your jaw and pulls it off your face!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					act("{RYou grab $n's jaw and pull it off $s face!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("{R$N grabs $n's jaw and pulls it off $s face!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
					victim->set_death_type = DEATHTYPE_RAWKILL;
					damage(victim, ch, 30000, 0, 0, FALSE);
					return;
				}*/

				if ((i = toxin_lookup(arg2)) < 0) return;

				if (ch->toxin[i] < 5) {
					act("Your $t supply is too low.", ch, NULL, NULL, NULL, NULL, toxin_table[i].name, NULL, TO_CHAR);
					return;
				}

				victim->hit_damage = dam = dice(ch->tot_level, 12);
				sprintf(arg,"damage '%s'", toxin_table[i].name);
				if(p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BITE,arg,0,0,0,0,0)) return;
				dam = victim->hit_damage;
				victim->hit_damage = 0;

				if (number_percent() > get_skill(ch,gsn_toxins)) {
					act("You shudder momentarily, but shrug it off.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					act("$n shudders momentarily, but shrugs it off.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					check_improve(ch, gsn_toxins, FALSE, 1);
					return;
				} else {
					act("{R$n begins to shudder violently.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					act("{RYou begin to shudder violently as you feel toxins seeping through your body.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
					check_improve(ch, gsn_toxins, TRUE, 1);
				}

				victim->bitten_type = i;
				ch->toxin[i] -= number_range(toxin_table[i].cost[0], toxin_table[i].cost[1]);
				victim->bitten = UMAX(5 * get_skill(ch,gsn_toxins)/ch->tot_level, 30);
				victim->bitten_level = ch->tot_level;

				if (!IS_SET(victim->affected_by[1], AFF2_TOXIN)) {
					af.where = TO_AFFECTS;
					af.group     = AFFGROUP_BIOLOGICAL;
					af.type  = gsn_toxins;
					af.level = victim->bitten_level;
					af.duration = 5;
					af.location = APPLY_STR;
					af.modifier = -1 * number_range(1,3);
					af.bitvector = 0;
					af.bitvector2 = AFF2_TOXIN;
					af.slot	= WEAR_NONE;
					affect_to_char(victim, &af);
				}
			} else {
				victim->hit_damage = dice(ch->tot_level, 12);
				if(p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BITE,"damage poison",0,0,0,0,0)) return;
				dam = victim->hit_damage;
				victim->hit_damage = 0;

				if (number_percent() < 20) {           // Chance of poison
					int level;
					AFFECT_DATA af;
					memset(&af,0,sizeof(af));

					level = ch->tot_level;

					if (!saves_spell(level / 2,victim,DAM_POISON) && !arg2[0]) {
						send_to_char("{gYou feel poison coursing through your veins.{x\n\r", victim);
						act("{g$n is poisoned by venom!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

						af.where     = TO_AFFECTS;
						af.group     = AFFGROUP_BIOLOGICAL;
						af.type      = gsn_poison;
						af.level     = level * 3/4;
						af.duration  = URANGE(1,level / 2, 5);
						af.location  = APPLY_STR;
						af.modifier  = -1;
						af.bitvector = AFF_POISON;
						af.bitvector2 = 0;
						af.slot	= WEAR_NONE;
						affect_join(victim, &af);
					}
				}
			}

			if(dam > 0) damage(ch,victim,dam,gsn_bite,DAM_BITE,FALSE);
		} else {
			victim->hit_damage = number_range(2,2 + 2 * ch->size + chance/20);
			if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ATTACK_BITE,"damage unknown",0,0,0,0,0)) {
				dam = victim->hit_damage;
				victim->hit_damage = 0;
				// All other bites (i'm sure there'll be more);
				damage(ch,victim,dam,gsn_bite,DAM_BITE,TRUE);
			} else
				victim->hit_damage = 0;
		}
	} else {	// Failed!
		damage(ch,victim,0,gsn_bite,DAM_BITE,FALSE);
		act("You miss!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("$n misses $N's neck.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("You evade $n's bite.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		check_improve(ch,gsn_bite,FALSE,1);
		WAIT_STATE(ch,skill_table[gsn_bite].beats * 3/2);
	}
}


void bitten_end(CHAR_DATA *ch)
{
	if (!ch) {
		bug("bitten_end: ch was null!", 0);
		return;
	}

	act("{RYour senses ignite as the toxins reach your brain.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$n's face flushes red as the toxins reach $s brain.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	affect_strip(ch, gsn_toxins);
	(*toxin_table[ch->bitten_type].spell) (gsn_toxins, ch->bitten_level, NULL, ch, TARGET_CHAR, WEAR_NONE);

	ch->bitten = 0;
	ch->bitten_level = 0;
	ch->bitten_type = 0;
}


void do_dirt(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	int chance, decept;

	one_argument(argument,arg);

	if (is_dead(ch))
		return;

	if (!(chance = get_skill(ch,gsn_dirt))) {
		send_to_char("You get your feet dirty.\n\r",ch);
		return;
	}

	victim = ch->fighting;
	if (!arg[0] && !victim) {
		send_to_char("Dirt kick whom?\n\r",ch);
		return;
	}

	if (MOUNTED(ch)) {
		send_to_char("You can't dirt kick while riding!\n\r", ch);
		return;
	}

	if (!victim && !(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r",ch);
		return;
	}

	if (IS_AFFECTED(victim,AFF_BLIND)) {
		act("$E's already been blinded.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (victim == ch) {
		send_to_char("Very funny.\n\r",ch);
		return;
	}

	if (is_safe(ch,victim, TRUE))
		return;

	if ((decept = get_skill(victim, gsn_deception)) > 0 && (number_percent() < (3 * decept / 4))) {
		act("$N anticipates your dirt kick and covers $S eyes!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("You quickly cover your eyes, evading $n's dirt kick!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$N anticipates $n's dirt kick and covers $S eyes!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

		check_improve(victim, gsn_deception, TRUE, 6);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DIRTKICK,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DIRTKICK,"pretest",0,0,0,0,0))
		return;

	// dex
	chance += get_curr_stat(ch,STAT_DEX);
	chance -= get_curr_stat(victim,STAT_DEX);

	// speed
	if (IS_AFFECTED(ch,AFF_HASTE)) chance += 10;
	if (IS_AFFECTED(victim,AFF_HASTE)) chance -= 15;

	chance += (ch->tot_level - victim->tot_level);

	// sloppy hack to prevent false zeroes
	if (!(chance % 5)) ++chance;

	// the right terrain helps
	switch(ch->in_room->sector_type) {
	case SECT_INSIDE:		chance -= 20;	break;
	case SECT_CITY:			chance -= 10;	break;
	case SECT_FIELD:		chance += 5;	break;
	case SECT_FOREST:				break;
	case SECT_HILLS:				break;
	case SECT_MOUNTAIN:		chance -= 10;	break;
	case SECT_WATER_SWIM:		chance = 0;	break;
	case SECT_WATER_NOSWIM:		chance = 0;	break;
	case SECT_TUNDRA:		chance = 0;	break;
	case SECT_AIR:			chance = 0;	break;
	case SECT_DESERT:		chance += 10;	break;
	case SECT_NETHERWORLD:		chance = 0;	break;
	case SECT_DOCK:			chance -= 10;	break;
	case SECT_ENCHANTED_FOREST:			break;
	case SECT_TOXIC_BOG:		chance = 0;	break;
	case SECT_CURSED_SANCTUM:	chance -= 20;	break;
	case SECT_BRAMBLE:				break;
	case SECT_SWAMP:		chance = 0;	break;
	case SECT_ACID:			chance = 0;	break;
	case SECT_LAVA:			chance = 0;	break;
	case SECT_SNOW:			chance = 0;	break;
	case SECT_ICE:			chance = 0;	break;
	case SECT_CAVE:			chance -= 5;	break;
	case SECT_UNDERWATER:		chance = 0;	break;
	case SECT_DEEP_UNDERWATER:	chance = 0;	break;
	case SECT_JUNGLE:				break;
	}

	if (!chance) {
		send_to_char("There isn't any dirt to kick.\n\r",ch);
		return;
	}

	chance /= 2;

	/* now the attack */
	if (number_percent() < chance) {
		AFFECT_DATA af;
		memset(&af,0,sizeof(af));
		act("{D$n is blinded by the dirt in $s eyes!{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
		act("{D$n kicks dirt in your eyes!{x",ch,victim, NULL, NULL, NULL, NULL, NULL,TO_VICT);
		// @@@NIB - why should this do damage?
//		damage(ch,victim,number_range(2,5),gsn_dirt,DAM_NONE,FALSE);
		send_to_char("{DYou can't see a thing!\n\r{x",victim);
		check_improve(ch,gsn_dirt,TRUE,2);
		WAIT_STATE(ch,skill_table[gsn_dirt].beats);

		af.where	= TO_AFFECTS;
		af.group     = AFFGROUP_PHYSICAL;
		af.type 	= gsn_blindness;
		af.level 	= ch->tot_level;
		af.duration	= 1;
		af.location	= APPLY_HITROLL;
		af.modifier	= -4;
		af.bitvector 	= AFF_BLIND;
		af.bitvector2 = 0;
		af.slot	= WEAR_NONE;

		affect_to_char(victim,&af);

		p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DIRTKICK,"attack_pass",0,0,0,0,0);
	} else {
		long cid[2], vid[2];

		char_id(ch, cid);
		char_id(victim, vid);

		if (number_percent() < 5) {
			AFFECT_DATA af;
			memset(&af,0,sizeof(af));
			act("Oh no! You missed and got yourself in your eye!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("What a fool! $n has managed to kick dirt into $s OWN eyes!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			damage(ch,ch,number_range(2,5),gsn_dirt,DAM_NONE,FALSE);
			send_to_char("{DYou can't see a thing!\n\r{x",ch);
			WAIT_STATE(ch,skill_table[gsn_dirt].beats);

			af.where	= TO_AFFECTS;
			af.group     = AFFGROUP_PHYSICAL;
			af.type 	= gsn_blindness;
			af.level 	= ch->tot_level;
			af.duration	= 1;
			af.location	= APPLY_HITROLL;
			af.modifier	= -4;
			af.bitvector 	= AFF_BLIND;
			af.bitvector2 = 0;
			af.slot	= WEAR_NONE;

			affect_to_char(ch,&af);
			return;
		}

		damage(ch,victim,0,gsn_dirt,DAM_NONE,TRUE);
		if( is_char_stillvalid(ch, cid) )
		{
			check_improve(ch,gsn_dirt,FALSE,2);
			WAIT_STATE(ch,skill_table[gsn_dirt].beats);

			if( is_char_stillvalid(victim, vid) )
				p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DIRTKICK,"attack_fail",0,0,0,0,0);
		}
	}
}


void do_kill(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;

	one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Kill whom?\n\r", ch);
		return;
	}

	if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (victim == ch) {
		send_to_char("You hit yourself. Ouch!\n\r", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE))
		return;

	if (ch->position == POS_FIGHTING) {
		send_to_char("You're already fighting!\n\r", ch);
		return;
	}

	multi_hit(ch, victim, TYPE_UNDEFINED);
	multi_hit(victim, ch, TYPE_UNDEFINED);
	set_fighting(ch, victim);
}


// Dracon/Dragon breathe skill.
void do_breathe(CHAR_DATA *ch, char *argument)
{
	static char *breath_names[] = { "acid", "fire", "frost", "gas", "lightning", NULL };
	static sh_int *breath_gsn[] = { &gsn_acid_breath, &gsn_fire_breath, &gsn_frost_breath, &gsn_gas_breath, &gsn_lightning_breath };
	static SPELL_FUN *breath_fun[] = { spell_acid_breath, spell_fire_breath, spell_frost_breath, spell_gas_breath, spell_lightning_breath };
	char arg[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	int mov, i;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (is_dead(ch))
		return;

	if (!get_skill(ch, gsn_breath)) {
		send_to_char("You can't breathe anything but air.\n\r", ch);
		return;
	}

	if (!arg[0]) {
		send_to_char("Breath what, and at whom?\n\r", ch);
		return;
	}

	if (!(victim = get_char_room(ch, NULL, arg2)) && !(victim = ch->fighting)) {
		send_to_char("Breathe at whom?\n\r", ch);
		return;
	}

	if (is_safe(ch,victim, TRUE))
		return;

	for(i=0;breath_names[i] && str_prefix(arg,breath_names[i]);i++);

	if(!breath_names[i]) {
		send_to_char("Apart from your bad breath, you can breath acid, lightning, gas, fire or frost.\n\r", ch);
		return;
	}

	/* Doesn't apply to NPCs until we give them movement (Syn)  */
	if (!IS_NPC(ch)) {
		mov = URANGE(5, ch->max_move/9, 100);
		if (ch->move < mov) {
			send_to_char("You don't have enough stamina to breath.\n\r", ch);
			return;
		}

		ch->move -= mov;
	}

	WAIT_STATE(ch,skill_table[gsn_breath].beats);

	(*breath_fun[i]) (*breath_gsn[i] , ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);

	check_improve(ch,gsn_breath,TRUE,4);
}


void do_backstab(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *rch, *rch_next;
	CHAR_DATA *victim;
	int dam;
	int roll;
	int skill, skill2, chance;
	OBJ_DATA *wield;
	bool failed = FALSE;

	one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Backstab whom?\n\r",ch);
		return;
	}

	if (is_dead(ch)) return;

	skill = get_skill(ch, gsn_backstab);
	skill2 = get_skill(ch, gsn_sword_and_dagger_style);

	if (!skill) {
		send_to_char("Leave that to the thieves.\n\r", ch);
		return;
	}

	if (MOUNTED(ch)) {
		send_to_char("You can't backstab while riding!\n\r", ch);
		return;
	}

	wield = get_eq_char(ch, WEAR_WIELD);

	if (!wield || (wield->value[0] != WEAPON_DAGGER && attack_table[wield->value[3]].damage != DAM_PIERCE)) {
		wield = get_eq_char(ch, WEAR_SECONDARY);
		if (!wield || (wield->value[0] != WEAPON_DAGGER && attack_table[wield->value[3]].damage != DAM_PIERCE)) {
			send_to_char("You can only backstab with a dagger or piercing type weapon.\n\r", ch);
			return;
		}
	}

	if (ch->fighting) {
		send_to_char("You're facing the wrong end.\n\r",ch);
		return;
	} else if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r",ch);
		WAIT_STATE(ch, skill_table[gsn_backstab].beats);
		return;
	}

	if (victim == ch) {
		send_to_char("How can you sneak up on yourself?\n\r", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE))
		return;

	if (victim->fighting) {
		send_to_char("You can't backstab a person who is fighting!\n\r", ch);
		return;
	}

	if (victim->position > POS_SLEEPING && (victim->hit < (3 * victim->max_hit / 4))) {
		act("$N is hurt and suspicious ... you can't sneak up.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		WAIT_STATE(ch, skill_table[gsn_backstab].beats);
		return;
	}

	for (rch = ch->in_room->people; rch; rch = rch_next) {
		rch_next = rch->next_in_room;
		if (rch->fighting && (rch->leader == ch || ch->leader == rch || rch->master == ch || ch->master == rch)) {
			act("$N is already in combat with your group.", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	// mounted people with riding skill can prevent backstabs
	if (MOUNTED(victim) && (chance = get_skill(victim,gsn_riding)) > 0) {
		CHAR_DATA *mount;
		int dam_mount;

		chance = (chance / 3) + get_curr_stat(victim, STAT_DEX) / 4;
		mount = MOUNTED(victim);

		if (number_percent() < chance) {
			dam_mount = UMIN(ch->hit/6, 3000);
			dam_mount *= chance / 100;
			act("{R$n notices $N sneaking around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n notices $N sneaking around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{R$n notices you sneaking around and kicks you with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			damage(mount, ch, dam_mount, gsn_kick, DAM_BASH, TRUE);
			multi_hit(victim, ch, TYPE_UNDEFINED);
			check_improve(victim, gsn_riding, TRUE, 10);
			return;
		}
	}

	roll = get_skill(victim, gsn_deception) / 10;
	if (number_percent() < roll) {
		act("{R$N notices your attempt to sneak around for a backstab!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{RYou notice $n's attempt to sneak around for a backstab!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{R$N notices $n's attempt to sneak around for a backstab!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);

		check_improve(victim, gsn_deception, TRUE, 8);
		multi_hit(victim, ch, TYPE_UNDEFINED);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BACKSTAB,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BACKSTAB,"pretest",0,0,0,0,0))
		return;

	WAIT_STATE(ch, skill_table[gsn_backstab].beats);

	dam = 0;
	if (number_percent() < skill || (skill >= 2 && !IS_AWAKE(victim))) {
		if (!IS_NPC(victim))
			dam = victim->max_hit*3/2;
		else if (victim->tot_level < 200)
			dam = 2*victim->max_hit;
		else
			dam = victim->max_hit/10;

		//if (ch->tot_level < victim->tot_level) dam = (dam * ch->tot_level)/(victim->tot_level/2);

		dam += 3 * dice(wield->value[1], wield->value[2]);
		dam = UMIN(dam, MAX_BACKSTAB_DAMAGE);

		victim->hit_damage = dam;

		failed = p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, wield, NULL, TRIG_ATTACK_BACKSTAB, "damage first",0,0,0,0,0);

		dam = victim->hit_damage;
		victim->hit_damage = 0;

		if(dam > 0) damage(ch, victim, dam, gsn_backstab, DAM_BACKSTAB, TRUE);
		check_improve(ch,gsn_backstab,TRUE,1);
	} else {
		send_to_char("You miss your backstab!\n\r", ch);
		failed = TRUE;
		check_improve(ch,gsn_backstab,FALSE,1);
	}

	if (!failed && get_profession(ch, SECOND_SUBCLASS_THIEF) == CLASS_THIEF_NINJA && victim && !IS_DEAD(victim) && victim->hit >= 1 &&
		wields_item_type(ch, WEAPON_SWORD) && wields_item_type(ch, WEAPON_DAGGER) && skill2 > 0) {
		if(number_percent() < skill2) {
			OBJ_DATA *wield2;

			wield2 = get_eq_char(ch, WEAR_WIELD);
			if (wield == wield2)
				wield2 = get_eq_char(ch, WEAR_SECONDARY);

			if (dam == 0) dam = victim->max_hit/3;

			act("{RYou slice $N with $p{R as $E turns around!{x", ch, victim, NULL, wield2, NULL, NULL, NULL, TO_CHAR);
			act("{R$n slices $N with $p{R as $E turns around!{x", ch, victim, NULL, wield2, NULL, NULL, NULL, TO_NOTVICT);
			act("{R$n slices you with $p{R as you turn around!{x", ch, victim, NULL, wield2, NULL, NULL, NULL, TO_VICT);

			dam /= 2;

			victim->hit_damage = dam;

			failed = p_percent_trigger(victim,NULL, NULL, NULL, ch, NULL, NULL, wield2, NULL, TRIG_ATTACK_BACKSTAB, "damage second",0,0,0,0,0);

			dam = victim->hit_damage;
			victim->hit_damage = 0;

			if(dam > 0) damage(ch, victim, dam, gsn_sword_and_dagger_style, DAM_SLASH, TRUE);
			check_improve(ch,gsn_sword_and_dagger_style,TRUE,1);
		} else
			check_improve(ch,gsn_sword_and_dagger_style,FALSE,1);
	}

	// Make sure to start some shit if the backstab failed.
	if (failed)
		multi_hit(victim, ch, TYPE_HIT);
}


void do_burgle(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char buf[MAX_STRING_LENGTH];
	int chance;
	int ammount;

	one_argument(argument, arg);

	// MAYBE ADD OTHER OPTIONS LATER
	if (arg[0]) {
		send_to_char("Just ""Burgle"" the place\n\r", ch);
		return;
	}

	if (!IS_SET(ch->in_room->room_flags, ROOM_BANK)) {
		send_to_char("You have to be IN a bank to rob it\n\r", ch);
		return;
	}

	check_improve(ch,gsn_burgle,TRUE,5);
	chance = get_skill(ch, gsn_burgle) / 10;

	if (ch->level > LEVEL_HERO)
		chance = 100;

	if (number_percent() < chance) {
		ammount = (number_percent() * ch->tot_level) / 10;
		sprintf(buf, "{WYou sly devil, you made off with {Y%d gold {Wcoins\n\r", ammount);
		send_to_char(buf, ch);
		ch->gold += ammount;
	} else {
		send_to_char("{MOh No, the Guards caught you, and now they're going to kill you!\n\r", ch);
		send_to_char("{RThe bank guards linch you on the closest tree\n\r", ch);

		raw_kill(ch, TRUE, TRUE, RAWKILL_NORMAL);
	}
}


void do_slit(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	OBJ_DATA *wield;
	int chance;

	one_argument(argument, arg);

	if (!get_skill(ch, gsn_slit_throat)) {
		send_to_char("You have no knowledge of this skill.\n\r", ch);
		return;
	}

	if (!(IS_AFFECTED(ch, AFF_SNEAK))) {
		send_to_char("You aren't moving silently enough.\n\r", ch);
		return;
	}

	if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (!IS_IMMORTAL(ch) && IS_AWAKE(victim) && !IS_AFFECTED(victim, AFF_SLEEP)) {
		act("$N must be sleeping to slit $S throat.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (victim == ch) {
		send_to_char("You might hurt yourself! \n\r ", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE))
		return;

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_SLIT,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_SLIT,"pretest",0,0,0,0,0))
		return;

	int skill = get_skill(ch, gsn_slit_throat);
	chance = skill;
	chance += (ch->tot_level - victim->tot_level);
	if (get_profession(ch, SECOND_SUBCLASS_THIEF) == CLASS_THIEF_NINJA &&
		get_profession(ch, SUBCLASS_THIEF) == CLASS_THIEF_ASSASSIN)
		chance = 3 * chance / 2;

	if (!get_eq_char(ch, WEAR_WIELD) && !get_eq_char(ch, WEAR_SECONDARY)) {
		send_to_char("You can't slit someone's throat without a weapon!\n\r", ch);
		return;
	}

	wield = get_eq_char(ch, WEAR_WIELD);
	if (!wield || (wield->value[0] != WEAPON_DAGGER && wield->value[0] != WEAPON_SWORD && wield->value[0] != WEAPON_AXE)) {
		wield = get_eq_char(ch, WEAR_SECONDARY);
		if (!wield || (wield->value[0] != WEAPON_DAGGER && wield->value[0] != WEAPON_SWORD && wield->value[0] != WEAPON_AXE)) {
			send_to_char("You must be wielding a bladed weapon.\r", ch);
			return;
		}
	}

	if (chance < 15) {
		act("{Y$n attempts to slit $N's throat but wakes $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{YYou trip over something, accidentally waking $N!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{YYou wake up startled as you catch $n about to slit your throat!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

		affect_strip(victim, gsn_sleep);
		do_function(victim, &do_wake, "");
		check_improve(ch,gsn_slit_throat,FALSE,5);
		p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_SLIT,"wakeup",0,0,0,0,0);
	} else if (chance < 25) {
		act("$N notices you and wakes up startled!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("You wake up in time to see $n attempting to slit your throat!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$N wakes up before $n can slit $M throat!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		damage(ch, victim, number_range(ch->max_hit / 10, ch->max_hit /2),
			TYPE_HIT, attack_table[(!get_eq_char(ch, WEAR_WIELD) ? get_eq_char(ch, WEAR_SECONDARY) : get_eq_char(ch, WEAR_WIELD))->value[3]].damage,
			TRUE);

		check_improve(ch,gsn_slit_throat,FALSE,5);

		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_SLIT,"wakeup",0,0,0,0,0))
			multi_hit(ch, victim, TYPE_UNDEFINED);
	} else { // perfect hit
		act("{R$n slits $N's throat in cold blood!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{RYou slit $N's throat in cold blood!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{RSomeone slit your throat!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

		victim->set_death_type = DEATHTYPE_ALIVE;
		victim->death_type = DEATHTYPE_SLIT;

		if (ch != victim) {
			group_gain(ch, victim, 25 + (skill / 2));
			if( ch->fighting == victim )
				stop_fighting(ch, FALSE);
		}

		// If invasion mob then check if quest point is earned
		if (!IS_NPC(ch) && IS_NPC(victim) && IS_SET(victim->act[1], ACT2_INVASION_MOB) && number_percent() < 5) {
			send_to_char("{WYou have been awarded a quest point!{x\n\r", ch);
			ch->questpoints++;
		}

		// Make sure to toggle the death trigger as we are using raw_kill, not damage
		{
			ROOM_INDEX_DATA *here = victim->in_room;
			victim->position = POS_STANDING;
			if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0))
				p_percent_trigger(NULL, NULL, here, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0);
		}

		raw_kill(victim, TRUE, TRUE, RAWKILL_NORMAL);
		check_improve(ch,gsn_slit_throat,TRUE,5);

		if (!IS_NPC(ch) && !IS_NPC(victim))
			player_kill(ch, victim);
		else if (!IS_NPC(ch) && IS_NPC(victim))
			ch->monster_kills++;

		// Check if slain victim was part of a quest. Checks if your horse got the kill, too.
		if (!IS_NPC(ch)) {
			check_quest_slay_mob(ch, victim, true);
			check_invasion_quest_slay_mob(ch, victim);
		}

		// Hahahaha
		if (RIDDEN(ch)) check_quest_slay_mob(RIDDEN(ch), victim, true);
	}
}


void do_blackjack(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
//	char buf[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	OBJ_DATA *helmet, *weapon;
	int chance, skill;
	long cid[2], vid[2];

	one_argument(argument, arg);

	if ((skill = get_skill(ch, gsn_blackjack)) < 1) {
		send_to_char("You have no knowledge of this skill.\n\r", ch);
		return;
	}

	if (is_dead(ch)) return;

	if (!arg[0]) {
		send_to_char("Blackjack whom?\n\r", ch);
		return;
	}

	if (ch->fighting) {
		send_to_char("You can't catch them unaware if they're fighting.\n\r",ch);
		return;
	}

	if (!IS_AFFECTED(ch, AFF_SNEAK)) {
		send_to_char("You aren't moving silently enough.\n\r", ch);
		return;
	}

	if ((victim = get_char_room(ch,NULL, arg)) == NULL) {
		send_to_char("They aren't here.\n\r", ch);
		WAIT_STATE(ch, skill_table[gsn_blackjack].beats);
		return;
	}

	if (IS_AFFECTED(victim, AFF_SLEEP)) {
		act("$E's already asleep.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (victim == ch) {
		send_to_char("You might hurt yourself!\n\r", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE)) return;

	if (!IS_NPC(ch) && (skill < 100) && IS_IMMORTAL(victim) && (victim->tot_level == MAX_LEVEL)) {
		act("It's IMPOSSIBLE!!! $N's power levels are FAR too high!\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_AWAKE(victim)) {
		act("$N is already asleep, that's like blackjacking fish in a barrel!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	char_id(ch, cid);
	char_id(victim, vid);

	// Make sure victim doesn't have a super strong helmet
	helmet = get_eq_char(victim, WEAR_HEAD);
	if (helmet && IS_SET(helmet->extra[1], ITEM_SUPER_STRONG)) {
		act("You hear a loud *DONG* as $n's weapon bounces off $N's helmet!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("Your weapon bounces off $N's strong helmet!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("Your helmet protects you from $n's blackjack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		multi_hit(victim, ch, TYPE_UNDEFINED);
		if( is_char_stillvalid(ch, cid) )
			WAIT_STATE(ch, skill_table[gsn_blackjack].beats);
		return;
	}

	if (IS_AFFECTED(victim, AFF_CALM)) {
		act("$N is too calm and relaxed not to notice you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("Shrouded in your relaxed state of mind, you catch $n attempt to blackjack you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("While in $S relaxed state, $N notices $n's attempted blackjack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		multi_hit(victim, ch, TYPE_UNDEFINED);
		if( is_char_stillvalid(ch, cid) )
			WAIT_STATE(ch, skill_table[gsn_blackjack].beats);
		return;
	}

	// Can fight it off if mounted and skilled!
	if (MOUNTED(victim) && get_skill(victim,gsn_riding) > 0) {
		CHAR_DATA *mount;
		int chance;
		int dam;

		chance = get_skill(victim, gsn_riding) / 3;
		chance += get_curr_stat(victim, STAT_DEX) / 4;
		mount = MOUNTED(victim);

		if (number_percent() < chance) {
			dam = UMIN(ch->hit/6, 3000);
			dam *= get_skill(victim, gsn_riding) / 100;
			act("{R$n notices $N sneaking around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n notices $N sneaking around and kicks $M with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{R$n notices you sneaking around and kicks you with $s hind legs!{x", mount, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			damage(mount, ch, dam, gsn_kick, DAM_BASH, TRUE);
			if( is_char_stillvalid(victim, vid) )
				check_improve(victim, gsn_riding, TRUE, 10);
			return;
		}
	}

	// decep
	if (number_percent() < get_skill(victim, gsn_deception)/3) {
		act("$N steps out of the way of your blackjack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("You step out of the way of $n's blackjack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$N steps out of the way of $n's blackjack!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		check_improve(victim, gsn_deception, TRUE, 1);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BLACKJACK,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BLACKJACK,"pretest",0,0,0,0,0))
		return;

	check_improve(ch,gsn_blackjack,TRUE,5);

	chance = get_skill(ch, gsn_blackjack);
	chance += (ch->tot_level - victim->tot_level);

	if (get_profession(ch, SECOND_SUBCLASS_THIEF) != CLASS_THIEF_HIGHWAYMAN)
		chance = chance/2;
	else
		chance = (chance*3)/2;

	weapon = get_eq_char(victim, WEAR_WIELD);
	if (weapon && (number_percent() > 25 || weapon->item_type != ITEM_WEAPON || !IS_WEAPON_STAT(weapon,WEAPON_ANNEALED)))
		weapon = NULL;

	if (number_percent() < chance) {
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BLACKJACK,"attack_pass",0,0,0,0,0)) {
			AFFECT_DATA af;
			memset(&af,0,sizeof(af));
			act("{YYou hit $N in the back of the head and knock $M out cold!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n hits $N in the back of the head, knocking $M out cold!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{YYou are knocked out by an unknown assailant!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

			stop_casting(victim, FALSE);
			interrupt_script(victim, TRUE);

			af.where = TO_AFFECTS;
			af.group     = AFFGROUP_PHYSICAL;
			af.type = gsn_sleep;
			af.level = ch->tot_level+(weapon?(weapon->level/5):0);
			af.duration = weapon ? number_range(2,4) : 1;
			af.location = APPLY_NONE;
			af.modifier = 0;
			af.bitvector = AFF_SLEEP;
			af.bitvector2 = 0;
			af.slot	= WEAR_NONE;

			REMOVE_BIT(victim->affected_by[0], AFF_HIDE);

			affect_to_char(victim, &af);
			victim->position = POS_SLEEPING;
		}
	} else {
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BLACKJACK,"attack_fail",0,0,0,0,0)) {
			act("{Y$n senses $N's attack and strikes first!{x", victim, ch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			act("{YYou sense $n sneaking up behind you and attack first!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$N senses your attack and is ready for you!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			victim->hit_damage = number_range(1, victim->max_hit/4);
			if (number_percent() < 20 && !p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BLACKJACK,"quick_hit",0,0,0,0,0)) {
				act("You give $N a quick whack across the head!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n whacks you across the head!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$n slips in a quick whack across $N's head!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				damage(ch, victim, victim->hit_damage, TYPE_UNDEFINED, DAM_NONE, FALSE);
			}
			victim->hit_damage = 0;
			multi_hit(victim, ch, TYPE_UNDEFINED);
		}
	}
	if( is_char_stillvalid(ch, cid) )
		WAIT_STATE(ch, skill_table[gsn_blackjack].beats);
}


void do_flee(CHAR_DATA *ch, char *argument)
{
	do_flee_full(ch, argument, FALSE, TRUE);
}

int do_flee_full(CHAR_DATA *ch, char *argument, bool conceal, bool pursue)
{
	ROOM_INDEX_DATA *was_in;
	ROOM_INDEX_DATA *now_in;
	CHAR_DATA *fleeing_from;
	char buf[MSL];
	int attempt;
	bool flee_lag = FALSE, flying;
	int door = -1;

//    argument = one_argument(argument, arg);

	if (argument && (fleeing_from = ch->fighting) == NULL)
	{
		if (ch->position == POS_FIGHTING)
			ch->position = POS_STANDING;

		send_to_char("You aren't fighting anyone.\n\r", ch);
		return -1;
	}

	if (ch->bashed > 0) {
		send_to_char("You must stand up first.\n\r", ch);
		return -1;
	}

	if (ch->fighting != NULL && !IS_NPC(ch->fighting))
		flee_lag = TRUE;

	if (p_percent_trigger(ch,NULL,NULL,NULL,ch,ch->fighting,NULL, NULL, NULL,argument?TRIG_PREFLEE:TRIG_PREWIMPY,argument,0,0,0,0,0) ||
		p_percent_trigger(ch->fighting,NULL,NULL,NULL,ch,NULL,NULL, NULL, NULL,argument?TRIG_PREFLEE:TRIG_PREWIMPY,argument,0,0,0,0,0))
		return MAX_DIR;

	if (IS_AFFECTED(ch,AFF_BLIND))
	{
		act("{R$n stumbles around blindly!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{RYou stumble around blindly!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		if (number_percent() < 70)
			return -1;
	}
	else
	{
		act("{R$n attempts to flee!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{RYou attempt to flee!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		if (number_percent() < 10)
		{
			send_to_char("{RPANIC! You couldn't escape!{x\n\r", ch);
			return -1;
		}
	}

	// Check that the char was in a room or vroom to start with, and get the room reference
	if ((was_in = ch->in_room) == NULL && ch->in_wilds == NULL)
	{
		plogf("fight.c, do_flee(): was_in == NULL (tried to flee from nowhere)");
		return -1;
	}

	flying = mobile_is_flying(ch);

	// parse_direction() handles direction argument strings, returns a door number,
	// or -1 if the direction string is not a valid direction.
	if (IS_NULLSTR(argument))
	{

		// ok, so let's try to find a valid direction to flee to...
		for (attempt = 0; attempt < MAX_FLEE_ATTEMPTS; attempt++)
		{
			EXIT_DATA *pexit;

			// pick a random door from the range 0 to 9 (MAX_DIR)... alledgedly.
			door = number_door();

			if (was_in->wilds == NULL)
			{
				plogf("fight.c, do_flee(): char is fleeing from a static room.");
				// Check if door is a valid exit for this char
				if (!(pexit = was_in->exit[door])
					|| (!pexit->u1.to_room && pexit->wilds.wilds_uid == 0)
					|| (IS_SET(pexit->exit_info, EX_CLOSED) && !IS_AFFECTED(ch, AFF_PASS_DOOR))
					|| (IS_SET(pexit->exit_info, EX_AERIAL) && !flying)
					|| (IS_NPC(ch) && (IS_SET(pexit->exit_info, EX_VLINK)))
					|| number_range(0,ch->daze) != 0
					|| (IS_NPC(ch) && (IS_SET(pexit->u1.to_room->room_flags, ROOM_NO_MOB) || IS_SET(pexit->u1.to_room->room_flags, ROOM_SAFE))))
					continue;
			}
			else
			{
				plogf("fight.c, do_flee(): char is fleeing from a wilds vroom.");
			}

			break;
		}

		if( attempt >= 10 )
			door = -1;
	}
	else if( (door = parse_direction(argument)) >= 0 ) {
		if (was_in->wilds == NULL)
		{
			EXIT_DATA *pexit;

			plogf("fight.c, do_flee(): char is fleeing from a static room.");
			// Check if door is a valid exit for this char
			if (!(pexit = was_in->exit[door])
				|| (!pexit->u1.to_room && pexit->wilds.wilds_uid == 0)
				|| (IS_SET(pexit->exit_info, EX_CLOSED) && !IS_AFFECTED(ch, AFF_PASS_DOOR))
				|| (IS_SET(pexit->exit_info, EX_AERIAL) && !flying)
				|| (IS_NPC(ch) && (IS_SET(pexit->exit_info, EX_VLINK)))
				|| number_range(0,ch->daze) != 0
				|| (IS_NPC(ch) && (IS_SET(pexit->u1.to_room->room_flags, ROOM_NO_MOB) || IS_SET(pexit->u1.to_room->room_flags, ROOM_SAFE)))) {
				door = -1;
			}
		}
		else
		{
			plogf("fight.c, do_flee(): char is fleeing from a wilds vroom.");
		}

		if( door >= 0 )
			move_char(ch, door, FALSE);
	}

	// Check if char was actually able to move
	if (door < 0 || (now_in = ch->in_room) == was_in)
	{
		send_to_char("{RPANIC! You couldn't escape!{x\n\r", ch);
		return -1;
	}

	/* if an NPC and was killed in the move_char procedure, e.g. by room flames,
	   the char data will have been freed and the in_room will be null, so bail out here. */
	if (IS_NPC(ch) && !ch->in_room)
		return door;

	char_from_room(ch);
	char_to_room(ch, was_in);
	if (!conceal) act("{R$n has fled!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	char_from_room(ch);
	char_to_room(ch, now_in);

	sprintf(buf, "{RYou flee to the %s!{x\n\r", dir_name[door]);
	send_to_char(buf, ch);

	p_percent_trigger(ch->fighting,NULL,NULL,NULL,ch,NULL,NULL, NULL, NULL,argument?TRIG_FLEE:TRIG_WIMPY,argument,0,0,0,0,0);
	p_percent_trigger(ch,NULL,NULL,NULL,ch,ch->fighting,NULL, NULL, NULL,argument?TRIG_FLEE:TRIG_WIMPY,argument,0,0,0,0,0);

	stop_fighting(ch, TRUE);



/* 05-29-2006 Syn - Disabling this for now. I originally added this because Nopraptor reported
   a bug of flee <direction> sending you elsewhere than the stated direction, causing him to get
   CPKed once. However, it causes a segfault when someone flees into a transfer script, and I
   might reenable it later if anyone reports any similar problems with flee to Nop's.

   -- NIB - This might've been Nopraptor giving a misspelled direction, which caused the code to pick a random direction.
		  - Also, this needs to check whether the exit in the starting room existed in the first place.
   -- NIB again - the segfault was likely due to the fact sprintf requires a buffer in the first argument......

	if (was_in->exit[door]->u1.to_room != now_in)
	{
	sprintf(buf, "do_flee: big mistake! %s tried to flee %s but it sent him somewhere else",
		HANDLE(ch), dir_name[door]);
	bug(buf, 0);
	}
*/

	NO_RECALL_STATE(ch, 50 + number_range(1,10));

	if (flee_lag)
		WAIT_STATE(ch, PULSE_VIOLENCE/5);

	if (pursue) {
		if (ch->fighting != NULL && get_skill(ch->fighting, gsn_pursuit) > 0)
			ch->pursuit_by = ch->fighting;

		// Stop "flee-killing".
		if (ch->fighting != NULL && IS_NPC(ch->fighting) && !IS_IMMORTAL(ch) &&
			(ch->tot_level > 50 || IS_REMORT(ch)) && ch->fighting->level > ch->tot_level - 5 &&
				number_percent() > 33 && can_see(ch->fighting, ch))
		{
			act("You sense something approaching from the $t.", ch, NULL, NULL, NULL, NULL, dir_name[rev_dir[door]], NULL, TO_CHAR);
			hunt_char(ch->fighting, ch);
		}

		// Pursuit skill
		if (ch->pursuit_by != NULL
		&&  IS_SET(ch->pursuit_by->act[0], PLR_PURSUIT)
		&&  get_curr_stat(ch, STAT_DEX) > 16)
		{
		if (number_percent() < get_skill(ch->pursuit_by, gsn_pursuit) - 5)
		{
			act("You pursue $N!", ch->pursuit_by, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$N follows you as you flee!", ch, ch->pursuit_by, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			if (ch->pursuit_by->cast > 0)
			stop_casting(ch->pursuit_by, TRUE);
			if (ch->pursuit_by->script_wait)
			script_end_failure(ch->pursuit_by, TRUE);
		interrupt_script(ch->pursuit_by, FALSE);
			move_char(ch->pursuit_by, door, FALSE);
			one_hit(ch->pursuit_by, ch, TYPE_HIT, FALSE);
			check_improve(ch, gsn_pursuit, TRUE, 1);
		}
		else
		{
			act("$N foils your pursuit!", ch->pursuit_by, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			check_improve(ch->pursuit_by, gsn_pursuit, FALSE, 1);
		}
		}

		ch->pursuit_by = NULL;
	}

	return door;
}


void do_rescue(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	CHAR_DATA *fch;

	one_argument(argument, arg);

	if (is_dead(ch))
	return;

	if (arg[0] == '\0')
	{
	send_to_char("Rescue whom?\n\r", ch);
	return;
	}

	if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}

	if (victim == ch)
	{
	send_to_char("What about fleeing instead?\n\r", ch);
	return;
	}

	// Syn - is_same_group added so you can rescue your own pets
	if (!IS_NPC(ch) && IS_NPC(victim) && !is_same_group(ch, victim))
	{
	act("$N doesn't need your help.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (ch->fighting == victim)
	{
	send_to_char("No, it would seem they are fighting you.\n\r", ch);
	return;
	}

	if ((fch = victim->fighting) == NULL)
	{
	send_to_char("That person is not fighting right now.\n\r", ch);
	return;
	}

	if (IS_NPC(fch) && !is_same_group(ch,victim))
	{
		send_to_char("Kill stealing is not permitted.\n\r",ch);
		return;
	}

	WAIT_STATE(ch, skill_table[gsn_rescue].beats);
	if (number_percent() > get_skill(ch,gsn_rescue))
	{
	send_to_char("You fail the rescue.\n\r", ch);
	check_improve(ch,gsn_rescue,FALSE,1);
	return;
	}

	if (fch->fighting == ch)
	{
	send_to_char("You can't rescue someone who isn't tanking.\n\r", ch);
	return;
	}

	act("You rescue $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
	act("$n rescues you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
	act("$n rescues $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	check_improve(ch,gsn_rescue,TRUE,1);

	stop_fighting(fch, FALSE);
	stop_fighting(ch, FALSE);
	stop_fighting(victim, FALSE);

	set_fighting(fch, ch);
	set_fighting(ch, fch);
}


void do_tail_kick(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	int chance, dam;
	char arg[MAX_INPUT_LENGTH];
	long cid[2], vid[2];

	argument = one_argument(argument, arg);

	if (is_dead(ch))
		return;

	if (!(chance = get_skill(ch, gsn_tail_kick))) {
		send_to_char("You better leave tail kicking to the Sith.\n\r", ch);
		return;
	}

	if (!IS_SET(ch->parts,PART_TAIL)) {
		send_to_char("You need a tail to tail kick.\n\r", ch);
		return;
	}

	if (MOUNTED(ch)) {
		send_to_char("You can't tail kick while riding!\n\r", ch);
		return;
	}

	if (!arg[0]) {
		if (!(victim = ch->fighting)) {
			send_to_char("Tail kick who?\n\r", ch);
			return;
		}
	} else if (arg[0] && !(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"pretest",0,0,0,0,0))
		return;

	WAIT_STATE(ch, skill_table[gsn_tail_kick].beats);
	char_id(ch, cid);
	char_id(victim, vid);

	if (chance > number_percent()) {
		victim->hit_damage = number_range(ch->tot_level/2, ch->tot_level)*3;
		p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"damage",0,0,0,0,0);
		dam = victim->hit_damage;
		victim->hit_damage = 0;
		damage(ch,victim, dam, gsn_tail_kick,DAM_BASH,TRUE);
		if(is_char_stillvalid(ch, cid)) {
			check_improve(ch,gsn_tail_kick,TRUE,1);

			if(is_char_stillvalid(victim, vid)) {
				p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"attack_pass",0,0,0,0,0);
				p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"attack_pass",0,0,0,0,0);
			}
		}
	} else {
		damage(ch, victim, 0, gsn_tail_kick,DAM_BASH,TRUE);
		if(is_char_stillvalid(ch, cid)) {
			check_improve(ch,gsn_tail_kick,FALSE,1);
			if(is_char_stillvalid(victim, vid)) {
				p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"attack_fail",0,0,0,0,0);
				p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TAILKICK,"attack_fail",0,0,0,0,0);
			}
		}
	}
}


void do_kick(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	int skill;
	long vid[2], cid[2];

	if (is_dead(ch)) return;

	if (!(skill = get_skill(ch, gsn_kick))) {
		send_to_char("You better leave the martial arts to fighters.\n\r", ch);
		return;
	}

	if (IS_NPC(ch) && !IS_SET(ch->off_flags,OFF_KICK) && !IS_SET(ch->act[0],ACT_MOUNT))
		return;

	if (MOUNTED(ch)) {
		send_to_char("You can't kick while riding!\n\r", ch);
		return;
	}

        if ((victim = get_char_room(ch, NULL, argument)) == NULL) {
                if ((victim = ch->fighting) == NULL) {
			send_to_char("Kick whom?\n\r", ch);
			return;
                }
	}

	if (victim == ch) {
		send_to_char("You aren't that limber.\n\r", ch);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"pretest",0,0,0,0,0))
		return;

	char_id(ch, cid);
	char_id(victim, vid);

	WAIT_STATE(ch, skill_table[gsn_kick].beats);

	if (skill > number_percent()) {
		int dam = 0;

		dam += 15 *number_range(ch->tot_level/2, ch->tot_level);

		if (get_skill(ch,gsn_martial_arts) > 0) {
			dam += (int) dam * (get_skill(ch,gsn_martial_arts) / 100);
			check_improve(ch,gsn_martial_arts,TRUE,6);
		}

		victim->hit_damage = dam;
		p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"damage",0,0,0,0,0);
		dam = victim->hit_damage;
		victim->hit_damage = 0;

		if(dam > 0) damage(ch,victim,dam, gsn_kick,DAM_BASH,TRUE);

		if(is_char_stillvalid(ch, cid)) {
			check_improve(ch,gsn_kick,TRUE,1);

			if(is_char_stillvalid(victim, vid)) {
				p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"attack_pass",0,0,0,0,0);
				p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"attack_pass",0,0,0,0,0);
			}
		}
	} else {
		damage(ch, victim, 0, gsn_kick,DAM_BASH,TRUE);
		check_improve(ch,gsn_kick,FALSE,1);
		p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"attack_fail",0,0,0,0,0);
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_KICK,"attack_fail",0,0,0,0,0);
	}
}


void do_disarm(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	char buf[MAX_INPUT_LENGTH];
	int chance,hth,ch_weapon,vict_weapon,ch_vict_weapon;

	hth = 0;

	if (is_dead(ch)) return;

	if (!(chance = get_skill(ch,gsn_disarm)) && !IS_NPC(ch)) {
		send_to_char("You don't know how to disarm opponents.\n\r", ch);
		return;
	}

	if (!get_eq_char(ch, WEAR_WIELD) && (!(hth = get_skill(ch,gsn_hand_to_hand)) || (IS_NPC(ch) && !IS_SET(ch->off_flags,OFF_DISARM)))) {
		send_to_char("You must wield a weapon to disarm.\n\r", ch);
		return;
	}

	if (!(victim = ch->fighting)) {
		send_to_char("You aren't fighting anyone.\n\r", ch);
		return;
	}

	if (!(obj = get_eq_char(victim, WEAR_WIELD))) {
		send_to_char("Your opponent is not wielding a weapon.\n\r", ch);
		return;
	}

	if (IS_DEAD(victim)) {
		sprintf(buf, "Your attack passes through %s's shadow.\n\r", pers(victim, ch));
		send_to_char(buf, ch);
		return;
	}

	/* find weapon skills */
	ch_weapon = get_weapon_skill(ch,get_weapon_sn(ch));
	vict_weapon = get_weapon_skill(victim,get_weapon_sn(victim));
	ch_vict_weapon = get_weapon_skill(ch,get_weapon_sn(victim));

	/* modifiers */

	/* skill */
	if (!get_eq_char(ch,WEAR_WIELD))
		chance = chance * hth/150;
	else
		chance = chance * ch_weapon/100;

	chance += (ch_vict_weapon/2 - vict_weapon) / 2;

	/* dex vs. strength */
	chance += get_curr_stat(ch,STAT_DEX);
	chance -= 2 * get_curr_stat(victim,STAT_STR);

	/* level */
	chance += (ch->tot_level - victim->tot_level) * 2;

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"pretest",0,0,0,0,0) ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"pretest",0,0,0,0,0) ||
		p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DISARM,"pretest",0,0,0,0,0))
		return;

	/* and now the attack */
	if (number_percent() < chance && number_percent() > (3 * get_skill(victim, gsn_deception) / 4)) {
		WAIT_STATE(ch, skill_table[gsn_disarm].beats);
		obj = disarm(ch, victim);
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"message_pass",0,0,0,0,0) &&
			!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"message_pass",0,0,0,0,0) && obj)
			p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"message_pass",0,0,0,0,0);
		check_improve(ch,gsn_disarm,TRUE,1);
	} else {
		WAIT_STATE(ch,skill_table[gsn_disarm].beats);
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DISARM,"message_fail",0,0,0,0,0) &&
			!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ATTACK_DISARM,"message_fail",0,0,0,0,0) &&
			!p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_DISARM,"message_fail",0,0,0,0,0)) {
			act("You fail to disarm $N.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n tries to disarm you, but fails.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("$n tries to disarm $N, but fails.",ch,victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
		check_improve(ch,gsn_disarm,FALSE,1);

		if (get_skill(victim, gsn_deception) > 0)
			check_improve(victim, gsn_deception, TRUE, 6);
	}
}


void do_slay(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	char arg[MAX_INPUT_LENGTH];
	char buf[MSL];
	int corpse_type;

	argument = one_argument(argument, arg);
	if (!arg[0]) {
		send_to_char("Slay whom?\n\r", ch);
		return;
	}

	if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (ch == victim) {
		send_to_char("Suicide is a mortal sin.\n\r", ch);
		return;
	}

	if (!IS_NPC(victim) && victim->tot_level >= get_trust(ch)) {
		send_to_char("You failed.\n\r", ch);
		return;
	}

	if (ch->mount == victim) {
		send_to_char("You can't kill your mount. You might hurt yourself!\n\r", ch);
		return;
	}

	if(argument[0]) {
		act("{RYou slay $M in cold blood!{x",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
		act("{R$n slays you in cold blood!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("{R$n slays $N in cold blood!{x",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		corpse_type = flag_lookup(argument,corpse_types);
		if(corpse_type == NO_FLAG) corpse_type = RAWKILL_NORMAL;
/*
	} else if (!str_cmp(ch->name, "Syn")) {
		act("{RYou focus your godly powers on $N and $E explodes in a blazing inferno!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{R$n focuses $s godly powers on you and you explode in a blazing inferno!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{R$n focuses $s godly powers on $N and $E explodes in a blazing inferno!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		corpse_type = RAWKILL_INCINERATE;
*/
	} else {
		act("{RYou slay $M in cold blood!{x",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
		act("{R$n slays you in cold blood!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("{R$n slays $N in cold blood!{x",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		corpse_type = RAWKILL_NORMAL;
	}
	if (!IS_NPC(ch) && !IS_NPC(victim))
		player_kill(ch, victim);
	else if (!IS_NPC(ch) && IS_NPC(victim))
		ch->monster_kills++;

	victim->death_type = DEATHTYPE_RAWKILL;
	victim->set_death_type = DEATHTYPE_ALIVE;

	sprintf(buf, "%s slayed %s!", ch->name, IS_NPC(victim) ? victim->short_descr : victim->name);
	raw_kill(victim, FALSE, TRUE, corpse_type);

	wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
	log_string(buf);
}


void do_challenge(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	char arg[MAX_INPUT_LENGTH];
	char buf[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg);

	if (IS_DEAD(ch))
	{
	act("You can't challenge. You are DEAD.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (IS_NPC(ch))
	{
		send_to_char("NPCs can't challenge.\n\r", ch);
	return;
	}

	if (ch->tot_level < 10)
	{
	send_to_char("You must be level 10 to challenge in the arena.\n\r", ch);
	return;
	}

	if (arg[0] == '\0')
	{
	if (IS_SET(ch->act[0], PLR_NO_CHALLENGE))
	{
		REMOVE_BIT(ch->act[0], PLR_NO_CHALLENGE);
		send_to_char("You are now accepting challenges.\n\r", ch);
		return;
	}
	else
	{
		SET_BIT(ch->act[0], PLR_NO_CHALLENGE);
		send_to_char("You are no longer accepting challenges.\n\r", ch);
		return;
	}
	return;
	}

	AREA_REGION *region = get_room_region(ch->in_room);

	if (!IS_VALID(region) ||
		region->place_flags == PLACE_NOWHERE ||
		region->place_flags == PLACE_OTHER_PLANE ||
		!str_prefix("Maze-Level", ch->in_room->area->name))
	{
	send_to_char("You can only challenge from the mortal world.\n\r", ch);
	return;
	}

	if (IS_SET(ch->act[0], PLR_NO_CHALLENGE))
	{
	send_to_char("You must enable challenges to challenge a player.\n\r", ch);
	return;
	}

	if (ch->pcdata->challenge_delay > 0)
	{
	sprintf(buf, "You may challenge again in %d minutes.\n\r",
		ch->pcdata->challenge_delay/2+1);
	send_to_char(buf, ch);
	return;
	}

	victim = get_char_world(ch, arg);

	if (victim == ch)
	{
	send_to_char("You don't find yourself much of a challenge.\n\r", ch);
	return;
	}

	if (victim == NULL)
	{
	send_to_char("No player exists by that name.\n\r", ch);
	return;
	}

	region = get_room_region(victim->in_room);

	if (!IS_VALID(region) ||
		region->place_flags == PLACE_NOWHERE ||
		region->place_flags == PLACE_OTHER_PLANE)
	{
	act("$N is in a magically-protected area.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (!str_cmp(ch->in_room->area->name, "Arena"))
	{
	act("You're already in the arena, just find $M!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	/*
	if (!can_escape(victim))
	return;
	*/

	if (IS_NPC(victim))
	{
	send_to_char("You can only challenge players in the arena.\n\r", ch);
	return;
	}

	if (IS_DEAD(victim))
	{
	send_to_char("You can't challenge DEAD players.\n\r", ch);
	return;
	}

	if (IS_SET(victim->act[0], PLR_NO_CHALLENGE))
	{
	send_to_char("That person is not accepting challenges.\n\r", ch);
	return;
	}

	sprintf(buf,
	"{M%s has challenged %s to a fight to the death in the arena!",
	ch->name, victim->name);
	crier_announce(buf);

	sprintf(buf,
		"{M%s has challenged you to a fight to the death in the arena!\n\rDo you accept? (Yes/No)\n\r{x", ch->name);
	send_to_char(buf, victim);
	sprintf(buf, "\n\r{MYou have challenged %s to a fight to the death in the arena!\n\r{x", victim->name);
	send_to_char(buf, ch);

	victim->challenged = ch;
	ch->pcdata->challenge_delay = 8;
	return;
}


void do_feign(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_INPUT_LENGTH];
	int chance;
	int door;

	if (is_dead(ch))
	return;

	if (MOUNTED(ch))
	{
	send_to_char("You can't feign while riding!\n\r", ch);
	return;
	}

	if (ch->position == POS_FEIGN)
	{
	ch->position = POS_STANDING;
	act("You stop acting dead and stand up.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n stops acting like a corpse and stands up.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if ((chance = get_skill(ch,gsn_feign)) == 0)
	{
	send_to_char("You try to look like a corpse but fail.\n\r",ch);
	act("$n pretends to be dead but fails miserably.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	return;
	}

	if (number_percent() >= chance)
	{
	send_to_char("You try to look like a corpse but fail.\n\r",ch);
	act("$n pretends to be dead but fails miserably.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	check_improve(ch,gsn_feign,FALSE,6);
	return;
	}

	if (ch->fighting)
	{
	send_to_char("You can't pretend to be dead while fighting!\n\r",ch);
	return;
	}

	ch->position = POS_FEIGN;

	act("You lie down and give your best impression of a corpse.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n lies down on the ground and pretends to be dead.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	sprintf(buf, "{RYou hear someone's death cry.{x");

	for (door = 0; door < 10; door++)
	{
	EXIT_DATA *pexit;

	if ((pexit = ch->in_room->exit[door]) != NULL
	&& pexit->u1.to_room != NULL
	&& pexit->u1.to_room != ch->in_room
	&& pexit->u1.to_room->people != NULL)
	{
		act(buf, pexit->u1.to_room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act(buf, pexit->u1.to_room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	}

	check_improve(ch,gsn_feign,TRUE,6);

	WAIT_STATE(ch,skill_table[gsn_feign].beats);
}


void do_resurrect(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	char arg[MAX_INPUT_LENGTH];
	OBJ_DATA *obj;
	int chance;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Resurrect whom?\n\r", ch);
		return;
	}

	if (is_dead(ch))
		return;

	if (get_skill(ch, gsn_resurrect) == 0)
	{
		send_to_char("Huh?\n\r", ch);
		return;
	}

	if (ch->fighting != NULL)
	{
		send_to_char("You can't, you are fighting!\n\r", ch);
		return;
	}

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
	{
		if (can_see_obj(ch, obj))
		{
			if (is_name(arg, obj->name))
			break;
		}
	}


	if (obj == NULL)
	{
		send_to_char("That doesn't appear to be in the room.\n\r", ch);
		return;
	}

	if (obj->item_type != ITEM_CORPSE_PC)
	{
		send_to_char("You can only resurrect a fresh PC corpse.\n\r", ch);
		return;
	}

	if (obj->owner == NULL)
	{
		send_to_char("You cannot identify that corpse's owner.\n\r", ch);
		return;
	}

	if (!str_cmp(obj->owner, ch->name)) {
		send_to_char("Doing that would create a paradox even I can't comprehend.\n\r", ch);
		return;
	}

	// 20070520 : NIB : Added use of corpse animation percent
	chance = obj->condition * CORPSE_RESURRECT(obj);
	if(number_range(1,10000) > chance) {
		act("$p seems to be immune to your divine energies.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_SET(CORPSE_PARTS(obj), PART_HEAD))
	{
		act("$p is missing its head.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	victim = get_char_world(NULL, obj->owner);
	if (victim == NULL)
	{
		act("The soul of $t is no longer within this world.", ch, NULL, NULL, NULL, NULL, obj->owner, NULL, TO_CHAR);
		return;
	}

	if (IS_SET(obj->extra[1], ITEM_NO_RESURRECT))
	{
		act("$p seems to be immune to your divine energies.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// Only allow resurrection of CPK corpses in CPK rooms
	if( IS_SET(ch->in_room->room_flags, ROOM_CHAOTIC) && !IS_SET(CORPSE_FLAGS(obj), CORPSE_CHAOTICDEATH) )
	{
		// Any player, or non-holyaura immortal, attempting to do so will be ZOTTED.
		if( !IS_NPC(ch) && (!IS_IMMORTAL(ch) || !IS_SET(ch->act[1], PLR_HOLYAURA)))
		{
			send_to_char("{YAttempting to resurrect a non-chaotic corpse in a chaotic room is {RFORBIDDEN{Y!{x\n\r", ch);
			ch->hit = 1;
			ch->mana = 1;
			ch->move = 1;
			return;
		}
	}

	// Only allow resurrection of PK corpses in PK rooms...
	if( is_room_pk(ch->in_room, TRUE) && !IS_SET(CORPSE_FLAGS(obj), CORPSE_PKDEATH) )
	{
		// No penalty here, just failure.
		act("$p seems to be immune to your divine energies.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (!IS_NPC(victim) && obj != victim->pcdata->corpse) {
		act("You may only resurrect $N's most current corpse.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// Check the victim if it can be resurrected directly
	if (p_percent_trigger( victim, NULL, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL,0,0,0,0,0) )
		return;

	// Check the corpse for anything blocking the resurrection
	if (p_percent_trigger( NULL, obj, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL,0,0,0,0,0) )
		return;

	// Check the ROOM the corpse is in for anything blocking resurrection
	if (p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, NULL, TRIG_PRERESURRECT, NULL,0,0,0,0,0) )
		return;

	/*
	if (victim->church != ch->church || ch->church == NULL)
	{
	act("Nothing happens as that being is not a member of your church.", ch,NULL, NULL, TO_CHAR);
	return;
	}
	*/

	RESURRECT_STATE(ch, 5 + victim->tot_level/10);
	ch->resurrect_target = obj;

	act("{YYou kneel down and place your hands over $p.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n kneels down and places $s hands over $p.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	return;
}


void resurrect_end(CHAR_DATA *ch)
{
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	OBJ_DATA *in;
	OBJ_DATA *in_next;
	bool success = TRUE;

	obj = ch->resurrect_target;

	if (obj == NULL)
	{
		sprintf(buf, "resurrect_end: ch->resurrect_target was null! ch %s", ch->name);
		bug(buf, 0);
		return;
	}

	for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
	{
		if (can_see_obj(ch, obj))
		{
			if (ch->resurrect_target == obj)
				break;
		}
	}

	ch->resurrect_target = NULL;

	if (obj == NULL)
	{
	 	send_to_char("That doesn't appear to be in the room.\n\r", ch);
		return;
	}

	if (obj->item_type != ITEM_CORPSE_NPC && obj->item_type != ITEM_CORPSE_PC)
	{
		send_to_char("This must be done on a fresh corpse.\n\r", ch);
		return;
	}

	if (!IS_SET(CORPSE_PARTS(obj), PART_HEAD))
	{
		act("$p is missing its head.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	victim = get_char_world(ch, obj->owner);

	if (victim == NULL)
	{
		act("The soul of $t is no longer within this world.", ch, NULL, NULL, NULL, NULL, obj->owner, NULL, TO_CHAR);
		return;
	}

	if (!IS_DEAD(victim))
	{
		act("The soul of $t has already been resurrected.", ch, NULL, NULL, NULL, NULL, obj->owner, NULL, TO_CHAR);
		return;
	}

	resurrect_pc(victim);
	char_from_room(victim);
	char_to_room(victim, obj->in_room);

	act("{WThe area around you brightens as a faint warmth flows in.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	act("{+$p shimmers with iridescence.", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	act("You feel mortal once more as you have been resurrected!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("You breathe quickly to full your lungs again as you open your eyes.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	act("The eyes of $n flick open.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{+$n sharply inhales a full breath.", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	act("{+$n has been resurrected!", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (in = obj->contains; in != NULL; in = in_next)
	{
		in_next = in->next_content;

		obj_from_obj(in);

		if (IS_MONEY(in))
		{
			victim->silver += MONEY(obj)->silver;
			victim->gold += MONEY(obj)->gold;
			extract_obj(in);
			continue;
		}		

		obj_to_char(in, victim);
	}

	obj_from_room(obj);
	extract_obj(obj);

	victim->hit = victim->max_hit/2;
	victim->mana = victim->max_mana/2;
	victim->move = victim->max_move/2;

	if (ch->church != victim->church || !ch->church)
	{
		int skill = get_skill(ch, gsn_resurrect);

		if( skill < 100 )
		{
			int h, m, v;
			bool deplete = FALSE;
			h = skill * ch->max_hit / 100;	if( h < 1 ) h = 1;
			m = skill * ch->max_mana / 100;	if( m < 0 ) m = 0;
			v = skill * ch->max_move / 100; if( v < 0 ) v = 0;

			if( h < ch->hit )
			{
				ch->hit = h;
				deplete = TRUE;
			}

			if( m < ch->mana )
			{
				ch->mana = m;
				deplete = TRUE;
			}

			if( v < ch->move)
			{
				ch->move = v;
				deplete = TRUE;
			}

			if( deplete )
				send_to_char("The rite depletes your life energies.\n\r", ch);
		}

		success = FALSE;
	}

	// Only get improvement when doing so on players
	// -- To prevent finding a weak ass mob and spamming this
	if( !IS_NPC(victim) )
		check_improve( ch, gsn_resurrect, success, 10 );
}


void do_holdup(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	int chance;

	one_argument(argument, arg);

	if (arg[0] == '\0')
	{
	send_to_char("Holdup whom?\n\r", ch);
	return;
	}

	if (ch->fighting != NULL)
	{
	send_to_char("You can't hold them up while fighting!\n\r",ch);
	return;
	}

	if (get_skill(ch, gsn_holdup) == 0)
	{
	send_to_char("You do not know this skill.\n\r", ch);
	return;
	}

	if (!(IS_AFFECTED(ch, AFF_SNEAK)) &&
		!(IS_AFFECTED(ch, AFF_HIDE)))
	{
	send_to_char("You must be hidden or sneaking.\n\r", ch);
	return;
	}

	if ((victim = get_char_room(ch,NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}

	if (ch->heldup != NULL)
	{
	if (ch->heldup == victim)
	{
		send_to_char("You already have them held up!\n\r", ch);
		return;
	}
	else
	{
		send_to_char("You already have someone held up!\n\r", ch);
		return;
	}
	}

	if (victim->position == POS_HELDUP)
	{
	act("$E's already heldup.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (victim == ch)
	{
	send_to_char("You might traumatise yourself! \n\r ", ch);
	return;
	}

	if (is_safe(ch, victim, TRUE))
	return;

	check_improve(ch,gsn_holdup,TRUE,5);

	chance = get_skill(ch, gsn_holdup);
	chance += (ch->tot_level - victim->tot_level);
	chance = chance/2;

	if (number_percent() < chance)
	{
	act("{YYou catch $N by surprise!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n jumps out of nowhere catching you by surprise, looks like it's a holdup!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{Y$n jumps out of nowhere catching $N by surprise! Looks like it's a holdup!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	victim->position = POS_HELDUP;
	ch->heldup = victim;
	WAIT_STATE(ch, skill_table[gsn_holdup].beats);
	check_improve(ch, gsn_holdup, TRUE, 1);
	}
	else
	{
	act("{YYou clumsily stumble on in catching $N's attention!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{YYou notice $n attempt a holdup, but you spot it first!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{Y$n fumbles $s holdup of $N!{X", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	multi_hit(victim, ch, TYPE_UNDEFINED);
	WAIT_STATE(ch, skill_table[gsn_holdup].beats);
	check_improve(ch, gsn_holdup, FALSE, 1);
	}
}


void do_judge(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	char buf[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	int chance;

	one_argument(argument,arg);

	if (is_dead(ch))
		return;

	if ((chance = get_skill(ch,gsn_judge)) == 0)
	{
	send_to_char("Judge? What's that?\n\r",ch);
	return;
	}

	if (arg[0] == '\0')
	{
	send_to_char("Judge whom?\n\r", ch);
	return;
	}
	else if ((victim = get_char_room(ch, NULL, arg)) != NULL)
	{
		int al = 0;
	al = victim->alignment;
	if (number_percent() < 50)
	{
		al += (int) 10 - (10 * (get_skill(ch, gsn_judge)/100));
	}
	else
	{
		al -= (int) 10 - (10 * (get_skill(ch, gsn_judge)/100));
	}

	sprintf(buf, "Your judge of character determines that %s is ",
		pers(victim, ch));
	if (al == -1000)
		strcat(buf, "evil incarnate!");
	else if (al < -800)
		strcat(buf, "extremely chaotic.");
	else if (al < -700)
		strcat(buf, "very chaotic.");
	else if (al < -600)
		strcat(buf, "chaotic.");
	else if (al < -500)
		strcat(buf, "extremely malicious.");
	else if (al < -400)
		strcat(buf, "very malicious.");
	else if (al < -300)
		strcat(buf, "malicious.");
	else if (al < -200)
		strcat(buf, "slightly malicious.");
	else if (al < -100)
		strcat(buf, "leans toward malice.");
	else if (al < -50)
		strcat(buf, "slightly immoral.");
	else if (al <= -1)
		strcat(buf, "relatively neutral.");
	else if (al == 0)
		strcat(buf, "completely neutral.");
	else if (al < 50)
		strcat(buf, "relatively neutral.");
	else if (al < 100)
		strcat(buf, "moral.");
	else if (al < 200)
		strcat(buf, "leans toward harmony.");
	else if (al < 300)
		strcat(buf, "slightly harmonic.");
	else if (al < 400)
		strcat(buf, "harmonic.");
	else if (al < 500)
		strcat(buf, "very harmonic.");
	else if (al < 600)
		strcat(buf, "pious.");
	else if (al < 700)
		strcat(buf, "extremely pious.");
	else if (al < 800)
		strcat(buf, "saintly.");
	else if (al < 900)
		strcat(buf, "extremely saintly.");
	else if (al < 1000)
		strcat(buf, "the champion of harmony.");
	else if (al == 1000)
		strcat(buf, "pious incarnate!");
	strcat(buf, "\n\r");
	send_to_char(buf, ch);

	check_improve(ch, gsn_judge, TRUE, 1);
	}
	else
	{
		send_to_char("They aren't here.\n\r", ch);
	}
}


void do_bind(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	char arg[MAX_INPUT_LENGTH];
	int chance;

	argument = one_argument(argument,arg);

	if (is_dead(ch))
	return;

	if ((chance = get_skill(ch, gsn_bind)) == 0)
	{
	send_to_char("Bind? What's that?\n\r",ch);
	return;
	}

	if (arg[0] == '\0')
	{
	send_to_char("Bind whom?\n\r", ch);
	return;
	}

	if ((victim = get_char_room(ch,NULL, arg)) == NULL)
	{
	send_to_char("They aren't in the room.\n\r", ch);
	return;
	}

	if (victim == ch && victim->hit < victim->max_hit/15)
	{
	act("You're too badly hurt to bind your own wounds.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (victim->hit > (victim->max_hit * 2)/3)
	{
	act("They aren't that badly hurt.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
	}

	if (victim != ch)
	{
	act("{Y$n begins to bind $N's wounds...{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YYou begins to bind $N's wounds...{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
	act("{YYou begin to bind your own wounds...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n begins to bind $s own wounds...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	ch->bind_victim = victim;

	if (ch->fighting != NULL)
	BIND_STATE(ch, 36);
	else
	BIND_STATE(ch, 24);
}


void bind_end(CHAR_DATA *ch)
{
	CHAR_DATA *victim;

	victim = ch->bind_victim;

	if (victim == NULL || victim->in_room == NULL || victim->in_room != ch->in_room)
	{
	send_to_char("Your patient has left the room.\n\r", ch);
	return;
	}

	if (number_percent() > get_skill(ch, gsn_bind))
	{
	if (ch != victim)
	{
  		act("$n fails to bind $N's wounds.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("You failed to bind $N's wounds.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
		act("$n fails to bind $s own wounds.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("You fail to bind your own wounds.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	check_improve(ch, gsn_bind, FALSE, 1);
	ch->move = (ch->move * 3)/4;
	return;
	}

	if (victim != ch)
	{
		act("$n has bound $N's major wounds.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("You successfully bind $N's major wounds.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
		act("$n has bound $s own major wounds.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("You successfully bind your own major wounds.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	victim->hit = (victim->max_hit * 2)/3;
	check_improve(ch, gsn_bind, TRUE, 1);
	ch->move = (ch->move * 3)/4;
}


void do_pursuit(CHAR_DATA *ch, char *argument)
{
	if (get_skill(ch, gsn_pursuit) == 0)
	{
		send_to_char("You can't do that.\n\r", ch);
	return;
	}

	if (IS_SET(ch->act[0],PLR_PURSUIT))
	{
		send_to_char("You will no longer pursue characters when they flee.\n\r",ch);
		REMOVE_BIT(ch->act[0],PLR_PURSUIT);
	}
	else
	{
		send_to_char("You will now pursue characters when they flee.\n\r",ch);
		SET_BIT(ch->act[0],PLR_PURSUIT);
	}
}


void do_weave(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	CHAR_DATA *fch;

	one_argument(argument, arg);

	if (is_dead(ch))
	return;

	if (arg[0] == '\0')
	{
	send_to_char("Weave to attack whom?\n\r", ch);
	return;
	}

	if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}

	if (victim == ch)
	{
	send_to_char("That is just silly.\n\r", ch);
	return;
	}

	if (ch->fighting == victim)
	{
	send_to_char("You dance around on the spot.\n\r", ch);
	return;
	}

	if ((fch = victim->fighting) == NULL)
	{
	send_to_char("That person is not fighting right now.\n\r", ch);
	return;
	}

	if (!is_same_group(ch, victim->fighting))
	{
		act("Your formation is not currently fighting $N.\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (get_skill(ch, gsn_weaving) == 0)
	{
	send_to_char("You know nothing of this skill.\n\r", ch);
	return;
	}

	WAIT_STATE(ch, skill_table[gsn_weaving].beats);
	if (number_percent() > get_skill(ch,gsn_weaving))
	{
	send_to_char("You fail to weave to your victim.\n\r", ch);
	check_improve(ch,gsn_weaving,FALSE,1);
	return;
	}

	if (number_percent() < 50)
	{
		act("You backflip into combat with $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
		act("$n backflips and starts attacking you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("$n backflips out of combat with you!", ch, ch->fighting, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("$n backflips out of combat and begins to attack $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	}
	else
	{
		act("You weave into combat with $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR   );
		act("$n starts attacking you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("$n stops attacking you!", ch, ch->fighting, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
		act("$n weaves out of combat and begins to attack $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	}

	check_improve(ch,gsn_weaving,TRUE,1);

	stop_fighting(ch, FALSE);
	set_fighting(ch, victim);
}


void do_rack(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	int dam;
	char arg[MSL];
	//char buf[MSL];

	argument = one_argument(argument, arg);

	if (is_dead(ch))
	return;

	if (get_skill(ch, gsn_spirit_rack) == 0)
	{
		send_to_char(
			"You can't harness the energies for a spirit rack.\n\r", ch);
		return;
	}

	if (MOUNTED(ch))
	{
		send_to_char("You can't spirit rack while riding!\n\r", ch);
		return;
	}

	if (arg[0] == '\0')
	{
		if ((victim = ch->fighting) == NULL)
		{
			send_to_char("You aren't fighting anyone.\n\r", ch);
			return;
		}
	}
	else if ((victim = get_char_room(ch, NULL, arg)) == NULL)
	{
	send_to_char("They aren't here.\n\r", ch);
	return;
	}
	if (!is_safe(ch,victim,TRUE))
	{
	WAIT_STATE(ch, skill_table[gsn_spirit_rack].beats);

	if (number_percent() < get_skill(ch, gsn_spirit_rack))
	{
	check_improve(ch,gsn_spirit_rack,TRUE,1);
	act("{BYou rack $N with spiritual energies!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{B$n racks you with spiritual energies!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{B$n racks $N with spiritual energies!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	dam = dice(20+number_range(3,4),8+5*get_skill(ch,gsn_spirit_rack)/100);
	//sprintf(buf, "%d\n\r", dam);gecho(buf);
	damage(ch, victim, dam, gsn_spirit_rack, DAM_ENERGY, TRUE);

	cold_effect(victim,ch->tot_level/2,dam,TARGET_CHAR);
	}
	else
	{
	act("{B$N ducks out of the way of your spirit rack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{BYou duck out of the way of $n's spirit rack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("{B$N ducks out of the way of $n's spirit rack!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

	check_improve(ch, gsn_spirit_rack, FALSE, 1);
	}
	}
	return;
}


void do_warcry(CHAR_DATA *ch, char *argument)
{
	AFFECT_DATA af;
	CHAR_DATA *victim;
	int chance;

	if (is_dead(ch))
		return;

	chance = get_skill(ch, gsn_warcry);

	if (!IS_NPC(ch) && !chance) {
		send_to_char("You give a weak battle cry.\n\r", ch);
		return;
	}

	if (IS_SET(ch->affected_by[1], AFF2_SILENCE)) {
		send_to_char("You open your mouth wide, but nothing comes out.\n\r", ch);
		act("$n opens $s mouth wide, but nothing comes out.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
	}

	if(p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_WARCRY,"pretest",0,0,0,0,0))
		return;

	if(!p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_WARCRY,"message",0,0,0,0,0)) {
		send_to_char("{RYou let out a powerful war cry!{x\n\r", ch);
		act("{R$n lets out a powerful war cry!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		if (!IS_AFFECTED2(ch, AFF2_WARCRY))
			act("{RYour adrenaline pumps and your heart rate surges!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
			act("{RYour heart speeds up a little, but nothing else happens.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	WAIT_STATE(ch, skill_table[gsn_warcry].beats);

	memset(&af,0,sizeof(af));
	af.where     = TO_AFFECTS;
	af.group     = AFFGROUP_PHYSICAL;
	af.type      = gsn_warcry;
	af.level     = ch->tot_level;
	af.duration  = 3;
	af.location  = APPLY_STR;
	af.modifier  = 4;
	af.bitvector = 0;
	af.bitvector2 = AFF2_WARCRY;
	af.slot	= WEAR_NONE;

	for (victim = ch->in_room->people; victim != NULL; victim = victim->next_in_room) {
		if ((victim->leader == ch || victim == ch) && !IS_AFFECTED2(victim, AFF2_WARCRY) && number_percent() < chance) {
			if (victim != ch && !p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_SKILL_WARCRY,"group",0,0,0,0,0)) {
				act("{RYour adrenaline rushes and your heart pounds as you hear $N's powerful warcry!{x", victim, ch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$N enters into a battle rage!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
			affect_to_char(victim, &af);
		}
	}

	p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_SKILL_WARCRY,"post",0,0,0,0,0);

	check_improve(ch, gsn_warcry, TRUE, 1);
}


// Count up tallies and make announcements on a player kill.
void player_kill(CHAR_DATA *ch, CHAR_DATA *victim)
{
	char buf[MAX_STRING_LENGTH];

	if (!pre_reckoning && reckoning_timer > 0 && !IS_NPC(victim) && !IS_SET(victim->in_room->room_flags, ROOM_ARENA)) {
		sprintf(buf, "{MThe reckoning has claimed %s, who was slain by the mighty %s!{x\n\r", victim->name, ch->name);
		gecho(buf);
	}

	/*
	// If someones just killed someone 75 levels below, and they are on a quest
	// then the killer is a pirate
	if (!IS_PK(victim) &&
	victim->tot_level + 75 < ch->tot_level &&
	(ch->in_room->area->place_flags == PLACE_FIRST_CONTINENT ||
	ch->in_room->area->place_flags == PLACE_SECOND_CONTINENT) &&
	!IS_SET(victim->in_room->room_flags, ROOM_ARENA)) {
	int place = 0;
	sprintf(buf, "Let it be known %s has cowardly slain %s, %s is now known as a pirate!", ch->name, victim->name, ch->name);
	crier_announce(buf);
	if (ch->in_room->area->place_flags == PLACE_FIRST_CONTINENT) {
	place = CONT_SERALIA;
	}
	else {
	place = CONT_ATHEMIA;
	}
	set_pirate_status(ch, place, UMAX(ch->tot_level - victim->tot_level, 0) * 1000);
	}

	*/

	// Arena win. Only announces and tallies in Plith arena to make "arena" flag more versatile.
	if (IS_SET(ch->in_room->room_flags, ROOM_ARENA) && !str_cmp(victim->in_room->area->name, "Arena") && ch != victim) {
		ch->arena_kills++;
		victim->arena_deaths++;

		sprintf(buf, "%s has been slain by %s in the arena!", victim->name, ch->name);
		crier_announce(buf);

	// War win.
	} else if (ch->in_war && victim->in_war) {
		int quest_points;

		ch->arena_kills++;
		victim->arena_deaths++;

		if (victim != ch) {
			sprintf(buf, "{M%s has been slain by %s on the battlefield!{x", victim->name, ch->name);
			crier_announce(buf);
		}

		quest_points = 20;
		sprintf(buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points);
		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		ch->questpoints += quest_points;
	} else if (ch != victim) { // Regular PK win.
		// CPK
		if (IS_SET(ch->in_room->room_flags, ROOM_CHAOTIC)) {
			ch->cpk_kills++;
			victim->cpk_deaths++;
			if (IN_CHURCH(ch)) {
				ch->church->cpk_wins++;
				ch->church_member->cpk_wins++;
			}

			if (IN_CHURCH(victim)) {
				victim->church->cpk_losses++;
				victim->church_member->cpk_losses++;
			}
		// PK
		} else {
			ch->player_kills++;
			victim->player_deaths++;

			if (IN_CHURCH(ch)) {
				ch->church->pk_wins++;
				ch->church_member->pk_wins++;
			}

			if (IN_CHURCH(victim)) {
				victim->church->pk_losses++;
				victim->church_member->pk_losses++;
			}
		}

		// Message the church
		if (ch->church)	{
			sprintf(buf, "{Y[%s killed %s at %s!]{x\n\r", ch->name, victim->name, ch->in_room->name);
			msg_church_members(ch->church, buf);
		}
	}
}

