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
*       ROM 2.4 is copyright 1993-1995 Russ Taylor                         *
*       ROM has been brought to you by the ROM consortium                  *
*           Russ Taylor (rtaylor@pacinfo.com)                              *
*           Gabrielle Taylor (gtaylor@pacinfo.com)                         *
*           Brian Moore (rom@rom.efn.org)                                  *
*       By using this code, you have agreed to follow the terms of the     *
*       ROM license, in the file Rom24/doc/rom.license                     *
***************************************************************************/

/***************************************************************************
*  Automated Quest code written by Vassago of MOONGATE, moongate.ams.com   *
*  4000. Copyright (c) 1996 Ryan Addams, All Rights Reserved. Use of this  *
*  code is allowed provided you add a credit line to the effect of:        *
*  "Quest Code (c) 1996 Ryan Addams" to your logon screen with the rest    *
*  of the standard diku/rom credits. If you use this or a modified version *
*  of this code, let me know via email: moongate@moongate.ams.com. Further *
*  updates will be posted to the rom mailing list. If you'd like to get    *
*  the latest version of quest.c, please send a request to the above add-  *
*  ress. Quest Code v2.00.                                                 *
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "olc.h"

void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

extern const int16_t movement_loss[SECT_MAX];

SECTOR_DATA *get_sector_data(char *name)
{
	ITERATOR it;
	SECTOR_DATA *sector;
	iterator_start(&it, sectors_list);
	while((sector = (SECTOR_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(name, sector->name))
			break;
	}
	iterator_stop(&it);

	return sector;
}

SECTOR_DATA **gsct_from_name(const char *name)
{
	for(int i = 0; global_sector_table[i].name; i++)
	{
		if (!str_prefix(name, global_sector_table[i].name))
			return global_sector_table[i].gsct;
	}

	return NULL;
}

char *gsct_to_name(SECTOR_DATA **gsct)
{
	for(int i = 0; global_sector_table[i].name; i++)
	{
		if (global_sector_table[i].gsct == gsct)
			return global_sector_table[i].name;
	}

	return NULL;
}

char *gsct_to_display(SECTOR_DATA **gsct)
{
	for(int i = 0; global_sector_table[i].name; i++)
	{
		if (global_sector_table[i].gsct == gsct)
			return global_sector_table[i].name;
	}

	return "{D(unset){x";
}

void save_sector(FILE *fp, SECTOR_DATA *sector)
{
	fprintf(fp, "#SECTOR %s~\n", sector->name);
	fprintf(fp, "Class %s~\n", flag_string(sector_classes, sector->sector_class));
	if (sector->gsct)
		fprintf(fp, "GSCT %s~\n", gsct_to_name(sector->gsct));
	fprintf(fp, "Description %s~\n", fix_string(sector->description));
	fprintf(fp, "Comments %s~\n", fix_string(sector->comments));
	fprintf(fp, "Flags %s\n", print_flags(sector->flags));
	fprintf(fp, "MoveCost %d\n", sector->move_cost);
	fprintf(fp, "HPRegen %d\n", sector->hp_regen);
	fprintf(fp, "ManaRegen %d\n", sector->mana_regen);
	fprintf(fp, "MoveRegen %d\n", sector->move_regen);
	fprintf(fp, "Soil %d\n", sector->soil);
	for(int i = 0; i < SECTOR_MAX_AFFINITIES; i++)
	{
		if (sector->affinities[i][0] > CATALYST_NONE && sector->affinities[i][0] < CATALYST_MAX)
		{
			fprintf(fp, "Affinity %s~ %d\n", flag_string(catalyst_types, sector->affinities[i][0]), sector->affinities[i][1]);
		}
	}
	fprintf(fp, "#-SECTOR\n");
}

void save_sectors()
{
	FILE *fp;

	log_string("save_sectors: saving " SECTORS_FILE);
	if ((fp = fopen(SECTORS_FILE, "w")) == NULL)
	{
		bug("save_sectors: fopen", 0);
		perror(SECTORS_FILE);
	}
	else
	{
		ITERATOR it;
		SECTOR_DATA *sector;
		iterator_start(&it, sectors_list);
		while((sector = (SECTOR_DATA *)iterator_nextdata(&it)))
		{
			log_string(formatf("Saving sector '%s'", sector->name));
			save_sector(fp, sector);
		}
		iterator_stop(&it);

		fprintf(fp, "#END\n");
		fclose(fp);
	}
}

void insert_sector(SECTOR_DATA *sector)
{
	ITERATOR it;
	SECTOR_DATA *s;
	iterator_start(&it, sectors_list);
	while((s = (SECTOR_DATA *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(sector->name, s->name);
		if (cmp < 0)
		{
			iterator_insert_before(&it, sector);
			break;
		}
	}
	iterator_stop(&it);

	if (!s)
		list_appendlink(sectors_list, sector);
}

SECTOR_DATA *load_sector(FILE *fp)
{
	SECTOR_DATA *sector;
	char *word;
	bool fMatch;

	sector = new_sector_data();
	sector->name = fread_string(fp);

	while(str_cmp((word = fread_word(fp)), "#-SECTOR"))
	{
		fMatch = false;

		switch(word[0])
		{
			case 'A':
				if (!str_cmp(word, "Affinity"))
				{
					int catalyst = stat_lookup(fread_string(fp), catalyst_types, CATALYST_NONE);
					int value = fread_number(fp);

					if (catalyst > CATALYST_NONE && catalyst < CATALYST_MAX)
					{
						int i;
						for(i = 0; i < SECTOR_MAX_AFFINITIES; i++)
						{
							if (sector->affinities[i][0] == CATALYST_NONE)
								break;
						}

						if (i < SECTOR_MAX_AFFINITIES)
						{
							sector->affinities[i][0] = catalyst;
							sector->affinities[i][1] = value;
						}
					}

					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEY("Class", sector->sector_class, stat_lookup(fread_string(fp), sector_classes, SECTCLASS_NONE));
				KEYS("Comments", sector->comments, fread_string(fp));
				break;

			case 'D':
				KEYS("Description", sector->description, fread_string(fp));
				break;

			case 'F':
				KEY("Flags", sector->flags, fread_flag(fp));
				break;

			case 'G':
				KEY("GSCT", sector->gsct, gsct_from_name(fread_string(fp)));
				break;

			case 'H':
				KEY("HPRegen", sector->hp_regen, fread_number(fp));
				break;

			case 'M':
				KEY("ManaRegen", sector->mana_regen, fread_number(fp));
				KEY("MoveCost", sector->move_cost, fread_number(fp));
				KEY("MoveRegen", sector->move_regen, fread_number(fp));
				break;

			case 'S':
				KEY("Soil", sector->soil, fread_number(fp));
				break;
		}

		if (!fMatch)
		{
			bug(formatf("load_sector: encountered unknown word '%s'\n\r", word), 0);
			fread_to_eol(fp);
		}
	}

	return sector;
}

static void delete_sector(void *ptr)
{
	free_sector_data((SECTOR_DATA *)ptr);
}

bool load_sectors()
{
	FILE *fp;
	SECTOR_DATA *sector;

	sectors_list = list_createx(false,NULL,delete_sector);
	if (!IS_VALID(sectors_list))
	{
		log_string("load_sectors: failed to create sectors_list.");
		return false;
	}

	log_string("load_sectors: loading " SECTORS_FILE);
	if ((fp = fopen(SECTORS_FILE, "r")) == NULL)
	{
		log_string("load_sectors: bootstrapping " SECTORS_FILE);

		for(int i = 0; sector_types[i].name; i++)
		{
			int type = sector_types[i].bit;

			if (type >= 0 && type < SECT_MAX)
			{
				log_string(formatf("Bootstrapping sector '%s'", sector_types[i].name));

				sector = new_sector_data();
				sector->name = str_dup(sector_types[i].name);

				int16_t clazz = SECTCLASS_NONE;
				long flags = 0;
				int16_t soil = 0;
				switch(type)
				{
					case SECT_INSIDE:
						clazz = SECTCLASS_CITY;
						list_appendlink(sector->hide_msgs, str_dup("in the corner"));
						list_appendlink(sector->hide_msgs, str_dup("amidst the shadows"));
						list_appendlink(sector->hide_msgs, str_dup("beneath some forgotten trash"));
						list_appendlink(sector->hide_msgs, str_dup("in a poorly lit area"));
						list_appendlink(sector->hide_msgs, str_dup("from view"));
						sector->gsct = &gsct_inside;
						SET_BIT(flags, SECTOR_CITY_LIGHTS);
						SET_BIT(flags, SECTOR_INDOORS);
						soil = -20;
						break;

					case SECT_CITY:
						clazz = SECTCLASS_CITY;
						list_appendlink(sector->hide_msgs, str_dup("in the corner"));
						list_appendlink(sector->hide_msgs, str_dup("amidst the shadows"));
						list_appendlink(sector->hide_msgs, str_dup("beneath some forgotten trash"));
						list_appendlink(sector->hide_msgs, str_dup("in a poorly lit area"));
						list_appendlink(sector->hide_msgs, str_dup("from view"));
						sector->gsct = &gsct_city;
						SET_BIT(flags, SECTOR_CITY_LIGHTS);
						soil = -10;
						break;

					case SECT_FIELD:
						clazz = SECTCLASS_PLAINS;
						SET_BIT(flags, SECTOR_NATURE);
						list_appendlink(sector->hide_msgs, str_dup("among the grasses"));
						list_appendlink(sector->hide_msgs, str_dup("in a bed of flowers"));
						list_appendlink(sector->hide_msgs, str_dup("under a pile of stones"));
						list_appendlink(sector->hide_msgs, str_dup("in a small hole"));
						list_appendlink(sector->hide_msgs, str_dup("from sight"));
						soil = 5;
						break;

					case SECT_FOREST:
						clazz = SECTCLASS_FOREST;
						SET_BIT(flags, SECTOR_NATURE);
						list_appendlink(sector->hide_msgs, str_dup("inside a tree"));
						list_appendlink(sector->hide_msgs, str_dup("under a stump"));
						list_appendlink(sector->hide_msgs, str_dup("in the thick vegetation"));
						list_appendlink(sector->hide_msgs, str_dup("in the branches of a tree"));
						list_appendlink(sector->hide_msgs, str_dup("from sight"));
						break;

					case SECT_HILLS:
						clazz = SECTCLASS_HILLS;
						SET_BIT(flags, SECTOR_NATURE);
						list_appendlink(sector->hide_msgs, str_dup("under a large rock"));
						list_appendlink(sector->hide_msgs, str_dup("from sight"));
						break;

					case SECT_MOUNTAIN:
						clazz = SECTCLASS_MOUNTAINS;
						SET_BIT(flags, SECTOR_NATURE);
						list_appendlink(sector->hide_msgs, str_dup("in the deep mountain crags"));
						soil = -10;
						break;

					case SECT_WATER_SWIM:
						clazz = SECTCLASS_WATER;
						list_appendlink(sector->hide_msgs, str_dup("in the sands beneath your feet"));
						sector->gsct = &gsct_water_swim;
						SET_BIT(flags, SECTOR_NO_FADE);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_WATER_NOSWIM:
						clazz = SECTCLASS_WATER;
						sector->gsct = &gsct_water_noswim;
						SET_BIT(flags, SECTOR_DEEP_WATER);
						SET_BIT(flags, SECTOR_NO_FADE);
						SET_BIT(flags, SECTOR_NO_GATE);
						SET_BIT(flags, SECTOR_NO_GOHALL);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_TUNDRA:
						clazz = SECTCLASS_SUBARCTIC;
						list_appendlink(sector->hide_msgs, str_dup("beneath a large pile of snow"));
						SET_BIT(flags, SECTOR_FROZEN);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_AIR:
						clazz = SECTCLASS_AIR;
						SET_BIT(flags, SECTOR_AERIAL);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_DESERT:
						clazz = SECTCLASS_DESERT;
						list_appendlink(sector->hide_msgs, str_dup("under a pile of desert sand"));
						soil = 10;
						break;

					case SECT_NETHERWORLD:
						clazz = SECTCLASS_NETHER;
						list_appendlink(sector->hide_msgs, str_dup("beneath a pile of bones"));
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_DOCK:
						clazz = SECTCLASS_CITY;
						list_appendlink(sector->hide_msgs, str_dup("under a couple of planks"));
						soil = -10;
						break;

					case SECT_ENCHANTED_FOREST:
						clazz = SECTCLASS_FOREST;
						SET_BIT(flags, SECTOR_CRUMBLES);
						SET_BIT(flags, SECTOR_NATURE);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_SLEEP_DRAIN);
						break;

					case SECT_TOXIC_BOG:
						clazz = SECTCLASS_SWAMP;
						SET_BIT(flags, SECTOR_NATURE);
						SET_BIT(flags, SECTOR_NO_SOIL);
						SET_BIT(flags, SECTOR_TOXIC);
						break;

					case SECT_CURSED_SANCTUM:
						clazz = SECTCLASS_DUNGEON;
						SET_BIT(flags, SECTOR_DRAIN_MANA);
						SET_BIT(flags, SECTOR_HARD_MAGIC);
						SET_BIT(flags, SECTOR_NO_GOHALL);
						SET_BIT(flags, SECTOR_SLOW_MAGIC);
						soil = -20;
						break;

					case SECT_BRAMBLE:
						clazz = SECTCLASS_FOREST;
						SET_BIT(flags, SECTOR_BRIARS);
						SET_BIT(flags, SECTOR_NATURE);
						break;

					case SECT_SWAMP:
						clazz = SECTCLASS_SWAMP;
						SET_BIT(flags, SECTOR_NATURE);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_ACID:
						clazz = SECTCLASS_HAZARDOUS;
						SET_BIT(flags, SECTOR_MELTS);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_LAVA:
						clazz = SECTCLASS_VULCAN;
						SET_BIT(flags, SECTOR_FLAME);
						SET_BIT(flags, SECTOR_MELTS);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_SNOW:
						clazz = SECTCLASS_SUBARCTIC;
						list_appendlink(sector->hide_msgs, str_dup("beneath a large pile of snow"));
						SET_BIT(flags, SECTOR_FROZEN);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_ICE:
						clazz = SECTCLASS_SUBARCTIC;
						list_appendlink(sector->hide_msgs, str_dup("beneath a large pile of snow"));
						SET_BIT(flags, SECTOR_FROZEN);
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_CAVE:
						clazz = SECTCLASS_UNDERGROUND;
						list_appendlink(sector->hide_msgs, str_dup("beneath a large pile of rocks"));
						list_appendlink(sector->hide_msgs, str_dup("behind a stalagmite"));
						soil = -5;
						break;

					case SECT_UNDERWATER:
						clazz = SECTCLASS_WATER;
						list_appendlink(sector->hide_msgs, str_dup("behind some submerged vegetation"));
						sector->gsct = &gsct_underwater_swim;
						SET_BIT(flags, SECTOR_NO_FADE);
						SET_BIT(flags, SECTOR_NO_SOIL);
						SET_BIT(flags, SECTOR_UNDERWATER);
						break;

					case SECT_DEEP_UNDERWATER:
						clazz = SECTCLASS_WATER;
						sector->gsct = &gsct_underwater_noswim;
						SET_BIT(flags, SECTOR_DEEP_WATER);
						SET_BIT(flags, SECTOR_NO_FADE);
						SET_BIT(flags, SECTOR_NO_GATE);
						SET_BIT(flags, SECTOR_NO_HIDE_OBJ);
						SET_BIT(flags, SECTOR_NO_SOIL);
						SET_BIT(flags, SECTOR_UNDERWATER);
						break;

					case SECT_JUNGLE:
						clazz = SECTCLASS_JUNGLE;
						SET_BIT(flags, SECTOR_NATURE);
						list_appendlink(sector->hide_msgs, str_dup("inside a tree"));
						list_appendlink(sector->hide_msgs, str_dup("under a stump"));
						list_appendlink(sector->hide_msgs, str_dup("in the thick vegetation"));
						list_appendlink(sector->hide_msgs, str_dup("in the branches of a tree"));
						list_appendlink(sector->hide_msgs, str_dup("from sight"));
						break;

					case SECT_PAVED_ROAD:
						clazz = SECTCLASS_ROADS;
						list_appendlink(sector->hide_msgs, str_dup("behind a rock on the side of the road"));
						SET_BIT(flags, SECTOR_NO_SOIL);
						break;

					case SECT_DIRT_ROAD:
						clazz = SECTCLASS_ROADS;
						list_appendlink(sector->hide_msgs, str_dup("behind a rock on the side of the road"));
						soil = 20;
						break;
				}

				sector->sector_class = clazz;
				sector->flags = flags;
				sector->soil = soil;
				sector->move_cost = movement_loss[type];

				insert_sector(sector);
			}
		}

		save_sectors();
	}
	else
	{
		char *word;
		bool fMatch;

		while(str_cmp((word = fread_word(fp)), "#END"))
		{
			fMatch = true;

			switch(word[0])
			{
				case '#':
					if (!str_cmp(word, "#SECTOR"))
					{
						sector = load_sector(fp);

						insert_sector(sector);
						fMatch = true;
						break;
					}
					break;
			}

			if (!fMatch)
			{
				bug(formatf("load_sectors: encountered unknown word '%s'.\n\r", word), 0);
				fread_to_eol(fp);
			}
		}
	}

	// Global Sector pointers
	for(int i = 0; global_sector_table[i].name; i++)
	{
		if (global_sector_table[i].gsct)
			*global_sector_table[i].gsct = NULL;
	}

	ITERATOR sit;
	iterator_start(&sit, sectors_list);
	while((sector = (SECTOR_DATA *)iterator_nextdata(&sit)))
	{
		if (sector->gsct)
			*(sector->gsct) = sector;
	}
	iterator_stop(&sit);

	// Only mandatory one because the server will assume it *IS* assigned after this point
	if (!gsct_inside)
	{
		log_string("load_sectors: Missing sector for gsct_inside.");
		log_string("load_sectors: Please assign \"GSCT Inside~\" to a sector and restart.");
		list_destroy(sectors_list);
		return false;
	}

	return true;
}



struct sectorlist_params
{
	char name[MIL];

	bool has_name;
};

static bool __sectorlist_parse_params(CHAR_DATA *ch, char *argument, struct sectorlist_params *params)
{
	params->has_name = false;

	while(argument[0])
	{
		char arg[MIL];
		argument = one_argument(argument, arg);

		if (!str_prefix(arg, "name"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Missing name in filter.\n\r", ch);
				return false;
			}

			argument = one_argument(argument, params->name);
			params->has_name = true;
		}
		else
		{
			send_to_char("Invalid parameters.\n\r", ch);
			send_to_char("  name <name>        - Filter by name\n\r", ch);
			return false;
		}
		
	}

	return true;
}

void do_sectorlist(CHAR_DATA *ch, char *argument)
{
	BUFFER *buffer = new_buf();
	char buf[MSL];

	struct sectorlist_params params;
	if (!__sectorlist_parse_params(ch, argument, &params))
		return;

	sprintf(buf, "%-4s %-20s\n\r",
		"#", "Name");
	add_buf(buffer, buf);
	sprintf(buf, "%-4s %-20s\n\r",
		"====", "====================");
	add_buf(buffer, buf);

	int i = 0;
	ITERATOR it;
	SECTOR_DATA *sector;
    iterator_start(&it, sectors_list);
    while((sector = (SECTOR_DATA *)iterator_nextdata(&it)))
    {
		if (params.has_name && str_prefix(params.name, sector->name)) continue;

		sprintf(buf, "%-4d %-20s\n\r", ++i,
			sector->name);
		add_buf(buffer, buf);
    }
    iterator_stop(&it);

	sprintf(buf, "%-4s %-20s\n\r",
		"====", "====================");
	add_buf(buffer, buf);

	sprintf(buf, "Total: %d\n\r", i);
	add_buf(buffer, buf);

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



SECTOREDIT( sectoredit_show )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	BUFFER *buffer = new_buf();

	add_buf(buffer, formatf("Name:          %s\n\r", sector->name));
	add_buf(buffer, formatf("GSCT:          %s\n\r", gsct_to_display(sector->gsct)));
	add_buf(buffer, formatf("Class:         %s\n\r", flag_string(sector_classes, sector->sector_class)));
	add_buf(buffer, formatf("Flags:         %s\n\r", flag_string(sector_flags, sector->flags)));
	add_buf(buffer, formatf("Move Cost:     %d\n\r", sector->move_cost));
	add_buf(buffer, formatf("Health Regen:  %d%%\n\r", sector->hp_regen));
	add_buf(buffer, formatf("Mana Regen:    %d%%\n\r", sector->mana_regen));
	add_buf(buffer, formatf("Move Regen:    %d%%\n\r", sector->move_regen));
	add_buf(buffer, formatf("Soil Chance:   %d%%\n\r", sector->soil));

	add_buf(buffer, "\n\rAffinities:\n\r");
	for(int i = 0; i < SECTOR_MAX_AFFINITIES; i++)
	{
		if (sector->affinities[i][0] > CATALYST_NONE && sector->affinities[i][0] < CATALYST_MAX)
			add_buf(buffer, formatf(" %d)  %-20s %5d\n\r", i+1, flag_string(catalyst_types, sector->affinities[i][0]), sector->affinities[i][1]));
		else
			add_buf(buffer, formatf(" %d)  {Dnone{x\n\r", i+1));
	}

	if (list_size(sector->hide_msgs) > 0)
	{
		add_buf(buffer, "\n\rHide Messages:\n\r");
		ITERATOR mit;
		int m = 0;
		char *message;
		iterator_start(&mit, sector->hide_msgs);
		while((message = (char *)iterator_nextdata(&mit)))
		{
			add_buf(buffer, formatf("%2d) ...%s\n\r", m+1, message));
		}
		iterator_stop(&mit);
	}

	add_buf(buffer, formatf("\n\rDescription:\n\r%s\n\r", string_indent(sector->description, 3)));

	add_buf(buffer, "\n\r-----\n\r{WBuilders' Comments:{X\n\r");
	add_buf(buffer, sector->comments);
	add_buf(buffer, "\n\r-----\n\r");

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

SECTOREDIT( sectoredit_affinity )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  affinity <1-3> <catalyst>[ <value>]\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	int index;
	if (!is_number(arg) || (index = atoi(arg)) < 1 || index > 3)
	{
		send_to_char("Please specify a number from 1 to 3.\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	int catalyst;
	if ((catalyst = stat_lookup(arg, catalyst_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Invalid catalyst types.  Use '? catalyst' for valid types.\n\r", ch);
		show_flag_cmds(ch, catalyst_types);
		return false;
	}

	if (catalyst == CATALYST_NONE)
	{
		sector->affinities[index-1][0] = CATALYST_NONE;
		sector->affinities[index-1][1] = 0;
		send_to_char(formatf("Affinity %d cleared.\n\r", index), ch);
	}
	else
	{
		int value;
		if (!is_number(argument) || (value = atoi(argument)) < 1)
		{
			send_to_char("Please provide a positive number.\n\r", ch);
			return false;
		}

		sector->affinities[index-1][0] = catalyst;
		sector->affinities[index-1][1] = value;
		send_to_char(formatf("Affinity %d set.\n\r", index), ch);
	}
	return true;
}

SECTOREDIT( sectoredit_class )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	int16_t clazz;
	if ((clazz = stat_lookup(argument, sector_classes, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Invalid sector class.  Use '? sectorclass' for valid classes.\n\r", ch);
		show_flag_cmds(ch, sector_classes);
		return false;
	}

	sector->sector_class = clazz;
	send_to_char("Sector Class changed.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_comments )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (argument[0] == '\0')
	{
		string_append(ch, &sector->comments);
		return true;
	}

	send_to_char("Syntax:  comments\n\r", ch);
	return false;
}

SECTOREDIT( sectoredit_create )
{
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  create <name>\n\r", ch);
		return false;
	}

	smash_tilde(argument);
	if (get_sector_data(argument) != NULL)
	{
		send_to_char("That name is already in use.\n\r", ch);
		return false;
	}

	SECTOR_DATA *sector = new_sector_data();
	sector->name = str_dup(argument);
	insert_sector(sector);

	ch->pcdata->immortal->last_olc_command = current_time;
	olc_set_editor(ch, ED_SECTOREDIT, sector);

	send_to_char("Sector created.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_description )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (argument[0] == '\0')
	{
		string_append(ch, &sector->description);
		return true;
	}

	send_to_char("Syntax:  description\n\r", ch);
	return false;
}

SECTOREDIT( sectoredit_flags )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	long value;
	if ((value = flag_value(sector_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid sector flag.  Use '? sector' for valid list.\n\r", ch);
		show_flag_cmds(ch, sector_flags);
		return false;
	}

	TOGGLE_BIT(sector->flags, value);

	// If turning on 'no_soil', reset the soil setting
	if (IS_SET(sector->flags, SECTOR_NO_SOIL) && IS_SET(value, SECTOR_NO_SOIL))
		sector->soil = 0;
	send_to_char("SECTOR Flags toggled.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_gsct )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  gsct set <global sector name>\n\r", ch);
		send_to_char("         gsct clear\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "set"))
	{
		SECTOR_DATA **gsct = gsct_from_name(argument);
		if (!gsct)
		{
			send_to_char("Invalid Globl Sector.  Use '? gsct' for valid list.\n\r", ch);
			show_help(ch, "gsct");
			return false;
		}

		if (*gsct) (*gsct)->gsct = NULL;
		*gsct = sector;
		sector->gsct = gsct;
		send_to_char("Global Sector set.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (sector->gsct && sector->gsct == &gsct_inside)
		{
			send_to_char("Unable to unassign {Winside{x without reassigning it in the process.\n\r", ch);
			return false;
		}

		if (sector->gsct)
			*(sector->gsct) = NULL;
		sector->gsct = NULL;
		send_to_char("Global Sector cleared.\n\r", ch);
		return true;
	}

	sectoredit_gsct(ch, "");
	return false;
}

SECTOREDIT( sectoredit_health )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	int regen;
	if (!is_number(argument) || (regen = atoi(argument)) < 0)
	{
		send_to_char("Please provide a non-negative number.\n\r", ch);
		return false;
	}

	sector->hp_regen = regen;
	send_to_char("Sector Health Regen set.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_hidemsgs )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (argument[0] == '\0' )
	{
		send_to_char("Syntax:  hidemsgs add <message>\n\r", ch);
		send_to_char("         hidemsgs remove <#>\n\r", ch);
		send_to_char("         hidemsgs clear\n\r", ch);
		send_to_char("\n\rHide messages are the tail end of the following messages:\n\r", ch);
		send_to_char("{MYou deftly hide {W<OBJECT> {Y<HIDE MESSAGE>{M.{x\n\r", ch);
		send_to_char("{MYou notice {W<HIDER>{M hide {W<OBJECT> {Y<HIDE MESSAGE>{M.{x\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	
	if (!str_prefix(arg, "add"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Please provide a message.\n\r", ch);
			return false;
		}

		smash_tilde(argument);
		list_appendlink(sector->hide_msgs, str_dup(argument));
		send_to_char("Hide Message added.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "remove"))
	{
		int index;
		if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(sector->hide_msgs))
		{
			send_to_char(formatf("Please provide a number from 1 to %d.\n\r", list_size(sector->hide_msgs)), ch);
			return false;
		}

		list_remnthlink(sector->hide_msgs, index, true);
		send_to_char("Hide Message removed.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		list_clear(sector->hide_msgs);
		send_to_char("Hide Messages cleared.\n\r", ch);
		return true;
	}

	sectoredit_hidemsgs(ch, "");
	return false;
}

SECTOREDIT( sectoredit_mana )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	int regen;
	if (!is_number(argument) || (regen = atoi(argument)) < 0)
	{
		send_to_char("Please provide a non-negative number.\n\r", ch);
		return false;
	}

	sector->mana_regen = regen;
	send_to_char("Sector Mana Regen set.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_move )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	int regen;
	if (!is_number(argument) || (regen = atoi(argument)) < 0)
	{
		send_to_char("Please provide a non-negative number.\n\r", ch);
		return false;
	}

	sector->move_regen = regen;
	send_to_char("Sector Move Regen set.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_movecost )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	int cost;
	if (!is_number(argument) || (cost = atoi(argument)) < 0)
	{
		send_to_char("Please provide a non-negative number.\n\r", ch);
		return false;
	}

	sector->move_cost = cost;
	send_to_char("Sector Move Cost set.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_name )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	smash_tilde(argument);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name <name>\n\r", ch);
		return false;
	}

	SECTOR_DATA *other = get_sector_data(argument);
	if (other && other != sector)
	{
		send_to_char("That name is already in use.\n\r", ch);
		return false;
	}

	free_string(sector->name);
	sector->name = str_dup(argument);
	list_remlink(sectors_list, sector, false);
	insert_sector(sector);

	send_to_char("SECTOR Name set.\n\r", ch);
	return true;
}

SECTOREDIT( sectoredit_soil )
{
	SECTOR_DATA *sector;

	EDIT_SECTOR(ch, sector);

	if (IS_SET(sector->flags, SECTOR_NO_SOIL))
	{
		send_to_char("Cannot change the soil chance while sector is {Wno_soil{x.\n\r", ch);
		return false;
	}

	int soil;
	if (!is_number(argument) || (soil = atoi(argument)) < -100 || soil > 100)
	{
		send_to_char("Please provide a number from -100 to 100.\n\r", ch);
		return false;
	}

	sector->soil = soil;
	send_to_char("Sector Soil Chance set.\n\r", ch);
	return true;
}



