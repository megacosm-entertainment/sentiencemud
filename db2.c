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
#include "strings.h"
#include "merc.h"
#include "skill_group.h"
#include "db.h"
#include "tables.h"
#include "io/json/json_socials.h"
#include "utils/localization.h"


int social_count;
struct social_type	social_table		[MAX_SOCIALS];


void load_socials(FILE *fp)
{
    social_count = 0;
    
    for (;;) {
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
        if (!strcmp(temp, "#0"))
            return;  /* done */
            
#if defined(social_debug)
        else
            printf("%s\n\r", temp);
#endif

        strcpy(social.name, temp);
        fread_to_eol(fp);

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.char_no_arg = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.char_no_arg = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.others_no_arg = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.others_no_arg = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.char_found = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.char_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.others_found = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.others_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.vict_found = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.vict_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.char_not_found = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.char_not_found = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.char_auto = NULL;
        else if (!strcmp(temp, "#")) {
            social_table[social_count] = social;
            social_count++;
            continue;
        }
        else
            social.char_auto = temp;

        temp = fread_string_eol(fp);
        if (!strcmp(temp, "$"))
            social.others_auto = NULL;
        else if (!strcmp(temp, "#")) {
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

/*
 * Try loading socials from new format first, then fall back to old format
 */
void load_socials_file(void)
{
    FILE *fp;
    bool loaded_new = false;
    char socials_file_buf[MAX_INPUT_LENGTH];
    char old_socials_file_buf[MAX_INPUT_LENGTH];
    const char *socials_file = resolve_game_path(SOCIALS_FILE, socials_file_buf, sizeof(socials_file_buf));
    const char *old_socials_file = resolve_game_path(OLD_SOCIALS_FILE, old_socials_file_buf, sizeof(old_socials_file_buf));

    social_count = 0;

    // First try loading from JSON format
    if (json_load_socials(SOCIALS_JSON_FILE)) {
        log_string(formatf("Loaded %d socials.", social_count));
        return;
    }

    // Try loading from the legacy dat format
    if ((fp = fopen(socials_file, "r")) != NULL) {
        log_string("Loading socials from legacy format...");
        loaded_new = load_new_socials(fp);
        fclose(fp);
    }

    // If dat format loading failed or file doesn't exist, try old .are format
    if (!loaded_new) {
        log_string("Legacy socials not found or invalid, trying old format...");
        if ((fp = fopen(old_socials_file, "r")) != NULL) {
            log_string("Loading socials from old format...");

            // Skip ahead to the #SOCIALS section
            while (!feof(fp)) {
                char *word = fread_word(fp);
                if (!str_cmp(word, "#SOCIALS")) {
                    break;
                }
            }

            load_socials(fp);
            fclose(fp);
        } else {
            pbugf(LOG_ERROR, "Could not find any socials file!");
        }
    }

    // Migrate to JSON format
    if (social_count > 0)
        json_save_socials(SOCIALS_JSON_FILE);

    log_string(formatf("Loaded %d socials.", social_count));
}

/*
 * Load socials from the new format file
 */
bool load_new_socials(FILE *fp)
{
    char *word;
    bool in_social = false;
    struct social_type social;
    int count = 0;
    
    // Make sure we're at the beginning of the file
    rewind(fp);

    while (!feof(fp)) {
        word = fread_word(fp);
        
        if (feof(fp))
            break;
            
        // Check for end of socials section
        if (!str_cmp(word, "#END"))
            break;
            
        if (!str_cmp(word, "#SOCIAL")) {
            // Clear the social structure
            memset(&social, 0, sizeof(struct social_type));
            social.char_no_arg = NULL;
            social.others_no_arg = NULL;
            social.char_found = NULL;
            social.others_found = NULL;
            social.vict_found = NULL;
            social.char_not_found = NULL;
            social.char_auto = NULL;
            social.others_auto = NULL;
            
            // Get the social name
            if (!feof(fp))
                strcpy(social.name, fread_string(fp));
            else
                break;
                
            in_social = true;
        }
        else if (!str_cmp(word, "#-SOCIAL")) {
            if (in_social) {
                social_table[count] = social;
                count++;
                in_social = false;
            }
        }
        else if (in_social) {
            if (feof(fp))
                break;
                
            if (!str_cmp(word, "Enabled")) {
                /* Skip the enabled flag - not currently used */
                fread_number(fp);
            }
            else if (!str_cmp(word, "CharNoArg")) {
                if (!feof(fp))
                    social.char_no_arg = fread_string(fp);
            }
            else if (!str_cmp(word, "OthersNoArg")) {
                if (!feof(fp))
                    social.others_no_arg = fread_string(fp);
            }
            else if (!str_cmp(word, "CharFound")) {
                if (!feof(fp))
                    social.char_found = fread_string(fp);
            }
            else if (!str_cmp(word, "OthersFound")) {
                if (!feof(fp))
                    social.others_found = fread_string(fp);
            }
            else if (!str_cmp(word, "VictFound")) {
                if (!feof(fp))
                    social.vict_found = fread_string(fp);
            }
            else if (!str_cmp(word, "CharNotFound")) {
                if (!feof(fp))
                    social.char_not_found = fread_string(fp);
            }
            else if (!str_cmp(word, "CharAuto")) {
                if (!feof(fp))
                    social.char_auto = fread_string(fp);
            }
            else if (!str_cmp(word, "OthersAuto")) {
                if (!feof(fp))
                    social.others_auto = fread_string(fp);
            }
            else {
                // Skip unknown fields
                if (!feof(fp))
                    fread_string(fp);
            }
        }
    }
    
    if (count > 0) {
        social_count = count;
        return true;
    }
    
    return false;
}

/*
 * Save socials in the new format
 */
void save_new_socials(void)
{
    json_save_socials(SOCIALS_JSON_FILE);
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

        if (!gq_mob->vnum_wnum.pArea && gq_mob->vnum_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(gq_mob->vnum_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&gq_mob->vnum_load, &gq_mob->vnum_wnum, fallback);
        }
        ch = create_mobile(get_mob_index(gq_mob->vnum_wnum.pArea, gq_mob->vnum_wnum.vnum), false);
        if ( gq_mob->obj_wnum.vnum != 0 )
        {
        if (!gq_mob->obj_wnum.pArea && gq_mob->obj_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(gq_mob->obj_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&gq_mob->obj_load, &gq_mob->obj_wnum, fallback);
        }
        OBJ_INDEX_DATA *obj_index = get_obj_index(gq_mob->obj_wnum.pArea, gq_mob->obj_wnum.vnum);
        obj = create_object( obj_index, obj_index->level, false);
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
        if (!gq_obj->vnum_wnum.pArea && gq_obj->vnum_load.vnum > 0) {
            AREA_DATA *fallback = NULL;
            WNUM wnum;
            if (resolve_widevnum(gq_obj->vnum_load.vnum, NULL, &wnum))
                fallback = wnum.pArea;
            if (!fallback) fallback = get_system_area_fallback();
            resolve_wnum_load(&gq_obj->vnum_load, &gq_obj->vnum_wnum, fallback);
        }
        obj = create_object(get_obj_index(gq_obj->vnum_wnum.pArea, gq_obj->vnum_wnum.vnum), 1, false);
        obj_to_room( obj, room );
        }
    }
    }
}


/* Get a random area. 1 - first continent, 2 - second continent, 0 - either */
/* FIXED VERSION - builds array of valid areas to avoid infinite loop */
AREA_DATA *get_random_area( CHAR_DATA *ch, int continent, bool no_get_random )
{
    int count, pick;
    AREA_DATA *area;
    AREA_DATA **valid_areas;
    int max_areas = 0;

    if( continent < MIN_CONTINENT || continent > MAX_CONTINENT ) return NULL;

    /* Count total areas for array allocation */
    for ( area = area_first; area != NULL; area = area->next )
        max_areas++;
    
    if (max_areas == 0) return NULL;

    /* Allocate temporary array to hold valid area pointers */
    valid_areas = alloc_mem(sizeof(AREA_DATA*) * max_areas);
    if (!valid_areas) return NULL;
    
    count = 0;

    /* Build list of valid areas based on continent filter */
    for (area = area_first; area != NULL; area = area->next)
    {
        bool matches = false;
        
        /* Check if area passes all common filters */
        if (!area->open ||
            !is_area_unlocked(ch, area) ||
            (no_get_random && IS_SET(area->area_flags, AREA_NO_GET_RANDOM)) ||
            !str_infix("Housing", area->name) ||
            !str_infix("Arena", area->name) ||
            !str_infix("Temples", area->name) ||
            !str_infix("Maze", area->name))
        {
            continue;
        }
        
        /* Check continent-specific filters */
        switch (continent)
        {
            case FIRST_CONTINENT:
                matches = (area->place_flags == PLACE_FIRST_CONTINENT);
                break;
                
            case SECOND_CONTINENT:
                matches = (area->place_flags == PLACE_SECOND_CONTINENT);
                break;
                
            case THIRD_CONTINENT:
                matches = (area->place_flags == PLACE_THIRD_CONTINENT);
                break;
                
            case FOURTH_CONTINENT:
                matches = (area->place_flags == PLACE_FOURTH_CONTINENT);
                break;
                
            case NORTH_CONTINENTS:
                matches = (area->place_flags == PLACE_FIRST_CONTINENT ||
                          area->place_flags == PLACE_FOURTH_CONTINENT);
                break;
                
            case SOUTH_CONTINENTS:
                matches = (area->place_flags == PLACE_SECOND_CONTINENT ||
                          area->place_flags == PLACE_THIRD_CONTINENT);
                break;
                
            case WEST_CONTINENTS:
                matches = (area->place_flags == PLACE_FIRST_CONTINENT ||
                          area->place_flags == PLACE_THIRD_CONTINENT);
                break;
                
            case EAST_CONTINENTS:
                matches = (area->place_flags == PLACE_SECOND_CONTINENT ||
                          area->place_flags == PLACE_FOURTH_CONTINENT);
                break;
                
            default:  /* All continents */
                matches = (area->place_flags == PLACE_FIRST_CONTINENT ||
                          area->place_flags == PLACE_SECOND_CONTINENT ||
                          area->place_flags == PLACE_THIRD_CONTINENT ||
                          area->place_flags == PLACE_FOURTH_CONTINENT);
                break;
        }
        
        if (matches)
        {
            valid_areas[count++] = area;
        }
    }

    /* Pick a random area from the valid list */
    if (count > 0)
    {
        pick = number_range(0, count - 1);
        area = valid_areas[pick];
    }
    else
    {
        area = NULL;
    }

    free_mem(valid_areas, sizeof(AREA_DATA*) * max_areas);
    return area;
}

OBJ_DATA *get_random_obj_area( CHAR_DATA *ch, AREA_DATA *area, ROOM_INDEX_DATA *room)
{
    int hash;
    int count = 0;
    int nth;
    OBJ_INDEX_DATA *oIndex;
    OBJ_INDEX_DATA *selected = NULL;
    OBJ_DATA *obj = NULL;

    if (area == NULL)
        return NULL;

    for (hash = 0; hash < MAX_KEY_HASH; hash++)
    {
        for (oIndex = area->obj_index_hash[hash]; oIndex != NULL; oIndex = oIndex->next)
        {
            if ( IS_SET( oIndex->wear_flags, ITEM_TAKE ) &&
                !IS_SET( oIndex->wear_flags, ITEM_NO_SAC ) &&
                !IS_SET( oIndex->extra[1], ITEM_NOQUEST ) &&
                !IS_SET( oIndex->extra[0], ITEM_MELT_DROP ) &&
                oIndex->item_type != ITEM_MONEY )
                count++;
        }
    }

    if (count < 1)
        return NULL;

    nth = number_range(1, count);

    for (hash = 0; hash < MAX_KEY_HASH && !selected; hash++)
    {
        for (oIndex = area->obj_index_hash[hash]; oIndex != NULL; oIndex = oIndex->next)
        {
            if ( IS_SET( oIndex->wear_flags, ITEM_TAKE ) &&
                !IS_SET( oIndex->wear_flags, ITEM_NO_SAC ) &&
                !IS_SET( oIndex->extra[1], ITEM_NOQUEST ) &&
                !IS_SET( oIndex->extra[0], ITEM_MELT_DROP ) &&
                oIndex->item_type != ITEM_MONEY &&
                --nth == 0 )
            {
                selected = oIndex;
                break;
            }
        }
    }

    if (selected == NULL)
        return NULL;

    if (room != NULL) {
        obj = create_object(selected, selected->level, true);
        obj_to_room(obj, room);
    } else
        obj = NULL;

    return obj;
}

/* get a random obj for a quest */
OBJ_DATA *get_random_obj( CHAR_DATA *ch, int continent )
{
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;
    int tries;

    room = get_random_room(ch, continent);

    for (tries = 0; tries < 200; tries++)
    {
        area = get_random_area(ch, continent, true);

        if ( IS_SET(area->area_flags,AREA_NO_GET_RANDOM) )
            continue;

        return get_random_obj_area(ch, area, room);
    }

    return NULL;
}

CHAR_DATA *get_random_mob_area( CHAR_DATA *ch, AREA_DATA *area)
{
    int hash;
    int mob_count = 0;
    int nth;
    MOB_INDEX_DATA *mIndex;
    CHAR_DATA *mob = NULL;
    int attempts;

    if (area == NULL)
        return NULL;

    for (hash = 0; hash < MAX_KEY_HASH; hash++)
        for (mIndex = area->mob_index_hash[hash]; mIndex != NULL; mIndex = mIndex->next)
            mob_count++;

    if (mob_count < 1)
        return NULL;

    for (attempts = 0; attempts < 1000; attempts++)
    {
        nth = number_range(1, mob_count);
        mIndex = NULL;
        for (hash = 0; hash < MAX_KEY_HASH && !mIndex; hash++)
        {
            MOB_INDEX_DATA *candidate;
            for (candidate = area->mob_index_hash[hash]; candidate != NULL; candidate = candidate->next)
            {
                if (--nth == 0)
                {
                    mIndex = candidate;
                    break;
                }
            }
        }

    if ( mIndex == NULL )
        continue;
    else
    {
        if (IS_SET(mIndex->act[0], ACT_PROTECTED)
        || IS_SET(mIndex->act[0], ACT_MOUNT)
        || IS_SET(mIndex->act[0], ACT_PET)
        || IS_SET(mIndex->act[0], ACT_TRAIN)
        || IS_SET(mIndex->act[0], ACT_PRACTICE)
        || IS_SET(mIndex->act[0], ACT_STAY_AREA)
        || IS_SET(mIndex->act[0], ACT_PROTECTED)
        || IS_SET(mIndex->act[0], ACT_BLACKSMITH)
        || IS_SET(mIndex->act[0], ACT_CREW_SELLER)
        || IS_SET(mIndex->act[0], ACT_IS_RESTRINGER)
        || IS_SET(mIndex->act[0], ACT_IS_HEALER)
        || IS_SET(mIndex->act[0], ACT_IS_CHANGER)
        || IS_SET(mIndex->act[0], ACT_IS_BANKER)
        || IS_SET(mIndex->act[1], ACT2_NOQUEST)
        || IS_SET(mIndex->act[1], ACT2_CHURCHMASTER)
        || IS_SET(mIndex->act[1], ACT2_PLANE_TUNNELER)
        || IS_SET(mIndex->act[1], ACT2_AIRSHIP_SELLER)
        || IS_SET(mIndex->act[1], ACT2_WIZI_MOB)
        || IS_SET(mIndex->act[1], ACT2_LOREMASTER )
        || IS_AFFECTED(mIndex, AFF_CHARM)
        || mIndex->pShop != NULL
        || mIndex->level > ( ch->tot_level + 20))
        continue;

           mob = get_char_world_index( ch, mIndex );
          if ( mob == NULL || mob->shop != NULL)
                continue;
    }

        if ( can_see_room(ch,mob->in_room) &&
            !room_is_private(mob->in_room, ch) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_PRIVATE) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_SOLITARY) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_DEATH_TRAP) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_SAFE) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_CPK) &&
            !IS_SET(mob->in_room->room_flag[1], ROOM_NO_GET_RANDOM) &&
            !IS_SET(mob->in_room->area->area_flags, AREA_NO_GET_RANDOM) &&
            !IS_SET(mob->in_room->room_flag[0], ROOM_NO_QUEST) &&
            mob->in_room->area->open &&
            !is_area_unlocked(ch, area) )
        break;
    }

    return mob;

}

/* get a random mob for a quest */
CHAR_DATA *get_random_mob( CHAR_DATA *ch, int continent )
{
    AREA_DATA *area;
    //char buf[MSL];

    area = get_random_area(ch, continent, true);

    return get_random_mob_area(ch, area);
}

bool valid_random_room(CHAR_DATA *ch, ROOM_INDEX_DATA *room, int n_room_flags, int n_room2_flags)
{
    if( ch )
    {
        return ( room != NULL &&
            can_see_room(ch, room) &&
            !room_is_private(room, ch) &&
            str_cmp(room->name, "NULL") &&
            !is_dislinked(room) &&
            (!n_room_flags || !IS_SET(room->room_flag[0], n_room_flags)) &&
            (!n_room2_flags || !IS_SET(room->room_flag[1], n_room2_flags)) );
    }
    else
    {
        return ( room != NULL &&
            str_cmp(room->name, "NULL") &&
            !is_dislinked(room) &&
            (!n_room_flags || !IS_SET(room->room_flag[0], n_room_flags)) &&
            (!n_room2_flags || !IS_SET(room->room_flag[1], n_room2_flags)) );
    }
}

ROOM_INDEX_DATA *get_random_room_list_byflags( CHAR_DATA *ch, LLIST *rooms, int n_room_flags, int n_room2_flags )
{
    ITERATOR it;
    ROOM_INDEX_DATA *room;

    int count = 0;

    iterator_start(&it, rooms);
    while( (room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) )
    {
        if( valid_random_room(ch, room, n_room_flags, n_room2_flags) )
            count++;
    }

    if( count > 0 )
    {
        iterator_reset(&it);
        int nth = number_range(1, count);
        while( (room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) )
        {
            if( valid_random_room(ch, room, n_room_flags, n_room2_flags) )
            {
                if( !--nth )
                {
                    break;
                }
            }
        }
    }

    iterator_stop(&it);

    return room;
}


ROOM_INDEX_DATA *get_random_room_area_byflags( CHAR_DATA *ch, AREA_DATA *area, int n_room_flags, int n_room2_flags )
{
    if (area == NULL || area->room_list == NULL)
        return NULL;

    return get_random_room_list_byflags(ch, area->room_list, n_room_flags, n_room2_flags);
}


/* get a random room for a quest */
ROOM_INDEX_DATA *get_random_room( CHAR_DATA *ch, int continent )
{
    AREA_DATA *area;


    area = get_random_area(ch, continent, true);

    return get_random_room_area_byflags(ch, area,
        (ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_SAFE | ROOM_CPK),
        (ROOM_NO_QUEST | ROOM_NO_GET_RANDOM));
}


/* get a random room from an area */
ROOM_INDEX_DATA *get_random_room_area( CHAR_DATA *ch, AREA_DATA *area )
{
    return get_random_room_area_byflags(ch, area,
        (ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CPK),
        ROOM_NO_GET_RANDOM);
}


/* Count how many letters in a string, not counting colour codes. */
int strlen_no_colours(const char *str) {
    if (!str) return 0;
    int count = 0;
    const char *p = str;
    int code_len;

    while (*p) {
        if (*p == COLOUR_CHAR && *(p+1) == COLOUR_CHAR) { // Escaped '{'
            count++;
            p += 2;
            continue;
        }
        code_len = get_colour_code_length_at_start(p);
        if (code_len > 0) {
            p += code_len; // Skip the entire color code
        } else {
            count++; // Visible character
            p++;
        }
    }
    return count;
}


int get_colour_width(char *text)
{
    char *plaintext = nocolour(text);
    int plen = strlen(plaintext);
    free_string(plaintext);
    int len = strlen(text);

    return (len - plen);
}


/* return a string without colour codes- {x {Y etc. */
char *nocolour( const char *string )
{
    int i,n;
    char buf[MSL];

    if( string[0] == '\0' )
        return str_dup(&str_empty[0]);

    int len = strlen(string);

    for (i = 0, n = 0; i < len && string[i] != '\0';) {
        if( string[i] == COLOUR_CHAR )
        {
            if (string[i+1] == '[')
                i+= 5;
            if( string[i+1] == COLOUR_CHAR )		// Double {{ becomes { when processed, but still counts as two
            {
                buf[n++] = COLOUR_CHAR;
            }

            i+=2;
        }
/*		
        else if (string[i] == '`')
        {
            if (string[i+1] == '[')
                i+= 7;
            else if(string[i+1] == '`')	// Double `` becomes ` when processed, but still counts as two
            {
                buf[n++] = '`';
                buf[n++] = '`';
                i+= 2;
            }
            else
            {
                i+= 2;
            }
        }
*/
        else
            buf[n++] = string[i++];
    }

    buf[n] = '\0';

    return str_dup(buf);
}


/* convert short desc to a keyword name, no feedback, errors return empty string */
char *short_to_name( const char *short_desc )
{
    char *temp_desc = nocolour(short_desc);
    char *keywords = NULL;

    LOCALIZATION_ERROR err = localization_short_to_keywords(temp_desc, &keywords, NULL);
    free_string(temp_desc);
    if (err != LOC_OK || IS_NULLSTR(keywords)) {
        if (keywords) free(keywords); // Since it can't be used
        keywords = str_empty;
    }

    return keywords;

#if 0
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
        while ( ISSPACE( temp_desc[i]))
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
#endif
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

#if 0
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
        pbugf(LOG_ERROR, "Load_Npc_Ships: # not found.");
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
            pbugf(LOG_ERROR, "Load_Npc_Ships: vnum %ld duplicated.", vnum );
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
                pbugf(LOG_ERROR, "Couldn't load cargo object for npc_ship because cargo object does not exist. Vnum: %ld", obj_vnum);
                continue;
            }

            // Add cargo object to npc ship index.
            pObj = create_object( pObjIndex, pObjIndex->level, false );
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
                pbugf(LOG_ERROR, "Couldn't load crew member for npc_ship because mob does not exist. Vnum: %ld", mob_vnum);
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
#endif

void do_dump( CHAR_DATA *ch, char *argument )
{
    int i;
    int n;
    FILE *fp;

    if ( !str_cmp(argument, "skills"))
    {
    if ( ( fp = fopen( SKILLS_DB_FILE, "w")) == NULL )
    {
        pbugf(LOG_ERROR, "do_dump: fopen");
        return;
    }

    i = 0;
    {
        SKILL_GROUP *sg;
        for (sg = skill_group_first(); sg; sg = sg->next) {
            ITERATOR it;
            char *skill_name;
            int n = 0;
            fprintf(fp, "[%s]:\n", sg->name);
            iterator_start(&it, sg->contents);
            while ((skill_name = (char *)iterator_nextdata(&it))) {
                fprintf(fp, "%i) %s\n", ++n, skill_name);
            }
            iterator_stop(&it);
            fprintf(fp, "\n");
        }
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
        pbugf(LOG_ERROR, "do_dump: fopen");
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
                    obj->update == true ? "Yes" : "No",
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
                    AREA_DATA *key_area = NULL;
                    if (obj->value[2] > 0) {
                        WNUM wnum;
                        if (resolve_widevnum(obj->value[2], NULL, &wnum))
                            key_area = wnum.pArea;
                        if (!key_area) key_area = get_system_area_fallback();
                    }
                    OBJ_INDEX_DATA *key = key_area ? get_obj_index(key_area, obj->value[2]) : NULL;
                    fprintf(fp, "%s[%ld]	%ld	%ld	%s	", key == NULL ? "None" : key->short_descr,
                        key == NULL ? 0 : key->vnum, obj->value[3],
                    obj->value[4], flag_string( container_flags, obj->value[1] ));
                } else {
                    fprintf(fp, "N/A	N/A	N/A	N/A ");
                }

                fprintf( fp, "%s	", extra_bit_name(obj->extra[0]));
                fprintf( fp, "%s	", extra2_bit_name(obj->extra[1]));
                fprintf( fp, "%s	", extra3_bit_name(obj->extra[2]));
                fprintf( fp, "%s	", extra4_bit_name(obj->extra[3]));
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
        pbugf(LOG_ERROR, "do_dump: fopen");
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
    pbugf(LOG_ERROR, "write_help_to_disk: hcat and help null, nothing to write");
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
        pbugf(LOG_ERROR, "write_help_to_disk: fp");
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
            pbugf(LOG_ERROR, "Load_Area_Trade: # not found.");
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



