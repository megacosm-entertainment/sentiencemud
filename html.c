/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

/*
 * Displays output commands for HTML page.
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"

/* Declare local functions */
char *init_html();
int copy_in_colour_code( char *new, char *colour );


BUFFER *get_stats_html( int type)
{
    BUFFER *output;
    char *text;

    output = new_buf();

    /* Get html header */
    text = init_html();

    add_buf(output, text);

    free_string( text );

    add_buf( output, buf_string( get_stats_for_html(type) ) );
    add_buf( output, "</td></tr></table></body></html>" );

    return output;
}


BUFFER *get_churches_html()
{
    CHURCH_DATA *chr;
    BUFFER *output;
    char *text;
    char temp[MAX_STRING_LENGTH*2];

    chr = NULL;

    output = new_buf();

    /* Get html header */
    text = init_html();

    add_buf(output, text);

    free_string( text );

    add_buf( output, "<span class=\"title\">Churches of Sentience</span><br/>" );
    add_buf( output, "<table cellspacing=\"0\" cellpadding=\"0\" style=\"width: 100%; margin-top: 30px;\">" );
    add_buf( output, "<tr><td valign=\"top\">" );
    add_buf( output, "The following is a list of the Churches within Sentience:<br/>" );
    add_buf( output, "<font colour=\"#FF99CC\">(Churches are sorted by date first created)</font><br/><br/>" );

    // Table
    add_buf( output, "<table width=\"100%\" border=\"0\" style=\"padding: 15px; padding-top: 10px;\"> <tr> <td class=\"title\">Church Name&nbsp;</td> <td class=\"title\">Church Type&nbsp;</td> <td class=\"title\">Alignment&nbsp;</td> <td class=\"title\">Player Killer?&nbsp;</td> </tr>" );

    for (chr = church_list; chr != NULL; chr = chr->next)
    {
	char tempbuf[25];

	switch (chr->size)
	{
	    case CHURCH_SIZE_BAND:
		sprintf(tempbuf, "Band");
		break;
	    case CHURCH_SIZE_CULT:
		sprintf(tempbuf, "Cult");
		break;
	    case CHURCH_SIZE_ORDER:
		sprintf(tempbuf, "Order");
		break;
	    case CHURCH_SIZE_CHURCH:
		sprintf(tempbuf, "Church");
		break;
	}
	sprintf( temp, "<tr><td>%s&nbsp;</td><td>%s&nbsp;</td><td>%s&nbsp;</td>", chr->name, tempbuf, (chr->alignment == CHURCH_GOOD ? "Good" : (chr->alignment == CHURCH_EVIL ? "Evil" : "Neutral")));
	add_buf( output, temp );

	if ( !chr->pk )
	{
	    sprintf( temp, "<td>No&nbsp;</td></tr>");
	}
	else
	{
	    sprintf( temp, "<td>{RYes{x ({W%ld{x wins : {Y%ld{x losses)&nbsp;</td></tr>",
		    chr->pk_wins, chr->pk_losses );
	}

	add_buf( output, temp );

    }
    add_buf( output, "</table></td></tr></table></body></html>" );

    return output;
}


char *init_html()
{
    char html[] = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"> <html> <head> <META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\"> <META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\"> <title>Sentience</title> <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\"> <link href=\"../styles.css\" rel=\"stylesheet\" type=\"text/css\"> </head> <body leftmargin=\"0\" topmargin=\"0\" marginwidth=\"0\" marginheight=\"0\" tracingsrc=\"../images/history-template-middle-middle.gif\" tracingopacity=\"52\"> <table width=\"100%\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\"> <tr> <td style=\"padding: 15px; padding-top: 10px;\">";

    return str_dup(html);
}


char *format_and_colour_html( char *buf )
{
    char newbuf[16000];
    char *a, *b;
    bool enc = false;

    a = buf;
    b = newbuf;
    while( *a != '\0' )
    {
	// Look for colour code
	if ( enc )
	{
	    int skip = 0;

	    switch( *a )
	    {
		case 'r' :
		    skip = copy_in_colour_code( b, "<font colour=\"990000\">" );
		    break;
		case 'R' :
		    skip = copy_in_colour_code( b, "<font colour=\"FF0000\">" );
		    break;
		case 'x' :
		    skip = copy_in_colour_code( b, "</font>" );
		    break;
		case 'C' :
		    skip = copy_in_colour_code( b, "<font colour=\"6666FF\">" );
		    break;
		case 'c' :
		    skip = copy_in_colour_code( b, "<font colour=\"33CCCC\">" );
		    break;
		case 'B' :
		    skip = copy_in_colour_code( b, "<font colour=\"0000FF\">" );
		    break;
		case 'b' :
		    skip = copy_in_colour_code( b, "<font colour=\"003399\">" );
		    break;
		case 'Y' :
		    skip = copy_in_colour_code( b, "<font colour=\"FFFF00\">" );
		    break;
		case 'y' :
		    skip = copy_in_colour_code( b, "<font colour=\"FFCC66\">" );
		    break;
		case 'W' :
		    skip = copy_in_colour_code( b, "<font colour=\"FFFFFF\">" );
		    break;
		case 'w' :
		    skip = copy_in_colour_code( b, "<font colour=\"CCCCCC\">" );
		    break;
		case 'G' :
		    skip = copy_in_colour_code( b, "<font colour=\"00FF00\">" );
		    break;
		case 'g' :
		    skip = copy_in_colour_code( b, "<font colour=\"009966\">" );
		    break;
		case 'M' :
		    skip = copy_in_colour_code( b, "<font colour=\"CC00FF\">" );
		    break;
		case 'm' :
		    skip = copy_in_colour_code( b, "<font colour=\"993399\">" );
		    break;
		case 'D' :
		    skip = copy_in_colour_code( b, "<font colour=\"666666\">" );
		    break;
		case '{' :
		    skip = copy_in_colour_code( b, "{" );
		    break;
		default:
		    skip = copy_in_colour_code( b, "</font>" );
	    }
	    while( skip-- > 0 )
	    {
		b++;
	    }
	    enc = false;
	}
	else
	    if ( *a == '{' )
	    {
		enc = true;
	    }
	    else
	    {
		*b = *a;
		b++;
	    }

	a++;
    }

    *b = *a;

    //log_string( "test" );
    //log_string( newbuf );
    return str_dup( newbuf );
}


int copy_in_colour_code( char *new, char *colour )
{
    int skip = strlen( colour );
    while( *colour != '\0')
    {
	*new = *colour;

	colour++;
	new++;
    }

    return skip;
}


BUFFER *get_players_html()
{
    char buf[2*MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    char *text;
    BUFFER *output;
    DESCRIPTOR_DATA *d;
    int iLevelLower;
    int iLevelUpper;
    int nMatch;
    int nMatch2;
    bool fImmortalOnly = false;
    bool fChurchOnly = false;
    int line_counter = 0;
    int buf_size;
    char *area_type;
    CHURCH_DATA *church = NULL;

    /*
     * Set default arguments.
     */
    iLevelLower = 0;
    iLevelUpper = MAX_LEVEL;

    /*
     * Now show matching chars.
     */
    nMatch = 0; /* players found */
    nMatch2 = 0; /* players online */
    for ( d = descriptor_list; d != NULL; d = d->next )
    {
	CHAR_DATA *wch;

	if (d->connected != CON_PLAYING )
	    continue;

	wch = (d->original != NULL) ? d->original : d->character;

	if ( wch )
	{
	    nMatch2++;
	}
    }

    buf[0] = '\0';
    output = new_buf();

    /* Get html header */
    text = init_html();

    add_buf(output, text);

    free_string( text );

    add_buf( output, " {b.,-~^~{B-,._.,{C[ {WPlayers in Sentience {C]{B-.._.,-{b~^~-,.{x\n" );
    add_buf( output, "<table cellspacing=\"0\" cellpadding=\"0\" style=\"width: 100%; margin-top: 30px;\">" );
    add_buf( output, "<tr><td valign=\"top\">" );

    add_buf( output, "<table width=\"100%\" border=\"0\" style=\"padding: 15px; padding-top: 10px;\"> <tr> <td class=\"title\">&nbsp;</td> <td class=\"title\">&nbsp;</td> <td class=\"title\">&nbsp;</td> <td class=\"title\">&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr>" );

    for (d = descriptor_list; d != NULL; d = d->next)
    {
	CHAR_DATA *wch;
	char const *class;
	char racestr[MAX_STRING_LENGTH];

	if (d->connected != CON_PLAYING)
	    continue;

	wch = (d->original != NULL) ? d->original : d->character;

	if (wch->tot_level < iLevelLower
		|| wch->tot_level > iLevelUpper
		|| (fImmortalOnly && wch->tot_level < LEVEL_IMMORTAL)
		|| (fChurchOnly && wch->church != church ))
	    continue;

	class = sub_class_table[wch->pcdata->sub_class_current].who_name[wch->sex];

	switch (wch->level)
	{
	    default:
		break;
		{
		    case MAX_LEVEL - 0:
			class = "  {w-{W=I{DM{WP={w-{x   ";
			break;
		    case MAX_LEVEL - 1:
			class = "  {RC{rr{Re{ra{Rt{ro{RR{x   ";
			break;
		    case MAX_LEVEL - 2:
			class = " {WSup{Drem{WacY{x  ";
			break;
		    case MAX_LEVEL - 3:
			class = " {bAsc{Bend{bant  ";
			break;
		    case MAX_LEVEL - 4:
			if ( wch->sex == SEX_FEMALE )
			    class = "  {wGo{Wdde{wss   ";
			else
			    class = "    {wG{Wo{wd     ";
			break;
		    case MAX_LEVEL - 5:
			class = "  {BM{Ci{MN{Di{YG{Go{Wd   ";
			break;
		    case MAX_LEVEL - 6:
			class = "  {x-{m=G{xIM{mP={x-  ";
			break;
		}
	}

	/*
	 * Format it up.
	 */

	sprintf( racestr, "{Y");
	strncat( racestr, pc_race_table[wch->race].who_name, 6 );

	nMatch++;

	area_type = get_char_where(wch);
	sprintf(buf,
		"<tr>"
		"<td>{G%-3d{x</td>"
		"<td>{G%-3d{x</td>"
		"<td>{M%s{x</td>"
		"<td>{Y%6s{x</td>"
		"<td>{R%12s{x</td>"
		"<td>{C%-6s{x</td> "
		"<td>%s%s%s%s{G%-12s{x</td>"
		"</tr>",
		wch->level,
		wch->tot_level,
		wch->sex == 0 ? "N" : (wch->sex == 1 ? "M" : "F"),
		wch->race < MAX_PC_RACE ? racestr : "      ",
		class,
		area_type,
		(IS_DEAD(wch) /*&& !IS_DEMON(wch) && !IS_ANGEL(wch)*/) ?
		"{D(Dead) {x" : "",
		wch->incog_level >= LEVEL_HERO ? "{D(Incog) {x" : "",
		wch->invis_level >= LEVEL_HERO ? "{W(Wizi) {x" : "",
		//IS_SET(wch->comm, COMM_AFK) ? "{M[AFK] {x" : "",
		//IS_SET(wch->comm, COMM_QUIET) ? "{R[Q] {x" : "",
		IS_SET(wch->act[0], PLR_BOTTER) ? "{G[BOTTER] {x" : "",
		wch->name);

		free_string(area_type);
		add_buf(output, buf);

		if (wch->church != NULL)
		{
		    buf_size = 50 - fstr_len(&buf[0]);

		    for (line_counter = 0; line_counter < buf_size; line_counter++)
		    {
			add_buf(output, " ");
		    }
		    add_buf(output, "{Y[{x");
		    add_buf(output, wch->church->flag);
		    add_buf(output, "{Y]{x");
		}
		else
		{
		    add_buf(output, "");
		}

		if (IS_SET(wch->act[0],PLR_HELPER))
		{
		    add_buf(output, " {W[H]{X");
		}

		if (IS_SET(wch->comm, COMM_AFK))
		{
		    add_buf(output, " {M[AFK]{x");
		}

		if (IS_SET(wch->comm, COMM_QUIET) )
		{
		    add_buf(output, " {R[Q]{x");
		}

		add_buf(output, "\n\r");
    }

    add_buf( output, "</table>" );
    sprintf(buf2, "\nPlayers online: %d\n", nMatch2);
    add_buf(output, buf2);

    add_buf( output, "</td></tr></table></body></html>" );

    return output;
}
