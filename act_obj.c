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
#include "tables.h"
#include "traits.h"
#include "skill_data.h"
#include "requirements.h"
#include "utils/localization.h"

/**
 * obj_has_money - Check if a container has money visible to character
 *
 * Iterates through a container's contents looking for ITEM_MONEY objects
 * that the character can see.
 *
 * @param ch         Character checking for money
 * @param container  Container object to search
 *
 * @return true if container has visible money, false otherwise
 */
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

/**
 * get_obj - Transfer an object to a character's inventory
 *
 * Core function for picking up objects. Validates the character can take
 * the object based on:
 * - ITEM_TAKE wear flag must be set
 * - Character's carry number limit
 * - Character's carry weight limit
 * - Object not being used by another character (furniture)
 * - ITEM_TRAPPED check (causes damage and prevents pickup)
 *
 * Handles both ground pickup and container extraction. Resets object
 * timer when taken from pit objects or corpses. Fires TRIG_GET triggers
 * on object and room after successful transfer.
 *
 * @param ch         Character taking the object
 * @param obj        Object being taken
 * @param container  Container object (NULL if from ground)
 *
 * Triggers: TRIG_GET (on object and room)
 */
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
    act( "$p: you can't carry that many items.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    if ((!obj->in_obj || obj->in_obj->carried_by != ch)
    &&  (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch)))
    {
    act( "$p: you can't carry that much weight.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
    return;
    }

    if (obj->in_room != NULL)
    {
    for (gch = obj->in_room->people; gch != NULL; gch = gch->next_in_room)
        if (gch->on == obj)
        {
        act("$N appears to be using $p.", ch, gch, NULL,obj, NULL, NULL, NULL,TO_CHAR, NULL, NULL);
        return;
        }
    }

    if ( IS_SET( obj->extra[1], ITEM_TRAPPED ) )
    {
        act("{RYou pick up $p, but recoil in pain and drop it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
    act("{R$n picks up $p, but recoils in pain and drops it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    damage( ch, ch, obj->trap_dam, 0, DAM_ENERGY, false );
    REMOVE_BIT( obj->extra[0], ITEM_TRAPPED );
    return;
    }

    if ( container != NULL )
    {
        if (container->pIndexData == get_reserved_obj_index("obj_pit")
    &&  get_staff_rank(ch) < obj->level)
    {
        send_to_char("You are not powerful enough to use it.\n\r",ch);
        return;
    }

        if (container->pIndexData == get_reserved_obj_index("obj_pit")
    &&  !CAN_WEAR(container, ITEM_TAKE)
    )
        obj->timer = 0;
    act( "You get $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL );
    act( "$n gets $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM, NULL, NULL );
    obj_from_obj( obj );
    }
    else
    {
    act( "You get $p.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL );
    act( "$n gets $p.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM, NULL, NULL );
    obj_from_room( obj );
    }

    if ( container == NULL || container->item_type == ITEM_CORPSE_PC
    || container->item_type == ITEM_CORPSE_NPC )
    reset_obj( obj );

    obj_to_char( obj, ch );

    p_give_trigger( NULL, obj, NULL, ch, obj, TRIG_GET );
    p_give_trigger( NULL, NULL, ch->in_room, ch, obj, TRIG_GET );

    return;
}

/**
 * give_money - Give gold and silver coins to a character with messages
 *
 * Creates appropriate grammatically-correct messages based on coin amounts
 * (singular/plural forms for gold and silver). Creates a money object and
 * transfers it to the character's inventory.
 *
 * @param ch         Character receiving the money
 * @param container  Container the money came from (for messaging, can be NULL)
 * @param gold       Amount of gold coins
 * @param silver     Amount of silver coins
 * @param indent     If true, indent the output message with spaces
 */
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

        act(buf1, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act(buf2, ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        obj_to_char(create_money(gold, silver), ch);
    }
}

/**
 * get_money_from_obj - Extract all money from a container
 *
 * Iterates through container contents, sums up all gold and silver
 * from ITEM_MONEY objects that the character can see and get.
 * Extracts each money object and creates a single consolidated
 * money object for the character.
 *
 * @param ch         Character getting the money
 * @param container  Container to extract money from
 */
void get_money_from_obj(CHAR_DATA *ch, OBJ_DATA *container)
{
    OBJ_DATA *obj;
    int gold = 0;
    int silver = 0;

    for(obj = container->contains; obj; obj = obj->next_content)
    {
        if( can_see_obj(ch, container) && obj->item_type == ITEM_MONEY )
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


/**
 * loot_corpse - Bulk loot all items from a corpse (excluding money)
 *
 * Efficiently loots all items from a corpse with consolidated messaging.
 * Groups identical objects by short_descr and displays counts rather than
 * individual pickup messages for each object.
 *
 * Process:
 * 1. Count non-money objects in corpse
 * 2. Allocate arrays to track unique objects and counts
 * 3. Group objects by matching short_descr
 * 4. Display grouped messages: "(N) You get X from corpse"
 * 5. Transfer objects and fire TRIG_GET triggers
 * 6. Clean up allocated memory
 *
 * @param ch      Character looting the corpse
 * @param corpse  Corpse object to loot
 *
 * Triggers: TRIG_GET (on each object)
 */
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
                act(buf, ch, NULL, NULL, objects[i], corpse, NULL, NULL, TO_ROOM, NULL, NULL);

                sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", counts[i]);
                act(buf, ch, NULL, NULL, objects[i], corpse, NULL, NULL, TO_CHAR, NULL, NULL);

                // Move objects and trigger TRIG_GET
                iterator_start(&it, lists[i]);
                while( (obj = (OBJ_DATA *)iterator_nextdata(&it)) )
                {
                    obj_from_obj(obj);
                    obj_to_char(obj, ch);

                    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL);
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




/**
 * do_get - Pick up objects from the ground or containers
 *
 * Versatile get command supporting multiple syntaxes:
 * - get <item>              : Pick up single item from ground
 * - get all                 : Pick up all items from ground
 * - get all.<type>          : Pick up all matching items from ground
 * - get <item> <container>  : Get item from container
 * - get all <container>     : Get all items from container
 * - get <N> gold/silver <container> : Get specific amount of coins
 * - get money <corpse>      : Get all money from corpse
 *
 * Special handling:
 * - NPC corpses owned by killer have priority looting
 * - PC corpses have ownership protection
 * - "loot" keyword triggers bulk looting with consolidated messages
 * - Pit objects have level restrictions
 * - Container open/close state checking
 *
 * @param ch        Character picking up objects
 * @param argument  Target object and optional container specification
 *
 * Triggers: TRIG_GET (via get_obj)
 *
 * Planned refactor: MOVED comment indicates intended move to object/object.c
 */
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
    int i = 0, amount;
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

    /* Get coins off the ground. */
    if (is_number(arg1) && (!str_cmp(arg2, "silver") || !str_cmp(arg2, "gold"))) {
        int gold, w;

        gold = !str_cmp(arg2, "gold");

        if(!arg3[0]) {
            send_to_char(gold?"Get gold from what?\n\r":"Get silver from what?\n\r",ch);
            return;
        }

        /* This section handles getting objects out of containers. */
        if ((container = get_obj_here(ch, NULL, arg3)) == NULL) {
            act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg3, TO_CHAR, NULL, NULL);
            return;
        }

        if (!can_get_obj(ch, container, NULL, NULL, false)) {
            send_to_char(gold?"Can't take gold from that.\n\r":"Can't take silver from that.\n\r",ch);
            return;
        }

        if (container->item_type == ITEM_MONEY) {
            char buffer[MIL];
            int ret;
            int *coin_ptr = gold ? &MONEY(container)->gold : &MONEY(container)->silver;
            if (!str_prefix("all.", arg1))
                amount = *coin_ptr;
            else
                amount = atol(arg1);

            if(amount < 1) {
                send_to_char("Take how much?\n\r",ch);
                return;
            }

            if(!*coin_ptr) {
                act("There is no $T in $p.", ch, NULL, NULL, NULL, NULL, container, gold?"gold":"silver", TO_CHAR, NULL, NULL);
                return;
            }

            if(amount > *coin_ptr) {
                act("There isn't that much $T in $p.", ch, NULL, NULL, container, NULL, NULL, gold?"gold":"silver", TO_CHAR, NULL, NULL);
                return;
            }

            w = get_weight_coins(gold?0:amount,gold?amount:0);

            if ((get_carry_weight(ch) + w) > can_carry_w(ch)) {
                act("$p: You can't carry that much weight.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                return;
            }

            if(gold) ch->gold += amount;
            else ch->silver += amount;

            *coin_ptr -= amount;

            sprintf(buffer,"%d %s coin%s", amount, gold?"gold":"silver", (amount==1)?"":"s");

            act("You take $T from $p.", ch, NULL, NULL, container, NULL, NULL, buffer, TO_CHAR, NULL, NULL);
            act("$n takes $T from $p.", ch, NULL, NULL, container, NULL, NULL, buffer, TO_ROOM, NULL, NULL);

            ret = p_percent_trigger(NULL, container, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL);

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
                    sprintf(buffer,"%d %d",gold?0:amount,gold?amount:0);
                    do_function(ch, &do_split, buffer);
                }
            }

            church_announce_theft(ch, NULL);

        } else {
            send_to_char("That isn't money.\n\r", ch);
        }

        return;
    }

    /* Get an obj off the ground */
    if (arg2[0] == '\0')
    {
        /* Get <obj> */
        if (str_cmp(arg1, "all") && str_prefix("all.", arg1)) {
            if ((obj = get_obj_list(ch, arg1, ch->in_room->contents)) == NULL ||
                !can_see_obj(ch, obj)) {
                act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg1, TO_CHAR, NULL, NULL);
                return;
            }

            if (!can_get_obj(ch, obj, NULL, NULL, false))
                return;

            act("You get $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n gets $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            reset_obj(obj);
            obj_from_room(obj);
            obj_to_char(obj, ch);

            if (IS_SET(obj->extra[1], ITEM_TRAPPED)) {
                act("{RYou pick up $p, but recoil in pain and drop it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("{R$n picks up $p, but recoils in pain and drops it!{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                damage(ch, ch, obj->trap_dam, 0, DAM_ENERGY, false);
                REMOVE_BIT(obj->extra[0], ITEM_TRAPPED);
                return;
            }

            p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL);

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
                                act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
                                act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                            }

                            send_to_char("Your hands are full.\n\r", ch);
                            found = false;
                        }

                        if (get_carry_weight(ch) + get_obj_weight(obj) > can_carry_w(ch)) {
                            if (i > 0 && match_obj != NULL) {
                                sprintf(buf, "{Y({G%2d{Y) {x$n gets $p.", i);
                                act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
                                act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                            }

                            send_to_char("You can't carry any more.\n\r", ch);
                            found = false;
                        }

                        if( obj != NULL ) {

                            obj_from_room(obj);

                            if( obj->item_type == ITEM_MONEY ) {
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
                            p_percent_trigger(NULL, match_obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL);

                            return;
                        }
                    }

                    if (i > 0 && match_obj != NULL) {
                        sprintf(buf, "{Y({G%2d{Y) {x$n gets $p.", i);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                        sprintf(buf, "{Y({G%2d{Y) {xYou get $p.", i);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                        p_percent_trigger(NULL, match_obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_GET, NULL);


                    } else if (!any) {
                        if (arg1[3] == '\0')
                            act("There is nothing here you can take.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        else
                            act("There is no $T here you can take.", ch, NULL, NULL, NULL, NULL, NULL, &arg1[4], TO_CHAR, NULL, NULL);
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
        act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR, NULL, NULL);
        return;
    }

    /* Get <obj> <container> */
    if (str_cmp(arg1, "all") && str_prefix("all.", arg1)) {
        if ((obj = get_obj_list(ch, arg1, container->contains)) == NULL) {
            act("I see nothing like that in the $T.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR, NULL, NULL);
            return;
        }

        if (!can_get_obj(ch, obj, container, NULL, false))
            return;

        if (container->item_type == ITEM_CORPSE_PC || container->item_type == ITEM_CORPSE_NPC)
            reset_obj(obj);

        act("You get $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gets $p from $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM, NULL, NULL);
        obj_from_obj(obj);
        obj_to_char(obj, ch);

        // If the container is in the current room
        if( container->in_room != NULL )
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
                            act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
                            act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                        }

                        send_to_char("Your hands are full.\n\r", ch);
                        return;
                    }

                    if (container->carried_by != ch && get_carry_weight(ch) + get_obj_weight(obj) >= can_carry_w(ch)) {
                        if (i > 0 && match_obj != NULL) {
                            sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", i);
                            act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
                            act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                        }

                        send_to_char("You can't carry any more.\n\r", ch);
                        return;
                    }

                    if( obj != NULL ) {

                        obj_from_obj(obj);

                        if( obj->item_type == ITEM_MONEY ) {
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
                        }

                        gotten = true;
                    }
                }

                if (i > 0 && match_obj != NULL) {
                    sprintf(buf, "{Y({G%2d{Y) {x$n gets $p from $P.", i);
                    act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                    sprintf(buf, "{Y({G%2d{Y) {xYou get $p from $P.", i);
                    act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                }


            } else if (!any) {
                if (arg1[3] == '\0')
                    act("There is nothing in $P.", ch, NULL, NULL, NULL, container, NULL, NULL, TO_CHAR, NULL, NULL);
                else
                    act("There is no $T in $p.", ch, NULL, NULL, container, NULL, NULL, &arg1[4], TO_CHAR, NULL, NULL);
            }
        }
            if ((new_gold > 0 || new_silver > 0) && obj_next == NULL){
                        give_money(ch, NULL, new_gold, new_silver, true);
                    }

            // If the container is in the current room and something was taken
            if( gotten && container->in_room != NULL ) {
                // Ignore player corpses
                if( container->item_type == ITEM_CORPSE_PC ) return;

                // Ignore mob corpses that have a timer
                //  - static mob corpses can exist, but they won't have a timer on them
                if( container->item_type == ITEM_CORPSE_NPC && container->timer > 0 ) return;

                church_announce_theft(ch, NULL);
            }
            if (!gotten)
            {
                sprintf(buf, "You were unable to take anything from %s.\n\r", container->short_descr);
                send_to_char(buf, ch);
            }
    }
}


/**
 * do_put - Place objects into containers
 *
 * Puts objects from inventory into a target container. Supports:
 * - put <item> <container>    : Put single item
 * - put all <container>       : Put all carried items
 * - put all.<type> <container>: Put all matching items
 * - put <N> gold/silver <container>: Put coins in container
 * - put all.gold/silver <container>: Put all coins
 *
 * Validates:
 * - Container weight capacity
 * - Container item count capacity (typed max_items field)
 * - CONT_PUT_ON flag for different messaging ("on" vs "in")
 * - CONT_CLOSED flag
 * - Special handling for furniture objects
 * - Bulk operations with grouped messaging
 *
 * @param ch        Character putting objects
 * @param argument  Item and container specification
 *
 * Triggers: TRIG_PUT (on container)
 *
 * Planned refactor: MOVED comment indicates intended move to object/object.c
 */
/**
 * Helper functions for do_put to handle container-like item types uniformly.
 * ITEM_CONTAINER, ITEM_CART, and ITEM_WEAPON_CONTAINER all support put
 * but have different type-specific data structs.
 */
static int put_container_max_weight(OBJ_DATA *obj)
{
    if (obj->item_type == ITEM_CONTAINER && CONTAINER(obj))
        return CONTAINER(obj)->max_weight;
    if (obj->item_type == ITEM_CART && CART(obj))
        return CART(obj)->capacity;
    if (obj->item_type == ITEM_WEAPON_CONTAINER && WEAPON_CON(obj))
        return WEAPON_CON(obj)->max_weight;
    return 0;
}

static int put_container_max_items(OBJ_DATA *obj)
{
    if (obj->item_type == ITEM_CONTAINER && CONTAINER(obj))
        return CONTAINER(obj)->max_items;
    if (obj->item_type == ITEM_CART && CART(obj))
        return CART(obj)->max_items;
    if (obj->item_type == ITEM_WEAPON_CONTAINER && WEAPON_CON(obj))
        return WEAPON_CON(obj)->max_items;
    return 0;
}

static int put_container_weight_mult(OBJ_DATA *obj)
{
    if (obj->item_type == ITEM_CONTAINER && CONTAINER(obj))
        return CONTAINER(obj)->weight_multiplier;
    if (obj->item_type == ITEM_CART && CART(obj))
        return CART(obj)->weight_multiplier;
    if (obj->item_type == ITEM_WEAPON_CONTAINER && WEAPON_CON(obj))
        return WEAPON_CON(obj)->weight_multiplier;
    return 100;
}

static bool put_container_has_flag(OBJ_DATA *obj, long flag)
{
    if (obj->item_type == ITEM_CONTAINER && CONTAINER(obj))
        return IS_SET(CONTAINER(obj)->flags, flag);
    if (obj->item_type == ITEM_CART && CART(obj))
        return IS_SET(CART(obj)->flags, flag);
    return false;
}

void do_put(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MSL];
    //char short_descr[MSL];
    OBJ_DATA *obj;
    OBJ_DATA *container;
    OBJ_DATA *match_obj;
    long amount;
    bool gold;
    int i = 0;
    bool found = true;
    //OBJ_DATA *any = NULL;

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
            act("You can't find a $T to put it in.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR, NULL, NULL);
            return;
        }

        if (container->item_type != ITEM_BANK)
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }

        sprintf(buf, "You put %ld %s coins in $p.", amount, gold ? "gold" : "silver");
        act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n puts some coins in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        act("You hear the sound of jingling coins from $p.",
            ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("You hear the sound of jingling coins from $p.",
            ch, NULL, NULL, container, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
        act(buf, ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        act("$n gets some coins from $p.", ch, NULL, NULL,
            container, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    if ((container = get_obj_inv(ch, arg2, false)) == NULL)
    {
        act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, arg2, TO_CHAR, NULL, NULL);
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

        /* Alemnos magic keyring */
        if (container->item_type == ITEM_KEYRING)
        {
            if (obj->item_type != ITEM_KEY)
            {
                act("You can only attach keys to $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                return;
            }
            else
            {
                OBJ_DATA *key;
                int i;

                if (IS_SET(obj->extra[0], ITEM_NOKEYRING))
                {
                    act("You try to attach $p to the keyring, but it recoils with a shock of energy.",
                        ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    act("$n tries to attach $p to $s keyring, but it recoils with a shock of energy.",
                        ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    return;
                }

                i = 0;
                for (key = container->contains; key != NULL; key = key->next_content)
                {
                    if (obj->pIndexData->vnum == key->pIndexData->vnum &&
                        obj->pIndexData->area == key->pIndexData->area)
                    {
                        act("$p is already on $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                        return;
                    }
                    i++;
                }

                if (i >= 50)
                {
                    act("$p is already rather full of keys!",
                        ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    return;
                }

                act("You attach $p to $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n attaches $p to $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                obj_from_char(obj);
                obj_to_obj(obj, container);
                return;
            }
        }

        /* Orb of Shadows makes 1 item perm cursed */
        if (container->pIndexData == get_reserved_obj_index("OBJ_VNUM_CURSED_ORB"))
        {
            if (obj->item_type == ITEM_CONTAINER)
            {
                act("You can't seem to get $p into the orb.",
                    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                return;
            }

            act("You put $p into the Orb of Shadows.",
                ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n puts $p into the Orb of Shadows.",
                ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You hear demonic chants and whispers from the Orb of Shadows.",
                ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("You hear demonic chants and whispers from $n's Orb of Shadows.",
                ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You retrieve $p from the Orb of Shadows.",
                ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n retrieves $p from the Orb of Shadows.",
                ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

            act("The Orb of Shadows dissipates into smoke.",
                ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n's Orb of Shadows dissipates into smoke.",
                ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

            SET_BIT(obj->extra[0], ITEM_NODROP);
            SET_BIT(obj->extra[0], ITEM_NOUNCURSE);
            extract_obj(container);
            return;
        }

        if (((get_obj_weight_container(container) + get_obj_weight(obj)) *
            put_container_weight_mult(container)/100) > put_container_max_weight(container))
        {
            act("$P cannot hold that much weight.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        if ((get_obj_number_container(container) + get_obj_number(obj)) > put_container_max_items(container))
        {
            act("$P is too full to hold $p.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        obj_from_char(obj);
        obj_to_obj(obj, container);

        if (put_container_has_flag(container, CONT_PUT_ON))
        {
            act("$n puts $p on $P.",ch, NULL, NULL,obj,container, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You put $p on $P.",ch, NULL, NULL,obj,container, NULL, NULL, TO_CHAR, NULL, NULL);
        }
        else
        {
            act("$n puts $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You put $p in $P.", ch, NULL, NULL, obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
        }

        p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,obj,NULL,TRIG_PUT, NULL);
    }
    else
    {
        /* Put all/all.<obj> <container> */
        if (container->item_type == ITEM_KEYRING ||
            container->pIndexData == get_reserved_obj_index("OBJ_VNUM_CURSED_ORB"))
        {
            act("You can only put items in $p one at a time.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        while (found)
        {
            found = false;
            i = 0;
            match_obj = NULL;
            OBJ_DATA *any = NULL;
            char short_descr[MSL];

            // First pass: find a matching object
            ITERATOR it;
            iterator_start(&it, ch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if (arg1[3] == '\0' || is_name(&arg1[4], obj->name))
                    any = obj;
                if (any == obj && can_put_obj(ch, obj, container, NULL, true)) {
                    snprintf(short_descr, sizeof(short_descr), "%s", obj->short_descr);
                    found = true;
                    break;
                }
            }
            iterator_stop(&it);

            if (found)
            {
                // Second pass: put all matching objects
                iterator_start(&it, ch->lcarrying);
                while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                    if (str_cmp(obj->short_descr, short_descr) ||
                        !can_put_obj(ch, obj, container, NULL, true))
                        continue;

                    if (((get_obj_weight_container(container) + get_obj_weight(obj)) *
                        put_container_weight_mult(container)/100) > put_container_max_weight(container))
                    {
                        if (i > 0 && match_obj != NULL)
                        {
                            if (put_container_has_flag(container, CONT_PUT_ON))
                            {
                                sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                            }
                            else
                            {
                                sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                            }
                        }

                        act("$p is full.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        iterator_stop(&it);
                        return;
                    }

                    if ((get_obj_number_container(container) + get_obj_number(obj)) > put_container_max_items(container))
                    {
                        if (i > 0 && match_obj != NULL)
                        {
                            if (put_container_has_flag(container, CONT_PUT_ON))
                            {
                                sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                            }
                            else
                            {
                                sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                                sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
                                act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                            }
                        }

                        act("$p can't hold any more.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        iterator_stop(&it);
                        return;
                    }

                    if (match_obj == NULL && obj != NULL)
                        match_obj = obj;

                    obj_from_char(obj);
                    obj_to_obj(obj, container);
                    i++;
                }
                iterator_stop(&it);

                if (i > 0 && match_obj != NULL)
                {
                    if (put_container_has_flag(container, CONT_PUT_ON))
                    {
                        sprintf(buf, "{Y({G%2d{Y) {x$n puts $p on $P.", i);
                        act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                        sprintf(buf, "{Y({G%2d{Y) {xYou put $p on $P.", i);
                        act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                    }
                    else
                    {
                        sprintf(buf, "{Y({G%2d{Y) {x$n puts $p in $P.", i);
                        act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_ROOM, NULL, NULL);

                        sprintf(buf, "{Y({G%2d{Y) {xYou put $p in $P.", i);
                        act(buf, ch, NULL, NULL, match_obj, container, NULL, NULL, TO_CHAR, NULL, NULL);
                    }
                }

                /* Too many to do individually, just let it handle all of them. */
                p_percent_trigger(NULL,container,NULL,NULL,ch, NULL, NULL,NULL,NULL,TRIG_PUT, NULL);
            }
            else if (!any)
            {
                if (arg1[3] == '\0')
                    act("You have nothing you can put in $p.", ch, NULL, NULL, container, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                else
                    act("You're not carrying any $T you can put in $p.", ch, NULL, NULL, container, NULL, NULL, &arg1[4], TO_CHAR, NULL, NULL);
            }
        }
    }
}


/**
 * do_drop - Drop objects from inventory to the ground
 *
 * Drops items from inventory to the current room. Supports:
 * - drop <item>       : Drop single item
 * - drop all          : Drop all carried items
 * - drop all.<type>   : Drop all matching items
 * - drop <N> gold/silver: Drop specific amount of coins
 * - drop all.gold/silver/coins: Drop all coins
 *
 * Special handling:
 * - Social areas block non-immortal drops
 * - Church drop boxes send items to church storage
 * - Pit objects (donations) set item timer to 0
 * - Duplicate items are grouped for consolidated messaging
 * - TRIG_DROP can intercept and cancel drops
 * - OBJ_NODROP items cannot be dropped by mortals
 *
 * @param ch        Character dropping objects
 * @param argument  Item specification and optional amount
 *
 * Triggers: TRIG_DROP, TRIG_DROPGET
 *
 * Planned refactor: MOVED comment indicates intended move to object/object.c
 */
void do_drop(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MSL];
    char buf[2*MAX_STRING_LENGTH];
    char short_descr[MSL];
    OBJ_DATA *obj;
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
    if ((!str_prefix("all.", arg)
        && (!str_cmp(arg+4, "gold") || !str_cmp(arg+4, "silver") || !str_cmp(arg+4, "coins")))
    ||  (is_number(arg) && (!str_cmp(arg2, "gold") || !str_cmp(arg2, "silver"))))
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
        act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n drops some coins.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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
            if(p_percent_trigger(NULL, ch->pulled_cart, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL))
                return;

            if (MOUNTED(ch))
            {
                act("You untie $p from your mount.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n unties $p from $s mount.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }
            else
            {
                act("You stop pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n stops pulling $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            }

            cart = ch->pulled_cart;
            ch->pulled_cart = NULL;
            cart->pulled_by = NULL;

            room = ch->in_room;

            /* If they try to stash a relic in housing it vanishes */
            if (is_relic(cart->pIndexData)
            &&  !str_cmp(ch->in_room->area->name, "Housing"))
            {
                ROOM_INDEX_DATA *room;

                act("$p vanishes in a mysterious purple haze.", ch, NULL, NULL, cart, NULL, NULL, NULL, TO_ALL, NULL, NULL);

                room = get_random_room(ch, 1);
                if (room == NULL)
                    room = get_reserved_room_index("room_default_recall");
            }

            obj_from_room(cart);
            obj_to_room(cart, room);

            if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
                sprintf(buf, "%s drops %s.", ch->name, cart->short_descr);
                plog(LOG_ADMIN, buf);
                wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
            }

            p_percent_trigger(NULL, cart, NULL, NULL, ch, NULL, NULL, cart, NULL, TRIG_DROP, NULL);
            p_give_trigger(NULL, NULL, ch->in_room, ch, cart, TRIG_DROP);

            return;
        }

        /* Awful hack to make it so people don't load up the perm-objs list too much. Will fix later. */
        ITERATOR it;
        iterator_start(&it, list_churches);
        while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
        {
            if (is_treasure_room(church, room))
            {
                if (ch->church == church)
                    send_to_char("Donations to the treasure room must be made using the church donate command.\n\r", ch);
                else
                    act("Only members of $t may donate to it.", ch, NULL, NULL, NULL, NULL, church->name, NULL, TO_CHAR, NULL, NULL);

                iterator_stop(&it);
                return;
            }
        }
        iterator_stop(&it);

        if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
        {
            send_to_char("You do not have that item.\n\r", ch);
            return;
        }

        if (obj->stached)
        {
            send_to_char("You can't drop key items.\n\r", ch);
            return;
        }

        if (!can_drop_obj(ch, obj, false) || IS_SET(obj->extra[1], ITEM_KEPT)) {
            send_to_char("You can't let go of it.\n\r", ch);
            return;
        }

        if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL))
            return;

        obj_from_char(obj);
        obj_to_room(obj, ch->in_room);
        act("$n drops $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
            sprintf(buf, "%s drops %s.", ch->name, obj->short_descr);
            plog(LOG_ADMIN, buf);
            wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
        }

        p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_DROP, NULL);
        p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_DROP);

        if (obj && IS_OBJ_STAT(obj,ITEM_MELT_DROP))
        {
            act("$p dissolves into smoke.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
            act("$p dissolves into smoke.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            extract_obj(obj);
        }
        else if (obj && (room_sector_has_flag(ch->in_room, SECTOR_CRUMBLES) ||
            room_in_sector(ch->in_room, SECT_ENCHANTED_FOREST)))
        {
            act("$p crumbles into dust.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("$p crumbles into dust.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            extract_obj(obj);
        }
    }
    else
    {
        /* Drop all/all.<obj> */
        ITERATOR it;
        iterator_start(&it, list_churches);
        while ((church = (CHURCH_DATA *)iterator_nextdata(&it)))
        {
            if (is_treasure_room(church, room))
            {
                if (ch->church == church)
                    send_to_char("Donations to the treasure room must be made using the church donate command.\n\r", ch);
                else
                    act("Only members of $t may donate to it.", ch, NULL, NULL, NULL, NULL, church->name, NULL, TO_CHAR, NULL, NULL);

                iterator_stop(&it);
                return;
            }
        }
        iterator_stop(&it);

        while (found)
        {
            found = false;
            i = 0;
            match_obj = NULL;
            //OBJ_DATA *obj_next = NULL;

            // First pass: find a matching object
            iterator_start(&it, ch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if (arg[3] == '\0' || is_name(&arg[4], obj->name))
                    any = obj;
                if (any == obj && can_drop_obj(ch, obj, true) && !IS_SET(obj->extra[1], ITEM_KEPT) &&
                    !p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, "silent"))
                {
                    snprintf(short_descr, sizeof(short_descr), "%s", obj->short_descr);
                    found = true;
                    break;
                }
            }
            iterator_stop(&it);

            if (found)
            {
                iterator_start(&it, ch->lcarrying);
                while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                    if (str_cmp(obj->short_descr, short_descr)
                        ||  !can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
                        continue;

                    if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDROP, NULL))
                        continue;

                    if (match_obj == NULL && obj != NULL)
                        match_obj = obj;

                    obj_from_char(obj);
                    obj_to_room(obj, ch->in_room);
                    i++;

                    if (IS_IMMORTAL(ch) && !IS_NPC(ch)) {
                        sprintf(buf, "%s drops %s.", ch->name, obj->short_descr);
                        plog(LOG_ADMIN, buf);
                        wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
                    }

                    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_DROP, NULL);
                    p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_DROP);

                    if (IS_SET(obj->extra[0], ITEM_MELT_DROP))
                        extract_obj(obj);

                    else if (room_sector_has_flag(ch->in_room, SECTOR_CRUMBLES) ||
                        room_in_sector(ch->in_room, SECT_ENCHANTED_FOREST))
                        extract_obj(obj);
                }
                iterator_stop(&it);

                if (i > 0 && match_obj != NULL)
                {
                    sprintf(buf, "{Y({G%2d{Y) {x$n drops $p.", i);
                    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    sprintf(buf, "{Y({G%2d{Y) {xYou drop $p.", i);
                    act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                    if (IS_SET(match_obj->extra[0], ITEM_MELT_DROP))
                    {
                        short_descr[0] = UPPER(short_descr[0]);
                        sprintf(buf, "{Y({G%2d{Y) {x%s vanishes in a puff of smoke.", i, short_descr);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    }
                    else if (room_sector_has_flag(ch->in_room, SECTOR_CRUMBLES) ||
                        room_in_sector(ch->in_room, SECT_ENCHANTED_FOREST))
                    {
                        short_descr[0] = UPPER(short_descr[0]);
                        sprintf(buf, "{Y({G%2d{Y) {x%s crumbles into dust.", i, short_descr);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                        act(buf, ch, NULL, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                    }
                }
            }
            else if (!any)
            {
                if (arg[3] == '\0')
                    act("You aren't carrying anything you can drop.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                else
                    act("You're not carrying any $T.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR, NULL, NULL);
            }
        }
    }
}


/**
 * do_give - Give objects or money to another character
 *
 * Transfers items from inventory to another character. Supports:
 * - give <item> <victim>      : Give single item
 * - give all <victim>         : Give all carried items
 * - give all.<type> <victim>  : Give all matching items
 * - give <N> gold/silver <victim>: Give coins
 * - give all.gold/silver <victim>: Give all coins
 *
 * Validates:
 * - Target's carry weight capacity (exempts changers/bankers)
 * - Target's carry count capacity
 * - ITEM_NODROP flag prevents giving for mortals
 * - Social area restrictions for non-immortals
 * - Bulk operations use grouped messaging
 *
 * Money is transferred directly between character fields rather than
 * creating an object. The temporary money object is extracted.
 *
 * @param ch        Character giving objects
 * @param argument  Item and target specification
 *
 * Triggers: TRIG_GIVE (on object and victim)
 *
 * Planned refactor: MOVED comment indicates intended move to object/object.c
 */
void do_give(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char short_descr[MSL];
    CHAR_DATA *victim;
    OBJ_DATA *obj;
    OBJ_DATA *match_obj;
    int i = 0;
    bool found = true;
    //OBJ_DATA *any = NULL;
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
        || !str_cmp(arg1, "all.gold")
        || !str_cmp(arg1, "all.silver"))
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
            act("$p: $N can't carry that much weight.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        act("$n gives $N some coins.",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        sprintf(buf,"You give $N %ld %s.",amount, gold ? "gold" : "silver");
        act(buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        /* Bribe trigger */
        p_bribe_trigger(victim, ch, !gold ? amount : amount * 100);

        if (IS_NPC(victim) && IS_SET(victim->act[0],ACT_IS_CHANGER))
            change_money(ch, victim, gold ? amount : 0, !gold ? amount : 0);

        if (IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_IMMORTAL(victim))
        {
            sprintf(buf,"%s gives %s %ld %s.",
                ch->name, IS_NPC(victim) ? victim->short_descr : victim->name,
                amount, gold ? "gold" : "silver");
            plog(LOG_ADMIN, buf);
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
        act("$N is dead. You can't give $M anything.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            OBJ_DATA *any = NULL;

            // First pass: find a matching object
            ITERATOR it;
            iterator_start(&it, ch->lcarrying);
            while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                if (is_name(arg1, obj->name))
                    any = obj;
                if (any == obj && can_give_obj(ch, obj, victim, true)) {
                    snprintf(short_descr, sizeof(short_descr), "%s", obj->short_descr);
                    found = true;
                    break;
                }
            }
            iterator_stop(&it);

            if (found)
            {
                iterator_start(&it, ch->lcarrying);
                while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
                    if (str_cmp(obj->short_descr, short_descr)
                        ||  !can_give_obj(ch, obj, victim, true))
                        continue;

                    if (victim->carry_number + get_obj_number(obj) > can_carry_n(victim))
                    {
                        if (i > 0 && match_obj != NULL)
                        {
                            sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        }

                        act("$N has $S hands full.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        found = false;
                        break;
                    }

                    if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim))
                    {
                        if (i > 0 && match_obj != NULL)
                        {
                            sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);

                            sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
                            act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        }

                        act("$N can't carry any more weight.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        found = false;
                        break;
                    }

                    if (match_obj == NULL && obj != NULL)
                        match_obj = obj;

                    reset_obj(obj);
                    obj_from_char(obj);
                    obj_to_char(obj, victim);
                    i++;

                    p_give_trigger(NULL, obj, NULL, ch, obj, TRIG_GIVE);
                    p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_GIVE);
                    p_give_trigger(victim, NULL, NULL, ch, obj, TRIG_GIVE);

                    if (!found)
                        break;
                }
                iterator_stop(&it);

                if (i > 0 && match_obj != NULL)
                {
                    sprintf(buf, "{Y({G%2d{Y) {x$n gives $p to $N.", i);
                    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);

                    sprintf(buf, "{Y({G%2d{Y) {x$n gives you $p.", i);
                    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);

                    sprintf(buf, "{Y({G%2d{Y) {xYou give $p to $N.", i);
                    act(buf, ch, victim, NULL, match_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                }
            }
            else if (!any)
                act("You aren't carrying any $t which you can give $M.", ch, victim, NULL, NULL, NULL, arg1, NULL, TO_CHAR, NULL, NULL);
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
        act("$N has $S hands full.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (get_carry_weight(victim) + get_obj_weight(obj) > can_carry_w(victim))
    {
        act("$N can't carry that much weight.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    obj_from_char(obj);
    obj_to_char(obj, victim);
    MOBtrigger = false;
    act("$n gives $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    act("$n gives you $p.",   ch, victim, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL   );
    act("You give $p to $N.", ch, victim, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL   );
    MOBtrigger = true;

    if (IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_IMMORTAL(victim))
    {
        sprintf(buf, "%s gives %s to %s.",
            ch->name,
            obj->short_descr,
            IS_NPC(victim) ? victim->short_descr : victim->name);
        plog(LOG_ADMIN, buf);
        wiznet(buf, NULL, NULL, WIZ_IMMLOG, 0, 0);
    }

    /* Give trigger */
    p_give_trigger(NULL, obj, NULL, ch, obj, TRIG_GIVE);
    p_give_trigger(NULL, NULL, ch->in_room, ch, obj, TRIG_GIVE);
    p_give_trigger(victim, NULL, NULL, ch, obj, TRIG_GIVE);
}


/**
 * change_money - Exchange gold for silver or vice versa
 *
 * Handles currency conversion between gold and silver with a 5% fee.
 * Conversion rates: 1 gold = 100 silver, so 1 gold becomes 95 silver
 * and 10000 silver becomes 95 gold after fees.
 *
 * If the changer NPC doesn't have enough currency, their funds are
 * temporarily boosted to complete the transaction.
 *
 * @param ch       Character requesting the exchange
 * @param changer  NPC money changer performing exchange
 * @param gold     Amount of gold being exchanged (0 if exchanging silver)
 * @param silver   Amount of silver being exchanged (0 if exchanging gold)
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
void change_money(CHAR_DATA *ch, CHAR_DATA *changer, long gold, long silver)
{
    char buf[MSL];
    long change;

    if (gold > 0)
    change = 95 * gold;
    else
    change = (95 * silver)/10000;

    if (change < 1)
    act("{R$n tells you 'I'm sorry, you did not give me enough to change.{x'", changer, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    else
    {
        if (gold && changer->silver < change)
            changer->silver = change;

        if (silver && changer->gold < change)
            changer->gold = change;

    sprintf(buf,"%ld %s %s", change, gold ? "silver" : "gold", ch->name);
    do_function(changer, &do_give, buf);

    act("{R$n tells you 'Thank you, come again.{x'", changer, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }
}

/**
 * do_donate - Teleport an item to the donation room
 *
 * Instantly transfers an item from inventory to the designated donation room
 * without requiring the character to physically travel there.
 *
 * Restrictions:
 * - Cannot donate during combat
 * - Cannot donate NODROP or KEPT items
 * - Cannot donate NO_DONATE flagged items
 * - Cannot donate corpses, owned items, MELT_DROP items, or timed items
 * - Containers must be empty
 * - Cannot donate while in the donation room itself
 *
 * Sets item cost to 0 upon donation.
 * Notifies everyone in the donation room of the arrival.
 *
 * @param ch        Character donating
 * @param argument  Item to donate
 *
 * Planned refactor: MOVED comment indicates intended move to object/donate.c
 */
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
    act("You can't donate $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        act("You must empty $p first.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    if (ch->in_room == get_reserved_room_index("room_donation"))
    {
    send_to_char("You're already here, just drop it.\n\r",ch);
    return;
    }

    act("$n donates $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
    act("You donate $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);

    obj->cost = 0;

    obj_from_char(obj);
    obj_to_room(obj, get_reserved_room_index("room_donation"));

    for (prev = obj->in_room->people; prev; prev = prev->next_in_room)
    send_to_char("{MYou hear a loud zap as an object drops from the shimmering rift onto the rug.{x\n\r", prev);
}


/**
 * do_repair - Repair damaged equipment
 *
 * Two repair methods supported:
 * 1. Self-repair using skill_resolve_gsn("repair") skill (if character has it)
 *    - Repairs condition based on skill check
 *    - Uses movement points as cost
 *    - Full success restores 100% condition
 *
 * 2. NPC blacksmith repair (if smithy mob in room)
 *    - Cost based on item value and damage amount
 *    - "repair all" repairs everything equipped
 *    - Skill-based repair takes time (interruptible)
 *
 * @param ch        Character repairing
 * @param argument  Item to repair or "all"
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
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
    if ((sk = get_skill(ch, skill_resolve_gsn("repair"))) > 0)
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
        act("$p is already in perfect condition.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (obj->timer > 0)
    {
        act("$p is too badly damaged and will crumble any second.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (obj->times_fixed >= obj->times_allowed_fixed)
    {
           act("$p is beyond repair.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    act("{YYou begin to repair $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{Y$n begins to repair to $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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
        act("{C$n says 'I'm sorry $N, that item is beyond repair.'{x", pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        return;
    }

    if (obj->condition >= 100)
    {
        act("{C$n says 'There would be no point in repairing that item. It's in good condition.'{x",
            pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        return;
    }

    cost = obj->level * 100;
    if ((ch->gold * 100 + ch->silver) < cost)
    {
        sprintf(buf, "{C$n says '$N, you will need %d silver for me to repair that item.'{x", obj->level * 100);
        act(buf, pMob, ch, NULL, NULL, NULL, NULL, NULL, TO_ALL, NULL, NULL);
        return;
    }

    act("$n gives $p to $N.", ch, pMob, NULL, obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    act("$n gives you $p.",   ch, pMob, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL   );
    act("You give $p to $N.", ch, pMob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL   );

    act("$n tinkers with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    act("$n tinkers with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL   );
    act("You tinker with $p and gives it back to $N.", pMob, ch, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL   );

    act("$n pockets some coins.", pMob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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


/**
 * do_restring - Change item's name/description at a restringer NPC
 *
 * Allows players to customize item descriptions for a cost.
 * Preserves original values so items can be unrestringed later.
 *
 * Options:
 * - restring <item> short <text> : Change short description (what you see)
 * - restring <item> long <text>  : Change long description (ground view)
 * - restring <item> desc         : Enter editor for full description
 *
 * Cost: 99 + max(1, item_cost/1000 + level/10) silver
 *
 * Restrictions:
 * - Requires ACT_IS_RESTRINGER mob in room
 * - NORESTRING items only allow color changes to short desc
 * - Tabard items cannot be restringed
 * - Minimum 5 character length for names
 *
 * @param ch        Character restringing
 * @param argument  Item, field, and new value
 *
 * Planned refactor: MOVED comment indicates intended move to object/desc.c
 */
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
        send_to_char("Syntax: restring item name  <new name>\n\r", ch);
        send_to_char("        restring item short <new name>\n\r", ch);
        send_to_char("        restring item long  <new name>\n\r", ch);
        send_to_char("        restring item desc  (for the description)\n\r", ch);
        return;
    }

    if (str_cmp(arg2, "desc") && strlen_no_colours(argument) < 5)
    {
        act("{R$N tells you, 'Surely you can think of a better name than that! It's too short!'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if ((obj = get_obj_inv_only(ch, arg1, true)) == NULL)
    {
        act("{R$N tells you, 'You don't have that item.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (IS_SET(obj->extra[0], ITEM_NORESTRING) || CAN_WEAR(obj, ITEM_WEAR_TABARD))
    {
        // Allow color changes to SHORTS on NORESTRING.
        if(str_cmp(arg2, "short") || str_cmp_nocolour(obj->short_descr, argument)) {
            act("{R$N tells you, 'Sorry, but you can't restring $p.'{x", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return;
        }

        norestring = true;
    }

    cost = 99 + UMAX(1, obj->pIndexData->cost/ 1000 + obj->level / 10);
    if ((ch->gold * 100 + ch->silver) < cost)
    {
        sprintf(buf, "{R$N tells you, 'You don't have enough money. It would cost %ld silver coins to restring it.'{x", cost);
         act (buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (!str_cmp(arg2, "long"))
    {
        if (obj->old_description == NULL)
            obj->old_description = obj->description;
        else
            free_string(obj->description);	// The object has already been restrung

        act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        strcat(argument, "{x");

        obj->description = str_dup(argument);
        act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        sprintf(buf, "The long description has been changed to %s\n\r", obj->description);
        send_to_char(buf, ch);

        do_say(mob, "Nice doin' business with ya bub.");

        deduct_cost(ch, cost);
        return;
    }

    if (!str_cmp("short", arg2))
    {
        act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        if (obj->old_short_descr == NULL)
            obj->old_short_descr = obj->short_descr;
        else
            free_string(obj->short_descr);	// The object has already been restrung

        strcat(argument, "{x");

        obj->short_descr = str_dup(argument);
        act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        sprintf(buf, "The short description has been changed to %s\n\r",
            obj->short_descr);
        send_to_char(buf, ch);

        // NORESTRING objects PRESERVE the name.
        if (!norestring) {
            char *keywords = NULL;
            char *invalid = NULL;
            LOCALIZATION_ERROR err = localization_short_to_keywords(obj->short_descr, &keywords, &invalid);
            if (err == LOC_OK)
            {
                if (IS_NULLSTR(keywords))
                {
                    send_to_char("{RNone of the short description could be applied to the name.{x\n\r"
                                 "Please use {Yrestring item name  <new name>{x to set the name.\n\r",
                                 ch);
                    if (keywords) free(keywords); // Since it can't be used
                }
                else
                {
                    if (obj->old_name == NULL)
                        obj->old_name = obj->name;
                    else
                        free_string(obj->name);	// The object has already been restrung

                    obj->name = keywords;

                    // If there were any invalid words, tell the player, in case they want to redo the whole name field
                    if (!IS_NULLSTR(invalid))
                    {
                        sprintf(buf, "{RCould not apply the following words from the short to the name:{x\n\r{W%s{x\n\r",
                            invalid);
                        send_to_char(buf, ch);
                    }

                }

            } else {
                send_to_char("{RNone of the short description could be applied to the name.{x\n\r"
                                "Please use {Yrestring item name  <new name>{x to set the name.\n\r",
                                ch);
                if (keywords) free(keywords); // Since it can't be used
            }
            if (invalid) free(invalid);
        }

        do_say(mob, "Nice doin' business with ya bub.");

        deduct_cost(ch, cost);
        return;
    }

    if (!str_cmp("name", arg2))
    {
        act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        char *temp_str = nocolour(argument);
        if (str_cmp(argument, temp_str))
        {
            free_string(temp_str);
            act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

            do_say(mob, "Don' be flashy in da name, bub.");
            send_to_char("{RPlease don't use colors in names.{x\n\r", ch);
            return;
        }
        free_string(temp_str);

        size_t bad_position;
        LOCALIZATION_ERROR err = localization_validate_string(argument, &bad_position, NULL);
        switch(err)
        {
        case LOC_OK:
            {
                act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                if (obj->old_name == NULL)
                    obj->old_name = obj->name;
                else
                    free_string(obj->name);	// The object has already been restrung

                obj->name = str_dup(argument);

                sprintf(buf, "The name has been changed to {G%s{x.\n\r", obj->name);
                send_to_char(buf, ch);

                do_say(mob, "Nice doin' business with ya bub.");

                deduct_cost(ch, cost);
                break;
            }

        case LOC_ERR_FORBIDDEN_CODEPOINT:
            {
                act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                do_say(mob, "Somethin' wrong with wha ya want, bub.");
                send_to_char(formatf("{RForbidden character at position {W%ld{R.{x\n\r", bad_position), ch);
                break;
            }

        case LOC_ERR_INVALID_UTF8:
            {
                act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                do_say(mob, "Somethin' wrong with wha ya want, bub.");
                send_to_char(formatf("{RErroneous character at position {W%ld{R.{x\n\r", bad_position), ch);
                break;
            }

        default:
            {
                act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                do_say(mob, "Somethin' wrong with wha ya want, bub.");
                send_to_char("{RError logged for investigation.  Thank you for your patience.{x\n\r", ch);
                pbugf(LOG_ERROR, "Encountered error '%s' when restringing name with argument '%s'", localization_error_string(err), argument);
                break;
            }
        }
        return;
    }

    if (!str_cmp(arg2, "desc"))
    {
        act("You give $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n gives $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("$n spins a 360 on $s heel.", mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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


/**
 * do_unrestring - Restore item's original description
 *
 * Reverts a restringed item back to its original values.
 * Requires ACT_IS_RESTRINGER mob in room.
 * Cost: 500 silver
 *
 * Restores:
 * - old_name -> name
 * - old_short_descr -> short_descr
 * - old_description -> description
 * - old_full_description -> full_description
 *
 * @param ch        Character unrestringing
 * @param argument  Item to restore
 *
 * Planned refactor: MOVED comment indicates intended move to object/desc.c
 */
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
            ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if ((obj = get_obj_inv_only(ch, arg, true)) == NULL)
    {
        act("{R$N tells you, 'You don't have that item.'{x", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    //if (obj->item_type == ITEM_SCROLL || obj->item_type == ITEM_POTION) {
    //	act("You cannot unrestring $p without destroying it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
    //	return;
    //}

    act("You hand $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n hands $p to $N.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("$N tinkers with $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$N tinkers with $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

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

    act("$N gives you $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$N gives $n $p.", ch, mob, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
}


/**
 * do_envenom - Apply poison to food, drink, or weapons
 *
 * Uses skill_resolve_gsn("envenom") skill to poison consumables or weapons.
 *
 * Food/Drink: Sets typed poison flag to 1. Blessed or burn-proof
 * items are immune.
 *
 * Weapons: Applies temporary poison weapon affect with damage based
 * on skill level. Duration scales with skill (level/2 + 1 hours).
 * Weapon must be bladed type (sword, dagger, axe, polearm).
 *
 * Higher skill = lower chance of being seen when poisoning.
 * Improves skill_resolve_gsn("envenom") on success/failure.
 *
 * @param ch        Character envenoming
 * @param argument  Item to poison
 *
 * Planned refactor: MOVED comment indicates intended move to combat/hidden.c
 */
void do_envenom(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    AFFECT_DATA af;
    int percent,skill;

    if (get_skill(ch, skill_resolve_gsn("envenom")) == 0)
    {
    send_to_char("What?\n\r", ch);
    return;
    }

    if (argument[0] == '\0')
    {
    send_to_char("Envenom what item?\n\r",ch);
    return;
    }

    obj =  get_obj_list(ch,argument,ch->lcarrying);

    if (obj== NULL)
    {
    send_to_char("You don't have that item.\n\r",ch);
    return;
    }

    if ((skill = get_skill(ch,skill_resolve_gsn("envenom"))) < 1)
    {
    send_to_char("Are you crazy? You'd poison yourself!\n\r",ch);
    return;
    }

    if (obj->item_type == ITEM_FOOD || obj->item_type == ITEM_DRINK_CON)
    {
    if (IS_OBJ_STAT(obj,ITEM_BLESS) || IS_OBJ_STAT(obj,ITEM_BURN_PROOF))
    {
        act("You fail to poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        return;
    }

    if (number_percent() < skill)  /* success! */
    {
        /* The better you get, the less likely people SEE it
              But, even mastered, there is a slight chance of people seeing */
        if(number_range(0,100) > skill)
        act("$n treats $p with deadly poison.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        act("You treat $p with deadly poison.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        int is_poisoned = (obj->item_type == ITEM_FOOD) ? FOOD(obj)->poison : FLUID_CON(obj)->poison;
        if (!is_poisoned)
        {
        if (obj->item_type == ITEM_FOOD)
            FOOD(obj)->poison = 1;
        else
            FLUID_CON(obj)->poison = 1;
        check_improve(ch,skill_resolve_gsn("envenom"),true,4);
        }
        WAIT_STATE(ch,skill_table[skill_resolve_gsn("envenom")].beats);
        return;
    }

    act("You fail to poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
    if (!((obj->item_type == ITEM_FOOD) ? FOOD(obj)->poison : FLUID_CON(obj)->poison))
        check_improve(ch,skill_resolve_gsn("envenom"),false,4);
    WAIT_STATE(ch,skill_table[skill_resolve_gsn("envenom")].beats);
    return;
     }

memset(&af,0,sizeof(af));
    if (obj->item_type == ITEM_WEAPON)
    {
        if (IS_WEAPON_STAT(obj,WEAPON_FLAMING)
        ||  IS_WEAPON_STAT(obj,WEAPON_FROST)
        ||  IS_WEAPON_STAT(obj,WEAPON_VAMPIRIC)
/*        ||  IS_WEAPON_STAT(obj,WEAPON_SHARP)	 Why??  Makes no sense. */
        ||  IS_WEAPON_STAT(obj,WEAPON_VORPAL)
        ||  IS_WEAPON_STAT(obj,WEAPON_SHOCKING)
        ||  IS_WEAPON_STAT(obj,WEAPON_ACIDIC)
        ||  IS_WEAPON_STAT(obj,WEAPON_RESONATE)
        ||  IS_WEAPON_STAT(obj,WEAPON_BLAZE)
        ||  IS_WEAPON_STAT(obj,WEAPON_SUCKLE)
        ||  IS_OBJ_STAT(obj,ITEM_BLESS) || IS_OBJ_STAT(obj,ITEM_BURN_PROOF))
        {
            act("You can't seem to envenom $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return;
        }

    if (WEAPON(obj)->damage_type < 0
    ||  attack_table[WEAPON(obj)->damage_type].damage == DAM_BASH)
    {
        send_to_char("You can only envenom edged weapons.\n\r",ch);
        return;
    }

        if (IS_WEAPON_STAT(obj,WEAPON_POISON))
        {
            act("$p is already envenomed.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return;
        }

    percent = number_percent();
    if (percent < skill)
    {

            af.where     = TO_WEAPON;
            af.group     = AFFGROUP_WEAPON;
            af.type      = skill_resolve_gsn("poison");
    af.skill = skill_find_uid(af.type);
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
        act("$n coats $p with deadly venom.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        act("You coat $p with venom.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("envenom"),true,3);
        WAIT_STATE(ch,skill_table[skill_resolve_gsn("envenom")].beats);
            return;
        }
    else
    {
        act("You fail to envenom $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("envenom"),false,3);
        WAIT_STATE(ch,skill_table[skill_resolve_gsn("envenom")].beats);
        return;
    }
    }

    act("You can't poison $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
}


/**
 * do_fill - Fill a drink container from a fountain
 *
 * Fills a drink container to capacity from a fountain in the room.
 * Sets the liquid type to match the fountain's contents.
 *
 * Validations:
 * - Must be holding a ITEM_DRINK_CON
 * - Fountain (ITEM_FOUNTAIN) must be in room
 * - Container must be empty or contain same liquid type
 * - Container must not already be full
 *
 * @param ch        Character filling container
 * @param argument  Container to fill
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_fill(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
//    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *fountain;
    bool found;

    one_argument(argument, arg);

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

    found = false;
    for (fountain = ch->in_room->contents; fountain != NULL;
    fountain = fountain->next_content)
    {
    if (fountain->item_type == ITEM_FOUNTAIN)
    {
        found = true;
        break;
    }
    }

    if (!found)
    {
    send_to_char("There is no fountain here!\n\r", ch);
    return;
    }

    if (obj->item_type != ITEM_DRINK_CON)
    {
    send_to_char("You can't fill that.\n\r", ch);
    return;
    }

    if (FLUID_CON(obj)->amount != 0 && FLUID_CON(obj)->liquid != FLUID_CON(fountain)->liquid)
    {
    send_to_char("There is already another liquid in it.\n\r", ch);
    return;
    }

    if (FLUID_CON(obj)->amount >= FLUID_CON(obj)->capacity)
    {
    send_to_char("Your container is full.\n\r", ch);
    return;
    }

    act("You fill $p with $t from $P.", ch, NULL, NULL, obj,fountain, liquid_name(FLUID_CON(fountain)->liquid), NULL, TO_CHAR, NULL, NULL);
    act("$n fills $p with $t from $P.", ch, NULL, NULL, obj,fountain, liquid_name(FLUID_CON(fountain)->liquid), NULL, TO_ROOM, NULL, NULL);
    FLUID_CON(obj)->liquid = FLUID_CON(fountain)->liquid;
    FLUID_CON(obj)->amount = FLUID_CON(obj)->capacity;
}


/**
 * do_pour - Pour liquid between containers or onto ground
 *
 * Transfers liquid from one drink container to another, or empties
 * a container onto the ground.
 *
 * Syntaxes:
 * - pour <container> out        : Empty container onto ground
 * - pour <container> <container>: Transfer to another container
 * - pour <container> <person>   : Fill what they're holding
 *
 * Validates liquid compatibility between containers.
 * Clears typed poison flag when emptying.
 *
 * @param ch        Character pouring
 * @param argument  Source container and destination
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
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

    if (out->item_type != ITEM_DRINK_CON)
    {
    send_to_char("That's not a drink container.\n\r",ch);
    return;
    }

    if (!str_cmp(argument,"out"))
    {
    if (FLUID_CON(out)->amount == 0)
    {
        send_to_char("It's already empty.\n\r",ch);
        return;
    }

    FLUID_CON(out)->amount = 0;
    FLUID_CON(out)->poison = 0;
    sprintf(buf,"You invert $p, spilling %s all over the ground.", liquid_name(FLUID_CON(out)->liquid));
    act(buf,ch, NULL, NULL,out, NULL, NULL,NULL,TO_CHAR, NULL, NULL);

    sprintf(buf,"$n inverts $p, spilling %s all over the ground.", liquid_name(FLUID_CON(out)->liquid));
    act(buf,ch, NULL, NULL,out, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
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

    if (in->item_type != ITEM_DRINK_CON)
    {
    send_to_char("You can only pour into other drink containers.\n\r",ch);
    return;
    }

    if (in == out)
    {
    send_to_char("You cannot change the laws of physics!\n\r",ch);
    return;
    }

    if (FLUID_CON(in)->amount != 0 && FLUID_CON(in)->liquid != FLUID_CON(out)->liquid)
    {
    send_to_char("They don't hold the same liquid.\n\r",ch);
    return;
    }

    if (FLUID_CON(out)->amount == 0)
    {
    act("There's nothing in $p to pour.",ch, NULL, NULL,out, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
    return;
    }

    if (FLUID_CON(in)->amount >= FLUID_CON(in)->capacity)
    {
    act("$p is already filled to the top.",ch, NULL, NULL,in, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
    return;
    }

    amount = UMIN(FLUID_CON(out)->amount,FLUID_CON(in)->capacity - FLUID_CON(in)->amount);

    FLUID_CON(in)->amount += amount;
    FLUID_CON(out)->amount -= amount;
    FLUID_CON(in)->liquid = FLUID_CON(out)->liquid;

    if (vch == NULL)
    {
        sprintf(buf,"You pour %s from $p into $P.", liquid_name(FLUID_CON(out)->liquid));
        act(buf,ch, NULL, NULL,out,in, NULL, NULL,TO_CHAR, NULL, NULL);
        sprintf(buf,"$n pours %s from $p into $P.", liquid_name(FLUID_CON(out)->liquid));
        act(buf,ch, NULL, NULL,out,in, NULL, NULL,TO_ROOM, NULL, NULL);
    }
    else
    {
        sprintf(buf,"You pour some %s for $N.", liquid_name(FLUID_CON(out)->liquid));
        act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);
        sprintf(buf,"$n pours you some %s.", liquid_name(FLUID_CON(out)->liquid));
        act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
        sprintf(buf,"$n pours some %s for $N.", liquid_name(FLUID_CON(out)->liquid));
        act(buf,ch,vch, NULL, NULL, NULL, NULL, NULL,TO_NOTVICT, NULL, NULL);
    }
}


/**
 * do_drink - Consume liquid from container or fountain
 *
 * Drinks from fountains or drink containers, affecting character
 * conditions (drunk, full, thirst, hunger) based on liquid type.
 *
 * Special handling:
 * - Very drunk characters fail to reach their mouth
 * - Vampires get special treatment from blood (liquid 14)
 * - Poisoned drinks (FLUID_CON(obj)->poison != 0) apply poison affect
 * - Fountains have infinite capacity
 * - Social status check prevents drinking in certain areas
 *
 * @param ch        Character drinking
 * @param argument  Container to drink from (optional, uses fountain if empty)
 *
 * Triggers: TRIG_DRINK
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_drink(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int amount;
    int liquid;

    one_argument(argument, arg);

    if (check_social_status(ch))
        return;

    if (arg[0] == '\0')
    {
    for (obj = ch->in_room->contents; obj; obj = obj->next_content)
    {
        if (obj->item_type == ITEM_FOUNTAIN)
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

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK] > 10)
    {
    send_to_char("You fail to reach your mouth.  *Hic*\n\r", ch);
    return;
    }

    switch (obj->item_type)
    {
    default:
        send_to_char("You can't drink from that.\n\r", ch);
        return;

    case ITEM_FOUNTAIN:
        if ((liquid = FLUID_CON(obj)->liquid)  < 0)
        {
        pbugf(LOG_ERROR, "Bad liquid number %d.", liquid);
        liquid = FLUID_CON(obj)->liquid = 0;
        }
        if (race_get_trait_bool(ch->race, "blood_feeding"))
           amount = 20;
        else
           amount = liquid_affect(liquid, COND_FULL) * 10;
        break;

    case ITEM_DRINK_CON:
        if (FLUID_CON(obj)->amount <= 0)
        {
        send_to_char("It is already empty.\n\r", ch);
        return;
        }

        if ((liquid = FLUID_CON(obj)->liquid)  < 0)
        {
        pbugf(LOG_ERROR, "Bad liquid number %d.", liquid);
        liquid = FLUID_CON(obj)->liquid = 0;
        }

        amount = liquid_affect(liquid, LIQ_AFF_SSIZE);
        amount = UMIN(amount, FLUID_CON(obj)->amount);
        break;
     }

    act("$n drinks $T from $p.",ch, NULL, NULL, obj, NULL, NULL, liquid_name(liquid), TO_ROOM, NULL, NULL);
    act("You drink $T from $p.",ch, NULL, NULL, obj, NULL, NULL, liquid_name(liquid), TO_CHAR, NULL, NULL);

    if (race_get_trait_bool(ch->race, "blood_feeding") && FLUID_CON(obj)->liquid == 14)
    {
        send_to_char("You feel refreshed.\n\r", ch);
      gain_condition(ch, COND_FULL,
    amount * 1 / 2);
      gain_condition(ch, COND_THIRST,
    amount * 1 / 2);
      gain_condition(ch, COND_HUNGER,
    amount * 1 / 2);
    }
    else
    {
    gain_condition(ch, COND_DRUNK, amount * liquid_affect(liquid, COND_DRUNK) / 36);
    gain_condition(ch, COND_FULL, amount * liquid_affect(liquid, COND_FULL) / 4);
    gain_condition(ch, COND_THIRST, amount * liquid_affect(liquid, COND_THIRST) / 2);
    gain_condition(ch, COND_HUNGER, amount * liquid_affect(liquid, COND_HUNGER) / 2);
    }

    if (!IS_NPC(ch) && ch->pcdata->condition[COND_DRUNK]  > 10)
    send_to_char("You feel drunk.\n\r", ch);
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_FULL]   > 40)
    send_to_char("You are full.\n\r", ch);
    if (!IS_NPC(ch) && ch->pcdata->condition[COND_THIRST] > 40)
    send_to_char("Your thirst is quenched.\n\r", ch);

    if (FLUID_CON(obj)->poison != 0
    && check_immune(ch, DAM_POISON) != IS_IMMUNE)
    {
    /* The drink was poisoned ! */
    AFFECT_DATA af;
memset(&af,0,sizeof(af));
    act("$n chokes and gags.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    send_to_char("You choke and gag.\n\r", ch);
    af.where     = TO_AFFECTS;
    af.group     = AFFGROUP_BIOLOGICAL;
    af.type      = skill_resolve_gsn("poison");
    af.skill = skill_find_uid(af.type);
    af.level	 = number_fuzzy(amount);
    af.duration  = 3 * amount;
    af.location  = APPLY_NONE;
    af.modifier  = 0;
    af.bitvector = AFF_POISON;
    af.bitvector2 = 0;
    af.slot	= WEAR_NONE;
    affect_join(ch, &af);
    }

    if (FLUID_CON(obj)->capacity > 0) {
        FLUID_CON(obj)->amount -= amount;
    }

    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DRINK, NULL);

    return;
}


/**
 * do_eat - Consume food or pills
 *
 * Eats ITEM_FOOD or ITEM_PILL objects from inventory.
 *
 * ITEM_FOOD:
 * - Satisfies hunger via FOOD(obj)->hunger
 * - FOOD(obj)->poison != 0 means poisoned (applies poison affect)
 *
 * ITEM_PILL:
 * - Casts up to 4 spells stored in values[1-4]
 * - Spell level from typed food payload (FOOD(obj)->hunger)
 *
 * Special items:
 * - Golden apple: Grants enough XP to level up
 *
 * Immortals can eat anything.
 * Extracts the item after consumption.
 *
 * @param ch        Character eating
 * @param argument  Food or pill to eat
 *
 * Triggers: TRIG_EAT
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_eat(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    SPELL_DATA *spell;

    one_argument(argument, arg);

    if (check_social_status(ch))
        return;

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

    act("$n eats $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("You eat $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    if (obj->pIndexData == get_reserved_obj_index("obj_golden_apple") && !IS_IMMORTAL(ch))
    {
        long xp;

    xp = exp_per_level(ch, NULL, ch->pcdata->points) - ch->exp;
    gain_exp(ch, NULL, xp, false);
    extract_obj(obj);
    return;
    }

    switch (obj->item_type)
    {
    case ITEM_FOOD:
        if (!IS_NPC(ch))
        {
        int condition;

        condition = ch->pcdata->condition[COND_HUNGER];

        gain_condition(ch, COND_FULL, FOOD(obj)->hunger);
        gain_condition(ch, COND_HUNGER, FOOD(obj)->full);
        if (condition == 0 && ch->pcdata->condition[COND_HUNGER] > 0)
            send_to_char("You are no longer hungry.\n\r", ch);
        }

        if (FOOD(obj)->poison != 0
        && check_immune(ch, DAM_POISON) != IS_IMMUNE)
        {
        /* The food was poisoned! */
        AFFECT_DATA af;
        memset(&af,0,sizeof(af));
        act("$n chokes and gags.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        send_to_char("You choke and gag.\n\r", ch);

        af.where	 = TO_AFFECTS;
        af.group     = AFFGROUP_BIOLOGICAL;
        af.type      = skill_resolve_gsn("poison");
    af.skill = skill_find_uid(af.type);
        af.level 	 = number_fuzzy(FOOD(obj)->hunger);
        af.duration  = 2 * FOOD(obj)->hunger;
        af.location  = APPLY_NONE;
        af.modifier  = 0;
        af.bitvector = AFF_POISON;
        af.bitvector2 = 0;
        af.slot	= WEAR_NONE;
        affect_join(ch, &af);
        }
        break;

    case ITEM_PILL:
        for (spell = obj->spells; spell != NULL; spell = spell->next)
        obj_cast_spell(spell->sn, FOOD(obj)->hunger, ch, ch, NULL);
        break;
    }

    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_EAT, NULL);

    extract_obj(obj);
}


/**
 * remove_obj - Remove an item from a specific wear slot
 *
 * Attempts to remove equipment from the specified wear location.
 * Returns true if slot is now empty (either was empty or successfully removed).
 *
 * Checks:
 * - ITEM_NOREMOVE flag blocks removal
 * - WEAR_REMOVEEQ macro for slot-specific rules
 * - WEAR_ALWAYSREMOVE overrides restrictions
 * - TRIG_PREREMOVE can cancel removal
 *
 * Sets script_lastreturn = 2 to indicate explicit REMOVE vs general unequip.
 *
 * @param ch       Character removing item
 * @param iWear    Wear slot constant (WEAR_*)
 * @param fReplace If false and slot occupied, don't remove
 *
 * @return true if slot is empty after call, false if blocked
 *
 * Planned refactor: MOVED comment indicates intended move to player/inv.c
 */
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
        if (IS_SET(obj->extra[0], ITEM_NOREMOVE) || !WEAR_REMOVEEQ(iWear)) {
            act("You can't remove $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            return false;
        }

        if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREMOVE, NULL))
            return false;
    }

    script_lastreturn = 2;	/* Indicate that it is a REMOVE not just a general unequip */

    if(!unequip_char(ch, obj, true)) {
    act("$n stops using $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("You stop using $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
    return true;
}

/**
 * get_wear_loc - Determine appropriate wear slot for an object
 *
 * Checks object's wear flags (CAN_WEAR) to determine which equipment
 * slot it should occupy. For paired slots (fingers, ears, wrists, etc.),
 * returns the left version.
 *
 * Checks wear flags in priority order:
 * ITEM_LIGHT, FINGER, RING_FINGER, NECK, BODY, HEAD, FACE, EYES, EAR,
 * LEGS, ANKLE, FEET, HANDS, ARMS, ABOUT, WAIST, WRIST, SHIELD, BACK,
 * SHOULDER, WIELD, HOLD, TABARD
 *
 * @param ch   Character wearing (unused but kept for signature consistency)
 * @param obj  Object to check wear location for
 *
 * @return WEAR_* constant for appropriate slot, or WEAR_NONE if unwearable
 */
int get_wear_loc(CHAR_DATA *ch, OBJ_DATA *obj)
{
    if (obj->item_type == ITEM_LIGHT)
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


/**
 * wear_obj - Equip an object to the appropriate slot
 *
 * Attempts to equip an object, determining the correct wear slot
 * and handling dual-slot items (fingers, wrists, etc.).
 *
 * Level/class restrictions:
 * - Object level must be <= character level (or ITEM_ALL_REMORT + remort)
 * - ITEM_REMORT_ONLY requires character to be remort
 * - Immortals bypass level restrictions
 *
 * Special handling:
 * - ITEM_LIGHT goes to WEAR_LIGHT slot
 * - Dual slots (FINGER_L/R, NECK_1/2, etc.) auto-select empty side
 * - Weapons check handedness and dual wield capability
 * - Shields check for two-handed weapon conflicts
 *
 * @param ch        Character equipping
 * @param obj       Object to equip
 * @param fReplace  If true, replace existing equipment if necessary
 *
 * Planned refactor: MOVED comment indicates intended move to player/inv.c
 */
void wear_obj(CHAR_DATA *ch, OBJ_DATA *obj, bool fReplace)
{
    char buf[MAX_STRING_LENGTH];

    if (!is_wearable(obj))
    {
        act("You can't wear, wield, or hold $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (!IS_IMMORTAL(ch) && !IS_NPC(ch)) {
        /* If the object is not a mortal object
           -or- is higher object level and the item is not flagged all_remort or the char is not remort */
        if ((obj->level > LEVEL_HERO) ||
            ((ch->tot_level < obj->level) && !(IS_SET(obj->extra[1], ITEM_ALL_REMORT) && IS_REMORT(ch)))) {
            sprintf(buf, "You must be level %d to use this object.\n\r", obj->level);
            send_to_char(buf, ch);
            act("$n tries to use $p, but is too inexperienced.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            return;
        }
    }

    if (IS_SET(obj->extra[1], ITEM_REMORT_ONLY) && !IS_REMORT(ch) && !IS_NPC(ch)) {
        send_to_char("You cannot use this object without remorting.\n\r", ch);
        act("$n tries to use $p, but is too inexperienced.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    /* Prerequisites check — evaluated before any wear position logic */
    if (!IS_NPC(ch) && obj->pIndexData && !IS_NULLSTR(obj->pIndexData->prerequisites)) {
        REQUIREMENT_CONTEXT prereq_ctx;
        prereq_ctx.actor      = ch;
        prereq_ctx.self_mob   = NULL;
        prereq_ctx.self_obj   = obj;
        prereq_ctx.self_room  = NULL;
        prereq_ctx.self_token = NULL;
        prereq_ctx.self_quest = NULL;
        if (!requirements_evaluate_text(obj->pIndexData->prerequisites, &prereq_ctx, true)) {
            send_to_char("You don't meet the requirements to use this item.\n\r", ch);
            return;
        }
    }

    if (obj->item_type == ITEM_LIGHT) {
        if (!remove_obj(ch, WEAR_LIGHT, fReplace))
            return;

        act("$n lights $p and holds it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You light $p and hold it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_LIGHT);
        return;
    }


    if (CAN_WEAR(obj, ITEM_WEAR_FINGER)) {
        if (get_eq_char(ch, WEAR_FINGER_L) != NULL && get_eq_char(ch, WEAR_FINGER_R) != NULL &&
            !remove_obj(ch, WEAR_FINGER_L, fReplace) && !remove_obj(ch, WEAR_FINGER_R, fReplace))
            return;

        if (get_eq_char(ch, WEAR_FINGER_L) == NULL)
        {
            act("$n wears $p on $s left finger.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your left finger.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            equip_char(ch, obj, WEAR_FINGER_L);
            return;
        }

        if (get_eq_char(ch, WEAR_FINGER_R) == NULL)
        {
            act("$n wears $p on $s right finger.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your right finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

        act("$n wears $p on $s ring finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your ring finger.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("$n wears $p around $s neck.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p around your neck.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            equip_char(ch, obj, WEAR_NECK_1);
            return;
        }

        if (get_eq_char(ch, WEAR_NECK_2) == NULL)
        {
            act("$n wears $p around $s neck.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p around your neck.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        act("$n wears $p on $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_BODY);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_TABARD))
    {
        if (!remove_obj(ch, WEAR_TABARD, fReplace))
            return;
        act("$n drapes $p down the front of $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You drape $p down the front of your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_TABARD);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_HEAD))
    {
        if (!remove_obj(ch, WEAR_HEAD, fReplace))
            return;
        act("$n wears $p on $s head.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your head.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_HEAD);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_FACE))
    {
        if (!remove_obj(ch, WEAR_FACE, fReplace))
            return;
        act("$n wears $p over $s face.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p over your face.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_FACE);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_EYES))
    {
        if (!remove_obj(ch, WEAR_EYES, fReplace))
            return;
        act("$n wears $p over $s eyes.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p over your eyes.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_EYES);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_EAR)) {
        if (get_eq_char(ch, WEAR_EAR_L) != NULL && get_eq_char(ch, WEAR_EAR_R) != NULL &&
            !remove_obj(ch, WEAR_EAR_L, fReplace) && !remove_obj(ch, WEAR_EAR_R, fReplace))
            return;

        if (get_eq_char(ch, WEAR_EAR_L) == NULL)
        {
            act("$n wears $p on $s left ear.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your left ear.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            equip_char(ch, obj, WEAR_EAR_L);
            return;
        }

        if (get_eq_char(ch, WEAR_EAR_R) == NULL)
        {
            act("$n wears $p on $s right ear.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your right ear.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        act("$n wears $p on $s legs.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your legs.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_LEGS);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ANKLE)) {
        if (get_eq_char(ch, WEAR_ANKLE_L) != NULL && get_eq_char(ch, WEAR_ANKLE_R) != NULL &&
            !remove_obj(ch, WEAR_ANKLE_L, fReplace) && !remove_obj(ch, WEAR_ANKLE_R, fReplace))
            return;

        if (get_eq_char(ch, WEAR_ANKLE_L) == NULL)
        {
            act("$n wears $p on $s left ankle.",    ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your left ankle.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            equip_char(ch, obj, WEAR_ANKLE_L);
            return;
        }

        if (get_eq_char(ch, WEAR_ANKLE_R) == NULL)
        {
            act("$n wears $p on $s right ankle.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p on your right ankle.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
        act("$n wears $p on $s feet.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your feet.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_FEET);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_HANDS))
    {
        if (!remove_obj(ch, WEAR_HANDS, fReplace))
            return;
        act("$n wears $p on $s hands.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your hands.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_HANDS);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ARMS))
    {
        if (!remove_obj(ch, WEAR_ARMS, fReplace))
            return;
        act("$n wears $p on $s arms.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p on your arms.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_ARMS);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))
    {
        if (!remove_obj(ch, WEAR_ABOUT, fReplace))
            return;
        act("$n wears $p about $s torso.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p about your torso.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_ABOUT);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_WAIST))
    {
        if (!remove_obj(ch, WEAR_WAIST, fReplace))
            return;
        act("$n wears $p about $s waist.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p about your waist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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
            act("$n wears $p around $s left wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p around your left wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            equip_char(ch, obj, WEAR_WRIST_L);
            return;
        }

        if (get_eq_char(ch, WEAR_WRIST_R) == NULL)
        {
            act("$n wears $p around $s right wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("You wear $p around your right wrist.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

        act("$n wears $p as a shield.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wear $p as a shield.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_SHIELD);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_BACK))
    {
        if (!remove_obj(ch, WEAR_BACK, fReplace))
            return;

        act("$n slings $p across $s back.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You sling $p across your back.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_BACK);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WEAR_SHOULDER))
    {
        if (!remove_obj(ch, WEAR_SHOULDER, fReplace))
            return;

        act("$n slings $p over $s shoulder.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You sling $p over your shoulder.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_SHOULDER);
        return;
    }

    if (CAN_WEAR(obj, ITEM_WIELD))
    {
        int sn,skill;

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
                (WEAPON(get_eq_char(ch, WEAR_SECONDARY))->weapon_class == WEAPON_POLEARM ||
                WEAPON(get_eq_char(ch, WEAR_SECONDARY))->weapon_class == WEAPON_SPEAR) &&
                (WEAPON(obj)->weapon_class == WEAPON_POLEARM || WEAPON(obj)->weapon_class == WEAPON_SPEAR))
        {
            send_to_char("You can't wield two of those at once.\n\r", ch);
            return;
        }

        if (both_hands_full(ch))
        {
            send_to_char("You don't have a spare hand!\n\r",ch);
            return;
        }

        act("$n wields $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You wield $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_WIELD);

        sn = get_weapon_sn(ch);

        if (sn == skill_resolve_gsn("hand to hand"))
           return;

        skill = get_weapon_skill(ch,sn);

        if (skill >= 100)
            act("$p feels like a part of you!",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else if (skill > 85)
            act("You feel quite confident with $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else if (skill > 70)
            act("You are skilled with $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else if (skill > 50)
            act("Your skill with $p is adequate.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else if (skill > 25)
            act("$p feels a little clumsy in your hands.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else if (skill > 1)
            act("You fumble and almost drop $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        else
            act("You don't even know which end is up on $p.", ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);

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

        act("$n holds $p in $s hand.",   ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You hold $p in your hand.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        equip_char(ch, obj, WEAR_HOLD);
        return;
    }

    if (fReplace)
        send_to_char("You can't wear, wield, or hold that.\n\r", ch);
}


/**
 * do_wear - Equip an item from inventory
 *
 * Player command to wear/wield/hold objects.
 *
 * Syntaxes:
 * - wear <item>   : Equip specific item
 * - wear all      : Equip all possible items (prioritizes last_wear_loc)
 *
 * Restrictions:
 * - Cannot wear while shifted (slayer/werewolf)
 * - Cannot wear while blinded
 * - Social status check for certain areas
 *
 * "wear all" performs two passes:
 * 1. First equips items to their last_wear_loc if set
 * 2. Then equips remaining wearable items to available slots
 *
 * @param ch        Character equipping
 * @param argument  Item to wear or "all"
 *
 * Triggers: TRIG_PREWEAR (can cancel)
 *
 * Planned refactor: MOVED comment indicates intended move to player/inv.c
 */
void do_wear(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    one_argument(argument, arg);

    if (check_social_status(ch))
        return;

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

    if (!str_cmp(arg, "all"))
    {


        send_to_char("You throw on your equipment.\n\r", ch);
        act("$n throws on $s equipment.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        /* First run through all equipment looking for last_wear_loc set. */
        ITERATOR it;
        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            if (obj->last_wear_loc != WEAR_NONE &&
                WEAR_AUTOEQUIP(obj->last_wear_loc) &&
                can_see_obj(ch, obj) &&
                obj->wear_loc == WEAR_NONE &&
                ch->tot_level >= obj->level) {
                if (both_hands_full(ch)
                && (CAN_WEAR(obj, ITEM_WEAR_SHIELD)
                     || CAN_WEAR(obj, ITEM_HOLD)
                     || CAN_WEAR(obj, ITEM_WIELD)))
                    continue;

                if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL))
                    continue;

                equip_char(ch, obj, obj->last_wear_loc);
            }
        }
        iterator_stop(&it);

        iterator_start(&it, ch->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            if (obj->wear_loc == WEAR_NONE
            && can_see_obj(ch, obj)
            && is_wearable(obj))
            {
                if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL))
                    continue;

                wear_obj(ch, obj, false);
            }
        }
        iterator_stop(&it);
        return;
    }
    else
    {
        if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
        {
            send_to_char("You do not have that item.\n\r", ch);
            return;
        }

        if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL))
            return;

        wear_obj(ch, obj, true);
    }
}


/**
 * removeall - Remove all equipped items
 *
 * Internal function to strip all equipment from a character.
 * Saves current wear locations (last_wear_loc) before removal
 * so "wear all" can restore equipment to same slots.
 *
 * @param ch  Character to strip equipment from
 *
 * Planned refactor: MOVED comment indicates intended move to player/inv.c
 */
void removeall(CHAR_DATA *ch)
{
    OBJ_DATA *obj;

    save_last_wear(ch);

    ITERATOR it;
    iterator_start(&it, ch->lworn);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (obj->wear_loc != WEAR_NONE)
        {
            remove_obj(ch, obj->wear_loc, true);
        }
    }
    iterator_stop(&it);
}


/**
 * do_remove - Unequip items from wear slots
 *
 * Player command to remove equipment.
 *
 * Syntaxes:
 * - remove <item>  : Remove specific item
 * - remove all     : Remove all removable equipment
 *
 * Restrictions:
 * - Cannot remove while blinded
 * - ITEM_NOREMOVE blocks individual removal
 * - Tattoos cannot be removed with "remove all"
 * - Social status check for certain areas
 * - WEAR_ALWAYSREMOVE overrides NOREMOVE flag
 *
 * Saves last_wear_loc before "remove all" for later "wear all".
 *
 * @param ch        Character removing equipment
 * @param argument  Item to remove or "all"
 *
 * Triggers: TRIG_PREREMOVE (can cancel)
 *
 * Planned refactor: MOVED comment indicates intended move to player/inv.c
 */
void do_remove(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;

    if (check_social_status(ch))
        return;

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
        save_last_wear(ch);

        send_to_char("You remove your equipment.\n\r", ch);
        act("$n removes $s equipment.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        ITERATOR it;
        iterator_start(&it, ch->lworn);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            if (obj->wear_loc != WEAR_NONE
                && obj->item_type != ITEM_TATTOO
                && can_see_obj(ch, obj)
                && (WEAR_ALWAYSREMOVE(obj->wear_loc) || !IS_SET(obj->extra[0], ITEM_NOREMOVE))
                && wear_params[obj->wear_loc][2])
            {
                if (p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREREMOVE, NULL))
                    continue;

                unequip_char(ch, obj, false);
            }
        }
        iterator_stop(&it);
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

/**
 * sacrifice_obj - Sacrifice a single object to the gods
 *
 * Internal function to sacrifice one object. Calculates deity point
 * reward via get_dp_value(), extracts the object, and displays
 * appropriate message.
 *
 * @param ch    Character sacrificing
 * @param obj   Object to sacrifice (extracted after)
 * @param name  Object name for error messages
 */
void sacrifice_obj(CHAR_DATA *ch, OBJ_DATA *obj, char *name)
{
    long deitypoints;
    char buf[MSL];

    if (obj == NULL || !can_see_obj(ch, obj))
    {
        act("I see no $T here.", ch, NULL, NULL, NULL, NULL, NULL, name, TO_CHAR, NULL, NULL);
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

    act("$n sacrifices $p to the gods.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    extract_obj(obj);

}

/**
 * do_sacrifice - Offer objects to the gods for deity points
 *
 * Destroys objects on the ground in exchange for deity points.
 *
 * Syntaxes:
 * - sacrifice <item>    : Sacrifice single item
 * - sacrifice all       : Sacrifice all sacrificable items
 * - sacrifice all.<type>: Sacrifice all matching items
 * - sacrifice <self>    : Humorous self-sacrifice message
 *
 * Special handling:
 * - Sacrificing in donation room triggers lightning punishment
 * - Groups similar items for consolidated messaging
 * - Tracks total deity points gained
 *
 * @param ch        Character sacrificing
 * @param argument  Item to sacrifice or "all"
 */
void do_sacrifice(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char short_descr[MSL];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    one_argument(argument, arg);

    if (arg[0] == '\0' || !str_cmp(arg, ch->name))
    {
        act("$n offers $mself to his god, who graciously declines.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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

        if (ch->in_room == get_reserved_room_index("room_donation"))
        {
            send_to_char("Where are your manners!?\n\r", ch);
            send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", ch);

            send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", ch);

            act("{Y$n is struck by a bolt of lightning!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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
                    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                }
            }
            else
            {
                if (!any)
                {
                    if (arg[3] == '\0')
                    {
                        act("There is nothing here you can sacrifice.", ch, NULL, NULL, NULL, NULL, NULL , NULL, TO_CHAR, NULL, NULL);
                    }
                    else
                    {
                        act("There's no $T here.", ch, NULL, NULL, NULL, NULL, NULL, &arg[4], TO_CHAR, NULL, NULL);
                    }
                }
            }
        }

        if (any)
        {
            if (total == 0)
            {
                sprintf(buf, "The gods accept your sacrifice, but give you nothing.");
                act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
            else if (total == 1)
            {
                sprintf(buf, "Pleased with your sacrifice, the gods reward you with a deity point.");
                act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }
            else
            {
                sprintf(buf, "Pleased with your sacrifice, the gods reward you with {Y%ld{x deity points.", total);
                act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            }

            ch->deitypoints += total;
        }
    }
}

/**
 * do_quaff - Drink a potion to cast its spells
 *
 * Consumes a potion from inventory, casting all spells stored in it.
 * Potions cast on the drinker at the potion's stored level.
 *
 * Features:
 * - Level check (tot_level >= obj->level)
 * - Social area restriction
 * - Multi-swig potions (FLUID_CON(obj)->amount = sips remaining)
 * - TRIG_PREDRINK can cancel
 * - 8 beat wait state after quaffing
 *
 * @param ch        Character quaffing
 * @param argument  Potion to drink
 *
 * Triggers: TRIG_PREDRINK, TRIG_DRINK
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_quaff(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    SPELL_DATA *spell;

    one_argument(argument, arg);

    if (IS_SOCIAL(ch))
    {
        send_to_char("You can't do that while socializing.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
    send_to_char("Quaff what?\n\r", ch);
    return;
    }

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
    send_to_char("You do not have that potion.\n\r", ch);
    return;
    }

    if (obj->item_type != ITEM_POTION)
    {
    send_to_char("You can quaff only potions.\n\r", ch);
    return;
    }

    if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREDRINK, NULL))
    return;

    if (ch->tot_level < obj->level)
    {
    send_to_char("This liquid is too powerful for you to drink.\n\r",ch);
    return;
    }

    if (obj->pIndexData == get_reserved_obj_index("obj_empty_vial"))
    {
    act("$p has nothing in it you can quaff.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    return;
    }

    /* Currently only alchemists can make multi-quaffable potions */
    if (FLUID_CON(obj)->amount > 0)
    {
    FLUID_CON(obj)->amount--;
    }

    if (FLUID_CON(obj)->amount > 0)
    {
    act("$n takes a small swig from $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("You take a small swig from $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    }
    else
    {
        act("$n quaffs $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You quaff $p.", ch, NULL, NULL, obj, NULL, NULL, NULL ,TO_CHAR, NULL, NULL);
    }

    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_DRINK, NULL);

    for (spell = obj->spells; spell != NULL; spell = spell->next)
    obj_cast_spell(spell->sn, spell->level, ch, ch, NULL);

    if (FLUID_CON(obj)->amount <= 0)
    extract_obj(obj);

    WAIT_STATE(ch, 8);
}


/**
 * do_recite - Read a scroll to cast its spells
 *
 * Recites a scroll from inventory, casting its spells on target.
 * Scrolls have cast time based on number of spells.
 *
 * Features:
 * - Level check (tot_level >= scroll->level)
 * - Silence check (AFF2_SILENCE blocks)
 * - Target can be character or object
 * - Variable cast time: 10-18 beats based on spell count
 * - Scroll skill affects success chance
 * - Extracts scroll after successful cast
 *
 * @param ch        Character reciting
 * @param argument  Scroll and optional target
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_recite(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *scroll;
    OBJ_DATA *obj;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (check_social_status(ch))
        return;

    if ((scroll = get_obj_carry(ch, arg1, ch)) == NULL)
    {
        send_to_char("You do not have that scroll.\n\r", ch);
        return;
    }

    if (scroll->item_type != ITEM_SCROLL)
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
    int spell_count = 0;
    for (SPELL_DATA *sp = scroll->spells; sp; sp = sp->next) spell_count++;
    if (spell_count <= 1)
        beats = 10;
    else if (spell_count <= 2)
        beats = 14;
    else
        beats = 18;

    // Both scripts MUST provide a reason.
    // Does the scroll forbid it?
    scroll->tempstore[0] = beats;
    if( p_percent_trigger( NULL, scroll, NULL, NULL, ch, victim, NULL,obj, NULL, TRIG_PRERECITE, NULL) )
        return;

    // Does the ROOM forbid it?
    ch->in_room->tempstore[0] = scroll->tempstore[0];
    if( p_percent_trigger( NULL, NULL, ch->in_room, NULL, ch, victim, NULL, obj, scroll, TRIG_PRERECITE, NULL) )
        return;

    // Does the PLAYER (TOKENS) forbid it?
    ch->tempstore[0] = ch->in_room->tempstore[0];
    if( p_percent_trigger( ch, NULL, NULL, NULL, ch, victim, NULL, obj, scroll, TRIG_PRERECITE, NULL) )
        return;

    beats = ch->tempstore[0];
    beats = UMAX(beats, 1);

    RECITE_STATE(ch, beats);

    act("{W$n begins to recite the words of $p...{x", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{WYou begin to recite the words of $p...{x", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    ch->recite_scroll = scroll;

    if (victim != NULL)
        ch->cast_target_name = str_dup(victim->name);
    else if (obj != NULL)
        ch->cast_target_name = str_dup(obj->name);
}


/**
 * recite_end - Complete scroll recitation and cast spells
 *
 * Called when scroll recitation delay completes. Validates target
 * still exists, performs skill check, then casts all spells on scroll.
 *
 * Special handling:
 * - "kill" spell scrolls explode harmlessly
 * - TRIG_RECITE can intercept and cancel casting
 * - Scrolls skill affects success (20 + skill*4/5)
 * - Extracts scroll after use
 *
 * @param ch  Character who finished reciting
 *
 * Triggers: TRIG_RECITE (on scroll, can cancel)
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void recite_end(CHAR_DATA *ch)
{
    CHAR_DATA *victim;
    OBJ_DATA *scroll;
    OBJ_DATA *obj;
    int kill;
    SPELL_DATA *spell;

    scroll = ch->recite_scroll;

    act("{W$n has completed reciting the scroll.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{WYou complete reciting the scroll.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    if (scroll == NULL)
    {
        send_to_char("The scroll has vanished.\n\r", ch);
        return;
    }

    if (ch->cast_target_name == NULL)
    {
        pbugf(LOG_ERROR "for %s, cast_target_name was null!",
            IS_NPC(ch) ? ch->short_descr : ch->name);
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
        act("The scroll has disappeared.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    kill = find_spell(ch, "kill");
    for (spell = scroll->spells; spell != NULL; spell = spell->next)
    {
        if (spell->sn == kill) {
            act("$p explodes into dust!", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ALL, NULL, NULL);
            extract_obj(scroll);
            return;
        }
    }

    if( p_percent_trigger( NULL, scroll, NULL, NULL, ch, victim, NULL, obj, NULL, TRIG_RECITE, NULL) <= 0 )
    {
        act("$p flares brightly then disappears!", ch, NULL, NULL, scroll, NULL, NULL, NULL, TO_ALL, NULL, NULL);

        if (number_percent() >= 20 + get_skill(ch,skill_resolve_gsn("scrolls")) * 4/5)
        {
            send_to_char("You mispronounce a syllable.\n\r",ch);
            check_improve(ch,skill_resolve_gsn("scrolls"),false,2);
        }
        else
        {
            for (spell = scroll->spells; spell != NULL; spell = spell->next)
                obj_cast_spell(spell->sn, spell->level, ch, victim, obj);
            check_improve(ch,skill_resolve_gsn("scrolls"),true,2);
        }

        extract_obj(scroll);
    }
}


/**
 * do_brandish - Wave a staff to cast its spells
 *
 * Brandishes held staff item to cast area-effect spells.
 * Staff spells target everyone in room based on spell targeting:
 * - TAR_IGNORE: Caster only
 * - TAR_CHAR_OFFENSIVE: Opposite alignment (NPC vs PC)
 * - TAR_CHAR_DEFENSIVE: Same alignment
 * - TAR_CHAR_SELF: Caster only
 *
 * Consumes one charge (WAND(staff)->charges). Staff destroyed when empty.
 * Staves skill affects success (20 + skill*4/5).
 * 2 PULSE_VIOLENCE wait state.
 *
 * @param ch        Character brandishing
 * @param argument  Unused
 *
 * Triggers: TRIG_BRANDISH (on staff, can cancel)
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_brandish(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;
    OBJ_DATA *staff;
    SPELL_DATA *spell;
    int sn;

    if ((staff = get_eq_char(ch, WEAR_HOLD)) == NULL)
    {
    send_to_char("You hold nothing in your hand.\n\r", ch);
    return;
    }

    if(p_percent_trigger(NULL, staff, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BRANDISH, argument))
        return;

    if (staff->item_type != ITEM_STAFF)
    {
    send_to_char("You can brandish only with a staff.\n\r", ch);
    return;
    }

    if (!staff->spells)
    {
    pbugf(LOG_ERROR, "No spells %d.", staff->pIndexData->vnum);
    return;
    }

    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);

    if (WAND(staff)->charges > 0)
    {
    act("$n brandishes $p.", ch, NULL, NULL, staff, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("You brandish $p.",  ch, NULL, NULL, staff, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    if (ch->tot_level < staff->level
    ||   number_percent() >= 20 + get_skill(ch,skill_resolve_gsn("staves")) * 4/5)
     {
        act ("You fail to invoke $p.",ch, NULL, NULL,staff, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act ("...and nothing happens.",ch,NULL,NULL, NULL, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("staves"),false,2);
    }
    else
    {
        for (vch = ch->in_room->people; vch; vch = vch_next)
        {
        vch_next	= vch->next_in_room;

                for (spell = staff->spells; spell != NULL; spell = spell->next)
        {
            sn = spell->sn;
            switch (skill_table[sn].target)
            {
            default:
            pbugf(LOG_ERROR, "Do_brandish: bad target for sn %d.", sn);
            return;

            case TAR_IGNORE:
            if (vch != ch)
                continue;
            break;

            case TAR_CHAR_OFFENSIVE:
            if (IS_NPC(ch) ? IS_NPC(vch) : !IS_NPC(vch))
                continue;
            break;

            case TAR_CHAR_DEFENSIVE:
            if (IS_NPC(ch) ? !IS_NPC(vch) : IS_NPC(vch))
                continue;
            break;

            case TAR_CHAR_SELF:
            if (vch != ch)
                continue;
            break;
            }

            obj_cast_spell(sn, spell->level, ch, vch, NULL);
        }

        check_improve(ch,skill_resolve_gsn("staves"),true,2);
        }
    }
    }

    if (--WAND(staff)->charges <= 0)
    {
    act("$n's $p blazes bright and is gone.", ch, NULL, NULL, staff, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("Your $p blazes bright and is gone.", ch, NULL, NULL, staff, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    //plogf(LOG_INFO,"It disappeared in a blaze");
    extract_obj(staff);
    }
}


/**
 * do_zap - Use a wand to cast its spell at a target
 *
 * Zaps with held wand item to cast single-target spell.
 * Without argument, targets self or current combat opponent.
 *
 * Consumes one charge (WAND(wand)->charges). Wand destroyed when empty.
 * Wands skill affects success (20 + skill*4/5).
 * 2 PULSE_VIOLENCE wait state.
 *
 * @param ch        Character zapping
 * @param argument  Optional target (character or object)
 *
 * Triggers: TRIG_ZAP (on wand, can cancel)
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
void do_zap(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    OBJ_DATA *wand;
    OBJ_DATA *obj;
    SPELL_DATA *spell;

    one_argument(argument, arg);
    if (arg[0] == '\0' && ch->fighting == NULL)
    {
    send_to_char("Zap whom or what?\n\r", ch);
    return;
    }

    if ((wand = get_eq_char(ch, WEAR_HOLD)) == NULL)
    {
    send_to_char("You hold nothing in your hand.\n\r", ch);
    return;
    }

    if(p_percent_trigger(NULL, wand, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_ZAP, arg))
        return;

    if (wand->item_type != ITEM_WAND)
    {
    send_to_char("You can zap only with a wand.\n\r", ch);
    return;
    }

    obj = NULL;
    if (arg[0] == '\0')
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
    if ((victim = get_char_room (ch, NULL,arg)) == NULL
    &&   (obj    = get_obj_here  (ch, NULL,arg)) == NULL)
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
        act("$n zaps $N with $p.", ch, victim, NULL, wand, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
        act("You zap $N with $p.", ch, victim, NULL, wand, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n zaps you with $p.",ch, victim, NULL, wand, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }
    else
    {
        act("$n zaps $P with $p.", ch, NULL, NULL, wand, obj, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You zap $P with $p.", ch, NULL, NULL, wand, obj, NULL, NULL, TO_CHAR, NULL, NULL);
    }

     if (ch->tot_level < wand->level
    ||  number_percent() >= 20 + get_skill(ch,skill_resolve_gsn("wands")) * 4/5)
    {
        act("Your efforts with $p produce only smoke and sparks.", ch, NULL, NULL,wand, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        act("$n's efforts with $p produce only smoke and sparks.", ch, NULL, NULL,wand, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("wands"),false,2);
    }
    else
    {
        for (spell = wand->spells; spell != NULL; spell = spell->next)
        obj_cast_spell(spell->sn, spell->level, ch, victim, obj);

        check_improve(ch,skill_resolve_gsn("wands"),true,2);
    }
    }

    if (--WAND(wand)->charges <= 0)
    {
    act("$n's $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("Your $p explodes into fragments.", ch, NULL, NULL, wand, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    //plogf(LOG_INFO,"It exploded into fragments");
    extract_obj(wand);
    }
}

/**
 * do_steal - Attempt to steal coins or items from a victim
 *
 * Uses skill_resolve_gsn("steal") skill to pilfer from another character.
 * Success is affected by:
 * - Victim awareness (sleeping -10%, can't see +25%, otherwise +50%)
 * - Highwayman subclass with active holdup = guaranteed success
 * - skill_resolve_gsn("deception") can detect PC thieves
 * - CPK rooms required for PC vs PC stealing
 *
 * Coin theft: Steals random portion based on level ratio.
 * Item theft: Cannot steal worn items or ITEM_INVENTORY items.
 *
 * Failure:
 * - Strips sneak affect
 * - Victim yells and NPCs attack
 *
 * @param ch        Character stealing
 * @param argument  Item/coins and target
 *
 * Planned refactor: MOVED comment indicates intended move to object/actions.c
 */
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

    WAIT_STATE(ch, skill_table[skill_resolve_gsn("steal")].beats);
    percent  = number_percent();

    if (!IS_AWAKE(victim))
        percent -= 10;
    else if (!can_see(victim,ch))
        percent += 25;
    else
    {
    if (ch->pcdata->second_sub_class_thief == CLASS_THIEF_HIGHWAYMAN)
        {
        if (ch->heldup == victim)
            percent = 0;
        else
        percent += 25;
    }
    else
        percent += 50;
    }

    if (percent > get_skill(ch,skill_resolve_gsn("steal"))
         || (!IS_NPC(victim)
         && number_percent() < get_skill(victim, skill_resolve_gsn("deception")))
         || (!IS_NPC(ch)
          && !IS_NPC(victim)
          && !is_room_full_cpk(ch->in_room)))
    {
    send_to_char("Oops.\n\r", ch);
    affect_strip(ch,skill_resolve_gsn("sneak"));
    REMOVE_BIT(ch->affected_by[0],AFF_SNEAK);

    act("$n tried to steal from you.\n\r", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL   );
    act("$n tried to steal from $N.\n\r",  ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    switch(number_range(0,3))
    {
    case 0 :
       sprintf(buf, "%s is a lousy thief!", ch->name);
       break;
        case 1 :
       sprintf(buf, "%s couldn't rob %s way out of a paper bag!",
            ch->name,get_his_her(ch));
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
            check_improve(ch,skill_resolve_gsn("steal"),false,2);
        multi_hit(victim, ch, TYPE_UNDEFINED);
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
    check_improve(ch,skill_resolve_gsn("steal"),true,2);
    return;
    }

    if ((obj = get_obj_carry(victim, arg1, ch)) == NULL)
    {
    send_to_char("You can't find it.\n\r", ch);
    return;
    }

    if (!is_room_full_cpk(ch->in_room) && !IS_NPC(victim) && !IS_NPC(ch))
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
    act("You pocket $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
    check_improve(ch,skill_resolve_gsn("steal"),true,2);
    send_to_char("{WGot it!{x\n\r", ch);
}

bool has_stock_reputation(CHAR_DATA *ch, SHOP_STOCK_DATA *stock)
{
    if (!stock)
        return false;

    if (!IS_VALID(stock->reputation))
        return true;

    REPUTATION_DATA *rep = find_reputation_char(ch, stock->reputation);
    int rank = IS_VALID(rep) ? rep->current_rank : stock->reputation->initial_rank;

    if (stock->min_reputation_rank > 0 && rank < stock->min_reputation_rank)
        return false;
    if (stock->max_reputation_rank > 0 && rank > stock->max_reputation_rank)
        return false;

    return true;
}

bool can_see_stock_reputation(CHAR_DATA *ch, SHOP_STOCK_DATA *stock)
{
    if (!stock)
        return false;

    if (!IS_VALID(stock->reputation))
        return true;

    REPUTATION_DATA *rep = find_reputation_char(ch, stock->reputation);
    int rank = IS_VALID(rep) ? rep->current_rank : stock->reputation->initial_rank;

    if (stock->min_show_rank > 0 && rank < stock->min_show_rank)
        return false;
    if (stock->max_show_rank > 0 && rank > stock->max_show_rank)
        return false;

    return true;
}

/**
 * find_keeper - Locate a shopkeeper by name in the room
 *
 * Searches the room for an NPC with a shop that matches the given name.
 * Validates shop is open during current game hours and that the
 * shopkeeper can see the customer.
 *
 * @param ch   Character looking for a shop
 * @param arg  Name of shopkeeper to find
 *
 * @return Pointer to keeper if found and available, NULL otherwise
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
CHAR_DATA *find_keeper(CHAR_DATA *ch, char *arg)
{
    /*char buf[MAX_STRING_LENGTH];*/
    CHAR_DATA *keeper;
    SHOP_DATA *pShop;

    if (arg[0] == '\0')
    {
        send_to_char("Please specify a shopkeeper.\n\r", ch);
        return NULL;
    }

    pShop = NULL;
    for (keeper = ch->in_room->people; keeper; keeper = keeper->next_in_room)
    {
    if (IS_NPC(keeper) && (pShop = keeper->shop) != NULL && (is_name(arg,keeper->name)))
    {
        if (IS_VALID(pShop->reputation))
        {
            REPUTATION_DATA *rep = find_reputation_char(ch, pShop->reputation);
            int repRank = IS_VALID(rep) ? rep->current_rank : pShop->reputation->initial_rank;

            if (repRank < pShop->min_reputation_rank)
            {
                pShop = NULL;
                continue;
            }
        }

        break;
    }
    }

    if (pShop == NULL || keeper == NULL)
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


/**
 * obj_to_keeper - Add a sold object to a shopkeeper's inventory
 *
 * Inserts object into keeper's inventory, standardizing price with
 * any existing duplicates. Sets ITEM_INVENTORY flag unless the item
 * is marked ITEM_SELL_ONCE.
 *
 * @param obj  Object being sold to keeper
 * @param ch   Shopkeeper receiving the object
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
void obj_to_keeper(OBJ_DATA *obj, CHAR_DATA *ch)
{
    OBJ_DATA *t_obj;

    /* see if any duplicates are found */
    ITERATOR it;
    iterator_start(&it, ch->lcarrying);
    while ((t_obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (obj->pIndexData == t_obj->pIndexData &&
            !str_cmp(obj->short_descr, t_obj->short_descr))
        {
            obj->cost = t_obj->cost; /* keep it standard */
            break;
        }
    }
    iterator_stop(&it);

    /* Add the object to the character's inventory list */
    list_appendlink(ch->lcarrying, obj);

    if(!IS_SET(obj->extra[1], ITEM_SELL_ONCE))
    {
        // If the item can be sold again, mark it as inventory
        SET_BIT(obj->extra[0], ITEM_INVENTORY);
    }

    obj->carried_by = ch;
    obj->in_room = NULL;
    obj->in_obj = NULL;
    ch->carry_number += get_obj_number(obj);
    ch->carry_weight += get_obj_weight(obj);
}

/**
 * adjust_keeper_price - Apply shop profit margins to a price
 *
 * Adjusts a base price using the shop's profit_buy or profit_sell
 * percentage multipliers.
 *
 * @param keeper  Shopkeeper with shop data
 * @param price   Base price to adjust
 * @param fBuy    true for buying (profit_buy), false for selling (profit_sell)
 *
 * @return Adjusted price, or 0 if keeper has no shop
 */
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

/**
 * get_stockonly_keeper - Find stock item by name (stock-only search)
 *
 * Searches only the shop's defined stock items (not keeper inventory)
 * for a matching object, mob, or ship by name. Handles numbered
 * arguments (e.g., "2.sword").
 *
 * @param ch        Customer searching
 * @param keeper    Shopkeeper with stock
 * @param argument  Item name to find (may include number prefix)
 *
 * @return Matching SHOP_STOCK_DATA or NULL if not found
 */
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

            if( stock->entity.wnum.vnum > 0 )
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

/**
 * get_stock_keeper - Find stock or inventory item and populate request
 *
 * Searches shop stock items and keeper's inventory for a matching item.
 * Populates the SHOP_REQUEST_DATA struct with details of the found item.
 * Handles numbered arguments.
 *
 * @param ch        Customer searching
 * @param keeper    Shopkeeper with stock/inventory
 * @param request   Request struct to populate with item details
 * @param argument  Item name to find
 *
 * @return true if item found and request populated, false otherwise
 */
bool get_stock_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, SHOP_REQUEST_DATA *request, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    SHOP_STOCK_DATA *stock;
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;
    if(keeper->shop != NULL) {
        // Check stock items first
        for(stock = keeper->shop->stock; stock; stock = stock->next)
        {
            // Out of stock.
            if(stock->max_quantity > 0 && stock->quantity < 1) continue;

            if(stock->entity.wnum.vnum > 0)
            {
                if(stock->obj != NULL)
                {
                    if(is_name(arg, stock->obj->name))
                    {
                        if(++count == number)
                        {
                            request->stock = stock;
                            request->obj = NULL;
                            return true;
                        }
                    }
                }
                else if(stock->mob != NULL)
                {
                    if(is_name(arg, stock->mob->player_name))
                    {
                        if(++count == number)
                        {
                            request->stock = stock;
                            request->obj = NULL;
                            return true;
                        }
                    }
                }
                else if(stock->ship != NULL)
                {
                    if(is_name(arg, stock->ship->name))
                    {
                        if(++count == number)
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
                if(is_name(arg, stock->custom_keyword))
                {
                    if(++count == number)
                    {
                        request->stock = stock;
                        request->obj = NULL;
                        return true;
                    }
                }
            }
        }
    }

    // Track items we've already seen to avoid duplicates
    OBJ_DATA *last_match = NULL;

    ITERATOR it;
    iterator_start(&it, keeper->lcarrying);
    while((obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if(IS_OBJ_STAT(obj, ITEM_INVENTORY) &&
            obj->wear_loc == WEAR_NONE &&
            can_see_obj(keeper, obj) &&
            can_see_obj(ch, obj) &&
            is_name(arg, obj->name))
        {
            // Skip if this is a duplicate of the last matched object
            if(last_match != NULL && 
                obj->pIndexData == last_match->pIndexData &&
                !str_cmp(obj->short_descr, last_match->short_descr))
                continue;

            last_match = obj;

            if(++count == number)
            {
                iterator_stop(&it);
                request->stock = NULL;
                request->obj = obj;
                return true;
            }
        }
    }
    iterator_stop(&it);

    return false;
}


/**
 * get_obj_keeper - Find object in keeper's inventory by name
 *
 * Searches keeper's carrying list for an object matching the argument.
 * Handles numbered arguments (e.g., "2.sword"). Skips duplicate items
 * with same pIndexData and short_descr.
 *
 * @param ch        Customer searching
 * @param keeper    Shopkeeper with inventory
 * @param argument  Object name to find (may include number prefix)
 *
 * @return Matching object or NULL if not found
 *
 * @note UNUSED - marked for potential removal
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
OBJ_DATA *get_obj_keeper(CHAR_DATA *ch, CHAR_DATA *keeper, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    int number;
    int count;

    number = number_argument(argument, arg);
    count  = 0;

    // Track items we've already seen to avoid duplicates
    OBJ_DATA *last_match = NULL;

    ITERATOR it;
    iterator_start(&it, keeper->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
    {
        if (obj->wear_loc == WEAR_NONE &&
            can_see_obj(keeper, obj) &&
            can_see_obj(ch, obj) &&
            is_name(arg, obj->name))
        {
            // Skip if this is a duplicate of the last matched object
            if (last_match != NULL && 
                obj->pIndexData == last_match->pIndexData &&
                !str_cmp(obj->short_descr, last_match->short_descr))
                continue;

            last_match = obj;
            
            if (++count == number)
            {
                iterator_stop(&it);
                return obj;
            }
        }
    }
    iterator_stop(&it);

    return NULL;
}


/**
 * get_cost - Calculate buy/sell price for an object at a shop
 *
 * Determines the gold cost of an object when buying from or selling
 * to a shopkeeper.
 *
 * Buying: base_cost * profit_buy / 100
 *
 * Selling:
 * - Must match shop's buy_type[] array
 * - base_cost * profit_sell / 100
 * - 25% reduction if keeper already has duplicate
 *
 * Special handling:
 * - Staff/wand prices scale by charges remaining (value[2]/value[1])
 * - Empty staves/wands worth 1/4 base price
 *
 * @param keeper  Shopkeeper with shop data
 * @param obj     Object to price
 * @param fBuy    true for buy price, false for sell price
 *
 * @return Calculated cost in gold, or 0 if invalid
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
int get_cost(CHAR_DATA *keeper, OBJ_DATA *obj, bool fBuy)
{
    SHOP_DATA *pShop;
    int cost;

    if (obj == NULL || (pShop = keeper->shop) == NULL)
        return 0;

    if (fBuy)
    {
        cost = obj->cost * pShop->profit_buy / 100;
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

        // Check for duplicate items that affect price
        ITERATOR it;
        iterator_start(&it, keeper->lcarrying);
        while ((obj2 = (OBJ_DATA *)iterator_nextdata(&it)))
        {
            if (IS_OBJ_STAT(obj2, ITEM_INVENTORY) &&
                obj->pIndexData == obj2->pIndexData &&
                !str_cmp(obj->short_descr, obj2->short_descr))
            {
                cost = cost * 3 / 4;
                break;
            }
        }
        iterator_stop(&it);
    }

    if (obj->item_type == ITEM_STAFF || obj->item_type == ITEM_WAND)
    {
        if (WAND(obj)->max_charges == 0)
            cost /= 4;
        else
            cost = cost * WAND(obj)->charges / WAND(obj)->max_charges;
    }

    return cost;
}


/**
 * do_buy - Purchase items from shops or traders
 *
 * Main buying command supporting multiple vendor types:
 * - Regular shops (ACT_IS_SHOP): buy from keeper inventory/stock
 * - Commodity traders (ACT2_TRADER): buy trade goods into carts
 * - Changers: Exchange gold/silver currency
 * - Bankers: Withdraw from bank account
 *
 * Features:
 * - Haggling with skill_resolve_gsn("haggle") skill for discounts
 * - Quantity purchases: "buy 5 sword"
 * - Stock items with limited quantities
 * - TRIG_BUY/TRIG_PREBUY can intercept purchases
 *
 * @param ch        Character buying
 * @param argument  Item name and optional quantity/keeper
 *
 * Triggers: TRIG_PREBUY (can cancel), TRIG_BUY (after purchase)
 *
 * Planned refactor: MOVED comment indicates intended move to object/shop.c
 */
void do_buy(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    long cost;
    int roll;
    CHAR_DATA *mob;
//    CHAR_DATA *plane_tunneler;
    CHAR_DATA *trader;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg_keeper[MIL];
    bool haggled = false;

 //   plane_tunneler = NULL;
    trader = NULL;

    if (argument[0] == '\0')
    {
        send_to_char("Buy what?\n\r", ch);
        return;
    }
/*
    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_SET(mob->act[1], ACT2_PLANE_TUNNELER) && IS_NPC(mob))
        {
            plane_tunneler = mob;
            break;
        }
    }
*/
    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_SET(mob->act[1], ACT2_TRADER) && IS_NPC(mob))
        {
            trader = mob;
            break;
        }
    }

    //////////////////////////////////////////
    //
    // COMMODOTIES TRADER - TODO: REWORK
    //
    //////////////////////////////////////////
    if ( trader != NULL )
    {
        TRADE_ITEM *temp = NULL;
        OBJ_INDEX_DATA *obj_index = NULL;
        OBJ_DATA *pObj = NULL;
        OBJ_DATA *cart = NULL;
        int counter = -1;
        char *trade_item;
        /*int counter;*/

        argument = one_argument( argument, arg );
        argument = one_argument( argument, arg2 );

//		TODO: TRIG_PREBUY_TRADER
//		if(p_percent_trigger(trader, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBUY, NULL))
//			return;

        if ( (cart = ch->pulled_cart) == NULL )
        {
            sprintf( buf, "%s, you must have a cart to put purchased trade goods!", pers( ch, trader ) );
            do_say( trader, buf );
            return;
        }

        if ( is_number(arg) )
        {
            trade_item = arg2;
            counter = atoi(arg);
        }
        else
        {
            trade_item = arg;
        }

        temp = ch->in_room->area->trade_list;
        while( temp != NULL)
        {
            obj_index = get_obj_index( ch->in_room->area, temp->obj_wnum.vnum );
            if ( is_name( trade_item, trade_table[temp->trade_type].name ) ||
                is_name( trade_item, obj_index->name ) )
            {
                break;
            }
            temp = temp->next;
        }

        if ( temp == NULL )
        {
            sprintf( buf, "Sorry %s, you can't buy that here.", pers( ch, trader ) );
               do_say( trader, buf );
            return;
        }

        if ( counter == -1 )
        {
            /* Check if unit will fit in cart */
            if ( obj_index->weight + get_obj_weight_container( cart ) > (CART(cart)->capacity) ||
                (get_obj_number_container(cart) >= CART(cart)->max_items))
            {
                sprintf( buf, "Your cart is fully laden %s, there is no place to put it.", pers( ch, trader ) );
                do_say( trader, buf );
                return;
            }

            cost = temp->buy_price;
            if ( cost > (ch->silver + (100*ch->gold)))
            {
                sprintf( buf, "You don't have enough money, %s. The price is %ld silver coins.",
                    pers( ch, trader ), temp->buy_price );
                do_say( trader, buf );
                return;
            }

            deduct_cost( ch, cost );

            /* Create object and stick it in the cart */
            pObj = create_object( get_obj_index( ch->in_room->area, temp->obj_wnum.vnum ), 1, true );
            if ( pObj == NULL )
            {
                pbugf(LOG_ERROR, "A commodity object did not exist, vnum was: %d", temp->obj_wnum.vnum );
                return;
            }

            sprintf( buf, "$N places $p in your cart and takes {Y%ld{x silver coins.", temp->buy_price );
            act( buf, ch, trader, NULL, pObj, NULL, NULL, NULL, TO_CHAR, NULL, NULL );

            obj_to_obj( pObj, cart );

            temp->qty--;
        }
        else
        {
            int count = 0;

            /* Check if unit will fit in cart */
            if ( counter*(obj_index->weight + get_obj_weight_container( cart )) > (CART(cart)->capacity) ||
                (get_obj_number_container(cart) + counter >= CART(cart)->max_items))
            {
                sprintf( buf, "Your cart can't hold that much, there is no place to put it." );
                do_say( trader, buf );
                return;
            }

            cost = temp->buy_price * counter;
            if ( cost > ch->silver + (100*ch->gold))
            {
                sprintf( buf, "You don't have enough money, %s. The price is %ld silver coins.",
                    pers( ch, trader ), cost );
                do_say( trader, buf );
                return;
            }

            deduct_cost( ch, cost );
            for (count = 0; count < counter; count++)
            {
                /* Create object and stick it in the cart */
                pObj = create_object( get_obj_index( ch->in_room->area, temp->obj_wnum.vnum ), 1, true );
                if ( pObj == NULL )
                {
                    pbugf(LOG_ERROR, "A commodity object did not exist, vnum was: %d", temp->obj_wnum.vnum );
                    return;
                }
                obj_to_obj( pObj, cart );
                temp->qty--;
            }

            sprintf( buf, "$N places %d units of $p in your cart and takes {Y%ld{x silver coins.", counter, cost );
            act( buf, ch, trader, NULL, pObj, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
            return;
        }

        return;
    }

    //////////////////////////////////////////
    //
    // PLANE TUNNELER - TODO: REWORK
    // REWORKED WITH THE POWER OF... SCRIPTING!
    //////////////////////////////////////////
/*
    if (plane_tunneler != NULL)
    {
        int i;
        bool found = false;

//		TODO: TRIG_PREBUY_TUNNELER
//		if(p_percent_trigger(plane_tunneler, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBUY, NULL))
//			return;

        if (ch->pulled_cart != NULL) {
            act("You can't go into the plane tunnels with $p.", ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR);
            return;
        }

        argument = one_argument(argument, arg);

        i = 0;
        while (tunneler_place_table[i].name != NULL)
        {
            if (is_name(arg, tunneler_place_table[i].name))
            {
                found = true;
                break;
            }

            i++;
        }

        if (!found)
        {
            sprintf(buf, "That's not a place, %s.", pers(ch, plane_tunneler));
            do_say(plane_tunneler, buf);
            return;
        }

        cost = tunneler_place_table[i].price;
        if (cost > ch->silver + (100*ch->gold))
        {
            sprintf(buf, "You don't have enough money, %s. The price is %d silver coins.",
                pers(ch, plane_tunneler), tunneler_place_table[i].price);
            do_say(plane_tunneler, buf);
            return;
        }

        deduct_cost(ch, cost);

        act("$n opens up a wavering magical tunnel.", plane_tunneler, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        act("{YYou step through the tunnel.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
        act("{Y$n steps through the tunnel.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

        char_from_room(ch);
        char_to_room(ch, get_room_index_global(tunneler_place_table[i].vnum));

        do_function(ch, &do_look, "auto");
        return;
    }
*/

    //////////////////////////////////////////
    //
    // PET SHOP
    //
    //////////////////////////////////////////
#if 0
    if (IS_SET(ch->in_room->room_flag[0], ROOM_PET_SHOP))
    {
        CHAR_DATA *pet;
        ROOM_INDEX_DATA *pRoomIndexNext;
        ROOM_INDEX_DATA *in_room;

        if (IS_NPC(ch))
            return;

//		TODO: TRIG_PREBUY_PET
//		if(p_percent_trigger(NULL, NULL, ch->in_room, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBUY, NULL))
//			return;

        smash_tilde(argument);
        argument = one_argument(argument,arg);

        pRoomIndexNext = get_room_index(ch->in_room->vnum + 1);
        if (pRoomIndexNext == NULL)
        {
            pbugf(LOG_ERROR, "Do_buy: bad pet shop at vnum %d.", ch->in_room->vnum);
            send_to_char("Sorry, you can't buy that here.\n\r", ch);
            return;
        }

        in_room		= ch->in_room;
        ch->in_room	= pRoomIndexNext;
        pet			= get_char_room(ch, NULL, arg);
        ch->in_room	= in_room;

        if (pet == NULL || !IS_SET(pet->act[0], ACT_PET))
        {
            send_to_char("Sorry, you can't buy that here.\n\r", ch);
            return;
        }

        if (ch->tot_level < pet->level)
        {
            send_to_char("You're not powerful enough to master this pet.\n\r", ch);
            return;
        }

        if (ch->pet != NULL)
        {
            send_to_char("You already own a pet.\n\r",ch);
            return;
        }

        cost = 10 * pet->level * pet->level;

        /* haggle */
        roll = number_percent();
        if (roll < get_skill(ch,skill_resolve_gsn("haggle")))
        {
            cost -= cost / 3 * roll / 100;
            /*sprintf(buf,"You haggle the price down to %d coins.\n\r",cost);*/
            /*send_to_char(buf,ch);*/
            haggled = true;
            check_improve(ch,skill_resolve_gsn("haggle"),true,4);
        }

        if ((ch->silver + 100 * ch->gold) < cost)
        {
            send_to_char("You can't afford it.\n\r", ch);
            return;
        }

        deduct_cost(ch,cost);
        pet = create_mobile(pet->pIndexData, false);
        SET_BIT(pet->act[0], ACT_PET);
        SET_BIT(pet->affected_by[0], AFF_CHARM);
        pet->comm = COMM_NOTELL|COMM_NOCHANNELS;

        argument = one_argument(argument, arg);
        if (arg[0] != '\0')
        {
            sprintf(buf, "%s %s", pet->name, arg);
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
        if (haggled) {
            sprintf(buf,"You haggle the price down to %ld coins.\n\r",cost);
            send_to_char(buf, ch);
        }
        send_to_char("Enjoy your pet.\n\r", ch);
        act("$n bought $N as a pet.", ch, pet, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
        return;
    }
    else
#endif
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

        argument = one_argument(argument, arg_keeper);

        if ((keeper = find_keeper(ch, arg_keeper)) == NULL)
            return;

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
                keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
            ch->reply = keeper;
            return;

        }

        if (number < 1 || number > 150)
        {
            act("{R$n tells you 'Get real!'{x",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
            return;
        }

        if( request.obj != NULL )
        {
            //
            // Attempting to buy a non-stock object
            obj = request.obj;

            keeper->tempstore[0] = number;
            int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PREBUY_OBJ, NULL);
            if( ret > 0 ) return;	// Messages should be done in the script
            if( ret < 0 )
            {
                // Error happened.  Give generic message
                act("{R$n tells you 'I can't sell that'.{x",
                    keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
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
                    keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
                ch->reply = keeper;
                return;
            }

            cost = cost * number;
            if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
            {
                /* haggle */
                roll = number_percent();
                if (roll < get_skill(ch,skill_resolve_gsn("haggle")))
                {
                    cost -= ((cost/2) * roll)/100;
                    haggled = true;
                    check_improve(ch,skill_resolve_gsn("haggle"),true,4);
                }
            }

            if ((ch->silver + ch->gold * 100) < cost)
            {
                if (number > 1)
                    act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT, NULL, NULL);
                else
                    act("{R$n tells you 'You can't afford to buy $p'.{x", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
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
                act("You haggle with $N.",ch,keeper, NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);

            if (number > 1)
            {
                sprintf(buf,"$n buys $p[%d].",number);
                act(buf,ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
                sprintf(buf,"You buy $p[%d] for%s.", number, get_shop_purchase_price(cost, 0, 0, 0));
                act(buf,ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            }
            else
            {
                act("$n buys $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                sprintf(buf,"You buy $p for%s.", get_shop_purchase_price(cost, 0, 0, 0));
                act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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

            if (!has_stock_reputation(ch, stock))
            {
                act("{R$n tells you 'You do not have the standing to purchase that.'{x",
                    keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                ch->reply = keeper;
                return;
            }

            // Attempting to buy from stock
            keeper->tempstore[0] = number;
            keeper->tempstore[1] = stock->type;
            keeper->tempstore[2] = stock->entity.wnum.vnum;
            int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREBUY, stock->custom_keyword);
            if( ret > 0 ) return;	// Messages should be done in the script
            if( ret < 0 )
            {
                // Error happened.  Give generic message
                act("{R$n tells you 'I can't sell that'.{x",
                    keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
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
                act("{R$n tells you 'I do not have that many.'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
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
                int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CHECK_BUYER, stock->custom_keyword);

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

            int chance = get_skill(ch, skill_resolve_gsn("haggle"));
            long new_value = 0;

            if( IS_NULLSTR(stock->custom_price) )
            {

                // Check price
                long silver = haggle_price(ch, keeper, chance, number, stock->silver, (ch->silver + 100*ch->gold), stock->discount, &haggled, false);
                if( silver < 0 )
                    return;

                long qp = haggle_price(ch, keeper, chance, number, stock->qp, ch->questpoints, stock->discount, &haggled, false);
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
                    act("You haggle with $N.",ch,keeper, NULL, NULL, NULL, NULL, NULL,TO_CHAR, NULL, NULL);
                    check_improve(ch,skill_resolve_gsn("haggle"),true,4);
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
                    ch->questpoints -= qp;
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
                keeper->tempstore[2] = stock->entity.wnum.vnum;
                keeper->tempstore[3] = UMAX(chance, 0);
                free_string(keeper->tempstring);
                keeper->tempstring = &str_empty[0];
                int ret = p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_CUSTOM_PRICE, stock->custom_keyword);
                if( ret > 0 ) return;	// Messages should be done in the script
                if( ret < 0 )
                {
                    if (number > 1)
                        act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
                    else
                        act("{R$n tells you 'You can't afford to buy that'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                    ch->reply = keeper;
                    return;
                }

                // Account for the fact that the value will not change if no script is called
                //  - to activate, do altermob $(self) tempstore3 = -1
                haggled = (keeper->tempstore[3] < 0);

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
                            act(buf,ch, NULL, NULL, t_obj, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
                            sprintf(buf,"You buy $p[%d]",number);

                            if( pricestr[0] != '\0')
                            {
                                strcat(buf, " for");
                                strcat(buf, pricestr);
                            }
                            strcat(buf, ".");
                            first = false;

                            act(buf,ch, NULL, NULL, t_obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
                        }
                        else
                        {
                            act("$n buys $p.", ch, NULL, NULL, t_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                            sprintf(buf,"You buy $p");

                            if( pricestr[0] != '\0')
                            {
                                strcat(buf, " for");
                                strcat(buf, pricestr);
                            }
                            strcat(buf, ".");

                            act(buf, ch, NULL, NULL, t_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                        }

                    }

                    obj_to_char(t_obj, ch);

                    // Prepare it for selling to vendors
                    if( t_obj->cost <= 0 )
                        t_obj->cost = new_value;

                    // Handles what happens AFTER you've bought the item.  Called for EVERY item
                    p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, t_obj, NULL, TRIG_BUY, NULL);

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
                        act("$n explodes into thin air!", pet, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                        char_from_room(pet);
                        extract_char(pet, true);
                        return;
                    }

                    ch->pet = pet;
                    mob = pet;

                    act("$n buys $N.", ch, pet, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    sprintf(buf,"You buy $N");

                }
                else if( stock->type == STOCK_MOUNT )
                {
                    CHAR_DATA *mount = create_mobile(stock->mob, false);

                    char_to_room(mount, ch->in_room);
                    mob = mount;

                    act("$n buys $N.", ch, mount, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    sprintf(buf,"You buy $N");
                }
                else if( stock->type == STOCK_GUARD )
                {
                    CHAR_DATA *guard = create_mobile(stock->mob, false);

                    char_to_room(guard, ch->in_room);
                    act("$n appears to salute $N.", guard, ch, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                    add_follower(guard, ch,true);
                    if (!add_grouped(guard, ch,true))
                    {
                        act("$n returns to $s quarters.", guard, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                        char_from_room(guard);
                        extract_char(guard, true);
                        return;
                    }

                    mob = guard;

                    act("$n hires $N.", ch, guard, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                    sprintf(buf,"You hire $N");
                }
                else if( stock->type == STOCK_CREW )
                {
                    CHAR_DATA *crew = create_mobile(stock->mob, false);

                    list_appendlink(target_ship->crew, crew);
                    crew->belongs_to_ship = target_ship;

                    char_to_room(crew, target_ship->instance->entrance);

                    act("{W$n boards {x$T{W.{x", crew, NULL, NULL, NULL, NULL, NULL, target_ship->ship_name, TO_ROOM, NULL, NULL);

                    mob = crew;

                    act("$n hires $N.", ch, crew, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
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

                act(buf, ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                if( stock->duration > 0 )
                {
                    // They are only here for a limited time
                    SET_BIT(mob->act[1], ACT2_HIRED);
                    mob->hired_to = current_time + stock->duration * 60;
                }

                p_percent_trigger(mob, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
                p_percent_trigger(keeper, NULL, NULL, NULL, ch, mob, NULL, NULL, NULL, TRIG_BUY, NULL);
            }
            else if( stock->ship != NULL )
            {
                WNUM wnum = { stock->ship->area, stock->ship->vnum };
                SHIP_DATA *ship = purchase_ship(ch, wnum, keeper->shop);

                if( !IS_VALID(ship) )
                {
                    // An error has occured!
                    send_to_char("{RERROR: {WSomething has occured in purchasing the ship.  Please notify an IMP.{x\n\r", ch);
                    return;
                }

                sprintf(buf, "$n buys %s %s.", get_article(stock->ship->name, false), stock->ship->name);
                act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                sprintf(buf, "You buy %s %s.", get_article(stock->ship->name, false), stock->ship->name);
                act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                if( IS_NULLSTR(keeper->shop->shipyard_description) )
                {
                    sprintf(buf, "You may find your %s in the nearby harbor.", stock->ship->name);
                }
                else
                {
                    sprintf(buf, "You may find your %s %s.", stock->ship->name, keeper->shop->shipyard_description);
                }

                act("{C$n says to $N, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_NOTVICT, NULL, NULL);
                act("{C$n says to you, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_VICT, NULL, NULL);
                act("{CYou say to $N, '$T{C'{x", keeper, ch, NULL, NULL, NULL, NULL, buf, TO_CHAR, NULL, NULL);

                act("{xTo board your ship, use '{Yenter $T{x' until you give it a name.{x", ch, NULL, NULL, NULL, NULL, NULL, ch->name, TO_CHAR, NULL, NULL);

                p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, ship->ship, NULL, TRIG_BUY, NULL);
            }
            else
            {
                if (number > 1)
                {
                    sprintf(buf,"$n buys $T[%d].",number);
                    act(buf,ch, NULL, NULL, NULL, NULL, NULL, stock->custom_descr,TO_ROOM, NULL, NULL);
                    sprintf(buf,"You buy $T[%d]",number);
                }
                else
                {
                    act("$n buys $T.", ch, NULL, NULL, NULL, NULL, NULL, stock->custom_descr, TO_ROOM, NULL, NULL);
                    sprintf(buf,"You buy $T");
                }

                if( pricestr[0] != '\0' )
                {
                    strcat(buf, " for");
                    strcat(buf, pricestr);
                }
                strcat(buf, ".");

                act(buf,ch, NULL, NULL, NULL, NULL, NULL,stock->custom_descr,TO_CHAR, NULL, NULL);

                keeper->tempstore[0] = number;						// Number of units
                keeper->tempstore[1] = UMAX(stock->duration, 0);	// Duration of product / service.
                // Just do the giving of items
                // All cost transactions have taken place, along with their messages
                p_percent_trigger(keeper, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BUY, stock->custom_keyword);
            }

            // Reduce the available stock when it is limited
            if( stock->quantity > 0 )
                stock->quantity -= number;

        }
    }
}


/**
 * do_blow - Blow a whistle item
 *
 * Uses ITEM_WHISTLE objects. Special handling for airship whistle
 * (reserved vnum) which summons the Plith airship to the player's
 * wilderness location.
 *
 * @param ch        Character blowing the whistle
 * @param argument  Whistle item to blow
 *
 * Triggers: TRIG_BLOW (on object)
 */
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

    act( "$n puts $p to $s lips and blows.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    act( "You put $p to your lips and blow.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL );

    if ( obj->pIndexData == get_reserved_obj_index("obj_airship_whistle") )
  {
    act( "The whistle glows vibrantly, then fades.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL );
    if ( !IN_WILDERNESS(ch) )
    {
      act( "{CA quiet voice whispers, 'You must be in the wilderness to be picked up.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
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
      act( "{CA quiet voice whispers, 'The airship is unavailable at the moment. Please try again later.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
    }
    else
    {
      act( "{CA quiet voice whispers, 'The airship is on it's way.'{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL );
      plith_airship->captain->ship_depart_time = 80;
      plith_airship->captain->ship_dest_x = ch->in_room->x;
      plith_airship->captain->ship_dest_y = ch->in_room->y;
    }
    return;
  }

    p_percent_trigger( NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_BLOW , NULL);

    return;
}

/**
 * do_list - Display items available for sale at a shop
 *
 * Shows inventory and stock items from a shopkeeper with prices.
 * Supports filtering by item name.
 *
 * Display includes:
 * - Item level range
 * - Stock quantity (if limited)
 * - Price in silver/gold or special currencies (DP, QP, etc.)
 * - Object/mob/ship/custom items
 *
 * Special handling for ROOM_SHIP_SHOP displays ship purchase menu.
 *
 * @param ch        Character browsing shop
 * @param argument  Optional keeper name and item filter
 */
void do_list(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg_keeper[MIL];


    argument = one_argument(argument, arg_keeper);
    one_argument(argument,arg);

    if (IS_SET(ch->in_room->room_flag[0], ROOM_SHIP_SHOP))
    {
        send_to_char("Min Crew   Max Crew  Max Cannons  Kg Capacity    Minimum Rank  Price   Name\n\r", ch); 
        send_to_char("                                                 {Y(GOLD){x\n\r", ch);
        send_to_char("{B-----------------------------------------------------------------------------{x\n\r", ch);
        send_to_char("   1          5         10           1000        None         {GFree{x    Sailing Boat\n\r", ch);
        send_to_char("   5          15        25          10000        Explorer     {GFree{x    Cargo Ship\n\r", ch);
        send_to_char("   15         32        40          25000        Captain      {GFree{x    Galleon\n\r", ch);
        send_to_char("   10         30        50           7500        Commander    {GFree{x    Frigate\n\r", ch);
        return;
    }
    else
    {
        CHAR_DATA *keeper;
        OBJ_DATA *obj;
        int cost,count;
        bool found;

        if ((keeper = find_keeper(ch, arg_keeper)) == NULL)
            return;

        one_argument(argument,arg);

        found = false;
        SHOP_STOCK_DATA *stock;
        for (stock = keeper->shop->stock; stock; stock = stock->next)
        {
            // Hide it if it's out of stock
            if( stock->max_quantity > 0 && stock->quantity < 1) continue;
            if( !can_see_stock_reputation(ch, stock) ) continue;

            switch(stock->type)
            {
            case STOCK_OBJECT:
                if( stock->entity.wnum.vnum > 0 && stock->obj != NULL )
                {
                    if( arg[0] != '\0' &&
                        !is_name(arg, stock->obj->name) &&
                        (IS_NULLSTR(stock->obj->list_keywords) || !is_name(arg, stock->obj->list_keywords)) )
                        continue;

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
                        IS_NULLSTR(stock->custom_descr)
                            ? (IS_NULLSTR(stock->obj->list_name) ? stock->obj->short_descr : stock->obj->list_name)
                            : stock->custom_descr;

                    if ( stock->max_quantity > 0 && (stock->duration > 0 || stock->obj->timer > 0))
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s {Y[EXPIRES]{X\n\r", level,pwidth,pricing,stock->quantity,descr);
                    }
                    else if( stock->max_quantity > 0 )
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s\n\r", level,pwidth,pricing,stock->quantity,descr);
                    }
                    else if (stock->duration > 0)
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s {Y[EXPIRES]{X\n\r", level,pwidth,pricing,descr);
                    }
                    else
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s\n\r", level,pwidth,pricing,descr);
                    }

                    send_to_char(buf, ch);
                }
                break;

            case STOCK_PET:
            case STOCK_MOUNT:
            case STOCK_GUARD:
            case STOCK_CREW:
                if( stock->entity.wnum.vnum > 0 && stock->mob != NULL )
                {
                    if( arg[0] != '\0' &&
                        !is_name(arg, stock->mob->player_name) &&
                        (IS_NULLSTR(stock->mob->list_keywords) || !is_name(arg, stock->mob->list_keywords)) )
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
                        IS_NULLSTR(stock->custom_descr)
                            ? (IS_NULLSTR(stock->mob->list_name) ? stock->mob->short_descr : stock->mob->list_name)
                            : stock->custom_descr;
                    if (stock->max_quantity > 0 && stock->duration > 0)
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s {Y[HIRELING]{x\n\r", level,pwidth,pricing,stock->quantity,descr);
                    }
                    else if( stock->max_quantity > 0 )
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y%4d{B ]{x %s\n\r", level,pwidth,pricing,stock->quantity,descr);
                    }
                    else if (stock->duration > 0)
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s {Y[HIRELING]{x\n\r", level,pwidth,pricing,descr);
                    }
                    else
                    {
                        sprintf(buf,"{B[{x%3d %*s {Y ---{B ]{x %s\n\r", level,pwidth,pricing,descr);
                    }

                    send_to_char(buf, ch);
                }
                break;

            case STOCK_SHIP:
                if( stock->entity.wnum.vnum > 0 && stock->ship != NULL )
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

        ITERATOR it;
        iterator_start(&it, keeper->lcarrying);
        while ((obj = (OBJ_DATA *)iterator_nextdata(&it)))
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

                // Count identical items
                ITERATOR inner_it;
                OBJ_DATA *t_obj;
                iterator_start(&inner_it, keeper->lcarrying);
                while ((t_obj = (OBJ_DATA *)iterator_nextdata(&inner_it)))
                {
                    if (t_obj != obj &&
                        IS_OBJ_STAT(t_obj, ITEM_INVENTORY) &&
                        obj->pIndexData == t_obj->pIndexData &&
                        !str_cmp(obj->short_descr, t_obj->short_descr))
                    {
                        count++;
                    }
                }
                iterator_stop(&inner_it);

                sprintf(buf,"{B[{x%3d %14d {Y%4d{B ]{x %s\n\r", obj->level,cost,count,obj->short_descr);

                send_to_char(buf, ch);
            }
        }
        iterator_stop(&it);

        if (!found)
            send_to_char("You can't buy anything here.\n\r", ch);
        return;
    }
}

/**
 * do_inspect - Get detailed information about shop items
 *
 * Asks a shopkeeper for lore/identify information about an item
 * before purchasing. Creates temporary object/mob to inspect.
 *
 * Supports:
 * - Objects: Shows spell_identify output
 * - Mobs (pets/mounts/guards): Shows basic_mob_lore
 * - Custom stock items: TRIG_INSPECT_CUSTOM handler
 *
 * ITEM_NO_LORE and ACT_NO_LORE block inspection.
 *
 * @param ch        Character inspecting
 * @param argument  Keeper name and item to inspect
 *
 * Triggers: TRIG_INSPECT_CUSTOM (on keeper for custom stock)
 */
void do_inspect(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg_keeper[MIL];
    char buf[MSL];
    time_t hiring_time = time(NULL);
    CHAR_DATA *keeper;
    SHOP_REQUEST_DATA request;

    argument = one_argument(argument, arg_keeper);
    one_argument(argument, arg);

    if ((keeper = find_keeper(ch, arg_keeper)) == NULL)
        return;


    bool found = get_stock_keeper(ch, keeper, &request, arg);

    if(!found)
    {
        act("{R$n tells you 'I don't sell that product. Maybe there is something else you would like to inspect?'{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        ch->reply = keeper;
        return;
    }

    if( request.obj != NULL )
    {
        if( IS_SET(request.obj->extra[1], ITEM_NO_LORE) )
        {
            act("{R$N tells you 'Sorry, I do not have any information about $p.'{x", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            ch->reply = keeper;
            return;
        }

        act("You ask $N for some information about $p.", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n asks $N for some information about $p.", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        spell_identify(skill_find("_inspect"), ch->tot_level, ch, request.obj, TARGET_OBJ, WEAR_NONE, INVOC_INTERNAL);
    }
    else if( request.stock != NULL )
    {
        if( request.stock->obj != NULL )
        {
            OBJ_DATA *obj = create_object(request.stock->obj, 0, true);

            if( IS_SET(obj->extra[1], ITEM_NO_LORE) )
            {
                act("{R$N tells you 'Sorry, I do not have any information about $p.'{x", ch, keeper, NULL, request.obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                ch->reply = keeper;
            }
            else
            {
                act("You ask $N for some information about $p.", ch, keeper, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n asks $N for some information about $p.", ch, keeper, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                obj_to_char(obj, ch);
                if (request.stock->duration > 0)
                {
                    sprintf(buf, "{YExpires After{y:{X %d hours{x\n\r", request.stock->duration);
                    send_to_char(buf, ch);
                }
                spell_identify(skill_find("_inspect"), ch->tot_level, ch, obj, TARGET_OBJ, WEAR_NONE, INVOC_INTERNAL);
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
                act("{R$n tells you 'Sorry, I do not have any information about $N.'{x", keeper, mob, ch, NULL, NULL, NULL, NULL, TO_THIRD, NULL, NULL);
                ch->reply = keeper;
            }
            else
            {
                act("You ask $N for some information about $v.", ch, keeper, mob, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$n asks $N for some information about $v.", ch, keeper, mob, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                if( request.stock->type == STOCK_PET )
                {
                    act("{GPET{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                }
                else if( request.stock->type == STOCK_PET )
                {
                    act("{GMOUNT{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                }
                else
                {
                    act("{GGUARD{g:{x $N", ch, mob, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                }
                if (request.stock->duration > 0)
                {
                    char hired_time[100];
                    hiring_time = current_time + request.stock->duration * 60;
                    strftime(hired_time, 100, "%a %b %d %X %Z %Y", localtime(&hiring_time));
                    sprintf(buf, "{AHired Until{a:{X %s{x\n\r", hired_time);
                    send_to_char(buf, ch);
                }

                show_basic_mob_lore(ch, mob);
            }

            extract_char(mob, true);
        }
        else if( !IS_NULLSTR(request.stock->custom_keyword) )
        {
            if(!p_exact_trigger(request.stock->custom_keyword, keeper, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_INSPECT_CUSTOM))
            {
                act("{R$N tells you 'Sorry, I do not have any information about $T.'{x", ch, keeper, NULL, NULL, NULL, NULL, request.stock->custom_descr, TO_CHAR, NULL, NULL);
                ch->reply = keeper;
                return;
            }
        }
        else
        {
            act("{R$n tells you 'I don't sell that product. Maybe there is something else you would like to inspect?'{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
            ch->reply = keeper;
            return;
        }
    }
}


/**
 * do_sell - Sell items to a shopkeeper
 *
 * Sells items from inventory to a shopkeeper for gold.
 * Shop must accept the item type in its buy_type[] array.
 *
 * Features:
 * - Haggling with skill_resolve_gsn("haggle") skill for better prices
 * - Commodity trading via ACT2_TRADER NPCs (cart system)
 * - Price reduction if shop already has duplicate
 * - TRIG_PRESELL can cancel sale
 *
 * Restrictions:
 * - Cannot sell ITEM_NOUNCURSE items
 * - Cannot sell if shop is closed (hours)
 * - Cannot sell if shop doesn't deal in item type
 *
 * @param ch        Character selling
 * @param argument  Keeper name and item to sell
 *
 * Triggers: TRIG_PRESELL (can cancel), TRIG_SELL (after sale)
 */
void do_sell(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg_keeper[MIL];
    CHAR_DATA *keeper = NULL;
    CHAR_DATA *trader = NULL;
    CHAR_DATA *mob = NULL;
    OBJ_DATA *obj = NULL;
    int cost,roll;

    argument = one_argument(argument, arg_keeper);
    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Sell what?\n\r", ch);
        return;
    }

    for (mob = ch->in_room->people; mob != NULL; mob = mob->next_in_room)
    {
        if (IS_SET(mob->act[1], ACT2_TRADER) && IS_NPC(mob))
        {
            trader = mob;
            break;
        }
    }

    if (trader != NULL)
    {
        TRADE_ITEM *temp;
        OBJ_INDEX_DATA *obj_index;
        OBJ_DATA *pObj;
        OBJ_DATA *cart;

        // TODO: TRIG_PRESELL_TRADER
//		if(p_percent_trigger(trader, NULL, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PRESELL, NULL))
//			return;

        argument = one_argument(argument, arg);

        if ((cart = ch->pulled_cart) == NULL)
        {
            sprintf(buf, "%s, you aren't pulling a cart!", pers(ch, trader));
            do_say(trader, buf);
            return;
        }

        for(pObj = cart->contains; pObj != NULL; pObj = pObj->next_content)
        {
            if (is_name(arg, pObj->name))
            {
                break;
            }
        }

        if (pObj == NULL)
        {
            sprintf(buf, "You can't sell what you don't have %s!", pers(ch, trader));
            do_say(trader, buf);
            return;
        }

        for(temp = ch->in_room->area->trade_list; temp != NULL; temp = temp->next)
        {
            obj_index = get_obj_index(ch->in_room->area, temp->obj_wnum.vnum);
            if (TRADE(obj_index)->trade_type == TRADE(pObj)->trade_type)
            {
                break;
            }
        }

        if (temp == NULL)
        {
            sprintf(buf, "Sorry %s, we aren't buying that commodity at the moment.", pers(ch, trader));
            do_say(trader, buf);
            return;
        }

        sprintf(buf, "You sell %s for {Y%ld{x gold and {Y%ld{x silver.\n\r",
            pObj->short_descr,
            temp->sell_price/100,
            temp->sell_price - (temp->sell_price/100) * 100);
        send_to_char(buf, ch);

        ch->gold	+= temp->sell_price/100;
        ch->silver	+= temp->sell_price - (temp->sell_price/100) * 100;
        temp->qty++;

        obj_from_obj(pObj);

        return;
    }

    if (trader == NULL && (keeper = find_keeper(ch, arg_keeper)) == NULL)
        return;

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
        act("{R$n tells you 'You don't have that item'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        ch->reply = keeper;
        return;
    }

    if(p_percent_trigger(trader, NULL, NULL, NULL, ch, NULL, NULL, obj, NULL, TRIG_PRESELL, NULL))
        return;

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
        send_to_char("You can't let go of it.\n\r", ch);
        return;
    }

    if (!can_see_obj(keeper,obj))
    {
        act("$n doesn't see what you are offering.",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
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
                int chance = get_skill(ch, skill_resolve_gsn("haggle"));

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
                            keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT, NULL, NULL);
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

                act("$n sells $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

                if( haggled ) {
                    send_to_char("You haggle with the shopkeeper.\n\r",ch);
                    check_improve(ch,skill_resolve_gsn("haggle"),true,4);
                }

                sprintf(buf, "You sell $p for%s.", get_shop_purchase_price(silver, qp, dp, pneuma));
                act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

                ch->gold		+= silver/100;
                ch->silver		+= silver%100;
                ch->questpoints	+= qp;
                ch->deitypoints	+= dp;
                ch->pneuma		+= pneuma;

                deduct_cost(keeper,silver);
                if (keeper->gold < 0)
                    keeper->gold = 0;
                if (keeper->silver< 0)
                    keeper->silver = 0;

                if (obj->item_type == ITEM_TRASH)
                {
                    plogf(LOG_INFO,"Item sell extract");
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
            act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
            return;
        }
    }

    // Not a part of the stock, or not eligible for stock refund
    if ((cost = get_cost(keeper, obj, false)) <= 0)
    {
        act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        return;
    }
    if (cost > (keeper-> silver + 100 * keeper->gold))
    {
        act("{R$n tells you 'I'm afraid I don't have enough wealth to buy $p.{x",
            keeper,ch, NULL, obj, NULL, NULL, NULL,TO_VICT, NULL, NULL);
        ch->reply = keeper;
        return;
    }

    act("$n sells $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    if( !IS_SET(keeper->shop->flags, SHOPFLAG_NO_HAGGLE) )
    {
        /* haggle */
        roll = number_percent();
        if (roll < get_skill(ch,skill_resolve_gsn("haggle")))
        {
            send_to_char("You haggle with the shopkeeper.\n\r",ch);
            cost += obj->cost / 2 * roll / 100;
            cost = UMIN(cost,95 * get_cost(keeper,obj,true) / 100);
            cost = UMIN(cost,(keeper->silver + 100 * keeper->gold));
            check_improve(ch,skill_resolve_gsn("haggle"),true,4);
        }
    }
    sprintf(buf, "You sell $p for%s", get_shop_purchase_price(cost, 0, 0, 0));
    act(buf, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    ch->gold	+= cost/100;
    ch->silver	+= cost - (cost/100) * 100;
    deduct_cost(keeper,cost);
    if (keeper->gold < 0)
        keeper->gold = 0;
    if (keeper->silver< 0)
        keeper->silver = 0;

    if (obj->item_type == ITEM_TRASH)
    {
        plogf(LOG_INFO,"Item sell extract");
        extract_obj(obj);
    }
    else
    {
        obj_from_char(obj);
        obj->timer = number_range(100,250);
        obj_to_keeper(obj, keeper);
    }
}


/**
 * do_value - Get price quote for selling an item
 *
 * Asks a shopkeeper how much they would pay for an item
 * without actually selling it.
 *
 * @param ch        Character getting quote
 * @param argument  Keeper name and item to value
 */
void do_value(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg_keeper[MIL];
    CHAR_DATA *keeper;
    OBJ_DATA *obj;
    int cost;

    argument = one_argument(argument, arg_keeper);
    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("Value what?\n\r", ch);
    return;
    }

    if ((keeper = find_keeper(ch, arg_keeper)) == NULL)
    return;

    if ((obj = get_obj_carry(ch, arg, ch)) == NULL)
    {
    act("{R$n tells you 'You don't have that item'.{x",
        keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    ch->reply = keeper;
    return;
    }

    if (!can_see_obj(keeper,obj))
    {
        act("$n doesn't see what you are offering.",keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
        return;
    }

    if (!can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
    send_to_char("You can't let go of it.\n\r", ch);
    return;
    }

    if ((cost = get_cost(keeper, obj, false)) <= 0)
    {
    act("$n looks uninterested in $p.", keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    return;
    }

    sprintf(buf,
    "{R$n tells you 'I'll give you %d silver and %d gold coins for $p'.{x",
    cost - (cost/100) * 100, cost/100);
    act(buf, keeper, ch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    ch->reply = keeper;

    return;
}


/**
 * do_secondary - Wield a weapon in the off-hand
 *
 * Equips a weapon to the WEAR_SECONDARY slot for dual wielding.
 *
 * Requirements:
 * - Must already have a primary weapon wielded
 * - Weapon must be ITEM_WIELD flagged
 * - Cannot dual wield two-handed weapons (except SIZE_GIANT+)
 * - Cannot dual wield spear/polearm with another spear/polearm
 * - Character level must meet weapon level
 * - Weapon must not be broken (condition 0)
 *
 * @param ch        Character dual wielding
 * @param argument  Weapon to wield in off-hand
 */
void do_secondary(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    OBJ_DATA *weapon;
    char buf[MAX_STRING_LENGTH];

    if (check_social_status(ch))
        return;

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

    if (!CAN_WEAR(obj, ITEM_WIELD)
     || obj->pIndexData->item_type != ITEM_WEAPON)
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
            act("Strong as you are, there's still no way you could dual wield $P.", ch, NULL, NULL, NULL, obj, NULL, NULL, TO_CHAR, NULL, NULL);
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
            ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    /* you can't dual wield spears/polearms */
    if (weapon != NULL
    && (IS_WEAPON_STAT(weapon,WEAPON_TWO_HANDS)
         || WEAPON(weapon)->weapon_class == WEAPON_POLEARM
     || WEAPON(weapon)->weapon_class == WEAPON_SPEAR))
    {
    if(WEAPON(obj)->weapon_class == WEAPON_SPEAR
    || WEAPON(obj)->weapon_class == WEAPON_POLEARM)
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
            ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    return;
    }


    if (!remove_obj(ch, WEAR_SECONDARY, true))
        return;

    act ("$n wields $p in $s off-hand.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
    act ("You wield $p in your off-hand.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
    equip_char (ch, obj, WEAR_SECONDARY);
}


/**
 * do_push - Push an object or interact with pushable items
 *
 * Interacts with objects via pushing. Primary uses:
 * - TRIG_PUSH: Simple push action on object
 * - TRIG_PUSH_ON: Push with target argument
 * - CONT_PUSHOPEN containers: Opens when pushed
 *
 * Searches room contents first, then character inventory.
 *
 * @param ch        Character pushing
 * @param argument  Object to push and optional target
 *
 * Triggers: TRIG_PUSH, TRIG_PUSH_ON, TRIG_OPEN (for containers)
 */
void do_push(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    OBJ_DATA *obj;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
    send_to_char("What do you want to push?\n\r", ch);
    return;
    }

    if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
    {
        if ((obj = get_obj_list(ch, arg, ch->lcarrying)) == NULL)
    {
        send_to_char ("You can't find it.\n\r",ch);
        return;
    }
    }

    /* @@@NIB : 20070121 : Added for the new trigger type*/
    if (argument[0]) {
        if(p_use_on_trigger(ch, obj, TRIG_PUSH_ON, argument)) return;
    } else {
        if(p_use_trigger(ch, obj, TRIG_PUSH)) return;
    }

    /* @@@NIB : 20070126 : for pushopen containers*/
    if(obj->item_type == ITEM_CONTAINER && IS_SET(CONTAINER(obj)->flags, CONT_PUSHOPEN)) {
     if(IS_SET(CONTAINER(obj)->flags, CONT_CLOSED)) {
         if(IS_SET(CONTAINER(obj)->flags, CONT_LOCKED)) {
         send_to_char("It's locked.\n\r", ch);
         return;
         }

         REMOVE_BIT(CONTAINER(obj)->flags, CONT_CLOSED);
         act("You open $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
         act("$n opens $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
         p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_OPEN, NULL);
         return;
     }
    }

    send_to_char("You can't push that.\n\r", ch);
}


/**
 * do_pull - Pull objects, carts, or lodged items
 *
 * Multi-purpose pull command:
 * - Lodged weapons: Dislodges from body (barbed weapons cause damage)
 * - ITEM_CART: Starts pulling cart behind character
 * - Script triggers: TRIG_PULL, TRIG_PULL_ON
 *
 * Cart pulling:
 * - Requires sufficient STR or being mounted
 * - One cart per character
 * - Strips sneak affect
 * - Church relics trigger theft announcements
 *
 * @param ch        Character pulling
 * @param argument  Object/mob to pull and optional target
 *
 * Triggers: TRIG_PULL, TRIG_PULL_ON
 */
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
            if ((obj = get_obj_list(ch, arg, ch->lcarrying)) == NULL) {
                send_to_char ("You can't find it.\n\r",ch);
                return;
            }
        }
    }

    /* @@@NIB : 20070121 : Added for the new trigger type*/
    /*	Also allows for PULL/PULL_ON scripts to drop to the CART code*/
    if (argument[0]) {
        if(p_act_trigger(argument, mob, obj, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PULL_ON)) return;
    } else {
        if(p_percent_trigger(mob, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PULL, NULL)) return;
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
        act("You can't dislodge $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if(!unequip_char(ch,obj,true)) {
        act("$n dislodges $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("You dislodge $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        if(obj->item_type == ITEM_WEAPON && IS_WEAPON_STAT(obj,WEAPON_BARBED)) {
            act("{R$p{R tears away flesh from $n.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
            act("{R$p{R tears away some of your flesh.{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            damage(ch, ch, UMIN(ch->hit,25), 0, 0, true);
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
                ch, obj->pulled_by, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        else
            act("You're already pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

        return;
    }

        if (IS_AFFECTED(ch, AFF_SNEAK))
    {
        send_to_char("You stop moving silently.\n\r", ch);
        affect_strip(ch, skill_resolve_gsn("sneak"));
    }

    if (ch->pulled_cart != NULL)
    {
        act("But you're already pulling $p!",
            ch, NULL, NULL, ch->pulled_cart, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    if (CART(obj)->min_strength > get_curr_stat(ch, STAT_STR) && !MOUNTED(ch))
    {
          act("You aren't strong enough to pull $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n attempts to pull $p but is too weak.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }
    else
    {
        if (MOUNTED(ch))
        {
            act("You hitch $p onto $N.", ch, MOUNTED(ch), NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("$n hitches $p onto $N.", ch, MOUNTED(ch), NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        }
        else
        {
            act("You start pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL,TO_CHAR, NULL, NULL);
            act("$n starts pulling $p.", ch, NULL, NULL, obj, NULL, NULL, NULL,TO_ROOM, NULL, NULL);
        }
    }

        if (is_relic(obj->pIndexData))
    {
        if (ch->church == NULL)
        {
            act("Relics are only usable by players in churches.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
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


/**
 * do_turn - Turn objects or turn undead creatures
 *
 * Dual-purpose command:
 *
 * 1. Turn Undead (skill_resolve_gsn("turn undead") skill):
 *    - Affects undead characters with holy damage
 *    - Success causes damage, flee, and panic/daze
 *    - Chance based on level difference and skill
 *
 * 2. Turn Objects:
 *    - TRIG_TURN: Simple turn action
 *    - TRIG_TURN_ON: Turn with target argument
 *
 * @param ch        Character turning
 * @param argument  Target and optional direction
 *
 * Triggers: TRIG_ATTACK_TURN (pretest), TRIG_TURN, TRIG_TURN_ON
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
    if ((vch = get_char_room(ch, NULL, arg)) && (skill = get_skill(ch, skill_resolve_gsn("turn undead"))) > 0) {
        int chance;

        if(p_percent_trigger(vch,NULL, NULL, NULL, ch, vch, NULL, NULL, NULL, TRIG_ATTACK_TURN,"pretest") ||
            p_percent_trigger(ch,NULL, NULL, NULL, ch, vch, NULL, NULL, NULL, TRIG_ATTACK_TURN,"pretest"))
            return;

        act("{YYou release your divine will over $N!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("{Y$n attempts to turn $N!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        act("{WYou feel a powerful divine presence pass through you!{x",ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);

        WAIT_STATE(ch, skill_table[skill_resolve_gsn("turn undead")].beats);

        if (IS_UNDEAD(vch)) {
            chance = (ch->tot_level - vch->tot_level) + skill / 5;
            if (number_percent() < chance) {
                act("{RYou scream with pain as your flesh sizzles and melts!{x", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                act("{R$n screams with pain as $s flesh sizzles and melts!{x", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
                damage(ch, vch, dice(ch->tot_level, 8), TYPE_UNDEFINED, DAM_HOLY, false);
                do_function(vch, &do_flee, NULL);
                PANIC_STATE(vch, 12);
                DAZE_STATE(vch, 12);
            } else {
                act("You wince, but resist $n's divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                act("$N winces, but resists your divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
                act("$N winces, but resists $n's divine will.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
            }
        } else {
            act("$N is unaffected.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
            act("You are unaffected.", ch, vch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
        }

        return;
    }

    if ((obj = get_obj_list(ch, arg, ch->in_room->contents)) == NULL)
    {
    if ((obj = get_obj_list(ch, arg, ch->lcarrying)) == NULL)
    {
    send_to_char ("You can't find it.\n\r",ch);
    return;
    }
    }

    /* @@@NIB : 20070121 : Added for the new trigger type*/
    if (argument[0]) {
    if(p_use_on_trigger(ch, obj, TRIG_TURN_ON, argument)) return;
    } else {
    if(p_use_trigger(ch, obj, TRIG_TURN)) return;
    }

    send_to_char("You can't turn that.\n\r", ch);
}


/**
 * do_skull - Extract a skull from a player corpse
 *
 * Evil-aligned skill (skill_resolve_gsn("skull")) to take skulls from PC corpses.
 * Creates either a golden skull (CPK death) or normal skull.
 *
 * Requirements:
 * - Must be NPC with alignment < 0, or PC with skill_resolve_gsn("skull")
 * - Target must be ITEM_CORPSE_PC with PART_HEAD
 * - Corpse cannot be immortal level
 *
 * Success chance based on skill/level and corpse type modifiers.
 * Failure destroys the head, leaving a headless corpse.
 *
 * @param ch        Character extracting skull
 * @param argument  Target corpse
 */
void do_skull(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *skull;
    int i;
    int chance, corpse;

    argument = one_argument(argument, arg);

    if ((IS_NPC(ch) && ch->alignment >= 0) || (!IS_NPC(ch) && get_skill(ch,skill_resolve_gsn("skull")) == 0))
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
        act("$p is protected by powers from above.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
    }

    corpse = CORPSE_TYPE(obj);
    const struct corpse_info *corpse_info = corpse_info_by_type(corpse);

    /* Used for corpse types that are impossible to skull even if there is a head...*/
    if (corpse_info->skulling_chance < 0) {
        send_to_char("You can't seem to remove its skull.  It doesn't want to budge.\n\r", ch);
        return;
    }

    if (IS_NPC(ch))
        chance = (ch->tot_level * 3)/4 - obj->level/10;
    else
        chance = get_skill(ch, skill_resolve_gsn("skull")) - 3;

    chance *= corpse_info->skulling_chance;

    if (number_range(1,10000) > chance)
    {
        act(corpse_info->skull_fail, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act(corpse_info->skull_fail_other, ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        check_improve(ch, skill_resolve_gsn("skull"), false, 1);
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

        sprintf(buf, corpse_info->short_headless, obj->owner);
        free_string(obj->short_descr);
        obj->short_descr = str_dup(buf);

        sprintf(buf, corpse_info->long_headless, obj->owner);
        free_string(obj->description);
        obj->description = str_dup(buf);

        sprintf(buf, corpse_info->full_headless, obj->owner);
        free_string(obj->full_description);
        obj->full_description = str_dup(buf);
        return;
    }


    /* 20070521 : NIB : Changed to check based upon where the CORPSE was created,*/
    /*			for when corpses can be dragged.  Used to keep people*/
    /*			from using CPK rooms to skull goldens.  This will have*/
    /*			no affect on looting as object placement is done at the*/
    /*			time of death.*/
    if (IS_SET(CORPSE_FLAGS(obj), CORPSE_CPKDEATH))
        skull = create_object(get_reserved_obj_index("obj_skull_golden"), 0, false);
    else
        skull = create_object(get_reserved_obj_index("obj_skull_normal"), 0, false);

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

    sprintf(buf, corpse_info->short_headless, obj->owner);
    free_string(obj->short_descr);
    obj->short_descr = str_dup(buf);

    sprintf(buf, corpse_info->long_headless, obj->owner);
    free_string(obj->description);
    obj->description = str_dup(buf);

    sprintf(buf, corpse_info->full_headless, obj->owner);
    free_string(obj->full_description);
    obj->full_description = str_dup(buf);

    sprintf(buf, corpse_info->skull_success, obj->owner);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    sprintf(buf, corpse_info->skull_success_other, obj->owner);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    obj_to_char(skull, ch);
    check_improve(ch, skill_resolve_gsn("skull"), true, 1);
    }
    else
        act("There's no $t here.", ch, NULL, NULL, NULL, NULL, arg, NULL, TO_CHAR, NULL, NULL);
}


/**
 * do_brew - Create potions from spells (skill_resolve_gsn("brew") skill)
 *
 * Alchemical skill to brew spells into potions. Requires:
 * - ITEM_EMPTY_VIAL in inventory
 * - Knowledge of skill_resolve_gsn("brew") skill
 * - Knowledge of the target spell
 * - Sufficient mana (2/3 of spell cost)
 *
 * Restrictions:
 * - Only TAR_CHAR_* spells can be brewed
 * - "mass healing" cannot be brewed
 * - Dead characters cannot brew
 *
 * Sets BREW_STATE and brew_sn, calls brew_end() after delay.
 *
 * @param ch        Character brewing
 * @param argument  Spell name to brew
 */
void do_brew(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    int sn;
    int spell;
    int chance;
    int mana;
    char arg[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (IS_DEAD(ch))
    {
        send_to_char("You can't do that. You are dead.\n\r", ch);
        return;
    }

    if ((chance = get_skill(ch,skill_resolve_gsn("brew"))) == 0)
    {
        send_to_char("Brew? What's that?\n\r",ch);
        return;
    }

    obj = NULL;
    // Replace traditional list traversal with iterator for lcarrying
    ITERATOR it;
    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (obj->item_type == ITEM_EMPTY_VIAL || obj->pIndexData == get_reserved_obj_index("obj_empty_vial"))
            break;
    }
    iterator_stop(&it);

    if (obj == NULL)
    {
        send_to_char("You do not have an empty vial to fill.\n\r", ch);
        return;
    }

    sn = 0;

    if (arg[0] == '\0')
    {
        send_to_char("What potion do you want to create?\n\r", ch);
        return;
    }

    sn = find_spell(ch, arg);

    if ((sn) < 1
    || skill_table[sn].spell_fun == spell_null
    || get_skill(ch, sn) == 0)
    {
        send_to_char("You don't know any spells of that name.\n\r", ch);
        return;
    }

    mana = 0;
    if (sn > 0)
    {
        mana += skill_table[sn].min_mana;
        mana = mana * 2 / 3;
    }

    if (ch->mana < mana)
    {
        send_to_char("You don't have enough mana to brew that potion.\n\r", ch);
        return;
    }

    ch->mana -= mana;

    /* Mass healing must not be one of the spells*/
    spell = find_spell(ch, "mass healing");
    if (spell == sn)
    {
        send_to_char("The vial explodes into dust!\n\r", ch);
        act("$n's empty vial explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    if (skill_table[sn].target != TAR_CHAR_DEFENSIVE
    &&   skill_table[sn].target != TAR_CHAR_SELF
    &&   skill_table[sn].target != TAR_OBJ_CHAR_DEF
    &&   skill_table[sn].target != TAR_CHAR_OFFENSIVE
    &&   skill_table[sn].target != TAR_OBJ_CHAR_OFF)
    {
        send_to_char("You may only brew potions of spells which you can cast on people.\n\r", ch);
        return;
    }

    extract_obj(obj);

    act("{Y$n begins to brew a potion...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYou begin to brew a potion...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    ch->brew_sn = sn;

    BREW_STATE(ch, 12);
}


/**
 * brew_end - Complete potion brewing process
 *
 * Called when brew delay completes. Performs skill check
 * and creates potion object on success.
 *
 * Success chance:
 * - (brew_skill*2/3) + (spell_skill/3) - 10 + (CON/4)
 * - ROOM_ALCHEMY gives 50% bonus
 * - Immortals always succeed
 *
 * Alchemist subclass creates multi-use potions (value[5]):
 * - <75 skill: 1 use
 * - 75-85 skill: 2 uses
 * - 85+ skill: 3 uses
 *
 * @param ch  Character finishing brewing
 * @param sn  Skill number of spell being brewed
 */
void brew_end(CHAR_DATA *ch, int16_t sn)
{
    char buf[2*MAX_STRING_LENGTH];
    OBJ_DATA *potion;
    int chance;
    char potion_name[MAX_STRING_LENGTH];
    SPELL_DATA *spell;

    chance = (get_skill(ch, skill_resolve_gsn("brew")) * 2)/3 +
        get_skill(ch, sn)/3 - 10 +
        (get_curr_stat(ch, STAT_CON))/4;

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = (chance * 3)/2;

    chance = URANGE(1, chance, 98);

    if (IS_IMMORTAL(ch))
    chance = 100;

    if (number_percent() >= chance)
    {
    act("{Y$n's attempt to brew a potion fails miserably.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYou fail to contain the magic within the vial, shattering the vial completely.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    check_improve(ch, skill_resolve_gsn("brew"), false, 2);
    return;
    }

    sprintf(potion_name, "%s", skill_table[sn].name);

    sprintf(buf, "You brew a potion of %s.", potion_name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    sprintf(buf, "$n brews a potion of %s.", potion_name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    check_improve(ch, skill_resolve_gsn("brew"), true, 2);

    potion = create_object(get_reserved_obj_index("obj_potion"), 1, false);

    sprintf(buf, potion->short_descr, potion_name);

    free_string(potion->short_descr);
    potion->short_descr = str_dup(buf);

    sprintf(buf, potion->description, potion_name);

    free_string(potion->description);
    potion->description = str_dup(buf);

    free_string(potion->full_description);
    potion->full_description = str_dup(buf);

    spell = new_spell();
    spell->sn = sn;
    spell->level = ch->tot_level;
    spell->next = potion->spells;
    potion->spells = spell;

    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
    {
    if (get_skill(ch, skill_resolve_gsn("brew")) < 75)
        FLUID_CON(potion)->amount = 1;
    else if (get_skill(ch, skill_resolve_gsn("brew")) < 85)
        FLUID_CON(potion)->amount = 2;
    else
        FLUID_CON(potion)->amount = 3;
    }

    free_string(potion->name);
    strcat(potion_name, " potion");
    potion->name = short_to_name(potion_name);
    obj_to_char(potion, ch);
}


/**
 * do_plant - Plant an object on another character
 *
 * Reverse of steal - places an item in target's inventory.
 * Uses skill_resolve_gsn("plant") skill. Target must be in room and visible.
 *
 * @param ch        Character planting
 * @param argument  Item and target
 */
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

    act("$n plants $p in the ground.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("You plant $p in the ground.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    SET_BIT(obj->extra[0], ITEM_PLANTED);
    obj_from_char(obj);
    obj_to_room(obj, ch->in_room);
}


/**
 * do_hands - Use healing hands skill to cure afflictions
 *
 * Clerical skill (skill_resolve_gsn("healing hands")) that cures multiple conditions:
 * - Disease
 * - Poison
 * - Blindness
 * - Toxic fumes
 *
 * Can target self or another character.
 * Success chance based on skill level.
 *
 * @param ch        Character using healing hands
 * @param argument  Target character
 */
void do_hands(CHAR_DATA *ch, char *argument)
{
    CHAR_DATA *victim;
    int sn;
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

    if ((chance = get_skill(ch,skill_resolve_gsn("healing hands"))) == 0)
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
    act("You place your hands on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n places $s hands on $N.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT, NULL, NULL);
    act("$n places $s hands on you.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }
    else
    {
    act("You place your hands over your heart.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("$n places $s hands over $s heart.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    }

    WAIT_STATE(ch, skill_table[skill_resolve_gsn("healing hands")].beats);

    if (number_percent() > get_skill(ch, skill_resolve_gsn("healing hands")))
    {
    act("You see a faint glow of magic, but nothing happens.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("You see a faint glow of magic eminating from $n's hands, but nothing happens.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    check_improve(ch, skill_resolve_gsn("healing hands"), false, 1);
    return;
    }

    act("{CYour hands glow a brilliant blue.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{C$n's hands glow a brilliant blue.{x", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    sn = skill_lookup("cure disease");
    spell_cure_disease(skill_find_uid(sn), ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);

    sn = skill_lookup("cure poison");
    spell_cure_poison(skill_find_uid(sn), ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);

    sn = skill_lookup("cure blindness");
    spell_cure_blindness(skill_find_uid(sn), ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);

    /* @@@NIB : 20070127 : for curing the toxic fumes*/
    sn = skill_lookup("cure toxic");
    spell_cure_toxic(skill_find_uid(sn), ch->tot_level, ch, victim, TARGET_CHAR, WEAR_NONE, INVOC_INTERNAL);
    check_improve(ch, skill_resolve_gsn("healing hands"), true, 1);
}


/**
 * do_scribe - Create scrolls from spells (skill_resolve_gsn("scribe") skill)
 *
 * Scribes up to 3 spells onto a blank scroll. Requires:
 * - ITEM_BLANK_SCROLL in inventory
 * - Knowledge of skill_resolve_gsn("scribe") skill
 * - Knowledge of target spell(s)
 * - Sufficient mana (2/3 of combined spell costs)
 *
 * Restrictions:
 * - "kill" spell cannot be scribed
 * - Dead characters cannot scribe
 *
 * Sets SCRIBE_STATE and scribe_sn1/2/3, calls scribe_end() after delay.
 * Cast time scales with number of spells (10-18 beats).
 *
 * @param ch        Character scribing
 * @param argument  Up to 3 spell names
 */
void do_scribe(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    int sn1, sn2, sn3;
    int mana;
    int chance;
    int kill;
    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);

    if (IS_DEAD(ch))
    {
        send_to_char("You can't do that. You are dead.\n\r", ch);
        return;
    }

    if ((chance = get_skill(ch,skill_resolve_gsn("scribe"))) == 0)
    {
        send_to_char("Scribe? What's that?\n\r",ch);
        return;
    }

    obj = NULL;
    // Replace traditional list traversal with iterator for lcarrying
    ITERATOR it;
    iterator_start(&it, ch->lcarrying);
    while ((obj = (OBJ_DATA *)iterator_nextdata(&it))) {
        if (obj->item_type == ITEM_BLANK_SCROLL || obj->pIndexData == get_reserved_obj_index("obj_blank_scroll"))
            break;
    }
    iterator_stop(&it);

    if (obj == NULL)
    {
        send_to_char("You do not have a blank scroll.\n\r", ch);
        return;
    }

    sn1 = 0;
    sn2 = 0;
    sn3 = 0;

    if (arg1[0] == '\0')
    {
        send_to_char("What do you wish to scribe?\n\r", ch);
        return;
    }

    sn1 = find_spell(ch, arg1);

    if ((sn1) < 1 || skill_table[sn1].spell_fun == spell_null ||
        get_skill(ch, sn1) == 0)
    {
        send_to_char("You don't know any spells of that name.\n\r", ch);
        return;
    }

    if (arg2[0] != '\0')
    {
        sn2 = find_spell(ch, arg2);

        if ((sn2) < 1 || skill_table[sn2].spell_fun == spell_null ||
            get_skill(ch, sn2) == 0)
        {
            send_to_char("You don't know any spells of that name.\n\r", ch);
            return;
        }
    }

    if (arg3[0] != '\0')
    {
        sn3 = find_spell(ch, arg3);

        if ((sn3) < 1 || skill_table[sn3].spell_fun == spell_null ||
            get_skill(ch, sn3) == 0)
        {
            send_to_char("You don't know any spells of that name.\n\r", ch);
            return;
        }
    }

    mana = 0;
    if (sn1 > 0) mana += skill_table[sn1].min_mana;
    if (sn2 > 0) mana += skill_table[sn2].min_mana;
    if (sn3 > 0) mana += skill_table[sn3].min_mana;

    if (mana > 200)
    {
        send_to_char("The scroll can't hold that much magic.\n\r", ch);
        return;
    }

    mana = 2 * mana / 3;
    if (ch->mana < mana)
    {
        send_to_char("You don't have enough mana to scribe that scroll.\n\r", ch);
        return;
    }

    ch->mana -= mana;

    act("{Y$n begins to write onto $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYou begin to write onto $p...{x", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    ch->scribe_sn = sn1;
    ch->scribe_sn2 = sn2;
    ch->scribe_sn3 = sn3;

    extract_obj(obj);

    /* Kill must not be one of the spells*/
    kill = find_spell(ch, "kill");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
        send_to_char("The scroll explodes into dust!\n\r", ch);
        act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    /* Mass healing must not be one of the spells*/
    kill = find_spell(ch, "mass healing");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
        send_to_char("The scroll explodes into dust!\n\r", ch);
        act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    kill = find_spell(ch, "spell trap");
    if (kill == sn1 || kill == sn2 || kill == sn3)
    {
        send_to_char("The scroll explodes into dust!\n\r", ch);
        act("$n's blank scroll explodes into dust!\n\r", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        return;
    }

    if (sn2 == 0)
        SCRIBE_STATE(ch, 12);
    else
    {
        if (sn3 == 0)
            SCRIBE_STATE(ch, 24);
        else
            SCRIBE_STATE(ch, 36);
    }
}

/**
 * scribe_end - Complete scroll scribing process
 *
 * Called when scribe delay completes. Creates scroll with 1-3 spells.
 *
 * Spell levels scale inversely with spell count:
 * - 1 spell: Full level
 * - 2 spells: Half level (alchemist bonus: +1/3)
 * - 3 spells: Third level (alchemist bonus: +1/4)
 *
 * ROOM_ALCHEMY gives 50% success bonus.
 * Failure causes scroll to explode harmlessly.
 *
 * @param ch   Character finishing scribing
 * @param sn   First spell skill number
 * @param sn2  Second spell (0 if none)
 * @param sn3  Third spell (0 if none)
 */
void scribe_end(CHAR_DATA *ch, int16_t sn, int16_t sn2, int16_t sn3)
{
    char buf[2*MAX_STRING_LENGTH];
    OBJ_DATA *scroll;
    int chance;
    char scroll_name[MAX_STRING_LENGTH];
    SPELL_DATA *spell;

    if (sn2 == 0)
        chance = get_skill(ch, skill_resolve_gsn("scribe"));
    else
    if (sn3 == 0)
        chance = get_skill(ch, skill_resolve_gsn("scribe")) / 2 + get_skill(ch, skill_resolve_gsn("scribe")) / 3 + get_skill(ch, skill_resolve_gsn("scribe"))/7;
    else
        chance = get_skill(ch, skill_resolve_gsn("scribe")) / 2 + get_skill(ch, skill_resolve_gsn("scribe")) / 3;

    if (IS_SET(ch->in_room->room_flag[1], ROOM_ALCHEMY))
        chance = (chance * 3)/2;

    chance = URANGE(1, chance, 98);

    if (IS_IMMORTAL(ch))
        chance = 100;

    if (number_percent() >= chance)
    {
        act("{Y$n's scroll explodes into flame.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        act("{YYour blank scroll explodes into flame as you make a minor mistake.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        check_improve(ch, skill_resolve_gsn("scribe"), false, 3);
        return;
    }

    if (sn2 == 0)
    sprintf(scroll_name, "%s", skill_table[sn].name);
    else
    {
        if (sn3 == 0)
            sprintf(scroll_name, "%s, %s", skill_table[sn].name, skill_table[sn2].name);
        else
            sprintf(scroll_name, "%s, %s, %s", skill_table[sn].name, skill_table[sn2].name, skill_table[sn3].name);
    }

    sprintf(buf, "You create a scroll of %s.", scroll_name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    sprintf(buf, "$n creates a scroll of %s.", scroll_name);
    act(buf, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    check_improve(ch, skill_resolve_gsn("scribe"), true, 3);

    scroll = create_object(get_reserved_obj_index("obj_scroll"), 1, false);

    sprintf(buf, scroll->short_descr, scroll_name);

    free_string(scroll->short_descr);
    scroll->short_descr = str_dup(buf);

    sprintf(buf, scroll->description, scroll_name);

    free_string(scroll->description);
    scroll->description = str_dup(buf);

    free_string(scroll->full_description);
    scroll->full_description = str_dup(buf);

    if (sn2 == 0)
    {
    spell = new_spell();
    spell->sn = sn;
    spell->level = ch->tot_level;
        spell->next = scroll->spells;
    scroll->spells = spell;
    }
    else if (sn3 == 0)
    {
    spell = new_spell();
    spell->sn = sn;
    spell->level = ch->tot_level/2;
    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
        spell->level += ch->tot_level/3;
    spell->next = scroll->spells;
    scroll->spells = spell;

    spell = new_spell();
    spell->sn = sn2;
    spell->level = ch->tot_level/2;
    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
        spell->level += ch->tot_level/3;
    spell->next = scroll->spells;
    scroll->spells = spell;
    }
    else
    {
    spell = new_spell();
    spell->sn = sn;
    spell->level = ch->tot_level/3;
    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
        spell->level += ch->tot_level/4;
    spell->next = scroll->spells;
    scroll->spells = spell;

    spell = new_spell();
    spell->sn = sn2;
    spell->level = ch->tot_level/3;
    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
        spell->level += ch->tot_level/4;
    spell->next = scroll->spells;
    scroll->spells = spell;

    spell = new_spell();
    spell->sn = sn3;
    spell->level = ch->tot_level/3;
    if (ch->pcdata->second_sub_class_cleric == CLASS_CLERIC_ALCHEMIST)
        spell->level += ch->tot_level/4;
    spell->next = scroll->spells;
    scroll->spells = spell;
    }

    free_string(scroll->name);
    strcat(scroll_name, " scroll");
    scroll->name = short_to_name(scroll_name);
    obj_to_char(scroll, ch);
}


/**
 * is_extra_damage_relic_in_room - Check for extra damage relic
 *
 * @param room  Room to check
 * @return true if relic of power is present
 */
bool is_extra_damage_relic_in_room(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;
    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_DAMAGE"))
    {
        return true;
     }
    }
    return false;
}


/**
 * is_extra_xp_relic_in_room - Check for extra XP relic
 *
 * @param room  Room to check
 * @return true if relic of knowledge is present
 */
bool is_extra_xp_relic_in_room(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;
    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == get_reserved_obj_index("OBJ_VNUM_RELIC_EXTRA_XP"))
    {
        return true;
     }
    }
    return false;
}

/**
 * is_hp_regen_relic_in_room - Check for HP regen relic
 *
 * @param room  Room to check
 * @return true if relic of health is present
 */
bool is_hp_regen_relic_in_room(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;
    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == get_reserved_obj_index("OBJ_VNUM_RELIC_HP_REGEN"))
    {
        return true;
     }
    }
    return false;
}


/**
 * is_mana_regen_relic_in_room - Check for mana regen relic
 *
 * @param room  Room to check
 * @return true if relic of magic is present
 */
bool is_mana_regen_relic_in_room(ROOM_INDEX_DATA *room)
{
    OBJ_DATA *obj;
    for (obj = room->contents; obj != NULL; obj = obj->next_content)
    {
    if (obj->pIndexData == get_reserved_obj_index("OBJ_VNUM_RELIC_MANA_REGEN"))
    {
        return true;
     }
    }
    return false;
}


/**
 * do_bomb - Create a smoke bomb (skill_resolve_gsn("bomb") skill)
 *
 * Begins crafting a smoke bomb. Requires:
 * - skill_resolve_gsn("bomb") skill
 * - Mana >= 50%
 * - Movement >= 50%
 *
 * Sets BOMB_STATE (24 beats) and calls bomb_end() on completion.
 *
 * @param ch        Character making bomb
 * @param argument  Unused
 */
void do_bomb(CHAR_DATA *ch, char *argument)
{
    if (is_dead(ch))
    return;

    if (get_skill(ch,skill_resolve_gsn("bomb")) == 0)
    {
        send_to_char("You know nothing about explosives.\n\r",ch);
        return;
    }

    if (ch->mana < ch->max_mana/2 || ch->move < ch->max_move/2) {
    send_to_char("You need to have mana and movement at at least half full to make a bomb.\n\r", ch);
    return;
    }

    act("{Y$n begins to create a smoke bomb...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYou begin to create a smoke bomb...{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    BOMB_STATE(ch, 24);
    return;
}


/**
 * bomb_end - Complete bomb creation
 *
 * Performs skill check. On success, creates smoke bomb object.
 * On failure, bomb explodes dealing 2/3 max HP damage.
 *
 * Consumes 25% mana and 25% movement regardless of outcome.
 *
 * @param ch  Character finishing bomb
 */
void bomb_end(CHAR_DATA *ch)
{
    OBJ_DATA *obj;
    int chance = get_skill(ch, skill_resolve_gsn("bomb"));

    if (number_percent() < chance)
    {
    act("{Y$n creates a smoke bomb.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYou complete the construction of a smoke bomb.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    obj = create_object(get_reserved_obj_index("obj_bomb_smoke"), ch->tot_level, false);
    obj->level = ch->tot_level;
    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
    {
        act("You drop $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n drops $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
        obj_to_room(obj,ch->in_room);
    }
    else
        obj_to_char(obj, ch);

    check_improve(ch, skill_resolve_gsn("bomb"), true, 1);
    } else {
    act("{Y$n's homemade explosives {REXPLODE{Y, causing $m great pain!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM, NULL, NULL);
    act("{YYour smoke bomb {REXPLODES{Y as you bumble up the recipe! {ROUCH!!!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    damage(ch, ch, (2 * ch->max_hit)/3, skill_resolve_gsn("bomb"), DAM_ENERGY, false);

    check_improve(ch, skill_resolve_gsn("bomb"), false, 1);
    }

    ch->mana -= ch->mana/4;
    ch->move -= ch->move/4;
}


/**
 * do_infuse - Infuse a weapon with elemental power
 *
 * Uses skill_resolve_gsn("infuse") skill to add temporary elemental damage to a weapon.
 * Available infusion types: fire, cold, shock, acid, poison.
 *
 * Requirements:
 * - skill_resolve_gsn("infuse") skill
 * - Weapon in inventory
 * - Weapon type must be edged (sword, dagger, axe, etc.)
 * - Weapon cannot already have that infusion
 *
 * @param ch        Character infusing
 * @param argument  Weapon and element type
 */
void do_infuse(CHAR_DATA *ch, char *argument)
{
    OBJ_DATA *obj;
    AFFECT_DATA af;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int percent,skill;
    long weapon;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if ((skill = get_skill(ch, skill_resolve_gsn("infuse"))) == 0)
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

    obj =  get_obj_list(ch,arg1,ch->lcarrying);

    if (obj== NULL)
    {
    send_to_char("You don't have that item.\n\r",ch);
    return;
    }
memset(&af,0,sizeof(af));
    if (obj->item_type == ITEM_WEAPON)
    {
        if (arg2[0] == '\0')
        {
        act("Infuse $p with what?", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        return;
        }

        if (IS_WEAPON_STAT(obj,WEAPON_FLAMING)
        ||  IS_WEAPON_STAT(obj,WEAPON_FROST)
        ||  IS_WEAPON_STAT(obj,WEAPON_VAMPIRIC)
/*        ||  IS_WEAPON_STAT(obj,WEAPON_SHARP)	 Why is this here?  Changed this to be manual*/
        ||  IS_WEAPON_STAT(obj,WEAPON_VORPAL)
        ||  IS_WEAPON_STAT(obj,WEAPON_SHOCKING)
        ||  IS_WEAPON_STAT(obj,WEAPON_ACIDIC)
        ||  IS_WEAPON_STAT(obj,WEAPON_RESONATE)
        ||  IS_WEAPON_STAT(obj,WEAPON_BLAZE)
        ||  IS_WEAPON_STAT(obj,WEAPON_SUCKLE))
        {
            act("$p is already infused with a magic enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
            return;
        }

    if (WEAPON(obj)->damage_type < 0
    ||  attack_table[WEAPON(obj)->damage_type].damage == DAM_BASH)
    {
        send_to_char("You can only envenom edged weapons.\n\r",ch);
        return;
    }

    weapon = -1;
    if (!str_cmp(arg2, "fire"))		weapon = WEAPON_FLAMING;
    else if (!str_cmp(arg2, "frost"))	weapon = WEAPON_FROST;
    else if (!str_cmp(arg2, "vampiric"))	weapon = WEAPON_VAMPIRIC;
/*	else if (!str_cmp(arg2, "sharp"))	weapon = WEAPON_SHARP;*/
    else if (!str_cmp(arg2, "vorpal"))	weapon = WEAPON_VORPAL;
    else if (!str_cmp(arg2, "shocking"))	weapon = WEAPON_SHOCKING;
    else if (!str_cmp(arg2, "acidic"))	weapon = WEAPON_ACIDIC;
    else if (!str_cmp(arg2, "resonate"))	weapon = WEAPON_RESONATE;
    else if (!str_cmp(arg2, "blaze"))	weapon = WEAPON_BLAZE;
    else if (!str_cmp(arg2, "suckle"))	weapon = WEAPON_SUCKLE;

    if (weapon == -1)
        {
        send_to_char("Can only infuse fire, frost, vampiric, sharp, vorpal or shocking.\n\r", ch);
        return;
        }

    percent = number_percent();
    if (percent < skill)
    {
            af.where     = TO_WEAPON;
            af.group     = AFFGROUP_WEAPON;
            af.type      = skill_resolve_gsn("infuse");
    af.skill = skill_find_uid(af.type);
            af.level     = (ch->tot_level * skill)/ 100;
            af.duration  = ((ch->tot_level/2) * skill)/ 100;
            af.location  = 0;
            af.modifier  = 0;

            af.bitvector = weapon;
        af.bitvector2 = 0;
        af.slot	= WEAR_NONE;
            affect_to_obj(obj,&af);

            act("$n carefully infuses $p with a magical enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_ROOM, NULL, NULL);
        act("You carefully infuse $p with a magical enchantment.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("infuse"),true,3);
        WAIT_STATE(ch,skill_table[skill_resolve_gsn("infuse")].beats);
            return;
        }
    else
    {
        act("You fail to infuse $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
        check_improve(ch,skill_resolve_gsn("infuse"),false,3);
        WAIT_STATE(ch,skill_table[skill_resolve_gsn("infuse")].beats);
        return;
    }
    }

    act("You can't infuse $p.",ch, NULL, NULL,obj, NULL, NULL,NULL,TO_CHAR, NULL, NULL);
}


/**
 * repair_end - Complete self-repair process
 *
 * Finishes skill-based item repair. Adds repair_amt to item condition.
 * Displays condition message based on new percentage.
 * 20% chance to increment times_fixed counter.
 *
 * @param ch  Character who finished repairing
 */
void repair_end(CHAR_DATA *ch)
{
    if (ch->repair_obj == NULL)
    {

    pbugf(LOG_ERROR, "ch->repair_obj was null for %s", ch->name);
    return;
    }

    act("{YYou complete repairing $p.{x", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    act("{Y$n completes $s repairs of $p.{x", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

    ch->repair_obj->condition += ch->repair_amt;
    ch->repair = 0;
    ch->repair_amt = 0;

    if (ch->repair_obj->condition < 10)
    act("$p is still almost crumbling.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 20)
    act("$p is still in extremely bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 30)
    act("$p is still in very bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 40)
    act("$p is still in bad condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 50)
    act("$p looks like it will hold together.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 60)
    act("$p looks to be in usable condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 75)
    act("$p now looks to be in fair condition.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 90)
    act("$p now looks almost new.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition < 100)
    act("$p now looks brand new.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    else if (ch->repair_obj->condition >= 100)
    act("$p has been repaired completely.", ch, NULL, NULL, ch->repair_obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);

    if (number_percent() < 20)
    ch->repair_obj->times_fixed++;

    ch->repair_obj = NULL;
}

/**
 * do_dig - Dig in wilderness to find buried items
 *
 * Uncovers ITEM_BURIED objects in the current room.
 * Requires:
 * - Must be in wilderness
 * - Must be holding ITEM_SHOVEL
 *
 * @param ch        Character digging
 * @param argument  Unused
 */
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
          act("You have discovered $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
          found = true;
       }
    }

    if (!found) {
      send_to_char("You found nothing.\n\r", ch);
      }
}


/**
 * do_use - Generic use command for scripted objects
 *
 * Invokes use triggers on objects. Can use on target or solo.
 *
 * - use <object>              : TRIG_USE
 * - use <object> <target>     : TRIG_USEWITH on both objects
 *
 * @param ch        Character using
 * @param argument  Object and optional target
 *
 * Triggers: TRIG_USE, TRIG_USEWITH
 */
void do_use(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    OBJ_DATA *obj, *tobj = NULL;
    CHAR_DATA *vch = NULL;


    argument = one_argument(argument, arg);

    if (check_social_status(ch))
        return;

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

        if(p_use_with_trigger(ch, obj, TRIG_USEWITH, tobj, NULL, vch, NULL)) return;
        if(tobj && p_use_with_trigger(ch, tobj, TRIG_USEWITH, obj, NULL, vch, NULL)) return;

        send_to_char("Nothing happens.\n\r", ch);
        return;
    }

    if(p_use_trigger(ch, obj, TRIG_USE)) return;

    send_to_char("Nothing happens.\n\r", ch);
}

/**
 * do_conceal - Hide an item in concealed wear slot
 *
 * Places an item in the WEAR_CONCEALED slot, hidden from normal view.
 * Similar to wearing but for secret items. Watchers may notice based
 * on skill vs character's level.
 *
 * Restrictions:
 * - Cannot conceal while shifted (slayer/werewolf)
 * - Cannot conceal while blind
 * - Only one concealed item at a time
 * - Cannot conceal immortal-level items as mortal
 *
 * @param ch        Character concealing
 * @param argument  Item to conceal
 *
 * Triggers: TRIG_PREWEAR (can cancel)
 */
void do_conceal(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    CHAR_DATA *rch;

    if (check_social_status(ch))
        return;

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

    if(p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_PREWEAR, NULL))
        return;

    act("You conceal $p.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
    for(rch = ch->in_room->people; rch; rch = rch->next_in_room) {
        if(!can_see(rch,ch)) continue;

        if(!IS_SAGE(rch)) {
            if(IS_AFFECTED(ch,AFF_SNEAK)) continue;
            if(IS_AFFECTED(ch,AFF_HIDE) && !IS_AFFECTED(rch,AFF_DETECT_HIDDEN)) continue;
        }

        act("$n conceals $p upon $mself.",  ch, rch, NULL, obj, NULL, NULL, NULL, TO_VICT, NULL, NULL);
    }

    if ((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL) && IS_EVIL(ch)) ||
        (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD) && IS_GOOD(ch)) ||
        (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch))) {
        act("You are zapped by $p and drop it.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR, NULL, NULL);
        act("$n is zapped by $p and drops it.",  ch, NULL, NULL, obj, NULL, NULL, NULL, TO_ROOM, NULL, NULL);

        REMOVE_BIT(obj->extra[1], ITEM_KEPT);

        obj_from_char(obj);
        obj_to_room(obj, ch->in_room);
        return;
    }

    obj->wear_loc = WEAR_CONCEALED;
    p_percent_trigger(NULL, obj, NULL, NULL, ch, NULL, NULL, NULL, NULL, TRIG_WEAR, NULL);
}


/**
 * haggle_price - Calculate final purchase price with haggling
 *
 * Computes the final price for purchasing items, applying:
 * - Shop profit margin via adjust_keeper_price
 * - Haggle discount based on skill check
 * - Quantity multiplier
 *
 * Checks if customer can afford the final price.
 *
 * @param ch         Customer
 * @param keeper     Shopkeeper
 * @param chance     Haggle skill percentage
 * @param number     Quantity being purchased
 * @param base_price Price per unit before shop markup
 * @param funds      Customer's available funds
 * @param discount   Maximum discount percentage (0-99)
 * @param haggled    Output: set to true if haggling succeeded
 * @param silent     If true, don't display "can't afford" message
 *
 * @return Final price, or -1 if cannot afford
 */
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
                    act("{R$n tells you 'You can't afford to buy that many.'{x", keeper,ch, NULL, NULL, NULL, NULL, NULL,TO_VICT, NULL, NULL);
                else
                    act("{R$n tells you 'You can't afford to buy that'.{x", keeper, ch, NULL, NULL, NULL, NULL, NULL, TO_VICT, NULL, NULL);
                ch->reply = keeper;
            }
            return -1;
        }
    }

    return UMAX(price, 0);
}

/**
 * get_stock_description - Get display description for a stock item
 *
 * Returns the appropriate short description for a shop stock item:
 * 1. custom_descr if set
 * 2. obj->short_descr if object stock
 * 3. mob->short_descr if mob stock
 * 4. "something" as fallback
 *
 * @param stock  Shop stock item
 *
 * @return Short description string
 */
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
