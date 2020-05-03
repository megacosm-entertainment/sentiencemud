/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "merc.h"
#include "interp.h"
#include "recycle.h"
#include "db.h"
#include "tables.h"

AUTO_WAR	*auto_war;
int 		auto_war_timer;
int 		auto_war_battle_timer;


void do_war(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if ( arg[0] == '\0' )
    {
	if ( !IS_SET( ch->comm, COMM_NOAUTOWAR ) )
	{
	    SET_BIT( ch->comm, COMM_NOAUTOWAR );
	    send_to_char("WAR channel off.\n\r", ch );
	}
	else
	{
	    REMOVE_BIT( ch->comm, COMM_NOAUTOWAR );
	    send_to_char("WAR channel on.\n\r", ch );
	}

	return;
    }

    if (!str_prefix(arg, "join"))
    {
	if ( auto_war == NULL )
	{
	    send_to_char( "There is no war currently active.\n\r", ch );
	    return;
	}

	if ( ch->in_war )
	{
	    send_to_char( "You have already joined the battle.\n\r", ch );
	    return;
	}

	if ( IS_DEAD( ch ) )
	{
	    send_to_char( "You can't, you are DEAD.\n\r", ch );
	    return;
	}

	if ( auto_war_timer == 0 )
	{
	    send_to_char( "The war has already started.\n\r", ch );
	    return;
	}

	if ( ch->tot_level < auto_war->min ||
		ch->tot_level > auto_war->max )
	{
	    send_to_char( "You are outside the level range for this war.\n\r", ch );
	    return;
	}

	if ( auto_war->war_type == AUTO_WAR_JIHAD && ( ch->alignment == 0 && IS_CHURCH_NEUTRAL(ch)))
	{
	    send_to_char( "As you have no religion and neutral alignment, you are unable to take part in this war.\n\r", ch );
	    return;
	}

	/* Add player to war */
	/* Check how many players are in each team */
	char_to_team( ch );

	act( "{D$n disappears in a puff of smoke.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );

	char_from_room( ch );
	char_to_room( ch, get_room_index( ROOM_VNUM_AUTO_WAR ) );

	send_to_char( "{YYou have joined the battle!{x\n\r", ch );
	act( "{Y$n has joined the battle!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act( "{D$n appears in a puff of smoke.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );

	do_function(ch, &do_look, "auto");
	return;
    }

    if (!str_prefix(arg, "statistics"))
    {
	if ( auto_war == NULL )
	{
	    send_to_char( "There is no war currently active.\n\r", ch );
	    return;
	}

	if ( auto_war_timer > 0 )
	{
	    CHAR_DATA *wch;

	    sprintf( buf, "{R,.-'``^``'-.,'- {YWar Statistics {R-',.-'``^``'-.,{x\n\r"
		    "\n\rThis is a {Y%s{x war for levels {Y%d{x - {Y%d{x.\n\r"
		    "\n\rThe minimum number of players required is {G%d{x.\n\r"
		    "\n\rThere is still approximately {Y%d{x minutes remaining before the start.\n\r"
		    "\n\rThe following players have entered the war:\n\r",
		    auto_war_table[ auto_war->war_type ].name,
		    auto_war->min,
		    auto_war->max,
		    auto_war->min_players,
		    auto_war_timer );
	    send_to_char( buf, ch );

	    if ( auto_war->team_players == NULL )
	    {
		send_to_char( "No players have currently entered.\n\r", ch );
		return;
	    }

	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		sprintf( buf, "{B[%s%-3ld%%{B] {B[{G%-3d{B:{G%3d{B] {B[ {R%-10s {B] {G%-36s{x\n\r",
			wch->hit < wch->max_hit * 2 / 3 ?
			( wch->hit < wch->max_hit / 2  ?
			  (wch->hit < wch->max_hit / 3 ? "{r" : "{R" ) : "{Y" )
			: "{G",
			((100 * wch->hit) / wch->max_hit),
			wch->level,
			wch->tot_level,
			capitalize( race_table[ wch->race ].name ),
			wch->name);
		send_to_char( buf, ch );
	    }
	}
	else
	{
	    CHAR_DATA *wch;

	    sprintf( buf, "{R,.-'`^`'-.,'- {YWar Statistics {R-',.-'`^`'-.,{x\n\r"
		    "\n\rThis is a {Y%s{x war for levels {Y%d{x - {Y%d{x.\n\r"
		    "\n\rThe war has started. There is {Y%d{x minutes remaining till the end.\n\r"
		    "\n\rThe following players have entered the war:\n\r",
		    auto_war_table[ auto_war->war_type ].name,
		    auto_war->min,
		    auto_war->max,
		    auto_war_battle_timer );
	    send_to_char( buf, ch );

	    if ( auto_war->team_players == NULL )
	    {
		send_to_char( "No players are currently in the war.\n\r", ch );
		return;
	    }

	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		sprintf( buf, "{B[%s%-3d%%{B] {B[{G%-3d{B:{G%3d{B] {B[ {R%-10s {B] {G%-36s{x\n\r",
			wch->hit < wch->max_hit * 2 / 3 ?
			( wch->hit < wch->max_hit / 2  ?
			  (wch->hit < wch->max_hit / 3 ? "{r" : "{R" ) : "{Y" )
			: "{G",
			(int)(wch->hit / wch->max_hit * 100),
			wch->level,
			wch->tot_level,
			capitalize( race_table[ wch->race ].name ),
			wch->name
		       );
		send_to_char( buf, ch );
	    }
	}

	return;
    }

    send_to_char( "Syntax:  war <join|statistics>\n\r", ch );
}


void scatter_players()
{
    ROOM_INDEX_DATA *pRoom = NULL;
    CHAR_DATA *ch;
    AREA_DATA *pArea = find_area( "Autowar Battlefield" );

    if (auto_war == NULL)
	return;

    for (ch = auto_war->team_players; ch != NULL; ch = ch->next_in_auto_war)
    {
	do
	    pRoom = get_random_room_area(ch, pArea);
	while (pRoom == NULL || pRoom->vnum == ROOM_VNUM_AUTO_WAR);

	act( "{D$n disappears in a puff of smoke.{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM );
	act( "{YYou have been transferred to the battlefield!{x", ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );

	char_from_room(ch);
	char_to_room(ch, pRoom);

	do_function(ch, &do_look, "auto");
    }
}


void auto_war_echo( char *message)
{
    CHAR_DATA *ch = NULL;

    if ( auto_war == NULL )
	return;

    for (ch = auto_war->team_players; ch != NULL; ch = ch->next_in_auto_war)
	send_to_char(message, ch);
}


void start_war()
{
    CHAR_DATA *wch;
    int counter;
    int race;
    int evil;
    int good;
    char buf[ MAX_STRING_LENGTH ];

    /* Check if we have enough players */
    if ( auto_war->team_players == NULL )
    {
	sprintf( buf, "{RDue to insufficient players, the war has been cancelled.{x\n\r" );
	war_channel( buf );
	free_auto_war( auto_war );
	return;
    }

    switch (auto_war->war_type)
    {
	case AUTO_WAR_FREE_FOR_ALL:
	    counter = 0;
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
		counter++;

	    if ( counter < auto_war->min_players )
	    {
		sprintf( buf, "{RDue to a lack of players, the war has been cancelled.{x\n\r" );
		war_channel( buf );
		free_auto_war( auto_war );
		return;
	    }

	    break;

	case AUTO_WAR_GENOCIDE:
	    race = auto_war->team_players->race;
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		if ( wch->race != race )
		    break;
	    }

	    if ( wch == NULL )
	    {
		sprintf( buf, "{RDue to a lack of players, the war has been cancelled.{x\n\r" );
		war_channel( buf );
		free_auto_war( auto_war );
		return;
	    }

	    break;

	case AUTO_WAR_JIHAD:
	    evil = 0;
	    good = 0;
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		if ( wch->alignment < 0 || IS_CHURCH_EVIL(wch))
		    evil++;
		else
		if ( wch->alignment > 0 || IS_CHURCH_GOOD(wch))
		    good++;
	    }

	    if (evil == 0 || good == 0)
	    {
		sprintf( buf, "{RDue to a lack of players, the war has been cancelled.{x\n\r" );
		war_channel( buf );
		free_auto_war( auto_war );
		return;
	    }

	    break;
    }

    sprintf( buf, "{RLet the {Y%s{R begin!{x\n\r", auto_war_table[ auto_war->war_type ].name );
    war_channel( buf );

    scatter_players();
    auto_war_battle_timer = 10;
}


void auto_war_time_finish()
{
    char buf[MAX_STRING_LENGTH];

    if ( auto_war == NULL )
	return;

    if ( auto_war_timer > 0 )
	return;

    sprintf( buf, "{RThe {Y10{R minute time limit is up.{x\n\r" );
    auto_war_echo( buf );

    if (auto_war->war_type == AUTO_WAR_FREE_FOR_ALL)
    {
	/* If the timer ends then the war is over. If a player had won the war would already
	 * be over. */
	free_auto_war( auto_war );
    }
    else
    if ( auto_war->war_type == AUTO_WAR_GENOCIDE )
    {
	CHAR_DATA *wch;
	int races[26];
	int i = 0;
	int max = 0;
	int min = 0;
	int quest_points = 0;

	/* Initialise races array */
	for ( i = 0; i < 26; i++ )
	    races[i] = 0;

	/* Find which race was most dominant */
	for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    races[wch->race]++;

	/* Find greatest value in array */
	for ( i = 0; i < 26; i++ )
	{
	    if ( races[ i ] > races[ max ] )
		max = i;
	}

	min = max;
	for ( i = 0; i < 26; i++ )
	{
	    if ( races[ i ] < races[ min ] )
		min = i;
	}

	    /*
	if ( min == max )
	{
	    sprintf( buf, "{RThe war was a draw.{x\n\r" );
	    war_channel( buf );
	    free_auto_war( auto_war );
	    return;
	}
	    */

	sprintf( buf, "{RThe winners of the {Y%s{R war were the {W%ss{R!{x\n\r",
	    auto_war_table[ auto_war->war_type ].name,
	    capitalize( race_table[ max ].name ) );
	war_channel( buf );

	/* Reward winners*/
	for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	{
	    if ( wch->race == max )
	    {
		quest_points = 50;
		sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
		act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		wch->questpoints += quest_points;
		wch->wars_won++;
	    }
	}

	free_auto_war( auto_war );
    }
    else
    if ( auto_war->war_type == AUTO_WAR_JIHAD )
    {
	CHAR_DATA *wch = NULL;
	int good = 0;
	int evil = 0;
	int quest_points = 0;

	/* Find which alignment was most dominant */
	for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	{
	    if ( wch->alignment < 0 || IS_CHURCH_EVIL(wch))
		evil++;
	    else
	    if ( wch->alignment > 0 || IS_CHURCH_GOOD(wch))
		good++;
	}

	if (good == 0 && evil == 0)
	{
	    sprintf( buf, "{RBoth evil and good were completely decimated.{x\n\r" );
	    war_channel( buf );
	}
	else
	if ( good > evil )
	{
	    sprintf( buf, "{RThe legions of light prevail victorious as winners of the {Y%s{R war!{x\n\r",
		    auto_war_table[ auto_war->war_type ].name );
	    war_channel( buf );

	    if (auto_war->team_players == NULL )
		gecho(" NULL " );

	    /* Reward winners*/
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		if ( wch->alignment > 0 )
		{
		    quest_points = 50;
		    sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
		    act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		    wch->questpoints += quest_points;
		    wch->wars_won++;
		}
	    }
	}
	else
	if ( good < evil )
	{
	    sprintf( buf, "{RThe armies of darkness prevail victorious as winners of the {Y%s{R war!{x\n\r",
		    auto_war_table[ auto_war->war_type ].name );
	    war_channel( buf );

	    /* Reward winners*/
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		if ( wch->alignment < 0 )
		{
		    quest_points = 50;
		    sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
		    act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		    wch->questpoints += quest_points;
		    wch->wars_won++;
		}
	    }
	}
	else
	if ( good == evil )
	{
	    sprintf( buf, "{RThe battle has ended in a draw.{x\n\r" );
	    war_channel( buf );
	}

	free_auto_war( auto_war );
    }
}


void test_for_end_of_war()
{
    char buf[MAX_STRING_LENGTH];

    if (auto_war == NULL || auto_war_timer > 0)
	return;

    /* Test for Free For All */
    if ( auto_war->war_type == AUTO_WAR_FREE_FOR_ALL )
    {
	int quest_points = 0;
	CHAR_DATA *wch;

	/* End of war is when only one race is left */
	if ( auto_war->team_players == NULL )
	{
	    sprintf( buf, "{RThe winners have forfeited the war.{x\n\r" );
	    war_channel( buf );
	}

	wch = auto_war->team_players;

	/* Is there only one player left */
	if ( wch->next_in_auto_war != NULL )
	    return;

	sprintf( buf, "{RThe almighty champion of the {Y%s{R was {W%s{R!{x\n\r",
	    auto_war_table[ auto_war->war_type ].name,
	    wch->name);
	war_channel( buf );

	/* Reward winners*/
	quest_points = 50;
	sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
	act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
	wch->questpoints += quest_points;
	wch->wars_won++;

	free_auto_war( auto_war );
    }
    else
    if ( auto_war->war_type == AUTO_WAR_GENOCIDE )
    {
	CHAR_DATA *wch;
	int race;
	bool ended = TRUE;

	/* End of war is when only one race is left */
	if ( auto_war->team_players == NULL )
	{
	    sprintf( buf, "{RThe winners have forfeited the war.{x\n\r" );
	    war_channel( buf );
	    return;
	}

	race = auto_war->team_players->race;

	/* Is there another player of a different race */
	for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	{
	    if ( wch->race != race )
		ended = FALSE;
	}

	if ( ended )
	{
	    int quest_points = 0;

	    sprintf( buf, "{RThe winners of the {Y%s{R war were the {W%ss{R!{x\n\r",
		    auto_war_table[ auto_war->war_type ].name,
		    capitalize( race_table[ race ].name ) );
	    war_channel( buf );

	    /* Reward winners*/
	    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	    {
		if ( wch->race == race )
		{
		    quest_points = 50;
		    sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
		    act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
		    wch->questpoints += quest_points;
		    wch->wars_won++;
		}
	    }

	    free_auto_war( auto_war );
	}
    }
    else
    if ( auto_war->war_type == AUTO_WAR_JIHAD )
    {
	CHAR_DATA *wch;
	bool ended = TRUE;
	bool good = FALSE;
	bool evil = FALSE;

	if ( auto_war->team_players == NULL )
	{
	    sprintf( buf, "{RThe war has been forfeited.{x\n\r" );
	    war_channel( buf );
	}

	if ( auto_war->team_players->alignment < 0 )
	    evil = TRUE;
	else
	    good = TRUE;

	/* Is there another player of a different alignment */
	for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
	{
	    if ( ( evil == TRUE &&
			( wch->alignment > 0 ||
			  IS_CHURCH_GOOD( wch ) ) ) ||
		    ( good == TRUE &&
		      ( wch->alignment < 0 ||
			IS_CHURCH_EVIL( wch ) ) ) )
	    {
		ended = FALSE;
	    }
	}

	if ( ended )
	{
	    int quest_points = 0;

	    if ( good )
	    {
		sprintf( buf, "{RThe legions of light prevail victorious as winners of the {Y%s{R war!{x\n\r",
			auto_war_table[ auto_war->war_type ].name );
		war_channel( buf );

		/* Reward winners*/
		for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
		{
		    if ( wch->alignment > 0 )
		    {
			quest_points = 50;
			sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
			act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
			wch->questpoints += quest_points;
			wch->wars_won++;
		    }
		}

	    }
	    else
		if ( evil )
		{
		    sprintf( buf, "{RThe armies of darkness prevail victorious as winners of the {Y%s{R war!{x\n\r",
			    auto_war_table[ auto_war->war_type ].name );
		    war_channel( buf );

		    /* Reward winners*/
		    for ( wch = auto_war->team_players; wch != NULL; wch = wch->next_in_auto_war )
		    {
			if ( wch->alignment < 0 )
			{
			    quest_points = 50;
			    sprintf( buf, "{WYou have been awarded {Y%d{W quest points!{x", quest_points );
			    act( buf, wch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR );
			    wch->questpoints += quest_points;
			    wch->wars_won++;
			}
		    }
		}
	    free_auto_war( auto_war );
	}
    }
}


void war_channel( char *msg )
{
    DESCRIPTOR_DATA *d;

    for ( d = descriptor_list; d != NULL; d = d->next )
    {
	CHAR_DATA *victim;

	victim = d->original ? d->original : d->character;

	if ( d->connected == CON_PLAYING &&
		!IS_SET(victim->comm,COMM_NOAUTOWAR) &&
		!IS_SET(victim->comm,COMM_QUIET) )
	{
	    send_to_char( msg, victim );
	}
    }
}
