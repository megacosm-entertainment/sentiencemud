/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"

bool __isspell_valid(CHAR_DATA *ch, OBJ_DATA *obj, SKILL_ENTRY *spell, int pretrigger, int trigger, char *token_message);
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);
bool has_inks(CHAR_DATA *ch, int *need, bool show);
bool are_spell_targets_compatible(SKILL_ENTRY *sa, SKILL_ENTRY *sb);
bool has_inks(CHAR_DATA *ch, int *need, bool show);
void extract_inks(CHAR_DATA *ch, int *need);
bool show_help(CHAR_DATA *ch, char *argument);

/* Used not only for depositing of pneuma but for the depositing of GQ items*/
void do_deposit(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    ROOM_INDEX_DATA *room;
    CHAR_DATA *mob;
    GQ_OBJ_DATA *gq_obj;
    int i = 0;
    bool found = FALSE;
    int qp = 0;
    int prac = 0;
    long exp = 0;
    int silver = 0;
    int gold = 0;

    room = ch->in_room;

    /* GQ section */
    for (mob = room->people; mob != NULL; mob = mob->next_in_room)
    {
	if (IS_SET(mob->act[1], ACT2_GQ_MASTER))
	    break;
    }

    if (mob != NULL && global == TRUE)
    {
	for (obj = ch->carrying; obj != NULL; obj = obj_next)
	{
	    obj_next = obj->next_content;

	    for (gq_obj = global_quest.objects; gq_obj != NULL;
		    gq_obj = gq_obj->next)
	    {
		if (obj->pIndexData->area->uid == gq_obj->wnum_load.auid && obj->pIndexData->vnum == gq_obj->wnum_load.vnum)
		{
		    found = TRUE;
		    qp += gq_obj->qp_reward;
		    prac += gq_obj->prac_reward;
		    exp += gq_obj->exp_reward;
		    silver += gq_obj->silver_reward;
		    gold += gq_obj->gold_reward;
		    extract_obj(obj);
		}
	    }
	}

	if (found)
	{
	    sprintf(buf, "Thank you, %s!", pers(ch, mob));
	    do_say(mob, buf);

	    sprintf(buf, "{WYou gain %d quest points, %d practices, and %ld experience points!{x\n\r",
		    qp, prac, exp);
	    send_to_char(buf, ch);

	    sprintf(buf, "$N hands you %d silver coins and %d gold coins.",
		    silver, gold);
	    act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	    ch->questpoints += qp;
	    ch->practice += prac;
	    gain_exp(ch, NULL, exp);
	    ch->silver += silver;
	    ch->gold += gold;
	}
	else
	{
	    sprintf(buf, "You have nothing to deposit, %s.", pers(ch, mob));
	    do_say(mob, buf);
	}
    }

    /* bottle section*/
    found = FALSE;
    for (mob = room->people; mob != NULL; mob = mob->next_in_room)
    {
	if (!IS_NPC(mob))
	    continue;

	if (ch->alignment > -250 && ch->alignment < 250 && mob->pIndexData == mob_index_soul_deposit_neutral)
	    break;

	if (ch->alignment <= -250 && mob->pIndexData == mob_index_soul_deposit_evil)
	    break;

	if (ch->alignment >= 250 && mob->pIndexData == mob_index_soul_deposit_good)
	    break;
    }

    if (mob == NULL)
	return;

    for (obj = ch->carrying; obj != NULL; obj = obj_next)
    {
	obj_next = obj->next_content;

	if (obj->pIndexData == obj_index_bottled_soul)
	{
	    found = TRUE;
	    extract_obj(obj);
	    i++;
	}
    }

    if (found)
    {
	sprintf(buf, "You have deposited {Y%d{x bottled souls with %s!", i,
		mob->short_descr);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	sprintf(buf, "$n deposits %d bottled souls.", i);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	if (boost_table[BOOST_PNEUMA].boost != 100)
	{
	    send_to_char("{WPNEUMA boost!{x\n\r", ch);
	    ch->pneuma += (i * boost_table[BOOST_PNEUMA].boost)/100;
	}
	else
	    ch->pneuma += i;
    }
    else
	send_to_char("You don't have any bottled souls to deposit.\n\r", ch);
}



// TODO: make VERB trigger on the hammer
/* could be used for various things in the future, atm just for crystal hammers*/
void do_strike(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    OBJ_DATA *obj_struck;
    char buf[MSL];

    if ((obj = get_eq_char(ch, WEAR_HOLD)) == NULL)
    {
        send_to_char("You don't have anything to strike with.\n\r", ch);
		return;
    }
    else
    {
		if (obj->pIndexData != obj_index_glass_hammer)
		{
			act("You can't accomplish anything with $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}
    }

    if (argument[0] == '\0')
    {
		send_to_char("Strike what?\n\r", ch);
		return;
    }

    if ((obj_struck = get_obj_carry(ch, argument, ch)) == NULL)
    {
		send_to_char("You don't have that item.\n\r", ch);
		return;
    }

    if (obj_struck == obj || obj_struck->timer > 0)
    {
		send_to_char("You really wouldn't want to do that.\n\r", ch);
		return;
    }

    /* Allow hammers to be used for instruments as well - Tieryo*/
    if (!IS_WEAPON(obj_struck) && obj_struck->item_type != ITEM_ARMOUR && !IS_INSTRUMENT(obj_struck))
    {
		send_to_char("This item can only be used on weapons and armour.\n\r", ch);
		return;
    }

    if (obj_struck->fragility == OBJ_FRAGILE_SOLID)
    {
	act("That would be pointless as $p does not decay.", ch, NULL, NULL, obj_struck, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    sprintf(buf, "{YYou strike $p{Y with %s and it flashes with a bright light!{x",
    	obj->short_descr);
    act(buf, ch, NULL, NULL, obj_struck, NULL, NULL, NULL, TO_CHAR);
    sprintf(buf, "{Y$n strikes $p{Y with %s and it flashes with a bright light!{x",
        obj->short_descr);
    act(buf, ch, NULL, NULL, obj_struck, NULL, NULL, NULL, TO_ROOM);

    obj_struck->condition = 100;
    obj_struck->times_fixed = 0;

    act("$p shatters into a million pieces!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n's $p shatters into a million pieces!", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    extract_obj(obj);
}


// TODO: REWORK THIS ENTIRE THING
void do_lore(CHAR_DATA *ch, char *argument)
{
#if 0
    CHAR_DATA *mob;
    OBJ_DATA *scroll;
    OBJ_INDEX_DATA *objIndex;
    EXTRA_DESCR_DATA *ed;
    char arg[MSL];
    char arg2[MSL];
    char arg3[MSL];
    char arg4[MSL];
    char buf[MSL];
    char buf2[MSL];
    char sd[50];
    char aname[75];
    int iHash;
    int min;
    int max;
    int i;
    int cost;
    long type;

    mob = NULL;
    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
	if (IS_SET(mob->act[1], ACT2_LOREMASTER))
	    break;
    }

    if (mob == NULL)
    {
	send_to_char("There is nobody that can do that for you here.\n\r", ch);
	return;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3); /* item type*/
    argument = one_argument(argument, arg4); /* wear-loc OR weapon class*/

    if (arg[0] == '\0' || arg2[0] == '\0'
    || !is_number(arg) || !is_number(arg2))
    {
	act("{R$N tells you 'To look up information on an item type 'lore <minimum level> <maximum level> <item type> <wear location/weapon class>'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$N tells you 'The item type and wear location/weapon class are optional.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    min = atoi(arg);
    max = atoi(arg2);
    if (min < 1 || max > 120 || min > max)
    {
	act("{R$N tells you 'There is no such item. The level range is 1-120.'{x",
		ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    cost = 2000 + (max - min) * 10;
    if (ch->gold * 100 + ch->silver < cost)
    {
	sprintf(buf,
	"{R$N tells you 'You don't have enough money for my services. You need %d silver.'{x", cost);
	act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if ((max - min) > 15)
    {
	act("{R$N tells you, 'Sorry $n, that's too wide a range! I would be up digging through the books all night!'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    type = flag_value(type_flags, arg3);
    if (arg3[0] != '\0' && type == NO_FLAG)
    {
	act("{R$N tells you, 'I've never heard of that type of item.'{x",
	    ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$N tells you, 'I mainly know about armour, weapons, ranged weapons, lights, containers, artifacts, and musical instruments.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (arg4[0] != '\0'
    &&   type != ITEM_WEAPON && type != ITEM_RANGED_WEAPON
    &&   flag_value(wear_flags, arg4) == NO_FLAG)
    {
	act("{R$N tells you, 'I've never heard of such a place to wear an item.'{x",
	    ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("{R$N tells you, 'The valid arguments are: finger, neck, body, head, legs, feet, hands, arms, shield, about, waist, wrist, wield, and hold.{x",
	    ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    /* Hack because 'spear' is 'staff' in weapon_table*/
    if (!str_cmp(arg4, "spear"))
	sprintf(arg4, "%s", "staff");

    if (arg4[0] != '\0'
    && ((type == ITEM_WEAPON
         &&   weapon_type (arg4) == WEAPON_UNKNOWN)
    || (type == ITEM_RANGED_WEAPON
         &&   ranged_weapon_type(arg4) == RANGED_WEAPON_EXOTIC)))
    {
	act("{R$N tells you, 'I've never heard of such a type of weapon.'{x",
	    ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    deduct_cost(ch, cost);
    act("You hand $N some coins.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("$n hands $N some coins.", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    act("$n wanders to the back of the room.",
        mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("$n digs through some books for a moment.",
    	mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("$n thinks, then scribbles something down on a scroll.",
        mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    iHash = 0;
    i = 0;
    sprintf(buf,
        "{YThe scroll says:{x\n\r"
	"I have searched through my studies and books\n\r"
	"and have found the following items pertaining to your\n\r"
	"inquiry:\n\r\n\r");

	// TODO: THIS NEEDS TO BE REDONE ENTIRELY
	AREA_DATA *pArea;
	for (pArea = area_first; pArea; pArea = pArea->next)
    for (objIndex = pArea->obj_index_hash[iHash]; iHash <= MAX_KEY_HASH;
          objIndex = pArea->obj_index_hash[iHash++])
    {
	if (i >= 10)
	    break;

        if (objIndex == NULL
	|| objIndex->level < min
	|| objIndex->level > max
	|| !str_cmp(objIndex->area->name, "Imm Zone")
	|| !str_cmp(objIndex->area->name, "The Godly Realm")
	|| !objIndex->area->open
	|| !IS_SET(objIndex->wear_flags, ITEM_TAKE)
	|| (objIndex->item_type != ITEM_WEAPON
	     && objIndex->item_type != ITEM_ARMOUR
	     && objIndex->item_type != ITEM_ARTIFACT
	     && objIndex->item_type != ITEM_LIGHT
	     && objIndex->item_type != ITEM_CONTAINER
	     && !IS_INSTRUMENT(objIndex)
	     && objIndex->item_type != ITEM_RANGED_WEAPON)
	|| (arg3[0] != '\0'
	     && type != objIndex->item_type)
	|| (arg4[0] != '\0'
	     && type != ITEM_WEAPON
	     && !IS_SET(objIndex->wear_flags, flag_value(wear_flags, arg4)))
	|| (arg4[0] != '\0'
	     && type == ITEM_RANGED_WEAPON
	     && str_cmp(ranged_weapon_name(objIndex->value[0]), arg4))
	|| (arg4[0] != '\0'
	     && type == ITEM_WEAPON
	     && str_cmp(weapon_name(objIndex->value[0]), arg4))
	|| number_percent() < 33)
	    continue;

	sprintf(sd, "%s", objIndex->short_descr);
	sd[0] = UPPER(sd[0]);
	{
	if (!str_cmp(objIndex->area->name, "Maze-Level1")
		    || !str_cmp(objIndex->area->name, "Maze-Level2")
		    || !str_cmp(objIndex->area->name, "Maze-Level3")
		    || !str_cmp(objIndex->area->name, "Maze-Level4")
		    || !str_cmp(objIndex->area->name, "Maze-Level5"))
	sprintf(aname, "%s", "Pyramid of the Abyss");
	else
	sprintf(aname, "%s", objIndex->area->name);
	}
	aname[0] = UPPER(aname[0]);
	sprintf(buf2,
	"{Y%d.{x %s, from {R%s{x, which is %slevel {Y%d{x %s.\n\r",
	    i + 1,
	    sd,
		aname,
	    objIndex->item_type == ITEM_ARMOUR ? "" : "a ",
	    objIndex->level,
	    item_name(objIndex->item_type));
	strcat(buf, buf2);

	i++;
    }

    sprintf(buf2, "\n\rThank you for your business.\n\r\n\rSigned, {m%s{x.", mob->short_descr);
    strcat(buf, buf2);

    scroll = create_object(obj_index_blank_scroll, 1, FALSE);
    free_string(scroll->name);
    free_string(scroll->short_descr);
    free_string(scroll->description);

    scroll->name = str_dup("scroll shimmering parchment");
    scroll->short_descr = str_dup("a shimmering parchment scroll");
    scroll->description = str_dup("A shimmering parchment scroll has been left behind here.");

    ed = new_extra_descr();
    ed->keyword = str_dup("scroll");
    ed->next = scroll->extra_descr;
    scroll->extra_descr = ed;
    ed->description = str_dup(buf);

    ed = new_extra_descr();
    ed->keyword = str_dup("shimmering");
    ed->next = scroll->extra_descr;
    scroll->extra_descr = ed;
    ed->description = str_dup(buf);

    ed = new_extra_descr();
    ed->keyword = str_dup("parchment");
    ed->next = scroll->extra_descr;
    scroll->extra_descr = ed;
    ed->description = str_dup(buf);

    SET_BIT(scroll->extra[0], ITEM_GLOW);

    act("$N gives you $p.", ch, mob, NULL, scroll, NULL, NULL, NULL, TO_CHAR);
    act("$N gives $n $p.", ch, mob, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
    obj_to_char(scroll, ch);
#else
	command_under_construction(ch);
#endif
}


/* Saves info about each piece of equipment, so next time the character types 'wear all', it knows what to put on.*/
void save_last_wear(CHAR_DATA *ch)
{
    OBJ_DATA *pObj = NULL;

    /* Reset last wear location*/
    for (pObj = ch->carrying; pObj != NULL; pObj = pObj->next_content)
	pObj->last_wear_loc = WEAR_NONE;

    for (pObj = ch->carrying; pObj != NULL; pObj = pObj->next_content)
	pObj->last_wear_loc = pObj->wear_loc;
}


// TODO: Rework for general artificing stuff
void do_combine(CHAR_DATA *ch, char *argument)
{
#if 0
    char arg[MSL];
    char arg2[MSL];
    int chance, roll;
    int charges;
    OBJ_DATA *obj1 = NULL;
    OBJ_DATA *obj2 = NULL;
    bool scrolls = FALSE;
    bool potions = FALSE;
    bool maximize = TRUE;
    bool destroy = FALSE;
    SPELL_DATA *spell1, *spell2;
	SPELL_DATA *max_spell1, *max_spell2;

    if ((chance = get_skill(ch, gsk_combine)) < 1)
    {
		send_to_char("Leave that to the alchemists.\n\r", ch);
		return;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
		send_to_char("Syntax: combine <item1> <item2>\n\r", ch);
		return;
    }

    /* setup objects*/
    if ((obj1 = get_obj_list(ch, arg, ch->carrying)) == NULL)
    {
		act("You aren't carrying any $t.", ch, NULL, NULL, NULL, NULL, arg, NULL, TO_CHAR);
		return;
    }

    if ((obj2 = get_obj_list(ch, arg2, ch->carrying)) == NULL)
    {
		act("You aren't carrying any $t.", ch, NULL, NULL, NULL, NULL, arg2, NULL, TO_CHAR);
		return;
    }

    /* make sure our objects are the right item types */
    switch (obj1->item_type)
    {
	case ITEM_SCROLL:
	    scrolls = TRUE;
	    break;
	case ITEM_FLUID_CON:
	    potions = TRUE;
	    break;
	default:
	    act("$p is not a scroll or a fluid container.", ch, NULL, NULL, obj1, NULL, NULL, NULL, TO_CHAR);
	    return;
    }

    if (scrolls)
    {
		if (obj2->item_type != ITEM_SCROLL)
		{
			act("$p and $P are not the same type of item.", ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_CHAR);
			return;
		}
    }

    if (potions)
    {
		if (obj2->item_type != ITEM_FLUID_CON)
		{
			act("$p and $P are not the same type of item.", ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_CHAR);
			return;
		}
    }

    if (obj1 == obj2)
    {
		send_to_char("You can't defy the laws of physics.\n\r", ch);
		return;
    }

    roll = number_percent();
    if( roll > (chance + 7) )
		destroy = TRUE;
	else if( roll > chance )
		maximize = FALSE;

	// Check they have spells
    if (!obj1->spells)
    {
	    act("$p does not contain any magic.", ch, NULL, NULL, obj1, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

    if (!obj2->spells)
    {
	    act("$p does not contain any magic.", ch, NULL, NULL, obj2, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	// Mark all spells as not having been used
	for (spell1 = obj1->spells; spell1; spell1 = spell1->next)
		spell1->repop = 0;

	for (spell2 = obj2->spells; spell2; spell2 = spell2->next)
		spell2->repop = FALSE;

	if (maximize)
	{
		// Pair up the spells by type and level, keeping the strongest of each type paired up.
		do
		{
			// Find the current maximum level of the unassigned spells
			max_spell1 = NULL;
			for (spell1 = obj1->spells; spell1; spell1 = spell1->next)
			{
				if( spell1->level > 0 && !spell1->repop &&
					(!max_spell1 || spell1->level > max_spell1->level) )
					max_spell1 = spell1;
			}

			if( max_spell1 ) {
				// Find the corresponding spell in obj2

				max_spell2 = NULL;
				for (spell2 = obj2->spells; spell2; spell2 = spell2->next)
				{
					if (max_spell1->sn == spell2->sn && spell2->level > 0 && !spell2->repop &&
						(!max_spell2 || spell2->level > max_spell2->level) )
						max_spell2 = spell2;
				}

				if( !max_spell2 )
				{
					send_to_char("You can only combine items which have the same spells.\n\r", ch);
					return;
				}

				max_spell1->repop = max_spell2->level;
				max_spell2->repop = TRUE;
			}
		}
		while(max_spell1 != NULL);
	}
	else
	{
		// Non-maximize, pairs up spells by type, ignoring respective levels.
		for (spell1 = obj1->spells; spell1; spell1 = spell1->next)
		{
			bool found = FALSE;
			if( spell1->level < 1 )
				continue;

			for (spell2 = obj2->spells; spell2; spell2 = spell2->next)
			{
				if (spell1->sn == spell2->sn && spell2->level > 0 && !spell2->repop )
				{
					spell1->repop = spell2->level;
					spell2->repop = TRUE;
					found = TRUE;
					break;
				}
			}

			if( found )
			{
				send_to_char("You can only combine items which have the same spells.\n\r", ch);
				return;
			}
		}
	}

	// Look for spells in obj2 that were not found.
	for (spell2 = obj2->spells; spell2; spell2 = spell2->next)
	{
		if ( spell2->level > 0 && !spell2->repop )
		{
			send_to_char("You can only combine items which have the same spells.\n\r", ch);
			return;
		}
	}

    /* add up charges on potions*/
    charges = 2;
    if (potions)
    {
		charges += obj1->value[5];
		charges += obj2->value[5];
		charges = UMIN(charges, 3);
		obj1->value[5] = charges;
    }

    if (destroy)
    {
		act("You make a slight mistake and $p and $P vanish in a mist!",
			ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_CHAR);
		act("$n makes a slight mistake and $p and $P vanish in a mist!",
			ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_ROOM);
		extract_obj(obj1);
		extract_obj(obj2);
		check_improve(ch, gsk_combine, 1, FALSE);
		return;
    }

    // Blend the levels together
	for (spell1 = obj1->spells; spell1; spell1 = spell1->next)
	{
		if (spell1->level > 0)
		{
			spell1->level = 2 * spell1->level/3 + 2 * spell1->repop/3;
			spell1->level = UMIN(spell1->level, ch->tot_level * 2);
		}
	}

    act("You combine $p and $P.", ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_CHAR);
    act("$n combines $p and $P.", ch, NULL, NULL, obj1, obj2, NULL, NULL, TO_ROOM);

    extract_obj(obj2);
    check_improve(ch, gsk_combine, 1, TRUE);
#else
	send_to_char("Combine is being reworked.\n\r", ch);
#endif
}


void do_keep(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    OBJ_DATA *obj;

    argument = one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax: keep <item>\n\r", ch);
	return;
    }

    if ((obj = get_obj_list(ch, arg, ch->carrying)) == NULL)
    {
	act("You aren't carrying any $t.", ch, NULL, NULL, NULL, NULL, arg, NULL, TO_CHAR);
	return;
    }

    if (IS_SET(obj->extra[1], ITEM_KEPT))
    {
	REMOVE_BIT(obj->extra[1], ITEM_KEPT);
	act("You will no longer keep $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    }
    else
    {
	SET_BIT(obj->extra[1], ITEM_KEPT);
	act("You will now keep $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    }
}


/* Reset an obj for a new owner. Used in get, etc.*/
void reset_obj(OBJ_DATA *obj)
{
    if (IS_SET(obj->extra[1], ITEM_KEPT))
	REMOVE_BIT(obj->extra[1], ITEM_KEPT);

    obj->last_wear_loc = WEAR_NONE;
}


void do_consume(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *corpse;
    char arg[MAX_STRING_LENGTH];
    int liquid, amount;
    int chance;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
	send_to_char("Syntax: consume <corpse>\n\r", ch);
	return;
    }

    if (is_dead(ch))
	return;

    if ((chance = get_skill(ch, gsk_consume)) == 0)
    {
        send_to_char("How disgusting!\n\r",ch);
        return;
    }

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_FULL] > 45)
    {
        send_to_char("You are full, you can't consume any more.\n\r", ch);
        return;
    }

    corpse = get_obj_list(ch, arg, ch->in_room->contents);

    if (corpse == NULL)
    {
        send_to_char("You can't find it.\n\r", ch);
        return;
    }

    if (!(corpse->item_type == ITEM_CORPSE_PC
	   || corpse->item_type == ITEM_CORPSE_NPC))
    {
        send_to_char("You can't consume that.\n\r", ch);
        return;
    }

    act("$n savagely consumes $p.", ch, NULL, NULL, corpse, NULL, NULL, NULL, TO_ROOM);
    act("You savagely consume $p.", ch, NULL, NULL, corpse, NULL, NULL, NULL, TO_CHAR);

    extract_obj(corpse);

    ch->hit = URANGE(1, ch->hit + (ch->max_hit - ch->hit)/4, ch->max_hit);

    WAIT_STATE(ch, 24);
    liquid = 13; /* Value for blood*/
    amount = liq_table[liquid].liq_affect[4] * 3;
    gain_condition(ch, COND_FULL,
        amount * 8 / 4);
    gain_condition(ch, COND_THIRST,
        amount * 8 / 10);
    gain_condition(ch, COND_HUNGER,
        amount * 8 / 2);

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_FULL]   > 40)
        send_to_char("You are full.\n\r", ch);
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_THIRST] > 40)
        send_to_char("Your thirst is quenched.\n\r", ch);
}


void obj_touch_spell(OBJ_DATA *tattoo, SKILL_DATA *skill, int level, CHAR_DATA *ch)
{
    if (!IS_VALID(skill))
	{
		bug("obj_touch_spell: bad skill data.", 0);
		return;
	}

	if (skill->token)
	{
		if (!get_script_token(skill->token, TRIG_TOKEN_TOUCH, TRIGSLOT_SPELL))
		{
			bug("obj_touch_spell: bad skill uid = %d.", skill->uid);
			return;
		}
	}
	else if (!skill->touch_fun)
    {
		bug("obj_touch_spell: bad skill uid = %d.", skill->uid);
		return;
    }

	if (skill->token)
	{
		//sprintf(_buf, "%s: victim = %s", skill->name, (victim?victim->name:"(null)"));
		//send_to_char(_buf, ch);
		//sprintf(_buf, "%s: obj = %s", skill->name, (obj?obj->short_descr:"(null)"));
		//send_to_char(_buf, ch);
		p_token_index_percent_trigger(skill->token, ch, NULL, NULL, tattoo, NULL, TRIG_TOKEN_TOUCH, NULL, 0,0,0,0,0, level,0,0,0,0);
	}
	else
		(*(skill->touch_fun)) (skill, level, ch, tattoo);
}

void do_touch(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    SPELL_DATA *spell;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		send_to_char("Touch what?\n\r", ch);
		return;
    }

    if ((obj = get_obj_wear(ch, arg, TRUE)) == NULL)
    {
		send_to_char("You do not have that tattoo.\n\r", ch);
		return;
    }

    if (!IS_TATTOO(obj))
    {
		send_to_char("You can touch only tattoos.\n\r", ch);
		return;
    }

	// This is for custom tattoo items that are affixed
	if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_TOUCH, argument,0,0,0,0,0))
		return;

    if (!TATTOO(obj)->touches)
    {
		send_to_char("Nothing happens.", ch);
    }
    else
    {
		act("$n touches $p briefly.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You touch $p briefly.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		ITERATOR spit;
		iterator_start(&spit, TATTOO(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&spit)))
		{
			obj_touch_spell(obj, spell->skill, spell->level, ch);
		}
		iterator_stop(&spit);

		if(TATTOO(obj)->touches > 0) --TATTOO(obj)->touches;

		if(TATTOO(obj)->fading_rate > 0 && TATTOO(obj)->fading_chance < 100)
		{
			TATTOO(obj)->fading_chance += TATTOO(obj)->fading_rate;	// If this goes over 100, so be it, the tattoo will fade
			if (TATTOO(obj)->fading_chance > 100) TATTOO(obj)->fading_chance = 100;
		}

		if(number_percent() < TATTOO(obj)->fading_chance)
		{
			// TODO: change (5,10) to (TATTOO(obj)->min_degrade, TATTOO(obj)->max_degrade)
			obj->condition -= number_range(5,10);
			if (obj->condition <= 0)
			{
				act("$p fades away as the ink dries.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ALL);
				extract_obj(obj);
			}
		}
		
		WAIT_STATE(ch, 8);
    }
}

void do_ruboff(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		send_to_char("Ruboff what?\n\r", ch);
		return;
    }

    if ((obj = get_obj_wear(ch, arg, TRUE)) == NULL)
    {
		send_to_char("You do not have that tattoo.\n\r", ch);
		return;
    }

    if (obj->item_type != ITEM_TATTOO)
    {
		send_to_char("You can only rub off tattoos.\n\r", ch);
		return;
    }

    if (IS_SET(obj->extra[0], ITEM_NOREMOVE))
    {
		send_to_char("The ink seems to be permanent.\n\r", ch);
		return;
    }

	if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREMOVE, NULL,0,0,0,0,0))
		return;

	if(!p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REMOVE, NULL,0,0,0,0,0))
	{
		act("$n rubs $p vigorously until it fades away..", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You rub $p vigorously until it fades away.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	}

	extract_obj(obj);
}

bool can_ink_spell(CHAR_DATA *ch, SKILL_ENTRY *spell, CHAR_DATA *victim, int wear_loc)
{
	if (!spell || !spell->isspell)
	{
		send_to_char("You don't know any spells by that name.\n\r", ch);
		return FALSE;
	}

	if (!IS_SET(spell->skill->flags, SKILL_CAN_INK))
	{
		if (victim != ch)
			act_new("You cannot ink $t into a tattoo on $N.", ch,victim,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
		else
			act_new("You cannot ink $t into a tattoo.", ch,NULL,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
		return FALSE;
	}

	if (IS_VALID(spell->token))
	{
		SCRIPT_DATA *script = get_script_token(spell->token->pIndexData, TRIG_TOKEN_TOUCH, TRIGSLOT_SPELL);
		if(!script) {
			if (victim != ch)
				act_new("You cannot ink $t into a tattoo on $N.", ch,victim,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			else
				act_new("You cannot ink $t into a tattoo.", ch,NULL,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return FALSE;
		}

		int ret = p_percent_trigger(NULL, NULL, NULL, spell->token, ch, NULL, NULL, NULL, NULL, TRIG_TOKEN_PREINK, NULL,wear_loc,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				if (victim != ch)
					act_new("You cannot ink $t into a tattoo on $N.", ch,victim,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
				else
					act_new("You cannot ink $t into a tattoo.", ch,NULL,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			}
			return FALSE;
		}
	}
	else
	{
		if(!spell->skill->touch_fun)
		{
			if (victim != ch)
				act_new("You cannot ink $t into a tattoo on $N.", ch,victim,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			else
				act_new("You cannot ink $t into a tattoo.", ch,NULL,NULL,NULL,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		if(spell->skill->preink_fun)
		{
			// prescribe function is responsible for messages.
			if (!(*spell->skill->preink_fun)(spell->skill, ch->tot_level, ch, victim, wear_loc))
				return false;
		}
	}

	return true;
}


void do_ink(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
	SKILL_ENTRY *spells[3];
    int have[CATALYST_MAX];
    int need[CATALYST_MAX];
    int loc;
    int i,n;
    int chance;
    char arg[MAX_STRING_LENGTH];

	if (IS_DEAD(ch))
	{
		send_to_char("You are can't do that. You are dead.\n\r", ch);
		return;
	}

	if (!(chance = get_skill(ch, gsk_tattoo))) {
		send_to_char("Ink? What's that?\n\r",ch);
		return;
	}

	memset(have,0,sizeof(have));
	memset(need,0,sizeof(need));
	for (obj = ch->carrying; obj != NULL; obj = obj->next_content) {
		if (IS_INK(obj))
		{
			for(int i = 0; i < MAX_INK_TYPES; i++)
			{
				if (INK(obj)->types[i] > CATALYST_NONE && INK(obj)->types[i] < CATALYST_MAX && INK(obj)->amounts[i] > 0)
					have[INK(obj)->types[i]] += INK(obj)->amounts[i];
			}
		}
	}

	argument = one_argument(argument, arg);

	if(!str_cmp(arg,"self"))
		victim = ch;
	else if((victim = get_char_room(ch, NULL, arg)) == NULL) {
		send_to_char("They aren't here.\n\r", ch);
		return;
	}

	if (argument[0] == '\0') {
		send_to_char("Where did you want to place the tattoo?\n\r", ch);
		show_flag_cmds(ch, tattoo_loc_flags);
		return;
	}

	argument = one_argument(argument, arg);

	if((loc = stat_lookup(arg, tattoo_loc_flags, WEAR_NONE)) == WEAR_NONE) {
		send_to_char("Place the tattoo where?\n\r", ch);
		show_flag_cmds(ch, tattoo_loc_flags);
		return;
	}

	if(get_eq_char(victim,ch->ink_loc)) {
		send_to_char("There is already a tattoo there.\n\r", ch);
		return;
	}

	/* Check for exposed skin*/

	if (argument[0] == '\0') {
		send_to_char("What tattoo do you want to create?\n\r", ch);
		return;
	}

	memset(spells,0,sizeof(spells));

	// Clear out catalyst usages
	memset(ch->catalyst_usage, 0, sizeof(ch->catalyst_usage));
	for(i = n = 0; i < 3; i++)
	{
		argument = one_argument(argument, arg);
		if(!arg[0]) break;
		spells[i] = skill_entry_findname(ch->sorted_skills, arg);
		if (!can_ink_spell(ch, spells[i], victim, loc))
			return;

		n++;
	}

	memset(need, 0, sizeof(need));
	for(i = 0; i < 3; i++)
	{
		if (spells[i])
		{
			for(int j = 0; j < 3; j++)
			{
				if (spells[i]->skill->inks[j][0] > CATALYST_NONE && spells[i]->skill->inks[j][0] < CATALYST_MAX)
					need[spells[i]->skill->inks[j][0]] += spells[i]->skill->inks[j][1];
			}
		}
	}
	if (!are_spell_targets_compatible(spells[0], spells[1]) ||
		!are_spell_targets_compatible(spells[0], spells[2]) ||
		!are_spell_targets_compatible(spells[1], spells[2]))
	{
		send_to_char("All spells must have the same kind of targeting requirements.\n\r", ch);

		// TODO: Report what is wrong?
		return;
	}

	bool have_all_catalysts = true;
	for(i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
		{
			int catalyst = has_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM);
			if (catalyst >= 0 && catalyst < ch->catalyst_usage[i])
			{
				sprintf(buf,"You appear to be missing a required %s catalyst. (%d/%d)\n\r", catalyst_descs[i],catalyst, ch->catalyst_usage[i]);
				send_to_char(buf, ch);
				have_all_catalysts = false;
			}
		}
	}

	if (!have_all_catalysts)
		return;

	if (!has_inks(ch, need, true))
		return;

	act("{Y$n lays out the necessary inks and begins tattooing...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YYou lay out the necessary inks and begin tattooing...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	extract_inks(ch, need);

	ch->ink_target = victim;
	ch->ink_loc = loc;
	for(int i = 0; i < 3; i++)
		ch->ink_info[i] = spells[i];


	int beats = (6 + n*n);
	if (chance > 75)
		beats = (125 - chance) * beats / 50;

	TATTOO_STATE(ch, beats);
}


void ink_end( CHAR_DATA *ch )
{
    char buf[2*MAX_STRING_LENGTH];
    OBJ_DATA *tattoo;
    int chance,n,level;
    char tattoo_name[MAX_STRING_LENGTH];
    SPELL_DATA *spell;

	// Make sure the victim doesn't have the wear location used
	if (get_eq_char(ch->ink_target, ch->ink_loc))
	{
		act("{Y$n's attempt to ink a tattoo fails miserably.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{YYou fail to coalesce the ink into a tattoo, dispersing them on the wind.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	chance = get_skill(ch, gsk_tattoo);
	if (ch->ink_info[2])
		chance = 5 * chance / 6;
	else if (ch->ink_info[1])
		chance = 41 * chance / 42;

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = 3 * chance / 2;

    chance = URANGE(1, chance, 99);

    if (IS_IMMORTAL(ch))
		chance = 100;

	// Make sure we *still* have all the catalysts,
	// as they might have been consumed or destroyed by the time the brewing is finished
	bool have_all_catalysts = true;
	for(int i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
		{
			int catalyst = has_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM);
			if (catalyst >= 0 && catalyst < ch->catalyst_usage[i])
			{
				sprintf(buf,"You appear to be missing a required %s catalyst. (%d/%d)\n\r", catalyst_descs[i],catalyst, ch->catalyst_usage[i]);
				send_to_char(buf, ch);
				have_all_catalysts = false;
			}
		}
	}

	if (!have_all_catalysts)
		return;

    if (number_percent() >= chance)
    {
		act("{Y$n's attempt to ink a tattoo fails miserably.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{YYou fail to coalesce the ink into a tattoo, dispersing them on the wind.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		check_improve(ch, gsk_tattoo, FALSE, 2);
		return;
    }

	// Use all the catalysts
	for(int i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
			use_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM,ch->catalyst_usage[i],TRUE);
	}

	// Post processing after successful scribing
	for(int i = 0; i < 3; i++)
	{
		SKILL_ENTRY *entry = ch->scribe_info[i];
		if (entry)
		{
			if (IS_VALID(entry->token))
			{
				p_percent_trigger(NULL, NULL, NULL, entry->token, ch, ch->ink_target, NULL, NULL, NULL, TRIG_TOKEN_SCRIBE, NULL,ch->ink_loc,0,0,0,0);
			}
			else if(entry->skill->ink_fun)
			{
				(*entry->skill->ink_fun)(entry->skill, ch->tot_level, ch, ch->ink_target, ch->ink_loc);
			}
		}
	}

	if (ch->ink_info[2]) sprintf(tattoo_name, "%s, %s, %s", skill_entry_name(ch->ink_info[0]), skill_entry_name(ch->ink_info[1]), skill_entry_name(ch->ink_info[2]));
	else if (ch->ink_info[1]) sprintf(tattoo_name, "%s, %s", skill_entry_name(ch->ink_info[0]), skill_entry_name(ch->ink_info[1]));
	else strcpy(tattoo_name, skill_entry_name(ch->ink_info[0]));

	if(ch->ink_target != ch) {
		act("You coalesce the ink into a tattoo of $t onto $N's skin.", ch, ch->ink_target, NULL, NULL, NULL, tattoo_name, NULL, TO_CHAR);
		act("$n coalesces the ink into a tattoo of $t onto your skin.", ch, ch->ink_target, NULL, NULL, NULL, tattoo_name, NULL, TO_VICT);
		act("$n coalesces the ink into a tattoo of $t onto $N's skin.", ch, ch->ink_target, NULL, NULL, NULL, tattoo_name, NULL, TO_NOTVICT);
	} else {
		act("You coalesce the ink into a tattoo of $t onto your skin.", ch, NULL, NULL, NULL, NULL, tattoo_name, NULL, TO_CHAR);
		act("$n coalesces the ink into a tattoo of $t onto $s skin.", ch, NULL, NULL, NULL, NULL, tattoo_name, NULL, TO_ROOM);
	}

    check_improve(ch, gsk_tattoo, TRUE, 2);

    tattoo = create_object(obj_index_empty_tattoo, 1, FALSE);

    sprintf(buf, tattoo->short_descr, tattoo_name);

    free_string(tattoo->short_descr);
    tattoo->short_descr = str_dup(buf);

    sprintf(buf, tattoo->description, tattoo_name);

    free_string(tattoo->description);
    tattoo->description = str_dup(buf);

    free_string(tattoo->full_description);
    tattoo->full_description = str_dup(buf);

    free_string(tattoo->name);
	strcat(tattoo_name, " tattoo");
    tattoo->name = short_to_name(tattoo_name);

	TATTOO(tattoo)->touches = number_range(1,UMAX(2,(ch->tot_level / 20)));
	if(chance < 50)
		TATTOO(tattoo)->fading_chance = 100 - chance * chance / 100;
	else
		TATTOO(tattoo)->fading_chance = (100 - chance) * (100 - chance) / 100;
	TATTOO(tattoo)->fading_rate = (100 - chance) / 20;

	n = 0;
	for (int i = 0; i < 3 && ch->ink_info[i]; n++, i++);

	level = ch->tot_level * ((n - 1) * chance + 100) / (n * 100);

	if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
		level = (ch->tot_level + level) / 2;

	for(int i = 0; i < 3; i++)
	{
		if (ch->ink_info[i])
		{
			spell = new_spell();
			spell->skill = ch->ink_info[i]->skill;
			//spell->token = IS_VALID(ch->ink_info[i]->token) ? ch->ink_info[i]->token->pIndexData : NULL;
			spell->level = level / (i + 1);

			list_appendlink(TATTOO(tattoo)->spells, spell);
		}
	}

    obj_to_char(tattoo, ch->ink_target);
    tattoo->wear_loc = ch->ink_loc;
}


void do_affix(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    bool silent = FALSE;
    OBJ_DATA *obj;
    int loc;

	argument = one_argument(argument, arg);

	if (arg[0] == '\0') {
		send_to_char("Affix what to where?\n\r", ch);
		return;
	}

	if ((obj = get_obj_list(ch,arg,ch->carrying)) == NULL) {
		send_to_char("You do not have that tattoo.\n\r", ch);
		return;
	}

	if (obj->item_type != ITEM_TATTOO && !IS_SET(obj->wear_flags, ITEM_WEAR_TATTOO)) {
		send_to_char("You can only affix tattoo.\n\r", ch);
		return;
	}

	if (obj->wear_loc != WEAR_NONE) {
		send_to_char("That appears to be used.\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg);

	if((loc = stat_lookup(arg, tattoo_loc_flags, WEAR_NONE)) == WEAR_NONE) {
		send_to_char("Affix the tattoo where?\n\r", ch);
		show_flag_cmds(ch, tattoo_loc_flags);
		return;
	}

	if(IS_NPC(ch) && !argument[0]) {
		argument = one_argument(argument, arg);

		if(!str_cmp(arg,"self"))
			victim = ch;
		else if((victim = get_char_room(ch, NULL, arg)) == NULL) {
			send_to_char("They aren't here.\n\r", ch);
			return;
		}

		if(!str_cmp(argument,"silent"))
			silent = TRUE;
	} else
		victim = ch;

	if(get_eq_char(victim,loc)) {
		send_to_char("There is already a tattoo there.\n\r", ch);
		return;
	}

	if(p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_PREWEAR, NULL,0,0,0,0,0))
		return;

	if(victim != ch) {
		obj_from_char(obj);
		obj_to_char(obj,victim);
	}

	obj->wear_loc = loc;

	if(!silent) {
		if(victim != ch) {
			act("$n affixes $p on $N's skin.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
			act("$n affixes $p on your skin.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT);
			act("You affix $p on $N's skin.", ch,  victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		} else {
			act("$n affixes $p to $s skin.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You affix $p to your skin.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		}
		p_percent_trigger(NULL, obj, NULL, NULL, ch, victim, NULL, NULL, NULL, TRIG_WEAR, NULL,0,0,0,0,0);
	}
}
