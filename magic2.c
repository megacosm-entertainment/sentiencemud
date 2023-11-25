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
*	    Russ Taylor (rtaylor@hypercube.org)				   *
*	    Gabrielle Taylor (gtaylor@hypercube.org)			   *
*	    Brian Moore (zump@rom.org)					   *
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
#include "merc.h"
#include "recycle.h"
#include "interp.h"
#include "magic.h"
#include "scripts.h"

void do_trance(CHAR_DATA *ch, char *argument)
{
    int chance;
    char buf[MSL];

    if ((chance = get_skill(ch, gsk_deep_trance)) == 0)
    {
	send_to_char("You do not have this skill.\n\r", ch);
	return;
    }

    if (ch->fighting == NULL)
    {
	send_to_char("You can only do this in combat.\n\r", ch);
	return;
    }

    if (ch->mana > (2 * ch->max_mana)/3)
    {
	sprintf(buf, "You cannot fall into a deep trance unless you have %ld or less mana.\n\r", (2 * ch->max_mana/3));
	send_to_char(buf, ch);
	return;
    }

    act("{YYou begin to meditate and fall into a trance.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("{Y$n begins to meditate and fall into a trance.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    ch->trance = 2 * PULSE_VIOLENCE + (100-get_skill(ch, gsk_deep_trance))/10;
}


void trance_end(CHAR_DATA *ch)
{
    int gain;
    char buf[MSL];
    int chance = get_skill(ch, gsk_deep_trance);
    bool worked = TRUE;

    send_to_char("{YYou come out of your trance.{x\n\r", ch);

    if (number_percent() < chance) {
	gain = ch->max_mana / 9;
	gain += get_skill(ch, gsk_deep_trance)/10;

	sprintf(buf, "You regain %d lost mana!", gain);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	ch->mana += gain;

	check_improve(ch, gsk_deep_trance, TRUE, 1);
    } else {
	send_to_char("You fail to gather any lost mana during your deep trance.\n\r", ch);
	check_improve(ch, gsk_deep_trance, FALSE, 1);
	worked = FALSE;
    }

    sprintf(buf, "{Y$n comes out of $s trance");

    if (worked == TRUE)
	strcat(buf, " looking energized.{x");
    else
	strcat(buf, ", but appears unchanged.{x");

    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}

void spell_deflected_cast(CHAR_DATA *ch, CHAR_DATA *victim, SKILL_DATA *skill, AFFECT_DATA *af)
{
#if 0
	if (skill->token)
	{
		if (IS_VALID(ch->cast_token) && ch->cast_token->pIndexData == skill->token)
			execute_script(ch->cast_script, NULL, NULL, NULL, ch->cast_token, NULL, NULL, NULL, ch != NULL ? ch : victim, NULL, NULL, victim, NULL,NULL, NULL,ch->cast_target_name,NULL,TRIG_TOKEN_DEFLECTION,(ch != NULL ? ch->tot_level : af->level),0,0,0,0);
		else
			p_token_index_percent_trigger(skill->token, ch != NULL ? ch : victim, victim, NULL, NULL, NULL, TRIG_TOKEN_DEFLECTION, NULL, 0,0,0,0,0, (ch != NULL ? ch->tot_level : af->level),0,0,0,0);
	}
	else if (skill->deflection_fun)
		(*skill->deflection_fun)(skill, ch != NULL ? ch->tot_level : af->level, ch != NULL ? ch : victim, victim, TARGET_CHAR, WEAR_NONE);
#endif
}

// TODO: Spell Deflection
// Instead of doing anything, it will just attempt to select another target who *doesn't* have the same affect.
// If it ends up being ch, it will check their chain lightning ability to see if they get struck, otherwise it returns null.
// Returns true if there was any interaction with the spell deflection aura... due to messages being generated.
// TODO: Need to make this handle the need to deal with a spell whose purpose is to *reveal* targets, but not necessarily reveal someone whose spell deflection prevents the spell from revealing them.
// One possibility is to not reveal names on those that are hidden
bool check_spell_deflection_new(CHAR_DATA *ch, CHAR_DATA *victim, SKILL_DATA *skill, bool only_visible, CHAR_DATA **target, DEFLECT_VALID_CB *validate)
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
		// visibility is not an issue here
		// No affect, just the bit, it has *no* power, just a message
		if (IS_NULLSTR(skill->msg_defl_noaff_char))
			act("{MThe spell strikes and penetrates your crimson aura!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
			act(skill->msg_defl_noaff_char, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		if (IS_NULLSTR(skill->msg_defl_noaff_room))
			act("{MThe spell strikes and penetrates the crimson aura around $n!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
			act(skill->msg_defl_noaff_room, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		*target = victim;
		return true;
	}

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL,lev,0,0,0,0) )
	{
		// Actual affect, can do some testing...
		if (IS_NULLSTR(skill->msg_defl_aff_char))
			act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
			act(skill->msg_defl_aff_char, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		if (IS_NULLSTR(skill->msg_defl_aff_room))
			act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		else
			act(skill->msg_defl_aff_room, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		// The spell penetrates the aura
		if (ch != NULL)	{
			if (ch == victim)
			{
				if (IS_NULLSTR(skill->msg_defl_pass_self))
					send_to_char("Your spell gets through your protective crimson aura!\n\r", ch);
				else
				{
					send_to_char(skill->msg_defl_pass_self, ch);
					send_to_char("\n\r", ch);
				}
			}
			else
			{
				if (!IS_NULLSTR(skill->msg_defl_pass_char))
					act("Your spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				else
					act(skill->msg_defl_pass_char, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				if (!IS_NULLSTR(skill->msg_defl_pass_vict))
					act("$n's spell gets through your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				else
					act(skill->msg_defl_pass_vict, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				if (!IS_NULLSTR(skill->msg_defl_pass_room))
					act("$n's spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
				else
					act(skill->msg_defl_pass_room, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		}
		*target = victim;
		return true;
	}

	// Actual affect, can do some testing...
	if (IS_NULLSTR(skill->msg_defl_aff_char))
		act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	else
		act(skill->msg_defl_aff_char, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	if (!only_visible ||
		!(IS_SET(victim->affected_by[0], AFF_HIDE) || IS_SET(victim->affected_by[0], AFF_INVISIBLE) || IS_SET(victim->affected_by[0], AFF_SNEAK)))
	{
		if (IS_NULLSTR(skill->msg_defl_aff_room))
			act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		else
			act(skill->msg_defl_aff_room, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	/* it bounces to a random person */
	if (skill->target != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++)
		{
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch != NULL && rch == ch) ||
				rch == victim ||
				IS_AFFECTED2(rch, AFF2_SPELL_DEFLECTION) ||		/* Don't pick someone who has it to prevent bouncing in this mess */
				(validate && !((validate)(ch, victim, rch, skill, af))) ||
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
		// No customization there
		send_to_char("{MThe crimson aura around you vanishes.{x\n\r", victim);
		// TODO: Need a way to only show messages in act() to those that can *SEE* all the actors
		act("{MThe crimson aura around $n vanishes.{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		affect_remove(victim, af);
		*target = victim;
		return true;
	}

	if (rch != NULL) {
		if (ch != NULL) {
			if (IS_NULLSTR(skill->msg_defl_refl_char))
				act("{YYour spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				act(skill->msg_defl_refl_char, ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			if (IS_NULLSTR(skill->msg_defl_refl_vict))
				act("{Y$n's spell bounces off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			else
				act(skill->msg_defl_refl_vict, ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			if (IS_NULLSTR(skill->msg_defl_refl_room))
				act("{Y$n's spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			else
				act(skill->msg_defl_refl_room, ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}
	} else {
		if (ch != NULL) {
			if (IS_NULLSTR(skill->msg_defl_refl_none_char))
				act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			else
				act(skill->msg_defl_refl_none_char, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			if (IS_NULLSTR(skill->msg_defl_refl_none_room))
				act("{Y$n's spell bounces around for a while, then dies out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			else
				act(skill->msg_defl_refl_none_room,ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}
	*target = rch;
	return true;
}


// Returns TRUE if the spell got through.
// TODO: make a copy for the various spell functions...
bool check_spell_deflection(CHAR_DATA *ch, CHAR_DATA *victim, SKILL_DATA *skill, DEFLECT_FUN *deflect)
{
	CHAR_DATA *rch = NULL;
	AFFECT_DATA *af;
	int attempts;
	int lev;

	if (!IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION))
		return TRUE;

	act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	// Find spell deflection
	for (af = victim->affected; af != NULL; af = af->next) {
		if (af->skill == gsk_spell_deflection)
			break;
	}

	if (af == NULL)
		return TRUE;

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL,0,0,0,0,0) )
	{
		if (ch != NULL)	{
			if (ch == victim)
				send_to_char("Your spell gets through your protective crimson aura!\n\r", ch);
			else {
				act("Your spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n's spell gets through your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$n's spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		}

		return TRUE;
	}

	/* it bounces to a random person */
	if (skill->target != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++) {
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch != NULL && rch == ch) ||
				rch == victim ||
				((skill->target == TAR_CHAR_OFFENSIVE ||
				skill->target == TAR_OBJ_CHAR_OFF) &&
				ch != NULL && is_safe(ch, rch, FALSE))) {
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
		return TRUE;
	}

	if (rch != NULL) {
		if (ch != NULL) {
			act("{YYour spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$n's spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}

		if (deflect)
			(*deflect)(ch, victim, skill, af);
		else
			spell_deflected_cast(ch, victim, skill, af);
	} else {
		if (ch != NULL) {
			act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces around for a while, then dies out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}

	return FALSE;
}

// TODO: Make work with the trigger itself, not the script
// TODO: Rework
// Returns TRUE if the spell got through.
bool check_spell_deflection_token(CHAR_DATA *ch, CHAR_DATA *victim, TOKEN_DATA *token, SCRIPT_DATA *script, char *target_name)
{
	CHAR_DATA *rch = NULL;
	AFFECT_DATA *af;
	int attempts;
	int lev;
	int type;

	if (!IS_AFFECTED2(victim, AFF2_SPELL_DEFLECTION))
		return TRUE;

	act("{MThe crimson aura around you pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{MThe crimson aura around $n pulses!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	// Find spell deflection
	for (af = victim->affected; af != NULL; af = af->next) {
		if (af->skill == gsk_spell_deflection)
			break;
	}

	if (af == NULL)
		return TRUE;

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL,0,0,0,0,0) ||
		p_percent_trigger(NULL,NULL,NULL,token,ch, victim, NULL,NULL,NULL,TRIG_SPELLPENETRATE, NULL,0,0,0,0,0) )
	{
		if (ch != NULL)	{
			if (ch == victim)
				send_to_char("Your spell gets through your protective crimson aura!\n\r", ch);
			else {
				act("Your spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n's spell gets through your protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$n's spell gets through $N's protective crimson aura!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		}

		return TRUE;
	}

	type = token->skill->skill->target;
	/* it bounces to a random person */
	if (type != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++) {
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch != NULL && rch == ch) ||
				rch == victim ||
				((type == TAR_CHAR_OFFENSIVE ||
				type == TAR_OBJ_CHAR_OFF) &&
				ch != NULL && is_safe(ch, rch, FALSE))) {
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
		return TRUE;
	}

	if (rch != NULL) {
		if (ch != NULL) {
			act("{YYour spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces off onto you!{x", ch, rch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			act("{Y$n's spell bounces off onto $N!{x", ch,  rch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		}

		execute_script(script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, rch, NULL,NULL, NULL,target_name,NULL,TRIG_NONE,0,0,0,0,0);
	} else {
		if (ch != NULL) {
			act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces around for a while, then dies out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}

	return FALSE;
}

