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
#include "interp.h"
#include "magic.h"

/*
 * The following special functions are available for mobiles.
 */
DECLARE_SPEC_FUN(	spec_breath_any		);
DECLARE_SPEC_FUN(	spec_breath_acid	);
DECLARE_SPEC_FUN(	spec_breath_fire	);
DECLARE_SPEC_FUN(	spec_breath_frost	);
DECLARE_SPEC_FUN(	spec_breath_gas		);
DECLARE_SPEC_FUN(	spec_breath_lightning	);
DECLARE_SPEC_FUN(	spec_cast_adept		);
DECLARE_SPEC_FUN(	spec_cast_cleric	);
DECLARE_SPEC_FUN(	spec_cast_judge		);
DECLARE_SPEC_FUN(	spec_cast_mage		);
DECLARE_SPEC_FUN(	spec_cast_undead	);
DECLARE_SPEC_FUN(	spec_executioner	);
DECLARE_SPEC_FUN(       spec_questmaster        );
DECLARE_SPEC_FUN(	spec_fido		);
DECLARE_SPEC_FUN(	spec_guard		);
DECLARE_SPEC_FUN(	spec_janitor		);
DECLARE_SPEC_FUN(	spec_mayor		);
DECLARE_SPEC_FUN(	spec_poison		);
DECLARE_SPEC_FUN(	spec_thief		);
DECLARE_SPEC_FUN(	spec_nasty		);
DECLARE_SPEC_FUN(	spec_patrolman		);
DECLARE_SPEC_FUN(	spec_protector		);
DECLARE_SPEC_FUN(	spec_dark_magic		);
DECLARE_SPEC_FUN(	spec_magic_master	);
DECLARE_SPEC_FUN(	spec_fight_bot	);
DECLARE_SPEC_FUN(	spec_pirate	);
DECLARE_SPEC_FUN(	spec_pirate_hunter	);
DECLARE_SPEC_FUN(	spec_invasion	);
DECLARE_SPEC_FUN(	spec_invasion_leader	);

/* the function table */
const   struct  spec_type    spec_table[] =
{
    {	"spec_breath_any",		spec_breath_any		},
    {	"spec_breath_acid",		spec_breath_acid	},
    {	"spec_breath_fire",		spec_breath_fire	},
    {	"spec_breath_frost",		spec_breath_frost	},
    {	"spec_breath_gas",		spec_breath_gas		},
    {	"spec_breath_lightning",	spec_breath_lightning	},
    {	"spec_cast_adept",		spec_cast_adept		},
    {	"spec_cast_cleric",		spec_cast_cleric	},
    {	"spec_cast_judge",		spec_cast_judge		},
    {	"spec_cast_mage",		spec_cast_mage		},
    {	"spec_cast_undead",		spec_cast_undead	},
    {	"spec_executioner",		spec_executioner	},
    {	"spec_fido",			spec_fido		},
    {	"spec_guard",			spec_guard		},
    {	"spec_janitor",			spec_janitor		},
    {	"spec_mayor",			spec_mayor		},
    {	"spec_poison",			spec_poison		},
    {	"spec_thief",			spec_thief		},
    {	"spec_nasty",			spec_nasty		},
    {	"spec_patrolman",		spec_patrolman		},
    {	"spec_questmaster",		spec_questmaster        },
    {	"spec_protector",		spec_protector        	},
    {	"spec_dark_magic",		spec_dark_magic        	},
    {	"spec_magic_master",		spec_magic_master       },
    {	"spec_fight_bot",		spec_fight_bot        	},
    {	"spec_pirate",		spec_pirate        	},
    {	"spec_pirate_hunter",		spec_pirate_hunter     	},
    {	"spec_invasion",		spec_invasion     	},
    {	"spec_invasion_leader",		spec_invasion_leader     	},
    {	NULL,				NULL			}
};

/*
 * Given a name, return the appropriate spec fun.
 */
SPEC_FUN *spec_lookup( const char *name )
{
   int i;

   for ( i = 0; spec_table[i].name != NULL; i++)
   {
        if (LOWER(name[0]) == LOWER(spec_table[i].name[0])
        &&  !str_prefix( name,spec_table[i].name))
            return spec_table[i].function;
   }

    return 0;
}

bool spec_questmaster (CHAR_DATA *ch)
{
    if (ch->fighting != NULL) return spec_cast_mage( ch );
    return false;
}

char *spec_name( SPEC_FUN *function)
{
    int i;

    for (i = 0; spec_table[i].function != NULL; i++)
    {
	if (function == spec_table[i].function)
	    return spec_table[i].name;
    }

    return NULL;
}


bool spec_protector(CHAR_DATA *ch)
{
    CHAR_DATA *vch,*victim = NULL;
    ITERATOR it;

    if (!IS_AWAKE(ch) || IS_AFFECTED(ch,AFF_CALM) || ch->in_room == NULL ||
    	IS_AFFECTED(ch,AFF_CHARM) || ch->fighting != NULL)
        return false;

    /* look for a fight in the room */
    iterator_start(&it, loaded_chars);
    while(( vch = (CHAR_DATA *)iterator_nextdata(&it)))
    {
		/* No attacking self */
		if (vch == ch)
			continue;

		if (vch->fighting != NULL && !IS_IMMORTAL(vch) && !str_cmp(vch->in_room->area->name, "plith") && vch->tot_level > 30 && vch->fighting->tot_level < 30 && IS_NPC(vch->fighting))  /* break it up! */
		{
			victim = vch;
			act("$n gasps.\n\r{C$n says 'Justice must be upheld!'{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("{W$n draws his sword and kicks his horse into a gallop.{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			char_from_room(ch);
			char_to_room(ch, victim->in_room);
			stop_fighting(victim, true);
			act("{W$n gallops in on his mighty steed!{x",ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("{C$n says 'By the council of Olaria, I Sir Albert Stiener, sentence you to\n\rgaol for the term of your natural life!'{x",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("$n drags $N away.",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			act("$n throws you in gaol!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);

			char_from_room(victim);
			char_to_room(victim, get_room_index(11308));

			break;
		}
    }
    iterator_stop(&it);

    return true;
}

bool spec_patrolman(CHAR_DATA *ch)
{
	ITERATOR it;
	CHAR_DATA *vch,*victim = NULL;
	OBJ_DATA *obj;
	char *message;
	int count = 0;

	if (!IS_AWAKE(ch) || IS_AFFECTED(ch,AFF_CALM) || ch->in_room == NULL ||
		IS_AFFECTED(ch,AFF_CHARM) || ch->fighting != NULL)
		return false;

	/* look for a fight in the room */
	for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
	{
		if (vch == ch)
			continue;

		if (vch->fighting != NULL)  /* break it up! */
		{
			if (number_range(0,count) == 0)
				victim = (vch->tot_level > vch->fighting->tot_level) ? vch : vch->fighting;
			count++;
		}
	}

	if (victim == NULL || (IS_NPC(victim) && victim->spec_fun == ch->spec_fun))
		return false;

	if (((obj = get_eq_char(ch,WEAR_NECK_1)) != NULL && obj->pIndexData->vnum == OBJ_VNUM_WHISTLE) ||
		((obj = get_eq_char(ch,WEAR_NECK_2)) != NULL && obj->pIndexData->vnum == OBJ_VNUM_WHISTLE)) {
		act("You blow down hard on $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		act("$n blows on $p, ***WHEEEEEEEEEEEET***",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);

		iterator_start(&it, loaded_chars);
		while(( vch = (CHAR_DATA *)iterator_nextdata(&it)))
		{
			if ( vch->in_room == NULL )
				continue;

			if (vch->in_room != ch->in_room && vch->in_room->area == ch->in_room->area)
				send_to_char( "You hear a shrill whistling sound.\n\r", vch );
		}
		iterator_stop(&it);
	}

	switch (number_range(0,6))
	{
	default:	message = NULL;		break;
	case 0:		message = "$n yells 'All roit! All roit! break it up!'"; break;
	case 1:		message = "$n says 'Society's to blame, but what's a bloke to do?'"; break;
	case 2:		message = "$n mumbles 'bloody kids will be the death of us all.'"; break;
	case 3:		message = "$n shouts 'Stop that! Stop that!' and attacks."; break;
	case 4:		message = "$n pulls out his billy and goes to work."; break;
	case 5:		message = "$n sighs in resignation and proceeds to break up the fight."; break;
	case 6:		message = "$n says 'Settle down, you hooligans!'"; break;
	}

	if (message != NULL)
		act(message,ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ALL);

	multi_hit(ch,victim,TYPE_UNDEFINED);

	return true;
}


bool spec_nasty( CHAR_DATA *ch )
{
    CHAR_DATA *victim, *v_next;

    if (!IS_AWAKE(ch)) {
       return false;
    }

    if (ch->position != POS_FIGHTING) {
       for ( victim = ch->in_room->people; victim != NULL; victim = v_next)
       {
          v_next = victim->next_in_room;
          if (!IS_NPC(victim)
             && (victim->tot_level > ch->tot_level)
             && (victim->tot_level < ch->tot_level + 10))
          {
	     do_function(ch, &do_backstab, victim->name);

             /* should steal some coins right away? :) */
             return true;
          }
       }
       return false;    /*  No one to attack */
    }

    /* okay, we must be fighting.... steal some coins and flee */
    if ( (victim = ch->fighting) == NULL)
        return false;   /* let's be paranoid.... */

    switch ( number_bits(2) )
    {
        case 1:  do_function(ch, &do_flee, "");
                 return true;

        default: return false;
    }
}

/*
 * Core procedure for dragons.
 */
bool dragon( CHAR_DATA *ch, char *spell_name )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    int sn;

    if ( ch->position != POS_FIGHTING )
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;
	if ( victim->fighting == ch && number_bits( 3 ) == 0 )
	    break;
    }

    if ( victim == NULL )
	return false;

    if ( ( sn = skill_lookup( spell_name ) ) < 0 )
	return false;
    (*skill_table[sn].spell_fun) ( sn, ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);
    return true;
}



/*
 * Special procedures for mobiles.
 */
bool spec_breath_any( CHAR_DATA *ch )
{
    if ( ch->position != POS_FIGHTING )
	return false;

    switch ( number_bits( 3 ) )
    {
    case 0: return spec_breath_fire		( ch );
    case 1:
    case 2: return spec_breath_lightning	( ch );
    case 3: return spec_breath_gas		( ch );
    case 4: return spec_breath_acid		( ch );
    case 5:
    case 6:
    case 7: return spec_breath_frost		( ch );
    }

    return false;
}



bool spec_breath_acid( CHAR_DATA *ch )
{
    return dragon( ch, "acid breath" );
}



bool spec_breath_fire( CHAR_DATA *ch )
{
    return dragon( ch, "fire breath" );
}



bool spec_breath_frost( CHAR_DATA *ch )
{
    return dragon( ch, "frost breath" );
}



bool spec_breath_gas( CHAR_DATA *ch )
{
    int sn;

    if ( ch->position != POS_FIGHTING )
	return false;

    if ( ( sn = skill_lookup( "gas breath" ) ) < 0 )
	return false;
    (*skill_table[sn].spell_fun) ( sn, ch->tot_level, ch, NULL,TARGET_CHAR, WEAR_NONE);
    return true;
}



bool spec_breath_lightning( CHAR_DATA *ch )
{
    return dragon( ch, "lightning breath" );
}



bool spec_cast_adept( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    char buf[MAX_STRING_LENGTH];

    if ( !IS_AWAKE(ch) )
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;
	if ( victim != ch && can_see( ch, victim ) && number_bits( 1 ) == 0
	     && !IS_NPC(victim) && victim->tot_level < 11)
	    break;
    }

    if ( victim == NULL )
	return false;

    switch ( number_bits( 4 ) )
    {
    case 0:
	sprintf( buf, "'armour' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 1:
	sprintf( buf, "'bless' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 2:
	sprintf( buf, "'cure blindness' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 3:
	sprintf( buf, "'heal' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 4:
	sprintf( buf, "'cure poison' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 5:
	sprintf( buf, "'refresh' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;

    case 6:
	sprintf( buf, "'cure disease' %s", victim->name );
	do_function( ch, &do_cast, buf );
	return true;
    }

    return false;
}



bool spec_cast_cleric( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    char buf[MAX_STRING_LENGTH];
    char *spell = NULL;
    int sn;

    if ( ch->position != POS_FIGHTING )
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;
	if ( victim->fighting == ch && number_bits( 2 ) == 0 )
	    break;
    }

    if ( victim == NULL )
	return false;

    for ( ;; )
    {
	int min_level = 0;

	switch ( number_bits( 4 ) )
	{
	case  0: min_level =  0; spell = "blindness";      break;
	case  1: min_level =  3; spell = "cause serious";  break;
	case  2: min_level =  7; spell = "earthquake";     break;
	case  3: min_level =  9; spell = "cause critical"; break;
	case  4: min_level = 10; spell = "dispel evil";    break;
	case  5: min_level = 12; spell = "curse";          break;
	case  6: break;
	case  7: min_level = 13; spell = "flamestrike";    break;
	case  8:
	case  9:
	case 10: min_level = 15; spell = "harm";           break;
	case 11: min_level = 15; spell = "plague";	   break;
	default: min_level = 16; spell = "dispel magic";   break;
	}

	if ( ch->tot_level >= min_level )
	    break;
    }

    if ( spell == NULL ) return false;

    if ( ( sn = skill_lookup( spell ) ) < 0 )
	return false;
    //mob_cast( ch, sn , ch->tot_level, victim->name);

	sprintf( buf, "'%s' %s", spell, victim->name );
	do_function( ch, &do_cast, buf );

    return true;
}

bool spec_cast_judge( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    char buf[MAX_STRING_LENGTH];
    char *spell;
    int sn;

    if ( ch->position != POS_FIGHTING )
        return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
        v_next = victim->next_in_room;
        if ( victim->fighting == ch && number_bits( 2 ) == 0 )
            break;
    }

    if ( victim == NULL )
        return false;

    spell = "high explosive";
    if ( ( sn = skill_lookup( spell ) ) < 0 )
        return false;
    //mob_cast( ch, sn , ch->tot_level, victim->name);

	sprintf( buf, "'%s' %s", spell, victim->name );
	do_function( ch, &do_cast, buf );

    return true;
}



bool spec_cast_mage( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    char buf[MAX_STRING_LENGTH];
    char *spell;
    int sn;

    if ( ch->position != POS_FIGHTING )
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;
	if ( victim->fighting == ch && number_bits( 2 ) == 0 )
	    break;
    }

    if ( victim == NULL )
	return false;

    for ( ;; )
    {
	int min_level;

	switch ( number_bits( 4 ) )
	{
	case  0: min_level =  0; spell = "fireball";      break;
	case  1: min_level =  3; spell = "chill touch";    break;
	case  2: min_level =  7; spell = "weaken";         break;
	case  3: min_level =  8; spell = "chill touch";       break;
	case  4: min_level = 11; spell = "colour spray";   break;
	case  5: min_level = 12; spell = "change sex";     break;
	case  6: min_level = 13; spell = "energy drain";   break;
	case  7:
	case  8:
	case  9: min_level = 15; spell = "fireball";       break;
	case 10: min_level = 20; spell = "plague";	   break;
	default: min_level = 20; spell = "acid blast";     break;
	}

	if ( ch->tot_level >= min_level )
	    break;
    }

    if ( ( sn = skill_lookup( spell ) ) < 0 )
		return false;

	sprintf( buf, "'%s' %s", spell, victim->name );
	do_function( ch, &do_cast, buf );

    return true;
}



bool spec_cast_undead( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
	char buf[MAX_STRING_LENGTH];
    char *spell;
    int sn;

    if ( ch->position != POS_FIGHTING )
		return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;
	if ( victim->fighting == ch && number_bits( 2 ) == 0 )
	    break;
    }

    if ( victim == NULL )
	return false;

    for ( ;; )
    {
	int min_level;

	switch ( number_bits( 4 ) )
	{
	case  0: min_level =  0; spell = "curse";          break;
	case  1: min_level =  3; spell = "weaken";         break;
	case  2: min_level =  6; spell = "chill touch";    break;
	case  3: min_level =  9; spell = "blindness";      break;
	case  4: min_level = 12; spell = "poison";         break;
	case  5: min_level = 15; spell = "energy drain";   break;
	case  6: min_level = 18; spell = "harm";           break;
	case  7: min_level = 21; spell = "teleport";       break;
	case  8: min_level = 20; spell = "plague";	   break;
	default: min_level = 18; spell = "harm";           break;
	}

	if ( ch->tot_level >= min_level )
	    break;
    }

    if ( ( sn = skill_lookup( spell ) ) < 0 )
		return false;

	sprintf( buf, "'%s' %s", spell, victim->name );
	do_function( ch, &do_cast, buf );

    return true;
}


bool spec_executioner( CHAR_DATA *ch )
{
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    char *crime;

    if ( !IS_AWAKE(ch) || ch->fighting != NULL )
	return false;

    crime = "";
    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;

/*	if ( !IS_NPC(victim) && IS_SET(victim->act[0], PLR_KILLER)
	&&   can_see(ch,victim))
	    { crime = "KILLER"; break; }

	if ( !IS_NPC(victim) && IS_SET(victim->act[0], PLR_THIEF)
	&&   can_see(ch,victim))
	    { crime = "THIEF"; break; }*/
    }

    if ( victim == NULL )
	return false;

    sprintf( buf, "%s is a %s!  PROTECT THE INNOCENT!  MORE BLOOOOD!!!",
	victim->name, crime );
    do_function(ch, &do_yell, buf );
    multi_hit( ch, victim, TYPE_UNDEFINED );
    return true;
}



bool spec_fido( CHAR_DATA *ch )
{
    OBJ_DATA *corpse;
    OBJ_DATA *c_next;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    if ( !IS_AWAKE(ch) )
	return false;

    for ( corpse = ch->in_room->contents; corpse != NULL; corpse = c_next )
    {
	c_next = corpse->next_content;
	if ( corpse->item_type != ITEM_CORPSE_NPC )
	    continue;

	act( "$n savagely devours a corpse.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	for ( obj = corpse->contains; obj; obj = obj_next )
	{
	    obj_next = obj->next_content;
	    obj_from_obj( obj );
	    obj_to_room( obj, ch->in_room );
	}
	extract_obj( corpse );
	return true;
    }

    return false;
}



bool spec_guard( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;

    if (!IS_AWAKE(ch) || ch->fighting != NULL)
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;

	if (IS_NPC(victim))
	    continue;

	if ((get_eq_char(victim, WEAR_BODY) == NULL || get_eq_char(victim, WEAR_LEGS) == NULL)
	&&   victim->sex == SEX_FEMALE)
	{
	    if (number_percent() < 5)
	    {
		act("$n looks at $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("$n looks at you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$n winks at $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("$n winks at you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{C$n says 'What can I get for a piece of silver?'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    if (number_percent() < 5)
	    {
		act("$n looks at $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("$n looks at you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("{C$n says 'My god woman, put some bloody clothes on.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	}
        else
	if (get_eq_char(victim, WEAR_LEGS) == NULL)
	{
	    if (number_percent() < 5)
	    {
		act("$n looks at $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
		act("$n looks at you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		act("$n pukes everywhere.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{C$n says 'My god man, put some bloody pants on.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    }
	}
    }

    return false;
}


bool spec_janitor( CHAR_DATA *ch )
{
    OBJ_DATA *trash;
    OBJ_DATA *trash_next;

    if ( !IS_AWAKE(ch) )
	return false;

    for ( trash = ch->in_room->contents; trash != NULL; trash = trash_next )
    {
	trash_next = trash->next_content;
	if ( !IS_SET( trash->wear_flags, ITEM_TAKE ) )
	    continue;
	if ( trash->item_type == ITEM_DRINK_CON
	||   trash->item_type == ITEM_TRASH
	||   trash->cost < 10 )
	{
	    act( "$n picks up some trash.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	    obj_from_room( trash );
	    obj_to_char( trash, ch );
	    return true;
	}
    }

    return false;
}



bool spec_mayor( CHAR_DATA *ch )
{
    /* Syn - disabling this since the mayor seems to go off his rocker
    static const char open_path[] =
    "W2a233033b30d00c0333d33gf2m0w322222d223O12d2f1g1.";
    static const char close_path[] =
	"Wh3f3g00000ip000011111122221112110a0S.";

    static const char *path;
    static int pos;
    static bool move;

    if ( !move )
    {
	if ( time_info.hour ==  6 )
	{
	    path = open_path;
	    move = true;
	    pos  = 0;
	}

	if ( time_info.hour == 20 )
	{
	    path = close_path;
	    move = true;
	    pos  = 0;
	}
    }

    if ( ch->fighting != NULL )
	return spec_cast_mage( ch );
    if ( !move || ch->position < POS_SLEEPING )
	return false;

    switch ( path[pos] )
    {
    case '0':
    case '1':
    case '2':
    case '3':
	move_char( ch, path[pos] - '0', false );
	break;

    case 'W':
	ch->position = POS_STANDING;
	act( "$n awakens and groans loudly.", ch, NULL, NULL, TO_ROOM );
	break;

    case 'S':
	ch->position = POS_SLEEPING;
	act( "$n lies down and falls asleep.", ch, NULL, NULL, TO_ROOM );

	// Transfer to his house just to make sure he is in the right spot
	char_from_room( ch );
	char_to_room( ch, get_room_index( 6888 ) );
	break;

    case 'a':
	act( "{C$n says 'Hello Honey!'{x", ch, NULL, NULL, TO_ROOM );
	break;

    case 'w':
	act( "{C$n says 'Rise and shine Maynard! Wakey Wakey hands off snakey! Time to get up!'{x", ch, NULL, NULL, TO_ROOM );
	break;

    case 'm':
	act( "{C$n says 'Cripes, he is up already, Maynard is like a rooster on speed!'{x", ch, NULL, NULL, TO_ROOM );
	break;

    case 'f':
	do_function(ch, &do_open, "door" );
	break;

    case 'g':
	do_function(ch, &do_close, "door" );
	break;

    case 'b':
	act( "{C$n says 'What a strange statue!'{x",
	    ch, NULL, NULL, TO_ROOM );
	break;

    case 'c':
	act( "{C$n says 'Vandals!  Youngsters have no respect for anything!'{x",
	    ch, NULL, NULL, TO_ROOM );
	break;

    case 'h':
	act( "{C$n says 'What a day! Time to go get some rest!'{x",
	    ch, NULL, NULL, TO_ROOM );
	break;

    case 'i':
	act( "{C$n says 'What was that rustle in the bushes!'{x",
	    ch, NULL, NULL, TO_ROOM );
	break;

    case 'p':
	break;

    case 'd':
	act( "{C$n says 'Good day, citizens!'{x", ch, NULL, NULL, TO_ROOM );
	break;

    case 'O':
	act( "{C$n says 'I must say, we need a gate to this city!'{x",
	    ch, NULL, NULL, TO_ROOM );
	break;

    case '.' :
	move = false;
	break;
    }

    pos++;
    */
    return false;
}



bool spec_poison( CHAR_DATA *ch )
{
    CHAR_DATA *victim;

    if ( ch->position != POS_FIGHTING
    || ( victim = ch->fighting ) == NULL
    || number_percent() > 2 * ch->tot_level
    || number_percent() < 80 )
	return false;

    act( "You bite $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR    );
    act( "$n bites $N!",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT );
    act( "$n bites you!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT    );
    spell_poison( gsn_poison, ch->tot_level, ch, victim,TARGET_CHAR, WEAR_NONE);
    return true;
}



bool spec_thief( CHAR_DATA *ch )
{
    CHAR_DATA *victim;
    CHAR_DATA *v_next;
    long gold,silver;

    if ( ch->position != POS_STANDING )
	return false;

    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;

	if ( IS_NPC(victim)
	||   victim->tot_level >= LEVEL_IMMORTAL
	||   number_bits( 5 ) != 0
	||   !can_see(ch,victim))
	    continue;

	if ( IS_AWAKE(victim) && number_range( 0, ch->tot_level ) == 0 )
	{
	    act( "You discover $n's hands in your wallet!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT );
	    act( "$N discovers $n's hands in $S wallet!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT );
	    return true;
	}
	else
	{
	    gold = victim->gold * UMIN(number_range(1,20),ch->tot_level / 2) / 100;
	    gold = UMIN(gold, ch->tot_level * ch->tot_level * 10 );
	    ch->gold     += gold;
	    victim->gold -= gold;
	    silver = victim->silver * UMIN(number_range(1,20),ch->tot_level/2)/100;
	    silver = UMIN(silver,ch->tot_level*ch->tot_level * 25);
	    ch->silver	+= silver;
	    victim->silver -= silver;
	    return true;
	}
    }

    return false;
}

bool spec_dark_magic( CHAR_DATA *ch )
{
	return false;
	/*
   CHAR_DATA *victim;
   CHAR_DATA *v_next;

   if ( ch->position != POS_FIGHTING )
    return false;

   for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
   {
    v_next = victim->next_in_room;
    if ( victim->fighting == ch && number_bits( 2 ) == 0 )
        break;
   }

   if ( victim == NULL )
    return false;
   if (ch->stunned)
       return false;

    switch ( number_bits( 4 ) )
    {

   case 0:
    if (IS_AFFECTED(ch, AFF_BLIND))
    {
    act("$n's eyes flare bright blue for a moment.", ch,NULL,NULL,TO_ROOM);
    //spell_cure_blindness( skill_lookup( "cure blindness" ), ch->tot_level, ch, ch, TAR_CHAR_SELF );
    mob_cast( ch, skill_lookup( "cure blindness" ) , ch->tot_level, ch->name);
       }
       else if (IS_AFFECTED(ch, AFF_CURSE))
       {
           act("$n's eyes flare black for a moment.", ch,NULL,NULL,TO_ROOM);
        //   spell_remove_curse( skill_lookup("remove curse" ), ch->tot_level, ch, ch, TAR_CHAR_SELF );
           mob_cast( ch, skill_lookup( "remove curse" ) , ch->tot_level, ch->name);
        }
        else if (!IS_AFFECTED(ch, AFF_SANCTUARY))
        {
            act("$n's eyes glow bright white for a moment.", ch,NULL, NULL, TO_ROOM);
         //   spell_sanctuary( skill_lookup("sanctuary" ), ch->tot_level, ch, ch, TAR_CHAR_SELF
            mob_cast( ch, skill_lookup( "sanctuary" ) , ch->tot_level, ch->name);
        }
        else if (!IS_AFFECTED(ch, AFF_FAERIE_FIRE))
        {
             act("$n meditates for a moment.", ch, NULL, NULL, TO_ROOM);
             //spell_faerie_fire( skill_lookup("faerie fire" ), ch->tot_level, ch, ch, TAR_CHAR_SELF );
             mob_cast( ch, skill_lookup( "faerie fire" ) , ch->tot_level, ch->name);
        }

        return true;

        case  1:
             if (!IS_AFFECTED(victim, AFF_POISON))
             {
                 act("{C$n says, 'Your blood shall be a pleasant scrifice!{x", ch, NULL, NULL, TO_ROOM);
               //  spell_poison( skill_lookup("poison" ), ch->tot_level, ch, victim, TAR_CHAR_SELF );
                 mob_cast( ch, skill_lookup( "poison" ) , ch->tot_level, victim->name);
              }
              else if (!IS_AFFECTED(victim, AFF_CURSE) )
              {
                  act("$n's eyes flash black for a moment.", ch, NULL, NULL, TO_ROOM);
               //   spell_curse( skill_lookup("curse" ), ch->tot_level, ch, victim,
                  mob_cast( ch, skill_lookup( "cure blindness" ) , ch->tot_level, victim->name);
//TAR_CHAR_SELF );
              }
              else if (IS_AFFECTED(victim, AFF_SANCTUARY))
              {
                 act("{C$n says, 'Your death's are at hand, flee while you "
		     "still can!{x", ch, NULL, NULL, TO_ROOM);
                // spell_dispel_magic( skill_lookup("dispel magic" ), ch->tot_level, ch, victim, TAR_CHAR_SELF );
                 mob_cast( ch, skill_lookup( "dispel magic" ) , ch->tot_level, victim->name);
               }
               return true;

               case 2:
                   if (ch->hit < (ch->max_hit * .50 ) )
                   {
                       act("{C$n says, 'Pathetic fools, you cannot hurt me!{x", ch, NULL, NULL, TO_ROOM);
//                       spell_heal( skill_lookup("heal" ), ch->tot_level, ch, ch,
                       mob_cast( ch, skill_lookup( "heal" ) , ch->tot_level, ch->name);
//TAR_CHAR_SELF);
                    }
                    else if (IS_AFFECTED(ch, AFF_PLAGUE))
                    {
                    act("{C$n says, 'Disease means nothing to me fools!{x", ch, NULL, NULL, TO_ROOM);
//                    spell_cure_disease( skill_lookup("cure disease" ), ch->tot_level, ch, ch, TAR_CHAR_SELF );
                    mob_cast( ch, skill_lookup( "cure disease" ) , ch->tot_level, ch->name);
                    }
                    if (ch->hit < (ch->max_hit * .25 ) )
                    {
                        act("{C$n says, 'Arrgghhh you are indead powerfull!{x", ch, NULL, NULL, TO_ROOM);
                    //    spell_flamestrike( skill_lookup("flamestrike" ), ch->tot_level, ch, victim, TAR_CHAR_SELF );
                        mob_cast( ch, skill_lookup( "flamestrike" ) , ch->tot_level, victim->name);
                     }
                     return true;

                   case 3:
                       if (!IS_AFFECTED(victim, AFF_PLAGUE))
                       {
                           act("$n's eyes flash a brilliant green.", ch, NULL, NULL, TO_ROOM);
                         //  spell_plague( skill_lookup("plague" ), ch->tot_level, ch, victim, TAR_CHAR_SELF );
                           mob_cast( ch, skill_lookup( "plague" ) , ch->tot_level, victim->name);
                        }
                        else if (ch->hit < (ch->max_hit * .10 ) )
                        {
                            do_flee(ch,"");
                        }
                        else if (IS_AFFECTED(ch, AFF_POISON))
                        {
                            act("$n's eyes flash yellow.", ch, NULL, NULL, TO_ROOM);
//                            spell_cure_poison( skill_lookup("cure poison" ), ch->tot_level, ch, ch, TAR_CHAR_SELF );
                            mob_cast( ch, skill_lookup( "cure poison" ) , ch->tot_level, ch->name);
                        }
                        else if (!IS_AFFECTED(victim, AFF_BLIND))
                        {
                            act("$n's eyes glaze over.", ch, NULL, NULL, TO_ROOM);
//                            spell_blindness( skill_lookup("blindness" ), ch->tot_level, ch, victim, TAR_CHAR_SELF );
                            mob_cast( ch, skill_lookup( "blindness" ) , ch->tot_level, victim->name);
                        }


                        return true;


        }

       return false; */
}

bool spec_magic_master( CHAR_DATA *ch )
{
   CHAR_DATA *victim;
   CHAR_DATA *v_next;
   char buf[MAX_STRING_LENGTH];

   if ( ch->position != POS_FIGHTING )
    return false;

   for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
   {
    v_next = victim->next_in_room;
    if ( victim->fighting == ch ) //&& number_percent() < 25 )
        break;
   }

	if ( victim == NULL )
	    return false;
    switch ( number_bits( 4 ) )
    {

	case 0:
    if (IS_AFFECTED(ch, AFF_BLIND))
    {
		sprintf( buf, "'cure blindness' me" );
		do_function( ch, &do_cast, buf );
       }
       else if (IS_AFFECTED(ch, AFF_CURSE))
       {
			sprintf( buf, "'remove curse' me" );
			do_function( ch, &do_cast, buf );
        }
        else if (!IS_AFFECTED(ch, AFF_SANCTUARY))
        {
			sprintf( buf, "'sanctuary' me" );
			do_function( ch, &do_cast, buf );
        }
        else if (!IS_AFFECTED(ch, AFF_FAERIE_FIRE))
        {
			sprintf( buf, "'faerie fire' me" );
			do_function( ch, &do_cast, buf );
        }

        return true;

        case  1:
             if (!IS_AFFECTED(victim, AFF_POISON))
             {
			sprintf( buf, "'poison' %s", victim->name );
			do_function( ch, &do_cast, buf );
              }
              else if (!IS_AFFECTED(victim, AFF_CURSE) )
              {
			sprintf( buf, "'curse' %s", victim->name );
			do_function( ch, &do_cast, buf );
              }
              else if (IS_AFFECTED(victim, AFF_SANCTUARY))
              {
			sprintf( buf, "'dispel magic' %s", victim->name );
			do_function( ch, &do_cast, buf );
               }
               return true;

               case 2:
                   if (ch->hit < (ch->max_hit * .50 ) )
                   {
			sprintf( buf, "'cure critical' me" );
			do_function( ch, &do_cast, buf );
                    }
                    else if (IS_AFFECTED(ch, AFF_PLAGUE))
                    {
			sprintf( buf, "'cure disease' me" );
			do_function( ch, &do_cast, buf );
                    }
                    if (ch->hit < (ch->max_hit * .25 ) )
                    {
			sprintf( buf, "'flamestrike' me" );
			do_function( ch, &do_cast, buf );
                     }
                     return true;

                   case 3:
                       if (!IS_AFFECTED(victim, AFF_PLAGUE))
                       {
			sprintf( buf, "'plague' %s", victim->name );
			do_function( ch, &do_cast, buf );
                        }
                        else if (ch->hit < (ch->max_hit * .10 ) )
                        {
                            do_flee(ch,"");
                        }
                        else if (IS_AFFECTED(ch, AFF_POISON))
                        {
			sprintf( buf, "'cure poison' me" );
			do_function( ch, &do_cast, buf );
                        }
                        else if (!IS_AFFECTED(victim, AFF_BLIND))
                        {
			sprintf( buf, "'blindness' %s", victim->name );
			do_function( ch, &do_cast, buf );
                        }


                        return true;


        }

       return false;
}

bool spec_fight_bot( CHAR_DATA *ch ) {
   CHAR_DATA *victim;
   char buf[MAX_STRING_LENGTH];

   if ( ch->position != POS_FIGHTING )
   {
       return false;
   }

   // Dont want to cast while fighting
   if ( ch->cast > 0 )
   {
	   return false;
   }

   victim = ch->fighting;

   if (!IS_AFFECTED(victim, AFF_WEB))
   {
       sprintf( buf, "'web' %s", victim->name );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   if (ch->hit < (ch->max_hit * .65 ) )
   {
       sprintf( buf, "'heal' me" );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   if (ch->hit < (ch->max_hit * .5 ) )
   {
	   if (!IS_AFFECTED2(victim, AFF2_SILENCE))
	   {
		   sprintf( buf, "'silence' %s", victim->name );
		   do_function( ch, &do_cast, buf );
	   	   return true;
	   }
   }

   if (IS_AFFECTED(ch, AFF_BLIND))
   {
	   sprintf( buf, "'cure blindness' me" );
	   do_function( ch, &do_cast, buf );
   }

   if (!IS_AFFECTED(ch, AFF_SANCTUARY))
   {
	   OBJ_DATA *obj;
	   OBJ_DATA *obj_next;

		for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
		{

			if ( obj->wear_loc != WEAR_NONE
			&&   can_see_obj( ch, obj ))
			{
				remove_obj( ch, obj->wear_loc, true );
			}
		}

		for ( obj = ch->carrying; obj != NULL; obj = obj_next )
		{
			obj_next = obj->next_content;
			if ( obj->wear_loc == WEAR_NONE && can_see_obj( ch, obj ) )
			wear_obj( ch, obj, false );
		}
   }

   if (!IS_AFFECTED(victim, AFF_BLIND))
   {
       sprintf( buf, "'blindness' %s", victim->name );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   if ( victim->cast > 0 )
   {
       sprintf( buf, "'counterspell' %s", victim->name );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   if (!IS_AFFECTED(victim, AFF_POISON))
   {
       sprintf( buf, "'poison' %s", victim->name );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   if (!IS_AFFECTED(victim, AFF_PLAGUE))
   {
       sprintf( buf, "'plague' %s", victim->name );
       do_function( ch, &do_cast, buf );
	   return true;
   }

   sprintf( buf, "'magic missile' %s", victim->name );
   do_function( ch, &do_cast, buf );

   return true;
}

bool spec_pirate( CHAR_DATA *ch ) {
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];

	if ( ch->position != POS_FIGHTING )
	{
		return false;
	}

	// Dont want to cast while fighting
	if ( ch->cast > 0 )
	{
		return false;
	}

	victim = ch->fighting;

	if (!IS_AFFECTED(victim, AFF2_SILENCE) && number_percent() < 2)
	{
		sprintf( buf, "'silence' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(victim, AFF_WEB) && number_percent() < 5)
	{
		sprintf( buf, "'web' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (ch->hit < (ch->max_hit * .65 ) && number_percent() < 5)
	{
		sprintf( buf, "'cure critical' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (IS_AFFECTED(ch, AFF_BLIND) && number_percent() < 5)
	{
		sprintf( buf, "'cure blindness' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(ch, AFF_SANCTUARY))
	{
		OBJ_DATA *obj;
		OBJ_DATA *obj_next;

		for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
		{

			if ( obj->wear_loc != WEAR_NONE
					&&   can_see_obj( ch, obj ))
			{
				remove_obj( ch, obj->wear_loc, true );
			}
		}

		for ( obj = ch->carrying; obj != NULL; obj = obj_next )
		{
			obj_next = obj->next_content;
			if ( obj->wear_loc == WEAR_NONE && can_see_obj( ch, obj ) )
				wear_obj( ch, obj, false );
		}
	}

	if (!IS_AFFECTED(victim, AFF_BLIND) && number_percent() < 5)
	{
		sprintf( buf, "'blindness' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if ( victim->cast > 0 && number_percent() < 5 )
	{
		sprintf( buf, "'counterspell' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(victim, AFF_POISON) && number_percent() < 5)
	{
		sprintf( buf, "'poison' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(victim, AFF_PLAGUE) && number_percent() < 5)
	{
		sprintf( buf, "'plague' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

   return true;
}

bool spec_pirate_hunter( CHAR_DATA *ch ) {
	CHAR_DATA *victim;
	char buf[MAX_STRING_LENGTH];

	if ( ch->position != POS_FIGHTING )
	{
		return false;
	}

	// Dont want to cast while fighting
	if ( ch->cast > 0 ) {
		return false;
	}

	victim = ch->fighting;

  if (number_percent() < 2) {
    do_say(ch, "I can't wait to go get the bounty thats on your head!");
  }
  else
  if (number_percent() < 2) {
    do_say(ch, "Die filthy pirate!");
  }

	if (!IS_AFFECTED(victim, AFF_WEB) && number_percent() < 5)
	{
		sprintf( buf, "'web' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (ch->hit < (ch->max_hit * .65 ) && number_percent() < 10)
	{
		sprintf( buf, "'cure critical' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (IS_AFFECTED(ch, AFF_BLIND) && number_percent() < 10)
	{
		sprintf( buf, "'cure blindness' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (IS_AFFECTED(ch, AFF_HASTE) && number_percent() < 10)
	{
		sprintf( buf, "'haste' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(ch, AFF_SANCTUARY) && number_percent() < 5)
	{
		sprintf( buf, "'sanctuary' me" );
		do_function( ch, &do_cast, buf );
    return true;
	}

	if (!IS_AFFECTED(victim, AFF_BLIND) && number_percent() < 5)
	{
		sprintf( buf, "'blindness' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

   return true;
}

bool spec_invasion( CHAR_DATA *ch )
{
    int i = 0;
    char buf[MSL];
    //CHAR_DATA *victim;
    //CHAR_DATA *v_next;


    if (!IS_AWAKE(ch) || ch->fighting != NULL)
	return false;

/*
    for ( victim = ch->in_room->people; victim != NULL; victim = v_next )
    {
	v_next = victim->next_in_room;

	if (IS_NPC(victim) && victim != ch && victim->fighting == NULL && number_percent() < 2) {
    set_fighting(ch, victim);
  }
 */

    if (number_percent() < 2) {
      i = number_range(0, 12);
			switch(i) {
        case 0:
          sprintf(buf, "$n runs around flailing $m arms wildly! $s is out of control!");
          break;
        case 1:
          sprintf(buf, "$n kicks the ground angrily.");
          break;
        case 2:
          sprintf(buf, "$n looks for a window to break.");
          break;
        case 3:
          sprintf(buf, "$n looks for someone to pound..");
          break;
        case 4:
          sprintf(buf, "$n hisses angrily at you.");
          break;
        case 5:
          sprintf(buf, "$n waves his fist in the air at you.");
          break;
        case 6:
          sprintf(buf, "$n gives a nasty mean look.");
          break;
        case 7:
          sprintf(buf, "$n stomps on a tin can.");
          break;
        case 8:
          sprintf(buf, "$n flares $m nostrils angrily at you.");
          break;
        case 9:
          sprintf(buf, "$n calls you names. How rude.");
          break;
        case 10:
          sprintf(buf, "$n wonders where $m leader as gone.");
          break;
        case 11:
          sprintf(buf, "$n bites at the air ferociously.");
          break;
        case 12:
          sprintf(buf, "You hear screams nearby.");
          break;
      }
      act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    return true;
   }
   return false;
}

bool spec_invasion_leader( CHAR_DATA *ch ) {
	CHAR_DATA *victim;
  CHAR_DATA *vch;
	char buf[MAX_STRING_LENGTH];
  int i = 0;

	if ( ch->position != POS_FIGHTING )
	{
		return false;
	}

	// Dont want to cast while fighting
	if ( ch->cast > 0 ) {
		return false;
	}

	victim = ch->fighting;

  for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room) {
    if (!IS_NPC(vch) &&
        vch->fighting == ch &&
        ch->tot_level - 30 < victim->tot_level - 30) {
        sprintf(buf, "This quest is for level %d maximum %s.", ch->tot_level - 30, victim->name);
        do_say(ch, buf);
        stop_fighting(vch, true);
    }
  }

  if (number_percent() < 10) {
    i = number_range(0, 6);

    switch(i) {
      case 0:
        do_say(ch, "You can't stop me from taking over this town!");
        break;

      case 1:
        do_say(ch, "Go play elsewhere, this is my town now!");
        break;

      case 2:
        do_say(ch, "You know you can't defeat me. Why do you try?");
        break;

      case 3:
        do_say(ch, "If you slay me, my minions will come after you!");
        break;

      case 4:
        do_say(ch, "Your time has finished!");
        break;

      case 5:
        do_say(ch, "I could defeat you any time I wanted.");
        break;

      case 6:
        if (ch->hit < (ch->max_hit * 0.25)) {
        	do_say(ch, "This isn't looking too good for me.");
        }
        break;
    }
  }

	if (!IS_AFFECTED(victim, AFF_WEB) && number_percent() < 2)
	{
		sprintf( buf, "'web' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (ch->hit < (ch->max_hit * .65 ) && number_percent() < 10)
	{
		sprintf( buf, "'cure critical' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (IS_AFFECTED(ch, AFF_BLIND) && number_percent() < 10)
	{
		sprintf( buf, "'cure blindness' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (IS_AFFECTED(ch, AFF_HASTE) && number_percent() < 10)
	{
		sprintf( buf, "'haste' me" );
		do_function( ch, &do_cast, buf );
		return true;
	}

	if (!IS_AFFECTED(ch, AFF_SANCTUARY) && number_percent() < 5)
	{
		sprintf( buf, "'sanctuary' me" );
		do_function( ch, &do_cast, buf );
    return true;
	}

	if (!IS_AFFECTED(victim, AFF_BLIND) && number_percent() < 5)
	{
		sprintf( buf, "'blindness' %s", victim->name );
		do_function( ch, &do_cast, buf );
		return true;
	}

   return true;
}
