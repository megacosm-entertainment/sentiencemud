/***************************************************************************
 *  This file contains auction code developed by Brian Babey, and any      *
 *  communication regarding it should be sent to [bbabey@iname.com]        *
 *  Web Address: http://www.erols.com/bribe/                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*       ROM 2.4 is copyright 1993-1995 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@pacinfo.com)                              *
*           Gabrielle Taylor (gtaylor@pacinfo.com)                         *
*           Brian Moore (rom@rom.efn.org)                                  *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"

void show_obj_stats( CHAR_DATA *ch, OBJ_DATA *obj );
void auction_channel( char *msg );

void do_auction( CHAR_DATA *ch, char * argument )
{
    long gold = 0;
    OBJ_DATA *obj;
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    long minimum_bid = 0;
    long bid = 0;
    DESCRIPTOR_DATA *d;

    argument = one_argument( argument, arg1 );
    argument = one_argument( argument, arg2 );

    if ( ch == NULL || IS_NPC(ch) )
	return;

    if ( IS_DEAD(ch) )
    {
	send_to_char("You can't, you are dead.\n\r", ch);
	return;
    }

   /*
    * Toggle the auction channel
    */
    if ( arg1[0] == '\0')
    {
	if ( IS_SET(ch->comm,COMM_NOAUCTION) )
	{
	    REMOVE_BIT(ch->comm,COMM_NOAUCTION );
	    send_to_char("Auction channel is now ON.\n\r",ch);
	    return;
	}

	SET_BIT(ch->comm,COMM_NOAUCTION);
	send_to_char("Auction channel is now OFF.\n\r",ch);
	return;
    }


   /*
    * info
    */
    if ( !str_cmp( arg1, "info" ) )
    {
	obj = auction_info.item;

	if ( !obj )
	{
	    send_to_char("There is nothing up for auction right now.\n\r",ch);
	    return;
	}

	if ( auction_info.owner == ch )
	{
	    sprintf( buf, "\n\r{MYou are currently auctioning {x%s{M.{x\n\r",
		obj->short_descr );
	    send_to_char( buf, ch );

	    sprintf( buf, "{MThe minimum bid for this item is {x%ld{M gold.{x\n\r",
	    	auction_info.minimum_bid == 0 ? 1 : auction_info.minimum_bid);
	    send_to_char( buf, ch );

	    if ( auction_info.high_bidder == NULL )
	    {
		send_to_char("{MNobody has currently bid on this item.\n\r",
			ch );
	    }
	    else
	    {
		sprintf( buf, "{MThe current high bid is {x%ld{M gold from {x%s{M.{x\n\r",
			auction_info.current_bid,
			auction_info.high_bidder->name );
		send_to_char( buf, ch );
	    }
	    spell_identify( gsn__auction, ch->tot_level, ch,
	    	(void *) auction_info.item, TARGET_OBJ, WEAR_NONE );
	    return;
	}

	sprintf( buf, "\n\r{M%s is currently auctioning {x%s{M.{x\n\r",
			auction_info.owner->name, obj->short_descr );
	send_to_char( buf, ch );

	sprintf( buf, "{MThe minimum bid for this item is {x%ld{M gold.{x\n\r",
	    	auction_info.minimum_bid == 0 ? 1 : auction_info.minimum_bid);
	send_to_char( buf, ch );

	if ( auction_info.high_bidder == NULL )
	{
	    send_to_char("{MNobody has currently bid on this item.\n\r", ch );
	}
	else
	{
	    sprintf( buf, "{MThe current high bid is {x%ld{M gold from {x%s{M.{x\n\r",
		    auction_info.current_bid,
		    auction_info.high_bidder->name );
	    send_to_char( buf, ch );
	}

	spell_identify( gsn__auction, ch->tot_level, ch, (void *) auction_info.item, TARGET_OBJ, WEAR_NONE );
        /* AO 010217 LAME
	if ( ch->tot_level < obj->level - 25 && !IS_REMORT(ch))
	{
	    send_to_char("{RWARNING: You will not be able to save this item.{x\n\r",
	        ch );
	} */
	return;
    }

    /* Stop*/
    if ( !str_cmp( arg1, "stop" ) )
    {
	if ( auction_info.owner == NULL || str_cmp( ch->name, auction_info.owner->name ) )
	{
	    send_to_char("Only the owner may stop the auction.\n\r", ch );
	    return;
	}

	if ( auction_info.status >= AUCTION_LENGTH - 2 )
	{
	    send_to_char("It is too late to stop the auction.\n\r", ch );
	    return;
	}

	sprintf(buf, "{M[AUCTION] %s has stopped the auction of %s{M - item removed.{x\n\r",
	    ch->name, auction_info.item->short_descr);
	log_string(buf);
	for ( d = descriptor_list; d != NULL; d = d->next )
	{
	    CHAR_DATA *victim;

	    victim = d->original ? d->original : d->character;

	    if ( d->connected == CON_PLAYING
	    && d->character != NULL
	    && d->character != ch
	    && !IS_SET(victim->comm, COMM_QUIET )
	    && !IS_SET(victim->comm, COMM_NOAUCTION ) )
		send_to_char( buf, victim );
	}

	sprintf(buf, "{M[AUCTION] You have stopped the auction of %s{M - item removed.{x\n\r", auction_info.item->short_descr );
	send_to_char( buf, ch );

	if (auction_info.high_bidder != NULL)
		auction_info.high_bidder->pcdata->bankbalance
			+= auction_info.gold_held;

	obj_to_char( auction_info.item, ch );

	auction_info.item		= NULL;
	auction_info.owner		= NULL;
	auction_info.high_bidder	= NULL;
	auction_info.current_bid	= 0;
	auction_info.status		= 0;
	auction_info.gold_held		= 0;
	auction_info.silver_held	= 0;
	return;
    }


   /*
    * confiscate
    */
    if ( !str_cmp( arg1, "confiscate" ) )
    {
        if (!IS_IMMORTAL( ch ) )
	{
	    send_to_char("You can't do that.", ch);
	    return;
        }

	if ( auction_info.item == NULL )
	{
	    send_to_char("There's nothing on auction.\n\r", ch );
	    return;
	}

	sprintf(buf, "%s has confiscated %s{M - item removed.{x\n\r",
			ch->name, auction_info.item->short_descr);
	auction_channel( buf );

	log_string(buf);

	if (auction_info.high_bidder != NULL)
		auction_info.high_bidder->pcdata->bankbalance
			+= auction_info.gold_held;

	obj_to_char( auction_info.item, ch );

	auction_info.item		= NULL;
	auction_info.owner		= NULL;
	auction_info.high_bidder	= NULL;
	auction_info.current_bid	= 0;
	auction_info.status		= 0;
	auction_info.gold_held		= 0;
	auction_info.silver_held	= 0;

	return;
    }

   /*
    * bid
    */
    if ( !str_cmp( arg1, "bid" ) )
    {
        obj = auction_info.item;

	if (ch == auction_info.owner)
	{
	    send_to_char("You can't bid on your own item.\n\r", ch);
	    return;
	}

        if ( !obj )
        {
            send_to_char("There is nothing up for auction right now.\n\r",ch);
            return;
        }

	if ( arg2[0] == '\0' )
	{
	    send_to_char("You must enter an amount to bid.\n\r",ch);
	    return;
	}

	bid = atol( arg2 );

        if ( auction_info.high_bidder == NULL )
	    minimum_bid = auction_info.minimum_bid;
        else
	    minimum_bid = (long)((double) auction_info.current_bid * 1.1 );

	if ( bid <= minimum_bid )
	{
	    sprintf( buf, "The minimum bid is %ld gold.\n\r",minimum_bid+1);
	    send_to_char(buf,ch);
	    return;
	}

	/*if ( (ch->silver + 100 * ch->gold) < bid )*/
	if ( ch->pcdata->bankbalance < bid )
	{
	    send_to_char("You can't cover that bid.\n\r",ch);
	    return;
	}

	sprintf(buf, "%s bids %ld gold on %s.\n\r",
	    ch->name, bid, auction_info.item->short_descr);
	auction_channel( buf );

	if ( auction_info.high_bidder != NULL )
	{
	    auction_info.high_bidder->pcdata->bankbalance += auction_info.gold_held;
	}

        gold = bid;
	ch->pcdata->bankbalance -= gold;

	auction_info.gold_held		= gold;
	auction_info.silver_held	= 0;
	auction_info.high_bidder	= ch;
	auction_info.current_bid	= bid;
	auction_info.status	 	= 0;

	return;
    }


   /*
    * put an item on auction
    */
    if ( (obj = get_obj_carry( ch, arg1, ch )) == NULL )
    {
	send_to_char("You aren't carrying that item.\n\r",ch);
	return;
    }

    if ( IS_OBJ_STAT( obj, ITEM_ROT_DEATH )
    || obj->timer > 0)
    {
 	send_to_char("You can't auction that.\n\r", ch);
	return;
    }

    if ( obj->item_type == ITEM_CONTAINER && obj->contains )
    {
	send_to_char("You can't auction bags or containers unless they are empty.\n\r", ch);
	return;
    }

    if ( !can_drop_obj(ch, obj, true) || IS_SET(obj->extra[1], ITEM_KEPT))
    {
	send_to_char("You can't let go of that item.\n\r",ch);
	return;
    }

    if ( auction_info.item != NULL )
    {
	send_to_char("There is already another item up for bid.\n\r",ch);
	return;
    }

    if ( obj->old_short_descr != NULL || obj->old_description != NULL )
    {
	send_to_char("You can't auction restrung items. You can unrestring the item, then auction it.\n\r", ch );
	return;
    }

    if (obj->pIndexData->vnum == OBJ_VNUM_SKULL || obj->pIndexData->vnum == OBJ_VNUM_GOLD_SKULL) {
	AFFECT_DATA *af;

	for (af = obj->affected; af != NULL; af = af->next) {
	    if (af->type == gsn_third_eye) {
		act("The enchantment on $p prevents it from being auctioned.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
		return;
	    }
	}
    }

    if (IS_SET(obj->extra[1], ITEM_NOAUCTION)) {
	act("$p cannot be auctioned.", ch, NULL, NULL, obj, NULL, NULL, NULL, TO_CHAR);
	return;
    }

    if (arg2[0] != '\0')
    {
    	if (!is_number(arg2))
        {
            send_to_char("Your starting price must be numeric.\n\r", ch);
	    return;
        }

        bid = atol( arg2 );

        if (bid < 1)
        {
  	    send_to_char("Your starting price must be 1 or higher.\n\r", ch);
	    return;
        }

    	auction_info.current_bid = bid;
    }
    else
    	auction_info.current_bid = 1;

    auction_info.minimum_bid    = bid;
    auction_info.owner		= ch;
    auction_info.item		= obj;
    auction_info.status		= 0;

    act("{RA big hairy gnome appears out of nowhere.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("{RA big hairy gnome appears out of nowhere.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("A big hairy gnome takes an item off you.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("A big hairy gnome takes an item off $n.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{RA big hairy gnome disappears with a loud POP!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("{RA big hairy gnome disappears with a loud POP!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

    sprintf(buf,"%s has put %s{M up for auction. The minimum bid is %ld.{x\n\r",
    ch->name,
    obj->short_descr,
    auction_info.current_bid == 0 ? (100) : auction_info.current_bid);
    auction_channel( buf );

    obj_from_char( obj );
}


void auction_update()
{
    char buf[MAX_STRING_LENGTH];
    int i;
    long tax;

    if ( auction_info.item == NULL )
	return;

    auction_info.status++;

    if ( auction_info.status == AUCTION_LENGTH )
    {
	/*if ( auction_info.high_bidder == NULL)*/
	/*   bug("HIGH_BIDDER WAS NULL",0);*/

	if ( auction_info.item == NULL)
	   bug("AUCTION_ITEM WAS NULL",0);

	if ( auction_info.item != NULL
	&& auction_info.item->short_descr == NULL)
	   bug("item short descr WAS NULL",0);

	/*if ( auction_info.high_bidder == NULL )*/
	/*   bug("HIGH_BIDDER name WAS NULL",0);*/

        if ( auction_info.high_bidder == NULL || auction_info.item == NULL )
	{
	    sprintf(buf, "No bids on %s{M - item removed.{x\n\r",
		auction_info.item->short_descr);
	    auction_channel( buf );

	    obj_to_char( auction_info.item, auction_info.owner );

	    send_to_char("{RA big hairy gnome appears out of nowhere.{x\n\r", auction_info.owner);
	    act("{RA big hairy gnome appears out of nowhere.{x", auction_info.owner, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    sprintf(buf, "A big hairy gnome gives you %s.\n\r",
		auction_info.item->short_descr );

	    send_to_char( buf, auction_info.owner );
	    act( "A big hairy gnome gives $n $p.", auction_info.owner, NULL, NULL, auction_info.item, NULL, NULL, NULL, TO_ROOM);
	    send_to_char("{RA big hairy gnome vanishes with a loud POP!{x\n\r", auction_info.owner);
	    act("{RA big hairy gnome vanishes with a loud POP!{x", auction_info.owner, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	    auction_info.item           = NULL;
	    auction_info.owner          = NULL;
	    auction_info.current_bid    = 0;
	    auction_info.status         = 0;

	    return;
	}


	sprintf(buf,"%s {MSOLD to %s for %ld gold.\n\r",
	    auction_info.item->short_descr,
	    auction_info.high_bidder->name,
	    auction_info.current_bid );
	i = 0;
	while ( buf[i] == '{' )
	{
	    i += 2;
	}

	buf[i] = UPPER( buf[i] );
	auction_channel( buf );

        tax = auction_info.gold_held * 5 / 100;
        auction_info.gold_held -= tax;

	auction_info.owner->pcdata->bankbalance += auction_info.gold_held;

        sprintf( buf, "%ld gold tax has been taken!\n\r", tax );
	send_to_char( buf, auction_info.owner );

        sprintf(buf, "%ld gold has been placed in your bank account.\n\r", auction_info.gold_held);
	send_to_char( buf, auction_info.owner );

	obj_to_char( auction_info.item, auction_info.high_bidder );

	send_to_char("{RA big hairy gnome appears out of nowhere.{x\n\r", auction_info.high_bidder);
	act("{RA big hairy gnome appears out of nowhere.{x", auction_info.high_bidder, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	sprintf(buf, "A big hairy gnome gives you %s.\n\r",
		auction_info.item->short_descr );
	send_to_char( buf, auction_info.high_bidder );
	act( "A big hairy gnome gives $n $p.", auction_info.high_bidder, NULL, NULL, auction_info.item, NULL, NULL, NULL, TO_ROOM);
        send_to_char("{RA big hairy gnome vanishes with a loud POP!{x\n\r", auction_info.high_bidder);
	act("{RA big hairy gnome vanishes with a loud POP!{x", auction_info.high_bidder, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);


	auction_info.item		= NULL;
	auction_info.owner		= NULL;
	auction_info.high_bidder	= NULL;
	auction_info.current_bid	= 0;
	auction_info.status		= 0;
	auction_info.gold_held		= 0;
	auction_info.silver_held	= 0;

	return;
    }

    if ( auction_info.status == AUCTION_LENGTH - 1 )
    {
	if ( auction_info.high_bidder == NULL )
	{
		sprintf( buf,"%s {M- going twice (no bid).{x\n\r",
			auction_info.item->short_descr ) ;
		i = 0;
		while ( buf[i] == '{' )
		{
		    i += 2;
		}

		buf[i] = UPPER( buf[i] );
		auction_channel( buf );
	}
	else
	{
		sprintf(buf, "%s{M - going twice to %s at %ld gold.\n\r",
				auction_info.item->short_descr,
				auction_info.high_bidder->name,
				auction_info.current_bid );
		i = 0;
		while ( buf[i] == '{' )
		{
		    i += 2;
		}

		buf[i] = UPPER( buf[i] );
		auction_channel( buf );
	}
	return;
    }

    if ( auction_info.status == AUCTION_LENGTH - 2 )
    {
	if ( auction_info.current_bid == 0
	|| auction_info.current_bid == auction_info.minimum_bid )
	{
	    sprintf(buf, "No bids on %s{M - item removed.{x\n\r",
		auction_info.item->short_descr);
	    auction_channel( buf );

	    obj_to_char( auction_info.item, auction_info.owner );
	    send_to_char("{RA big hairy gnome appears out of nowhere.{x\n\r",
	    	auction_info.owner);
	    act("{RA big hairy gnome appears out of nowhere.{x",
	    	auction_info.owner, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	    sprintf(buf, "A big hairy gnome gives you %s.\n\r",
		auction_info.item->short_descr );
	    send_to_char( buf, auction_info.owner );
	    act( "A big hairy gnome gives $n $p.",
	    	auction_info.owner, NULL, NULL, auction_info.item, NULL, NULL, NULL, TO_ROOM);
	    send_to_char("{RA big hairy gnome vanishes with a loud POP!{x\n\r", 		auction_info.owner);
	    act("{RA big hairy gnome vanishes with a loud POP!{x",
	    	auction_info.owner, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	    auction_info.item           = NULL;
	    auction_info.owner          = NULL;
	    auction_info.current_bid    = 0;
	    auction_info.status         = 0;

	    return;
	}

	if ( auction_info.high_bidder == NULL )
	{
		sprintf( buf,"%s {M- going once (no bid).{x\n\r",
		     auction_info.item->short_descr );

		i = 0;
		while ( buf[i] == '{' )
		{
		    i += 2;
		}

		buf[i] = UPPER( buf[i] );

		auction_channel( buf );
	}
	else
	{
		sprintf(buf, "%s {M- going once to %s at %ld gold.{x\n\r",
				auction_info.item->short_descr,
				auction_info.high_bidder->name,
				auction_info.current_bid );
		i = 0;
		while ( buf[i] == '{' )
		{
		    i += 2;
		}

		buf[i] = UPPER( buf[i] );

		auction_channel( buf );
	}
        return;
    }
}


void auction_channel( char *msg )
{
    char buf[MAX_STRING_LENGTH];
    DESCRIPTOR_DATA *d;

    sprintf(buf, "\n\r{M[AUCTION] %s{x", msg ); /* Add colour if you wish */

      for ( d = descriptor_list; d != NULL; d = d->next )
      {
	CHAR_DATA *victim;

	victim = d->original ? d->original : d->character;

	if ( d->connected == CON_PLAYING &&
	     !IS_SET(victim->comm,COMM_NOAUCTION) &&
	     !IS_SET(victim->comm,COMM_QUIET) )
		send_to_char( buf, victim );
      }
}
