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
#include "tables.h"


void do_smite(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	OBJ_DATA *wield;
	char arg[MAX_STRING_LENGTH];
	int chance;

	argument = one_argument(argument, arg);

	if ((chance = get_skill(ch, gsn_smite)) == 0) {
		send_to_char("You can't do that.\n\r", ch);
		return;
	}

	if (arg[0] == '\0' && (victim = ch->fighting) == NULL) {
		send_to_char("Smite whom?\n\r", ch);
		return;
	}

	if (arg[0] != '\0' && (victim = get_char_room(ch, NULL, arg)) == NULL) {
		send_to_char("You don't see anybody like that around.\n\r", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE))
		return;

	wield = get_eq_char(ch, WEAR_WIELD);
	if (wield == NULL) {
		if ((wield = get_eq_char(ch, WEAR_SECONDARY)) == NULL) {
			act("You aren't even wielding a weapon.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}

	if (IS_GOOD(ch) && !IS_SET(wield->extra_flags, ITEM_BLESS) && !IS_SET(wield->extra_flags, ITEM_HOLY)) {
		act("$p is not a holy weapon.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (IS_EVIL(ch) && !IS_SET(wield->extra_flags, ITEM_EVIL)) {
		act("$p is not a cursed weapon.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (ch->alignment == 0 && !IS_SET(wield->extra_flags, ITEM_EVIL) && !IS_SET(wield->extra_flags, ITEM_BLESS)) {
		act("$p is neither a cursed nor a holy weapon.", ch, NULL, NULL, wield, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	/* AO 092516 - commenting out, I think this will make the skill more useful given its other restrictions
	if ((IS_EVIL(ch) && victim->alignment > 300) || (IS_GOOD(ch) && victim->alignment < -300) || (ch->alignment == 0 && victim->alignment != 0)) {
		act("$N is not a creature you can smite.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}
	*/

	if (victim->hit > victim->max_hit/4) {
		act("$N is still too strong for you to smite.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_SMITE,"pretest") ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_SMITE,"pretest") ||
		p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_SMITE,"pretest"))
		return;

	if (IS_AFFECTED(ch, AFF_FRENZY))
		chance += 10;

	if (IS_AFFECTED(victim, AFF_FRENZY))
		chance -= 15;

	WAIT_STATE(ch, skill_table[gsn_smite].beats);

	if (number_percent() < chance) {
		act("{GYou smite $N with a powerful $t!", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_CHAR);
		act("{Y$n smites $N with a powerful $t!{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_NOTVICT);
		act("{R$n smites you with a powerful $t!{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_VICT);

		victim->set_death_type = DEATHTYPE_SMITE;
		damage(ch, victim, 30000, gsn_smite, DAM_NONE, TRUE);
		check_improve(ch, gsn_smite, FALSE, 1);
	} else {
		damage(ch, victim, dice(wield->value[1]*2,wield->value[2]*2), gsn_smite, DAM_HOLY, TRUE);
		check_improve(ch, gsn_smite, FALSE, 1);
	}
}


void do_stake(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    OBJ_DATA *stake;
    int chance;
    char arg[MAX_STRING_LENGTH];

    if ((chance = get_skill(ch, gsn_stake)) == 0)
    {
       send_to_char("You know nothing of this skill.\n\r", ch);
       return;
    }

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Stake whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    for (stake = ch->carrying; stake != NULL; stake = stake->next_content)
    {
    	if (stake->item_type == ITEM_WEAPON && stake->value[0] == WEAPON_STAKE
    		&& (!str_cmp(stake->material, "wood") || !str_cmp(stake->material, "silver")))
	    break;
    }

    if (stake == NULL)
    {
    	send_to_char("You aren't carrying or wielding a wooden or silver stake.\n\r", ch);
	return;
    }

    if (!IS_VAMPIRE(victim))
    {
	act("$N isn't a vampire.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (IS_AWAKE(victim))
    {
	act("$N must be sleeping or $E will see you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    chance += (ch->tot_level - victim->tot_level) / 3;

    WAIT_STATE(ch, skill_table[gsn_stake].beats);

    if (number_percent() < chance)
    {
	act("{RYou plunge $p into $N's heart, turning $M to dust!{x", ch, victim, NULL, stake, NULL, NULL, NULL, TO_CHAR);
	act("{RYour body disintegrates as $n plunges $p into your heart!{x", ch, victim, NULL, stake, NULL, NULL, NULL, TO_VICT);
	act("{R$N's body turns to dust as $n plunges $p into $S heart!{x", 	ch, victim, NULL, stake, NULL, NULL, NULL, TO_NOTVICT);

	if (IS_NPC(victim))
	    ch->monster_kills++;
	else
	    player_kill(ch, victim);

	victim->death_type = DEATHTYPE_STAKE;

	{
		ROOM_INDEX_DATA *here = victim->in_room;
		victim->position = POS_STANDING;
		if(!p_percent_trigger(victim, NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL))
			p_percent_trigger(NULL, NULL, here, NULL, ch, victim, NULL, NULL, NULL, TRIG_DEATH, NULL);
	}

	raw_kill(victim, FALSE, TRUE, RAWKILL_INCINERATE);
	check_improve(ch, gsn_stake, TRUE, 1);
	return;
    }
    else
    {
	act("You stumble and wake $N!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("You awake to the sight of $n looming over you with $p in $s hand!", ch, victim, NULL, stake, NULL, NULL, NULL, TO_VICT);
	act("$n sneaks up on $N, stakeing $p, but wakes $M!", ch, victim, NULL, stake, NULL, NULL, NULL, TO_NOTVICT);

	one_hit(victim, ch, 0, FALSE);
	check_improve(ch, gsn_stake, FALSE, 1);
    }
}


void do_trample(CHAR_DATA *ch, char *argument)
{
	char arg[MSL];
	char buf[MSL];
	int chance, skill;
	int dam, damclass;
	CHAR_DATA *victim;
	CHAR_DATA *mount;

	if (!(skill = get_skill(ch, gsn_trample))) {
		send_to_char("You have no knowledge of this skill.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);
	if (!arg[0]) {
		send_to_char("Trample whom?\n\r", ch);
		return;
	}

	if (!MOUNTED(ch)) {
		send_to_char("You aren't mounted.\n\r", ch);
		return;
	}

	if (!(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("Your victim isn't here.\n\r", ch);
		return;
	}

	if (MOUNTED(victim)) {
		send_to_char("You can't trample somebody already mounted.\n\r", ch);
		return;
	}

	if (is_safe(ch,victim,TRUE))
		return;

	mount = MOUNTED(ch);

	// Check all three...
	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"pretest") ||
		p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"premount") ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"prerider"))
		return;

	chance = skill;
	chance += get_curr_stat(ch, STAT_DEX) / 3;
	dam =  (500 * (skill)/100);
	dam += (250 * get_skill(ch,gsn_riding))/100;
	dam += UMIN((victim->max_hit)/5, 2000);

	if (ch->tot_level != victim->tot_level) {
		dam = (dam * ch->tot_level)/victim->tot_level;
		chance = (chance * ch->tot_level)/victim->tot_level;
	}

	dam = URANGE(1, dam, 4500);

	if (number_percent() < chance) {
		if(!p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"message_pass")) {
			sprintf(buf, "{RYou charge full-speed on %s into $N!{x", pers(mount, ch));
			act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			sprintf(buf, "{RRiding %s, $n charges into you full-speed!{x", pers(mount, victim));
			act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			sprintf(buf, "{RRiding %s, $n charges into $N at full speed!{x", mount->short_descr);
			act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}

		victim->hit_damage = dam;
		victim->hit_class = DAM_BASH;
		if(!p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"damage_mount") &&
			!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"damage") && victim->hit_damage > 0) {
			dam = victim->hit_damage;
			damclass = victim->hit_class;
			victim->hit_damage = 0;
			victim->hit_class = DAM_NONE;
			damage(ch, victim, dam, gsn_trample, damclass, TRUE);
		} else {
			victim->hit_damage = 0;
			victim->hit_class = DAM_NONE;
		}

		check_improve(ch, gsn_trample, TRUE, 1);
		check_improve(ch, gsn_riding, TRUE, 10);
		victim->position = POS_RESTING;
		victim->bashed = skill_table[gsn_trample].beats;
		WAIT_STATE(ch, skill_table[gsn_trample].beats);
		WAIT_STATE(victim, skill_table[gsn_trample].beats);

		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"attack_pass"))
			multi_hit(ch, victim, TYPE_UNDEFINED);
	} else {
		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"message_fail")) {
			act("{RYou charge towards $N but $E scrambles out of the way!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{R$n charges towards you but you scramble out of the way!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{R$n charges towards $N at full speed but $E scrambles out of the way!{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
		WAIT_STATE(ch, (skill_table[gsn_trample].beats * 3)/2);
		check_improve(ch, gsn_trample, FALSE, 1);
		check_improve(ch, gsn_riding, FALSE, 10);

		if(!p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"attack_fail"))
			multi_hit(victim, ch, TYPE_UNDEFINED);
	}
	p_percent_trigger(mount,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"postmount");
	p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_TRAMPLE,"postrider");
}


// Shift to werewolf or slayer
void do_shift(CHAR_DATA *ch, char *argument)
{
    if ((!IS_SLAYER(ch) && !IS_VAMPIRE(ch)) || get_skill(ch, gsn_shift) == 0)
    {
	send_to_char("You can't do that.\n\r", ch);
	return;
    }

    if (IS_MORPHED(ch))
    {
	send_to_char("You are already morphed as something else.\n\r", ch);
	return;
    }

    if (is_dead(ch))
	return;

    if (!IS_SHIFTED(ch) && number_percent() > get_skill(ch, gsn_shift) - 1)
    {
	send_to_char("You try to let go of the demonic forces within you, but fail.\n\r", ch);
	return;
    }

    shift_char(ch, FALSE);

    /* wait state is here because shift_char above is also used in log out/in routines.
       Only wait when shifting, not going back.*/
    if (ch->shifted != SHIFTED_NONE)
	WAIT_STATE(ch, skill_table[gsn_shift].beats);
}


/* Syn - function to encapsulate shifting so that we don't have to call do_shift to unshift people
   on logout/reboot and re-shift them when they log in */
void shift_char(CHAR_DATA *ch, bool silent)
{
    char buf[MSL];
    AFFECT_DATA af;
    MOB_INDEX_DATA *pMob;
    OBJ_DATA *obj, *obj_next;
    int num_classes;
memset(&af,0,sizeof(af));

    // Unshift
    if (IS_SHIFTED(ch))
    {
	ch->shifted = SHIFTED_NONE;

	free_string(ch->short_descr);
	ch->short_descr = str_dup("");

	free_string(ch->long_descr);
	ch->long_descr = str_dup("");

	ch->dam_type = 17; /*punch */

        while (ch->affected)
            affect_remove(ch, ch->affected);

	REMOVE_BIT(ch->affected_by, AFF_INFRARED);
        ch->affected_by = race_table[ch->race].aff;

	if (!silent) {
	    act("$n winces in pain as $e constrains the demon inside $m.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    act("You constrain the demon inside you.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
    }
    else
    {
	if(IS_AFFECTED2(ch, AFF2_MORPHLOCK)) {
		if (!silent) send_to_char("Your body refuses to change!\n\r", ch);
		return;
	}

	if (IS_SLAYER(ch))
	{
	    if (!silent) {
		send_to_char("{YYou feel the demon inside you taking control.{x\n\r", ch);
		act("{Y$n's eyes turn dark red.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		if (!IS_REMORT(ch))
		{
		    act("{RSharp black spikes rip up out from your body as you take on the form of the Slayer!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    sprintf(buf, "{RSharp black spikes rip up out from %s's torso as $e takes on the form of the Slayer!{x",ch->name);
		}
		else
		{
		    act("{RSharp black spikes rip up out from your body as you take on the form of the Changeling!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		    sprintf(buf, "{RSharp black spikes rip up out from %s's torso as $e takes on the form of the Changeling!{x",ch->name);
		}

		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	}
	else
	{
	    if (!silent) {
		send_to_char("{RYou become a Werewolf!{x\n\r", ch);
		sprintf(buf, "{R%s shifts into a Werewolf!{x", ch->name);
		act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	}

	// Change damage type
	ch->dam_type = 5; // Claw

	// Lower stats
	ch->hit = ch->hit / 2;
	ch->move = ch->move / 2;
	ch->mana = 0;

	// take off equipment
	for (obj = ch->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    if (obj->wear_loc != WEAR_NONE)
	        unequip_char(ch, obj, FALSE);
	}

	// remove affects
	while (ch->affected)
	    affect_remove(ch, ch->affected);

	/* Give them slayer/werewolf affects, while keeping their racial ones */
	if (IS_SLAYER(ch))
	{
	    ch->shifted = SHIFTED_SLAYER;
	    pMob = IS_REMORT(ch) ? get_mob_index(MOB_VNUM_CHANGELING) : get_mob_index(MOB_VNUM_SLAYER);
	    ch->affected_by |= race_table[pMob->race].aff;
	}
	else
	{
            ch->shifted = SHIFTED_WEREWOLF;
	    pMob = get_mob_index(MOB_VNUM_WEREWOLF);
	    ch->affected_by |= race_table[pMob->race].aff;
	}

	/* figure out how many classes - 1 to figure out how much stat boost to give. */
	num_classes = 0;
	if (IS_REMORT(ch))
	    num_classes = 4 + ((ch->tot_level >= 31) + (ch->tot_level >= 61) + (ch->tot_level >= 91) + 1);
	else
	    num_classes += ((ch->tot_level >= 31) + (ch->tot_level >= 61) + (ch->tot_level >= 91));

	af.slot	= WEAR_NONE;
	// Add some affects
	af.where     = TO_AFFECTS;
	af.group     = AFFGROUP_METARACIAL;
	af.type      = gsn_sanctuary;
	af.level     = ch->tot_level * 2;
	af.duration = -1;
	af.location  = APPLY_AC;
	af.modifier  = -100 + -100 * num_classes;
	af.bitvector = AFF_SANCTUARY;
	af.bitvector2 = 0;
	affect_to_char(ch, &af);

	af.where     = TO_AFFECTS;
	af.type      = gsn_haste;
	af.level     = ch->tot_level * 2;
	af.duration = -1;
	af.location  = APPLY_DEX;
	af.modifier  = 2;
	af.bitvector = AFF_HASTE;
	af.bitvector2 = 0;
	affect_to_char(ch, &af);

	/* make it worth it */
        af.where = TO_AFFECTS;
	af.type = gsn_regeneration;
	af.level = ch->tot_level;
	af.duration = -1;
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = AFF_REGENERATION;
	af.bitvector2 = 0;
	affect_to_char(ch, &af);

	af.where       = TO_AFFECTS;
	af.type	 = gsn_shift;
	af.level  = ch->tot_level;
	af.duration = -1;
	af.bitvector = 0;
	af.bitvector2 = 0;
	af.location = APPLY_STR;
	af.modifier = 1 + num_classes;
        affect_to_char(ch, &af);
	af.location = APPLY_INT;
	af.modifier = 1 + num_classes;
        affect_to_char(ch, &af);
	af.location = APPLY_DEX;
	af.modifier = 1 + num_classes;
        affect_to_char(ch, &af);
	af.location = APPLY_HITROLL;
	af.modifier = 10 + num_classes;
        affect_to_char(ch, &af);
	af.location = APPLY_DAMROLL;
	af.modifier = 10 + num_classes;
        affect_to_char(ch, &af);

        SET_BIT(ch->affected_by, AFF_INFRARED);

	if (pMob == NULL)
	{
	    bug("No MOB_VNUM object to shift to.", 0);
	    return;
	}

	ch->short_descr = str_dup(pMob->short_descr);
	ch->long_descr = str_dup(pMob->long_descr);
    }
}


// Vampires' shapeshift
void do_shape(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_STRING_LENGTH];
	CHAR_DATA *pMob;
	int lev, skill;

	argument = one_argument(argument, arg);

	if (IS_NPC(ch)) return;

	if (!(skill = get_skill(ch, gsn_shape))) {
		send_to_char("You can't do that.\n\r", ch);
		return;
	}

	if (IS_SHIFTED(ch)) {
		send_to_char("You have already shifted into something else.\n\r", ch);
		return;
	}

	if (is_dead(ch)) return;

	if (!arg[0]) {
		if (!IS_MORPHED(ch))
			send_to_char("Shape into what?\n\r", ch);
		else if(IS_AFFECTED2(ch, AFF2_MORPHLOCK))
			send_to_char("Your body refuses to change shape!\n\r", ch);
		else {
			ch->morphed = FALSE;
			free_string(ch->short_descr);
			free_string(ch->long_descr);

			ch->short_descr = str_dup("");
			ch->long_descr  = str_dup("");

			send_to_char("{YYou turn back to your normal self.{x\n\r", ch);
		}

		return;
	}

	pMob = get_char_world(ch, arg);
	if (!pMob || !IS_NPC(pMob)) {
		send_to_char("You fail to recognize what to shape yourself into.\n\r", ch);
		return;
	}

	lev = (ch->tot_level + (IS_REMORT(ch) ? ch->tot_level/3 : 0)) * skill / 100;
	if (pMob->tot_level > lev) {
		act("$N is too powerful for you to imitate.", ch, pMob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if(IS_AFFECTED2(ch, AFF2_MORPHLOCK)) {
		send_to_char("Your body refuses to change shape!\n\r", ch);
		return;
	}

	send_to_char("{YYou concentrate for a minute or two.{x\n\r", ch);

	act("$n's eyes turn a misty white.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	sprintf(buf, "{YYou twist your body into the shape of %s!\n\r{x", pMob->short_descr);
	send_to_char(buf, ch);

	act("{YThere is a puff of smoke as $n turns into $t!{x", ch, NULL, NULL, NULL, NULL, pMob->short_descr, NULL, TO_ROOM);

	ch->morphed = TRUE;
	ch->short_descr = str_dup(pMob->short_descr);
	ch->long_descr  = str_dup(pMob->long_descr);

	check_improve(ch, gsn_shape,TRUE,6);
}


// Set a short PK-timer on people.
void set_pk_timer(CHAR_DATA *ch, CHAR_DATA *victim, int time)
{
	if (!ch) {
		bug("set_pk_timer: ch null", 0);
		return;
	}

	send_to_char("{RYou feel a swirl of dangerous energy surrounding you!{x\n\r", ch);
	act("{RA swirl of dangerous energy surrounds $n!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	ch->pk_timer = PULSE_VIOLENCE * 4;
}


bool check_evasion(CHAR_DATA *ch)
{
    // Evasion lets you get away from aggro mobs.
    if (!IS_NPC(ch)
    &&  get_skill(ch, gsn_evasion) > 0
    &&  IS_AFFECTED2(ch, AFF2_EVASION)
    &&  number_percent() < (get_skill(ch, gsn_evasion) - 25 + get_curr_stat(ch,STAT_DEX)))
	return TRUE;

    return FALSE;
}


void do_behead(CHAR_DATA *ch, char *argument)
{
	CHAR_DATA *victim;
	OBJ_DATA *wield;
	int skill, hit, chance;
	char arg[MIL];

	argument = one_argument(argument, arg);

	if (!(skill = get_skill(ch, gsn_behead))) {
		send_to_char("You can't do that.\n\r", ch);
		return;
	}

	if (!arg[0] && !(victim = ch->fighting)) {
		send_to_char("Behead whom?\n\r", ch);
		return;
	}

	if (arg[0] && !(victim = get_char_room(ch, NULL, arg))) {
		send_to_char("You don't see anybody like that around.\n\r", ch);
		return;
	}

	if (ch->in_room != victim->in_room) {
		send_to_char("You don't see anybody like that around.\n\r", ch);
		return;
	}

	if (is_safe(ch, victim, TRUE))
		return;

	wield = get_eq_char(ch, WEAR_WIELD);
	if(!wield)
		wield = get_eq_char(ch, WEAR_SECONDARY);

	if(!wield || attack_table[wield->value[3]].damage != DAM_SLASH) {
		send_to_char("You need a slashing weapon to behead.\n\r", ch);
		return;
	}

	if(!IS_SET(victim->parts,PART_HEAD)) {
		act("$N lacks a head to behead...", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if(victim->hit >= (victim->max_hit / 10)) {
		act("$N is still too strong to behead.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}


	if(p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_BEHEAD,"pretest") ||
		p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_BEHEAD,"pretest") ||
		p_percent_trigger(NULL, wield, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BEHEAD,"pretest"))
		return;

	chance = skill - get_curr_stat(victim, STAT_DEX) + get_curr_stat(ch, STAT_STR) + 2;

	WAIT_STATE(ch, skill_table[gsn_behead].beats);

	act("{RWith a mighty $t, $n brings $s weight upon $N...{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_NOTVICT);
	act("{RWith a mighty $t, you bring your weight upon $N...{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_CHAR);
	act("{RWith a mighty $t, $n brings $s weight upon you...{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_VICT);
	if(number_percent() < chance) {
		if(!check_acro(ch, victim, wield) &&
			!check_catch(ch, victim, wield) &&
			!check_spear_block(ch,victim, wield) &&
			!check_parry(ch, victim, wield) &&
			!check_dodge(ch, victim, wield) &&
			!check_shield_block(ch,victim, wield) &&
			!check_speed_swerve(ch,victim, wield)) {
			victim->set_death_type = DEATHTYPE_BEHEAD;

			victim->hit_damage = 30000;
			p_percent_trigger(NULL,wield, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_ATTACK_BEHEAD,"damage");
			p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_BEHEAD,"damage");

			hit = victim->hit_damage;
			victim->hit_damage = 0;

			if(hit > 0) {
				damage(ch, victim, hit, gsn_behead, DAM_SLASH, FALSE);
			} else {
				p_percent_trigger(victim,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_BEHEAD,"failvict");
				p_percent_trigger(ch,NULL, NULL, NULL, ch, victim, NULL, wield, NULL, TRIG_ATTACK_BEHEAD,"failatt");
			}

			check_improve(ch, gsn_behead, TRUE, 6);
		} else
			check_improve(ch, gsn_behead, FALSE, 6);
	} else {
		act("{Y$N quickly ducks under your decapitating $t!{x", ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_CHAR);
		act("{GYou quickly duck under $n's decapitating $t!{x",	ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_VICT);
		act("{Y$N quickly ducks under $n's decapitating $t!{x",	ch, victim, NULL, NULL, NULL, attack_table[wield->value[3]].noun, NULL, TO_NOTVICT);
		check_improve(ch, gsn_behead, FALSE, 6);
	}
}
