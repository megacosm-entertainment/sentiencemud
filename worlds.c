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
#include "interp.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "olc.h"
#include "wilds.h"

#define VERSION_WORLDS_000		0x00000000

#define VERSION_WORLDS_001		0x00000001

#define VERSION_WORLDS			VERSION_WORLDS_001

#define GRAVITY_CONSTANT		(6.67430e-11)
#define NANO_GRAVITY			(6.67430e-20)	// 1e-9 * G
#define NANO_PI_SQ4				(4e9 * M_PI * M_PI)

long *new_vnum_data();
void *copy_vnum_data(void *ptr);
void free_vnum_data(long *data);
void delete_vnum_data(void *ptr);
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table);

double sin_table[36000];
double cos_table[36000];
double tan_table[36000];

int version_worlds;

void init_trig_tables()
{
	for(int i = 0; i < 36000; i++)
	{
		double angle = M_PI * i / 18000;

		sin_table[i] = sin(angle);
		cos_table[i] = cos(angle);
		tan_table[i] = tan(angle);
	}
}

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

double get_latitude(WORLD_DATA *world, long y)
{
	if (world->map && world->map->map_size_y > 1)
	{
		return (world->south_edge - world->north_edge) * y / (world->map->map_size_y - 1) + world->north_edge;
	}

	return NAN;
}

double get_longitude(WORLD_DATA *world, long x)
{
	if (world->map && world->map->map_size_x > 1)
	{
		return 360.0 * x / (world->map->map_size_x - 1);
	}

	return NAN;
}


// Checks whether world B is a satellite of world A
bool is_world_satellite(WORLD_DATA *a, WORLD_DATA *b)
{
	while(b != NULL && b->parent != NULL)
	{
		if (b->parent == a) return true;

		b = b->parent;
	}

	return false;
}

void update_orbital_data(WORLD_DATA *world)
{
	if (world->parent != NULL && world->parent->mass > 0.0 && world->orbit.distance > 0)
	{
		double d = world->orbit.distance;
		world->orbit.period = (long)sqrt(NANO_PI_SQ4 * d * d * d / (GRAVITY_CONSTANT * world->parent->mass));
	}
	else
		world->orbit.period = 0;
}

void update_orbit_angle(WORLD_DATA *world, long step)
{
	ORBIT *orbit = &world->orbit;

	orbit->time += step;

	if (orbit->time >= orbit->period)
		orbit->time -= orbit->period;
	
	orbit->angle = 2.0 * M_PI * orbit->time / orbit->period;
}

void reset_orbit_angle(WORLD_DATA *world)
{
	world->orbit.time = world->orbit.offset;

	world->orbit.angle = 36000 * world->orbit.time / world->orbit.period;
}

void update_orbit_position(ORBIT *orbit)
{
	double x = orbit->distance * cos_table[orbit->angle];
	double y = orbit->distance * sin_table[orbit->angle];
	double z = 0.0;

	double c = cos_table[orbit->tilt];
	double s = sin_table[orbit->tilt];

	// Orbital Tilt: Rotate Z to X about Y
	orbit->position.x = s * z + c * x;
	orbit->position.y = y;
	orbit->position.z = c * z - s * x;
}

// Recursively update all of the satellite positions
void update_satellite_positions(WORLD_DATA *world)
{
	ITERATOR it;
	WORLD_DATA *satellite;
	iterator_start(&it, world->satellites);
	while((satellite = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		satellite->orbit.position.x += world->orbit.position.x;
		satellite->orbit.position.y += world->orbit.position.y;
		satellite->orbit.position.z += world->orbit.position.z;

		update_satellite_positions(satellite);
	}
	iterator_stop(&it);
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
	if (location_isset(&world->death_room)) {
		if(world->death_room.wuid)
			fprintf(fp, "DeathRoomW %lu %lu %lu %lu\n", 	world->death_room.wuid, world->death_room.id[0], world->death_room.id[1], world->death_room.id[2]);
		else
			fprintf(fp, "DeathRoom %ld %ld\n",
				world->death_room.area->uid,
				world->death_room.id[0]);
    }

	// Parentage is resolved on load.

	// Orbit (must have an orbital distance)
	if (world->orbit.distance > 0)
	{
		fprintf(fp, "#ORBIT\n");
		fprintf(fp, "Distance %ld\n", world->orbit.distance);
		fprintf(fp, "Procession %d\n", world->orbit.procession);
		fprintf(fp, "Tilt %d\n", world->orbit.tilt);
		fprintf(fp, "Offset %ld\n", world->orbit.offset);
		fprintf(fp, "Period %ld\n", world->orbit.period);
		fprintf(fp, "#-ORBIT\n");
	}

	fprintf(fp, "AxialTilt %d\n", world->tilt);
	if (world->map)
	{
		fprintf(fp, "Map %ld\n", world->map->uid);
		fprintf(fp, "NorthEdge %lf\n", world->north_edge);
		fprintf(fp, "SouthEdge %lf\n", world->south_edge);
	}
	fprintf(fp, "Day %d\n", world->day);
	fprintf(fp, "Mass %lf\n", world->mass);

	ITERATOR sit;
	WORLD_DATA *satellite;
	iterator_start(&sit, world->satellites);
	while((satellite = (WORLD_DATA *)iterator_nextdata(&sit)))
	{
		fprintf(fp, "Satellite %ld\n", satellite->vnum);
	}
	iterator_stop(&sit);

	ITERATOR rit;
	AREA_REGION *region;
	iterator_start(&rit, world->regions);
	while((region = (AREA_REGION *)iterator_nextdata(&rit)))
	{
		fprintf(fp, "Region %ld %ld", region->area->uid, region->uid);
	}
	iterator_stop(&rit);

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
			case 'D':
				KEY("Distance", orbit->distance, fread_number(fp));
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
}

void insert_satellite(WORLD_DATA *world, WORLD_DATA *satellite)
{
	ITERATOR it;
	WORLD_DATA *sat;
	iterator_start(&it, world->satellites);
	while((sat = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		if (satellite->orbit.distance < sat->orbit.distance)
		{
			iterator_insert_before(&it, satellite);
			break;
		}
	}
	iterator_stop(&it);

	if (!sat)
		list_appendlink(world->satellites, satellite);
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
	data->regions->deleter = delete_list_area_region_data;

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
				KEY("AxialTilt", data->tilt, fread_number(fp));
				break;
			
			case 'C':
				KEYS("Comments", data->comments, fread_string(fp));
				break;
			
			case 'D':
				KEY("Day", data->day, fread_number(fp));
				if (!str_cmp(word, "DeathRoom"))
				{
					long auid = fread_number(fp);
					long vnum = fread_number(fp);

					rs_location_set(&data->rs_death_room, auid, 0, vnum, 0, 0);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "DeathRoomW"))
				{
					long w = fread_number(fp);
					long x = fread_number(fp);
					long y = fread_number(fp);
					long z = fread_number(fp);

					rs_location_set(&data->rs_death_room, 0, w, x, y, z);
					fMatch = true;
					break;
				}
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'M':
				KEY("Map", data->map_uid, fread_number(fp));
				KEY("Mass", data->mass, fread_double(fp));
				break;
			
			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				KEY("NorthEdge", data->north_edge, fread_double(fp));
				break;

			case 'R':
				if (!str_cmp(word, "Region"))
				{
					LLIST_AREA_REGION_DATA *pregion = new_list_area_region_data();
					if (pregion)
					{
						pregion->aid = fread_number(fp);
						pregion->rid = fread_number(fp);

						if (!list_appendlink(data->regions, pregion))
							free_list_area_region_data(pregion);
					}

					fMatch = true;
					break;
				}
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
				KEY("SouthEdge", data->south_edge, fread_double(fp));
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

void insert_world(WORLD_DATA *world)
{
	ITERATOR it;
	WORLD_DATA *w;
	
	iterator_start(&it, world_list);
	while((w = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		if (world->vnum < w->vnum)
		{
			iterator_insert_before(&it, world);
			break;
		}
	}
	iterator_stop(&it);

	if (!w)
		list_appendlink(world_list, world);
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
					insert_world(world);

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
		LLIST *satellites = world->satellites;
		world->satellites = list_create(false);

		if (list_size(satellites) > 0)
		{
			ITERATOR sit;
			WORLD_DATA *satellite;
			long *pvnum;
			iterator_start(&sit, satellites);
			while((pvnum = (long *)iterator_nextdata(&sit)))
			{
				satellite = get_world_data(*pvnum);
				if (satellite)
				{
					satellite->parent = world;	// Link satellite to parent world
					insert_satellite(world, satellite);
				}
				else
				{
					log_string(formatf("load_worlds: world %ld cannot resolve satellite with vnum %ld.", world->vnum, *pvnum));
				}
			}
			iterator_stop(&sit);

			save = true;
		}

		list_destroy(satellites);
	}
	iterator_stop(&it);

	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		reset_orbit_angle(world);
		update_orbit_angle(world, gconfig.game_time);
		update_orbit_position(&world->orbit);
	}
	iterator_stop(&it);

	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		// Only if the world is not in orbit.
		if (world->parent == NULL)
		{
			update_satellite_positions(world);
		}
	}
	iterator_stop(&it);


	if (save)
		save_worlds();

	return true;
}

void resolve_worlds()
{
	ITERATOR it;
	WORLD_DATA *world;

	iterator_start(&it, world_list);
	while((world = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		// Resolve surface map
		if (world->map_uid > 0)
			world->map = get_wilds_from_uid(NULL, world->map_uid);

		if (rs_location_isset(&world->rs_death_room))
		{
			AREA_DATA *area = get_area_from_uid(world->rs_death_room.auid);

			location_set(&world->death_room,area,world->rs_death_room.wuid,world->rs_death_room.id[0],world->rs_death_room.id[1],world->rs_death_room.id[2]);
		}
		
		// Resolve area regions
		LLIST *regions = list_create(false);
		ITERATOR rit;
		LLIST_AREA_REGION_DATA *pregion;

		iterator_start(&rit, world->regions);
		while((pregion = (LLIST_AREA_REGION_DATA *)iterator_nextdata(&rit)))
		{
			AREA_DATA *area = get_area_from_uid(pregion->aid);
			if (area)
			{
				AREA_REGION *region = get_area_region_by_uid(area, pregion->rid);
				if (region)
				{
					list_appendlink(regions, region);
					region->world = world;
				}
			}
		}
		iterator_stop(&rit);

		list_destroy(world->regions);
		world->regions = regions;
	}
	iterator_stop(&it);
}




/////////////////
// WorldEdit
//

const struct olc_cmd_type worldedit_table[] =
{
	{	"?",				show_help					},
	{	"commands",			show_commands				},
	{	"comments",			worldedit_comments			},
	{	"constellations",	worldedit_constellations	},
	{	"create",			worldedit_create			},
	{	"day",				worldedit_day				},
	{	"deathroom",		worldedit_deathroom			},
	{	"description",		worldedit_description		},
	{	"edges",			worldedit_edges				},
	{	"map",				worldedit_map				},
	{	"mass",				worldedit_mass				},
	{	"name",				worldedit_name				},
	{	"orbit",			worldedit_orbit				},
	{	"plane",			worldedit_plane				},
	{	"realm",			worldedit_realm				},
	{	"regions",			worldedit_regions			},
	{	"satellites",		worldedit_satellites		},
	{	"show",				worldedit_show				},
	{	"tilt",				worldedit_tilt				},
	{	"type",				worldedit_type				},
	{	NULL,				NULL						}
};


void do_worldedit(CHAR_DATA *ch, char *argument)
{
	WORLD_DATA *world;
    char command[MSL];

	argument = one_argument(argument, command);

	if (is_number(command))
	{
		long vnum = atoi(command);

		world = get_world_data(vnum);
		if (!world)
		{
			send_to_char("No such world with that vnum exists.\n\r", ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		olc_set_editor(ch, ED_WORLDEDIT, world);
		return;
	}

	if (!str_cmp(command, "create"))
	{
		worldedit_create(ch, "");
		return;
	}

	send_to_char("Syntax:  worldedit <vnum>\n\r", ch);
	send_to_char("         worldedit create\n\r", ch);
}

void do_worldlist(CHAR_DATA *ch, char *argument)
{
	// List worlds
	command_under_construction(ch);
}


void do_worldshow(CHAR_DATA *ch, char *argument)
{
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  worldshow <vnum>\n\r", ch);
		return;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	long vnum = atol(arg);
	WORLD_DATA *world = get_world_data(vnum);
	if (!world)
	{
		send_to_char("There is no world with that vnum.\n\r", ch);
		return;		
	}

	olc_show_item(ch, world, worldedit_show, argument);
}

void worldedit(CHAR_DATA *ch, char *argument)
{
	char command[MIL];
	char arg[MIL];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (!IS_IMPLEMENTOR(ch))
	{
		send_to_char("WorldEdit:  Insufficient security to edit worlds - action logged.\n\r", ch);
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
		skedit_show(ch, argument);
		return;
	}

	// Tabbing
	if (is_number(command))
	{
		int tab = atoi(command);
		if (tab < 1 || tab > ch->desc->nMaxEditTabs)
		{
			send_to_char("Huh?\n\r", ch);
			return;
		}

		ch->desc->nEditTab = tab - 1;
		worldedit_show(ch, "");
		return;
	}

	for (cmd = 0; worldedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, worldedit_table[cmd].name))
		{
			if ((*worldedit_table[cmd].olc_fun) (ch, argument))
			{
				save_worlds();
			}
			return;
		}
	}

	interpret(ch, arg);
}

WORLDEDIT( worldedit_comments )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] != '\0')
	{
		send_to_char("Syntax:  comments\n\r", ch);
		return false;
	}

	string_append(ch, &world->comments);
	return true;
}

WORLDEDIT( worldedit_constellations )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  constellations list\n\r", ch);
		send_to_char("         constellations add <name>\n\r", ch);
		send_to_char("         constellations remove <#>\n\r", ch);
		send_to_char("         constellations <#> name <name>\n\r", ch);
		send_to_char("         constellations <#> comments\n\r", ch);
		send_to_char("         constellations <#> description\n\r", ch);
		send_to_char("         constellations <#> ascii\n\r", ch);
		send_to_char("         constellations <#> ra <degree>\n\r", ch);
		send_to_char("         constellations <#> declination <degree>\n\r", ch);
		return false;
	}

	char buf[MSL];
	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "add"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  constellation add <name>\n\r", ch);
			send_to_char("Please provide a name.\n\r", ch);
			return false;
		}


		CONSTELLATION_DATA *con = new_constellation_data();
		smash_tilde(argument);
		con->name = str_dup(argument);

		list_appendlink(world->constellations, con);
		send_to_char("Constellation added.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "list"))
	{
		if (list_size(world->constellations) > 0)
		{
			ITERATOR it;
			CONSTELLATION_DATA *con;

			BUFFER *buffer = new_buf();
			add_buf(buffer, "     [        Name        ] [ Right Asc ] [ Declination ]\n\r");
			add_buf(buffer, "==========================================================\n\r");

			int i = 0;
			iterator_start(&it, world->constellations);
			while((con = (CONSTELLATION_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, "%-4d  %-20.20s    %9.2lf     %9.2lf %c\n\r", ++i, con->name,
					0.01 * con->right_ascension,
					0.01 * abs(con->declination),
					((con->declination < 0) ? 'S' : ((con->declination > 0) ? 'N' : ' ')));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			add_buf(buffer, "----------------------------------------------------------\n\r");

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
			send_to_char("There are no constellations.\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "remove"))
	{
		if (list_size(world->constellations) < 1)
		{
			send_to_char("There are no constellations to remove.\n\r", ch);
			return false;
		}

		int index;
		if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(world->constellations))
		{
			send_to_char(formatf("Please provide a number from 1 to %d.\n\r", list_size(world->constellations)), ch);
			return false;
		}

		list_remnthlink(world->constellations, index, true);
		send_to_char(formatf("Constellation #%d removed.\n\r", index), ch);
		return true;
	}

	if (is_number(arg))
	{
		if (list_size(world->constellations) < 1)
		{
			send_to_char("There are no constellations to edit.\n\r", ch);
			return false;
		}

		int index;
		if ((index = atoi(arg)) < 1 || index > list_size(world->constellations))
		{
			send_to_char(formatf("Please provide a number from 1 to %d.\n\r", list_size(world->constellations)), ch);
			return false;
		}

		CONSTELLATION_DATA *con = (CONSTELLATION_DATA *)list_nthdata(world->constellations, index);

		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  constellations <#> name <name>\n\r", ch);
			send_to_char("         constellations <#> comments\n\r", ch);
			send_to_char("         constellations <#> description\n\r", ch);
			send_to_char("         constellations <#> ascii\n\r", ch);
			send_to_char("         constellations <#> ra <degree>\n\r", ch);
			send_to_char("         constellations <#> declination <degree>\n\r", ch);
			return false;
		}

		char arg2[MIL];
		argument = one_argument(argument, arg2);

		if (!str_prefix(arg2, "name"))
		{
			if (argument[0] == '\0')
			{
				send_to_char("Please provide a name.\n\r", ch);
				return false;
			}

			smash_tilde(argument);
			free_string(con->name);
			con->name = str_dup(argument);
			send_to_char(formatf("Constellation #%d Name changed.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg2, "comments"))
		{
			if (argument[0] != '\0')
			{
				send_to_char("Syntax:  comments\n\r", ch);
				return false;
			}

			string_append(ch, &con->comments);
			return true;
		}

		if (!str_prefix(arg2, "description"))
		{
			if (argument[0] != '\0')
			{
				send_to_char("Syntax:  description\n\r", ch);
				return false;
			}

			string_append(ch, &con->description);
			return true;
		}

		if (!str_prefix(arg2, "ascii"))
		{
			if (argument[0] != '\0')
			{
				send_to_char("Syntax:  ascii\n\r", ch);
				return false;
			}

			string_append(ch, &con->ascii);
			return true;
		}

		if (!str_prefix(arg2, "ra"))
		{
			double ra;
			if (!is_double(argument) || (ra = atof(argument)) < 0.0 || ra > 359.99)
			{
				send_to_char("Please provide a number from 0.00 to 359.99.\n\r", ch);
				return false;
			}

			con->right_ascension = (int)(100 * ra);
			send_to_char(formatf("Constellation #%d Right Ascension changed.\n\r", index), ch);
			return true;
		}

		if (!str_prefix(arg2, "declination"))
		{
			double dec;
			if (!is_double(argument) || (dec = atof(argument)) < -90.0 || dec > 90)
			{
				send_to_char("Please provide a number from -90.0 (South) to 90.0 (North).\n\r", ch);
				return false;
			}

			con->declination = (int)(100 * dec);
			send_to_char(formatf("Constellation #%d Declination changed.\n\r", index), ch);
			return true;
		}

		worldedit_constellations(ch, arg);
		return false;
	}

	worldedit_constellations(ch, "");
	return false;
}

WORLDEDIT( worldedit_create )
{
	WORLD_DATA *world;

	long vnum = 1;
	if (argument[0] == '\0')
	{
		// World List is ordered by vnum
		ITERATOR it;
		iterator_start(&it, world_list);
		while((world = (WORLD_DATA *)iterator_nextdata(&it)))
		{
			if (world->vnum > vnum)
			{
				// Found a gap
				break;
			}
			++vnum;	// Next vnum possible
		}
		iterator_stop(&it);
	}
	else if (!is_number(argument) || (vnum = atol(argument)) < 1)
	{
		send_to_char("Please provide a number.\n\r", ch);
		return false;
	}

	world = get_world_data(vnum);
	if (world)
	{
		send_to_char("A world already exists with that vnum.\n\r", ch);
		return false;
	}

	world = new_world_data();
	world->vnum = vnum;
	insert_world(world);

	olc_set_editor(ch, ED_WORLDEDIT, world);

	send_to_char("World created.\n\r", ch);
	return true;
}

WORLDEDIT( worldedit_day )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  day <length in seconds>\n\r", ch);
		return false;
	}

	int day;
	if (!is_number(argument) || (day = atoi(argument)) < 1)
	{
		send_to_char("Specify a positive number.\n\r", ch);
		return false;
	}

	world->day = day;
	send_to_char("Day length changed.\n\r", ch);
	return true;
}

WORLDEDIT( worldedit_deathroom )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  deathroom room <widevnum>\n\r", ch);
		send_to_char("         deathroom wilds <uid> <x> <y>\n\r", ch);
		send_to_char("         deathroom clear\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "clear"))
	{
		location_clear(&world->death_room);
		send_to_char("World death room cleared.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "room"))
	{
		WNUM wnum;
		if (!parse_widevnum(argument, ch->in_room->area, &wnum) || !wnum.pArea || wnum.vnum < 1)
		{
			send_to_char("Please specify a widevnum.\n\r", ch);
			return false;
		}

		ROOM_INDEX_DATA *room = get_room_index(wnum.pArea, wnum.vnum);
		if (!room)
		{
			send_to_char("No such room by that widevnum.\n\r", ch);
			return false;
		}

		location_set(&world->death_room, room->area, 0, room->vnum, 0, 0);
		send_to_char(formatf("World death room set to %s (%ld#%ld).\n\r", room->name, room->area->uid, room->vnum), ch);
		return true;
	}

	if (!str_prefix(arg, "wilds"))
	{
		char arg2[MIL];
		char arg3[MIL];

		long w;
		if (!is_number(arg2) || (w = atol(arg2)) < 1)
		{
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		WILDS_DATA *wilds = get_wilds_from_uid(NULL, w);
		if (!wilds)
		{
			send_to_char("No such wilds with that uid.\n\r", ch);
			return false;
		}

		long x, y;
		if (!is_number(arg3) || (x = atol(arg3)) < 0 || x >= wilds->map_size_x)
		{
			send_to_char(formatf("Please specify a number from 0 to %d.\n\r", wilds->map_size_x - 1), ch);
			return false;
		}

		if (!is_number(argument) || (y = atol(argument)) < 0 || y >= wilds->map_size_y)
		{
			send_to_char(formatf("Please specify a number from 0 to %d.\n\r", wilds->map_size_y - 1), ch);
			return false;
		}

		location_set(&world->death_room, NULL, w, x, y, 0);
		send_to_char(formatf("World death room set to wilderness '%s' (%ld) at (%ld, %ld).\n\r", wilds->name, w, x, y), ch);
		return true;
	}

	return false;
}

WORLDEDIT( worldedit_description )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] != '\0')
	{
		send_to_char("Syntax:  description\n\r", ch);
		return false;
	}

	string_append(ch, &world->description);
	return true;
}

WORLDEDIT( worldedit_edges )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  edges north <latitude in degrees>\n\r", ch);
		send_to_char("         edges south <latitude in degrees>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	
	if (!str_prefix(arg, "north"))
	{
		double latitude;
		if (!is_double(argument) || (latitude = atof(argument)) < -90.0 || latitude > 90.0)
		{
			send_to_char("Please specify a number from -90.0 to 90.0.\n\r", ch);
			return false;
		}

		world->north_edge = latitude;
		send_to_char("Latitude set for Northern Edge.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "south"))
	{
		double latitude;
		if (!is_double(argument) || (latitude = atof(argument)) < -90.0 || latitude > 90.0)
		{
			send_to_char("Please specify a number from -90.0 to 90.0.\n\r", ch);
			return false;
		}

		world->south_edge = latitude;
		send_to_char("Latitude set for Southern Edge.\n\r", ch);
		return true;
	}

	worldedit_edges(ch, "");
	return false;
}

WORLDEDIT( worldedit_map )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  map <wilderness uid>\n\r", ch);
		send_to_char("         map clear\n\r", ch);
		return false;
	}

	if (!str_prefix(argument, "clear"))
	{
		world->map = NULL;
		send_to_char("Overworld map cleared.\n\r", ch);
		return true;
	}

	long uid;
	if (!is_number(argument) || (uid = atol(argument)) < 1)
	{
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	WILDS_DATA *wilds = get_wilds_from_uid(NULL, uid);
	if (!wilds)
	{
		send_to_char("No such wilderness with that uid.\n\r", ch);
		return false;
	}

	world->map = wilds;
	send_to_char(formatf("Overworld map changed to '%s' (%ld).\n\r", wilds->name, uid), ch);
	return true;
}

WORLDEDIT( worldedit_mass )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	double mass;
	if (!is_double(argument) || (mass = atof(argument)) <= 0.0)
	{
		send_to_char("Syntax:  mass <mass in kg>\n\r", ch);
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	world->mass = mass;
	update_orbital_data(world);

	// Update the period of the satellites
	ITERATOR it;
	WORLD_DATA *satellite;
	iterator_start(&it, world->satellites);
	while((satellite = (WORLD_DATA *)iterator_nextdata(&it)))
	{
		update_orbital_data(satellite);
	}
	iterator_stop(&it);

	return true;
}

WORLDEDIT( worldedit_name )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  name <name>\n\r", ch);
		send_to_char("Please specify a name.\n\r", ch);
		return false;
	}

	smash_tilde(argument);
	free_string(world->name);
	world->name = str_dup(argument);
	send_to_char("World name changed.\n\r", ch);
	return true;
}

WORLDEDIT( worldedit_orbit )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  orbit distance <distance in meters>\n\r", ch);
		send_to_char("         orbit procession <degrees> (NYI)\n\r", ch);
		send_to_char("         orbit tilt <degrees> (NYI)\n\r", ch);
		send_to_char("         orbit offset <seconds>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "distance"))
	{
		long distance;
		if (!is_number(argument) || (distance = atol(argument)) < 1)
		{
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		world->orbit.distance = distance;
		update_orbital_data(world);

		// Update position in the satellite list
		if (world->parent != NULL)
		{
			list_remlink(world->parent->satellites, world, false);
			insert_satellite(world->parent, world);
		}

		send_to_char("Orbital distance changed.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "procession"))
	{
		command_under_construction(ch);
		return false;
	}

	if (!str_prefix(arg, "tilt"))
	{
		command_under_construction(ch);
		return false;
	}

	if (!str_prefix(arg, "offset"))
	{
		if (!is_number(argument))
		{
			send_to_char("Please specify a number.\n\r", ch);
			return false;
		}

		long offset = atol(argument);
		world->orbit.offset = offset;
		reset_orbit_angle(world);
		update_orbit_angle(world, gconfig.game_time);
		
		send_to_char("Orbit time offset changed.\n\r", ch);
		return true;
	}

	worldedit_orbit(ch, "");
	return false;
}

WORLDEDIT( worldedit_plane )
{
	command_under_construction(ch);
	return false;
}

WORLDEDIT( worldedit_realm )
{
	command_under_construction(ch);
	return false;
}

WORLDEDIT( worldedit_regions )
{
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  regions list\n\r", ch);
		send_to_char("         regions add <area uid> <region uid|default>\n\r", ch);
		send_to_char("         regions clear\n\r", ch);
		send_to_char("         regions remove <#>\n\r", ch);
		return false;
	}

	char buf[MSL];
	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "list"))
	{
		if (list_size(world->regions) > 0)
		{
			ITERATOR it;
			AREA_REGION *region;

			BUFFER *buffer = new_buf();
			add_buf(buffer, "     [ UID ] [        Name        ] [        Area        ]\n\r");
			add_buf(buffer, "===========================================================\n\r");

			int i = 0;
			iterator_start(&it, world->regions);
			while((region = (AREA_REGION *)iterator_nextdata(&it)))
			{
				sprintf(buf, "%-4d    %s    %-20.20s   %s\n\r", ++i,
					((region->uid > 0) ? formatf("%3d", region->uid) : "DEF"),
					region->name,
					formatf("(%ld) %s", region->area->uid, region->area->name));
				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			add_buf(buffer, "-----------------------------------------------------------\n\r");

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
			send_to_char("There are no regions assigned to this world.\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "add"))
	{
		char arg2[MIL];
		argument = one_argument(argument, arg2);

		long aid;
		if (!is_number(arg2) || (aid = atol(arg2)) < 1)
		{
			send_to_char("Please specify a positive number.\n\r", ch);
			return false;
		}

		AREA_DATA *area = get_area_from_uid(aid);
		if (!area)
		{
			send_to_char("No such area with that uid.\n\r", ch);
			return false;
		}

		AREA_REGION *region;

		if (!str_prefix(argument, "default"))
			region = &area->region;
		else
		{
			long rid;
			if (!is_number(argument) || (rid = atol(argument)) < 1)
			{
				send_to_char("Please specify a positive number.\n\r", ch);
				return false;
			}

			region = get_area_region_by_uid(area, rid);
			if (!region)
			{
				send_to_char("No such region with that uid.\n\r", ch);
				return false;
			}
		}

		if (region->world != NULL)
		{
			send_to_char("That region is already assigned to a world.\n\r", ch);
			return false;
		}

		region->world = world;
		list_appendlink(world->regions, region);
		send_to_char("Region added.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (list_size(world->regions) > 0)
		{
			int index;
			if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(world->regions))
			{
				send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(world->regions)), ch);
				return false;
			}

			// Unlink world from regions
			ITERATOR it;
			AREA_REGION *region;
			iterator_start(&it, world->regions);
			while((region = (AREA_REGION *)iterator_nextdata(&it)))
			{
				region->world = NULL;
			}
			iterator_stop(&it);
			list_clear(world->regions);

			send_to_char(formatf("Regions cleared.\n\r", index), ch);
			return true;
		}

		send_to_char("There are no regions assigned to the world.\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "remove"))
	{
		if (list_size(world->regions) > 0)
		{
			int index;
			if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(world->regions))
			{
				send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(world->regions)), ch);
				return false;
			}

			AREA_REGION *region = (AREA_REGION *)list_nthdata(world->regions, index);
			list_remnthlink(world->regions, index, false);
			region->world = NULL;

			send_to_char(formatf("Region #%d removed.\n\r", index), ch);
			return true;
		}

		send_to_char("There are no regions assigned to the world.\n\r", ch);
		return false;
	}

	worldedit_regions(ch, "");
	return false;
}

WORLDEDIT( worldedit_satellites )
{
	WORLD_DATA *world;
	EDIT_WORLD(ch, world);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  satellites list\n\r", ch);
		send_to_char("         satellites clear\n\r", ch);
		send_to_char("         satellites add <world>\n\r", ch);
		send_to_char("         satellites remove <#>\n\r", ch);
		return false;
	}

	char arg[MIL];
	char buf[MSL];
	argument = one_argument(argument, arg);
	
	if (!str_prefix(arg, "list"))
	{
		if (list_size(world->satellites) > 0)
		{
			ITERATOR it;
			WORLD_DATA *satellite;

			BUFFER *buffer = new_buf();
			add_buf(buffer, "     [        Name        ]\n\r");
			add_buf(buffer, "============================\n\r");

			int i = 0;
			iterator_start(&it, world->satellites);
			while((satellite = (WORLD_DATA *)iterator_nextdata(&it)))
			{
				sprintf(buf, "%-4d  %s\n\r", ++i, satellite->name);
				add_buf(buffer, buf);
			}
			iterator_stop(&it);

			add_buf(buffer, "----------------------------\n\r");

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
			send_to_char("There are no satellites.\n\r", ch);
		return false;
	}
	
	if (!str_prefix(arg, "clear"))
	{
		if (list_size(world->satellites) > 0)
		{
			ITERATOR it;
			WORLD_DATA *satellite;
			iterator_start(&it, world->satellites);
			while((satellite = (WORLD_DATA *)iterator_nextdata(&it)))
			{
				satellite->parent = NULL;
			}
			iterator_stop(&it);

			list_clear(world->satellites);
			send_to_char("Satellites cleared.\n\r", ch);
			return true;
		}

		send_to_char("There are no satellites.\n\r", ch);
		return false;
	}
	
	if (!str_prefix(arg, "add"))
	{
		long vnum = atol(argument);
		WORLD_DATA *satellite = get_world_data(vnum);

		if (!satellite)
		{
			send_to_char("No such world with that vnum.\n\r", ch);
			return false;
		}

		if (satellite == world)
		{
			send_to_char("Cannot make a world its own satellite.\n\r", ch);
			return false;
		}

		if (satellite->parent != NULL)
		{
			send_to_char("That world is already a satellite.\n\r", ch);
			return false;
		}

		if (is_world_satellite(satellite, world))
		{
			send_to_char("That would create a paradox.  Try not to break reality.\n\r", ch);
			return false;
		}

		satellite->parent = world;
		list_appendlink(world->satellites, satellite);

		update_orbital_data(satellite);

		send_to_char("Satellite added.\n\r", ch);
		return true;
	}
	
	if (!str_prefix(arg, "remove"))
	{
		if (list_size(world->satellites) > 0)
		{
			int index;
			if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(world->satellites))
			{
				send_to_char(formatf("Please provide a number from 1 to %d.\n\r", list_size(world->satellites)), ch);
				return false;
			}

			WORLD_DATA *satellite = (WORLD_DATA *)list_nthdata(world->satellites, index);

			list_remnthlink(world->satellites, index, false);
			satellite->parent = NULL;
			send_to_char(formatf("Satellite #%d (%s - %d) removed.\n\r", index, satellite->name, satellite->vnum), ch);
			return true;
		}

		send_to_char("There are no satellites around this world.\n\r", ch);
		return false;
	}

	worldedit_satellites(ch, "");
	return false;
}

const char *worldedit_tabs[] =
{
	"General",
	"Regions",
	"Orbit",
	"Satellites",
	"Constellations",
	NULL
};


WORLDEDIT( worldedit_show )
{
	char buf[MSL];
	BUFFER *buffer;
	WORLD_DATA *world;

	EDIT_WORLD(ch, world);

	// Base show
	if (argument[0] == '\0')
	{
		buffer = new_buf();
		// Show Header
		sprintf(buf, "%s: {W%s {x({W%ld{x)\n\r", flag_string(world_types, world->type), world->name, world->vnum);
		add_buf(buffer, buf);

		olc_buffer_show_tabs(ch, buffer, worldedit_tabs);

		ITERATOR it;

		switch(ch->desc->nEditTab)
		{
		case 0:	// General

			olc_buffer_show_string(ch, buffer, (world->plane ? world->plane->name : NULL), "plane", "Plane:", 20, "xDW");
			olc_buffer_show_string(ch, buffer, (world->plane ? world->realm->name : NULL), "realm", "Realm:", 20, "xDW");

			add_buf(buffer, "Description:\n\r");
			add_buf(buffer, string_indent(world->description, 3));
			add_buf(buffer, "{x\n\r");

			add_buf(buffer, "Builder Comments:\n\r");
			add_buf(buffer, string_indent(world->comments, 3));
			add_buf(buffer, "{x\n\r");

			if (location_isset(&world->death_room))
			{
				if (world->death_room.wuid)
				{
					WILDS_DATA *wilds = get_wilds_from_uid(NULL, world->death_room.wuid);

					olc_buffer_show_string(ch, buffer, 
						formatf("%s (%ld) at (%ld, %ld)",
							(wilds ? wilds->name : "none"),
							world->death_room.wuid,
							world->death_room.id[0],
							world->death_room.id[1]),
						"deathroom", "Death Room:", 20, "xDW");
				}
				else if(world->death_room.area && world->death_room.id[0])
				{
					ROOM_INDEX_DATA *room = get_room_index(world->death_room.area, world->death_room.id[0]);
					olc_buffer_show_string(ch, buffer, 
						formatf("%s (%ld#%ld) in %s",
							(room ? room->name : "none"),
							world->death_room.area->uid,
							world->death_room.id[0],
							world->death_room.area->name),
						"deathroom", "Death Room:", 20, "xDW");
				}
				else
					olc_buffer_show_string(ch, buffer, "{D(invalid)", "deathroom", "Death Room:", 20, "xDW");
			}
			else
				olc_buffer_show_string(ch, buffer, NULL, "deathroom", "Death Room:", 20, "xDW");

			olc_buffer_show_string(ch, buffer, formatf("%d kilogram%s", world->mass, ((world->mass == 1)?"":"s")), "mass", "Mass:", 20, "xDW");
			break;
		
		case 1:	// Regions
			if (list_size(world->regions) > 0)
			{
				ITERATOR it;
				AREA_REGION *region;

				add_buf(buffer, "     [ UID ] [        Name        ] [        Area        ]\n\r");
				add_buf(buffer, "===========================================================\n\r");

				int i = 0;
				iterator_start(&it, world->regions);
				while((region = (AREA_REGION *)iterator_nextdata(&it)))
				{
					sprintf(buf, "%-4d    %s    %-20.20s   %s\n\r", ++i,
						((region->uid > 0) ? formatf("%3d", region->uid) : "DEF"),
						region->name,
						formatf("(%ld) %s", region->area->uid, region->area->name));
					add_buf(buffer, buf);
				}
				iterator_stop(&it);

				add_buf(buffer, "-----------------------------------------------------------\n\r");
			}
			else
				add_buf(buffer, "No Regions Assigned.\n\r");
			break;
		
		case 2: // Orbit
			olc_buffer_show_string(ch, buffer, formatf("%ld meter%s", world->orbit.distance, ((world->orbit.distance == 1)?"":"s")), "orbit distance", "Distance:", 20, "xDW");

			olc_buffer_show_string(ch, buffer, formatf("%d second%s", world->orbit.offset, ((world->orbit.offset == 1)?"":"s")), "orbit offset", "Offset:", 20, "xDW");

			olc_buffer_show_string(ch, buffer, formatf("%d second%s", world->orbit.period, ((world->orbit.period == 1)?"":"s")), NULL, "Period:", 20, "xDW");

			olc_buffer_show_string(ch, buffer, formatf("%.2lf degree%s", (0.01 * world->orbit.procession), ((world->orbit.procession == 100)?"":"s")), "orbit procession", "Orbital Procession:", 20, "xDW");
			olc_buffer_show_string(ch, buffer, formatf("%.2lf degree%s", (0.01 * world->orbit.tilt), ((world->orbit.tilt == 100)?"":"s")), "orbit tilt", "Orbital Tilt:", 20, "xDW");

			break;
		
		case 3: // Satellites
			if (list_size(world->satellites) > 0)
			{
				add_buf(buffer, "     [       World       ] [      Distance     ]\n\r");
				add_buf(buffer, "=================================================\n\r");

				int i = 0;
				WORLD_DATA *satellite;
				iterator_start(&it, world->satellites);
				while((satellite = (WORLD_DATA *)iterator_nextdata(&it)))
				{
					int len = 20 - strlen_no_colours(satellite->name);
					if (len < 0) len = 0;

					sprintf(buf, "%4d  %s%s   %18ldm\n\r", ++i,
						MXPCreateSend(ch->desc, formatf("worldshow %ld", satellite->vnum), satellite->name),
						formatf("%*.*s", len, len, ""),
						satellite->orbit.distance);
					add_buf(buffer, buf);
				}
				iterator_stop(&it);

				add_buf(buffer, "-------------------------------------------------\n\r");
			}
			else
				add_buf(buffer, "No Satellites Defined.\n\r");
			break;
		
		case 4: // Constellations (listing only, for details do `show constellation #`)
			if (list_size(world->constellations) > 0)
			{
				ITERATOR it;
				CONSTELLATION_DATA *con;

				add_buf(buffer, "     [        Name        ] [ Right Asc ] [ Declination ]\n\r");
				add_buf(buffer, "==========================================================\n\r");

				int i = 0;
				iterator_start(&it, world->constellations);
				while((con = (CONSTELLATION_DATA *)iterator_nextdata(&it)))
				{
					++i;
					int len = 20 - strlen_no_colours(con->name);
					if (len < 0) len = 0;

					sprintf(buf, "%-4d  %s%s    %9.2lf     %9.2lf %c\n\r", i,
						MXPCreateSend(ch->desc, formatf("show constellation %d", i), con->name),
						formatf("%-*.*s", len, len, ""),
						0.01 * con->right_ascension,
						0.01 * abs(con->declination),
						((con->declination < 0) ? 'S' : ((con->declination > 0) ? 'N' : ' ')));
					add_buf(buffer, buf);
				}
				iterator_stop(&it);

				add_buf(buffer, "----------------------------------------------------------\n\r");

			}
			else
				add_buf(buffer, "No Constellations Defined.\n\r");
			break;
		}


		// TODO: Handle paging
		send_to_char(buffer->string, ch);

		free_buf(buffer);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "?"))
	{
		send_to_char("Syntax:  show                     - show base world info\n\r", ch);
		send_to_char("         show ?                   - show this help\n\r", ch);
		send_to_char("         show constellation <#>   - show more details about constellation\n\r", ch);
		return false;
	}

	if (!str_prefix(arg, "constellation"))
	{
		int index;
		if (!is_number(argument) || (index = atoi(argument)) < 1 || index > list_size(world->constellations))
		{
			send_to_char(formatf("Please specify a number from 1 to %d.\n\r", list_size(world->constellations)), ch);
			return false;
		}

		CONSTELLATION_DATA *con = (CONSTELLATION_DATA *)list_nthdata(world->constellations, index);

		buffer = new_buf();

		add_buf(buffer, formatf("Constellation %d: {W%s{x\n\r", index, con->name));

		add_buf(buffer, "Description:\n\r");
		add_buf(buffer, string_indent(con->description, 3));
		add_buf(buffer, "{x\n\r");

		olc_buffer_show_string(ch, buffer, formatf("%.1f degree%s", (0.1 * con->right_ascension), ((con->right_ascension == 10)?"":"s")), "ra", "Right Ascension:", 20, "xDW");
		if (con->declination > 0)
			olc_buffer_show_string(ch, buffer, formatf("%.1f degree%s N", (0.1 * con->declination), ((con->declination == 10)?"":"s")), "declination", "Declination:", 20, "xDW");
		else if (con->declination < 0)
			olc_buffer_show_string(ch, buffer, formatf("%.1f degree%s S", (-0.1 * con->declination), ((con->declination == -10)?"":"s")), "declination", "Declination:", 20, "xDW");
		else
			olc_buffer_show_string(ch, buffer, formatf("%.1f degree%s N", (0.1 * con->declination), ((con->declination == 10)?"":"s")), "declination", "Declination:", 20, "xDW");

		add_buf(buffer, "Builder Comments:\n\r");
		add_buf(buffer, string_indent(con->comments, 3));
		add_buf(buffer, "{x\n\r");



		add_buf(buffer, "ASCII Art:\n\r");
		add_buf(buffer, string_indent(con->ascii, 3));
		add_buf(buffer, "{x\n\r");

		// TODO: Handle paging
		send_to_char(buffer->string, ch);

		free_buf(buffer);
	}

	worldedit_show(ch, "?");
	return false;
}

WORLDEDIT( worldedit_tilt )
{
	WORLD_DATA *world;
	EDIT_WORLD(ch, world);

	double tilt;
	if (!is_double(argument) || (tilt = atof(argument)) < 0.0 || tilt > 179.9)
	{
		send_to_char("Please provide a number from 0.0 to 179.9\n\r", ch);
		return false;
	}

	world->tilt = (int16_t)(10 * tilt);
	send_to_char("Axial tilt changed.\n\r", ch);
	return true;
}

WORLDEDIT( worldedit_type )
{
	WORLD_DATA *world;
	EDIT_WORLD(ch, world);

	int16_t type;
	if ((type = stat_lookup(argument, world_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Invalid world type.  Use '? world' for valid types.\n\r", ch);
		show_flag_cmds(ch, world_types);
		return false;
	}

	world->type = type;
	send_to_char("World type changed.\n\r", ch);
	return true;
}
