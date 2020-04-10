/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "merc.h"
#include "interp.h"
#include "olc_save.h"


void do_house(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("House syntax:\n\r", ch);
        send_to_char("HOUSE BUY         - Buy property you are standing in.\n\r",
		    ch );
        send_to_char("HOUSE NAME        - Change the name of the house room.\n\r",
		    ch);
        send_to_char("HOUSE DESCRIPTION - Change your room description.\n\r", ch);
        send_to_char("HOUSE FEATURE     - Buy a house room feature.\n\r", ch);
        return;
    }

    if (!str_cmp(arg, "buy"))
    {
        if (!IS_SET(ch->in_room->room_flags, ROOM_HOUSE_UNSOLD))
	{
	    send_to_char("This property is not for sale.\n\r", ch);
	    return;
	}

	if (ch->home != 0)
	{
	    send_to_char("You already have a house.\n\r", ch);
	    return;
	}

	if (ch->gold < 25000)
	{
	    send_to_char("A house will cost 25,000 gold. You don't have enough.\n\r", ch);
	    return;
	}

	ch->gold -= 25000;

 	send_to_char("Congratulations homeowner, on your fine purchase!\n\r", ch);
	send_to_char("You may come here at any time by typing 'gohome'.\n\r", ch);
	act("{RA big hairy gnome appears in a flash of lightning.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{RA big hairy gnome appears in a flash of lightning.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("A big hairy gnome pulls out a checklist and ticks a box.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	act("A big hairy gnome pulls out a checklist and ticks a box.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{RA big hairy gnome disappears in a flash of lightning.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("{RA big hairy gnome disappears in a flash of lightning.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	REMOVE_BIT(ch->in_room->room_flags, ROOM_HOUSE_UNSOLD);

	ch->home = ch->in_room->vnum;
	ch->in_room->home_owner = str_dup( ch->name );
	save_area_new(ch->in_room->area);
	return;
    }
    else if (!str_cmp(arg, "name"))
    {
        if (ch->in_room->vnum != ch->home)
	{
	    send_to_char("You must be in the room to change it's name.\n\r", ch);
	    return;
	}

 	if (argument[0] == '\0')
 	{
     	    send_to_char("You must enter a house room name.\n\r", ch);
	    return;
	}

	if ( strlen_no_colours(argument) < 6 ) {
	    send_to_char("House name too short.\n\r", ch );
	    return;
	}

	if ( strlen( argument ) > 45 )
	    argument[45] = '\0';

	ch->in_room->name = nocolour(argument);

	act("A small gnome appears, changes the room's and then disappears.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("A small gnome appears, changes the room's and then disappears.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	SET_BIT(ch->in_room->area->area_flags, AREA_CHANGED);
	return;
    }
    else if (!str_cmp(arg, "description"))
    {
        if (ch->in_room->vnum != ch->home)
 	{
   	    send_to_char("You must be in the room to change it's description.\n\r", ch);
	    return;
	}

	string_append(ch, &ch->in_room->description);
	act("There is a crack of lightning and a small hairy gnome appears out of nowhere.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	act("There is a crack of lightning and a small hairy gnome appears out of nowhere.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	SET_BIT(ch->in_room->area->area_flags, AREA_CHANGED);
	return;
    }
    else if (!str_cmp(arg, "feature"))
    {

	argument = one_argument( argument, arg2 );

	if (ch->in_room->vnum != ch->home)
	{
	    send_to_char("You must be in the room to change it's flag.\n\r", ch);
	    return;
	}

	if (ch->gold < 5000)
	{
	    send_to_char("You can't afford 5000 gold for the new feature.\n\r", ch );
	    return;
	}

	if (!str_cmp(arg2, "cpk"))
	{
	    send_to_char("This room is now CPK!\n\r", ch);
	    SET_BIT(ch->in_room->room_flags, ROOM_CPK);
	    ch->gold -= 5000;
	}
	else if (!str_cmp(arg2, "pk"))
	{
	    send_to_char("This room is now PK!\n\r", ch);
	    SET_BIT(ch->in_room->room_flags, ROOM_PK);
	    ch->gold -= 5000;
	}
	else if (!str_cmp(arg2, "private"))
	{
	    send_to_char("This room is now PRIVATE!\n\r", ch);
	    SET_BIT(ch->in_room->room_flags, ROOM_PRIVATE);
	    ch->gold -= 5000;
	}
	else if (!str_cmp(arg2, "underwater"))
	{
	    send_to_char("This room is now filled with water!\n\r", ch);
	    SET_BIT(ch->in_room->room_flags, ROOM_UNDERWATER);
	    ch->gold -= 5000;
	}
	else if (!str_cmp(arg2, "safe"))
	{
	    send_to_char("This room is now a SAFE ROOM!\n\r", ch);
	    SET_BIT(ch->in_room->room_flags, ROOM_SAFE);
	    ch->gold -= 5000;
	}
	else
	{
  	    send_to_char("That isn't a valid room feature.\n\r", ch);
	    send_to_char("Valid features are: cpk pk private underwater safe.\n\r", ch);
	    return;
	}

	SET_BIT(ch->in_room->area->area_flags, AREA_CHANGED);

	return;
    }
    else
    {
	send_to_char("House syntax:\n\r", ch);
	send_to_char("HOUSE BUY         - Buy property you are standing in.\n\r", ch);
	send_to_char("HOUSE NAME        - Change the name of the house room.\n\r", ch);
	send_to_char("HOUSE DESCRIPTION - Change your room description.\n\r", ch);
	send_to_char("HOUSE FEATURE     - Buy a house room feature.\n\r", ch);
    }
}


void do_evict(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    DESCRIPTOR_DATA d;

    if (ch->tot_level < MAX_LEVEL - 1)
    {
	send_to_char("You don't have clearance to do this.\n\r", ch );
	return;
    }

    argument = one_argument(argument, arg);

    if( arg[0] == '\0')
    {
	send_to_char( "Evict whom?\n\r", ch );
	return;
    }

    if ( ( victim = get_char_world( ch, arg ) ) == NULL )
    {
       if (!load_char_obj(&d, arg))
       {
	   send_to_char("There is no player by that name.\n\r", ch );
	   return;
       }
       else
       {
           d.character->desc = NULL;

	   if ( d.character->home != 0 )
	   {
	       d.character->home = 0;
	       act("You have evicted $N!", ch, d.character, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	   }
	   else
	   {
	       act("$N doesn't have a home!", ch, d.character, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
	   }

	   save_char_obj(d.character);

	   do_quit(d.character,NULL);
	   return;
       }
    }

    if (victim->home != 0)
    {
	victim->home = 0;
	if ( victim != ch )
	    act("You have evicted $N!",ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	else
	    act("You have evicted yourself!", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    }
    else
	act("$N doesn't have a home!", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
}


void do_housemove(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char buf[MSL];
    CHAR_DATA *victim;
    DESCRIPTOR_DATA d;
    ROOM_INDEX_DATA *to_room;

    argument = one_argument(argument, arg);
    argument = one_argument_norm(argument, arg2);

    if ( ch->tot_level < MAX_LEVEL)
    {
	send_to_char("You don't have clearance to do this.\n\r", ch );
	return;
    }

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char( "Syntax:\n\r" , ch);
	send_to_char( "Housemove <person> <room vnum>\n\r", ch);
	return;
    }

    if ( ( to_room = get_room_index( atol( arg2 ) ) ) == NULL )
    {
	send_to_char("The destination room doesn't exist.\n\r", ch );
	return;
    }

    if ( ( victim = get_char_world( ch, arg ) ) == NULL )
    {
        if (!load_char_obj(&d, arg))
	{
	    send_to_char("There is no player by that name.\n\r", ch );
	    return;
	}
	else
	{
	    d.character->desc = NULL;
	    victim = d.character;

	    victim->home = to_room->vnum;
	    sprintf( buf, "$N's home is now %ld (%s).",
			    to_room->vnum, to_room->name );
	    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );

	    char_to_room( victim, get_room_index( 1 ) );
	    do_quit(victim,NULL);
	    return;
	}
    }

    victim->home = to_room->vnum;
    sprintf( buf, "$N's home is now %ld (%s).",
	to_room->vnum, to_room->name );
    act( buf, ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
    save_char_obj( victim );
}


void do_gohome(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *location;

    if (IS_DEAD(ch))
    {
        send_to_char("You are dead. You can't gohome.\n\r", ch);
	return;
    }

    if (ch->home == 0 )
    {
	send_to_char("{BYou don't own a home.\n\r{x", ch);
	return;
    }

    if (ch->home != 0 && (location = get_room_index(ch->home)) == NULL)
    {
	send_to_char("You don't own a home.{x\n\r", ch );
	ch->home = 0;
	save_char_obj( ch );
	return;
    }

    if ( ch->no_recall > 0 )
    {
        send_to_char("You can't summon enough energy.\n\r", ch );
        return;
    }

    if (IS_SET(ch->in_room->room_flags, ROOM_NO_RECALL))
    {
	send_to_char("Not from this room.\n\r", ch);
	return;
    }
	// Adding this to go with the no_recall area flag. - Areo 08-10-2006
    if (IS_SET(ch->in_room->area->area_flags, AREA_NO_RECALL))
    {
	send_to_char("Nothing happens.\n\r", ch);
	return;
	}

    if (PULLING_CART(ch) )
    {
	send_to_char("You must first drop what you are pulling.\n\r", ch);
	return;
    }

    if ( !can_escape(ch) )
	return;

    if (is_affected(ch, skill_lookup("silence")))
    {
	send_to_char("You can't go home, you've been silenced!\n\r", ch);
	return;
    }

    if (is_affected(ch, skill_lookup("web")))
    {
	send_to_char("You can't go home, you've been webbed!\n\r", ch);
	return;
    }

    if (ch->fighting != NULL)
    {
	send_to_char("Finish your fight first.\n\r", ch);
	return;
    }

    if ( !str_cmp( ch->in_room->area->name, "Wilderness") )
    {
	if ( get_region( ch ) != REGION_FIRST_CONTINENT )
	{
	    send_to_char("Your home is too far away.\n\r", ch );
	    return;
	}
    }
    else
    {
	if ( !SAME_PLACE(ch->in_room, location) )
	{
	    send_to_char("Your home is too far away.\n\r", ch );
	    return;
	}
    }

    send_to_char
	("You click your heels three times and whisper \"there's no place like home.\"\n\r",
	 ch);
    act("{RThere is a tremendous explosion as a swirling vortex forms!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{RThere is a tremendous explosion as a swirling vortex forms!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("$n steps into the swirling vortex.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("You step into the swirling vortex.", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
    act("{RThere is a loud pop as the vortex collapses in on itself.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    act("{RThere is a loud pop as the vortex collapses in on itself.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

    char_from_room(ch);
    char_to_room(ch, location);

    do_function(ch, &do_look, "auto");
}
