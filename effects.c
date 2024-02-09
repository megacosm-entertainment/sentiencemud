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
// VIZZWILDS
#include "wilds.h"


void acid_effect(void *vo, int level, int dam, int target)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    if (target == TARGET_ROOM) /* nail objects on the floor */
    {
	ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *) vo;
	OBJ_DATA *obj, *obj_next;

	for (obj = room->contents; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    acid_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
    }

    if (target == TARGET_CHAR)  /* do the effect on a victim */
    {
        victim = (CHAR_DATA *) vo;

	/* let's toast some gear */
	for (obj = victim->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    acid_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
    }

    if (target == TARGET_OBJ) /* toast an object */
    {
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	OBJ_DATA *t_obj,*n_obj;
	int chance;
	char *msg;

	if (IS_OBJ_STAT(obj,ITEM_BURN_PROOF)
	||  IS_OBJ_STAT(obj,ITEM_NOPURGE)
	||  number_range(0,4) == 0)
	    return;

	chance = level / 6 + dam / 12;

	if (chance > 25)
	    chance = (chance - 25) / 2 + 25;
	if (chance > 50)
	    chance = (chance - 50) / 2 + 50;

	if (IS_OBJ_STAT(obj,ITEM_BLESS))
	    chance -= 5;

	chance -= obj->level * 2;

	switch (obj->item_type)
	{
	    default:
		return;
	    case ITEM_CONTAINER:
	    case ITEM_WEAPON_CONTAINER:
	    case ITEM_CORPSE_PC:
	    case ITEM_CORPSE_NPC:
		msg = "$p fumes and dissolves.";
		break;
	    case ITEM_ARMOUR:
		msg = "$p is pitted and etched.";
		break;
	    case ITEM_CLOTHING:
		msg = "$p is corroded into scrap.";
	 	break;
	    case ITEM_STAFF:
	    case ITEM_WAND:
		chance -= 10;
		msg = "$p corrodes and breaks.";
		break;
	    case ITEM_SCROLL:
		chance += 10;
		msg = "$p is burned into waste.";
		break;
	}

	chance = URANGE(1,chance,95);

	if (number_percent() > chance)
	    return;

	if (obj->carried_by != NULL)
	    act(msg,obj->carried_by, NULL, NULL, obj, NULL, NULL,NULL,TO_ALL);
	else if (obj->in_room != NULL && obj->in_room->people != NULL)
	    act(msg,obj->in_room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

	if (obj->item_type == ITEM_ARMOUR)  /* etch it */
	{
	    int i;

            if (obj->carried_by != NULL && obj->wear_loc != WEAR_NONE)
	    {
                for (i = 0; i < 4; i++)
                    obj->carried_by->armour[i] += 1;

		if ( obj->condition >= 2 )
		{
		    --obj->condition;
		}
	    }
            return;
	}

	/* get rid of the object */
	if (obj->contains)  /* dump contents */
	{
	    ROOM_INDEX_DATA *to_room;
	    to_room = obj_room(obj);
	    for (t_obj = obj->contains; t_obj != NULL; t_obj = n_obj)
	    {
		n_obj = t_obj->next_content;
		obj_from_obj(t_obj);
		if (to_room)
		    obj_to_room(t_obj, to_room);
		else
		{
		    extract_obj(t_obj);
		    continue;
		}

		acid_effect(t_obj,level/2,dam/2,TARGET_OBJ);
	    }
 	}

	extract_obj(obj);
	return;
    }
}


void cold_effect(void *vo, int level, int dam, int target)
{
    if (target == TARGET_ROOM) /* nail objects on the floor */
    {
        ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *) vo;
        OBJ_DATA *obj, *obj_next;

        for (obj = room->contents; obj != NULL; obj = obj_next)
        {
            obj_next = obj->next_content;
            cold_effect(obj,level,dam,TARGET_OBJ);
        }
        return;
    }

    if (target == TARGET_CHAR) /* whack a character */
    {
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	OBJ_DATA *obj, *obj_next;

	/* chill touch effect */
	if ( number_percent() < 15 && !saves_spell(level, victim,DAM_COLD) )
	{
	    AFFECT_DATA af;
memset(&af,0,sizeof(af));
            act("{C$n turns blue and shivers.{x",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
	    act("{CA chill sinks deep into your bones.{x",victim,NULL, NULL, NULL, NULL, NULL,NULL,TO_CHAR);
            af.where     = TO_AFFECTS;
            af.group     = AFFGROUP_BIOLOGICAL;
            af.type      = gsn_chill_touch;
            af.level     = level;
            af.duration  = 6;
            af.location  = APPLY_STR;
            af.modifier  = -1;
            af.bitvector = 0;
	    af.bitvector2 = 0;
	af.slot	= WEAR_NONE;
            affect_join( victim, &af );
	}

	/* hunger! (warmth sucked out */
	if (!IS_NPC(victim))
	    gain_condition(victim,COND_HUNGER,dam/20);

	/* let's toast some gear */
	for (obj = victim->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    cold_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
   }

   if (target == TARGET_OBJ) /* toast an object */
   {
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	int chance;
	char *msg;

	if (IS_OBJ_STAT(obj,ITEM_FREEZE_PROOF)
	||  IS_OBJ_STAT(obj,ITEM_NOPURGE)
	||  number_range(0,4) == 0)
	    return;

	chance = level / 4 + dam / 10;

	if (chance > 25)
	    chance = (chance - 25) / 2 + 25;
	if (chance > 50)
	    chance = (chance - 50) / 2 + 50;

	if (IS_OBJ_STAT(obj,ITEM_BLESS))
	    chance -= 5;

 	chance -= obj->level * 2;

	switch(obj->item_type)
	{
	    default:
		return;
	    case ITEM_POTION:
		msg = "{C$p freezes and shatters!{x";
		chance += 25;
		break;
	    case ITEM_DRINK_CON:
		msg = "{C$p freezes and shatters!{x";
		chance += 5;
		break;
	}

	chance = URANGE(5,chance,95);

	if (number_percent() > chance)
	    return;

	if (obj->carried_by != NULL)
	    act(msg,obj->carried_by, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
	else if (obj->in_room != NULL && obj->in_room->people != NULL)
	    act(msg,obj->in_room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

	extract_obj(obj);
	return;
    }
}


void fire_effect(void *vo, int level, int dam, int target)
{
    if (target == TARGET_ROOM)  /* nail objects on the floor */
    {
	ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *) vo;
	OBJ_DATA *obj, *obj_next;

	for (obj = room->contents; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    fire_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
    }

    if (target == TARGET_CHAR)   /* do the effect on a victim */
    {
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	OBJ_DATA *obj, *obj_next;
	AFFECT_DATA *pAf;

	/* chance of blindness */
	if (!IS_AFFECTED(victim,AFF_BLIND)
	&& number_percent() < 15 )
	{
            AFFECT_DATA af;
memset(&af,0,sizeof(af));
            act("{D$n is blinded by smoke!{x",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
            act("{RYour eyes tear up from smoke...you can't see a thing!{x",
		victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_CHAR);
            af.where        = TO_AFFECTS;
            af.group        = AFFGROUP_PHYSICAL;
            af.type         = gsn_fire_breath;
            af.level        = level;
            af.duration     = 1;//number_range(0,level/10);
            af.location     = APPLY_HITROLL;
            af.modifier     = (-4 - number_range(0,level/10));
            af.bitvector    = AFF_BLIND;
		    af.bitvector2   = 0;
			af.slot	= WEAR_NONE;

            affect_to_char(victim,&af);
	}

 	for ( pAf = victim->affected; pAf != NULL; pAf = pAf->next )
	{
	    if ( pAf->type == gsn_chill_touch && number_percent() < 25)
	    {
		affect_remove( victim, pAf );
		send_to_char("You stop shivering and your muscles warm up.\n\r", victim );
		act("$n stops shivering as $s muscles warm up.",
		    victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
		break;
	    }
	}

	/* let's toast some gear! */
	for (obj = victim->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;

	    fire_effect(obj,level,dam,TARGET_OBJ);
        }
	return;
    }

    if (target == TARGET_OBJ)  /* toast an object */
    {
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	OBJ_DATA *t_obj,*n_obj;
	int chance;
	char *msg;

    	if (IS_OBJ_STAT(obj,ITEM_BURN_PROOF)
        || IS_OBJ_STAT(obj,ITEM_NOPURGE)
	|| number_range(0,4) == 0)
            return;

        chance = level / 4 + dam / 10;

        if (IS_OBJ_STAT(obj,ITEM_BLESS))
            chance -= 10;

        chance -= obj->level/8;

        switch ( obj->item_type )
        {
        default:
	    return;
        case ITEM_CONTAINER:
            msg = "{R$p ignites and burns!{x";
            break;
        case ITEM_POTION:
            chance += 25;
            msg = "{B$p bubbles and boils!{x";
            break;
        case ITEM_SCROLL:
            chance += 50;
            msg = "{R$p crackles and burns!{x";
            break;
        case ITEM_MAP:
            chance += 50;
            msg = "{R$p crackles and burns!{x";
            break;
        case ITEM_STAFF:
            chance += 10;
            msg = "{D$p smokes and chars!{x";
            break;
        case ITEM_WAND:
            msg = "{Y$p sparks and sputters!{x";
            break;
        case ITEM_FOOD:
            msg = "{D$p blackens and crisps!{x";
            break;
        case ITEM_PILL:
            msg = "{Y$p melts and drips!{x";
            break;
        case ITEM_TATTOO:
            msg = "{Y$p dries and flakes away!{x";
            break;
	case ITEM_SMOKE_BOMB:
	    msg = "{Y$p violently explodes with a loud {R*BANG*{x";
	    break;
        }

        chance = URANGE(5,chance,95);

        if (number_percent() > chance)
            return;

	if (obj->carried_by != NULL)
            act( msg, obj->carried_by, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL );
	else if (obj->in_room != NULL && obj->in_room->people != NULL)
	    act(msg,obj->in_room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

        if (obj->contains)
        {
	    ROOM_INDEX_DATA *to_room;
            /* dump the contents */
            for (t_obj = obj->contains; t_obj != NULL; t_obj = n_obj)
            {
                n_obj = t_obj->next_content;
		if ((to_room = obj_room(obj)))
		    obj_to_room(t_obj, to_room);
		else
		{
		    extract_obj(t_obj);
		    continue;
		}
		fire_effect(t_obj,level/2,dam/2,TARGET_OBJ);
            }
        }

	if ( obj->item_type == ITEM_SMOKE_BOMB )
	{
	    CHAR_DATA *vch;
	    ROOM_INDEX_DATA *room;

	    if ( obj->in_room != NULL )
		room = obj->in_room;
	    else
	    if ( obj->carried_by != NULL )
		room = obj->carried_by->in_room;
	    else
	    {
		bug("fire_effect: smoke bomb had no room", 0 );
		return;
	    }

	    for ( vch = room->people; vch != NULL; vch = vch->next_in_room )
	    {
		damage( vch, vch, number_range(20,50), 0, DAM_FIRE, false );
	    }
	}

        extract_obj( obj );
	return;
    }
}


void poison_effect(void *vo,int level, int dam, int target)
{
    if (target == TARGET_ROOM)  /* nail objects on the floor */
    {
        ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *) vo;
        OBJ_DATA *obj, *obj_next;

        for (obj = room->contents; obj != NULL; obj = obj_next)
        {
            obj_next = obj->next_content;
            poison_effect(obj,level,dam,TARGET_OBJ);
        }
        return;
    }

    if (target == TARGET_CHAR)   /* do the effect on a victim */
    {
        CHAR_DATA *victim = (CHAR_DATA *) vo;
        OBJ_DATA *obj, *obj_next;

	/* chance of poisoning */
        if (number_percent() < 15 && check_immune(victim, DAM_POISON) != IS_IMMUNE)
        {
	    AFFECT_DATA af;
memset(&af,0,sizeof(af));
            send_to_char("{GYou feel poison coursing through your veins.{x\n\r",
                victim);
            act("{G$n looks very ill.{x",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);

            af.where     = TO_AFFECTS;
            af.group     = AFFGROUP_BIOLOGICAL;
            af.type      = gsn_poison;
            af.level     = level;
            af.duration  = level / 2;
            af.location  = APPLY_STR;
            af.modifier  = -1;
            af.bitvector = AFF_POISON;
	    af.bitvector2 = 0;
            affect_join( victim, &af );
        }

	/* equipment */
	for (obj = victim->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    poison_effect(obj,level,dam,TARGET_OBJ);
	}

	return;
    }

    if (target == TARGET_OBJ)  /* do some poisoning */
    {
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	int chance;

	if (IS_OBJ_STAT(obj,ITEM_BURN_PROOF)
  	||  IS_OBJ_STAT(obj,ITEM_BLESS)
	||  number_range(0,4) == 0)
	    return;

	chance = level / 4 + dam / 10;
	if (chance > 25)
	    chance = (chance - 25) / 2 + 25;
	if (chance > 50)
	    chance = (chance - 50) / 2 + 50;

	chance -= obj->level * 2;

	switch (obj->item_type)
	{
	    default:
		return;
	    case ITEM_FOOD:
		break;
	    case ITEM_DRINK_CON:
		if (obj->value[0] == obj->value[1])
		    return;
		break;
	}

	chance = URANGE(5,chance,95);

	if (number_percent() > chance)
	    return;

	obj->value[3] = 1;
	return;
    }
}


void shock_effect(void *vo,int level, int dam, int target)
{
    if (target == TARGET_ROOM)
    {
	ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *) vo;
	OBJ_DATA *obj, *obj_next;

	for (obj = room->contents; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    shock_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
    }

    if (target == TARGET_CHAR)
    {
	CHAR_DATA *victim = (CHAR_DATA *) vo;
	OBJ_DATA *obj, *obj_next;

	/* daze and confused? */
	if ( number_percent() < 10 && !saves_spell(level, victim,DAM_LIGHTNING))
	{
	    send_to_char("{YYour muscles stop responding.{x\n\r",victim);
	    DAZE_STATE(victim, 12);//UMAX(12,level/4 + dam/20));
	}

	/* toast some gear */
	for (obj = victim->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;
	    shock_effect(obj,level,dam,TARGET_OBJ);
	}
	return;
    }

    if (target == TARGET_OBJ)
    {
	OBJ_DATA *obj = (OBJ_DATA *) vo;
	int chance;
	char *msg;

	if (IS_OBJ_STAT(obj,ITEM_BURN_PROOF)
	||  IS_OBJ_STAT(obj,ITEM_NOPURGE)
	||  number_range(0,4) == 0)
	    return;

	chance = level / 4 + dam / 10;

	if (chance > 25)
	    chance = (chance - 25) / 2 + 25;
	if (chance > 50)
	    chance = (chance - 50) /2 + 50;

	if (IS_OBJ_STAT(obj,ITEM_BLESS))
	    chance -= 5;

 	chance -= obj->level * 2;

	switch(obj->item_type)
	{
	    default:
		return;
	   case ITEM_WAND:
	   case ITEM_STAFF:
		chance += 10;
		msg = "{R$p overloads and explodes!{x";
		break;
	   case ITEM_JEWELRY:
		chance -= 10;
		msg = "{Y$p is fused into a worthless lump.{x";
	}

	chance = URANGE(5,chance,95);

	if (number_percent() > chance)
	    return;

	if (obj->carried_by != NULL)
	    act(msg,obj->carried_by, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
	else if (obj->in_room != NULL && obj->in_room->people != NULL)
	    act(msg,obj->in_room->people, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

	extract_obj(obj);
	return;
    }
}

void damage_vampires( CHAR_DATA *ch, int dam )
{
	int chance;

    dam -= get_age(ch) * 2;
    chance = get_skill( ch, gsn_temperance );
    if (chance > 0) {
	if ( chance < 6 ) dam -= 5;
	else if ( chance < 30 ) dam -= 20;
	else if ( chance < 50 ) dam = dam * 5/6;
	else if ( chance < 75 ) dam = dam * 3/4;
	else if ( chance < 80 ) dam = dam * 2/3;
	else if ( chance < 90 ) dam = dam * 3/5;
	else dam = dam / 2;
    }

    check_improve( ch, gsn_temperance, true, 8 );

    dam = UMAX( dam, 25 );

    vamp_sun_message( ch, dam/4 );

    if ( ch->mana - dam/4 < 0 )
    	ch->mana = 0;
    else
        ch->mana -= dam/4;

    if ( ch->move - dam/4 < 0 )
    	ch->move = 0;
    else
        ch->move -= dam/4;

    if ( ch->hit - dam/4 < 1 )
	raw_kill( ch, false, false, RAWKILL_INCINERATE );
    else
	ch->hit -= dam/4;
}

void hurt_vampires( CHAR_DATA *ch)
{
    int dam;

    if ( !IS_VAMPIRE(ch)
    || IS_IMMORTAL(ch)
    || !IS_OUTSIDE(ch)
    || IN_NETHERWORLD(ch)
    || (time_info.hour < 6 || time_info.hour > 16 ))
	return;

    switch ( weather_info.sky )
    {
	case SKY_CLOUDLESS:
	    dam = ch->max_hit/3;
	    break;
	case SKY_CLOUDY:
	    dam = ch->max_hit/4;
	    break;
	case SKY_RAINING:
	    dam = ch->max_hit/5;
	    break;
	default:
	    dam = ch->max_hit/6;
	    break;
    }

    if ( time_info.hour >= 9 && time_info.hour < 4 )
	dam *= 2;

    if ( time_info.hour >= 16 )
	dam /= 2;

    if (IS_AFFECTED(ch, gsn_stone_skin))
	dam /= 2;

    damage_vampires(ch,dam);
}


void vamp_sun_message( CHAR_DATA *ch, int dam )
{
    if ( dam == 0 )
    	return;

    if ( ch->hit - dam < 1 )
    {
	act("Your head explodes as your body burns in the sunlight.",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
	act("$n's head explodes as $s body burns in the sunlight.",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	return;
    }

    if ( dam / ch->max_hit < 20 )
    {
	act("{R$n suffers as the sun's rays strike $s flesh.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act("{YYou suffer from the light of the sun.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    }
    else if ( dam / ch->max_hit < 25 )
    {
	act("{R$n writhes in pain inflicted by the sunlight.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act("{YYou writhe in pain from the light of the sun.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    }
    else if ( dam / ch->max_hit < 33 )
    {
	act("{R$n screams in pain as $s flesh burns in the sunlight.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act("{YYou writhe in agony as your flesh burns in the sun.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    }
    else
    {
	act( "{R$n shrieks in agony as $s flesh dissolves in the sunlight.{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act("{YYou writhe in agony as your flesh dissolves in the direct sunlight.\n\r{x",
		ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    }
}

void toxic_fumes_effect(CHAR_DATA *victim,CHAR_DATA *ch)
{
    int level, duration;
    AFFECT_DATA af;

    level = ch ? ch->tot_level : 120;
    duration = ch ? URANGE(1,level,5) : -1;

    if (ch && saves_spell(level, victim,DAM_POISON) && saves_spell(level, victim,DAM_DISEASE))
    {
	act("$n inhales the toxic fumes, but seems to ignore them.",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
	send_to_char("You breathe in the toxic fumes, but they have no affect.\n\r",victim);
	return;
    }

    if(MOUNTED(victim)) {
	send_to_char("You breathe in the toxic fumes, following from your mount with illness.\n\r", victim);
	act("$n falls from $s mount after inhaling the toxic fumes.",victim, NULL, NULL, NULL, NULL,NULL,NULL,TO_ROOM);
	interpret(victim,"dismount");
    } else {
	send_to_char("You collapse with illness from breathing in the toxic fumes.\n\r", victim);
	act("$n collapses from inhaling the toxic fumes.",victim,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
    }
    // Interrupt what they are doing too!
    interpret(victim,"cringe");
    victim->position = POS_RESTING;
	memset(&af,0,sizeof(af));
	af.slot	= WEAR_NONE;
	af.bitvector = 0;
	af.modifier = 0;
	af.group = AFFGROUP_BIOLOGICAL;

    	if(!IS_SET(victim->imm_flags,IMM_POISON)) {
		af.bitvector |= AFF_POISON;
		af.modifier -= level / 30;
	}
    	if(!IS_SET(victim->imm_flags,IMM_DISEASE)) {
		af.bitvector |= AFF_PLAGUE;
		af.modifier -= level / 30;
	}

	if(af.bitvector == (AFF_POISON|AFF_PLAGUE) && number_range(0,999) < level)
		af.modifier -= number_range(1,3);

	if(af.bitvector) {
		af.where     = TO_AFFECTS;
		af.type      = gsn_toxic_fumes;
		af.level     = level;
		af.duration  = duration;
		af.location  = APPLY_STR;
		af.bitvector2 = 0;
		affect_to_char(victim, &af);
	}

	af.where     = TO_AFFECTS;
	af.type      = gsn_toxic_fumes;
	af.level     = level;
	af.duration  = duration;
	af.location  = APPLY_MOVE;
	af.modifier  = -(level / 8);
	af.bitvector = 0;
	af.bitvector2 = AFF2_FATIGUE;
	affect_to_char(victim, &af);
}
