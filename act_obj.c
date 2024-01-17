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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "merc.h"
#include "magic.h"
#include "interp.h"
#include "recycle.h"
#include "scripts.h"
#include "tables.h"

void update_money(OBJ_DATA *money);

bool __isspell_valid(CHAR_DATA *ch, OBJ_DATA *obj, SKILL_ENTRY *spell, int pretrigger, int trigger, char *token_message)
{
	if (!spell || !spell->isspell)
	{
		send_to_char("You don't know any spells by that name.\n\r", ch);
		return false;
	}

	if (IS_VALID(spell->token))
	{
		int ret = p_percent_trigger(NULL, NULL, NULL, spell->token, ch, NULL, NULL, obj, NULL, pretrigger, NULL,0,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				act_new(token_message, ch,NULL,NULL,obj,NULL,spell->token->name,NULL,TO_CHAR,POS_DEAD,NULL);
				//sprintf(buf, token_message, spell->token->name, obj->short_descr);
				//send_to_char(buf, ch);
			}
			return false;
		}

		SCRIPT_DATA *script = get_script_token(spell->token->pIndexData, trigger, TRIGSLOT_SPELL);
		if(!script) {
			act_new(token_message, ch,NULL,NULL,obj,NULL,spell->token->name,NULL,TO_CHAR,POS_DEAD,NULL);
			//sprintf(buf, token_message, spell->token->name, obj->short_descr);
			//send_to_char(buf, ch);
			return false;
		}
	}
	// Add prespell functions

	return true;
}

bool are_spell_targets_compatible(SKILL_ENTRY *sa, SKILL_ENTRY *sb)
{
	if (!sa || !sb) return true;

	int a = sa->skill->target;
	int b = sb->skill->target;

	switch(a)
	{
	case TAR_IGNORE:
	case TAR_OBJ_GROUND:
	case TAR_IGNORE_CHAR_DEF:
		return a == b;	// They *MUST* be the same for this target


	case TAR_CHAR_DEFENSIVE:
	case TAR_CHAR_OFFENSIVE:
	case TAR_CHAR_SELF:
	case TAR_CHAR_FORMATION:
		if (b == TAR_CHAR_OFFENSIVE) return true;
		if (b == TAR_CHAR_DEFENSIVE) return true;
		if (b == TAR_CHAR_SELF) return true;
		if (b == TAR_CHAR_FORMATION) return true;
		if (b == TAR_OBJ_CHAR_OFF) return true;
		if (b == TAR_OBJ_CHAR_DEF) return true;
		break;

	case TAR_OBJ_CHAR_OFF:
	case TAR_OBJ_CHAR_DEF:
		if (b == TAR_IGNORE) return false;
		if (b == TAR_OBJ_GROUND) return false;
		if (b == TAR_IGNORE_CHAR_DEF) return false;
		return true;

	case TAR_OBJ_INV:
		if (b == TAR_OBJ_INV) return true;
		if (b == TAR_OBJ_CHAR_OFF) return true;
		if (b == TAR_OBJ_CHAR_DEF) return true;
		break;
	}

	return false;
}


bool has_inks(CHAR_DATA *ch, int *need, bool show)
{
    int have[CATALYST_MAX];
	OBJ_DATA *obj;

	if (!ch || !need)
		return false;

	memset(have,0,sizeof(have));
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

	bool ret = true;
	for(int i = CATALYST_NONE; i < CATALYST_MAX; i++)
	{
		if (have[i] < need[i])
		{
			if (show)
			{
				char buf[MSL];
				sprintf(buf, "You are missing an ink with %s essence.\n\r", catalyst_descs[i]);
				send_to_char(buf, ch);
			}
			ret = false;
		}
	}

	return ret;
}

void extract_inks(CHAR_DATA *ch, int *need)
{
	OBJ_DATA *obj,*next;
	for (obj = ch->carrying; obj != NULL; obj = next) {
		next = obj->next_content;
		bool found = false;
		if (IS_INK(obj))
		{
			for(int i = 0; i < MAX_INK_TYPES; i++)
			{
				if (INK(obj)->types[i] > CATALYST_NONE && INK(obj)->types[i] < CATALYST_MAX && INK(obj)->amounts[i] > 0 && need[INK(obj)->types[i]] > 0)
				{
					need[INK(obj)->types[i]] -= INK(obj)->amounts[i];
					found = true;
				}
			}
		}
		if (found) extract_obj(obj);
	}
}

bool obj_has_money(CHAR_DATA *ch, OBJ_DATA *container)
{
	OBJ_DATA *obj;
	if( container == NULL ) return false;

	for(obj = container->contains; obj; obj = obj->next_content)
	{
		if( can_see_obj(ch, container) && obj->item_type == ITEM_MONEY )
			return true;
	}


	return false;
}

void get_obj( CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container )
{
    CHAR_DATA *gch;

    if ( !CAN_WEAR(obj, ITEM_TAKE) )
    {
	send_to_char( "You can't take that.\n\r", ch );
	return;
    }

    if ( ch->carry_number + get_obj_number( obj ) > can_carry_n( ch ) )
    {
	act( "$p: you can't carry that many items.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR );
	return;
    }

    if ((!obj->in_obj || obj->in_obj->carried_by != ch)
    &&  (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch)))
    {
	act( "$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR );
	return;
    }

    if (obj->in_room != NULL)
    {
	for (gch = obj->in_room->people; gch != NULL; gch = gch->next_in_room)
	    if (gch->on == obj)
	    {
		act("$N appears to be using $p.", ch, gch, NULL,obj, NULL, NULL, NULL,TO_CHAR);
		return;
	    }
    }

    if ( IS_SET( obj->extra[1], ITEM_TRAPPED ) )
    {
        act("{RYou pick up $p, but recoil in pain and drop it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR );
	act("{R$n picks up $p, but recoils in pain and drops it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM );
	damage( ch, ch, obj->trap_dam, NULL, TYPE_UNDEFINED, DAM_ENERGY, false );
	REMOVE_BIT( obj->extra[1], ITEM_TRAPPED );
	return;
    }

    if ( container != NULL )
    {
		if (container->pIndexData == obj_index_pit)
		{
			if (get_trust(ch) < obj->level)
			{
				send_to_char("You are not powerful enough to use it.\n\r",ch);
				return;
			}

    		if (!CAN_WEAR(container, ITEM_TAKE))
		    	obj->timer = 0;
		}

		act( "You get $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR );
		act( "$n gets $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM );
		obj_from_obj( obj );
    }
    else
    {
		act( "You get $p.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR );
		act( "$n gets $p.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM );
		obj_from_room( obj );
    }

    if ( container == NULL || container->item_type == ITEM_CORPSE_PC || container->item_type == ITEM_CORPSE_NPC )
	reset_obj( obj );

    obj_to_char( obj, ch );

    p_give_trigger( NULL, obj, NULL, ch, obj, TRIG_GET ,0,0,0,0,0);
    p_give_trigger( NULL, NULL, ch->in_room, ch, obj, TRIG_GET ,0,0,0,0,0);

    return;
}

void give_money(CHAR_DATA *ch, OBJ_DATA *container, int gold, int silver, bool indent)
{
	char buf1[MSL];
	char buf2[MSL];


	if( silver > 0 || gold > 0) {
		if(gold > 1) {
			if( silver > 1 ) {
				sprintf(buf1, "%s{xYou get %d gold coins and %d silver coins", (indent?"     ":""), gold, silver);
				sprintf(buf2, "%s{x$n gets %d gold coins and %d silver coins", (indent?"     ":""), gold, silver);
			} else if( silver == 1 ) {
				sprintf(buf1, "%s{xYou get %d gold coins and a silver coin", (indent?"     ":""), gold);
				sprintf(buf2, "%s{x$n gets %d gold coins and a silver coin", (indent?"     ":""), gold);
			} else {
				sprintf(buf1, "%s{xYou get %d gold coins", (indent?"     ":""), gold);
				sprintf(buf2, "%s{x$n gets %d gold coins", (indent?"     ":""), gold);
			}
		} else if( gold == 1 ) {
			if( silver > 1 ) {
				sprintf(buf1, "%s{xYou get a gold coin and %d silver coins", (indent?"     ":""), silver);
				sprintf(buf2, "%s{x$n gets a gold coin and %d silver coins", (indent?"     ":""), silver);
			} else if( silver == 1 ) {
				sprintf(buf1, "%s{xYou get a gold coin and a silver coin", (indent?"     ":""));
				sprintf(buf2, "%s{x$n gets a gold coin and a silver coin", (indent?"     ":""));
			} else {
				sprintf(buf1, "%s{xYou get a gold coin", (indent?"     ":""));
				sprintf(buf2, "%s{x$n gets a gold coin", (indent?"     ":""));
			}
		} else if(silver > 1) {
			sprintf(buf1, "%s{xYou get %d silver coins", (indent?"     ":""), silver);
			sprintf(buf2, "%s{x$n gets %d silver coins", (indent?"     ":""), silver);
		} else {
			sprintf(buf1, "%s{xYou get a silver coin", (indent?"     ":""));
			sprintf(buf2, "%s{x$n gets a silver coin", (indent?"     ":""));
		}

		if( container != NULL ) {
			strcat(buf1, " from $p.");
			strcat(buf2, " from $p.");
		} else {
			strcat(buf1, ".");
			strcat(buf2, ".");
		}

		act(buf1, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
		act(buf2, ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);

		obj_to_char(create_money(gold, silver), ch);
	}
}

void get_money_from_obj(CHAR_DATA *ch, OBJ_DATA *container)
{
	OBJ_DATA *obj;
	int gold = 0;
	int silver = 0;

	for(obj = container->contains; obj; obj = obj->next_content)
	{
		if( can_see_obj(ch, container) && obj->item_type == ITEM_MONEY && IS_MONEY(obj) )
		{
			if (!can_get_obj(ch, obj, container, NULL, false))
				continue;

			silver += MONEY(obj)->silver;
			gold += MONEY(obj)->gold;

			extract_obj(obj);
		}
	}

	if (gold > 0 || silver > 0)
		give_money(ch, container, gold, silver, false);
}


// Loots all lootable items, except for money
void loot_corpse(CHAR_DATA *ch, OBJ_DATA *corpse)
{
	OBJ_DATA *obj, *obj_next;
	//OBJ_DATA *match_obj;
	OBJ_DATA **objects = NULL;
	int *counts = NULL;
	LLIST **lists = NULL;
	ITERATOR it;
	int i, n_matches, num_objs;
	char buf[MSL];

	bool found;

	num_objs = 0;
	for(obj = corpse->contains; obj; obj = obj->next_content)
	{
		if( obj->item_type == ITEM_MONEY ) continue;
		num_objs++;
	}

	if( num_objs > 0 )
	{
		objects = (OBJ_DATA**)alloc_mem(num_objs * sizeof(OBJ_DATA *));
		counts = (int *)alloc_mem(num_objs * sizeof(int));
		lists = (LLIST **)alloc_mem(num_objs * sizeof(LLIST *));

		for( i = 0; i < num_objs; i++ )
		{
			objects[i] = NULL;
			counts[i] = 0;
			lists[i] = NULL;
		}

		n_matches = 0;

		// Collect all the objects and match counts
		for(obj = corpse->contains; obj; obj = obj_next)
		{
			obj_next = obj->next_content;

			// Skip money, it's handled differently
			if( obj->item_type == ITEM_MONEY ) continue;

			// Can't see it
			if( !can_see_obj(ch, obj) ) continue;

			// Can't get it (either the corpse won't let it go or the character can't hold it)
			if( !can_get_obj(ch, obj, corpse, NULL, true) ) continue;

			found = false;
			for( i = 0; i < n_matches && i < num_objs && objects[i] != NULL; i++)
			{
				// "No names" will match
				if( IS_NULLSTR(obj->short_descr) )
				{
					if( IS_NULLSTR(objects[i]->short_descr) )
					{
						counts[i]++;
						list_appendlink(lists[i], obj);
						found = true;
						break;
					}
				}
				else if( !str_cmp(obj->short_descr, objects[i]->short_descr) )
				{
						counts[i]++;
						list_appendlink(lists[i], obj);
						found = true;
						break;
				}
			}

			if( !found && n_matches < num_objs )
			{
				objects[n_matches] = obj;
				counts[n_matches] = 1;
				lists[n_matches] = list_create(false);
				list_appendlink(lists[n_matches], obj);

				++n_matches;
			}
		}

		for( i = 0; i < n_matches && i < num_objs; i++)
		{
			if( objects[i] != NULL)
			{
				// Do the messages
				sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", counts[i]);
				act(buf, ch, NULL, NULL, objects[i], corpse, NULL, NULL, TO_ROOM);

				sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", counts[i]);
				act(buf, ch, NULL, NULL, objects[i], corpse, NULL, NULL, TO_CHAR);

				// Move objects and trigger TRIG_GET
				iterator_start(&it, lists[i]);
				while( (obj = (OBJ_DATA *)iterator_nextdata(&it)) )
				{
					obj_from_obj(obj);
					obj_to_char(obj, ch);

					p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);
				}
				iterator_stop(&it);
			}
		}


		for( i = 0; i < num_objs; i++)
		{
			if( lists[i] != NULL )
				list_destroy(lists[i]);
		}

		if( lists != NULL ) free_mem(lists, num_objs * sizeof(LLIST *));
		if( objects != NULL ) free_mem(objects, num_objs * sizeof(OBJ_DATA *));
		if( counts != NULL ) free_mem(counts, num_objs * sizeof(int));
	}
}


void do_get(CHAR_DATA *ch, char *argument)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];
	char buf[MAX_STRING_LENGTH];
	//char short_descr[MSL];
	OBJ_DATA *obj, *obj_next = NULL;
	OBJ_DATA *container;
	OBJ_DATA *match_obj;
	int i = 0;
	bool found = true;
	OBJ_DATA *any = NULL;

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);

	if (is_dead(ch))
		return;

	if (arg1[0] == '\0') {
		send_to_char("Get what?\n\r", ch);
		return;
	}

	if (!str_cmp(arg2, "all") || !str_prefix("all.", arg2)) {
		send_to_char("You can't do that.\n\r", ch);
		return;
	}

	// Get <#> gold|silver <container>
	if (is_number(arg1) && (!str_prefix(arg2, "silver") || !str_prefix(arg2, "gold")))
	{
		int amount = atoi(arg1);

		if(amount < 1) {
			send_to_char("Take how much?\n\r",ch);
			return;
		}

		bool gold = !str_prefix(arg2, "gold");

		if(!arg3[0])
		{
			send_to_char(gold?"Get gold from what?\n\r":"Get silver from what?\n\r",ch);
			return;
		}

		/* This section handles getting objects out of containers. */
		if ((container = get_obj_here(ch, NULL, arg3)) == NULL)
		{
			act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg3, TO_CHAR);
			return;
		}

		if (IS_MONEY(container))
		{
			if (!can_get_obj(ch, container, NULL, NULL, false)) {
				send_to_char(gold?"Can't take gold from that.\n\r":"Can't take silver from that.\n\r",ch);
				return;
			}

			if ((gold && MONEY(container)->gold < amount) || (!gold && MONEY(container)->silver < amount))
			{
				send_to_char(gold?"There isn't that much gold to pick up.\n\r":"There isn't that much silver to pick up.\n\r", ch);
				return;
			}

			int g = ch->gold;
			int s = ch->silver;
			int new_g = gold ? (g + amount) : g;
			int new_s = gold ? s : (s + amount);

			ch->gold = 0;
			ch->silver = 0;

			int w = get_weight_coins(new_g, new_s);
			if ((get_carry_weight(ch) + w) > can_carry_w(ch))
			{
				ch->gold = g;
				ch->silver = s;

				act("$p: You can't carry that much weight.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
				return;
			}

			if (gold)
			{
				MONEY(container)->gold -= amount;
			}
			else
			{
				MONEY(container)->silver -= amount;
			}

			ch->gold = new_g;
			ch->silver = new_s;

			sprintf(buf,"%d %s coin%s", amount, gold?"gold":"silver", (amount==1)?"":"s");

			act("You take $T from $p.", ch, NULL, NULL, container, NULL, NULL, buf, TO_CHAR);
			act("$n takes $T from $p.", ch, NULL, NULL, container, NULL, NULL, buf, TO_ROOM);

			// Let the script know how much was taken
			container->tempstore[0] = gold ? amount : 0;
			container->tempstore[1] = gold ? 0 : amount;
			int ret = p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);

			if(!MONEY(container)->silver && !MONEY(container)->gold)
				extract_obj(container);

			if(ret) return;

			if (IS_SET(ch->act[0],PLR_AUTOSPLIT)) {
				int members;
				CHAR_DATA *gch;

				members = 0;
				for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room) {
					if (gch->pcdata != NULL && is_same_group(gch, ch))
						members++;
				}

				if (members > 1 && amount > 1) {
					sprintf(buf,"%d %d",gold?0:amount,gold?amount:0);
					do_function(ch, &do_split, buf);
				}
			}

			church_announce_theft(ch, NULL);
			return;
		}

		if (IS_CONTAINER(container) || container->item_type == ITEM_CORPSE_NPC || container->item_type == ITEM_CORPSE_PC)
		{
			// Look for a money object in the container
			OBJ_DATA *money;

			for(money = container->contains; money; money = money->next)
			{
				if (IS_MONEY(money))
					break;
			}
			
			if (IS_VALID(money))
			{
				if (!can_get_obj(ch, money, container, NULL, false)) {
					send_to_char(gold?"Can't take gold from that.\n\r":"Can't take silver from that.\n\r",ch);
					return;
				}
			}
			else
			{
				if (IS_SET(CONTAINER(container)->flags, CONT_PUT_ON))
					act("There is money on $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
				else
					act("There is money in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
				return;
			}

			if ((gold && MONEY(money)->gold < amount) || (!gold && MONEY(money)->silver < amount))
			{
				char *coin = gold ? "gold" : "silver";
				if (IS_SET(CONTAINER(container)->flags, CONT_PUT_ON))
					act("There isn't that much $T on $p.", ch, NULL, NULL, container, NULL, NULL, coin, TO_CHAR);
				else
					act("There isn't that much $T in $p.", ch, NULL, NULL, container, NULL, NULL, coin, TO_CHAR);
				return;
			}

			int g = ch->gold;
			int s = ch->silver;
			int new_g = gold ? (g + amount) : g;
			int new_s = gold ? s : (s + amount);

			ch->gold = 0;
			ch->silver = 0;

			int w = get_weight_coins(new_g, new_s);
			if ((get_carry_weight(ch) + w) > can_carry_w(ch))
			{
				ch->gold = g;
				ch->silver = s;

				act("$p: You can't carry that much weight.", ch, NULL, NULL, money, NULL, NULL, NULL, TO_CHAR);
				return;
			}

			if (gold)
			{
				MONEY(money)->gold -= amount;
			}
			else
			{
				MONEY(money)->silver -= amount;
			}
			ch->gold = new_g;
			ch->silver = new_s;
			update_money(money);

			sprintf(buf,"%d %s coin%s", amount, gold?"gold":"silver", (amount==1)?"":"s");

			act("You take $T from $p.", ch, NULL, NULL, container, NULL, NULL, buf, TO_CHAR);
			act("$n takes $T from $p.", ch, NULL, NULL, container, NULL, NULL, buf, TO_ROOM);

			// Let the script know how much was taken
			container->tempstore[0] = gold ? amount : 0;
			container->tempstore[1] = gold ? 0 : amount;
			int ret = p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);

			if(!MONEY(money)->silver && !MONEY(money)->gold)
				extract_obj(money);

			if(ret) return;

			if (IS_SET(ch->act[0],PLR_AUTOSPLIT)) {
				int members;
				CHAR_DATA *gch;

				members = 0;
				for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room) {
					if (gch->pcdata != NULL && is_same_group(gch, ch))
						members++;
				}

				if (members > 1 && amount > 1) {
					sprintf(buf,"%d %d",gold?0:amount,gold?amount:0);
					do_function(ch, &do_split, buf);
				}
			}

			church_announce_theft(ch, NULL);
			return;
		}

		send_to_char("That is not money.\n\r", ch);
		return;
	}

	/* Get an obj off the ground */
	if (arg2[0] == '\0')
	{
		/* Get <obj> */
		if (str_cmp(arg1, "all") && str_prefix("all.", arg1)) {
			if ((obj = get_obj_list(ch, arg1, ch->in_room->contents)) == NULL ||
				!can_see_obj(ch, obj)) {
				act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg1, TO_CHAR);
				return;
			}

			if (!can_get_obj(ch, obj, NULL, NULL, false))
				return;

			act("You get $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("$n gets $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			reset_obj(obj);

			if (IS_SET(obj->extra[1], ITEM_TRAPPED)) {
				act("{RYou pick up $p, but recoil in pain and drop it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("{R$n picks up $p, but recoils in pain and drops it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				damage(ch, ch, obj->trap_dam, NULL, TYPE_UNDEFINED, DAM_ENERGY, false);
				REMOVE_BIT(obj->extra[1], ITEM_TRAPPED);
				return;
			}

			obj_from_room(obj);
			obj_to_char(obj, ch);

			p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);

			church_announce_theft(ch, NULL);

			return;
		} else {
			int new_silver = 0;
			int new_gold = 0;

			bool gotten = false;

			/* Get all/all.<obj> */
			while (found) {
				found = false;
				i = 0;
				match_obj = NULL;
				char *s_d = NULL;

				for (obj = ch->in_room->contents; obj != NULL; obj = obj_next) {
					obj_next = obj->next_content;

					if ((arg1[3] == '\0' || is_name(&arg1[4], obj->name)))
						any = obj;

					if (any && any == obj && can_get_obj(ch, obj, NULL, NULL, true)) {
						s_d = obj->short_descr;
						//sprintf(short_descr, "%s", obj->short_descr);
						found = true;
						break;
					}
				}

				if (found) {
					for (obj = ch->in_room->contents; obj != NULL; obj = obj_next) {
						obj_next = obj->next_content;

						if (IS_NULLSTR(obj->short_descr) ||
							str_cmp(obj->short_descr, s_d) ||
							!can_get_obj(ch, obj, NULL, NULL, true))
							continue;

						if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch)) {
							if (i > 0 && match_obj != NULL) {
								sprintf(buf, "{Y({G%2d{Y) {x$n gets $p.", i);
								act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

								sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
								act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
							}

							send_to_char("Your hands are full.\n\r", ch);
							found = false;
						}

						if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch)) {
							if (i > 0 && match_obj != NULL) {
								sprintf(buf, "{Y({G%2d{Y) {x$n gets $p.", i);
								act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

								sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
								act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
							}

							send_to_char("You can't carry any more.\n\r", ch);
							found = false;
						}

						if( obj != NULL ) {

							obj_from_room(obj);

							if( IS_MONEY(obj) ) {
								new_silver += MONEY(obj)->silver;
								new_gold += MONEY(obj)->gold;

								// Keep money until the very end
								extract_obj(obj);
							} else {
								if (match_obj == NULL)
									match_obj = obj;
								obj_to_char(obj, ch);
								i++;

							}

							gotten = true;
						}

						if (!found && match_obj != NULL) {
							p_percent_trigger(NULL, match_obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);

							return;
						}
					}

					if (i > 0 && match_obj != NULL) {
						sprintf(buf, "{Y({G%2d{Y) {x$n gets $p.", i);
						act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

						sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
						act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);

						p_percent_trigger(NULL, match_obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL,0,0,0,0,0);


					} else if (!any) {
						if (arg1[3] == '\0')
							act("There is nothing here you can take.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
						else
							act("There is no $T here you can take.", ch, NULL, NULL, NULL, NULL, NULL, &arg1[4], TO_CHAR);
					}

				}
			}

			// Is obj_next==NULL necessary?
			if ((new_gold > 0 || new_silver > 0) && !obj_next){
				give_money(ch, NULL, new_gold, new_silver, true);
			}

			if( gotten )
				church_announce_theft(ch, NULL);
		}

		return;
	}

	/* This section handles getting objects out of containers. */
	if ((container = get_obj_inv(ch,arg2, false)) == NULL) {
		act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR);
		return;
	}

	/* Get <obj> <container> */
	if (str_cmp(arg1, "all") && str_prefix("all.", arg1)) {
		if ((obj = get_obj_list(ch, arg1, container->contains)) == NULL) {
			act("I see nothing like that in the $T.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR);
			return;
		}

		if (!can_get_obj(ch, obj, container, NULL, false))
			return;

		if (container->item_type == ITEM_CORPSE_PC || container->item_type == ITEM_CORPSE_NPC)
			reset_obj(obj);

		act("You get $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);
		act("$n gets $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM);
		obj_from_obj(obj);
		obj_to_char(obj, ch);

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_GET, NULL,0,0,0,0,0);
		p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_GET, NULL,0,0,0,0,0);

		if (!container->contains)
		{
			p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_EMPTIED, NULL,CONTEXT_CONTAINER,0,0,0,0);
		}

		// If the container is in the current room
		if( !container->carried_by && !container->in_obj && container->in_room != NULL )
		{
			// Ignore player corpses
			if( container->item_type == ITEM_CORPSE_PC ) return;

			// Ignore mob corpses that have a timer
			//  - static mob corpses can exist, but they won't have a timer on them
			if( container->item_type == ITEM_CORPSE_NPC && container->timer > 0 ) return;

			church_announce_theft(ch, NULL);
		}
	} else {
		int new_gold = 0;
		int new_silver = 0;

		bool gotten = false;
		/* Get all/all.<obj> <container> */
		while (found) {
			found = false;
			i = 0;
			match_obj = NULL;
			char *s_d = NULL;

			for (obj = container->contains; obj != NULL; obj = obj_next) {
				obj_next = obj->next_content;

				if (arg1[3] == '\0' || is_name(&arg1[4], obj->name))
					any = obj;

				if (any && any == obj && can_get_obj(ch, obj, container, NULL, true)) {
					s_d = obj->short_descr;
					//sprintf(short_descr, "%s", obj->short_descr);
					found = true;
					break;
				}
			}

			if (found) {
				for (obj = container->contains; obj != NULL; obj = obj_next) {
					obj_next = obj->next_content;

					if (IS_NULLSTR(obj->short_descr) ||
						str_cmp(obj->short_descr, s_d) ||
						!can_get_obj(ch, obj, container, NULL, true))
						continue;

					if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch)) {
						if (i > 0 && match_obj != NULL) {
							sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

							sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
						}

						send_to_char("Your hands are full.\n\r", ch);
						return;
					}

					if (container->carried_by != ch && get_carry_weight(ch) + get_obj_weight(obj) >= can_carry_w(ch)) {
						if (i > 0 && match_obj != NULL) {
							sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

							sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
						}

						send_to_char("You can't carry any more.\n\r", ch);
						return;
					}

					if( obj != NULL ) {

						obj_from_obj(obj);

						if( IS_MONEY(obj) ) {
							new_silver += MONEY(obj)->silver;
							new_gold += MONEY(obj)->gold;

							// Keep money until the very end
							extract_obj(obj);
						} else {
							if (match_obj == NULL)
								match_obj = obj;
							obj_to_char(obj, ch);
							i++;

							if (container->item_type == ITEM_CORPSE_PC || container->item_type == ITEM_CORPSE_NPC)
								reset_obj(obj);

							p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_GET, NULL,0,0,0,0,0);
							p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_GET, NULL,0,0,0,0,0);
						}

						gotten = true;
					}
				}

				if (i > 0 && match_obj != NULL) {
					sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

					sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
				}


			} else if (!any) {
				if (arg1[3] == '\0')
					act("There is nothing in $P.", ch, NULL, NULL, NULL, container, NULL, NULL, TO_CHAR);
				else
					act("There is no $T in $p.", ch, NULL, NULL, container, NULL, NULL, &arg1[4], TO_CHAR);
			}
		}
			if ((new_gold > 0 || new_silver > 0) && obj_next == NULL){
						give_money(ch, NULL, new_gold, new_silver, true);
					}

			// If the container is in the current room and something was taken
			if( gotten ) {
				if (!container->contains)
				{
					p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_EMPTIED, NULL,CONTEXT_CONTAINER,0,0,0,0);
				}

				if (!container->carried_by && !container->in_obj && container->in_room != NULL ) {

					// Ignore player corpses
					if( container->item_type == ITEM_CORPSE_PC ) return;

					// Ignore mob corpses that have a timer
					//  - static mob corpses can exist, but they won't have a timer on them
					if( container->item_type == ITEM_CORPSE_NPC && container->timer > 0 ) return;

					church_announce_theft(ch, NULL);
				}
			}
			if (!gotten)
			{
				sprintf(buf, "You were unable to take anything from %s.\n\r", container->short_descr);
				send_to_char(buf, ch);
			}
	}
}


void do_put(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MSL];
    char short_descr[MSL];
    OBJ_DATA *obj, *obj_next;
    OBJ_DATA *container;
    OBJ_DATA *match_obj;
    long amount;
    bool gold;
    int i = 0;
    bool found = true;
    OBJ_DATA *any = NULL;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (!str_cmp(arg2,"in") || !str_cmp(arg2,"on"))
		argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
		send_to_char("Put what in what?\n\r", ch);
		return;
    }

    if (!str_cmp(arg2, "all") || !str_prefix("all.", arg2))
    {
		send_to_char("You can't do that.\n\r", ch);
		return;
    }

	// Check if we are doing all.gold or all.silver
    if (!str_prefix("all.", arg1) && (!str_cmp(arg1 + 4, "gold") || !str_cmp(arg1 + 4, "silver")))
    {
		gold = !str_cmp(arg1 + 4, "gold");

		if ((amount = gold ? ch->gold : ch->silver) == 0)
		{
			sprintf(buf, "You don't have any %s.\n\r", gold ? "gold" : "silver");
			send_to_char(buf, ch);
			return;
		}

		if (arg2[0] == '\0')
		{
			send_to_char("Put it in what?\n\r", ch);
			return;
		}

		if ((container = get_obj_here(ch, NULL, arg2)) == NULL)
		{
			act("You can't find a $T to put it in.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR);
			return;
		}

		// Putting money into a bank
		if (container->item_type == ITEM_BANK)
		{
			sprintf(buf, "You put %ld %s coins in $p.", amount, gold ? "gold" : "silver");
			act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			act("$n puts some coins in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);

			act("You hear the sound of jingling coins from $p.",
				ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			act("You hear the sound of jingling coins from $p.",
				ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);

			if (gold)
			{
				ch->gold = 0;
				amount = 95 * amount;
				ch->silver += amount;
			}
			else
			{
				ch->silver = 0;
				amount = (amount * 95)/10000;
				ch->gold += amount;
			}

			sprintf(buf, "You get %ld %s coins from $p.", amount, gold ? "silver" : "gold");
			act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);

			act("$n gets some coins from $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);
			return;
		}

		// Putting money into an existing pile of money
		if (IS_MONEY(container))
		{
			if (gold)
			{
				ch->gold = 0;
				MONEY(container)->gold += amount;
			}
			else
			{
				ch->silver = 0;
				MONEY(container)->silver += amount;
			}


			act("$n puts some coins on $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
			sprintf(buf, "You put %ld %s coins on $P.", amount, gold ? "silver" : "gold");
			act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);

			update_money(container);
			return;
		}

		// Putting money into a container
		//  Look for existing money and glom onto that
		//  If no money in container, add a money object
		if (IS_CONTAINER(container))
		{
			OBJ_DATA *money;

			for(money = container->contains; money; money = money->next)
			{
				if (IS_MONEY(money))
					break;
			}

			// Found a pile of money
			if (IS_VALID(money))
			{
				int g = MONEY(money)->gold;
				int s = MONEY(money)->silver;

				int new_g = gold ? (g + amount) : g;
				int new_s = gold ? s : (s + amount);

				// Temporarily zero out the money
				MONEY(money)->gold = 0;
				MONEY(money)->silver = 0;

				int w = container_get_content_weight(container, NULL);	// Weight without the money
				int new_w = w + get_weight_coins(new_s, new_g);		// Weight of the new money

				if ((w + new_w) > CONTAINER(container)->max_weight)
				{
					// Can't fit, reset the money object back to what it was.
					MONEY(money)->gold = g;
					MONEY(money)->silver = s;

					act("$P cannot hold that much weight.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
					return;
				}

				// Don't need to check the volume because money doesn't contribute to volume usage

				MONEY(money)->gold = new_g;
				MONEY(money)->silver = new_s;

				update_money(money);
			}
			else
			{
				// No money object exists
				int new_g = gold ? amount : 0;
				int new_s = gold ? 0 : amount;

				int w = container_get_content_weight(container, NULL);	// Weight without the money
				int new_w = w + get_weight_coins(new_s, new_g);		// Weight of the new money

				if ((w + new_w) > CONTAINER(container)->max_weight)
				{
					act("$P cannot hold that much weight.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
					return;
				}

				// Don't need to check the volume because money doesn't contribute to volume usage

				money = create_money(new_g, new_s);
				obj_to_obj(money, container);
			}

			if (gold)
				ch->gold = 0;
			else
				ch->silver = 0;

			if (IS_SET(CONTAINER(container)->flags, CONT_PUT_ON))
			{
				act("$n puts some coins on $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
				sprintf(buf, "You put %ld %s coins on $P.", amount, gold ? "silver" : "gold");
				act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
			}
			else
			{
				act("$n puts some coins in $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
				sprintf(buf, "You put %ld %s coins in $P.", amount, gold ? "silver" : "gold");
				act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
			}

			return;
		}

		send_to_char("You can't do that.\n\r", ch);
		return;
    }

	// put # gold|silver <container>
	if (is_number(arg1))
	{
		int amount = atoi(arg1);

		if (amount < 1)
		{
			send_to_char("How much did you want to put?\n\r", ch);
			return;
		}

		bool gold;

		if (!str_prefix(arg2, "gold"))
			gold = true;
		else if (!str_prefix(arg2, "silver"))
			gold = false;
		else
		{
			send_to_char("Put what coins where?\n\r", ch);
			return;
		}

		if ((gold && ch->gold < amount) || (!gold && ch->silver < amount))
		{
			sprintf(buf, "You don't have any %s.\n\r", gold ? "gold" : "silver");
			send_to_char(buf, ch);
			return;
		}

		if (arg3[0] == '\0')
		{
			send_to_char("Put it in what?\n\r", ch);
			return;
		}

		if ((container = get_obj_here(ch, NULL, arg3)) == NULL)
		{
			act("You can't find a $T to put it in.", ch, NULL, NULL, NULL, NULL, NULL, arg3, TO_CHAR);
			return;
		}

		// Putting money into a bank
		if (container->item_type == ITEM_BANK)
		{
			sprintf(buf, "You put %d %s coins in $p.", amount, gold ? "gold" : "silver");
			act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			act("$n puts some coins in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);

			act("You hear the sound of jingling coins from $p.",
				ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			act("You hear the sound of jingling coins from $p.",
				ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);

			if (gold)
			{
				ch->gold -= amount;
				amount = 95 * amount;
				ch->silver += amount;
			}
			else
			{
				ch->silver -= amount;
				amount = (amount * 95)/10000;
				ch->gold += amount;
			}

			sprintf(buf, "You get %d %s coins from $p.", amount, gold ? "silver" : "gold");
			act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);

			act("$n gets some coins from $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM);
			return;
		}

		// Putting money into an existing pile of money
		if (IS_MONEY(container))
		{
			if (gold)
			{
				ch->gold -= amount;
				MONEY(container)->gold += amount;
			}
			else
			{
				ch->silver -= amount;
				MONEY(container)->silver += amount;
			}


			act("$n puts some coins on $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
			sprintf(buf, "You put %d %s coins on $P.", amount, gold ? "silver" : "gold");
			act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);

			update_money(container);
			return;
		}

		// Putting money into a container
		//  Look for existing money and glom onto that
		//  If no money in container, add a money object
		if (IS_CONTAINER(container))
		{
			OBJ_DATA *money;

			for(money = container->contains; money; money = money->next)
			{
				if (IS_MONEY(money))
					break;
			}

			// Found a pile of money
			if (IS_VALID(money))
			{
				int g = MONEY(money)->gold;
				int s = MONEY(money)->silver;

				int new_g = gold ? (g + amount) : g;
				int new_s = gold ? s : (s + amount);

				// Temporarily zero out the money
				MONEY(money)->gold = 0;
				MONEY(money)->silver = 0;

				int w = container_get_content_weight(container, NULL);	// Weight without the money
				int new_w = w + get_weight_coins(new_s, new_g);		// Weight of the new money

				if ((w + new_w) > CONTAINER(container)->max_weight)
				{
					// Can't fit, reset the money object back to what it was.
					MONEY(money)->gold = g;
					MONEY(money)->silver = s;

					act("$P cannot hold that much weight.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
					return;
				}

				// Don't need to check the volume because money doesn't contribute to volume usage

				MONEY(money)->gold = new_g;
				MONEY(money)->silver = new_s;

				update_money(money);
			}
			else
			{
				// No money object exists
				int new_g = gold ? amount : 0;
				int new_s = gold ? 0 : amount;

				int w = container_get_content_weight(container, NULL);	// Weight without the money
				int new_w = w + get_weight_coins(new_s, new_g);		// Weight of the new money

				if ((w + new_w) > CONTAINER(container)->max_weight)
				{
					act("$P cannot hold that much weight.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
					return;
				}

				// Don't need to check the volume because money doesn't contribute to volume usage

				money = create_money(new_g, new_s);
				obj_to_obj(money, container);
			}

			if (gold)
				ch->gold -= amount;
			else
				ch->silver -= amount;

			if (IS_SET(CONTAINER(container)->flags, CONT_PUT_ON))
			{
				act("$n puts some coins on $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
				sprintf(buf, "You put %d %s coins on $P.", amount, gold ? "gold" : "silver");
				act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
			}
			else
			{
				act("$n puts some coins in $P.",ch, NULL, NULL,NULL,container, NULL, NULL, TO_ROOM);
				sprintf(buf, "You put %d %s coins in $P.", amount, gold ? "gold" : "silver");
				act(buf,ch, NULL, NULL,NULL,container, NULL, NULL, TO_CHAR);
			}

			return;
		}

		send_to_char("You can't do that.\n\r", ch);
		return;
	}

    if ((container = get_obj_inv(ch, arg2, false)) == NULL)
    {
		act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR);
		return;
    }

	if (!IS_CONTAINER(container))
	{
		send_to_char("That is not a container.\n\r", ch);
		return;
	}

    /* 'put obj container' */
    if (str_cmp(arg1, "all") && str_prefix("all.", arg1))
    {
		if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
		{
		    send_to_char("You do not have that item.\n\r", ch);
		    return;
		}

		if (!can_put_obj(ch, obj, container, NULL, false))
		    return;

		if (!container_can_fit_weight(container, obj))
		{
			act("$P cannot hold that much weight.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR);
			return;
		}

		if (!container_can_fit_volume(container, obj))
		{
			act("$P is too full to hold $p.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR);
			return;
		}


#if 0
		// TODO: Could be moved to "can_put_obj" ?
		// TODO: I think this has been completed?
		if (container->item_type == ITEM_KEYRING)
		{
		    if (obj->item_type != ITEM_KEY)
		    {
				act("You can only attach keys to $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
		        return;
		    }
		    else
		    {
				OBJ_DATA *key;
				int i;

				// TODO: Moved
				// TODO: Just say you can't put it on there.
				if (IS_SET(obj->extra[0], ITEM_NOKEYRING))
				{
					act("You try to attach $p to the keyring, but it recoils with a shock of energy.",
						ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
					act("$n tries to attach $p to $s keyring, but it recoils with a shock of energy.",
						ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
					return;
				}

				// TODO: done
				// TODO: widevnum
				// TODO: add "no_duplicate" flag
				i = 0;
				for (key = container->contains; key != NULL; key = key->next_content)
				{
			   	    if (obj->pIndexData->vnum == key->pIndexData->vnum)
				    {
						act("$p is already on $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);
						return;
				    }
				    i++;
				}

				// TODO: Just use the max volume
				if (i >= 50)
				{
				    act("$p is already rather full of keys!",
		        		ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		    		return;
				}

				act("You attach $p to $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);
				act("$n attaches $p to $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM);

				obj_from_char(obj);
				obj_to_obj(obj, container);

				p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_PUT, NULL,0,0,0,0,0);
				p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PUT, NULL,0,0,0,0,0);

				i++;
				if (i >= 50)
				{
					p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_FILLED, NULL,0,0,0,0,0);
				}
				return;
		    }
		}
#endif

        /* Orb of Shadows makes 1 item perm cursed */
		// TODO: Make into scripting (PREPUT)
		/* KEEP HERE FOR REFERENCE
		if (container->pIndexData == obj_index_cursed_orb)
		{
		    if (obj->item_type == ITEM_CONTAINER)
		    {
		        act("You can't seem to get $p into the orb.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				return;
		    }

			act("You put $p into the Orb of Shadows.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("$n puts $p into the Orb of Shadows.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You hear demonic chants and whispers from the Orb of Shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("You hear demonic chants and whispers from $n's Orb of Shadows.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			act("You retrieve $p from the Orb of Shadows.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			act("$n retrieves $p from the Orb of Shadows.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

			act("The Orb of Shadows dissipates into smoke.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("$n's Orb of Shadows dissipates into smoke.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

			SET_BIT(obj->extra[0], ITEM_NODROP);
			SET_BIT(obj->extra[0], ITEM_NOUNCURSE);
			extract_obj(container);
			return;
		}
		*/

		
		if (!container_can_fit_weight(container, obj))
		{
			act("$P cannot hold that much weight.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR);
			return;
		}

		if (!container_can_fit_volume(container, obj))
		{
			act("$P is too full to hold $p.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR);
			return;
		}

		obj_from_char(obj);
		obj_to_obj(obj, container);

		if (IS_SET(CONTAINER(container)->flags,CONT_PUT_ON))
		{
		    act("$n puts $p on $P.",ch, NULL, NULL,obj,container, NULL, NULL, TO_ROOM);
		    act("You put $p on $P.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR);
		}
		else
		{
		    act("$n puts $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM);
		    act("You put $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR);
		}

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, container, NULL, TRIG_PUT, NULL,0,0,0,0,0);
		p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_PUT, NULL,0,0,0,0,0);

		if (!container_can_fit_weight(container, NULL) || !container_can_fit_volume(container, NULL))
		{
			p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_FILLED, NULL, CONTEXT_CONTAINER,0,0,0,0);
		}
    }
    else
    {
		/* Put all/all.<obj> <container> */

		if (IS_SET(CONTAINER(container)->flags, CONT_SINGULAR))
		{
			act("You can only put items in $p one at a time.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		while (found)
		{
			found = false;
			i = 0;
			match_obj = NULL;

		    for (obj = ch->carrying; obj != NULL; obj = obj_next)
		    {
				obj_next = obj->next_content;

				if (arg1[3] == '\0' || is_name(&arg1[4], obj->name))
				    any = obj;

				if (any == obj && can_put_obj(ch, obj, container, NULL, true))
				{
				    sprintf(short_descr, "%s", obj->short_descr);
				    found = true;
				    break;
				}
		    }

		    if (found)
		    {
				for (obj = ch->carrying; obj != NULL; obj = obj_next)
				{
				    obj_next = obj->next_content;

				    if (str_cmp(obj->short_descr, short_descr) ||
				    	!can_put_obj(ch, obj, container, NULL, false))
						continue;

					if (!container_can_fit_weight(container, obj))
				    {
						if (i > 0 && match_obj != NULL)
						{
						    if (IS_SET(CONTAINER(container)->flags,CONT_PUT_ON))
						    {
								sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
								act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

								sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
								act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
							}
						    else
						    {
							sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

							sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
						}
					}

					act("$p is full.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
					return;
			    }

				if (!container_can_fit_volume(container, obj))
			    {
					if (i > 0 && match_obj != NULL)
					{
					    if (IS_SET(CONTAINER(container)->flags,CONT_PUT_ON))
					    {
							sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

							sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
						}
						else
						{
							sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

							sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
							act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
					    }
					}

					act("$p can't hold any more.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
					return;
				}

			    if (match_obj == NULL && obj != NULL)
					match_obj = obj;

				obj_from_char(obj);
				obj_to_obj(obj, container);
				i++;
			}

			if (i > 0 && match_obj != NULL)
			{
				if (IS_SET(CONTAINER(container)->flags,CONT_PUT_ON))
				{
					sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

					sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
				}
				else
				{
					sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM);

					sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
					act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR);
				}
			}

			/* Too many to do individually, just let it handle all of them. */
			p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_PUT, NULL,0,0,0,0,0);

			if (!container_can_fit_weight(container, NULL) || !container_can_fit_volume(container, NULL))
			{
				p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_FILLED, NULL,CONTEXT_CONTAINER,0,0,0,0);
			}
	    }

	    if (!any)
	    {
			if (arg1[3] == '\0')
			    act("You have nothing you can put in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR);
			else
			    act("You're not carrying any $T you can put in $p.", ch, NULL, NULL, container, NULL, NULL, &arg1[4], TO_CHAR);
			}
		}
    }
}


void do_drop(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MSL];
    char buf[2*MAX_STRING_LENGTH];
    char short_descr[MSL];
    OBJ_DATA *obj, *obj_next;
    OBJ_DATA *match_obj;
    bool found = true;
    ROOM_INDEX_DATA *room = ch->in_room;
    CHURCH_DATA *church;
    int i = 0;
    long gold = 0;
    long silver = 0;
    OBJ_DATA *any = NULL;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0')
    {
		send_to_char("Drop what?\n\r", ch);
		return;
    }

    if (is_dead(ch))
		return;

    if (IS_SOCIAL(ch) && !IS_IMMORTAL(ch))
    {
		send_to_char("You can't do that here.\n\r", ch);
		return;
    }

    /* Handle money dropping */
    if ((!str_prefix("all.", arg) && (!str_cmp(arg+4, "gold") || !str_cmp(arg+4, "silver") || !str_cmp(arg+4, "coins"))) ||
		(is_number(arg) && (!str_cmp(arg2, "gold") || !str_cmp(arg2, "silver"))))
    {
	if (!str_prefix("all.", arg))
	{
	    if (!str_cmp(arg+4, "gold") || !str_cmp(arg+4, "coins"))
		gold = ch->gold;

            if (!str_cmp(arg+4, "silver") || !str_cmp(arg+4, "coins"))
		silver = ch->silver;

	    if (gold == 0 && silver == 0) {
		send_to_char("You're flat broke.\n\r", ch);
		return;
	    }
	}
	else
	{
	    if (atol(arg) <= 0)
	    {
		send_to_char("You can't do that.\n\r", ch);
		return;
	    }

	    if (!str_cmp(arg2, "gold"))
		gold = atol(arg);

            if (!str_cmp(arg2, "silver"))
		silver = atol(arg);
	}

	if ((gold > 0 && ch->gold < gold) || (silver > 0 && ch->silver < silver))
	{
	    send_to_char("You haven't got that much.\n\r", ch);
	    return;
	}

	ch->gold -= gold;
	ch->silver -= silver;

	obj = create_money(gold, silver);
	act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n drops some coins.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	obj_to_room(obj, ch->in_room);
	return;
    }

    /* Drop <obj> */
    if (str_cmp(arg, "all") && str_prefix("all.", arg))
    {
	OBJ_DATA *cart;

	/* Drop a cart */
	if (ch->pulled_cart != NULL && is_name(arg, ch->pulled_cart->name))
	{
	    if(p_percent_trigger(NULL, ch->pulled_cart, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL,0,0,0,0,0))
		return;


	    if (MOUNTED(ch))
	    {
	   	act("You untie $p from your mount.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
	   	act("$n unties $p from $s mount.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    {
	   	act("You stop pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
	   	act("$n stops pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM);
	    }

		cart = ch->pulled_cart;
		ch->pulled_cart = NULL;
		cart->pulled_by = NULL;

	    room = ch->in_room;

	    /* If they try to stash a relic in housing it vanishes */
		// TODO: Make housing a set of districts.
		// 
	    if (is_relic(cart->pIndexData)
	    &&  !str_cmp(ch->in_room->area->name, "Housing"))
	    {
		ROOM_INDEX_DATA *room;

		act("$p vanishes in a mysterious purple haze.", ch, NULL, NULL, cart, NULL, NULL, NULL, TO_ALL);

		/* Housing in first continent only so people can't use this
		   to transport it to second continent */
		room = get_random_room(ch, 1);
		if (room == NULL)
		    room = room_index_temple;
	    }

	    obj_from_room(cart);
	    obj_to_room(cart, room);

	if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
	    sprintf(buf, "%s drops %s.", ch->name, cart->short_descr);
	    log_string(buf);
	    wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
	}


		p_percent_trigger(NULL, cart, NULL, NULL, ch, NULL, NULL, cart, NULL, TRIG_DROP, NULL,0,0,0,0,0);
		p_give_trigger(NULL, NULL, ch->in_room, ch, cart, TRIG_DROP,0,0,0,0,0);

	    return;
	}

	/* Awful hack to make it so people don't load up the perm-objs list too much. Will fix later. */
	for (church = church_list; church != NULL; church = church->next)
	{
	    if (is_treasure_room(church, room))
	    {
		if (ch->church == church)
		    send_to_char("Donations to the treasure room must be made using the church donate command.\n\r", ch);
		else
		    act("Only members of $t may donate to it.", ch, NULL, NULL, NULL, NULL, church->name, NULL, TO_CHAR);

		return;
	    }
	}

	if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
	{
	    send_to_char("You do not have that item.\n\r", ch);
	    return;
	}

	if (!can_drop_obj(ch, obj, false) || IS_SET(obj->extra[1], ITEM_KEPT)) {
	    send_to_char("You can't let go of it.\n\r", ch);
	    return;
	}

	if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL,0,0,0,0,0))
		return;

	obj_from_char(obj);
	obj_to_room(obj, ch->in_room);
	act("$n drops $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
	    sprintf(buf, "%s drops %s.", ch->name, obj->short_descr);
	    log_string(buf);
	    wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
	}

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_DROP, NULL,0,0,0,0,0);
	    p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_DROP,0,0,0,0,0);

	if (obj && IS_OBJ_STAT(obj,ITEM_MELT_DROP))
	{
	    act("$p dissolves into smoke.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
	    act("$p dissolves into smoke.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
	    extract_obj(obj);
	}

	else if (obj && ch->in_room->sector_type == SECT_ENCHANTED_FOREST)
	{
	    act("$p crumbles into dust.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	    act("$p crumbles into dust.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    extract_obj(obj);
	}
    }
    else
    {
        /* Drop all/all.<obj> */
	for (church = church_list; church != NULL; church = church->next)
	{
	    if (is_treasure_room(church, room))
	    {
		if (ch->church == church)
		    send_to_char("Donations to the treasure room must be made using the church donate command.\n\r", ch);
		else
		    act("Only members of $t may donate to it.", ch, NULL, NULL, NULL, NULL, church->name, NULL, TO_CHAR);

		return;
	    }
	}

	while (found)
	{
	    found = false;
	    i = 0;
	    match_obj = NULL;

	    for (obj = ch->carrying; obj != NULL; obj = obj_next)
	    {
		obj_next = obj->next_content;

		if (arg[3] == '\0' || is_name(&arg[4], obj->name))
		    any = obj;

		if (any == obj && can_drop_obj(ch, obj, true) && !IS_SET(obj->extra[1], ITEM_KEPT) &&
			!p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, "silent",0,0,0,0,0))
		{
		    sprintf(short_descr, "%s", obj->short_descr);
		    found = true;
		    break;
		}
	    }

	    if (found)
	    {
		for (obj = ch->carrying; obj != NULL; obj = obj_next)
		{
		    obj_next = obj->next_content;

		    if (str_cmp(obj->short_descr, short_descr)
		    ||  !can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
			continue;

		if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL,0,0,0,0,0))
			continue;

		    if (match_obj == NULL && obj != NULL)
			match_obj = obj;

		    obj_from_char(obj);
		    obj_to_room(obj, ch->in_room);
		    i++;

			if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
			    sprintf(buf, "%s drops %s.", ch->name, obj->short_descr);
			    log_string(buf);
			    wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
			}

			p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_DROP, NULL,0,0,0,0,0);
		    p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_DROP,0,0,0,0,0);


		    if (IS_SET(obj->extra[0], ITEM_MELT_DROP))
			extract_obj(obj);

		    else if (ch->in_room->sector_type == SECT_ENCHANTED_FOREST)
			extract_obj(obj);
		}

		if (i > 0 && match_obj != NULL)
		{
		    sprintf(buf, "{Y({G%2d{Y) {x$n drops $p.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);

		    sprintf(buf, "{Y({G%2d{Y) {xYou drop $p.", i);
		    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);

		    if (IS_SET(match_obj->extra[0], ITEM_MELT_DROP))
		    {
			short_descr[0] = UPPER(short_descr[0]);
			sprintf(buf, "{Y({G%2d{Y) {x%s vanishes in a puff of smoke.", i, short_descr);
			act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
			act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);
		    }

		    else if (ch->in_room->sector_type == SECT_ENCHANTED_FOREST)
		    {
			short_descr[0] = UPPER(short_descr[0]);
			sprintf(buf, "{Y({G%2d{Y) {x%s crumbles into dust.", i, short_descr);
			act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM);
			act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
		    }
		}
	    }
	    else if (!any)
	    {
		if (arg[3] == '\0')
		    act("You aren't carrying anything you can drop.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		else
		    act("You're not carrying any $T.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR);
	    }
	}
    }
}


void do_give(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char short_descr[MSL];
    CHAR_DATA *victim;
    OBJ_DATA *obj, *obj_next;
    OBJ_DATA *match_obj;
    int i = 0;
    bool found = true;
    OBJ_DATA *any = false;
    long amount;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Give what to whom?\n\r", ch);
	return;
    }

    if (is_dead(ch))
	return;

    if (IS_SOCIAL(ch) && !IS_IMMORTAL(ch))
    {
	send_to_char("You can't do that here.\n\r", ch);
	return;
    }

    if ((is_number(arg1) && (!str_cmp(arg2, "silver") || !str_cmp(arg2, "gold")))
    ||  !str_cmp(arg1, "all.gold")
    ||  !str_cmp(arg1, "all.silver"))
    {
	bool gold;

	if ((victim = get_char_room(ch, NULL, !str_prefix("all.", arg1) ? arg2 : arg3)) == NULL)
	{
	    send_to_char("They aren't here.\n\r", ch);
	    return;
	}

	if (victim == ch) {
	    send_to_char("Whatever for?\n\r", ch);
	    return;
	}

	if (is_number(arg1))
	    gold = !str_cmp(arg2, "gold");
	else
	    gold = !str_cmp(arg1 + 4, "gold");

	if (!str_prefix("all.", arg1))
	{
	    if (gold)
		amount = ch->gold;
	    else
		amount = ch->silver;
	}
	else
	    amount = atol(arg1);

	if (amount <= 0)
	{
	    send_to_char("You can't do that.\n\r", ch);
	    return;
	}

        if ((gold && ch->gold < amount) || (!gold && ch->silver < amount))
	{
	    send_to_char("You haven't got that much.\n\r", ch);
	    return;
	}

	if (gold)
	    obj = create_money(amount, 0);
	else
   	    obj = create_money(0, amount);

	if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim)
	&& !(IS_NPC(victim) && (IS_SET(victim->act[0],ACT_IS_CHANGER)
			    || IS_SET(victim->act[0],ACT_IS_BANKER))))
	{
	    act("$p: $N can't carry that much weight.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    extract_obj(obj);
	    return;
	}

	if (gold)
	{
     	    ch->gold -= amount;
	    victim->gold += amount;
	}
	else
	{
   	    ch->silver -= amount;
   	    victim->silver += amount;
	}

	sprintf(buf,"$n gives you %ld %s.",amount, gold ? "gold" : "silver");
	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	act("$n gives $N some coins.",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	sprintf(buf,"You give $N %ld %s.",amount, gold ? "gold" : "silver");
	act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	/* Bribe trigger */
	p_bribe_trigger(victim, ch, !gold ? amount : amount * 100,0,0,0,0,0);

        if (IS_NPC(victim) && IS_SET(victim->act[0],ACT_IS_CHANGER))
	    change_money(ch, victim, gold ? amount : 0, !gold ? amount : 0);

	if (IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_IMMORTAL(victim))
	{
	    sprintf(buf,"%s gives %s %ld %s.",
	        ch->name, IS_NPC(victim) ? victim->short_descr : victim->name,
		amount, gold ? "gold" : "silver");
	    log_string(buf);
	    wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
	}

	return;
    }

    if ((victim = get_char_room(ch, NULL, arg2)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim == ch) {
	send_to_char("Whatever for?\n\r", ch);
	return;
    }

    if (IS_DEAD(victim))
    {
	act("$N is dead. You can't give $M anything.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    /* Give all.<item> <person> */
    if (!str_prefix("all.", arg1))
    {
	strcpy(arg1, arg1+4);

	while (found)
	{
	    found = false;
	    i = 0;
	    match_obj = NULL;

	    for (obj = ch->carrying; obj != NULL; obj = obj_next)
	    {
		obj_next = obj->next_content;

		if (is_name(arg1, obj->name))
		    any = obj;

		if (any == obj && can_give_obj(ch, obj, victim, true))
		{
		    sprintf(short_descr, obj->short_descr);
		    found = true;
		    break;
		}
	    }

	    if (found)
	    {
		for (obj = ch->carrying; obj != NULL; obj = obj_next)
		{
		    obj_next = obj->next_content;

		    if (str_cmp(obj->short_descr, short_descr)
		    ||  !can_give_obj(ch, obj, victim, true))
			continue;

		    if (victim->carry_number + get_obj_number(obj) >
			    can_carry_n(victim))
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT);

			    sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT);

			    sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
			}

			act("$N has $S hands full.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			found = false;
		    }

		    if (get_carry_weight(victim) + get_obj_weight(obj)
			    > can_carry_w(victim))
		    {
			if (i > 0 && match_obj != NULL)
			{
			    sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT);

			    sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT);

			    sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
			    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);
			}

			act("$N can't carry any more weight.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			found = false;
		    }

		    if (match_obj == NULL && obj != NULL)
			match_obj = obj;

		    reset_obj(obj);
		    obj_from_char(obj);
		    obj_to_char(obj, victim);
		    i++;

			p_give_trigger(NULL, obj, NULL, ch, obj, TRIG_GIVE,0,0,0,0,0);
			p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_GIVE,0,0,0,0,0);
			p_give_trigger(victim, NULL, NULL, ch, obj, TRIG_GIVE,0,0,0,0,0);

	            if (!found)
			return;
		}

		if (i > 0 && match_obj != NULL)
		{
		    sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
		    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT);

		    sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
		    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT);

		    sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
		    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR);

			//p_give_trigger(NULL, match_obj, NULL, ch, match_obj, TRIG_GIVE,0,0,0,0,0);
			//p_give_trigger(NULL, NULL, ch->in_room, ch, match_obj, TRIG_GIVE,0,0,0,0,0);
			//p_give_trigger(victim, NULL, NULL, ch, match_obj, TRIG_GIVE,0,0,0,0,0);
		}
	    }
	    else if (!any)
		act("You aren't carrying any $t which you can give $M.", ch, victim, NULL, NULL, NULL, arg1, NULL, TO_CHAR);
	}

	return;
    }

    /* Give <item> <person> */
    if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
    {
	send_to_char("You do not have that item.\n\r", ch);
	return;
    }

    if (!can_give_obj(ch, obj, victim, false))
	return;

    if (victim->carry_number + get_obj_number(obj) > can_carry_n(victim))
    {
	act("$N has $S hands full.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim))
    {
	act("$N can't carry that much weight.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    obj_from_char(obj);
    obj_to_char(obj, victim);
    MOBtrigger = false;
    act("$n gives $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
    act("$n gives you $p.",   ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT   );
    act("You give $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR   );
    MOBtrigger = true;

    if (IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_IMMORTAL(victim))
    {
        sprintf(buf, "%s gives %s to %s.",
			ch->name,
			obj->short_descr,
			IS_NPC(victim) ? victim->short_descr : victim->name);
		log_string(buf);
		wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
    }

    /* Give trigger */
	p_give_trigger(NULL, obj, NULL, ch, obj, TRIG_GIVE,0,0,0,0,0);
	p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_GIVE,0,0,0,0,0);
	p_give_trigger(victim, NULL, NULL, ch, obj, TRIG_GIVE,0,0,0,0,0);
}


void change_money(CHAR_DATA *ch, CHAR_DATA *changer, long gold, long silver)
{
    char buf[MSL];
    long change;

    if (gold > 0)
	change = 95 * gold;
    else
	change = (95 * silver)/10000;

    if (change < 1)
	act("{R$n tells you 'I'm sorry, you did not give me enough to change.{x'", changer, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    else
    {
	if (gold && changer->silver < change)
	    changer->silver = change;

        if (silver && changer->gold < change)
	    changer->gold = change;

	sprintf(buf,"%ld %s %s", change, gold ? "silver" : "gold", ch->name);
	do_function(changer, &do_give, buf);

	act("{R$n tells you 'Thank you, come again.{x'", changer, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    }
}

void do_donate(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    CHAR_DATA *prev;

    char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Donate what?\n\r",ch);
	return;
    }

    if (ch->position == POS_FIGHTING)
    {
	send_to_char("You're fighting!\n\r",ch);
	return;
    }

    if ((obj = get_obj_carry (ch, arg, ch)) == NULL)
    {
	send_to_char("You do not have that item.\n\r",ch);
	return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
	send_to_char("You can't let go of it.\n\r",ch);
	return;
    }

    if (IS_SET(obj->extra[1], ITEM_NO_DONATE))
    {
	act("You can't donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (IS_SET(obj->extra[1], ITEM_KEPT)) {
	send_to_char("You can't donate kept items.\n\r", ch);
	return;
    }

    if (obj->item_type == ITEM_CORPSE_NPC
    || obj->item_type == ITEM_CORPSE_PC
    || obj->owner     != NULL
    || IS_OBJ_STAT(obj,ITEM_MELT_DROP)
    || obj->timer > 0)
    {
	send_to_char("You can't donate that!\n\r",ch);
	return;
    }

    if (obj->contains != NULL) {
        act("You must empty $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (wnum_match_room(room_wnum_donation, ch->in_room))
    {
	send_to_char("You're already here, just drop it.\n\r",ch);
	return;
    }

    act("$n donates $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
    act("You donate $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);

	obj->cost = 0;

    obj_from_char(obj);
    obj_to_room(obj, room_index_donation);

    for (prev = obj->in_room->people; prev; prev = prev->next_in_room)
	send_to_char("{MYou hear a loud zap as an object drops from the shimmering rift onto the rug.{x\n\r", prev);
}


void do_repair(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    char arg[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int sk;
    CHAR_DATA *pMob;
    long cost;

    argument = one_argument(argument, arg);

    /* first check if they can repair it themselves */
    if ((sk = get_skill(ch, gsk_repair)) > 0)
    {
        if (arg[0] == '\0')
	{
	    send_to_char("Repair what item?\n\r", ch);
	    return;
	}

        obj = get_obj_carry(ch, arg, ch);
	if (obj == NULL)
	{
  	    send_to_char("You don't see that anywhere around.\n\r", ch);
	    return;
	}

	if (obj->condition >= 100)
	{
	    act("$p is already in perfect condition.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	if (obj->timer > 0)
	{
	    act("$p is too badly damaged and will crumble any second.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	if (obj->times_fixed >= obj->times_allowed_fixed)
	{
   	    act("$p is beyond repair.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	act("{YYou begin to repair $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("{Y$n begins to repair to $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	ch->repair = sk / 5 + number_range(1, 10);
	ch->repair_obj = obj;
	ch->repair_amt = UMIN(100 - obj->condition,
			       sk / 10 + number_range(1,3));
	return;
    }

    for (pMob = ch->in_room->people;
          pMob != NULL;
	  pMob = pMob->next_in_room)
    {
        if (IS_SET(pMob->act[0], ACT_BLACKSMITH))
            break;
    }

    if (pMob == NULL)
    {
        send_to_char("There is no blacksmith here.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("Syntax: repair <item>\n\r"
                     "For more help: help repair\n\r", ch);
        return;
    }

    if ((obj = get_obj_carry(ch, arg, ch)) != NULL)
    {
        if (obj->times_fixed >= obj->times_allowed_fixed)
	{
	    act("{C$n says 'I'm sorry $N, that item is beyond repair.'{x", pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	    return;
	}

	if (obj->condition >= 100)
	{
	    act("{C$n says 'There would be no point in repairing that item. It's in good condition.'{x",
	        pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	    return;
	}

	cost = obj->level * 100;
	if ((ch->gold * 100 + ch->silver) < cost)
	{
	    sprintf(buf, "{C$n says '$N, you will need %d silver for me to repair that item.'{x", obj->level * 100);
	    act(buf, pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	    return;
	}

	act("$n gives $p to $N.", ch, pMob, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
	act("$n gives you $p.",   ch, pMob, NULL, obj, NULL, NULL, NULL, TO_VICT   );
	act("You give $p to $N.", ch, pMob, NULL, obj, NULL, NULL, NULL, TO_CHAR   );

	act("$n tinkers with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_NOTVICT);
	act("$n tinkers with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_VICT   );
	act("You tinker with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_CHAR   );

	act("$n pockets some coins.", pMob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	deduct_cost(ch, cost);
	obj->times_fixed++;
	obj->condition = 100;
    }
    else
    {
        send_to_char("The item must be in your inventory.\n\r", ch);
    }

    return;
}


void do_restring(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    CHAR_DATA *mob;
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    long cost;
    bool norestring = false;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_SET(mob->act[0], ACT_IS_RESTRINGER))
            break;
    }

    if (mob == NULL)
    {
        send_to_char("There is no restringer here.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0' || arg2[0] == '\0' || (argument[0] == '\0' && str_cmp(arg2, "desc")))
    {
		send_to_char("Syntax: restring item short <new name>\n\r", ch);
		send_to_char("        restring item long  <new name>\n\r", ch);
		send_to_char("        restring item desc  (for the description)\n\r", ch);
        return;
    }

    if (str_cmp(arg2, "desc") && strlen_no_colours(argument) < 5)
    {
		act("{R$N tells you, 'Surely you can think of a better name than that! It's too short!'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
    }

    if ((obj = get_obj_inv_only(ch, arg1, true)) == NULL)
    {
		act("{R$N tells you, 'You don't have that item.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
    }

    if (IS_SET(obj->extra[0], ITEM_NORESTRING) || CAN_WEAR(obj, ITEM_WEAR_TABARD))
    {
		// Allow color changes to SHORTS on NORESTRING.
		if( str_cmp(arg2, "short") || str_cmp_nocolour(obj->short_descr, argument)) {
	    	act("{R$N tells you, 'Sorry, but you can't restring $p.'{x", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}

    	norestring = true;
    }

    cost = 99 + UMAX(1, obj->pIndexData->cost/ 1000 + obj->level / 10);
    if ((ch->gold * 100 + ch->silver) < cost)
    {
		sprintf(buf, "{R$N tells you, 'You don't have enough money. It would cost %ld silver coins to restring it.'{x", cost);
	 	act (buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
        return;
    }

    if (!str_cmp(arg2, "long"))
    {
		if (obj->old_description == NULL)
			obj->old_description = obj->description;
		else
			free_string(obj->description);	// The object has already been restrung

		act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		strcat(argument, "{x");

		obj->description = str_dup(argument);
		act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		sprintf(buf, "The long description has been changed to %s\n\r", obj->description);
		send_to_char(buf, ch);

		do_say(mob, "Nice doin' business with ya bub.");

		deduct_cost(ch, cost);
		return;
	}

	if (!str_cmp("short", arg2))
	{
		act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		if (obj->old_short_descr == NULL)
			obj->old_short_descr = obj->short_descr;
		else
			free_string(obj->short_descr);	// The object has already been restrung

		strcat(argument, "{x");

		obj->short_descr = str_dup(argument);
		act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		sprintf(buf, "The short description has been changed to %s\n\r",
			obj->short_descr);
		send_to_char(buf, ch);

		do_say(mob, "Nice doin' business with ya bub.");

		deduct_cost(ch, cost);

		// NORESTRING objects PRESERVE the name.
		if (!norestring) {
			if (obj->old_name == NULL)
				obj->old_name = obj->name;
			else
				free_string(obj->name);	// The object has already been restrung

			obj->name = short_to_name(obj->short_descr);
		}
		return;
    }

    if (!str_cmp(arg2, "desc"))
    {
		act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		if (obj->old_short_descr == NULL)
			obj->old_full_description = obj->full_description;
		else
			free_string(obj->full_description);

		obj->full_description = str_dup("");
		string_append(ch, &obj->full_description);

		deduct_cost(ch, cost);
		return;
    }

    send_to_char("Syntax: restring item short <new name>\n\r", ch);
    send_to_char("        restring item long  <new name>\n\r", ch);
    send_to_char("        restring item desc  (for the description)\n\r", ch);
}


void do_unrestring(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    CHAR_DATA *mob;
    char arg[MAX_STRING_LENGTH];
//    long cost;

    argument = one_argument(argument, arg);

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_SET(mob->act[0], ACT_IS_RESTRINGER))
            break;
    }

    if (mob == NULL)
    {
        send_to_char("There is no restringer here.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Syntax: unrestring <item>\n\r", ch);
        return;
    }

    if ((ch->gold * 100 + ch->silver) < 500)
    {
        act("{R$N tells you, 'You don't have enough money.'{x",
        	ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
        return;
    }

    if ((obj = get_obj_inv_only(ch, arg, true)) == NULL)
    {
		act("{R$N tells you, 'You don't have that item.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
    }

    act("You hand $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$n hands $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    act("$N tinkers with $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$N tinkers with $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);

    if( obj->old_name )
    {
	    free_string(obj->name);
	    obj->name = obj->old_name;
	    obj->old_name = NULL;
	}

    if( obj->old_short_descr )
    {
	    free_string(obj->short_descr);
	    obj->short_descr = obj->old_short_descr;
	    obj->old_short_descr = NULL;
	}

    if( obj->old_description )
    {
	    free_string(obj->description);
	    obj->description = obj->old_description;
	    obj->old_description = NULL;
	}

    if( obj->old_full_description )
    {
	    free_string(obj->full_description);
	    obj->full_description = obj->old_full_description;
	    obj->old_full_description = NULL;
	}

    act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM);
}


void do_envenom(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    AFFECT_DATA af;
    int percent,skill;

    if (get_skill(ch, gsk_envenom) == 0)
    {
	send_to_char("What?\n\r", ch);
	return;
    }

    if (argument[0] == '\0')
    {
	send_to_char("Envenom what item?\n\r",ch);
	return;
    }

    obj =  get_obj_list(ch,argument,ch->carrying);

    if (obj== NULL)
    {
	send_to_char("You don't have that item.\n\r",ch);
	return;
    }

    if ((skill = get_skill(ch, gsk_envenom)) < 1)
    {
	send_to_char("Are you crazy? You'd poison yourself!\n\r",ch);
	return;
    }

    if (IS_FOOD(obj) || IS_FLUID_CON(obj))
    {
		if (IS_OBJ_STAT(obj,ITEM_BLESS) || IS_OBJ_STAT(obj,ITEM_BURN_PROOF))
		{
			act("You fail to poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			return;
		}

		if (number_percent() < skill)  /* success! */
		{
			/* The better you get, the less likely people SEE it
				But, even mastered, there is a slight chance of people seeing */
			if(number_range(0,100) > skill)
			{
				act("$n treats $p with deadly poison.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
			}

			act("You treat $p with deadly poison.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);


			int poison = URANGE(1, skill / 3, 99);		// Never allow this kind of applied poison to be permanent.
			bool applied = false;
			if (IS_FOOD(obj))
			{
				if (FOOD(obj)->poison > poison)
				{
					act("$p is already sufficiently poisoned.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
					return;
				}

				applied = !FOOD(obj)->poison;
				FOOD(obj)->poison = poison;
			}
			else if (IS_FLUID_CON(obj))
			{
				if (FLUID_CON(obj)->poison > poison)
				{
					act("$p is already sufficiently poisoned.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);
					return;
				}

				applied = !FLUID_CON(obj)->poison;
				FLUID_CON(obj)->poison = poison;
			}

			act("$p is infused with poisonous vapors.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ALL);

			if (applied)
				check_improve(ch,gsk_envenom,true,4);
			WAIT_STATE(ch,gsk_envenom->beats);
			return;
		}

		act("You fail to poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
		if (!obj->value[3])
			check_improve(ch,gsk_envenom,false,4);
		WAIT_STATE(ch,gsk_envenom->beats);
		return;
	}

	memset(&af,0,sizeof(af));
    if (IS_WEAPON(obj))
    {
        if (IS_WEAPON_STAT(obj,WEAPON_FLAMING)
        ||  IS_WEAPON_STAT(obj,WEAPON_FROST)
        ||  IS_WEAPON_STAT(obj,WEAPON_VAMPIRIC)
        ||  IS_WEAPON_STAT(obj,WEAPON_VORPAL)
        ||  IS_WEAPON_STAT(obj,WEAPON_SHOCKING)
        ||  IS_WEAPON_STAT(obj,WEAPON_ACIDIC)
        ||  IS_WEAPON_STAT(obj,WEAPON_RESONATE)
        ||  IS_WEAPON_STAT(obj,WEAPON_BLAZE)
        ||  IS_WEAPON_STAT(obj,WEAPON_SUCKLE)
        ||  IS_OBJ_STAT(obj,ITEM_BLESS) || IS_OBJ_STAT(obj,ITEM_BURN_PROOF))
        {
            act("You can't seem to envenom $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
            return;
        }

	if (obj->value[3] < 0
	||  attack_table[obj->value[3]].damage == DAM_BASH)
	{
	    send_to_char("You can only envenom edged weapons.\n\r",ch);
	    return;
	}

        if (IS_WEAPON_STAT(obj,WEAPON_POISON))
        {
            act("$p is already envenomed.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
            return;
        }

	percent = number_percent();
	if (percent < skill)
	{
            af.where     = TO_WEAPON;
            af.group     = AFFGROUP_WEAPON;
            af.skill     = gsk_poison;
            af.level     = ch->tot_level * percent / 100;
            af.duration  = ch->tot_level/2 * percent / 100;
            af.location  = 0;
            af.modifier  = 0;
            af.bitvector = WEAPON_POISON;
	    af.bitvector2 = 0;
		af.slot	= WEAR_NONE;
            affect_to_obj(obj,&af);

		/* The better you get, the less likely people SEE it
			But, even mastered, there is a slight chance of people seeing */
	    if(number_range(0,105) > skill)
		act("$n coats $p with deadly venom.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
	    act("You coat $p with venom.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
	    check_improve(ch,gsk_envenom,true,3);
	    WAIT_STATE(ch,gsk_envenom->beats);
            return;
        }
	else
	{
	    act("You fail to envenom $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
	    check_improve(ch,gsk_envenom,false,3);
	    WAIT_STATE(ch,gsk_envenom->beats);
	    return;
	}
    }

    act("You can't poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
}


// fill <fluid_container>[ <fluid_container (in room)>]
void do_fill(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
//    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *fountain;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		send_to_char("Fill what?\n\r", ch);
		return;
    }

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
		send_to_char("You do not have that item.\n\r", ch);
		return;
    }

	int number;
	char argf[MIL];

	number = number_argument(argument, argf);

    for (fountain = ch->in_room->contents; fountain != NULL; fountain = fountain->next_content)
    {
		if (IS_FLUID_CON(fountain) && (argf[0] == '\0' || (is_name(argf, fountain->name) && number-- == 1)))
		{
		    break;
		}
    }

    if (!fountain || !IS_FLUID_CON(fountain))
    {
		send_to_char("There is no fluid container here!\n\r", ch);
		return;
    }

    if (!IS_FLUID_CON(obj))
    {
		send_to_char("You can't fill that.\n\r", ch);
		return;
    }

	if (!FLUID_CON(fountain)->liquid || (FLUID_CON(fountain)->capacity > 0 && FLUID_CON(obj)->amount < 1))
	{
		send_to_char("That fountain is empty.\n\r", ch);
		return;
	}

	// Fix invalid fluid values
	if (FLUID_CON(obj)->capacity >= 0 && FLUID_CON(obj)->amount < 0)
		FLUID_CON(obj)->amount = 0;

	if (FLUID_CON(obj)->amount != 0 &&
		FLUID_CON(obj)->liquid &&
		!fluid_has_same_fluid(FLUID_CON(obj), FLUID_CON(fountain)))
	{
		send_to_char("There is already another liquid in it.\n\r", ch);
		return;
	}

	if (FLUID_CON(obj)->amount >= FLUID_CON(obj)->capacity)
    {
	    act("$p is full.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
    }

    act("You fill $p with $t from $P.", ch, NULL, NULL, obj,fountain, FLUID_CON(fountain)->liquid->name, NULL, TO_CHAR);
    act("$n fills $p with $t from $P.", ch, NULL, NULL, obj,fountain, FLUID_CON(fountain)->liquid->name, NULL, TO_ROOM);
	FLUID_CON(obj)->liquid = FLUID_CON(fountain)->liquid;	// Copy the liquid type
	list_destroy(FLUID_CON(obj)->spells);
	FLUID_CON(obj)->spells = list_copy(FLUID_CON(fountain)->spells);
	if (FLUID_CON(fountain)->poison > 0)
	{
		if (FLUID_CON(obj)->poison < 100)
		{
			FLUID_CON(obj)->poison = UMAX(FLUID_CON(obj)->poison, FLUID_CON(fountain)->poison);	// Combine the poison
			FLUID_CON(obj)->poison = UMIN(FLUID_CON(obj)->poison, 99);
		}

		// Weaken the poison
		if (FLUID_CON(fountain)->poison < 100)
			FLUID_CON(fountain)->poison--;
	}
	// If the drink is empty, has poison, there is a chance it gets "washed" out
	else if (FLUID_CON(obj)->amount < 1 && FLUID_CON(obj)->poison > 0 && number_percent() >= FLUID_CON(obj)->poison)
	{
		FLUID_CON(obj)->poison = 0;
	}

	int amount;
	if (FLUID_CON(fountain)->amount > 0)
	{
		amount = FLUID_CON(obj)->capacity - FLUID_CON(obj)->amount;
		if (amount >= FLUID_CON(fountain)->amount)
		{
			// Container more capacity than the fountain
			FLUID_CON(obj)->amount += FLUID_CON(fountain)->amount;
			FLUID_CON(fountain)->amount = 0;
		}
		else
		{
			FLUID_CON(obj)->amount = FLUID_CON(obj)->capacity;
			FLUID_CON(fountain)->amount -= amount;
		}
	}
	else
	{
		// Fountain has infinite capacity
		amount = FLUID_CON(obj)->capacity - FLUID_CON(obj)->amount;
		FLUID_CON(obj)->amount = FLUID_CON(obj)->capacity;	// Filled to the brim.
	}

	// if $(obj1) is valid, then $(obj) is the container
	// if $(obj2) is valid, then $(obj) is the fountain
	// register1 == amount

	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, fountain, NULL, TRIG_FILL, NULL,amount,0,0,0,0);

	p_percent_trigger(NULL, fountain, NULL, NULL, ch, NULL, NULL, NULL, obj, TRIG_FILL, NULL,amount,0,0,0,0);

	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, fountain, obj, TRIG_FILL, NULL,amount,0,0,0,0);

	if (FLUID_CON(obj)->amount >= FLUID_CON(obj)->capacity)
	{
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, fountain, NULL, TRIG_FLUID_FILLED, NULL,CONTEXT_FLUID_CON,0,0,0,0);
	}

	if (FLUID_CON(fountain)->capacity > 0 && FLUID_CON(fountain)->amount < 1)
	{
		if (FLUID_CON(fountain)->refill_rate < 1)
		{
			// Clear the liquid
			FLUID_CON(fountain)->liquid = NULL;
			list_clear(FLUID_CON(fountain)->spells);
		}

		p_percent_trigger(NULL, fountain, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_FLUID_EMPTIED, NULL,CONTEXT_FLUID_CON,0,0,0,0);
	}
}

void do_pour(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH],buf[MAX_STRING_LENGTH];
    OBJ_DATA *out, *in;
    CHAR_DATA *vch = NULL;
    int amount;

    argument = one_argument(argument,arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
		send_to_char("Pour what into what?\n\r",ch);
		return;
    }

    if ((out = get_obj_carry(ch,arg, ch)) == NULL)
    {
		send_to_char("You don't have that item.\n\r",ch);
		return;
    }

	if (!IS_FLUID_CON(out))
	{
		send_to_char("That's not a fluid container.\n\r",ch);
		return;
	}

    if (!str_cmp(argument,"out"))
    {
		if (!FLUID_CON(out)->liquid || FLUID_CON(out)->amount == 0)
		{
			send_to_char("It's already empty.\n\r",ch);
			return;
		}

		sprintf(buf,"You invert $p, spilling %s all over the ground.", FLUID_CON(out)->liquid->name);
		act(buf,ch, NULL, NULL,out, NULL, NULL,NULL,TO_CHAR);

		sprintf(buf,"$n inverts $p, spilling %s all over the ground.", FLUID_CON(out)->liquid->name);
		act(buf,ch, NULL, NULL,out, NULL, NULL,NULL,TO_ROOM);

		// No $(obj1) or $(obj2) indicates "pour out"
		p_percent_trigger(NULL, out, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_POUR, NULL, 0,0,0,0,0);
		p_percent_trigger(NULL, out, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_FLUID_EMPTIED, NULL,CONTEXT_FLUID_CON,0,0,0,0);

		if (FLUID_CON(out)->poison > 0 && FLUID_CON(out)->poison < 100)
		{
			if (number_percent() >= FLUID_CON(out)->poison)
				FLUID_CON(out)->poison = 0;
			else
				FLUID_CON(out)->poison--;
		}
		
		FLUID_CON(out)->amount = 0;

		if (FLUID_CON(out)->refill_rate < 1)
		{
			FLUID_CON(out)->liquid = NULL;
			list_clear(FLUID_CON(out)->spells);
		}
		return;
    }

    if ((in = get_obj_here(ch, NULL, argument)) == NULL)
    {
		vch = get_char_room(ch,NULL, argument);

		if (vch == NULL)
		{
			send_to_char("Pour into what?\n\r",ch);
			return;
		}

		in = get_eq_char(vch,WEAR_HOLD);

		if (in == NULL)
		{
			send_to_char("They aren't holding anything.",ch);
			return;
		}
    }

    if (!IS_FLUID_CON(in))
    {
		send_to_char("You can only pour into other fluid containers.\n\r",ch);
		return;
    }

    if (in == out)
    {
		send_to_char("You cannot change the laws of physics!\n\r",ch);
		return;
    }

	if (!FLUID_CON(out)->liquid || FLUID_CON(out)->amount == 0)
    {
		act("There's nothing in $p to pour.",ch, NULL, NULL,out, NULL, NULL,NULL,TO_CHAR);
		return;
    }

	if (FLUID_CON(in)->amount != 0 &&
		FLUID_CON(in)->liquid &&
		!fluid_has_same_fluid(FLUID_CON(in), FLUID_CON(out)))
	{
		send_to_char("They don't hold the same liquid.\n\r",ch);
		return;
    }

    if (FLUID_CON(in)->amount >= FLUID_CON(in)->capacity)
    {
		act("$p is already filled to the top.",ch, NULL, NULL,in, NULL, NULL,NULL,TO_CHAR);
		return;
    }

    amount = UMIN(FLUID_CON(out)->amount,FLUID_CON(in)->capacity - FLUID_CON(in)->amount);

	FLUID_CON(in)->amount += amount;
	FLUID_CON(out)->amount -= amount;
	FLUID_CON(in)->liquid = FLUID_CON(out)->liquid;
	list_destroy(FLUID_CON(in)->spells);
	FLUID_CON(in)->spells = list_copy(FLUID_CON(out)->spells);
	if (FLUID_CON(out)->poison > 0)
	{
		if (FLUID_CON(in)->poison < 100)
		{
			FLUID_CON(in)->poison = UMAX(FLUID_CON(in)->poison, FLUID_CON(out)->poison);	// Combine the poison
			FLUID_CON(in)->poison = UMIN(FLUID_CON(in)->poison, 99);				// Cap for applied poisons
		}

		// Weaken the poison if not permanent
		if (FLUID_CON(out)->poison < 100)
			FLUID_CON(out)->poison--;
	}
	// If the drink is empty, has poison, there is a chance it gets "washed" out
	else if (FLUID_CON(in)->amount < 1 && FLUID_CON(in)->poison > 0 && number_percent() >= FLUID_CON(in)->poison)
	{
		FLUID_CON(in)->poison = 0;
	}


    if (vch == NULL)
    {
    	sprintf(buf,"You pour %s from $p into $P.", FLUID_CON(out)->liquid->name);
    	act(buf,ch, NULL, NULL,out,in, NULL, NULL,TO_CHAR);
    	sprintf(buf,"$n pours %s from $p into $P.", FLUID_CON(out)->liquid->name);
    	act(buf,ch, NULL, NULL,out,in, NULL, NULL,TO_ROOM);
    }
    else
    {
		sprintf(buf,"You pour some %s for $N.", FLUID_CON(out)->liquid->name);
		act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
		sprintf(buf,"$n pours you some %s.", FLUID_CON(out)->liquid->name);
		act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
		sprintf(buf,"$n pours some %s for $N.", FLUID_CON(out)->liquid->name);
		act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT);
    }

	// $obj1 vs $obj2 will make the distinction here...
	// $obj1 is the OUT, $obj2 is the IN
	// amount is in register1
	p_percent_trigger(NULL, out, NULL, NULL, ch, vch, NULL, NULL, in, TRIG_POUR, NULL,amount,0,0,0,0);
	p_percent_trigger(NULL, in, NULL, NULL, ch, vch, NULL, out, NULL, TRIG_POUR, NULL,amount,0,0,0,0);
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, vch, NULL, out, in, TRIG_POUR, NULL,amount,0,0,0,0);
	
	if (FLUID_CON(out)->capacity > 0 && FLUID_CON(out)->amount <= 0)
	{
		FLUID_CON(out)->amount = 0;
		if (FLUID_CON(out)->refill_rate < 1)
		{
			FLUID_CON(out)->liquid = NULL;
			list_clear(FLUID_CON(out)->spells);
		}
		p_percent_trigger(NULL, out, NULL, NULL, ch, vch, NULL, NULL, in, TRIG_FLUID_EMPTIED, NULL,CONTEXT_FLUID_CON,0,0,0,0);
	}

	if (FLUID_CON(in)->capacity > 0 && FLUID_CON(in)->amount >= FLUID_CON(in)->capacity)
	{
		FLUID_CON(in)->amount = FLUID_CON(in)->capacity;
		p_percent_trigger(NULL, in, NULL, NULL, ch, vch, NULL, NULL, out, TRIG_FLUID_FILLED, NULL,CONTEXT_FLUID_CON,0,0,0,0);
	}
}

void __drink_fluid_con(CHAR_DATA *ch, OBJ_DATA *obj, int max_amount, char *verb)
{
	char buf[MSL];

	if (!IS_FLUID_CON(obj))
	{
		sprintf(buf, "You can't %s from that.\n\r", verb);
		send_to_char(buf, ch);
		return;
	}

	// Too drunk
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
    {
		send_to_char("You fail to reach your mouth.  *Hic*\n\r", ch);
		return;
    }

	if (IS_SET(FLUID_CON(obj)->flags, CONT_CLOSED))
	{
		send_to_char("It is closed.\n\r", ch);
		return;
	}

	LIQUID *liquid;
	if (!(liquid = FLUID_CON(obj)->liquid))
	{
		sprintf(buf, "There is nothing to %s.\n\r", verb);
		send_to_char(buf, ch);
		return;
	}

	if (FLUID_CON(obj)->capacity >= 0 && FLUID_CON(obj)->amount < 1)
	{
		send_to_char("It is already empty.\n\r", ch);
		return;
	}

	// Spelled fluids (potions) require a level check.
    if (list_size(FLUID_CON(obj)->spells) > 0 && ch->tot_level < obj->level)
    {
		sprintf(buf, "This liquid is too powerful for you to %s.\n\r", verb);
		send_to_char(buf, ch);
		return;
    }

	int amount = LIQ_SERVING;
	if (max_amount < 0)	// Everything, or one serving if infinite
	{
		if (FLUID_CON(obj)->capacity < 0)
			amount = LIQ_SERVING;
		else
			amount = FLUID_CON(obj)->capacity;
	}
	else if (max_amount == 0)
	{
		amount = LIQ_SERVING;	// One serving
	}
	else
	{
		if (amount > max_amount)
			amount = max_amount;
	}

	// Cap the amount available
	if (FLUID_CON(obj)->amount > 0 && amount > FLUID_CON(obj)->amount)
	{
		amount = FLUID_CON(obj)->amount;
	}

	// See if the character is blocked from drinking
	int ret;
	if ((ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREDRINK, NULL,amount,0,0,0,0)))
	{
		if (ret == 1)
		{
			sprintf(buf, "You can't %s from that.\n\r", verb);
			send_to_char(buf, ch);
		}
		return;
	}

	// See if the object will let the character eat it.
	if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDRINK, NULL,amount,0,0,0,0)))
	{
		if (ret == 1)
		{
			sprintf(buf, "You can't %s from that.\n\r", verb);
			send_to_char(buf, ch);
		}
		return;
	}
	
	// See if the room lets the character eat.
	if ((ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREDRINK, NULL,amount,0,0,0,0)))
	{
		if (ret == 1)
		{
			sprintf(buf, "You can't %s from that here.\n\r", verb);
			send_to_char(buf, ch);
		}
		return;
	}


    act("$n $t $T from $p.",ch, NULL, NULL, obj, NULL, verb, liquid->name, TO_ROOM);
    act("You $t $T from $p.",ch, NULL, NULL, obj, NULL, verb, liquid->name, TO_CHAR);

	// TODO: Take into account savagery
    if (IS_VAMPIRE(ch) && liquid == liquid_blood)
    {
    	send_to_char("You feel refreshed.\n\r", ch);
		gain_condition(ch, COND_FULL, amount / 2 + 1);
		gain_condition(ch, COND_THIRST, amount / 2 + 1);
		gain_condition(ch, COND_HUNGER, amount / 2 + 1);
    }
    else
    {
		gain_condition(ch, COND_DRUNK, amount * liquid->proof / 36);
		gain_condition(ch, COND_FULL, amount * liquid->full / 4);
		gain_condition(ch, COND_THIRST, amount * liquid->thirst / 2);
		gain_condition(ch, COND_HUNGER, amount * liquid->hunger / 2);
    }

	if (!IS_NPC(ch))
	{
		if (ch->pcdata->condition[COND_DRUNK]  > 10)
			send_to_char("You feel drunk.\n\r", ch);

		// TODO: Take into account savagery
		if (ch->pcdata->condition[COND_FULL]   > 40)
			send_to_char("You are full.\n\r", ch);
		if (ch->pcdata->condition[COND_THIRST] > 40)
		{
			if (get_room_savage_level(ch->in_room) > 0)
				send_to_char("Your thirst is quenched.\n\r", ch);
		}
	}

	if (FLUID_CON(obj)->poison > 0 && check_immune(ch, DAM_POISON) != IS_IMMUNE)
	{
		AFFECT_DATA af;
		memset(&af,0,sizeof(af));
		act("$n chokes and gags.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("You choke and gag.\n\r", ch);
		af.where = TO_AFFECTS;
		af.group = AFFGROUP_BIOLOGICAL;
		af.skill = gsk_poison;
		af.level = number_fuzzy(amount);
		af.duration = 3 * FLUID_CON(obj)->poison * amount / 100;
		af.location = APPLY_NONE;
		af.modifier = 0;
		af.bitvector = AFF_POISON;
		af.bitvector2 = 0;
		af.slot	= WEAR_NONE;
		affect_join(ch, &af);
	}

	bool spells = false;
	if (list_size(FLUID_CON(obj)->spells) > 0)
	{
		ITERATOR it;
		SPELL_DATA *spell;
		iterator_start(&it, FLUID_CON(obj)->spells);
		while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
		{
			if (spell->skill->token)
				p_token_index_percent_trigger(spell->skill->token, ch, NULL, NULL, obj, NULL, TRIG_TOKEN_QUAFF, NULL, 0,0,0,0,0, spell->level,0,0,0,0);
			else if (spell->skill->quaff_fun)
				(*spell->skill->quaff_fun)(spell->skill, spell->level, ch, obj);
		}
		iterator_stop(&it);

		spells = true;
	}

	if (FLUID_CON(obj)->capacity > 0)
	{
		FLUID_CON(obj)->amount -= amount;
	}

	// Amount consumed is in $(register1)
	// Verb is in $(phrase)
	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DRINK, verb, amount,0,0,0,0);
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_DRINK, verb, amount,0,0,0,0);

	if (FLUID_CON(obj)->capacity > 0 && FLUID_CON(obj)->amount < 1)
	{
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_FLUID_EMPTIED, NULL,CONTEXT_FLUID_CON,0,0,0,0);

		if (FLUID_CON(obj)->poison > 0 && FLUID_CON(obj)->poison < 100)
		{
			if (number_percent() >= FLUID_CON(obj)->poison)
				FLUID_CON(obj)->poison = 0;
			else
				FLUID_CON(obj)->poison--;
		}
		
		FLUID_CON(obj)->amount = 0;
		FLUID_CON(obj)->liquid = NULL;
		list_clear(FLUID_CON(obj)->spells);

		if (IS_SET(FLUID_CON(obj)->flags, FLUID_CON_DESTROY_ON_CONSUME))
		{
			extract_obj(obj);
		}
	}

	// Those with spells cause a wait state, regardless of amount consumed.
	if (spells)
		WAIT_STATE(ch, 8);

    return;

}

void do_sip(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		for (obj = ch->in_room->contents; obj; obj = obj->next_content)
		{
			if (IS_FLUID_CON(obj))
				break;
		}

		if (obj == NULL)
		{
			send_to_char("Sip what?\n\r", ch);
			return;
		}
    }
    else
    {
		if ((obj = get_obj_here(ch, NULL, arg)) == NULL)
		{
			send_to_char("You can't find it.\n\r", ch);
			return;
		}
    }

	__drink_fluid_con(ch, obj, 1, "sip");
}

void do_drink(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		for (obj = ch->in_room->contents; obj; obj = obj->next_content)
		{
			if (IS_FLUID_CON(obj))
				break;
		}

		if (obj == NULL)
		{
			send_to_char("Drink what?\n\r", ch);
			return;
		}
    }
    else
    {
		if ((obj = get_obj_here(ch, NULL, arg)) == NULL)
		{
			send_to_char("You can't find it.\n\r", ch);
			return;
		}
    }

	__drink_fluid_con(ch, obj, 0, "drink");
}

void do_quaff(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		for (obj = ch->in_room->contents; obj; obj = obj->next_content)
		{
			if (IS_FLUID_CON(obj))
				break;
		}

		if (obj == NULL)
		{
			send_to_char("Quaff what?\n\r", ch);
			return;
		}
    }
    else
    {
		if ((obj = get_obj_here(ch, NULL, arg)) == NULL)
		{
			send_to_char("You can't find it.\n\r", ch);
			return;
		}
    }

	__drink_fluid_con(ch, obj, -1, "quaff");
}


void do_eat(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    //SPELL_DATA *spell;
	int ret;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Eat what?\n\r", ch);
	return;
    }

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
	send_to_char("You do not have that item.\n\r", ch);
	return;
    }

    if (!IS_IMMORTAL(ch))
    {
		if (obj->item_type != ITEM_FOOD && obj->item_type != ITEM_PILL)
		{
			send_to_char("That's not edible.\n\r", ch);
			return;
		}
    }

	// See if the character is blocked from eating
	if ((ret = p_percent_trigger(ch, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREEAT, NULL,0,0,0,0,0)))
	{
		if (ret == 1)
		{
			send_to_char("You cannot eat that.\n\r", ch);
		}
		return;
	}

	// See if the object will let the character eat it.
	if ((ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREEAT, NULL,0,0,0,0,0)))
	{
		if (ret == 1)
		{
			send_to_char("You cannot eat that.\n\r", ch);
		}
		return;
	}
	
	// See if the room lets the character eat.
	if ((ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREEAT, NULL,0,0,0,0,0)))
	{
		if (ret == 1)
		{
			send_to_char("You cannot eat that here.\n\r", ch);
		}
		return;
	}

    act("$n eats $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    act("You eat $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	if (IS_FOOD(obj))
	{
	    if (!IS_NPC(ch))
	    {
			gain_condition(ch, COND_FULL, FOOD(obj)->full);
			gain_condition(ch, COND_HUNGER, FOOD(obj)->hunger);
		}

	    if (FOOD(obj)->poison > 0 && check_immune(ch, DAM_POISON) != IS_IMMUNE)
	    {
			/* The food was poisoned! */
			AFFECT_DATA af;
			memset(&af,0,sizeof(af));
			act("$n chokes and gags.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			send_to_char("You choke and gag.\n\r", ch);

			af.where	 = TO_AFFECTS;
			af.group     = AFFGROUP_BIOLOGICAL;
			af.skill      = gsk_poison;
			af.level 	 = number_fuzzy(FOOD(obj)->full);
			af.duration  = FOOD(obj)->poison * FOOD(obj)->full / 50;	// is really 2 * poison% * hours / 100
			af.location  = APPLY_NONE;
			af.modifier  = 0;
			af.bitvector = AFF_POISON;
			af.bitvector2 = 0;
			af.slot	= WEAR_NONE;
			affect_join(ch, &af);
	    }

		food_apply_buffs(ch, obj, obj->level, FOOD(obj)->full);
	}
	else if (obj->item_type == ITEM_PILL)
	{
		obj_apply_spells(ch, obj, ch, NULL, obj->spells, TRIG_APPLY_AFFECT);
	}


	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_EAT, NULL,0,0,0,0,0);
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_EAT, NULL,0,0,0,0,0);

    extract_obj(obj);
}


bool remove_obj(CHAR_DATA *ch, int iWear, bool fReplace)
{
    OBJ_DATA *obj;

    if ((obj = get_eq_char(ch, iWear)) == NULL) {
		return true;
	}

    if (!fReplace) {
		return false;
	}

	if( !WEAR_ALWAYSREMOVE(iWear) ) {
		if (IS_SET(obj->extra[0], ITEM_NOREMOVE) || !WEAR_REMOVEEQ(iWear) || obj->item_type == ITEM_TATTOO) {
			act("You can't remove $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return false;
		}

		if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREMOVE, NULL,0,0,0,0,0))
			return false;
	}

    script_lastreturn = 2;	/* Indicate that it is a REMOVE not just a general unequip */

    if(!unequip_char(ch, obj, true)) {
	act("$n stops using $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	act("You stop using $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    }
    return true;
}

// When there is a pair, it will return either the first or left version
int get_wear_loc(CHAR_DATA *ch, OBJ_DATA *obj)
{
	if (obj->item_type == ITEM_LIGHT)	// Primary LIGHT objects are always this slot
		return WEAR_LIGHT;

    if (CAN_WEAR(obj, ITEM_WEAR_FINGER))
		return WEAR_FINGER_L;

    if (CAN_WEAR(obj, ITEM_WEAR_RING_FINGER))
    	return WEAR_RING_FINGER;

    if (CAN_WEAR(obj, ITEM_WEAR_NECK))
    	return WEAR_NECK_1;

    if (CAN_WEAR(obj, ITEM_WEAR_BODY))
    	return WEAR_BODY;

    if (CAN_WEAR(obj, ITEM_WEAR_HEAD))
    	return WEAR_HEAD;

    if (CAN_WEAR(obj, ITEM_WEAR_FACE))
    	return WEAR_FACE;

    if (CAN_WEAR(obj, ITEM_WEAR_EYES))
    	return WEAR_EYES;

    if (CAN_WEAR(obj, ITEM_WEAR_EAR))
    	return WEAR_EAR_L;

    if (CAN_WEAR(obj, ITEM_WEAR_LEGS))
    	return WEAR_LEGS;

    if (CAN_WEAR(obj, ITEM_WEAR_ANKLE))
		return WEAR_ANKLE_L;

    if (CAN_WEAR(obj, ITEM_WEAR_FEET))
    	return WEAR_FEET;

    if (CAN_WEAR(obj, ITEM_WEAR_HANDS))
    	return WEAR_HANDS;

    if (CAN_WEAR(obj, ITEM_WEAR_ARMS))
    	return WEAR_ARMS;

    if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))
    	return WEAR_ABOUT;

    if (CAN_WEAR(obj, ITEM_WEAR_WAIST))
    	return WEAR_WAIST;

    if (CAN_WEAR(obj, ITEM_WEAR_WRIST))
    	return WEAR_WRIST_L;

    if (CAN_WEAR(obj, ITEM_WEAR_SHIELD))
    	return WEAR_SHIELD;

    if (CAN_WEAR(obj, ITEM_WEAR_BACK))
    	return WEAR_BACK;

    if (CAN_WEAR(obj, ITEM_WEAR_SHOULDER))
    	return WEAR_SHOULDER;

    if (CAN_WEAR(obj, ITEM_WIELD))
    	return WEAR_WIELD;

    if (CAN_WEAR(obj, ITEM_HOLD))
    	return WEAR_HOLD;

    if (CAN_WEAR(obj, ITEM_WEAR_TABARD))
		return WEAR_TABARD;


	return WEAR_NONE;
}


void wear_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool fReplace)
{
    char buf[MAX_STRING_LENGTH];

    if (!is_wearable(obj))
    {
		act("You can't wear, wield, or hold $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
    }

	if (!IS_NPC(ch) && !(IS_IMMORTAL(ch) && IS_SET(ch->act[1], PLR_HOLYAURA))) {
		CLASS_LEVEL *cl = get_class_level(ch, NULL);
		CLASS_DATA *clazz = cl ? cl->clazz : NULL;
		int level = cl ? cl->level : 1;

		// NIB: Removed the LEVEL_HERO check and the Remort check
		if (level < obj->level) {
			sprintf(buf, "You must be level %d to use this object.\n\r", obj->level);
			send_to_char(buf, ch);
			act("$n tries to use $p, but is too inexperienced.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			return;
		}

		if (IS_VALID(obj->clazz) && obj->clazz != clazz)
		{
			sprintf(buf, "You must be %s %s to use this object.\n\r", get_article(obj->clazz->display[ch->sex], false), obj->clazz->display[ch->sex]);
			send_to_char(buf, ch);
			//act("$n tries to use $p, but is the wrong class.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			return;
		}

		if (obj->clazz_type != CLASS_NONE && (!IS_VALID(clazz) || obj->clazz_type != clazz->type))
		{
			char *type = flag_string(class_types, obj->clazz_type);
			sprintf(buf, "You must be %s %s to use this object.\n\r", get_article(type, false), type);
			send_to_char(buf, ch);
			//act("$n tries to use $p, but is the wrong class.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			return;
		}

		if (list_size(obj->race) > 0 && !list_contains(obj->race, ch->race, NULL))
		{
			int i = 1;

			strcpy(buf, "You must be");
			ITERATOR rit;
			RACE_DATA *race;
			iterator_start(&rit, obj->race);
			while((race = (RACE_DATA *)iterator_nextdata(&rit)))
			{
				if (i > 1)
				{
					if (i == list_size(obj->race))
						strcat(buf, " or ");
					else
						strcat(buf, ",");
				}
				strcat(buf, formatf(" %s %s", get_article(race->name, false), race->name));
				++i;
			}
			iterator_stop(&rit);
			strcat(buf, " to use this object.\n\r");
			send_to_char(buf, ch);
			//act("$n tries to use $p, but is the wrong race.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			return;
		}


	}

	if (IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && !IS_REMORT(ch) && !IS_NPC(ch)) {
		send_to_char("You cannot use this object without remorting.\n\r", ch);
		act("$n tries to use $p, but is too inexperienced.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		return;
	}

	if (obj->item_type == ITEM_LIGHT) {
		if (!remove_obj(ch, WEAR_LIGHT, fReplace))
			return;

		if (IS_SET(LIGHT(obj)->flags, LIGHT_IS_ACTIVE))
		{
			act("$n holds $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You holds $p.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		}
		else
		{
			SET_BIT(LIGHT(obj)->flags, LIGHT_IS_ACTIVE);
			act("$n lights $p and holds it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You light $p and hold it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		}
		equip_char(ch, obj, WEAR_LIGHT);
		return;
	}


    if (CAN_WEAR(obj, ITEM_WEAR_FINGER)) {
		if (get_eq_char(ch, WEAR_FINGER_L) != NULL && get_eq_char(ch, WEAR_FINGER_R) != NULL &&
			!remove_obj(ch, WEAR_FINGER_L, fReplace) && !remove_obj(ch, WEAR_FINGER_R, fReplace))
	    	return;

		if (get_eq_char(ch, WEAR_FINGER_L) == NULL)
		{
			act("$n wears $p on $s left finger.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your left finger.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_FINGER_L);
			return;
		}

		if (get_eq_char(ch, WEAR_FINGER_R) == NULL)
		{
			act("$n wears $p on $s right finger.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your right finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_FINGER_R);
			return;
		}

		send_to_char("You already wear two rings.\n\r", ch);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_RING_FINGER))
    {
		if (!remove_obj(ch, WEAR_RING_FINGER, fReplace))
		    return;

		act("$n wears $p on $s ring finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your ring finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_RING_FINGER);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_NECK))
    {
		if (get_eq_char(ch, WEAR_NECK_1) != NULL && get_eq_char(ch, WEAR_NECK_2) != NULL &&
			!remove_obj(ch, WEAR_NECK_1, fReplace) && !remove_obj(ch, WEAR_NECK_2, fReplace))
	    	return;

		if (get_eq_char(ch, WEAR_NECK_1) == NULL)
		{
			act("$n wears $p around $s neck.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p around your neck.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_NECK_1);
			return;
		}

		if (get_eq_char(ch, WEAR_NECK_2) == NULL)
		{
			act("$n wears $p around $s neck.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p around your neck.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_NECK_2);
			return;
		}

		send_to_char("You already wear two neck items.\n\r", ch);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_BODY))
    {
		if (!remove_obj(ch, WEAR_BODY, fReplace))
			return;
		act("$n wears $p on $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_BODY);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_TABARD))
    {
		if (!remove_obj(ch, WEAR_TABARD, fReplace))
			return;
		act("$n drapes $p down the front of $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You drape $p down the front of your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_TABARD);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_HEAD))
    {
		if (!remove_obj(ch, WEAR_HEAD, fReplace))
			return;
		act("$n wears $p on $s head.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your head.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_HEAD);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_FACE))
    {
		if (!remove_obj(ch, WEAR_FACE, fReplace))
			return;
		act("$n wears $p over $s face.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p over your face.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_FACE);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_EYES))
    {
		if (!remove_obj(ch, WEAR_EYES, fReplace))
			return;
		act("$n wears $p over $s eyes.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p over your eyes.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_EYES);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_EAR)) {
		if (get_eq_char(ch, WEAR_EAR_L) != NULL && get_eq_char(ch, WEAR_EAR_R) != NULL &&
			!remove_obj(ch, WEAR_EAR_L, fReplace) && !remove_obj(ch, WEAR_EAR_R, fReplace))
	    	return;

		if (get_eq_char(ch, WEAR_EAR_L) == NULL)
		{
			act("$n wears $p on $s left ear.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your left ear.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_EAR_L);
			return;
		}

		if (get_eq_char(ch, WEAR_EAR_R) == NULL)
		{
			act("$n wears $p on $s right ear.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your right ear.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_EAR_R);
			return;
		}

		send_to_char("You already wear two ear items .\n\r", ch);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_LEGS))
    {
		if (!remove_obj(ch, WEAR_LEGS, fReplace))
			return;
		act("$n wears $p on $s legs.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your legs.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_LEGS);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ANKLE)) {
		if (get_eq_char(ch, WEAR_ANKLE_L) != NULL && get_eq_char(ch, WEAR_ANKLE_R) != NULL &&
			!remove_obj(ch, WEAR_ANKLE_L, fReplace) && !remove_obj(ch, WEAR_ANKLE_R, fReplace))
	    	return;

		if (get_eq_char(ch, WEAR_ANKLE_L) == NULL)
		{
			act("$n wears $p on $s left ankle.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your left ankle.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_ANKLE_L);
			return;
		}

		if (get_eq_char(ch, WEAR_ANKLE_R) == NULL)
		{
			act("$n wears $p on $s right ankle.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p on your right ankle.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_ANKLE_R);
			return;
		}

		send_to_char("You already wear two ankle items .\n\r", ch);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_FEET))
    {
		if (!remove_obj(ch, WEAR_FEET, fReplace))
			return;
		act("$n wears $p on $s feet.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your feet.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_FEET);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_HANDS))
    {
		if (!remove_obj(ch, WEAR_HANDS, fReplace))
			return;
		act("$n wears $p on $s hands.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your hands.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_HANDS);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ARMS))
    {
		if (!remove_obj(ch, WEAR_ARMS, fReplace))
			return;
		act("$n wears $p on $s arms.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p on your arms.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_ARMS);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))
    {
		if (!remove_obj(ch, WEAR_ABOUT, fReplace))
			return;
		act("$n wears $p about $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p about your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_ABOUT);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_WAIST))
    {
		if (!remove_obj(ch, WEAR_WAIST, fReplace))
			return;
		act("$n wears $p about $s waist.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p about your waist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_WAIST);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_WRIST))
    {
		if (get_eq_char(ch, WEAR_WRIST_L) != NULL && get_eq_char(ch, WEAR_WRIST_R) != NULL &&
			!remove_obj(ch, WEAR_WRIST_L, fReplace) && !remove_obj(ch, WEAR_WRIST_R, fReplace))
			return;

		if (get_eq_char(ch, WEAR_WRIST_L) == NULL)
		{
			act("$n wears $p around $s left wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p around your left wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_WRIST_L);
			return;
		}

		if (get_eq_char(ch, WEAR_WRIST_R) == NULL)
		{
			act("$n wears $p around $s right wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("You wear $p around your right wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			equip_char(ch, obj, WEAR_WRIST_R);
			return;
		}

		send_to_char("You already wear two wrist items.\n\r", ch);
		return;
    }

   /*
    * Shield
    */
    if (CAN_WEAR(obj, ITEM_WEAR_SHIELD))
    {
		if (!remove_obj(ch, WEAR_SHIELD, fReplace))
			return;

		if (both_hands_full(ch))
		{
			send_to_char("You don't have a spare hand.\n\r", ch);
			return;
		}

		act("$n wears $p as a shield.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wear $p as a shield.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_SHIELD);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_BACK))
    {
		if (!remove_obj(ch, WEAR_BACK, fReplace))
			return;

		act("$n slings $p across $s back.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You sling $p across your back.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_BACK);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_SHOULDER))
    {
		if (!remove_obj(ch, WEAR_SHOULDER, fReplace))
			return;

		act("$n slings $p over $s shoulder.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You sling $p over your shoulder.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_SHOULDER);
		return;
    }

    if (CAN_WEAR(obj, ITEM_WIELD))
    {
		int skill;

		if (!remove_obj(ch, WEAR_WIELD, fReplace))
			return;

        if (obj->condition == 0)
		{
		    send_to_char("You can't wield that weapon. It's broken!\n\r", ch);
		    return;
        }

		if (!IS_NPC(ch) && get_obj_weight(obj) > (str_app[get_curr_stat(ch,STAT_STR)].wield * 10))
		{
			send_to_char("It is too heavy for you to wield.\n\r", ch);
			return;
		}

		if (IS_WEAPON_STAT(obj, WEAPON_TWO_HANDS) && one_hand_full(ch) && ch->size < SIZE_HUGE)
		{
			send_to_char("That's a two-handed weapon, and you only have one hand free.\n\r", ch);
			return;
		}

		if (ch->size < SIZE_HUGE &&
			(get_eq_char(ch, WEAR_SECONDARY) != NULL) &&
				(get_eq_char(ch, WEAR_SECONDARY)->value[0] == WEAPON_POLEARM ||
				(get_eq_char(ch, WEAR_SECONDARY))->value[0] == WEAPON_SPEAR) &&
				(obj->value[0] == WEAPON_POLEARM || obj->value[0] == WEAPON_SPEAR))
		{
			send_to_char("You can't wield two of those at once.\n\r", ch);
			return;
		}

		if (both_hands_full(ch))
		{
			send_to_char("You don't have a spare hand!\n\r",ch);
			return;
		}

		act("$n wields $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You wield $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_WIELD);

		SKILL_DATA *sk = get_weapon_sn(ch);

		if (sk == gsk_hand_to_hand)
		   return;

        skill = get_weapon_skill(ch,sk);

        if (skill >= 100)
            act("$p feels like a part of you!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else if (skill > 85)
            act("You feel quite confident with $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else if (skill > 70)
            act("You are skilled with $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else if (skill > 50)
            act("Your skill with $p is adequate.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else if (skill > 25)
            act("$p feels a little clumsy in your hands.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else if (skill > 1)
            act("You fumble and almost drop $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
        else
            act("You don't even know which end is up on $p.", ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);

		return;
    }

    if (CAN_WEAR(obj, ITEM_HOLD))
    {
		if (both_hands_full(ch))
		{
		    send_to_char("You don't have a spare hand.\n\r", ch);
		    return;
		}

		if (!remove_obj(ch, WEAR_HOLD, fReplace))
		    return;

		act("$n holds $p in $s hand.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You hold $p in your hand.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		equip_char(ch, obj, WEAR_HOLD);
		return;
    }

    if (fReplace)
		send_to_char("You can't wear, wield, or hold that.\n\r", ch);
}


void do_wear(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (IS_SHIFTED_SLAYER(ch) || IS_SHIFTED_WEREWOLF(ch))
    {
		send_to_char("You can't do that in your current form.\n\r", ch);
		return;
    }

    if (IS_AFFECTED(ch, AFF_BLIND))
    {
		send_to_char("You can't see a thing!\n\r", ch);
		return;
    }

    if (arg[0] == '\0')
    {
		send_to_char("Wear, wield, or hold what?\n\r", ch);
		return;
    }

	CLASS_LEVEL *cl = get_class_level(ch, NULL);
	int level = cl ? cl->level : 1;

    if (!str_cmp(arg, "all"))
    {
		OBJ_DATA *obj_next = NULL;
//		bool found = false;

		send_to_char("You throw on your equipment.\n\r", ch);
		act("$n throws on $s equipment.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		/* First run through all equipment looking for last_wear_loc set. */
		for (obj = ch->carrying; obj != NULL; obj = obj_next)
		{
			obj_next = obj->next_content;
			if (obj->last_wear_loc != WEAR_NONE &&
				WEAR_AUTOEQUIP(obj->last_wear_loc) &&
				can_see_obj(ch, obj) &&
				obj->wear_loc == WEAR_NONE &&
				allowed_to_wear(ch, obj) &&
				level >= obj->level) {
			if (both_hands_full(ch)
			&& (CAN_WEAR(obj, ITEM_WEAR_SHIELD)
				 || CAN_WEAR(obj, ITEM_HOLD)
				 || CAN_WEAR(obj, ITEM_WIELD)))
				continue;

			if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL,0,0,0,0,0))
				continue;

			equip_char(ch, obj, obj->last_wear_loc);
//			found = true;
			}
		}

		for (obj = ch->carrying; obj != NULL; obj = obj_next)
		{
			obj_next = obj->next_content;
			if (obj->wear_loc == WEAR_NONE
			&& can_see_obj(ch, obj)
			&& allowed_to_wear(ch, obj)
			&& is_wearable(obj))
			{

			if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL,0,0,0,0,0))
				continue;

			wear_obj(ch, obj, false);
//			found = true;
			}
		}
		return;
    }
    else
    {
		if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
		{
		    send_to_char("You do not have that item.\n\r", ch);
		    return;
		}

		if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL,0,0,0,0,0))
			return;

		wear_obj(ch, obj, true);
    }
}


void removeall(CHAR_DATA *ch)
{
    OBJ_DATA *obj;

    save_last_wear(ch);

    for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
    {
        if (obj->wear_loc != WEAR_NONE)
        {
            remove_obj(ch, obj->wear_loc, true);
	}
    }
}


void do_remove(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (IS_AFFECTED(ch, AFF_BLIND))
    {
        send_to_char("You can't see a thing!\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
	send_to_char("Remove what?\n\r", ch);
	return;
    }

    if (!str_cmp(arg, "all"))
    {
//	bool found = false;
	save_last_wear(ch);

	send_to_char("You remove your equipment.\n\r", ch);
	act("$n removes $s equipment.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	for (obj = ch->carrying; obj != NULL; obj = obj->next_content)
	{
  	    if (obj->wear_loc != WEAR_NONE
  	    &&   obj->item_type != ITEM_TATTOO
	    &&   can_see_obj(ch, obj)
	    &&   (WEAR_ALWAYSREMOVE(obj->wear_loc) || !IS_SET(obj->extra[0], ITEM_NOREMOVE))
	    &&   wear_params[obj->wear_loc][2])
	    {

		if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREMOVE, NULL,0,0,0,0,0))
			continue;

		unequip_char(ch, obj, false);
//		found = true;
	    }
	}
    }
    else
    {
        if ((obj = get_obj_wear(ch, arg, true)) == NULL)
        {
	    send_to_char("You do not have that item.\n\r", ch);
	    return;
        }

	remove_obj(ch, obj->wear_loc, true);
    }
    return;
}

void sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, char *name)
{
	long deitypoints;
	char buf[MSL];

	if (obj == NULL || !can_see_obj(ch, obj))
	{
		act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, name, TO_CHAR);
		return;
	}

	if (!can_sacrifice_obj(ch, obj, false))
		return;

	switch ((deitypoints = get_dp_value(obj)))
	{
		case 0:
			send_to_char("The gods accept your sacrifice, but give you nothing.\n\r", ch);
			break;

		case 1:
			send_to_char("Pleased with your sacrifice, the gods reward you with a deity point.\n\r", ch);
			break;

		default:
		sprintf(buf,"Pleased with your sacrifice, the gods reward you with {Y%ld{x deity points.\n\r", deitypoints);
		send_to_char(buf,ch);
	}

	ch->deitypoints += deitypoints;

	act("$n sacrifices $p to the gods.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	extract_obj(obj);

}

void do_sacrifice(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char short_descr[MSL];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    one_argument(argument, arg);

    if (arg[0] == '\0' || !str_cmp(arg, ch->name))
    {
		act("$n offers $mself to his god, who graciously declines.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		send_to_char("The gods appreciate your offer and may accept it later.\n\r", ch);
		return;
    }

    /* Sacrifice <obj> */
    if (str_cmp(arg, "all") && str_prefix("all.", arg))
    {
        obj = get_obj_list(ch, arg, ch->in_room->contents);

		sacrifice_obj(ch, obj, arg);
    }
    else
    {
		/* 'sac all' or 'sac all.obj' */
		int i = 0;
		char buf[2*MAX_STRING_LENGTH];
		bool found = true;
		bool any = false;
		long total = 0;

		if (wnum_match_room(room_wnum_donation, ch->in_room))
		{
			send_to_char("Where are your manners!?\n\r", ch);
			send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", ch);

			send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", ch);

			act("{Y$n is struck by a bolt of lightning!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
			send_to_char("{ROUCH! That really did hurt!{x\n\r", ch);

			ch->hit = 1;
			ch->mana = 1;
			ch->move = 1;
			return;
		}

		while (found)
		{
			found = false;
			i = 0;

			/* Is there an object that matches name */
			for (obj = ch->in_room->contents; obj != NULL; obj = obj_next)
			{
				obj_next = obj->next_content;

				if ((arg[3] == '\0' || is_name(&arg[4], obj->name)) &&
						can_sacrifice_obj(ch, obj, true))
				{
					strncpy(short_descr, obj->short_descr, sizeof(short_descr)-1);
					found = true;
					any = true;
					break;
				}
			}

			/* Found one, extract all of that type */
			if (found)
			{
				for (obj = ch->in_room->contents; obj != NULL; obj = obj_next)
				{
					obj_next = obj->next_content;

					if (str_cmp(obj->short_descr, short_descr) ||
						!can_sacrifice_obj(ch, obj, true))
						continue;

					total += get_dp_value(obj);
					extract_obj(obj);
					i++;
				}

				if (i > 0)
				{
					sprintf(buf, "{Y({G%2d{Y) {x$n sacrifices %s.", i, short_descr);
					act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				}
			}
			else
			{
				if (!any)
				{
					if (arg[3] == '\0')
					{
						act("There is nothing here you can sacrifice.", ch, NULL, NULL, NULL, NULL, NULL , NULL, TO_CHAR);
					}
					else
					{
						act("There's no $T here.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR);
					}
				}
			}
		}

		if (any)
		{
			if (total == 0)
			{
				sprintf(buf, "The gods accept your sacrifice, but give you nothing.");
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
			else if (total == 1)
			{
				sprintf(buf, "Pleased with your sacrifice, the gods reward you with a deity point.");
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
			else
			{
				sprintf(buf, "Pleased with your sacrifice, the gods reward you with {Y%ld{x deity points.", total);
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}

			ch->deitypoints += total;
		}
	}
}



void do_recite(CHAR_DATA *ch, char *argument)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	CHAR_DATA *victim;
	OBJ_DATA *scroll;
	OBJ_DATA *obj;

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);

	if ((scroll = get_obj_carry(ch, arg1, ch)) == NULL)
	{
		send_to_char("You do not have that scroll.\n\r", ch);
		return;
	}

	if (!IS_SCROLL(scroll))
	{
		send_to_char("You can recite only scrolls.\n\r", ch);
		return;
	}

	if (ch->tot_level < scroll->level)
	{
		send_to_char("This scroll is too complex for you to comprehend.\n\r",ch);
		return;
	}

	if (IS_AFFECTED2(ch, AFF2_SILENCE))
	{
		send_to_char("You are silenced! You are unable to recite the scroll!\n\r", ch);
		return;
	}

	obj = NULL;
	if (arg2[0] == '\0')
	{
		victim = ch;
	}
	else
	{
		if ((victim = get_char_room (ch, NULL, arg2)) == NULL &&
			(obj    = get_obj_here  (ch, NULL, arg2)) == NULL)
		{
			send_to_char("You can't find it.\n\r", ch);
			return;
		}
	}

	int beats;
	if (scroll->value[2] == 0)
		beats = 10;
	else if (scroll->value[3] == 0)
		beats = 14;
	else
		beats = 18;

	// TODO: Fix these so they utilize the return value
	// 0: allow
	// 1: denied - give standard message
	// 2: denied - silent (script should give a reason)

	// Both scripts MUST provide a reason.
	// Does the scroll forbid it?
	scroll->tempstore[0] = beats;
	if(p_percent_trigger( NULL, scroll, NULL, NULL, ch, victim, NULL,obj, NULL, TRIG_PRERECITE, NULL,0,0,0,0,0))
		return;

	// Does the ROOM forbid it?
	ch->in_room->tempstore[0] = scroll->tempstore[0];
	if(p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, scroll, TRIG_PRERECITE, NULL,0,0,0,0,0))
		return;

	// Does the PLAYER (TOKENS) forbid it?
	ch->tempstore[0] = ch->in_room->tempstore[0];
	if(p_percent_trigger( ch, NULL, NULL, NULL, ch, victim, NULL, obj, scroll, TRIG_PRERECITE, NULL,0,0,0,0,0))
		return;

	beats = ch->tempstore[0];
	beats = UMAX(beats, 1);

	RECITE_STATE(ch, beats);

	act("{W$n begins to recite the words of $p...{x", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ROOM);
	act("{WYou begin to recite the words of $p...{x", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_CHAR);

	ch->recite_scroll = scroll;

	if (victim != NULL)
		ch->cast_target_name = str_dup(victim->name);
	else if (obj != NULL)
		ch->cast_target_name = str_dup(obj->name);
}


void obj_recite_spell(OBJ_DATA *scroll, SKILL_DATA *skill, int level, CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *obj)
{
    void *vo;
    int target = TARGET_NONE;

    if (!IS_VALID(skill))
	{
		bug("obj_recite_spell: bad skill data.", 0);
		return;
	}

	if (skill->token)
	{
		if (!get_script_token(skill->token, TRIG_TOKEN_RECITE, TRIGSLOT_SPELL))
		{
			bug("obj_recite_spell: bad skill uid = %d.", skill->uid);
			return;
		}
	}
	else if (!skill->recite_fun)
    {
		bug("obj_recite_spell: bad skill uid = %d.", skill->uid);
		return;
    }

    switch (skill->target)
    {
        default:
	    bug("obj_recite_spell: bad target for skill uid = %d.", skill->uid);
	    return;

	case TAR_IGNORE:
	    vo = NULL;
		victim = NULL;
		obj = NULL;
	    break;

	case TAR_CHAR_OFFENSIVE:
	    if (victim == NULL)
  	        victim = ch->fighting;

	    if (victim == NULL)
	    {
	        send_to_char("You can't do that.\n\r", ch);
			return;
	    }

	    if (is_safe(ch,victim, true) && ch != victim)
	    {
			send_to_char("Something isn't right...\n\r",ch);
			return;
	    }

	    vo = (void *) victim;
	    target = TARGET_CHAR;
		obj = NULL;
	    break;

	case TAR_CHAR_DEFENSIVE:
	case TAR_CHAR_SELF:
	    if (victim == NULL)
		    victim = ch;

	    vo = (void *) victim;
	    target = TARGET_CHAR;
		obj = NULL;
	    break;

	case TAR_OBJ_INV:
	    if (obj == NULL)
	    {
	        send_to_char("You can't do that.\n\r", ch);
		return;
	    }
	    vo = (void *) obj;
	    target = TARGET_OBJ;
		victim = NULL;
	    break;

	case TAR_OBJ_CHAR_OFF:
	    if (victim == NULL && obj == NULL)
	    {
	        if (ch->fighting != NULL)
	   	    	victim = ch->fighting;
  	        else
	        {
			    send_to_char("You can't do that.\n\r",ch);
		   	    return;
			}
	    }

	    if (victim != NULL)
	    {
	        if (is_safe_spell(ch,victim,false) && ch != victim)
	        {
	            send_to_char("Something isn't right...\n\r",ch);
			    return;
			}

			vo = (void *) victim;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else
	    {
		    vo = (void *) obj;
		    target = TARGET_OBJ;
			victim = NULL;
	    }
	    break;


	case TAR_OBJ_CHAR_DEF:
	    if (victim == NULL && obj == NULL)
	    {
	        vo = (void *) ch;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else if (victim != NULL)
	    {
			vo = (void *) victim;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else
	    {
			vo = (void *) obj;
			target = TARGET_OBJ;
			obj = NULL;
	    }

	    break;
    }

	// Does the spell need to be checked for deflections to switch victim?
	if (target == TARGET_CHAR && victim != NULL)
	{
		CHAR_DATA *tch;
		check_spell_deflection_new(ch, victim, skill, false, &tch, NULL);
		if (!tch)
			return;
		
		victim = tch;
		vo = (void *) tch;
	}

	//char _buf[MSL];
	if (skill->token)
		p_token_index_percent_trigger(skill->token, ch, victim, NULL, scroll, obj, TRIG_TOKEN_RECITE, NULL, 0,0,0,0,0, level,0,0,0,0);
	else
		(*(skill->recite_fun)) (skill, level, ch, scroll, vo, target);

    if ((skill->target == TAR_CHAR_OFFENSIVE || (skill->target == TAR_OBJ_CHAR_OFF && target == TARGET_CHAR)) &&
		victim != NULL && victim != ch && victim->master != ch)
    {
		CHAR_DATA *vch;
		CHAR_DATA *vch_next;

		for (vch = ch->in_room->people; vch; vch = vch_next)
		{
			vch_next = vch->next_in_room;
			if (victim == vch && victim->fighting == NULL)
			{
				multi_hit(victim, ch, NULL, TYPE_UNDEFINED);
				break;
			}
		}
    }
}


void recite_end(CHAR_DATA *ch)
{
	CHAR_DATA *victim;
	OBJ_DATA *scroll;
	OBJ_DATA *obj;
	//int kill;
	//SPELL_DATA *spell;
	char buf[MSL];

	scroll = ch->recite_scroll;

	act("{W$n has completed reciting the scroll.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{WYou complete reciting the scroll.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	if (scroll == NULL)
	{
		send_to_char("The scroll has vanished.\n\r", ch);
		return;
	}

	if (ch->cast_target_name == NULL)
	{
		sprintf(buf, "recite_end: for %s, cast_target_name was null!",
			IS_NPC(ch) ? ch->short_descr : ch->name);
		bug(buf, 0);
		return;
	}

	victim = get_char_room(ch, NULL, ch->cast_target_name);
	obj    = get_obj_here (ch, NULL, ch->cast_target_name);

	free_string(ch->cast_target_name);
	ch->cast_target_name = NULL;

	if (victim == NULL && obj == NULL)
	{
		send_to_char("Your target has left the room.\n\r", ch);
		return;
	}

	if (scroll == NULL)
	{
		act("The scroll has disappeared.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	
#if 0
	kill = find_spell(ch, "kill");
	for (spell = scroll->spells; spell != NULL; spell = spell->next)
	{
		if (spell->sn == kill) {
			act("$p explodes into dust!", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ALL);
			extract_obj(scroll);
			return;
		}
	}
#endif

	if( p_percent_trigger( NULL, scroll, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_RECITE, NULL,0,0,0,0,0) <= 0 )
	{
		act("$p flares brightly then disappears!", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ALL);

		if (number_percent() >= 20 + get_skill(ch, gsk_scrolls) * 4/5)
		{
			send_to_char("You mispronounce a syllable.\n\r",ch);
			check_improve(ch,gsk_scrolls,false,2);
		}
		else
		{
			ITERATOR spit;
			SPELL_DATA *spell;
			iterator_start(&spit, SCROLL(scroll)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&spit)))
			{
				//send_to_char(spell->skill->name, ch);
				//send_to_char("\n\r", ch);
				obj_recite_spell(scroll, spell->skill, spell->level, ch, victim, obj);
			}
			iterator_stop(&spit);

			check_improve(ch,gsk_scrolls,true,2);
		}

		extract_obj(scroll);
	}
}


void obj_brandish_spell(OBJ_DATA *weapon, SKILL_DATA *skill, int level, CHAR_DATA *ch, CHAR_DATA *victim)
{
    if (!IS_VALID(skill))
	{
		bug("obj_brandish_spell: bad skill data.", 0);
		return;
	}

	if (skill->token)
	{
		if (!get_script_token(skill->token, TRIG_TOKEN_BRANDISH, TRIGSLOT_SPELL))
		{
			bug("obj_brandish_spell: bad skill uid = %d.", skill->uid);
			return;
		}
	}
	else if (!skill->brandish_fun)
    {
		bug("obj_brandish_spell: bad skill uid = %d.", skill->uid);
		return;
    }

	// Does the spell need to be checked for deflections to switch victim?
	CHAR_DATA *tch;
	check_spell_deflection_new(ch, victim, skill, false, &tch, NULL);
	if (!tch)
		return;
	
	victim = tch;

	//char _buf[MSL];
	if (skill->token)
		p_token_index_percent_trigger(skill->token, ch, victim, NULL, weapon, NULL, TRIG_TOKEN_BRANDISH, NULL, 0,0,0,0,0, level,0,0,0,0);
	else
		(*(skill->brandish_fun)) (skill, level, ch, weapon, victim);
}

void do_brandish(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;
    OBJ_DATA *weapon;
	SKILL_DATA *weapon_skill;

	if ((weapon = get_obj_carry(ch, argument, ch)) == NULL)
	{
		send_to_char("You do not have that weapon.\n\r", ch);
		return;
	}

	if (!IS_WEAPON(weapon) || list_size(WEAPON(weapon)->spells) < 1)
	{
		send_to_char("You can only brandish imbued weapons.\n\r", ch);
		return;
	}

	if (weapon->wear_loc != WEAR_WIELD && weapon->wear_loc != WEAR_SECONDARY)
	{
		act("You must wielding $p to brandish it.",  ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	int ret = p_percent_trigger(NULL, weapon, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBRANDISH, argument,0,0,0,0,0);
	if (ret > 0)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot brandish that.\n\r", ch);
		}
    	return;
	}

    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);

    if (WEAPON(weapon)->charges > 0)
    {
		weapon_skill = get_objweapon_sn(weapon);

		act("$n brandishes $p.", ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_ROOM);
		act("You brandish $p.",  ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_CHAR);
		if (ch->tot_level < weapon->level || number_percent() >= (20 + 4 * get_skill(ch, weapon_skill) / 5))
		{
			act ("You fail to invoke $p.",ch, NULL, NULL,weapon, NULL, NULL,NULL,TO_CHAR);
			act ("...and nothing happens.",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM);
			check_improve(ch,gsk_staves,false,2);
		}
		else
		{
			ITERATOR it;
			SPELL_DATA *spell;
		    for (vch = ch->in_room->people; vch; vch = vch_next)
		    {
				vch_next	= vch->next_in_room;

				bool offensive = false;
				switch(spell->skill->target)
				{
				default:
					bug(formatf("Bad target for spell %s.", spell->skill->name), 0);
					continue;

				case TAR_IGNORE:
					if (vch != ch)
						continue;
					break;

				case TAR_CHAR_OFFENSIVE:
					if (IS_NPC(ch) == IS_NPC(vch))
						continue;

					offensive = true;
					break;

				case TAR_CHAR_DEFENSIVE:
					if (IS_NPC(ch) != IS_NPC(vch))
						continue;
					break;

				case TAR_CHAR_SELF:
					if (vch != ch)
						continue;
					break;
				}

				iterator_start(&it, WEAPON(weapon)->spells);
				while((spell = (SPELL_DATA *)iterator_nextdata(&it)))
				{
					obj_brandish_spell(weapon, spell->skill, spell->level, ch, vch);
				}
				iterator_stop(&it);

				// Let it do something even if there were no spells
			    p_percent_trigger(NULL, weapon, NULL, NULL, ch, vch, NULL, NULL, NULL, TRIG_BRANDISH, argument,0,0,0,0,0);

				if (offensive && vch != ch && vch->master != ch && vch->fighting == NULL)
				{
					multi_hit(vch, ch, NULL, TYPE_UNDEFINED);
				}
			}

			check_improve(ch,weapon_skill,true,2);
		}
    }

	// When $(victim) isn't defined, it's the aftermath
	p_percent_trigger(NULL, weapon, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BRANDISH, argument,0,0,0,0,0);

	if (WEAPON(weapon)->charges > 0)
	{
		--WEAPON(weapon)->charges;
		if (WEAPON(weapon)->charges < 1)
		{
			// If a weapon can't recharge, it will explode
			if (WEAPON(weapon)->recharge_time < 1)
			{
				// TODO: Make this a trigger?
				act("$n's $p blazes bright and is gone.", ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_ROOM);
				act("Your $p blazes bright and is gone.", ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_CHAR);
				extract_obj(weapon);
				return;
			}

			// TODO: What should happen if a weapon is overused?
			// Should it damage the weapon?
			// If so, should the condition of the weapon affect whether the weapon works?
			// Wand gets damaged from overuse
			if (weapon->fragility != OBJ_FRAGILE_SOLID)
			{
				switch (weapon->fragility)
				{
				case OBJ_FRAGILE_STRONG:
					if (number_percent() < 25)
						weapon->condition--;
					break;
				case OBJ_FRAGILE_NORMAL:
					if (number_percent() < 50)
						weapon->condition--;
					break;
				case OBJ_FRAGILE_WEAK:
					weapon->condition--;
					break;
				default:
					break;
				}

				if (weapon->condition <= 0)
				{
					// TODO: Make a trigger?
					act("$n's $p blazes bright and is gone.", ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_ROOM);
					act("Your $p blazes bright and is gone.", ch, NULL, NULL, weapon, NULL, NULL, NULL, TO_CHAR);
					extract_obj(weapon);
					return;
				}
			}
		}

		// Start recharging
		if (WEAPON(weapon)->charges < WEAPON(weapon)->max_charges && WEAPON(weapon)->recharge_time > 0 && WEAPON(weapon)->cooldown < 1)
		{
			WEAPON(weapon)->cooldown = WEAPON(weapon)->recharge_time;
		}
	}
}

void obj_zap_spell(OBJ_DATA *wand, SKILL_DATA *skill, int level, CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *obj)
{
    void *vo;
    int target = TARGET_NONE;

    if (!IS_VALID(skill))
	{
		bug("obj_zap_spell: bad skill data.", 0);
		return;
	}

	if (skill->token)
	{
		if (!get_script_token(skill->token, TRIG_TOKEN_ZAP, TRIGSLOT_SPELL))
		{
			bug("obj_zap_spell: bad skill uid = %d.", skill->uid);
			return;
		}
	}
	else if (!skill->zap_fun)
    {
		bug("obj_zap_spell: bad skill uid = %d.", skill->uid);
		return;
    }

    switch (skill->target)
    {
        default:
	    bug("obj_zap_spell: bad target for skill uid = %d.", skill->uid);
	    return;

	case TAR_IGNORE:
	    vo = NULL;
		victim = NULL;
		obj = NULL;
	    break;

	case TAR_CHAR_OFFENSIVE:
	    if (victim == NULL)
  	        victim = ch->fighting;

	    if (victim == NULL)
	    {
	        send_to_char("You can't do that.\n\r", ch);
			return;
	    }

	    if (is_safe(ch,victim, true) && ch != victim)
	    {
			send_to_char("Something isn't right...\n\r",ch);
			return;
	    }

	    vo = (void *) victim;
	    target = TARGET_CHAR;
		obj = NULL;
	    break;

	case TAR_CHAR_DEFENSIVE:
	case TAR_CHAR_SELF:
	    if (victim == NULL)
		    victim = ch;

	    vo = (void *) victim;
	    target = TARGET_CHAR;
		obj = NULL;
	    break;

	case TAR_OBJ_INV:
	    if (obj == NULL)
	    {
	        send_to_char("You can't do that.\n\r", ch);
		return;
	    }
	    vo = (void *) obj;
	    target = TARGET_OBJ;
		victim = NULL;
	    break;

	case TAR_OBJ_CHAR_OFF:
	    if (victim == NULL && obj == NULL)
	    {
	        if (ch->fighting != NULL)
	   	    	victim = ch->fighting;
  	        else
	        {
			    send_to_char("You can't do that.\n\r",ch);
		   	    return;
			}
	    }

	    if (victim != NULL)
	    {
	        if (is_safe_spell(ch,victim,false) && ch != victim)
	        {
	            send_to_char("Something isn't right...\n\r",ch);
			    return;
			}

			vo = (void *) victim;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else
	    {
		    vo = (void *) obj;
		    target = TARGET_OBJ;
			victim = NULL;
	    }
	    break;


	case TAR_OBJ_CHAR_DEF:
	    if (victim == NULL && obj == NULL)
	    {
	        vo = (void *) ch;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else if (victim != NULL)
	    {
			vo = (void *) victim;
			target = TARGET_CHAR;
			obj = NULL;
	    }
	    else
	    {
			vo = (void *) obj;
			target = TARGET_OBJ;
			obj = NULL;
	    }

	    break;
    }

	// Does the spell need to be checked for deflections to switch victim?
	if (target == TARGET_CHAR && victim != NULL)
	{
		CHAR_DATA *tch;
		check_spell_deflection_new(ch, victim, skill, false, &tch, NULL);
		if (!tch)
			return;
		
		victim = tch;
		vo = (void *) tch;
	}

	if (skill->token)
		p_token_index_percent_trigger(skill->token, ch, victim, NULL, wand, obj, TRIG_TOKEN_ZAP, NULL, 0,0,0,0,0, level,0,0,0,0);
	else
		(*(skill->zap_fun)) (skill, level, ch, wand, vo, target);

    if ((skill->target == TAR_CHAR_OFFENSIVE || (skill->target == TAR_OBJ_CHAR_OFF && target == TARGET_CHAR)) &&
		victim != NULL && victim != ch && victim->master != ch)
    {
		CHAR_DATA *vch;
		CHAR_DATA *vch_next;

		for (vch = ch->in_room->people; vch; vch = vch_next)
		{
			vch_next = vch->next_in_room;
			if (victim == vch && victim->fighting == NULL)
			{
				multi_hit(victim, ch, NULL, TYPE_UNDEFINED);
				break;
			}
		}
    }
}

// zap <wand> <target>
void do_zap(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *wand;
    OBJ_DATA *obj;

    argument = one_argument(argument, arg);
    if (arg[0] == '\0' && ch->fighting == NULL)
    {
		send_to_char("Zap whom or what?\n\r", ch);
		return;
    }

	if ((wand = get_obj_carry(ch, arg, ch)) == NULL)
	{
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_WAND(wand))
	{
		send_to_char("You cannot zap that.\n\r", ch);
		return;
	}

	// TODO: Utilize return code
    if(p_percent_trigger(NULL, wand, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREZAP, arg,0,0,0,0,0))
    	return;

    obj = NULL;
    if (argument[0] == '\0')
    {
		if (ch->fighting != NULL)
		    victim = ch->fighting;
		else
		{
			send_to_char("Zap whom or what?\n\r", ch);
			return;
		}
    }
    else
    {
		if ((victim = get_char_room (ch, NULL,argument)) == NULL &&
			(obj    = get_obj_here  (ch, NULL,argument)) == NULL)
		{
			send_to_char("You can't find it.\n\r", ch);
			return;
		}
    }

    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);

    if (WAND(wand)->charges > 0)
    {
		if (victim != NULL)
		{
			act("$n zaps $N with $p.", ch, victim, NULL, wand, NULL, NULL, NULL, TO_NOTVICT);
			act("You zap $N with $p.", ch, victim, NULL, wand, NULL, NULL, NULL, TO_CHAR);
			act("$n zaps you with $p.",ch, victim, NULL, wand, NULL, NULL, NULL, TO_VICT);
		}
		else
		{
			act("$n zaps $P with $p.", ch, NULL, NULL, wand, obj, NULL, NULL, TO_ROOM);
			act("You zap $P with $p.", ch, NULL, NULL, wand, obj, NULL, NULL, TO_CHAR);
		}

		bool success;
		if (ch->tot_level < wand->level || number_percent() >= (20 + 4 * get_skill(ch, gsk_wands) / 5))
		{
			act("Your efforts with $p produce only smoke and sparks.", ch, NULL, NULL,wand, NULL, NULL,NULL,TO_CHAR);
			act("$n's efforts with $p produce only smoke and sparks.", ch, NULL, NULL,wand, NULL, NULL,NULL,TO_ROOM);
			success = false;
		}
		else
		{
			ITERATOR spit;
			SPELL_DATA *spell;
			iterator_start(&spit, WAND(wand)->spells);
			while((spell = (SPELL_DATA *)iterator_nextdata(&spit)))
			{
				// If the wand is too damaged, the wand can fail.
				if (number_range(0, 64) > wand->condition)
				{
					act("$n's $p sparks and sputters.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_ROOM);
					act("Your $p sparks and sputters.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_CHAR);
					break;
				}

				obj_zap_spell(wand, spell->skill, spell->level, ch, victim, obj);
			}
			iterator_stop(&spit);
			success = true;
		}

		p_percent_trigger(NULL, wand, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_ZAP, argument,success,0,0,0,0);
		check_improve(ch,gsk_wands,success,2);
    }

	--WAND(wand)->charges;
    if (WAND(wand)->charges < 1)
    {
		// If a wand can't recharge, it will explode
		if (WAND(wand)->recharge_time < 1)
		{
			act("$n's $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_ROOM);
			act("Your $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_CHAR);
			extract_obj(wand);
			return;
		}

		// Wand gets damaged from overuse
		if (wand->fragility != OBJ_FRAGILE_SOLID)
		{
			switch (wand->fragility)
			{
			case OBJ_FRAGILE_STRONG:
				if (number_percent() < 25)
					wand->condition--;
				break;
			case OBJ_FRAGILE_NORMAL:
				if (number_percent() < 50)
					wand->condition--;
				break;
			case OBJ_FRAGILE_WEAK:
				wand->condition--;
				break;
			default:
				break;
			}

			if (wand->condition <= 0)
			{
				act("$n's $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_ROOM);
				act("Your $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_CHAR);
				extract_obj(wand);
				return;
			}
		}
    }

	// Start recharging
	if (WAND(wand)->charges < WAND(wand)->max_charges && WAND(wand)->recharge_time > 0 && WAND(wand)->cooldown < 1)
	{
		WAND(wand)->cooldown = WAND(wand)->recharge_time;
	}
}

void do_steal(CHAR_DATA *ch, char *argument)
{
    char buf  [MAX_STRING_LENGTH];
    char arg1 [MAX_INPUT_LENGTH];
    char arg2 [MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    int percent;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Steal what from whom?\n\r", ch);
	return;
    }

    if ((victim = get_char_room(ch, NULL, arg2)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (victim == ch)
    {
	send_to_char("That's pointless.\n\r", ch);
	return;
    }

    if (IS_IMMORTAL(victim) && !IS_IMMORTAL(ch)) {
	send_to_char("You can't steal from immortals.\n\r", ch);
	return;
    }

    if (IS_DEAD(ch))
    {
	send_to_char("Your hands pass through the object.\n\r", ch);
	return;
    }

    if (IS_DEAD(victim))
    {
	send_to_char("You can't steal from a shadow.\n\r", ch);
	return;
    }

    if (is_safe(ch,victim, true))
	return;

    if (IS_NPC(victim)
    &&  victim->position == POS_FIGHTING)
    {
	send_to_char( "Kill stealing is not permitted.\n\r"
		       "You'd better not -- you might get hit.\n\r",ch);
	return;
    }

    WAIT_STATE(ch, gsk_steal->beats);
    percent  = number_percent();

    if (!IS_AWAKE(victim))
    	percent -= 10;
    else if (!can_see(victim,ch))
    	percent += 25;
    else
    {
		if (ch->pcdata->current_class->clazz == gcl_highwayman)
        {
		    if (ch->heldup == victim)
		        percent = 0;
	    	else
				percent += 25;
		}
		else
		    percent += 50;
    }

    if (percent > get_skill(ch, gsk_steal)
         || (!IS_NPC(victim)
	     && number_percent() < get_skill(victim, gsk_deception))
         || (!IS_NPC(ch)
	      && !IS_NPC(victim)
	      && !IS_SET(ch->in_room->room_flag[0], ROOM_CHAOTIC|ROOM_PK)))	// Require full CPK
    {
	send_to_char("Oops.\n\r", ch);
	affect_strip(ch, gsk_sneak);
	REMOVE_BIT(ch->affected_by[0],AFF_SNEAK);

	act("$n tried to steal from you.\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT   );
	act("$n tried to steal from $N.\n\r",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	switch(number_range(0,3))
	{
	case 0 :
	   sprintf(buf, "%s is a lousy thief!", ch->name);
	   break;
        case 1 :
	   sprintf(buf, "%s couldn't rob %s way out of a paper bag!",
		    ch->name,(ch->sex == 2) ? "her" : "his");
	   break;
	case 2 :
	    sprintf(buf,"%s tried to rob me!",ch->name);
	    break;
	case 3 :
	    sprintf(buf,"Keep your hands out of there, %s!",ch->name);
	    break;
        }
        if (!IS_AWAKE(victim))
            do_function(victim, &do_wake, "");
	if (IS_AWAKE(victim))
	    do_function(victim, &do_yell, buf);
	if (!IS_NPC(ch))
	{
	    if (IS_NPC(victim))
	    {
	        check_improve(ch,gsk_steal,false,2);
			multi_hit(victim, ch, NULL, TYPE_UNDEFINED);
	    }
	}

	return;
    }

    if (!str_cmp(arg1, "coin" )
    ||   !str_cmp(arg1, "coins")
    ||   !str_cmp(arg1, "gold" )
    ||	 !str_cmp(arg1, "silver"))
    {
	int gold, silver;

	gold = victim->gold * number_range(1, ch->level) / MAX_LEVEL;
	silver = victim->silver * number_range(1,ch->level) / MAX_LEVEL;
	if (gold <= 0 && silver <= 0)
	{
	    send_to_char("You couldn't get any coins.\n\r", ch);
	    return;
	}

	ch->gold     	+= gold;
	ch->silver   	+= silver;
	victim->silver 	-= silver;
	victim->gold 	-= gold;
	if (silver <= 0)
	    sprintf(buf, "Bingo!  You got %d gold coins.\n\r", gold);
	else if (gold <= 0)
	    sprintf(buf, "Bingo!  You got %d silver coins.\n\r",silver);
	else
	    sprintf(buf, "Bingo!  You got %d silver and %d gold coins.\n\r",
		    silver,gold);

	send_to_char(buf, ch);
	check_improve(ch,gsk_steal,true,2);
	return;
    }

    if ((obj = get_obj_carry(victim, arg1, ch)) == NULL)
    {
	send_to_char("You can't find it.\n\r", ch);
	return;
    }

    if (!IS_SET(ch->in_room->room_flag[0], ROOM_CHAOTIC|ROOM_PK) && !IS_NPC(victim) && !IS_NPC(ch))
    {
	send_to_char("You can only steal items in a chaotic room.\n\r", ch);
	return;
    }

    if (!can_drop_obj(ch, obj, true))
    {
	send_to_char("You can't pry it away.\n\r", ch);
	return;
    }

	/*
	TODO: Allow highway man to steal stuff from shopkeepers
    if (
         ch->pcdata->second_sub_class_thief != CLASS_THIEF_HIGHWAYMAN
         && (IS_SET(obj->extra[0], ITEM_INVENTORY)
         ||   obj->level > ch->tot_level + 30))
    {
	send_to_char("You can't pry it away.\n\r", ch);
	return;
    }
    */

    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
    {
	send_to_char("You have your hands full.\n\r", ch);
	return;
    }

    if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch))
    {
	send_to_char("You can't carry that much weight.\n\r", ch);
	return;
    }

    obj_from_char(obj);
    obj_to_char(obj, ch);
	REMOVE_BIT(obj->extra[1], ITEM_KEPT);
    act("You pocket $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
    check_improve(ch,gsk_steal,true,2);
    send_to_char("{WGot it!{x\n\r", ch);
}

bool has_stock_reputation(CHAR_DATA *ch, SHOP_STOCK_DATA *stock)
{
	if (!IS_VALID(stock->reputation)) return true;	// No requirements, so ignore.

	REPUTATION_DATA *rep = find_reputation_char(ch, stock->reputation);
	int rank = IS_VALID(rep) ? rep->current_rank : stock->reputation->initial_rank;

	if (stock->min_reputation_rank > 0 && rank < stock->min_reputation_rank) return false;
	if (stock->max_reputation_rank > 0 && rank > stock->max_reputation_rank) return false;

	return true;
}

bool can_see_stock_reputation(CHAR_DATA *ch, SHOP_STOCK_DATA *stock)
{
	if (!IS_VALID(stock->reputation)) return true;	// No requirements, so ignore.

	REPUTATION_DATA *rep = find_reputation_char(ch, stock->reputation);
	int rank = IS_VALID(rep) ? rep->current_rank : stock->reputation->initial_rank;

	if (stock->min_show_rank > 0 && rank < stock->min_show_rank) return false;
	if (stock->max_show_rank > 0 && rank > stock->max_show_rank) return false;

	return true;
}


CHAR_DATA *find_keeper(CHAR_DATA *ch)
{
    /*char buf[MAX_STRING_LENGTH];*/
    CHAR_DATA *keeper;
    SHOP_DATA *pShop;

    pShop = NULL;
    for (keeper = ch->in_room->people; keeper; keeper = keeper->next_in_room)
    {
		if (IS_NPC(keeper) && (pShop = keeper->shop) != NULL)
		{
			if (IS_VALID(pShop->reputation))
			{
				// If shopkeeper has a reputation requirement, make sure they meet it.
				REPUTATION_DATA *rep = find_reputation_char(ch, pShop->reputation);
				int repRank = IS_VALID(rep) ? rep->current_rank : pShop->reputation->initial_rank;

				if (repRank >= pShop->min_reputation_rank)
				{
					break;
				}
				else
					pShop = NULL;
			}
			else
				break;
		}
    }

    if (!keeper || !pShop)
    {
		send_to_char("You can't do that here.\n\r", ch);
		return NULL;
    }

    /*
     * Shop hours.
     */
    if (time_info.hour < pShop->open_hour)
    {
	do_function(keeper, &do_say, "Sorry, I am closed. Come back later.");
	return NULL;
    }

    if (time_info.hour > pShop->close_hour)
    {
	do_function(keeper, &do_say, "Sorry, I am closed. Come back tomorrow.");
	return NULL;
    }

    /*
     * Invisible or hidden people.
     */
    if (!can_see(keeper, ch))
    {
	do_function(keeper, &do_say, "I don't trade with folks I can't see.");
	return NULL;
    }

    return keeper;
}


/* insert an object at the right spot for the keeper */
void obj_to_keeper(OBJ_DATA *obj, CHAR_DATA *ch)
{
    OBJ_DATA *t_obj, *t_obj_next;

    /* see if any duplicates are found */
    for (t_obj = ch->carrying; t_obj != NULL; t_obj = t_obj_next)
    {
	t_obj_next = t_obj->next_content;

	if (obj->pIndexData == t_obj->pIndexData
	&&  !str_cmp(obj->short_descr,t_obj->short_descr))
	{
	    obj->cost = t_obj->cost; /* keep it standard */
	    break;
	}
    }

    if (t_obj == NULL)
    {
	obj->next_content = ch->carrying;
	ch->carrying = obj;
    }
    else
    {
	obj->next_content = t_obj->next_content;
	t_obj->next_content = obj;
    }


	if(!IS_SET(obj->extra[1], ITEM_SELL_ONCE))
	{
		// If the item can be sold again, mark it as inventory
		SET_BIT(obj->extra[0], ITEM_INVENTORY);
	}
    obj->carried_by      = ch;
    obj->in_room         = NULL;
    obj->in_obj          = NULL;
    ch->carry_number    += get_obj_number(obj);
    ch->carry_weight    += get_obj_weight(obj);
}

long adjust_keeper_price(CHAR_DATA *keeper, long price, bool fBuy)
{
    if (keeper->shop == NULL)
		return 0;

	if (fBuy)
	{
		return price * keeper->shop->profit_buy / 100;
    }
    else
    {
		return price * keeper->shop->profit_sell / 100;
	}
}

SHOP_STOCK_DATA *get_stockonly_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	SHOP_STOCK_DATA *stock;
	int number;
	int count;

    number = number_argument(argument, arg);
    count  = 0;
    if( keeper->shop != NULL) {
		// Check stock items first
		for(stock = keeper->shop->stock; stock; stock = stock->next)
		{
			// Out of stock.
			if( stock->max_quantity > 0 && stock->quantity < 1) continue;

			if( stock->wnum.pArea && stock->wnum.vnum > 0 )
			{
				if( stock->obj != NULL )
				{
					if( is_name(arg, stock->obj->name) )
					{
						if( ++count == number )
						{
							return stock;
						}
					}
				}
				else if( stock->mob != NULL )
				{
					if( is_name(arg, stock->mob->player_name) )
					{
						if( ++count == number )
						{
							return stock;
						}
					}
				}
				else if( stock->ship != NULL )
				{
					if( is_name(arg, stock->ship->name) )
					{
						if( ++count == number )
						{
							return stock;
						}
					}
				}
			}
			else if(!IS_NULLSTR(stock->custom_keyword))
			{
				if( is_name(arg, stock->custom_keyword) )
				{
					if( ++count == number )
					{
						return stock;
					}
				}
			}
		}
	}

	return NULL;
}

bool get_stock_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, SHOP_REQUEST_DATA *request, char *argument)
{
	char arg[MAX_INPUT_LENGTH];
	SHOP_STOCK_DATA *stock;
	OBJ_DATA *obj;
	int number;
	int count;

	number = number_argument(argument, arg);
	count  = 0;
    if( keeper->shop != NULL) {
		// Check stock items first
		for(stock = keeper->shop->stock; stock; stock = stock->next)
		{
			// Out of stock.
			if( stock->max_quantity > 0 && stock->quantity < 1) continue;

			// Skip here if the item is to be hidden from view when unavailable.
			if ( !can_see_stock_reputation(ch, stock) ) continue;

			if( stock->wnum.pArea && stock->wnum.vnum > 0 )
			{
				if( stock->obj != NULL )
				{
					if( is_name(arg, stock->obj->name) )
					{
						if( ++count == number )
						{
							request->stock = stock;
							request->obj = NULL;
							return true;
						}
					}
				}
				else if( stock->mob != NULL )
				{
					if( is_name(arg, stock->mob->player_name) )
					{
						if( ++count == number )
						{
							request->stock = stock;
							request->obj = NULL;
							return true;
						}
					}
				}
				else if( stock->ship != NULL )
				{
					if( is_name(arg, stock->ship->name) )
					{
						if( ++count == number )
						{
							request->stock = stock;
							request->obj = NULL;
							return true;
						}
					}
				}

			}
			else if(!IS_NULLSTR(stock->custom_keyword))
			{
				if( is_name(arg, stock->custom_keyword) )
				{
					if( ++count == number )
					{
						request->stock = stock;
						request->obj = NULL;
						return true;
					}
				}
			}
		}
	}

	for (obj = keeper->carrying; obj != NULL; obj = obj->next_content)
	{
		if (IS_OBJ_STAT(obj, ITEM_INVENTORY) &&
			obj->wear_loc == WEAR_NONE &&
        	can_see_obj(keeper, obj) &&
        	can_see_obj(ch,obj) &&
        	is_name(arg, obj->name))
        {
			if (++count == number)
			{
				request->stock = NULL;
				request->obj = obj;
				return true;
			}

			/* skip other objects of the same name */
			while (obj->next_content != NULL &&
				obj->pIndexData == obj->next_content->pIndexData &&
				!str_cmp(obj->short_descr,obj->next_content->short_descr))
				obj = obj->next_content;
		}
	}


	return false;
}


/* get an object from a shopkeeper's list */
// UNUSED
OBJ_DATA *get_obj_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;


	for (obj = keeper->carrying; obj != NULL; obj = obj->next_content)
	{
		if (obj->wear_loc == WEAR_NONE &&
        	can_see_obj(keeper, obj) &&
        	can_see_obj(ch,obj) &&
        	is_name(arg, obj->name))
        {
			if (++count == number)
			{
				return obj;
			}

			/* skip other objects of the same name */
			while (obj->next_content != NULL &&
				obj->pIndexData == obj->next_content->pIndexData &&
				!str_cmp(obj->short_descr,obj->next_content->short_descr))
				obj = obj->next_content;
		}
	}


    return NULL;
}


int get_cost(CHAR_DATA *keeper, OBJ_DATA *obj, bool fBuy)
{
    SHOP_DATA *pShop;
    int cost;

    if (obj == NULL || (pShop = keeper->shop) == NULL)
		return 0;

    if (fBuy)
    {
		cost = obj->cost * pShop->profit_buy  / 100;
    }
    else
    {
		OBJ_DATA *obj2;
		int itype;

		cost = 0;
		for (itype = 0; itype < MAX_TRADE; itype++)
		{
		    if (obj->item_type == pShop->buy_type[itype])
		    {
				cost = obj->cost * pShop->profit_sell / 100;
				break;
		    }
		}

		for (obj2 = keeper->carrying; obj2; obj2 = obj2->next_content)
		{
		    if (IS_OBJ_STAT(obj2,ITEM_INVENTORY) &&
				obj->pIndexData == obj2->pIndexData &&
		    	!str_cmp(obj->short_descr,obj2->short_descr))
			{
				cost = cost * 3 / 4;
			}
		}
    }

	/*
    if (obj->item_type == ITEM_STAFF || obj->item_type == ITEM_WAND)
    {
		if (obj->value[1] == 0)
		    cost /= 4;
		else
		    cost = cost * obj->value[2] / obj->value[1];
    }
	*/

    return cost;
}


void do_buy(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	long cost;
	int roll;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    bool haggled = false;

    if (argument[0] == '\0')
    {
		send_to_char("Buy what?\n\r", ch);
		return;
    }


	{
		//////////////////////////////////////////
		//
		// NORMAL SHOP
		//
		//////////////////////////////////////////

		CHAR_DATA *keeper;
		OBJ_DATA *obj,*t_obj;
		SHOP_REQUEST_DATA request;
		int number, count = 1;

		if ((keeper = find_keeper(ch)) == NULL)
			return;

		check_mob_factions(ch, keeper);

		argument = one_argument(argument, arg);
		if( is_number(arg) )
		{
			argument = one_argument(argument, arg2);
		}
		else
		{
			arg2[0] = '\0';
		}


		bool found;

		// Find the stock item
		if (arg2[0] == '\0')
		{
			number = 1;
			found = get_stock_keeper(ch, keeper, &request, arg);
		}
		else
		{
			number = atoi(arg);
			found = get_stock_keeper(ch, keeper, &request, arg2);
		}

		if(!found)
		{
			act("{R$n tells you 'I don't sell that -- try 'list''.{x",
				keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			ch->reply = keeper;
			return;
		}

		// Check that they meet the reputation requirements.
		if (!has_stock_reputation(ch, request.stock))
		{
			act("{R$n tells you 'You may not buy that.'{x",
				keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			ch->reply = keeper;
			return;
		}
		

		if (number < 1 || number > 150)
		{
			act("{R$n tells you 'Get real!'{x",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
			return;
		}

		if( request.obj != NULL )
		{
			//
			// Attempting to buy a non-stock object
			obj = request.obj;

			keeper->tempstore[0] = number;
			int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREBUY_OBJ, NULL,0,0,0,0,0);
			if( ret > 0 ) return;	// Messages should be done in the script
			if( ret < 0 )
			{
				// Error happened.  Give generic message
				act("{R$n tells you 'I can't sell that'.{x",
					keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				ch->reply = keeper;
				return;
			}

			cost = get_cost(keeper, obj, true);

			for (t_obj = obj->next_content;
				count < number && t_obj != NULL;
				t_obj = t_obj->next_content)
			{
				if (t_obj->pIndexData == obj->pIndexData &&
					IS_OBJ_STAT(t_obj, ITEM_INVENTORY) &&
					!str_cmp(t_obj->short_descr,obj->short_descr))
					count++;
				else
					break;
			}

			if (count < number || IS_SET(obj->extra[1], ITEM_SELL_ONCE))
			{
				act("{R$n tells you 'I don't have that many in stock.{x",
					keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
				ch->reply = keeper;
				return;
			}

			cost = cost * number;
			if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
			{
				/* haggle */
				roll = number_percent();
				if (roll < get_skill(ch, gsk_haggle))
				{
					cost -= ((cost/2) * roll)/100;
					haggled = true;
					check_improve(ch,gsk_haggle,true,4);
				}
			}

			if ((ch->silver + ch->gold * 100) < cost)
			{
				if (number > 1)
					act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT);
				else
					act("{R$n tells you 'You can't afford to buy $p'.{x", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT);
				ch->reply = keeper;
				return;
			}
			if (ch->carry_number +  number * get_obj_number(obj) > can_carry_n(ch))
			{
				send_to_char("You can't carry that many items.\n\r", ch);
				return;
			}

			if (get_carry_weight(ch) + number * get_obj_weight(obj) > can_carry_w(ch))
			{
				send_to_char("You can't carry that much weight.\n\r", ch);
				return;
			}

			if (haggled)
				act("You haggle with $N.",ch,keeper, NULL, NULL, NULL, NULL, NULL,TO_CHAR);

			if (number > 1)
			{
				sprintf(buf,"$n buys $p[%d].",number);
				act(buf,ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
				sprintf(buf,"You buy $p[%d] for%s.", number, get_shop_purchase_price(cost, 0, 0, 0));
				act(buf,ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			}
			else
			{
				act("$n buys $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				sprintf(buf,"You buy $p for%s.", get_shop_purchase_price(cost, 0, 0, 0));
				act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			}

			deduct_cost(ch,cost);
			keeper->gold += cost/100;
			keeper->silver += cost - (cost/100) * 100;

			for (obj = request.obj, count = 0; count < number && obj != NULL; obj = t_obj)
			{
				t_obj = obj->next_content;
				if (obj->pIndexData == request.obj->pIndexData &&
					IS_OBJ_STAT(obj, ITEM_INVENTORY) &&
					!str_cmp(obj->short_descr,request.obj->short_descr))
				{
					obj_from_char(obj);

					if (obj->timer > 0)
						obj->timer = 0;

					obj_to_char(obj, ch);
					if (cost < obj->cost)
						obj->cost = cost;

					count++;
				}

			}
		}
		else
		{
			SHIP_DATA *target_ship = NULL;	// Used by STOCK_CREW

			SHOP_STOCK_DATA *stock = request.stock;
			char pricestr[MIL+1];

			// Attempting to buy from stock
			keeper->tempstore[0] = number;
			keeper->tempstore[1] = stock->type;
			keeper->tempstore[2] = stock->wnum.pArea ? stock->wnum.pArea->uid : 0;
			keeper->tempstore[3] = stock->wnum.vnum;
			int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBUY, stock->custom_keyword,0,0,0,0,0);
			if( ret > 0 ) return;	// Messages should be done in the script
			if( ret < 0 )
			{
				// Error happened.  Give generic message
				act("{R$n tells you 'I can't sell that'.{x",
					keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				ch->reply = keeper;
				return;
			}

			if( (stock->mob != NULL || stock->ship != NULL || stock->singular) && number > 1 )
			{
				send_to_char("You can only purchase one of those at a time.\n\r", ch);
				return;
			}

			if( stock->max_quantity > 0 && number > stock->quantity )
			{
				act("{R$n tells you 'I do not have that many.'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				ch->reply = keeper;
				return;
			}

			if( stock->obj != NULL )
			{
				if ((ch->carry_number + number) > can_carry_n(ch))
				{
					send_to_char("You can't carry that many items.\n\r", ch);
					return;
				}

				if ((get_carry_weight(ch) + number * stock->obj->weight) > can_carry_w(ch))
				{
					send_to_char("You can't carry that much weight.\n\r", ch);
					return;
				}
			}
			else if(stock->mob != NULL )
			{
				if( stock->type == STOCK_PET )
				{
					if( ch->pet != NULL )
					{
						send_to_char("You already have a pet.\n\r", ch);
						return;
					}

					if (ch->num_grouped >= 9)
					{
						send_to_char("Your group is full.\n\r", ch);
						return;
					}
				}
				else if( stock->type == STOCK_MOUNT )
				{
					if( ch->mount != NULL )
					{
						send_to_char("You already have a mount.\n\r", ch);
						return;
					}

					if (ch->num_grouped >= 9)
					{
						send_to_char("Your group is full.\n\r", ch);
						return;
					}
				}
				else if( stock->type == STOCK_GUARD )
				{
					if (ch->num_grouped >= 9)
					{
						send_to_char("Your group is full.\n\r", ch);
						return;
					}
				}
				else if( stock->type == STOCK_CREW )
				{
					if( IS_NPC(ch) )
					{
						return;
					}

					// SYNTAX for buying crew: buy <crew> <ship#>         (for personal ships)
					//                         buy <crew> church <ship#>  (for church ships, if max rank in church)

					int chrank = find_char_position_in_church(ch);

					if( IS_NULLSTR(argument) )
					{
						send_to_char("Syntax: buy <crew> <ship#>\n\r", ch);
						if( chrank == CHURCH_RANK_D )
							send_to_char("        buy <crew> church <ship#>\n\r", ch);

						return;
					}

					int index;
					LLIST *ships;

					if( (chrank == CHURCH_RANK_D) )
					{
						char arg5[MIL];

						argument = one_argument(argument, arg5);

						if( str_prefix(arg5, "church") || !is_number(argument) )
						{
							send_to_char("That is not a number.\n\r", ch);
							return;
						}

						send_to_char("Not yet implemented.\n\r", ch);
						return;
					}
					else
					{
						if( !is_number(argument) )
						{
							send_to_char("That is not a number.\n\r", ch);
							return;
						}
						else
						{
							index = atoi(argument);
							ships = ch->pcdata->ships;
						}
					}

					if( index < 1 || index > list_size(ships) )
					{
						send_to_char("That is not a valid ship.\n\r", ch);
						return;
					}

					target_ship = (SHIP_DATA *)list_nthdata(ships, index);

					if( !IS_VALID(target_ship) )
					{
						send_to_char("That is not a valid ship.\n\r", ch);
						return;
					}

					if( list_size(target_ship->crew) >= target_ship->max_crew )
					{
						send_to_char("Ship cannot handle anymore crew.\n\r", ch);
						return;
					}

					if( !target_ship->instance->entrance )
					{
						send_to_char("Cannot find where to place the crew on the ship.\n\r", ch);
						return;
					}
				}

			}
			else if( stock->ship != NULL )
			{
				if( !is_shipyard_valid(keeper->shop->shipyard,
					keeper->shop->shipyard_region[0][0],
					keeper->shop->shipyard_region[0][1],
					keeper->shop->shipyard_region[1][0],
					keeper->shop->shipyard_region[1][1]) )
				{
					send_to_char("The shipyard is currently shutdown at the moment.\n\r", ch);
					return;
				}
			}
			else
			{
				// Check for non-standard objects
				keeper->tempstore[0] = number;
				int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CHECK_BUYER, stock->custom_keyword,0,0,0,0,0);

				if( ret == 1 )
				{
					send_to_char("You can't carry that many items.\n\r", ch);
					return;
				}

				if( ret == 2 )
				{
					send_to_char("You can't carry that much weight.\n\r", ch);
					return;
				}
			}

			int chance = get_skill(ch, gsk_haggle);
			long new_value = 0;

			if( IS_NULLSTR(stock->custom_price) )
			{

				// Check price
				long silver = haggle_price(ch, keeper, chance, number, stock->silver, (ch->silver + 100*ch->gold), stock->discount, &haggled, false);
				if( silver < 0 )
					return;

				long qp = haggle_price(ch, keeper, chance, number, stock->qp, ch->missionpoints, stock->discount, &haggled, false);
				if( qp < 0 )
					return;

				long dp = haggle_price(ch, keeper, chance, number, stock->dp, ch->deitypoints, stock->discount, &haggled, false);
				if( dp < 0 )
					return;

				long pneuma = haggle_price(ch, keeper, chance, number, stock->pneuma, ch->pneuma, stock->discount, &haggled, false);
				if( pneuma < 0 )
					return;

				if( haggled )
				{
					act("You haggle with $N.",ch,keeper, NULL, NULL, NULL, NULL, NULL,TO_CHAR);
					check_improve(ch,gsk_haggle,true,4);
				}

				// Deduct price
				if( silver > 0 )
				{
					deduct_cost(ch,silver);
					keeper->gold += silver/100;
					keeper->silver += silver - (silver/100) * 100;
				}

				if( qp > 0 )
				{
					ch->missionpoints -= qp;
				}

				if( dp > 0 )
				{
					ch->deitypoints -= dp;
				}

				if( pneuma > 0 )
				{
					ch->pneuma -= pneuma;
				}

				new_value = silver + qp + (dp / 100) + pneuma;	// Get some kind of value for resale

				// Default messaging
				strncpy(pricestr, get_shop_purchase_price(silver, qp, dp, pneuma), MIL);
				pricestr[MIL] = '\0';
			}
			else
			{
				// Handle the custom pricing
				// - entire currency transaction needs to take place
				keeper->tempstore[0] = number;
				keeper->tempstore[1] = stock->type;
				keeper->tempstore[2] = stock->wnum.pArea ? stock->wnum.pArea->uid : 0;
				keeper->tempstore[3] = stock->wnum.vnum;
				keeper->tempstore[4] = UMAX(chance, 0);
				free_string(keeper->tempstring);
				keeper->tempstring = &str_empty[0];
				int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CUSTOM_PRICE, stock->custom_keyword,0,0,0,0,0);
				if( ret > 0 ) return;	// Messages should be done in the script
				if( ret < 0 )
				{
					if (number > 1)
						act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
					else
						act("{R$n tells you 'You can't afford to buy that'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
					ch->reply = keeper;
					return;
				}

				// Account for the fact that the value will not change if no script is called
				//  - to activate, do altermob $(self) tempstore5 = -1
				haggled = (keeper->tempstore[4] < 0);

				// Script should specify the price string.
				if(!IS_NULLSTR(keeper->tempstring))
				{
					strncpy(pricestr, keeper->tempstring, MIL);
					pricestr[MIL] = '\0';
				}
				else
					pricestr[0] = '\0';	// No price string
			}

			if( stock->obj != NULL )
			{
				bool first = true;

				for (count = 0; count < number; count++)
				{
					t_obj = create_object(stock->obj, stock->obj->level, true);

					if (t_obj->timer > 0)
						t_obj->timer = 0;

					if( stock->duration > 0 )
					{
						// They are only here for a limited time
						t_obj->timer = stock->duration;
					}

					if( first )
					{
						if (number > 1)
						{
							sprintf(buf,"$n buys $p[%d].",number);
							act(buf,ch, NULL, NULL, t_obj, NULL, NULL, NULL,TO_ROOM);
							sprintf(buf,"You buy $p[%d]",number);

							if( pricestr[0] != '\0')
							{
								strcat(buf, " for");
								strcat(buf, pricestr);
							}
							strcat(buf, ".");

							act(buf,ch, NULL, NULL, t_obj, NULL, NULL,NULL,TO_CHAR);
						}
						else
						{
							act("$n buys $p.", ch, NULL, NULL, t_obj, NULL, NULL, NULL, TO_ROOM);
							sprintf(buf,"You buy $p");

							if( pricestr[0] != '\0')
							{
								strcat(buf, " for");
								strcat(buf, pricestr);
							}
							strcat(buf, ".");

							act(buf, ch, NULL, NULL, t_obj, NULL, NULL, NULL, TO_CHAR);
						}

					}

					obj_to_char(t_obj, ch);

					// Prepare it for selling to vendors
					if( t_obj->cost <= 0 )
						t_obj->cost = new_value;

					// Handles what happens AFTER you've bought the item.  Called for EVERY item
					p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, t_obj, NULL, TRIG_BUY, NULL,0,0,0,0,0);

				}
			}
			else if(stock->mob != NULL)
			{
				CHAR_DATA *mob;
				if( stock->type == STOCK_PET )
				{
					char arg_name[MIL];
					CHAR_DATA *pet = create_mobile(stock->mob, false);
					SET_BIT(pet->act[0], ACT_PET);
					SET_BIT(pet->affected_by[0], AFF_CHARM);
					pet->comm = COMM_NOTELL|COMM_NOCHANNELS;

					one_argument(argument, arg_name);

					if (arg_name[0] != '\0')
					{
						sprintf(buf, "%s %s", pet->name, arg_name);
						free_string(pet->name);
						pet->name = str_dup(buf);
					}

					sprintf(buf, "%sA neck tag says 'I belong to %s'.\n\r", pet->description, ch->name);
					free_string(pet->description);
					pet->description = str_dup(buf);

					char_to_room(pet, ch->in_room);
					add_follower(pet, ch,true);
					if (!add_grouped(pet, ch,true))
					{
						act("$n explodes into thin air!", pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
						char_from_room(pet);
						extract_char(pet, true);
						return;
					}

					ch->pet = pet;
					mob = pet;

					act("$n buys $N.", ch, pet, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					sprintf(buf,"You buy $N");

				}
				else if( stock->type == STOCK_MOUNT )
				{
					CHAR_DATA *mount = create_mobile(stock->mob, false);

					char_to_room(mount, ch->in_room);
					mob = mount;

					act("$n buys $N.", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					sprintf(buf,"You buy $N");
				}
				else if( stock->type == STOCK_GUARD )
				{
					CHAR_DATA *guard = create_mobile(stock->mob, false);

					char_to_room(guard, ch->in_room);
					act("$n appears to salute $N.", guard, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

					add_follower(guard, ch,true);
					if (!add_grouped(guard, ch,true))
					{
						act("$n returns to $s quarters.", guard, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
						char_from_room(guard);
						extract_char(guard, true);
						return;
					}

					mob = guard;

					act("$n hires $N.", ch, guard, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					sprintf(buf,"You hire $N");
				}
				else if( stock->type == STOCK_CREW )
				{
					CHAR_DATA *crew = create_mobile(stock->mob, false);

					list_appendlink(target_ship->crew, crew);
					crew->belongs_to_ship = target_ship;

					char_to_room(crew, target_ship->instance->entrance);

					act("{W$n boards {x$T{W.{x", crew, NULL, NULL, NULL, NULL, NULL, target_ship->ship_name, TO_ROOM);

					mob = crew;

					act("$n hires $N.", ch, crew, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
					sprintf(buf,"You hire $N");
				}
				else
				{
					// Complain
					return;
				}


				if( pricestr[0] != '\0' )
				{
					strcat(buf, " for");
					strcat(buf, pricestr);
				}
				strcat(buf, ".");

				act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

				if( stock->duration > 0 )
				{
					// They are only here for a limited time
					SET_BIT(mob->act[1], ACT2_HIRED);
					mob->hired_to = current_time + stock->duration * 60;
				}

				p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);
				p_percent_trigger(keeper, NULL, NULL, NULL, ch, mob, NULL, NULL, NULL, TRIG_BUY, NULL,0,0,0,0,0);
			}
			else if( stock->ship != NULL )
			{
				SHIP_DATA *ship = purchase_ship(ch, stock->wnum, keeper->shop);

				if( !IS_VALID(ship) )
				{
					// An error has occured!
					send_to_char("{RERROR: {WSomething has occured in purchasing the ship.  Please notify an IMP.{x\n\r", ch);
					return;
				}

				sprintf(buf, "$n buys %s %s.", get_article(stock->ship->name, false), stock->ship->name);
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

				sprintf(buf, "You buy %s %s.", get_article(stock->ship->name, false), stock->ship->name);
				act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				if( IS_NULLSTR(keeper->shop->shipyard_description) )
				{
					sprintf(buf, "You may find your %s in the nearby harbor.", stock->ship->name);
				}
				else
				{
					sprintf(buf, "You may find your %s %s.", stock->ship->name, keeper->shop->shipyard_description);
				}

				act("{C$n says to $N, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_NOTVICT);
				act("{C$n says to you, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_VICT);
				act("{CYou say to $N, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_CHAR);

				act("{xTo board your ship, use '{Yenter $T{x' until you give it a name.{x", ch, NULL, NULL, NULL, NULL, NULL, ch->name, TO_CHAR);

				p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, ship->ship, NULL, TRIG_BUY, NULL,0,0,0,0,0);
			}
			else
			{
				if (number > 1)
				{
					sprintf(buf,"$n buys $T[%d].",number);
					act(buf,ch, NULL, NULL, NULL, NULL, NULL, stock->custom_descr,TO_ROOM);
					sprintf(buf,"You buy $T[%d]",number);
				}
				else
				{
					act("$n buys $T.", ch, NULL, NULL, NULL, NULL, NULL, stock->custom_descr, TO_ROOM);
					sprintf(buf,"You buy $T");
				}

				if( pricestr[0] != '\0' )
				{
					strcat(buf, " for");
					strcat(buf, pricestr);
				}
				strcat(buf, ".");

				act(buf,ch, NULL, NULL, NULL, NULL, NULL,stock->custom_descr,TO_CHAR);

				keeper->tempstore[0] = number;						// Number of units
				keeper->tempstore[1] = UMAX(stock->duration, 0);	// Duration of product / service.
				// Just do the giving of items
				// All cost transactions have taken place, along with their messages
				p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BUY, stock->custom_keyword,0,0,0,0,0);
			}

			// Reduce the available stock when it is limited
			if( stock->quantity > 0 )
				stock->quantity -= number;

		}
	}
}


void do_blow( CHAR_DATA *ch, char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument( argument, arg );
    if ( arg[0] == '\0' )
    {
  send_to_char( "Blow what?\n\r", ch );
  return;
    }

    if ( ( obj = get_obj_carry( ch, arg, ch ) ) == NULL )
    {
  send_to_char( "You do not have that item.\n\r", ch );
  return;
    }

  if ( obj->item_type != ITEM_WHISTLE )
  {
      send_to_char( "You can't blow that.\n\r", ch );
      return;
  }

    act( "$n puts $p to $s lips and blows.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM );
    act( "You put $p to your lips and blow.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR );

	// TODO: When NPC ships can exist properly and the general airship can be made, revisit this
	/*
    if ( obj->pIndexData == obj_index_gold_whistle )
	{
		act( "The whistle glows vibrantly, then fades.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
		if ( !IN_WILDERNESS(ch) )
		{
			act( "{CA quiet voice whispers, 'You must be in the wilderness to be picked up.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
			return;
		}

		if ( plith_airship == NULL ||
		plith_airship->ship == NULL ||
		plith_airship->ship->ship == NULL ||
		plith_airship->ship->ship->in_room == NULL ||
		str_cmp(plith_airship->ship->ship->in_room->area->name, "Plith") ||
		plith_airship->captain == NULL ||
			plith_airship->captain->ship_depart_time > 0)
		{
		act( "{CA quiet voice whispers, 'The airship is unavailable at the moment. Please try again later.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		}
		else
		{
		act( "{CA quiet voice whispers, 'The airship is on it's way.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		plith_airship->captain->ship_depart_time = 80;
		plith_airship->captain->ship_dest_x = ch->in_room->x;
		plith_airship->captain->ship_dest_y = ch->in_room->y;
		}
		return;
	}
	*/

    p_percent_trigger( NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BLOW , NULL,0,0,0,0,0);

    return;
}

void do_list(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];

	{
		CHAR_DATA *keeper;
		OBJ_DATA *obj;
		int cost,count;
		bool found;
		char arg[MAX_INPUT_LENGTH];

		if ((keeper = find_keeper(ch)) == NULL)
		    return;

		check_mob_factions(ch, keeper);

        one_argument(argument,arg);

		found = false;
		SHOP_STOCK_DATA *stock;
		for (stock = keeper->shop->stock; stock; stock = stock->next)
		{
			// Hide it if it's out of stock
			if( stock->max_quantity > 0 && stock->quantity < 1) continue;

			switch(stock->type)
			{
			case STOCK_OBJECT:
				if( stock->wnum.pArea && stock->wnum.vnum > 0 && stock->obj != NULL )
				{
					if( arg[0] != '\0' && !is_name(arg, stock->obj->name) )
						continue;

					if ( !can_see_stock_reputation(ch, stock))
						continue;

					bool has_rep = has_stock_reputation(ch, stock);

					if (!found)
					{
						found = true;
						send_to_char("{B[ {GLv       Price     Qty{B ]{x {YItem{x\n\r", ch);
					}

					int level = stock->level;
					if( level < 1 ) level = stock->obj->level;
					level = UMAX(level, 1);

					char *pricing = get_shop_stock_price(stock);
					int pwidth = get_colour_width(pricing) + 14;

					char *descr =
						IS_NULLSTR(stock->custom_descr) ? stock->obj->short_descr : stock->custom_descr;

					char repName[MIL];
					repName[0] = '\0';
					if (!has_rep && IS_VALID(stock->reputation))
					{
						REPUTATION_INDEX_RANK_DATA *stockMinRank, *stockMaxRank;

						if (stock->min_reputation_rank > 0 && stock->min_reputation_rank <= list_size(stock->reputation->ranks))
							stockMinRank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(stock->reputation->ranks, stock->min_reputation_rank);
						else
							stockMinRank = NULL;

						if (stock->max_reputation_rank > 0 && stock->max_reputation_rank <= list_size(stock->reputation->ranks))
							stockMaxRank = (REPUTATION_INDEX_RANK_DATA *)list_nthdata(stock->reputation->ranks, stock->max_reputation_rank);
						else
							stockMaxRank = NULL;

						if (IS_VALID(stockMinRank))
						{
							if (IS_VALID(stockMaxRank))
							{
								sprintf(repName, " {R(requires %s to %s in %s){x",
									stockMinRank->name,
									stockMaxRank->name,
									stock->reputation->name);
							}
							else
							{
								sprintf(repName, " {R(requires %s in %s){x",
									stockMinRank->name,
									stock->reputation->name);
							}
						}
						else if(stockMaxRank)
						{
							sprintf(repName, " {R(available upto %s in %s){x",
								stockMaxRank->name,
								stock->reputation->name);
						}
					}

					if( stock->max_quantity > 0 )
					{
						sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s%s\n\r", level,pwidth,pricing,stock->quantity,descr, repName);
					}
					else
					{
						sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s%s\n\r", level,pwidth,pricing,descr, repName);
					}

					send_to_char(buf, ch);
				}
				break;

			case STOCK_PET:
			case STOCK_MOUNT:
			case STOCK_GUARD:
			case STOCK_CREW:
				if( stock->wnum.pArea && stock->wnum.vnum > 0 && stock->mob != NULL )
				{
					if( arg[0] != '\0' && !is_name(arg, stock->mob->player_name) )
						continue;

					if (!found)
					{
						found = true;
						send_to_char("{B[ {GLv       Price     Qty{B ]{x {YItem{x\n\r", ch);
					}

					int level = stock->level;
					if( level < 1 ) level = stock->mob->level;
					level = UMAX(level, 1);

					char *pricing = get_shop_stock_price(stock);
					int pwidth = get_colour_width(pricing) + 14;

					char *descr =
						IS_NULLSTR(stock->custom_descr) ? stock->mob->short_descr : stock->custom_descr;

					if( stock->max_quantity > 0 )
					{
						sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s\n\r", level,pwidth,pricing,stock->quantity,descr);
					}
					else
					{
						sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s\n\r", level,pwidth,pricing,descr);
					}

					send_to_char(buf, ch);
				}
				break;

			case STOCK_SHIP:
				if( stock->wnum.pArea && stock->wnum.vnum > 0 && stock->ship != NULL )
				{
					if( arg[0] != '\0' && !is_name(arg, stock->ship->name) )
						continue;

					if (!found)
					{
						found = true;
						send_to_char("{B[ {GLv       Price     Qty{B ]{x {YItem{x\n\r", ch);
					}

					int level = stock->level;
					level = UMAX(level, 1);

					char *pricing = get_shop_stock_price(stock);
					int pwidth = get_colour_width(pricing) + 14;

					char *descr =
						IS_NULLSTR(stock->custom_descr) ? stock->ship->name : stock->custom_descr;

					if( stock->max_quantity > 0 )
					{
						sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s\n\r", level,pwidth,pricing,stock->quantity,descr);
					}
					else
					{
						sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s\n\r", level,pwidth,pricing,descr);
					}

					send_to_char(buf, ch);
				}
				break;

			default:
				if(!IS_NULLSTR(stock->custom_keyword))
				{
					if( arg[0] != '\0' && !is_name(arg, stock->custom_keyword) )
						continue;

					if (!found)
					{
						found = true;
						send_to_char("{B[ {GLv       Price     Qty{B ]{x {YItem{x\n\r", ch);
					}

					int level = UMAX(stock->level, 1);

					char *pricing = get_shop_stock_price(stock);
					int pwidth = get_colour_width(pricing) + 14;

					if( stock->max_quantity > 0 )
					{
						sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s (%s)\n\r", level,pwidth,pricing,stock->quantity,stock->custom_descr, stock->custom_keyword);
					}
					else
					{
						sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s (%s)\n\r", level,pwidth,pricing,stock->custom_descr, stock->custom_keyword);
					}

					send_to_char(buf, ch);
				}
				break;
			}
		}

		for (obj = keeper->carrying; obj; obj = obj->next_content)
		{
		    if (IS_OBJ_STAT(obj,ITEM_INVENTORY) &&
				obj->wear_loc == WEAR_NONE &&
		    	can_see_obj(ch, obj) &&
		    	(cost = get_cost(keeper, obj, true)) > 0 &&
		    	(arg[0] == '\0' || is_name(arg,obj->name)))
	    	{
				if (!found)
				{
					found = true;
					send_to_char("{B[ {GLv       Price     Qty{B ]{x {YItem{x\n\r", ch);
				}

				count = 1;

				while (obj->next_content != NULL &&
						IS_OBJ_STAT(obj->next_content,ITEM_INVENTORY) &&
						obj->pIndexData == obj->next_content->pIndexData &&
						!str_cmp(obj->short_descr, obj->next_content->short_descr))
				{
					obj = obj->next_content;
					count++;
				}

				sprintf(buf,"{B[{x%3d %14d {Y%4d{B ]{x %s\n\r", obj->level,cost,count,obj->short_descr);

				send_to_char(buf, ch);
		    }
		}

		if (!found)
			send_to_char("You can't buy anything here.\n\r", ch);
		return;
    }
}


void do_inspect(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *keeper;
	SHOP_REQUEST_DATA request;

    one_argument(argument, arg);

    if ((keeper = find_keeper(ch)) == NULL)
		return;

	check_mob_factions(ch, keeper);

	bool found = get_stock_keeper(ch, keeper, &request, arg);

	if(!found)
	{
		act("{R$n tells you 'I don't sell that product. Maybe there is something else you would like to inspect?'{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		ch->reply = keeper;
		return;
	}

	if( request.obj != NULL )
	{
		if( IS_SET(request.obj->extra[1], ITEM_NO_LORE) )
		{
			act("{R$N tells you 'Sorry, I do not have any information about $p.'{x", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR);
			ch->reply = keeper;
			return;
		}

		act("You ask $N for some information about $p.", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR);
		act("$n asks $N for some information about $p.", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_ROOM);
		spell_identify(&gsk__inspect, ch->tot_level, ch, request.obj, TARGET_OBJ, WEAR_NONE);
	}
	else if( request.stock != NULL )
	{
		if( request.stock->obj != NULL )
		{
		    OBJ_DATA *obj = create_object(request.stock->obj, 0, true);

			if( IS_SET(obj->extra[1], ITEM_NO_LORE) )
			{
				act("{R$N tells you 'Sorry, I do not have any information about $p.'{x", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR);
				ch->reply = keeper;
			}
			else
			{
				act("You ask $N for some information about $p.", ch, keeper, NULL, obj, NULL, NULL, NULL, TO_CHAR);
				act("$n asks $N for some information about $p.", ch, keeper, NULL, obj, NULL, NULL, NULL, TO_ROOM);
				obj_to_char(obj, ch);
				spell_identify(&gsk__inspect, ch->tot_level, ch, obj, TARGET_OBJ, WEAR_NONE);
			}
			extract_obj(obj);
			return;
		}
		else if( request.stock->mob != NULL )
		{
			CHAR_DATA *mob = create_mobile(request.stock->mob, false);
			char_to_room(mob, ch->in_room);

			if( IS_SET(mob->act[0], ACT_NO_LORE) )
			{
				act("{R$n tells you 'Sorry, I do not have any information about $N.'{x", keeper, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD);
				ch->reply = keeper;
			}
			else
			{
				act("You ask $N for some information about $v.", ch, keeper, mob, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$n asks $N for some information about $v.", ch, keeper, mob, NULL, NULL, NULL, NULL, TO_ROOM);

				if( request.stock->type == STOCK_PET )
				{
					act("{GPET{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				}
				else if( request.stock->type == STOCK_PET )
				{
					act("{GMOUNT{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				}
				else
				{
					act("{GGUARD{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				}

				show_basic_mob_lore(ch, mob);
			}

			extract_char(mob, true);
		}
		else if( !IS_NULLSTR(request.stock->custom_keyword) )
		{
	    	if(!p_exact_trigger(request.stock->custom_keyword, keeper, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_INSPECT_CUSTOM,0,0,0,0,0))
	    	{
				act("{R$N tells you 'Sorry, I do not have any information about $T.'{x", ch, keeper, NULL, NULL, NULL, NULL, request.stock->custom_descr, TO_CHAR);
				ch->reply = keeper;
				return;
			}
		}
		else
		{
			act("{R$n tells you 'I don't sell that product. Maybe there is something else you would like to inspect?'{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
			ch->reply = keeper;
			return;
		}
	}
}


void do_sell(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	CHAR_DATA *keeper = NULL;
	OBJ_DATA *obj = NULL;
	int cost,roll;

	one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Sell what?\n\r", ch);
		return;
	}

	if ((keeper = find_keeper(ch)) == NULL)
	{
		send_to_char("You can't do that here.\n\r", ch);
		return;
	}

	check_mob_factions(ch, keeper);

	if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
	{
		act("{R$n tells you 'You don't have that item'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		ch->reply = keeper;
		return;
	}

	if(p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRESELL, NULL,0,0,0,0,0))
		return;

	if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
	{
		send_to_char("You can't let go of it.\n\r", ch);
		return;
	}

	if (!can_see_obj(keeper,obj))
	{
		act("$n doesn't see what you are offering.",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
		return;
	}

	if( keeper->shop->stock != NULL )
	{
		// Check the keeper's stock for hits
		SHOP_STOCK_DATA *stock;
		for(stock = keeper->shop->stock; stock; stock = stock->next)
		{
			if(stock->obj != NULL && stock->obj == obj->pIndexData)
				break;
		}

		if( stock != NULL )
		{
			if( IS_NULLSTR(stock->custom_price) )
			{
				bool haggled = false;
				int chance = get_skill(ch, gsk_haggle);

				long silver = adjust_keeper_price(keeper, stock->silver, false);
				if( silver > 0 )
				{
					long wealth = (keeper-> silver + 100 * keeper->gold);
					if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
					{
						roll = number_percent();
						if (roll < chance)
						{
							haggled = true;
							silver += stock->silver * roll / 200;
							silver = UMIN(silver,95 * stock->silver / 100);
							silver = UMIN(silver,wealth);
						}
					}

					if (silver > wealth)
					{
						act("{R$n tells you 'I'm afraid I don't have enough wealth to buy $p.{x",
							keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT);
						ch->reply = keeper;
						return;
					}
				}


				// Add some way to limit these?
				long qp = adjust_keeper_price(keeper, stock->qp, false);
				if( qp > 0 )
				{
					if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
					{
						roll = number_percent();
						if (roll < chance)
						{
							haggled = true;
							qp += stock->qp * roll / 200;
							qp = UMIN(qp,95 * stock->qp / 100);
						}
					}
				}

				long dp = adjust_keeper_price(keeper, stock->dp, false);
				if( dp > 0 )
				{
					if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
					{
						roll = number_percent();
						if (roll < chance)
						{
							haggled = true;
							dp += stock->dp * roll / 200;
							dp = UMIN(dp,95 * stock->dp / 100);
						}
					}
				}

				long pneuma = adjust_keeper_price(keeper, stock->pneuma, false);
				if( pneuma > 0 )
				{
					if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
					{
						roll = number_percent();
						if (roll < chance)
						{
							haggled = true;
							pneuma += stock->pneuma * roll / 200;
							pneuma = UMIN(pneuma,95 * stock->pneuma / 100);
						}
					}
				}

				act("$n sells $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

				if( haggled ) {
					send_to_char("You haggle with the shopkeeper.\n\r",ch);
					check_improve(ch,gsk_haggle,true,4);
				}

				sprintf(buf, "You sell $p for%s.", get_shop_purchase_price(silver, qp, dp, pneuma));
				act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

				ch->gold		+= silver/100;
				ch->silver		+= silver%100;
				ch->missionpoints	+= qp;
				ch->deitypoints	+= dp;
				ch->pneuma		+= pneuma;

				deduct_cost(keeper,silver);
				if (keeper->gold < 0)
					keeper->gold = 0;
				if (keeper->silver< 0)
					keeper->silver = 0;

				if (obj->item_type == ITEM_TRASH)
				{
					log_string("Item sell extract");
					extract_obj(obj);
				}
				else
				{
					obj_from_char(obj);
					obj->timer = number_range(100,250);
					obj_to_keeper(obj, keeper);
				}
				return;
			}

			// Custom prices are not eligible for refund
		}

		if (IS_SET(keeper->shop->flags, SHOPFLAG_STOCK_ONLY))
		{
			act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT);
			return;
		}
	}

	// Not a part of the stock, or not eligible for stock refund
    if ((cost = get_cost(keeper, obj, false)) <= 0)
    {
		act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT);
		return;
    }
    if (cost > (keeper-> silver + 100 * keeper->gold))
    {
		act("{R$n tells you 'I'm afraid I don't have enough wealth to buy $p.{x",
			keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT);
		ch->reply = keeper;
		return;
    }

    act("$n sells $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
	{
		/* haggle */
		roll = number_percent();
		if (roll < get_skill(ch, gsk_haggle))
		{
			send_to_char("You haggle with the shopkeeper.\n\r",ch);
			cost += obj->cost / 2 * roll / 100;
			cost = UMIN(cost,95 * get_cost(keeper,obj,true) / 100);
			cost = UMIN(cost,(keeper->silver + 100 * keeper->gold));
			check_improve(ch,gsk_haggle,true,4);
		}
	}
	sprintf(buf, "You sell $p for%s", get_shop_purchase_price(cost, 0, 0, 0));
	act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	ch->gold	+= cost/100;
	ch->silver	+= cost - (cost/100) * 100;
    deduct_cost(keeper,cost);
    if (keeper->gold < 0)
		keeper->gold = 0;
    if (keeper->silver< 0)
		keeper->silver = 0;

	if (obj->item_type == ITEM_TRASH)
	{
		log_string("Item sell extract");
		extract_obj(obj);
	}
	else
	{
		obj_from_char(obj);
		obj->timer = number_range(100,250);
		obj_to_keeper(obj, keeper);
	}
}


void do_value(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *keeper;
    OBJ_DATA *obj;
    int cost;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Value what?\n\r", ch);
	return;
    }

    if ((keeper = find_keeper(ch)) == NULL)
	return;

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
	act("{R$n tells you 'You don't have that item'.{x",
	    keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
	ch->reply = keeper;
	return;
    }

    if (!can_see_obj(keeper,obj))
    {
        act("$n doesn't see what you are offering.",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
        return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
	send_to_char("You can't let go of it.\n\r", ch);
	return;
    }

    if ((cost = get_cost(keeper, obj, false)) <= 0)
    {
	act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT);
	return;
    }

    sprintf(buf,
	"{R$n tells you 'I'll give you %d silver and %d gold coins for $p'.{x",
	cost - (cost/100) * 100, cost/100);
    act(buf, keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT);
    ch->reply = keeper;

    return;
}


void do_secondary(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    OBJ_DATA *weapon;
    char buf[MAX_STRING_LENGTH];

    if (argument[0] == '\0')
    {
        send_to_char ("Wear which weapon in your off-hand?\n\r",ch);
        return;
    }

    obj = get_obj_carry(ch, argument, ch);

    if (obj == NULL)
    {
        send_to_char ("You don't have that weapon.\n\r",ch);
        return;
    }

    if (!CAN_WEAR(obj, ITEM_WIELD) || !IS_WEAPON(obj->pIndexData))
    {
	send_to_char("That's not a weapon.\n\r", ch);
	return;
    }

    if (IS_WEAPON_STAT(obj, WEAPON_TWO_HANDS)
    &&  ch->size < SIZE_GIANT)
    {
	switch (ch->size)
	{
	    case SIZE_HUGE:
	        act("Strong as you are, there's still no way you could dual wield $P.", ch, NULL, NULL, NULL, obj, NULL, NULL, TO_CHAR);
		break;
	   default:
		send_to_char("That weapon is too big to dual wield.\n\r", ch);
		break;
	}

	return;
    }

    if (obj->condition == 0)
    {
	send_to_char("You can't wield that weapon. It's broken!\n\r", ch);
	return;
    }

    if ((weapon = get_eq_char(ch, WEAR_WIELD)) == NULL)
    {
	send_to_char("You must wield a primary weapon before wielding something in your off-hand.\n\r", ch);
	return;
    }

    if (both_hands_full(ch))
    {
        send_to_char("Your hands are already rather full!\n\r",ch);
        return;
    }

    if (ch->tot_level < obj->level)
    {
        sprintf(buf, "You must be level %d to use this object.\n\r",
			obj->level);
        send_to_char(buf, ch);
        act("$n tries to use $p, but is too inexperienced.",
			ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
        return;
    }

    /* you can't dual wield spears/polearms */
    if (weapon != NULL
    && (IS_WEAPON_STAT(weapon,WEAPON_TWO_HANDS)
         || weapon->value[0] == WEAPON_POLEARM
	 || weapon->value[0] == WEAPON_SPEAR))
    {
	if(obj->value[0] == WEAPON_SPEAR
	|| obj->value[0] == WEAPON_POLEARM)
	{
	    send_to_char("Your hands are tied up with your weapon!\n\r", ch);
	    return;
	}
    }

    if (IS_SET(obj->extra[1], ITEM_REMORT_ONLY)
    && !IS_REMORT(ch) && !IS_NPC(ch))
    {
	send_to_char("You cannot use this object without remorting.\n\r", ch);
	act("$n tries to use $p, but is too inexperienced.",
			ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	return;
    }


    if (!remove_obj(ch, WEAR_SECONDARY, true))
        return;

    act ("$n wields $p in $s off-hand.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
    act ("You wield $p in your off-hand.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
    equip_char (ch, obj, WEAR_SECONDARY);
}


void do_push(CHAR_DATA *ch, char *argument)
{
    char arg[MIL];
    OBJ_DATA *obj;
	char *start = argument;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
		send_to_char("What do you want to push?\n\r", ch);
		return;
    }

    if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
    {
        if ((obj = get_obj_list(ch, arg, ch->carrying)) == NULL)
		{
			send_to_char ("You can't find it.\n\r",ch);
			return;
		}
    }

    if (argument[0]) {
        if(p_use_on_trigger(ch, obj, TRIG_PUSH_ON, argument,0,0,0,0,0)) return;
    } else {
        if(p_use_trigger(ch, obj, TRIG_PUSH,0,0,0,0,0)) return;
    }

	// Ok, need determine if we can target things unambiguously
	if (obj_oclu_ambiguous(obj) && argument[0] == '\0')
	{
		act("Push what on $p?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		obj_oclu_show_parts(ch, obj);
		return;
	}

	OCLU_CONTEXT context;
	if (!oclu_get_context(&context, obj, argument))
	{
		act("You do not see that on $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (context.item_type != ITEM_PORTAL && IS_SET(*context.flags, CONT_PUSHOPEN))
	{
		if (!IS_SET(*context.flags, CONT_CLOSED))
		{
			send_to_char("It's already open.\n\r", ch);
			return;
		}
		if (!IS_SET(*context.flags, CONT_CLOSEABLE))
		{
			send_to_char("You can't do that.\n\r", ch);
			return;
		}

		if( *context.lock )
		{
			if (IS_SET((*context.lock)->flags, LOCK_LOCKED))
			{
				OBJ_DATA *key;
				if ((key = lockstate_getkey(ch, *context.lock)) != NULL)
				{
					do_function(ch, &do_unlock, start);
					do_function(ch, &do_push, start);
					return;
				}

				send_to_char("It's locked.\n\r", ch);
				return;
			}
		}

		REMOVE_BIT((*context.flags), CONT_CLOSED);
		if (context.item_type == ITEM_BOOK)
		{
			// Special handling for books.
			if (BOOK(obj)->open_page > 0)
			{
				BOOK(obj)->current_page = BOOK(obj)->open_page;
			}
			else if (BOOK(obj)->current_page < 1)
				BOOK(obj)->current_page = 1;

			char page[MIL];
			sprintf(page, "to page %d", BOOK(obj)->current_page);

			if (context.is_default)
			{
				act("You open $p $T.",ch, NULL, NULL,obj, NULL, NULL, page,TO_CHAR);
				act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				act("You open $t on $p $T.",ch, NULL, NULL,obj, NULL, context.label, page,TO_CHAR);
				act("$n opens $t on $p.", ch, NULL, NULL, obj, NULL, context.label, NULL, TO_ROOM);
			}
		}
		else
		{
			if (context.is_default)
			{
				act("You open $p.",ch, NULL, NULL,obj, NULL, NULL, NULL,TO_CHAR);
				act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			}
			else
			{
				act("You open $t on $p.",ch, NULL, NULL,obj, NULL, context.label, NULL,TO_CHAR);
				act("$n opens $t on $p.", ch, NULL, NULL, obj, NULL, context.label, NULL, TO_ROOM);
			}
		}

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_OPEN, NULL,context.which,0,0,0,0);
		return;
	}

    send_to_char("You can't push that.\n\r", ch);
}


void do_pull(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("What do you want to pull?\n\r", ch);
	return;
    }

    if (IS_DEAD(ch)) {
	send_to_char("You can't pull anything while dead.\n\r", ch);
	return;
    }

	if((mob = get_char_room(ch, NULL, arg)) == NULL) {
		if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL) {
			if ((obj = get_obj_list(ch, arg, ch->carrying)) == NULL) {
				send_to_char ("You can't find it.\n\r",ch);
				return;
			}
		}
	}

    /* @@@NIB : 20070121 : Added for the new trigger type*/
    /*	Also allows for PULL/PULL_ON scripts to drop to the CART code*/
    if (argument[0]) {
	    if(p_act_trigger(argument, mob, obj, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PULL_ON,0,0,0,0,0)) return;
    } else {
	    if(p_percent_trigger(mob, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PULL, NULL,0,0,0,0,0)) return;
    }

	if(obj) {
    if (obj->carried_by == ch &&
	    (obj->wear_loc == WEAR_LODGED_HEAD ||
		obj->wear_loc == WEAR_LODGED_TORSO ||
		obj->wear_loc == WEAR_LODGED_ARM_L ||
		obj->wear_loc == WEAR_LODGED_ARM_R ||
		obj->wear_loc == WEAR_LODGED_LEG_L ||
		obj->wear_loc == WEAR_LODGED_LEG_R)) {
	if(IS_SET(obj->extra[0], ITEM_NOREMOVE))
		act("You can't dislodge $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	else if(!unequip_char(ch,obj,true)) {
		act("$n dislodges $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		act("You dislodge $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		if(IS_WEAPON_STAT(obj,WEAPON_BARBED)) {
			act("{R$p{R tears away flesh from $n.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
			act("{R$p{R tears away some of your flesh.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			damage(ch, ch, UMIN(ch->hit,25), NULL, TYPE_UNDEFINED, DAM_PIERCE, true);
		}
	}
	return;
    }

    if (obj->item_type == ITEM_CART)
    {
	if (obj->carried_by != NULL) {
	    send_to_char("You'll have to drop it first.\n\r", ch);
	    return;
	}

        /* make sure someone isn't pulling it!*/
	if (obj->pulled_by) {
		if (obj->pulled_by != ch)
			act("$N appears to be pulling $p at the moment.",
				ch, obj->pulled_by, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		else
			act("You're already pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

		return;
	}

        if (IS_AFFECTED(ch, AFF_SNEAK))
	{
	    send_to_char("You stop moving silently.\n\r", ch);
	    affect_strip(ch, gsk_sneak);
	}

	if (ch->pulled_cart != NULL)
	{
	    act("But you're already pulling $p!",
	    	ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	if (obj->value[2] > get_curr_stat(ch, STAT_STR) && !MOUNTED(ch))
	{
  	    act("You aren't strong enough to pull $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    act("$n attempts to pull $p but is too weak.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	    return;
	}
	else
	{
	    if (MOUNTED(ch))
	    {
	        act("You hitch $p onto $N.", ch, MOUNTED(ch), NULL, obj, NULL, NULL, NULL, TO_CHAR);
	        act("$n hitches $p onto $N.", ch, MOUNTED(ch), NULL, obj, NULL, NULL, NULL, TO_ROOM);
	    }
	    else
	    {
	        act("You start pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL,TO_CHAR);
	        act("$n starts pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL,TO_ROOM);
	    }
	}

        if (is_relic(obj->pIndexData))
	{
	    if (ch->church == NULL)
	    {
	        act("{YA huge arc of lightning leaps out from $p striking you!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	        act("{YA huge arc of lightning leaps out from $p striking $n!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	        damage(ch, ch, 30000, NULL, TYPE_UNDEFINED, DAM_LIGHTNING, false);
		return;
	    }
	    else
			church_announce_theft(ch, obj);
	}

	ch->pulled_cart = obj;
	obj->pulled_by = ch;
	return;
    }
}
    send_to_char("You can't pull that.\n\r", ch);
}


/*
 * Used for not only the turning of objects but also the turning of undead.
 */
void do_turn(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	CHAR_DATA *vch;
	int skill;

	argument = one_argument(argument, arg);

	if (arg[0]) {
		send_to_char("Turn what?\n\r", ch);
		return;
	}

	/* First look for a character to turn */
	if ((vch = get_char_room(ch, NULL, arg)) && (skill = get_skill(ch, gsk_turn_undead)) > 0) {
		int chance;

		if(p_percent_trigger(vch,NULL, NULL, NULL, ch, vch, NULL, NULL, NULL, TRIG_ATTACK_TURN,"pretest",0,0,0,0,0) ||
			p_percent_trigger(ch,NULL, NULL, NULL, ch, vch, NULL, NULL, NULL, TRIG_ATTACK_TURN,"pretest",0,0,0,0,0))
			return;

		act("{YYou release your divine will over $N!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
		act("{Y$n attempts to turn $N!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

		act("{WYou feel a powerful divine presence pass through you!{x",ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);

		WAIT_STATE(ch, gsk_turn_undead->beats);

		if (IS_UNDEAD(vch)) {
			chance = (ch->tot_level - vch->tot_level) + skill / 5;
			if (number_percent() < chance) {
				act("{RYou scream with pain as your flesh sizzles and melts!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("{R$n screams with pain as $s flesh sizzles and melts!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
				damage(ch, vch, dice(ch->tot_level, 8), NULL, TYPE_UNDEFINED, DAM_HOLY, false);
				do_function(vch, &do_flee, NULL);
				PANIC_STATE(vch, 12);
				DAZE_STATE(vch, 12);
			} else {
				act("You wince, but resist $n's divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				act("$N winces, but resists your divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
				act("$N winces, but resists $n's divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
			}
		} else {
			act("$N is unaffected.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			act("You are unaffected.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
		}

		return;
	}

	if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
	{
	if ((obj = get_obj_list(ch, arg, ch->carrying)) == NULL)
	{
	send_to_char ("You can't find it.\n\r",ch);
	return;
	}
	}

	/* @@@NIB : 20070121 : Added for the new trigger type*/
	if (argument[0]) {
	if(p_use_on_trigger(ch, obj, TRIG_TURN_ON, argument,0,0,0,0,0)) return;
	} else {
	if(p_use_trigger(ch, obj, TRIG_TURN,0,0,0,0,0)) return;
	}

	send_to_char("You can't turn that.\n\r", ch);
}


void do_skull(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *skull;
    int i;
    int chance, corpse;

    argument = one_argument(argument, arg);

    if ((IS_NPC(ch) && ch->alignment >= 0) || (!IS_NPC(ch) && get_skill(ch, gsk_skull) == 0))
    {
	send_to_char("Why would you want to do such a thing?\n\r",ch);
	return;
    }

    if (is_dead(ch))
	return;

    if (arg[0] == '\0')
    {
	send_to_char("Take the skull from what?\n\r", ch);
	return;
    }

    if ((obj = get_obj_here(ch, NULL, arg)) != NULL)
    {
	if (obj->item_type != ITEM_CORPSE_PC)
	{
	    send_to_char("You can only take the skull from a player's corpse.\n\r", ch);
	    return;
	}

	if (!IS_SET(CORPSE_PARTS(obj),PART_HEAD)) {
	    send_to_char("There is no skull to take.\n\r", ch);
	    return;
	}

	if (obj->level >= LEVEL_IMMORTAL) {
	    act("$p is protected by powers from above.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    return;
	}

	corpse = CORPSE_TYPE(obj);

	/* Used for corpse types that are impossible to skull even if there is a head...*/
	if (corpse_info_table[corpse].skulling_chance < 0) {
		send_to_char("You can't seem to remove its skull.  It doesn't want to budge.\n\r", ch);
		return;
	}

	if (IS_NPC(ch))
	    chance = (ch->tot_level * 3)/4 - obj->level/10;
	else
	    chance = get_skill(ch, gsk_skull) - 3;

	chance *= corpse_info_table[corpse].skulling_chance;

	if (number_range(1,10000) > chance)
	{
	    act(corpse_info_table[corpse].skull_fail, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    act(corpse_info_table[corpse].skull_fail_other, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	    check_improve(ch, gsk_skull, false, 1);
//	    SET_BIT(obj->extra[0], ITEM_NOSKULL);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_HEAD);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_BRAINS);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_EAR);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_EYE);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_LONG_TONGUE);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_EYESTALKS);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_FANGS);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_HORNS);
	    REMOVE_BIT(CORPSE_PARTS(obj),PART_TUSKS);

	    sprintf(buf, corpse_info_table[corpse].short_headless, obj->owner);
	    free_string(obj->short_descr);
	    obj->short_descr = str_dup(buf);

	    sprintf(buf, corpse_info_table[corpse].long_headless, obj->owner);
	    free_string(obj->description);
	    obj->description = str_dup(buf);

	    sprintf(buf, corpse_info_table[corpse].full_headless, obj->owner);
	    free_string(obj->full_description);
	    obj->full_description = str_dup(buf);
	    return;
	}


	/* 20070521 : NIB : Changed to check based upon where the CORPSE was created,*/
	/*			for when corpses can be dragged.  Used to keep people*/
	/*			from using CPK rooms to skull goldens.  This will have*/
	/*			no affect on looting as object placement is done at the*/
	/*			time of death.*/
	if (IS_SET(CORPSE_FLAGS(obj), CORPSE_CHAOTICDEATH))
	    skull = create_object(obj_index_gold_skull, 0, false);
	else
	    skull = create_object(obj_index_skull, 0, false);

//	SET_BIT(obj->extra[0], ITEM_NOSKULL);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_HEAD);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_BRAINS);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_EAR);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_EYE);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_LONG_TONGUE);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_EYESTALKS);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_FANGS);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_HORNS);
	REMOVE_BIT(CORPSE_PARTS(obj),PART_TUSKS);

	sprintf(buf, skull->short_descr, obj->owner);
	free_string(skull->short_descr);
	skull->short_descr = str_dup(buf);

	sprintf(buf, skull->description, obj->owner);
	free_string(skull->description);
	skull->description = str_dup(buf);

	sprintf(buf, skull->full_description, obj->owner);
	free_string(skull->full_description);
	skull->full_description = str_dup(buf);

	sprintf(buf, "skull %s", obj->owner);
	for (i = 0; buf[i] != '\0'; i++)
	    buf[i] = LOWER(buf[i]);

	free_string(skull->name);
	skull->name = str_dup(buf);

	skull->owner = str_dup(obj->owner);

	sprintf(buf, corpse_info_table[corpse].short_headless, obj->owner);
	free_string(obj->short_descr);
	obj->short_descr = str_dup(buf);

	sprintf(buf, corpse_info_table[corpse].long_headless, obj->owner);
	free_string(obj->description);
	obj->description = str_dup(buf);

	sprintf(buf, corpse_info_table[corpse].full_headless, obj->owner);
	free_string(obj->full_description);
	obj->full_description = str_dup(buf);

	sprintf(buf, corpse_info_table[corpse].skull_success, obj->owner);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	sprintf(buf, corpse_info_table[corpse].skull_success_other, obj->owner);
	act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	obj_to_char(skull, ch);
	check_improve(ch, gsk_skull, true, 1);
    }
    else
        act("There's no $t here.", ch, NULL, NULL, NULL, NULL, arg, NULL, TO_CHAR);
}

bool can_brew_spell(CHAR_DATA *ch, OBJ_DATA *obj, SKILL_ENTRY *spell)
{
	if (!spell || !spell->isspell)
	{
		send_to_char("You don't know any spells by that name.\n\r", ch);
		return false;
	}

	if (!IS_SET(spell->skill->flags, SKILL_CAN_BREW))
	{
		act_new("You cannot brew $t in $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
		return false;
	}

	if (IS_VALID(spell->token))
	{
		SCRIPT_DATA *script = get_script_token(spell->token->pIndexData, TRIG_TOKEN_QUAFF, TRIGSLOT_SPELL);
		if(!script) {
			act_new("You cannot brew $t in $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		int ret = p_percent_trigger(NULL, NULL, NULL, spell->token, ch, NULL, NULL, obj, NULL, TRIG_TOKEN_PREBREW, NULL,0,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				act_new("You cannot brew $t in $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			}
			return false;
		}
	}
	else
	{
		if(!spell->skill->quaff_fun)
		{
			act_new("You cannot brew $t in $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		if(spell->skill->prebrew_fun)
		{
			// prebrew function is responsible for messages.
			if (!(*spell->skill->prebrew_fun)(spell->skill, ch->tot_level, ch, obj))
				return false;
		}
	}

	return true;
}


// brew <spell> <container>
void do_brew(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    //int sn;
	//TOKEN_DATA *token;
    int chance;
    int mana;
    char arg[MAX_STRING_LENGTH];
	char buf[MSL];

    argument = one_argument(argument, arg);

    if (IS_DEAD(ch))
    {
		send_to_char("You are can't do that. You are dead.\n\r", ch);
		return;
    }

    if ((chance = get_skill(ch, gsk_brew)) == 0)
    {
		send_to_char("Brew? What's that?\n\r",ch);
		return;
    }

	// Need a finite fluid container with no spells.
	if ((obj = get_obj_carry(ch, argument, ch)) == NULL || !IS_FLUID_CON(obj) || FLUID_CON(obj)->capacity <= 0 || list_size(FLUID_CON(obj)->spells) > 0)
	{
		send_to_char("You do not have a valid fluid container to brew in.\n\r", ch);
		return;
	}

	if (FLUID_CON(obj)->amount < FLUID_CON(obj)->capacity)
	{
		act("Please fill up $p before brewing.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

    if (arg[0] == '\0')
    {
		send_to_char("What potion do you want to create?\n\r", ch);
		return;
    }

	LIQUID *liquid = FLUID_CON(obj)->liquid;
	if (!IS_VALID(liquid))
	{
		act("$p requires a liquid medium to brew.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// WXYZ
	// Clear out catalyst usages
	memset(ch->catalyst_usage, 0, sizeof(ch->catalyst_usage));
	SKILL_ENTRY *spell = skill_entry_findname(ch->sorted_skills, arg);
	if(!can_brew_spell(ch, obj, spell))
		return;

	if (spell->skill->brew_mana > liquid->max_mana)
	{
		act("$t powerful enough to brew $T.", ch, NULL, NULL, obj, NULL, liquid->name, spell->skill->name, TO_CHAR);
		return;
	}

	// Check the catalysts
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

	mana = 2 * skill_entry_mana(ch, spell) / 3;

    if (ch->mana < mana)
    {
		send_to_char("You don't have enough mana to brew that potion.\n\r", ch);
		return;
    }

    ch->mana -= mana;

    /* Mass healing must not be one of the spells*/
	if (spell->skill == gsk_mass_healing)
    {
		send_to_char("You may not brew that potion.\n\r", ch);
		return;
    }

	int target = skill_entry_target(ch, spell);
	if (target != TAR_CHAR_DEFENSIVE &&
		target != TAR_CHAR_SELF &&
		target != TAR_OBJ_CHAR_DEF &&
		target != TAR_CHAR_OFFENSIVE &&
		target != TAR_OBJ_CHAR_OFF)
    {
		send_to_char("You may only brew potions of spells which you can cast on people.\n\r", ch);
		return;
    }

    act("{Y$n begins to brew a potion...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{YYou begin to brew a potion...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	ch->brew_info = spell;
	ch->brew_obj = obj;

    BREW_STATE(ch, 12);
}


void brew_end(CHAR_DATA *ch )
{
    char buf[2*MAX_STRING_LENGTH];
    OBJ_DATA *potion;
    int chance;
    char potion_name[MAX_STRING_LENGTH];
    SPELL_DATA *spell;

    chance = 2 * get_skill(ch, gsk_brew) /3 +
			(get_curr_stat(ch, STAT_CON) / 4) +
			skill_entry_rating(ch, ch->brew_info) - 10;

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = (chance * 3)/2;

    chance = URANGE(1, chance, 98);

	// TODO: Add Inspiration system that guarantees artificing
    if (IS_IMMORTAL(ch))
	{
		chance = 100;
	}

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
		act("{Y$n's attempt to brew a potion fails miserably.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		act("{YYou fail to contain the magic within $p.{x", ch, NULL, NULL, ch->brew_obj, NULL, NULL, NULL, TO_CHAR);
		check_improve(ch, gsk_brew, false, 2);
		return;
    }

	// Use all the catalysts
	for(int i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
			use_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM,ch->catalyst_usage[i],true);
	}

	// Post processing after successful brewing
	if (IS_VALID(ch->brew_info->token))
	{
		p_percent_trigger(NULL, NULL, NULL, ch->brew_info->token, ch, NULL, NULL, ch->brew_obj, NULL, TRIG_TOKEN_BREW, NULL,0,0,0,0,0);
	}
	else if(ch->brew_info->skill->brew_fun)
	{
		(*ch->brew_info->skill->brew_fun)(ch->brew_info->skill, ch->tot_level, ch, ch->brew_obj);
	}

    sprintf(potion_name, "%s", skill_entry_name(ch->brew_info));

    sprintf(buf, "You brew a potion of %s in $p.", potion_name);
    act(buf, ch, NULL, NULL, ch->brew_obj, NULL, NULL, NULL, TO_CHAR);
    sprintf(buf, "$n brews a potion of %s in $p.", potion_name);
    act(buf, ch, NULL, NULL, ch->brew_obj, NULL, NULL, NULL, TO_ROOM);

    check_improve(ch, gsk_brew, true, 2);

	potion = ch->brew_obj;

	// Only update the strings on the potion if it's the "empty vial" index
	if (potion->pIndexData == obj_index_empty_vial && obj_index_potion != NULL)
	{
	    sprintf(buf, obj_index_potion->short_descr, potion_name);

		free_string(potion->short_descr);
		potion->short_descr = str_dup(buf);

		sprintf(buf, obj_index_potion->description, potion_name);

		free_string(potion->description);
		potion->description = str_dup(buf);

		free_string(potion->full_description);
		potion->full_description = str_dup(buf);

		free_string(potion->name);
		strcat(potion_name, " potion");
		potion->name = short_to_name(potion_name);
	}

    spell = new_spell();
    spell->skill = ch->brew_info->skill;
	//spell->token = IS_VALID(ch->brew_info->token) ? ch->brew_info->token->pIndexData : NULL;
    spell->level = ch->tot_level;
    //spell->next = potion->spells;
	list_appendlink(FLUID_CON(potion)->spells, spell);

	LIQUID *liq = liquid_potion ? liquid_potion : liquid_water;

	FLUID_CON(potion)->liquid = liq;

	// TODO: Need to convert this to some kind of trait?
	int uses = 1;
    if (ch->pcdata->current_class->clazz == gcl_alchemist)
    {
		if (get_skill(ch, gsk_brew) < 75)
			uses = 2;
		else if (get_skill(ch, gsk_brew) < 85)
	    	uses = 3;
		else
		    uses = 4;
    }
	FLUID_CON(potion)->amount = FLUID_CON(potion)->capacity = LIQ_SERVING * uses;
}

static inline bool is_imbueable_jewelry(OBJ_DATA *obj)
{
	return IS_JEWELRY(obj) && JEWELRY(obj)->max_mana > 0 && list_size(JEWELRY(obj)->spells) < 1;
}

static inline bool is_imbueable_wand(OBJ_DATA *obj)
{
	return IS_WAND(obj) && WAND(obj)->max_mana > 0 && list_size(WAND(obj)->spells) < 1;
}

static inline bool is_imbueable_weapon(OBJ_DATA *obj)
{
	return IS_WEAPON(obj) && WEAPON(obj)->max_mana > 0 && list_size(WEAPON(obj)->spells) < 1;
}

int get_imbue_target(OBJ_DATA *obj)
{
	if (is_imbueable_jewelry(obj))
	{
		if (is_imbueable_wand(obj)) return IMBUE_UNKNOWN;
		if (is_imbueable_weapon(obj)) return IMBUE_UNKNOWN;

		return IMBUE_JEWELRY;
	}

	if (is_imbueable_wand(obj))
	{
		if (is_imbueable_weapon(obj)) return IMBUE_UNKNOWN;

		return IMBUE_WAND;
	}

	if (is_imbueable_weapon(obj))
	{
		return IMBUE_WEAPON;
	}

	return IMBUE_NONE;
}

bool can_imbue_spell(CHAR_DATA *ch, OBJ_DATA *obj, SKILL_ENTRY *spell, int imbue_type)
{
	if (!spell || !spell->isspell)
	{
		send_to_char("You don't know any spells by that name.\n\r", ch);
		return false;
	}

	if (!IS_SET(spell->skill->flags, SKILL_CAN_IMBUE))
	{
		act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
		return false;
	}

	if (IS_VALID(spell->token))
	{
		int trigger;
		switch(imbue_type)
		{
			case IMBUE_JEWELRY:	trigger = TRIG_TOKEN_EQUIP; break;
			case IMBUE_WAND:	trigger = TRIG_TOKEN_ZAP; break;
			case IMBUE_WEAPON:	trigger = TRIG_TOKEN_BRANDISH; break;
			default:
				act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
				return false;
		}

		SCRIPT_DATA *script = get_script_token(spell->token->pIndexData, trigger, TRIGSLOT_SPELL);
		if(!script) {
			act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		int ret = p_percent_trigger(NULL, NULL, NULL, spell->token, ch, NULL, NULL, obj, NULL, TRIG_TOKEN_PREIMBUE, NULL,0,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			}
			return false;
		}
	}
	else
	{
		switch(imbue_type)
		{
			case IMBUE_JEWELRY:
				if(!spell->skill->equip_fun)
				{
					act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
					return false;
				}
				break;

			case IMBUE_WAND:
				if(!spell->skill->zap_fun)
				{
					act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
					return false;
				}
				break;

			case IMBUE_WEAPON:
				if(!spell->skill->brandish_fun)
				{
					act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
					return false;
				}
				break;

			default:
				act_new("You cannot imbue $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
				return false;
		}

		if(spell->skill->preimbue_fun)
		{
			// preimbue function is responsible for messages.
			if (!(*spell->skill->preimbue_fun)(spell->skill, ch->tot_level, ch, obj, imbue_type))
				return false;
		}
	}

	return true;
}


// imbue <object>[ <context>] <spell1>[ <spell2>[ <spell3>]]
void do_imbue(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
	SKILL_ENTRY *spells[3];
    int mana;
    int chance;
    //int kill;
    char arg[MIL];
	char buf[MSL];

    if (IS_DEAD(ch))
    {
		send_to_char("You can't do that. You are dead.\n\r", ch);
		return;
    }

    if ((chance = get_skill(ch, gsk_imbue)) == 0)
    {
		send_to_char("Imbue? What's that?\n\r",ch);
		return;
    }

	argument = one_argument(argument, arg);
	if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
	{
		send_to_char("You do not have a valid item to imbue.\n\r", ch);
		return;
	}

	int max_mana = 0;
	int imbue_type = get_imbue_target(obj);

	if (imbue_type == IMBUE_NONE)
	{
		act("You cannot imbue $p with anything.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// Has multiple options, so specify
	if (imbue_type == IMBUE_UNKNOWN)
	{
		// Need to check for the context

		argument = one_argument(argument, arg);
		if (!str_prefix(arg, "jewelry"))
		{
			if (!IS_JEWELRY(obj))
			{
				send_to_char("That is not jewelry.\n\r", ch);
				return;
			}

			if (!is_imbueable_jewelry(obj))
			{
				send_to_char("You cannot imbue that.\n\r", ch);
				return;
			}

			imbue_type = IMBUE_JEWELRY;
			max_mana = JEWELRY(obj)->max_mana;
		}
		else if (!str_prefix(arg, "wand"))
		{
			if (!IS_WAND(obj))
			{
				send_to_char("That is not a wand.\n\r", ch);
				return;
			}

			if (!is_imbueable_wand(obj))
			{
				send_to_char("You cannot imbue that.\n\r", ch);
				return;
			}

			imbue_type = IMBUE_WAND;
			max_mana = WAND(obj)->max_mana;
		}
		else if (!str_prefix(arg, "weapon"))
		{
			if (!IS_WEAPON(obj))
			{
				send_to_char("That is not a weapon.\n\r", ch);
				return;
			}

			if (!is_imbueable_weapon(obj))
			{
				send_to_char("You cannot imbue that.\n\r", ch);
				return;
			}

			imbue_type = IMBUE_WEAPON;
			max_mana = WEAPON(obj)->max_mana;
		}
		else
		{
			bool first = true;
			sprintf(buf, "$p has multiple parts that can be imbued:");

			// TODO: Add JEWELRY, GEM and other adornments
			if (is_imbueable_jewelry(obj))
			{
				if (first)
					strcat(buf, " jewelry");
				else
					strcat(buf, ", jewelry");
				first = false;
			}
			if (is_imbueable_wand(obj))
			{
				if (first)
					strcat(buf, " wand");
				else
					strcat(buf, ", wand");
				first = false;
			}
			if (is_imbueable_weapon(obj))
			{
				if (first)
					strcat(buf, " weapon");
				else
					strcat(buf, ", weapon");
				first = false;
			}
			act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
			return;
		}
	}
	else if (imbue_type == IMBUE_JEWELRY)
		max_mana = JEWELRY(obj)->max_mana;
	else if (imbue_type == IMBUE_WAND)
		max_mana = WAND(obj)->max_mana;
	else if (imbue_type == IMBUE_WEAPON)
		max_mana = WEAPON(obj)->max_mana;


    if (argument[0] == '\0')
    {
		send_to_char("What do you wish to imbue?\n\r", ch);
		return;
    }

	spells[0] = NULL;
	spells[1] = NULL;
	spells[2] = NULL;

	// Clear out catalyst usages
	memset(ch->catalyst_usage, 0, sizeof(ch->catalyst_usage));
	argument = one_argument(argument, arg);
	spells[0] = skill_entry_findname(ch->sorted_skills, arg);
	if (!can_imbue_spell(ch, obj, spells[0], imbue_type))
		return;

	if (argument[0] != '\0')
	{
		argument = one_argument(argument, arg);
		spells[1] = skill_entry_findname(ch->sorted_skills, arg);
		if (!can_imbue_spell(ch, obj, spells[1], imbue_type))
			return;

		if (argument[0] != '\0')
		{
			argument = one_argument(argument, arg);
			spells[2] = skill_entry_findname(ch->sorted_skills, arg);
			if (!can_imbue_spell(ch, obj, spells[2], imbue_type))
				return;
		}
	}

    mana = 0;
	for(int i = 0; i < 3; i++)
	{
		if (spells[i])
			mana += spells[i]->skill->imbue_mana;
	}

    if (mana > max_mana)
    {
		act("$p cannot hold that much magic.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
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

    act("{Y$n begins to imbue $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    act("{YYou begin to imbue $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	ch->imbue_type = imbue_type;
	ch->imbue_info[0] = spells[0];
	ch->imbue_info[1] = spells[1];
	ch->imbue_info[2] = spells[2];
	ch->imbue_obj = obj;

    IMBUE_STATE(ch, 12);

}

bool restring_imbue_obj(OBJ_DATA *obj)
{
	int types = 0;

	if (IS_JEWELRY(obj)) types++;
	if (IS_WAND(obj)) types++;
	if (IS_WEAPON(obj)) types++;

	return types == 1;
}

void imbue_end(CHAR_DATA *ch)
{
    char buf[2*MAX_STRING_LENGTH];
	SPELL_DATA *spell;
    int chance;
    char imbue_name[MAX_STRING_LENGTH];

	chance = get_skill(ch, gsk_imbue);
	if (ch->imbue_info[2])
		chance = 5 * chance / 6;	// 1/2 + 1/3 = 5/6
	else if (ch->imbue_info[1])
		chance = 41 * chance / 42;	// 1/2 + 1/3 + 1/7 = 41/42

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = 3 * chance / 2;

    chance = URANGE(1, chance, 99);

	// TODO: Add Inspiration system that guarantees artificing
    if (IS_IMMORTAL(ch))
	{
        chance = 100;
	}

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
		// TODO: Possible to critical failure, causing the imbuing item to lose its ability to be imbued?
//        act("{Y$n's scroll explodes into flame.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
//        act("{YYour blank scroll explodes into flame as you make a minor mistake.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

		switch(ch->imbue_type)
		{
			case IMBUE_JEWELRY:
				break;
			case IMBUE_WAND:
				break;
			case IMBUE_WEAPON:
				break;
		}
        check_improve(ch, gsk_imbue, false, 3);
        return;
    }

	// Use all the catalysts
	for(int i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
			use_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM,ch->catalyst_usage[i],true);
	}

	// Post processing after successful imbuing
	for(int i = 0; i < 3; i++)
	{
		SKILL_ENTRY *entry = ch->imbue_info[i];
		if (entry)
		{
			if (IS_VALID(entry->token))
			{
				p_percent_trigger(NULL, NULL, NULL, entry->token, ch, NULL, NULL, ch->imbue_obj, NULL, TRIG_TOKEN_IMBUE, NULL,ch->imbue_type,0,0,0,0);
			}
			else if(entry->skill->imbue_fun)
			{
				(*entry->skill->imbue_fun)(entry->skill, ch->tot_level, ch, ch->imbue_obj, ch->imbue_type);
			}
		}
	}

	if (ch->imbue_info[2])
		sprintf(imbue_name, "%s, %s, %s", skill_entry_name(ch->imbue_info[0]), skill_entry_name(ch->imbue_info[1]), skill_entry_name(ch->imbue_info[2]));
	else if (ch->imbue_info[1])
		sprintf(imbue_name, "%s, %s", skill_entry_name(ch->imbue_info[0]), skill_entry_name(ch->imbue_info[1]));
	else
		strcpy(imbue_name, skill_entry_name(ch->imbue_info[0]));

	act("You imbue $t into $p.", ch, NULL, NULL, ch->imbue_obj, NULL, imbue_name, NULL, TO_CHAR);
	act("$n imbues $t into $p.", ch, NULL, NULL, ch->imbue_obj, NULL, imbue_name, NULL, TO_ROOM);

    check_improve(ch, gsk_imbue, true, 3);

	int count = 0;
	for(int i = 0; i < 3; i++)
		if (ch->imbue_info[i]) count++;

	// TODO: change when classes are redone
	// TODO: Maybe change to a perk?
	bool alchemist = (ch->pcdata->current_class->clazz == gcl_alchemist);

	// TODO: Charges and recharging are in the item or a product of imbuing?

	LLIST *spells = NULL;
	switch(ch->imbue_type)
	{
		case IMBUE_JEWELRY:	spells = JEWELRY(ch->imbue_obj)->spells; break;
		case IMBUE_WAND:	spells = WAND(ch->imbue_obj)->spells; break;
		case IMBUE_WEAPON:	spells = WEAPON(ch->imbue_obj)->spells; break;
		default:
			return;
	}

	for(int i = 0; i < 3; i++)
	{
		if (ch->imbue_info[i])
		{
			spell = new_spell();

			spell->skill = ch->imbue_info[i]->skill;
			spell->level = ch->tot_level / count;
			if (count > 1 && alchemist)
				spell->level += ch->tot_level / (count + 1);
			list_appendlink(spells, spell);
		}
	}

	// Imbuing it will make it "magical"
	SET_BIT(ch->imbue_obj->extra[0], ITEM_MAGIC);

	// Determine if the object should be restrung	
	if (restring_imbue_obj(ch->imbue_obj))
	{
		OBJ_INDEX_DATA *template = NULL;
		char *type = "";
		switch(ch->imbue_type)
		{
			case IMBUE_JEWELRY:
				break;

			case IMBUE_WAND:
				template = obj_index_wand;
				type = "wand";
				break;
			
			case IMBUE_WEAPON:
				// TODO: Make it based upon the weapon type?
				//  Example: staff of fly
				//           sword of lightning bolt

				break;
		}

		if (template)
		{
		    sprintf(buf, template->short_descr, imbue_name);

			free_string(ch->imbue_obj->short_descr);
			ch->imbue_obj->short_descr = str_dup(buf);

			sprintf(buf, template->description, imbue_name);

			free_string(ch->imbue_obj->description);
			ch->imbue_obj->description = str_dup(buf);

			free_string(ch->imbue_obj->full_description);
			ch->imbue_obj->full_description = str_dup(buf);

			free_string(ch->imbue_obj->name);
			strcat(imbue_name, " ");
			strcat(imbue_name, type);
			ch->imbue_obj->name = short_to_name(imbue_name);
		}
	}
}

void do_plant(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    char arg1[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);

    if (is_dead(ch))
	return;

    if (arg1[0] == '\0')
    {
        send_to_char("Plant what?\n\r", ch);
        return;
    }

    if ((obj = get_obj_carry(ch, arg1, ch)) == NULL)
    {
        send_to_char("You do not have that item.\n\r", ch);
        return;
    }

    if (obj->item_type != ITEM_SEED)
    {
        send_to_char("You can't plant that item.\n\r", ch);
        return;
    }

    if (!IS_OUTSIDE(ch))
    {
        send_to_char("There is no way you can plant that here.\n\r", ch);
        return;
    }

    act("$n plants $p in the ground.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    act("You plant $p in the ground.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

    SET_BIT(obj->extra[0], ITEM_PLANTED);
    obj_from_char(obj);
    obj_to_room(obj, ch->in_room);
}


void do_hands(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    int chance;
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (is_dead(ch))
	return;

    if (arg[0] == '\0')
    {
	send_to_char("Who do you wish to heal?\n\r", ch);
	return;
    }

    if ((chance = get_skill(ch, gsk_healing_hands)) == 0)
    {
	send_to_char("Hands? Keep your hands to yourself.\n\r",ch);
	return;
    }

    if ((victim = get_char_room(ch, NULL, arg)) == NULL)
    {
	send_to_char("They aren't here.\n\r", ch);
	return;
    }

    if (ch != victim)
    {
	act("You place your hands on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n places $s hands on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	act("$n places $s hands on you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT);
    }
    else
    {
	act("You place your hands over your heart.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("$n places $s hands over $s heart.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    }

    WAIT_STATE(ch, gsk_healing_hands->beats);

    if (number_percent() > get_skill(ch, gsk_healing_hands))
    {
	act("You see a faint glow of magic, but nothing happens.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("You see a faint glow of magic eminating from $n's hands, but nothing happens.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	check_improve(ch, gsk_healing_hands, false, 1);
	return;
    }

    act("{CYour hands glow a brilliant blue.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("{C$n's hands glow a brilliant blue.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    spell_cure_disease(gsk_cure_disease, ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);
    spell_cure_poison(gsk_cure_poison, ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);
    spell_cure_blindness(gsk_cure_blindness, ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);
    spell_cure_toxic(gsk_cure_toxic, ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE);

    check_improve(ch, gsk_healing_hands, true, 1);
}


bool can_scribe_spell(CHAR_DATA *ch, OBJ_DATA *obj, SKILL_ENTRY *spell)
{
	if (!spell || !spell->isspell)
	{
		send_to_char("You don't know any spells by that name.\n\r", ch);
		return false;
	}

	if (!IS_SET(spell->skill->flags, SKILL_CAN_SCRIBE))
	{
		act_new("You cannot scribe $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
		return false;
	}

	if (IS_VALID(spell->token))
	{
		SCRIPT_DATA *script = get_script_token(spell->token->pIndexData, TRIG_TOKEN_RECITE, TRIGSLOT_SPELL);
		if(!script) {
			act_new("You cannot scribe $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		int ret = p_percent_trigger(NULL, NULL, NULL, spell->token, ch, NULL, NULL, obj, NULL, TRIG_TOKEN_PRESCRIBE, NULL,0,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				act_new("You cannot scribe $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			}
			return false;
		}
	}
	else
	{
		if(!spell->skill->recite_fun)
		{
			act_new("You cannot scribe $t onto $p.", ch,NULL,NULL,obj,NULL,spell->skill->name,NULL,TO_CHAR,POS_DEAD,NULL);
			return false;
		}

		if(spell->skill->prescribe_fun)
		{
			// prescribe function is responsible for messages.
			if (!(*spell->skill->prescribe_fun)(spell->skill, ch->tot_level, ch, obj))
				return false;
		}
	}

	return true;
}

// scribe <scroll> <spell1>[ <spell2>[ <spell3>]]
void do_scribe(CHAR_DATA *ch, char *argument)
{
	int need[CATALYST_MAX];
    OBJ_DATA *obj;
	SKILL_ENTRY *spells[3];
    int mana;
    int chance;
    //int kill;
    char arg[MIL];
	char buf[MSL];

    if (IS_DEAD(ch))
    {
		send_to_char("You can't do that. You are dead.\n\r", ch);
		return;
    }

    if ((chance = get_skill(ch, gsk_scribe)) == 0)
    {
		send_to_char("Scribe? What's that?\n\r",ch);
		return;
    }

	argument = one_argument(argument, arg);
	if ((obj = get_obj_carry(ch, arg, ch)) == NULL || !IS_SCROLL(obj) || list_size(SCROLL(obj)->spells) > 0)
	{
		send_to_char("You do not have a valid scroll to scribe on.\n\r", ch);
		return;
	}

    if (argument[0] == '\0')
    {
		send_to_char("What do you wish to scribe?\n\r", ch);
		return;
    }

	spells[0] = NULL;
	spells[1] = NULL;
	spells[2] = NULL;

	// Clear out catalyst usages
	memset(ch->catalyst_usage, 0, sizeof(ch->catalyst_usage));
	argument = one_argument(argument, arg);
	spells[0] = skill_entry_findname(ch->sorted_skills, arg);
	if (!can_scribe_spell(ch, obj, spells[0]))
		return;

	if (argument[0] != '\0')
	{
		argument = one_argument(argument, arg);
		spells[1] = skill_entry_findname(ch->sorted_skills, arg);
		if (!can_scribe_spell(ch, obj, spells[1]))
			return;

		if (argument[0] != '\0')
		{
			argument = one_argument(argument, arg);
			spells[2] = skill_entry_findname(ch->sorted_skills, arg);
			if (!can_scribe_spell(ch, obj, spells[2]))
				return;
		}
	}

    mana = 0;
	memset(need, 0, sizeof(need));
	for(int i = 0; i < 3; i++)
	{
		if (spells[i])
		{
			//mana += skill_entry_mana(ch, spells[i]);
			mana += spells[i]->skill->scribe_mana;
			for(int j = 0; j < 3; j++)
			{
				if (spells[i]->skill->inks[j][0] > CATALYST_NONE && spells[i]->skill->inks[j][0] < CATALYST_MAX)
					need[spells[i]->skill->inks[j][0]] += spells[i]->skill->inks[j][1];
			}
		}
	}

	int max_mana = (SCROLL(obj)->max_mana > 0) ? SCROLL(obj)->max_mana : 200;
    if (mana > max_mana)
    {
		send_to_char("The scroll can't hold that much magic.\n\r", ch);
		return;
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

	if (!has_inks(ch, need, true))
		return;

	// Get actual cost
    mana = 2 * mana / 3;
    if (ch->mana < mana)
    {
		send_to_char("You don't have enough mana to scribe that scroll.\n\r", ch);
		return;
    }

    ch->mana -= mana;

    act("{Y$n begins to write onto $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
    act("{YYou begin to write onto $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);

	int beats = 0;
	for(int i = 0; i < 3; i++)
	{
		ch->scribe_info[i] = spells[i];

		if(spells[i]) beats += 12;
	}
	ch->scribe_obj = obj;

	extract_inks(ch, need);

	// At this point, beats is greater than zero as we are doing at least one spell

	// Give speed increase for those with scribe over 75%
	if (chance > 75)
		beats = (125 - chance) * beats / 50;	// 75% = 100% time, 100% = 50% time

#if 0
	// TODO: Revisit
    /* Kill must not be one of the spells*/
    kill = find_spell(ch, "kill");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
		send_to_char("The scroll explodes into dust!\n\r", ch);
		act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
    }

    /* Mass healing must not be one of the spells*/
    kill = find_spell(ch, "mass healing");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
		send_to_char("The scroll explodes into dust!\n\r", ch);
		act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
    }

    kill = find_spell(ch, "spell trap");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
		send_to_char("The scroll explodes into dust!\n\r", ch);
		act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
		return;
    }
#endif

	SCRIBE_STATE(ch, beats);
}


void scribe_end(CHAR_DATA *ch)
{
    char buf[2*MAX_STRING_LENGTH];
    OBJ_DATA *scroll;
	SPELL_DATA *spell;
    int chance;
    char scroll_name[MAX_STRING_LENGTH];

	chance = get_skill(ch, gsk_scribe);
	if (ch->scribe_info[2])
		chance = 5 * chance / 6;	// 1/2 + 1/3 = 5/6
	else if (ch->scribe_info[1])
		chance = 41 * chance / 42;	// 1/2 + 1/3 + 1/7 = 41/42

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = 3 * chance / 2;

    chance = URANGE(1, chance, 99);

	// TODO: Add Inspiration system that guarantees artificing
    if (IS_IMMORTAL(ch))
	{
        chance = 100;
	}

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
        act("{Y$n's scroll explodes into flame.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        act("{YYour blank scroll explodes into flame as you make a minor mistake.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
        check_improve(ch, gsk_scribe, false, 3);
        return;
    }

	// Use all the catalysts
	for(int i = 0; i < CATALYST_MAX; i++)
	{
		if (ch->catalyst_usage[i] > 0)
			use_catalyst(ch,NULL,i,CATALYST_INVENTORY|CATALYST_ROOM,ch->catalyst_usage[i],true);
	}

	// Post processing after successful scribing
	for(int i = 0; i < 3; i++)
	{
		SKILL_ENTRY *entry = ch->scribe_info[i];
		if (entry)
		{
			if (IS_VALID(entry->token))
			{
				p_percent_trigger(NULL, NULL, NULL, entry->token, ch, NULL, NULL, ch->scribe_obj, NULL, TRIG_TOKEN_SCRIBE, NULL,0,0,0,0,0);
			}
			else if(entry->skill->scribe_fun)
			{
				(*entry->skill->scribe_fun)(entry->skill, ch->tot_level, ch, ch->scribe_obj);
			}
		}
	}

	if (ch->scribe_info[2])
		sprintf(scroll_name, "%s, %s, %s", skill_entry_name(ch->scribe_info[0]), skill_entry_name(ch->scribe_info[1]), skill_entry_name(ch->scribe_info[2]));
	else if (ch->scribe_info[1])
		sprintf(scroll_name, "%s, %s", skill_entry_name(ch->scribe_info[0]), skill_entry_name(ch->scribe_info[1]));
	else
		strcpy(scroll_name, skill_entry_name(ch->scribe_info[0]));

	act("You scribe $t onto $p", ch, NULL, NULL, ch->scribe_obj, NULL, scroll_name, NULL, TO_CHAR);
	act("$n scribes $t onto $p", ch, NULL, NULL, ch->scribe_obj, NULL, scroll_name, NULL, TO_ROOM);

    check_improve(ch, gsk_scribe, true, 3);

    scroll = ch->scribe_obj;//create_object(obj_index_scroll, 1, false);

	OBJ_INDEX_DATA *template = obj_index_scroll;
	// TODO: Add a reference to SCROLL objects in multityping for the SCRIBE template

	// Update regardless
	sprintf(buf, template->short_descr, scroll_name);

	free_string(scroll->short_descr);
	scroll->short_descr = str_dup(buf);

	sprintf(buf, template->description, scroll_name);

	free_string(scroll->description);
	scroll->description = str_dup(buf);

	free_string(scroll->full_description);
	scroll->full_description = str_dup(buf);

	int count = 0;
	for(int i = 0; i < 3; i++)
		if (ch->scribe_info[i]) count++;

	// TODO: change when classes are redone
	// TODO: Maybe change to a perk?
	bool alchemist = (ch->pcdata->current_class->clazz == gcl_alchemist);

	for(int i = 0; i < 3; i++)
	{
		if (ch->scribe_info[i])
		{
			spell = new_spell();

			spell->skill = ch->scribe_info[i]->skill;
			//spell->token = IS_VALID(ch->scribe_info[i]->token) ? ch->scribe_info[i]->token->pIndexData : NULL;
			spell->level = ch->tot_level / count;
			if (count > 1 && alchemist)
				spell->level += ch->tot_level / (count + 1);
			list_appendlink(SCROLL(scroll)->spells, spell);
		}
	}

    free_string(scroll->name);
    strcat(scroll_name, " scroll");
    scroll->name = short_to_name(scroll_name);
}

bool is_object_in_room(ROOM_INDEX_DATA *room, OBJ_INDEX_DATA *obj_index)
{
	OBJ_DATA *obj;
    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
		if (obj->pIndexData == obj_index)
		{
			return true;
		}
    }
    return false;
}

bool is_extra_damage_relic_in_room(ROOM_INDEX_DATA *room)
{
	return is_object_in_room(room, obj_index_relic_extra_damage);
}


bool is_extra_xp_relic_in_room(ROOM_INDEX_DATA *room)
{
	return is_object_in_room(room, obj_index_relic_extra_xp);
}

bool is_hp_regen_relic_in_room(ROOM_INDEX_DATA *room)
{
	return is_object_in_room(room, obj_index_relic_hp_regen);
}


bool is_mana_regen_relic_in_room(ROOM_INDEX_DATA *room)
{
	return is_object_in_room(room, obj_index_relic_mana_regen);
}


void do_bomb(CHAR_DATA *ch, char *argument)
{
    if (is_dead(ch))
	return;

    if (get_skill(ch, gsk_bomb) == 0)
    {
        send_to_char("You know nothing about explosives.\n\r",ch);
        return;
    }

    if (ch->mana < ch->max_mana/2 || ch->move < ch->max_move/2) {
	send_to_char("You need to have mana and movement at at least half full to make a bomb.\n\r", ch);
	return;
    }

    act("{Y$n begins to create a smoke bomb...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{YYou begin to create a smoke bomb...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    BOMB_STATE(ch, 24);
    return;
}


void bomb_end(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    int chance = get_skill(ch, gsk_bomb);

    if (number_percent() < chance)
    {
	act("{Y$n creates a smoke bomb.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YYou complete the construction of a smoke bomb.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	obj = create_object(obj_index_smoke_bomb, ch->tot_level, false);
	obj->level = ch->tot_level;
	if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
	{
	    act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	    act("$n drops $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	    obj_to_room(obj,ch->in_room);
	}
	else
	    obj_to_char(obj, ch);

	check_improve(ch, gsk_bomb, true, 1);
    } else {
	act("{Y$n's homemade explosives {REXPLODE{Y, causing $m great pain!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{YYour smoke bomb {REXPLODES{Y as you bumble up the recipe! {ROUCH!!!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	damage(ch, ch, (2 * ch->max_hit)/3, gsk_bomb, 0, DAM_ENERGY, false);

	check_improve(ch, gsk_bomb, false, 1);
    }

    ch->mana -= ch->mana/4;
    ch->move -= ch->move/4;
}


void do_infuse(CHAR_DATA *ch, char *argument)
{
#if 0
    OBJ_DATA *obj;
    AFFECT_DATA af;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int percent,skill;
    long weapon;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ((skill = get_skill(ch, gsk_infuse)) == 0)
    {
		send_to_char("What?\n\r", ch);
		return;
    }

    /* find out what */
    if (arg1[0] == '\0')
    {
		send_to_char("Infuse what item?\n\r",ch);
		return;
    }

    obj =  get_obj_list(ch,arg1,ch->carrying);

    if (obj== NULL)
    {
		send_to_char("You don't have that item.\n\r",ch);
		return;
    }

    if (IS_WEAPON(obj))
    {
        if (arg2[0] == '\0')
        {
		    act("Infuse $p with what?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		    return;
        }

        if (IS_WEAPON_STAT(obj,WEAPON_FLAMING) ||
			IS_WEAPON_STAT(obj,WEAPON_FROST) ||
			IS_WEAPON_STAT(obj,WEAPON_VAMPIRIC) ||
			IS_WEAPON_STAT(obj,WEAPON_VORPAL) ||
			IS_WEAPON_STAT(obj,WEAPON_SHOCKING) ||
			IS_WEAPON_STAT(obj,WEAPON_ACIDIC) ||
			IS_WEAPON_STAT(obj,WEAPON_RESONATE) ||
			IS_WEAPON_STAT(obj,WEAPON_BLAZE) ||
			IS_WEAPON_STAT(obj,WEAPON_SUCKLE))
        {
            act("$p is already infused with a magic enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
            return;
        }


		if (WEAPON(obj)->attack < 0 || attack_table[WEAPON(obj)->attack].damage == DAM_BASH)
		{
			send_to_char("You can only envenom edged weapons.\n\r",ch);
			return;
		}

		weapon = -1;
		if (!str_cmp(arg2, "fire"))					weapon = WEAPON_FLAMING;
		else if (!str_cmp(arg2, "frost"))			weapon = WEAPON_FROST;
		else if (!str_cmp(arg2, "vampiric"))		weapon = WEAPON_VAMPIRIC;
		else if (!str_cmp(arg2, "vorpal"))			weapon = WEAPON_VORPAL;
		else if (!str_cmp(arg2, "shocking"))		weapon = WEAPON_SHOCKING;
		else if (!str_cmp(arg2, "acidic"))			weapon = WEAPON_ACIDIC;
		else if (!str_cmp(arg2, "resonate"))		weapon = WEAPON_RESONATE;
		else if (!str_cmp(arg2, "blaze"))			weapon = WEAPON_BLAZE;
		else if (!str_cmp(arg2, "suckle"))			weapon = WEAPON_SUCKLE;

		if (weapon == -1)
		{
			send_to_char("Can only infuse fire, frost, vampiric, sharp, vorpal, shocking, acidic, resonate, blaze, suckle.\n\r", ch);
			return;
		}

		percent = number_percent();
		if (percent < skill)
		{
			memset(&af,0,sizeof(af));
			af.where     = TO_WEAPON;
			af.group     = AFFGROUP_WEAPON;
			af.skill     = gsk_infuse;
			af.level     = (ch->tot_level * skill)/ 100;
			af.duration  = ((ch->tot_level/2) * skill)/ 100;
			af.location  = 0;
			af.modifier  = 0;

			af.bitvector = weapon;
			af.bitvector2 = 0;
			af.slot	= WEAR_NONE;
			affect_to_obj(obj,&af);

			act("$n carefully infuses $p with a magical enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM);
			act("You carefully infuse $p with a magical enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			check_improve(ch,gsk_infuse,true,3);
			WAIT_STATE(ch,gsk_infuse->beats);
			return;
		}
		else
		{
			act("You fail to infuse $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
			check_improve(ch,gsk_infuse,false,3);
			WAIT_STATE(ch,gsk_infuse->beats);
			return;
		}
    }
    act("You can't infuse $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR);
#else
	command_under_construction(ch);
#endif
}


void repair_end(CHAR_DATA *ch)
{
    if (ch->repair_obj == NULL)
    {
	char buf[MAX_STRING_LENGTH];

	sprintf(buf, "repair_end: ch->repair_obj was null! ");
	sprintf(buf, "char was %s.", ch->name);

	bug(buf, 0);
	return;
    }

    act("{YYou complete repairing $p.{x", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    act("{Y$n completes $s repairs of $p.{x", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_ROOM);

    ch->repair_obj->condition += ch->repair_amt;
    ch->repair = 0;
    ch->repair_amt = 0;

    if (ch->repair_obj->condition < 10)
	act("$p is still almost crumbling.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 20)
	act("$p is still in extremely bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 30)
	act("$p is still in very bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 40)
	act("$p is still in bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 50)
	act("$p looks like it will hold together.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 60)
	act("$p looks to be in usable condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 75)
	act("$p now looks to be in fair condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 90)
	act("$p now looks almost new.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition < 100)
	act("$p now looks brand new.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);
    else if (ch->repair_obj->condition >= 100)
	act("$p has been repaired completely.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR);

    if (number_percent() < 20)
	ch->repair_obj->times_fixed++;

    ch->repair_obj = NULL;
}

void do_dig(CHAR_DATA *ch, char *argument) {
  OBJ_DATA *obj;
  bool found = false;

	if (!IN_WILDERNESS(ch)) {
    send_to_char("The ground is too hard to dig here.\n\r", ch);
		return;
	}

	obj = get_eq_char(ch,WEAR_HOLD);
	if ( obj == NULL || obj->item_type != ITEM_SHOVEL ) {
    send_to_char("You must be holding a shovel to dig.\n\r", ch);
    return;
  }

  send_to_char("You start digging the ground, carefully looking for any items.\n\r", ch);

    for (obj = ch->in_room->contents; obj != NULL; obj = obj->next_content)
    {
       if (IS_SET(obj->extra[1], ITEM_BURIED)) {
          REMOVE_BIT(obj->extra[1], ITEM_BURIED);
          act("You have discovered $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
          found = true;
       }
    }

    if (!found) {
      send_to_char("You found nothing.\n\r", ch);
	  }
}


void do_use(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	OBJ_DATA *obj, *tobj = NULL;
	CHAR_DATA *vch = NULL;


	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("What do you want to use?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char ("You can't find it.\n\r",ch);
		return;
	}

	if(argument[0]) {
		if(!(vch = get_char_room(ch, NULL, argument)) &&
			!(tobj = get_obj_here(ch, NULL, argument))) {
			send_to_char("What do you want to use it with?\n\r", ch);
			return;
		}

		if(p_use_with_trigger(ch, obj, TRIG_USEWITH, tobj, NULL, vch, NULL,0,0,0,0,0)) return;
		if(tobj && p_use_with_trigger(ch, tobj, TRIG_USEWITH, obj, NULL, vch, NULL,0,0,0,0,0)) return;

		send_to_char("Nothing happens.\n\r", ch);
		return;
	}

	if(p_use_trigger(ch, obj, TRIG_USE,0,0,0,0,0)) return;

	send_to_char("Nothing happens.\n\r", ch);
}

/* Conceal merely hides an item.  No special affects will be done when doing this.*/
void do_conceal(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	OBJ_DATA *obj;
	CHAR_DATA *rch;

	one_argument(argument, arg);

	if (IS_SHIFTED_SLAYER(ch) || IS_SHIFTED_WEREWOLF(ch)) {
		send_to_char("You can't do that in your current form.\n\r", ch);
		return;
	}

	if (IS_AFFECTED(ch, AFF_BLIND)) {
		send_to_char("You can't see a thing!\n\r", ch);
		return;
	}

	if (!arg[0]) {
		send_to_char("Conceal what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_carry(ch, arg, ch))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (get_eq_char(ch, WEAR_CONCEALED)) {
		send_to_char("You already have something concealed.\n\r", ch);
		return;
	}

	if (!IS_IMMORTAL(ch) && obj->level > LEVEL_HERO) {
		send_to_char("Powerful forces prevent you from concealing that.\n\r", ch);
		return;
	}

	if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL,0,0,0,0,0))
		return;

	act("You conceal $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	for(rch = ch->in_room->people; rch; rch = rch->next_in_room) {
		if(!can_see(rch,ch)) continue;

		if(!IS_SAGE(rch)) {
			if(IS_AFFECTED(ch,AFF_SNEAK)) continue;
			if(IS_AFFECTED(ch,AFF_HIDE) && !IS_AFFECTED(rch,AFF_DETECT_HIDDEN)) continue;
		}

		act("$n conceals $p upon $mself.",  ch, rch, NULL, obj, NULL, NULL, NULL, TO_VICT);
	}

	if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL) && IS_EVIL(ch)) ||
		(IS_OBJ_STAT(obj, ITEM_ANTI_GOOD) && IS_GOOD(ch)) ||
		(IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch))) {
		act("You are zapped by $p and drop it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n is zapped by $p and drops it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		REMOVE_BIT(obj->extra[1], ITEM_KEPT);

		obj_from_char(obj);
		obj_to_room(obj, ch->in_room);
		return;
	}

	obj->wear_loc = WEAR_CONCEALED;
	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WEAR, NULL,0,0,0,0,0);
}


long haggle_price(CHAR_DATA *ch, CHAR_DATA *keeper, int chance, int number, long base_price, long funds, int discount, bool *haggled, bool silent)
{
	long price = adjust_keeper_price(keeper,(long)number * base_price, true);

	discount = URANGE(0,discount,99);

	if(IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE))
		haggled = NULL;

	if( price > 0 )
	{
		if( discount > 0 && haggled != NULL )
		{
			int roll = number_percent();
			if( roll < chance )
			{
				long disc = (discount * price) / 100;

				price -= (disc * roll) / 100;
				*haggled = true;
			}
		}

		if( funds < price )
		{
			if( !silent )
			{
				if (number > 1)
					act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT);
				else
					act("{R$n tells you 'You can't afford to buy that'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT);
				ch->reply = keeper;
			}
			return -1;
		}
	}

	return UMAX(price, 0);
}

char *get_stock_description(SHOP_STOCK_DATA *stock)
{
	if( !IS_NULLSTR(stock->custom_descr) )
		return stock->custom_descr;

	else if( stock->obj != NULL )
		return stock->obj->short_descr;

	else if( stock->mob != NULL )
		return stock->mob->short_descr;

	return "something";
}

void do_ignite(CHAR_DATA *ch, char *argument)
{
	OBJ_DATA *obj;
	char arg[MIL];

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Ignite what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_LIGHT(obj))
	{
		send_to_char("You cannot ignite that.\n\r", ch);
		return;
	}

	if (IS_SET(LIGHT(obj)->flags, LIGHT_IS_ACTIVE))
	{
		act("$p is already lit.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	// TODO: Add ignition mode onto LIGHT data

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREIGNITE, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot ignite that.\n\r", ch);
		}

		return;
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREIGNITE, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot ignite that here.\n\r", ch);
		}

		return;
	}

	// Apply light to the room
	if (obj->carried_by == ch && !light_char_has_light(ch))
		ch->in_room->light++;
	else if (!obj->in_obj && obj->in_room != NULL)
		obj->in_room->light++;

	act("You ignite $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n ignites $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	SET_BIT(LIGHT(obj)->flags, LIGHT_IS_ACTIVE);

	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_IGNITE, NULL, 0,0,0,0,0);
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_IGNITE, NULL, 0,0,0,0,0);
}

void do_extinguish(CHAR_DATA *ch, char *argument)
{
	OBJ_DATA *obj;
	char arg[MIL];

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Ignite what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_LIGHT(obj))
	{
		send_to_char("You cannot extinguish that.\n\r", ch);
		return;
	}

	if (!IS_SET(LIGHT(obj)->flags, LIGHT_IS_ACTIVE))
	{
		act("$p is not lit.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	if (IS_SET(LIGHT(obj)->flags, LIGHT_NO_EXTINGUISH))
	{
		act("$p cannot be put out.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	}

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREEXTINGUISH, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot extinguish that.\n\r", ch);
		}

		return;
	}

	ret = p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREEXTINGUISH, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot extinguish that here.\n\r", ch);
		}

		return;
	}

	REMOVE_BIT(LIGHT(obj)->flags, LIGHT_IS_ACTIVE);

	act("You extinguish $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	act("$n extinguishes $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

	// Remove light to the room
	if (obj->carried_by == ch && !light_char_has_light(ch))
		ch->in_room->light--;
	else if (!obj->in_obj && obj->in_room != NULL)
		obj->in_room->light--;

	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_EXTINGUISH, NULL, 0,0,0,0,0);
	p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, obj, NULL, TRIG_EXTINGUISH, NULL, 0,0,0,0,0);

	if (IS_SET(LIGHT(obj)->flags, LIGHT_REMOVE_ON_EXTINGUISH))
	{
		free_light_data(LIGHT(obj));
		LIGHT(obj) = NULL;

		if (obj->item_type == ITEM_LIGHT)
		{
			extract_obj(obj);
		}
	}
}

void show_book_page(CHAR_DATA *ch, OBJ_DATA *book)
{
	BOOK_PAGE *page = book_get_page(BOOK(book), BOOK(book)->current_page);
	if (!IS_VALID(page))
	{
		send_to_char("The page appears to be missing.\n\r", ch);
		return;
	}

	if (IS_NULLSTR(page->title) && IS_NULLSTR(page->text))
	{
		send_to_char("The page is blank.\n\r", ch);
		return;
	}

	int len = strlen(page->title);
	int lennc = strlen_no_colours(page->title);

	// (80 - lennc) / 2 + lennc + length of color codes
	int wtitle = (80 + lennc) / 2 + (len - lennc);

	BUFFER *buffer = new_buf();
	char buf[MSL];
	sprintf(buf, "%*.*s\n\r", wtitle, wtitle, page->title);
	add_buf(buffer, buf);
	add_buf(buffer, "--------------------------------------------------------------------------------\n\r");
	add_buf(buffer, page->text);
	add_buf(buffer, "--------------------------------------------------------------------------------\n\r");
	
	char page_no[MIL];
	sprintf(page_no, "%d", page->page_no);
	int wpage_no = (80 + strlen(page_no)) / 2;
	sprintf(buf, "%*.*s", wpage_no, wpage_no, page_no);
	add_buf(buffer, buf);

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
}

// READ <BOOK>
void do_read(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	OBJ_DATA *obj;
	char *start = argument;

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Read what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_BOOK(obj))
	{
		// Re-route to looking at it if it's not a book object
		do_look(ch, start);
		return;
	}

	if (list_size(BOOK(obj)->pages) < 1)
	{
		send_to_char("There is nothing to read.\n\r", ch);
		return;
	}

	if (IS_SET(BOOK(obj)->flags, BOOK_CLOSED))
	{
		send_to_char("The book is closed.\n\r", ch);
		return;
	}

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PREREAD, NULL, BOOK(obj)->current_page,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot read that.\n\r", ch);
		}

		return;
	}

	show_book_page(ch, obj);

	p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_READ, NULL, BOOK(obj)->current_page,0,0,0,0);
}

void do_write(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg[MIL];
	OBJ_DATA *obj;

	argument = one_argument(argument, arg);

	if (IS_NULLSTR(argument))
	{
		send_to_char("Syntax:  write <book> title <title>   (up to 70 characters plain text)\n\r", ch);
		send_to_char("         write <book> text            (opens string editor)\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_BOOK(obj))
	{
		send_to_char("You cannot write in that.\n\r", ch);
		return;
	}

	if (list_size(BOOK(obj)->pages) < 1)
	{
		send_to_char("There are no pages to write on in this book.\n\r", ch);
		return;
	}

	if (IS_SET(BOOK(obj)->flags, BOOK_CLOSED))
	{
		send_to_char("The book is closed.\n\r", ch);
		return;
	}

	if (!IS_SET(BOOK(obj)->flags, BOOK_WRITABLE))
	{
		send_to_char("The pages of this book cannot be altered.\n\r", ch);
		return;
	}

	BOOK_PAGE *page = book_get_page(BOOK(obj), BOOK(obj)->current_page);
	if (!IS_VALID(page))
	{
		send_to_char("The page appears to be missing.\n\r", ch);
		return;
	}

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PREWRITE, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot write in that book.\n\r", ch);
		}

		return;
	}

	char arg2[MIL];
	argument = one_argument(argument, arg2);

	if (!str_prefix(arg2, "title"))
	{
		if (IS_NULLSTR(argument))
		{
			free_string(page->title);
			page->title = str_dup("");

			sprintf(buf, "You have removed the title for page %d.\n\r", page->page_no);
			send_to_char(buf, ch);
			act("$n removes something from $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		}
		else
		{
			if (strlen_no_colours(argument) > 70)
			{
				send_to_char("Title is too long.  Please limit non-colour text to 70 characters.\n\r", ch);
				return;
			}

			smash_tilde(argument);
			free_string(page->title);
			page->title = str_dup(argument);

			sprintf(buf, "You have changed the title for page %d to %s.\n\r", page->page_no, page->title);
			send_to_char(buf, ch);
			act("$n writes something in $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);

		}

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_WRITE_TITLE, NULL, 0,0,0,0,0);
		return;
	}

	if (!str_prefix(arg2, "text"))
	{
		act("$n starts writing something in $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
		ch->desc->writing_book = obj;
		string_append(ch, &page->text);
		return;
	}

	do_write(ch, "");
}

void do_seal(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	OBJ_DATA *obj;

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Read what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_carry(ch, arg, ch))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_BOOK(obj))
	{
		send_to_char("You cannot seal that.\n\r", ch);
		return;
	}

	if (!IS_SET(BOOK(obj)->flags, BOOK_WRITABLE))
	{
		send_to_char("The pages of this book cannot be altered.\n\r", ch);
		return;
	}

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PRESEAL, NULL, 0,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot seal that book.\n\r", ch);
		}

		return;
	}

	ch->seal_book = obj;
}

// PAGE <book> first|last|next|previous|<page#>
// Book must be OPEN
void do_page(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg[MIL];
	OBJ_DATA *obj;

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Page what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_BOOK(obj))
	{
		send_to_char("That is not a book.\n\r", ch);
		return;
	}

	if (list_size(BOOK(obj)->pages) < 1)
	{
		send_to_char("There are no pages in that book.\n\r", ch);
		return;
	}

	if (IS_SET(BOOK(obj)->flags, BOOK_CLOSED))
	{
		send_to_char("The book is closed.\n\r", ch);
		return;
	}

	BOOK_PAGE *page;
	int page_no;
	if (!str_prefix(argument, "first"))
	{
		page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, 1);
		page_no = page->page_no;
		act("You turn $p to the first page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n turns $p to the first page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	}
	else if (!str_prefix(argument, "last"))
	{
		page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, -1);
		page_no = page->page_no;
		act("You turn $p to the last page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n turns $p to the last page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	}
	else if (!str_prefix(argument, "next"))
	{
		// Get the last page
		BOOK_PAGE *last_page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, -1);
		if (BOOK(obj)->current_page == last_page->page_no)
		{
			send_to_char("There are no more pages.\n\r", ch);
			return;
		}

		ITERATOR it;
		BOOK_PAGE *page;
		iterator_start(&it, BOOK(obj)->pages);
		while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
			if (page->page_no > BOOK(obj)->current_page)
				break;
		iterator_stop(&it);

		if (page)
			page_no = page->page_no;
		else
		{
			send_to_char("There are no more pages.\n\r", ch);
			return;
		}

		act("You turn $p to the next page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n flips $p to the next page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	}
	else if (!str_prefix(argument, "previous"))
	{
		// Get the first page
		BOOK_PAGE *first_page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, 1);
		if (BOOK(obj)->current_page == first_page->page_no)
		{
			send_to_char("There are no more pages.\n\r", ch);
			return;
		}

		ITERATOR it;
		BOOK_PAGE *page;
		iterator_start(&it, BOOK(obj)->pages);
		while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
			if (page->page_no >= BOOK(obj)->current_page)
				break;
		if (page)
			page = (BOOK_PAGE *)iterator_prevdata(&it);
		iterator_stop(&it);

		if (page)
			page_no = page->page_no;
		else
		{
			send_to_char("There are no more pages.\n\r", ch);
			return;
		}

		act("You turn $p to the previous page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		act("$n flips $p back a page.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM);
	}
	else if (!is_number(argument))
	{
		send_to_char("Please specify a number.\n\r", ch);
		return;
	}
	else
	{
		// Get the last page
		BOOK_PAGE *last_page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, -1);

		page_no = atoi(argument);
		if (page_no < 1 || page_no > last_page->page_no)
		{
			sprintf(buf, "Please specify a page number from 1 to %d.\n\r", last_page->page_no);
			send_to_char(buf, ch);
			return;
		}

		char pagestr[MIL];
		sprintf(pagestr, "%d", page_no);

		char pagestr2[MIL];
		int third = last_page->page_no / 3;
		if (page_no < third)
			strcpy(pagestr2, "toward the beginning");
		else if (page_no > (2 * third))
			strcpy(pagestr2, "toward the end");
		else
			strcpy(pagestr2, "in the middle");

		act("You turn $p to page $t.", ch, NULL, NULL, obj, NULL, pagestr, NULL, TO_CHAR);
		act("$n turns $p to a page $t.", ch, NULL, NULL, obj, NULL, pagestr2, NULL, TO_ROOM);
	}
	BOOK(obj)->current_page = page_no;

	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PAGE, NULL, page_no,0,0,0,0);
	if (!ret)
	{
		int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PREREAD, NULL, page_no,0,0,0,0);
		if (ret)
		{
			if (ret != PRET_SILENT)
			{
				send_to_char("You cannot read that.\n\r", ch);
			}

			return;
		}

		show_book_page(ch, obj);

		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_READ, NULL, page_no,0,0,0,0);
	}
}

// RIP <book>[ <page#>] (defaults to current page)
// Contents of the page are put into a scroll.
// TODO: Instead of a scroll, have a PAGE type so the title and text can be preserved separately.
void do_rip(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	char arg[MIL];
	OBJ_DATA *obj;

	argument = one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Page what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_here(ch, NULL, arg))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_BOOK(obj))
	{
		send_to_char("That is not a book.\n\r", ch);
		return;
	}

	if (list_size(BOOK(obj)->pages) < 1)
	{
		send_to_char("There are no pages in that book.\n\r", ch);
		return;
	}

	if (IS_SET(BOOK(obj)->flags, BOOK_CLOSED))
	{
		send_to_char("The book is closed.\n\r", ch);
		return;
	}

	if (IS_SET(BOOK(obj)->flags, BOOK_NO_RIP))
	{
		send_to_char("The pages are too strong to rip out.\n\r", ch);
		return;
	}

	if (list_size(BOOK(obj)->pages) < 1)
	{
		send_to_char("There are no pages to rip out.\n\r", ch);
		return;
	}

	BOOK_PAGE *last_page = (BOOK_PAGE *)list_nthdata(BOOK(obj)->pages, -1);
	int page_no;
	if (IS_NULLSTR(argument))
		page_no = BOOK(obj)->current_page;
	else if(!is_number(argument) || (page_no = atoi(argument)) < 1 || page_no < last_page->page_no)
	{
		sprintf(buf, "Please specify a page number from 1 to %d.\n\r", last_page->page_no);
		send_to_char(buf, ch);
		return;
	}

	// register1 = page_no
	int ret = p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BOOK_PAGE_PRERIP, NULL, page_no,0,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			send_to_char("You cannot rip that page out.\n\r", ch);
		}

		return;
	}

	ITERATOR it;
	BOOK_PAGE *page;
	iterator_start(&it, BOOK(obj)->pages);
	while((page = (BOOK_PAGE *)iterator_nextdata(&it)))
		if (page->page_no == page_no)
			break;

	OBJ_DATA *torn_page = NULL;
	if (page)
	{
		torn_page = create_object(obj_index_page, 1, false);

		sprintf(buf, torn_page->short_descr, obj->short_descr);
		free_string(torn_page->short_descr);
		torn_page->short_descr = str_dup(buf);

		if (!IS_NULLSTR(page->title))
		{
			char *title = nocolour(page->title);
			sprintf(buf, "A torn page, titled '%s', lies crumpled up here.", title);
			free_string(title);
			free_string(torn_page->description);
			torn_page->description = str_dup(buf);
		}

		int len = strlen(page->title);
		int lennc = strlen_no_colours(page->title);

		// (80 - lennc) / 2 + lennc + length of color codes
		int wtitle = (80 + lennc) / 2 + (len - lennc);

		BUFFER *buffer = new_buf();

		sprintf(buf, "%*.*s\n\r", wtitle, wtitle, page->title);
		add_buf(buffer, buf);

		add_buf(buffer, "--------------------------------------------------------------------------------\n\r");
		if (IS_NULLSTR(page->text))
		{
			char *blank = "BLANK PAGE";
			int wblank = (80 + strlen(blank)) / 2;
			sprintf(buf, "\n\r%*.*s\n\r\n\r", wblank, wblank, blank);
			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, page->text);
		}
		add_buf(buffer, "--------------------------------------------------------------------------------\n\r");
		
		char page_no_str[MIL];
		sprintf(page_no_str, "%d", page_no);
		int wpage_no = (80 + strlen(page_no_str)) / 2;
		sprintf(buf, "%*.*s", wpage_no, wpage_no, page_no_str);
		add_buf(buffer, buf);

		free_string(torn_page->full_description);
		torn_page->full_description = str_dup(buf_string(buffer));
		free_buf(buffer);

		free_string(torn_page->name);
		torn_page->name = short_to_name(torn_page->short_descr);

		// Install multi-typing stuff
		if (!IS_PAGE(torn_page)) PAGE(torn_page) = new_book_page();
		PAGE(torn_page)->page_no = page->page_no;
		free_string(PAGE(torn_page)->title);
		PAGE(torn_page)->title = str_dup(page->title);
		free_string(PAGE(torn_page)->text);
		PAGE(torn_page)->text = str_dup(page->text);

		obj_to_char(torn_page, ch);

		iterator_remcurrent(&it);

		char pagestr[MIL];
		sprintf(pagestr, "%d", page_no);
		act("You rip page $t out of $p.", ch, NULL, NULL, obj, NULL, pagestr, NULL, TO_CHAR);
		act("$n rips a page out of $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	}
	else
	{
		send_to_char("That page is already missing.\n\r", ch);
	}
	iterator_stop(&it);


	if (page)
	{
		// Do this AFTER cleaning up the iterator
		// obj1 = torn page
		// register1 = page number
		p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, torn_page, NULL, TRIG_BOOK_PAGE_RIP, NULL, page_no,0,0,0,0);
	}
}


// ATTACH <page> <book>[ <append>]
void do_attach(CHAR_DATA *ch, char *argument)
{
	char arg[MIL];
	char arg2[MIL];
	OBJ_DATA *page;
	OBJ_DATA *book;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!arg[0] || !arg2[0]) {
		send_to_char("Attach what to what?\n\r", ch);
		return;
	}

	if (!(page = get_obj_carry(ch, arg, ch))) {
		send_to_char("You do not have that item.\n\r", ch);
		return;
	}

	if (!IS_PAGE(page))
	{
		send_to_char("That is not a page.\n\r", ch);
		return;
	}

	if (!(book = get_obj_here(ch, NULL, argument))) {
		send_to_char("You do not see that here.\n\r", ch);
		return;
	}

	if (!IS_BOOK(book))
	{
		send_to_char("That is not a book.\n\r", ch);
		return;
	}

	int page_no = PAGE(page)->page_no;
	int mode = 0;			// 0 = insert normally, 1 = append, 2 = specific page
	if (is_number(argument))
	{
		page_no = atoi(argument);
		if (page_no < 1)
		{
			send_to_char("Please specify a positive page number.\n\r", ch);
			return;
		}

		mode = 2;
	}
	else if (!str_prefix(argument, "append"))
	{
		BOOK_PAGE *last_page = (BOOK_PAGE *)list_nthdata(BOOK(book)->pages, -1);

		if (IS_VALID(last_page))
			page_no = last_page->page_no + 1;
		else
			page_no = 1;

		mode = 1;
	}

	// $(obj1) == page
	int ret = p_percent_trigger(NULL, book, NULL, NULL, ch, NULL, NULL, page, NULL, TRIG_BOOK_PAGE_PREATTACH, NULL, page_no,mode,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			act("You cannot attach $p to $P.", ch, NULL, NULL, page, book, NULL, NULL, TO_CHAR);
		}
		return;
	}

	// $(obj2) == book
	ret = p_percent_trigger(NULL, page, NULL, NULL, ch, NULL, NULL, NULL, book, TRIG_BOOK_PAGE_PREATTACH, NULL, page_no,mode,0,0,0);
	if (ret)
	{
		if (ret != PRET_SILENT)
		{
			act("You cannot attach $p to $P.", ch, NULL, NULL, page, book, NULL, NULL, TO_CHAR);
		}
		return;
	}

	BOOK_PAGE *new_page = copy_book_page(PAGE(page));
	new_page->page_no = page_no;

	if (!book_insert_page(BOOK(book), new_page))
	{
		act("You try to attach $p to $P, but it falls out.", ch, NULL, NULL, page, book, NULL, NULL, TO_CHAR);
		act("$n tries to attach $p to $P, but it falls out.", ch, NULL, NULL, page, book, NULL, NULL, TO_ROOM);
		free_book_page(new_page);
	}
	else
	{
		act("You attach $p to $P.", ch, NULL, NULL, page, book, NULL, NULL, TO_CHAR);
		act("$n attaches $p to $P.", ch, NULL, NULL, page, book, NULL, NULL, TO_ROOM);

		p_percent_trigger(NULL, book, NULL, NULL, ch, NULL, NULL, page, NULL, TRIG_BOOK_PAGE_ATTACH, NULL, page_no,mode,0,0,0);
		p_percent_trigger(NULL, page, NULL, NULL, ch, NULL, NULL, NULL, book, TRIG_BOOK_PAGE_ATTACH, NULL, page_no,mode,0,0,0);

		// Get rid of old page
		extract_obj(page);
	}



}