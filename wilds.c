/* Author : Vizzini (sc0jack@yahoo.co.uk)
 * Wilds v2.x
 *
 * History:
 *
 * v2.x - Load/save wilds. Encapsulated wilds memory and area data structures.
 * v1.x - Load only wilds. The "just get something working" version.
 * v0.x - Initial alphas. Unstable and buggy. Testing concepts.
 */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "wilds.h"
#include "olc_save.h"
#include "tables.h"

/* external global variables */
extern bool fBootDb;
extern GLOBAL_DATA gconfig;

/* internal global variables */
LLIST *loaded_wilds = NULL;
long top_wilds;
long top_wilds_terrain;
long top_wilds_vlink;

/* These are pointers to linked lists of "freed" (but still allocated) structures.
   They are used in the routines as "heaps" of perms. In game, it is often more
   efficient performance-wise to not actually "free" memory allocated, but to store
   it on a heap of allocated structures to save time on reallocating later. This is
   known as "recycling" of data structures */
WILDS_DATA *wilds_free;
WILDS_VLINK *wilds_vlink_free;
WILDS_TERRAIN *wilds_terrain_free;
WILDS_REGION *wilds_region_free;

/* external routines */
extern EXIT_DATA *new_exit args ((void));
extern void free_exit args ((EXIT_DATA *pExit));
extern char *fix_string args ((const char *str));
extern char *fwrite_flag args ((long flags, char buf[]));
extern ROOM_INDEX_DATA *new_room_index args ((void));
extern void free_room_index args ((ROOM_INDEX_DATA * pRoomIndex));
extern void fwrite_room args ((FILE *fp, ROOM_INDEX_DATA *pRoomIndex));
extern ROOM_INDEX_DATA *read_room_new(FILE *fp, AREA_DATA *area, int roomtype);

/* local routines */
int		get_squares_to_show_x args ((int bonus_view));
int		get_squares_to_show_y args ((int bonus_view));
bool		map_char_cmp args ((WILDS_DATA *pWilds, int x, int y, char *check));
bool		check_for_bad_room args ((WILDS_DATA *pWilds, int x, int y));
ROOM_INDEX_DATA *create_vroom args ((WILDS_DATA *pWilds,
                                     int x, int y,
                                     WILDS_TERRAIN *pTerrain));
void link_vroom(ROOM_INDEX_DATA *pWildsRoom);
void            load_wilds args ((FILE *fp, AREA_DATA *area));
WILDS_DATA      *new_wilds args ((void));
void            free_wilds args ((WILDS_DATA *pWilds));
WILDS_DATA      *get_wilds_from_uid args ((AREA_DATA *pArea, long uid));
void            char_to_vroom args ((CHAR_DATA *ch, WILDS_DATA *pWilds, int x, int y));
ROOM_INDEX_DATA *get_wilds_vroom args ((WILDS_DATA *pWilds, int x, int y));
int             get_wilds_vroom_x_by_dir args ((WILDS_DATA *pWilds, int x, int y, int door));
int             get_wilds_vroom_y_by_dir args ((WILDS_DATA *pWilds, int x, int y, int door));
void            do_vlinks args ((CHAR_DATA *ch, char *argument));
void            do_regions args ((CHAR_DATA *ch, char *argument));
WILDS_VLINK     *NEW_Vlink args ((void));
WILDS_VLINK     *fread_vlink args ((FILE *fp, AREA_DATA *area));
WILDS_VLINK     *get_vlink_from_uid args((WILDS_DATA *pWilds, long uid));
WILDS_VLINK     *get_vlink_from_index args((WILDS_DATA *pWilds, long index));
void            free_vlink args ((WILDS_VLINK *pVLink));
void            add_vlink args ((WILDS_DATA *pWilds, WILDS_VLINK *pVLink));
void            del_vlink args ((WILDS_DATA *pWilds, WILDS_VLINK *pVLink));
bool            link_vlink args ((WILDS_VLINK *pVLink));
void		fix_vlinks (void);
void            link_vlinks args ((WILDS_DATA *pWilds));
WILDS_TERRAIN   *new_terrain args ((WILDS_DATA *pWilds));
WILDS_TERRAIN   *fread_terrain args ((FILE *fp, WILDS_DATA *pWilds));
void            fwrite_terrain args ((FILE *fp, WILDS_TERRAIN *pTerrain));
void            free_terrain args ((WILDS_TERRAIN *pTerrain));
bool		add_terrain args ((WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain));
bool		del_terrain args ((WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain));
bool		check_terrain_exists args ((WILDS_DATA *pWilds, char token));
WILDS_REGION    *new_region args ((WILDS_DATA *pWilds));
WILDS_REGION    *fread_region args ((FILE *fp, WILDS_DATA *pWilds));
void            fwrite_region args ((FILE *fp, WILDS_REGION *pRegion));
void            free_region args ((WILDS_REGION *pRegion));
bool		add_region args ((WILDS_DATA *pWilds, WILDS_REGION *pRegion));
bool		del_region args ((WILDS_DATA *pWilds, WILDS_REGION *pRegion));


int dir_offsets[MAX_DIR][2] = {
	{0, -1},
	{1, 0},
	{0, 1},
	{-1, 0},
	{0, 0},
	{0, 0},
	{1, -1},
	{-1, -1},
	{1, 1},
	{-1, 1},
};

char *vlinkage_bit_name(int vlinkage)
{
    static char buf[512];

    buf[0] = '\0';

    if ((vlinkage & VLINK_TO_WILDS) && (vlinkage & VLINK_FROM_WILDS))
        strcat(buf, " two_way");
    else
        if (vlinkage & VLINK_TO_WILDS    ) strcat(buf, " to_wilds");
        else
            if (vlinkage & VLINK_FROM_WILDS  ) strcat(buf, " from_wilds");

    return ( buf[0] != '\0' ) ? buf+1 : "none";
}


void fix_vlinks(void)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;

    // Loop thru all loaded areas
    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        // Does the area contain wilds sectors?
        if (pArea->wilds != NULL)
        {
            // Loop thru all wilds sectors
            for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
            {
                // Call mid-level function to link all vlinks in the wilds
                link_vlinks(pWilds);
            }
        }
    }

    return;
}

int get_squares_to_show_x(int bonus_view)
{
    int squares_to_show_x;

    squares_to_show_x = 10;

    switch (weather_info.sky)
    {
        case SKY_CLOUDLESS:
            squares_to_show_x = 16 + bonus_view;
            break;
        case SKY_CLOUDY:
            squares_to_show_x = 14 + bonus_view;
            break;
        case SKY_RAINING:
            squares_to_show_x = 12 + bonus_view;
            break;
        case SKY_LIGHTNING:
            squares_to_show_x = 10 + bonus_view;
            break;
    }

    if (weather_info.sky == SUN_SET)
        squares_to_show_x -= 1;

    if (weather_info.sky == SUN_DARK)
        squares_to_show_x -= 3;

    return squares_to_show_x;
}

int get_squares_to_show_y(int bonus_view)
{
    int squares_to_show_y;

    squares_to_show_y = 10;

    switch (weather_info.sky)
    {
        case SKY_CLOUDLESS:
            squares_to_show_y = 10 + bonus_view;
            break;
        case SKY_CLOUDY:
            squares_to_show_y = 10 + bonus_view;
            break;
        case SKY_RAINING:
            squares_to_show_y = 8 + bonus_view;
            break;
        case SKY_LIGHTNING:
            squares_to_show_y = 8 + bonus_view;
            break;
    }

    if (weather_info.sky == SUN_SET)
        squares_to_show_y -= 1;

    if (weather_info.sky == SUN_DARK)
        squares_to_show_y -= 3;

    return squares_to_show_y;
}

bool map_char_cmp(WILDS_DATA *pWilds, int x, int y, char *check)
{
    char map_char[2];

    // Get character on the map at these coordinates
    if ( x > pWilds->map_size_x )
        sprintf(map_char, "%c",
                pWilds->map[y * pWilds->map_size_x + x - pWilds->map_size_x]);
    else
        sprintf(map_char, "%c", pWilds->map[y * pWilds->map_size_x + x]);

    if (!str_cmp(map_char, check))
        return true;
    else
        return false;

}

bool check_for_bad_room(WILDS_DATA *pWilds, int x, int y)
{
    WILDS_TERRAIN *pTerrain;

    pTerrain = get_terrain_by_coors(pWilds, x, y);

    if (!pTerrain || pTerrain->nonroom)
        return false;
    else
        return true;

}

ROOM_INDEX_DATA *create_vroom(WILDS_DATA *pWilds,
                              int x, int y,
                              WILDS_TERRAIN *pTerrain)
{
    ROOM_INDEX_DATA *pRoomIndex;
    WILDS_VLINK *pVLink;
    int16_t door;


// First check pointer parameters are valid
    if ( !pTerrain )
    {
        plogf("wilds.c, create_vroom(): pTerrain is NULL");
        abort();
    }

    if ( !pTerrain->template )
    {
        plogf("wilds.c, create_vroom(): pTerrain->template is NULL");
        abort();
    }

// Create a new room structure and load it up with sensible defaults
    pRoomIndex                  = new_room_index();
    pRoomIndex->area            = pWilds->pArea;
    pRoomIndex->wilds		= pWilds;
    pRoomIndex->vnum            = 0; /* Vizz - Wilds v2 doesn't use vnums */
    pRoomIndex->x               = x;
    pRoomIndex->y               = y;
    pRoomIndex->z		= 0;	// Fix to later to deal with heights
    pRoomIndex->name            = str_dup(pTerrain->template->name);
    pRoomIndex->description     = NULL;
    pRoomIndex->owner           = NULL;
    pRoomIndex->room_flag[0]      = pTerrain->template->room_flag[0];
    pRoomIndex->room_flag[1]	= pTerrain->template->room_flag[1]|ROOM_VIRTUAL_ROOM;
    pRoomIndex->sector     = pTerrain->template->sector;
    pRoomIndex->sector_flags = pRoomIndex->sector->flags;
    pRoomIndex->parent_template = pTerrain;
    pRoomIndex->heal_rate       = 100;
    pRoomIndex->mana_rate       = 100;

    list_appendlink(pWilds->loaded_vrooms,pRoomIndex);
    pWilds->loaded_rooms++;

    top_wilds_vroom++;

    for ( door = 0; door < MAX_DIR; door++ )
        pRoomIndex->exit[door] = NULL;

// Check for vlinks in this room before we create the exits
    for (pVLink = pWilds->pVLink; pVLink ; pVLink = pVLink->next)
    {
        if (pRoomIndex->x == pVLink->wildsorigin_x
            && pRoomIndex->y == pVLink->wildsorigin_y
	    && IS_SET(pVLink->default_linkage, VLINK_FROM_WILDS))
        {
            link_vlink(pVLink);
        }
    }

	// Nib - replaced the ugly repetitive code with this SAL (Simple Ass Loop) (tm)...
	link_vroom(pRoomIndex);

//	sprintf(buf, "create_vroom: %ld %ld %ld", pWilds->uid, pRoomIndex->x, pRoomIndex->y);
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

    return (pRoomIndex);
}

WILDS_VLINK *vroom_get_to_vlink(WILDS_DATA *pWilds, int x, int y, int door)
{
	WILDS_VLINK *pVLink;

    if (!pWilds)
    {
        plogf("wilds.c, vroom_get_to_vlink(): pRoomIndex->wilds is NULL.");
        return NULL;
    }

    for (pVLink = pWilds->pVLink; pVLink ; pVLink = pVLink->next)
    {
        if (pVLink->wildsorigin_x == x &&
        	pVLink->wildsorigin_y == y &&
        	pVLink->door == door &&
        	IS_SET(pVLink->default_linkage, VLINK_FROM_WILDS))
        	return pVLink;
    }

    return NULL;

}

bool vroom_has_from_vlinks(ROOM_INDEX_DATA *pRoomIndex)
{
    WILDS_DATA *pWilds;
	WILDS_VLINK *pVLink;

    pWilds = pRoomIndex->wilds;

    if (!pWilds)
    {
        plogf("wilds.c, vroom_has_from_vlinks(): pRoomIndex->wilds is NULL.");
        return false;
    }

    for (pVLink = pWilds->pVLink; pVLink ; pVLink = pVLink->next)
    {
        if (pRoomIndex->x == pVLink->wildsorigin_x && pRoomIndex->y == pVLink->wildsorigin_y &&
        	IS_SET(pVLink->current_linkage, VLINK_FROM_WILDS))
        {
            return true;
        }
    }



	return false;
}

void destroy_wilds_vroom(ROOM_INDEX_DATA *pRoomIndex)
{
    WILDS_DATA *pWilds;
    ROOM_INDEX_DATA *clone, *next_clone;

// Check pointer parameter is valid
    if (!pRoomIndex)
    {
        plogf("wilds.c, destroy_wilds_vroom(): pRoomIndex is NULL.");
        return;
    }

    // Already GC'd
    if (list_hasdata(gc_rooms, pRoomIndex))
    {
        return;
    }

	// A persistant or non-wilderness room
	if( pRoomIndex->persist || !IS_SET(pRoomIndex->room_flag[1], ROOM_VIRTUAL_ROOM)) {
//		sprintf(buf, "destroy_wilds_vroom: %ld %ld - persist %s, virtual room %s", pRoomIndex->x, pRoomIndex->y, (pRoomIndex->persist ? "YES" : "NO"), (IS_SET(pRoomIndex->room_flag[1], ROOM_VIRTUAL_ROOM)?"YES":"NO"));
//		wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
		return;
	}

    pWilds = pRoomIndex->wilds;

    if (!pWilds)
    {
//		sprintf(buf, "destroy_wilds_vroom: ??? %ld %ld - no wilds?", pRoomIndex->x, pRoomIndex->y);
//		wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
        plogf("wilds.c, destroy_wilds_vroom(): pRoomIndex->wilds is NULL.");
        return;
    }

	// A wilds room that has a vlink from this room will stay
//	if( vroom_has_from_vlinks(pRoomIndex) ) {
//		sprintf(buf, "destroy_wilds_vroom: %ld %ld %ld - has from vlinks", pWilds->uid, pRoomIndex->x, pRoomIndex->y);
//		wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);
//		return;
//	}

    if( pRoomIndex->progs ) {
		if( pRoomIndex->progs->script_ref > 0 ) {
			pRoomIndex->progs->extract_when_done = true;
			return;
		}
	}

    for(clone = pRoomIndex->clone_rooms; clone; clone = next_clone) {
	    next_clone = clone->next_clone;
	    p_percent_trigger(NULL, NULL, clone, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_CLONE_EXTRACT, NULL,0,0,0,0,0);
	    room_from_environment(clone);
    }

	list_remlink(pWilds->loaded_vrooms,pRoomIndex, false);
	pWilds->loaded_rooms--;

//	sprintf(buf, "destroy_wilds_vroom: %ld %ld %ld - destroying room", pWilds->uid, pRoomIndex->x, pRoomIndex->y);
//	wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

	//free_room_index(pRoomIndex);  Moved to garbage collection
    list_appendlink(gc_rooms, pRoomIndex);

    return;
}

char *allocate_wildsmap(int map_size_x, int map_size_y)
{
    char *tempmap;

// simply allocates enough space in memory to hold the specified wilds map array
    tempmap = calloc(sizeof(char), (map_size_x * map_size_y + 1));
    return (tempmap);
}

CHAR_DATA *allocate_char_matrix(int map_size_x, int map_size_y)
{
    CHAR_DATA *tempmap;

// simply allocates enough space in memory to hold the specified wilds map array
    tempmap = calloc(sizeof(CHAR_DATA *), (map_size_x * map_size_y ));
    return (tempmap);
}

OBJ_DATA *allocate_obj_matrix(int map_size_x, int map_size_y)
{
    OBJ_DATA *tempmap;

// simply allocates enough space in memory to hold the specified wilds map array
    tempmap = calloc(sizeof(OBJ_DATA *), (map_size_x * map_size_y ));
    return (tempmap);
}

void load_wilds( FILE *fp, AREA_DATA *pArea )
{
	LLIST_WILDS_DATA *data;
    WILDS_DATA *pWilds, *pLastWilds;
    WILDS_VLINK *temp_pVLink;
    WILDS_TERRAIN *pTerrain;
    WILDS_REGION *pRegion;
    long arraysize = 0;
    char      *word;
    int       y,j;

    pWilds = new_wilds();
    pWilds->pArea = pArea;

    for ( ; ; )
    {
        word   = feof( fp ) ? "End" : fread_word( fp );

        switch ( UPPER(word[0]) )
        {
            case '#':
                if ( !str_cmp( word, "#VMAP" ) )
                {
                    pWilds->map_size_x = fread_number(fp);
                    pWilds->map_size_y = fread_number(fp);
                    /* Vizz - answer in kilobytes */
                    arraysize = pWilds->map_size_x * pWilds->map_size_y / 1024;

                    if (arraysize < 1024) /* if less than 1 meg, output in Kb */
                        plogf("wilds.c, load_wilds(): Allocating %d x %d (%ld vrooms) = %ld Kb.",
                              pWilds->map_size_x, pWilds->map_size_y,
                              (pWilds->map_size_x * pWilds->map_size_y),
                              arraysize);
                    else /* Vizz - output in Mb */
                        plogf("wilds.c, load_wilds(): Allocating %d x %d (%ld vrooms) = %ld Mb.",
                              pWilds->map_size_x, pWilds->map_size_y,
                              (pWilds->map_size_x * pWilds->map_size_y),
                              arraysize / 1024);

                    pWilds->staticmap = allocate_wildsmap(pWilds->map_size_x, pWilds->map_size_y);
                    pWilds->map = allocate_wildsmap(pWilds->map_size_x, pWilds->map_size_y);

		// Instead of using the string space when this is going to be stored elsewhere... use freads
                    for (y = 0, j = 0; y < pWilds->map_size_y; y++, j+= pWilds->map_size_x) {
			    fread(pWilds->staticmap+j,1,pWilds->map_size_x,fp);
			    fread_to_eol(fp);
		    }

                /* Vizz - map a copy of the staticmap to apply things like vlinks, flooding etc to */
                    memcpy(pWilds->map, pWilds->staticmap,pWilds->map_size_x*pWilds->map_size_y);

                /* Vizz - allocate mob/obj wilds matrix */
//                    pWilds->char_matrix = allocate_char_matrix(pWilds->map_size_x, pWilds->map_size_y);
//                    pWilds->obj_matrix = allocate_obj_matrix(pWilds->map_size_x, pWilds->map_size_y);

                    if (pArea->wilds)
                    {
                        pLastWilds = pArea->wilds;

                        while(pLastWilds->next)
                            pLastWilds = pLastWilds->next;

                        pLastWilds->next = pWilds;
                    }
                    else
                    {
                        pArea->wilds = pWilds;
                    }

                }
		else
                if ( !str_cmp( word, "#-VMAP" ) )
                {
					;
                }
		else
                if ( !str_cmp( word, "#-WILDS" ) )
                {
                    if (pWilds->uid == 0)
                    {
                        plogf("wilds.c, load_wilds(): Wilds '%s' has no UID. Assigning next available one.",
                              pWilds->name);
                        pWilds->uid = gconfig.next_wilds_uid++;
			gconfig_write();
                    }

					if( (data = alloc_mem(sizeof(LLIST_WILDS_DATA))) ) {
						data->wilds = pWilds;
						data->uid = pWilds->uid;

						list_appendlink(loaded_wilds, data);
					}

                    return;
                }
		else
                if ( !str_cmp( word, "#TERRAIN" ) )
                {
                            pTerrain = fread_terrain( fp, pWilds );
                            add_terrain (pWilds, pTerrain);
                        }
                else
                if ( !str_cmp( word, "#REGION" ) )
                {
                            pRegion = fread_region( fp, pWilds );
                            add_region (pWilds, pRegion);
                        }
                else
                if ( !str_cmp( word, "#VLINK" ) )
                {
                    temp_pVLink = fread_vlink(fp, pArea);
                    add_vlink(pWilds, temp_pVLink);
                }


                break;

            case 'D':
                if ( !str_cmp( word, "DefaultTerrain" ) )
                {
                    pWilds->cDefaultTerrain = fgetc(fp);
                    plogf("wilds.c, load_wilds(): Default Terrain type is '%c'.", pWilds->cDefaultTerrain);
                    break;
                }
                if ( !str_cmp( word, "DefaultRegion"))
                {
                    pWilds->defaultRegion = flag_value(wilderness_regions, fread_word(fp));
                    if (pWilds->defaultRegion == NO_FLAG)
                        pWilds->defaultRegion = REGION_UNKNOWN;
                    break;
                }
                if ( !str_cmp( word, "DefaultPlace"))
                {
                    pWilds->defaultPlaceFlags = flag_value(place_flags, fread_word(fp));
                    if (pWilds->defaultPlaceFlags == NO_FLAG)
                        pWilds->defaultPlaceFlags = PLACE_NOWHERE;
                    break;
                }

                break;

            case 'N':
                if ( !str_cmp( word, "Name" ) )
                {
                    free_string(pWilds->name);
                    pWilds->name = fread_string(fp);
                }

                break;


            case 'R':
                if ( !str_cmp( word, "Repop" ) )
                    pWilds->repop = fread_number(fp);

                break;

            case 'U':
                if ( !str_cmp( word, "Uid" ) )
                    pWilds->uid = fread_number(fp);

                break;

        } /* end switch */
    } /* end for */

    // 

    return;
}

// Looks for a loaded-up vroom matching the coordinates specified
ROOM_INDEX_DATA *get_wilds_vroom(WILDS_DATA *pWilds, int x, int y)
{
	ITERATOR it;
    ROOM_INDEX_DATA *pVroom = NULL;

	iterator_start(&it, pWilds->loaded_vrooms);
	while(( pVroom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)))
        if (pVroom->x == x && pVroom->y == y)
        	break;
	iterator_stop(&it);

    return pVroom;
}

int get_wilds_vroom_x_by_dir(WILDS_DATA *pWilds, int x, int y, int door)
{
    int rel_vroom_x;

    const int vector_offset[10][2]=
    {
/* Vizz - {  x,  y }           */
          {  0, -1 }, /* North */
          {  1,  0 }, /* East  */
          {  0,  1 }, /* South */
          { -1,  0 }, /* West  */
          {  0,  0 }, /* Up    */
          {  0,  0 }, /* Down  */
          {  1, -1 }, /* NorthEast */
          { -1, -1 }, /* NorthWest */
          {  1,  1 }, /* SouthEast */
          { -1,  1 }  /* SouthWest */
    };

    /* Vizz - work out coordinates of vroom using the vector table to translate the direction */
    rel_vroom_x = x + vector_offset[door][0];

    return (rel_vroom_x);
}

int get_wilds_vroom_y_by_dir(WILDS_DATA *pWilds, int x, int y, int door)
{
    int rel_vroom_y;

    const int vector_offset[10][2]=
    {
/* Vizz - {  x,  y }           */
          {  0, -1 }, /* North */
          {  1,  0 }, /* East  */
          {  0,  1 }, /* South */
          { -1,  0 }, /* West  */
          {  0,  0 }, /* Up    */
          {  0,  0 }, /* Down  */
          {  1, -1 }, /* NorthEast */
          { -1, -1 }, /* NorthWest */
          {  1,  1 }, /* SouthEast */
          { -1,  1 }  /* SouthWest */
    };

    /* Vizz - work out coordinates of vroom using the vector table to translate the direction */
    rel_vroom_y = y + vector_offset[door][1];

    return (rel_vroom_y);
}

ROOM_INDEX_DATA *create_wilds_vroom(WILDS_DATA *pWilds, int x, int y)
{
    ROOM_INDEX_DATA *pRoomIndex;
    WILDS_TERRAIN *pTerrain;
    bool nonroom = false;

    if (!pWilds)
    {
        #ifdef DEBUG
            plogf("wilds.c, create_wilds_vroom(): pWilds = NULL");
        #endif
        return(NULL);
    }
    else
    {

        if ((pTerrain = get_terrain_by_coors (pWilds, x, y)) == NULL)
            nonroom = true;

        if (!nonroom)
        {
            #ifdef DEBUG
                plogf("wilds.c, create_wilds_vroom(): INFO    - Terrain at coordinates is valid. Creating vroom.");
            #endif
            pRoomIndex = create_vroom(pWilds,
                                x + pWilds->startx,
                                y + pWilds->starty,
                                pTerrain);
        }
        else
        {
            #ifdef DEBUG
                plogf("wilds.c, create_wilds_vroom(): FAILURE -  Terrain at coordinates is invalid. Returning NULL");
            #endif
            return (NULL);
        }

    }

    return (pRoomIndex);
}


// This will find the FIRST region that matches it.. so order matters.
// Move regions UP/DOWN in the list if you want to change their precedence
WILDS_REGION *get_region_by_coors (WILDS_DATA *pWilds, int x, int y)
{
    WILDS_REGION *pRegion;
    bool found = false;

    /* Check pointer is valid */
    if (!pWilds)
    {
#ifdef DEBUG
        plogf("wilds.c, get_region_by_coors(): Invalid pWilds pointer.");
#endif
        return(NULL);
    }

    for(pRegion = pWilds->pRegion;pRegion;pRegion = pRegion->next)
    {
        if (x >= pRegion->startx && x <= pRegion->endx &&
            y >= pRegion->starty && y <= pRegion->endy)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
#ifdef DEBUG
            plogf("wilds.c, get_region_by_coors(): Region type %c not found in list. Returning NULL.", j);
#endif
        return (NULL);
    }

#ifdef DEBUG
        plogf("wilds.c, get_region_by_coors(): SUCCESS - Region found. Returning pRegion", j);
#endif
    return (pRegion);
}

WILDS_TERRAIN *get_terrain_by_coors (WILDS_DATA *pWilds, int x, int y)
{
    WILDS_TERRAIN *pTerrain;
    bool found = false;
    char j;

    /* Check pointer is valid */
    if (!pWilds)
    {
        plogf("wilds.c, get_terrain_by_coors(): Invalid pWilds pointer.");
        return(NULL);
    }

	j = pWilds->staticmap[(y * pWilds->map_size_x) + x];

    for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == j)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        #ifdef DEBUG
            plogf("wilds.c, get_terrain_by_coors(): Terrain type %c not found in list. Returning NULL.", j);
        #endif
        return (NULL);
    }

    #ifdef DEBUG
        plogf("wilds.c, get_terrain_by_coors(): SUCCESS - Terrain found. Returning pTerrain", j);
    #endif
    return (pTerrain);
}



WILDS_TERRAIN *get_terrain_by_token (WILDS_DATA *pWilds, char token)
{
    WILDS_TERRAIN *pTerrain;

    for (pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
    {
        if (pTerrain->mapchar == token)
            return (pTerrain);
    }

    return NULL;
}

WILDS_VLINK *fread_vlink(FILE *fp, AREA_DATA *area)
{
    WILDS_VLINK *pVLink;
    char      *word;

    if (!fp)
    {
        plogf ("wilds.c, fread_vlink(): Invalid fp pointer.");
        abort();
    }

    pVLink = new_vlink();

    for ( ; ; )
    {
        word   = feof( fp ) ? "End" : fread_word( fp );

        switch ( UPPER(word[0]) )
        {
            case '#':
                if (!str_cmp(word, "#-VLINK")) {
			if(!pVLink->uid) {
				plogf("wilds.c, fread(): Vlink (%ld, %ld) has no UID. Assigning next available one.",
				      pVLink->wildsorigin_x, pVLink->wildsorigin_y);
				pVLink->uid = ++gconfig.next_vlink_uid;
				gconfig_write();
			}
                    return (pVLink);
	        }

            break;

            case 'D':
                if (!str_cmp(word, "Default_linkage"))
                {
                    pVLink->default_linkage = fread_flag (fp);
                    // This is the wrong place to put this... put it somewhere after reading in .are's
                    pVLink->current_linkage = VLINK_UNLINKED;
                }
                else
                if (!str_cmp(word, "Destvnum")) {
                    pVLink->destwnum = fread_widevnum (fp, area->uid);
                } else
                if (!str_cmp(word, "Door"))
                {
                    pVLink->door = fread_number (fp);

                    if (pVLink->door < 0 || pVLink->door > (MAX_DIR - 1))
                    {
                        plogf("wilds.c, fread_vlink(): vlink has bad door number.");
                        abort();
                    }
                }

            break;

            case 'M':
                if (!str_cmp(word, "Map_tile"))
                    pVLink->map_tile = fread_string_eol (fp);
            break;

            case 'O':
                if (!str_cmp(word, "Orig_desc"))
                    pVLink->orig_description = fread_string (fp);
                else if (!str_cmp(word, "Orig_keyword"))
                    pVLink->orig_keyword = fread_string (fp);
                else if (!str_cmp(word, "Orig_rsflags"))
                    pVLink->orig_rs_flags = fread_flag (fp);
                else if (!str_cmp(word, "Orig_key"))
                    pVLink->orig_key = fread_widevnum (fp, area->uid);
                else if (!str_cmp(word, "Orig_lock"))
                    pVLink->orig_lock = fread_number (fp);
                else if (!str_cmp(word, "Orig_pick"))
                    pVLink->orig_pick = fread_number (fp);

            break;

            case 'R':
                if (!str_cmp(word, "Rev_desc"))
                    pVLink->rev_description = fread_string (fp);
                else if (!str_cmp(word, "Rev_keyword"))
                    pVLink->rev_keyword = fread_string (fp);
                else if (!str_cmp(word, "Rev_rsflags"))
                    pVLink->rev_rs_flags = fread_flag (fp);
                else if (!str_cmp(word, "Rev_key"))
                    pVLink->rev_key = fread_number (fp);
                else if (!str_cmp(word, "Rev_lock"))
                    pVLink->rev_lock = fread_number (fp);
                else if (!str_cmp(word, "Rev_pick"))
                    pVLink->rev_pick = fread_number (fp);

            break;

            case 'U':
                if (!str_cmp(word, "Uid"))
                    pVLink->uid = fread_number (fp);
            break;

            case 'W':
                if (!str_cmp(word, "Wildsorigin_x"))
                    pVLink->wildsorigin_x = fread_number (fp);
                else
                if (!str_cmp(word, "Wildsorigin_y"))
                    pVLink->wildsorigin_y = fread_number (fp);
        } /* end switch */
    } /* end for */
}


void fwrite_vlink (FILE *fp, WILDS_VLINK *pVLink)
{
    if (!fp)
    {
        plogf("wilds.c, fwrite_vlink(): Invalid fp");
        return;
    }

    if (!pVLink)
    {
        plogf("wilds.c, fwrite_vlink(): Invalid pVLink");
        return;
    }

    fprintf(fp, "#VLINK\n");
    fprintf(fp, "Uid %ld\n", pVLink->uid);
    fprintf(fp, "Wildsorigin_x %d\n", pVLink->wildsorigin_x);
    fprintf(fp, "Wildsorigin_y %d\n", pVLink->wildsorigin_y);
    fprintf(fp, "Door %d\n", pVLink->door);
    fprintf(fp, "Destvnum %ld#%ld\n", pVLink->destwnum.auid, pVLink->destwnum.vnum);

    // Vizz - maptile is a raw string, so we deliberately don't use fix_string().
    fprintf(fp, "Map_tile %s\n", pVLink->map_tile);
    fprintf(fp, "Default_linkage %d\n", pVLink->default_linkage);

    // Vizz - pVLink->current_linkage is not stored, because it's a runtime only variable.
    fprintf(fp, "Orig_desc %s~\n", fix_string(pVLink->orig_description));
    fprintf(fp, "Orig_keyword %s~\n", fix_string(pVLink->orig_keyword));
    fprintf(fp, "Orig_rsflags %ld\n", pVLink->orig_rs_flags);
    fprintf(fp, "Orig_key %ld#%ld\n", pVLink->orig_key.auid, pVLink->orig_key.vnum);
    fprintf(fp, "Orig_lock %d\n", pVLink->orig_lock);
    fprintf(fp, "Orig_pick %d\n", pVLink->orig_pick);
    fprintf(fp, "Rev_desc %s~\n",  fix_string(pVLink->rev_description));
    fprintf(fp, "Rev_keyword %s~\n",  fix_string(pVLink->rev_keyword));
    fprintf(fp, "Rev_rsflags %ld\n", pVLink->rev_rs_flags);
    fprintf(fp, "Rev_key %ld\n", pVLink->rev_key);
    fprintf(fp, "Rev_lock %d\n", pVLink->rev_lock);
    fprintf(fp, "Rev_pick %d\n", pVLink->rev_pick);
    fprintf(fp, "#-VLINK\n\n");

    return;
}


WILDS_TERRAIN *fread_terrain (FILE *fp, WILDS_DATA *pWilds)
{
    WILDS_TERRAIN *pTerrain;
    char      *word;

/*
 * Check pointers are valid.
 */
    if(!fp)
    {
        plogf("wilds.c, fread_terrain(): Invalid fp");
        abort();
    }

    if(!pWilds)
    {
        plogf("wilds.c, fread_terrain(): Invalid pWilds");
        abort();
    }

    pTerrain = new_terrain(pWilds);

    for ( ; ; )
    {
        word   = feof( fp ) ? "End" : fread_word( fp );

        switch ( UPPER(word[0]) )
        {
            case '#':
                if ( !str_cmp( word, "#-TERRAIN" ) )
                {
                    plogf("wilds.c, fread_terrain(): Finished reading terrain record.");
                    return (pTerrain);
                }
                else
                if ( !str_cmp( word, "#ROOM" ) )
                {
                    plogf("wilds.c, fread_terrain(): Found #ROOM record.");
                    // need to use sent's current room reading logic
                    pTerrain->template = read_room_new(fp, pWilds->pArea, ROOMTYPE_TERRAIN);
                }

                break;

            case 'B':
                if ( !str_cmp( word, "Briefdesc"))
                {
                    pTerrain->briefdesc = fread_string(fp);
                    plogf("wilds.c, fread_terrain(): Briefdesc = '%s'.", pTerrain->briefdesc);
                }

                break;

            case 'N':
                if ( !str_cmp( word, "Nonroom"))
                {
                    pTerrain->nonroom = fread_number(fp);
                    plogf("wilds.c, fread_terrain(): Nonroom = %d.", pTerrain->nonroom);
                }

                break;

            case 'S':
                if ( !str_cmp( word, "Showchar"))
                {
                    pTerrain->showchar = fread_string_eol(fp);
                    plogf("wilds.c, fread_terrain(): Showchar = '%s'.", pTerrain->showchar);
                }
                else
                if ( !str_cmp( word, "Showname"))
                {
                    pTerrain->showname = fread_string(fp);
                    plogf("wilds.c, fread_terrain(): Showname = '%s'.", pTerrain->showname);
                }

                break;

            case 'T':
                if (!str_cmp(word, "Tile"))
                {
                    fgetc(fp);
                    pTerrain->mapchar = fgetc(fp);
                    plogf("wilds.c, fread_terrain(): Tile = '%c'.", pTerrain->mapchar);

                }

                break;

        } /* End switch */

    } /* End for */

}


void fwrite_terrain (FILE *fp, WILDS_TERRAIN *pTerrain)
{
    if (!fp)
    {
        plogf("wilds.c, fwrite_terrain(): Invalid fp");
        return;
    }

    if (!pTerrain)
    {
        plogf("wilds.c, fwrite_terrain(): Invalid pTerrain");
        return;
    }

    fprintf(fp, "#TERRAIN\n");
    fprintf(fp, "Tile %c\n", pTerrain->mapchar);
    fprintf(fp, "Showchar %s\n", pTerrain->showchar);
    fprintf(fp, "Showname %s~\n", fix_string(pTerrain->showname));
    fprintf(fp, "Briefdesc %s~\n", fix_string(pTerrain->briefdesc));
    fprintf(fp, "Nonroom %d\n", pTerrain->nonroom);
    save_room_new(fp, pTerrain->template, ROOMTYPE_TERRAIN);
    fprintf(fp, "#-TERRAIN\n\n");
    return;
}

WILDS_REGION *fread_region (FILE *fp, WILDS_DATA *pWilds)
{
    WILDS_REGION *pRegion;
    char      *word;

/*
 * Check pointers are valid.
 */
    if(!fp)
    {
        plogf("wilds.c, fread_region(): Invalid fp");
        abort();
    }

    if(!pWilds)
    {
        plogf("wilds.c, fread_region(): Invalid pWilds");
        abort();
    }

    pRegion = new_region(pWilds);

    for ( ; ; )
    {
        word   = feof( fp ) ? "End" : fread_word( fp );

        switch ( UPPER(word[0]) )
        {
            case '#':
                if ( !str_cmp( word, "#-REGION" ) )
                {
                    plogf("wilds.c, fread_region(): Finished reading region record.");
                    return (pRegion);
                }
                break;

            case 'E':
                if ( !str_cmp(word, "End"))
                {
                    pRegion->endx = fread_number(fp);
                    pRegion->endy = fread_number(fp);
                    break;
                }
                break;

            case 'L':
                if ( !str_cmp(word, "Label"))
                {
                    pRegion->region = flag_value(wilderness_regions, fread_word(fp));
                    if (pRegion->region == NO_FLAG)
                        pRegion->region = REGION_UNKNOWN;
                    break;
                }
                break;

            case 'P':
                if (!str_cmp(word, "Place"))
                {
                    pRegion->area_place_flags = flag_value(place_flags, fread_word(fp));
                    if (pRegion->area_place_flags == NO_FLAG)
                        pRegion->area_place_flags = PLACE_NOWHERE;
                }
                break;

            case 'S':
                if ( !str_cmp(word, "Start"))
                {
                    pRegion->startx = fread_number(fp);
                    pRegion->starty = fread_number(fp);
                    break;
                }
                break;

        } /* End switch */

    } /* End for */

}


void fwrite_region (FILE *fp, WILDS_REGION *pRegion)
{
    if (!fp)
    {
        plogf("wilds.c, fwrite_region(): Invalid fp");
        return;
    }

    if (!pRegion)
    {
        plogf("wilds.c, fwrite_region(): Invalid pRegion");
        return;
    }

    // This is to maintain order
    if (pRegion->next)
        fwrite_region(fp, pRegion->next);

    fprintf(fp, "#REGION\n");
    fprintf(fp, "Start %d %d\n", pRegion->startx, pRegion->starty);
    fprintf(fp, "End %d %d\n", pRegion->endx, pRegion->endy);
    fprintf(fp, "Label '%s'\n", flag_string(wilderness_regions, pRegion->region));
    fprintf(fp, "Place '%s'\n", flag_string(place_flags, pRegion->area_place_flags));
    fprintf(fp, "#-REGION\n\n");
    return;
}

void link_vroom(ROOM_INDEX_DATA *pWildsRoom)
{
	EXIT_DATA *pexit;
	int door;
	long dest_x;
	long dest_y;

	if(check_for_bad_room(pWildsRoom->wilds,pWildsRoom->x,pWildsRoom->y)) {
		for(door = 0; door < MAX_DIR; door++) if(!pWildsRoom->exit[door]) {
			dest_x = pWildsRoom->x + dir_offsets[door][0];
			dest_y = pWildsRoom->y + dir_offsets[door][1];

			if((dest_x == pWildsRoom->x && dest_y == pWildsRoom->y) || dest_x < 0 || dest_x >= pWildsRoom->wilds->map_size_x ||
				dest_y < 0 || dest_y >= pWildsRoom->wilds->map_size_y ||
				!check_for_bad_room(pWildsRoom->wilds,dest_x,dest_y)) continue;

			pexit                   = new_exit();
			pexit->u1.to_room       = NULL;
			pexit->orig_door        = door;
			pexit->from_room	= pWildsRoom;
			pexit->wilds.x          = dest_x;
			pexit->wilds.y          = dest_y;
			pexit->wilds.area_uid   = pWildsRoom->wilds->pArea->uid;
			pexit->wilds.wilds_uid  = pWildsRoom->wilds->uid;
			pWildsRoom->exit[door]  = pexit;
		}
	}
}

bool link_vlink(WILDS_VLINK *pVLink)
{
    WILDS_DATA *pWilds = NULL;
    ROOM_INDEX_DATA *pWildsRoom = NULL;
    ROOM_INDEX_DATA *pRevRoom = NULL;
    EXIT_DATA *pExit = NULL;
    bool found = false;
    long portal_x = 0;
    long portal_y = 0;
    int rev = 0;

    // Check vlink pointer is valid
    if (!pVLink)
    {
        plogf("wilds.c, link_vlink(): pVLink is NULL");
        return (false);
    }

    pWilds = pVLink->pWilds;
    pWildsRoom = get_wilds_vroom (pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y);
        portal_x = get_wilds_vroom_x_by_dir(pWilds,
                                            pVLink->wildsorigin_x,
                                            pVLink->wildsorigin_y,
                                            pVLink->door);

        portal_y = get_wilds_vroom_y_by_dir(pWilds,
                                            pVLink->wildsorigin_x,
                                            pVLink->wildsorigin_y,
                                            pVLink->door);

        // Mark the vlink entrance on the wildsmap
        pWilds->map[(portal_y * pWilds->map_size_x)+ portal_x] = '0';


	if (IS_SET(pVLink->default_linkage, VLINK_FROM_WILDS)) {
		// if the wilds room happens to be loaded up
		if (pWildsRoom) {
			if (!(pExit = pWildsRoom->exit[pVLink->door])) {
				pExit = new_exit ();
			} else if(!IS_SET(pExit->exit_info,EX_VLINK)) {
				if(pExit->long_desc) { free_string(pExit->long_desc); pExit->long_desc=NULL; }
				if(pExit->short_desc) { free_string(pExit->short_desc); pExit->short_desc=NULL; }
				if(pExit->keyword) { free_string(pExit->keyword); pExit->keyword=NULL; }
			} else {
				plogf("wilds.c, link_vlink(): Wilds-side vlink exit already exists.");
				return (false);
			}

			found = true;
			pExit->short_desc = str_dup("");
			pExit->long_desc = str_dup(pVLink->orig_description);
			pExit->keyword = str_dup(pVLink->orig_keyword);
			pExit->rs_flags = pVLink->orig_rs_flags | EX_VLINK;
			pExit->exit_info = pExit->rs_flags;
			pExit->door.rs_lock.key_wnum.pArea = get_area_from_uid(pVLink->orig_key.auid);
            pExit->door.rs_lock.key_wnum.vnum = pVLink->orig_key.vnum;
			pExit->door.rs_lock.flags = pVLink->orig_lock;
			pExit->door.rs_lock.pick_chance = pVLink->orig_pick;
			pExit->door.lock = pExit->door.rs_lock;
			pExit->u1.to_room = get_room_index_auid(pVLink->destwnum.auid, pVLink->destwnum.vnum);
			pExit->orig_door = pVLink->door;    /* OLC */
			pExit->wilds.x = 0;
			pExit->wilds.y = 0;
			pExit->wilds.area_uid = 0;
			pExit->wilds.wilds_uid = 0;

			pWildsRoom->exit[pVLink->door] = pExit;
			pExit->from_room = pWildsRoom;
			SET_BIT(pVLink->current_linkage, VLINK_FROM_WILDS);
		}
	}

    if (IS_SET(pVLink->default_linkage, VLINK_TO_WILDS))
    {
        // if the static room happens to be loaded up
        if ((pRevRoom=get_room_index_auid(pVLink->destwnum.auid, pVLink->destwnum.vnum))!=NULL)
        {
			if( IS_SET(pRevRoom->room_flag[1], ROOM_BLUEPRINT) ||
				IS_SET(pRevRoom->area->area_flags, AREA_BLUEPRINT) )
			{
				plogf("wilds.c, link_vlink(): Room involved in blueprints.");
			}
			else
			{
				rev = rev_dir[pVLink->door];

				if (pRevRoom->exit[rev]==NULL)
				{
					found = true;
					pExit = new_exit ();
					pExit->long_desc = str_dup(pVLink->rev_description);
					pExit->keyword = str_dup(pVLink->rev_keyword);
					pExit->rs_flags = pVLink->rev_rs_flags | EX_VLINK;
					pExit->exit_info = pExit->rs_flags;
        			pExit->door.rs_lock.key_wnum.pArea = get_area_from_uid(pVLink->orig_key.auid);
                    pExit->door.rs_lock.key_wnum.vnum = pVLink->orig_key.vnum;
					pExit->door.rs_lock.flags = pVLink->rev_lock;
					pExit->door.rs_lock.pick_chance = pVLink->rev_pick;
					pExit->door.lock = pExit->door.rs_lock;
					pExit->u1.to_room = NULL;
					pExit->wilds.x = pVLink->wildsorigin_x;
					pExit->wilds.y = pVLink->wildsorigin_y;
					pExit->wilds.area_uid = pVLink->pWilds->pArea->uid;
					pExit->wilds.wilds_uid = pVLink->pWilds->uid;
					pExit->orig_door = rev;    /* OLC */

					pRevRoom->exit[rev] = pExit;
					pExit->from_room = pRevRoom;
					SET_BIT(pVLink->current_linkage, VLINK_TO_WILDS);
				}
				else
					return (false);
			}
        }
        else
        {
            plogf("wilds.c, link_vlink(): Static room not found.");
        }

    }

    if (found)
        return (true);
    else
        return (false);
}

bool unlink_vlink(WILDS_VLINK *pVLink)
{
    WILDS_DATA *pWilds = NULL;
    ROOM_INDEX_DATA *pWildsRoom = NULL;
    ROOM_INDEX_DATA *pRevRoom = NULL;
    EXIT_DATA *pExit = NULL;
    bool found = false;
    long portal_x = 0;
    long portal_y = 0;
    int rev = 0;

    // Check vlink pointer is valid
    if (!pVLink)
    {
        plogf("wilds.c, unlink_vlink(): pVLink is NULL");
        return (false);
    }

    if (pVLink->current_linkage == VLINK_UNLINKED)
    {
        plogf("wilds.c, unlink_vlink(): current_linkage is VLINK_UNLINKED, so can't unlink.");
        return(false);
    }
    pWilds = pVLink->pWilds;

portal_x = get_wilds_vroom_x_by_dir(pWilds,
				    pVLink->wildsorigin_x,
				    pVLink->wildsorigin_y,
				    pVLink->door);

portal_y = get_wilds_vroom_y_by_dir(pWilds,
				    pVLink->wildsorigin_x,
				    pVLink->wildsorigin_y,
				    pVLink->door);

/* Remove the vlink entrance from the wildsmap, restoring the original terrain tile */
pWilds->map[(portal_y * pWilds->map_size_x) + portal_x] =
    pWilds->staticmap[(portal_y * pWilds->map_size_x) + portal_x];

    if (IS_SET(pVLink->current_linkage, VLINK_FROM_WILDS))
    {
        /* First, check if wilds-side exit exists */
        if ((pWildsRoom = get_wilds_vroom(pVLink->pWilds,
                                          pVLink->wildsorigin_x,
                                          pVLink->wildsorigin_y)) != NULL)
        {
            if ((pExit = pWildsRoom->exit[pVLink->door]) != NULL)
            {
		    pWildsRoom->exit[pVLink->door] = NULL;
                free_exit(pExit);
                found = true;
		link_vroom(pWildsRoom);
            }
            else
                plogf("wilds.c, unlink_vlink(): wilds side of vlink - exit missing!");
        }
	/* Mark vlink as unlinked in from_wilds direction */
	REMOVE_BIT(pVLink->current_linkage, VLINK_FROM_WILDS);
    }

    if (IS_SET(pVLink->current_linkage, VLINK_TO_WILDS))
    {
        /* Check if reverse-side exit exists */
        if ((pRevRoom = get_room_index_auid(pVLink->destwnum.auid, pVLink->destwnum.vnum)) !=NULL)
        {
            rev = rev_dir[pVLink->door];
            if ((pExit = pRevRoom->exit[rev]) != NULL)
            {
		    pRevRoom->exit[rev] = NULL;
                free_exit(pExit);
                found = true;
            }
            else
                plogf("wilds.c, unlink_vlink(): reverse side of vlink - exit missing!");
        }
	REMOVE_BIT(pVLink->current_linkage, VLINK_TO_WILDS);
    }

    if (found)
    {
        pVLink->current_linkage = VLINK_UNLINKED;
        return (true);
    }
    else
        return (false);
}

WILDS_VLINK *find_vlink_to_coord(WILDS_DATA *pWilds, int x, int y)
{
	WILDS_VLINK *pVLink;

	for(pVLink=pWilds->pVLink;pVLink;pVLink = pVLink->next) {
		if((x == get_wilds_vroom_x_by_dir(pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y, pVLink->door)) &&
			(y == get_wilds_vroom_y_by_dir(pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y, pVLink->door)))
			break;
	}

	return pVLink;
}

WILDS_VLINK *find_vlink_from_coord(WILDS_DATA *pWilds, int x, int y, int door)
{
	WILDS_VLINK *pVLink;

	for(pVLink=pWilds->pVLink;pVLink;pVLink = pVLink->next) {
		if(pVLink->wildsorigin_x == x &&
			pVLink->wildsorigin_y == y &&
			pVLink->door == door)
			return pVLink;
	}

	return NULL;
}

const char *vroom_dir_opened[] = {"{YN","{YE{b\n\r","{YS{b","{YW{B<{b-","{YU{b-{B({WA{B){b-","{YD{b-{B>{b","    {YNE{x\n\r","{YNW{b    ","    {YSE{x\n\r","{YSW{b    "};
const char *vroom_dir_closed[] = {"{b-","-\n\r","-{b","-{B<{b-","--{B({WA{B){b-","--{B>{b","     {b-{x\n\r","{b-     ","     -{x\n\r","-     "};

void vroom_show_valid_door(CHAR_DATA *ch, WILDS_DATA *pWilds, int wx, int wy, int door)
{
	WILDS_VLINK *vlink = find_vlink_from_coord(pWilds, wx, wy, door);

	if(vlink == NULL) {
		int to_x;
		int to_y;

		if( door == DIR_UP || door == DIR_DOWN) {
			send_to_char(vroom_dir_closed[door], ch);
			return;
		}

		to_x = get_wilds_vroom_x_by_dir(pWilds, wx, wy, door);
		to_y = get_wilds_vroom_y_by_dir(pWilds, wx, wy, door);

		if(!check_for_bad_room(pWilds, to_x, to_y)) {
			send_to_char(vroom_dir_closed[door], ch);
			return;
		}
	}

	send_to_char(vroom_dir_opened[door], ch);
}

void show_vroom_header_to_char(WILDS_TERRAIN *pTerrain, WILDS_DATA *pWilds, int wx, int wy, CHAR_DATA *to)
{
	char buf[MAX_STRING_LENGTH];
	int linelength = 0;
	int count;

	if (IS_IMMORTAL(to) && (IS_NPC(to) || IS_SET(to->act[0], PLR_HOLYLIGHT))) {
		sprintf (buf, "\n\r{C [ Area: %ld '%s', Wilds uid: %ld '%s', Vroom (%d, %d) ]{x",
			pWilds->pArea->uid, pWilds->pArea->name,
			pWilds->uid, pWilds->name,
			wx, wy);

		send_to_char(buf, to);
	}

	linelength = strlen(pTerrain->template->name);
	linelength = 50 - linelength;

	if (IS_SET(pTerrain->template->room_flag[0], ROOM_SAFE))
		sprintf(buf, "\n\r {W%s", pTerrain->template->name);
	else if (IS_SET(pTerrain->template->room_flag[0], ROOM_UNDERWATER))
		sprintf(buf, "\n\r {C%s", pTerrain->template->name);
	else
		sprintf(buf, "\n\r {Y%s", pTerrain->template->name);

	send_to_char(buf, to);

	if (IS_SET(pTerrain->template->room_flag[0], ROOM_PK) && IS_SET(pTerrain->template->room_flag[0], ROOM_CHAOTIC)) {
		sprintf(buf, "  {M[CPK ROOM]");
		send_to_char(buf, to);
		linelength -= 13;
	} else if (IS_SET(pTerrain->template->room_flag[0], ROOM_CHAOTIC)) {
		sprintf(buf, "  {M[CHAOTIC]");
		send_to_char(buf, to);
		linelength -= 12;
	} else if (IS_SET(pTerrain->template->room_flag[0], ROOM_PK)) {
		sprintf(buf, "  {R[NPK ROOM]");
		send_to_char(buf, to);
		linelength -= 12;
	}

	if (IS_SET(pTerrain->template->room_flag[1], ROOM_MULTIPLAY)) {
		sprintf(buf, "  {W[FREE FOR ALL]");
		send_to_char(buf, to);
		linelength -= 16;
	}

	if (IS_SET(pTerrain->template->room_flag[0], ROOM_HOUSE_UNSOLD)) {
		sprintf(buf, "  {R[PRIME REAL ESTATE]");
		send_to_char(buf, to);
		linelength -= 21;
	}

	for (count = 0; count < linelength; count++)
		send_to_char(" ", to);

	vroom_show_valid_door(to, pWilds, wx, wy, DIR_NORTHWEST);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_NORTH);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_NORTHEAST);

	send_to_char ("{B({b-----------------------------------------------{B){b  ", to);

	vroom_show_valid_door(to, pWilds, wx, wy, DIR_WEST);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_UP);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_DOWN);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_EAST);


	for (count = 0; count < 51; count++)
		send_to_char(" ", to);

	vroom_show_valid_door(to, pWilds, wx, wy, DIR_SOUTHWEST);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_SOUTH);
	vroom_show_valid_door(to, pWilds, wx, wy, DIR_SOUTHEAST);

	send_to_char("\n\r", to);


}

bool is_wilds_coords(WILDS_COORD *coord, WILDS_DATA *wilds, int x, int y)
{
	if( !coord ) return false;

	if( coord->wilds != wilds ) return false;

	if( coord->x != x ) return false;

	if( coord->y != y ) return false;

	return true;
}

static void set_map_tile(char **map, int x, int y, char color, char tile)
{
	// WARNING: NO SANITY CHECKS ARE MADE IN HERE FOR COORDINATES
	char *mp = &map[y][2 * x];

	mp[0] = color;
	mp[1] = tile;
}

void show_map_to_char_wyx(WILDS_DATA *pWilds, int wx, int wy,
                      CHAR_DATA * to,
                      int vx,
                      int vy,
                      int bonus_view_x,
				      int bonus_view_y,
                      bool olc)
{
	WILDS_TERRAIN *pTerrain;
	WILDS_VLINK *pVLink;
	int x, y;
	long index;
	DESCRIPTOR_DATA * d;
	bool foundterrain = false;
	char j[6];
	char last_terrain[6];
	char temp[6];
	int squares_to_show_x;
	int squares_to_show_y;
	char last_colour_char;
	char buf[MSL];
	char padding1[MIL];
	char padding2[MIL];
	char tlcoor[MIL];
	char trcoor[MIL];
	char blcoor[MIL];
	char brcoor[MIL];
	int vp_startx, vp_starty, vp_endx, vp_endy;
	int i, pad;
	ITERATOR it;
	SHIP_DATA *ship;
	extern	LLIST *loaded_ships;


	BUFFER *output = new_buf();
	add_buf(output, "\n\r");

	squares_to_show_x = get_squares_to_show_x(bonus_view_x);
	squares_to_show_y = get_squares_to_show_y(bonus_view_y);
	last_colour_char = ' ';

	vp_startx = wx - squares_to_show_x;
	vp_endx   = wx + squares_to_show_x;
	vp_starty = wy - squares_to_show_y;
	vp_endy   = wy + squares_to_show_y;

	if (olc)
	{
		if (vp_startx < 0)
		{
			vp_startx = 0;
			vp_endx   = (squares_to_show_x * 2);
		}
		else if (vp_startx > pWilds->map_size_x)
		{
			vp_startx = pWilds->map_size_x - (squares_to_show_x * 2);
			vp_endx   = pWilds->map_size_x;
		}

		if (vp_starty < 0)
		{
			vp_starty = 0;
			vp_endy   = (squares_to_show_y * 2);
		}
		else if (vp_startx > pWilds->map_size_x)
		{
			vp_startx = pWilds->map_size_x - (squares_to_show_x * 2);
			vp_endx   = pWilds->map_size_x;
		}

		add_buf(output, "View Window                      Edit Window\n\r");

		sprintf(tlcoor, "(%d, %d)", vp_startx, vp_starty);
		sprintf(trcoor, "(%d, %d)", vp_endx, vp_starty);
		pad = squares_to_show_x * 2 + 7;

		for( i=0; i < pad ; i++ )
		{
		   padding1[i] = ' ';
		}

		padding1[i] = 0;
		pad = ((squares_to_show_x * 2) - strlen(tlcoor)) - strlen(trcoor);

		for( i=0; i < pad ; i++ )
		{
		   padding2[i] = ' ';
		}

		padding2[i] = 0;
		sprintf(buf, "%s%s%s%s\n\r", padding1, tlcoor, padding2, trcoor);
		add_buf(output, buf);
	}

	const int cols = 2 * squares_to_show_x + 1;
	const int rows = 2 * squares_to_show_y + 1;
	const int col_size = 2;	// XY -> X color code, Y tile
	const int row_size = (col_size * cols);

	char **map_str = malloc(rows * sizeof(char *));
	for( int r = 0; r < rows; r++)
		map_str[r] = malloc(row_size);

	char **olc_str = NULL;

	if( olc )
	{
		olc_str = malloc(rows * sizeof(char *));
		for( int r = 0; r < rows; r++)
			olc_str[r] = malloc(cols + 1);
	}

	// Create map data
	for (y = vp_starty;y <= vp_endy;y++)
	{
		char *mp = map_str[y - vp_starty];
		char *op = NULL;

		if( olc ) op = olc_str[y - vp_starty];

		for (x = vp_startx;x <= vp_endx;x++)
		{
			if (x >= 0 && x < pWilds->map_size_x && y >= 0 && y < pWilds->map_size_y)
			{
				index = y * pWilds->map_size_x + x;
				sprintf(j, "%c",pWilds->map[index]);
				if (!str_cmp(j, last_terrain))
				{
					sprintf(temp, last_terrain);
				}
				else
				{
					/* Vizz - Search the terrain list linearly for now at least. could index this later for speed */
					foundterrain = false;
					for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
					{
						if (pWilds->map[index] == pTerrain->mapchar)
						{
							sprintf(temp, pTerrain->showchar);
							sprintf(last_terrain, temp);
							foundterrain = true;
						}

					}

					if (!foundterrain)
					{
						/* Vizz - highlight vlink entrances */
						if (!strcmp(j, "0"))
							sprintf(temp, "{YO");
						else
							/* Vizz - allow for non-terrain defined characters - display verbatim */
							sprintf(temp, j);
					}
				}

				*mp++ = temp[1];
				*mp++ = temp[2];

				if( olc )
				{
					*op++ = j[0];
				}
			}
			else
			{
				if (!olc)
				{
					*mp++ = 'x';
					if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0)
					{
						*mp++ = '.';
					}
					else
					{
						*mp++ = ' ';
					}
				}
				else
				{
					*mp++ = 'x';
					*mp++ = ' ';
					*op++ = ' ';
				}
			}
		}

		if( olc )
			*op = '\0';
	}

	///////////////////////////////////////
	// Put various markers


	// Vlinks
	for(pVLink=pWilds->pVLink;pVLink;pVLink = pVLink->next)
	{
		int vx = get_wilds_vroom_x_by_dir(pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y, pVLink->door);
		int vy = get_wilds_vroom_y_by_dir(pWilds, pVLink->wildsorigin_x, pVLink->wildsorigin_y, pVLink->door);

		if( (vx >= vp_startx && vx <= vp_endx) &&
			(vy >= vp_starty && vy <= vp_endy) )
		{
			set_map_tile(map_str, vx - vp_startx, vy - vp_starty, pVLink->map_tile[1], pVLink->map_tile[2]);
		}
	}

	// Objects and Mobiles
    ROOM_INDEX_DATA *pVroom;
	iterator_start(&it, pWilds->loaded_vrooms);
	while(( pVroom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)))
	{

        if ((pVroom->x >= vp_startx && pVroom->x <= vp_endx) &&
        	(pVroom->y >= vp_starty && pVroom->y <= vp_endy))
        {
			bool found = false;
			for(CHAR_DATA *mob = pVroom->people; mob; mob = mob->next_in_room)
			{
				if( IS_NPC(mob) && IS_SET(mob->act[1], ACT2_SHOW_IN_WILDS) )
				{
					found = true;
					break;
				}
			}

			if( found )
			{
				set_map_tile(map_str, pVroom->x - vp_startx, pVroom->y - vp_starty, 'Y', '@');
				continue;
			}


			for(OBJ_DATA *obj = pVroom->contents; obj; obj = obj->next_content)
			{
				if( IS_SET(obj->extra[2], ITEM_SHOW_IN_WILDS))
				{
					found = true;
					break;
				}
			}

			if( found )
			{
				set_map_tile(map_str, pVroom->x - vp_startx, pVroom->y - vp_starty, 'Y', '*');
			}
		}
	}
	iterator_stop(&it);

	// Players
	for (d = descriptor_list; d != NULL;d = d->next)
	{
		if (d->connected == CON_PLAYING && d->character != to &&
			can_see(to, d->character) &&
			(d->character->in_room->wilds == pWilds ||
				(d->character->in_room->viewwilds == pWilds && IS_SET(d->character->in_room->room_flag[1],ROOM_VISIBLE_ON_MAP))) &&
			(d->character->in_room->x >= vp_startx && d->character->in_room->x <= vp_endx) &&
			(d->character->in_room->y >= vp_starty && d->character->in_room->y <= vp_endy))
		{
			set_map_tile(map_str, d->character->in_room->x - vp_startx, d->character->in_room->y - vp_starty, 'W', '@');
		}
	}

	// Ships
	iterator_start(&it, loaded_ships);
	while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
	{
		if( ship->ship->in_room->wilds == pWilds &&
			(ship->ship->in_room->x >= vp_startx && ship->ship->in_room->x <= vp_endx) &&
			(ship->ship->in_room->y >= vp_starty && ship->ship->in_room->y <= vp_endy) )
		{
			get_ship_wildsicon(ship, temp, sizeof(temp) - 1);

			set_map_tile(map_str, ship->ship->in_room->x - vp_startx, ship->ship->in_room->y - vp_starty, temp[1], temp[2]);
		}

		for( int i = 0; i < 3; i++ )
		{
			WILDS_COORD wc = ship->last_coords[i];

			if( wc.wilds == pWilds &&
				(wc.x >= vp_startx && wc.x <= vp_endx) &&
				(wc.y >= vp_starty && wc.y <= vp_endy))
			{
				set_map_tile(map_str, wc.x - vp_startx, wc.y - vp_starty, 'C', '~');
			}
		}
	}
	iterator_stop(&it);

	// Viewer
	if( (vx >= vp_startx && vx <= vp_endx) &&
		(vy >= vp_starty && vy <= vp_endy) )
	{
		set_map_tile(map_str, vx - vp_startx, vy - vp_starty, 'M', '@');
	}

	last_colour_char = ' ';
	for (y = 0; y < rows; y++)
	{
		char *mp = map_str[y];

		for(x = 0; x < cols; x++, mp += col_size)
		{
			char color = mp[0];
			char tile = mp[1];

			if( color != last_colour_char )
			{
				temp[0] = '{';
				temp[1] = color;
				temp[2] = tile;
				temp[3] = '\0';
				last_colour_char = color;
			}
			else
			{
				temp[0] = tile;
				temp[1] = '\0';
			}

			add_buf(output, temp);
		}

		if(olc)
		{
			add_buf(output, "       {x");
			add_buf(output, olc_str[y]);
			last_colour_char = ' ';
		}

		add_buf(output, "\n\r");
	}

	if (olc)
	{
		sprintf(blcoor, "(%d, %d)", vp_startx, vp_endy);
		sprintf(brcoor, "(%d, %d)", vp_endx, vp_endy);

		pad = squares_to_show_x * 2 + 7;
		for( i=0; i < pad ; i++ )
		   padding1[i] = ' ';
		padding1[i] = 0;

		pad = ((squares_to_show_x * 2) - strlen(blcoor)) - strlen(brcoor);
		for( i=0; i < pad ; i++ )
		   padding2[i] = ' ';
		padding2[i] = 0;

		sprintf(buf, "{x%s%s%s%s\n\r", padding1, blcoor, padding2, brcoor);
		add_buf(output, buf);
	}

	add_buf(output, "{x");

	send_to_char(output->string, to);

#if 0
	for (y = vp_starty;y <= vp_endy;y++)
	{
		cString = 0;
		for (x = vp_startx;x <= vp_endx;x++, mp+=col_size)
		{
			found = false;

			if (x >= 0 && x < pWilds->map_size_x && y >= 0 && y < pWilds->map_size_y)
			{
				if((pVLink = find_vlink_to_coord(pWilds,x,y)) && pVLink->map_tile && pVLink->map_tile[0])
				{
					strcpy(temp,pVLink->map_tile);
					found = true;
				}

				for (d = descriptor_list; d != NULL;d = d->next)
				{
					if (d->connected == CON_PLAYING && d->character != to &&
						can_see(to, d->character) &&
						(d->character->in_room->wilds == pWilds ||
							(d->character->in_room->viewwilds == pWilds && IS_SET(d->character->in_room->room_flag[1],ROOM_VISIBLE_ON_MAP))) &&
						d->character->in_room->x == x &&
						d->character->in_room->y == y)
					{
						sprintf(temp, "{W@");
						found = true;
					}
				}

				iterator_start(&it, loaded_ships);
				while( (ship = (SHIP_DATA *)iterator_nextdata(&it)) )
				{
					if( ship->ship->in_room->wilds == pWilds &&
						ship->ship->in_room->x == x &&
						ship->ship->in_room->y == y )
					{
						get_ship_wildsicon(ship, temp, sizeof(temp) - 1);
						found = true;
					}
					else if(is_wilds_coords(&ship->last_coords[0], pWilds, x, y) ||
						is_wilds_coords(&ship->last_coords[1], pWilds, x, y) ||
						is_wilds_coords(&ship->last_coords[2], pWilds, x, y) )
					{
						sprintf(temp, "{C~");
						found = true;
					}

				}
				iterator_stop(&it);

				if ((vx == x) && (vy == y))
				{
					sprintf(temp, "{M@");
					found = true;
				}


				if (!found)
				{
					sprintf(j, "%c",pWilds->map[index]);
					if (!str_cmp(j, last_terrain))
					{
						sprintf(temp, last_terrain);
					}
					else
					{
						/* Vizz - Search the terrain list linearly for now at least. could index this later for speed */
						foundterrain = false;
						for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
						{
							if (pWilds->map[index] == pTerrain->mapchar)
							{
								sprintf(temp, pTerrain->showchar);
								sprintf(last_terrain, temp);
								foundterrain = true;
							}

						}

						if (!foundterrain)
						{
							/* Vizz - highlight vlink entrances */
							if (!strcmp(j, "0"))
								sprintf(temp, "{YO");
							else
								/* Vizz - allow for non-terrain defined characters - display verbatim */
								sprintf(temp, j);
						}
					}
				}

				if (last_char_same && (temp[2] != last_char || temp[1] != last_colour_char))
				{
					last_char_same = false;
				}

				if (temp[2] == last_char && temp[1] == last_colour_char)
				{
					 last_char_same = true;
				}

				if (last_char_same)
				{
					 sprintf(temp, "%c", temp[2]);
				}

				send_to_char(temp, to);

				if (olc)
				{
					edit_mapstring[cString] = j[0];
					cString++;
				}

				if (last_char_same)
				{
					last_char = temp[0];
				}
				else
				{
					last_char = temp[2];
					last_colour_char = temp[1];
				}
			}
			else
			{
				/* If we're displaying outside the map bounds, fill in with starfield */
				if (!olc)
				{
					if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0)
					{
						last_char = '.'; last_colour_char = 'x';
						send_to_char("{x.", to);
					}
					else
						send_to_char(" ", to);
				}
				else
				{
					send_to_char(" ", to);
				}
				edit_mapstring[cString] = ' ';
				cString++;
			}
		}

		if (olc)
		{
			edit_mapstring[cString] = '\0';
			sprintf(buf, "       {x%s{%c", edit_mapstring, last_colour_char);
			send_to_char(buf, to);
		}
		send_to_char("\n\r", to);
	}

	if (olc)
	{
		sprintf(blcoor, "(%d, %d)", vp_startx, vp_endy);
		sprintf(brcoor, "(%d, %d)", vp_endx, vp_endy);
		pad = squares_to_show_x * 2 + 7;

		for( i=0; i < pad ; i++ )
		{
		   padding1[i] = ' ';
		}

		padding1[i] = 0;
		pad = ((squares_to_show_x * 2) - strlen(blcoor)) - strlen(brcoor);

		for( i=0; i < pad ; i++ )
		{
		   padding2[i] = ' ';
		}

		padding2[i] = 0;
		sprintf(buf, "{x%s%s%s%s\n\r",
				padding1, blcoor, padding2, brcoor);
		send_to_char(buf, to);
	}

	send_to_char("{x", to);
#endif

	free_buf(output);

	if( map_str )
	{
		for( int r = 0; r < rows; r++ )
		{
			if( map_str[r] )
				free(map_str[r]);
		}

		free(map_str);
	}

	if( olc_str )
	{
		for( int r = 0; r < rows; r++ )
		{
			if( olc_str[r] )
				free(olc_str[r]);
		}

		free(olc_str);
	}

	return;
}

void show_map_to_char(CHAR_DATA * ch, CHAR_DATA * to, int bonus_view_x, int bonus_view_y, bool olc)
{
    WILDS_DATA *pWilds;

    if (olc) {
        pWilds = ch->desc->pEdit;
    } else {
        pWilds = ch->in_wilds;
	}

	show_map_to_char_wyx(pWilds, ch->in_room->x, ch->in_room->y, to, ch->in_room->x, ch->in_room->y, bonus_view_x, bonus_view_y, olc);
}

void get_wilds_mapstring(BUFFER *buffer, WILDS_DATA *pWilds,
						int wx, int wy,
						int vx, int vy,
						int bonus_view_x, int bonus_view_y,
						char *marker)
{
    WILDS_TERRAIN *pTerrain;
    WILDS_VLINK *pVLink;
    int x, y;
    long index;
    bool found = false;
    bool foundterrain = false;
    char j[6];
    char last_terrain[6];
    char temp[6];
    int squares_to_show_x;
    int squares_to_show_y;
    bool last_char_same;
    char last_char;
    char last_colour_char;
    int vp_startx, vp_starty, vp_endx, vp_endy;

    squares_to_show_x = get_squares_to_show_x(bonus_view_x);
    squares_to_show_y = get_squares_to_show_y(bonus_view_y);
    last_char_same = false;
    last_char = ' ';
    last_colour_char = ' ';

    vp_startx = wx - squares_to_show_x;
    vp_endx   = wx + squares_to_show_x;
    vp_starty = wy - squares_to_show_y;
    vp_endy   = wy + squares_to_show_y;

    if( IS_NULLSTR(marker) )
    	marker = "{RX{x";

    for (y = vp_starty;y <= vp_endy;y++)
    {
        for (x = vp_startx;x <= vp_endx;x++)

        {
            found = false;
            index = y * pWilds->map_size_x + x;

            if (x >= 0 && x < pWilds->map_size_x && y >= 0 && y < pWilds->map_size_y)
            {
				if((pVLink = find_vlink_to_coord(pWilds,x,y)) && pVLink->map_tile && pVLink->map_tile[0]) {
					strcpy(temp,pVLink->map_tile);
					found = true;
				}

                if ((vx == x) && (vy == y))
                {
                    sprintf(temp, marker);
                    found = true;
                }

                if (!found)
                {
                    sprintf(j, "%c",pWilds->map[index]);
                    if (!str_cmp(j, last_terrain))
                    {
                        sprintf(temp, last_terrain);
                    }
                    else
                    {
                        foundterrain = false;
                        for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
                        {
                            if (pWilds->map[index] == pTerrain->mapchar)
                            {
                                sprintf(temp, pTerrain->showchar);
                                sprintf(last_terrain, temp);
                                foundterrain = true;
                            }

                        }

                        if (!foundterrain)
                        {
                            if (!strcmp(j, "0"))
                                sprintf(temp, "{YO");
                            else
                                sprintf(temp, j);
                        }
                    }
                }

                if (last_char_same
                    && (temp[2] != last_char
                        || temp[1] != last_colour_char))
                {
                    last_char_same = false;
                }

                if (temp[2] == last_char && temp[1] == last_colour_char)
                {
                     last_char_same = true;
                }

                if (last_char_same)
                {
                     sprintf(temp, "%c", temp[2]);
                }

				add_buf(buffer, temp);

                if (last_char_same)
                {
                    last_char = temp[0];
                }
                else
                {
                    last_char = temp[2];
                    last_colour_char = temp[1];
                }
            }
            else
            {
                /* If we're displaying outside the map bounds, fill in with starfield */
				if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0)
				{
					last_char = '.'; last_colour_char = 'x';
					add_buf(buffer, "{x.");
				}
				else
					add_buf(buffer, " ");
            }
        }
		add_buf(buffer, "\n\r");
    }
    return;
}


#if 0
void show_map_to_char(CHAR_DATA * ch,
                      CHAR_DATA * to,
                      int bonus_view_x,
		      int bonus_view_y,
                      bool olc)
{
    WILDS_DATA *pWilds;
    WILDS_TERRAIN *pTerrain;
    WILDS_VLINK *pVLink;
    int x, y;
    long index;
    DESCRIPTOR_DATA * d;
    bool found = false;
    bool foundterrain = false;
    char j[5];
    char last_terrain[5];
    char temp[5];
    int squares_to_show_x;
    int squares_to_show_y;
    bool last_char_same;
    char last_char;
    char last_colour_char;
    char edit_mapstring[80];
    char buf[MIL];
    char padding1[MIL];
    char padding2[MIL];
    char tlcoor[MIL];
    char trcoor[MIL];
    char blcoor[MIL];
    char brcoor[MIL];
    int cString;
    int vp_startx, vp_starty, vp_endx, vp_endy;
    int i, pad;

    if (olc)
        pWilds = ch->desc->pEdit;
    else
        pWilds = ch->in_wilds;

    squares_to_show_x = get_squares_to_show_x(bonus_view_x);
    squares_to_show_y = get_squares_to_show_y(bonus_view_y);
    last_char_same = false;
    last_char = ' ';
    last_colour_char = ' ';
    edit_mapstring[0] = '\0';
    send_to_char("\n\r", ch);

    vp_startx = ch->in_room->x - squares_to_show_x;
    vp_endx   = ch->in_room->x + squares_to_show_x;
    vp_starty = ch->in_room->y - squares_to_show_y;
    vp_endy   = ch->in_room->y + squares_to_show_y;

    if (olc)
    {
        if (vp_startx < 0)
        {
            vp_startx = ch->in_room->x;
            vp_endx   = ch->in_room->x + (squares_to_show_x * 2);
        }
        else
            if (vp_startx > pWilds->map_size_x)
            {
                vp_startx = pWilds->map_size_x - (squares_to_show_x * 2);
                vp_endx   = pWilds->map_size_x;
            }

        if (vp_starty < 0)
        {
            vp_starty = ch->in_room->y;
            vp_endy   = ch->in_room->y + (squares_to_show_y * 2);
        }
        else
            if (vp_startx > pWilds->map_size_x)
            {
                vp_startx = pWilds->map_size_x - (squares_to_show_x * 2);
                vp_endx   = pWilds->map_size_x;
            }

        send_to_char("View Window                      Edit Window\n\r", to);

        sprintf(tlcoor, "(%d, %d)", vp_startx, vp_starty);
        sprintf(trcoor, "(%d, %d)", vp_endx, vp_starty);
        pad = squares_to_show_x * 2 + 7;

        for( i=0; i < pad ; i++ )
        {
           padding1[i] = ' ';
        }

        padding1[i] = 0;
        pad = ((squares_to_show_x * 2) - strlen(tlcoor)) - strlen(trcoor);

        for( i=0; i < pad ; i++ )
        {
           padding2[i] = ' ';
        }

        padding2[i] = 0;
        sprintf(buf, "%s%s%s%s\n\r",
                padding1, tlcoor, padding2, trcoor);
        send_to_char(buf, to);
    }

    for (y = vp_starty;y <= vp_endy;y++)
    {
        cString = 0;
        for (x = vp_startx;x <= vp_endx;x++)

        {
            found = false;
            index = y * pWilds->map_size_x + x;

            if (x >= 0
                && x < pWilds->map_size_x
                && y >= 0
                && y < pWilds->map_size_y)
            {

		if((pVLink = find_vlink_to_coord(pWilds,x,y)) && pVLink->map_tile && pVLink->map_tile[0]) {
			strcpy(temp,pVLink->map_tile);
			found = true;
		}

                if (ch->in_room->x == x
                    && ch->in_room->y == y)
                {
                    sprintf(temp, "{M@{x");
                    found = true;
                }

                for (d = descriptor_list; d != NULL;d = d->next)
                {
                    if (d->connected == CON_PLAYING && d->character != ch &&
                    	can_see(ch, d->character) &&
                    	(d->character->in_room->wilds == pWilds ||
                    		(d->character->in_room->viewwilds == pWilds && IS_SET(d->character->in_room->room_flag[1],ROOM_VISIBLE_ON_MAP))) &&
                    	d->character->in_room->x == x &&
                    	d->character->in_room->y == y)
                    {
                        sprintf(temp, "{W@{x");
                        found = true;
                    }
                }

/* Vizz - if no PC found in the room, display the terrain char */
                if (!found)
                {
                    sprintf(j, "%c",pWilds->map[index]);
                    if (!str_cmp(j, last_terrain))
                    {
                        sprintf(temp, last_terrain);
                    }
                    else
                    {
/* Vizz - Search the terrain list linearly for now at least. could index this later for speed */
                        foundterrain = false;
                        for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
                        {
                            if (pWilds->map[index] == pTerrain->mapchar)
                            {
                                sprintf(temp, pTerrain->showchar);
                                sprintf(last_terrain, temp);
                                foundterrain = true;
                            }

                        }

                        if (!foundterrain)
                        {
                            /* Vizz - highlight vlink entrances */
                            if (!strcmp(j, "0"))
                                sprintf(temp, "{YO");
                            else
                                /* Vizz - allow for non-terrain defined characters - display verbatim */
                                sprintf(temp, j);
                        }
                    }
                }

                if (last_char_same
                    && (temp[2] != last_char
                        || temp[1] != last_colour_char))
                {
                    last_char_same = false;
                }

                if (temp[2] == last_char && temp[1] == last_colour_char)
                {
                     last_char_same = true;
                }

                if (last_char_same)
                {
                     sprintf(temp, "%c", temp[2]);
                }

                send_to_char(temp, to);

                if (olc)
                {
                    edit_mapstring[cString] = j[0];
                    cString++;
                }

                if (last_char_same)
                {
                    last_char = temp[0];
                }
                else
                {
                    last_char = temp[2];
                    last_colour_char = temp[1];
                }
            }
            else
            {
                /* If we're displaying outside the map bounds, fill in with starfield */
                if (!olc)
                    if (x % 5 + y % 6 == 0 && x % 2 + y % 3 == 0)
                    {
                        last_char = '.'; last_colour_char = 'x';
                        send_to_char("{x.", to);
                    }
                    else
                        send_to_char(" ", to);
                else
                    send_to_char(" ", to);
                    edit_mapstring[cString] = ' ';
                    cString++;
            }
        }

        if (olc)
        {
            char buf[81];

            edit_mapstring[cString] = '\0';
            sprintf(buf, "       {x%s{%c", edit_mapstring, last_colour_char);
            send_to_char(buf, to);
        }

        send_to_char("\n\r", to);
    }

    if (olc)
    {
        sprintf(blcoor, "(%d, %d)", vp_startx, vp_endy);
        sprintf(brcoor, "(%d, %d)", vp_endx, vp_endy);
        pad = squares_to_show_x * 2 + 7;

        for( i=0; i < pad ; i++ )
        {
           padding1[i] = ' ';
        }

        padding1[i] = 0;
        pad = ((squares_to_show_x * 2) - strlen(blcoor)) - strlen(brcoor);

        for( i=0; i < pad ; i++ )
        {
           padding2[i] = ' ';
        }

        padding2[i] = 0;
        sprintf(buf, "{x%s%s%s%s\n\r",
                padding1, blcoor, padding2, brcoor);
        send_to_char(buf, to);
    }

    send_to_char("{x", to);
    return;
}
#endif

void save_wilds (FILE * fp, AREA_DATA * pArea)
{
    WILDS_DATA *	pWilds;
    WILDS_TERRAIN *	pTerrain;
    WILDS_VLINK *       pVLink;
    int			y, j;

    if (pArea->wilds == NULL)
    {
        return;
    }

    for (pWilds = pArea->wilds;pWilds;pWilds = pWilds->next)
    {
        fprintf(fp, "#WILDS\n");
        fprintf(fp, "Uid %ld\n", pWilds->uid);
        fprintf(fp, "Name %s~\n", pWilds->name);
        fprintf(fp, "Repop %d~\n", pWilds->repop);
        if (pWilds->defaultPlaceFlags != PLACE_NOWHERE)
            fprintf(fp, "DefaultPlace '%s'\n", flag_string(place_flags, pWilds->defaultPlaceFlags));
        if (pWilds->defaultRegion >= REGION_UNKNOWN)
            fprintf(fp, "DefaultRegion '%s'\n", flag_string(wilderness_regions, pWilds->defaultRegion));
        fprintf(fp, "#VMAP %d %d\n", pWilds->map_size_x, pWilds->map_size_y);

        for (y = 0, j = 0; y < pWilds->map_size_y; y++, j+=pWilds->map_size_x)
        {
	    fwrite(pWilds->staticmap+j,pWilds->map_size_x,1,fp);

            if (y < (pWilds->map_size_y - 1))
                fprintf(fp, "\n");
            else
                fprintf(fp, "~\n");

        }

        fprintf(fp, "#-VMAP\n\n");

        for(pTerrain = pWilds->pTerrain;pTerrain;pTerrain = pTerrain->next)
            fwrite_terrain(fp, pTerrain);

        fprintf(fp, "\n");

        fwrite_region(fp, pWilds->pRegion);

        fprintf(fp, "\n");

        for(pVLink = pWilds->pVLink;pVLink != NULL;pVLink = pVLink->next)
        {
             fwrite_vlink(fp, pVLink);
        }

        fprintf(fp, "#-WILDS\n\n");
    }

    return;
}

void do_regions(CHAR_DATA *ch, char *argument)
{
    WILDS_REGION *pRegion;
    char buf[MSL];

    WILDS_DATA *pWilds = ch->in_wilds;

    if( is_number(argument) )
    {
		pWilds = get_wilds_from_uid(NULL, atol(argument));
		if( !pWilds )
		{
			send_to_char("Regions: That is not a wilderness map.\n\r", ch);
			return;
		}
	}
	else if (!ch->in_wilds)
    {
        plogf("wilds.c, do_regions(): ch->in_wilds invalid.");
        send_to_char("Regions: You don't appear to be in a wilderness map.\n\r", ch);
        return;
    }

    send_to_char("{w[{WRegions{w]{x\n\r\n\r", ch);

    if (pWilds->pRegion)
    {
        BUFFER *buffer = new_buf();

        add_buf(buffer, "     [Start X] [Start Y] [ End X ] [ End Y ] [      Region      ] [     Place     ]\n\r");
        add_buf(buffer, "====================================================================================\n\r");

        int i = 0;
        for(pRegion = pWilds->pRegion; pRegion; pRegion = pRegion->next)
        {
            sprintf(buf, "%4d  %7d   %7d   %7d   %7d   %-18s   %-15s\n\r", ++i,
                pRegion->startx, pRegion->starty,
                pRegion->endx, pRegion->endy,
                flag_string(wilderness_regions, pRegion->region),
                flag_string(place_flags, pRegion->area_place_flags));
            add_buf(buffer, buf);
        }

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
    else
        send_to_char("    None defined.\n\r", ch);
}

void do_vlinks(CHAR_DATA *ch, char *argument)
{
    WILDS_VLINK *pVLink;
    char buf[MSL];

    WILDS_DATA *pWilds = ch->in_wilds;

    if( is_number(argument) )
    {
		pWilds = get_wilds_from_uid(NULL, atol(argument));
		if( !pWilds )
		{
			send_to_char("Vlinks: That is not a wilderness map.\n\r", ch);
			return;
		}
	}
	else if (!ch->in_wilds)
    {
        plogf("wilds.c, do_vlinks(): ch->in_wilds invalid.");
        send_to_char("Vlinks: You don't appear to be in a wilderness map.\n\r", ch);
        return;
    }

    send_to_char("{w[{WVLinks{w]{x\n\r\n\r", ch);

    if (pWilds->pVLink)
    {
        BUFFER *buffer;

        buffer=new_buf();
        add_buf(buffer, "[   Uid] [x coor] [y coor] [direction] [    dest wnum    ] [default state] [current state]{x\n\r");
        for(pVLink=pWilds->pVLink;pVLink!=NULL;pVLink = pVLink->next)
        {
            sprintf(buf, "{x({W%6ld{x)  {W%6d   %6d   %9s   %8ld#%-8ld   %7s   %7s{x\n\r",
                           pVLink->uid,
                           pVLink->wildsorigin_x,
                           pVLink->wildsorigin_y,
                           dir_name[pVLink->door],
                           pVLink->destwnum.auid,
                           pVLink->destwnum.vnum,
                           (IS_SET(pVLink->default_linkage, VLINK_TO_WILDS) &&
			    IS_SET(pVLink->default_linkage, VLINK_FROM_WILDS)) ? "two-way" :
			        IS_SET(pVLink->default_linkage, VLINK_TO_WILDS) ? "to wilds" :
			            IS_SET(pVLink->default_linkage, VLINK_FROM_WILDS) ? "from wilds" :
                                        "not set",
                           (IS_SET(pVLink->current_linkage, VLINK_TO_WILDS) &&
			    IS_SET(pVLink->current_linkage, VLINK_FROM_WILDS)) ? "two-way" :
			        IS_SET(pVLink->current_linkage, VLINK_TO_WILDS) ? "to wilds" :
			            IS_SET(pVLink->current_linkage, VLINK_FROM_WILDS) ? "from wilds" :
                                        "not set");
            add_buf(buffer, buf);
        }

        page_to_char(buf_string(buffer), ch);
        free_buf(buffer);

    }
    else
        send_to_char("    None defined.\n\r", ch);

    if (!IS_SET(ch->comm, COMM_COMPACT))
    {
        send_to_char("\n\r", ch);
    }

    return;
}

WILDS_DATA *new_wilds (void)
{
    static WILDS_DATA pwilds_zero;
    WILDS_DATA *pWilds;

    if(!wilds_free)
    {
        pWilds = alloc_perm (sizeof (*pWilds));
        top_wilds++;
    }
    else
    {
        pWilds = wilds_free;
        wilds_free = wilds_free->next;
    }

    // Initialise wilds_data structure in memory referenced by pWilds pointer
    *pWilds = pwilds_zero;

    pWilds->next = NULL;
    pWilds->uid = 0; /* Vizz - UID 0 is invalid - helps us know when to set the UID */
    pWilds->name = str_dup("New Wilds");
    pWilds->map = &str_empty[0];
    pWilds->staticmap = &str_empty[0];
    pWilds->map_size_x = 0;
    pWilds->map_size_y = 0;
    pWilds->startx = 0;
    pWilds->starty = 0;
    pWilds->pTerrain = NULL;
    pWilds->cDefaultTerrain = 'S'; // Arbitrary default terrain char
    pWilds->defaultRegion = REGION_UNKNOWN;
    pWilds->defaultPlaceFlags = PLACE_NOWHERE;
    pWilds->pVLink = NULL;
    pWilds->pRegion = NULL;
//    pWilds->char_matrix = NULL;
//    pWilds->obj_matrix = NULL;
	pWilds->loaded_rooms = 0;
	pWilds->loaded_vrooms = list_create(false);
    VALIDATE (pWilds);

    return pWilds;
}



void free_wilds (WILDS_DATA * pWilds)
{
    WILDS_VLINK *pVLink, *pVLink_next;
    WILDS_TERRAIN *pTerrain, *pTerrain_next;
    WILDS_REGION *pRegion, *pRegion_next;

    if (!IS_VALID (pWilds))
        return;

    free_string (pWilds->name);

/* Vizz - calloc'ed, to avoid MAX_PERM_BLOCK having to be huge
 *        if we were using alloc_perm and free_string
 */
    free(pWilds->map);
    free(pWilds->staticmap);

    if (pWilds->pVLink)
    {
// Loop thru VLinks list and free all indirect structures
        for(pVLink = pWilds->pVLink;pVLink != NULL;pVLink = pVLink_next)
        {
            pVLink_next = pVLink->next;
            free_vlink (pVLink);
        }
    }

    if (pWilds->pTerrain)
    {
// Loop thru Terrain list and free all indirect structures
        for(pTerrain = pWilds->pTerrain;pTerrain != NULL;pTerrain = pTerrain_next)
        {
            pTerrain_next = pTerrain->next;
            free_terrain (pTerrain);
        }
    }

    if (pWilds->pRegion)
    {
        for(pRegion = pWilds->pRegion;pRegion != NULL;pRegion = pRegion_next)
        {
            pRegion_next = pRegion->next;
            free_region (pRegion);
        }
    }

    pWilds->pVLink = NULL;
    pWilds->pTerrain = NULL;
    list_destroy(pWilds->loaded_vrooms);
    INVALIDATE (pWilds);

    pWilds->next = wilds_free->next;
    wilds_free = pWilds;
    return;
}

void char_to_vroom (CHAR_DATA *ch, WILDS_DATA *pWilds, int x, int y)
{
    ROOM_INDEX_DATA *room;
    OBJ_DATA *obj;

    // Check arguments are valid.
    if (pWilds == NULL)
    {
        plogf ("wilds.c, char_to_vroom(): pWilds is NULL.");

	// No wilds pointer, so send the char to the default room.
        if (room_index_default != NULL)
        {
            char_to_room (ch, room_index_default);
            return;
        }

	// Just in case...
        plogf("wilds.c, char_to_vroom(): Default room could not be found!");
        return;
    }

	// Dude... mounts? :P
    if (MOUNTED(ch) && MOUNTED(ch)->in_room == ch->in_room
    && MOUNTED(ch)->in_room == NULL)
	char_to_vroom(MOUNTED(ch), pWilds, x, y);


    ch->in_wilds = pWilds;
    ch->at_wilds_x = x;
    ch->at_wilds_y = y;

	// Check if there's a loaded vroom for them, or load one up.
	if(!(room = get_wilds_vroom(pWilds, x, y)))
	    room = create_wilds_vroom(pWilds, x, y);

	ch->in_room = room;
	ch->next_in_room = room->people;

	list_addlink(room->lpeople, ch);
	list_addlink(room->lentity, ch);

	room->people = ch;

    // Is the character a player?
    if (!IS_NPC (ch))
    {
        if (ch->in_room->area->empty)
        {
            ch->in_room->area->empty = false;
            ch->in_room->area->age = 0;
        }

        ++ch->in_room->area->nplayer;

        if (ch->in_wilds)
        {
            if (ch->in_wilds->empty)
            {
                ch->in_wilds->empty = false;
                ch->in_wilds->age = 0;
            }

            ++ch->in_wilds->nplayer;
        }

    }
    else
        ++ch->in_wilds->loaded_mobs;

    if ((obj = get_eq_char (ch, WEAR_LIGHT)) != NULL
        && obj->item_type == ITEM_LIGHT && obj->value[2] != 0)
        ++ch->in_room->light;

    if (IS_AFFECTED (ch, AFF_PLAGUE))
    {
        AFFECT_DATA *af, plague;
        CHAR_DATA *vch;

        for (af = ch->affected; af != NULL; af = af->next)
        {
            if (af->skill == gsk_plague)
                break;
        }

        if (af == NULL)
        {
            REMOVE_BIT (ch->affected_by[0], AFF_PLAGUE);
            return;
        }

        if (af->level == 1)
            return;

		plague.slot	= WEAR_NONE;
        plague.where = TO_AFFECTS;
		plague.custom_name = NULL;
        plague.group = af->group;
        plague.catalyst_type = -1;
        plague.skill = gsk_plague;
        plague.level = af->level - 1;
        plague.duration = number_range (1, 2 * plague.level);
        plague.location = APPLY_STR;
        plague.modifier = -5;
        plague.bitvector = AFF_PLAGUE;
        plague.bitvector2	= 0;

        for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
        {
            if (!saves_spell (plague.level - 2, vch, DAM_DISEASE)
                && !IS_IMMORTAL (vch) &&
                !IS_AFFECTED (vch, AFF_PLAGUE) && number_bits (6) == 0)
            {
                send_to_char ("You feel hot and feverish.\n\r", vch);
                act ("$n shivers and looks very ill.", vch, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
                affect_join (vch, &plague);
            }
        }
    }

    return;
}

void add_vlink (WILDS_DATA *pWilds, WILDS_VLINK *pVLink)
{
    /*
     * Check pointers are valid
     */
    if (!pWilds)
    {
        plogf("wilds.c, add_vlink(): Invalid pWilds pointer.");
        abort();
    }

    if (!pVLink)
    {
        plogf("wilds.c, add_vlink(): Invalid pVLink pointer.");
        abort();
    }

	pVLink->pWilds = pWilds;
	pVLink->next = pWilds->pVLink;
	pWilds->pVLink = pVLink;

    return;
}

WILDS_VLINK *new_vlink ()
{
    static WILDS_VLINK pvlink_zero;
    WILDS_VLINK *pVLink;

    if(!wilds_vlink_free)
    {
        pVLink = alloc_perm (sizeof (*pVLink));
        top_wilds_vlink++;
    }
    else
    {
        pVLink = wilds_vlink_free;
        wilds_vlink_free = wilds_vlink_free->next;
    }

    *pVLink = pvlink_zero;

    pVLink->next = NULL;
    pVLink->pWilds = NULL;
    pVLink->orig_pick = 100;
    pVLink->rev_pick = 100;

    VALIDATE (pVLink);

    return pVLink;
}



void free_vlink (WILDS_VLINK *pVLink)
{
    if (!IS_VALID(pVLink))
        return;

    free_string (pVLink->orig_description);
    free_string (pVLink->orig_keyword);
    free_string (pVLink->rev_description);
    free_string (pVLink->rev_keyword);
    INVALIDATE (pVLink);

    pVLink->next = wilds_vlink_free;
    wilds_vlink_free = pVLink;
    return;
}

bool add_terrain (WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain)
{
    if (!IS_VALID(pTerrain))
        return false;

    if (pWilds->pTerrain != NULL)
    {
        /* Point back from the previous to the new list head struct */
        pWilds->pTerrain->prev = pTerrain;

        /* Point forward from the new to the previous list head struct */
        pTerrain->next = pWilds->pTerrain;

        /* Put new struct in list */
        pWilds->pTerrain = pTerrain;

        return true;
    }
    else
    {
        /* Put new struct in list */
        pWilds->pTerrain = pTerrain;
        return true;
    }
}

bool del_terrain (WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain)
{
    WILDS_TERRAIN *prev_pTerrain, *next_pTerrain;

    if (!IS_VALID(pTerrain))
        return false;

    prev_pTerrain = pTerrain->prev;
    next_pTerrain = pTerrain->next;

    if (prev_pTerrain)
    {
        plogf("prev_pTerrain->mapchar: '%c'", prev_pTerrain->mapchar);
        prev_pTerrain->next = next_pTerrain;
    }

    if (next_pTerrain)
    {
        plogf("next_pTerrain->mapchar: '%c'", next_pTerrain->mapchar);
        next_pTerrain->prev = prev_pTerrain;
    }

    plogf("pTerrain->mapchar: '%c'", pTerrain->mapchar);

    if (pWilds->pTerrain == pTerrain)
        pWilds->pTerrain = next_pTerrain;

    free_terrain(pTerrain);
    return true;
}

WILDS_TERRAIN *new_terrain (WILDS_DATA *pWilds)
{
    static WILDS_TERRAIN pterrain_zero;
    WILDS_TERRAIN *pTerrain;

    if(!wilds_terrain_free)
    {
        pTerrain = alloc_perm (sizeof (*pTerrain));
        top_wilds_terrain++;
    }
    else
    {
        pTerrain = wilds_terrain_free;
        wilds_terrain_free = wilds_terrain_free->next;
    }

    *pTerrain = pterrain_zero;

    pTerrain->prev = NULL;
    pTerrain->next = NULL;
    pTerrain->showname = NULL;
    pTerrain->briefdesc = NULL;
    pTerrain->template = new_room_index();
    pTerrain->pWilds = pWilds;
    VALIDATE (pTerrain);

    return pTerrain;
}

void free_terrain (WILDS_TERRAIN *pTerrain)
{
    if (!IS_VALID(pTerrain))
        return;

    free_string (pTerrain->showchar);
    free_room_index (pTerrain->template);
    pTerrain->pWilds = NULL;
    INVALIDATE (pTerrain);

    pTerrain->prev = NULL;
    pTerrain->next = wilds_terrain_free;
    wilds_terrain_free = pTerrain;
    return;
}

bool add_region (WILDS_DATA *pWilds, WILDS_REGION *pRegion)
{
    if (!IS_VALID(pRegion))
        return false;

    if (pWilds->pRegion != NULL)
    {
        /* Point back from the previous to the new list head struct */
        pWilds->pRegion->prev = pRegion;

        /* Point forward from the new to the previous list head struct */
        pRegion->next = pWilds->pRegion;

        /* Put new struct in list */
        pWilds->pRegion = pRegion;

        return true;
    }
    else
    {
        /* Put new struct in list */
        pWilds->pRegion = pRegion;
        return true;
    }
}

bool del_region (WILDS_DATA *pWilds, WILDS_REGION *pRegion)
{
    WILDS_REGION *prev_pRegion, *next_pRegion;

    if (!IS_VALID(pRegion))
        return false;

    prev_pRegion = pRegion->prev;
    next_pRegion = pRegion->next;

    if (prev_pRegion)
    {
        prev_pRegion->next = next_pRegion;
    }

    if (next_pRegion)
    {
        next_pRegion->prev = prev_pRegion;
    }

    if (pWilds->pRegion == pRegion)
        pWilds->pRegion = next_pRegion;

    free_region(pRegion);
    return true;
}

WILDS_REGION *new_region (WILDS_DATA *pWilds)
{
    static WILDS_REGION pregion_zero;
    WILDS_REGION *pRegion;

    if(!wilds_region_free)
    {
        pRegion = alloc_perm (sizeof (*pRegion));
    }
    else
    {
        pRegion = wilds_region_free;
        wilds_region_free = wilds_region_free->next;
    }

    *pRegion = pregion_zero;

    pRegion->prev = NULL;
    pRegion->next = NULL;
    pRegion->pWilds = pWilds;
    pRegion->region = REGION_UNKNOWN;
    pRegion->area_place_flags = PLACE_NOWHERE;
    VALIDATE (pRegion);

    return pRegion;
}

void free_region (WILDS_REGION *pRegion)
{
    if (!IS_VALID(pRegion))
        return;

    pRegion->pWilds = NULL;
    INVALIDATE (pRegion);

    pRegion->prev = NULL;
    pRegion->next = wilds_region_free;
    wilds_region_free = pRegion;
    return;
}


void link_vlinks (WILDS_DATA *pWilds)
{
    ROOM_INDEX_DATA *pRevLinkRoomIndex;
    WILDS_VLINK *pVLink = NULL;

    if (pWilds == NULL)
    {
        plogf("wilds.c, link_vlinks(): Failed to link vlinks - pWilds pointer is NULL");
        return;
    }

    if (pWilds->pVLink == NULL)
    {
        plogf("wilds.c, link_vlinks(): No Vlinks found.");
        return;
    }

    for (pVLink = pWilds->pVLink;pVLink;pVLink = pVLink->next)
    {
        if ((pRevLinkRoomIndex = get_room_index_auid (pVLink->destwnum.auid, pVLink->destwnum.vnum)) == NULL)
        {
            plogf("wilds.c, apply_vlink(): destwnum %ld#%ld does not exist.", pVLink->destwnum.auid, pVLink->destwnum.vnum);
            continue;
        }

        if (IS_SET(pRevLinkRoomIndex->room_flag[1], ROOM_BLUEPRINT) ||
        	IS_SET(pRevLinkRoomIndex->area->area_flags, AREA_BLUEPRINT))
        {
            plogf("wilds.c, apply_vlink(): destwnum %ld#%ld involved in blueprints.", pVLink->destwnum.auid, pVLink->destwnum.vnum);
            continue;
		}

        if (IS_SET(pVLink->default_linkage, (VLINK_FROM_WILDS|VLINK_TO_WILDS)))
        {
            if (pVLink->wildsorigin_x >= 0 && pVLink->wildsorigin_y >= 0
                && pVLink->wildsorigin_x < pWilds->map_size_x
                && pVLink->wildsorigin_y < pWilds->map_size_y)
            {
				if (link_vlink(pVLink))
					plogf("link_vlinks(): VLink %s from (%d, %d) to %ld#%ld Linked Successfully.",
						dir_name[pVLink->door],
						pVLink->wildsorigin_x,
						pVLink->wildsorigin_y,
						pVLink->destwnum.auid,
                        pVLink->destwnum.vnum);
				else
					plogf("wilds.c, link_vlinks(): VLink failed.");
				continue;
            }
            else
                plogf("wilds.c, link_vlinks(): VLink failed - coordinates are invalid.");

			continue;
        }
    } /* end for */

    return;
}

WILDS_DATA *get_wilds_from_uid (AREA_DATA *pArea, long uid)
{

    WILDS_DATA *pWilds;

//    printf("get_wilds_from_uid (%08X[%s],%ld)\n\r", pArea, (pArea?pArea->file_name:"(null)"), uid);

    // Vizz - If a NULL pArea pointer is passed, search all areas (slower)
    if (!pArea)
    {
        for (pArea = area_first; pArea; pArea = pArea->next)
        {
//	    printf("get_wilds_from_uid -> checking %08X[%s]\n\r", pArea, (pArea?pArea->file_name:"(null)"));
            for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
            {
                if (pWilds->uid == uid)
                    return (pWilds);
            }
        }
    }
    else
    {
        // Vizz - Search the supplied area (faster for when area is known)
        for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
        {
            if (pWilds->uid == uid)
                return (pWilds);
        }
    }

    plogf("wilds.c, get_wilds_from_uid(): Could not find wilds pointer from uid.");
    return (NULL);
}

WILDS_VLINK *get_vlink_from_uid (WILDS_DATA *pWilds, long uid)
{

    AREA_DATA *pArea;
    WILDS_VLINK *pVLink;

    // Vizz - If a NULL pWilds pointer is passed, search all wilds (slower)
    if (!pWilds)
    {
        for (pArea = area_first; pArea; pArea = pArea->next)
        {
            for (pWilds = pArea->wilds; pWilds; pWilds = pWilds->next)
            {
                for (pVLink = pWilds->pVLink; pVLink; pVLink = pVLink->next)
                {
                    if (pVLink->uid == uid)
                        return (pVLink);
                }
            }
        }
    }
    else
    {
        // Vizz - Search the supplied Wilds (faster for when wilds is known)
        for (pVLink = pWilds->pVLink; pVLink; pVLink = pVLink->next)
        {
            if (pVLink->uid == uid)
                return (pVLink);
        }
    }

    plogf("wilds.c, get_vlink_from_uid(): Could not find vlink pointer from uid.");
    return (NULL);
}

WILDS_VLINK *get_vlink_from_index (WILDS_DATA *pWilds, long index)
{
	WILDS_VLINK *pVLink;
	long idx;

	if(!pWilds) return NULL;

	for (idx = 0, pVLink = pWilds->pVLink; pVLink && idx < index; idx++, pVLink = pVLink->next);

	return pVLink;
}

CHAR_DATA *get_people_from_wilds(WILDS_DATA *pWilds, int x, int y)
{
	ROOM_INDEX_DATA *room;
	register CHAR_DATA *people;

	if ((room = get_wilds_vroom(pWilds, x, y)))
		return room->people;
	else {
		for(people = pWilds->char_list; people; people = people->next_in_wilds) {
			if(people->at_wilds_x == x && people->at_wilds_y == y) return people;

			if(people->at_wilds_y > y || (people->at_wilds_y == y && people->at_wilds_x > x)) break;
		}
	}

	return NULL;
}

OBJ_DATA *get_contents_from_wilds(WILDS_DATA *pWilds, int x, int y)
{
	ROOM_INDEX_DATA *room;
	register OBJ_DATA *obj;

	if ((room = get_wilds_vroom(pWilds, x, y)))
		return room->contents;
	else {
		for(obj = pWilds->obj_list; obj; obj = obj->next_in_wilds) {
			if(obj->x == x && obj->y == y) return obj;

			if(obj->y > y || (obj->y == y && obj->x > x)) break;

		}
	}

	return NULL;
}

bool link_contents_wilds(WILDS_DATA *pWilds, int x, int y, OBJ_DATA *contents)
{
	register OBJ_DATA *prev, *obj;

	if(!contents) return false;

	for(prev = NULL, obj = pWilds->obj_list; obj; prev = obj, obj = obj->next_in_wilds) {
		if(obj->x == x && obj->y == y) return false;

		if(obj->y > y || (obj->y == y && obj->x > x)) break;
	}

	if(prev) {
		prev->next_in_wilds = contents;
		contents->prev_in_wilds = prev;
	} else if(pWilds->obj_list) {
		pWilds->obj_list = contents;
		contents->prev_in_wilds = NULL;
	}
	if(obj) {
		obj->prev_in_wilds = contents;
		contents->next_in_wilds = obj;
	}

	return true;
}

OBJ_DATA *unlink_contents_wilds(WILDS_DATA *pWilds, int x, int y)
{
	register OBJ_DATA *obj;

	for(obj = pWilds->obj_list; obj; obj = obj->next_in_wilds) {
		if(obj->x == x && obj->y == y) {
			// Change A->B->C to A->C
			// or
			// Change HEAD(B)->C to HEAD(C)
			if(obj->prev_in_wilds)
				obj->prev_in_wilds->next_in_wilds = obj->next_in_wilds;
			else
				pWilds->obj_list = obj->next_in_wilds;

			// Change A<-B<-C to A<-C
			// or
			// nothing (B is the TAIL)
			if(obj->next_in_wilds)
				obj->next_in_wilds->prev_in_wilds = obj->prev_in_wilds;

			obj->prev_in_wilds = obj->next_in_wilds = NULL;
			return obj;
		}

		if(obj->y > y || (obj->y == y && obj->x > x)) return NULL;
	}

	return NULL;
}

// This will search the rooms
ROOM_INDEX_DATA *wilds_seek_down(register WILDS_DATA *wilds, register int x, register int y, register int z, bool ground)
{
    register AREA_DATA *area;
	register ROOM_INDEX_DATA *room, *highest = NULL, *vroom;
	register int i;

    for(area = area_first; area; area = area->next)
    {
        for(i = 0; i < MAX_KEY_HASH; i++)
        {
            for(room = area->room_index_hash[i]; room; room = room->next) {
                if((room->wilds == wilds || room->viewwilds == wilds) && room->x == x && room->y == y && room->z <= z) {
                    if(!highest || (room->z > highest->z)) highest = room;
                }
            }
        }
    }

	// Limit the search to above ground...
	if(ground) {
		vroom = get_wilds_vroom(wilds, x, y);
		if(!vroom) vroom = create_wilds_vroom(wilds, x, y);

		if(highest->z < vroom->z)
			highest = vroom;
	}

	return highest;
}

void do_wlist(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    ITERATOR iter;
	LLIST_WILDS_DATA *data;
    WILDS_DATA *pWilds;
    BUFFER *buffer;
    //int place_type = 0;

    buffer = new_buf();

    sprintf(buf, "[%-7s] [%-26.26s] [%15s] [%-10s]\n\r", "UID", "Name", "Dimensions", "Area");
    add_buf(buffer, buf);


	iterator_start(&iter, loaded_wilds);
	while((data = (LLIST_WILDS_DATA *)iterator_nextdata(&iter)))
	{
		pWilds = data->wilds;
		sprintf(buf,"[%7ld] [%-26.26s] [ %5d x %-5d ] %s\n\r", pWilds->uid,
			(IS_NULLSTR(pWilds->name) ? "no name" : pWilds->name),
			pWilds->map_size_x, pWilds->map_size_y,
			((!IS_NULLSTR(pWilds->pArea->name)) ? pWilds->pArea->name : ""));
		add_buf(buffer, buf);
	}
	iterator_stop(&iter);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


