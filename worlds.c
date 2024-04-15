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
#include <math.h>
#include "merc.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "olc.h"

#define VERSION_WORLDS_000		0x00000000

#define VERSION_WORLDS_001		0x00000001

#define VERSION_WORLDS			VERSION_WORLDS_001

long *new_vnum_data();
void *copy_vnum_data(void *ptr);
void free_vnum_data(long *data);
void delete_vnum_data(void *ptr);
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

int version_worlds;

WORLD_DATA *get_world_data(long vnum)
{
	ITERATOR it;
	WORLD_DATA *world;
	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		if (world->vnum == vnum)
			break;
	}
	iterator_stop(&it);

	return world;
}

void compute_orbit_focus(ORBIT *orbit)
{
	if (orbit->major >= orbit->minor)
		orbit->focus = (long)(sqrt(orbit->major * orbit->major - orbit->minor * orbit->minor) + 0.5);
	else
		orbit->focus = 0;
}

void save_constellation(FILE *fp, CONSTELLATION_DATA *zodiac)
{
	fprintf(fp, "#CONSTELLATION\n");

	fprintf(fp, "Name %s~\n", fix_string(zodiac->name));
	fprintf(fp, "Description %s~\n", fix_string(zodiac->description));
	fprintf(fp, "Comments %s~\n", fix_string(zodiac->comments));
	fprintf(fp, "Ascii %s~\n", fix_string(zodiac->ascii));

	fprintf(fp, "RA %ld\n", zodiac->right_ascension);
	fprintf(fp, "DEC %ld\n", zodiac->declination);

	// Other aspects
	
	fprintf(fp, "#-CONSTELLATION\n");
}

void save_world(FILE *fp, WORLD_DATA *world)
{
	fprintf(fp, "#WORLD %ld\n", world->vnum);
	fprintf(fp, "Type %s~\n", flag_string(world_types, world->type));

	fprintf(fp, "Name %s~\n", fix_string(world->name));
	fprintf(fp, "Description %s~\n", fix_string(world->description));
	fprintf(fp, "Comments %s~\n", fix_string(world->comments));

	// TODO: Realm
	// TODO: Plane
	// Parentage is resolved on load.

	// Orbit (must have a major axis size to have an "orbit")
	if (world->orbit.major > 0 && world->orbit.minor > 0)
	{
		fprintf(fp, "#ORBIT\n");
		fprintf(fp, "Axes %ld %ld\n", world->orbit.major, world->orbit.minor);
		// Focus is computed on load and edits
		fprintf(fp, "Procession %d\n", world->orbit.procession);
		fprintf(fp, "Tilt %d\n", world->orbit.tilt);
		fprintf(fp, "Offset %ld\n", world->orbit.offset);
		fprintf(fp, "Period %ld\n", world->orbit.period);
		fprintf(fp, "#-ORBIT\n");
	}

	fprintf(fp, "AxialTilt %d\n", world->tilt);

	ITERATOR sit;
	WORLD_DATA *satellite;
	iterator_start(&sit, world->satellites);
	while((satellite = (WORLD_DATA *)iterator_nextdata(&sit)))
	{
		fprintf(fp, "Satellite %ld\n", satellite->vnum);
	}
	iterator_stop(&sit);

	ITERATOR ait;
	AREA_DATA *area;
	iterator_start(&ait, world->areas);
	while((area = (AREA_DATA *)iterator_nextdata(&ait)))
	{
		fprintf(fp, "Area %ld", area->uid);
	}
	iterator_stop(&ait);

	ITERATOR cit;
	CONSTELLATION_DATA *constellation;
	iterator_start(&cit, world->constellations);
	while((constellation = (CONSTELLATION_DATA *)iterator_nextdata(&cit)))
	{
		save_constellation(fp, constellation);
	}
	iterator_stop(&cit);

	fprintf(fp, "#-WORLD\n");
}

void save_worlds()
{
	FILE *fp;

	log_string("save_worlds: saving " WORLDS_FILE);
	if ((fp = fopen(WORLDS_FILE, "w")) == NULL)
	{
		bug("save_worlds: fopen", 0);
		perror(WORLDS_FILE);
		return;
	}

	fprintf(fp, "Version %d\n", VERSION_WORLDS);

	ITERATOR it;
	WORLD_DATA *world;
	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		save_world(fp, world);
	}	
	iterator_stop(&it);

	fprintf(fp, "End\n");
	fclose(fp);
}

CONSTELLATION_DATA *load_constellation(FILE *fp)
{
	CONSTELLATION_DATA *data = new_constellation_data();

	char buf[MSL];
	char *word;
	bool fMatch;

    while (str_cmp((word = fread_word(fp)), "#-CONSTELLATION"))
	{
		fMatch = false;
		switch(word[0])
		{
			case 'A':
				KEYS("Ascii", data->ascii, fread_string(fp));
				break;
			
			case 'C':
				KEYS("Comments", data->comments, fread_string(fp));
				break;
			
			case 'D':
				KEY("DEC", data->declination, fread_number(fp));
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;
			
			case 'R':
				KEY("RA", data->right_ascension, fread_number(fp));
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "load_constellation: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

void load_orbit(FILE *fp, ORBIT *orbit)
{
	char buf[MSL];
	char *word;
	bool fMatch;

    while (str_cmp((word = fread_word(fp)), "#-ORBIT"))
	{
		fMatch = false;
		switch(word[0])
		{
			case 'A':
				if (!str_cmp(word, "Axes"))
				{
					long major = fread_number(fp);
					long minor = fread_number(fp);

					// Make sure the Major Axis >= Minor Axis
					orbit->major = UMAX(major, minor);
					orbit->minor = UMIN(major, minor);

					fMatch = true;
					break;
				}
				break;

			case 'O':
				KEY("Offset", orbit->offset, fread_number(fp));
				break;

			case 'P':
				KEY("Period", orbit->period, fread_number(fp));
				KEY("Procession", orbit->procession, fread_number(fp));
				break;

			case 'T':
				KEY("Tilt", orbit->tilt, fread_number(fp));
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "load_orbit: no match for word %s", word);
			bug(buf, 0);
		}
	}

	// Set the focus
	compute_orbit_focus(orbit);
}


WORLD_DATA *load_world(FILE *fp)
{
	WORLD_DATA *data = new_world_data();

	char buf[MSL];
	char *word;
	bool fMatch;

	data->vnum = fread_number(fp);
	
	// Reconfigure lists
	data->satellites->deleter = delete_vnum_data;
	data->areas->deleter = delete_vnum_data;

    while (str_cmp((word = fread_word(fp)), "#-WORLD"))
	{
		fMatch = false;
		switch(word[0])
		{
			case '#':
				if (!str_cmp(word, "#CONSTELLATION"))
				{
					CONSTELLATION_DATA *zodiac = load_constellation(fp);
					if (IS_VALID(zodiac))
						list_appendlink(data->constellations, zodiac);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#ORBIT"))
				{
					load_orbit(fp, &(data->orbit));
					fMatch = true;
					break;
				}
				break;

			case 'A':
				if (!str_cmp(word, "Area"))
				{
					long *puid = new_vnum_data();

					*puid = fread_number(fp);
					if (!list_appendlink(data->areas, puid))
						free_vnum_data(puid);

					fMatch = true;
					break;
				}
				KEY("AxialTilt", data->tilt, fread_number(fp));
				break;
			
			case 'C':
				KEYS("Comments", data->comments, fread_string(fp));
				break;
			
			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;
			
			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'S':
				if (!str_cmp(word, "Satellite"))
				{
					long *pvnum = new_vnum_data();

					*pvnum = fread_number(fp);
					if (!list_appendlink(data->satellites, pvnum))
						free_vnum_data(pvnum);

					fMatch = true;
					break;
				}
				break;

			case 'T':
				KEY("Type", data->type, stat_lookup(fread_string(fp), world_types, WORLDTYPE_NONE));
				break;
		}

		if (!fMatch)
		{
			sprintf(buf, "load_world: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

static void delete_world_data(void *ptr)
{
	free_world_data((WORLD_DATA *)ptr);
}

bool load_worlds()
{
	FILE *fp;
	char buf[MSL];
	char *word;
	bool fMatch;
	WORLD_DATA *world;

	top_world = 0;
	version_worlds = VERSION_WORLDS;

	log_string("load_worlds: creating world_list");
	world_list = list_createx(false, NULL, delete_world_data);
	if (!IS_VALID(world_list))
	{
		log_string("world_list was not created.");
		return false;
	}

	log_string("load_worlds: loading " WORLDS_FILE);
	if ((fp = fopen(WORLDS_FILE, "r")) == NULL)
	{
		log_string("load_worlds: " WORLDS_FILE " missing.  Starting with no worlds.");
		return true;
	}

	while(str_cmp((word = fread_word(fp)), "End"))
	{
		fMatch = false;

		switch(word[0])
		{
		case '#':
			if (!str_cmp(word, "#WORLD"))
			{
				world = load_world(fp);
				if (world)
				{
					list_appendlink(world_list, world);

					if (world->vnum > top_world)
						top_world = world->vnum;
				}
				else
					log_string("Failed to load a world.");

				fMatch = true;
				break;
			}

		case 'V':
			KEY("Version", version_worlds, fread_number(fp));
			break;
		}

		if (!fMatch) {
			sprintf(buf, "load_worlds: no match for word %s", word);
			bug(buf, 0);
		}
	}

	// Resolve all the worlds at this point
	bool save = false;

	ITERATOR it;
	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		LLIST *satellites = list_create(false);

		if (list_size(world->satellites) > 0)
		{
			ITERATOR sit;
			WORLD_DATA *satellite;
			long *pvnum;
			iterator_start(&sit, world->satellites);
			while((pvnum = (long *)iterator_nextdata(&sit)))
			{
				satellite = get_world_data(*pvnum);
				if (satellite)
				{
					list_appendlink(satellites, satellite);
					satellite->parent = world;	// Link satellite to parent world
				}
				else
				{
					log_string(formatf("load_worlds: world %ld cannot resolve satellite with vnum %ld.", world->vnum, *pvnum));
				}
			}
			iterator_stop(&sit);

			save = true;
		}

		list_destroy(world->satellites);
		world->satellites = satellites;
	}
	iterator_stop(&it);

	if (save)
		save_worlds();

	return true;
}

