/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*
 * Displays output for statistics.
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"

STAT_DATA stat_table[10];


/* Declare local functions */

void do_stats( CHAR_DATA *ch, char *argument )
{
    char arg[MAX_STRING_LENGTH];
    BUFFER *output = NULL;

    //return;

    argument = one_argument( argument, arg );

    if (arg[0] == '\0')
    {
	send_to_char("Usage: Stats pkers | cpkers | quests | wealthiest | monsters | ratio\n\r", ch);
	return;
    }

    if ( !str_cmp( arg, "pkers" ) )
    {
	output = get_stats( REPORT_TOP_PLAYER_KILLERS );
    }
    else
    if ( !str_cmp( arg, "cpkers" ) )
    {
	output = get_stats( REPORT_TOP_CPLAYER_KILLERS );
    }
    else
    if ( !str_cmp( arg, "wealthiest" ) )
    {
	output = get_stats( REPORT_TOP_WEALTHIEST );
    }
    else
    if ( !str_cmp( arg, "monsters" ) )
    {
	output = get_stats( REPORT_TOP_MONSTER_KILLERS );
    }
    else
    if ( !str_cmp( arg, "ratio" ) )
    {
	output = get_stats( REPORT_TOP_WORST_RATIO );
    }
    else
    if ( !str_cmp( arg, "quests" ) )
    {
	output = get_stats( REPORT_TOP_QUESTS );
    }
    else
    {
	send_to_char("Usage: Stats <pkers|cpkers|quests|monsters|wealthiest|ratio>\n\r", ch);
	return;
    }

    page_to_char(buf_string(output), ch);
    free_buf(output);
}


BUFFER *get_stats( int type)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    BUFFER *output;
    int i;

    buf[0] = '\0';
    output = new_buf();

    if ( stat_table[type].report_name == NULL )
    {
	snprintf( buf, sizeof(buf), "Sorry, these stats are currently unavailable.\n\r" );
	add_buf( output, buf );
	bug("get_stats: stats for type %d weren't loaded!", type );
	return output;
    }

    add_buf(output, stat_table[type].report_name);
    add_buf(output, "\r\n\r\n");
    add_buf(output, stat_table[type].description);
    add_buf(output, "\r\n\r\n");

    sprintf( buf2, "Rank %%-%ds %%s\r\n", 44 - (int)strlen( stat_table[type].column[1] ) / 2 );
    sprintf( buf, buf2, stat_table[type].column[0], stat_table[type].column[1] );
    add_buf(output, buf);

    add_buf(output, "{B----------{C------------------{W------------{C------------------{B---------{x");
    add_buf(output, "\r\n");

    for ( i = 0; i < 10; i++ )
    {
	sprintf( buf, " {Y#%-4d{x %-40s %s\n\r", i+1, stat_table[type].name[i], stat_table[type].value[i] );
	add_buf(output, buf);
    }

    return output;
}


BUFFER *get_stats_for_html( int type)
{
    char buf[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    BUFFER *output;
    int i;

    buf[0] = '\0';
    output = new_buf();

    add_buf(output, stat_table[type].report_name);
    add_buf(output, "\r\n\r\n{w");
    add_buf(output, stat_table[type].description);

    //sprintf( buf2, "<table width=\"80%\" border=\"0\" style=\"padding: 15px; padding-top: 10px;\"><tr><td width=\"10%\">Rank</td><td width=\"70%\">%%-%ds</td><td width=\"20%\">%%s</td></td></tr>", 44 - strlen( stat_table[type].column[1] ) / 2 );
    sprintf( buf2, "</td></tr><tr><td valign=\"top\"><table width=\"80%%\" border=\"0\" style=\"padding: 15px;\"><tr><td width=\"10%%\">Rank</td><td width=\"70%%\">%%-%ds</td><td width=\"20%%\">%%s</td></td></tr>", 44 - (int)strlen( stat_table[type].column[1] ) / 2 );
    sprintf( buf, buf2, stat_table[type].column[0], stat_table[type].column[1] );
    add_buf(output, buf);

    add_buf(output, "<tr><td colspan=\"3\">{B----------{C------------------{W--------------------------------------{C------------------{B---------{x</td></tr>");
    for ( i = 0; i < 10; i++ )
    {
	sprintf( buf, "<tr><td width=\"10%%\">{Y#%-4d{x</td><td width=\"70%%\">%-40s</td><td width=\"20%%\">%s</td></tr>", i+1, stat_table[type].name[i], stat_table[type].value[i] );
	add_buf(output, buf);
    }

    add_buf(output, "</table>");
    return output;
}
