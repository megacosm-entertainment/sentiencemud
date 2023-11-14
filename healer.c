/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,	   *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *									   *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael	   *
 *  Chastain, Michael Quan, and Mitchell Tse.				   *
 *									   *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc	   *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.						   *
 *									   *
 *  Much time and thought has gone into this software and you are	   *
 *  benefitting.  We hope that you share your changes too.  What goes	   *
 *  around, comes around.						   *
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
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "merc.h"
#include "magic.h"

void do_heal(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *mob;
    char arg[MAX_INPUT_LENGTH];
    int cost;
    SKILL_DATA *skill = NULL;
    SPELL_FUN *spell;
    char *words;

    /* check for healer */
    for ( mob = ch->in_room->people; mob; mob = mob->next_in_room )
    {
        if ( IS_NPC(mob) && IS_SET(mob->act[0], ACT_IS_HEALER) )
            break;
    }

    if ( mob == NULL )
    {
        send_to_char( "You can't do that here.\n\r", ch );
        return;
    }

    one_argument(argument,arg);

    if (arg[0] == '\0')
    {
        /* display price list */
	act("$N says 'I offer the following spells:'",ch,mob, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	send_to_char("  light: cure light wounds      10 silver\n\r",ch);
	send_to_char("  serious: cure serious wounds  15 silver\n\r",ch);
	send_to_char("  critic: cure critical wounds  25 silver\n\r",ch);
	send_to_char("  heal: healing spell           50 silver\n\r",ch);
	send_to_char("  blind: cure blindness         20 silver\n\r",ch);
	send_to_char("  disease: cure disease         15 silver\n\r",ch);
	send_to_char("  poison: cure poison           25 silver\n\r",ch);
	send_to_char("  toxic: cure toxic fumes       25 silver\n\r",ch);
	send_to_char("  uncurse: remove curse         50 silver\n\r",ch);
	send_to_char("  refresh: restore movement      5 silver\n\r",ch);
	send_to_char("  mana: restore mana            10 silver\n\r",ch);
	send_to_char(" Type heal <type> to be healed.\n\r",ch);
	return;
    }

    if (!str_prefix(arg,"light"))
    {
        spell = spell_cure_light;
        skill = gsk_cure_light;
        words = "judicandus dies";
        cost  = 10;
    }

    else if (!str_prefix(arg,"serious"))
    {
	spell = spell_cure_serious;
	skill = gsk_cure_serious;
	words = "judicandus gzfuajg";
	cost  = 15;
    }

    else if (!str_prefix(arg,"critical"))
    {
	spell = spell_cure_critical;
	skill = gsk_cure_critical;
	words = "judicandus qfuhuqar";
	cost  = 25;
    }

    else if (!str_prefix(arg,"heal"))
    {
	spell = spell_heal;
	skill = gsk_heal;
	words = "pzar";
	cost  = 50;
    }

    else if (!str_prefix(arg,"blindness"))
    {
	spell = spell_cure_blindness;
	skill = gsk_cure_blindness;
      	words = "judicandus noselacri";
        cost  = 20;
    }

    else if (!str_prefix(arg,"disease"))
    {
	spell = spell_cure_disease;
	skill = gsk_cure_disease;
	words = "judicandus eugzagz";
	cost = 15;
    }

    else if (!str_prefix(arg,"poison"))
    {
	spell = spell_cure_poison;
	skill = gsk_cure_poison;
	words = "judicandus sausabru";
	cost  = 25;
    }

    else if (!str_prefix(arg,"toxic"))
    {
	spell = spell_cure_toxic;
	skill = gsk_cure_toxic;
	words = "judicandus sithlus";
	cost  = 25;
    }

    else if (!str_prefix(arg,"uncurse") || !str_prefix(arg,"curse"))
    {
	spell = spell_remove_curse;
	skill = gsk_remove_curse;
	words = "candussido judifgz";
	cost  = 50;
    }

    else if (!str_prefix(arg,"mana") || !str_prefix(arg,"energize"))
    {
        spell = NULL;
        skill = NULL;
        words = "energizer";
        cost = 10;
    }


    else if (!str_prefix(arg,"refresh") || !str_prefix(arg,"moves"))
    {
	spell =  spell_refresh;
	skill = gsk_refresh;
	words = "candusima";
	cost  = 5;
    }

    else
    {
	act("{C$N says 'Type 'heal' for a list of spells.'{x", ch,mob, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	return;
    }

    if (cost > (ch->silver + 100*ch->gold))
    {
	act("{C$N says 'You do not have enough money for my services.'{x", ch,mob, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
	return;
    }

    WAIT_STATE(ch,PULSE_VIOLENCE);

    deduct_cost(ch,cost);
    mob->silver += cost;
    act("$n utters the words '$T'.",mob, NULL, NULL, NULL, NULL,NULL,words,TO_ROOM);

    // TODO: make this a spell?  Perhaps "enervate"?  It "transfers" some of your mana to the target
    if (spell == NULL)  /* restore mana trap...kinda hackish */
    {
	ch->mana += dice(2,8) + mob->level / 3;
	ch->mana = UMIN(ch->mana,ch->max_mana);
	send_to_char("A warm glow passes through you.\n\r",ch);
	return;
     }

    if (IS_VALID(skill))
        spell(skill,mob->level,mob,ch,TARGET_CHAR, WEAR_NONE);
}
