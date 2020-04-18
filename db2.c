/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include "merc.h"
#include "db.h"
#include "tables.h"

int social_count;
struct social_type	social_table		[MAX_SOCIALS];


void load_socials( FILE *fp)
{
    for ( ; ; )
    {
    	struct social_type social;
    	char *temp;
        /* clear social */
	social.char_no_arg = NULL;
	social.others_no_arg = NULL;
	social.char_found = NULL;
	social.others_found = NULL;
	social.vict_found = NULL;
	social.char_not_found = NULL;
	social.char_auto = NULL;
	social.others_auto = NULL;

    	temp = fread_word(fp);
    	if (!strcmp(temp,"#0"))
	    return;  /* done */
#if defined(social_debug)
	else
	    printf("%s\n\r",temp);
#endif

    	strcpy(social.name,temp);
    	fread_to_eol(fp);

	temp = fread_string_eol(fp);
	if (!strcmp(temp,"$"))
	     social.char_no_arg = NULL;
	else if (!strcmp(temp,"#"))
	{
	     social_table[social_count] = social;
	     social_count++;
	     continue;
	}
        else
	    social.char_no_arg = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.others_no_arg = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.others_no_arg = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.char_found = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
       	else
	    social.char_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.others_found = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.others_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.vict_found = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.vict_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.char_not_found = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.char_not_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.char_auto = NULL;
        else if (!strcmp(temp,"#"))
        {
	     social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.char_auto = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp,"$"))
             social.others_auto = NULL;
        else if (!strcmp(temp,"#"))
        {
             social_table[social_count] = social;
             social_count++;
             continue;
        }
        else
	    social.others_auto = temp;

	social_table[social_count] = social;
    	social_count++;
   }
}


/* Reset the GQ */
void global_reset( void )
{
    GQ_MOB_DATA *gq_mob;
    GQ_OBJ_DATA *gq_obj;
    CHAR_DATA *ch;
    ROOM_INDEX_DATA *room = NULL;

    // Repop mobs
    for ( gq_mob = global_quest.mobs; gq_mob != NULL; gq_mob = gq_mob->next )
    {
	do
	{
	    room = get_random_room( NULL, 0 );
	} while ( room == NULL );

	if ( gq_mob->count < gq_mob->max )
	{
	    OBJ_DATA *obj;

	    if ( gq_mob->class == 1
	    && gq_mob->count + 1 > 50 )
		continue;

	    ch = create_mobile(get_mob_index(gq_mob->vnum), FALSE);
	    if ( gq_mob->obj != 0 )
	    {
		obj = create_object( get_obj_index( gq_mob->obj ),
			get_obj_index( gq_mob->obj )->level,
			FALSE);
		obj_to_char(obj, ch);
	    }

	    char_to_room(ch, room);
	}
    }

    // Repop objects
    for ( gq_obj = global_quest.objects; gq_obj != NULL; gq_obj = gq_obj->next )
    {
	int attempts = 100;
	do
	{
	    room = get_random_room( NULL, 0 );
	} while ( room == NULL && --attempts >= 0 );

	if ( gq_obj->count < gq_obj->max )
	{
	    OBJ_DATA *obj;

	    if ( number_percent() < gq_obj->repop )
	    {
		obj = create_object(get_obj_index(gq_obj->vnum), 1, FALSE);
		obj_to_room( obj, room );
	    }
	}
    }
}


/* Get a random area. 1 - first continent, 2 - second continent, 0 - either */
AREA_DATA *get_random_area( int continent, bool no_get_random )
{
    int i;
    AREA_DATA *area;

    switch (continent)
    {
	case FIRST_CONTINENT:
	case SECOND_CONTINENT:
	case BOTH_CONTINENTS:
	    break;

	default: return NULL;
    }

    /* get max #areas */
    i = 0;
    for ( area = area_first; area != NULL; area = area->next )
        i++;


    switch (continent)
    {
	case FIRST_CONTINENT:
	    do
	    {
		area = get_area_data(number_range(1, i));
	    } while (area == NULL
	    || !area->open
	    || (no_get_random && IS_SET(area->area_flags, AREA_NO_GET_RANDOM))
	    || (area->place_flags != PLACE_FIRST_CONTINENT)
	    || !str_infix( "Housing", area->name )
	    || !str_infix( "Arena", area->name)
	    || !str_infix( "Temples", area->name)
	    || !str_infix( "Maze", area->name));

	    break;

	case SECOND_CONTINENT:
	    do
	    {
		area = get_area_data( number_range( 1, i));
	    } while (area == NULL
	    || !area->open
	    || (no_get_random && IS_SET(area->area_flags, AREA_NO_GET_RANDOM))
	    || ( area->place_flags != PLACE_SECOND_CONTINENT )
	    || !str_infix("Temples", area->name)
	    || !str_infix("Maze", area->name ));

	    break;

	default:
	    do
	    {
		area = get_area_data( number_range( 1, i));
	    } while ( area == NULL
	    || !area->open
	    || (no_get_random && IS_SET(area->area_flags, AREA_NO_GET_RANDOM))
	    || !str_infix("Temples", area->name)
	    || !str_infix("Housing", area->name )
	    || ((area->place_flags != PLACE_FIRST_CONTINENT ) && (area->place_flags != PLACE_SECOND_CONTINENT))
	    || !str_infix("Arena", area->name)
	    || !str_infix("Maze", area->name));
	    break;
    }

    return area;
}

OBJ_DATA *get_random_obj_area( CHAR_DATA *ch, AREA_DATA *area, ROOM_INDEX_DATA *room)
{
	OBJ_INDEX_DATA *oIndex;
	OBJ_DATA *obj = NULL;
	int tries;

	if (area == NULL)
		return NULL;

    for (tries = 0; tries < 200; tries++)
    {
		oIndex = get_obj_index( number_range( area->min_vnum, area->max_vnum));
		if ( oIndex == NULL )
			continue;

		if ( oIndex != NULL &&
			IS_SET( oIndex->wear_flags, ITEM_TAKE ) &&
			!IS_SET( oIndex->wear_flags, ITEM_NO_SAC ) &&
			!IS_SET( oIndex->extra2_flags, ITEM_NOQUEST ) &&
			!IS_SET( oIndex->extra_flags, ITEM_MELT_DROP ) &&
			oIndex->item_type != ITEM_MONEY )
			break;
	}

    if (room != NULL) {
		obj = create_object(oIndex, oIndex->level, TRUE);
		obj_to_room(obj, room);
    } else
		obj = NULL;

    return obj;
}

/* get a random obj for a quest */
OBJ_DATA *get_random_obj( CHAR_DATA *ch, int continent )
{
	AREA_DATA *area;
	OBJ_INDEX_DATA *oIndex;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room;
	int tries;

    room = get_random_room(ch, continent);

	for (tries = 0; tries < 200; tries++)
	{
		switch (continent)
		{
		case FIRST_CONTINENT:	area = get_random_area(FIRST_CONTINENT, TRUE); 	break;
		case SECOND_CONTINENT:	area = get_random_area(SECOND_CONTINENT, TRUE);	break;
		default:				area = get_random_area(BOTH_CONTINENTS, TRUE);	break;
		}

		if ( IS_SET(area->area_flags,AREA_NO_GET_RANDOM) )
			continue;

	    return get_random_obj_area(ch, area, room);
	}

    return NULL;
}

CHAR_DATA *get_random_mob_area( CHAR_DATA *ch, AREA_DATA *area)
{
    MOB_INDEX_DATA *mIndex;
    CHAR_DATA *mob = NULL;
    ROOM_INDEX_DATA *first_room;
    long first_vnum;
    int attempts;

	if (area == NULL)
		return NULL;

    for (attempts = 0; attempts < 1000; attempts++)
    {
        /* grab a pIndexData first to increase diversity */
	mIndex = get_mob_index( number_range( area->min_vnum, area->max_vnum));
	first_vnum = area->min_vnum;
	do
	{
	    first_room = get_room_index( first_vnum++ );
	}
	while ( first_room == NULL );

	if ( mIndex == NULL )
	    continue;
	else
	{
	    if (IS_SET(mIndex->act, ACT_PROTECTED)
	    || IS_SET(mIndex->act, ACT_MOUNT)
	    || IS_SET(mIndex->act, ACT_PET)
	    || IS_SET(mIndex->act, ACT_TRAIN)
	    || IS_SET(mIndex->act, ACT_PRACTICE)
	    || IS_SET(mIndex->act, ACT_STAY_AREA)
	    || IS_SET(mIndex->act, ACT_PROTECTED)
	    || IS_SET(mIndex->act, ACT_BLACKSMITH)
	    || IS_SET(mIndex->act, ACT_CREW_SELLER)
	    || IS_SET(mIndex->act, ACT_IS_RESTRINGER)
	    || IS_SET(mIndex->act, ACT_IS_HEALER)
	    || IS_SET(mIndex->act, ACT_IS_CHANGER)
	    || IS_SET(mIndex->act, ACT_IS_BANKER)
	    || IS_SET(mIndex->act2, ACT2_NOQUEST)
	    || IS_SET(mIndex->act2, ACT2_CHURCHMASTER)
	    || IS_SET(mIndex->act2, ACT2_PLANE_TUNNELER)
	    || IS_SET(mIndex->act2, ACT2_AIRSHIP_SELLER)
	    || IS_SET(mIndex->act2, ACT2_WIZI_MOB)
	    || IS_SET(mIndex->act2, ACT2_LOREMASTER )
	    || mIndex->pShop != NULL
	    || mIndex->level > ( ch->tot_level + 20))
		continue;

   	    mob = get_char_world_index( ch, mIndex );
  	    if ( mob == NULL || mob->shop != NULL)
                continue;
	}

        if ( can_see_room(ch,mob->in_room)
	&& !room_is_private(mob->in_room, ch)
	&& !IS_SET(mob->in_room->room_flags, ROOM_PRIVATE)
	&& !IS_SET(mob->in_room->room_flags, ROOM_SOLITARY)
	&& !IS_SET(mob->in_room->room_flags, ROOM_DEATH_TRAP)
	&& !IS_SET(mob->in_room->room_flags, ROOM_SAFE)
	&& !IS_SET(mob->in_room->room_flags, ROOM_CPK) )
	    break;
    }

    return mob;

}

/* get a random mob for a quest */
CHAR_DATA *get_random_mob( CHAR_DATA *ch, int continent )
{
    AREA_DATA *area;
    //char buf[MSL];

    switch (continent)
    {
	case FIRST_CONTINENT:	area = get_random_area(FIRST_CONTINENT, TRUE); 	break;
	case SECOND_CONTINENT:	area = get_random_area(SECOND_CONTINENT, TRUE);	break;
	default:				area = get_random_area(BOTH_CONTINENTS, TRUE);	break;
    }

	return get_random_mob_area(ch, area);
}

ROOM_INDEX_DATA *get_random_room_area_byflags( CHAR_DATA *ch, AREA_DATA *area, int n_room_flags, int n_room2_flags )
{
	ROOM_INDEX_DATA *room;

	if (area == NULL)
		return NULL;

	for ( ; ; )
	{
		room = get_room_index( number_range( area->min_vnum, area->max_vnum));
		if( ch )
		{
			if( room != NULL &&
				can_see_room(ch, room) &&
				!room_is_private(room, ch) &&
				str_cmp(room->name, "NULL") &&
				!is_dislinked(room) &&
				(!n_room_flags || !IS_SET(room->room_flags, n_room_flags)) &&
				(!n_room2_flags || !IS_SET(room->room2_flags, n_room2_flags)) )
				break;
		}
		else
		{
			if( room != NULL &&
				!room_is_private(room, NULL) &&
				!is_dislinked(room) &&
				(!n_room_flags || !IS_SET(room->room_flags, n_room_flags)) &&
				(!n_room2_flags || !IS_SET(room->room2_flags, n_room2_flags)) )
				break;
		}
	}

	return room;
}


/* get a random room for a quest */
ROOM_INDEX_DATA *get_random_room( CHAR_DATA *ch, int continent )
{
    AREA_DATA *area;

    switch (continent)
    {
	case FIRST_CONTINENT:	area = get_random_area(FIRST_CONTINENT, TRUE); 	break;
	case SECOND_CONTINENT:	area = get_random_area(SECOND_CONTINENT, TRUE);	break;
	default:				area = get_random_area(BOTH_CONTINENTS, TRUE);	break;
    }

    return get_random_room_area_byflags(ch, area,
    	(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_SAFE | ROOM_CPK),
    	(ROOM_NO_QUEST));
}


/* get a random room from an area */
ROOM_INDEX_DATA *get_random_room_area( CHAR_DATA *ch, AREA_DATA *area )
{
    return get_random_room_area_byflags(ch, area,
    	(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CPK),
    	0);
}


/* Count how many letters in a string, not counting colour codes. */
int strlen_no_colours( const char *str )
{
    int count;
    int i;

    if ( str == NULL )
        return 0;

    count = 0;
    for ( i = 0; str[i] != '\0'; i++ )
    {

        if (str[i] == '`' )
        {
		i++;
                if (str[i] == '[' )
			i += 5;
                continue;
        }


	if (str[i] == '{' )
	{
	    i++;
	    continue;
	}

	count++;
    }

    return count;
}


int get_colour_width(char *text)
{
	char *plaintext = nocolour(text);
	int plen = strlen(plaintext);
	free_string(plaintext);
	int len = strlen(text);

	return len - plen;
}


/* return a string without colour codes- {x {Y etc. */
char *nocolour( const char *string )
{
    int i,n;
    char buf[MSL];

    for (i = 0, n = 0; string[i] != '\0'; i++, n++) {
	while (string[i] == '{')
	    i+= 2;
        while (string[i] == '`')
        {
                if (string[i+1] == '[')
                        i+= 7;
                else
                        i+= 2;
        }

        buf[n] = string[i];
    }

    buf[n] = '\0';

    return str_dup(buf);
}


/* convert short desc to a keyword name */
char *short_to_name( const char *short_desc )
{
    char name[MSL];
    char arg[MIL];
    char *temp_desc;
    int i;
    int n;

    name[0] = '\0';

    /* remove colours, special characters etc
    n = 0;
    for ( i = 0; short_desc[i] != '\0'; i++ )
    {
        while ( short_desc[i] == '{' )
	{
	    i += 2;
	}


	if ( short_desc[i] == '!'
	||   short_desc[i] == '@'
	||   short_desc[i] == '#'
	||   short_desc[i] == '$'
	||   short_desc[i] == '%'
	||   short_desc[i] == '^'
	||   short_desc[i] == '&'
	||   short_desc[i] == '*'
	||   short_desc[i] == '('
	||   short_desc[i] == ')'
	||   short_desc[i] == '['
	||   short_desc[i] == ']'
	||   short_desc[i] == '<'
	||   short_desc[i] == '>' )
	    i++;

	while ( short_desc[i] == '{' )
	{
	    i += 2;
	}


	temp_desc[n] = short_desc[i];

	n++;
    }


    temp_desc[n] = '\0';
*/
    temp_desc = nocolour(short_desc);
    i = 0;
    while( temp_desc[i] != '\0' )
    {
		temp_desc[i] = LOWER(temp_desc[i]);
		i++;
    }

    i = 0;
    do
    {
		n = 0;
		while ( isspace( temp_desc[i]))
		{
		    i++;
		}

		while ( temp_desc[i] != ' ' && temp_desc[i] != '\0')
		{
		    arg[n] = temp_desc[i++];
		    n++;
		}

		arg[n] = '\0';

		if (strlen(arg) > 2 &&
			str_cmp(arg, "the") &&
			str_cmp(arg, "and") &&
			str_cmp(arg, "some") &&
			str_cmp(arg, "with"))
		{
		    if ( name[0] == '\0' )
		    {
		        strcat( name, arg );
		    }
		    else
		    {
				strcat( name, " " );
				strcat( name, arg );
		    }
		}
    }
    while ( temp_desc[i] != '\0' );

    free_string(temp_desc);	// temp_desc wasn't being free'd... oops

    return str_dup(name);
}


/* fix a short descr so if it starts with "An", "The", etc it gets uncapped */
void fix_short_description( char *short_descr )
{
    char *temp;
    char buf[MSL];
    char first_word[MSL];

    temp = str_dup( short_descr );
    one_argument( temp, first_word );
    free_string( temp );

    if ( first_word[0] == '\0' )
	return;

    if ((!str_cmp(first_word, "the") && first_word[0] == 'T' )
    || ((!str_cmp(first_word, "a")
    || !str_cmp(first_word, "an")) && first_word[0] == 'A'))
    {
	sprintf( buf, "fix_short_description: fixing '%s'", short_descr );
	log_string( buf );
        short_descr[0] = LOWER(short_descr[0] );
    }
}


void load_npc_ships()
{
    FILE *fp;
    WAYPOINT_DATA *waypoint;
    MOB_INDEX_DATA *pMob;
    NPC_SHIP_INDEX_DATA *npc_ship = NULL;
    char buf[256];
    char letter;

    log_string("db2.c, Loading npc ships...");
    if ( ( fp = fopen( NPC_SHIPS_FILE, "r" ) ) == NULL )
    {
        exit( 1 );
    }

    letter                          = fread_letter( fp );

    if ( letter != '#' )
    {
        bug( "Load_Npc_Ships: # not found.", 0 );
        exit( 1 );
    }

    for ( ; ; )
    {
        long vnum;
        long captain_vnum;
        char letter2;
        int iHash;

        vnum                            = fread_number( fp );
        if ( vnum == 0 )
            break;

        if ( get_npc_ship_index( vnum ) != NULL )
        {
            bug( "Load_Npc_Ships: vnum %ld duplicated.", vnum );
            exit( 1 );
        }

	npc_ship        = new_npc_ship_index();
	npc_ship->vnum  = vnum;
	captain_vnum 	= fread_number( fp );
	if (captain_vnum != 0)
	{
            npc_ship->captain		= get_mob_index( captain_vnum );
	}
	else
	{
	    npc_ship->captain		= NULL;
	}

	npc_ship->name              = fread_string( fp );
	npc_ship->flag              = fread_string( fp );
	npc_ship->gold              = fread_number( fp );

	npc_ship->ship_type         = fread_number( fp );
	npc_ship->npc_type          = fread_number( fp );
    npc_ship->npc_sub_type      = fread_number( fp );
	npc_ship->original_x        = fread_number( fp );
	npc_ship->original_y        = fread_number( fp );
	npc_ship->area	      	    = fread_string( fp );
	npc_ship->ships_destroyed   = fread_number( fp );
	npc_ship->plunder_captured  = fread_number( fp );
	npc_ship->current_name      = fread_string( fp );
	//npc_ship->chance_repop      = fread_number( fp );
	fread_letter(fp);
	npc_ship->initial_ships_destroyed      = fread_number( fp );

	if (npc_ship->ships_destroyed == 0) {
	    npc_ship->ships_destroyed = npc_ship->initial_ships_destroyed;
	}

	for ( ; ; )
	{
   	    letter2 = fread_letter( fp );

	    if ( letter2 == '#' )
	    {
	        break;
            }

	    // Load Waypoint
	    if ( letter2 == 'W' )
	    {
	        waypoint = new_waypoint();
		waypoint->x = fread_number( fp );
		waypoint->y = fread_number( fp );
		waypoint->hour = fread_number( fp );
		waypoint->day = fread_number( fp );
		waypoint->month = fread_number( fp );

		// Add waypoint
		if ( npc_ship->waypoint_list == NULL )
		{
			npc_ship->waypoint_list = waypoint;
		}
		else
		{
		    WAYPOINT_DATA *temp;
		    temp = npc_ship->waypoint_list;
		    while(temp->next != NULL)
		    {
		        temp = temp->next;
		    }
		    temp->next = waypoint;
		}
		continue;
	    }

		// Load Cargo
	    if ( letter2 == 'G' )
	    {
		    long obj_vnum;
			OBJ_DATA *pObj;
			OBJ_INDEX_DATA *pObjIndex;
		    obj_vnum = fread_number( fp );
		    log_string( "loading cargo" );

		    sprintf(buf, "Letter was %c       obj vnum was %ld\n", letter, obj_vnum);
		    log_string(buf);
		    if ( (pObjIndex = get_obj_index( obj_vnum) ) == NULL)
		    {
			    bug("Couldn't load cargo object for npc_ship because cargo object does not exist.", obj_vnum);
			    continue;
		    }

			// Add cargo object to npc ship index.
			pObj = create_object( pObjIndex, pObjIndex->level, FALSE );
			pObj->next_content = npc_ship->cargo;
			npc_ship->cargo = pObj;
			continue;
		}

	    // Load Crew
	    log_string( "about to load crew" );
	    sprintf( buf, "%c", letter2 );
	    log_string( buf );

	    if ( letter2 == 'C' )
	    {
		    long mob_vnum;
		    SHIP_CREW_DATA *crew;
		    mob_vnum = fread_number( fp );
		    log_string( "loading crew" );

		    sprintf(buf, "Letter was %c         num was %ld\n", letter, mob_vnum);
		    log_string(buf);
		    if ( (pMob = get_mob_index( mob_vnum )) == NULL)
		    {
			    bug("Couldn't load crew member for npc_ship because mob does not exist.", 0);
			    continue;
		    }

		    crew = new_ship_crew();
		    crew->vnum = pMob->vnum;

		    // Add mob
		    if ( !npc_ship->crew )
		    {
			    npc_ship->crew = crew;
		    }
		    else
		    {
			    crew->next = npc_ship->crew;
			    npc_ship->crew = crew;
		    }
		    continue;
	    }
	}

	if ( vnum > top_vnum_npc_ship )
		top_vnum_npc_ship = vnum;

	iHash                       = vnum % MAX_KEY_HASH;
	npc_ship->next              = ship_index_hash[iHash];
	ship_index_hash[iHash]      = npc_ship;
    }
    fclose( fp );
    return;
}


void do_dump( CHAR_DATA *ch, char *argument )
{
    int i;
    int n;
    FILE *fp;

    if ( !str_cmp(argument, "skills"))
    {
	if ( ( fp = fopen( SKILLS_DB_FILE, "w")) == NULL )
	{
	    bug("do_dump: fopen", 0 );
	    return;
	}

	i = 0;
	while ( group_table[i].name != NULL )
	{
	    fprintf( fp, "[%s]:\n", group_table[i].name );
	    n = 0;
	    while ( group_table[i].spells[n] != NULL )
	    {
		fprintf( fp, "%i) %s\n", n+1, group_table[i].spells[n] );
		n++;
	    }

	    fprintf( fp, "\n");

	    i++;
	}

	send_to_char("Skills dumped.\n\r", ch );

	fclose( fp );
	return;
    }

    if ( !str_cmp( argument, "objects"))
    {
 	long vnum = 0;
	AFFECT_DATA *af;
	OBJ_INDEX_DATA *obj;

    	if ( ( fp = fopen( OBJ_DB_FILE, "w")) == NULL ) {
	    bug("do_dump: fopen", 0 );
	    return;
	}
	setbuf(fp,NULL);

	fprintf(fp, "Vnum	ShortDesc	Name	Level	Area	Type	WearFlags	"
	            "Update	Fragility	Weight(kg)	Condition (%%)	Timer	Cost	Material	"
		    "AC Pierce	AC Bash	AC Slash	AC Exotic	ArmourStrength	"
		    "WeaponClass	DiceNumber	DiceType	DamageType	WeaponAttributes	"
		    "SpellLevel	Spell1	Spell2	"
		    "Key	Capacity	WeightMultiplier	ContainerFlags	"
		    "ExtraFlags	Extra2Flags	Update	Timer	"
		    "Mod1	Mod2	Mod3	Mod4	Mod5	Mod6	Mod7	Mod8	Mod9	Mod10"
		    "\n");

	for ( vnum = 0; vnum < MAX_KEY_HASH; vnum++) {
		for (obj = obj_index_hash[vnum % MAX_KEY_HASH]; obj != NULL; obj = obj->next) {
			int numAffects = 0;

			if (obj->level > 0 && obj->level <= 120) {
				fprintf( fp, "%ld	%s	%s	%d	%s	%s	%s	",
					obj->vnum,
					obj->short_descr,
					obj->name,
					obj->level,
					obj->area->name,
					item_name(obj->item_type),
					flag_string(wear_flags, obj->wear_flags));

				fprintf( fp, "%s	%s	%d	%d	%d	%ld	%s	",
					obj->update == TRUE ? "Yes" : "No",
					fragile_table[obj->fragility].name,
					obj->weight,
					obj->condition,
					obj->timer,
					obj->cost,
					obj->material);

				//Armour attributes
				if ( obj->item_type == ITEM_ARMOUR ) {
					fprintf( fp, "%ld	%ld	%ld	%ld	%s	",
						obj->value[0], obj->value[1], obj->value[2], obj->value[3],
						armour_strength_table[obj->value[4]].name );
				} else {
					fprintf( fp, "N/A	N/A	N/A	N/A	N/A	");
				}

				//Weapon attributes
				if (obj->item_type == ITEM_WEAPON) {
					fprintf( fp, "%s	%ld	%ld	%s	%s	", get_weapon_class(obj), obj->value[1], obj->value[2],
					attack_table[obj->value[3]].noun, flag_string( weapon_type2,  obj->value[4]));
				} else if (obj->item_type == ITEM_RANGED_WEAPON) {
					fprintf( fp, "%s	%ld	%ld	%s	%s	", flag_string( ranged_weapon_class, obj->value[0]),
						obj->value[1], obj->value[2], "N/A",	"N/A");
				} else {
					fprintf( fp, "N/A	-1	-1	N/A	N/A	");
				}

				//Spells
				if (obj->item_type == ITEM_WEAPON || obj->item_type == ITEM_ARMOUR || obj->item_type == ITEM_ARTIFACT
					|| obj->item_type == ITEM_LIGHT) {
					switch(obj->item_type) {
					case ITEM_WEAPON:
					case ITEM_ARMOUR:
						fprintf(fp, "%ld	%s	%s	", obj->value[5], obj->value[6] > 0 ? skill_table[obj->value[6]].name :
							"none", obj->value[7] > 0 ? skill_table[obj->value[7]].name : "none");
						break;
					case ITEM_ARTIFACT:
						fprintf(fp, "%ld	%s	%s	", obj->value[0], obj->value[1] > 0 ? skill_table[obj->value[1]].name :
							"none", obj->value[2] > 0 ? skill_table[obj->value[2]].name : "none");
						break;

					case ITEM_LIGHT:
						fprintf(fp, "%ld	%s	%s	", obj->value[3], obj->value[4] > 0 ? skill_table[obj->value[4]].name :
							"none", obj->value[5] > 0 ? skill_table[obj->value[5]].name : "none");
						break;
					}
				} else {
					fprintf(fp, "-1	N/A	N/A	");
				}

				//Container attributes
				if (obj->item_type == ITEM_CONTAINER) {
					OBJ_INDEX_DATA *key = get_obj_index(obj->value[2]);
					fprintf(fp, "%s[%ld]	%ld	%ld	%s	", key == NULL ? "None" : key->short_descr,
						key == NULL ? 0 : key->vnum, obj->value[3],
					obj->value[4], flag_string( container_flags, obj->value[1] ));
				} else {
					fprintf(fp, "N/A	N/A	N/A	N/A ");
				}

				fprintf( fp, "%s	", extra_bit_name(obj->extra_flags));
				fprintf( fp, "%s	", extra2_bit_name(obj->extra2_flags));
				fprintf( fp, "%s	", extra3_bit_name(obj->extra3_flags));
				fprintf( fp, "%s	", extra4_bit_name(obj->extra4_flags));
				fprintf( fp, "%s	%d	", obj->update ? "Yes" : "No", obj->timer);

				for ( af = obj->affected; af != NULL; af = af->next ) {
					numAffects++;
					fprintf( fp, "%s by %d [%d%%]	",
					flag_string( apply_flags, af->location ),
					af->modifier,
					af->random );
				}

				for (; numAffects != 10; numAffects++) {
					fprintf(fp, "None");
					if (numAffects != 9) fprintf(fp, "	");
				}

				fprintf(fp, "\n");
			}
		}
	}

	fclose(fp);
	return;
    }

    if (!str_cmp(argument, "help"))
    {

	if ((fp = fopen(HELP_DB_FILE, "w")) == NULL)
	{
	    bug("do_dump: fopen", 0);
	    return;
	}

	fclose(fp);

	write_help_to_disk(topHelpCat, NULL);
	send_to_char("Help files dumped.\n\r", ch);
	return;
    }

    send_to_char("Syntax: dump <skills/objects/help>\n\r", ch );
}


void write_help_to_disk(HELP_CATEGORY *hcat, HELP_DATA *help)
{
    FILE *fp;
    char filename[255];
    char buf[MSL];
    HELP_CATEGORY *hcatnest;
    HELP_DATA *helpnest;

    if (!hcat && !help)
    {
	bug("write_help_to_disk: hcat and help null, nothing to write", 0);
	return;
    }

    if (hcat != NULL)
    {
	for (helpnest = hcat->inside_helps; helpnest != NULL; helpnest = helpnest->next)
	    write_help_to_disk(NULL, helpnest);

	for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next)
	    write_help_to_disk(hcatnest, NULL);
    }

    if (help != NULL)
    {
	sprintf(filename, HELP_DIR);
        sprintf(buf, "%s", help->hCat->name);
	strcat(filename, buf);
	sprintf(buf, "%s", help->keyword);
	strcat(filename, buf);

	if (strlen(filename) > 50)
	    filename[50] = '\0';

	strcat(filename, ".txt");

	if ((fp = fopen(filename, "w")) != NULL)
	{
	    fprintf(fp, "%s", fix_string(help->text));
	    fclose(fp);
	}
	else
	{
	    bug("write_help_to_disk: fp", 0);
	    return;
	}
    }
}


void load_area_trade( AREA_DATA *pArea, FILE *fp )
{
    for ( ; ; )
    {
        int trade_type;
        char letter;
	long min_price;
	long max_price;
	long max_qty;
	long replenish_amount;
	long replenish_time;
	long obj_vnum;

        letter                          = fread_letter( fp );
        if ( letter != '#' )
        {
            bug( "Load_Area_Trade: # not found.", 0 );
            exit( 1 );
        }

        trade_type                            = fread_number( fp );
        if ( trade_type == 0 )
            break;

	min_price 		= fread_number( fp );
	max_price 		= fread_number( fp );
	max_qty 		= fread_number( fp );
	replenish_amount	= fread_number( fp );
	replenish_time 		= fread_number( fp );
	obj_vnum 		= fread_number( fp );
	new_trade_item( pArea,
			trade_type,
			replenish_time,
			replenish_amount,
			max_qty,
			min_price,
			max_price,
			obj_vnum );
    }
    return;
}


/* Load report information */
void load_statistics()
{
    log_string("stats.c, Loading Statistics...");

    // Load Top10PKers.info
    load_stat( "Top10PKers.info", REPORT_TOP_PLAYER_KILLERS );

    // Load Top10CPKers.info
    load_stat( "Top10CPKers.info", REPORT_TOP_CPLAYER_KILLERS );

    // Load Top10monsters.info
    load_stat( "Top10Monsters.info", REPORT_TOP_MONSTER_KILLERS );

    // Load Top10wealthiest.info
    load_stat( "Top10Wealthiest.info", REPORT_TOP_WEALTHIEST );

    // Load Top10ratio.info
    load_stat( "Top10Ratio.info", REPORT_TOP_WORST_RATIO );

    // Load Top10quests.info
    load_stat( "Top10Quests.info", REPORT_TOP_QUESTS );
}


void load_stat( char *filename, int type )
{
    FILE *fp;
    char buf[MAX_STRING_LENGTH];
    int i;

    // Attach directory
    sprintf( buf, STATS_DIR"%s", filename );

    if ( ( fp = fopen( buf, "r")) == NULL )
    {
	sprintf( buf, "Couldn't load file %s.", filename );
	bug( buf, 0 );
	return;
    }

    stat_table[type].report_name = fread_string( fp );
    stat_table[type].description = format_string( fread_string( fp));
    stat_table[type].columns = fread_number(fp);
    stat_table[type].column[0] = fread_string( fp );
    stat_table[type].column[1] = fread_string( fp );

    for ( i = 0; i < 10; i++ )
    {
	stat_table[type].name[i] = fread_string( fp );
	stat_table[type].value[i] = fread_string( fp );
    }

    fclose( fp );
}


// Generate the resets in the Pyramid of the Abyss. This is here
// so we can edit the areas dynamically just like real areas and the
// resets will be set up.
void generate_poa_resets( int level )
{
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;
    RESET_DATA *reset;
    int i;
    long vnum;
    char buf[MSL];

    if ( level == -1 )
    {
	for ( i = 1; i <= MAX_POA_LEVELS; i++ )
	{
	    generate_poa_resets( i );
	}

	return;
    }

    sprintf( buf, "Maze-Level%d", level );

    if ( ( area = find_area( buf)) == NULL )
    {
	bug("generate_poa_resets: couldn't find area for level %d.", level );
	return;
    }

    vnum = area->min_vnum;
    while ( vnum <= area->max_vnum )
    {
	int num_resets;
	int count = 0;

	if ( ( room = get_room_index( vnum)) == NULL )
	    continue;

	// Decide on # resets per room. Not hugely necesarry now but may be
	// more when we add more mobs/levels.
	switch ( level )
	{
	    case 1:  num_resets = 2; break;
	    case 2:  num_resets = 2; break;
	    case 3:  num_resets = 2; break;
	    case 4:  num_resets = 2; break;
	    case 5:  num_resets = 2; break;
	    default: num_resets = 2; break;
	}

	while ( count++ < num_resets )
	{
	    MOB_INDEX_DATA *mob;

	    do
		mob = get_random_mob_index( area );
	    while ( mob == NULL );

	    reset = new_reset_data();
	    reset->command = 'M';
	    reset->arg1    = mob->vnum; // Mob vnum
	    if ( IS_SET( mob->act2, ACT2_RESET_ONCE )
            || mob->vnum == area->max_vnum )
		reset->arg2 = 1;
	    else
	    switch ( level )
	    {
		case 1: reset->arg2 = 15; break;
		case 2: reset->arg2 = 25; break;
		case 3: reset->arg2 = 35; break;
		case 4: reset->arg2 = 45; break;
		case 5: reset->arg2 = 55; break;
	    }
	    reset->arg3    = room->vnum;
	    reset->arg4    = 1;
	    add_reset( room, reset, 0 );
	}
	vnum++;
    }
}


// Get random mob_index from an area. Usually for POA and the like.
MOB_INDEX_DATA *get_random_mob_index( AREA_DATA *area )
{
    MOB_INDEX_DATA *mob = NULL;
    int attempts = 200;
    int i = 0;

    do
    {
	mob = get_mob_index( number_range( area->min_vnum, area->max_vnum));
    }
    while ( mob == NULL && i++ < attempts );

    return mob;
}

