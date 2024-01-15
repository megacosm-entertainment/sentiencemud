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

RACE_DATA *get_race_data(const char *name)
{
	ITERATOR it;
	RACE_DATA *race;
	
	iterator_start(&it, race_list);
	while((race = (RACE_DATA *)iterator_nextdata(&it)))
	{
		if (!str_prefix(name, race->name))
			break;
	}
	iterator_stop(&it);

	return race;
}

RACE_DATA *get_race_uid(const int16_t uid)
{
	ITERATOR it;
	RACE_DATA *race;
	
	iterator_start(&it, race_list);
	while((race = (RACE_DATA *)iterator_nextdata(&it)))
	{
		if (race->uid == uid)
			break;
	}
	iterator_stop(&it);

	return race;
}

void insert_race(RACE_DATA *race)
{
	ITERATOR it;
	RACE_DATA *r;
	iterator_start(&it, race_list);
	while((r = (RACE_DATA *)iterator_nextdata(&it)))
	{
		int cmp = str_cmp(race->name, r->name);
		if (cmp < 0)
		{
			iterator_insert_before(&it, race);
			break;
		}
	}
	iterator_stop(&it);

	if (!r)
	{
		list_appendlink(race_list, race);
	}
}

RACE_DATA **gr_from_name(const char *name)
{
	for(int i = 0; gr_table[i].name; i++)
	{
		if (!str_prefix(name, gr_table[i].name))
			return gr_table[i].gr;
	}
	
	return NULL;
}

char *gr_to_name(RACE_DATA **gr)
{
	for(int i = 0; gr_table[i].name; i++)
	{
		if (gr_table[i].gr == gr)
			return gr_table[i].name;
	}
	
	return NULL;
}


void save_race(FILE *fp, RACE_DATA *race)
{
	if (race->playable)
		fprintf(fp, "#PCRACE %d\n", race->uid);
	else
		fprintf(fp, "#RACE %d\n", race->uid);

	fprintf(fp, "Name %s~\n", fix_string(race->name));
	fprintf(fp, "Description %s~\n", fix_string(race->description));
	fprintf(fp, "Comments %s~\n", fix_string(race->comments));

	if (race->pgr)
		fprintf(fp, "GR %s~\n", gr_to_name(race->pgr));

	fprintf(fp, "Flags %s\n", print_flags(race->flags));

	fprintf(fp, "Act %s %s\n", print_flags(race->act[0]), print_flags(race->act[1]));
	fprintf(fp, "Aff %s %s\n", print_flags(race->aff[0]), print_flags(race->aff[1]));
	fprintf(fp, "Off %s\n", print_flags(race->off));
	fprintf(fp, "Imm %s\n", print_flags(race->imm));
	fprintf(fp, "Res %s\n", print_flags(race->res));
	fprintf(fp, "Vuln %s\n", print_flags(race->vuln));
	fprintf(fp, "Form %s\n", print_flags(race->form));
	fprintf(fp, "Parts %s\n", print_flags(race->parts));

	if (race->playable)
	{
		if (IS_VALID(race->premort))
			fprintf(fp, "GRemort %s~\n", race->premort->name);

		if (race->starting)
			fprintf(fp, "Starting\n");

		if (race->remort)
			fprintf(fp, "Remort\n");

		fprintf(fp, "Who %s~\n", fix_string(race->who));

		ITERATOR sit;
		SKILL_DATA *skill;
		iterator_start(&sit, race->skills);
		while((skill = (SKILL_DATA *)iterator_nextdata(&sit)))
		{
			fprintf(fp, "Skill %s~\n", skill->name);
		}
		iterator_stop(&sit);

		for(int i = 0; i < MAX_STATS; i++)
		{
			fprintf(fp, "Stat %s~ %d\n", flag_string(stat_types, i), race->stats[i]);
			fprintf(fp, "MaxStat %s~ %d\n", flag_string(stat_types, i), race->max_stats[i]);
		}

		for(int i = 0; i < 3; i++)
		{
			fprintf(fp, "MaxVital %s~ %d\n", flag_string(vital_types, i), race->max_vitals[i]);
		}

		fprintf(fp, "Size %d %d\n", race->min_size, race->max_size);
		fprintf(fp, "DefaultAlignment %d\n", race->default_alignment);
	}

	fprintf(fp, "#-RACE\n");
}

void save_races()
{
	FILE *fp;

	log_string("save_races: saving " RACES_FILE);
	if ((fp = fopen(RACES_FILE, "w")) == NULL)
	{
		bug("save_races: fopen", 0);
		perror(RACES_FILE);
		return;
	}

	ITERATOR it;
	RACE_DATA *race;
	iterator_start(&it, race_list);
	while((race = (RACE_DATA *)iterator_nextdata(&it)))
	{
		save_race(fp, race);
	}	
	iterator_stop(&it);

	fprintf(fp, "End\n");
	fclose(fp);

}

RACE_DATA *load_race(FILE *fp, bool playable)
{
	RACE_DATA *data = new_race_data();

	char buf[MSL];
	char *word;
	bool fMatch;

	data->uid = fread_number(fp);
	data->playable = playable;
	data->load_remort = NULL;

    while (str_cmp((word = fread_word(fp)), "#-RACE"))
	{
		fMatch = false;
		switch(word[0])
		{
			case 'A':
				if (!str_cmp(word, "Act"))
				{
					data->act[0] = fread_flag(fp);
					data->act[1] = fread_flag(fp);
					fMatch = true;
					break;
				}
				if (!str_cmp(word, "Aff"))
				{
					data->aff[0] = fread_flag(fp);
					data->aff[1] = fread_flag(fp);
					fMatch = true;
					break;
				}
				break;

			case 'C':
				KEYS("Comments", data->comments, fread_string(fp));
				break;

			case 'D':
				KEYS("Description", data->description, fread_string(fp));
				break;

			case 'F':
				KEY("Flags", data->flags, fread_flag(fp));
				KEY("Form", data->form, fread_flag(fp));
				break;

			case 'G':
				KEY("GR", data->pgr, gr_from_name(fread_string(fp)));
				break;

			case 'I':
				KEY("Imm", data->imm, fread_flag(fp));
				break;

			case 'N':
				KEYS("Name", data->name, fread_string(fp));
				break;

			case 'O':
				KEY("Off", data->off, fread_flag(fp));
				break;

			case 'P':
				KEY("Parts", data->parts, fread_flag(fp));
				break;

			case 'R':
				KEY("Res", data->res, fread_flag(fp));
				break;

			case 'V':
				KEY("Vuln", data->vuln, fread_flag(fp));
				break;
		}

		if (!fMatch && data->playable)
		{
			switch(word[0])
			{
				case 'D':
					KEY("DefaultAlignment", data->default_alignment, fread_number(fp));
					break;

				case 'G':
					KEY("GRemort", data->load_remort, fread_string(fp));
					break;

				case 'M':
					if (!str_cmp(word, "MaxStat"))
					{
						char *name = fread_string(fp);
						int value = fread_number(fp);

						int stat = stat_lookup(name, stat_types, NO_FLAG);
						if (stat != NO_FLAG)
						{
							data->max_stats[stat] = value;
						}

						fMatch = true;
						break;
					}
					if (!str_cmp(word, "MaxVital"))
					{
						char *name = fread_string(fp);
						int value = fread_number(fp);

						int vital = stat_lookup(name, vital_types, NO_FLAG);
						if (vital != NO_FLAG)
						{
							data->max_vitals[vital] = value;
						}

						fMatch = true;
						break;
					}
					break;

				case 'R':
					KEY("Remort", data->remort, true);
					break;

				case 'S':
					if (!str_cmp(word, "Size"))
					{
						int min_size = fread_number(fp);
						int max_size = fread_number(fp);

						data->min_size = min_size;
						data->max_size = max_size;

						fMatch = true;
						break;
					}

					if (!str_cmp(word, "Skill"))
					{
						SKILL_DATA *skill = get_skill_data(fread_string(fp));

						if (IS_VALID(skill))
						{
							list_appendlink(data->skills, skill);
						}
						fMatch = true;
						break;
					}

					KEY("Starting", data->starting, true);

					if (!str_cmp(word, "Stat"))
					{
						char *name = fread_string(fp);
						int value = fread_number(fp);

						int stat = stat_lookup(name, stat_types, NO_FLAG);
						if (stat != NO_FLAG)
						{
							data->stats[stat] = value;
						}

						fMatch = true;
						break;
					}
					break;

				case 'W':
					KEYS("Who", data->who, fread_string(fp));
					break;
			}
		}

		if (!fMatch)
		{
			sprintf(buf, "load_race: no match for word %s", word);
			bug(buf, 0);
		}
	}

	return data;
}

static void delete_race_data(void *ptr)
{
	free_race_data((RACE_DATA *)ptr);
}

bool load_races()
{
	FILE *fp;
	char buf[MSL];
	char *word;
	bool fMatch;
	RACE_DATA *race;
	top_race_uid = 0;

	log_string("load_races: creating race_list");
	race_list = list_createx(false, NULL, delete_race_data);
	if (!IS_VALID(race_list))
	{
		log_string("race_list was not created.");
		return false;
	}

	log_string("load_races: loading " RACES_FILE);
	if ((fp = fopen(RACES_FILE, "r")) == NULL)
	{
		log_string("load_races: '" RACES_FILE "' file not found.  Bootstrapping races.");

		for (int i = 0; __race_table[i].name; i++)
		{
			if (__race_table[i].pgrn)
				*__race_table[i].pgrn = i;
		}
		for (int i = 0; __pc_race_table[i].name; i++)
		{
			if (__pc_race_table[i].pgrn)
				*__pc_race_table[i].pgrn = i;
		}

		// Skip the first entry ("none")
		for(int i = 1; __race_table[i].name; i++)
		{
			RACE_DATA *race = new_race_data();
			race->uid = ++top_race_uid;

			race->name = str_dup(__race_table[i].name);
			race->playable = __race_table[i].pc_race;
			race->pgr = gr_from_name(race->name);

			race->act[0] = __race_table[i].act;
			race->act[1] = __race_table[i].act2;
			race->aff[0] = __race_table[i].aff;
			race->aff[1] = __race_table[i].aff2;
			race->off = __race_table[i].off;
			race->imm = __race_table[i].imm;
			race->res = __race_table[i].res;
			race->vuln = __race_table[i].vuln;
			race->form = __race_table[i].form;
			race->parts = __race_table[i].parts;

			if (race->playable && __race_table[i].pgprn)
			{
				int16_t j = *(__race_table[i].pgprn);

				race->who = str_dup(__pc_race_table[j].who_name);

				for(int k = 0; k < 9; k++) if (!IS_NULLSTR(__pc_race_table[j].skills[k]))
				{
					SKILL_DATA *skill = get_skill_data(__pc_race_table[j].skills[k]);
					if (IS_VALID(skill))
						list_appendlink(race->skills, skill);
				}

				for(int k = 0; k < MAX_STATS; k++)
				{
					race->stats[k] = __pc_race_table[j].stats[k];
					race->max_stats[k] = __pc_race_table[j].max_stats[k];
				}

				for(int k = 0; k < 3; k++)
				{
					race->max_vitals[k] = __pc_race_table[j].max_vital_stats[k];
				}

				race->min_size = __pc_race_table[j].size;
				race->max_size = __pc_race_table[j].size;
				race->default_alignment = __pc_race_table[j].alignment;

				race->remort = __pc_race_table[j].remort;
			}

			insert_race(race);
		}

		// Link REMORT pointers
		for(int i = 1; __pc_race_table[i].name; i++)
		{
			RACE_DATA *race = get_race_data(__pc_race_table[i].name);

			if (__pc_race_table[i].prgrn && *(__pc_race_table[i].prgrn) != -1)
			{
				int j = *(__pc_race_table[i].prgrn);

				race->premort = get_race_data(__pc_race_table[j].name);
			}
		}
	}
	else
	{
		while(str_cmp((word = fread_word(fp)), "End"))
		{
			fMatch = false;

			switch(word[0])
			{
			case '#':
				if (!str_cmp(word, "#RACE"))
				{
					race = load_race(fp, false);
					if (race)
					{
						insert_race(race);

						if (race->uid > top_race_uid)
							top_race_uid = race->uid;
					}
					else
						log_string("Failed to load a race.");

					fMatch = true;
					break;
				}
				if (!str_cmp(word, "#PCRACE"))
				{
					race = load_race(fp, true);
					if (race)
					{
						insert_race(race);

						if (race->uid > top_race_uid)
							top_race_uid = race->uid;
					}
					else
						log_string("Failed to load a race.");

					fMatch = true;
					break;
				}
				break;
			}

			if (!fMatch) {
				sprintf(buf, "load_classes: no match for word %s", word);
				bug(buf, 0);
			}
		}

		bool save = false;

		for(int i = 0; gr_table[i].name; i++)
			if (gr_table[i].gr)
				*(gr_table[i].gr) = NULL;

		ITERATOR it;
		iterator_start(&it, race_list);
		while((race = (RACE_DATA *)iterator_nextdata(&it)))
		{
			if (race->pgr)
				*(race->pgr) = race;

			if (!race->uid)
			{
				race->uid = ++top_race_uid;
				save = true;
			}

			if (race->load_remort)
			{
				race->premort = get_race_data(race->load_remort);
				free_string(race->load_remort);
				race->load_remort = NULL;
			}
		}
		iterator_stop(&it);

		if (save)
			save_races();
	}

	return true;
}

struct racelist_params
{
	char name[MIL];
	bool playable;
	bool remort;

	bool has_name;
	bool has_playable;
	bool has_remort;
};

static bool __racelist_parse_params(CHAR_DATA *ch, char *argument, struct racelist_params *params)
{
	params->has_name = false;
	params->has_playable = false;
	params->has_remort = false;

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
		else if (!str_prefix(arg, "pc"))
		{
			params->playable = true;
			params->has_playable = true;
		}
		else if (!str_prefix(arg, "npc"))
		{
			if (params->has_remort)
			{
				if (params->remort)
					send_to_char("Combining {Wnpc{x with {Wremort{x filter.\n\r", ch);
				else
					send_to_char("Combining {Wnpc{x with {Wmort{x filter.\n\r", ch);
				return false;
			}

			params->playable = false;
			params->has_playable = true;
		}
		else if (!str_prefix(arg, "remort"))
		{
			if (params->has_playable && !params->playable)
			{
				send_to_char("Combining {Wremort{x with {Wnpc{x filter.\n\r", ch);
				return false;
			}

			params->playable = true;
			params->has_playable = true;
			params->remort = true;
			params->has_remort = true;
		}
		else if (!str_prefix(arg, "mort"))
		{
			if (params->has_playable && !params->playable)
			{
				send_to_char("Combining {Wmort{x with {Wnpc{x filter.\n\r", ch);
				return false;
			}

			params->playable = true;
			params->has_playable = true;
			params->remort = false;
			params->has_remort = true;
		}
		else
		{
			send_to_char("Invalid parameters.\n\r", ch);
			send_to_char("  name <name>        - Filter by name\n\r", ch);
			send_to_char("  npc                - Only show non-playable races\n\r", ch);
			send_to_char("  pc                 - Only show playable races\n\r", ch);
			send_to_char("  mort               - Only show mort races (playable only)\n\r", ch);
			send_to_char("  remort             - Only show remort races (playable only)\n\r", ch);
			return false;
		}
		
	}

	return true;
}

void do_racelist(CHAR_DATA *ch, char *argument)
{
	BUFFER *buffer = new_buf();
	char buf[MSL];

	struct racelist_params params;
	if (!__racelist_parse_params(ch, argument, &params))
		return;

	sprintf(buf, "%-4s %-20s %-9s %-7s\n\r",
		"#", "Name", "Playable?", "Remort?");
	add_buf(buffer, buf);
	sprintf(buf, "%-4s %-20s %-9s %-8s\n\r",
		"====", "====================", "=========", "=======");
	add_buf(buffer, buf);

	int i = 0;
	ITERATOR it;
	RACE_DATA *race;
    iterator_start(&it, race_list);
    while((race = (RACE_DATA *)iterator_nextdata(&it)))
    {
		if (params.has_name && str_prefix(params.name, race->name)) continue;
		if (params.has_playable && race->playable != params.playable) continue;
		if (params.has_remort && (!race->playable || race->remort != params.remort)) continue;

		char race_color;
		if (race->playable)
		{
			if (race->remort)
				race_color = 'Y';
			else
				race_color = 'G';
		}
		else
			race_color = 'x';

		sprintf(buf, "%-4d {%c%-20s %s %s\n\r", ++i,
			race_color, race->name,
			(race->playable ? "{W   YES   {x" : "{D    NO   {x"),
			(race->playable && race->remort ? "{W  YES  {x" : "{D   NO  {x"));
		add_buf(buffer, buf);
    }
    iterator_stop(&it);

	sprintf(buf, "%-4s %-20s %-9s %-8s\n\r",
		"====", "====================", "=========", "========");
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


RACEEDIT( raceedit_show )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);
	BUFFER *buffer = new_buf();

	add_buf(buffer, formatf("%s Race: %s (%d)\n\r", (race->playable ? "Player" : "NPC"), race->name, race->uid));

	if (race->pgr)
		add_buf(buffer, formatf("GR: %s\n", gr_to_name(race->pgr)));
	else
		add_buf(buffer, "GR: {Dnone{x\n");

	add_buf(buffer, "Description:\n\r");
	add_buf(buffer, string_indent(race->description, 3));
	add_buf(buffer, "{x\n\r");

	add_buf(buffer, "Builder Comments:\n\r");
	add_buf(buffer, string_indent(race->comments, 3));
	add_buf(buffer, "{x\n\r");

	// TODO: Update when race flags are added
	add_buf(buffer, "Flags: none\n");

	add_buf(buffer, formatf("Act:           %s\n", bitmatrix_string(act_flagbank, race->act)));
	add_buf(buffer, formatf("Affected:      %s\n", bitmatrix_string(affect_flagbank, race->aff)));
	add_buf(buffer, formatf("Off:           %s\n", flag_string(off_flags, race->off)));
	add_buf(buffer, formatf("Immunity:      %s\n", flag_string(imm_flags, race->imm)));
	add_buf(buffer, formatf("Reistence:     %s\n", flag_string(imm_flags, race->res)));
	add_buf(buffer, formatf("Vulnerability: %s\n", flag_string(imm_flags, race->vuln)));
	add_buf(buffer, formatf("Form:          %s\n", flag_string(form_flags, race->form)));
	add_buf(buffer, formatf("Parts:         %s\n", flag_string(part_flags, race->parts)));

	if (race->playable)
	{
		add_buf(buffer, formatf("Starting:      %s{x\n", (race->starting ? "{WYES" : "{DNO")));
		add_buf(buffer, formatf("Remort:        %s{x\n", (race->remort ? "{WYES" : "{DNO")));

		if (IS_VALID(race->premort))
			add_buf(buffer, formatf("Remort Race:   %s\n", race->premort->name));
		else
			add_buf(buffer, "Remort Race:   {Dnone{x\n");

		add_buf(buffer, formatf("Who: %s{x\n\r", race->who));

		add_buf(buffer, "Stats:\n\r");
		for(int i = 0; i < MAX_STATS; i++)
		{
			add_buf(buffer, formatf(" - %-20s  %2d / %-2d\n\r", formatf("%s:", flag_string(stat_types, i)), race->stats[i], race->max_stats[i]));
		}

		add_buf(buffer, "Vitals:\n\r");
		for(int i = 0; i < 3; i++)
		{
			add_buf(buffer, formatf(" - %-10s  %d\n\r", formatf("%s:", flag_string(vital_types, i)), race->max_vitals[i]));
		}

		add_buf(buffer, formatf("Size: %s to %s\n\r", size_table[race->min_size].name, size_table[race->max_size].name));

		add_buf(buffer, formatf("Default Alignment: %d\n\r", race->default_alignment));

		if (list_size(race->skills) > 0)
		{
			add_buf(buffer, "Skills:\n\r");
			ITERATOR sit;
			SKILL_DATA *skill;
			iterator_start(&sit, race->skills);
			while((skill = (SKILL_DATA *)iterator_nextdata(&sit)))
			{
				add_buf(buffer, formatf(" - %s\n\r", skill->name));
			}
			iterator_stop(&sit);
			add_buf(buffer, "\n\r");
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

RACEEDIT( raceedit_create )
{
	RACE_DATA *race;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  raceedit create <pc|npc> <name>\n\r", ch);
		send_to_char("Please specify {Wpc{x or {Wnpc{x.\n\r", ch);
		return false;
	}

	bool playable;

	char arg[MIL];
	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "pc"))
	{
		playable = true;
	}
	else if (!str_prefix(arg, "npc"))
	{
		playable = false;
	}
	else
	{
		send_to_char("Syntax:  raceedit create <pc|npc> <name>\n\r", ch);
		send_to_char("Please specify {Wpc{x or {Wnpc{x.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  raceedit create <pc|npc> <name>\n\r", ch);
		send_to_char("Please provide a name.\n\r", ch);
		return false;
	}

	if ((race = get_race_data(argument)))
	{
		send_to_char("That name is already in use.\n\r", ch);
		return false;
	}

	race = new_race_data();
	race->playable = playable;
	smash_tilde(argument);
	race->name = str_dup(argument);
	insert_race(race);
	save_races();

	olc_set_editor(ch, ED_RACEEDIT, race);

	send_to_char("Race created.\n\r", ch);
	return false;
}

RACEEDIT( raceedit_name )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	smash_tilde(argument);

	if (argument[0] == '\0')
	{
		send_to_char("Please provide a name.\n\r", ch);
		return false;
	}

	RACE_DATA *other = get_race_data(argument);
	if (IS_VALID(other) && race != other)
	{
		send_to_char("That name is already in use.\n\r", ch);
		return false;
	}

	free_string(race->name);
	race->name = str_dup(argument);

	list_remlink(race_list, race, false);
	insert_race(race);

	send_to_char("Race Name changed.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_description )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	string_append(ch, &race->description);
	return true;
}

RACEEDIT( raceedit_comments )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	string_append(ch, &race->comments);
	return true;
}

RACEEDIT( raceedit_flags )
{
	send_to_char("Not Implemented Yet.\n\r", ch);
	return false;
}

RACEEDIT( raceedit_act )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long bits[2];
	if (!bitmatrix_lookup(argument, act_flagbank, bits))
	{
		send_to_char("Invalid act flags.  Use '? act' for valid flags.\n\r", ch);
		show_help(ch, "act");
		return false;
	}

	TOGGLE_BIT(race->act[0], bits[0]);
	TOGGLE_BIT(race->act[1], bits[1]);
	send_to_char("Race Act toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_aff )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long bits[2];
	if (!bitmatrix_lookup(argument, affect_flagbank, bits))
	{
		send_to_char("Invalid affect flags.  Use '? affect' for valid flags.\n\r", ch);
		show_help(ch, "affect");
		return false;
	}

	TOGGLE_BIT(race->aff[0], bits[0]);
	TOGGLE_BIT(race->aff[1], bits[1]);
	send_to_char("Race Affect toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_off )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(off_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid offensive flags.  Use '? off' for valid list.\n\r", ch);
		show_flag_cmds(ch, off_flags);
		return false;
	}

	TOGGLE_BIT(race->off, value);
	send_to_char("Race Offensive toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_imm )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(imm_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid imm flags.  Use '? imm' for valid list.\n\r", ch);
		show_flag_cmds(ch, imm_flags);
		return false;
	}

	TOGGLE_BIT(race->imm, value);
	send_to_char("Race Immunity toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_res )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(imm_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid res flags.  Use '? res' for valid list.\n\r", ch);
		show_flag_cmds(ch, imm_flags);
		return false;
	}

	TOGGLE_BIT(race->res, value);
	send_to_char("Race Resistence toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_vuln )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(imm_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid vuln flags.  Use '? vuln' for valid list.\n\r", ch);
		show_flag_cmds(ch, imm_flags);
		return false;
	}

	TOGGLE_BIT(race->vuln, value);
	send_to_char("Race Vulnerability toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_form )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(form_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid form flags.  Use '? form' for valid list.\n\r", ch);
		show_flag_cmds(ch, form_flags);
		return false;
	}

	TOGGLE_BIT(race->form, value);
	send_to_char("Race Form toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_parts )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	long value;
	if ((value = flag_value(part_flags, argument)) == NO_FLAG)
	{
		send_to_char("Invalid part flags.  Use '? parts' for valid list.\n\r", ch);
		show_flag_cmds(ch, part_flags);
		return false;
	}

	TOGGLE_BIT(race->parts, value);
	send_to_char("Race Parts toggled.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_gr )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  gr set <global race>\n\r", ch);
		send_to_char("         gr clear\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	if (!str_prefix(arg, "set"))
	{
		RACE_DATA **gr = gr_from_name(argument);

		if (!gr)
		{
			send_to_char("Invalid global race.  Use '? gr' for valid races.\n\r", ch);
			show_help(ch, "gr");
			return false;
		}

		if (*gr) (*gr)->pgr = NULL;

		race->pgr = gr;
		*gr = race;

		send_to_char("Race Global Reference set.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (!race->pgr)
		{
			send_to_char("Race has no global reference.\n\r", ch);
			return false;
		}

		*(race->pgr) = NULL;
		race->pgr = NULL;

		send_to_char("Race Global Reference cleared.\n\r", ch);
		return true;
	}

	raceedit_gr(ch, "");
	return false;
}

RACEEDIT( raceedit_starting )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (!race->playable)
	{
		send_to_char("May only modify player fields on playable races.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  starting <yes|no>\n\r", ch);
		return false;
	}

	bool starting;
	if (!str_prefix(argument, "yes"))
		starting = true;
	else if (!str_prefix(argument, "no"))
		starting = false;
	else
	{
		send_to_char("Please specify yes or no.\n\r", ch);
		return false;
	}

	if(starting && race->remort)
	{
		send_to_char("Unable to make a remort race as a starting race.\n\r", ch);
		return false;
	}

	race->starting = starting;
	send_to_char("Race Starting changed.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_remort )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (!race->playable)
	{
		send_to_char("May only modify remort fields on playable races.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		if (race->remort)
		{
			send_to_char("Syntax:  remort reset\n\r", ch);
		}
		else
		{
			send_to_char("Syntax:  remort assign <race name>\n\r", ch);
			send_to_char("         remort set\n\r", ch);
		}

		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	if (race->remort)
	{
		if (!str_prefix(arg, "reset"))
		{
			race->remort = false;

			// Find any playable race that remorts into this race and unlink it
			ITERATOR it;
			RACE_DATA *r;
			iterator_start(&it, race_list);
			while((r = (RACE_DATA *)iterator_nextdata(&it)))
			{
				if (r->playable && IS_VALID(r->premort) && r->premort == race)
				{
					r->premort = NULL;
				}
			}
			iterator_stop(&it);

			send_to_char("Race Remort Status reset.\n\r", ch);
			return true;
		}
	}
	else
	{
		if (!str_prefix(arg, "assign"))
		{
			if (race->remort)
			{
				send_to_char("Race is already remort.\n\r", ch);
				return false;
			}

			if (argument[0] == '\0')
			{
				send_to_char("Please specify a race name.\n\r", ch);
				return false;
			}

			RACE_DATA *remort = get_race_data(argument);
			if (!IS_VALID(remort))
			{
				send_to_char("No such race by that name.\n\r", ch);
				return false;
			}

			if (!remort->playable)
			{
				send_to_char("That is not a playable race.\n\r", ch);
				return false;
			}

			if (!remort->remort)
			{
				send_to_char("That is not a remort race.\n\r", ch);
				return false;
			}

			race->premort = remort;
			send_to_char("Race Remort Reference assigned.\n\r", ch);
			return true;
		}
		else if (!str_prefix(arg, "set"))
		{
			if(race->starting)
			{
				send_to_char("Unable to make a starting race as a remort race.\n\r", ch);
				return false;
			}

			// Unassign the remort linkage
			race->premort = NULL;
			race->remort = true;
			send_to_char("Race Remort Status set.\n\r", ch);
			return true;
		}
	}

	raceedit_remort(ch, "");
	return false;
}

RACEEDIT( raceedit_who )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	smash_tilde(argument);
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  who <string>\n\r", ch);
		return false;
	}

	if (strlen_no_colours(argument) > 6)
	{
		send_to_char("Who string cannot be longer than 6 plain characters.\n\r", ch);
		return false;
	}

	free_string(race->who);
	race->who = str_dup(argument);

	send_to_char("Race Who set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_stats )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  stats <stat> <starting value>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	int stat;
	if ((stat = stat_lookup(arg, stat_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  stats <stat> <starting value>\n\r", ch);
		send_to_char("Invalid stat.  Use '? stat' for valid types.\n\r", ch);
		show_flag_cmds(ch, stat_types);
		return false;
	}

	int value;
	if (!is_number(argument) || (value = atoi(argument)) < 1)
	{
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	race->stats[stat] = value;
	send_to_char("Race Starting Stat set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_maxstats )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  maxstats <stat> <maximum>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	int stat;
	if ((stat = stat_lookup(arg, stat_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  maxstats <stat> <maximum>\n\r", ch);
		send_to_char("Invalid stat.  Use '? stat' for valid types.\n\r", ch);
		show_flag_cmds(ch, stat_types);
		return false;
	}

	int value;
	if (!is_number(argument) || (value = atoi(argument)) < 1)
	{
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	race->max_stats[stat] = value;
	send_to_char("Race Max Stat set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_maxvitals )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  maxvitals <hp|mana|move> <maximum>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	int vital;
	if ((vital = stat_lookup(arg, vital_types, NO_FLAG)) == NO_FLAG)
	{
		send_to_char("Syntax:  maxvitals <hp|mana|move> <maximum>\n\r", ch);
		send_to_char("Please specify {Whp{x, {Wmana{x or {Wmove{x.\n\r", ch);
		return false;
	}

	int value;
	if (!is_number(argument) || (value = atoi(argument)) < 1)
	{
		send_to_char("Please specify a positive number.\n\r", ch);
		return false;
	}

	race->max_vitals[vital] = value;
	send_to_char("Race Max Vital set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_size )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (!race->playable)
	{
		send_to_char("Race must be playable.\n\r", ch);
		return false;
	}

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  size <minimum> <maximum>\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);

	int min_size;
	if ((min_size = flag_value(size_flags, arg)) == NO_FLAG)
	{
		send_to_char("Invalid size.  Use '? size' for valid list.\n\r", ch);
		show_flag_cmds(ch, size_flags);
		return false;
	}

	int max_size;
	if ((max_size = flag_value(size_flags, arg)) == NO_FLAG)
	{
		send_to_char("Invalid size.  Use '? size' for valid list.\n\r", ch);
		show_flag_cmds(ch, size_flags);
		return false;
	}

	if (min_size > max_size)
	{
		send_to_char("Minimum size is bigger than the maximum.\n\r", ch);
		return false;
	}

	race->min_size = min_size;
	race->max_size = max_size;
	send_to_char("Race Sizing set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_align )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (!race->playable)
	{
		send_to_char("Race must be playable.\n\r", ch);
		return false;
	}

	int alignment;
	if (!is_number(argument) || (alignment = atoi(argument)) < -1000 || alignment > 1000)
	{
		send_to_char("Please specify an alignment from -1000 to 1000.\n\r", ch);
		return false;
	}

	race->default_alignment = alignment;
	send_to_char("Race Default Alignment set.\n\r", ch);
	return true;
}

RACEEDIT( raceedit_skills )
{
	RACE_DATA *race;

	EDIT_RACE(ch, race);

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skills add <skill name>\n\r", ch);
		send_to_char("         skills remove <skill name>\n\r", ch);
		send_to_char("         skills clear\n\r", ch);
		return false;
	}

	char arg[MIL];
	argument = one_argument(argument, arg);
	if (!str_prefix(arg, "add"))
	{
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skills add <skill name>\n\r", ch);
			return false;
		}

		SKILL_DATA *skill = get_skill_data(argument);
		if (!IS_VALID(skill))
		{
			send_to_char("No such skill by that name.\n\r", ch);
			return false;
		}

		if (list_size(skill->levels) > 0)
		{
			send_to_char("Skill may not have class restrictions on it.\n\r", ch);
			return false;
		}

		if (list_contains(race->skills, skill, NULL))
		{
			send_to_char("Race already has that skill.\n\r", ch);
			return false;
		}

		list_appendlink(race->skills, skill);
		send_to_char("Skill added to race.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "remove"))
	{
		if (list_size(race->skills) < 1)
		{
			send_to_char("Race has no skills.\n\r", ch);
			return false;
		}
		
		if (argument[0] == '\0')
		{
			send_to_char("Syntax:  skills remove <skill name>\n\r", ch);
			return false;
		}

		SKILL_DATA *skill = get_skill_data(argument);
		if (!IS_VALID(skill))
		{
			send_to_char("No such skill by that name.\n\r", ch);
			return false;
		}

		if (!list_contains(race->skills, skill, NULL))
		{
			send_to_char("Race does not have that skill.\n\r", ch);
			return false;
		}

		list_remlink(race->skills, skill, false);
		send_to_char("Skill removed from race.\n\r", ch);
		return true;
	}

	if (!str_prefix(arg, "clear"))
	{
		if (list_size(race->skills) > 0)
		{
			list_clear(race->skills);
			send_to_char("Race skills cleared.\n\r", ch);
			return true;
		}

		send_to_char("Race has no skills.\n\r", ch);
		return false;
	}

	raceedit_skills(ch, "");
	return false;
}