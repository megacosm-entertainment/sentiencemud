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

void room_update(ROOM_INDEX_DATA *room);
void save_script_new(FILE *fp, AREA_DATA *area,SCRIPT_DATA *scr,char *type);
SCRIPT_DATA *read_script_new( FILE *fp, AREA_DATA *area, int type);

bool blueprints_changed = false;
long top_blueprint_section_vnum = 0;
long top_blueprint_vnum = 0;
LLIST *loaded_instances;

void fix_blueprint_section(BLUEPRINT_SECTION *bs)
{
	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( bl->vnum < bs->lower_vnum || bl->vnum > bs->upper_vnum )
			continue;

		if( bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
		{
			bl->room = get_room_index(bl->vnum);

			if( bl->room )
				bl->ex = bl->room->exit[bl->door];
		}
	}
}

BLUEPRINT_LINK *load_blueprint_link(FILE *fp)
{
	BLUEPRINT_LINK *link;
	char *word;
	bool fMatch;

	link = new_blueprint_link();

	while (str_cmp((word = fread_word(fp)), "#-LINK"))
	{
		fMatch = false;
		switch(word[0])
		{
		case 'D':
			KEY("Door", link->door, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", link->name, fread_string(fp));
			break;

		case 'R':
			KEY("Room", link->vnum, fread_number(fp));
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_blueprint_link: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return link;
}

BLUEPRINT_SECTION *load_blueprint_section(FILE *fp)
{
	BLUEPRINT_SECTION *bs;
	char *word;
	bool fMatch;

	bs = new_blueprint_section();
	bs->vnum = fread_number(fp);

	if( bs->vnum > top_blueprint_section_vnum)
		top_blueprint_section_vnum = bs->vnum;

	while (str_cmp((word = fread_word(fp)), "#-SECTION"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#LINK") )
			{
				BLUEPRINT_LINK *link = load_blueprint_link(fp);

				// Append to the end
				link->next = NULL;
				if( bs->links )
				{
					BLUEPRINT_LINK *cur;

					for(cur = bs->links;cur->next; cur = cur->next)
					{
						;
					}

					cur->next = link;
				}
				else
				{
					bs->links = link;
				}

				fMatch = true;
				break;
			}
			break;

		case 'C':
			KEYS("Comments", bs->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", bs->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", bs->flags, fread_number(fp));
			break;

		case 'L':
			KEY("Lower", bs->lower_vnum, fread_number(fp));
			break;

		case 'N':
			KEYS("Name", bs->name, fread_string(fp));
			break;

		case 'R':
			KEY("Recall", bs->recall, fread_number(fp));
			break;

		case 'T':
			KEY("Type", bs->type, fread_number(fp));
			break;

		case 'U':
			KEY("Upper", bs->upper_vnum, fread_number(fp));
			break;
		}


		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_blueprint_section: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	fix_blueprint_section(bs);
	return bs;
}

BLUEPRINT *load_blueprint(FILE *fp)
{
	BLUEPRINT *bp;
	char *word;
	bool fMatch;
	char buf[MSL];

	bp = new_blueprint();
	bp->vnum = fread_number(fp);

	if( bp->vnum > top_blueprint_vnum)
		top_blueprint_vnum = bp->vnum;

	while (str_cmp((word = fread_word(fp)), "#-BLUEPRINT"))
	{
		fMatch = false;

		switch(word[0])
		{
		case 'A':
			KEY("AreaWho", bp->area_who, fread_number(fp));

		case 'C':
			KEYS("Comments", bp->comments, fread_string(fp));
			break;

		case 'D':
			KEYS("Description", bp->description, fread_string(fp));
			break;

		case 'F':
			KEY("Flags", bp->flags, fread_number(fp));
			break;

		case 'I':
			if (!str_cmp(word, "InstanceProg")) {
				int tindex;
				char *p;

				long vnum = fread_number(fp);
				p = fread_string(fp);

				tindex = trigger_index(p, PRG_IPROG);
				if(tindex < 0) {
					sprintf(buf, "load_blueprint: invalid trigger type %s", p);
					bug(buf, 0);
				} else {
					PROG_LIST *ipr = new_trigger();

					ipr->vnum = vnum;
					ipr->trig_type = tindex;
					ipr->trig_phrase = fread_string(fp);
					if( tindex == TRIG_SPELLCAST ) {
						char buf[MIL];
						int tsn = skill_lookup(ipr->trig_phrase);

						if( tsn < 0 ) {
							sprintf(buf, "load_blueprint: invalid spell '%s' for TRIG_SPELLCAST", p);
							bug(buf, 0);
							free_trigger(ipr);
							fMatch = true;
							break;
						}

						free_string(ipr->trig_phrase);
						sprintf(buf, "%d", tsn);
						ipr->trig_phrase = str_dup(buf);
						ipr->trig_number = tsn;
						ipr->numeric = true;

					} else {
						ipr->trig_number = atoi(ipr->trig_phrase);
						ipr->numeric = is_number(ipr->trig_phrase);
					}

					if(!bp->progs) bp->progs = new_prog_bank();

					list_appendlink(bp->progs[trigger_table[tindex].slot], ipr);
				}
				fMatch = true;
			}
			break;

		case 'N':
			KEYS("Name", bp->name, fread_string(fp));
			break;

		case 'R':
			KEY("Repop", bp->repop, fread_number(fp));
			break;

		case 'S':
			if( !str_cmp(word, "SpecialRoom") )
			{
				char *name = fread_string(fp);
				int section = fread_number(fp);
				long vnum = fread_number(fp);

				BLUEPRINT_SPECIAL_ROOM *special = new_blueprint_special_room();

				special->name = name;
				special->section = section;
				special->vnum = vnum;

				list_appendlink(bp->special_rooms, special);
				fMatch = true;
				break;
			}

			if( !str_cmp(word, "Static") )
			{
				bp->mode = BLUEPRINT_MODE_STATIC;

				fMatch = true;
				break;
			}

			KEY("StaticRecall", bp->static_recall, fread_number(fp));

			if( !str_cmp(word, "StaticEntry") )
			{
				int section = fread_number(fp);
				int link = fread_number(fp);

				bp->static_entry_section = section;
				bp->static_entry_link = link;

				fMatch = true;
				break;
			}

			if( !str_cmp(word, "StaticExit") )
			{
				int section = fread_number(fp);
				int link = fread_number(fp);

				bp->static_exit_section = section;
				bp->static_exit_link = link;

				fMatch = true;
				break;
			}

			if( !str_cmp(word, "StaticLink") )
			{
				int section1 = fread_number(fp);
				int link1 = fread_number(fp);
				int section2 = fread_number(fp);
				int link2 = fread_number(fp);

				STATIC_BLUEPRINT_LINK *sbl = new_static_blueprint_link();

				sbl->blueprint = bp;
				sbl->section1 = section1;
				sbl->link1 = link1;
				sbl->section2 = section2;
				sbl->link2 = link2;

				sbl->next = bp->static_layout;
				bp->static_layout = sbl;
				fMatch = true;
				break;
			}

			if( !str_cmp(word, "Section") )
			{
				long section = fread_number(fp);

				BLUEPRINT_SECTION *bs = get_blueprint_section(section);
				if( bs )
				{
					list_appendlink(bp->sections, bs);
				}
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

				variables_setindex_integer (&bp->index_vars,name,value,saved);
			}

			if (!str_cmp(word, "VarStr")) {
				char *name;
				char *str;
				bool saved;

				fMatch = true;

				name = fread_string(fp);
				saved = fread_number(fp);
				str = fread_string(fp);

				variables_setindex_string (&bp->index_vars,name,str,false,saved);
			}

			if (!str_cmp(word, "VarRoom")) {
				char *name;
				int value;
				bool saved;

				fMatch = true;

				name = fread_string(fp);
				saved = fread_number(fp);
				value = fread_number(fp);

				variables_setindex_room (&bp->index_vars,name,value,saved);
			}

			break;


		}


		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_blueprint: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	return bp;
}


// load blueprints
// CALLED AFTER ALL AREAS ARE LOADED
void load_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "r");
	if (fp == NULL)
	{
		bug("Couldn't load blueprints.dat", 0);
		return;
	}

	char *word;
	bool fMatch;

	top_iprog_index = 0;

	while (str_cmp((word = fread_word(fp)), "#END"))
	{
		fMatch = false;

		if( !str_cmp(word, "#SECTION") )
		{
			BLUEPRINT_SECTION *bs = load_blueprint_section(fp);
			int iHash = bs->vnum % MAX_KEY_HASH;

			bs->next = blueprint_section_hash[iHash];
			blueprint_section_hash[iHash] = bs;

			fMatch = true;
			continue;
		}

		if( !str_cmp(word, "#BLUEPRINT") )
		{
			BLUEPRINT *bp = load_blueprint(fp);
			int iHash = bp->vnum % MAX_KEY_HASH;

			bp->next = blueprint_hash[iHash];
			blueprint_hash[iHash] = bp;

			fMatch = true;
			continue;
		}

		if (!str_cmp(word, "#INSTANCEPROG"))
		{
		    SCRIPT_DATA *pr = read_script_new(fp, NULL, IFC_I);
		    if(pr) {
		    	pr->next = iprog_list;
		    	iprog_list = pr;

		    	if( pr->vnum > top_iprog_index )
		    		top_iprog_index = pr->vnum;
		    }

		    fMatch = true;
			continue;
		}


		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "load_blueprints: no match for word %.50s", word);
			bug(buf, 0);
		}

	}

	fclose(fp);
}

void save_blueprint_section(FILE *fp, BLUEPRINT_SECTION *bs)
{
	fprintf(fp, "#SECTION %ld\n", bs->vnum);
	fprintf(fp, "Name %s~\n", fix_string(bs->name));
	fprintf(fp, "Description %s~\n", fix_string(bs->description));
	fprintf(fp, "Comments %s~\n", fix_string(bs->comments));

	fprintf(fp, "Type %d\n", bs->type);
	fprintf(fp, "Flags %d\n", bs->flags);

	fprintf(fp, "Recall %ld\n", bs->recall);
	fprintf(fp, "Lower %ld\n", bs->lower_vnum);
	fprintf(fp, "Upper %ld\n", bs->upper_vnum);

	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( valid_section_link(bl) )
		{
			fprintf(fp, "#LINK\n");
			fprintf(fp, "Name %s~\n", fix_string(bl->name));
			fprintf(fp, "Room %ld\n", bl->vnum);
			fprintf(fp, "Door %d\n", bl->door);
			fprintf(fp, "#-LINK\n");
		}
	}

	fprintf(fp, "#-SECTION\n\n");
}


void save_blueprint(FILE *fp, BLUEPRINT *bp)
{

	fprintf(fp, "#BLUEPRINT %ld\n", bp->vnum);
	fprintf(fp, "Name %s~\n", fix_string(bp->name));
	fprintf(fp, "Description %s~\n", fix_string(bp->description));
	fprintf(fp, "Comments %s~\n", fix_string(bp->comments));
	fprintf(fp, "AreaWho %d\n", bp->area_who);
	fprintf(fp, "Repop %d\n", bp->repop);
	fprintf(fp, "Flags %d\n", bp->flags);

	ITERATOR sit;
	BLUEPRINT_SECTION *bs;
	iterator_start(&sit, bp->sections);
	while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
	{
		fprintf(fp, "Section %ld\n", bs->vnum);
	}
	iterator_stop(&sit);

	if( bp->mode == BLUEPRINT_MODE_STATIC )
	{
		fprintf(fp, "Static\n");

		if( bp->static_recall > 0 )
			fprintf(fp, "StaticRecall %d\n", bp->static_recall);

		if( bp->static_entry_section > 0 && bp->static_entry_link > 0 )
			fprintf(fp, "StaticEntry %d %d\n", bp->static_entry_section, bp->static_entry_link);

		if( bp->static_exit_section > 0 && bp->static_exit_link > 0 )
			fprintf(fp, "StaticExit %d %d\n", bp->static_exit_section, bp->static_exit_link);

		for(STATIC_BLUEPRINT_LINK *sbl = bp->static_layout; sbl; sbl = sbl->next)
		{
			if( valid_static_link(sbl) )
			{
				fprintf(fp, "StaticLink %d %d %d %d\n",
					sbl->section1, sbl->link1,
					sbl->section2, sbl->link2);
			}
		}

		ITERATOR rit;
		BLUEPRINT_SPECIAL_ROOM *special;
		iterator_start(&rit, bp->special_rooms);
		while( (special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&rit)) )
		{
			fprintf(fp, "SpecialRoom %s~ %d %ld\n", fix_string(special->name), special->section, special->vnum);
		}
		iterator_stop(&rit);
	}

    if(bp->progs) {
		ITERATOR it;
		PROG_LIST *trigger;
		for(int i = 0; i < TRIGSLOT_MAX; i++) if(list_size(bp->progs[i]) > 0) {
			iterator_start(&it, bp->progs[i]);
			while((trigger = (PROG_LIST *)iterator_nextdata(&it)))
				fprintf(fp, "InstanceProg %ld %s~ %s~\n", trigger->vnum, trigger_name(trigger->trig_type), trigger_phrase(trigger->trig_type,trigger->trig_phrase));
			iterator_stop(&it);
		}
	}

	if(bp->index_vars) {
		for(pVARIABLE var = bp->index_vars; var; var = var->next) {
			if(var->type == VAR_INTEGER)
				fprintf(fp, "VarInt %s~ %d %d\n", var->name, var->save, var->_.i);
			else if(var->type == VAR_STRING || var->type == VAR_STRING_S)
				fprintf(fp, "VarStr %s~ %d %s~\n", var->name, var->save, var->_.s ? var->_.s : "");
			else if(var->type == VAR_ROOM && var->_.r && var->_.r->vnum)
				fprintf(fp, "VarRoom %s~ %d %d\n", var->name, var->save, (int)var->_.r->vnum);

		}
	}

	fprintf(fp, "#-BLUEPRINT\n\n");
}

// save blueprints
bool save_blueprints()
{
	FILE *fp = fopen(BLUEPRINTS_FILE, "w");
	if (fp == NULL)
	{
		bug("Couldn't save blueprints.dat", 0);
		return false;
	}

	int iHash;

	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(BLUEPRINT_SECTION *bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
		{
			save_blueprint_section(fp, bs);
		}
	}

	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(BLUEPRINT *bp = blueprint_hash[iHash]; bp; bp = bp->next)
		{
			save_blueprint(fp, bp);
		}
	}

	for( SCRIPT_DATA *scr = iprog_list; scr; scr = scr->next)
	{
		save_script_new(fp,NULL,scr,"INSTANCE");
	}

	fprintf(fp, "#END\n");

	fclose(fp);

	blueprints_changed = false;
	return true;
}

bool valid_section_link(BLUEPRINT_LINK *bl)
{
	if( !IS_VALID(bl) ) return false;

	if( bl->vnum <= 0 ) return false;

	if( bl->door < 0 || bl->door >= MAX_DIR ) return false;

	if( !bl->room ) return false;

	if( !bl->ex ) return false;

	// Only environment exits can be used as links
	if( !IS_SET(bl->ex->exit_info, EX_ENVIRONMENT) ) return false;

	return true;
}

BLUEPRINT_LINK *get_section_link(BLUEPRINT_SECTION *bs, int link)
{
	if( !IS_VALID(bs) ) return NULL;

	if( link < 1 ) return NULL;

	for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
	{
		if( !--link )
			return bl;
	}

	return NULL;
}

bool valid_static_link(STATIC_BLUEPRINT_LINK *sbl)
{
	if( !IS_VALID(sbl) ) return false;
	if( !IS_VALID(sbl->blueprint) ) return false;

	BLUEPRINT_SECTION *section1 = (BLUEPRINT_SECTION *)list_nthdata(sbl->blueprint->sections, sbl->section1);
	if( !IS_VALID(section1) ) return false;

	BLUEPRINT_SECTION *section2 = (BLUEPRINT_SECTION *)list_nthdata(sbl->blueprint->sections, sbl->section2);
	if( !IS_VALID(section2) ) return false;

	BLUEPRINT_LINK *link1 = get_section_link(section1, sbl->link1);
	if( !valid_section_link(link1) ) return false;

	BLUEPRINT_LINK *link2 = get_section_link(section2, sbl->link2);
	if( !valid_section_link(link2) ) return false;

	// Only allow links that are reverse directions to link
	if( rev_dir[link1->door] != link2->door ) return false;

	return true;
}

BLUEPRINT_SECTION *get_blueprint_section(long vnum)
{
	int iHash = vnum % MAX_KEY_HASH;

	for(BLUEPRINT_SECTION *bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
	{
		if( bs->vnum == vnum )
			return bs;
	}

	return NULL;
}

BLUEPRINT_SECTION *get_blueprint_section_byroom(long vnum)
{
	for(int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(BLUEPRINT_SECTION *bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
		{
			if( vnum >= bs->lower_vnum && vnum <= bs->upper_vnum )
				return bs;
		}
	}

	return NULL;
}

BLUEPRINT *get_blueprint(long vnum)
{
	int iHash = vnum % MAX_KEY_HASH;

	for(BLUEPRINT *bp = blueprint_hash[iHash]; bp; bp = bp->next)
	{
		if( bp->vnum == vnum )
			return bp;
	}

	return NULL;
}

bool rooms_in_same_section(long vnum1, long vnum2)
{
	BLUEPRINT_SECTION *s1 = get_blueprint_section_byroom(vnum1);
	BLUEPRINT_SECTION *s2 = get_blueprint_section_byroom(vnum2);

	if( !s1 && !s2 ) return true;	// If neither are in a blueprint section, they are considered in the same section

	return s1 && s2 && (s1 == s2);
}

ROOM_INDEX_DATA *instance_section_get_room_byvnum(INSTANCE_SECTION *section, long vnum)
{
	if( !IS_VALID(section) ) return NULL;

	ROOM_INDEX_DATA *room;
	ITERATOR rit;
	iterator_start(&rit, section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		if( room->vnum == vnum )
			break;
	}
	iterator_stop(&rit);

	return room;

}

ROOM_INDEX_DATA *instance_section_get_room(INSTANCE_SECTION *section, ROOM_INDEX_DATA *source)
{
	if( !source ) return NULL;

	return instance_section_get_room_byvnum(section, source->vnum);
}

int instance_section_count_mob(INSTANCE_SECTION *section, MOB_INDEX_DATA *pMobIndex)
{
	if( !IS_VALID(section) ) return 0;

	int count = 0;
	ROOM_INDEX_DATA *room;
	ITERATOR rit;
	iterator_start(&rit, section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		for(CHAR_DATA *pMob = room->people; pMob; pMob = pMob->next_in_room)
		{
			if( IS_NPC(pMob) && pMob->pIndexData == pMobIndex && !IS_SET(pMob->act[0], ACT_ANIMATED) )
				++count;
		}
	}
	iterator_stop(&rit);

	return count;
}

int instance_count_mob(INSTANCE *instance, MOB_INDEX_DATA *pMobIndex)
{
	if( !IS_VALID(instance) ) return 0;

	int count = 0;
	ITERATOR it;
	INSTANCE_SECTION *section;
	iterator_start(&it, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&it)) )
	{
		count += instance_section_count_mob(section, pMobIndex);
	}
	iterator_stop(&it);

	return count;
}

// create instance



INSTANCE_SECTION *clone_blueprint_section(BLUEPRINT_SECTION *parent)
{
	ROOM_INDEX_DATA *room;

	INSTANCE_SECTION *section = new_instance_section();

	if( !section ) return NULL;

	section->section = parent;

	// Clone rooms
	for(long vnum = parent->lower_vnum; vnum <= parent->upper_vnum; vnum++)
	{
		ROOM_INDEX_DATA *source = get_room_index(vnum);

		if( source )
		{
			room = create_virtual_room_nouid(source,false,false,true);

			if( !room )
			{
				free_instance_section(section);
				return NULL;
			}

			get_vroom_id(room);

			if( !list_appendlink(section->rooms, room) )
			{
				extract_clone_room(room->source,room->id[0],room->id[1],true);
				free_instance_section(section);
				return NULL;
			}

			room->instance_section = section;
		}
	}


	ITERATOR rit;
	iterator_start(&rit, section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		// Clone the non-environment exits
		for(int i = 0; i < MAX_DIR; i++)
		{
			EXIT_DATA *exParent = room->source->exit[i];

			if( exParent )
			{
				EXIT_DATA *exClone;

				room->exit[i] = exClone = new_exit();
				exClone->u1.to_room = instance_section_get_room(section, exParent->u1.to_room);
				exClone->exit_info = exParent->exit_info;
				exClone->keyword = str_dup(exParent->keyword);
				exClone->short_desc = str_dup(exParent->short_desc);
				exClone->long_desc = str_dup(exParent->long_desc);
				exClone->rs_flags = exParent->rs_flags;
				exClone->orig_door = exParent->orig_door;
				exClone->door.strength = exParent->door.strength;
				exClone->door.material = str_dup(exParent->door.material);
				exClone->door.lock = exParent->door.rs_lock;
				exClone->door.rs_lock = exParent->door.rs_lock;
				exClone->from_room = room;
			}
		}

		// Correct any portal objects
		for(OBJ_DATA *obj = room->contents; obj; obj = obj->next_content)
		{
			// Make sure portals that lead anywhere within the section uses the correct room id
			if( obj->item_type == ITEM_PORTAL )
			{
				if( !IS_SET(obj->value[2], GATE_DUNGEON) )
				{
					ROOM_INDEX_DATA *dest;
					long vnum = obj->value[3];	// Destination vnum

					// Must point to a non-wilderness room
					if( vnum > 0 && obj->value[5] <= 0 )
					{
						if( (dest = instance_section_get_room_byvnum(section, vnum)) )
						{
							obj->value[6] = dest->id[0];
							obj->value[7] = dest->id[1];
						}
						else
						{
							dest = get_room_index(vnum);

							if( !dest ||
								IS_SET(dest->room_flag[1], ROOM_BLUEPRINT) ||
								IS_SET(dest->area->area_flags, AREA_BLUEPRINT) )
							{
								// Nullify destination
								obj->value[3] = 0;
							}

							// Force it to be static
							obj->value[6] = 0;
							obj->value[7] = 0;
						}
					}
				}
			}
		}
	}
	iterator_stop(&rit);

	return section;
}

INSTANCE_SECTION *instance_get_section(INSTANCE *instance, int section_no)
{
	if( section_no < 1 ) return NULL;

	return list_nthdata(instance->sections, section_no);
}

bool generate_static_instance(INSTANCE *instance)
{
	ITERATOR bsit;
	BLUEPRINT_SECTION *bs;
	BLUEPRINT *bp = instance->blueprint;

	bool valid = true;
	iterator_start(&bsit, bp->sections);
	while((bs = (BLUEPRINT_SECTION *)iterator_nextdata(&bsit)))
	{
		INSTANCE_SECTION *section = clone_blueprint_section(bs);

		if( !section )
		{
			valid = false;
			break;
		}

		section->blueprint = bp;
		section->instance = instance;

		list_appendlist(instance->rooms, section->rooms);

		list_appendlink(instance->sections, section);
	}
	iterator_stop(&bsit);

	if( valid )
	{
		// Connect all the sections together
		STATIC_BLUEPRINT_LINK *link;

		for(link = bp->static_layout; link; link = link->next)
		{
			INSTANCE_SECTION *section1 = instance_get_section(instance, link->section1);
			INSTANCE_SECTION *section2 = instance_get_section(instance, link->section2);

			if( section1 && section2 )
			{
				BLUEPRINT_LINK *link1 = get_section_link(section1->section, link->link1);
				BLUEPRINT_LINK *link2 = get_section_link(section2->section, link->link2);

				if( link1 && link2 )
				{
					ROOM_INDEX_DATA *room1 = instance_section_get_room_byvnum(section1, link1->vnum);
					ROOM_INDEX_DATA *room2 = instance_section_get_room_byvnum(section2, link2->vnum);

					if( room1 && room2 )
					{
						EXIT_DATA *ex1 = room1->exit[link1->door];
						EXIT_DATA *ex2 = room2->exit[link2->door];

						if( !ex1 )
						{
							ex1 = new_exit();
							ex1->from_room = room1;
							ex1->orig_door = link1->door;

							room1->exit[link1->door] = ex1;
						}

						if( !ex2 )
						{
							ex2 = new_exit();
							ex2->from_room = room2;
							ex2->orig_door = link2->door;

							room1->exit[link2->door] = ex2;
						}

						REMOVE_BIT(ex1->rs_flags, EX_ENVIRONMENT);
						ex1->exit_info = ex1->rs_flags;
						ex1->door.lock = ex1->door.rs_lock;
						ex1->u1.to_room = room2;

						REMOVE_BIT(ex2->rs_flags, EX_ENVIRONMENT);
						ex2->exit_info = ex2->rs_flags;
						ex2->door.lock = ex2->door.rs_lock;
						ex2->u1.to_room = room1;
					}
				}
			}
		}

		// Assign the entry exit (PREVFLOOR) if defined
		if( bp->static_entry_section > 0 && bp->static_entry_link > 0 )
		{
			INSTANCE_SECTION *section = instance_get_section(instance, bp->static_entry_section);
			if( section )
			{
				BLUEPRINT_LINK *bl = get_section_link(section->section, bp->static_entry_link);

				if( bl )
				{
					ROOM_INDEX_DATA *room = instance_section_get_room_byvnum(section, bl->vnum);
					if( room )
					{
						EXIT_DATA *ex = room->exit[bl->door];

						if( !ex )
						{
							ex = new_exit();
							ex->from_room = room;
							ex->orig_door = bl->door;
							room->exit[bl->door] = ex;
						}

						REMOVE_BIT(ex->rs_flags, EX_ENVIRONMENT);
						SET_BIT(ex->rs_flags, EX_PREVFLOOR);
						ex->exit_info = ex->rs_flags;
						ex->door.lock = ex->door.rs_lock;
						ex->u1.to_room = NULL;

						instance->entrance = room;
					}
				}
			}
		}

		// Assign the exit exit (NEXTFLOOR) if defined
		if( bp->static_exit_section > 0 && bp->static_exit_link > 0 )
		{
			INSTANCE_SECTION *section = instance_get_section(instance, bp->static_exit_section);
			if( section )
			{
				BLUEPRINT_LINK *bl = get_section_link(section->section, bp->static_exit_link);

				if( bl )
				{
					ROOM_INDEX_DATA *room = instance_section_get_room_byvnum(section, bl->vnum);
					if( room )
					{
						EXIT_DATA *ex = room->exit[bl->door];

						if( !ex )
						{
							ex = new_exit();
							ex->from_room = room;
							ex->orig_door = bl->door;
							room->exit[bl->door] = ex;
						}

						REMOVE_BIT(ex->rs_flags, EX_ENVIRONMENT);
						SET_BIT(ex->rs_flags, EX_NEXTFLOOR);
						ex->exit_info = ex->rs_flags;
						ex->door.lock = ex->door.rs_lock;
						ex->u1.to_room = NULL;


						instance->exit = room;
					}
				}
			}
		}

		// Assign the recall point based upon the recall section's recall, if defined
		if( bp->static_recall > 0 )
		{
			INSTANCE_SECTION *recall_section = instance_get_section(instance, bp->static_recall);

			if( recall_section )
			{
				instance->recall = instance_section_get_room_byvnum(recall_section, recall_section->section->recall);
			}
		}
	}

	return valid;
}

void instance_section_reset_rooms(INSTANCE_SECTION *section)
{
	ITERATOR rit;
	ROOM_INDEX_DATA *room;

	iterator_start(&rit, section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
	{
		reset_room(room, false);
	}

	iterator_stop(&rit);
}

void reset_instance(INSTANCE *instance)
{
	ITERATOR it;
	INSTANCE_SECTION *section;
	iterator_start(&it, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&it)) )
	{
		instance_section_reset_rooms(section);
	}
	iterator_stop(&it);
}


INSTANCE *create_instance(BLUEPRINT *blueprint)
{
	INSTANCE *instance = new_instance();

	if( instance )
	{
		instance->blueprint = blueprint;
		instance->flags = blueprint->flags;

		instance->progs			= new_prog_data();
		instance->progs->progs	= blueprint->progs;
		variable_copylist(&blueprint->index_vars,&instance->progs->vars,false);

		if( blueprint->mode == BLUEPRINT_MODE_STATIC )
		{
			if( !generate_static_instance(instance) )
			{
				free_instance(instance);
				return NULL;
			}
		}
		else
		{
			// Unsupported blueprint mode
			char buf[MSL];
			sprintf(buf, "create_instance - unsupported mode %d for blueprint %ld", blueprint->mode, blueprint->vnum);
			bug(buf, 0);

			free_instance(instance);
			return NULL;
		}

		ITERATOR it;
		INSTANCE_SECTION *section;
		BLUEPRINT_SPECIAL_ROOM *special;
		iterator_start(&it, blueprint->special_rooms);
		while( (special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&it)) )
		{
			section = (INSTANCE_SECTION *)list_nthdata(instance->sections, special->section);
			if( IS_VALID(section) )
			{
				ROOM_INDEX_DATA *room = instance_section_get_room_byvnum(section, special->vnum);

				if( room )
				{
					NAMED_SPECIAL_ROOM *isr = new_named_special_room();

					free_string(isr->name);
					isr->name = str_dup(special->name);
					isr->room = room;

					list_appendlink(instance->special_rooms, isr);
				}
			}

		}
		iterator_stop(&it);

		reset_instance(instance);
	}

	return instance;
}


void update_instance_section(INSTANCE_SECTION *section)
{
	ITERATOR rit;
	ROOM_INDEX_DATA *room;

	iterator_start(&rit, section->rooms);
	while((room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)))
		room_update(room);
	iterator_stop(&rit);
}

void update_instance(INSTANCE *instance)
{
	p_percent2_trigger(NULL, instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RANDOM, NULL);

	ITERATOR sit;
	INSTANCE_SECTION *section;
	iterator_start(&sit, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&sit)) )
	{
		update_instance_section(section);
	}
	iterator_stop(&sit);
}

bool instance_can_idle(INSTANCE *instance)
{
	return IS_SET(instance->flags, INSTANCE_DESTROY) ||
			(!IS_SET(instance->flags, INSTANCE_NO_IDLE) &&
				(!IS_SET(instance->flags, INSTANCE_IDLE_ON_COMPLETE) ||
				IS_SET(instance->flags, INSTANCE_COMPLETED)));
}

void instance_check_empty(INSTANCE *instance)
{
	if( instance->empty )
	{
		if( list_size(instance->players) > 0 )
			instance->empty = false;
	}
	else if( list_size(instance->players) < 1 )
	{
		instance->empty = true;
		if( instance_can_idle(instance) )
			instance->idle_timer = UMAX(15, instance->idle_timer);
	}

	if( !instance->empty && !instance_can_idle(instance) )
		instance->idle_timer = 0;
}


void instance_update()
{
	ITERATOR it;
	INSTANCE *instance;

	iterator_start(&it, loaded_instances);
	while((instance = (INSTANCE *)iterator_nextdata(&it)))
	{
		// Skip instances owned by dungeons
		if( IS_VALID(instance->dungeon) ) continue;

		if( IS_VALID(instance->ship) ) continue;

		if( instance_isorphaned(instance) && list_size(instance->players) < 1 )
		{
			// Do not keep an empty orphaned instance
			extract_instance(instance);
			continue;
		}

		update_instance(instance);

		if( instance_can_idle(instance) )
		{
			if( instance->idle_timer > 0 )
			{
				if( !--instance->idle_timer )
				{
					extract_instance(instance);
					continue;
				}
			}
		}

		instance_check_empty(instance);

		instance->age++;
		if( instance->blueprint->repop > 0 && (instance->age >= instance->blueprint->repop) )
		{
			p_percent2_trigger(NULL, instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_RESET, NULL);

			reset_instance(instance);

			instance->age = 0;
		}
	}
	iterator_stop(&it);
}

void extract_instance(INSTANCE *instance)
{
	ITERATOR it;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *room;
	ROOM_INDEX_DATA *environ = NULL;

    if(instance->progs) {
	    SET_BIT(instance->progs->entity_flags,PROG_NODESTRUCT);
	    if(instance->progs->script_ref > 0) {
			instance->progs->extract_when_done = true;
			return;
		}
    }

	if( IS_VALID(instance->object) )
		environ = obj_room(instance->object);

	else if( IS_VALID(instance->ship) )
		environ = obj_room(instance->ship->ship);

	if( !environ )
		environ = instance->environ;

	room = environ;
	if( !room )
		room = get_room_index(11001);

	// Dump all mobiles
	iterator_start(&it, instance->mobiles);
	while( (ch = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		char_from_room(ch);
		char_to_room(ch, room);
	}
	iterator_stop(&it);

	// Dump objects
	room = environ;
	if( !room )
		room = get_room_index(ROOM_VNUM_DONATION);

	iterator_start(&it, instance->objects);
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

	list_remlink(loaded_instances, instance, true);

	free_instance(instance);
}

void instance_apply_specialkeys(INSTANCE *instance, LLIST *special_keys)
{
	ITERATOR sit, rit;
	INSTANCE_SECTION *section;
	ROOM_INDEX_DATA *room;

	if( !IS_VALID(instance) || !IS_VALID(special_keys) ) return;

	iterator_start(&sit, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&sit)) )
	{
		iterator_start(&rit, section->rooms);
		while( (room = (ROOM_INDEX_DATA *)iterator_nextdata(&rit)) )
		{
			for( int i = 0; i < MAX_DIR; i++ )
			{
				EXIT_DATA *ex = room->exit[i];

				if( ex && ex->door.lock.key_vnum > 0 )
				{
					SPECIAL_KEY_DATA *sk = get_special_key(special_keys, ex->door.lock.key_vnum);

					if( sk )
					{
						ex->door.lock.keys = sk->list;
						ex->door.rs_lock.keys = sk->list;
					}
				}
			}
		}
		iterator_stop(&rit);
	}
	iterator_stop(&sit);
}

CHAR_DATA *instance_section_find_mobile(INSTANCE_SECTION *section, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	ROOM_INDEX_DATA *room;
	CHAR_DATA *mob = NULL, *ch;

	iterator_start(&it, section->rooms);
	while( !IS_VALID(mob) && (room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) )
	{
		for( ch = room->people; ch; ch = ch->next_in_room )
		{
			if( ch->id[0] == id1 && ch->id[1] == id2 )
			{
				mob = ch;
				break;
			}
		}
	}
	iterator_stop(&it);

	return mob;
}

CHAR_DATA *instance_find_mobile(INSTANCE *instance, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	INSTANCE_SECTION *section;
	CHAR_DATA *mob = NULL;

	iterator_start(&it, instance->sections);
	while( !IS_VALID(mob) && (section = (INSTANCE_SECTION *)iterator_nextdata(&it)) )
	{
		mob = instance_section_find_mobile(section, id1, id2);
	}
	iterator_stop(&it);

	return mob;
}

//////////////////////////////////////////////////////////////
//
// OLC Editors
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// Blueprint Section Edit
//

const struct olc_cmd_type bsedit_table[] =
{
	{ "?",				show_help			},
	{ "commands",		show_commands		},
	{ "list",			bsedit_list			},
	{ "show",			bsedit_show			},
	{ "create",			bsedit_create		},
	{ "name",			bsedit_name			},
	{ "description",	bsedit_description	},
	{ "comments",		bsedit_comments		},
	{ "type",			bsedit_type			},
	{ "flags",			bsedit_flags		},
	{ "recall",			bsedit_recall		},
	{ "rooms",			bsedit_rooms		},
	{ "link",			bsedit_link			},
	{ NULL,				NULL				}

};

bool can_edit_blueprints(CHAR_DATA *ch)
{
	return !IS_NPC(ch) && (ch->pcdata->security >= 9) && (ch->tot_level >= MAX_LEVEL);
}

void list_blueprint_sections(CHAR_DATA *ch, char *argument)
{
	if( !can_edit_blueprints(ch) )
	{
		send_to_char("You do not have access to blueprints.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many sections you can see.{x\n\r", ch);

	int lines = 0;
	bool error = false;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_blueprint_section_vnum; vnum++)
	{
		BLUEPRINT_SECTION *section = get_blueprint_section(vnum);

		if( section )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s  {G%-16.16s{x   %11ld   %11ld-%-11ld \n\r",
				vnum,
				section->name,
				flag_string(blueprint_section_types, section->type),
				section->recall,
				section->lower_vnum,
				section->upper_vnum);

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
		send_to_char("Too many blueprints to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No blueprint sections to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ] [      Type      ] [  Recall   ] [    Room Vnum Range    ]{x\n\r", ch);
			send_to_char("{Y==========================================================================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
	}
	free_buf(buffer);
}

void do_bslist(CHAR_DATA *ch, char *argument)
{
	list_blueprint_sections(ch, argument);
}

void do_bsedit(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT_SECTION *bs;
	long value;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;


	if (!can_edit_blueprints(ch))
	{
		send_to_char("BPEdit:  Insufficient security to edit blueprints.\n\r", ch);
		return;
	}

	if (is_number(arg1))
	{
		value = atol(arg1);
		if (!(bs = get_blueprint_section(value)))
		{
			send_to_char("BSEdit:  That vnum does not exist.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)bs;
		ch->desc->editor = ED_BPSECT;
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (bsedit_create(ch, argument))
			{
				blueprints_changed = true;
				ch->pcdata->immortal->last_olc_command = current_time;
				ch->desc->editor = ED_BPSECT;
			}

			return;
		}

	}

	send_to_char("Syntax: bsedit <vnum>\n\r"
				 "        bsedit create <vnum>\n\r", ch);
}


void bsedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_blueprints(ch))
	{
		send_to_char("BSEdit:  Insufficient security to edit blueprint sections.\n\r", ch);
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
		bsedit_show(ch, argument);
		return;
	}

	for (cmd = 0; bsedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, bsedit_table[cmd].name))
		{
			if ((*bsedit_table[cmd].olc_fun) (ch, argument))
			{
				blueprints_changed = true;
			}

			return;
		}
	}

    interpret(ch, arg);
}

BSEDIT( bsedit_list )
{
	list_blueprint_sections(ch, argument);
	return false;
}

BSEDIT( bsedit_show )
{
	BLUEPRINT_SECTION *bs;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_BPSECT(ch, bs);

	buffer = new_buf();

	sprintf(buf, "Name:        [%5ld] %s\n\r", bs->vnum, bs->name);
	add_buf(buffer, buf);

	sprintf(buf, "Type:        %s\n\r", flag_string(blueprint_section_types, bs->type));
	add_buf(buffer, buf);

	sprintf(buf, "Flags:       %s\n\r", flag_string(blueprint_section_flags, bs->flags));
	add_buf(buffer, buf);

	if( bs->recall > 0 )
	{
		ROOM_INDEX_DATA *recall_room = get_room_index(bs->recall);
		sprintf(buf, "Recall:      [%5ld] %s\n\r", bs->recall, recall_room ? recall_room->name : "-invalid-");
	}
	else
	{
		sprintf(buf, "Recall:      no recall defined\n\r");
	}
	add_buf(buffer, buf);

	sprintf(buf, "Lower Vnum:  %ld\n\r", bs->lower_vnum);
	add_buf(buffer, buf);

	sprintf(buf, "Upper Vnum:  %ld\n\r", bs->upper_vnum);
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, bs->description);
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, bs->comments);
	add_buf(buffer, "\n\r-----\n\r");

	if( bs->links )
	{
		int bli = 0;
		// List links
		add_buf(buffer, "{YSection Links:{x\n\r");
		for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
		{
			++bli;
			ROOM_INDEX_DATA *room = bl->room;

			char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
			char excolor = bl->ex ? 'W' : 'D';

			sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s {%c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, bl->name, excolor, door, bl->vnum, room ? room->name : "nowhere");
			add_buf(buffer, buf);
		}
	}

	page_to_char(buffer->string, ch);

	free_buf(buffer);
	return false;
}


void do_bsshow(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT_SECTION *bs;
	void *old_edit;
	long value;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  bsshow <vnum>\n\r", ch);
		return;
	}

	if (!is_number(argument))
	{
		send_to_char("Vnum must be a number.\n\r", ch);
		return;
	}

	value = atol(argument);
	if (!(bs = get_blueprint_section(value)))
	{
		send_to_char("That blueprint section does not exist.\n\r", ch);
		return;
	}

	old_edit = ch->desc->pEdit;
	ch->desc->pEdit = (void *) bs;

	bsedit_show(ch, argument);
	ch->desc->pEdit = old_edit;
	return;
}


BSEDIT( bsedit_create )
{
	BLUEPRINT_SECTION *bs;
	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_blueprint_section_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_blueprint_section_vnum; last_vnum++)
		{
			if( !get_blueprint_section(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}
	else if( get_blueprint_section(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return false;
	}

	bs = new_blueprint_section();
	bs->vnum = value;

	iHash							= bs->vnum % MAX_KEY_HASH;
	bs->next						= blueprint_section_hash[iHash];
	blueprint_section_hash[iHash]	= bs;
	ch->desc->pEdit					= (void *)bs;

	if( bs->vnum > top_blueprint_section_vnum)
		top_blueprint_section_vnum = bs->vnum;

    return true;
}

BSEDIT( bsedit_name )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(bs->name);
	bs->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return true;
}

BSEDIT( bsedit_description )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		string_append(ch, &bs->description);
		return true;
	}

	send_to_char("Syntax:  description - line edit\n\r", ch);
	return false;
}

BSEDIT( bsedit_comments )
{
	BLUEPRINT_SECTION *bs;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		string_append(ch, &bs->comments);
		return true;
	}

	send_to_char("Syntax:  comments - line edit\n\r", ch);
	return false;
}

BSEDIT( bsedit_type )
{
	BLUEPRINT_SECTION *bs;
	int value;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  type <type>\n\r", ch);
		send_to_char("'? section_types' for list of types.\n\r", ch);
		return false;
	}

	if( (value = flag_value(blueprint_section_types, argument)) == NO_FLAG )
	{
		send_to_char("That is not a valid type.\n\r", ch);
		send_to_char("'? section_types' for list of types.\n\r", ch);
		return false;
	}

	bs->type = value;
	send_to_char("Section type changed.\n\r", ch);
	return true;
}

BSEDIT( bsedit_flags )
{
	BLUEPRINT_SECTION *bs;
	int value;

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? section_flags' for list of flags.\n\r", ch);
		return false;
	}

	if( (value = flag_value(blueprint_section_flags, argument)) == NO_FLAG )
	{
		send_to_char("That is not a valid flag.\n\r", ch);
		send_to_char("'? section_flags' for list of flags.\n\r", ch);
		return false;
	}

	bs->flags ^= value;
	send_to_char("Section flags changed.\n\r", ch);
	return true;
}


BSEDIT( bsedit_recall )
{
	BLUEPRINT_SECTION *bs;
	ROOM_INDEX_DATA *room;
	long vnum;
	char buf[MSL];

	EDIT_BPSECT(ch, bs);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  recall [vnum]\n\r", ch);
		send_to_char("         recall none\n\r", ch);
		return false;
	}

	if( !str_cmp(argument, "none") )
	{
		if( bs->recall < 1 )
		{
			send_to_char("Recall was not defined.\n\r", ch);
			return false;
		}

		bs->recall = 0;

		send_to_char("Recall cleared.\n\r", ch);
		return true;
	}

	if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
	{
		send_to_char("Vnum range must be set first.\n\r", ch);
		return false;
	}

	if (!is_number(argument))
	{
		send_to_char("That is not a number.\n\r", ch);
		return false;
	}

	vnum = atol(argument);
	if( vnum <= 0 )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return false;
	}

	if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
	{
		sprintf(buf, "Value must be a value from %ld to %ld.\n\r", bs->lower_vnum, bs->upper_vnum);
		send_to_char(buf, ch);
		return false;
	}

	room = get_room_index(vnum);
	if( room == NULL )
	{
		send_to_char("That room does not exist.\n\r", ch);
		return false;
	}

	bs->recall = vnum;
	sprintf(buf, "Recall set to %.30s (%ld)\n\r", room->name, vnum);
	send_to_char(buf, ch);
	return true;
}

bool validate_vnum_range(CHAR_DATA *ch, BLUEPRINT_SECTION *section, long lower, long upper)
{
	char buf[MSL];

	// Check the range spans just one area.
	if( !check_range(lower, upper) )
	{
		send_to_char("Vnums must be in the same area.\n\r", ch);
		return false;
	}

	// Check that are no overlaps
	BLUEPRINT_SECTION *bs;
	int iHash;
	for(iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for(bs = blueprint_section_hash[iHash]; bs; bs = bs->next)
		{
			// Only check against other sections
			if( bs != section )
			{
				if( (lower >= bs->lower_vnum && lower <= bs->upper_vnum ) ||
					(upper >= bs->lower_vnum && upper <= bs->upper_vnum ) ||
					(bs->lower_vnum >= lower && bs->lower_vnum <= upper ) ||
					(bs->upper_vnum >= lower && bs->upper_vnum <= upper ) )
				{
					send_to_char("Blueprint section vnum ranges cannot overlap.\n\r", ch);
					return false;
				}
			}
		}
	}

	// Verify there are any rooms in the range
	bool found = false;
	bool valid = true;
	BUFFER *buffer = new_buf();		// This will buffer up ALL the problem rooms

	for(long vnum = lower; vnum <= upper; vnum++)
	{
		ROOM_INDEX_DATA *room = get_room_index(vnum);

		if( room )
		{
			found = true;

			if( !IS_SET(room->room_flag[1], ROOM_BLUEPRINT) &&
				!IS_SET(room->area->area_flags, AREA_BLUEPRINT) )
			{
				sprintf(buf, "{xRoom {W%ld{x is not allocated for use in blueprints.\n\r", room->vnum);
				add_buf(buffer, buf);
				valid = false;
			}

			if( IS_SET(room->room_flag[1], (ROOM_NOCLONE|ROOM_VIRTUAL_ROOM)) )
			{
				sprintf(buf, "{xRoom {W%ld{x cannot be used in blueprints.\n\r", room->vnum);
				add_buf(buffer, buf);
				valid = false;
			}

			// Verify the room does not have non-environment exits pointing OUT of the range of vnums
			for( int i = 0; i < MAX_DIR; i++ )
			{
				EXIT_DATA *ex = room->exit[i];

				if( (ex != NULL) &&
					!IS_SET(ex->exit_info, EX_ENVIRONMENT) &&
					(ex->u1.to_room != NULL) )
				{
					if( IS_SET(ex->exit_info, EX_VLINK) )
					{
						sprintf(buf, "{xRoom {W%ld{x has an exit ({W%s{x) leading to wilderness.\n\r", room->vnum, dir_name[i]);
						add_buf(buffer, buf);
						valid = false;
					}
					else if( ex->u1.to_room->vnum < lower || ex->u1.to_room->vnum > upper )
					{
						sprintf(buf, "{xRoom {W%ld{x has an exit ({W%s{x) leading outside of the vnum range.\n\r", room->vnum, dir_name[i]);
						add_buf(buffer, buf);
						valid = false;
					}
				}
			}

		}
	}

	if( !found )
	{
		send_to_char("There are no rooms in that range.\n\r", ch);
	}
	else if( !valid )
	{
		page_to_char(buffer->string, ch);
	}

	free_buf(buffer);
	return found && valid;
}

BSEDIT( bsedit_rooms )
{
	BLUEPRINT_SECTION *bs;
	long lvnum, uvnum;
	char buf[MSL];
	char arg[MIL];

	EDIT_BPSECT(ch, bs);

	argument = one_argument(argument, arg);

	if( arg[0] == '\0' || argument[0] == '\0' )
	{
		send_to_char("Syntax:  rooms [lower vnum][upper vnum]\n\r", ch);
		send_to_char("{YVnums must be in the same area.{x\n\r", ch);
		return false;
	}

	if( !is_number(arg) || !is_number(argument) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return false;
	}

	lvnum = atol(arg);
	uvnum = atol(argument);

	// Silently swap the bounds if necessary, don't be annoying
	if( uvnum < lvnum )
	{
		long vnum = lvnum;
		lvnum = uvnum;
		uvnum = vnum;
	}

	if( validate_vnum_range(ch, bs, lvnum, uvnum) )
	{
		bs->lower_vnum = lvnum;
		bs->upper_vnum = uvnum;

		send_to_char("Vnum range set.\n\r", ch);

		// Make sure recall point is still inside room range
		if( bs->recall > 0 )
		{
			if( bs->recall < lvnum || bs->recall > uvnum )
			{
				send_to_char("{YRecall room outside of new range.  Clearing.{x\n\r", ch);
				bs->recall = 0;
			}
		}

		// Make sure link are still inside room range
		if( bs->links )
		{
			BLUEPRINT_LINK *prev = NULL, *cur, *next;

			for(cur = bs->links; cur; cur = next)
			{
				next = cur->next;

				if( cur->vnum < lvnum || cur->vnum > uvnum )
				{
					sprintf(buf, "Link %.30s outside of new vnum range.  Removing.\n\r", cur->name);
					send_to_char(buf, ch);

					if( prev )
						prev->next = next;
					else
						bs->links = next;

					free_blueprint_link(cur);
				}
				else
				{
					prev = cur;
				}
			}

		}

		return true;
	}

	return false;
}

BSEDIT( bsedit_link )
{
	BLUEPRINT_SECTION *bs;
	BLUEPRINT_LINK *link;
	char arg[MIL];
	char arg2[MIL];

	EDIT_BPSECT(ch, bs);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  link list\n\r", ch);
		send_to_char("         link add <vnum> <door>\n\r", ch);
		send_to_char("         link # delete\n\r", ch);
		send_to_char("         link # name <name>\n\r", ch);
		send_to_char("         link # room <vnum>\n\r", ch);
		send_to_char("         link # exit <door>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if( !str_cmp(arg, "list" ) )
	{
		if( bs->links )
		{
			char buf[MSL];

			int bli = 0;
			// List links
			send_to_char("{YSection Links:{x\n\r", ch);
			for(BLUEPRINT_LINK *bl = bs->links; bl; bl = bl->next)
			{
				++bli;
				ROOM_INDEX_DATA *room = bl->room;

				char *door = (bl->door >= 0 && bl->door < MAX_DIR) ? dir_name[bl->door] : "none";
				char excolor = bl->ex ? 'W' : 'D';

				sprintf(buf, " {Y[{W%3d{Y] {G%-30.30s {%c%-9s{x in {Y[{W%5ld{Y]{x %s\n\r", bli, bl->name, excolor, door, bl->vnum, room ? room->name : "nowhere");
				send_to_char(buf, ch);
			}
		}
		else
		{
			send_to_char("No links defined.\n\r", ch);
		}

		return false;
	}

	if( !str_cmp(arg, "add") )
	{
		if( bs->lower_vnum < 1 || bs->upper_vnum < 1 )
		{
			send_to_char("Vnum range must be set first.\n\r", ch);
			return false;
		}

		if( !is_number(arg2) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		long vnum = atol(arg2);
		if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
		{
			send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
			return false;
		}

		ROOM_INDEX_DATA *room = get_room_index(vnum);
		if( !room )
		{
			send_to_char("That room does not exist.\n\r", ch);
			return false;
		}

		bool found = false;
		for( int i = 0; i < MAX_DIR; i++ )
		{
			if( room->exit[i] )
				found = true;
		}

		if( !found )
		{
			send_to_char("That room has no exits.\n\r,", ch);
			return false;
		}

		int door = parse_door(argument);
		if( door < 0 )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return false;
		}

		EXIT_DATA *ex = room->exit[door];
		if( !ex )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return false;
		}

		if( !IS_SET(ex->exit_info, EX_ENVIRONMENT) )
		{
			send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
			return false;
		}

		link = new_blueprint_link();
		link->vnum = vnum;
		link->door = door;
		link->room = room;
		link->ex = ex;

		// Append to the end
		link->next = NULL;
		if( bs->links )
		{
			BLUEPRINT_LINK *cur;

			for(cur = bs->links;cur->next; cur = cur->next)
			{
				;
			}

			cur->next = link;
		}
		else
		{
			bs->links = link;
		}

		send_to_char("Link added.\n\r", ch);
		return true;
	}

	if( !is_number(arg) )
	{
		send_to_char("That is not a number.\n\r", ch);
		return false;
	}

	int linkno = atoi(arg);
	if( linkno < 1 )
	{
		send_to_char("That is not a section link.\n\r", ch);
		return false;
	}


	if( !str_cmp(arg2, "delete") )
	{
		BLUEPRINT_LINK *prev = NULL;

		for( link = bs->links; link; link = link->next )
		{
			if(!--linkno)
				break;

			prev = link;
		}

		if(!link)
		{
			send_to_char("That is not a section link.\n\r", ch);
			return false;
		}

		if( prev )
			prev->next = link->next;
		else
			bs->links = link->next;

		free_blueprint_link(link);
		send_to_char("Link removed.\n\r", ch);
		return true;
	}

	link = get_section_link(bs, linkno);
	if( !link )
	{
		send_to_char("That is not a section link.\n\r", ch);
		return false;
	}

	if( !str_cmp(arg2, "name") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # name <name>\n\r", ch);
			return false;
		}

		free_string(link->name);
		link->name = str_dup(argument);

		send_to_char("Name changed.\n\r", ch);
		return true;
	}

	if( !str_cmp(arg2, "room") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # room <vnum>\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		long vnum = atol(argument);
		if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
		{
			send_to_char("Vnum is out of range of blueprint section.\n\r", ch);
			return false;
		}

		ROOM_INDEX_DATA *room = get_room_index(vnum);
		if( !room )
		{
			send_to_char("That room does not exist.\n\r", ch);
			return false;
		}

		bool found = false;
		for( int i = 0; i < MAX_DIR; i++ )
		{
			if( room->exit[i] )
				found = true;
		}

		if( !found )
		{
			send_to_char("That room has no exits.\n\r,", ch);
			return false;
		}

		link->vnum = vnum;
		link->door = -1;
		link->room = room;
		link->ex = NULL;


		send_to_char("Room changed.\n\r", ch);
		return true;
	}

	if( !str_cmp(arg2, "exit") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  link # exit <door>\n\r", ch);
			return false;
		}

		int door = parse_door(argument);
		if( door < 0 )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return false;
		}

		EXIT_DATA *ex = link->room->exit[door];
		if( !ex )
		{
			send_to_char("That is an invalid exit.\n\r", ch);
			return false;
		}

		if( !IS_SET(ex->exit_info, EX_ENVIRONMENT) )
		{
			send_to_char("Exit links must be {YENVIRONMENT{x exits.\n\r", ch);
			return false;
		}

		link->door = door;
		link->ex = ex;
		send_to_char("Exit changed.\n\r", ch);
		return true;
	}

	bsedit_link(ch, "");
	return false;
}


//////////////////////////////////////////////////////////////
//
// Blueprint Edit
//

const struct olc_cmd_type bpedit_table[] =
{
	{ "?",				show_help			},
	{ "addiprog",		bpedit_addiprog		},
	{ "areawho",		bpedit_areawho		},
	{ "commands",		show_commands		},
	{ "comments",		bpedit_comments		},
	{ "create",			bpedit_create		},
	{ "deliprog",		bpedit_deliprog		},
	{ "description",	bpedit_description	},
	{ "flags",			bpedit_flags		},
	{ "list",			bpedit_list			},
	{ "mode",			bpedit_mode			},
	{ "name",			bpedit_name			},
	{ "repop",			bpedit_repop		},
	{ "section",		bpedit_section		},
	{ "show",			bpedit_show			},
	{ "static",			bpedit_static		},
	{ "varclear",		bpedit_varclear		},
	{ "varset",			bpedit_varset		},
	{ NULL,				NULL				}
};

void list_blueprints(CHAR_DATA *ch, char *argument)
{
	static const char *blueprint_modes[] =
	{
		"{GStatic",
		"{YDynamic",
		"{RProcedural"
	};

	if( !can_edit_blueprints(ch) )
	{
		send_to_char("You do not have access to blueprints.\n\r", ch);
		return;
	}

	if(!ch->lines)
		send_to_char("{RWARNING:{W Having scrolling off may limit how many blueprints you can see.{x\n\r", ch);

	int lines = 0;
	bool error = false;
	BUFFER *buffer = new_buf();
	char buf[MSL];

	for(long vnum = 1; vnum <= top_blueprint_vnum; vnum++)
	{
		BLUEPRINT *blueprint= get_blueprint(vnum);

		if( blueprint )
		{
			sprintf(buf, "{Y[{W%5ld{Y] {x%-30.30s  %-16.16s{x\n\r",
				vnum,
				blueprint->name,
				blueprint_modes[URANGE(0,blueprint->mode,2)]);

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
		send_to_char("Too many blueprints to list.  Please shorten!\n\r", ch);
	}
	else
	{
		if( !lines )
		{
			add_buf( buffer, "No blueprint to display.\n\r" );
		}
		else
		{
			// Header
			send_to_char("{Y Vnum   [            Name            ] [      Mode      ]{x\n\r", ch);
			send_to_char("{Y=========================================================={x\n\r", ch);
		}

		page_to_char(buffer->string, ch);
	}
	free_buf(buffer);
}


void do_bplist(CHAR_DATA *ch, char *argument)
{
	list_blueprints(ch, argument);
}

void do_bpedit(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT *bp;
	long value;
	char arg1[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (!can_edit_blueprints(ch))
	{
		send_to_char("BPEdit:  Insufficient security to edit blueprints.\n\r", ch);
		return;
	}

	if (is_number(arg1))
	{
		value = atol(arg1);
		if (!(bp = get_blueprint(value)))
		{
			send_to_char("BPEdit:  That vnum does not exist.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		ch->desc->pEdit = (void *)bp;
		ch->desc->editor = ED_BLUEPRINT;
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (bpedit_create(ch, argument))
			{
				blueprints_changed = true;
				ch->pcdata->immortal->last_olc_command = current_time;
				ch->desc->editor = ED_BLUEPRINT;
			}

			return;
		}
	}

	send_to_char("Syntax: bpedit <vnum>\n\r"
				 "        bpedit create <vnum>\n\r", ch);
}


void bpedit(CHAR_DATA *ch, char *argument)
{
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!can_edit_blueprints(ch))
	{
		send_to_char("BPEdit:  Insufficient security to edit blueprints.\n\r", ch);
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
		bpedit_show(ch, argument);
		return;
	}

	for (cmd = 0; bpedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, bpedit_table[cmd].name))
		{
			if ((*bpedit_table[cmd].olc_fun) (ch, argument))
			{
				blueprints_changed = true;
			}

			return;
		}
	}

    interpret(ch, arg);
}

BPEDIT( bpedit_list )
{
	list_blueprints(ch, argument);
	return false;
}

BPEDIT( bpedit_show )
{
	BLUEPRINT *bp;
	BUFFER *buffer;
	char buf[MSL];

	EDIT_BLUEPRINT(ch, bp);

	buffer = new_buf();

	sprintf(buf, "{xName:        [%5ld] %s{x\n\r", bp->vnum, bp->name);
	add_buf(buffer, buf);

	if( bp->repop > 0)
		sprintf(buf, "Repop:       %d minutes\n\r", bp->repop);
	else
		sprintf(buf, "Repop:       {Dnever{x\n\r");
	add_buf(buffer, buf);

	sprintf(buf, "{xAreaWho:     [%s] [%s]{x\n\r", flag_string(area_who_titles, bp->area_who), flag_string(area_who_display, bp->area_who));
	add_buf(buffer, buf);

	sprintf(buf, "{xFlags:       %s{x\n\r", flag_string(instance_flags, bp->flags));
	add_buf(buffer, buf);

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, bp->description);
	add_buf(buffer, "\n\r");

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, bp->comments);
	add_buf(buffer, "\n\r-----\n\r");

	switch(bp->mode)
	{
	case BLUEPRINT_MODE_STATIC:
		add_buf(buffer, "{xMode:        [{WStatic{x]\n\r");
		break;

	default:
		add_buf(buffer, "{xMode:        [Unknown]\n\r");
		break;
	}

	if( list_size(bp->sections) > 0 )
	{
		int line = 0;
		BLUEPRINT_SECTION * bs;
		ITERATOR sit;

		add_buf(buffer, "{YSections:{x\n\r");
		add_buf(buffer, "     [  Vnum  ] [             Name             ]\n\r");
		add_buf(buffer, "------------------------------------------------\n\r");

		iterator_start(&sit, bp->sections);
		while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
		{
			sprintf(buf, "{W%4d  {G%8ld{x   %-30.30s{x\n\r", ++line, bs->vnum, bs->name);
			add_buf(buffer, buf);
		}

		iterator_stop(&sit);
		add_buf(buffer, "------------------------------------------------\n\r\n\r");
	}
	else
	{
		add_buf(buffer, "{YSections:{x\n\r   None\n\r\n\r");
	}

	add_buf(buffer, "Special Rooms:\n\r");
	if( list_size(bp->special_rooms) > 0 )
	{
		BLUEPRINT_SPECIAL_ROOM *special;

		char buf[MSL];
		int line = 0;

		ITERATOR sit;

		add_buf(buffer, "     [             Name             ] [             Room             ]\n\r");
		add_buf(buffer, "---------------------------------------------------------------------------------\n\r");

		iterator_start(&sit, bp->special_rooms);
		while( (special = (BLUEPRINT_SPECIAL_ROOM *)iterator_nextdata(&sit)) )
		{
			BLUEPRINT_SECTION *section = list_nthdata(bp->sections, special->section);
			ROOM_INDEX_DATA *room = get_room_index(special->vnum);

			if( !IS_VALID(section) || !room || room->vnum < section->lower_vnum || room->vnum > section->upper_vnum)
			{
				snprintf(buf, MSL-1, "{W%4d  %-30.30s   {D-{Winvalid{D-{x\n\r", ++line, special->name);
			}
			else
			{
				snprintf(buf, MSL-1, "{W%4d  %-30.30s   (%ld) {Y%s{x in (%ld) {Y%s{x\n\r", ++line, special->name, room->vnum, room->name, section->vnum, section->name);
			}
			add_buf(buffer, buf);
		}
		iterator_stop(&sit);
		add_buf(buffer, "---------------------------------------------------------------------------------\n\r");
	}
	else
	{
		add_buf(buffer, "   None\n\r");
	}
	add_buf(buffer, "\n\r");



	if( bp->mode == BLUEPRINT_MODE_STATIC )
	{
		if( bp->static_layout )
		{
			int linkno = 0;
			add_buf(buffer, "{CLinks:{x\n\r");

			add_buf(buffer, "     [ Section 1 ] [ Link 1 ] [ Section 2 ] [ Link 2 ]\n\r");
			add_buf(buffer, "-------------------------------------------------------\n\r");

			STATIC_BLUEPRINT_LINK *sbl;
			for(sbl = bp->static_layout; sbl; sbl = sbl->next)
			{
				sprintf(buf, "{W%4d   {G%9d     {Y%6d     {G%9d     {Y%6d{x\n\r",
					++linkno, sbl->section1, sbl->link1, sbl->section2, sbl->link2);
				add_buf(buffer, buf);
			}

			add_buf(buffer, "-------------------------------------------------------\n\r\n\r");
		}
		else
		{
			add_buf(buffer, "{CLinks:{x\n\r   None\n\r\n\r");
		}

		if( bp->static_recall > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_recall);

			if( bs )
			{
				sprintf(buf, "{xRecall:     %d [%ld] %-.30s\n\r", bp->static_recall, bs->vnum, bs->name);
			}
			else
			{
				sprintf(buf, "{xRecall:     %d [---] {Dinvalid{x\n\r", bp->static_recall);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xRecall:     None\n\r");
		}

		if( bp->static_entry_section > 0 && bp->static_entry_link > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_entry_section);

			if( bs )
			{
				BLUEPRINT_LINK *bl = get_section_link(bs, bp->static_entry_link);

				char section_name[31];

				strncpy(section_name, bs->name, 30);
				section_name[30] = '\0';

				if( bl && bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
				{
					sprintf(buf, "{xEntry:      %d [%ld] %s (%ld:%s)\n\r", bp->static_entry_section, bs->vnum, section_name, bl->vnum, dir_name[bl->door]);
				}
				else
				{
					sprintf(buf, "{xEntry:      %d [%ld] %s ({Dinvalid{x)\n\r", bp->static_entry_section, bs->vnum, section_name);
				}
			}
			else
			{
				sprintf(buf, "{xEntry:      %d [---] {Dinvalid{x\n\r", bp->static_entry_section);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xEntry:      None\n\r");
		}


		if( bp->static_exit_section > 0 && bp->static_exit_link > 0 )
		{
			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, bp->static_exit_section);

			if( bs )
			{
				BLUEPRINT_LINK *bl = get_section_link(bs, bp->static_exit_link);

				char section_name[31];

				strncpy(section_name, bs->name, 30);
				section_name[30] = '\0';

				if( bl && bl->vnum > 0 && bl->door >= 0 && bl->door < MAX_DIR )
				{
					sprintf(buf, "{xExit:       %d [%ld] %s (%ld:%s)\n\r", bp->static_exit_section, bs->vnum, section_name, bl->vnum, dir_name[bl->door]);
				}
				else
				{
					sprintf(buf, "{xExit:       %d [%ld] %s ({Dinvalid{x)\n\r", bp->static_exit_section, bs->vnum, section_name);
				}
			}
			else
			{
				sprintf(buf, "{xExit:       %d [---] {Dinvalid{x\n\r", bp->static_exit_section);
			}

			add_buf(buffer, buf);
		}
		else
		{
			add_buf(buffer, "{xExit:       None\n\r");
		}

	}

	if (bp->progs) {
		int cnt, slot;

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
			if(list_size(bp->progs[slot]) > 0) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "Prog Vnum", "Trigger", "Phrase");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
			add_buf(buffer, buf);

			ITERATOR it;
			PROG_LIST *trigger;
			for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
				iterator_start(&it, bp->progs[slot]);
				while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
					sprintf(buf, "{C[{W%4d{C]{x %-20ld %-10s %-6s\n\r", cnt,
						trigger->vnum,trigger_name(trigger->trig_type),
						trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, false, false));
					add_buf(buffer, buf);
					cnt++;
				}
				iterator_stop(&it);
			}
		}
	}

	if (bp->index_vars) {
		pVARIABLE var;
		int cnt;

		for (cnt = 0, var = bp->index_vars; var; var = var->next) ++cnt;

		if (cnt > 0) {
			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "Name", "Type", "Saved", "Value");
			add_buf(buffer, buf);

			sprintf(buf, "{R%-20s %-8s %-5s %-10s\n\r{x", "----", "----", "-----", "-----");
			add_buf(buffer, buf);

			for (var = bp->index_vars; var; var = var->next) {
				switch(var->type) {
				case VAR_INTEGER:
					sprintf(buf, "{x%-20.20s {GNUMBER     {Y%c   {W%d{x\n\r", var->name,var->save?'Y':'N',var->_.i);
					break;
				case VAR_STRING:
				case VAR_STRING_S:
					sprintf(buf, "{x%-20.20s {GSTRING     {Y%c   {W%s{x\n\r", var->name,var->save?'Y':'N',var->_.s?var->_.s:"(empty)");
					break;
				case VAR_ROOM:
					if(var->_.r && var->_.r->vnum > 0)
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W%s {R({W%d{R){x\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->vnum);
					else
						sprintf(buf, "{x%-20.20s {GROOM       {Y%c   {W-no-where-{x\n\r",var->name,var->save?'Y':'N');
					break;
				default:
					continue;
				}
				add_buf(buffer, buf);
			}
		}
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
	return false;
}

void do_bpshow(CHAR_DATA *ch, char *argument)
{
	BLUEPRINT *bp;
	void *old_edit;
	long value;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  bpshow <vnum>\n\r", ch);
		return;
	}

	if (!is_number(argument))
	{
		send_to_char("Vnum must be a number.\n\r", ch);
		return;
	}

	value = atol(argument);
	if (!(bp = get_blueprint(value)))
	{
		send_to_char("That blueprint does not exist.\n\r", ch);
		return;
	}

	old_edit = ch->desc->pEdit;
	ch->desc->pEdit = (void *) bp;

	bpedit_show(ch, argument);
	ch->desc->pEdit = old_edit;
	return;
}


BPEDIT( bpedit_create )
{
	BLUEPRINT *bp;

	long  value;
	int  iHash;

	value = atol(argument);
	if (argument[0] == '\0' || value == 0)
	{
		long last_vnum = 0;
		value = top_blueprint_vnum + 1;
		for(last_vnum = 1; last_vnum <= top_blueprint_vnum; last_vnum++)
		{
			if( !get_blueprint(last_vnum) )
			{
				value = last_vnum;
				break;
			}
		}
	}
	else if( get_blueprint(value) )
	{
		send_to_char("That vnum already exists.\n\r", ch);
		return false;
	}

	bp = new_blueprint();
	bp->vnum = value;

	iHash							= bp->vnum % MAX_KEY_HASH;
	bp->next						= blueprint_hash[iHash];
	blueprint_hash[iHash]			= bp;
	ch->desc->pEdit					= (void *)bp;

	if( bp->vnum > top_blueprint_vnum)
		top_blueprint_vnum = bp->vnum;

    return true;

}


BPEDIT( bpedit_name )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name [string]\n\r", ch);
		return false;
	}

	free_string(bp->name);
	bp->name = str_dup(argument);
	send_to_char("Name changed.\n\r", ch);
	return true;
}

BPEDIT( bpedit_repop )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if( !is_number(argument) )
	{
		send_to_char("Syntax:  repop [age]\n\r", ch);
		return false;
	}

	int repop = atoi(argument);
	bp->repop = UMAX(0, repop);
	send_to_char("Repop changed.\n\r", ch);
	return true;
}

BPEDIT( bpedit_flags )
{
	BLUEPRINT *bp;
	int value;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  flags <flags>\n\r", ch);
		send_to_char("'? instance' for list of flags.\n\r", ch);
		return false;
	}

	if( (value = flag_value(instance_flags, argument)) != NO_FLAG )
	{
		bp->flags ^= value;
		send_to_char("Instance flags changed.\n\r", ch);
		return true;
	}

	bpedit_flags(ch, "");
	return false;

}


BPEDIT( bpedit_description )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		string_append(ch, &bp->description);
		return true;
	}

	send_to_char("Syntax:  description\n\r", ch);
	return false;
}

BPEDIT( bpedit_comments )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		string_append(ch, &bp->comments);
		return true;
	}

	send_to_char("Syntax:  comments\n\r", ch);
	return false;
}

BPEDIT( bpedit_areawho )
{
	BLUEPRINT *bp;
	int value;

	EDIT_BLUEPRINT(ch, bp);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  areawho <value>\n\r", ch);
		send_to_char("See '? areawho' for list\n\r", ch);
		return false;
	}

	if ( !str_prefix(argument, "blank") )
	{
	    bp->area_who = AREA_BLANK;

	    send_to_char("Area who title cleared.\n\r", ch);
	    return true;
	}


	if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
	{
		bp->area_who = value;

		send_to_char("Area who title set.\n\r", ch);
		return true;
	}

	bpedit_areawho(ch, "");
	return false;
}

BPEDIT( bpedit_mode )
{
	BLUEPRINT *bp;

	EDIT_BLUEPRINT(ch, bp);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  mode static|procedural\n\r", ch);
		return false;
	}

	if( !str_prefix(argument, "static") )
	{
		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			send_to_char("Blueprint is already in STATIC mode.\n\r", ch);
			return false;
		}


		bp->mode = BLUEPRINT_MODE_STATIC;
		// Remove non-static data

		// Initialize static data
		bp->static_layout = NULL;
		bp->static_recall = -1;
		bp->static_entry_section = -1;
		bp->static_entry_link = -1;
		bp->static_exit_section = -1;
		bp->static_exit_link = -1;

		send_to_char("Blueprint changed to STATIC mode.\n\r", ch);
		return true;
	}

	if( !str_prefix(argument, "procedural") )
	{
		send_to_char("Procedural mode is not implemented yet.\n\r", ch);
		return false;
	}

	bpedit_mode(ch, "");
	return false;
}

BPEDIT( bpedit_section )
{
	BLUEPRINT *bp;
	BLUEPRINT_SECTION *bs;
	char arg[MIL];

	EDIT_BLUEPRINT(ch, bp);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  section add <vnum>\n\r", ch);
		send_to_char("         section delete <#>\n\r", ch);
		send_to_char("         section list\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "add") )
	{
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		bs = get_blueprint_section(atol(argument));
		if( !bs )
		{
			send_to_char("That blueprint section does not exist.\n\r", ch);
			return false;
		}

		if( !list_appendlink(bp->sections, bs) )
		{
			send_to_char("{WError adding blueprint section to blueprint.{x\n\r", ch);
			return false;
		}


		send_to_char("Blueprint section added.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "delete") )
	{
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int index = atoi(argument);

		if( index < 1 || index > list_size(bp->sections) )
		{
			send_to_char("Index out of range.\n\r", ch);
			return false;
		}

		list_remnthlink(bp->sections, index, true);

		send_to_char("Blueprint section removed.\n\r", ch);
		if( bp->mode == BLUEPRINT_MODE_STATIC )
		{
			// Remove any invalid layout definitions since the section has been removed
			STATIC_BLUEPRINT_LINK *prev, *cur, *next;

			prev = NULL;
			for(cur = bp->static_layout; cur; cur = next)
			{
				next = cur->next;

				// Link references deleted section
				if( cur->section1 == index || cur->section2 == index )
				{
					if( !prev )
						bp->static_layout = next;
					else
						prev->next = next;
					free_static_blueprint_link(cur);
					continue;
				}

				// If link references a section AFTER the specified index, shift down by one
				if( cur->section1 > index )
					cur->section1--;

				if( cur->section2 > index )
					cur->section2--;

				prev = cur;
			}

			// Check RECALL
			if( bp->static_recall == index )
				bp->static_recall = -1;
			else if( bp->static_recall > index )
				bp->static_recall--;

			// Check ENTRY
			if( bp->static_entry_section == index )
			{
				bp->static_entry_section = -1;
				bp->static_entry_link = -1;
			}
			else if( bp->static_entry_section > index )
				bp->static_entry_section--;

			// Check EXIT
			if( bp->static_exit_section == index )
			{
				bp->static_exit_section = -1;
				bp->static_exit_link = -1;
			}
			else if( bp->static_exit_section > index )
				bp->static_exit_section--;
		}

		return true;
	}

	if( !str_prefix(arg, "list") )
	{
		if( list_size(bp->sections) > 0 )
		{
			BUFFER *buffer = new_buf();

			char buf[MSL];
			int line = 0;

			ITERATOR sit;

			add_buf(buffer, "     [  Vnum  ] [             Name             ]\n\r");
			add_buf(buffer, "------------------------------------------------\n\r");

			iterator_start(&sit, bp->sections);
			while( (bs = (BLUEPRINT_SECTION *)iterator_nextdata(&sit)) )
			{
				sprintf(buf, "{W%4d  {G%8ld{x   %-30.30s{x\n\r", ++line, bs->vnum, bs->name);
				add_buf(buffer, buf);
			}

			iterator_stop(&sit);
			add_buf(buffer, "------------------------------------------------\n\r");

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
			send_to_char("Blueprint has no blueprint sections assigned.\n\r", ch);
		}

		return false;
	}

	bpedit_section(ch, "");
	return false;
}

BPEDIT( bpedit_static )
{
	BLUEPRINT *bp;
	char arg[MIL];

	EDIT_BLUEPRINT(ch, bp);

	if( bp->mode != BLUEPRINT_MODE_STATIC )
	{
		send_to_char("Blueprint is not in STATIC mode.\n\r", ch);
		return false;
	}

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  static link add <section1#> <link1#> <section2#> <link2#>\n\r", ch);
		send_to_char("         static link remove #\n\r", ch);
		send_to_char("         static recall <section#>\n\r", ch);
		send_to_char("         static recall clear\n\r", ch);
		send_to_char("         static entry <section#> <link#>\n\r", ch);
		send_to_char("         static entry clear\n\r", ch);
		send_to_char("         static exit <section#> <link#>\n\r", ch);
		send_to_char("         static exit clear\n\r", ch);
		send_to_char("         static special add <section#> <room vnum> <name>\n\r", ch);
		send_to_char("         static special # remove\n\r", ch);
		send_to_char("         static special # name <name>\n\r", ch);
		send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "special") )
	{
		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];

		argument = one_argument(argument, arg2);

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
			send_to_char("         static special # remove\n\r", ch);
			send_to_char("         static special # name <name>\n\r", ch);
			send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
			return false;
		}

		if( is_number(arg2) )
		{
			int index = atoi(arg2);

			BLUEPRINT_SPECIAL_ROOM *special = list_nthdata(bp->special_rooms, index);

			if( !IS_VALID(special) )
			{
				send_to_char("No such special room.\n\r", ch);
				return false;
			}

			if( argument[0] == '\0' )
			{
				send_to_char("Syntax:  static special # remove\n\r", ch);
				send_to_char("         static special # name <name>\n\r", ch);
				send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
				return false;
			}

			if( !str_prefix(argument, "remove") || !str_prefix(argument, "delete") )
			{
				list_remnthlink(bp->special_rooms, index, true);
				send_to_char("Special Room removed.\n\r", ch);
				return true;
			}


			argument = one_argument(argument, arg3);

			if( !str_prefix(arg3, "name") )
			{
				if( argument[0] == '\0' )
				{
					send_to_char("Syntax:  static special # name <name>\n\r", ch);
					return false;
				}

				free_string(special->name);
				special->name = str_dup(argument);

				send_to_char("Name changed.\n\r", ch);
				return true;
			}

			argument = one_argument(argument, arg4);

			if( !str_prefix(arg3, "room") )
			{
				if( !is_number(arg4) || !is_number(argument) )
				{
					send_to_char("That is not a number.\n\r", ch);
					return false;
				}

				int section = atoi(arg4);
				long vnum = atol(argument);

				if( section < 1 || section > list_size(bp->sections) )
				{
					send_to_char("Section number out of range.\n\r", ch);
					return false;
				}

				BLUEPRINT_SECTION *bs = list_nthdata(bp->sections, section);

				if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
				{
					send_to_char("Room vnum not in the section.\n\r", ch);
					return false;
				}

				if( !get_room_index(vnum) )
				{
					send_to_char("Room does not exist.\n\r", ch);
					return false;
				}

				special->section = section;
				special->vnum = vnum;

				send_to_char("Special room changed.\n\r", ch);
				return true;
			}

			send_to_char("Syntax:  static special # remove\n\r", ch);
			send_to_char("         static special # name <name>\n\r", ch);
			send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
			return false;
		}

		if( !str_prefix(arg2, "add") )
		{
			if( argument[0] == '\0' )
			{
				send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
				return false;
			}

			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);


			if( !is_number(arg3) || !is_number(arg4) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int section = atoi(arg3);
			long vnum = atol(arg4);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section number out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = list_nthdata(bp->sections, section);

			if( vnum < bs->lower_vnum || vnum > bs->upper_vnum )
			{
				send_to_char("Room vnum not in the section.\n\r", ch);
				return false;
			}

			if( !get_room_index(vnum) )
			{
				send_to_char("Room does not exist.\n\r", ch);
				return false;
			}

			char name[MIL+1];
			strncpy(name, argument, MIL);
			name[MIL] = '\0';

			smash_tilde(name);

			BLUEPRINT_SPECIAL_ROOM *special = new_blueprint_special_room();
			free_string(special->name);
			special->name = str_dup(name);
			special->section = section;
			special->vnum = vnum;

			list_appendlink(bp->special_rooms, special);

			send_to_char("Special Room added.\n\r", ch);
			return true;
		}

		send_to_char("Syntax:  static special add <section#> <room vnum> <name>\n\r", ch);
		send_to_char("         static special # remove\n\r", ch);
		send_to_char("         static special # name <name>\n\r", ch);
		send_to_char("         static special # room <section#> <room vnum>\n\r", ch);
		return false;
	}

	if( !str_prefix(arg, "link") )
	{
		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];
		char arg5[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static link add <section1#> <link1#> <section2#> <link2#>\n\r", ch);
			send_to_char("         static link remove #\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( !str_prefix(arg2, "add") )
		{
			BLUEPRINT_SECTION *bs;

			argument = one_argument(argument, arg3);
			argument = one_argument(argument, arg4);
			argument = one_argument(argument, arg5);

			if( !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || !is_number(argument) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int section1 = atoi(arg3);
			int link1 = atoi(arg4);
			int section2 = atoi(arg5);
			int link2 = atoi(argument);

			if( section1 < 1 || section1 > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section1);
			if( !get_section_link(bs, link1) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}

			if( section2 < 1 || section2 > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section2);
			if( !get_section_link(bs, link2) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}

			STATIC_BLUEPRINT_LINK *sbl = new_static_blueprint_link();

			sbl->blueprint = bp;
			sbl->section1 = section1;
			sbl->link1 = link1;
			sbl->section2 = section2;
			sbl->link2 = link2;

			sbl->next = bp->static_layout;
			bp->static_layout = sbl;

			send_to_char("Static link added.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "remove") )
		{
			STATIC_BLUEPRINT_LINK *prev, *cur;

			if( !is_number(arg2) )
			{
				send_to_char("That is not a number.\n\r", ch);
				return false;
			}

			int index = atoi(arg);
			if( index < 1 )
			{
				send_to_char("Link does not exist.\n\r", ch);
				return false;
			}

			prev = NULL;
			for(cur = bp->static_layout; cur && index > 0; prev = cur, cur = cur->next)
			{
				if( !--index )
				{
					if( prev )
						prev->next = cur->next;
					else
						bp->static_layout = cur->next;

					free_static_blueprint_link(cur);
					send_to_char("Link removed.\n\r", ch);
					return true;
				}
			}


			send_to_char("Link does not exist.\n\r", ch);
			return false;
		}

		bpedit_static(ch, "link");
		return false;
	}

	if( !str_prefix(arg, "recall") )
	{
		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static recall <section#>\n\r", ch);
			send_to_char("         static recall clear\n\r", ch);
			return false;
		}

		if( is_number(argument) )
		{
			int index = atoi(argument);
			if( index < 1 || index > list_size(bp->sections) )
			{
				send_to_char("Index out of range.\n\r", ch);
				return false;
			}

			bp->static_recall = index;
			send_to_char("Blueprint recall section changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(argument, "clear") )
		{
			bp->static_recall = -1;
			send_to_char("Blueprint recall section cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "recall");
		return false;
	}

	if( !str_prefix(arg, "entry") )
	{
		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static entry <section#> <link#>\n\r", ch);
			send_to_char("         static entry clear\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( is_number(arg2) && is_number(argument) )
		{
			int section = atoi(arg2);
			int link = atoi(argument);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section index out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section);

			if( !get_section_link(bs, link) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}


			bp->static_entry_section = section;
			bp->static_entry_link = link;
			send_to_char("Blueprint entry point changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "clear") )
		{
			bp->static_entry_section = -1;
			bp->static_entry_link = -1;
			send_to_char("Blueprint entry point cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "entry");
		return false;
	}

	if( !str_prefix(arg, "exit") )
	{
		char arg2[MIL];

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  static exit <section#> <link#>\n\r", ch);
			send_to_char("         static exit clear\n\r", ch);
			return false;
		}

		argument = one_argument(argument, arg2);

		if( is_number(arg2) && is_number(argument) )
		{
			int section = atoi(arg2);
			int link = atoi(argument);

			if( section < 1 || section > list_size(bp->sections) )
			{
				send_to_char("Section index out of range.\n\r", ch);
				return false;
			}

			BLUEPRINT_SECTION *bs = (BLUEPRINT_SECTION *)list_nthdata(bp->sections, section);

			if( !get_section_link(bs, link) )
			{
				send_to_char("Link index out of range.\n\r", ch);
				return false;
			}


			bp->static_exit_section = section;
			bp->static_exit_link = link;
			send_to_char("Blueprint exit point changed.\n\r", ch);
			return true;
		}

		if( !str_prefix(arg2, "clear") )
		{
			bp->static_exit_section = -1;
			bp->static_exit_link = -1;
			send_to_char("Blueprint exit point cleared.\n\r", ch);
			return true;
		}

		bpedit_static(ch, "exit");
		return false;
	}


	bpedit_static(ch, "");
	return false;
}


BPEDIT (bpedit_addiprog)
{
    int tindex, slot;
	BLUEPRINT *blueprint;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_BLUEPRINT(ch, blueprint);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addiprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_IPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "iprog");
	return false;
    }

    slot = trigger_table[tindex].slot;

    if ((code = get_script_index (atol(num), PRG_IPROG)) == NULL)
    {
	send_to_char("No such INSTANCEProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!blueprint->progs) blueprint->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;

    list_appendlink(blueprint->progs[slot], list);

    send_to_char("Iprog Added.\n\r",ch);
    return true;
}

BPEDIT (bpedit_deliprog)
{
    BLUEPRINT *blueprint;
    char iprog[MAX_STRING_LENGTH];
    int value;

    EDIT_BLUEPRINT(ch, blueprint);

    one_argument(argument, iprog);
    if (!is_number(iprog) || iprog[0] == '\0')
    {
       send_to_char("Syntax:  deliprog [#iprog]\n\r",ch);
       return false;
    }

    value = atol (iprog);

    if (value < 0)
    {
        send_to_char("Only non-negative iprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(blueprint->progs,value)) {
	send_to_char("No such iprog.\n\r",ch);
	return false;
    }

    send_to_char("Iprog removed.\n\r", ch);
    return true;
}

BPEDIT(bpedit_varset)
{
    BLUEPRINT *blueprint;

	EDIT_BLUEPRINT(ch, blueprint);

	return olc_varset(&blueprint->index_vars, ch, argument, false);
}

BPEDIT(bpedit_varclear)
{
    BLUEPRINT *blueprint;

	EDIT_BLUEPRINT(ch, blueprint);

	return olc_varclear(&blueprint->index_vars, ch, argument, false);
}


//////////////////////////////////////////////////////////////
//
// Immortal Commands
//


void do_instance(CHAR_DATA *ch, char *argument)
{
	char arg1[MIL];

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  instance list\n\r", ch);
		send_to_char("         instance unload\n\r", ch);
		return;
	}

	argument = one_argument(argument, arg1);

	if( !str_prefix(arg1, "list") )
	{
		if(!ch->lines)
			send_to_char("{RWARNING:{W Having scrolling off may limit how many instances you can see.{x\n\r", ch);

		int lines = 0;
		bool error = false;
		BUFFER *buffer = new_buf();
		char buf[MSL];


		ITERATOR it;
		INSTANCE *instance;

		iterator_start(&it, loaded_instances);
		while((instance = (INSTANCE *)iterator_nextdata(&it)))
		{
			++lines;

			char *owner = instance_get_ownership(instance);

			char color = 'G';

			if( IS_SET(instance->flags, INSTANCE_DESTROY) )
				color = 'R';
			else if( IS_SET(instance->flags, INSTANCE_COMPLETED) )
				color = 'W';

			sprintf(buf, "%4d {Y[{W%5ld{Y] {%c%-30.30s{x  %s{x\n\r",
				lines,
				instance->blueprint->vnum,
				color,
				instance->blueprint->name,
				owner);

			if( !add_buf(buffer, buf) || (!ch->lines && strlen(buf_string(buffer)) > MAX_STRING_LENGTH) )
			{
				error = true;
				break;
			}
		}
		iterator_stop(&it);

		if( error )
		{
			send_to_char("Too many instances to list.  Please shorten!\n\r", ch);
		}
		else
		{
			if( !lines )
			{
				add_buf( buffer, "No instances to display.\n\r" );
			}
			else
			{
				// Header
				send_to_char("{Y      Vnum   [            Name            ] [      Owner     ]{x\n\r", ch);
				send_to_char("{Y==============================================================={x\n\r", ch);
			}

			page_to_char(buffer->string, ch);
		}
		free_buf(buffer);

		return;
	}


	/*
	if( !str_prefix(arg1, "load") )
	{
		if( !can_edit_blueprints(ch) )
		{
			send_to_char("Insufficient access to load blueprints.\n\r", ch);
			return;
		}

		if( list_size(loaded_instances) > 0 )
		{
			send_to_char("TEMPORARY: There is already an instance loaded.\n\r", ch);
			return;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		BLUEPRINT *bp = get_blueprint(atol(argument));

		if( !bp )
		{
			send_to_char("That blueprint does not exist.\n\r", ch);
			return;
		}

		INSTANCE *instance = create_instance(bp);

		if( !instance )
		{
			send_to_char("{WERROR SPAWNING INSTANCE!{x\n\r", ch);
			return;
		}

		list_appendlink(loaded_instances, instance);


		// Get the entry point
		ROOM_INDEX_DATA *room = instance->entrance;

		if( !room )
		{
			// Fallback to the recall if the entry is not defined
			room = instance->recall;
		}

		if( !room )
		{
			send_to_char("{WERROR GETTING ENTRY ROOM IN INSTANCE!{x\n\r", ch);
			return;
		}

		char_from_room(ch);
		char_to_room(ch, room);
		do_function (ch, &do_look, "");

		send_to_char("{YInstance loaded.{x\n\r", ch);
		return;
	}
	*/

	if( !str_prefix(arg1, "unload") )
	{
		if( !can_edit_blueprints(ch) )
		{
			send_to_char("Insufficient access to unload blueprints.\n\r", ch);
			return;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return;
		}

		int index = atoi(argument);

		if( list_size(loaded_instances) < index )
		{
			send_to_char("There is no instance loaded.\n\r", ch);
			return;
		}


		INSTANCE *instance = (INSTANCE *)list_nthdata(loaded_instances, index);

		if( !instance_isorphaned(instance) )
		{
			send_to_char("Instance is not orphaned.\n\r", ch);
			return;
		}

		if( list_size(instance->players) > 0 )
		{
			char buf[MSL];
			if( IS_SET(instance->flags, INSTANCE_DESTROY) )
			{
				send_to_char("Instance is already flagged for unloading.\n\r", ch);
				return;
			}

			SET_BIT(instance->flags, INSTANCE_DESTROY);
			if( instance->idle_timer > 0 )
				instance->idle_timer = UMIN(INSTANCE_DESTROY_TIMEOUT, instance->idle_timer);
			else
				instance->idle_timer = INSTANCE_DESTROY_TIMEOUT;

			sprintf(buf, "{RWARNING: Instance is being forcibly unloaded.  You have %d minutes to escape before the end!{x\n\r", instance->idle_timer);
			instance_echo(instance, buf);

			send_to_char("Instance flagged for unloading.\n\r", ch);
		}
		else
		{
			extract_instance(instance);
			send_to_char("Instance unloaded.\n\r", ch);
		}

		/*
		list_remlink(loaded_instances, instance, true);

		if( ch->in_room->instance_section->instance == instance )
		{
			// Take them out of the instance
			ROOM_INDEX_DATA *plith_recall = get_room_index(11001);

			char_from_room(ch);
			char_to_room(ch, plith_recall);
		}

		free_instance(instance);

		send_to_char("Instance unloaded.\n\r", ch);
		*/
		return;
	}

	do_instance(ch, "");
	return;
}

////////////////////////////////////////////////////////
//
// Instance Save/Load
//

void instance_section_save(FILE *fp, INSTANCE_SECTION *section)
{
	fprintf(fp, "#SECTION %ld\n\r", section->section->vnum);

	ITERATOR it;
	ROOM_INDEX_DATA *room;
	iterator_start(&it, section->rooms);
	while( (room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) )
	{
		persist_save_room(fp, room);
	}

	iterator_stop(&it);

	fprintf(fp, "#-SECTION\n\r");
}

void instance_save_roominfo(FILE *fp, char *field, ROOM_INDEX_DATA *room)
{
	if( room )
	{
		fprintf(fp, "%s %ld %lu %lu\n\r", field, room->vnum, room->id[0], room->id[1]);
	}
}

void instance_save(FILE *fp, INSTANCE *instance)
{
	fprintf(fp, "#INSTANCE %ld\n\r", instance->blueprint->vnum);

	fprintf(fp, "Floor %d\n\r", instance->floor);
	fprintf(fp, "Flags %d\n\r", instance->flags);

	if( instance->object_uid[0] > 0 || instance->object_uid[1] > 0 )
	{
		fprintf(fp, "Object %lu %lu\n\r", instance->object_uid[0], instance->object_uid[1]);
	}

	ITERATOR it;
	LLIST_UID_DATA *luid;
	iterator_start(&it, instance->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		fprintf(fp, "Player %lu %lu\n\r", luid->id[0], luid->id[1]);
	}
	iterator_stop(&it);

	INSTANCE_SECTION *section;
	iterator_start(&it, instance->sections);
	while( (section = (INSTANCE_SECTION *)iterator_nextdata(&it)) )
	{
		instance_section_save(fp, section);
	}
	iterator_stop(&it);

	instance_save_roominfo(fp, "Recall", instance->recall);
	instance_save_roominfo(fp, "Entrance", instance->entrance);
	instance_save_roominfo(fp, "Exit", instance->exit);

	fprintf(fp, "#-INSTANCE\n\r");
}

void instance_section_tallyentities(INSTANCE_SECTION *section)
{
	ITERATOR it;
	ROOM_INDEX_DATA *room;

	iterator_start(&it, section->rooms);
	while( (room = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) )
	{
		for(OBJ_DATA *obj = room->contents; obj; obj = obj->next_content)
		{
			if(!IS_SET(obj->extra[2], ITEM_INSTANCE_OBJ))
			{
				list_appendlink(section->instance->objects, obj);
			}
		}

		for(CHAR_DATA *ch = room->people; ch; ch = ch->next_in_room)
		{
			if( IS_NPC(ch) )
			{
				if( !IS_SET(ch->act[1], ACT2_INSTANCE_MOB) )
					list_appendlink(section->instance->mobiles, ch);
				else if ( IS_BOSS(ch) )
					list_appendlink(section->instance->bosses, ch);
			}
		}

		list_appendlink(section->instance->rooms, room);
	}
	iterator_stop(&it);
}

INSTANCE_SECTION *instance_section_load(FILE *fp)
{
	char *word;
	bool fMatch;
	bool fError = false;

	INSTANCE_SECTION *section = new_instance_section();
	long vnum = fread_number(fp);

	section->section = get_blueprint_section(vnum);

	while (str_cmp((word = fread_word(fp)), "#-SECTION"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#CROOM") )
			{
				ROOM_INDEX_DATA *room = persist_load_room(fp, 'C');
				if(room)
				{
					room->instance_section = section;

					variable_dynamic_fix_clone_room(room);
					list_appendlink(section->rooms, room);
				}
				else
					fError = true;

				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "instance_section_load: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	if( fError )
	{
		free_instance_section(section);
		return NULL;
	}

	return section;
}

INSTANCE *instance_load(FILE *fp)
{
	char *word;
	bool fMatch;
	bool fError = false;

	INSTANCE *instance = new_instance();
	long vnum = fread_number(fp);

	instance->blueprint = get_blueprint(vnum);

	instance->progs			= new_prog_data();
	instance->progs->progs	= instance->blueprint->progs;
	variable_copylist(&instance->blueprint->index_vars,&instance->progs->vars,false);

	while (str_cmp((word = fread_word(fp)), "#-INSTANCE"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if( !str_cmp(word, "#SECTION") )
			{
				INSTANCE_SECTION *section = instance_section_load(fp);

				if( section )
				{
					section->instance = instance;
					section->blueprint = instance->blueprint;

					list_appendlink(instance->sections, section);

					instance_section_tallyentities(section);
				}
				else
					fError = true;

				fMatch = true;
				break;
			}
			break;

		case 'E':
			if( !str_cmp(word, "Entrance") )
			{
				long room_vnum = fread_number(fp);
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				//log_string("get_clone_room: instance->entrance");
				instance->entrance = get_clone_room(get_room_index(room_vnum), id1, id2);

				fMatch = true;
				break;
			}

			if( !str_cmp(word, "Exit") )
			{
				long room_vnum = fread_number(fp);
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				//log_string("get_clone_room: instance->exit");
				instance->exit = get_clone_room(get_room_index(room_vnum), id1, id2);

				fMatch = true;
				break;
			}

			break;

		case 'F':
			KEY("Flags", instance->flags, fread_number(fp));
			KEY("Floor", instance->floor, fread_number(fp));
			break;

		case 'O':
			if( !str_cmp(word, "Object") )
			{
				instance->object_uid[0] = fread_number(fp);
				instance->object_uid[1] = fread_number(fp);

				fMatch = true;
				break;
			}
			break;

		case 'P':
			if( !str_cmp(word, "Player") )
			{
				unsigned long uid[2];
				uid[0] = fread_number(fp);
				uid[1] = fread_number(fp);

				instance_addowner_playerid(instance, uid[0], uid[1]);

				fMatch = true;
				break;
			}
			break;

		case 'R':
			if( !str_cmp(word, "Recall") )
			{
				long room_vnum = fread_number(fp);
				unsigned long id1 = fread_number(fp);
				unsigned long id2 = fread_number(fp);

				//log_string("get_clone_room: instance->recall");
				instance->recall = get_clone_room(get_room_index(room_vnum), id1, id2);

				fMatch = true;
				break;
			}
			break;
		}

		if (!fMatch) {
			char buf[MSL];
			sprintf(buf, "instance_load: no match for word %.50s", word);
			bug(buf, 0);
		}
	}

	if( fError )
	{
		free_instance(instance);
		return NULL;
	}

	if( instance->object_uid[0] > 0 && instance->object_uid[1] > 0 )
	{
		OBJ_DATA *obj = idfind_object(instance->object_uid[0], instance->object_uid[1]);

		if( IS_VALID(obj) )
			instance->object = obj;
	}

	return instance;
}



void resolve_instances()
{
	ITERATOR it;
	INSTANCE *instance;

	iterator_start(&it, loaded_instances);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		if( instance->object_uid[0] > 0 && instance->object_uid[1] > 0 )
		{
			OBJ_DATA *obj = idfind_object(instance->object_uid[0], instance->object_uid[1]);

			if( IS_VALID(obj) )
				instance->object = obj;
		}
	}
	iterator_stop(&it);
}

void resolve_instance_player(INSTANCE *instance, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, instance->player_owners);
	while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&it)) )
	{
		if( luid->id[0] == ch->id[0] &&
			luid->id[1] == ch->id[1])
		{
			luid->ptr = ch;
			continue;
		}
	}
	iterator_stop(&it);
}

void resolve_instances_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	INSTANCE *instance;

	iterator_start(&it, loaded_instances);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		resolve_instance_player(instance, ch);

		// Iterate over the character's quest list
	}
	iterator_stop(&it);
}


void detach_instances_player(CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	INSTANCE *instance;

	iterator_start(&it, loaded_instances);
	while( (instance = (INSTANCE *)iterator_nextdata(&it)) )
	{
		instance_removeowner_player(instance, ch);

		// Iterate over the character's quest list
	}
	iterator_stop(&it);
}

void instance_echo(INSTANCE *instance, char *text)
{
	if( !IS_VALID(instance) || IS_NULLSTR(text) ) return;

	ITERATOR it;
	CHAR_DATA *ch;

	iterator_start(&it, instance->players);
	while( (ch = (CHAR_DATA *)iterator_nextdata(&it)) )
	{
		send_to_char(text, ch);
		send_to_char("\n\r", ch);
	}
	iterator_stop(&it);
}

ROOM_INDEX_DATA *section_random_room(CHAR_DATA *ch, INSTANCE_SECTION *section)
{
	if( !IS_VALID(section) ) return NULL;

	return get_random_room_list_byflags( ch, section->rooms,
		(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CPK),
		ROOM_NO_GET_RANDOM );
}

ROOM_INDEX_DATA *instance_random_room(CHAR_DATA *ch, INSTANCE *instance)
{
	if( !IS_VALID(instance) ) return NULL;

	return get_random_room_list_byflags( ch, instance->rooms,
		(ROOM_PRIVATE | ROOM_SOLITARY | ROOM_DEATH_TRAP | ROOM_CPK),
		ROOM_NO_GET_RANDOM );
}

ROOM_INDEX_DATA *get_instance_special_room(INSTANCE *instance, int index)
{
	if( !IS_VALID(instance) || index < 1) return NULL;

	NAMED_SPECIAL_ROOM *special = list_nthdata(instance->special_rooms, index);

	if( IS_VALID(special) )
		return special->room;

	return NULL;
}

ROOM_INDEX_DATA *get_instance_special_room_byname(INSTANCE *instance, char *name)
{
	int number;
	char arg[MSL];

	if( !IS_VALID(instance) ) return NULL;

	number = number_argument(name, arg);

	if( number < 1 ) return NULL;

	ITERATOR it;
	ROOM_INDEX_DATA *room = NULL;
	NAMED_SPECIAL_ROOM *special;
	iterator_start(&it, instance->special_rooms);
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


void instance_addowner_player(INSTANCE *instance, CHAR_DATA *ch)
{
	// Don't add twice
	if( instance_isowner_player(instance, ch) ) return;

	LLIST_UID_DATA *luid = new_list_uid_data();
	luid->id[0] = ch->id[0];
	luid->id[1] = ch->id[1];
	luid->ptr = ch;

	list_appendlink(instance->player_owners, luid);
}

void instance_addowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2)
{
	// Don't add twice
	if( instance_isowner_playerid(instance, id1, id2) ) return;

	LLIST_UID_DATA *luid = new_list_uid_data();
	luid->id[0] = id1;
	luid->id[1] = id2;
	luid->ptr = NULL;

	list_appendlink(instance->player_owners, luid);
}

void instance_removeowner_player(INSTANCE *instance, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return;

	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, instance->player_owners);
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

void instance_removeowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;

	iterator_start(&it, instance->player_owners);
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

bool instance_isowner_player(INSTANCE *instance, CHAR_DATA *ch)
{
	if( IS_NPC(ch) ) return false;

	ITERATOR it;
	LLIST_UID_DATA *luid;
	bool ret = false;

	iterator_start(&it, instance->player_owners);
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

bool instance_isowner_playerid(INSTANCE *instance, unsigned long id1, unsigned long id2)
{
	ITERATOR it;
	LLIST_UID_DATA *luid;
	bool ret = false;

	iterator_start(&it, instance->player_owners);
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

bool instance_canswitch_player(INSTANCE *instance, CHAR_DATA *ch)
{
	// TODO: Add lockout system
	return true;
}

bool instance_isorphaned(INSTANCE *instance)
{
	if( !IS_VALID(instance) ) return true;

	if( IS_VALID(instance->dungeon) ) return false;

	if( IS_VALID(instance->ship) ) return false;

	// Does it have player owners?
	if( list_size(instance->player_owners) > 0 ) return false;

	// Does it have an object owner?
	if( IS_VALID(instance->object) ||
		instance->object_uid[0] > 0 ||
		instance->object_uid[1] > 0)
		return false;

	// Does it have a quest owner?
//  if( IS_VALID(instance->quest) ) return false;

	// Add other ownership

	return true;
}

char *instance_get_ownership(INSTANCE *instance)
{
	static char buf[4][MSL+1];
	static int idx = 0;

	if (++idx > 3)
		idx = 0;

	char *p = buf[idx];
	if( !IS_VALID(instance) )
	{
		strncpy(p, "{R    ! {WERROR {R!   {x", MSL);
	}
	else if( IS_VALID(instance->dungeon) )
	{
		strncpy(p, "{R     D{rU{RN{rG{RE{rO{RN    {x", MSL);
	}
	else if( IS_VALID(instance->ship) )
	{
		strncpy(p, "{C      S{cH{CI{cP      {x", MSL);
	}
	else if( IS_VALID(instance->object) || instance->object_uid[0] > 0 || instance->object_uid[1] > 0)
	{
		strncpy(p, "{Y     OBJECT     {x", MSL);
	}
//	else if( IS_VALID(instance->quest) )
//	{
//		strncpy(p, "{C      QUEST     {x", MSL);
//	}
	else if( list_size(instance->player_owners) > 0 )
	{
		strncpy(p, "{G     PLAYER     {x", MSL);
	}
	else
	{
		strncpy(p, "{D   - {WORPHAN{D -   {x", MSL);
	}


	p[MSL] = '\0';
	return p;
}


INSTANCE *get_room_instance(ROOM_INDEX_DATA *room)
{
	if( !room ) return NULL;

	if( !IS_VALID(room->instance_section) ) return NULL;

	if( !IS_VALID(room->instance_section->instance) ) return NULL;

	return room->instance_section->instance;
}
