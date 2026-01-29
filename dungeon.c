/***************************************************************************
 *  File: olc_act.c                                                        *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "strings.h"
#include "merc.h"
#include "tables.h"
#include "olc.h"
#include "recycle.h"
#include "interp.h"
#include "scripts.h"
#include "wilds.h"


INSTANCE *instance_load(FILE *fp);
void update_instance(INSTANCE *instance);
void reset_instance(INSTANCE *instance);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);

extern LLIST *loaded_instances;

bool dungeons_changed = false;
long top_dungeon_vnum = 0;
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
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (mode == LEVELMODE_GROUP)
			{
				if (!str_cmp(word, "#STATICLEVEL"))
				{
					DUNGEON_INDEX_LEVEL_DATA *lvl = load_dungeon_index_level(fp, LEVELMODE_STATIC);

					list_appendlink(level->group, lvl);
					fMatch = true;
				}

				if (!str_cmp(word, "#WEIGHTEDLEVEL"))
				{
					DUNGEON_INDEX_LEVEL_DATA *lvl = load_dungeon_index_level(fp, LEVELMODE_WEIGHTED);

					list_appendlink(level->group, lvl);
					fMatch = true;
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

				fMatch = true;
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

	while (str_cmp((word = fread_word(fp)), "#-EXIT"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (mode != EXITMODE_GROUP)
			{
				if (!str_cmp(word, "#STATICEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_STATIC);

					list_appendlink(ex->group, gex);
					fMatch = true;
					break;
				}

				if (!str_cmp(word, "#SOURCEEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_SOURCE);

					list_appendlink(ex->group, gex);
					fMatch = true;
					break;
				}

				if (!str_cmp(word, "#DESTEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_DEST);

					list_appendlink(ex->group, gex);
					fMatch = true;
					break;
				}

				if (!str_cmp(word, "#WEIGHTEDEXIT"))
				{
					DUNGEON_INDEX_SPECIAL_EXIT *gex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED);

					list_appendlink(ex->group, gex);
					fMatch = true;
					break;
				}
			}
			break;

		case 'F':
			if (!str_cmp(word, "From"))
			{
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
				
				fMatch = true;
				break;
			}
			break;

		case 'T':
			if (!str_cmp(word, "To"))
			{
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

				fMatch = true;
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

DUNGEON_INDEX_DATA *load_dungeon_index(FILE *fp)
{
	DUNGEON_INDEX_DATA *dng;
	char *word;
	bool fMatch;
	char buf[MSL];

	dng = new_dungeon_index();
	dng->vnum = fread_number(fp);

	if( dng->vnum > top_dungeon_vnum)
		top_dungeon_vnum = dng->vnum;

	while (str_cmp((word = fread_word(fp)), "#-DUNGEON"))
	{
		fMatch = false;

		switch(word[0])
		{

		case '#':
			if (!str_cmp(word, "#STATICLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_STATIC);

				list_appendlink(dng->levels, level);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#WEIGHTEDLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_WEIGHTED);

				list_appendlink(dng->levels, level);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#GROUPLEVEL"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = load_dungeon_index_level(fp, LEVELMODE_GROUP);

				list_appendlink(dng->levels, level);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#STATICEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_STATIC);

				list_appendlink(dng->special_exits, ex);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#SOURCEEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_SOURCE);

				list_appendlink(dng->special_exits, ex);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#DESTEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED_DEST);

				list_appendlink(dng->special_exits, ex);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#WEIGHTEDEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_WEIGHTED);

				list_appendlink(dng->special_exits, ex);
				fMatch = true;
				break;
			}

			if (!str_cmp(word, "#GROUPEXIT"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = load_dungeon_index_special_exit(fp, EXITMODE_GROUP);

				list_appendlink(dng->special_exits, ex);
				fMatch = true;
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


				long vnum = fread_number(fp);
				p = fread_string(fp);

				tindex = trigger_index(p, PRG_DPROG);
				if(tindex < 0) {
					sprintf(buf, "load_dungeon_index: invalid trigger type %s", p);
					bug(buf, 0);
				} else {
					PROG_LIST *dpr = new_trigger();

					dpr->vnum = vnum;
					dpr->trig_type = tindex;
					dpr->trig_phrase = fread_string(fp);
					if( tindex == TRIG_SPELLCAST ) {
						char buf[MIL];
						int tsn = skill_lookup(dpr->trig_phrase);

						if( tsn < 0 ) {
							sprintf(buf, "load_dungeon_index: invalid spell '%s' for TRIG_SPELLCAST", p);
							bug(buf, 0);
							free_trigger(dpr);
							fMatch = true;
							break;
						}

						free_string(dpr->trig_phrase);
						sprintf(buf, "%d", tsn);
						dpr->trig_phrase = str_dup(buf);
						dpr->trig_number = tsn;
						dpr->numeric = true;

					} else {
						dpr->trig_number = atoi(dpr->trig_phrase);
						dpr->numeric = is_number(dpr->trig_phrase);
					}

					if(!dng->progs) dng->progs = new_prog_bank();

					list_appendlink(dng->progs[trigger_table[tindex].slot], dpr);
				}
				fMatch = true;
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
				long bp_vnum = fread_number(fp);

				BLUEPRINT *bp = get_blueprint(bp_vnum);

				if( bp )
				{
					list_appendlink(dng->floors, bp);
				}

				fMatch = true;
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
				fMatch = true;
				break;
			}
			break;

		case 'V':
			if (!str_cmp(word, "VarInt")) {
				char *name;
				int value;
				bool saved;

				fMatch = true;

				name = fread_string(fp);
				saved = fread_number(fp);
				value = fread_number(fp);

				variables_setindex_integer (&dng->index_vars,name,value,saved);
			}

			if (!str_cmp(word, "VarStr")) {
				char *name;
				char *str;
				bool saved;

				fMatch = true;

				name = fread_string(fp);
				saved = fread_number(fp);
				str = fread_string(fp);

				variables_setindex_string (&dng->index_vars,name,str,false,saved);
			}

			if (!str_cmp(word, "VarRoom")) {
				char *name;
				int value;
				bool saved;

				fMatch = true;

				name = fread_string(fp);
				saved = fread_number(fp);
				value = fread_number(fp);

				variables_setindex_room (&dng->index_vars,name,value,saved);
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
		fMatch = false;

		if( !str_cmp(word, "#DUNGEON") )
		{
			DUNGEON_INDEX_DATA *dng = load_dungeon_index(fp);
			int iHash = dng->vnum % MAX_KEY_HASH;

			dng->next = dungeon_index_hash[iHash];
			dungeon_index_hash[iHash] = dng;

			fMatch = true;
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

		    fMatch = true;
		}


		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_dungeons: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	fclose(fp);
}

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
					save_dungeon_index_special_exit(fp, gex, false);
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
				fprintf(fp, "DungeonProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	if(dng->index_vars) {
		for(pVARIABLE var = dng->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}


	fprintf(fp, "#-DUNGEON\n\n");
}

bool save_dungeons()
{
	FILE *fp = fopen(DUNGEONS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save dungeons.dat", 0);
		return false;
	}

	int iHash;
	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = dungeon_index_hash[iHash]; dng; dng = dng->next)
		{
			save_dungeon_index(fp, dng);
		}
	}

	for( SCRIPT_DATA *scr = dprog_list; scr; scr = scr->next)
	{
		save_script_new(fp,NULL,scr,"DUNGEON");
	}

	fprintf(fp, "#END\n");

	fclose(fp);

	dungeons_changed = false;
	return true;
}

bool can_edit_dungeons(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}


DUNGEON_INDEX_DATA *get_dungeon_index(long vnum)
{
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(DUNGEON_INDEX_DATA *dng = dungeon_index_hash[iHash]; dng; dng = dng->next)
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
		return true;

	INSTANCE *instance = create_instance(bp);

	if( !instance )
		return true;

	instance->dungeon = dng;
	list_appendlink(dng->floors, instance);
	instance->floor = list_size(dng->floors);
	list_appendlist(dng->rooms, instance->rooms);
	list_appendlink(loaded_instances, instance);
	return false;
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
				return true;
			}

			return add_dungeon_instance(dng, bp);
		}

		case LEVELMODE_GROUP:
		{
			bool error = false;
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
					error = true;
					break;
				}
			}

			free_mem(source, sizeof(int) * count);
			return error;
		}
	}

	return true;
}

/*
static bool add_dungeon_levels(DUNGEON *dng)
{
	bool error = false;
	DUNGEON_INDEX_LEVEL_DATA *level;
	ITERATOR lit;

	list_clear(dng->floors);
	list_clear(dng->rooms);
	iterator_start(&lit, dng->index->levels);
	while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&lit)) )
	{
		if (add_dungeon_level(dng, level))
		{
			error = true;
			break;
		}
	}
	iterator_stop(&lit);

	return error;
}
*/

static DUNGEON_INDEX_WEIGHTED_EXIT_DATA *get_weighted_random_exit(LLIST *list, int total)
{
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

static bool add_dungeon_special_exit(DUNGEON *dng, DUNGEON_INDEX_SPECIAL_EXIT *dsex)
{
	bool error = false;
	ITERATOR it;
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
			DUNGEON_INDEX_SPECIAL_EXIT *special;
			iterator_start(&it, dsex->group);
			while( (special = (DUNGEON_INDEX_SPECIAL_EXIT *)iterator_nextdata(&it)) )
			{
				if (add_dungeon_special_exit(dng, special))
				{
					error = true;
					break;
				}
			}
			iterator_stop(&it);
			return error;
		}
	}

	if (!from || !to)
	{
		// Failed to get an exit reference
		return true;
	}

	INSTANCE *from_level = (INSTANCE *)list_nthdata(dng->floors, from->level);
	if (!IS_VALID(from_level))
	{
		return true;
	}

	INSTANCE *to_level = (INSTANCE *)list_nthdata(dng->floors, to->level);
	if (!IS_VALID(to_level))
	{
		return true;
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
								toClone->door.rs_lock.key_vnum = from_exit->door.rs_lock.key_vnum;
								// TODO: toClone->door.rs_lock.keys = from_exit->door.rs_lock.keys;
								toClone->door.rs_lock.pick_chance = from_exit->door.rs_lock.pick_chance;
							}
							else
							{
								toClone->rs_flags = 0;
								toClone->door.rs_lock.flags = 0;
								toClone->door.rs_lock.key_vnum = 0;
								// TODO: toClone->door.rs_lock.keys....
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
					fromClone->door.rs_lock.key_vnum = to_exit->door.rs_lock.key_vnum;
					// TODO: fromClone->door.rs_lock.keys = to_exit->door.rs_lock.keys;
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

	return false;
}

DUNGEON *create_dungeon(long vnum)
{
	ITERATOR it;

	DUNGEON_INDEX_DATA *index = get_dungeon_index(vnum);

	if( !IS_VALID(index) )
	{
		return NULL;
	}

	DUNGEON *dng = new_dungeon();
	dng->index = index;
	dng->flags = index->flags;

	dng->progs			= new_prog_data();
	dng->progs->progs	= index->progs;
	variable_copylist(&index->index_vars,&dng->progs->vars,false);

	// TODO: update for widevnum
	dng->entry_room = get_room_index(index->area, index->entry_room);
	if( !dng->entry_room )
	{
		free_dungeon(dng);
		return NULL;
	}

	// TODO: update for widevnum
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

	bool error = false;
	DUNGEON_INDEX_LEVEL_DATA *level;
	//BLUEPRINT *bp;
	INSTANCE *instance;
	iterator_start(&it, index->levels);
	while( (level = (DUNGEON_INDEX_LEVEL_DATA *)iterator_nextdata(&it)) )
	{
		if (add_dungeon_level(dng, level))
		{
			error = true;
			break;
		}
	}
	iterator_stop(&it);

	if (!error)
	{
		DUNGEON_INDEX_SPECIAL_ROOM *special;
		iterator_start(&it, index->special_rooms);
		while( (special = (DUNGEON_INDEX_SPECIAL_ROOM *)iterator_nextdata(&it)) )
		{
			// Get the instance for the specified level.
			instance = (INSTANCE *)list_nthdata(dng->floors, special->level);

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
				error = true;
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
			dungeon->progs->extract_when_done = true;
			return;
		}
    }

	room = dungeon->entry_room;
	if( !room ) {
		AREA_DATA *fallback_area = find_area_by_vnum(11001);
		if (!fallback_area) fallback_area = get_system_area_fallback();
		room = get_room_index(fallback_area, 11001);
	}

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
		room = get_reserved_room_index("room_donation");

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


	list_remlink(loaded_dungeons, dungeon, false);

	// Remove instances from loaded list
	iterator_start(&it, dungeon->floors);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		list_remlink(loaded_instances, instance, true);
	}
	iterator_stop(&it);

	free_dungeon(dungeon);
}

// TODO: WIDEVNUM
DUNGEON *find_dungeon_byplayer(CHAR_DATA *ch, long vnum)
{
	ITERATOR dit;
	DUNGEON *dng;

	if( IS_NPC(ch) ) return NULL;

	iterator_start(&dit, loaded_dungeons);
	while( (dng = (DUNGEON *)iterator_nextdata(&dit)) )
	{
		if( dng->index->vnum == vnum && dungeon_isowner_player(dng, ch) )
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

// TODO: WIDEVNUM
ROOM_INDEX_DATA *spawn_dungeon_player(CHAR_DATA *ch, long vnum, int floor)
{
	CHAR_DATA *leader = get_player_leader(ch);

	DUNGEON *leader_dng = find_dungeon_byplayer(leader, vnum);
	DUNGEON *ch_dng = find_dungeon_byplayer(ch, vnum);

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

		leader_dng = create_dungeon(vnum);

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

	INSTANCE *instance = (INSTANCE *)list_nthdata(leader_dng->floors, floor);

	if( !IS_VALID(instance) )
	{
		return NULL;
	}

	return instance->entrance;
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
			dungeon->empty = false;
	}
	else if( list_size(dungeon->players) < 1 )
	{
		dungeon->empty = true;
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
	{ "show",			dngedit_show		},
	{ "special",		dngedit_special		},
	{ "varclear",		dngedit_varclear	},
	{ "varset",			dngedit_varset		},
	{ "zoneout",		dngedit_zoneout		},
	{ NULL,				NULL				}

};

void list_dungeons(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_dungeons(ch) )
	{
		send_to_char("You do not have access to dungeons.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many dungeons you can see.{x\n\r", ch);

	int lines = 0;
	bool error = false;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_dungeon_vnum; vnum++)
	{
		DUNGEON_INDEX_DATA *dng = get_dungeon_index(vnum);

		if( dng )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s\n\r",
				vnum,
				dng->name);

			++lines;
			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = true;
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
			send_to_char("{Y Vnum   [            Name            ]{x\n\r", ch);
			send_to_char("{Y======================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
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
	long value;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (!can_edit_dungeons(ch))
	{
		send_to_char("DNGEdit:  Insufficient security to edit dungeons.\n\r", ch);
		return;
	}

	if (is_number(arg1))
	{
		value = atol(arg1);
		if (!(dng = get_dungeon_index(value)))
		{
			send_to_char("DNGEdit:  That vnum does not exist.\n\r", ch);
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
				dungeons_changed = true;
				ch->pcdata->immortal->last_olc_command = current_time;
				ch->desc->editor = ED_DUNGEON;
			}

			return;
		}

	}

	send_to_char("Syntax: dngedit <vnum>\n\r"
				 "        dngedit create <vnum>\n\r", ch);
}

void dngedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_dungeons(ch))
	{
		send_to_char("DNGEdit:  Insufficient security to edit dungeons.\n\r", ch);
		edit_done(ch);
		return;
	}

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
				dungeons_changed = true;
			}

			return;
		}
	}

    interpret(ch, arg);
}



void do_dngshow(CHAR_DATA *ch, char *argument)
{
	DUNGEON_INDEX_DATA *dng;
	void *old_edit;
	long value;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  dngshow <vnum>\n\r", ch);
		return;
	}

	if (!is_number(argument))
	{
		send_to_char("Vnum must be a number.\n\r", ch);
		return;
	}

	value = atol(argument);
	if (!(dng= get_dungeon_index(value)))
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

		if( !room ) {
			AREA_DATA *fallback_area = find_area_by_vnum(11001);
			if (!fallback_area) fallback_area = get_system_area_fallback();
			room = get_room_index(fallback_area, 11001);
		}

		// Should deal with their mount and pet if they have one
		char_from_room(ch);
		char_to_room(ch, room);

		if (ch->pet != NULL)
		{
			char_from_room (ch->pet);
			char_to_room(ch->pet, room);
		}

		act("{Y$n leaves $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dungeon->index->name, TO_ROOM, NULL, NULL);
		act("{YYou leave $T.{x", ch, NULL, NULL, NULL, NULL, NULL, dungeon->index->name, TO_CHAR, NULL, NULL);
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
			bool error = false;
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
					error = true;
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

	fprintf(fp, "#DUNGEON %ld\n\r", dungeon->index->vnum);
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
	long vnum = fread_number(fp);

	dungeon->index = get_dungeon_index(vnum);

	dungeon->progs			= new_prog_data();
	dungeon->progs->progs	= dungeon->index->progs;
	variable_copylist(&dungeon->index->index_vars,&dungeon->progs->vars,false);


	dungeon->entry_room = get_room_index(dungeon->index->area, dungeon->index->entry_room);
	dungeon->exit_room = get_room_index(dungeon->index->area, dungeon->index->exit_room);

	while (str_cmp((word = fread_word(fp)), "#-DUNGEON"))
	{
		fMatch = false;

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

				fMatch = true;
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

				fMatch = true;
				break;
			}
			break;

		case 'U':
			if( !str_cmp(word, "Uid") )
			{
				dungeon->uid[0] = fread_number(fp);
				dungeon->uid[1] = fread_number(fp);

				fMatch = true;
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
		(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CPK),
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

OBJ_DATA *get_room_dungeon_portal(ROOM_INDEX_DATA *room, long vnum)
{
	OBJ_DATA *obj;

	if( get_room_dungeon(room) ) return NULL;

	for(obj = room->contents; obj; obj = obj->next_content)
	{
		if( (obj->item_type == ITEM_PORTAL) &&
			IS_SET(obj->value[2], GATE_DUNGEON) &&
			(obj->value[3] == vnum) )
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
