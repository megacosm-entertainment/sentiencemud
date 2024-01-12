/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "merc.h"
#include "interp.h"

void deduct_mana(CHAR_DATA *ch,int cost);

bool is_ranged_weapon(OBJ_DATA *obj)
{
	if (!IS_VALID(obj)) return false;

	if (!IS_WEAPON(obj)) return false;

	return (WEAPON(obj)->range > 0);
}

bool is_quiver(OBJ_DATA *obj, int subtype)
{
	if (!IS_VALID(obj)) return false;

	if (!IS_CONTAINER(obj)) return false;

	return container_filters_for_item_type(obj, ITEM_AMMO, subtype);
}

void do_shoot( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *victim;
    CHAR_DATA *vch = NULL;
    OBJ_DATA *bow = NULL;
    OBJ_DATA *obj = NULL;
    OBJ_DATA *quiver = NULL;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int direction = -1;
    int ii;
    int range, beats;

    if ( IS_SET(ch->in_room->room_flag[0], ROOM_SAFE ) )
    {
    	send_to_char("You cannot use ranged weapons from safe rooms.\n\r", ch );
	return;
    }

    // Is person holding weapon?
    bow = get_eq_char( ch, WEAR_WIELD );
    if ( bow == NULL || !is_ranged_weapon(bow) )
    {
		bow = get_eq_char( ch, WEAR_HOLD );
		if ( bow == NULL || !is_ranged_weapon(bow) )
		{
		    bow = get_eq_char( ch, WEAR_BACK );
		    if ( bow == NULL || !is_ranged_weapon(bow) )
		    {
				send_to_char("You don't have a ranged weapon.\n\r", ch );
				return;
			}
		}
    }

	int ammo_type = WEAPON(bow)->ammo;

	int mana_cost = 0;
	if (ammo_type == AMMO_MANA)
	{
		// Need to determine how much mana to use
	
		ch->tempstore[0] = 100;
		// Tempstore1 == mana cost, defaults to 100
		p_percent_trigger(NULL, bow, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_AMMO_MANA, NULL, 0, 0, 0, 0, 0);
		mana_cost = UMAX(0, ch->tempstore[0]);
	}

	if (mana_cost > 0 && (ch->mana + ch->manastore) < mana_cost)
	{
		send_to_char("You don't have enough mana.\n\r", ch);
		return;
	}

	// Get the victim first...
	argument = one_argument( argument, arg1 );
	argument = one_argument( argument, arg2 );

    victim = NULL;
    if ( arg1[0] == '\0' && arg2[0] == '\0' && (victim = ch->fighting) == NULL )
    {
		send_to_char("Syntax: shoot <target> [direction]\n\r", ch );
		return;
    }

    // Did player put in a direction?
    if ( arg2[0] != '\0' )
    {
        if ( ( direction = parse_direction( arg2 ) ) == -1 )
        {
            send_to_char("That isn't a direction.\n\r", ch);
            return;
        }
    }

    if ( victim == NULL )
    {
		if ( direction != -1 )
			victim = search_dir_name( ch, arg1, direction, WEAPON(bow)->range );
		else
		{
			direction = -1;

			if ( ( victim = get_char_room( ch, NULL, arg1 ) ) == NULL )
			{
				for ( ii = 0; ii < MAX_DIR; ii++ )
				{
					if ( (vch = search_dir_name( ch, arg1, ii, WEAPON(bow)->range ) ) != NULL )
					{
						if ( direction != -1 )
						{
							send_to_char("Multiple targets like that, which direction?\n\r", ch );
							return;
						}

						victim = vch;
						direction = ii;
					}
				}
			}
		}
    }

    if ( victim == NULL )
    {
		if ( direction == -1 )
			send_to_char("You don't see anyone like that around.\n\r", ch );
		else
			act("You don't see anyone like that to the $t.", ch, NULL, NULL, NULL, NULL, dir_name[direction], NULL, TO_CHAR );

		return;
    }

    if ( victim == ch )
    {
		send_to_char("There are far more pleasant ways to commit suicide.\n\r", ch );
		return;
    }

    if ( is_safe( ch, victim, TRUE ) )
		return;

	// Note: AMMO_MANA weapons *must* use this to be able to shoot anything as it won't be able to pull any physical ammo out.
	ch->outbound_object = NULL;
	p_percent_trigger(NULL, bow, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_AMMO_SCRIPTED, NULL, 0, 0, 0, 0, 0);

	obj = ch->outbound_object;
	if (obj == NULL)
	{
		// If the weapon hasn't generated any ammo, but isn't defined to *use* any, complain and fail.
		if (ammo_type == AMMO_NONE || ammo_type == AMMO_MANA)
		{
			send_to_char("You can't shoot that.\n\r", ch);
			return;
		}

		// Check for a quiver first, make sure it's the right kind
		if ( ( quiver = get_eq_char( ch, WEAR_SHOULDER ) ) != NULL )
		{
			for ( obj = quiver->contains; obj != NULL; obj = obj->next_content )
			{
				if ( can_see_obj(ch, obj) &&
					IS_AMMO(obj) && AMMO(obj)->type == ammo_type &&
					obj->level <= ch->tot_level)
				{
					// If it pulls an ammo item from the quiver,
					// Make sure the stop_ranged doesn't nuke it as it exists in something.
					REMOVE_BIT(obj->extra[1], ITEM_CREATED);
					break;
				}
			}

			// No object from the quiver, try getting a scripted object
			if (obj == NULL)
			{
				ch->outbound_object = NULL;
				p_percent_trigger(NULL, quiver, NULL, NULL, ch, victim, NULL, bow, NULL, TRIG_AMMO_SCRIPTED, NULL, 0, 0, 0, 0, 0);
				obj = ch->outbound_object;
			}

			// If no quiver or if quiver is empty, check inventory
			if ( obj == NULL )
			{
				quiver = NULL;

				for ( obj = ch->carrying; obj != NULL; obj = obj->next_content )
				{
					if ( can_see_obj(ch, obj) &&
						IS_AMMO(obj) && AMMO(obj)->type == ammo_type &&
						obj->level <= ch->tot_level)
					{
						// If it pulls an ammo item from your inventory,
						// Make sure the stop_ranged doesn't nuke it as it exists in something.
						REMOVE_BIT(obj->extra[1], ITEM_CREATED);
						break;
					}
				}
			}
		}

	}

	if ( obj == NULL )
	{
		send_to_char("You have a weapon, but nothing to fire.\n\r", ch );
		return;
	}


    range = get_distance( ch, arg1, direction, WEAPON(bow)->range );

    if ( quiver != NULL )
    { // have quiver
	if ( ch->in_room == victim->in_room ) // same room
	{
	    sprintf( buf, "$n draws %s from %s and takes careful aim at $N.",
		obj->short_descr,
		quiver->short_descr );
	    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT );

	    sprintf( buf, "{R$n draws %s from %s and takes careful aim at you!{x",
		obj->short_descr,
		quiver->short_descr );
	    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT );
	}
	else // ranged attack
	{
	    sprintf(buf, "$n draws %s from %s and takes careful aim %swards.",
		    obj->short_descr, quiver->short_descr, dir_name[ direction ] );
	    act( buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	sprintf(buf, "You draw %s from %s and take careful aim at %s.",
	    obj->short_descr, quiver->short_descr, pers(victim, ch));
	act( buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    }
    else
    { // no quiver
	if ( ch->in_room == victim->in_room )
	{
	    sprintf( buf, "$n takes %s and aims carefully at $N.",
		obj->short_descr );

	    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT );

	    sprintf( buf, "{R$n takes %s and aims carefully at you!{x",
		obj->short_descr );
	    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT );
	}
	else
	{
	    sprintf(buf, "$n takes %s and aims carefully %swards.",
		    obj->short_descr, dir_name[ direction ] );
	    act( buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	sprintf(buf, "You take %s and aim carefully at %s.",
	    obj->short_descr, pers(victim, ch));
	act( buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    }

    // If arrow from quiver then faster to load and aim
	SKILL_DATA *ranged_gsk = gsk_archery;
    switch ( WEAPON(bow)->weapon_class ) {
    default:					beats = 8; break;
    case WEAPON_BOW:			beats = 6; break;
    case WEAPON_CROSSBOW:		beats = 12; break;
    case WEAPON_HARPOON:		beats = 12; ranged_gsk = gsk_harpooning; break;
    case WEAPON_BLOWGUN:		beats = 3; ranged_gsk = gsk_blowgun; break;
	// case WEAPON_GUN:			beats = 3; ranged_gsk = gsk_firearms; break;
    }
    if(!quiver) beats *= 2;
    if(ch->fighting) beats *= 2;
    RANGED_STATE( ch, beats );

	// Rather than just improving archery for all ranged attacks, it will use the appropriate skill
    if ( get_skill(ch, ranged_gsk) > 0 )
    {
    	ch->ranged -= (ch->ranged * get_skill(ch, ranged_gsk))/300;
		check_improve(ch, ranged_gsk, TRUE, 6);
    }

    ch->projectile_weapon = bow;
    ch->projectile = obj;
    ch->projectile_dir = direction;
    ch->projectile_range = range;
	ch->projectile_mana = mana_cost;
    ch->projectile_victim = str_dup( victim->name );
}


void ranged_end( CHAR_DATA *ch )
{
    char buf[MAX_STRING_LENGTH];//, buf2[MSL];
    OBJ_DATA *bow = NULL;
    OBJ_DATA *obj = NULL;
    OBJ_DATA *shield = NULL;
    CHAR_DATA *victim = NULL;
    ROOM_INDEX_DATA *target_room = ch->in_room;
	SKILL_DATA *sk;
    int direction;
    int skill;
    int beats;
	int mana;
    int dam;
    int dt;

    bow = ch->projectile_weapon;
    obj = ch->projectile;
    direction = ch->projectile_dir;
	mana = ch->projectile_mana;

	REMOVE_BIT(obj->extra[1], ITEM_CREATED);

    // Take it out of the weapon container
    if ( obj->in_obj != NULL )
		obj_from_obj( obj );
	else if (obj->carried_by == NULL)
	{
		if (obj->in_room != NULL)
			obj_from_room( obj );
	}
	else if (obj->carried_by != ch)
		obj_from_char( obj );
	
	if (obj->carried_by == NULL)
		obj_to_char(obj, ch);

    if ( ch->projectile_victim == NULL || ch->projectile_victim[0] == '\0' )
    {
		sprintf( buf, "ranged_end: ch->projectile_victim NULL, ch %s!", ch->name);
		bug( buf, 0 );
		return;
    }

	if (mana > 0 && (ch->mana + ch->manastore) < mana)
	{
		send_to_char("You don't have enough mana.\n\r", ch);
		return;
	}

    // Confirm victim still exists before firing.
    if ( ch->projectile_range > 0 )
		victim = search_dir_name( ch, ch->projectile_victim, direction, WEAPON(bow)->range );
    else
		victim = get_char_room( ch, NULL, ch->projectile_victim );

    // If NULL then target's moved
    if ( victim == NULL )
    {
		// See if by any chance the victim has moved into the shooter's room.
		if ( ( victim = get_char_room( ch, NULL, ch->projectile_victim ) ) == NULL )
		{
			send_to_char("Your target is no longer in clear view.\n\r", ch);
			ch->projectile_victim = NULL;
			ch->projectile_dir = -1;
			ch->projectile_range = 0;
			ch->projectile_mana = 0;
			ch->projectile = NULL;
			return;
		}
    }

	if (mana > 0)
		deduct_mana(ch, mana);

    switch(WEAPON(bow)->weapon_class) {
    default:
    case WEAPON_EXOTIC:		sk = gsk_exotic; break;
    case WEAPON_BOW:		sk = gsk_bow; break;
    case WEAPON_CROSSBOW:	sk = gsk_crossbow; break;
    case WEAPON_HARPOON:	sk = gsk_harpooning; break;
    case WEAPON_BLOWGUN:	sk = gsk_blowgun; break;
    }
    skill = get_skill(ch, sk);
    beats = sk->beats;
    // @@@NIB : 20070128 ----------

    //sprintf( buf, "%s gets %d%% from skill, ", ch->name, skill );


	// TODO: Turn this into a trait
    if ( IS_ELF( ch ) ) {
		skill += 5;
    }

    if (!IS_NPC(ch)) {
		// TODO: Turn these into traits
	    if ( sk == gsk_blowgun ) {
	        // @@@NIB : 20070126 : if an assassin/ninja, they get a +5% accuracy
	        //	with blowguns
			// TODO: Make this a trait (ninja + assassin synergy)
			if (get_current_class(ch) == gcl_ninja)
			    skill += 5;
		// TODO: Make this a trait
	    } else if ( get_current_class(ch) == gcl_ranger )
			skill += 5;
    }


    skill += get_curr_stat(ch, STAT_DEX) / 8;

    // @@@NIB : 20070128
    if(sk == gsk_bow || sk == gsk_crossbow) {
		// Add in archery skill
		skill += get_skill( ch, gsk_archery) / 5;
	//sprintf(buf2, "+%d%% from archery, total skill is %d%%\n\r", get_skill(ch, gsk_archery)/5, skill); strcat(buf, buf2); //send_to_char(buf,ch);
    }

    if ( number_percent() > skill )
    {
		send_to_char("You fumble your weapon as you attempt to fire it.\n\r", ch );
		act("{R$n attempts to fire $p, but fumbles.{x", ch, NULL, NULL, bow, NULL, NULL, NULL, TO_ROOM );

		if (IS_SET(obj->extra[0], ITEM_EPHEMERAL))
		{
			act("{B$p falls to the ground and vanishes.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			extract_obj(obj);
			obj = ch->projectile = NULL;
		}
		else
		{
			act("{B$p falls onto the ground.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			obj_to_room( obj, ch->in_room );
		}
		check_improve( ch, sk, FALSE, 1 );
		check_improve( ch, gsk_archery, FALSE, 1 );
		stop_ranged( ch, FALSE );
		return;
    }


    dam = (int)(dice_roll(&WEAPON(bow)->attacks[0].damage) + dice_roll(&AMMO(obj)->damage) + 40*log(GET_DAMROLL( ch )));

    dam += (dam * get_skill(ch, gsk_archery))/175;

    dam = (dam * 3) / 2; // So ranged weapons do more damage than throwing bolts

	//sprintf(buf, "%s has damage %d and skill %d\n\r", HANDLE(ch), dam, skill ); //send_to_char(buf,ch);

	// Figure out damage type in case arrows are special
    if (IS_SET(AMMO(obj)->flags, WEAPON_SHARP) && !IS_SET(AMMO(obj)->flags, WEAPON_DULL)) {
		dt = DAM_PIERCE;
		dam += dam / 20;
    } else if (!IS_SET(AMMO(obj)->flags, WEAPON_SHARP) && IS_SET(AMMO(obj)->flags, WEAPON_DULL)) {
		dt = DAM_BASH;
		dam -= dam / 5;
    }

    if (IS_SET(AMMO(obj)->flags, WEAPON_POISON))
		dt = DAM_POISON;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_VAMPIRIC))
		dt = DAM_NEGATIVE;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_FLAMING))
		dt = DAM_FIRE;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_FROST))
		dt = DAM_COLD;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_SHOCKING))
		dt = DAM_LIGHTNING;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_ACIDIC))
		dt = DAM_ACID;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_BLAZE))
		dt = DAM_LIGHT;
    else if (IS_SET(AMMO(obj)->flags, WEAPON_RESONATE))
		dt = DAM_SOUND;
    else
		dt = attack_table[AMMO(obj)->damage_type].damage;

    // ch and victim in the same room
    if ( ch->in_room == victim->in_room )
    {
		act("{Y$n fires $p{Y at you!{x", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
		act("{Y$n fires $p{Y at $N!{x",  ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
		act("{YYou fire $p{Y at $N.{x",  ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		// If we killed them make the victim null
		if ( damage( ch, victim, dam, gsk_archery, TYPE_UNDEFINED, dt, TRUE))
		    victim = NULL;

		//obj_from_char( obj );

		if (IS_SET(obj->extra[0], ITEM_EPHEMERAL))
		{
			act("{B$p vanishes.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			extract_obj(obj);
			obj = ch->projectile = NULL;
		}
		else
		{
			if ( victim == NULL )
				obj_to_room( obj, ch->in_room );
			else
			{
				obj_to_char( obj, victim );
				if ( victim->fighting == ch )
					stop_fighting( victim, FALSE );
			}
		}

		check_improve( ch, sk, TRUE, 1 );
		if(sk == gsk_bow || sk == gsk_crossbow)
			check_improve( ch, gsk_archery, TRUE, 1 );
		WAIT_STATE(ch,beats);
		stop_ranged( ch, FALSE );
		return;
    }

    // ranged firing section
    sprintf( buf, "{YYou fire $p %swards.{x", dir_name[direction] );
    act( buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

    sprintf( buf, "{Y$n fires $p %swards.{x", dir_name[direction] );
    act( buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

    target_room = ch->in_room->exit[direction]->u1.to_room;

    sprintf( buf, "{W%s flies across the room %swards.{x\n\r", obj->short_descr, dir_name[direction] );
    buf[2] = UPPER( buf[2] );

    skill = UMIN(skill, 96);
    /* send the arrow on its course
       we know the victim is on the path since it was searched above
       test skill in each room */
    while (1)
    {
		if ( target_room == victim->in_room )
		    break;

		// It fell down
		if ( number_percent() > (skill + get_curr_stat(ch, STAT_DEX) / 10))
		{
			send_to_char( "Your shot fell short of its target.\n\r", ch );
			if (IS_SET(obj->extra[0], ITEM_EPHEMERAL))
			{
				sprintf( buf, "{Y%s flies in from the %s and hits the ground, and vanishes.{x\n\r", obj->short_descr, dir_name[rev_dir[direction]] );
				extract_obj(obj);
				obj = ch->projectile = NULL;
			}
			else
			{
				sprintf( buf, "{Y%s flies in from the %s and hits the ground.{x\n\r", obj->short_descr, dir_name[rev_dir[direction]] );
				obj_to_room( obj, target_room );
			}
			buf[2] = UPPER( buf[2] );

			room_echo( target_room, buf );

			WAIT_STATE(ch,beats);

			// Decay arrows
			if ( obj != NULL && --obj->condition <= 0 )
			{
				if( !p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_AMMO_BREAK, NULL, 0,0,0,0,0) )
				{
					if (IS_NULLSTR(AMMO(obj)->msg_break))
						sprintf( buf, "{+%s cracks and breaks into pieces.\n\r", obj->short_descr );
					else
						sprintf( buf, AMMO(obj)->msg_break, obj->short_descr);
					room_echo( target_room, buf );
				}

				extract_obj(obj);
				obj = ch->projectile = NULL;
			}

			check_improve( ch, sk, FALSE, 1 );
			if(sk == gsk_bow || sk == gsk_crossbow)
				check_improve( ch, gsk_archery, FALSE, 1 );
			stop_ranged( ch, FALSE );
			return;
		}

		room_echo( target_room, buf );
		target_room = target_room->exit[direction]->u1.to_room;
    }

    // OK, arrow is in the victims room, generate messages.
    sprintf( buf, "{W%s flies in from the %s!{x\n\r", obj->short_descr, dir_name[rev_dir[direction]] );
    buf[2] = UPPER( buf[2] );
    room_echo( target_room, buf );

    //send_to_char( "Your shot had enough range...\n\r", ch );

    /* enough distance, check aim */
    if ( number_percent() > URANGE(0, (skill + get_curr_stat(ch, STAT_DEX) / 5), 90) )
    {
		/* generate miss messages */
		damage( ch, victim, 0, gsk_archery, TYPE_UNDEFINED, dt, TRUE );

		switch( number_range(0, 3) )
		{
	    case 0: act("{Y$p thumps into the ground next to you!{x\n\r", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR); break; 
	    case 1: act("{Y$p skids along the ground next you!{x\n\r", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR); break;
	    case 2: act("{Y$p thuds into the ground next to you!{x\n\r", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR); break;
	    case 3: act("{YYou hear a swish as $p barely misses your head!{x\n\r", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR); break;
		}

		if (IS_SET(obj->extra[0], ITEM_EPHEMERAL))
		{
			act("{B$p vanishes.{x", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
			extract_obj(obj);
			obj = ch->projectile = NULL;
		}
		else
		{
			obj_to_room( obj, target_room );
			if ( --obj->condition <= 0)
			{
				if( !p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_AMMO_BREAK, NULL, 0,0,0,0,0) )
				{
					if (IS_NULLSTR(AMMO(obj)->msg_break))
						sprintf( buf, "{+%s cracks and breaks into pieces.\n\r", obj->short_descr );
					else
						sprintf( buf, AMMO(obj)->msg_break, obj->short_descr);
					room_echo( target_room, buf );
				}

				extract_obj(obj);
				obj = ch->projectile = NULL;
			}
		}
		

		if ( victim->fighting == ch )
	    	stop_fighting( victim, FALSE );

		check_improve( ch, sk, FALSE, 1 );
		if(sk == gsk_bow || sk == gsk_crossbow)
			check_improve( ch, gsk_archery, FALSE, 1 );
		WAIT_STATE(ch,beats);

		stop_ranged( ch, FALSE );
		return;
    }

    // Does shield stop it?
    if (( shield = get_eq_char(ch, WEAR_SHIELD)) == NULL ||
		!check_shield_block_projectile( ch, victim, obj->short_descr, obj ) )
    {
		damage( ch, victim, dam, gsk_archery, TYPE_UNDEFINED, dt, TRUE );
		WAIT_STATE(ch,beats);
    }

    if ( victim != NULL && IS_NPC(victim) && victim->hit > victim->max_hit/5 )
    {
		// Make sure to wake sleeping chars, but not those under the influence of "sleep" spell
		if (!IS_AWAKE(victim))
			do_function(victim, &do_stand, "");

        if (IS_AWAKE(victim)) {
		    hunt_char(victim,ch);
		    act("You hear an angry snarl.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		}
    }

    if (IS_SET(obj->extra[0], ITEM_EPHEMERAL))
	{
		act("{B$p vanishes.{x", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
		extract_obj(obj);
		obj = ch->projectile = NULL;
	}
	else
	{
		if ( victim == NULL )
			obj_to_room( obj, target_room );
		else
		{
			obj_to_char( obj, victim );
			if ( victim->fighting == ch )
				stop_fighting( victim, FALSE );
		}

		// Decay arrows
		if ( --obj->condition <= 0 )
		{
			if( !p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_AMMO_BREAK, NULL, 0,0,0,0,0) )
			{
				if (IS_NULLSTR(AMMO(obj)->msg_break))
					sprintf( buf, "{+%s cracks and breaks into pieces.\n\r", obj->short_descr );
				else
					sprintf( buf, AMMO(obj)->msg_break, obj->short_descr);
				room_echo( target_room, buf );
			}

			extract_obj(obj);
			obj = ch->projectile = NULL;
		}
	}

    check_improve( ch, sk, TRUE, 1 );
    if(sk == gsk_bow || sk == gsk_crossbow)
	check_improve( ch, gsk_archery, TRUE, 1 );
    stop_ranged( ch, FALSE );
}


CHAR_DATA *search_dir_name( CHAR_DATA *ch, char *argument, int direction, int range )
{
    CHAR_DATA *victim = NULL;
    ROOM_INDEX_DATA *in_room = ch->in_room;
    ROOM_INDEX_DATA *search_room = ch->in_room;
    EXIT_DATA *pexit;
    int irange;

    if ( direction < 0 || direction > 9 )
	return NULL;

    if ( argument == NULL || argument[0] == '\0' )
	return NULL;

    for ( irange = 1; irange <= range; irange++ )
    {
	if ( ( pexit = search_room->exit[direction] ) == NULL )
	    break;

	if ( ( search_room = pexit->u1.to_room ) == NULL )
	    break;

	if ( IS_SET( pexit->exit_info, EX_CLOSED ) )
	    break;

	char_from_room( ch );
	char_to_room( ch, search_room );

	if ( ( victim = get_char_room( ch, NULL, argument ) ) != NULL )
	    break;
    }

    char_from_room( ch );
    char_to_room( ch, in_room );

    if (victim != NULL && victim->position == POS_FEIGN)
	return NULL;

    return victim;
}


/*
 * Gets the distance that the victim is away.
 * Used to determine delay.
 */
int get_distance( CHAR_DATA *ch, char *argument, int direction, int range )
{
    CHAR_DATA *victim = NULL;
    ROOM_INDEX_DATA *in_room = ch->in_room;
    ROOM_INDEX_DATA *search_room = ch->in_room;
    EXIT_DATA *pexit;
    int irange;

    if ( direction < 0 || direction > 5 )
	return 0;

    if ( argument == NULL || argument[0] == '\0' )
	return 0;

    for ( irange = 1; irange <= range; irange++ )
    {
	if ( ( pexit = search_room->exit[direction] ) == NULL )
	    break;

	if ( ( search_room = pexit->u1.to_room ) == NULL )
	    break;

	if ( IS_SET( pexit->exit_info, EX_CLOSED ) )
	    break;

	char_from_room( ch );
	char_to_room( ch, search_room );

	if ( ( victim = get_char_room( ch, NULL, argument ) ) != NULL )
	    break;
    }

    char_from_room( ch );
    char_to_room( ch, in_room );
    return irange;
}


void do_throw( CHAR_DATA *ch, char *argument )
{
    CHAR_DATA *victim;
    CHAR_DATA *vch = NULL;
    OBJ_DATA *obj;
    OBJ_DATA *cloud;
    ROOM_INDEX_DATA *in_room = ch->in_room;
    ROOM_INDEX_DATA *target_room;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int dir;
    int skill = 0;
    int dam;
    int ii;
    bool found = FALSE;

    argument = one_argument( argument, arg1 );
    argument = one_argument( argument, arg2 );
    argument = one_argument( argument, arg3 );

    if ( IS_SET(ch->in_room->room_flag[0], ROOM_SAFE ) )
    {
    	send_to_char("You cannot use ranged weapons from safe rooms.\n\r", ch );
	return;
    }

    if ( arg1[0] == '\0' )
    {
	send_to_char("Syntax: throw <weapon> <target> [direction]\n\r"
		     "        throw <smoke bomb>\n\r", ch );
	return;
    }

    obj = get_obj_carry( ch, arg1, ch );

    if ( obj == NULL )
    {
	send_to_char("You don't see that in your inventory.\n\r", ch );
	return;
    }

    if ( obj->item_type != ITEM_WEAPON
    && obj->item_type != ITEM_SMOKE_BOMB )
    {
	send_to_char("That doesn't look too throwable.\n\r" , ch );
	return;
    }

    if (!can_drop_obj(ch, obj, TRUE) || IS_SET(obj->extra[1], ITEM_KEPT)) {
	send_to_char("You can't let go of it.\n\r", ch);
	return;
    }

    if ( arg2[0] == '\0' )
    {
	if ( obj != NULL && obj->item_type == ITEM_SMOKE_BOMB )
	{
	    act("{YYou throw down $p and it explodes into a cloud of noxious "
		"smoke!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR );
	    act("{Y$n throws $p down and it explodes into a cloud of noxious "
		"smoke!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM );
	    act("{YYou choke and gag as the fumes begin to take effect!",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL );

	    if ( obj_index_stinking_cloud == NULL )
	    {
		bug("do_throw: stinking cloud had null index!\n\r", 0);
		return;
	    }

	    for ( cloud = ch->in_room->contents; cloud != NULL;
	          cloud = cloud->next_content )
	    {
		if ( cloud->pIndexData == obj_index_stinking_cloud )
		{
		    found = TRUE;
		    break;
		}
	    }

	    if ( !found )
	    {
		cloud = create_object( obj_index_stinking_cloud,
			0, TRUE );
		cloud->timer = 4;
		obj_to_room( cloud, ch->in_room );
	    }
	    else
		cloud->timer += 4;

	    cloud->level = obj->level;
	    extract_obj( obj );

	    check_improve( ch, gsk_throw, TRUE, 1 );
	}
	else
	{
	    send_to_char("Syntax: throw <weapon> <target>\n\r"
		         "        throw <smoke bomb>", ch );
	}

        return;
    }

    if ( (dir = parse_direction( arg3 ) ) == -1 )
    {
	if ( ( victim = get_char_room( ch, NULL, arg2 ) ) == NULL )
	{
	    for ( ii = 0; ii < 6; ii++ )
	    {
		if ( (vch = search_dir_name( ch, arg2, ii, 1 ) ) != NULL )
		{
		    if ( dir != -1 )
		    {
			send_to_char("Multiple targets like that, which dir?\n\r", ch );
			return;
		    }
		    victim = vch;
		    dir = ii;
		}
	    }
	}
    }
    else
    {
	victim = search_dir_name( ch, arg2, dir, 1 );
    }

    if ( victim == NULL )
    {
	if ( arg3[0] == '\0' )
	    send_to_char("You don't see anyone like that around.\n\r", ch );
	else
	    act("You don't see anyone like that to the $t.", ch, NULL, NULL, NULL, NULL, arg3, NULL, TO_CHAR );
	return;
    }

    if ( is_safe( ch, victim, TRUE ) )
	return;

    WAIT_STATE( ch, gsk_throw->beats );

    /* we have a victim and a dir.  start the missile off. */
    skill = get_skill(ch, gsk_throw);
    if ( IS_ELF( ch ) )
	skill += 5;

    if ( !IS_NPC(ch) && get_current_class(ch) == gcl_ranger )
		skill += 5;

    if ( number_percent() > (skill + get_curr_stat( ch, STAT_DEX )/2) )
    {
	send_to_char("You fumble your throw.\n\r", ch );
	act("$n attempts to throw $p, but fumbles.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

            p_give_trigger( NULL, obj, NULL, ch, obj, TRIG_THROW ,0,0,0,0,0);
            p_give_trigger( NULL, NULL, ch->in_room, ch, obj, TRIG_THROW ,0,0,0,0,0);

	obj_from_char( obj );
	obj_to_room( obj, ch->in_room );
	check_improve( ch, gsk_throw, FALSE, 1 );
	return;
    }

    dam = dice( obj->value[1], obj->value[2] )
	+ 40*log(GET_DAMROLL( ch ));

    if ( ch->in_room == victim->in_room )
    {
	act("$n throws $s $p at you!", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
	act("$N throws $S $p at $n!",  victim, ch, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
	act("You throw $p at $N!",     ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	if ( damage( ch, victim, dam, gsk_throw, TYPE_UNDEFINED, obj->value[3], TRUE ) == FALSE )
	    victim = NULL;

	obj_from_char( obj );
	if ( victim == NULL )
	    obj_to_room( obj, ch->in_room );
	else
	{
	    obj_to_char( obj, victim );
	    if ( victim->fighting == ch )
		stop_fighting( victim, FALSE );
	}
            p_give_trigger( NULL, obj, NULL, victim, obj, TRIG_THROW ,0,0,0,0,0);
            p_give_trigger( NULL, NULL, ch->in_room, victim, obj, TRIG_THROW ,0,0,0,0,0);


	return;
    }

    /* ranged firing section */
    {
	sprintf( buf, "{YYou throw $p{Y %swards.{x", dir_name[dir] );
	act( buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	sprintf( buf, "{Y$n throw $s $p{Y %swards.{x", dir_name[dir] );
	act( buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	target_room = ch->in_room->exit[dir]->u1.to_room;

	sprintf( buf, "{W$p flies across the room %swards.{x\n\r", dir_name[dir] );

	/* send the obj on it's course */
	/* we know the victim is on the path since it was searched above */
	/* test skill in each room */
	while ( 1 )
	{
	    if ( target_room == victim->in_room )
		break;

	    if ( number_percent() > (skill + get_curr_stat( ch, STAT_DEX )/5) )
	    {
		send_to_char( "Your throw fell short of its target.\n\r", ch );
		act( "{W$p flies in from the $T and skitters along the ground.{x",
		    ch, NULL, NULL, obj, NULL, NULL, dir_name[rev_dir[dir]], TO_ROOM);

		obj_from_char( obj );
		obj_to_room( obj, target_room );

		    p_give_trigger( NULL, obj, NULL, ch, obj, TRIG_THROW ,0,0,0,0,0);
		    p_give_trigger( NULL, NULL, in_room, ch, obj, TRIG_THROW ,0,0,0,0,0);

		return;
	    }

	    target_room = ch->in_room->exit[dir]->u1.to_room;
	    room_echo( target_room, buf );
	}

	if ( str_cmp( dir_name[rev_dir[dir]], "up" )
	&& str_cmp( dir_name[rev_dir[dir]], "down" ) )
	{
	    act( "{W$p flies in from the $T!{x", ch, NULL, NULL, obj, NULL, NULL, dir_name[rev_dir[dir]],  TO_ROOM );
	}
	else
	{
	    act( "{W$p flies in from $Twards!{x", ch, NULL, NULL, obj, NULL, NULL, dir_name[rev_dir[dir]], TO_ROOM);
	}

	//send_to_char( "Your throw had enough range...\n\r", ch );

	/* enough distance, check aim */
	if ( number_percent() > (skill + get_curr_stat( ch, STAT_DEX )/5) )
	{
	    /* generate miss messages */
            if (ch->in_room == victim->in_room)
            {
	        damage( ch, victim, 0, gsk_throw, TYPE_UNDEFINED, obj->value[3], TRUE );
            }
	    else
            {
		switch( number_range(0, 2) )
                {
		    case 0:
		        act("$p skitters across the ground near you!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		        act("$p skitters across the ground near $n!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
                        break;
                    case 1:
		        act("$p bounces in out of nowhere!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		        act("$p bounces in out of nowhere!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
                        break;
                    case 2:
		        act("$p crashes to the ground next to you!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		        act("$p crashes to the ground next to $n!", victim, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
                        break;
		}
            }

	    obj_from_char( obj );
	    obj_to_room( obj, target_room );
	    if ( victim->fighting == ch )
			stop_fighting( victim, FALSE );

		p_give_trigger( NULL, obj, NULL, ch, obj, TRIG_THROW ,0,0,0,0,0);
		p_give_trigger( NULL, NULL, in_room, ch, obj, TRIG_THROW ,0,0,0,0,0);

	    check_improve( ch, gsk_throw, FALSE, 1 );
	    return;
	}

	/* do damage and generate messages */
	if ( damage( ch, victim, dam, gsk_throw, TYPE_UNDEFINED , attack_table[obj->value[3]].damage,  TRUE ) == FALSE )
	{
	    victim = NULL;
	}
	obj_from_char( obj );
	if ( victim == NULL )
	{
	    obj_to_room( obj, target_room );
	}
	else
	{
	    obj_to_char( obj, victim );
	    if ( victim->fighting == ch )
		stop_fighting( victim, FALSE );
	}

	if ( victim != NULL && IS_NPC(victim) && victim->hit > victim->max_hit/5 )
	{
	    hunt_char(victim,ch);
	    act("You hear an angry snarl.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
	}

        if ( victim )
        	p_give_trigger( NULL, obj, NULL, victim, obj, TRIG_THROW ,0,0,0,0,0);
	p_give_trigger( NULL, NULL, in_room, ch, obj, TRIG_THROW ,0,0,0,0,0);

	check_improve( ch, gsk_throw, TRUE, 1 );
    }

    return;
}


// Returns % ranged skill with all the trimmings.
int get_ranged_skill(CHAR_DATA *ch)
{
    OBJ_DATA *bow;
	SKILL_DATA *sk = NULL;
    int skill;
    char buf[MSL], buf2[MSL];

    skill = get_curr_stat(ch, STAT_DEX) / 8;
    sprintf(buf,"%s gets +%d%% from dex, ",ch->name, get_curr_stat(ch, STAT_DEX)/8);

    // @@@NIB : 20070128 ----------
    if ( ( bow = ch->projectile_weapon) ) {
	switch(WEAPON(bow)->weapon_class) {
	default:
	case WEAPON_EXOTIC:		sk = gsk_exotic; break;
	case WEAPON_BOW:		sk = gsk_bow; break;
	case WEAPON_CROSSBOW:	sk = gsk_crossbow; break;
	case WEAPON_HARPOON:	sk = gsk_harpooning; break;
	case WEAPON_BLOWGUN:	sk = gsk_blowgun; break;
	}
	skill += get_skill(ch, sk);
    }
    // @@@NIB : 20070128 ----------

    sprintf( buf2, " +%d%% from skill, ", skill ); strcat(buf,buf2);

    if ( IS_ELF( ch ) ) {
	skill += 5;
    }

    if (!IS_NPC(ch)) {
	    if ( sk == gsk_blowgun ) {
	        // @@@NIB : 20070128 : if an assassin/ninja, they get a +5% accuracy
	        //	with blowguns
		// TODO: Make this a trait (ninja + assassin synergy?)
		if (get_current_class(ch) == gcl_ninja)
		    skill += 5;

		// TODO: Make this a trait
	    } else if ( get_current_class(ch) == gcl_ranger )
		skill += 5;
    }

    // @@@NIB : 20070128
    if(sk == gsk_bow || sk == gsk_crossbow) {
	skill += get_skill( ch, gsk_archery) / 5;
	sprintf(buf2, "+%d%% from archery, total skill is %d%%\n\r", get_skill(ch, gsk_archery)/5, skill); strcat(buf, buf2); //send_to_char(buf,ch);
    }

    return skill;
}
