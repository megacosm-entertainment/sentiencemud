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
*       ROM 2.4 is copyright 1993-1998 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@hypercube.org)                            *
*           Gabrielle Taylor (gtaylor@hypercube.org)                       *
*           Brian Moore (zump@rom.org)                                     *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

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
#include <sys/resource.h>
#include <stdarg.h>
#include "merc.h"
#include "recycle.h"
#include "olc.h"
#include "tables.h"
#include "scripts.h"

INSTANCE *instance_load(FILE *fp);
void update_instance(INSTANCE *instance);
void reset_instance(INSTANCE *instance);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);

extern LLIST *loaded_instances;

LLIST *loaded_dungeons;

DUNGEON_INDEX_LEVEL_DATA *load_dungeon_index_level(FILE *fp, int mode)
{
	DUNGEON_INDEX_LEVEL_DATA *level;
	char *word;
	bool fMatch;
	//char buf[MSL];
	//int floor;

	level = new_dungeon_index_level();
	level->mode = mode;

	if (mode == LEVELMODE_STATIC)
		level->floor = fread_number(fp);
	else if(mode == LEVELMODE_WEIGHTED)
	{
		level->total_weight = 0;
	}
	else if(mode == LEVELMODE_GROUP)
	{

	}

	while (str_cmp((word = fread_word(fp)), "#-LEVEL"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if (mode == LEVELMODE_GROUP)
			{
				if (!str_cmp(word, "#STATICLEVEL"))
				{
					DUNGEON_INDEX_LEVEL_DATA *lvl = load_dungeon_index_level(fp, LEVELMODE_STATIC);

					list_appendlink(level->group, lvl);
					fMatch = TRUE;
					break;
				}

				if (!str_cmp(word, "#WEIGHTEDLEVEL"))
				{
					DUNGEON_INDEX_LEVEL_DATA *lvl = load_dungeon_index_level(fp, LEVELMODE_WEIGHTED);

					list_appendlink(level->group, lvl);
					fMatch = TRUE;
					break;
				}
			}
			break;

		case 'F':
			if (!str_cmp(word, "Floor"))
			{
				if (mode == LEVELMODE_WEIGHTED)
				{
					DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
					weighted->weight = fread_number(fp);
					weighted->floor = fread_number(fp);
					list_appendlink(level->weighted_floors, weighted);

					level->total_weight += weighted->weight;
				}
				else
				{
					// Complain about getting weighted floor data on a static reference?
					fread_to_eol(fp);
				}

				fMatch = TRUE;
			}
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_dungeon_index_level: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return level;
}

DUNGEON_INDEX_SPECIAL_EXIT *load_dungeon_index_special_exit(FILE *fp, int mode)
{
	DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
	char *word;
	bool fMatch;
	char buf[MSL];

	int max_from = 0;
	int max_to = 0;

	ex->mode = mode;
	ex->name = fread_string(fp);

	if (mode == EXITMODE_STATIC || mode == EXITMODE_WEIGHTED_DEST)
		max_from = 1;
	
	if (mode == EXITMODE_STATIC || mode == EXITMODE_WEIGHTED_SOURCE)
		max_to = 1;

	while (str_cmp((word = fread_word(fp)), "#-EXIT"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if (mode == EXITMODE_GROUP)
			{
				if (!str_cmp(word, "#STATICEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_STATIC);

					list_appendlink(ex->group, gex);
					fMatch = TRUE;
					break;
				}

				if (!str_cmp(word, "#SOURCEEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_SOURCE);

					list_appendlink(ex->group, gex);
					fMatch = TRUE;
					break;
				}

				if (!str_cmp(word, "#DESTEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_DEST);

					list_appendlink(ex->group, gex);
					fMatch = TRUE;
					break;
				}

				if (!str_cmp(word, "#WEIGHTEDEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED);

					list_appendlink(ex->group, gex);
					fMatch = TRUE;
					break;
				}
			}
			break;

		case 'F':
			if (!str_cmp(word, "From"))
			{
				if (mode == EXITMODE_GROUP)
				{
					bug("load_dungeon_index_special_exit: specifying From entry on a group exit.", 0);
					continue;
				}

				if (max_from > 0 && list_size(ex->from) >= max_from)
				{
					bug("load_dungeon_index_special_exit: too many From entries found for exit mode.", 0);
					continue;
				}

				DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted = new_weighted_random_exit();
				weighted->weight = fread_number(fp);
				weighted->level = fread_number(fp);
				weighted->door = fread_number(fp);
				list_appendlink(ex->from, weighted);
				ex->total_from += weighted->weight;
				
				fMatch = TRUE;
				break;
			}
			break;

		case 'T':
			if (!str_cmp(word, "To"))
			{
				if (mode == EXITMODE_GROUP)
				{
					bug("load_dungeon_index_special_exit: specifying To entry on a group exit.", 0);
					continue;
				}

				if (max_to > 0 && list_size(ex->to) >= max_to)
				{
					bug("load_dungeon_index_special_exit: too many To entries found for exit mode.", 0);
					continue;
				}

				DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted = new_weighted_random_exit();
				weighted->weight = fread_number(fp);
				weighted->level = fread_number(fp);
				weighted->door = fread_number(fp);
				list_appendlink(ex->to, weighted);
				ex->total_to += weighted->weight;

				fMatch = TRUE;
				break;
			}
			break;
		}

		if (!fMatch) {
			sprintf(buf, "load_dungeon_index_special_exit: no match for word %.50s", word);
			bug(buf, 0);
		}
	}
	
	return ex;
}

DUNGEON_INDEX_DATA *load_dungeon_index(FILE *fp, AREA_DATA *area)
{
	DUNGEON_INDEX_DATA *dng;
	char *word;
	bool fMatch;
	char buf[MSL];

	dng = new_dungeon_index();
	dng->vnum = fread_number(fp);

	while (str_cmp((word = fread_word(fp)), "#-DUNGEON"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#STATICLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_STATIC);

				list_appendlink(dng->levels, level);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#WEIGHTEDLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_WEIGHTED);

				list_appendlink(dng->levels, level);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#GROUPLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_GROUP);

				list_appendlink(dng->levels, level);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#STATICEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_STATIC);

				list_appendlink(dng->special_exits, ex);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#SOURCEEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_SOURCE);

				list_appendlink(dng->special_exits, ex);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#DESTEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_DEST);

				list_appendlink(dng->special_exits, ex);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#WEIGHTEDEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED);

				list_appendlink(dng->special_exits, ex);
				fMatch = TRUE;
				break;
			}

			if (!str_cmp(word, "#GROUPEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_GROUP);

				list_appendlink(dng->special_exits, ex);
				fMatch = TRUE;
				break;
			}
			break;

		case 'A':
			KEY("AreaWho", dng->area_who, fread_number(fp));
			break;

		case 'C':
			KEYS("Comments", dng->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", dng->description, fread_string(fp));
			if (!str_cmp(word, "DungeonProg")) {
				int tindex;
				char *p;


				WNUM_LOAD wnum = fread_widevnum(fp, area->uid);
				p = fread_string(fp);

				tindex = trigger_index(p, PRG_DPROG);
				if(tindex < 0) {
					sprintf(buf, "load_dungeon_index: invalid trigger type %s", p);
					bug(buf, 0);
				} else {
					PROG_LIST *dpr = new_trigger();

					dpr->wnum_load = wnum;
					dpr->trig_type = tindex;
					dpr->trig_phrase = fread_string(fp);
					if( tindex == TRIG_SPELLCAST ) {
						char buf[MIL];
						int tsn = skill_lookup(dpr->trig_phrase);

						if( tsn < 0 ) {
							sprintf(buf, "load_dungeon_index: invalid spell '%s' for TRIG_SPELLCAST", p);
							bug(buf, 0);
							free_trigger(dpr);
							fMatch = TRUE;
							break;
						}

						free_string(dpr->trig_phrase);
						sprintf(buf, "%d", tsn);
						dpr->trig_phrase = str_dup(buf);
						dpr->trig_number = tsn;
						dpr->numeric = TRUE;

					} else {
						dpr->trig_number = atoi(dpr->trig_phrase);
						dpr->numeric = is_number(dpr->trig_phrase);
					}

					if(!dng->progs) dng->progs = new_prog_bank();

					list_appendlink(dng->progs[trigger_table[tindex].slot], dpr);
				}
				fMatch = TRUE;
			}
			break;

		case 'E':
			KEY("Entry", dng->entry_room, fread_number(fp));
			KEY("Exit", dng->exit_room, fread_number(fp));
			break;

		case 'F':
			KEY("Flags", dng->flags, fread_number(fp));
			if( !str_cmp(word, "Floor") )
			{
				WNUM_LOAD *wnum = fread_widevnumptr(fp);
				
				if( wnum )
				{
					list_appendlink(dng->floors, wnum);
				}

				fMatch = TRUE;
				break;
			}
			break;

		case 'M':
			KEYS("MountOut", dng->zone_out_mount, fread_string(fp));
			break;

		case 'N':
			KEYS("Name", dng->name, fread_string(fp));
			break;

		case 'P':
			KEYS("PortalOut", dng->zone_out_portal, fread_string(fp));
			break;

		case 'R':
			KEY("Repop", dng->repop, fread_number(fp));
			break;

		case 'S':
			if( !str_cmp(word, "SpecialRoom") )
			{
				DUNGEON_INDEX_SPECIAL_ROOM *special = new_dungeon_index_special_room();

				special->name = fread_string(fp);
				special->level = fread_number(fp);
				special->room = fread_number(fp);

				list_appendlink(dng->special_rooms, special);
				fMatch = TRUE;
				break;
			}
			break;

		case 'V':
			if (olc_load_index_vars(fp, word, &dng->index_vars, area))
			{
				fMatch = TRUE;
				break;
			}

			break;

		case 'Z':
			KEYS("ZoneOut", dng->zone_out, fread_string(fp));
			break;

		}

		if (!fMatch) {
			sprintf(buf, "load_dungeon_index: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return dng;

}


void fix_dungeon_index(DUNGEON_INDEX_DATA *dng)
{
	LLIST *floors = list_create(FALSE);

	WNUM_LOAD *wnum;
	ITERATOR it;
	iterator_start(&it, dng->floors);
	while( (wnum = (WNUM_LOAD *)iterator_nextdata(&it)) )
	{
		BLUEPRINT *bp = get_blueprint_auid(wnum->auid, wnum->vnum);

		if (bp)
			list_appendlink(floors, bp);
	}
	iterator_stop(&it);

	list_destroy(dng->floors);

	dng->floors = floors;
}

void fix_dungeon_indexes()
{
	AREA_DATA *area;
	DUNGEON_INDEX_DATA *dng;
	int iHash;

	for(area = area_first; area; area = area->next)
	{
		for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
		{
			for(dng = area->dungeon_index_hash[iHash]; dng; dng = dng->next)
				fix_dungeon_index(dng);
		}
	}
}

/*
void load_dungeons()
{
	FILE *fp = fopen(DUNGEONS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load dungeons.dat", 0);
		return;
	}
	char *word;
	bool fMatch;

	top_dprog_index = 0;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = FALSE;

		if( !str_cmp(word, "#DUNGEON") )
		{
			DUNGEON_INDEX_DATA *dng = load_dungeon_index(fp);
			int iHash = dng->vnum % MAX_KEY_HASH;

			dng->next = dungeon_index_hash[iHash];
			dungeon_index_hash[iHash] = dng;

			fMatch = TRUE;
		}

		if (!str_cmp(word, "#DUNGEONPROG"))
		{
		    SCRIPT_DATA *pr = read_script_new(fp, NULL, IFC_D);
		    if(pr) {
		    	pr->next = dprog_list;
		    	dprog_list = pr;

		    	if( pr->vnum > top_dprog_index )
		    		top_dprog_index = pr->vnum;
		    }

		    fMatch = TRUE;
		}


		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_dungeons: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	fclose(fp);
}
*/

void save_dungeon_index_level(FILE *fp, DUNGEON_INDEX_LEVEL_DATA *level, bool allow_groups)
{
	switch(level->mode)
	{
		case LEVELMODE_STATIC:
			fprintf(fp, "#STATICLEVEL %d\n", level->floor);
			break;

		case LEVELMODE_WEIGHTED:
			fprintf(fp, "#WEIGHTEDLEVEL\n");
			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *floor;
			ITERATOR wit;
			iterator_start(&wit, level->weighted_floors);
			while((floor = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
			{
				fprintf(fp, "Floor %d %d\n", floor->weight, floor->floor);
			}
			iterator_stop(&wit);
			break;

		case LEVELMODE_GROUP:
			fprintf(fp, "#GROUPLEVEL\n");
			DUNGEON_INDEX_LEVEL_DATA *group;
			ITERATOR git;
			iterator_start(&git, level->group);
			while( (group = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)) )
			{
				save_dungeon_index_level(fp, group, false);
			}

			iterator_stop(&git);
			break;
		
		default:
			return;
	}
	fprintf(fp, "#-LEVEL\n");
}

void save_dungeon_index_special_exit(FILE *fp, DUNGEON_INDEX_SPECIAL_EXIT *special, bool allow_groups)
{
	ITERATOR it;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;

	switch(special->mode)
	{
		case EXITMODE_STATIC:
			fprintf(fp, "#STATICEXIT %s~\n", fix_string(special->name));
			from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(special->from, 1);
			fprintf(fp, "From 1 %d %d\n", from->level, from->door);
			to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(special->to, 1);
			fprintf(fp, "To 1 %d %d\n", to->level, to->door);
			break;

		case EXITMODE_WEIGHTED_SOURCE:
			fprintf(fp, "#SOURCEEXIT %s~\n", fix_string(special->name));
			iterator_start(&it, special->from);
			while( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)) )
			{
				fprintf(fp, "From %d %d %d\n", from->weight, from->level, from->door);
			}
			iterator_stop(&it);
			to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(special->to, 1);
			fprintf(fp, "To 1 %d %d\n", to->level, to->door);
			break;

		case EXITMODE_WEIGHTED_DEST:
			fprintf(fp, "#DESTEXIT %s~\n", fix_string(special->name));
			from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(special->from, 1);
			fprintf(fp, "From 1 %d %d\n", from->level, from->door);
			iterator_start(&it, special->to);
			while( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)) )
			{
				fprintf(fp, "To %d %d %d\n", to->weight, to->level, to->door);
			}
			iterator_stop(&it);
			break;

		case EXITMODE_WEIGHTED:
			fprintf(fp, "#WEIGHTEDEXIT %s~\n", fix_string(special->name));
			iterator_start(&it, special->from);
			while( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)) )
			{
				fprintf(fp, "From %d %d %d\n", from->weight, from->level, from->door);
			}
			iterator_stop(&it);
			iterator_start(&it, special->to);
			while( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)) )
			{
				fprintf(fp, "To %d %d %d\n", to->weight, to->level, to->door);
			}
			iterator_stop(&it);
			break;
		
		case EXITMODE_GROUP:
			if (allow_groups)
			{
				DUNGEON_INDEX_SPECIAL_EXIT *gex;
				fprintf(fp, "#GROUPEXIT %s~\n", fix_string(special->name));
				iterator_start(&it, special->group);
				while( (gex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)) )
				{
					save_dungeon_index_special_exit(fp, gex, FALSE);
				}	
				iterator_stop(&it);
			}
			break;
	}
	fprintf(fp, "#-EXIT\n");
}

void save_dungeon_index(FILE *fp, DUNGEON_INDEX_DATA *dng)
{
	ITERATOR it;

	fprintf(fp, "#DUNGEON %ld\n", dng->vnum);
	fprintf(fp, "Name %s~\n", fix_string(dng->name));
	fprintf(fp, "Description %s~\n", fix_string(dng->description));
	fprintf(fp, "Comments %s~\n", fix_string(dng->comments));
	fprintf(fp, "AreaWho %d\n", dng->area_who);
	fprintf(fp, "Repop %d\n", dng->repop);

	fprintf(fp, "Flags %d\n", dng->flags);

	if( dng->entry_room > 0 )
		fprintf(fp, "Entry %ld\n", dng->entry_room);

	if( dng->exit_room > 0 )
		fprintf(fp, "Exit %ld\n", dng->exit_room);

	fprintf(fp, "ZoneOut %s~\n", fix_string(dng->zone_out));
	fprintf(fp, "PortalOut %s~\n", fix_string(dng->zone_out_portal));
	fprintf(fp, "MountOut %s~\n", fix_string(dng->zone_out_mount));

	BLUEPRINT *bp;
	iterator_start(&it, dng->floors);
	while((bp = (BLUEPRINT *)iterator_nextdata(&it)))
	{
		fprintf(fp, "Floor %ld\n", bp->vnum);
	}
	iterator_stop(&it);

	// Only save the level design if the dungeon is set to manual mode
	if (!IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		DUNGEON_INDEX_LEVEL_DATA *level;
		ITERATOR lit;
		iterator_start(&lit, dng->levels);
		while((level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&lit)))
		{
			save_dungeon_index_level(fp, level, true);
		}
		iterator_stop(&lit);

		DUNGEON_INDEX_SPECIAL_ROOM *special;
		iterator_start(&it, dng->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&it)) )
		{
			fprintf(fp, "SpecialRoom %s~ %d %d\n", fix_string(special->name), special->level, special->room);
		}
		iterator_stop(&it);

		DUNGEON_INDEX_SPECIAL_EXIT *ex;
		iterator_start(&it, dng->special_exits);
		while( (ex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)) )
		{
			save_dungeon_index_special_exit(fp, ex, true);

		}
		iterator_stop(&it);
	}

    if(dng->progs) {
		ITERATOR it;
		PROG_LIST *trigger;
		for(int i = 0; i < TRIGSLOT_MAX; i++) if(list_size(dng->progs[i]) > 0) {
			iterator_start(&it, dng->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "DungeonProg %s %s~ %s~\n",
					widevnum_string_wnum(trigger->wnum, dng->area),
					trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	olc_save_index_vars(fp, dng->index_vars, dng->area);

	fprintf(fp, "#-DUNGEON\n\n");
}

void save_dungeons(FILE *fp, AREA_DATA *area)
{
	int iHash;
	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = area->dungeon_index_hash[iHash]; dng; dng = dng->next)
		{
			save_dungeon_index(fp, dng);
		}
	}

	for( SCRIPT_DATA *scr = area->dprog_list; scr; scr = scr->next)
	{
		save_script_new(fp,NULL,scr,"DUNGEON");
	}
}

bool can_edit_dungeons(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}

DUNGEON_INDEX_DATA *get_dungeon_index_wnum(WNUM wuid)
{
	return get_dungeon_index(wuid.pArea, wuid.vnum);
}

DUNGEON_INDEX_DATA *get_dungeon_index_auid(long auid, long vnum)
{
	return get_dungeon_index(get_area_from_uid(auid), vnum);
}

DUNGEON_INDEX_DATA *get_dungeon_index(AREA_DATA *pArea, long vnum)
{
	if (!pArea) return NULL;

	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = pArea->dungeon_index_hash[iHash]; dng; dng = dng->next)
		{
			if( dng->vnum == vnum )
				return dng;
		}
	}

	return NULL;
}

static bool add_dungeon_instance(DUNGEON *dng, BLUEPRINT *bp)
{
	// Complain
	if (!IS_VALID(bp))
		return TRUE;

	INSTANCE *instance = create_instance(bp);

	if( !instance )
		return TRUE;

	instance->dungeon = dng;
	list_appendlink(dng->floors, instance);
	instance->floor = list_size(dng->floors);
	list_appendlist(dng->rooms, instance->rooms);
	list_appendlink(loaded_instances, instance);
	return FALSE;
}

static bool add_dungeon_level(DUNGEON *dng, DUNGEON_INDEX_LEVEL_DATA *level)
{
	BLUEPRINT *bp;
	switch(level->mode)
	{
		case LEVELMODE_STATIC:
			bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);
			return add_dungeon_instance(dng, bp);

		case LEVELMODE_WEIGHTED:
		{
			int w = number_range(1, level->total_weight);
			bp = NULL;	// Should NEVER get this!

			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
			ITERATOR wit;
			iterator_start(&wit, level->weighted_floors);
			while((weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
			{
				if (w <= weighted->weight)
				{
					bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);
					break;
				}

				w -= weighted->weight;
			}
			iterator_stop(&wit);

			if (!IS_VALID(bp))
			{
				// Complain about an impossible situation
				return TRUE;
			}

			return add_dungeon_instance(dng, bp);
		}

		case LEVELMODE_GROUP:
		{
			bool error = FALSE;
			DUNGEON_INDEX_LEVEL_DATA *lvl;

			int count = list_size(level->group);

			int *source = (int *)alloc_mem(sizeof(int) * count);
			for(int i = 0; i < count; i++)
				source[i] = i + 1;

			for(int i = count - 1; i >= 0; i--)
			{
				int ilevel = number_range(0, i);
				int nlevel = source[ilevel];
				source[ilevel] = source[i];

				lvl = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(level->group, nlevel);

				if (add_dungeon_level(dng, lvl))
				{
					error = TRUE;
					break;
				}
			}

			free_mem(source, sizeof(int) * count);
			return error;
		}
	}

	return TRUE;
}

/*
static bool add_dungeon_levels(DUNGEON *dng)
{
	bool error = FALSE;
	DUNGEON_INDEX_LEVEL_DATA *level;
	ITERATOR lit;

	list_clear(dng->floors);
	list_clear(dng->rooms);
	iterator_start(&lit, dng->index->levels);
	while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&lit)) )
	{
		if (add_dungeon_level(dng, level))
		{
			error = TRUE;
			break;
		}
	}
	iterator_stop(&lit);

	return error;
}
*/

static DUNGEON_INDEX_WEIGHTED_EXIT_DATA *get_weighted_random_exit(LLIST *list, int total)
{
	if (list_size(list) == 1) return (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(list, 1);

	int w = number_range(1, total);

	ITERATOR it;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *selected = NULL;
	iterator_start(&it, list);
	while( (weighted = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&it)) )
	{
		if (w <= weighted->weight)
		{
			selected = weighted;
			break;
		}
		else
			w -= weighted->weight;
	}
	iterator_stop(&it);

	return selected;
}

static EXIT_DATA *clone_dungeon_exit(ROOM_INDEX_DATA *room, int door)
{
	EXIT_DATA *ex = room->exit[door];

	if (!IS_VALID(ex))
	{
		EXIT_DATA *index = room->source->exit[door];

		room->exit[door] = ex = new_exit();
		ex->orig_door = door;
		ex->from_room = room;

		if (IS_VALID(index))
		{
			ex->rs_flags = index->rs_flags;
			REMOVE_BIT(ex->rs_flags, EX_ENVIRONMENT);
			ex->door.rs_lock = index->door.rs_lock;
		}
	}

	return ex;
}

INSTANCE *dungeon_get_instance_level(DUNGEON *dng, int level_no)
{
	if (!IS_VALID(dng)) return NULL;

	ITERATOR it;
	if (level_no < 0)
	{
		int ordinal = -level_no;
		INSTANCE *instance;

		iterator_start(&it, dng->floors);
		while((instance = (INSTANCE *)iterator_nextdata(&it)))
		{
			if (instance->ordinal == ordinal)
				break;
		}
		iterator_stop(&it);
		
		return instance;
	}
	else
		return (INSTANCE *)list_nthdata(dng->floors, level_no);
}

static bool add_dungeon_special_exit_from_to(DUNGEON *dng, DUNGEON_INDEX_SPECIAL_EXIT *dsex,
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from, DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to)
{
	if (!from || !to)
	{
		// Failed to get an exit reference
		return TRUE;
	}

	INSTANCE *from_level = dungeon_get_instance_level(dng, from->level);
	if (!IS_VALID(from_level))
	{
		return TRUE;
	}

	INSTANCE *to_level = dungeon_get_instance_level(dng, to->level);
	if (!IS_VALID(to_level))
	{
		return TRUE;
	}

	BLUEPRINT_EXIT_DATA *from_ex = get_blueprint_exit(from_level->blueprint, from->door);
	BLUEPRINT_EXIT_DATA *to_ex = get_blueprint_entrance(to_level->blueprint, to->door);

	ROOM_INDEX_DATA *from_room = NULL;
	int from_door = -1;
	EXIT_DATA *from_exit = NULL;
	EXIT_DATA *fromClone = NULL;

	if (from_ex)
	{
		INSTANCE_SECTION *from_section = instance_get_section(from_level, from_ex->section);

		if (from_section)
		{
			BLUEPRINT_LINK *from_link = get_section_link(from_section->section, from_ex->link);

			if (from_link)
			{
				from_room = instance_section_get_room_byvnum(from_section, from_link->vnum);
				from_door = from_link->door;

				if (from_room && from_door >= 0 && from_door < MAX_DIR)
				{
					from_exit = from_room->source->exit[from_door];
					fromClone = from_room->exit[from_door];
				}
			}
		}
	}

	ROOM_INDEX_DATA *to_room = NULL;
	int to_door = -1;
	EXIT_DATA *to_exit = NULL;
	EXIT_DATA *toClone = NULL;

	if (to_ex)
	{
		INSTANCE_SECTION *to_section = instance_get_section(to_level, to_ex->section);

		if (to_section)
		{
			BLUEPRINT_LINK *to_link = get_section_link(to_section->section, to_ex->link);

			if (to_link)
			{
				to_room = instance_section_get_room_byvnum(to_section, to_link->vnum);
				to_door = to_link->door;

				if (to_room && to_door >= 0 && to_door < MAX_DIR)
				{
					to_exit = to_room->source->exit[to_door];
					toClone = to_room->exit[to_door];
				}
			}
		}
	}

	// Must have a source room, and either no source exit or an unlinked exit
	if (from_room && (!IS_VALID(from_exit) || !from_exit->u1.to_room))
	{
		// Cannot link up an exit that is already linked up somewhere else
		//  Or the remote index exit exists and has a destination already
		if ((!IS_VALID(fromClone) || !fromClone->u1.to_room) &&
			(!IS_VALID(to_exit) || !to_exit->u1.to_room))
		{

			// Deal with the remote exit, first
			if (to_room)
			{
				// If the from exit doesn't have a clone or isn't already linked
				if (!IS_VALID(fromClone) || !fromClone->u1.to_room)
				{
					if (IS_VALID(to_exit))
					{
						// Only connect if we can make a two-way exit?
						if (dsex->connect_if_twoway)
						{
							if (!IS_VALID(toClone))
								toClone = clone_dungeon_exit(to_room, to_door);
						}
					}
					else
					{
						// The target room doesn't have a remote exit, so make it
						toClone = clone_dungeon_exit(to_room, to_door);
					}
				}

				if (IS_VALID(toClone))
				{
					REMOVE_BIT(toClone->rs_flags, EX_ENVIRONMENT);

					if (!toClone->u1.to_room)
					{
						toClone->u1.to_room = from_room;
						
						// We are creating a two-way exit
						//  Are the two exits reverses of each other
						//  If so, set the reset data on the remote exit from the source exit
						if (from_door == rev_dir[to_door])
						{
							// Only do this when they are reverses as the exit code doesn't account for exits
							//    linked to each other not being this way, such as one going north, the other
							//    going west.
							if (IS_VALID(from_exit))
							{
								toClone->rs_flags = from_exit->rs_flags;
								toClone->door.rs_lock.flags = from_exit->door.rs_lock.flags;
								toClone->door.rs_lock.key_wnum = from_exit->door.rs_lock.key_wnum;
								toClone->door.rs_lock.keys = from_exit->door.rs_lock.keys;	// This will not be destroyed when the exit is freed
								toClone->door.rs_lock.pick_chance = from_exit->door.rs_lock.pick_chance;
							}
							else
							{
								toClone->rs_flags = 0;
								toClone->door.rs_lock.flags = 0;
								toClone->door.rs_lock.key_wnum.pArea = NULL;
								toClone->door.rs_lock.key_wnum.vnum = 0;
								toClone->door.rs_lock.keys = NULL;
								toClone->door.rs_lock.pick_chance = 0;
							}
						}
					}
				}
			}
		}

		if (!dsex->create_if_exists ||									// Creates unlinked exit
			(to_room && !dsex->connect_if_twoway) ||			// Creates one-way exit
			(IS_VALID(toClone) && toClone->u1.to_room == from_room))	// Creates two-exit
		{
			if (!IS_VALID(fromClone))
			{
				fromClone = clone_dungeon_exit(from_room, from_door);

				// Explicit remote exit, no source exit and directions are reverses.
				//   Put all exit settings from remote exit onto source exit to make them symmetric
				if (!IS_VALID(from_exit) && IS_VALID(to_exit) && from_door == rev_dir[to_door])
				{
					fromClone->rs_flags = to_exit->rs_flags;
					fromClone->door.rs_lock.flags = to_exit->door.rs_lock.flags;
					fromClone->door.rs_lock.key_wnum = to_exit->door.rs_lock.key_wnum;
					fromClone->door.rs_lock.keys = to_exit->door.rs_lock.keys;
					fromClone->door.rs_lock.pick_chance = to_exit->door.rs_lock.pick_chance;
				}
			}

			fromClone->u1.to_room = to_room;
		}
	}

	NAMED_SPECIAL_EXIT *special = new_named_special_exit();
	special->name = str_dup(dsex->name);
	if(from_room)
	{
		special->room = from_room;
		if (IS_VALID(fromClone))
			special->ex = fromClone;
	}
	list_appendlink(dng->special_exits, special);

	return FALSE;	
}

static bool add_dungeon_special_exit(DUNGEON *dng, DUNGEON_INDEX_SPECIAL_EXIT *dsex)
{
	bool error = FALSE;
	//	DUNGEON_INDEX_DATA *index = dng->index;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = NULL;
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = NULL;

	switch(dsex->mode)
	{
	// Fixed source and Fixed destination
	case EXITMODE_STATIC:
		from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);	// Get the first entries
		to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
		break;

	// Weight Random source and Fixed destination
	case EXITMODE_WEIGHTED_SOURCE:
		from = get_weighted_random_exit(dsex->from, dsex->total_from);
		to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
		break;

	// Fixed source and Weighted Random destination
	case EXITMODE_WEIGHTED_DEST:
		from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
		to = get_weighted_random_exit(dsex->to, dsex->total_to);
		break;

	// Weighted Random source and Weighted Random destination
	case EXITMODE_WEIGHTED:
		from = get_weighted_random_exit(dsex->from, dsex->total_from);
		to = get_weighted_random_exit(dsex->to, dsex->total_to);
		break;

	case EXITMODE_GROUP:
		{
			int count = list_size(dsex->group);

			int *dest = (int *)alloc_mem(sizeof(int) * count);

			for(int i = 0; i < count; i++)
				dest[i] = i + 1;

			for(int i = count - 1; i >= 0; i--)
			{
				int iexit = number_range(0, i);
				int ndest = dest[iexit];
				dest[iexit] = dest[i];

				DUNGEON_INDEX_SPECIAL_EXIT *fex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dsex->group, i + 1);
				DUNGEON_INDEX_SPECIAL_EXIT *tex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dsex->group, ndest);

				from = get_weighted_random_exit(fex->from, fex->total_from);
				to = get_weighted_random_exit(tex->to, tex->total_to);

				if (add_dungeon_special_exit_from_to(dng, dsex, from, to))
				{
					error = TRUE;
					break;
				}
			}

			free_mem(dest, sizeof(int) * count);
			return error;
		}
	}

	return add_dungeon_special_exit_from_to(dng, dsex, from, to);
}

inline static void __dungeon_set_portal_room(OBJ_DATA *portal, ROOM_INDEX_DATA *room)
{
	portal->value[3] = GATETYPE_NORMAL;

	if (room)
	{
		if (room->source)
		{
			portal->value[5] = room->source->area->uid;
			portal->value[6] = room->source->vnum;
			portal->value[7] = room->id[0];
			portal->value[8] = room->id[1];
		}
		else
		{
			portal->value[5] = room->area->uid;
			portal->value[6] = room->vnum;
			portal->value[7] = 0;
			portal->value[8] = 0;
		}
	}
	else
	{
		portal->value[5] = 0;
		portal->value[6] = 0;
		portal->value[7] = 0;
		portal->value[8] = 0;
	}
}


static void __dungeon_correct_portals(DUNGEON *dng)
{
	ROOM_INDEX_DATA *room;
	ITERATOR rit;

	iterator_start(&rit, dng->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		for(OBJ_DATA *obj = room->contents; obj; obj = obj->next_content)
		{
			if (obj->item_type == ITEM_PORTAL)
			{
				switch(obj->value[3])
				{
					case GATETYPE_DUNGEON_SPECIAL:
					{
						ROOM_INDEX_DATA *room = NULL;

						if (obj->value[5] > 0)
						{
							room = get_dungeon_special_room(dng, obj->value[5]);
						}

						__dungeon_set_portal_room(obj, room);
						break;
					}
				}
			}
		}
	}
	iterator_stop(&rit);
}

DUNGEON *create_dungeon(AREA_DATA *pArea, long vnum)
{
	ITERATOR it;

	DUNGEON_INDEX_DATA *index = get_dungeon_index(pArea, vnum);

	if( !IS_VALID(index) )
	{
		return NULL;
	}

	DUNGEON *dng = new_dungeon();
	dng->index = index;
	dng->flags = index->flags;

	dng->progs			= new_prog_data();
	dng->progs->progs	= index->progs;
	variable_copylist(&index->index_vars,&dng->progs->vars,FALSE);

	dng->entry_room = get_room_index(index->area, index->entry_room);
	if( !dng->entry_room )
	{
		free_dungeon(dng);
		return NULL;
	}

	dng->exit_room = get_room_index(index->area, index->exit_room);
	if( !dng->exit_room )
	{
		free_dungeon(dng);
		return NULL;
	}

	dng->flags = index->flags;

	// Allow a script to create the level definitions, provided it's set to do that.
	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		list_clear(index->levels);
		list_clear(index->special_rooms);
		list_clear(index->special_exits);
		p_percent2_trigger(NULL, NULL, dng, NULL, NULL, NULL, NULL, NULL, TRIG_DUNGEON_SCHEMATIC, NULL);
	}

	bool error = FALSE;
	DUNGEON_INDEX_LEVEL_DATA *level;
	//BLUEPRINT *bp;
	INSTANCE *instance;
	iterator_start(&it, index->levels);
	while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)) )
	{
		if (add_dungeon_level(dng, level))
		{
			error = TRUE;
			break;
		}
	}
	iterator_stop(&it);

	if (!error)
	{
		__dungeon_correct_portals(dng);

		DUNGEON_INDEX_SPECIAL_ROOM *special;
		iterator_start(&it, index->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&it)) )
		{
			// TODO: Allow the ordinal processing
			// Get the instance for the specified level.
			instance = dungeon_get_instance_level(dng, special->level);

			if( IS_VALID(instance) )
			{
				// Get special room from the instance.
				NAMED_SPECIAL_ROOM *isr = list_nthdata(instance->special_rooms, special->room);

				// Room was found, add to dungeon under new name
				if( isr )
				{
					NAMED_SPECIAL_ROOM *dsr = new_named_special_room();

					free_string(dsr->name);
					dsr->name = str_dup(special->name);
					dsr->room = isr->room;

					list_appendlink(dng->special_rooms, dsr);
				}
			}
		}
		iterator_stop(&it);

		DUNGEON_INDEX_SPECIAL_EXIT *dsex;
		iterator_start(&it, index->special_exits);
		while( (dsex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)) )
		{
			if (add_dungeon_special_exit(dng, dsex))
			{
				error = TRUE;
				break;
			}

		}
		iterator_stop(&it);
	}

	if( error )
	{
		free_dungeon(dng);
		return NULL;
	}

	get_dungeon_id(dng);

	list_appendlink(loaded_dungeons, dng);
	return dng;
}

void extract_dungeon(DUNGEON *dungeon)
{
	ITERATOR it;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	INSTANCE *instance;

    if(dungeon->progs) {
	    SET_BIT(dungeon->progs->entity_flags,PROG_NODESTRUCT);
	    if(dungeon->progs->script_ref > 0) {
			dungeon->progs->extract_when_done = TRUE;
			return;
		}
    }

	room = dungeon->entry_room;
	if( !room )
		room = room_index_temple;

	// Dump all mobiles
	iterator_start(&it, dungeon->mobiles);
	while( (ch = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		char_from_room(ch);
		char_to_room(ch, room);
	}
	iterator_stop(&it);

	// Dump objects
	room = dungeon->entry_room;
	if( !room )
		room = room_index_donation;

	iterator_start(&it, dungeon->objects);
	while( (obj = (OBJ_DATA *)iterator_nextdata(&it)) )
	{
		if( obj->in_obj )
			obj_from_obj (obj);
		else if( obj->carried_by )
			obj_from_char(obj);
		else if( obj->in_room)
			obj_from_room(obj);

		obj_to_room(obj, room);
	}
	iterator_stop(&it);


	list_remlink(loaded_dungeons, dungeon);

	// Remove instances from loaded list
	iterator_start(&it, dungeon->floors);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		list_remlink(loaded_instances, instance);
	}
	iterator_stop(&it);

	free_dungeon(dungeon);
}

// TODO: WIDEVNUM
DUNGEON *find_dungeon_byplayer(CHAR_DATA *ch, AREA_DATA *pArea, long vnum)
{
	ITERATOR dit;
	DUNGEON *dng;

	if( IS_NPC(ch) ) return NULL;

	iterator_start(&dit, loaded_dungeons);
	while( (dng = (DUNGEON *)iterator_nextdata(&dit)) )
	{
		if( dng->index->area == pArea && dng->index->vnum == vnum && dungeon_isowner_player(dng, ch) )
			break;
	}
	iterator_stop(&dit);

	return dng;
}

CHAR_DATA *get_player_leader(CHAR_DATA *ch)
{
	CHAR_DATA *leader = ch;

	while( (leader->leader != NULL) && !IS_NPC(leader->leader) )
	{
		leader = leader->leader;
	}

	return leader;
}

DUNGEON *spawn_dungeon_player(CHAR_DATA *ch, AREA_DATA *pArea, long vnum)
{
	CHAR_DATA *leader = get_player_leader(ch);

	DUNGEON *leader_dng = find_dungeon_byplayer(leader, pArea, vnum);
	DUNGEON *ch_dng = find_dungeon_byplayer(ch, pArea, vnum);

	// Check if the player already has a dungeon
	if( IS_VALID(ch_dng) )
	{
		// Different dungeon?
		if( ch_dng != leader_dng )
		{
			if( !dungeon_canswitch_player(ch_dng, ch) )
			{
				leader_dng = ch_dng;
			}
			else
			{
				dungeon_removeowner_player(ch_dng, ch);
			}
		}
	}

	if( !IS_VALID(leader_dng) )
	{
		if( IS_NPC(leader) )
		{
			return NULL;
		}

		leader_dng = create_dungeon(pArea, vnum);

		if( !leader_dng )
			return NULL;

		dungeon_addowner_player(leader_dng, leader);

		p_percent2_trigger(NULL, NULL, leader_dng, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
		ITERATOR it;
		INSTANCE *instance;
		iterator_start(&it, leader_dng->floors);
		while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
		{
			p_percent2_trigger(NULL, instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
		}
		iterator_stop(&it);
	}

	dungeon_addowner_player(leader_dng, ch);

	return leader_dng;
}

ROOM_INDEX_DATA *spawn_dungeon_player_floor(CHAR_DATA *ch, AREA_DATA *pArea, long vnum, int floor)
{
	DUNGEON *dng = spawn_dungeon_player(ch, pArea, vnum);

	if (!dng) return NULL;

	INSTANCE *instance = (INSTANCE *)list_nthdata(dng->floors, floor);

	if( !IS_VALID(instance) )
	{
		extract_dungeon(dng);
		return NULL;
	}

	return instance->entrance;
}

ROOM_INDEX_DATA *spawn_dungeon_player_special_room(CHAR_DATA *ch, AREA_DATA *pArea, long vnum, int special_room, char *special_room_name)
{
	DUNGEON *dng = spawn_dungeon_player(ch, pArea, vnum);

	if (!dng) return NULL;
	
	if (special_room > 0)
		return get_dungeon_special_room(dng, special_room);
	else if (!IS_NULLSTR(special_room_name))
		return get_dungeon_special_room_byname(dng, special_room_name);

	extract_dungeon(dng);
	return NULL;
}

bool dungeon_can_idle(DUNGEON *dungeon)
{
	return IS_SET(dungeon->flags, DUNGEON_DESTROY) ||
			(!IS_SET(dungeon->flags, DUNGEON_NO_IDLE) &&
				(!IS_SET(dungeon->flags, DUNGEON_IDLE_ON_COMPLETE) ||
				IS_SET(dungeon->flags, DUNGEON_COMPLETED)));
}

void dungeon_check_empty(DUNGEON *dungeon)
{
	if( dungeon->empty )
	{
		if( list_size(dungeon->players) > 0 )
			dungeon->empty = FALSE;
	}
	else if( list_size(dungeon->players) < 1 )
	{
		dungeon->empty = TRUE;
		if( dungeon_can_idle(dungeon) )
			dungeon->idle_timer = UMAX(DUNGEON_IDLE_TIMEOUT, dungeon->idle_timer);
	}

	if( !dungeon->empty && !dungeon_can_idle(dungeon) )
		dungeon->idle_timer = 0;
}

void dungeon_update()
{
	ITERATOR it;
	ITERATOR iit;
	DUNGEON *dungeon;
	INSTANCE *instance;

	iterator_start(&it, loaded_dungeons);
	while( (dungeon = (DUNGEON *)iterator_nextdata(&it)) )
	{
		if( dungeon_isorphaned(dungeon) && list_size(dungeon->players) < 1 )
		{
			// Do NOT keep an empty orphaned dungeon
			extract_dungeon(dungeon);
			continue;
		}

		p_percent2_trigger(NULL, NULL, dungeon, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

		iterator_start(&iit, dungeon->floors);
		while( (instance = (INSTANCE *)iterator_nextdata(&iit)) )
		{
			update_instance(instance);
		}
		iterator_stop(&iit);

		if( IS_SET(dungeon->flags, DUNGEON_DESTROY) || !IS_SET(dungeon->flags, DUNGEON_IDLE_ON_COMPLETE) || IS_SET(dungeon->flags, DUNGEON_COMPLETED) )
		{
			if( dungeon->idle_timer > 0 )
			{
				if( !--dungeon->idle_timer )
				{
					extract_dungeon(dungeon);
					continue;
				}
			}
		}

		dungeon_check_empty(dungeon);

		dungeon->age++;
		if (dungeon->index->repop > 0 && (dungeon->age >= dungeon->index->repop) )
		{
			p_percent2_trigger(NULL, NULL, dungeon, NULL, NULL, NULL, NULL, NULL, TRIG_RESET, NULL);

			iterator_start(&iit, dungeon->floors);
			while( (instance = (INSTANCE *)iterator_nextdata(&iit)) )
			{
				reset_instance(instance);
			}
			iterator_stop(&iit);

			dungeon->age = 0;
		}
	}
	iterator_stop(&it);
}


/////////////////////////////////////////
//
// DUNGEON EDITOR
//

const struct olc_cmd_type dngedit_table[] =
{
	{ "?",				show_help			},
	{ "adddprog",		dngedit_adddprog	},
	{ "areawho",		dngedit_areawho		},
	{ "commands",		show_commands		},
	{ "comments",		dngedit_comments	},
	{ "create",			dngedit_create		},
	{ "deldprog",		dngedit_deldprog	},
	{ "description",	dngedit_description	},
	{ "entry",			dngedit_entry		},
	{ "exit",			dngedit_exit		},
	{ "flags",			dngedit_flags		},
	{ "floors",			dngedit_floors		},
	{ "levels",			dngedit_levels		},
	{ "list",			dngedit_list		},
	{ "mountout",		dngedit_mountout	},
	{ "name",			dngedit_name		},
	{ "portalout",		dngedit_portalout	},
	{ "scripted",		dngedit_scripted	},
	{ "show",			dngedit_show		},
	{ "special",		dngedit_special		},
	{ "varclear",		dngedit_varclear	},
	{ "varset",			dngedit_varset		},
	{ "zoneout",		dngedit_zoneout		},
	{ NULL,				NULL				}

};

void list_dungeons(CHAR_DATA *ch, char *argument)
{
	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many dungeons you can see.{x\n\r", ch);

	AREA_DATA *area = ch->in_room->area;
	int lines = 0;
	bool error = FALSE;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= area->top_dungeon_vnum; vnum++)
	{
		DUNGEON_INDEX_DATA *dng = get_dungeon_index(area, vnum);

		if( dng )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s\n\r",
				vnum,
				dng->name);

			++lines;
			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = TRUE;
				break;
			}
		}
	}

	if( error )
	{
		send_to_char("Too many dungeons to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No dungeons to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("Dungeons in current area.\n\r", ch);
			send_to_char("{Y Vnum   [            Name            ]{x\n\r", ch);
			send_to_char("{Y======================================={x\n\r", ch);

			page_to_char(buffer->string, ch);
		}
	}
	free_buf(buffer);
}

void do_dnglist(CHAR_DATA *ch, char *argument)
{
	list_dungeons(ch, argument);
}

void do_dngedit(CHAR_DATA *ch, char *argument)
{
	DUNGEON_INDEX_DATA *dng;
	WNUM wnum;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (parse_widevnum(arg1, ch->in_room->area, &wnum))
	{
		if (!wnum.pArea || wnum.vnum < 1)
		{
			send_to_char("Widevnum not associated with an area.\n\r", ch);
			return;
		}

	    if (!IS_BUILDER(ch, wnum.pArea))
		{
			send_to_char("DngEdit:  widevnum in an area you cannot build in.\n\r", ch);
			return;
		}

		if (!(dng = get_dungeon_index(wnum.pArea, wnum.vnum)))
		{
			send_to_char("DNGEdit:  That dungeon does not exist.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)dng;
		ch->desc->editor = ED_DUNGEON;
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (dngedit_create(ch, argument))
			{
				ch->pcdata->immortal->last_olc_command = current_time;
				ch->desc->editor = ED_DUNGEON;

				EDIT_DUNGEON(ch, dng);
				SET_BIT(dng->area->area_flags, AREA_CHANGED);
			}

			return;
		}

	}

	send_to_char("Syntax: dngedit <widevnum>\n\r"
				 "        dngedit create <widevnum>\n\r", ch);
}

void dngedit(CHAR_DATA *ch, char *argument)
{
	DUNGEON_INDEX_DATA *dng;
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!str_cmp(command, "done"))
	{
		edit_done(ch);
		return;
	}

    ch->pcdata->immortal->last_olc_command = current_time;

	if (command[0] == '\0')
	{
		dngedit_show(ch, argument);
		return;
	}

	for (cmd = 0; dngedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, dngedit_table[cmd].name))
		{
			if ((*dngedit_table[cmd].olc_fun) (ch, argument))
			{
				SET_BIT(dng->area->area_flags, AREA_CHANGED);
			}

			return;
		}
	}

    interpret(ch, arg);
}

DNGEDIT( dngedit_list )
{
	list_dungeons(ch, argument);
	return FALSE;
}

DUNGEON_INDEX_LEVEL_DATA *dungeon_index_get_nth_level(DUNGEON_INDEX_DATA *dng, int level_no, DUNGEON_INDEX_LEVEL_DATA **in_group)
{
	if (!IS_VALID(dng) || !level_no) return NULL;

	if (in_group) *in_group = NULL;

	DUNGEON_INDEX_LEVEL_DATA *level = NULL;
	if (level_no < 0)
	{
		int ordinal = -level_no;
		ITERATOR it;
		DUNGEON_INDEX_LEVEL_DATA *lvl;

		iterator_start(&it, dng->levels);
		while((lvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)))
		{
			if (lvl->ordinal == ordinal)
			{
				level = lvl;
				break;
			}
			else if(lvl->mode == LEVELMODE_GROUP)
			{
				ITERATOR git;
				DUNGEON_INDEX_LEVEL_DATA *glvl;

				iterator_start(&git, lvl->group);
				while((glvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
				{
					if (glvl->ordinal == ordinal)
					{
						level = glvl;
						if(in_group) *in_group = lvl;
						break;
					}
				}
				iterator_stop(&git);

				if (level)
					break;
			}
		}
		iterator_stop(&it);
	}
	else
	{
		ITERATOR it;
		DUNGEON_INDEX_LEVEL_DATA *lvl;

		iterator_start(&it, dng->levels);
		while((lvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)))
		{
			if (lvl->mode == LEVELMODE_GROUP)
			{
				ITERATOR git;
				DUNGEON_INDEX_LEVEL_DATA *glvl;

				iterator_start(&git, lvl->group);
				while((glvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
				{
					if (level_no == 1)
					{
						level = glvl;
						if(in_group) *in_group = lvl;
						break;
					}

					level_no--;
				}
				iterator_stop(&git);

				if(level)
					break;
			}
			else if(level_no == 1)
			{
				level = lvl;
				break;
			}
			else
				level_no--;
		}
		iterator_stop(&it);
	}

	return level;
}

BLUEPRINT *dungeon_index_get_representative_blueprint(DUNGEON_INDEX_DATA *dng, int level_no, bool *exact)
{
	DUNGEON_INDEX_LEVEL_DATA *in_group = NULL;
	DUNGEON_INDEX_LEVEL_DATA *lvl = dungeon_index_get_nth_level(dng, level_no, &in_group);

	if(IS_VALID(lvl) || lvl->mode == LEVELMODE_GROUP) return NULL;

	if(lvl->mode == LEVELMODE_STATIC)
	{
		if (exact) *exact = !in_group;
		return (BLUEPRINT *)list_nthdata(dng->floors, lvl->floor);
	}
	else
	{
		ITERATOR wit;
		DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
		DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *most_likely = NULL;

		iterator_start(&wit, lvl->weighted_floors);
		while((weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
		{
			if(!most_likely || weighted->weight > most_likely->weight)
			{
				most_likely = weighted;
			}
		}
		iterator_stop(&wit);

		if(!most_likely) return NULL;

		if(exact) *exact = FALSE;
		return (BLUEPRINT *)list_nthdata(dng->floors, most_likely->floor);
	}
}

void dngedit_buffer_floors(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
	char buf[MSL];

	if( list_size(dng->floors) > 0 )
	{
		ITERATOR fit;
		BLUEPRINT *bp;

		add_buf(buffer, "{gFloors:{x\n\r");
		add_buf(buffer, "{g     [  Vnum  ] [             Name             ]\n\r");
		add_buf(buffer, "{g=================================================\n\r");

		int floor = 0;
		iterator_start(&fit, dng->floors);
		while( (bp = (BLUEPRINT *)iterator_nextdata(&fit)) )
		{
			sprintf(buf, "{W%4d  {G%8ld   {x%-.30s{x\n\r", ++floor, bp->vnum, bp->name);
			add_buf(buffer, buf);
		}
		iterator_stop(&fit);
		add_buf(buffer, "=================================================\n\r");
	}
	else
	{
		add_buf(buffer, "{gFloors:{x\n\r");
		add_buf(buffer, "   None\n\r");
	}
}

void dngedit_buffer_levels(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
	char buf[MSL];

	add_buf(buffer, "{yLevels:{x\n\r");
	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		add_buf(buffer, "  {WSCRIPTED{x\n\r");
	}
	else if (list_size(dng->levels) > 0)
	{
		ITERATOR it;
		DUNGEON_INDEX_LEVEL_DATA *level;

		add_buf(buffer, "{y     [  Mode  ] [ Ordinal ]{x\n\r");
		add_buf(buffer, "{y===================================================={x\n\r");

		int levelno = 1;
		iterator_start(&it, dng->levels);
		while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)) )
		{
			switch(level->mode)
			{
				case LEVELMODE_STATIC:
				{
					BLUEPRINT *bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);
					sprintf(buf, "{W%4d  {Y STATIC {x   %7d    %4d - {x%23.23s{x\n\r", levelno++, level->ordinal, level->floor, bp->name);
					add_buf(buffer, buf);
					break;
				}

				case LEVELMODE_WEIGHTED:
				{
					ITERATOR wit;
					DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
					BLUEPRINT *bp;

					sprintf(buf, "{W%4d  {CWEIGHTED{x   %7d\n\r", levelno++, level->ordinal);
					add_buf(buffer, buf);
					add_buf(buffer, "{c               [ Weight ] [              Floor             ]{x\n\r");
					add_buf(buffer, "{c          ==================================================={x\n\r");

					int weightno = 1;
					iterator_start(&wit, level->weighted_floors);
					while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)) )
					{
						
						bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);
						sprintf(buf, "          {c%4d   {W%6d     {x%4d - %23.23s{x\n\r", weightno++, weighted->weight, weighted->floor, bp->name);
						add_buf(buffer, buf);
					}
					iterator_stop(&wit);
					add_buf(buffer, "{c          ----------------------------------------------------{x\n\r");
					break;
				}

				case LEVELMODE_GROUP:
				{
					ITERATOR git;
					DUNGEON_INDEX_LEVEL_DATA *lvl;

					sprintf(buf, "{W%4d  {G  GROUP {x\n\r", levelno++);
					add_buf(buffer, buf);

					if (list_size(level->group) > 0)
					{
						add_buf(buffer, "          {y     [  Mode  ]{x\n\r");
						add_buf(buffer, "          {y===================================================={x\n\r");

						int glevelno = 1;
						iterator_start(&git, level->group);
						while( (lvl = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)) )
						{
							if (lvl->mode == LEVELMODE_STATIC)
							{
								BLUEPRINT *bp = (BLUEPRINT *)list_nthdata(dng->floors, lvl->floor);
								sprintf(buf, "          {W%4d  {Y STATIC {x     %4d - {x%23.23s{x\n\r", glevelno++, lvl->floor, bp->name);
								add_buf(buffer, buf);	
							}
							else if(lvl->mode == LEVELMODE_WEIGHTED)
							{
								ITERATOR wit;
								DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
								BLUEPRINT *bp;

								sprintf(buf, "          {W%4d  {CWEIGHTED{x\n\r", glevelno++);
								add_buf(buffer, buf);
								add_buf(buffer, "{c                         [ Weight ] [              Floor             ]{x\n\r");
								add_buf(buffer, "{c                    ==================================================={x\n\r");

								int weightno = 1;
								iterator_start(&wit, level->weighted_floors);
								while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)) )
								{
									
									bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);
									sprintf(buf, "                    {c%4d   {W%6d     {x%4d - %23.23s{x\n\r", weightno++, weighted->weight, weighted->floor, bp->name);
									add_buf(buffer, buf);
								}
								iterator_stop(&wit);
								add_buf(buffer, "{c                    ----------------------------------------------------{x\n\r");
							}
						}
						iterator_stop(&git);
					}
					else
						add_buf(buffer, "          none\n\r");

					break;
				}
			}
		}
		iterator_stop(&it);
		add_buf(buffer, "{y----------------------------------------------------{x\n\r");
	}
	else
	{
		add_buf(buffer, "  None\n\r");
	}
}

void dngedit_buffer_special_exits(BUFFER *buffer, DUNGEON_INDEX_DATA *dng)
{
	char buf[MSL];

	add_buf(buffer, "{xSpecial Exits:{x\n\r");
	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		add_buf(buffer, "  {WSCRIPTED{x\n\r");
	}
	else if(list_size(dng->special_exits) > 0)
	{
		ITERATOR it;
		DUNGEON_INDEX_SPECIAL_EXIT *dsex;

		add_buf(buffer, "{x     [    Mode    ]{x\n\r");
		add_buf(buffer, "{x========================================================{x\n\r");

		int exitno = 1;
		iterator_start(&it, dng->special_exits);
		while ( (dsex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)))
		{
			switch(dsex->mode)
			{
				case EXITMODE_STATIC:
				{
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
					sprintf(buf, "%4d  {Y   STATIC   {x\n\r", exitno++);
					sprintf(buf, "          Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
					add_buf(buffer, buf);
					sprintf(buf, "          Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), to->level, to->door);
					add_buf(buffer, buf);
					break;	
				}

				case EXITMODE_WEIGHTED_SOURCE:
				{
					ITERATOR wit;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
					sprintf(buf, "%4d  {C   SOURCE   {x\n\r", exitno++);
					add_buf(buffer, buf);

					int fromexitno = 1;
					add_buf(buffer, "          Source:\n\r");
					add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
					add_buf(buffer, "          ===================================={x\n\r");
					iterator_start(&wit, dsex->from);
					while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
					{
						sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&wit);
					add_buf(buffer, "          ----------------------------------------------------{x\n\r");

					sprintf(buf, "          Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), to->level, to->door);
					add_buf(buffer, buf);
					break;
				}

				case EXITMODE_WEIGHTED_DEST:
				{
					ITERATOR wit;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
					sprintf(buf, "%4d  {C DESTINATION{x\n\r", exitno++);
					add_buf(buffer, buf);

					sprintf(buf, "          Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
					add_buf(buffer, buf);

					int toexitno = 1;
					add_buf(buffer, "          Destination:\n\r");
					add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
					add_buf(buffer, "          ===================================={x\n\r");
					iterator_start(&wit, dsex->to);
					while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
					{
						sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&wit);
					add_buf(buffer, "          ----------------------------------------------------{x\n\r");
					break;
				}

				case EXITMODE_WEIGHTED:
				{
					ITERATOR wit;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
					sprintf(buf, "%4d  {C  WEIGHTED  {x\n\r", exitno++);
					add_buf(buffer, buf);

					int fromexitno = 1;
					add_buf(buffer, "          Source:\n\r");
					add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
					add_buf(buffer, "          ===================================={x\n\r");
					iterator_start(&wit, dsex->from);
					while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
					{
						sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&wit);
					add_buf(buffer, "          ----------------------------------------------------{x\n\r");

					int toexitno = 1;
					add_buf(buffer, "          Destination:\n\r");
					add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
					add_buf(buffer, "          ===================================={x\n\r");
					iterator_start(&wit, dsex->to);
					while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
					{
						sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&wit);
					add_buf(buffer, "          ----------------------------------------------------{x\n\r");
					break;
				}

				case EXITMODE_GROUP:
				{
					int count = list_size(dsex->group);

					sprintf(buf, "%4d  {G    GROUP   {x\n\r", exitno++);
					add_buf(buffer, buf);
					if (count > 0)
					{
						ITERATOR git;
						DUNGEON_INDEX_SPECIAL_EXIT *gex;

						add_buf(buffer, "{x               [    Mode    ]{x\n\r");
						add_buf(buffer, "{x          ========================================================{x\n\r");

						int gexitno = 1;
						iterator_start(&git, dsex->group);
						while ( (gex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&git)) )
						{
							switch(gex->mode)
							{
								case EXITMODE_STATIC:
								{
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
									sprintf(buf, "          %4d  {Y   STATIC   {x\n\r", gexitno++);
									sprintf(buf, "                    Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
									add_buf(buffer, buf);
									sprintf(buf, "                    Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), to->level, to->door);
									add_buf(buffer, buf);
									break;
								}

								case EXITMODE_WEIGHTED_SOURCE:
								{
									ITERATOR wit;
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
									sprintf(buf, "          %4d  {C   SOURCE   {x\n\r", gexitno++);
									add_buf(buffer, buf);

									int fromexitno = 1;
									add_buf(buffer, "                    Source:\n\r");
									add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
									add_buf(buffer, "                    ===================================={x\n\r");
									iterator_start(&wit, gex->from);
									while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
									{
										sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
										add_buf(buffer, buf);
									}
									iterator_stop(&wit);
									add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

									sprintf(buf, "                    Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), to->level, to->door);
									add_buf(buffer, buf);
									break;
								}

								case EXITMODE_WEIGHTED_DEST:
								{
									ITERATOR wit;
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
									sprintf(buf, "          %4d  {C DESTINATION{x\n\r", gexitno++);
									add_buf(buffer, buf);

									sprintf(buf, "                    Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
									add_buf(buffer, buf);

									int toexitno = 1;
									add_buf(buffer, "                    Destination:\n\r");
									add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
									add_buf(buffer, "                    ===================================={x\n\r");
									iterator_start(&wit, gex->to);
									while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
									{
										sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
										add_buf(buffer, buf);
									}
									iterator_stop(&wit);
									add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
									break;
								}

								case EXITMODE_WEIGHTED:
								{
									ITERATOR wit;
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
									DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
									sprintf(buf, "          %4d  {C  WEIGHTED  {x\n\r", gexitno++);
									add_buf(buffer, buf);

									int fromexitno = 1;
									add_buf(buffer, "                    Source:\n\r");
									add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
									add_buf(buffer, "                    ===================================={x\n\r");
									iterator_start(&wit, gex->from);
									while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
									{
										sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
										add_buf(buffer, buf);
									}
									iterator_stop(&wit);
									add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

									int toexitno = 1;
									add_buf(buffer, "                    Destination:\n\r");
									add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
									add_buf(buffer, "                    ===================================={x\n\r");
									iterator_start(&wit, gex->to);
									while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
									{
										sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
										add_buf(buffer, buf);
									}
									iterator_stop(&wit);
									add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
									break;
								}
							}

						}
						iterator_stop(&git);
						add_buf(buffer, "{x          --------------------------------------------------------{x\n\r");
					}
					else
					{
						add_buf(buffer, "          None\n\r");
					}
					break;
				}
			}
		}

		iterator_stop(&it);
		add_buf(buffer, "{x----------------------------------------------------{x\n\r");

		add_buf(buffer, "{YYELLOW{x - Generated level position index\n\r");
		add_buf(buffer, "{GGREEN{x  - Ordinal level position index\n\r");
	}
	else
	{
		add_buf(buffer, "  None\n\r");
	}


}

DNGEDIT( dngedit_scripted )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		REMOVE_BIT(dng->flags, DUNGEON_SCRIPTED_LEVELS);
		send_to_char("Scripted Levels Mode disabled.\n\r", ch);
	}
	else
	{
		SET_BIT(dng->flags, DUNGEON_SCRIPTED_LEVELS);
		list_clear(dng->levels);
		list_clear(dng->special_rooms);
		list_clear(dng->special_exits);
		send_to_char("Scripted Levels Mode enabled.\n\r", ch);
	}

	return TRUE;
}

DNGEDIT( dngedit_show )
{
	DUNGEON_INDEX_DATA *dng;
	ROOM_INDEX_DATA *room;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_DUNGEON(ch, dng);

	buffer = new_buf();

	sprintf(buf, "Name:        [%5ld] %s\n\r", dng->vnum, dng->name);
	add_buf(buffer, buf);

	sprintf(buf, "Flags:       %s\n\r", flag_string(dungeon_flags, dng->flags));
	add_buf(buffer, buf);

	sprintf(buf, "AreaWho:     %s\n\r", flag_string(area_who_titles, dng->area_who));
	add_buf(buffer, buf);

	if( dng->repop > 0)
		sprintf(buf, "Repop:       %d minutes\n\r", dng->repop);
	else
		sprintf(buf, "Repop:       {Dnever{X\n\r");
	add_buf(buffer, buf);

	room = get_room_index(dng->area, dng->entry_room);
	if( room )
	{
		sprintf(buf, "Entry:       [%ld] %-.30s\n\r", room->vnum, room->name);
		add_buf(buffer, buf);
	}
	else
		add_buf(buffer, "Entry:       {Dinvalid{x\n\r");

	room = get_room_index(dng->area, dng->exit_room);
	if( room )
	{
		sprintf(buf, "Exit:        [%ld] %-.30s\n\r", room->vnum, room->name);
		add_buf(buffer, buf);
	}
	else
		add_buf(buffer, "Exit:        {Dinvalid{x\n\r");

	add_buf(buffer, "ZoneOut:     ");
	add_buf(buffer, dng->zone_out);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "PortalOut:     ");
	add_buf(buffer, dng->zone_out_portal);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "MountOut:     ");
	add_buf(buffer, dng->zone_out_mount);
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, dng->description);
	add_buf(buffer, "\n\r");

	dngedit_buffer_floors(buffer, dng);

	dngedit_buffer_levels(buffer, dng);

	add_buf(buffer, "Special Rooms:\n\r");
	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		add_buf(buffer, "   {WSCRIPTED{x\n\r");
	}
	else if( list_size(dng->special_rooms) > 0 )
	{
		DUNGEON_INDEX_SPECIAL_ROOM *special;

		char buf[MSL];
		int line = 0;

		ITERATOR sit;

		add_buf(buffer, "     [             Name             ] [ Level ] [ Room ]\n\r");
		add_buf(buffer, "---------------------------------------------------------\n\r");

		iterator_start(&sit, dng->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			sprintf(buf, "{W%4d  %-30.30s   {%c%7d{x     %4d\n\r", ++line, special->name, (special->level<0?'G':(special->level>0?'Y':'W')), abs(special->level), special->room);
			add_buf(buffer, buf);
		}

		iterator_stop(&sit);
		add_buf(buffer, "---------------------------------------------------------\n\r");
		add_buf(buffer, "{YYELLOW{x - Generated level position index\n\r");
		add_buf(buffer, "{GGREEN{x  - Ordinal level position index\n\r");
	}
	else
	{
		add_buf(buffer, "   None\n\r");
	}
	add_buf(buffer, "\n\r");

	dngedit_buffer_special_exits(buffer, dng);

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, dng->comments);
	add_buf(buffer, "\n\r-----\n\r");


	if (dng->progs) {
		int cnt, slot;

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
			if(list_size(dng->progs[slot]) > 0) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "Prog Vnum", "Trigger", "Phrase");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
			add_buf(buffer, buf);

			ITERATOR it;
			PROG_LIST *trigger;
			for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
				iterator_start(&it, dng->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					sprintf(buf, "{C[{W%4d{C]{x %ld#%ld %-10s %-6s\n\r", cnt,
						trigger->wnum.pArea ? trigger->wnum.pArea->uid : 0,
						trigger->wnum.vnum,trigger_name(trigger->trig_type),
						trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, FALSE, FALSE));
					add_buf(buffer, buf);
					cnt++;
				}
				iterator_stop(&it);
			}
		}
	}

	olc_show_index_vars(buffer, dng->index_vars);

	if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
	{
		send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
	}
	else
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return FALSE;
}

void do_dngshow(CHAR_DATA *ch, char *argument)
{
	DUNGEON_INDEX_DATA *dng;
	void *old_edit;
	WNUM wnum;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  dngshow <widevnum>\n\r", ch);
		return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		send_to_char("Please specify a widevnum.\n\r", ch);
		return;
	}

	if (!(dng= get_dungeon_index(wnum.pArea, wnum.vnum)))
	{
		send_to_char("That dungeon does not exist.\n\r", ch);
		return;
	}

	old_edit = ch->desc->pEdit;
	ch->desc->pEdit = (void *) dng;

	dngedit_show(ch, argument);
	ch->desc->pEdit = old_edit;
	return;
}

DNGEDIT( dngedit_create )
{
	AREA_DATA *area = ch->in_room->area;
	DUNGEON_INDEX_DATA *dng;
	WNUM wnum;
	int  iHash;

	if (argument[0] == '\0' || !parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
	{
		long last_vnum = 0;
		long value = area->top_dungeon_vnum + 1;
		for(last_vnum = 1; last_vnum <= area->top_dungeon_vnum; last_vnum++)
		{
			if( !get_dungeon_index(area, last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}

		wnum.pArea = area;
		wnum.vnum = value;
	}

	if( get_dungeon_index(wnum.pArea, wnum.vnum) )
	{
		send_to_char("That dungeon already exists.\n\r", ch);
		return FALSE;
	}

    if (!IS_BUILDER(ch, wnum.pArea))
    {
		send_to_char("BpEdit:  widevnum in an area you cannot build in.\n\r", ch);
		return FALSE;
    }

	dng = new_dungeon_index();
	dng->area = wnum.pArea;
	dng->vnum = wnum.vnum;

	iHash							= dng->vnum % MAX_KEY_HASH;
	dng->next						= wnum.pArea->dungeon_index_hash[iHash];
	wnum.pArea->dungeon_index_hash[iHash]	= dng;
	ch->desc->pEdit					= (void *)dng;

	if( dng->vnum > wnum.pArea->top_dungeon_vnum)
		wnum.pArea->top_dungeon_vnum = dng->vnum;

    return TRUE;
}

DNGEDIT( dngedit_name )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return FALSE;
	}

	free_string(dng->name);
	dng->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_repop )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if( !is_number(argument) )
	{
		send_to_char("Syntax:  repop [age]\n\r", ch);
		return FALSE;
	}

	int repop = atoi(argument);
	dng->repop = UMAX(0, repop);
	send_to_char("Repop changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_description )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->description);
		return TRUE;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return FALSE;
}

DNGEDIT( dngedit_comments )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		string_append(ch, &dng->comments);
		return TRUE;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return FALSE;
}

DNGEDIT( dngedit_areawho )
{
	DUNGEON_INDEX_DATA *dng;
	int value;

	EDIT_DUNGEON(ch, dng);

    if (argument[0] != '\0')
    {
		if ( !str_prefix(argument, "blank") )
		{
			dng->area_who = AREA_BLANK;

			send_to_char("Area who title cleared.\n\r", ch);
			return TRUE;
		}

		if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
		{
			if( value == AREA_INSTANCE || value == AREA_DUTY )
			{
				send_to_char("Area who title only allowed in blueprints.\n\r", ch);
				return FALSE;
			}

			dng->area_who = value;

			send_to_char("Area who title set.\n\r", ch);
			return TRUE;
		}
    }

    send_to_char("Syntax:  areawho [title]\n\r"
				"Type '? areawho' for a list of who titles.\n\r", ch);
    return FALSE;

}

DNGEDIT( dngedit_floors )
{
	DUNGEON_INDEX_DATA *dng;
	char arg[MIL];

	EDIT_DUNGEON(ch, dng);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  floors add <widevnum>\n\r", ch);
		send_to_char("         floors remove #\n\r", ch);
		send_to_char("         floors list\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		BUFFER *buffer = new_buf();

		dngedit_buffer_floors(buffer, dng);

		page_to_char(buffer->string, ch);
		free_buf(buffer);
		return FALSE;
	}

	if( !str_prefix(arg, "add") )
	{
		WNUM wnum;

		if( !parse_widevnum(argument, ch->in_room->area, &wnum) )
		{
			send_to_char("Please specify a widevnum.\n\r", ch);
			return FALSE;
		}

		BLUEPRINT *bp = get_blueprint(wnum.pArea, wnum.vnum);

		if( !bp )
		{
			send_to_char("That blueprint does not exist.\n\r", ch);
			return FALSE;
		}

		if (list_size(bp->entrances) < 1)
		{
			send_to_char("WARNING: Blueprint is missing default entrance.\n\r", ch);
		}

		if (list_size(bp->exits) < 1)
		{
			send_to_char("WARNING: Blueprint is missing default exit.\n\r", ch);
		}

		/*
		// Disabling this type of check because there will be ways to exit a dungeon without using exits
		// This also wouldn't allow single level dungeons with this type of thing
		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{


			if( bp->static_entry_section < 1 || bp->static_entry_link < 1 ||
				bp->static_exit_section < 1 || bp->static_exit_link < 1 )
			{
				send_to_char("Blueprint must have an entrance and exit specified.\n\r", ch);
				return FALSE;
			}
		}
		else
		{
			send_to_char("Blueprint mode not supported yet.\n\r", ch);
			return FALSE;
		}
		*/

		list_appendlink(dng->floors, bp);
		send_to_char("Floor added.\n\r", ch);
		return TRUE;
	}

	if( !str_prefix(arg, "remove") || !str_prefix(arg, "delete") )
	{
		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return FALSE;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(dng->floors) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return FALSE;
		}

		list_remnthlink(dng->floors, index);

		// TODO: Need to go through everything to make sure the floor is no longer referenced

		// Iterate over Level definitions to remove all references to this floor.

		send_to_char("Floor removed.\n\r", ch);
		return TRUE;
	}

	dngedit_floors(ch, "");
	return FALSE;
}

void dungeon_update_level_ordinals(DUNGEON_INDEX_DATA *dng)
{
	int ordinal = 1;
	ITERATOR it;
	DUNGEON_INDEX_LEVEL_DATA *level;

	iterator_start(&it, dng->levels);
	while((level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)))
	{
		if (level->mode == LEVELMODE_GROUP)
		{
			level->ordinal = 0;	// Groups don't have an ordinal on themselves
			ITERATOR git;
			DUNGEON_INDEX_LEVEL_DATA *glevel;
			iterator_start(&git, level->group);
			while((glevel = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
			{
				glevel->ordinal = ordinal++;
			}
			iterator_stop(&git);
		}
		else
			level->ordinal = ordinal++;
	}

	iterator_stop(&it);
}

DNGEDIT( dngedit_levels )
{
	char buf[MSL];
	DUNGEON_INDEX_DATA *dng;
	char arg[MIL];
	char arg2[MIL];
	int floor;
	
	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  levels {Radd{x static <floor>\n\r", ch);
		send_to_char("         levels {Radd{x weighted\n\r", ch);
		send_to_char("         levels {Radd{x grouped\n\r", ch);
		send_to_char("         levels {Rmove{x <#> up|down|top|bottom|first|last\n\r", ch);
		send_to_char("         levels {Rmove{x <from> <to>\n\r", ch);
		send_to_char("         levels {Rweight{x <#> list\n\r", ch);
		send_to_char("         levels {Rweight{x <#> add <weight> <floor>\n\r", ch);
		send_to_char("         levels {Rweight{x <#> set <#> <weight> <floor>\n\r", ch);
		send_to_char("         levels {Rweight{x <#> remove <#>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> add static <floor>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> add weighted\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> move <#> up|down|top|bottom|first|last\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> move <from> <to>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> weight <#> list\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> weight <#> add <weight> <floor>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> weight <#> set <#> <weight> <floor>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> weight <#> remove <#>\n\r", ch);
		send_to_char("         levels {Rgroup{x <#> remove <#>\n\r", ch);
		send_to_char("         levels {Rremove{x <#>\n\r", ch);
		return FALSE;
	}

	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		send_to_char("Please turn off scripted levels to edit levels manually.\n\r", ch);
		return FALSE;
	}

	argument = one_argument(argument, arg);

	// levels add static <floor>
	// levels add weighted
	// levels add grouped
	if (!str_prefix(arg, "add"))
	{
		argument = one_argument(argument, arg2);

		if (!str_prefix(arg2, "static"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Syntax: levels add static <floor>\n\r", ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				send_to_char("Please specify a number for a floor.", ch);
				return FALSE;
			}

			if (list_size(dng->floors) < 1)
			{
				send_to_char("Please create floors first.\n\r", ch);
				return FALSE;
			}

			floor = atoi(argument);
			if ( floor <= 0 || floor > list_size(dng->floors))
			{
				sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_STATIC;
			level->floor = floor;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);

			sprintf(buf, "Static level %d added.\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return TRUE;
		}

		if (!str_prefix(arg2, "weighted"))
		{
			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_WEIGHTED;
			level->floor = 0;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);

			sprintf(buf, "Weighted Random level %d added.\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return TRUE;
		}

		if (!str_prefix(arg2, "grouped"))
		{
			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_GROUP;
			level->floor = 0;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);

			sprintf(buf, "Group level %d added.\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return TRUE;
		}

		send_to_char("Invalid type of level.  Please specify either static, weighted floors or grouped levels.\n\r", ch);
		return FALSE;
	}

	// levels move <#> up|down|top|first|bottom|last
	// levels move <from> <to>
	if (!str_prefix(arg, "move"))
	{
		char arg3[MIL];

		argument = one_argument(argument, arg2);
		if (!is_number(arg2))
		{
			send_to_char("Please specify a valid level number.", ch);
			return FALSE;
		}

		int index = atoi(arg2);
		if (index <= 0 || index > list_size(dng->levels))
		{
			sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return FALSE;
		}

		int to_index = -1;
		argument = one_argument(argument, arg3);
		if (is_number(arg3))
		{
			// levels move <from> <to>
			to_index = atoi(arg3);
			if (to_index < 1 || to_index > list_size(dng->levels))
			{
				sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
				send_to_char(buf, ch);
				return FALSE;
			}
		}
		else if (!str_prefix(arg3, "up"))
		{
			if (index <= 1)
			{
				send_to_char("That level cannot move up any further.\n\r", ch);
				return FALSE;
			}

			to_index = index - 1;
		}
		else if (!str_prefix(arg3, "down"))
		{
			if (index >= list_size(dng->levels))
			{
				send_to_char("That level cannot move down any further.\n\r", ch);
				return FALSE;
			}

			to_index = index + 1;
		}
		else if (!str_prefix(arg3, "top") || !str_prefix(arg3, "first"))
		{
			if (index <= 1)
			{
				send_to_char("That level is already up as far as it can go.\n\r", ch);
				return FALSE;
			}

			to_index = 1;
		}
		else if (!str_prefix(arg3, "bottom") || !str_prefix(arg3, "last"))
		{
			if (index >= list_size(dng->levels))
			{
				send_to_char("That level is already down ass far as it can go.\n\r", ch);
				return FALSE;
			}

			to_index = list_size(dng->levels);
		}
		else
		{
			send_to_char("Syntax:  levels move <#> up|down|top|bottom|first|last\n\r", ch);
			send_to_char("         levels move <from> <to>\n\r", ch);
			return FALSE;
		}
		
		if (index == to_index)
		{
			send_to_char("You shove the level as hard as possible, barely moving.\n\r", ch);
			return FALSE;
		}

		list_movelink(dng->levels, index, to_index);
		dungeon_update_level_ordinals(dng);
		send_to_char("Level moved.\n\r", ch);
		return TRUE;
	}

	// levels weight <#> list
	// levels weight <#> add <weight> <floor>
	// levels weight <#> set <#> <weight> <floor>
	// levels weight <#> remove <#>
	if (!str_prefix(arg, "weight"))
	{
		char arg3[MIL];
		//char arg4[MIL];
		int index;
		DUNGEON_INDEX_LEVEL_DATA *level;
		//DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;

		argument = one_argument(argument, arg2);
		if (!is_number(arg2))
		{
			send_to_char("Please specify a valid level number.", ch);
			return FALSE;
		}

		index = atoi(arg2);
		if (index <= 0 || index > list_size(dng->levels))
		{
			sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(dng->levels));
			return FALSE;
		}

		level = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, index);
		if (!IS_VALID(level))
		{
			send_to_char("Failed to retrieve level information.\n\r", ch);
			return FALSE;
		}

		if (level->mode != LEVELMODE_WEIGHTED)
		{
			send_to_char("That level is not a weighted random level.  Please specify a weighted random level.\n\r", ch);
			return FALSE;
		}

		argument = one_argument(argument, arg3);

		if (!str_prefix(arg3, "list"))
		{
			BUFFER *buffer = new_buf();

			add_buf(buffer, "     [ Weight ] [ Floor ] [   Vnum   ] [             Name             ]\n\r");
			add_buf(buffer, "------------------------------------------------------------------------\n\r");

			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
			ITERATOR wit;
			iterator_start(&wit, level->weighted_floors);

			int row = 0;
			while((weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
			{
				BLUEPRINT *bp = list_nthdata(dng->floors, weighted->floor);

				snprintf(buf, MSL-1, "%4d   %6d     %5d     %8ld    %30.30s\n\r", ++row, weighted->weight, weighted->floor, bp->vnum, bp->name);
				add_buf(buffer, buf);
			}

			iterator_stop(&wit);

			if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
			{
				send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
			}
			else
			{
				page_to_char(buffer->string, ch);
			}
			
			free_buf(buffer);
		}
		else if (!str_prefix(arg3, "add"))
		{
			// levels weight <#> add <weight> <floor>
			char arg4[MIL];

			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  levels weight <#> add <weight> <floor>\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg4);

			if (!is_number(arg4))
			{
				send_to_char("Please specify a positive number for the weight.\n\r", ch);
				return FALSE;
			}

			int weight = atoi(arg4);
			if (weight < 1)
			{
				send_to_char("Please specify a positive number for the weight.\n\r", ch);
				return FALSE;
			}

			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  levels weight <#> add <weight> <floor>\n\r", ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			int floor = atoi(argument);
			if (floor < 1 || floor > list_size(dng->floors))
			{
				sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
			weighted->weight = weight;
			weighted->floor = floor;

			list_appendlink(level->weighted_floors, weighted);
			level->total_weight += weight;

			send_to_char("Weighted Random entry added.\n\r", ch);
			return TRUE;
		}
		else if (!str_prefix(arg3, "set"))
		{
			// levels weight <#> set <#> <weight> <floor>
			char arg4[MIL];
			char arg5[MIL];

			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  levels weight <#> set <#> <weight> <floor>\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg4);
			argument = one_argument(argument, arg5);

			if (!is_number(arg4))
			{
				sprintf(buf, "Please specify a weighted random entry number from 1 to %d.\n\r", list_size(level->weighted_floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			int index = atoi(arg4);
			if (index < 1 || index > list_size(level->weighted_floors))
			{
				sprintf(buf, "Please specify a weighted random entry number from 1 to %d.\n\r", list_size(level->weighted_floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			if (!is_number(arg5))
			{
				send_to_char("Please specify a positive number for the weight.\n\r", ch);
				return FALSE;
			}

			int weight = atoi(arg5);
			if (weight < 1)
			{
				send_to_char("Please specify a positive number for the weight.\n\r", ch);
				return FALSE;
			}

			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  levels weight <#> set <#> <weight> <floor>\n\r", ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			int floor = atoi(argument);
			if (floor < 1 || floor > list_size(dng->floors))
			{
				sprintf(buf, "Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
				send_to_char(buf, ch);
				return FALSE;
			}


			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);

			level->total_weight -= weighted->weight;

			weighted->weight = weight;
			weighted->floor = floor;

			level->total_weight += weight;

			send_to_char("Weighted Random entry set.\n\r", ch);
			return TRUE;
		}
		else if (!str_prefix(arg3, "remove"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  levels weight <#> remove <#>\n\r", ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				send_to_char("Please specify a number.\n\r", ch);
				return FALSE;
			}

			int index = atoi(argument);
			if (index < 1 || index > list_size(level->weighted_floors))
			{
				sprintf(buf, "Invalid weight entry index.  Please specify a value from 1 to %d.\n\r", list_size(level->weighted_floors));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);
			
			// Sanity check
			if (weighted)
			{
				level->total_weight -= weighted->weight;
			}

			list_remnthlink(level->weighted_floors, index);

			send_to_char("Weight Random entry removed.\n\r", ch);
			return FALSE;
		}
	}

	// levels group <#> add static <floor>\n\r", ch);
	// levels group <#> add weighted\n\r", ch);
	// levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
	// levels group <#> move <from> <to>\n\r", ch);
	// levels group <#> weight <#> list\n\r", ch);
	// levels group <#> weight <#> add <weight> <floor>\n\r", ch);
	// levels group <#> weight <#> set <#> <weight> <floor>\n\r", ch);
	// levels group <#> weight <#> remove <#>\n\r", ch);
	// levels group <#> remove <#>\n\r", ch);
	if (!str_prefix(arg, "group"))
	{
		if (!is_number(arg2))
		{
			send_to_char("Syntax:  levels group {R<#>{x <command>\n\r", ch);
			send_to_char("         Please specify a number.\n\r", ch);
			return FALSE;
		}

		int index = atoi(arg2);
		if (index < 1 || index > list_size(dng->levels))
		{
			send_to_char("Syntax:  levels group {R<#>{x <command>\n\r", ch);
			sprintf(buf, "         Please specify a group number from 1 to %d.\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return FALSE;
		}

		DUNGEON_INDEX_LEVEL_DATA *level = list_nthdata(dng->levels, index);
		if (!IS_VALID(level))
		{
			send_to_char("No such level exists.\n\r", ch);
			return FALSE;
		}

		if (level->mode != LEVELMODE_GROUP)
		{
			sprintf(buf, "Level %d is not a group level set.\n\r", index);
			send_to_char(buf, ch);
			return FALSE;
		}

		if (IS_NULLSTR(argument))
		{
			send_to_char("Syntax:  levels group <#> {Radd{x static <floor>\n\r", ch);
			send_to_char("         levels group <#> {Radd{x weighted\n\r", ch);
			send_to_char("         levels group <#> {Rmove{x <#> up|down|top|bottom|first|last\n\r", ch);
			send_to_char("         levels group <#> {Rmove{x <from> <to>\n\r", ch);
			send_to_char("         levels group <#> {Rweight{x <#> list\n\r", ch);
			send_to_char("         levels group <#> {Rweight{x <#> add <weight> <floor>\n\r", ch);
			send_to_char("         levels group <#> {Rweight{x <#> set <#> <weight> <floor>\n\r", ch);
			send_to_char("         levels group <#> {Rweight{x <#> remove <#>\n\r", ch);
			send_to_char("         levels group <#> {Rremove{x <#>\n\r", ch);
			return FALSE;
		}

		char arg3[MIL];

		argument = one_argument(argument, arg3);

		// levels group <#> add static <floor>
		// levels group <#> add weighted
		if (!str_prefix(arg3, "add"))
		{
			if (IS_NULLSTR(argument))
			{
				send_to_char("Syntax:  levels group <#> add {Rstatic{x <floor>\n\r", ch);
				send_to_char("         levels group <#> add {Rweighted{x\n\r", ch);
				return FALSE;
			}

			char arg4[MIL];
			argument = one_argument(argument, arg4);

			if (!str_prefix(arg4, "static"))
			{
				if (IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  levels group <#> add static <floor>\n\r", ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  levels group <#> add static {R<floor>{x\n\r", ch);
					send_to_char("         Please specify a number.\n\r", ch);
					return FALSE;
				}

				int floor = atoi(argument);
				if (floor < 1 || floor > list_size(dng->floors))
				{
					send_to_char("Syntax:  levels group <#> add static {R<floor>{x\n\r", ch);
					sprintf(buf, "         Please specify a floor number between 1 and %d\n\r", list_size(dng->floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *lvl = new_dungeon_index_level();
				lvl->mode = LEVELMODE_STATIC;
				lvl->floor = floor;
				list_appendlink(level->group, lvl);
				dungeon_update_level_ordinals(dng);

				sprintf(buf, "Static level added to Group Level %d.\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

			if (!str_prefix(arg4, "weighted"))
			{
				DUNGEON_INDEX_LEVEL_DATA *lvl = new_dungeon_index_level();
				lvl->mode = LEVELMODE_WEIGHTED;
				lvl->floor = 0;
				list_appendlink(level->group, lvl);
				dungeon_update_level_ordinals(dng);

				sprintf(buf, "Weighted Random level added to Group Level %d.\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

		}

		// levels group <#> move <#> up|down|top|first|bottom|last
		// levels group <#> move <from> <to>
		if (!str_prefix(arg3, "move"))
		{
			if (IS_NULLSTR(argument))
			{
				send_to_char("Syntax:  levels group <#> move <#> up|down|top|first|bottom|last\n\r", ch);
				send_to_char("         levels group <#> move <from> <to>\n\r", ch);
				return FALSE;
			}

			char arg4[MIL];
			//char arg5[MIL];

			argument = one_argument(argument, arg4);
			if (!is_number(arg4))
			{
				send_to_char("Please specify a valid level number.", ch);
				return FALSE;
			}

			int entry = atoi(arg4);
			if (entry <= 0 || entry > list_size(level->group))
			{
				send_to_char("Syntax:  levels group <#> move {R<#>{x up|down|top|first|bottom|last\n\r", ch);
				send_to_char("         levels group <#> move {R<from>{x <to>\n\r", ch);
				sprintf(buf, "         Please specify a level number from 1 to %d.\n\r", list_size(level->group));
				send_to_char(buf, ch);
				return FALSE;
			}

			int to_entry = -1;
			if (is_number(argument))
			{
				// levels move <from> <to>
				to_entry = atoi(argument);
				if (to_entry <= 0 || to_entry > list_size(dng->levels))
				{
					send_to_char("Syntax:  levels group <#> move {R<#>{x up|down|top|first|bottom|last\n\r", ch);
					send_to_char("         levels group <#> move {R<from>{x <to>\n\r", ch);
					sprintf(buf, "         Please specify a level number from 1 to %d.\n\r", list_size(level->group));
					return FALSE;
				}
			}
			else if (!str_prefix(argument, "up"))
			{
				if (entry <= 1)
				{
					send_to_char("That level cannot move up any further.\n\r", ch);
					return FALSE;
				}

				to_entry = entry - 1;
			}
			else if (!str_prefix(argument, "down"))
			{
				if (entry >= list_size(level->group))
				{
					send_to_char("That level cannot move down any further.\n\r", ch);
					return FALSE;
				}

				to_entry = entry + 1;
			}
			else if (!str_prefix(argument, "top") || !str_prefix(argument, "first"))
			{
				if (entry <= 1)
				{
					send_to_char("That level is already up as far as it can go.\n\r", ch);
					return FALSE;
				}

				to_entry = 1;
			}
			else if (!str_prefix(argument, "bottom") || !str_prefix(argument, "last"))
			{
				if (entry >= list_size(level->group))
				{
					send_to_char("That level is already down ass far as it can go.\n\r", ch);
					return FALSE;
				}

				to_entry = list_size(level->group);
			}
			else
			{
				send_to_char("Syntax:  levels group <#> move <#> up|down|top|bottom|first|last\n\r", ch);
				send_to_char("         levels group <#>s move <from> <to>\n\r", ch);
				return FALSE;
			}
			
			if (entry == to_entry)
			{
				send_to_char("You shove the level as hard as possible, barely moving.\n\r", ch);
				return FALSE;
			}

			list_movelink(level->group, entry, to_entry);
			dungeon_update_level_ordinals(dng);

			send_to_char("Level moved.\n\r", ch);
			return TRUE;
		}

		// levels group <#> weight <#> list
		// levels group <#> weight <#> add <weight> <floor>
		// levels group <#> weight <#> set <#> <weight> <floor>
		// levels group <#> weight <#> remove <#>
		if (!str_prefix(arg3, "weight"))
		{
			char arg4[MIL];
			char arg5[MIL];
			//char arg6[MIL];
			DUNGEON_INDEX_LEVEL_DATA *lvl;
			//DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;

			argument = one_argument(argument, arg4);
			if (!is_number(arg4))
			{
				send_to_char("Please specify a valid level number.", ch);
				return FALSE;
			}

			int entry = atoi(arg4);
			if (entry < 1 || entry > list_size(level->group))
			{
				sprintf(buf, "Please specify a level number from 1 to %d.\n\r", list_size(level->group));
				return FALSE;
			}

			lvl = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(level->group, entry);
			if (!IS_VALID(lvl))
			{
				send_to_char("Failed to retrieve level information.\n\r", ch);
				return FALSE;
			}

			if (lvl->mode != LEVELMODE_WEIGHTED)
			{
				send_to_char("That level is not a weighted random level.  Please specify a weighted random level.\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg5);

			if (!str_prefix(arg5, "list"))
			{
				BUFFER *buffer = new_buf();

				add_buf(buffer, "     [ Weight ] [ Floor ] [   Vnum   ] [             Name             ]\n\r");
				add_buf(buffer, "------------------------------------------------------------------------\n\r");

				DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
				ITERATOR wit;
				iterator_start(&wit, lvl->weighted_floors);

				int row = 0;
				while((weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
				{
					BLUEPRINT *bp = list_nthdata(dng->floors, weighted->floor);

					snprintf(buf, MSL-1, "%4d   %6d     %5d     %8ld    %30.30s\n\r", ++row, weighted->weight, weighted->floor, bp->vnum, bp->name);
					add_buf(buffer, buf);
				}

				iterator_stop(&wit);

				if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
				{
					send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
				}
				else
				{
					page_to_char(buffer->string, ch);
				}
				
				free_buf(buffer);
			}
			else if (!str_prefix(arg5, "add"))
			{
				// levels group <#> weight <#> add <weight> <floor>
				char arg6[MIL];

				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  levels group <#> weight <#> add <weight> <floor>\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg6);

				if (!is_number(arg6))
				{
					send_to_char("Syntax:  levels group <#> weight <#> add {R<weight>{x <floor>\n\r", ch);
					send_to_char("         Please specify a positive number for the weight.\n\r", ch);
					return FALSE;
				}

				int weight = atoi(arg6);
				if (weight < 1)
				{
					send_to_char("Syntax:  levels group <#> weight <#> add {R<weight>{x <floor>\n\r", ch);
					send_to_char("         Please specify a positive number for the weight.\n\r", ch);
					return FALSE;
				}

				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  levels group <#> weight <#> add <weight> <floor>\n\r", ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  levels group <#> weight <#> add <weight> {R<floor>{x\n\r", ch);
					sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				int floor = atoi(argument);
				if (floor < 1 || floor > list_size(dng->floors))
				{
					send_to_char("Syntax:  levels group <#> weight <#> add <weight> {R<floor>{x\n\r", ch);
					sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
				weighted->weight = weight;
				weighted->floor = floor;

				list_appendlink(lvl->weighted_floors, weighted);
				lvl->total_weight += weight;

				send_to_char("Weighted Random entry added.\n\r", ch);
				return TRUE;
			}
			else if (!str_prefix(arg5, "set"))
			{
				// levels weight <#> set <#> <weight> <floor>
				char arg6[MIL];
				char arg7[MIL];

				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> {R<weight> <floor>{x\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg6);
				argument = one_argument(argument, arg7);

				if (!is_number(arg6))
				{
					send_to_char("Syntax:  levels group <#> weight <#> set {R<#>{x <weight> <floor>\n\r", ch);
					sprintf(buf, "         Please specify a weighted random entry number from 1 to %d.\n\r", list_size(lvl->weighted_floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				int index = atoi(arg6);
				if (index < 1 || index > list_size(lvl->weighted_floors))
				{
					send_to_char("Syntax:  levels group <#> weight <#> set {R<#>{x <weight> <floor>\n\r", ch);
					sprintf(buf, "         Please specify a weighted random entry number from 1 to %d.\n\r", list_size(lvl->weighted_floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!is_number(arg7))
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> {R<weight>{x <floor>\n\r", ch);
					send_to_char("         Please specify a positive number for the weight.\n\r", ch);
					return FALSE;
				}

				int weight = atoi(arg7);
				if (weight < 1)
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> {R<weight>{x <floor>\n\r", ch);
					send_to_char("         Please specify a positive number for the weight.\n\r", ch);
					return FALSE;
				}

				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {R<floor>{x\n\r", ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {R<floor>{x\n\r", ch);
					sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				int floor = atoi(argument);
				if (floor < 1 || floor > list_size(dng->floors))
				{
					send_to_char("Syntax:  levels group <#> weight <#> set <#> <weight> {R<floor>{x\n\r", ch);
					sprintf(buf, "         Please specify a floor number from 1 to %d.\n\r", list_size(dng->floors));
					send_to_char(buf, ch);
					return FALSE;
				}


				DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(level->weighted_floors, index);

				lvl->total_weight -= weighted->weight;

				weighted->weight = weight;
				weighted->floor = floor;

				lvl->total_weight += weight;

				send_to_char("Weighted Random entry set.\n\r", ch);
				return TRUE;
			}
			else if (!str_prefix(arg5, "remove"))
			{
				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  levels group <#> weight <#> remove {R<#>{x\n\r", ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  levels group <#> weight <#> remove {R<#>{x\n\r", ch);
					send_to_char("         Please specify a number.\n\r", ch);
					return FALSE;
				}

				int index = atoi(argument);
				if (index < 1 || index > list_size(lvl->weighted_floors))
				{
					sprintf(buf, "Invalid weight entry index.  Please specify a value from 1 to %d.\n\r", list_size(lvl->weighted_floors));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)list_nthdata(lvl->weighted_floors, index);
				
				// Sanity check
				if (weighted)
				{
					lvl->total_weight -= weighted->weight;
				}

				list_remnthlink(lvl->weighted_floors, index);

				send_to_char("Weight Random entry removed.\n\r", ch);
				return FALSE;
			}
		}

		// levels group <#> remove <#>
		if (!str_prefix(arg3, "remove"))
		{
			if (IS_NULLSTR(argument))
			{
				send_to_char("Syntax:  levels group <#> remove {R<#>{x\n\r", ch);
				send_to_char("         Please give a number.\n\r", ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				send_to_char("Syntax:  levels group <#> remove {R<#>{x\n\r", ch);
				send_to_char("         Please give a number.\n\r", ch);
				return FALSE;
			}

			int index = atoi(argument);
			if (index < 1 || index > list_size(level->group))
			{
				send_to_char("Syntax:  levels group <#> remove {R<#>{x\n\r", ch);
				sprintf(buf, "         Please give a number between 1 and %d\n\r", list_size(level->group));
				send_to_char(buf, ch);
				return FALSE;
			}
		
			list_remnthlink(level->group, index);
			dungeon_update_level_ordinals(dng);

			send_to_char("Level removed.\n\r", ch);
			return TRUE;
		}

		dngedit_levels(ch, "group");
	}

	// levels remove <#>
	if (!str_prefix(arg, "remove"))
	{
		if (IS_NULLSTR(argument))
		{
			send_to_char("Syntax:  levels remove {R<#>{x\n\r", ch);
			send_to_char("         Please give a number.\n\r", ch);
			return FALSE;
		}

		if (!is_number(argument))
		{
			send_to_char("Syntax:  levels remove {R<#>{x\n\r", ch);
			send_to_char("         Please give a number.\n\r", ch);
			return FALSE;
		}

		int index = atoi(argument);
		if (index < 1 || index > list_size(dng->levels))
		{
			send_to_char("Syntax:  levels remove {R<#>{x\n\r", ch);
			sprintf(buf, "         Please give a number between 1 and %d\n\r", list_size(dng->levels));
			send_to_char(buf, ch);
			return FALSE;
		}

		DUNGEON_INDEX_LEVEL_DATA *level = list_nthdata(dng->levels, index);
		if (!IS_VALID(level))
		{
			send_to_char("Failed to retrieve level information.\n\r", ch);
			return FALSE;
		}
	
		list_remnthlink(dng->levels, index);
		dungeon_update_level_ordinals(dng);

		send_to_char("Level removed.\n\r", ch);
		return TRUE;
	}

	dngedit_levels(ch, "");
	return FALSE;
}

DNGEDIT( dngedit_entry )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  entry <local vnum>\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	value = atol(argument);

	if( !get_room_index(dng->area, value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	dng->entry_room = value;
	send_to_char("Entry room changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_exit )
{
	DUNGEON_INDEX_DATA *dng;
	long value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  exit <local vnum>\n\r", ch);
		return FALSE;
	}

	if( !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return FALSE;
	}

	value = atol(argument);

	if( !get_room_index(dng->area, value) )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return FALSE;
	}

	dng->exit_room = value;
	send_to_char("Exit room changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_flags )
{
	DUNGEON_INDEX_DATA *dng;
	int value;

	EDIT_DUNGEON(ch, dng);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? dungeon' for list of flags.\n\r", ch);
		return FALSE;
	}

	if( (value = flag_value(dungeon_flags, argument)) != NO_FLAG )
	{
		dng->flags ^= value;
		send_to_char("Dungeon flags changed.\n\r", ch);
		return TRUE;
	}

	dngedit_flags(ch, "");
	return FALSE;

}

DNGEDIT( dngedit_zoneout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  zoneout [string]\n\r", ch);
		return FALSE;
	}

	free_string(dng->zone_out);
	dng->zone_out = str_dup(argument);
	send_to_char("ZoneOut changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_portalout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  portalout [string]\n\r", ch);
		return FALSE;
	}

	free_string(dng->zone_out_portal);
	dng->zone_out_portal = str_dup(argument);
	send_to_char("PortalOut changed.\n\r", ch);
	return TRUE;
}

DNGEDIT( dngedit_mountout )
{
	DUNGEON_INDEX_DATA *dng;

	EDIT_DUNGEON(ch, dng);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  mountout [string]\n\r", ch);
		return FALSE;
	}

	free_string(dng->zone_out_mount);
	dng->zone_out_mount = str_dup(argument);
	send_to_char("MountOut changed.\n\r", ch);
	return TRUE;
}

static int get_blueprint_entrance_count(BLUEPRINT *bp)
{
	return list_size(bp->entrances);
}

static int get_blueprint_exit_count(BLUEPRINT *bp)
{
	return list_size(bp->exits);
}

int dungeon_index_generation_count(DUNGEON_INDEX_DATA *dng)
{
	int count = 0;
	ITERATOR it;
	DUNGEON_INDEX_LEVEL_DATA *level;
	iterator_start(&it, dng->levels);
	while((level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)))
	{
		if (level->mode == LEVELMODE_GROUP)
			count = list_size(level->group);
		else
			count++;
	}
	iterator_stop(&it);
	return count;
}

int get_dungeon_index_level_special_entrances(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *level)
{
	BLUEPRINT *bp;
	switch(level->mode)
	{
	case LEVELMODE_STATIC:
		bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);

		return get_blueprint_entrance_count(bp);

	case LEVELMODE_WEIGHTED:
		{
			int min_count = -1;

			ITERATOR wit;
			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
			iterator_start(&wit, level->weighted_floors);
			while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
			{
				bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);

				int count = get_blueprint_entrance_count(bp);

				if (min_count < 0 || count < min_count)
					min_count = count;
			}
			iterator_stop(&wit);
			return UMAX(0, min_count);
		}

	case LEVELMODE_GROUP:
		{
			int min_count = -1;

			ITERATOR git;
			DUNGEON_INDEX_LEVEL_DATA *glevel;
			iterator_start(&git, level->group);
			while( (glevel = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
			{
				int count = get_dungeon_index_level_special_entrances(dng, glevel);

				if (min_count < 0 || count < min_count)
					min_count = count;
			}
			iterator_stop(&git);
			return UMAX(0, min_count);
		}
	}

	return 0;
}


int get_dungeon_index_level_special_exits(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *level)
{
	BLUEPRINT *bp;
	switch(level->mode)
	{
	case LEVELMODE_STATIC:
		bp = (BLUEPRINT *)list_nthdata(dng->floors, level->floor);

		return get_blueprint_exit_count(bp);

	case LEVELMODE_WEIGHTED:
		{
			int min_count = -1;

			ITERATOR wit;
			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted;
			iterator_start(&wit, level->weighted_floors);
			while( (weighted = (DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *)iterator_nextdata(&wit)))
			{
				bp = (BLUEPRINT *)list_nthdata(dng->floors, weighted->floor);

				int count = get_blueprint_exit_count(bp);

				if (min_count < 0 || count < min_count)
					min_count = count;
			}
			iterator_stop(&wit);
			return UMAX(0, min_count);
		}

	case LEVELMODE_GROUP:
		{
			int min_count = -1;

			ITERATOR git;
			DUNGEON_INDEX_LEVEL_DATA *glevel;
			iterator_start(&git, level->group);
			while( (glevel = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&git)))
			{
				int count = get_dungeon_index_level_special_exits(dng, glevel);

				if (min_count < 0 || count < min_count)
					min_count = count;
			}
			iterator_stop(&git);
			return UMAX(0, min_count);
		}
	}

	return 0;
}

void add_dungeon_index_weighted_exit_data(LLIST *list, int weight, int level, int door)
{
	DUNGEON_INDEX_WEIGHTED_EXIT_DATA *weighted = new_weighted_random_exit();

	weighted->weight = weight;
	weighted->level = level;
	weighted->door = door;

	list_appendlink(list, weighted);
}

DNGEDIT( dngedit_special )
{
	DUNGEON_INDEX_DATA *dng;
	char arg[MIL];
	char arg2[MIL];
	char arg3[MIL];
	char arg4[MIL];
	char arg5[MIL];
	char arg6[MIL];
	char arg7[MIL];

	EDIT_DUNGEON(ch, dng);

	argument = one_argument(argument, arg);

	if (arg[0] == '\0')
	{
		send_to_char("Syntax:  special room list\n\r", ch);
		send_to_char("         special room add generated|ordinal <level> <special room> <name>\n\r", ch);
		send_to_char("         special room # remove\n\r", ch);
		send_to_char("         special room # name <name>\n\r", ch);
		send_to_char("         special room # level generated|ordinal <level>\n\r", ch);
		send_to_char("         special room # room <special room>\n\r", ch);
		send_to_char("         special exit list\n\r", ch);
		send_to_char("         special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit add source generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit add destination generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit add weighted\n\r", ch);
		send_to_char("         special exit add group\n\r", ch);
		send_to_char("         special exit from # list\n\r", ch);
		send_to_char("         special exit from # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit from # set # <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit from # remove #\n\r", ch);
		send_to_char("         special exit to # list\n\r", ch);
		send_to_char("         special exit to # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit to # set # <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit to # remove #\n\r", ch);
		send_to_char("         special exit list\n\r", ch);
		send_to_char("         special exit group # add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit group # add source generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit group # add destination generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit group # add weighted\n\r", ch);
		send_to_char("         special exit group # add group\n\r", ch);
		send_to_char("         special exit group # from # list\n\r", ch);
		send_to_char("         special exit group # from # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit group # from # set # <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit group # from # remove #\n\r", ch);
		send_to_char("         special exit group # to # list\n\r", ch);
		send_to_char("         special exit group # to # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit group # to # set # <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit group # to # remove #\n\r", ch);
		send_to_char("         special exit group # remove #\n\r", ch);
		send_to_char("         special exit remove #\n\r", ch);
		return FALSE;
	}

	if (IS_SET(dng->flags, DUNGEON_SCRIPTED_LEVELS))
	{
		send_to_char("Please turn off scripted levels to edit special rooms/exits manually.\n\r", ch);
		return FALSE;
	}

	if (!str_prefix(arg, "room"))
	{
		argument = one_argument(argument, arg2);

		if( !str_prefix(arg2, "list") )
		{
			if( list_size(dng->special_rooms) > 0 )
			{
				BUFFER *buffer = new_buf();
				DUNGEON_INDEX_SPECIAL_ROOM *special;

				char buf[MSL];
				int line = 0;

				ITERATOR sit;

				add_buf(buffer, "     [             Name             ] [ Level ] [ Room ]\n\r");
				add_buf(buffer, "---------------------------------------------------------\n\r");

				iterator_start(&sit, dng->special_rooms);
				while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
				{
					snprintf(buf, MSL - 1, "%4d %30.30s{x    {%c%5d{x     %4d\n\r", ++line, special->name, (special->level<0?'G':(special->level>0?'Y':'W')), special->level, special->room);
					buf[MSL-1] = '\0';
					add_buf(buffer, buf);
				}

				iterator_stop(&sit);
				add_buf(buffer, "---------------------------------------------------------d\n\r");
				add_buf(buffer, "{YYELLOW{x - Generated level position index\n\r");
				add_buf(buffer, "{GGREEN{x  - Ordinal level position index\n\r");

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
			{
				send_to_char("Dungeon has no special rooms defined.\n\r", ch);
			}

			return FALSE;
		}

		if( is_number(arg2) )
		{
			int index = atoi(arg2);

			DUNGEON_INDEX_SPECIAL_ROOM *special = list_nthdata(dng->special_rooms, index);

			if( !IS_VALID(special) )
			{
				send_to_char("No such special room.\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg3);
			if( arg3[0] == '\0' )
			{
				dngedit_special(ch, "");
				return FALSE;
			}

			if( !str_prefix(arg3, "remove") || !str_prefix(arg3, "delete") )
			{
				list_remnthlink(dng->special_rooms, index);

				send_to_char("Special room deleted.\n\r", ch);
				return TRUE;
			}

			if( !str_prefix(arg3, "level") )
			{
				bool mode = TRISTATE;

				int levels = dungeon_index_generation_count(dng);

				argument = one_argument(argument, arg4);
				if (!str_prefix(arg4, "generated"))
					mode = FALSE;
				else if (!str_prefix(arg4, "ordinal"))
					mode = TRUE;
				else
				{
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				if( !is_number(argument) )
				{
					send_to_char("That is not a number.\n\r", ch);
					return FALSE;
				}

				int level = atoi(argument);
				if( level < 1 || level > levels )
				{
					send_to_char("Level out of range.\n\r", ch);
					return FALSE;
				}

				special->level = mode ? -level : level;
				special->room = -1;

				send_to_char("Level changed.\n\r", ch);
				return TRUE;
			}

			if( !str_prefix(arg3, "name") )
			{
				argument = one_argument(argument, arg4);

				if( IS_NULLSTR(arg4) )
				{
					send_to_char("Syntax:  special room # name [name]\n\r", ch);
					return FALSE;
				}


				smash_tilde(arg4);
				free_string(special->name);
				special->name = str_dup(arg4);

				send_to_char("Special room name changed.\n\r", ch);
				return TRUE;
			}

			if( !str_prefix(arg3, "room") )
			{
				argument = one_argument(argument, arg4);
				if( !is_number(arg4))
				{
					send_to_char("That is not a number.\n\r", ch);
					return FALSE;
				}

				int room = atol(arg4);

				special->room = room;

				send_to_char("Special room changed.\n\r", ch);
				return TRUE;
			}
		}
		else if( !str_prefix(arg2, "add") )
		{
			bool mode = TRISTATE;
			char argm[MIL];
			argument = one_argument(argument, argm);
			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);

			if( argument[0] == '\0' )
			{
				send_to_char("Syntax:  special room add generated|ordinal [level] [special room] [name]\n\r", ch);
				return FALSE;
			}

			if (!str_prefix(argm, "generated"))
				mode = FALSE;
			else if (!str_prefix(argm, "ordinal"))
				mode = TRUE;
			else
			{
				send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
				return FALSE;
			}

			if( !is_number(arg3) || !is_number(arg4) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return FALSE;
			}

			int level = atoi(arg3);
			int room = atoi(arg4);
			int levels = dungeon_index_generation_count(dng);

			if( level < 1 || level > levels )
			{
				send_to_char("Level out of range.\n\r", ch);
				return FALSE;
			}

			char name[MIL+1];
			strncpy(name, argument, MIL);
			name[MIL] = '\0';
			smash_tilde(name);

			DUNGEON_INDEX_SPECIAL_ROOM *special = new_dungeon_index_special_room();

			free_string(special->name);
			special->name = str_dup(name);
			special->level = mode ? -level : level;
			special->room = room;

			list_appendlink(dng->special_rooms, special);

			send_to_char("Special Room added.\n\r", ch);
			return TRUE;
		}


		send_to_char("Syntax:  special {Wroom{x list\n\r", ch);
		send_to_char("         special {Wroom{x add generated|ordinal <level> <special room> <name>\n\r", ch);
		send_to_char("         special {Wroom{x # remove\n\r", ch);
		send_to_char("         special {Wroom{x # name <name>\n\r", ch);
		send_to_char("         special {Wroom{x # level <level>\n\r", ch);
		send_to_char("         special {Wroom{x # room <special room>\n\r", ch);
		return FALSE;
	}

	if (!str_prefix(arg, "exit"))
	{
		argument = one_argument(argument, arg2);

		if (!str_prefix(arg2, "list"))
		{
			if (list_size(dng->special_exits) > 0)
			{
				char buf[MSL];
				ITERATOR it;
				DUNGEON_INDEX_SPECIAL_EXIT *dsex;
				BUFFER *buffer = new_buf();

				add_buf(buffer, "{x     [    Mode    ]{x\n\r");
				add_buf(buffer, "{x========================================================{x\n\r");

				int exitno = 1;
				iterator_start(&it, dng->special_exits);
				while ( (dsex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)))
				{
					switch(dsex->mode)
					{
						case EXITMODE_STATIC:
						{
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
							sprintf(buf, "%4d  {Y   STATIC  {x\n\r", exitno++);
							add_buf(buffer, buf);
							sprintf(buf, "          Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
							add_buf(buffer, buf);
							sprintf(buf, "          Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
							add_buf(buffer, buf);
							break;	
						}

						case EXITMODE_WEIGHTED_SOURCE:
						{
							ITERATOR wit;
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->to, 1);
							sprintf(buf, "%4d  {C   SOURCE   {x\n\r", exitno++);
							add_buf(buffer, buf);

							int fromexitno = 1;
							add_buf(buffer, "          Source:\n\r");
							add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
							add_buf(buffer, "          ===================================={x\n\r");
							iterator_start(&wit, dsex->from);
							while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
							{
								sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
								add_buf(buffer, buf);
							}
							iterator_stop(&wit);
							add_buf(buffer, "          ----------------------------------------------------{x\n\r");

							sprintf(buf, "          Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
							add_buf(buffer, buf);
							break;
						}

						case EXITMODE_WEIGHTED_DEST:
						{
							ITERATOR wit;
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(dsex->from, 1);
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
							sprintf(buf, "%4d  {C DESTINATION{x\n\r", exitno++);
							add_buf(buffer, buf);

							sprintf(buf, "          Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
							add_buf(buffer, buf);

							int toexitno = 1;
							add_buf(buffer, "          Destination:\n\r");
							add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
							add_buf(buffer, "          ===================================={x\n\r");
							iterator_start(&wit, dsex->to);
							while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
							{
								sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
								add_buf(buffer, buf);
							}
							iterator_stop(&wit);
							add_buf(buffer, "          ----------------------------------------------------{x\n\r");
							break;
						}

						case EXITMODE_WEIGHTED:
						{
							ITERATOR wit;
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
							DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
							sprintf(buf, "%4d  {C  WEIGHTED  {x\n\r", exitno++);
							add_buf(buffer, buf);

							int fromexitno = 1;
							add_buf(buffer, "          Source:\n\r");
							add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
							add_buf(buffer, "          ===================================={x\n\r");
							iterator_start(&wit, dsex->from);
							while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
							{
								sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
								add_buf(buffer, buf);
							}
							iterator_stop(&wit);
							add_buf(buffer, "          ----------------------------------------------------{x\n\r");

							int toexitno = 1;
							add_buf(buffer, "          Destination:\n\r");
							add_buf(buffer, "               [ Weight ] [ Level ] [ Exit# ]{x\n\r");
							add_buf(buffer, "          ===================================={x\n\r");
							iterator_start(&wit, dsex->to);
							while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
							{
								sprintf(buf, "          %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
								add_buf(buffer, buf);
							}
							iterator_stop(&wit);
							add_buf(buffer, "          ----------------------------------------------------{x\n\r");
							break;
						}

						case EXITMODE_GROUP:
						{
							int count = list_size(dsex->group);
							sprintf(buf, "%4d  {G    GROUP   {x\n\r", exitno++);
							add_buf(buffer, buf);
							if (count > 0)
							{
								ITERATOR git;
								DUNGEON_INDEX_SPECIAL_EXIT *gex;

								add_buf(buffer, "{x               [    Mode    ]{x\n\r");
								add_buf(buffer, "{x          ========================================================{x\n\r");

								int gexitno = 1;
								iterator_start(&git, dsex->group);
								while ( (gex = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&git)) )
								{
									switch(gex->mode)
									{
										case EXITMODE_STATIC:
										{
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
											sprintf(buf, "          %4d  {Y   STATIC   {x\n\r", gexitno++);
											sprintf(buf, "                    Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
											add_buf(buffer, buf);
											sprintf(buf, "                    Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
											add_buf(buffer, buf);
											break;
										}

										case EXITMODE_WEIGHTED_SOURCE:
										{
											ITERATOR wit;
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->to, 1);
											sprintf(buf, "          %4d  {C   SOURCE   {x\n\r", gexitno++);
											add_buf(buffer, buf);

											int fromexitno = 1;
											add_buf(buffer, "                    Source:\n\r");
											add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
											add_buf(buffer, "                    ===================================={x\n\r");
											iterator_start(&wit, gex->from);
											while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
											{
												sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
												add_buf(buffer, buf);
											}
											iterator_stop(&wit);
											add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

											sprintf(buf,    "                    Destination:   {%c%4d{x (%d)\n\r", (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
											add_buf(buffer, buf);
											break;
										}

										case EXITMODE_WEIGHTED_DEST:
										{
											ITERATOR wit;
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(gex->from, 1);
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
											sprintf(buf, "          %4d  {C DESTINATION{x\n\r", gexitno++);
											add_buf(buffer, buf);

											sprintf(buf,    "                    Source:        {%c%4d{x (%d)\n\r", (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
											add_buf(buffer, buf);

											int toexitno = 1;
											add_buf(buffer, "                    Destination:\n\r");
											add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
											add_buf(buffer, "                    ===================================={x\n\r");
											iterator_start(&wit, gex->to);
											while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
											{
												sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
												add_buf(buffer, buf);
											}
											iterator_stop(&wit);
											add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
											break;
										}

										case EXITMODE_WEIGHTED:
										{
											ITERATOR wit;
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
											DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
											sprintf(buf, "          %4d  {C  WEIGHTED  {x\n\r", gexitno++);
											add_buf(buffer, buf);

											int fromexitno = 1;
											add_buf(buffer, "                    Source:\n\r");
											add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
											add_buf(buffer, "                    ===================================={x\n\r");
											iterator_start(&wit, gex->from);
											while ( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
											{
												sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", fromexitno++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
												add_buf(buffer, buf);
											}
											iterator_stop(&wit);
											add_buf(buffer, "                    ----------------------------------------------------{x\n\r");

											int toexitno = 1;
											add_buf(buffer, "                    Destination:\n\r");
											add_buf(buffer, "                         [ Weight ] [ Level ] [ Exit# ]{x\n\r");
											add_buf(buffer, "                    ===================================={x\n\r");
											iterator_start(&wit, gex->to);
											while ( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&wit)) )
											{
												sprintf(buf, "                    %4d   %6d    {%c%5d{x     %5d\n\r", toexitno++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
												add_buf(buffer, buf);
											}
											iterator_stop(&wit);
											add_buf(buffer, "                    ----------------------------------------------------{x\n\r");
											break;
										}
									}

								}
								iterator_stop(&git);
								add_buf(buffer, "{x          --------------------------------------------------------{x\n\r");
							}
							else
							{
								add_buf(buffer, "          None\n\r");
							}
							break;
						}
					}

				}

				iterator_stop(&it);
				add_buf(buffer, "{x----------------------------------------------------{x\n\r");
				add_buf(buffer, "{YYELLOW{x - Generated level position index\n\r");
				add_buf(buffer, "{GGREEN{x  - Ordinal level position index\n\r");

				if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
				send_to_char("Dungeon has no special exits defined.\n\r", ch);
			return FALSE;
		}

		if (!str_prefix(arg2, "add"))
		{
			int levels = dungeon_index_generation_count(dng);
			char buf[MSL];

			if (list_size(dng->levels) < 1)
			{
				send_to_char("Please add level definitions before adding special exits.\n\r", ch);
				return FALSE;
			}

			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  special exit {Wadd{x static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit {Wadd{x source generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit {Wadd{x destination generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit {Wadd{x weighted\n\r", ch);
				send_to_char("         special exit {Wadd{x group\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg3);

			// Add Static Special Exit
			// special exit add static <from-level> <from-exit> <to-level> <to-entrance>
			if (!str_prefix(arg3, "static"))
			{
				bool from_mode = TRISTATE;
				bool to_mode = TRISTATE;
				char argmf[MIL];
				char argmt[MIL];

				argument = one_argument(argument, argmf);
				if (!str_prefix(argmf, "generated"))
					from_mode = FALSE;
				else if(!str_prefix(argmf, "ordinal"))
					from_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit add static {Rgenerated|ordinal{x <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				if (IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal {R<from-level>{x <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, arg4);

				if (!is_number(arg4))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal {R<from-level>{x <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				int from_level = atoi(arg4);
				if (from_level < 1 || from_level > levels)
				{
					send_to_char("Syntax:  special exit add static generated|ordinal {R<from-level>{x <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *group;
				DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);

				int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

				if (IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> {R<from-exit>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, arg5);

				if (!is_number(arg5))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> {R<from-exit>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
					send_to_char(buf, ch);
					return FALSE;
				}

				int from_exit = atoi(arg5);
				if (from_exit < 1 || from_exit > from_exits)
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> {R<from-exit>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, argmt);
				if (!str_prefix(argmt, "generated"))
					to_mode = FALSE;
				else if (!str_prefix(argmt, "ordinal"))
					to_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				if (IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, arg6);

				if (!is_number(arg6))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				int to_level = atoi(arg6);
				if (to_level < 1 || to_level > levels)
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level), &group);

				int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

				if (IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
					send_to_char(buf, ch);
					return FALSE;
				}

				int to_entry = atoi(argument);
				if (to_entry < 1 || to_entry > to_entries)
				{
					send_to_char("Syntax:  special exit add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (from_mode == to_mode && from_level == to_level && from_exit == to_entry)
				{
					send_to_char("Both exits are the same.  Unable to connect them.\n\r", ch);
					return FALSE;
				}

				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_STATIC;
				
				add_dungeon_index_weighted_exit_data(ex->from, 1, from_mode?-from_level:from_level, from_exit);
				add_dungeon_index_weighted_exit_data(ex->to, 1, to_mode?-to_level:to_level, to_entry);

				list_appendlink(dng->special_exits, ex);

				send_to_char("Static special exit added.\n\r", ch);
				return TRUE;
			}

			// Add Source Special Exit
			// special exit add source <to-level> <to-entrance>
			if (!str_prefix(arg3, "source"))
			{
				bool to_mode = TRISTATE;
				char argmt[MIL];

				argument = one_argument(argument, argmt);
				if (!str_prefix(argmt, "generated"))
					to_mode = FALSE;
				else if (!str_prefix(argmt, "ordinal"))
					to_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit add source {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;

				}

				argument = one_argument(argument, arg6);

				if (!is_number(arg6))
				{
					send_to_char("Syntax:  special exit add source generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				int to_level = atoi(arg6);
				if (to_level < 1 || to_level > levels)
				{
					send_to_char("Syntax:  special exit add source generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *group;
				DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level), &group);

				int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit add source generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
					send_to_char(buf, ch);
					return FALSE;
				}

				int to_entry = atoi(argument);
				if (to_entry < 1 || to_entry > to_entries)
				{
					send_to_char("Syntax:  special exit add source generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED_SOURCE;
				
				add_dungeon_index_weighted_exit_data(ex->to, 1, (to_mode?-to_level:to_level), to_entry);

				list_appendlink(dng->special_exits, ex);

				int index = list_size(dng->special_exits);

				send_to_char("Source special exit added.\n\r", ch);
				sprintf(buf, "Please add source exits using {Wspecial exit from {Y%d{W add ...{x\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

			if (!str_prefix(arg3, "destination"))
			{
				bool from_mode = TRISTATE;
				char argmf[MIL];

				argument = one_argument(argument, argmf);
				if (!str_prefix(argmf, "generated"))
					from_mode = FALSE;
				else if (!str_prefix(argmf, "ordinal"))
					from_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit add destination {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg4);

				if (!is_number(arg4))
				{
					send_to_char("Syntax:  special exit add destination generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
					sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
					send_to_char(buf, ch);
					return FALSE;
				}

				int from_level = atoi(arg4);
				if (from_level < 1 || from_level > list_size(dng->levels))
				{
					send_to_char("Syntax:  special exit add destination generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
					sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", list_size(dng->levels));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *group;
				DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level), &group);

				int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit add destination generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
					sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
					send_to_char(buf, ch);
					return FALSE;
				}

				int from_exit = atoi(argument);
				if (from_exit < 1 || from_exit > from_exits)
				{
					send_to_char("Syntax:  special exit add destination generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
					sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED_DEST;
				
				add_dungeon_index_weighted_exit_data(ex->from, 1, (from_mode?-from_level:from_level), from_exit);

				list_appendlink(dng->special_exits, ex);

				int index = list_size(dng->special_exits);

				send_to_char("Destination special exit added.\n\r", ch);
				sprintf(buf, "Please add target entrances using {Wspecial exit to {Y%d{W add ...{x\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

			if (!str_prefix(arg3, "weighted"))
			{
				if (!IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add weighted\n\r", ch);
					return FALSE;
				}
				
				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED;

				list_appendlink(dng->special_exits, ex);

				int index = list_size(dng->special_exits);

				send_to_char("Weighted special exit added.\n\r", ch);
				sprintf(buf, "Please add source exits using {Wspecial exit from {Y%d{W add ...{x\n\r", index);
				send_to_char(buf, ch);
				sprintf(buf, "Please add target entrances using {Wspecial exit to {Y%d{W add ...{x\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

			if (!str_prefix(arg3, "group"))
			{
				if (!IS_NULLSTR(argument))
				{
					send_to_char("Syntax:  special exit add group\n\r", ch);
					return FALSE;
				}
				
				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_GROUP;
				
				list_appendlink(dng->special_exits, ex);

				int index = list_size(dng->special_exits);

				send_to_char("Group special exit added.\n\r", ch);
				sprintf(buf, "Please add exit definitions using {Wspecial exit group {Y%d{W add ...{x\n\r", index);
				send_to_char(buf, ch);
				return TRUE;
			}

			send_to_char("Syntax:  special exit add {Wstatic{x generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit add {Wsource{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit add {Wdestination{x generated|ordinal <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit add {Wweighted{x\n\r", ch);
			send_to_char("         special exit add {Wgroup{x\n\r", ch);
			return FALSE;
		}

		if (!str_prefix(arg2, "group"))
		{
			int levels = dungeon_index_generation_count(dng);
			char buf[MSL];
			char argg[MIL];			// used for group #
			char argg2[MIL];		// used for subcommand

			argument = one_argument(argument, argg);
			if (!is_number(argg))
			{
				send_to_char("Syntax:  special exit group {R#{x add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x add source generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x add destinationgenerated|ordinal  <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x add weighted\n\r", ch);
				send_to_char("         special exit group {R#{x from # list\n\r", ch);
				send_to_char("         special exit group {R#{x from # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x from # set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x from # remove #\n\r", ch);
				send_to_char("         special exit group {R#{x to # list\n\r", ch);
				send_to_char("         special exit group {R#{x to # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x to # set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x to # remove #\n\r", ch);
				send_to_char("         special exit group {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			int gindex = atoi(argg);
			if (gindex < 1 || gindex > list_size(dng->special_exits))
			{
				send_to_char("Syntax:  special exit group {R#{x add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x add source generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x add destinationgenerated|ordinal  <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x add weighted\n\r", ch);
				send_to_char("         special exit group {R#{x from # list\n\r", ch);
				send_to_char("         special exit group {R#{x from # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x from # set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group {R#{x from # remove #\n\r", ch);
				send_to_char("         special exit group {R#{x to # list\n\r", ch);
				send_to_char("         special exit group {R#{x to # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x to # set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group {R#{x to # remove #\n\r", ch);
				send_to_char("         special exit group {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_SPECIAL_EXIT *gex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, gindex);
			if (!IS_VALID(gex))
			{
				sprintf(buf, "Not such special exit %d found.\n\r", gindex);
				send_to_char(buf, ch);
				return FALSE;
			}

			if (gex->mode != EXITMODE_GROUP)
			{
				sprintf(buf, "Special exit %d is not a GROUP exit.\n\r", gindex);
				send_to_char(buf, ch);
				return FALSE;
			}

			argument = one_argument(argument, argg2);			

			if (!str_prefix(argg2, "add"))
			{
				if (list_size(dng->levels) < 1)
				{
					send_to_char("Please add level definitions before adding special exits.\n\r", ch);
					return FALSE;
				}

				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  special exit group # {Radd{x static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # {Radd{x source generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # {Radd{x destination generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         special exit group # {Radd{x weighted\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg3);

				// Add Static Special Exit
				// special exit group # add static <from-level> <from-exit> <to-level> <to-entrance>
				if (!str_prefix(arg3, "static"))
				{
					bool from_mode = TRISTATE;
					bool to_mode = TRISTATE;
					char argmf[MIL];
					char argmt[MIL];

					argument = one_argument(argument, argmf);
					if (!str_prefix(argmf, "generated"))
						from_mode = FALSE;
					else if (!str_prefix(argmf, "ordinal"))
						from_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # add static {Rgenerated|ordinal{x <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg4);
					if (!is_number(arg4))
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal {R<from-level>{x <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int from_level = atoi(arg4);
					if (from_level < 1 || from_level > levels)
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal {R<from-level>{x <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level), &group);

					int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

					argument = one_argument(argument, arg5);

					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
						send_to_char(buf, ch);
						return FALSE;
					}

					int from_exit = atoi(arg5);
					if (from_exit < 1 || from_exit > from_exits)
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> {R<from-exit>{x <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
						send_to_char(buf, ch);
						return FALSE;
					}

					argument = one_argument(argument, argmt);
					if (!str_prefix(argmt, "generated"))
						to_mode = FALSE;
					else if (!str_prefix(argmt, "ordinal"))
						to_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> <from-exit> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);

					if (!is_number(arg6))
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> <from-exit> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int to_level = atoi(arg6);
					if (to_level < 1 || to_level > levels)
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> <from-exit> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level), &group);

					int to_entries = get_dungeon_index_level_special_entrances(dng, to_level_data);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
						send_to_char(buf, ch);
						return FALSE;
					}

					int to_entry = atoi(argument);
					if (to_entry < 1 || to_entry > to_entries)
					{
						send_to_char("Syntax:  special exit group # add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (from_mode == to_mode && from_level == to_level && from_exit == to_entry)
					{
						send_to_char("Both exits are the same.  Unable to connect them.\n\r", ch);
						return FALSE;
					}

					DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
					ex->mode = EXITMODE_STATIC;
					
					add_dungeon_index_weighted_exit_data(ex->from, 1, (from_mode?-from_level:from_level), from_exit);
					add_dungeon_index_weighted_exit_data(ex->to, 1, (to_mode?-to_level:to_level), to_entry);

					list_appendlink(gex->group, ex);

					send_to_char("Static special exit added to group.\n\r", ch);
					return TRUE;
				}

				// Add Source Special Exit
				// special exit group # add source <to-level> <to-entrance>
				if (!str_prefix(arg3, "source"))
				{
					bool to_mode = TRISTATE;
					char argmt[MIL];

					argument = one_argument(argument, argmt);
					if (!str_prefix(argmt, "generated"))
						to_mode = FALSE;
					else if (!str_prefix(argmt, "ordinal"))
						to_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # add source {Rgenerate|ordinal{x <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);

					if (!is_number(arg6))
					{
						send_to_char("Syntax:  special exit group # add source generate|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int to_level = atoi(arg6);
					if (to_level < 1 || to_level > levels)
					{
						send_to_char("Syntax:  special exit group # add source generate|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a target level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level), &group);

					int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

					if (IS_NULLSTR(argument))
					{
						send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
						send_to_char(buf, ch);
						return FALSE;
					}

					int to_entry = atoi(argument);
					if (to_entry < 1 || to_entry > to_entries)
					{
						send_to_char("Syntax:  special exit group # add source <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a target entrance number from 1 to %d.\n\r", to_entries);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
					ex->mode = EXITMODE_WEIGHTED_SOURCE;
					
					add_dungeon_index_weighted_exit_data(ex->to, 1, (to_mode?-to_level:to_level), to_entry);

					list_appendlink(gex->group, ex);

					int index = list_size(gex->group);

					send_to_char("Source special exit group # added.\n\r", ch);
					sprintf(buf, "Please add source exits using {Wspecial exit group {Y%d{W from {Y%d{W add ...{x\n\r", gindex, index);
					send_to_char(buf, ch);
					return TRUE;
				}

				if (!str_prefix(arg3, "destination"))
				{
					bool from_mode = TRISTATE;
					char argmf[MIL];

					argument = one_argument(argument, argmf);
					if (!str_prefix(argmf, "generated"))
						from_mode = FALSE;
					else if (!str_prefix(argmf, "ordinal"))
						from_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # add destination {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg4);

					if (!is_number(arg4))
					{
						send_to_char("Syntax:  special exit group # add destination generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int from_level = atoi(arg4);
					if (from_level < 1 || from_level > levels)
					{
						send_to_char("Syntax:  special exit group # add destination generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a source level number from 1 to %d.\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level), &group);

					int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # add destination generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
						send_to_char(buf, ch);
						return FALSE;
					}

					int from_exit = atoi(argument);
					if (from_exit < 1 || from_exit > from_exits)
					{
						send_to_char("Syntax:  special exit group # add destination generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a source exit number from 1 to %d.\n\r", from_exits);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
					ex->mode = EXITMODE_WEIGHTED_DEST;
					
					add_dungeon_index_weighted_exit_data(ex->from, 1, (from_mode?-from_level:from_level), from_exit);

					list_appendlink(gex->group, ex);

					int index = list_size(gex->group);

					send_to_char("Destination special exit added to group.\n\r", ch);
					sprintf(buf, "Please add target entrances using {Wspecial exit group {Y%d{W to {Y%d{W add ...{x\n\r", gindex, index);
					send_to_char(buf, ch);
					return TRUE;
				}

				if (!str_prefix(arg3, "weighted"))
				{
					if (!IS_NULLSTR(argument))
					{
						send_to_char("Syntax:  special exit group # add weighted\n\r", ch);
						return FALSE;
					}
					
					DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
					ex->mode = EXITMODE_WEIGHTED;

					list_appendlink(gex->group, ex);

					int index = list_size(gex->group);

					send_to_char("Weighted special exit group # added.\n\r", ch);
					sprintf(buf, "Please add source exits using {Wspecial exit group {Y%d{W from {Y%d{W add ...{x\n\r", gindex, index);
					send_to_char(buf, ch);
					sprintf(buf, "Please add target entrances using {Wspecial exit group {Y%d{W to {Y%d{W add ...{x\n\r", gindex, index);
					send_to_char(buf, ch);
					return TRUE;
				}

				send_to_char("Syntax:  special exit group # add {Wstatic{x <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group # add {Wsource{x <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group # add {Wdestination{x <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group # add {Wweighted{x\n\r", ch);
				return FALSE;
			}

			if (!str_prefix(arg2, "from"))
			{
				if (list_size(gex->group) < 1)
				{
					send_to_char("Please add a special exit definition first.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg3);
				if (!is_number(arg3))
				{
					send_to_char("Syntax:  special exit group # from {R#{x list\n\r", ch);
					send_to_char("         special exit group # from {R#{x add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         special exit group # from {R#{x set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         special exit group # from {R#{x remove #\n\r", ch);
					sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				int index = atoi(arg3);
				if (index < 1 || index > list_size(gex->group))
				{
					send_to_char("Syntax:  special exit group # from {R#{x list\n\r", ch);
					send_to_char("         special exit group # from {R#{x add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         special exit group # from {R#{x set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         special exit group # from {R#{x remove #\n\r", ch);
					sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, index);

				if (ex->mode == EXITMODE_GROUP)
				{
					send_to_char("Cannot alter the From definitions on a GROUP exit.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg4);

				if (!str_prefix(arg4, "list"))
				{
					if (list_size(ex->from) > 0)
					{
						BUFFER *buffer = new_buf();
						ITERATOR fit;

						add_buf(buffer, "     [ Weight ] [ Level ] [ Exit ]\n\r");
						add_buf(buffer, "===================================\n\r");
						
						int findex = 1;
						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
						iterator_start(&fit, ex->from);
						while( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&fit)) )
						{
							sprintf(buf, "%4d   %6d     {%c%5d{x     %4d\n\r", findex++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
							add_buf(buffer, buf);
						}
						iterator_stop(&fit);

						add_buf(buffer, "-----------------------------------\n\r");

						if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
					{
						send_to_char("There are no From definitions on this special exit.\n\r", ch);
					}
					return FALSE;
				}

				if (!str_prefix(arg4, "add"))
				{
					if (ex->mode == EXITMODE_STATIC)
					{
						sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new From definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (ex->mode == EXITMODE_WEIGHTED_DEST)
					{
						sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot add any new From definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit group # from # add {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit group # from # add {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool from_mode = TRISTATE;
					char argmf[MIL];

					argument = one_argument(argument, argmf);
					if (!str_prefix(argmf, "generated"))
						from_mode = FALSE;
					else if (!str_prefix(argmf, "ordinal"))
						from_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # from # add <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit group # from # add <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int flindex = atoi(arg7);
					if (flindex < 1 || flindex > levels)
					{
						send_to_char("Syntax:  special exit group # from # add <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
					int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # from # add <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					int fexit = atoi(argument);
					if (fexit < 1 || fexit > fexits)
					{
						send_to_char("Syntax:  special exit group # from # add <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					add_dungeon_index_weighted_exit_data(ex->from, weight, (from_mode?-flindex:flindex), fexit);
					ex->total_from += weight;

					sprintf(buf, "From definition added to special exit %d.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!str_prefix(arg4, "set"))
				{
					if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_DEST)
					{
						if (list_size(ex->from) < 1)
						{
							send_to_char("Special exit appears to be missing necessary From definition.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg6);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # from # set {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						int weight = atoi(arg6);
						if (weight < 1)
						{
							send_to_char("Syntax:  special exit group # from # set {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						bool from_mode = TRISTATE;
						char argmf[MIL];

						if (!str_prefix(argmf, "generated"))
							from_mode = FALSE;
						else if (!str_prefix(argmf, "ordinal"))
							from_mode = TRUE;
						else
						{
							send_to_char("Syntax:  special exit group # from # set <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
							send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg7);
						if (!is_number(arg7))
						{
							send_to_char("Syntax:  special exit group # from # set <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						int flindex = atoi(arg7);
						if (flindex < 1 || flindex > levels)
						{
							send_to_char("Syntax:  special exit group # from # set <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_LEVEL_DATA *group;
						DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
						int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

						if (!is_number(argument))
						{
							send_to_char("Syntax:  special exit group # from # set <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
							send_to_char(buf, ch);
							return FALSE;
						}

						int fexit = atoi(argument);
						if (fexit < 1 || fexit > fexits)
						{
							send_to_char("Syntax:  special exit group # from # set <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, 1);
						ex->total_from -= fex->weight;
						ex->total_from += weight;

						fex->weight = weight;
						fex->level = from_mode?-flindex:flindex;
						fex->door = fexit;

						sprintf(buf, "From definition set on special exit %d.\n\r", index);
						send_to_char(buf, ch);
						return TRUE;
					}
					else
					{
						if (list_size(ex->from) < 1)
						{
							send_to_char("Special exit has no From definition.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg5);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # from # set {R#{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
							send_to_char(buf, ch);
							return FALSE;
						}

						int findex = atoi(arg5);
						if (findex < 1 || findex > list_size(ex->from))
						{
							send_to_char("Syntax:  special exit group # from # set {R#{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
							send_to_char(buf, ch);
							return FALSE;
						}

						argument = one_argument(argument, arg6);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # from # set # {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						int weight = atoi(arg6);
						if (weight < 1)
						{
							send_to_char("Syntax:  special exit group # from # set # {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						bool from_mode = TRISTATE;
						char argmf[MIL];

						if (!str_prefix(argmf, "generated"))
							from_mode = FALSE;
						else if (!str_prefix(argmf, "ordinal"))
							from_mode = TRUE;
						else
						{
							send_to_char("Syntax:  special exit group # from # set # <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
							send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg7);
						if (!is_number(arg7))
						{
							send_to_char("Syntax:  special exit group # from # set # <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						int flindex = atoi(arg7);
						if (flindex < 1 || flindex > levels)
						{
							send_to_char("Syntax:  special exit group # from # set # <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_LEVEL_DATA *group;
						DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
						int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

						if (!is_number(argument))
						{
							send_to_char("Syntax:  special exit group # from # set # <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
							send_to_char(buf, ch);
							return FALSE;
						}

						int fexit = atoi(argument);
						if (fexit < 1 || fexit > fexits)
						{
							send_to_char("Syntax:  special exit group # from # set # <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
							sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, findex);
						ex->total_from -= fex->weight;
						ex->total_from += weight;

						fex->weight = weight;
						fex->level = from_mode?-flindex:flindex;
						fex->door = fexit;

						sprintf(buf, "From definition %d set on special exit %d.\n\r", findex, index);
						send_to_char(buf, ch);
						return TRUE;
					}
				}

				if (!str_prefix(arg4, "remove"))
				{
					if (ex->mode == EXITMODE_STATIC)
					{
						sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the From definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (ex->mode == EXITMODE_WEIGHTED_DEST)
					{
						sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot remove the From definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # from # remove {R#{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
						send_to_char(buf, ch);
						return FALSE;
					}

					int findex = atoi(argument);
					if (findex < 1 || findex > list_size(ex->from))
					{
						send_to_char("Syntax:  special exit group # from # remove {R#{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
						send_to_char(buf, ch);
						return FALSE;
					}

					list_remnthlink(ex->from, findex);
					send_to_char("From definition removed from special exit.\n\r", ch);
					if (list_size(ex->from) < 1)
						send_to_char("{RWarning:{x Please add a from definition for this exit to work.\n\r", ch);
					return TRUE;
				}

				send_to_char("Syntax:  special exit group # from # {Rlist{x\n\r", ch);
				send_to_char("         special exit group # from # {Radd{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group # from # {Rset{x[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit group # from # {Rremove{x #\n\r", ch);
				return FALSE;
			}

			if (!str_prefix(arg2, "to"))
			{
				if (list_size(gex->group) < 1)
				{
					send_to_char("Please add a special exit definition first.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg3);
				if (!is_number(arg3))
				{
					send_to_char("Syntax:  special exit group # to {R#{x list\n\r", ch);
					send_to_char("         special exit group # to {R#{x add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # to {R#{x set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # to {R#{x remove #\n\r", ch);
					sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				int index = atoi(arg3);
				if (index < 1 || index > list_size(gex->group))
				{
					send_to_char("Syntax:  special exit group # to {R#{x list\n\r", ch);
					send_to_char("         special exit group # to {R#{x add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # to {R#{x set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         special exit group # to {R#{x remove #\n\r", ch);
					sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, index);

				if (ex->mode == EXITMODE_GROUP)
				{
					send_to_char("Cannot alter the From definitions on a GROUP exit.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg4);

				if (!str_prefix(arg4, "list"))
				{
					if (list_size(ex->to) > 0)
					{
						BUFFER *buffer = new_buf();
						ITERATOR fit;

						add_buf(buffer, "     [ Weight ] [ Level ] [ Exit ]\n\r");
						add_buf(buffer, "===================================\n\r");
						
						int tindex = 1;
						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
						iterator_start(&fit, ex->to);
						while( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&fit)) )
						{
							sprintf(buf, "%4d   %6d     {%c%5d{x     %4d\n\r", tindex++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
							add_buf(buffer, buf);
						}
						iterator_stop(&fit);

						add_buf(buffer, "-----------------------------------\n\r");

						if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
					{
						send_to_char("There are no To definitions on this special exit.\n\r", ch);
					}
					return FALSE;
				}

				if (!str_prefix(arg4, "add"))
				{
					if (ex->mode == EXITMODE_STATIC)
					{
						sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new To definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
					{
						sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot add any new To definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit group # to # add {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit group # to # add {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool to_mode = TRISTATE;
					char argmt[MIL];

					argument = one_argument(argument, argmt);
					if (!str_prefix(argmt, "generated"))
						to_mode = FALSE;
					else if (!str_prefix(argmt, "ordinal"))
						to_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit group # to # add <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit group # to # add <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int tlindex = atoi(arg7);
					if (tlindex < 1 || tlindex > levels)
					{
						send_to_char("Syntax:  special exit group # to # add <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
					int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # to # add <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					int tentry = atoi(argument);
					if (tentry < 1 || tentry > tentries)
					{
						send_to_char("Syntax:  special exit group # to # add <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					add_dungeon_index_weighted_exit_data(ex->to, weight, (to_mode?-tlindex:tlindex), tentry);
					ex->total_to += weight;

					sprintf(buf, "To definition added to special exit %d.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!str_prefix(arg4, "set"))
				{
					if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_SOURCE)
					{
						if (list_size(ex->to) < 1)
						{
							send_to_char("Special exit appears to be missing necessary To definition.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg6);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # to # set {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						int weight = atoi(arg6);
						if (weight < 1)
						{
							send_to_char("Syntax:  special exit group # to # set {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						bool to_mode = TRISTATE;
						char argmf[MIL];

						if (!str_prefix(argmf, "generated"))
							to_mode = FALSE;
						else if (!str_prefix(argmf, "ordinal"))
							to_mode = TRUE;
						else
						{
							send_to_char("Syntax:  special exit group # to # set <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
							send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg7);
						if (!is_number(arg7))
						{
							send_to_char("Syntax:  special exit group # to # set <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						int tlindex = atoi(arg7);
						if (tlindex < 1 || tlindex > levels)
						{
							send_to_char("Syntax:  special exit group # to # set <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_LEVEL_DATA *group;
						DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
						int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

						if (!is_number(argument))
						{
							send_to_char("Syntax:  special exit group # to # set <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
							send_to_char(buf, ch);
							return FALSE;
						}

						int tentry = atoi(argument);
						if (tentry < 1 || tentry > tentries)
						{
							send_to_char("Syntax:  special exit group # to # set <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, 1);
						ex->total_to -= tex->weight;
						ex->total_to += weight;

						tex->weight = weight;
						tex->level = to_mode?-tlindex:tlindex;
						tex->door = tentry;

						sprintf(buf, "To definition set on special exit %d.\n\r", index);
						send_to_char(buf, ch);
						return TRUE;
					}
					else
					{
						if (list_size(ex->to) < 1)
						{
							send_to_char("Special exit has no From definition.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg5);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # to # set {R#{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
							send_to_char(buf, ch);
							return FALSE;
						}

						int tindex = atoi(arg5);
						if (tindex < 1 || tindex > list_size(ex->to))
						{
							send_to_char("Syntax:  special exit group # to # set {R#{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
							send_to_char(buf, ch);
							return FALSE;
						}

						argument = one_argument(argument, arg6);
						if (!is_number(arg5))
						{
							send_to_char("Syntax:  special exit group # to # set # {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						int weight = atoi(arg6);
						if (weight < 1)
						{
							send_to_char("Syntax:  special exit group # to # set # {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
							send_to_char("         Please specify a positive number.\n\r", ch);
							return FALSE;
						}

						bool to_mode = TRISTATE;
						char argmf[MIL];

						if (!str_prefix(argmf, "generated"))
							to_mode = FALSE;
						else if (!str_prefix(argmf, "ordinal"))
							to_mode = TRUE;
						else
						{
							send_to_char("Syntax:  special exit group # to # set # <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
							send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
							return FALSE;
						}

						argument = one_argument(argument, arg7);
						if (!is_number(arg7))
						{
							send_to_char("Syntax:  special exit group # to # set # <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						int tlindex = atoi(arg7);
						if (tlindex < 1 || tlindex > levels)
						{
							send_to_char("Syntax:  special exit group # to # set # <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_LEVEL_DATA *group;
						DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
						int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

						if (!is_number(argument))
						{
							send_to_char("Syntax:  special exit group # to # set # <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
							send_to_char(buf, ch);
							return FALSE;
						}

						int tentry = atoi(argument);
						if (tentry < 1 || tentry > tentries)
						{
							send_to_char("Syntax:  special exit group # to # set # <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
							sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
							send_to_char(buf, ch);
							return FALSE;
						}

						DUNGEON_INDEX_WEIGHTED_EXIT_DATA *tex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, tindex);
						ex->total_to -= tex->weight;
						ex->total_to += weight;

						tex->weight = weight;
						tex->level = to_mode?-tlindex:tlindex;
						tex->door = tentry;

						sprintf(buf, "To definition %d set on special exit %d.\n\r", tindex, index);
						send_to_char(buf, ch);
						return TRUE;
					}
				}

				if (!str_prefix(arg4, "remove"))
				{
					if (ex->mode == EXITMODE_STATIC)
					{
						sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the To definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
					{
						sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot remove the To definition.\n\r", index);
						send_to_char(buf, ch);
						return FALSE;
					}

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit group # to # remove {R#{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
						send_to_char(buf, ch);
						return FALSE;
					}

					int tindex = atoi(argument);
					if (tindex < 1 || tindex > list_size(ex->to))
					{
						send_to_char("Syntax:  special exit group # to # remove {R#{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
						send_to_char(buf, ch);
						return FALSE;
					}

					list_remnthlink(ex->to, tindex);
					send_to_char("To definition removed to special exit.\n\r", ch);
					if (list_size(ex->to) < 1)
						send_to_char("{RWarning:{x Please add a To definition for this exit to work.\n\r", ch);
					return TRUE;
				}

				send_to_char("Syntax:  special exit group # to # {Rlist{x\n\r", ch);
				send_to_char("         special exit group # to # {Radd{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group # to # {Rset{x[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit group # to # {Rremove{x #\n\r", ch);
				return FALSE;
			}

			if (!str_prefix(arg2, "remove"))
			{
				if (argument[0] == '\0')
				{
					send_to_char("Syntax:  special exit group # remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				if (list_size(dng->special_exits) < 1)
				{
					send_to_char("There are no special exits.\n\r", ch);
					return FALSE;
				}

				int index = atoi(argument);
				if (index < 1 || index > list_size(gex->group))
				{
					send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(gex->group));
					send_to_char(buf, ch);
					return FALSE;
				}

				list_remnthlink(gex->group, index);
				sprintf(buf, "Special exit %d removed from group %d.\n\r", index, gindex);
				send_to_char(buf, ch);
				return TRUE;
			}


			send_to_char("Syntax:  special exit group # {Radd{x static <from-level> <from-exit> <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit group # {Radd{x source <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit group # {Radd{x destination <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit group # {Radd{x weighted\n\r", ch);
			send_to_char("         special exit group # {Rfrom{x # list\n\r", ch);
			send_to_char("         special exit group # {Rfrom{x # add <weight> <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit group # {Rfrom{x # set[ #] <weight> <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit group # {Rfrom{x # remove #\n\r", ch);
			send_to_char("         special exit group # {Rto{x # list\n\r", ch);
			send_to_char("         special exit group # {Rto{x # add <weight> <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit group # {Rto{x # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit group # {Rto{x # remove #\n\r", ch);
			send_to_char("         special exit group # {Rremove{x #\n\r", ch);
			return FALSE;
		}

		if (!str_prefix(arg2, "from"))
		{
			int levels = dungeon_index_generation_count(dng);
			char buf[MSL];
			if (list_size(dng->special_exits) < 1)
			{
				send_to_char("Please add a special exit definition first.\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg3);
			if (!is_number(arg3))
			{
				send_to_char("Syntax:  special exit from {R#{x list\n\r", ch);
				send_to_char("         special exit from {R#{x add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from {R#{x set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			int index = atoi(arg3);
			if (index < 1 || index > list_size(dng->special_exits))
			{
				send_to_char("Syntax:  special exit from {R#{x list\n\r", ch);
				send_to_char("         special exit from {R#{x add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from {R#{x set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, index);

			if (ex->mode == EXITMODE_GROUP)
			{
				send_to_char("Cannot alter the From definitions on a GROUP exit.\n\r", ch);
				return FALSE;
			}

			if (IS_NULLSTR(argument))
			{
				send_to_char("Syntax:  special exit from # {Rlist{x\n\r", ch);
				send_to_char("         special exit from # {Radd{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from # {Rset{x[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
				send_to_char("         special exit from # {Rremove{x #\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg4);

			if (!str_prefix(arg4, "list"))
			{
				if (list_size(ex->from) > 0)
				{
					BUFFER *buffer = new_buf();
					ITERATOR fit;

					add_buf(buffer, "     [ Weight ] [ Level ] [ Exit ]\n\r");
					add_buf(buffer, "===================================\n\r");
					
					int findex = 1;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *from;
					iterator_start(&fit, ex->from);
					while( (from = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&fit)) )
					{
						sprintf(buf, "%4d   %6d     {%c%5d{x     %4d\n\r", findex++, from->weight, (from->level<0?'G':(from->level>0?'Y':'W')), abs(from->level), from->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&fit);

					add_buf(buffer, "-----------------------------------\n\r");

					if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
				{
					send_to_char("There are no From definitions on this special exit.\n\r", ch);
				}
				return FALSE;
			}

			if (!str_prefix(arg4, "add"))
			{
				if (ex->mode == EXITMODE_STATIC)
				{
					sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new From definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (ex->mode == EXITMODE_WEIGHTED_DEST)
				{
					sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot add any new From definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, arg6);
				if (!is_number(arg5))
				{
					send_to_char("Syntax:  special exit from # add {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         Please specify a positive number.\n\r", ch);
					return FALSE;
				}

				int weight = atoi(arg6);
				if (weight < 1)
				{
					send_to_char("Syntax:  special exit from # add {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
					send_to_char("         Please specify a positive number.\n\r", ch);
					return FALSE;
				}

				bool from_mode = TRISTATE;
				char argm[MIL];
				
				argument = one_argument(argument, argm);
				if(!str_prefix(argm, "generated"))
					from_mode = FALSE;
				else if(!str_prefix(argm, "ordinal"))
					from_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit from # add <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg7);
				if (!is_number(arg7))
				{
					send_to_char("Syntax:  special exit from # add <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				int flindex = atoi(arg7);
				if (flindex < 1 || flindex > levels)
				{
					send_to_char("Syntax:  special exit from # add <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *group;
				DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
				int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit from # add <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
					send_to_char(buf, ch);
					return FALSE;
				}

				int fexit = atoi(argument);
				if (fexit < 1 || fexit > fexits)
				{
					send_to_char("Syntax:  special exit from # add <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
					send_to_char(buf, ch);
					return FALSE;
				}

				add_dungeon_index_weighted_exit_data(ex->from, weight, flindex, fexit);
				ex->total_from += weight;

				sprintf(buf, "From definition added to special exit %d.\n\r", index);
				send_to_char(buf, ch);
				return FALSE;
			}

			if (!str_prefix(arg4, "set"))
			{
				if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_DEST)
				{
					if (list_size(ex->from) < 1)
					{
						send_to_char("Special exit appears to be missing necessary From definition.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit from # set {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit from # set {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool from_mode = TRISTATE;
					char argm[MIL];

					argument = one_argument(argument, argm);
					if (!str_prefix(argm, "generated"))
						from_mode = FALSE;
					else if (!str_prefix(argm, "ordinal"))
						from_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit from # set <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit from # set <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int flindex = atoi(arg7);
					if (flindex < 1 || flindex > levels)
					{
						send_to_char("Syntax:  special exit from # set <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
					int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit from # set <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					int fexit = atoi(argument);
					if (fexit < 1 || fexit > fexits)
					{
						send_to_char("Syntax:  special exit from # set <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, 1);
					ex->total_from -= fex->weight;
					ex->total_from += weight;

					fex->weight = weight;
					fex->level = from_mode?-flindex:flindex;
					fex->door = fexit;

					sprintf(buf, "From definition set on special exit %d.\n\r", index);
					send_to_char(buf, ch);
					return TRUE;
				}
				else
				{
					if (list_size(ex->from) < 1)
					{
						send_to_char("Special exit has no From definition.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg5);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit from # set {R#{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
						send_to_char(buf, ch);
						return FALSE;
					}

					int findex = atoi(arg5);
					if (findex < 1 || findex > list_size(ex->from))
					{
						send_to_char("Syntax:  special exit from # set {R#{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
						send_to_char(buf, ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit from # set # {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit from # set # {R<weight>{x generated|ordinal <from-level> <from-exit>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool from_mode = TRISTATE;
					char argm[MIL];

					argument = one_argument(argument, argm);
					if (!str_prefix(argm, "generated"))
						from_mode = FALSE;
					else if (!str_prefix(argm, "ordinal"))
						from_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit from # set # <weight> {Rgenerated|ordinal{x <from-level> <from-exit>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit from # set # <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
						send_to_char(buf, ch);
						return FALSE;
					}

					int flindex = atoi(arg7);
					if (flindex < 1 || flindex > list_size(dng->levels))
					{
						send_to_char("Syntax:  special exit from # set # <weight> generated|ordinal {R<from-level>{x <from-exit>\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", list_size(dng->levels));
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (from_mode?-flindex:flindex), &group);
					int fexits = get_dungeon_index_level_special_exits(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit from # set # <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					int fexit = atoi(argument);
					if (fexit < 1 || fexit > fexits)
					{
						send_to_char("Syntax:  special exit from # set # <weight> generated|ordinal <from-level> {R<from-exit>{x\n\r", ch);
						sprintf(buf, "         Please specify a number from 1 to %d\n\r", fexits);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->from, findex);
					ex->total_from -= fex->weight;
					ex->total_from += weight;

					fex->weight = weight;
					fex->level = from_mode?-flindex:flindex;
					fex->door = fexit;

					sprintf(buf, "From definition %d set on special exit %d.\n\r", findex, index);
					send_to_char(buf, ch);
					return TRUE;

				}
			}

			if (!str_prefix(arg4, "remove"))
			{
				char buf[MSL];
				if (ex->mode == EXITMODE_STATIC)
				{
					sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the From definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (ex->mode == EXITMODE_WEIGHTED_DEST)
				{
					sprintf(buf, "Special exit %d is a DESTINATION exit.  Cannot remove the From definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit from # remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
					send_to_char(buf, ch);
					return FALSE;
				}

				int findex = atoi(argument);
				if (findex < 1 || findex > list_size(ex->from))
				{
					send_to_char("Syntax:  special exit from # remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(ex->from));
					send_to_char(buf, ch);
					return FALSE;
				}

				list_remnthlink(ex->from, findex);
				send_to_char("From definition removed from special exit.\n\r", ch);
				if (list_size(ex->from) < 1)
					send_to_char("{RWarning:{x Please add a from definition for this exit to work.\n\r", ch);
				return TRUE;
			}

			send_to_char("Syntax:  special exit from # {Rlist{x\n\r", ch);
			send_to_char("         special exit from # {Radd{x <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit from # {Rset{x[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
			send_to_char("         special exit from # {Rremove{x #\n\r", ch);
			return FALSE;
		}

		if (!str_prefix(arg2, "to"))
		{
			int levels = dungeon_index_generation_count(dng);
			char buf[MSL];
			if (list_size(dng->special_exits) < 1)
			{
				send_to_char("Please add a special exit definition first.\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg3);
			if (!is_number(arg3))
			{
				send_to_char("Syntax:  special exit to {R#{x list\n\r", ch);
				send_to_char("         special exit to {R#{x add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to {R#{x set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			int index = atoi(arg3);
			if (index < 1 || index > list_size(dng->special_exits))
			{
				send_to_char("Syntax:  special exit to {R#{x list\n\r", ch);
				send_to_char("         special exit to {R#{x add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to {R#{x set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to {R#{x remove #\n\r", ch);
				sprintf(buf, "         Please specify a number between 1 and {Y%d{x.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, index);

			if (ex->mode == EXITMODE_GROUP)
			{
				send_to_char("Cannot alter the To definitions on a GROUP exit.\n\r", ch);
				return FALSE;
			}

			if (IS_NULLSTR(argument))
			{
				send_to_char("Syntax:  special exit to # {Rlist{x\n\r", ch);
				send_to_char("         special exit to # {Radd{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to # {Rset{x[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
				send_to_char("         special exit to # {Rremove{x #\n\r", ch);
				return FALSE;
			}

			argument = one_argument(argument, arg4);

			if (!str_prefix(arg4, "list"))
			{
				if (list_size(ex->to) > 0)
				{
					BUFFER *buffer = new_buf();
					ITERATOR fit;

					add_buf(buffer, "     [ Weight ] [ Level ] [ Exit ]\n\r");
					add_buf(buffer, "===================================\n\r");
					
					int tindex = 1;
					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *to;
					iterator_start(&fit, ex->to);
					while( (to = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)iterator_nextdata(&fit)) )
					{
						sprintf(buf, "%4d   %6d     {%c%5d{x     %4d\n\r", tindex++, to->weight, (to->level<0?'G':(to->level>0?'Y':'W')), abs(to->level), to->door);
						add_buf(buffer, buf);
					}
					iterator_stop(&fit);

					add_buf(buffer, "-----------------------------------\n\r");

					if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
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
				{
					send_to_char("There are no To definitions on this special exit.\n\r", ch);
				}
				return FALSE;
			}

			if (!str_prefix(arg4, "add"))
			{
				if (ex->mode == EXITMODE_STATIC)
				{
					sprintf(buf, "Special exit %d is a STATIC exit.  Cannot add any new To definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
				{
					sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot add any new To definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				argument = one_argument(argument, arg6);
				if (!is_number(arg5))
				{
					send_to_char("Syntax:  special exit to # add {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         Please specify a positive number.\n\r", ch);
					return FALSE;
				}

				int weight = atoi(arg6);
				if (weight < 1)
				{
					send_to_char("Syntax:  special exit to # add {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
					send_to_char("         Please specify a positive number.\n\r", ch);
					return FALSE;
				}

				bool to_mode = TRISTATE;
				char argm[MIL];

				argument = one_argument(argument, argm);
				if (!str_prefix(argm, "generated"))
					to_mode = FALSE;
				else if (!str_prefix(argm, "ordinal"))
					to_mode = TRUE;
				else
				{
					send_to_char("Syntax:  special exit to # add <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
					send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
					return FALSE;
				}

				argument = one_argument(argument, arg7);
				if (!is_number(arg7))
				{
					send_to_char("Syntax:  special exit to # add <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				int tlindex = atoi(arg7);
				if (tlindex < 1 || tlindex > levels)
				{
					send_to_char("Syntax:  special exit to # add <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
					send_to_char(buf, ch);
					return FALSE;
				}

				DUNGEON_INDEX_LEVEL_DATA *group;
				DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
				int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit to # add <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
					send_to_char(buf, ch);
					return FALSE;
				}

				int tentry = atoi(argument);
				if (tentry < 1 || tentry > tentries)
				{
					send_to_char("Syntax:  special exit to # add <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
					send_to_char(buf, ch);
					return FALSE;
				}

				add_dungeon_index_weighted_exit_data(ex->to, weight, to_mode?-tlindex:tlindex, tentry);
				ex->total_to += weight;

				sprintf(buf, "To definition added to special exit %d.\n\r", index);
				send_to_char(buf, ch);
				return FALSE;
			}

			if (!str_prefix(arg4, "set"))
			{
				if (ex->mode == EXITMODE_STATIC || ex->mode == EXITMODE_WEIGHTED_SOURCE)
				{
					if (list_size(ex->to) < 1)
					{
						send_to_char("Special exit appears to be missing necessary To definition.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit to # set {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit to # set {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool to_mode = TRISTATE;
					char argm[MIL];

					argument = one_argument(argument, argm);
					if (!str_prefix(argm, "generated"))
						to_mode = FALSE;
					else if (!str_prefix(argm, "ordinal"))
						to_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit to # set <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit to # set <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					int tlindex = atoi(arg7);
					if (tlindex < 1 || tlindex > levels)
					{
						send_to_char("Syntax:  special exit to # set <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", levels);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
					int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit to # set <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					int tentry = atoi(argument);
					if (tentry < 1 || tentry > tentries)
					{
						send_to_char("Syntax:  special exit to # set <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, 1);
					ex->total_to -= fex->weight;
					ex->total_to += weight;

					fex->weight = weight;
					fex->level = to_mode?-tlindex:tlindex;
					fex->door = tentry;

					sprintf(buf, "To definition set on special exit %d.\n\r", index);
					send_to_char(buf, ch);
					return TRUE;
				}
				else
				{
					if (list_size(ex->to) < 1)
					{
						send_to_char("Special exit has no To definition.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg5);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit to # set {R#{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
						send_to_char(buf, ch);
						return FALSE;
					}

					int tindex = atoi(arg5);
					if (tindex < 1 || tindex > list_size(ex->to))
					{
						send_to_char("Syntax:  special exit to # set {R#{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
						send_to_char(buf, ch);
						return FALSE;
					}

					argument = one_argument(argument, arg6);
					if (!is_number(arg5))
					{
						send_to_char("Syntax:  special exit to # set # {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					int weight = atoi(arg6);
					if (weight < 1)
					{
						send_to_char("Syntax:  special exit to # set # {R<weight>{x generated|ordinal <to-level> <to-entrance>\n\r", ch);
						send_to_char("         Please specify a positive number.\n\r", ch);
						return FALSE;
					}

					bool to_mode = TRISTATE;
					char argm[MIL];

					argument = one_argument(argument, argm);
					if (!str_prefix(argm, "generated"))
						to_mode = FALSE;
					else if (!str_prefix(argm, "ordinal"))
						to_mode = TRUE;
					else
					{
						send_to_char("Syntax:  special exit to # set # <weight> {Rgenerated|ordinal{x <to-level> <to-entrance>\n\r", ch);
						send_to_char("Please specify either {Ygenerated{x or {Gordinal{x.\n\r", ch);
						return FALSE;
					}

					argument = one_argument(argument, arg7);
					if (!is_number(arg7))
					{
						send_to_char("Syntax:  special exit to # set # <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
						send_to_char(buf, ch);
						return FALSE;
					}

					int tlindex = atoi(arg7);
					if (tlindex < 1 || tlindex > list_size(dng->levels))
					{
						send_to_char("Syntax:  special exit to # set # <weight> generated|ordinal {R<to-level>{x <to-entrance>\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", list_size(dng->levels));
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_LEVEL_DATA *group;
					DUNGEON_INDEX_LEVEL_DATA *flevel = dungeon_index_get_nth_level(dng, (to_mode?-tlindex:tlindex), &group);
					int tentries = get_dungeon_index_level_special_entrances(dng, group?group:flevel);

					if (!is_number(argument))
					{
						send_to_char("Syntax:  special exit to # set # <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					int tentry = atoi(argument);
					if (tentry < 1 || tentry > tentries)
					{
						send_to_char("Syntax:  special exit to # set # <weight> generated|ordinal <to-level> {R<to-entrance>{x\n\r", ch);
						sprintf(buf, "         Please specify a number to 1 to %d\n\r", tentries);
						send_to_char(buf, ch);
						return FALSE;
					}

					DUNGEON_INDEX_WEIGHTED_EXIT_DATA *fex = (DUNGEON_INDEX_WEIGHTED_EXIT_DATA *)list_nthdata(ex->to, tindex);
					ex->total_to -= fex->weight;
					ex->total_to += weight;

					fex->weight = weight;
					fex->level = to_mode?-tlindex:tlindex;
					fex->door = tentry;

					sprintf(buf, "To definition %d set on special exit %d.\n\r", tindex, index);
					send_to_char(buf, ch);
					return TRUE;

				}
			}

			if (!str_prefix(arg4, "remove"))
			{
				char buf[MSL];
				if (ex->mode == EXITMODE_STATIC)
				{
					sprintf(buf, "Special exit %d is a STATIC exit.  Cannot remove the To definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (ex->mode == EXITMODE_WEIGHTED_SOURCE)
				{
					sprintf(buf, "Special exit %d is a SOURCE exit.  Cannot remove the To definition.\n\r", index);
					send_to_char(buf, ch);
					return FALSE;
				}

				if (!is_number(argument))
				{
					send_to_char("Syntax:  special exit to # remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
					send_to_char(buf, ch);
					return FALSE;
				}

				int tindex = atoi(argument);
				if (tindex < 1 || tindex > list_size(ex->to))
				{
					send_to_char("Syntax:  special exit to # remove {R#{x\n\r", ch);
					sprintf(buf, "         Please specify a number to 1 to %d.\n\r", list_size(ex->to));
					send_to_char(buf, ch);
					return FALSE;
				}

				list_remnthlink(ex->to, tindex);
				send_to_char("To definition removed to special exit.\n\r", ch);
				if (list_size(ex->to) < 1)
					send_to_char("{RWarning:{x Please add a to definition for this exit to work.\n\r", ch);
				return TRUE;
			}

			send_to_char("Syntax:  special exit to # {Rlist{x\n\r", ch);
			send_to_char("         special exit to # {Radd{x <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit to # {Rset{x[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
			send_to_char("         special exit to # {Rremove{x #\n\r", ch);
			return FALSE;
		}

		if (!str_prefix(arg2, "remove"))
		{
			char buf[MSL];
			if (argument[0] == '\0')
			{
				send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
				sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			if (!is_number(argument))
			{
				send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
				sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			if (list_size(dng->special_exits) < 1)
			{
				send_to_char("There are no special exits.\n\r", ch);
				return FALSE;
			}

			int index = atoi(argument);
			if (index < 1 || index > list_size(dng->special_exits))
			{
				send_to_char("Syntax:  special exit remove {R#{x\n\r", ch);
				sprintf(buf, "         Please specify a number from 1 to %d.\n\r", list_size(dng->special_exits));
				send_to_char(buf, ch);
				return FALSE;
			}

			list_remnthlink(dng->special_exits, index);
			send_to_char("Special exit removed.\n\r", ch);
			return TRUE;
		}

		send_to_char("Syntax:  special exit {Rlist{x\n\r", ch);
		send_to_char("         special exit {Radd{x static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Radd{x source generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Radd{x destination generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Radd{x weighted\n\r", ch);
		send_to_char("         special exit {Radd{x group\n\r", ch);
		send_to_char("         special exit {Rfrom{x # list\n\r", ch);
		send_to_char("         special exit {Rfrom{x # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Rfrom{x # set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Rfrom{x # remove #\n\r", ch);
		send_to_char("         special exit {Rto{x # list\n\r", ch);
		send_to_char("         special exit {Rto{x # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rto{x # set[ #] <weight> <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rto{x # remove #\n\r", ch);
		send_to_char("         special exit {Rgroup{x # add static generated|ordinal <from-level> <from-exit> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # add source generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # add destination generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # add weighted\n\r", ch);
		send_to_char("         special exit {Rgroup{x # add group\n\r", ch);
		send_to_char("         special exit {Rgroup{x # from # list\n\r", ch);
		send_to_char("         special exit {Rgroup{x # from # add <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # from # set[ #] <weight> generated|ordinal <from-level> <from-exit>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # from # remove #\n\r", ch);
		send_to_char("         special exit {Rgroup{x # to # list\n\r", ch);
		send_to_char("         special exit {Rgroup{x # to # add <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # to # set[ #] <weight> generated|ordinal <to-level> <to-entrance>\n\r", ch);
		send_to_char("         special exit {Rgroup{x # to # remove #\n\r", ch);
		send_to_char("         special exit {Rgroup{x # remove #\n\r", ch);
		send_to_char("         special exit {Rremove{x #\n\r", ch);
		return FALSE;
	}

	dngedit_special(ch, "");
	return FALSE;
}

DNGEDIT (dngedit_adddprog)
{
    int tindex, slot;
	DUNGEON_INDEX_DATA *dungeon;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_DUNGEON(ch, dungeon);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

	WNUM wnum;

    if (!parse_widevnum(num, ch->in_room->area, &wnum) || trigger[0] =='\0' || phrase[0] =='\0')
    {
		send_to_char("Syntax:   adddprog [wnum] [trigger] [phrase]\n\r",ch);
		return FALSE;
    }

    if ((tindex = trigger_index(trigger, PRG_DPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "dprog");
	return FALSE;
    }

    slot = trigger_table[tindex].slot;
	if(!wnum.pArea) wnum.pArea = dungeon->area;

    if ((code = get_script_index (wnum.pArea, wnum.vnum, PRG_DPROG)) == NULL)
    {
	send_to_char("No such DUNGEONProgram.\n\r",ch);
	return FALSE;
    }

    // Make sure this has a list of progs!
    if(!dungeon->progs) dungeon->progs = new_prog_bank();

    list                  = new_trigger();
    list->wnum            = wnum;
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;

    list_appendlink(dungeon->progs[slot], list);

    send_to_char("Dprog Added.\n\r",ch);
    return TRUE;
}

DNGEDIT (dngedit_deldprog)
{
    DUNGEON_INDEX_DATA *dungeon;
    char dprog[MAX_STRING_LENGTH];
    int value;

    EDIT_DUNGEON(ch, dungeon);

    one_argument(argument, dprog);
    if (!is_number(dprog) || dprog[0] == '\0')
    {
       send_to_char("Syntax:  deldprog [#dprog]\n\r",ch);
       return FALSE;
    }

    value = atol (dprog);

    if (value < 0)
    {
        send_to_char("Only non-negative dprog-numbers allowed.\n\r",ch);
        return FALSE;
    }

    if(!edit_deltrigger(dungeon->progs,value)) {
	send_to_char("No such dprog.\n\r",ch);
	return FALSE;
    }

    send_to_char("Dprog removed.\n\r", ch);
    return TRUE;
}

DNGEDIT(dngedit_varset)
{
    DUNGEON_INDEX_DATA *dungeon;

	EDIT_DUNGEON(ch, dungeon);

	return olc_varset(&dungeon->index_vars, ch, argument);
}

DNGEDIT(dngedit_varclear)
{
    DUNGEON_INDEX_DATA *dungeon;

	EDIT_DUNGEON(ch, dungeon);

	return olc_varclear(&dungeon->index_vars, ch, argument);
}



//////////////////////////////////////////////////////////////
//
// Immortal Commands
//
void do_dungeon(CHAR_DATA *ch, char *argument)
{
	char arg1[MIL];

	if( IS_NPC(ch) ) return;

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  dungeon leave\n\r", ch);
		if( ch->tot_level >= MAX_LEVEL )
		{
			send_to_char("         dungeon list\n\r", ch);
			send_to_char("         dungeon unload #\n\r", ch);
		}
		return;
	}

	argument = one_argument(argument, arg1);

	if( !str_prefix(arg1, "leave") )
	{
		DUNGEON *dungeon = get_room_dungeon(ch->in_room);

		if( !IS_VALID(dungeon) )
		{
			send_to_char("You are not in a dungeon.\n\r", ch);
			return;
		}

		// Prevent them from just up and leaving the dungeon mid... anything
		if( ch->position != POS_STANDING )
		{
			switch( ch->position )
			{
			case POS_DEAD:
				send_to_char( "Lie still; you are DEAD.\n\r", ch );
				break;

			case POS_MORTAL:
			case POS_INCAP:
				send_to_char( "You are far too hurt for that.\n\r", ch );
				break;

			case POS_STUNNED:
				send_to_char( "You are too stunned to do that.\n\r", ch );
				break;

			case POS_SLEEPING:
				send_to_char( "In your dreams, or what?\n\r", ch );
				break;

			case POS_RESTING:
				send_to_char( "You are resting at the moment.\n\r", ch);
				break;

			case POS_SITTING:
				send_to_char( "Better stand up first.\n\r",ch);
				break;

			case POS_FIGHTING:
				send_to_char( "No way!  You are still fighting!\n\r", ch);
				break;
			}

			return;
		}

		ROOM_INDEX_DATA *room = dungeon->entry_room;

		if( !room )
			room = room_index_temple;

		// Should deal with their mount and pet if they have one
		char_from_room(ch);
		char_to_room(ch, room);

		if (ch->pet != NULL)
		{
			char_from_room (ch->pet);
			char_to_room(ch->pet, room);
		}

		act("{Y$n leaves $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dungeon->index->name, TO_ROOM);
		act("{YYou leave $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dungeon->index->name, TO_CHAR);
		do_function(ch, &do_look, "auto");
		return;
	}


	if( ch->tot_level >= MAX_LEVEL )
	{
		if( !str_prefix(arg1, "list") )
		{

			if(!ch->lines)
				send_to_char("{RWARNING:{W Having scrolling off may limit how many dungeons you can see.{x\n\r", ch);

			int lines = 0;
			bool error = FALSE;
			BUFFER *buffer = new_buf();
			char buf[MSL];


			ITERATOR it;
			DUNGEON *dungeon;

			iterator_start(&it, loaded_dungeons);
			while((dungeon = (DUNGEON *)iterator_nextdata(&it)))
			{
				sprintf(buf, "dungeon list: %ld, %s", dungeon->index->vnum, dungeon->index->name);
				wiznet(buf,NULL,NULL,WIZ_TESTING,0,0);

				char plr_str[21];
				char idle_str[21];
				++lines;

				int players = list_size(dungeon->players);

				if( players > 0 )
				{
					snprintf(plr_str, 20, "{W%d", players);
					plr_str[20] = '\0';
				}
				else
				{
					strcpy(plr_str, "{Dempty");
				}

				if( (IS_SET(dungeon->flags, DUNGEON_DESTROY) || !IS_SET(dungeon->flags, DUNGEON_IDLE_ON_COMPLETE) || IS_SET(dungeon->flags, DUNGEON_COMPLETED)) &&
					dungeon->idle_timer > 0 )
				{
					snprintf(idle_str, 20, "{G%d", dungeon->idle_timer);
					idle_str[20] = '\0';
				}
				else
				{
					strcpy(idle_str, "{YActive");
				}

				char color = 'G';

				if( IS_SET(dungeon->flags, DUNGEON_DESTROY) )
					color = 'R';
				else if( IS_SET(dungeon->flags, DUNGEON_COMPLETED) )
					color = 'W';

				sprintf(buf, "%4d {Y[{W%5ld{Y] {%c%-30.30s   %13.13s   %8.8s{x\n\r",
					lines,
					dungeon->index->vnum,
					color, dungeon->index->name,
					plr_str, idle_str);

				if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
				{
					error = TRUE;
					break;
				}
			}
			iterator_stop(&it);

			if( error )
			{
				send_to_char("Too many dungeons to list.  Please shorten!\n\r", ch);
			}
			else
			{
				if( !lines )
				{
					add_buf( buffer, "No dungeons to display.\n\r" );
				}
				else
				{
					// Header
					send_to_char("{Y      Vnum   [            Name            ] [  Players  ] [ Idle ]{x\n\r", ch);
					send_to_char("{Y==================================================================={x\n\r", ch);
				}

				page_to_char(buffer->string, ch);
			}
			free_buf(buffer);

			return;
		}

		if( !str_prefix(arg1, "unload") )
		{
			char buf[MSL];

			if( !can_edit_dungeons(ch) )
			{
				send_to_char("Insufficient access to unload dungeons.\n\r", ch);
				return;
			}

			if( !is_number(argument) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return;
			}

			int index = atoi(argument);

			if( list_size(loaded_dungeons) < index )
			{
				send_to_char("No dungeon at that index.\n\r", ch);
				return;
			}

			// Set a flag on the dungeon and set the idle timer
			DUNGEON *dungeon = (DUNGEON *)list_nthdata(loaded_dungeons, index);

			if( list_size(dungeon->players) > 0 )
			{
				if( IS_SET(dungeon->flags, DUNGEON_DESTROY) )
				{
					send_to_char("Dungeon is already flagged for unloading.\n\r", ch);
					return;
				}

				SET_BIT(dungeon->flags, DUNGEON_DESTROY);
				if( dungeon->idle_timer > 0 )
					dungeon->idle_timer = UMIN(DUNGEON_DESTROY_TIMEOUT, dungeon->idle_timer);
				else
					dungeon->idle_timer = DUNGEON_DESTROY_TIMEOUT;

				sprintf(buf, "{RWARNING: Dungeon is being forcibly unloaded.  You have %d minutes to escape before the end!{x\n\r", dungeon->idle_timer);
				dungeon_echo(dungeon, buf);

				send_to_char("Dungeon flagged for unloading.\n\r", ch);
			}
			else
			{
				extract_dungeon(dungeon);
				send_to_char("Dungeon unloaded.\n\r", ch);
			}
			return;
		}
	}

	do_dungeon(ch, "");
	return;
}

//////////////////////////////////////////////////////////
//
// Dungeon Save/Load
//
void dungeon_save(FILE *fp, DUNGEON *dungeon)
{
	ITERATOR it;
	INSTANCE *instance;
	LLIST_UID_DATA *luid;

	fprintf(fp, "#DUNGEON %ld#%ld\n\r", dungeon->index->area->uid, dungeon->index->vnum);
	fprintf(fp, "Uid %ld %ld\n\r", dungeon->uid[0], dungeon->uid[1]);
	// ->entry_room - not saved... resolved on load
	// ->exit_room - not saved...  resolved on load

	fprintf(fp, "Flags %d\n\r", dungeon->flags);

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "Player %lu %lu\n\r", luid->id[0], luid->id[1]);
	}
	iterator_stop(&it);

	if( dungeon->idle_timer > 0 )
	{
		fprintf(fp, "IdleTimer %d\n\r", dungeon->idle_timer);
	}

	iterator_start(&it, dungeon->floors);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		instance_save(fp, instance);
	}
	iterator_stop(&it);


	fprintf(fp, "#-DUNGEON\n\r");
}

void dungeon_tallyentities(DUNGEON *dungeon, INSTANCE *instance)
{
	list_appendlist(dungeon->mobiles, instance->mobiles);
	list_appendlist(dungeon->objects, instance->objects);
	list_appendlist(dungeon->rooms, instance->rooms);
	list_appendlist(dungeon->bosses, instance->bosses);
}

DUNGEON *dungeon_load(FILE *fp)
{
	char *word;
	bool fMatch;

	DUNGEON *dungeon = new_dungeon();
	WNUM_LOAD wnum = fread_widevnum(fp, 0);

	dungeon->index = get_dungeon_index_auid(wnum.auid, wnum.vnum);

	dungeon->progs			= new_prog_data();
	dungeon->progs->progs	= dungeon->index->progs;
	variable_copylist(&dungeon->index->index_vars,&dungeon->progs->vars,FALSE);


	dungeon->entry_room = get_room_index(dungeon->index->area,dungeon->index->entry_room);
	dungeon->exit_room = get_room_index(dungeon->index->area,dungeon->index->exit_room);

	while (str_cmp((word = fread_word(fp)), "#-DUNGEON"))
	{
		fMatch = FALSE;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#INSTANCE") )
			{
				INSTANCE *instance = instance_load(fp);

				if( instance )
				{
					instance->dungeon = dungeon;
					list_appendlink(dungeon->floors, instance);

					dungeon_tallyentities(dungeon, instance);
				}

				fMatch = TRUE;
				break;
			}

			break;

		case 'F':
			KEY("Flags", dungeon->flags, fread_number(fp));
			break;

		case 'I':
			KEY("IdleTimer", dungeon->idle_timer, fread_number(fp));
			break;

		case 'P':
			if( !str_cmp(word, "Player") )
			{
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				dungeon_addowner_playerid(dungeon, id1, id2);

				fMatch = TRUE;
				break;
			}
			break;

		case 'U':
			if( !str_cmp(word, "Uid") )
			{
				dungeon->uid[0] = fread_number(fp);
				dungeon->uid[1] = fread_number(fp);

				fMatch = TRUE;
				break;
			}

			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "dungeon_load: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	get_dungeon_id(dungeon);

	log_stringf("dungeon_load: dungeon %ld loaded", dungeon->index->vnum);
	return dungeon;
}

void resolve_dungeon_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == ch->id[0] && luid->id[1] == ch->id[1])
		{
			luid->ptr = ch;
			break;
		}
	}
	iterator_stop(&it);
}

void resolve_dungeons_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	DUNGEON *dungeon;
	iterator_start(&it, loaded_dungeons);
	while( (dungeon = (DUNGEON *)iterator_nextdata(&it)) )
	{
		resolve_dungeon_player(dungeon, ch);
	}
	iterator_stop(&it);

}

void detach_dungeon_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == ch->id[0] && luid->id[1] == ch->id[1])
		{
			luid->ptr = NULL;
			break;
		}
	}
	iterator_stop(&it);
}

void detach_dungeons_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	DUNGEON *dungeon;
	iterator_start(&it, loaded_dungeons);
	while( (dungeon = (DUNGEON *)iterator_nextdata(&it)) )
	{
		detach_dungeon_player(dungeon, ch);

		// Check player quests
	}
	iterator_stop(&it);

}

void dungeon_echo(DUNGEON *dungeon, char *text)
{
	if( !IS_VALID(dungeon) || IS_NULLSTR(text) ) return;

	ITERATOR it;
	CHAR_DATA *ch;

	iterator_start(&it, dungeon->players);
	while( (ch = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		send_to_char(text, ch);
		send_to_char("\n\r", ch);
	}
	iterator_stop(&it);
}

ROOM_INDEX_DATA *dungeon_random_room(CHAR_DATA *ch, DUNGEON *dungeon)
{
	if( !IS_VALID(dungeon) ) return NULL;

	return get_random_room_list_byflags( ch, dungeon->rooms,
		(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CHAOTIC),
		ROOM_NO_GET_RANDOM );
}

DUNGEON *get_room_dungeon(ROOM_INDEX_DATA *room)
{
	if( !room ) return NULL;

	if( !IS_VALID(room->instance_section) ) return NULL;

	if( !IS_VALID(room->instance_section->instance) ) return NULL;

	if( !IS_VALID(room->instance_section->instance->dungeon) ) return NULL;

	return room->instance_section->instance->dungeon;
}

OBJ_DATA *get_room_dungeon_portal(ROOM_INDEX_DATA *room, WNUM wnum)
{
	OBJ_DATA *obj;

	if( get_room_dungeon(room) ) return NULL;

	for(obj = room->contents; obj; obj = obj->next_content)
	{
		if( obj->item_type == ITEM_PORTAL &&
			obj->value[3] == GATETYPE_DUNGEON &&
			obj->pIndexData->area == wnum.pArea &&
			obj->value[5] == wnum.vnum)
		{
			return obj;
		}
	}

	return NULL;
}

ROOM_INDEX_DATA *get_dungeon_special_room(DUNGEON *dungeon, int index)
{
	if( !IS_VALID(dungeon) || index < 1) return NULL;

	NAMED_SPECIAL_ROOM *special = list_nthdata(dungeon->special_rooms, index);

	if( IS_VALID(special) )
		return special->room;

	return NULL;
}

ROOM_INDEX_DATA *get_dungeon_special_room_byname(DUNGEON *dungeon, char *name)
{
	int number;
	char arg[MSL];

	if( !IS_VALID(dungeon) ) return NULL;

	number = number_argument(name, arg);

	if( number < 1 ) return NULL;

	ITERATOR it;
	ROOM_INDEX_DATA *room = NULL;
	NAMED_SPECIAL_ROOM *special;
	iterator_start(&it, dungeon->special_rooms);
	while( (special = (NAMED_SPECIAL_ROOM *)iterator_nextdata(&it)) )
	{
		if( is_name(arg, special->name) )
		{
			if( !--number )
			{
				room = special->room;
				break;
			}
		}
	}
	iterator_stop(&it);

	return room;
}

int dungeon_count_mob(DUNGEON *dungeon, MOB_INDEX_DATA *pMobIndex)
{
	ITERATOR it;
	INSTANCE *instance;

	if( !IS_VALID(dungeon) || !pMobIndex ) return 0;

	int count = 0;
	iterator_start(&it, dungeon->floors);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		count += instance_count_mob(instance, pMobIndex);
	}
	iterator_stop(&it);

	return count;
}

void dungeon_addowner_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	// Don't add twice
	if( dungeon_isowner_player(dungeon, ch) ) return;

	LLIST_UID_DATA *luid = new_list_uid_data();
	luid->id[0] = ch->id[0];
	luid->id[1] = ch->id[1];
	luid->ptr = ch;

	list_appendlink(dungeon->player_owners, luid);
}

void dungeon_addowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2)
{
	// Don't add twice
	if( dungeon_isowner_playerid(dungeon, id1, id2) ) return;

	LLIST_UID_DATA *luid = new_list_uid_data();
	luid->id[0] = id1;
	luid->id[1] = id2;
	luid->ptr = NULL;

	list_appendlink(dungeon->player_owners, luid);
}

void dungeon_removeowner_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == ch->id[0] && luid->id[1] == ch->id[1] )
		{
			iterator_remcurrent(&it);
			break;
		}
	}
	iterator_stop(&it);
}

void dungeon_removeowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == id1 && luid->id[1] == id2 )
		{
			iterator_remcurrent(&it);
			break;
		}
	}
	iterator_stop(&it);
}

bool dungeon_isowner_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return false;

	ITERATOR it;
	LLIST_UID_DATA *luid;
	bool ret = false;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == ch->id[0] && luid->id[1] == ch->id[1] )
		{
			ret = true;
			break;
		}
	}
	iterator_stop(&it);

	return ret;
}

bool dungeon_isowner_playerid(DUNGEON *dungeon, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;
	bool ret = false;

	iterator_start(&it, dungeon->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == id1 && luid->id[1] == id2)
		{
			ret = true;
			break;
		}
	}
	iterator_stop(&it);

	return ret;
}

bool dungeon_canswitch_player(DUNGEON *dungeon, CHAR_DATA *ch)
{
	// TODO: Add lockout system
	return true;
}

bool dungeon_isorphaned(DUNGEON *dungeon)
{
	if( list_size(dungeon->player_owners) > 0 ) return false;

	// Any other owners?

	return true;
}
