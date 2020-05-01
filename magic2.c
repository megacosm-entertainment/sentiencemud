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

    if ((chance = get_skill(ch, gsn_deep_trance)) == 0)
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

    ch->trance = 2 * PULSE_VIOLENCE + (100-get_skill(ch,gsn_deep_trance))/10;
}


void trance_end(CHAR_DATA *ch)
{
    int gain;
    char buf[MSL];
    int chance = get_skill(ch, gsn_deep_trance);
    bool worked = TRUE;

    send_to_char("{YYou come out of your trance.{x\n\r", ch);

    if (number_percent() < chance) {
	gain = ch->max_mana / 9;
	gain += get_skill(ch, gsn_deep_trance)/10;

	sprintf(buf, "You regain %d lost mana!", gain);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	ch->mana += gain;

	check_improve(ch, gsn_deep_trance, TRUE, 1);
    } else {
	send_to_char("You fail to gather any lost mana during your deep trance.\n\r", ch);
	check_improve(ch, gsn_deep_trance, FALSE, 1);
	worked = FALSE;
    }

    sprintf(buf, "{Y$n comes out of $s trance");

    if (worked == TRUE)
	strcat(buf, " looking energized.{x");
    else
	strcat(buf, ", but appears unchanged.{x");

    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
}

// Returns TRUE if the spell got through.
bool check_spell_deflection(CHAR_DATA *ch, CHAR_DATA *victim, int sn)
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
		if (af->type == gsn_spell_deflection)
			break;
	}

	if (af == NULL)
		return TRUE;

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL) )
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
	if (skill_table[sn].target != TAR_IGNORE)
		for (attempts = 0; attempts < 6; attempts++) {
			rch = get_random_char(NULL, NULL, victim->in_room, NULL);
			if ((ch != NULL && rch == ch) ||
				rch == victim ||
				((skill_table[sn].target == TAR_CHAR_OFFENSIVE ||
				skill_table[sn].target == TAR_OBJ_CHAR_OFF) &&
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

		(*skill_table[sn].spell_fun)(sn, ch != NULL ? ch->tot_level : af->level, ch != NULL ? ch : rch, rch, TARGET_CHAR, WEAR_NONE);
	} else {
		if (ch != NULL) {
			act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces around for a while, then dies out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}

	return FALSE;
}


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
		if (af->type == gsn_spell_deflection)
			break;
	}

	if (af == NULL)
		return TRUE;

	lev = (af->level * 3)/4;
	lev = URANGE(15, lev, 90);

	if (number_percent() > lev ||
		!p_percent_trigger(victim,NULL,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_SPELLREFLECT, NULL) ||
		p_percent_trigger(NULL,NULL,NULL,token,ch, victim, NULL,NULL,NULL,TRIG_SPELLPENETRATE, NULL) )
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

	type = token->pIndexData->value[TOKVAL_SPELL_TARGET];
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

		execute_script(script->vnum, script, NULL, NULL, NULL, token, NULL, NULL, NULL, ch, NULL, NULL, rch, NULL,NULL, NULL,target_name,NULL,0,0,0,0,0);
	} else {
		if (ch != NULL) {
			act("{YYour spell bounces around for a while, then dies out.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("{Y$n's spell bounces around for a while, then dies out.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		}
	}

	return FALSE;
}

