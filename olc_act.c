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

extern GLOBAL_DATA         gconfig;
/* Return true if area changed, false if not. */
AREA_DATA *get_area_data args ((long anum));
AREA_DATA *get_area_from_uid args ((long uid));


bool redit_blueprint_oncreate = false;

struct olc_help_type
{
    char *command;
	int structure_type;
    const void *structure;
    char *desc;
};

#define STRUCT_FLAGS	0
#define STRUCT_FLAGBANK	1
#define STRUCT_TRIGGERS	2
#define STRUCT_SPEC		3
#define STRUCT_LIQUID	4
#define STRUCT_ATTACK	5
#define STRUCT_MATERIAL	6
#define STRUCT_SKILL	7


// This table contains help commands and a brief description of each.
const struct olc_help_type help_table[] =
/*
{
    {	"area",		area_flags,	 "Area attributes."		 },
    {	"room",		room_flags,	 "Room attributes."		 },
    {   "room2",	room2_flags,	 "Room2 attributes."		 },
    {	"sector",	sector_flags,	 "Sector types, terrain."	 },
    {	"exit",		exit_flags,	 "Exit types."			 },
    {	"portal_exit",		portal_exit_flags,	 "Exit (Portal) types."			 },
    {	"lock",		lock_flags,	 "Lock state types."			 },
    {	"type",		type_flags,	 "Types of objects."		 },
    {	"areawho",	area_who_titles, "Type of area for who."	 },
    {	"placetype",	place_flags,	 "Where is the town/city etc."	 },
    {	"extra",	extra_flags,	 "Object attributes."		 },
    {	"extra2",	extra2_flags,	 "Object attributes 2."		 },
    {	"extra3",	extra3_flags,	 "Object attributes 3."		 },
    {	"extra4",	extra4_flags,	 "Object attributes 4."		 },
    {	"wear",		wear_flags,	 "Where to wear object."	 },
    {	"spec",		spec_table,	 "Available special programs." 	 },
    {	"sex",		sex_flags,	 "Sexes."			 },
    {	"act",		act_flags,	 "Mobile attributes."		 },
    {	"act2",		act2_flags,	 "Mobile attributes."		 },
    {	"affect",	affect_flags,	 "Mobile affects."		 },
    {   "affect2",	affect2_flags,   "Mobile affects."		 },
    {	"wear-loc",	wear_loc_flags,	 "Where mobile wears object."	 },
    {	"spells",	skill_table,	 "Names of current spells." 	 },
    {	"container",	container_flags, "Container status."		 },
    {	"armour",	ac_type,	 "Ac for different attacks."	 },
    {   "apply",	apply_flags,	 "Apply flags"			 },
    {	"form",		form_flags,	 "Mobile body form."	         },
    {	"part",		part_flags,	 "Mobile body parts."		 },
    {	"imm",		imm_flags,	 "Mobile immunity."		 },
    {	"res",		res_flags,	 "Mobile resistance."	         },
    {	"vuln",		vuln_flags,	 "Mobile vulnerability."	 },
    {	"off",		off_flags,	 "Mobile offensive behaviour."	 },
    {	"size",		size_flags,	 "Mobile size."			 },
    {   "position",     position_flags,  "Mobile positions."             },
    {   "wclass",       weapon_class,    "Weapon class."                 },
    {   "wtype",        weapon_type2,    "Special weapon type."          },
    {	"portal",	portal_flags,	 "Portal types."		 },
    {	"furniture",	furniture_flags, "Furniture types."		 },
    {   "liquid",	liq_table,	 "Liquid types."		 },
    {	"apptype",	apply_types,	 "Apply types."			 },
    {	"weapon",	attack_table,	 "Weapon types."		 },
    {   "ranged",       ranged_weapon_class, "Ranged weapon types."      },
    {   "material",	material_table,  "Object materials."		 },
    {	"mprog",	trigger_table,	 "MobProgram types."		 },
    {	"oprog",	trigger_table,	 "ObjProgram types."		 },
    {	"rprog",	trigger_table,	 "RoomProgram types."		 },
    {	"tprog",	trigger_table,	 "TokenProgram types."		 },
    {	"aprog",	trigger_table,	 "AreaProgram types."		 },
    {	"iprog",	trigger_table,	 "InstanceProgram types."		 },
    {	"dprog",	trigger_table,	 "DungeonProgram types."		 },
    {   "condition",    room_condition_flags, "Room Condition types."    },
    {   "tokenflags",   token_flags,	 "Token flags."			 },
    {	"projectflags",	project_flags,	 "Project flags."		 },
    {	"immortalflags",immortal_flags,	 "Immortal duties."		 },
    {   "scriptflags",  script_flags,	 "Script Flags {D({Wrestricted{D){x."    },
    {	"corpsetypes",	corpse_types,	 "Corpse types."		},
    {	"catalyst",	catalyst_types,	 "Catalyst types."		},
    {	"spell_targets",	spell_target_types,	"Spell Target Types."	},
    {	"song_targets",	song_target_types,	"Song Target Types."	},
    {	"instruments",	instrument_types,	"Instrument Types"	},
    {	"shop",		shop_flags,	 "Shop flags"		 },
    {	"section_type",		blueprint_section_types,	 "Blueprint Section Types"		 },
    {	"section_flags",		blueprint_section_flags,	 "Blueprint Section Flags"		 },
    {	"instance",		instance_flags,	 "Instance Flags"		 },
    {	"dungeon",		dungeon_flags,	 "Dungeon Flags"		 },
    {	"ship",			ship_flags,	 "Ship flags"		 },
    {	"shipclass",		ship_class_types,	 "Ship class types"		 },
    {	NULL,		NULL,		 NULL				 }
*/
{
	{	"act",					STRUCT_FLAGBANK,	act_flagbank,				"Mobile	attributes."	},
	{	"affect",				STRUCT_FLAGBANK,	affect_flagbank,			"Mobile	affects."	},
	{	"apply",				STRUCT_FLAGS,		apply_flags,				"Apply flags"	},
	{	"apptype",				STRUCT_FLAGS,		apply_types,				"Apply types."	},
	{	"aprog",				STRUCT_TRIGGERS,	trigger_table,				"AreaProgram types."	},
	{	"area",					STRUCT_FLAGS,		area_flags,					"Area attributes."	},
	{	"areawho",				STRUCT_FLAGS,		area_who_titles,			"Type of area for who."	},
	{	"armour",				STRUCT_FLAGS,		ac_type,					"Ac for different attacks."	},
	{	"catalyst",				STRUCT_FLAGS,		catalyst_types,				"Catalyst types."	},
	{	"condition",			STRUCT_FLAGS,		room_condition_flags,		"Room Condition types."	},
	{	"container",			STRUCT_FLAGS,		container_flags,			"Container status."	},
	{	"corpsetypes",			STRUCT_FLAGS,		corpse_types,				"Corpse types."	},
	{	"damageclass",			STRUCT_FLAGS,		damage_classes,				"Types of damages."},
	{	"dprog",				STRUCT_TRIGGERS,	trigger_table,				"DungeonProgram types."	},
	{	"dungeon",				STRUCT_FLAGS,		dungeon_flags,				"Dungeon Flags"	},
	{	"exit",					STRUCT_FLAGS,		exit_flags,					"Exit types."	},
	{	"extra",				STRUCT_FLAGBANK,	extra_flagbank,				"Object attributes."	},
	{	"form",					STRUCT_FLAGS,		form_flags,					"Mobile body form."	},
	{	"furniture",			STRUCT_FLAGS,		furniture_flags,			"Furniture types."	},
	{	"imm",					STRUCT_FLAGS,		imm_flags,					"Mobile immunity."	},
	{	"immortalflags",		STRUCT_FLAGS,		immortal_flags,				"Immortal duties."	},
	{	"instance",				STRUCT_FLAGS,		instance_flags,				"Instance Flags"	},
	{	"instruments",			STRUCT_FLAGS,		instrument_types,			"Instrument Types"	},
	{	"iprog",				STRUCT_TRIGGERS,	trigger_table,				"InstanceProgram types."	},
	{	"liquid",				STRUCT_LIQUID,		liq_table,					"Liquid types."	},
	{	"lock",					STRUCT_FLAGS,		lock_flags,					"Lock state types."	},
	{	"material",				STRUCT_MATERIAL,	material_table,				"Object materials."	},
	{	"mprog",				STRUCT_TRIGGERS,	trigger_table,				"MobProgram types."	},
	{	"off",					STRUCT_FLAGS,		off_flags,					"Mobile offensive behaviour."	},
	{	"oprog",				STRUCT_TRIGGERS,	trigger_table,				"ObjProgram types."	},
	{	"part",					STRUCT_FLAGS,		part_flags,					"Mobile body parts."	},
	{	"placetype",			STRUCT_FLAGS,		place_flags,				"Where is the town/city etc."	},
	{	"portal",				STRUCT_FLAGS,		portal_flags,				"Portal types."	},
	{	"portal_exit",			STRUCT_FLAGS,		portal_exit_flags,			"Exit (Portal) types."	},
	{	"position",				STRUCT_FLAGS,		position_flags,				"Mobile positions."	},
	{	"projectflags",			STRUCT_FLAGS,		project_flags,				"Project flags."	},
	{	"ranged",				STRUCT_FLAGS,		ranged_weapon_class,		"Ranged	weapon types."	},
	{	"res",					STRUCT_FLAGS,		res_flags,					"Mobile resistance."	},
	{	"room",					STRUCT_FLAGBANK,	room_flagbank,				"Room attributes."	},
	{	"rprog",				STRUCT_TRIGGERS,	trigger_table,				"RoomProgram types."	},
	{	"scriptflags",			STRUCT_FLAGS,		script_flags,				"Script Flags {D({Wrestricted{D){x."	},
	{	"section_flags",		STRUCT_FLAGS,		blueprint_section_flags,	"Blueprint Section Flags"	},
	{	"section_type",			STRUCT_FLAGS,		blueprint_section_types,	"Blueprint Section Types"	},
	{	"sector",				STRUCT_FLAGS,		sector_flags,				"Sector types, terrain."	},
	{	"sex",					STRUCT_FLAGS,		sex_flags,					"Sexes."	},
	{	"ship",					STRUCT_FLAGS,		ship_flags,					"Ship flags"	},
	{	"shipclass",			STRUCT_FLAGS,		ship_class_types,			"Ship class types"	},
	{	"shop",					STRUCT_FLAGS,		shop_flags,					"Shop flags"	},
	{	"size",					STRUCT_FLAGS,		size_flags,					"Mobile size."	},
	{	"song_targets",			STRUCT_FLAGS,		song_target_types,			"Song Target Types."	},
	{	"spec",					STRUCT_SPEC,		spec_table,					"Available special programs. {D(DEPRECATED){x"	},
	{	"spell_targets",		STRUCT_FLAGS,		spell_target_types,			"Spell Target Types."	},
	{	"spells",				STRUCT_SKILL,		skill_table,				"Names of current spells."	},
	{	"tokenflags",			STRUCT_FLAGS,		token_flags,				"Token flags."	},
	{	"tprog",				STRUCT_TRIGGERS,	trigger_table,				"TokenProgram types."	},
	{	"type",					STRUCT_FLAGS,		type_flags,					"Types of objects."	},
	{	"vuln",					STRUCT_FLAGS,		vuln_flags,					"Mobile vulnerability."	},
	{	"wclass",				STRUCT_FLAGS,		weapon_class,				"Weapon class."	},
	{	"weapon",				STRUCT_ATTACK,		attack_table,				"Weapon types."	},
	{	"wear",					STRUCT_FLAGS,		wear_flags,					"Where to wear object."	},
	{	"wear-loc",				STRUCT_FLAGS,		wear_loc_flags,				"Where mobile wears object."	},
	{	"wtype",				STRUCT_FLAGS,		weapon_type2,				"Special weapon type."	},
	{	NULL,					STRUCT_FLAGS,		NULL,						NULL									}
};


// Displays settable flags and stats.
void show_flag_cmds(CHAR_DATA *ch, const struct flag_type *flag_table)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH ];
    int  flag;
    int  col;

    buf1[0] = '\0';
    col = 0;
    for (flag = 0; flag_table[flag].name != NULL; flag++)
    {
	if (flag_table[flag].settable)
	{
	    sprintf(buf, "%-19.18s", flag_table[flag].name);
	    strcat(buf1, buf);
	    if (++col % 4 == 0)
		strcat(buf1, "\n\r");
	}
    }

    if (col % 4 != 0)
	strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
    return;
}

void show_flagbank_cmds(CHAR_DATA *ch, const struct flag_type **flag_bank)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH ];
    int  col;

    buf1[0] = '\0';
    col = 0;
	for(int b = 0; flag_bank[b]; b++)
	{
		for (int f = 0; flag_bank[b][f].name != NULL; f++)
		{
			if (flag_bank[b][f].settable)
			{
				sprintf(buf, "%-19.18s", flag_bank[b][f].name);
				strcat(buf1, buf);
				if (++col % 4 == 0)
					strcat(buf1, "\n\r");
			}
		}
	}

    if (col % 4 != 0)
		strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
    return;
}

// Displays all skill functions.
void show_skill_cmds(CHAR_DATA *ch, int tar)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH*2 ];
    int  sn;
    int  col;

    buf1[0] = '\0';
    col = 0;
    for (sn = 0; sn < MAX_SKILL; sn++)
    {
	if (!skill_table[sn].name)
	    break;

	if (!str_cmp(skill_table[sn].name, "reserved")
	  || skill_table[sn].spell_fun == spell_null)
	    continue;

	if (tar == -1 || skill_table[sn].target == tar)
	{
	    sprintf(buf, "%-19.18s", skill_table[sn].name);
	    strcat(buf1, buf);
	    if (++col % 4 == 0)
		strcat(buf1, "\n\r");
	}
    }

    if (col % 4 != 0)
	strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
    return;
}


// Displays settable special functions.
void show_spec_cmds(CHAR_DATA *ch)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH ];
    int  spec;
    int  col;

    buf1[0] = '\0';
    col = 0;
    send_to_char("Preceed special functions with 'spec_'\n\r\n\r", ch);
    for (spec = 0; spec_table[spec].function != NULL; spec++)
    {
	sprintf(buf, "%-19.18s", &spec_table[spec].name[5]);
	strcat(buf1, buf);
	if (++col % 4 == 0)
	    strcat(buf1, "\n\r");
    }

    if (col % 4 != 0)
	strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
    return;
}


// Displays help for many tables used in OLC.
bool show_help(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char spell[MAX_INPUT_LENGTH];
    int cnt;

    argument = one_argument(argument, arg);
    one_argument(argument, spell);

    // Display syntax.
    if (arg[0] == '\0')
    {
	send_to_char("Syntax:  ? [command]\n\r\n\r", ch);
	send_to_char("[command]  [description]\n\r", ch);
	for (cnt = 0; help_table[cnt].command != NULL; cnt++)
	{
	    sprintf(buf, "%-10.10s -%s\n\r",
	        capitalize(help_table[cnt].command),
		help_table[cnt].desc);
	    send_to_char(buf, ch);
	}
	return false;
    }

    // Find the command, show changeable data.
/*
    for (cnt = 0; help_table[cnt].command != NULL; cnt++)
    {
        if ( arg[0] == help_table[cnt].command[0]
          && !str_prefix(arg, help_table[cnt].command))
	{
	    if (help_table[cnt].structure == spec_table)
	    {
		show_spec_cmds(ch);
		return false;
	    }
	    else
	    if (help_table[cnt].structure == liq_table)
	    {
	        show_liqlist(ch);
	        return false;
	    }
	    else
	    if (help_table[cnt].structure == attack_table)
	    {
	        show_damlist(ch);
	        return false;
	    }
	    else
	    if (help_table[cnt].structure == material_table)
	    {
		show_material_list(ch);
		return false;
	    }
	    else
	    if (help_table[cnt].structure == skill_table)
	    {
		if (spell[0] == '\0')
		{
		    send_to_char("Syntax:  ? spells "
		        "[ignore/attack/defend/self/object/all]\n\r", ch);
		    return false;
		}

		if (!str_prefix(spell, "all"))
		    show_skill_cmds(ch, -1);
		else if (!str_prefix(spell, "ignore"))
		    show_skill_cmds(ch, TAR_IGNORE);
		else if (!str_prefix(spell, "attack"))
		    show_skill_cmds(ch, TAR_CHAR_OFFENSIVE);
		else if (!str_prefix(spell, "defend"))
		    show_skill_cmds(ch, TAR_CHAR_DEFENSIVE);
		else if (!str_prefix(spell, "self"))
		    show_skill_cmds(ch, TAR_CHAR_SELF);
		else if (!str_prefix(spell, "object"))
		    show_skill_cmds(ch, TAR_OBJ_INV);
		else
		    send_to_char("Syntax:  ? spell "
		        "[ignore/attack/defend/self/object/all]\n\r", ch);

		return false;
	    }
	    else
	    if (help_table[cnt].structure == trigger_table)
	    {
			int i, n;

			if (!str_prefix(arg, "mprog"))
			{
				send_to_char("MobProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
				if (trigger_table[i].mob)
				{
					n++;
					sprintf(buf, "%-20s", trigger_table[i].name);
					send_to_char(buf, ch);
					if (!(n % 4))
					send_to_char("\n\r", ch);
				}
				}

				if (n % 4)
				send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "oprog"))
			{
				send_to_char("ObjProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
				if (trigger_table[i].obj)
				{
					n++;
					sprintf(buf, "%-20s", trigger_table[i].name);
					send_to_char(buf, ch);
					if (!(n % 4))
					send_to_char("\n\r", ch);
				}
				}

				if (n % 4)
				send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "rprog"))
			{
				send_to_char("RoomProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
				if (trigger_table[i].room)
				{
					n++;
					sprintf(buf, "%-20s", trigger_table[i].name);
					send_to_char(buf, ch);
					if (!(n % 4))
					send_to_char("\n\r", ch);
				}
				}

				if (n % 4)
				send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "tprog"))
			{
				send_to_char("TokenProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
					if (trigger_table[i].token)
					{
						n++;
						sprintf(buf, "%-20s", trigger_table[i].name);
						send_to_char(buf, ch);
						if (!(n % 4))
							send_to_char("\n\r", ch);
					}
				}

				if (n % 4)
					send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "aprog"))
			{
				send_to_char("AreaProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
					if (trigger_table[i].area)
					{
						n++;
						sprintf(buf, "%-20s", trigger_table[i].name);
						send_to_char(buf, ch);
						if (!(n % 4))
							send_to_char("\n\r", ch);
					}
				}

				if (n % 4)
					send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "iprog"))
			{
				send_to_char("InstanceProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
					if (trigger_table[i].instance)
					{
						n++;
						sprintf(buf, "%-20s", trigger_table[i].name);
						send_to_char(buf, ch);
						if (!(n % 4))
							send_to_char("\n\r", ch);
					}
				}

				if (n % 4)
					send_to_char("\n\r", ch);
			}
			else if (!str_prefix(arg, "dprog"))
			{
				send_to_char("DungeonProgram Triggers:\n\r", ch);
				for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
				{
					if (trigger_table[i].dungeon)
					{
						n++;
						sprintf(buf, "%-20s", trigger_table[i].name);
						send_to_char(buf, ch);
						if (!(n % 4))
							send_to_char("\n\r", ch);
					}
				}

				if (n % 4)
					send_to_char("\n\r", ch);
			}

			return false;
	    }
	    else
	    {
		show_flag_cmds(ch, help_table[cnt].structure);
		return false;
	    }
	}
    }
*/
    for (cnt = 0; help_table[cnt].command != NULL; cnt++)
    {
        if ( arg[0] == help_table[cnt].command[0]
          && !str_prefix(arg, help_table[cnt].command))
		{
			switch(help_table[cnt].structure_type)
			{
				case STRUCT_SPEC:
					show_spec_cmds(ch);
					break;

				case STRUCT_LIQUID:
					show_liqlist(ch);
					break;

				case STRUCT_ATTACK:
					show_damlist(ch);
					break;

				case STRUCT_MATERIAL:
					show_material_list(ch);
					return false;
				
				case STRUCT_SKILL:
					if (spell[0] == '\0')
					{
						send_to_char("Syntax:  ? spells [ignore/attack/defend/self/object/all]\n\r", ch);
						return false;
					}

					if (!str_prefix(spell, "all"))
						show_skill_cmds(ch, -1);
					else if (!str_prefix(spell, "ignore"))
						show_skill_cmds(ch, TAR_IGNORE);
					else if (!str_prefix(spell, "attack"))
						show_skill_cmds(ch, TAR_CHAR_OFFENSIVE);
					else if (!str_prefix(spell, "defend"))
						show_skill_cmds(ch, TAR_CHAR_DEFENSIVE);
					else if (!str_prefix(spell, "self"))
						show_skill_cmds(ch, TAR_CHAR_SELF);
					else if (!str_prefix(spell, "object"))
						show_skill_cmds(ch, TAR_OBJ_INV);
					else
						send_to_char("Syntax:  ? spell [ignore/attack/defend/self/object/all]\n\r", ch);

					break;
				case STRUCT_TRIGGERS:
	    			if (help_table[cnt].structure == trigger_table)
	    			{
						int i, n;

						if (!str_prefix(arg, "mprog"))
						{
							send_to_char("MobProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].mob)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "oprog"))
						{
							send_to_char("ObjProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].obj)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "rprog"))
						{
							send_to_char("RoomProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].room)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "tprog"))
						{
							send_to_char("TokenProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].token)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "aprog"))
						{
							send_to_char("AreaProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].area)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "iprog"))
						{
							send_to_char("InstanceProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].instance)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						else if (!str_prefix(arg, "dprog"))
						{
							send_to_char("DungeonProgram Triggers:\n\r", ch);
							for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
							{
								if (trigger_table[i].dungeon)
								{
									n++;
									sprintf(buf, "%-20s", trigger_table[i].name);
									send_to_char(buf, ch);
									if (!(n % 4))
										send_to_char("\n\r", ch);
								}
							}
							if (n % 4)
								send_to_char("\n\r", ch);
						}
						break;

				case STRUCT_FLAGS:
					show_flag_cmds(ch, help_table[cnt].structure);
					break;

				case STRUCT_FLAGBANK:
					show_flagbank_cmds(ch, (const struct flag_type **)help_table[cnt].structure);
					break;
				
				default:
					show_help(ch, "");
					return false;
			}
			return false;
		}
    }
	}

//    show_help(ch, "");
    return false;
}


// Show the list of object materials to a builder
void show_material_list(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH ];
    char buf1[MAX_STRING_LENGTH ];
    int i;
    int col;

    buf1[0] = '\0';
    col = 0;
    for (i = 0; material_table[i].name != NULL; i++)
    {
	sprintf(buf, "%-19.18s", material_table[i].name);
	strcat(buf1, buf);
	if (++col % 4 == 0)
	    strcat(buf1, "\n\r");
    }

    if (col % 4 != 0)
	strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
    return;
}


// Purpose:	Ensures the range spans only one area.
bool check_range(long lower, long upper)
{
    AREA_DATA *pArea;
    int cnt = 0;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
	/*
	 * lower < area < upper
	 */
        if ((lower <= pArea->min_vnum && pArea->min_vnum <= upper)
	||   (lower <= pArea->max_vnum && pArea->max_vnum <= upper))
	    ++cnt;

	if (cnt > 1)
	    return false;
    }
    return true;
}

bool edit_deltrigger(LLIST **list, int index)
{
	PROG_LIST *trigger;
	int slot;
	ITERATOR it;

	if(list) {
		for(slot = 0; slot < TRIGSLOT_MAX; slot++) {
			iterator_start(&it, list[slot]);
			while(( trigger = (PROG_LIST *)iterator_nextdata(&it) )) {
				if(!index--) {
					iterator_remcurrent(&it);
					break;
				}
			}
			iterator_stop(&it);

			if(trigger) {
				free_trigger(trigger);
				return true;
			}
		}
	}

	return false;
}


AREA_DATA *get_vnum_area(long vnum)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        if (vnum >= pArea->min_vnum
          && vnum <= pArea->max_vnum)
            return pArea;
    }

    return 0;
}


AEDIT(aedit_show)
{
    AREA_DATA *pArea;
    char buf  [MAX_STRING_LENGTH];
    ROOM_INDEX_DATA *recall;
//	ITERATOR it;
//	PROG_LIST *trigger;
	BUFFER *buffer;
	buffer = new_buf();

    EDIT_AREA(ch, pArea);
	sprintf(buf, "{X======== {W%s{X ========\n\r", pArea->name);
	add_buf(buffer, buf);

	sprintf(buf, "{WArea: {R[{X%5ld{R]{X %s {R({WID: {X%ld{R){X\n\r", pArea->anum, pArea->name, pArea->uid);
	add_buf(buffer, buf);

//    sprintf(buf, "Name:        [%5ld] %s\n\r", pArea->anum, pArea->name);
//    send_to_char(buf, ch);

	sprintf(buf, "\n\r{WSystem Infomation:{X\n\r");
	add_buf(buffer, buf);

    sprintf(buf, "{WFile:        {R[{X%s{R]{X\n\r", pArea->file_name);
    add_buf(buffer, buf);

    sprintf(buf, "{WAge:         {R[{X%d{R]{X\n\r",	pArea->age);
    add_buf(buffer, buf);

    sprintf(buf, "{WRepop:       {R[{X%d minutes{R]{X\n\r", pArea->repop);
    add_buf(buffer, buf);

    sprintf(buf, "{WPlayers:     {R[{X%d{R]{X\n\r", pArea->nplayer);
    add_buf(buffer, buf);

	sprintf(buf, "{WCredits:     {R[{X%s{R]{X\n\r", pArea->credits);
    add_buf(buffer, buf);

    sprintf(buf, "{WFlags:       {R[{X%s{R]{X\n\r",
		   flag_string(area_flags, pArea->area_flags));
    add_buf(buffer, buf);

    sprintf(buf, "{WOpen:        {R[{X%s{R]{X\n\r", pArea->open ? "Yes" : "No");
    add_buf(buffer, buf);

//
// OLC Data
//

	sprintf(buf, "\n\r{WOLC Info:{X\n\r");
	add_buf(buffer, buf);

    sprintf(buf, "{WVnums:       {R[{X%ld-%ld{R]{X\n\r", pArea->min_vnum, pArea->max_vnum);
    add_buf(buffer, buf);

	sprintf(buf, "{WRepop:       {R[{X%d minutes{R]{X\n\r", pArea->repop);
    add_buf(buffer, buf);

	sprintf(buf, "{WSecurity:    {R[{X%d{R]{X\n\r", pArea->security);
    add_buf(buffer, buf);

	sprintf(buf, "{WBuilders:    {R[{X%s{R]{X\n\r", pArea->builders);
    add_buf(buffer, buf);

	sprintf(buf, "{WSuggested Levels:  {R[{X%d-%d{R]{X\n\r", pArea->min_level, pArea->max_level);
	add_buf(buffer, buf);

//
// Room Data
//

	sprintf(buf, "\n\r{WLocation Information:{X\n\r");
	add_buf(buffer, buf);

	if(pArea->recall.wuid) {
		WILDS_DATA *wilds = get_wilds_from_uid(NULL,pArea->recall.wuid);
		if(wilds)
			sprintf(buf, "{WRecall:      Wilds {X%s {R[{X%lu{R]{X} at {R<{X%lu,%lu,%lu{R>{X\n\r", wilds->name, pArea->recall.wuid,
				pArea->recall.id[0],pArea->recall.id[1],pArea->recall.id[2]);
		else
			sprintf(buf, "{WRecall:      Wilds {X??? {R[{X%lu{R]{X\n\r", pArea->recall.wuid);
	} else if(pArea->recall.id[0] > 0 && (recall = get_room_index(pArea->recall.id[0]))) {
			sprintf(buf, "{WRecall:      Room {R[{X%5ld{R]{X {X%s\n\r", pArea->recall.id[0], recall->name);
	} else
			sprintf(buf, "{WRecall:      {R[{X%lu{R]{X none\n\r", pArea->recall.id[0]);
	add_buf(buffer, buf);

    sprintf(buf, "{WAreaWho:     {R[{X%s{R] [{X%s{R]{X\n\r", flag_string(area_who_titles, pArea->area_who), flag_string(area_who_display, pArea->area_who));
    add_buf(buffer, buf);

    sprintf(buf, "{WPlaceType:   {R[{X%s{R]{X\n\r",
	    flag_string(place_flags, pArea->place_flags));
    add_buf(buffer, buf);

    sprintf(buf, "{WAirshipLand: {R[{X%s{R({X%ld{R)]{X\n\r", get_room_index(pArea->airship_land_spot) == NULL ? "{XNone" :
        get_room_index(pArea->airship_land_spot)->name, pArea->airship_land_spot);
    add_buf(buffer, buf);



	sprintf(buf, "\n\r{WWilderness Map Locations:{X\n\r");
	add_buf(buffer, buf);

    if( pArea->wilds_uid > 0 )
    {
		WILDS_DATA *pWilds = get_wilds_from_uid(NULL, pArea->wilds_uid);
    	sprintf(buf, "{WWilderness:     {R[{X%ld{R]{X %s\n\r", pArea->wilds_uid, pWilds?pWilds->name:"(null)");
	    add_buf(buffer, buf);
	}
	else
	{
    	sprintf(buf, "{WWilderness:     {Xnone\n\r");
	    add_buf(buffer, buf);
	}

    sprintf(buf, "{WX,Y:            {R[{X%d, %d{R]{X\n\r", pArea->x, pArea->y);
    add_buf(buffer, buf);

    sprintf(buf, "{WLandX,LandY:    {R[{X%d, %d{R]{X\n\r", pArea->land_x, pArea->land_y);
    add_buf(buffer, buf);


    // Trade stuff. One trade center per area at most
    if (pArea->trade_list != NULL)
    {
        TRADE_ITEM *temp;
        sprintf(buf, "{WTrade Items available within this area:{X\n\r");
		add_buf(buffer, buf);
	 	sprintf(buf,"{MName               Obj_Vnum Rep.Time Rep.Amount  Max_Qty Min_Price Max_Price{x\n\r");
		add_buf(buffer,buf);
        temp = pArea->trade_list;

        while(temp != NULL)
	{
	    sprintf(buf, "%-18s %-10ld %-10ld %-10ld %-10ld %-6ld %ld\n\r", trade_table[temp->trade_type].name, temp->obj_vnum, temp->replenish_time, temp->replenish_amount, temp->max_qty, temp->min_price, temp->max_price);
	    add_buf(buffer, buf);
            temp = temp->next;
	}

    }


	sprintf(buf, "\n\r{WDescription:{X\n\r%s\n\r", pArea->description);
	add_buf(buffer, buf);

	sprintf(buf, "\n\r{WPlayer Notes:{X\n\r%s\n\r", pArea->notes);
	add_buf(buffer, buf);

	sprintf(buf,"\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pArea->comments);
	add_buf(buffer, buf);


	if (pArea->progs->progs)
		olc_show_progs(buffer, pArea->progs->progs, PRG_APROG, "AreaProg Vnum");

	if (pArea->index_vars)
		olc_show_index_vars(buffer, pArea->index_vars);
	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);

    return false;
}


AEDIT(aedit_flags)
{
    AREA_DATA *area;
    int value;

    EDIT_AREA(ch, area);

    if ((value = flag_value(area_flags, argument)) != NO_FLAG)
    {
	TOGGLE_BIT(area->area_flags, value);

	send_to_char("Flag toggled.\n\r", ch);
	return true;
    }
    else
    {
	send_to_char("No such flag.\n\r", ch);
	return false;
    }

    return false;
}


AEDIT(aedit_x)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
	send_to_char("Syntax:  x [#x coord on map]\n\r", ch);
	return false;
    }

    pArea->x = atoi(argument);
    send_to_char("X Coordinate of Area set.\n\r", ch);

    return true;
}


AEDIT(aedit_y)
{
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
	send_to_char("Syntax:  y [#y coord on map]\n\r", ch);
	return false;
    }

    pArea->y = atoi(argument);
    send_to_char("Y Coordinate of Area set.\n\r", ch);

    return true;
}


AEDIT(aedit_land_x)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
	send_to_char("Syntax:  landx [#x coord on map]\n\r", ch);
	return false;
    }

    pArea->land_x = atoi(argument);
    send_to_char("X Coordinate set.\n\r", ch);

    return true;
}


AEDIT(aedit_land_y)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
	send_to_char("Syntax:  landy [#y coord on map]\n\r", ch);
	return false;
    }

    pArea->land_y = atoi(argument);
    send_to_char("Y Coordinate set.\n\r", ch);

    return true;
}

AEDIT(aedit_wilds)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
		send_to_char("Syntax:  wilds [map uid]\n\r", ch);
		return false;
    }

    long wuid = atol(argument);

    if( !get_wilds_from_uid(NULL, wuid) )
    {
		send_to_char("Invalid wilds map.\n\r", ch);
		return false;
	}

    pArea->wilds_uid = wuid;
    send_to_char("Wilderness Map UID set set.\n\r", ch);

    return true;
}


AEDIT(aedit_airshipland)
{
    AREA_DATA *pArea;
    char buf[MSL];

    EDIT_AREA(ch, pArea);

    if (!is_number(argument))
    {
	send_to_char("Syntax:  airshipland [vnum]\n\r", ch);
	return false;
    }

    if (get_room_index(atol(argument)) == NULL) {
	send_to_char("That room doesn't exist.\n\r", ch);
	return false;
    }

    pArea->airship_land_spot = atol(argument);
    sprintf(buf, "Set airship land spot of %s to %ld - %s\n\r",
        pArea->name, atol(argument), get_room_index(atol(argument))->name);
    send_to_char(buf, ch);
    return true;
}

AEDIT( aedit_add_trade )
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *pObj;

    char arg1[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    char arg3[MAX_STRING_LENGTH];
    char arg4[MAX_STRING_LENGTH];
    char arg5[MAX_STRING_LENGTH];
    char arg6[MAX_STRING_LENGTH];

    long replenish_time;
    long replenish_amount;
    long max_qty;
    long min_price;
    long max_price;
    long obj_vnum;

    EDIT_AREA(ch, pArea);

    argument = one_argument( argument, arg1);
    argument = one_argument( argument, arg2);
    argument = one_argument( argument, arg3);
    argument = one_argument( argument, arg4);
    argument = one_argument( argument, arg5);
    argument = one_argument( argument, arg6);

    if ( arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0' ||
			arg5[0] == '\0' || arg6[0] == '\0' )
    {
	send_to_char("addtrade obj_vnum replenish_time replenish_amount max_qty min_price max_price\n\r", ch);
	return false;
    }

	obj_vnum = atoi( arg1 );
	replenish_time = atoi( arg2 );
	replenish_amount = atoi( arg3 );
	max_qty = atoi( arg4 );
	min_price = atoi( arg5 );
	max_price = atoi( arg6 );

	if ( ( pObj = get_obj_index( obj_vnum ) ) == NULL )
	{
	    send_to_char( "That object does not exist!\n\r", ch );
	    return false;
	}

	if ( pObj->value[0] == TRADE_NONE )
	{
		send_to_char( "That is not a valid trade item.\n\r", ch );
		return false;
	}

    new_trade_item(pArea, pObj->value[0], replenish_time, replenish_amount, max_qty, min_price, max_price, obj_vnum);
    send_to_char("Trade item added.\n\r", ch);

    return true;
}


AEDIT( aedit_set_trade)
{
    return false;
}


AEDIT( aedit_view_trade )
{
	char buf[MAX_STRING_LENGTH];
    AREA_DATA *pTArea;
    TRADE_ITEM *pItem;
	int i = 0;

    char arg[MAX_STRING_LENGTH];

    argument = one_argument( argument, arg );

	if ( ( arg[0] == '\0' ) || (( i = get_trade_item( arg )) == 0 ) )
	{
		send_to_char("viewtrade 'trade type'\n\r\n\rAvailable trade types are:\n", ch);

		while( trade_table[ i ].trade_type != TRADE_LAST )
		{
			send_to_char( trade_table[ i ].name, ch );
			send_to_char( "\n\r", ch );
			i++;
		}

		return false;
	}

	sprintf( buf, "Showing all trade types across areas for {G%s{x:\n\r",
			trade_table[i].name );
	send_to_char( buf, ch );
	send_to_char( "\n\rArea            Type        Rep. Amt.   Rep Time.   Qty    MaxQty     Min    Max    Buy    Sell\n\r", ch );
	send_to_char( "----------------------------------------------------------------------------------------------------\n\r", ch );

    for ( pTArea = area_first; pTArea != NULL; pTArea = pTArea->next )
    {
		for ( pItem = pTArea->trade_list; pItem != NULL; pItem = pItem->next )
		{
			if ( pItem->trade_type == i )
			{
				sprintf( buf, "%-16s %-15s %-12ld %-9ld %-7ld %-7ld %-7ld %-7ld %-7ld %-7ld\n\r",
						pTArea->name, ( pItem->replenish_amount > 0 ) ? "{RSupplier{x" : "{GConsumer{x",
						pItem->replenish_amount,
						pItem->replenish_time,
						pItem->qty,
						pItem->max_qty,
						pItem->min_price,
						pItem->max_price,
						pItem->buy_price,
						pItem->sell_price );
				send_to_char( buf, ch );
			}
		}
	}

	return true;
}


AEDIT( aedit_remove_trade)
{
    AREA_DATA *pArea;
    char arg1[MAX_STRING_LENGTH];
    TRADE_ITEM *type;
    TRADE_ITEM *temp;

    EDIT_AREA(ch, pArea);

    argument = one_argument( argument, arg1);
    //sprintf(buf, "%s %s %s %s\n\r", arg1, arg2, arg3, arg4);
    //send_to_char(buf, ch);
    if ( arg1[0] == '\0')// || arg2[0] == '\0' || arg3[0] == '\0' || arg4[0] == '\0')
    {
	send_to_char("removetrade name\n\r", ch);
	return false;
    }

    type = find_trade_item(pArea, arg1);

    if (type == NULL)
    {
	send_to_char("That is not a valid trade item.\n\r", ch);
	return false;
    }


    if  ( type == pArea->trade_list )
    {
	pArea->trade_list = type->next;
    }
    else
	for ( temp = pArea->trade_list; temp; temp = temp->next )
	{
	    if ( temp->next == type )
	    {
		temp->next = type->next;
		break;
	    }
	}

    free_trade_item(type);
    send_to_char("Trade item removed.\n\r", ch);

    return false;
}

AEDIT(aedit_open)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:   open [Yes/No]\n\r", ch);
	return false;
    }

    if (!str_prefix(argument, "yes"))
    {
	pArea->open = true;
    }
    else
    {
	pArea->open = false;
    }

    send_to_char("Open set.\n\r", ch);
    return true;
}


AEDIT(aedit_create)
{
    AREA_DATA *pArea;

    pArea               =   new_area();
    pArea->uid = gconfig.next_area_uid++;
    gconfig_write();
    area_last->next     =   pArea;
    area_last           =   pArea;      /* Thanks, Walker. */
    ch->desc->pEdit     =   (void *)pArea;

    SET_BIT(pArea->area_flags, AREA_ADDED);
    send_to_char("Area Created.\n\r", ch);
    return false;
}


AEDIT(aedit_name)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax:   name [$name]\n\r", ch);
        return false;
    }

    free_string(pArea->name);
    pArea->name = str_dup(argument);

    send_to_char("Name set.\n\r", ch);
    return true;
}

AEDIT(aedit_desc)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
	string_append(ch, &pArea->description);
	return true;
    }

    send_to_char("Syntax:  desc\n\r", ch);
    return false;
}

AEDIT(aedit_comments)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
	string_append(ch, &pArea->comments);
	return true;
    }

    send_to_char("Syntax:  comment\n\r", ch);
    return false;
}

AEDIT(aedit_notes)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
	string_append(ch, &pArea->notes);
	return true;
    }

    send_to_char("Syntax:  notes\n\r", ch);
    return false;
}


AEDIT(aedit_repop)
{
    AREA_DATA *pArea;
    int value;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: repop [#mins]\n\r", ch);
        return false;
    }

    if (!is_number(argument))
    {
	send_to_char("That's not a number!\n\r", ch);
	return false;
    }

    if ((value = atoi(argument)) < 5 || value > 120)
    {
	send_to_char("Value is out of range. Range is 5-120 minutes.\n\r", ch);
	return false;
    }

    pArea->repop = value;
    send_to_char("Repop time set.\n\r", ch);
    return true;
}


AEDIT(aedit_credits)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:   credits [$credits]\n\r", ch);
	return false;
    }

    free_string(pArea->credits);
    pArea->credits = str_dup(argument);

    send_to_char("Credits set.\n\r", ch);
    return true;
}


AEDIT(aedit_areawho)
{
    AREA_DATA *pArea;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_AREA(ch, pArea);

	if ( !str_prefix(argument, "blank") )
	{
	    pArea->area_who = AREA_BLANK;

	    send_to_char("Area who title cleared.\n\r", ch);
	    return true;
	}

	if ((value = flag_value(area_who_titles, argument)) != NO_FLAG)
	{
		if( value == AREA_INSTANCE || value == AREA_DUTY )
		{
			send_to_char("Area who title only allowed in blueprints.\n\r", ch);
			return false;
		}

	    pArea->area_who = value;

	    send_to_char("Area who title set.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  areawho [title]\n\r"
		  "Type '? areawho' for a list of who titles.\n\r", ch);
    return false;
}

AEDIT(aedit_placetype)
{
    AREA_DATA *pArea;
    int value;

    if (argument[0] != '\0')
    {
		EDIT_AREA(ch, pArea);

		if(!str_cmp(argument, "none")) {
			pArea->place_flags = PLACE_NOWHERE;

			send_to_char("Area place type cleared.\n\r", ch);
			return true;
		} else if ((value = flag_value(place_flags, argument)) != NO_FLAG) {
			pArea->place_flags = value;

			send_to_char("Area place type set.\n\r", ch);
			return true;
		}
    }

    send_to_char("Syntax:  placetype [flag]\n\r"
		  "Type '? placetype' for a list of possible values.\n\r", ch);
    return false;
}


AEDIT(aedit_file)
{
    AREA_DATA *pArea;
    char file[MAX_STRING_LENGTH];
    int i, length;

    EDIT_AREA(ch, pArea);

    one_argument(argument, file);	/* Forces Lowercase */

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  filename [$file]\n\r", ch);
	return false;
    }

    /*
     * Simple Syntax Check.
     */
    length = strlen(argument);
    if (length > 12)
    {
	send_to_char("No more than twelve characters allowed.\n\r", ch);
	return false;
    }

    /*
     * Allow only letters and numbers.
     */
    for (i = 0; i < length; i++)
    {
	if (!ISALNUM(file[i]))
	{
	    send_to_char("Only letters and numbers are valid.\n\r", ch);
	    return false;
	}
    }

    free_string(pArea->file_name);
    strcat(file, ".are");
    pArea->file_name = str_dup(file);

    send_to_char("Filename set.\n\r", ch);
    return true;
}


AEDIT(aedit_age)
{
    AREA_DATA *pArea;
    char age[MAX_STRING_LENGTH];

    EDIT_AREA(ch, pArea);

    one_argument(argument, age);

    if (!is_number(age) || age[0] == '\0')
    {
	send_to_char("Syntax:  age [#xage]\n\r", ch);
	return false;
    }

    pArea->age = atoi(age);

    send_to_char("Age set.\n\r", ch);
    return true;
}


AEDIT(aedit_recall)
{
	AREA_DATA *pArea;
	char arg1[MIL];
	char arg2[MIL];
	char arg3[MIL];
	char arg4[MIL];
	int vnum, x, y, z;

	EDIT_AREA(ch, pArea);

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);
	argument = one_argument(argument, arg4);

	if (!is_number(arg1) || !arg1[0]) {
		send_to_char("Syntax:  recall <vnum>\n\r", ch);
		send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
		return false;
	}

	vnum = atoi(arg1);

	if(vnum < 1) {
		location_clear(&pArea->recall);
		send_to_char("Recall cleared.\n\r", ch);
	} else if(!arg2[0]) {
		if(!get_room_index(vnum)) {
			send_to_char("AEdit:  Room vnum does not exist.\n\r", ch);
			return false;
		}

		location_set(&pArea->recall,0,vnum,0,0);
		send_to_char("Recall set.\n\r", ch);
	} else if(!arg3[0] || !arg4[0] || !is_number(arg2) || !is_number(arg3) || !is_number(arg4)) {
		send_to_char("Syntax:  recall <vnum>\n\r", ch);
		send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
		return false;
	} else if(!get_wilds_from_uid(NULL,vnum)) {
		send_to_char("AEdit:  Wilderness UID does not exist.\n\r", ch);
		return false;
	} else {
		x = atoi(arg2);
		y = atoi(arg3);
		z = atoi(arg4);
		location_set(&pArea->recall,vnum,x,y,z);
		send_to_char("Recall set.\n\r", ch);
	}

	return true;
}


AEDIT(aedit_security)
{
    AREA_DATA *pArea;
    char sec[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
    int  value;

    EDIT_AREA(ch, pArea);

    one_argument(argument, sec);

    if (!is_number(sec) || sec[0] == '\0')
    {
	send_to_char("Syntax:  security [#xlevel]\n\r", ch);
	return false;
    }

    value = atoi(sec);

    if (value > ch->pcdata->security || value < 0)
    {
	if (ch->pcdata->security != 0)
	{
	    sprintf(buf, "Security is 0-%d.\n\r", ch->pcdata->security);
	    send_to_char(buf, ch);
	}
	else
	    send_to_char("Security is 0 only.\n\r", ch);
	return false;
    }

    pArea->security = value;

    send_to_char("Security set.\n\r", ch);
    return true;
}


AEDIT(aedit_builder)
{
	AREA_DATA *pArea;
	char name[MAX_STRING_LENGTH];
	char buf[MAX_STRING_LENGTH];

	EDIT_AREA(ch, pArea);

	one_argument(argument, name);

	if (name[0] == '\0')
	{
		send_to_char("Syntax:  builder [$name]  -toggles builder\n\r", ch);
		send_to_char("Syntax:  builder All      -allows everyone\n\r", ch);
		return false;
	}

	name[0] = UPPER(name[0]);

	if (strstr(pArea->builders, name) != NULL)
	{
		pArea->builders = string_replace(pArea->builders, name, "\0");
		pArea->builders = string_unpad(pArea->builders);

		if (pArea->builders[0] == '\0')
		{
			free_string(pArea->builders);
			pArea->builders = str_dup("None");
		}
		send_to_char("Builder removed.\n\r", ch);
		return true;
	}
	else
	{
		buf[0] = '\0';

		if (!player_exists(name) && str_cmp(name, "All"))
		{
			act("There is no character by the name of $t.", ch, NULL, NULL, NULL, NULL, name, NULL, TO_CHAR);
			return false;
		}

		if (strstr(pArea->builders, "None") != NULL)
		{
			pArea->builders = string_replace(pArea->builders, "None", "\0");
			pArea->builders = string_unpad(pArea->builders);
		}

		if (pArea->builders[0] != '\0')
		{
			strcat(buf, pArea->builders);
			strcat(buf, " ");
		}

		strcat(buf, name);
		free_string(pArea->builders);
		pArea->builders = string_proper(str_dup(buf));

		send_to_char("Builder added.\n\r", ch);
		send_to_char(pArea->builders,ch);
		return true;
	}

	return false;
}


AEDIT(aedit_vnum)
{
    AREA_DATA *pArea;
    char lower[MAX_STRING_LENGTH];
    char upper[MAX_STRING_LENGTH];
    int  ilower;
    int  iupper;

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, lower);
    one_argument(argument, upper);

    if (!is_number(lower) || lower[0] == '\0'
    || !is_number(upper) || upper[0] == '\0')
    {
	send_to_char("Syntax:  vnum [#xlower] [#xupper]\n\r", ch);
	return false;
    }

    if ((ilower = atoi(lower)) > (iupper = atoi(upper)))
    {
	send_to_char("AEdit:  Upper must be larger then lower.\n\r", ch);
	return false;
    }

    if (!check_range(atoi(lower), atoi(upper)))
    {
	send_to_char("AEdit:  Range must include only this area.\n\r", ch);
	return false;
    }

    if (get_vnum_area(ilower)
    && get_vnum_area(ilower) != pArea)
    {
	send_to_char("AEdit:  Lower vnum already assigned.\n\r", ch);
	return false;
    }

    pArea->min_vnum = ilower;
    send_to_char("Lower vnum set.\n\r", ch);

    if (get_vnum_area(iupper)
    && get_vnum_area(iupper) != pArea)
    {
	send_to_char("AEdit:  Upper vnum already assigned.\n\r", ch);
	return true;	/* The lower value has been set. */
    }

    pArea->max_vnum = iupper;
    send_to_char("Upper vnum set.\n\r", ch);

    return true;
}

AEDIT(aedit_levels)
{
    AREA_DATA *pArea;
    char lower[MAX_STRING_LENGTH];
    char upper[MAX_STRING_LENGTH];
    int  ilower;
    int  iupper;

    EDIT_AREA(ch, pArea);

    argument = one_argument(argument, lower);
    one_argument(argument, upper);

    if (!is_number(lower) || lower[0] == '\0'
    || !is_number(upper) || upper[0] == '\0')
    {
	send_to_char("Syntax:  levels [#xlower] [#xupper]\n\r", ch);
	return false;
    }

    if ((ilower = atoi(lower)) > (iupper = atoi(upper)))
    {
	send_to_char("AEdit:  Upper must be larger then lower.\n\r", ch);
	return false;
    }

    if ((ilower = atoi(lower)) > 120 || (iupper = atoi(upper)) < 1)
    {
	send_to_char("AEdit:  Range must be between 1 and 120.\n\r", ch);
	return false;
    }

    pArea->min_level = ilower;
    send_to_char("Lower level set.\n\r", ch);

    pArea->max_level = iupper;
    send_to_char("Upper level set.\n\r", ch);

    return true;
}


AEDIT(aedit_postoffice)
{
    AREA_DATA *pArea;
    long vnum;
    char buf[MSL];
    ROOM_INDEX_DATA *room;

    EDIT_AREA(ch, pArea);

    if (argument[0] == '\0') {
	send_to_char("Syntax:   postoffice <vnum in the area>\n\r", ch);
	return false;
    }

    if ((vnum = atol(argument)) < pArea->min_vnum || vnum > pArea->max_vnum) {
	send_to_char("That vnum is not in the area.\n\r", ch);
	return false;
    }

    if ((room = get_room_index(vnum)) == NULL) {
	send_to_char("That room vnum doesn't exist.\n\r", ch);
	return false;
    }

    sprintf(buf, "Set post office of %s to %s(%ld)\n\r", pArea->name, room->name, vnum);
    send_to_char(buf, ch);

    pArea->post_office = vnum;
    return true;
}

AEDIT (aedit_addaprog)
{
    int tindex, slot;
    AREA_DATA *pArea;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_AREA(ch, pArea);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addaprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_APROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "aprog");
	return false;
    }

    slot = trigger_table[tindex].slot;

    if ((code = get_script_index (atol(num), PRG_APROG)) == NULL)
    {
	send_to_char("No such AREAProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!pArea->progs->progs) pArea->progs->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);

    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pArea->progs->progs[slot], list);

    send_to_char("Aprog Added.\n\r",ch);
    return true;
}


AEDIT (aedit_delaprog)
{
    AREA_DATA *pArea;
    char aprog[MAX_STRING_LENGTH];
    int value;

    EDIT_AREA(ch, pArea);

    one_argument(argument, aprog);
    if (!is_number(aprog) || aprog[0] == '\0')
    {
       send_to_char("Syntax:  delaprog [#aprog]\n\r",ch);
       return false;
    }

    value = atol (aprog);

    if (value < 0)
    {
        send_to_char("Only non-negative aprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(pArea->progs->progs,value)) {
	send_to_char("No such aprog.\n\r",ch);
	return false;
    }

    send_to_char("Aprog removed.\n\r", ch);
    return true;
}

AEDIT(aedit_varset)
{
    AREA_DATA *pArea;
 
    EDIT_AREA(ch, pArea);

	if (olc_varset(&pArea->index_vars, ch, argument, false))
	{
		// Install variable into the "live"
		olc_varset(&pArea->progs->vars, ch, argument, true);
		return true;
	}
	return false;
}

AEDIT(aedit_varclear)
{
    AREA_DATA *pArea;

    EDIT_AREA(ch, pArea);

	if (olc_varclear(&pArea->index_vars, ch, argument, false))
	{
		// Clear variable on "live"
		olc_varclear(&pArea->progs->vars, ch, argument, true);
		return true;
	}

	return false;
}



REDIT(redit_show)
{
    ROOM_INDEX_DATA *pRoom;
    char buf[MAX_STRING_LENGTH];
    BUFFER *buf1;
	ROOM_INDEX_DATA *recall;
//    ITERATOR it;
//    PROG_LIST *trigger;
    int door;
    CONDITIONAL_DESCR_DATA *cd;
    int i;
    bool found=false;

    EDIT_ROOM(ch, pRoom);

    buf1 = new_buf();
    sprintf(buf, "Base Description:\n\r%s\n\r", pRoom->description);
    add_buf(buf1, buf);

    sprintf(buf, "Name:         {r[{x%s{r]{x\n\r"
                 "Area:         {r[{x%5ld{r]{x %s\n\r",
	    pRoom->name, pRoom->area->anum, pRoom->area->name);
    add_buf(buf1, buf);

    if (IS_SET(pRoom->rs_room_flag[1], ROOM_VIRTUAL_ROOM))
        sprintf (buf, "VRoom at ({W%ld{x, {W%ld{x), in wilds uid ({W%ld{x) '{W%s{x'\n\r",
                 pRoom->x, pRoom->y, pRoom->wilds->uid, pRoom->wilds->name);
    else if(pRoom->viewwilds)
        sprintf(buf, "Vnum:         {r[{x%5ld{r]{x\n\r"
                     "Sector:       {r[{x%s{r]{x\n\r"
                     "Map Coordinate at ({W%ld{x, {W%ld{x, {W%ld{x), in wilds uid ({W%ld{x) '{W%s{x'\n\r",
	        pRoom->vnum, flag_string(sector_flags, pRoom->rs_sector_type),
	        pRoom->x, pRoom->y, pRoom->z, pRoom->viewwilds->uid, pRoom->viewwilds->name);
    else
        sprintf(buf, "Vnum:         {r[{x%5ld{r]{x\n\r"
                     "Sector:       {r[{x%s{r]{x\n\r",
	        pRoom->vnum, flag_string(sector_flags, pRoom->rs_sector_type));

    add_buf(buf1, buf);

    sprintf(buf, "Persist:      {r[%s{r]{x\n\r", (pRoom->persist ? "{WON" : "{Doff"));
    add_buf(buf1, buf);

    sprintf(buf, "Room flags:   {r[{x%s{r]{x\n\r",
	    bitmatrix_string(room_flagbank, pRoom->rs_room_flag));
    add_buf(buf1, buf);
/*
    sprintf(buf, "Room2 flags:  {r[{x%s{r]{x\n\r",
	    flag_string(room2_flags, pRoom->room2_flags));
    add_buf(buf1, buf);
*/
    if (pRoom->rs_heal_rate != 100 || pRoom->rs_mana_rate != 100 || pRoom->rs_move_rate != 100)
    {
	sprintf(buf,
	         "Health rec:   {r[{x%d{r]{x\n\r"
		 "Mana rec:     {r[{x%d{r]{x\n\r"
		 "Move rec:     {r[{x%d{r]{x\n\r",
		pRoom->rs_heal_rate , pRoom->rs_mana_rate, pRoom->rs_move_rate);
        add_buf(buf1, buf);
    }
	if (rs_location_isset(&pRoom->rs_recall))
	{
		if(pRoom->rs_recall.wuid) {
			WILDS_DATA *wilds = get_wilds_from_uid(NULL,pRoom->rs_recall.wuid);
			if(wilds)
				sprintf(buf, "{WRecall:      Wilds {X%s {R[{X%lu{R]{X at {R<{X%lu,%lu,%lu{R>{X\n\r", wilds->name, pRoom->rs_recall.wuid,
					pRoom->rs_recall.id[0],pRoom->rs_recall.id[1],pRoom->rs_recall.id[2]);
			else
				sprintf(buf, "{WRecall:      Wilds {X??? {R[{X%lu{R]{X\n\r", pRoom->rs_recall.wuid);
		} else if(pRoom->rs_recall.id[0] > 0 && (recall = get_room_index(pRoom->rs_recall.id[0]))) {
				sprintf(buf, "{WRecall:      Room {R[{X%5ld{R]{X {X%s\n\r", pRoom->rs_recall.id[0], recall->name);
		} else
				sprintf(buf, "{WRecall:      {R[{X%lu{R]{X none\n\r", pRoom->rs_recall.id[0]);
		add_buf(buf1, buf);
	}

    if (pRoom->locale) {
	sprintf(buf, "Locale:       {r[{x%ld{r]{x\n\r", pRoom->locale);
	add_buf(buf1, buf);
    }

    if (!IS_NULLSTR(pRoom->owner))
    {
	sprintf(buf,
	         "Owner:        {r[{x%s{r]{x\n\r", pRoom->owner);
        add_buf(buf1, buf);
    }

    if (pRoom->home_owner != NULL && pRoom->home_owner[0] != '\0')
    {
	sprintf(buf,
	         "Home owner:   {r[{x%s{r]{x\n\r", pRoom->home_owner);
        add_buf(buf1, buf);
    }

	sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pRoom->comments);
	add_buf(buf1, buf);


    if (pRoom->extra_descr)
    {
	EXTRA_DESCR_DATA *ed;

	add_buf(buf1,
	         "Desc Kwds:    {r[{x");

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    add_buf(buf1, ed->keyword);

	    if (ed->next)
		add_buf(buf1, " ");
	}

	add_buf(buf1, "{r]{x\n\r");
    }

    found=0;
    for (door = 0; door < MAX_DIR; door++)
    {
	EXIT_DATA *pexit;

	if ((pexit = pRoom->exit[door]))
	{
	    AREA_DATA *pArea = NULL;
	    WILDS_DATA *pWilds = NULL;
	    char word[MAX_INPUT_LENGTH];
	    char reset_state[MAX_STRING_LENGTH];
	    char *state;
	    int i, length;
            bool ffound = false;


            if (pRoom->wilds)
            {
                if (IS_SET(pexit->exit_info, EX_VLINK))
		{
                    sprintf (buf, "-{W%-9s{x to {W%6ld{x, Area Uid ({W%ld{x), '{W%s{x'.\n\r",
                             capitalize (dir_name[door]),
                             pexit->u1.to_room ? pexit->u1.to_room->vnum : 0,
			     pexit->u1.to_room ? pexit->u1.to_room->area->uid : 0,
			     pexit->u1.to_room ? pexit->u1.to_room->area->name : "{RERROR");
		}
                else
                    sprintf (buf, "-{W%-9s{x to ({W%d{x,{W%d{x).\n\r",
                             capitalize (dir_name[door]),
                             pexit->wilds.x, pexit->wilds.y);
            }
            else
            {
                if (IS_SET(pexit->exit_info, EX_VLINK))
		{
		    pArea = get_area_from_uid(pexit->wilds.area_uid);
		    pWilds = get_wilds_from_uid(pArea, pexit->wilds.wilds_uid);
                    sprintf (buf, "-{W%-9s{x to ({W%d{x,{W%d{x), Wilds Uid ({W%ld{x), '{W%s{x'.\n\r",
                             capitalize (dir_name[door]),
                             pexit->wilds.x, pexit->wilds.y,
			     pWilds ? pWilds->uid : 0,
			     pWilds ? pWilds->name : "(null)");
	            add_buf(buf1, buf);

		    sprintf (buf, "                         Area Uid ({W%ld{x), '{W%s{x'.\n\r",
			     pArea ? pArea->uid : 0,
			     pArea ? pArea->name : "(null)");
		}
                else
                    sprintf (buf, "-{W%-9s{x to {W%6ld{x\n\r",
                             capitalize (dir_name[door]),
                             pexit->u1.to_room ? pexit->u1.to_room->vnum : 0);
            }

	    add_buf(buf1, buf);

            /*
             * Format up the exit info.
             * Capitalize all flags that are not part of the reset info.
             */
            strcpy (reset_state, flag_string (exit_flags, pexit->rs_flags));
            state = flag_string (exit_flags, pexit->exit_info);
            add_buf(buf1, "    Exit flags: [{W");
            ffound = false;
            for (;;)
            {
                state = one_argument (state, word);

                if (word[0] == '\0')
                {
                    add_buf(buf1, "{x]\n\r");
                    break;
                }

		if (str_infix(word, reset_state))
		{
		    length = strlen(word);
		    for (i = 0; i < length; i++)
			word[i] = UPPER(word[i]);
		}

                if (ffound == true)
                    add_buf(buf1, " ");

                add_buf(buf1, word);
                ffound = true;
            }

            if (pexit->long_desc && pexit->long_desc[0] != '\0')
            {
                sprintf (buf, "    Exit Description:\n\r    {W%s{x\n\r", pexit->long_desc);
                add_buf(buf1, buf);
            }

            sprintf (buf, "    Keywords: [{W%s{x]\n\r"
                          "    Short Description: '{W%s{x'\n\r",
                     pexit->keyword && pexit->keyword[0] != '\0' ? pexit->keyword : "(Not set)",
                     pexit->short_desc && pexit->short_desc[0] != '\0' ? pexit->short_desc : "(Not set)");
            add_buf(buf1, buf);

            if (IS_SET(pexit->rs_flags, EX_ISDOOR))
            {
                sprintf (buf, "    -Door Material: [{W%s{x] Strength: [{W%d{x]  Lock Flags: [{W%s{x]  Key vnum: [{W%ld{x] Pick chance: [{W%d%%{x]\n\r",
                         pexit->door.material,
                         pexit->door.strength,
                         flag_string(lock_flags, pexit->door.lock.flags),
                         pexit->door.lock.key_vnum,
                         pexit->door.lock.pick_chance);
                add_buf(buf1, buf);
            }
            else
                add_buf(buf1, "\n\r");

            found = true;
	}
    }

    if (found == false)
        add_buf(buf1, "    {W(None set){x\n\r");

    if (pRoom->progs->progs)
		olc_show_progs(buf1, pRoom->progs->progs, PRG_RPROG, "RoomProg Vnum");

	if (pRoom->index_vars)
		olc_show_index_vars(buf1, pRoom->index_vars);

    if (pRoom->conditional_descr)
    {
	char phrase[MIL];

	sprintf(buf, "\n\rConditional Descriptions for {r[{x%5ld{r]{x:\n\r", pRoom->vnum);

	add_buf(buf1, buf);

	for (i = 0, cd = pRoom->conditional_descr; cd != NULL; cd = cd->next)
	{
	    if (i == 0)
	    {
		add_buf(buf1, "{Y Num  Condition Phrase{x\n\r");
		add_buf(buf1, "{Y ---  --------- ------{x\n\r");
	    }

	    if (cd->condition == CONDITION_HOUR || cd->condition == CONDITION_SCRIPT)
			sprintf(phrase, "%d", cd->phrase);
		else {
			strncpy(phrase, condition_phrase_to_name(cd->condition, cd->phrase), MIL-1);
			phrase[MIL-1] = '\0';
		}


	    sprintf(buf, "{r[{x%3d{r]{x %-9s %s\n\r", i, condition_type_to_name(cd->condition), phrase );

	    add_buf(buf1, buf);
	    i++;
	}
    }

    page_to_char (buf_string(buf1), ch);
    free_buf(buf1);

		if (ch->in_room->reset_first)
	{
	    send_to_char(
		"\n\rResets: M = mobile, R = room, O = object, "
		"P = pet, S = shopkeeper\n\r", ch);
	    display_resets(ch);
	}

    return false;
}


/* The version to allow rooms to change exits */
bool rp_change_exit(ROOM_INDEX_DATA *pRoom, char *argument, int door)
{
    EXIT_DATA *pExit;
    ROOM_INDEX_DATA *pToRoom;
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int  value;

    /*
     * Now parse the arguments.
     */
    argument = one_argument(argument, command);
    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	bug("Rprog: No vnum to create entrance or delete, on room %d.",
		pRoom->vnum);
	return false;
    }

    if (!str_cmp(arg, "delete"))
    {
	int16_t rev;

	if (!pRoom->exit[door])
	{
	    bug("RProg: Couldn't delete room. %d", pRoom->vnum);
	    return false;
	}

	/*
	 * Remove ToRoom Exit.
	 */
	rev = rev_dir[door];
	pToRoom = pRoom->exit[door]->u1.to_room;

	if (pToRoom->exit[rev])
	{
	    free_exit(pToRoom->exit[rev]);
	    pToRoom->exit[rev] = NULL;
	}

	/*
	 * Remove this exit.
	 */
	free_exit(pRoom->exit[door]);
	pRoom->exit[door] = NULL;

	return true;
    }

    value = atoi(arg);

    if (!get_room_index(value))
    {
       bug("Rprog: A link cannot link non-existant room.\n\r",0);
       return false;
    }

    if (get_room_index(value)->exit[rev_dir[door]])
    {
       bug("Rprog: Reverse-side exit to room already exists.", 0);
       return false;
    }

    if (!pRoom->exit[door])
    {
       pRoom->exit[door] = new_exit();
	pRoom->exit[door]->from_room = pRoom;
    }

    pToRoom = pRoom->exit[door]->u1.to_room = get_room_index(value);
    pRoom->exit[door]->orig_door = door;

    /*	pRoom->exit[door]->vnum = value;                Can't set vnum in ROM */

    door                    = rev_dir[door];
    pExit                   = new_exit();
    pExit->u1.to_room       = pRoom;
    /*	pExit->vnum             = pRoom->vnum;    Can't set vnum in ROM */
    pExit->orig_door	= door;
    pToRoom->exit[door]       = pExit;
    pExit->from_room = pToRoom;

    return true;
}


/* Local function. */
bool change_exit(CHAR_DATA *ch, char *argument, int door)
{
	ROOM_INDEX_DATA *pRoom;
	ROOM_INDEX_DATA *to_room;
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_INPUT_LENGTH];
	char buf[MSL];
	long value;

	EDIT_ROOM_SIMPLE(ch, pRoom);

	/*
	* Now parse the arguments.
	*/
	argument = one_argument_norm(argument, command);
	one_argument_norm(argument, arg);

	if (command[0] == '\0' && argument[0] == '\0')	/* Move command. */
	{
		move_char(ch, door, true);
		return false;
	}

	if(room_is_clone(pRoom)) return false;

	if (command[0] == '?')
	{
		send_to_char("You must specify an argument.\n\r", ch);
		return false;
	}

	if (!str_cmp(command, "delete"))
	{
		ROOM_INDEX_DATA *pToRoom;
		int16_t rev;

		if (!pRoom->exit[door])
		{
			send_to_char("REdit:  Cannot delete a null exit.\n\r", ch);
			return false;
		}

		/*
		* Remove ToRoom Exit.
		*/
		rev = rev_dir[door];
		pToRoom = pRoom->exit[door]->u1.to_room;
		if (pToRoom == NULL)
		{
			sprintf(buf, "change_exit: pToRoom was null! room is %s (%ld), door is %i",
				pRoom->name, pRoom->vnum, door);
			bug(buf, 0);
			send_to_char("REdit: couldn't delete that exit, probably a bad link. Please report to coder@megacosm.net\n\r", ch);
			return false;
		}

		if (pToRoom->exit[rev])
		{
			free_exit(pToRoom->exit[rev]);
			pToRoom->exit[rev] = NULL;
		}

		/*
		* Remove this exit.
		*/
		free_exit(pRoom->exit[door]);
		pRoom->exit[door] = NULL;

		send_to_char("Exit unlinked.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "flag"))
	{
		// Set the exit flags
		if (!room_is_clone(pRoom) && (value = flag_value(exit_flags, argument)) != NO_FLAG)
		{
			ROOM_INDEX_DATA *pToRoom;
			int16_t rev;

			if (!pRoom->exit[door])
			{
				// Environment exits can be created directly
				if( value == EX_ENVIRONMENT )
				{
					pRoom->exit[door] = new_exit();
					pRoom->exit[door]->from_room = pRoom;
					pRoom->exit[door]->u1.to_room = NULL;
					pRoom->exit[door]->orig_door = door;
					SET_BIT(pRoom->exit[door]->rs_flags,  EX_ENVIRONMENT);

					send_to_char("Environment exit created.\n\r",ch);
					return true;
				}

				send_to_char("Exit doesn't exist.\n\r",ch);
				return false;
			}

			TOGGLE_BIT(pRoom->exit[door]->rs_flags,  value);
			// Don't toggle exit_info because it can be changed by players.
			pRoom->exit[door]->exit_info = pRoom->exit[door]->rs_flags;

			pToRoom = pRoom->exit[door]->u1.to_room;
			rev = rev_dir[door];

			// Set the exit as environment
			if( IS_SET(pRoom->exit[door]->exit_info, EX_ENVIRONMENT) && IS_SET(value, EX_ENVIRONMENT) )
			{
				// Delete remote exit
				if( pToRoom != NULL && pRoom->exit[rev] != NULL && pRoom->exit[rev]->u1.to_room == pRoom)
				{
					free_exit(pRoom->exit[rev]);
					pRoom->exit[rev] = NULL;
				}

				// Remove destination
				pRoom->exit[door]->u1.to_room = NULL;
				send_to_char("Exit flag toggled.\n\rEnvironment exit distination unlinked.\n\r", ch);
			}
			else
			{
				if (pToRoom != NULL && pToRoom->exit[rev] != NULL)
				{
					TOGGLE_BIT(pToRoom->exit[rev]->rs_flags,  value);
					TOGGLE_BIT(pToRoom->exit[rev]->exit_info, value);
				}

				send_to_char("Exit flag toggled.\n\r", ch);
			}

			return true;
		}
	}

	if (!str_cmp(command, "link"))
	{
		EXIT_DATA *pExit;

		if (arg[0] == '\0' || !is_number(arg))
		{
			send_to_char("Syntax:  [direction] link [vnum]\n\r", ch);
			return false;
		}

		value = atol(arg);

		ROOM_INDEX_DATA *pToRoom = get_room_index(value);

		if (!pToRoom)
		{
			send_to_char("REdit:  Cannot link to non-existant room.\n\r", ch);
			return false;
		}

		if (!IS_BUILDER(ch, pToRoom->area))
		{
			send_to_char("REdit:  Cannot link to that area.\n\r", ch);
			return false;
		}

		if( !rooms_in_same_section(pRoom->vnum, value) )
		{
			send_to_char("REdit:  Attempting to link outside of a defined blueprint section.\n\r", ch);
			return false;
		}

		if (pToRoom->exit[rev_dir[door]])
		{
			send_to_char("REdit:  Remote side's exit already exists.\n\r", ch);
			return false;
		}

		if (!pRoom->exit[door])
		{
			pRoom->exit[door] = new_exit();
			pRoom->exit[door]->from_room = pRoom;
		}
		else
		{
			if( IS_SET(pRoom->exit[door]->exit_info, EX_ENVIRONMENT) )
			{
				send_to_char("REdit:  Environment exits cannot be linked.\n\r", ch);
				return false;
			}
		}


		pRoom->exit[door]->u1.to_room	= pToRoom;
		pRoom->exit[door]->orig_door	= door;
		door							= rev_dir[door];
		pExit							= new_exit();

		pExit->u1.to_room				= pRoom;
		pExit->orig_door				= door;
		pToRoom->exit[door]				= pExit;
		pExit->from_room				= pToRoom;

		send_to_char("Two-way link established.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "dig"))
	{
		if (arg[0] == '\0' || !is_number(arg))
		{
			send_to_char("Syntax:  [direction] dig [vnum]\n\r", ch);
			return false;
		}

		if( IS_SET(ch->in_room->room_flag[1], ROOM_BLUEPRINT) ||
			IS_SET(ch->in_room->area->area_flags, ROOM_BLUEPRINT) )
		{
			value = atol(arg);

			if( !rooms_in_same_section(pRoom->vnum, value) )
			{
				send_to_char("REdit:  Attempting to dig outside of a defined blueprint section.\n\r", ch);
				return false;
			}

			redit_blueprint_oncreate = (IS_SET(ch->in_room->room_flag[1], ROOM_BLUEPRINT)) && true;
		}

		if( pRoom->exit[door] && IS_SET(pRoom->exit[door]->exit_info, EX_ENVIRONMENT) )
		{
			send_to_char("REdit:  Environment exits cannot be linked.\n\r", ch);
			return false;
		}

		redit_create(ch, arg);
		sprintf(buf, "link %s", arg);
		change_exit(ch, buf, door);
		return true;
	}

	if (!str_cmp(command, "room"))
	{
		ROOM_INDEX_DATA *pToRoom;
		EXIT_DATA *pExit;
		int16_t rev;

		if (arg[0] == '\0' || !is_number(arg))
		{
			send_to_char("Syntax:  [direction] room [vnum]\n\r", ch);
			return false;
		}

		if (!(pExit = pRoom->exit[door]))
		{
			pExit = pRoom->exit[door] = new_exit();
		}
		else
		{
			if( IS_SET(pRoom->exit[door]->exit_info, EX_ENVIRONMENT) )
			{
				send_to_char("REdit:  Environment exits cannot be linked.\n\r", ch);
				return false;
			}

		}

		value = atol(arg);

		pToRoom = get_room_index(value);

		if (!pToRoom)
		{
			send_to_char("REdit:  Cannot link to non-existant room.\n\r", ch);
			return false;
		}

		if( !rooms_in_same_section(pRoom->vnum, value) )
		{
			send_to_char("REdit:  Attempting to link outside of a defined blueprint section.\n\r", ch);
			return false;
		}

		rev = rev_dir[door];
		if( pToRoom->exit[rev] && IS_SET(pToRoom->exit[rev]->exit_info, EX_ENVIRONMENT) )
		{
			send_to_char("REdit:  Destination has an environment exit in the reverse direction.\n\r", ch);
			return false;
		}

		pRoom->exit[door]->u1.to_room	= pToRoom;
		pRoom->exit[door]->orig_door	= door;
		pExit->from_room				= pRoom;

		send_to_char("One-way link established.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "lockflags"))
	{
		if( (value = flag_value(lock_flags, argument)) == NO_FLAG )
		{
			send_to_char("Syntax:  [direction] lockflags [flags]\n\r", ch);
			return false;
		}


		if (!pRoom->exit[door])
		{
			send_to_char("Exit doesn't exist.\n\r",ch);
			return false;
		}

		TOGGLE_BIT(pRoom->exit[door]->door.rs_lock.flags,  value);
		// Don't toggle exit_info because it can be changed by players.
		pRoom->exit[door]->door.lock.flags = pRoom->exit[door]->door.rs_lock.flags;

		ROOM_INDEX_DATA *pToRoom = pRoom->exit[door]->u1.to_room;
		int rev = rev_dir[door];

		if (pToRoom != NULL && pToRoom->exit[rev] != NULL)
		{
			TOGGLE_BIT(pToRoom->exit[rev]->door.rs_lock.flags,  value);
			TOGGLE_BIT(pToRoom->exit[rev]->door.lock.flags, value);
		}

		send_to_char("Exit flag toggled.\n\r", ch);
		return true;
	}


	if (!str_cmp(command, "pick_chance"))
	{
		if (arg[0] == '\0' || !is_number(arg))
		{
			send_to_char("Syntax:  [direction] pick_chance [0-100]\n\r", ch);
			return false;
		}

		if (!pRoom->exit[door])
		{
			send_to_char("Exit doesn't exist.\n\r",ch);
			return false;
		}

		value = atoi(arg);

		if( value < 0 || value > 100 )
		{
			send_to_char("Chance between 0% and 100%.\n\r",ch);
			return false;
		}

		pRoom->exit[door]->door.lock.pick_chance =
		pRoom->exit[door]->door.rs_lock.pick_chance = value;

		send_to_char("Exit pick chance set.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "key"))
	{
		if (arg[0] == '\0' || !is_number(arg))
		{
			send_to_char("Syntax:  [direction] key [vnum]\n\r", ch);
			return false;
		}

		if (!pRoom->exit[door])
		{
			send_to_char("Exit doesn't exist.\n\r",ch);
			return false;
		}

		value = atoi(arg);

		if (!get_obj_index(value))
		{
			send_to_char("REdit:  Item doesn't exist.\n\r", ch);
			return false;
		}

		if (get_obj_index(atol(argument))->item_type != ITEM_KEY)
		{
			send_to_char("REdit:  Key doesn't exist.\n\r", ch);
			return false;
		}

		pRoom->exit[door]->door.lock.key_vnum =
		pRoom->exit[door]->door.rs_lock.key_vnum = atol(arg);

		send_to_char("Exit key set.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "name"))
	{
	if (arg[0] == '\0')
	{
	send_to_char("Syntax:  [direction] name [string]\n\r", ch);
	send_to_char("         [direction] name none\n\r", ch);
	return false;
	}

	if (!pRoom->exit[door])
	{
	send_to_char("Exit doesn't exist.\n\r",ch);
	return false;
	}

	free_string(pRoom->exit[door]->keyword);
	if (str_cmp(arg,"none"))
	{
	pRoom->exit[door]->keyword = str_dup(arg);
	if ((to_room = pRoom->exit[door]->u1.to_room) != NULL
	&&   to_room->exit[rev_dir[door]] != NULL) {
	free_string(to_room->exit[rev_dir[door]]->keyword);
	to_room->exit[rev_dir[door]]->keyword = str_dup(arg);
	}
	}
	else
	{
	pRoom->exit[door]->keyword = str_dup("");
	if ((to_room = pRoom->exit[door]->u1.to_room) != NULL
	&&   to_room->exit[rev_dir[door]] != NULL) {
	free_string(to_room->exit[rev_dir[door]]->keyword);
	to_room->exit[rev_dir[door]]->keyword = str_dup("");
	}
	}

	send_to_char("Exit name set.\n\r", ch);
	return true;
	}

	if (!str_prefix(command, "description"))
	{
	if (arg[0] == '\0')
	{
	if (!pRoom->exit[door])
	{
	send_to_char("Exit doesn't exist.\n\r",ch);
	return false;
	}

	string_append(ch, &pRoom->exit[door]->short_desc);
	return true;
	}

	send_to_char("Syntax:  [direction] desc\n\r", ch);
	return false;
	}

	return false;
}


REDIT(redit_north)
{
    if (change_exit(ch, argument, DIR_NORTH))
	return true;

    return false;
}


REDIT(redit_west)
{
    if (change_exit(ch, argument, DIR_WEST))
	return true;

    return false;
}



REDIT(redit_south)
{
    if (change_exit(ch, argument, DIR_SOUTH))
	return true;

    return false;
}



REDIT(redit_east)
{
    if (change_exit(ch, argument, DIR_EAST))
	return true;

    return false;
}


REDIT(redit_southeast)
{
    if (change_exit(ch, argument, DIR_SOUTHEAST))
	return true;

    return false;
}


REDIT(redit_southwest)
{
    if (change_exit(ch, argument, DIR_SOUTHWEST))
	return true;

    return false;
}


REDIT(redit_northeast)
{
    if (change_exit(ch, argument, DIR_NORTHEAST))
	return true;

    return false;
}


REDIT(redit_northwest)
{
    if (change_exit(ch, argument, DIR_NORTHWEST))
	return true;

    return false;
}


REDIT(redit_up)
{
    if (change_exit(ch, argument, DIR_UP))
	return true;

    return false;
}


REDIT(redit_down)
{
    if (change_exit(ch, argument, DIR_DOWN))
	return true;

    return false;
}
/*
REDIT(redit_varset)
{
    ROOM_INDEX_DATA *pRoom;
    char name[MIL];
    char type[MIL];
    char yesno[MIL];
    bool saved;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  varset <name> <number|string|room> <yes|no> <value>\n\r", ch);
	return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, type);
    argument = one_argument(argument, yesno);

    if(!variable_validname(name)) {
	send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
	return false;
    }

    saved = !str_cmp(yesno,"yes");

    if(!argument[0]) {
	send_to_char("Set what on the variable?\n\r", ch);
	return false;
    }

    if(!str_cmp(type,"room")) {
	if(!is_number(argument)) {
	    send_to_char("Specify a room vnum.\n\r", ch);
	    return false;
	}

	variables_setindex_room(&pRoom->index_vars,name,atoi(argument), saved);
    } else if(!str_cmp(type,"string"))
	variables_setindex_string(&pRoom->index_vars,name,argument,false, saved);
    else if(!str_cmp(type,"number")) {
	if(!is_number(argument)) {
	    send_to_char("Specify an integer.\n\r", ch);
	    return false;
	}

	variables_setindex_integer(&pRoom->index_vars,name,atoi(argument), saved);
    } else {
	send_to_char("Invalid type of variable.\n\r", ch);
	return false;
    }

    variable_copyto(&pRoom->index_vars,&pRoom->progs->vars,name,name,false);
    send_to_char("Variable set.\n\r", ch);
    return true;
}

REDIT(redit_varclear)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0') {
	send_to_char("Syntax:  varclear <name>\n\r", ch);
	return false;
    }

    if(!variable_validname(argument)) {
	send_to_char("Variable names can only have alphabetical characters.\n\r", ch);
	return false;
    }

    if(!variable_remove(&pRoom->index_vars,argument)) {
	send_to_char("No such variable defined.\n\r", ch);
	return false;
    }

    variable_remove(&pRoom->progs->vars,argument);
    send_to_char("Variable cleared.\n\r", ch);
    return true;
}
*/

REDIT(redit_varset)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if(olc_varset(&pRoom->index_vars, ch, argument, false))
    {
        // This will *NOT* update cloned rooms...
        olc_varset(&pRoom->progs->vars, ch, argument, true);
        return true;
    }
    return false;
}

REDIT(redit_varclear)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

	if(olc_varclear(&pRoom->index_vars, ch, argument, false))
	{
		// This will *NOT* update cloned rooms...
		olc_varclear(&pRoom->progs->vars, ch, argument, true);
		return true;
	}

	return false;
}

REDIT(redit_ed)
{
    ROOM_INDEX_DATA *pRoom;
    EXTRA_DESCR_DATA *ed;

    char command[MAX_INPUT_LENGTH];
    char keyword[MAX_INPUT_LENGTH];
    char copy_item[MAX_INPUT_LENGTH];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, command);
    argument = one_argument(argument, keyword);
    one_argument(argument, copy_item);

    if (command[0] == '\0' || keyword[0] == '\0')
    {
	send_to_char("Syntax:  ed add [keyword]\n\r", ch);
	send_to_char("         ed edit [keyword]\n\r", ch);
	send_to_char("         ed show [keyword]\n\r", ch);
	send_to_char("         ed delete [keyword]\n\r", ch);
	send_to_char("         ed format [keyword]\n\r", ch);
	send_to_char("         ed copy existing_keyword new_keyword\n\r", ch);
	send_to_char("         ed environment [keyword]\n\r", ch);

	return false;
    }

    if (!str_cmp(command, "copy"))
    {
	EXTRA_DESCR_DATA *ed2;

    	if (keyword[0] == '\0' || copy_item[0] == '\0')
	{
	   send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
	   return false;
        }

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
	    return false;
	}

	ed2			=   new_extra_descr();
	ed2->keyword		=   str_dup(copy_item);
	if( ed->description )
		ed2->description		= str_dup(ed->description);
	else
		ed2->description		= NULL;
	ed2->next		=   pRoom->extra_descr;
	pRoom->extra_descr	=   ed2;

	send_to_char("Done.\n\r", ch);

	return true;
    }

    if (!str_cmp(command, "environment"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed environment [keyword]\n\r", ch);
	    return false;
	}

	ed			=   new_extra_descr();
	ed->keyword		=   str_dup(keyword);
	ed->description		= NULL;
	ed->next		=   pRoom->extra_descr;
	pRoom->extra_descr	=   ed;

	send_to_char("Enviromental extra description added.\n\r", ch);

	return true;
    }

    if (!str_cmp(command, "add"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed add [keyword]\n\r", ch);
	    return false;
	}

	ed			=   new_extra_descr();
	ed->keyword		=   str_dup(keyword);
	ed->description		=   str_dup("");
	ed->next		=   pRoom->extra_descr;
	pRoom->extra_descr	=   ed;

	string_append(ch, &ed->description);

	return true;
    }


    if (!str_cmp(command, "edit"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed edit [keyword]\n\r", ch);
	    return false;
	}

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
	    return false;
	}

	if( !ed->description )
		ed->description = str_dup("");

	string_append(ch, &ed->description);

	return true;
    }


    if (!str_cmp(command, "delete"))
    {
	EXTRA_DESCR_DATA *ped = NULL;

	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed delete [keyword]\n\r", ch);
	    return false;
	}

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	    ped = ed;
	}

	if (!ed)
	{
	    send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
	    return false;
	}

	if (!ped)
	    pRoom->extra_descr = ed->next;
	else
	    ped->next = ed->next;

	free_extra_descr(ed);

	send_to_char("Extra description deleted.\n\r", ch);
	return true;
    }


    if (!str_cmp(command, "format"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed format [keyword]\n\r", ch);
	    return false;
	}

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
	    return false;
	}

	if( !ed->description )
	{
	    send_to_char("REdit:  Extra description is an environmental extra description.\n\r", ch);
	    return false;
	}

	ed->description = format_string(ed->description);

	send_to_char("Extra description formatted.\n\r", ch);
	return true;
    }

    if (!str_cmp(command, "show"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed show [keyword]\n\r", ch);
	    return false;
	}

	for (ed = pRoom->extra_descr; ed; ed = ed->next)
	{
	    if (is_name(keyword, ed->keyword))
		break;
	}

	if (!ed)
	{
	    send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
	    return false;
	}

	if (!ed->description)
	{
		send_to_char("REdit:  Cannot show environmental extra description.\n\r", ch);
		return false;
	}

	page_to_char(ed->description, ch);

	return true;
    }

    redit_ed(ch, "");
    return false;
}


REDIT(redit_create)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *pRoom;
    int value;
    int iHash;
    long auto_vnum = 0;

    value = atoi(argument);

    if (argument[0] == '\0' || value <= 0)
    {
	//send_to_char("Syntax:  create [vnum > 0]\n\r", ch);
	ROOM_INDEX_DATA *temp_room;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_room = get_room_index(auto_vnum);
	if (temp_room != NULL) {
		while (temp_room != NULL)
		{
			temp_room = get_room_index(auto_vnum);
			if (temp_room == NULL) break;
			auto_vnum++;
		}
	}

	if (auto_vnum > ch->in_room->area->max_vnum) {
		send_to_char("Sorry, this area has no more space left.\n\r",
				ch);
		return false;
	}
    }

    if (auto_vnum != 0) value = auto_vnum;

    pArea = get_vnum_area(value);
    if (!pArea)
    {
	send_to_char("REdit:  That vnum is not assigned an area.\n\r", ch);
	return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("REdit:  Vnum in an area you cannot build in.\n\r", ch);
	return false;
    }

    if (get_room_index(value))
    {
	send_to_char("REdit:  Room vnum already exists.\n\r", ch);
	return false;
    }

    pRoom			= new_room_index();
    pRoom->area			= pArea;
	list_appendlink(pArea->room_list, pRoom);	// Add to the area room list
    pRoom->vnum			= value;
    if (value > top_vnum_room)
        top_vnum_room = value;

	// Check whether to automatically set the room as blueprint
    if( redit_blueprint_oncreate )
    {
		ROOM_INDEX_DATA *pPrevRoom;

		EDIT_ROOM(ch, pPrevRoom);
		// Only copy if the new room is in the same area as the previous room
		if( pPrevRoom && pPrevRoom->area == pArea )
		{
			SET_BIT(pRoom->room_flag[1], ROOM_BLUEPRINT);
		}
		redit_blueprint_oncreate = false;
	}

    iHash			= value % MAX_KEY_HASH;
    pRoom->next			= room_index_hash[iHash];
    room_index_hash[iHash]	= pRoom;
    ch->desc->pEdit		= (void *)pRoom;

    SET_BIT(pRoom->area->area_flags, AREA_CHANGED);
    send_to_char("Room created.\n\r", ch);
    return true;
}


REDIT(redit_name)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  name [name]\n\r", ch);
	return false;
    }

    argument[0] = UPPER(argument[0]);

    free_string(pRoom->name);
    pRoom->name = str_dup(argument);

    send_to_char("Name set.\n\r", ch);
    return true;
}


REDIT(redit_desc)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0')
    {
	string_append(ch, &pRoom->description);
	return true;
    }

    send_to_char("Syntax:  desc\n\r", ch);
    return false;
}

REDIT(redit_comments)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0')
    {
	string_append(ch, &pRoom->comments);
	return true;
    }

    send_to_char("Syntax:  comment\n\r", ch);
    return false;
}

REDIT(redit_recall)
{
	ROOM_INDEX_DATA *pRoom;
	char arg1[MIL];
	char arg2[MIL];
	char arg3[MIL];
	char arg4[MIL];
	int vnum, x, y, z;

	EDIT_ROOM(ch, pRoom);

	argument = one_argument(argument, arg1);
	argument = one_argument(argument, arg2);
	argument = one_argument(argument, arg3);
	argument = one_argument(argument, arg4);

	if (!is_number(arg1) || !arg1[0]) {
		send_to_char("Syntax:  recall <vnum>\n\r", ch);
		send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
		return false;
	}

	vnum = atoi(arg1);

	if(vnum < 1) {
		rs_location_clear(&pRoom->rs_recall);
		send_to_char("Recall cleared.\n\r", ch);
	} else if(!arg2[0]) {
		if(!get_room_index(vnum)) {
			send_to_char("AEdit:  Room vnum does not exist.\n\r", ch);
			return false;
		}

		rs_location_set(&pRoom->rs_recall,0,vnum,0,0);
		send_to_char("Recall set.\n\r", ch);
	} else if(!arg3[0] || !arg4[0] || !is_number(arg2) || !is_number(arg3) || !is_number(arg4)) {
		send_to_char("Syntax:  recall <vnum>\n\r", ch);
		send_to_char("         recall <wuid> <x> <y> <z>\n\r", ch);
		return false;
	} else if(!get_wilds_from_uid(NULL,vnum)) {
		send_to_char("AEdit:  Wilderness UID does not exist.\n\r", ch);
		return false;
	} else {
		x = atoi(arg2);
		y = atoi(arg3);
		z = atoi(arg4);
		rs_location_set(&pRoom->rs_recall,vnum,x,y,z);
		send_to_char("Recall set.\n\r", ch);
	}

	return true;
}


REDIT(redit_heal)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (is_number(argument))
       {
          pRoom->rs_heal_rate = atoi (argument);
          send_to_char ("Heal rate set.\n\r", ch);
          return true;
       }

    send_to_char ("Syntax : heal <#xnumber>\n\r", ch);
    return false;
}


REDIT(redit_mana)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (is_number(argument))
       {
          pRoom->rs_mana_rate = atoi (argument);
          send_to_char ("Mana rate set.\n\r", ch);
          return true;
       }

    send_to_char ("Syntax : mana <#xnumber>\n\r", ch);
    return false;
}


REDIT(redit_move)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (is_number(argument))
    {
	pRoom->rs_move_rate = atoi (argument);
	send_to_char ("Movement regen rate set.\n\r", ch);
	return true;
    }

    send_to_char ("Syntax: move <#xnumber>\n\r", ch);
    return false;
}


REDIT(redit_mreset)
{
    ROOM_INDEX_DATA	*pRoom;
    MOB_INDEX_DATA	*pMobIndex;
    CHAR_DATA		*newmob;
    char		arg [ MAX_INPUT_LENGTH ];
    char		arg2 [ MAX_INPUT_LENGTH ];

    RESET_DATA		*pReset;
    char		output [ MAX_STRING_LENGTH ];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || !is_number(arg))
    {
	send_to_char ("Syntax:  mreset <vnum> <max #x> <mix #x>\n\r", ch);
	return false;
    }

    if (!(pMobIndex = get_mob_index(atoi(arg))))
    {
	send_to_char("REdit: No mobile has that vnum.\n\r", ch);
	return false;
    }

    if (pMobIndex->area != pRoom->area)
    {
	send_to_char("REdit: No such mobile in this area.\n\r", ch);
	return false;
    }

    /*
     * Create the mobile reset.
     */
    pReset              = new_reset_data();
    pReset->command	= 'M';
    pReset->arg1	= pMobIndex->vnum;
    pReset->arg2	= is_number(arg2) ? atoi(arg2) : MAX_MOB;
    pReset->arg3	= pRoom->vnum;
    pReset->arg4	= is_number(argument) ? atoi (argument) : 1;
    add_reset(pRoom, pReset, 0/* Last slot*/);

    /*
     * Create the mobile.
     */
    newmob = create_mobile(pMobIndex, false);
    char_to_room(newmob, pRoom);
//    if (HAS_TRIGGER_MOB(newmob, TRIG_REPOP))
	p_percent_trigger(newmob, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
    sprintf(output, "%s (%ld) has been loaded and added to resets.\n\r"
	"There will be a maximum of %ld loaded to this room.\n\r",
	capitalize(pMobIndex->short_descr),
	pMobIndex->vnum,
	pReset->arg2);
    send_to_char(output, ch);
    act("$n has created $N!", ch, newmob, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
    return true;
}


struct wear_type
{
    int	wear_loc;
    int	wear_bit;
};


const struct wear_type wear_table[] =
{
    {	WEAR_NONE,	ITEM_TAKE		},
    {	WEAR_LIGHT,	ITEM_LIGHT		},
    {	WEAR_FINGER_L,	ITEM_WEAR_FINGER	},
    {	WEAR_FINGER_R,	ITEM_WEAR_FINGER	},
    {	WEAR_NECK_1,	ITEM_WEAR_NECK		},
    {	WEAR_NECK_2,	ITEM_WEAR_NECK		},
    {	WEAR_BODY,	ITEM_WEAR_BODY		},
    {	WEAR_HEAD,	ITEM_WEAR_HEAD		},
    {	WEAR_LEGS,	ITEM_WEAR_LEGS		},
    {	WEAR_FEET,	ITEM_WEAR_FEET		},
    {	WEAR_HANDS,	ITEM_WEAR_HANDS		},
    {	WEAR_ARMS,	ITEM_WEAR_ARMS		},
    {	WEAR_SHIELD,	ITEM_WEAR_SHIELD	},
    {	WEAR_ABOUT,	ITEM_WEAR_ABOUT		},
    {	WEAR_WAIST,	ITEM_WEAR_WAIST		},
    {	WEAR_WRIST_L,	ITEM_WEAR_WRIST		},
    {	WEAR_WRIST_R,	ITEM_WEAR_WRIST		},
    {	WEAR_WIELD,	ITEM_WIELD		},
    {	WEAR_HOLD,	ITEM_HOLD		},
    {   WEAR_RING_FINGER, ITEM_WEAR_RING_FINGER },
    {	NO_FLAG,	NO_FLAG			}
};


// Returns the location of the bit that matches the count.
int wear_loc(long bits, int count)
{
    int flag;

    for (flag = 0; wear_table[flag].wear_bit != NO_FLAG; flag++)
    {
        if (IS_SET(bits, wear_table[flag].wear_bit) && --count < 1)
            return wear_table[flag].wear_loc;
    }

    return NO_FLAG;
}


/*****************************************************************************
 Name:		wear_bit
 Purpose:	Converts a wear_loc into a bit.
 Called by:	redit_oreset(olc_act.c).
 ****************************************************************************/
int wear_bit(int loc)
{
    int flag;

    for (flag = 0; wear_table[flag].wear_loc != NO_FLAG; flag++)
    {
        if (loc == wear_table[flag].wear_loc)
            return wear_table[flag].wear_bit;
    }

    return 0;
}


REDIT(redit_oreset)
{
    ROOM_INDEX_DATA	*pRoom;
    OBJ_INDEX_DATA	*pObjIndex;
    OBJ_DATA		*newobj;
    OBJ_DATA		*to_obj;
    CHAR_DATA		*to_mob;
    char		arg1 [ MAX_INPUT_LENGTH ];
    char		arg2 [ MAX_INPUT_LENGTH ];
    int			olevel = 0;

    RESET_DATA		*pReset;
    char		output [ MAX_STRING_LENGTH ];

    EDIT_ROOM(ch, pRoom);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || !is_number(arg1))
    {
	send_to_char ("Syntax:  oreset <vnum> <args>\n\r", ch);
	send_to_char ("        -no_args               = into room\n\r", ch);
	send_to_char ("        -<obj_name>            = into obj\n\r", ch);
	send_to_char ("        -<mob_name> <wear_loc> = into mob\n\r", ch);
	return false;
    }

    if (!(pObjIndex = get_obj_index(atoi(arg1))))
    {
	send_to_char("REdit: No object has that vnum.\n\r", ch);
	return false;
    }

    if (pObjIndex->area != pRoom->area)
    {
	send_to_char("REdit: No such object in this area.\n\r", ch);
	return false;
    }

    /*
     * Load into room.
     */
    if (arg2[0] == '\0')
    {
	pReset		= new_reset_data();
	pReset->command	= 'O';
	pReset->arg1	= pObjIndex->vnum;
	pReset->arg2	= 0;
	pReset->arg3	= pRoom->vnum;
	pReset->arg4	= 0;
	add_reset(pRoom, pReset, 0/* Last slot*/);

	newobj = create_object(pObjIndex, number_fuzzy(olevel), true);
	obj_to_room(newobj, pRoom);

	sprintf(output, "%s (%ld) has been loaded and added to resets.\n\r",
	    capitalize(pObjIndex->short_descr),
	    pObjIndex->vnum);
	send_to_char(output, ch);
    }
    else
    /*
     * Load into object's inventory.
     */
    if (argument[0] == '\0'
    && ((to_obj = get_obj_list(ch, arg2, pRoom->contents)) != NULL))
    {
	pReset		= new_reset_data();
	pReset->command	= 'P';
	pReset->arg1	= pObjIndex->vnum;
	pReset->arg2	= 0;
	pReset->arg3	= to_obj->pIndexData->vnum;
	pReset->arg4	= 1;
	add_reset(pRoom, pReset, 0/* Last slot*/);

	newobj = create_object(pObjIndex, number_fuzzy(olevel), true);
	newobj->cost = 0;
	obj_to_obj(newobj, to_obj);

	sprintf(output, "%s (%ld) has been loaded into "
	    "%s (%ld) and added to resets.\n\r",
	    capitalize(newobj->short_descr),
	    newobj->pIndexData->vnum,
	    to_obj->short_descr,
	    to_obj->pIndexData->vnum);
	send_to_char(output, ch);
    }
    else
    /*
     * Load into mobile's inventory.
     */
    if ((to_mob = get_char_room(ch, NULL, arg2)) != NULL)
    {
	int	wear_loc;

	/*
	 * Make sure the location on mobile is valid.
	 */
	if ((wear_loc = flag_value(wear_loc_flags, argument)) == NO_FLAG)
	{
	    send_to_char("REdit: Invalid wear_loc.  '? wear-loc'\n\r", ch);
	    return false;
	}

	/*
	 * Disallow loading a sword(WEAR_WIELD) into WEAR_HEAD.
	 */
	if (!IS_SET(pObjIndex->wear_flags, wear_bit(wear_loc)))
	{
	    sprintf(output,
	        "%s (%ld) has wear flags: [%s]\n\r",
	        capitalize(pObjIndex->short_descr),
	        pObjIndex->vnum,
		flag_string(wear_flags, pObjIndex->wear_flags));
	    send_to_char(output, ch);
	    return false;
	}

	/*
	 * Can't load into same position.
	 */
	if (get_eq_char(to_mob, wear_loc))
	{
	    send_to_char("REdit:  Object already equipped.\n\r", ch);
	    return false;
	}

	pReset		= new_reset_data();
	pReset->arg1	= pObjIndex->vnum;
	pReset->arg2	= wear_loc;
	if (pReset->arg2 == WEAR_NONE)
	    pReset->command = 'G';
	else
	    pReset->command = 'E';
	pReset->arg3	= wear_loc;

	add_reset(pRoom, pReset, 0/* Last slot*/);

	olevel  = URANGE(0, to_mob->level - 2, LEVEL_HERO);
        newobj = create_object(pObjIndex, number_fuzzy(olevel), true);

#if 0
	if (to_mob->pIndexData->pShop)	/* Shop-keeper? */
	{
	    switch (pObjIndex->item_type)
	    {
	    default:		olevel = 0;				break;
	    case ITEM_PILL:	olevel = number_range( 0, 10);	break;
	    case ITEM_POTION:	olevel = number_range( 0, 10);	break;
	    case ITEM_SCROLL:	olevel = number_range( 5, 15);	break;
	    case ITEM_WAND:	olevel = number_range(10, 20);	break;
	    case ITEM_STAFF:	olevel = number_range(15, 25);	break;
	    case ITEM_TATTOO:	olevel = number_range( 0, 10);	break;
	    case ITEM_ARMOUR:	olevel = number_range( 5, 15);	break;
	    case ITEM_SEED:	olevel = number_range( 5, 15);	break;
	    case ITEM_RANGED_WEAPON:	olevel = number_range( 5, 15);	break;
	    case ITEM_WEAPON:	if (pReset->command == 'G')
	    			    olevel = number_range(5, 15);
				else
				    olevel = number_fuzzy(olevel);
		break;
	    }

	    newobj = create_object(pObjIndex, olevel, true);
	    if (pReset->arg2 == WEAR_NONE)
		SET_BIT(newobj->extra[0], ITEM_INVENTORY);
	}
	else
#endif
	    newobj = create_object(pObjIndex, number_fuzzy(olevel), true);


	obj_to_char(newobj, to_mob);
	if (pReset->command == 'E')
	    equip_char(to_mob, newobj, pReset->arg3);

	sprintf(output, "%s (%ld) has been loaded "
	    "%s of %s (%ld) and added to resets.\n\r",
	    capitalize(pObjIndex->short_descr),
	    pObjIndex->vnum,
	    flag_string(wear_loc_strings, pReset->arg3),
	    to_mob->short_descr,
	    to_mob->pIndexData->vnum);
	send_to_char(output, ch);
    }
    else	/* Display Syntax */
    {
	send_to_char("REdit:  That mobile isn't here.\n\r", ch);
	return false;
    }

    act("$n has created $p!", ch, NULL, NULL, newobj, NULL, NULL, NULL, TO_ROOM);
    return true;
}

REDIT(redit_persist)
{
	ROOM_INDEX_DATA *pRoom;

	EDIT_ROOM(ch, pRoom);


	if (!str_cmp(argument,"on")) {
	    if (ch->tot_level < (MAX_LEVEL-1)) {
			send_to_char("Insufficient security.  Department of Homeland Security has been notified.\n\r", ch);
			return false;
	    }

		persist_addroom(pRoom);
		send_to_char("Persistance enabled.\n\r", ch);
	} else if (!str_cmp(argument,"off")) {
		persist_removeroom(pRoom);
		send_to_char("Persistance disabled.\n\r", ch);
	} else {
		send_to_char("Usage: persist on/off\n\r", ch);
		return false;
	}

	return true;
}


// send obj values to a buffer
void print_obj_values(OBJ_INDEX_DATA *obj, BUFFER *buffer)
{
    char buf[MAX_STRING_LENGTH];

    add_buf(buffer, "\n\r");

    switch(obj->item_type)
    {
	default:	// No values
	    break;
	case ITEM_LIGHT:

            if (obj->value[2] == -1)
		sprintf(buf, "{B[  {Wv2{B]{G Light:{x  Infinite[-1]\n\r");
            else
		sprintf(buf, "{B[  {Wv2{B]{G Light:{x  [%ld]\n\r", obj->value[2]);

	    add_buf(buffer, buf);
	    break;

	case ITEM_WAND:
	case ITEM_STAFF:
            sprintf(buf,
		"{B[  {Wv0{B]{G Level:{x          [%ld]\n\r"
		"{B[  {Wv1{B]{G Charges Total:{x  [%ld]\n\r"
		"{B[  {Wv2{B]{G Charges Left:{x   [%ld]\n\r",
		obj->value[0],
		obj->value[1],
		obj->value[2]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_PORTAL:
		if( IS_SET(obj->value[2], GATE_DUNGEON) )
		{
			// DUNGEON portal
			sprintf(buf,
				"{B[  {Wv0{B]{G Charges:{x           [%ld]\n\r"
				"{B[  {Wv1{B]{G Exit Flags:{x        %s\n\r"
				"{B[  {Wv2{B]{G Portal Flags:{x      %s\n\r"
				"{B[  {Wv3{B]{G Goes to (dungeon):{x [%ld]\n\r"
				"{B[  {Wv4{B]{G Key:{x               [%ld] %s\n\r"
				"{B[  {Wv5{B]{G Goes to (floor):  {x [%ld]\n\r",
				obj->value[0],
				flag_string(portal_exit_flags, obj->value[1]),
				flag_string(portal_flags, obj->value[2]),
				obj->value[3],
				obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				obj->value[5]);
		}
		else if( IS_SET(obj->value[2], GATE_AREARANDOM) || obj->value[3] == -1 )
		{
			// AREARANDOM portal
			sprintf(buf,
				"{B[  {Wv0{B]{G Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
				"{B[  {Wv4{B]{G Key:{x            [%ld] %s\n\r"
				"{B[  {Wv5{B]{G Goes to (area id):{x [%ld]\n\r",
				obj->value[0],
				flag_string(portal_exit_flags, obj->value[1]),
				flag_string(portal_flags, obj->value[2]),
				obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				obj->value[5]);
		}
		else if(obj->value[3] > 0)
		{
			// STATIC portal
			sprintf(buf,
				"{B[  {Wv0{B]{G Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
				"{B[  {Wv3{B]{G Goes to (vnum):{x [%ld]\n\r"
				"{B[  {Wv4{B]{G Key:{x            [%ld] %s\n\r",
				obj->value[0],
				flag_string(portal_exit_flags, obj->value[1]),
				flag_string(portal_flags, obj->value[2]),
				obj->value[3],
				obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none");
		}
		else
		{
			// WILDERNESS portal
			sprintf(buf,
				"{B[  {Wv0{B]{G Charges:{x        [%ld]\n\r"
				"{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
				"{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
				"{B[  {Wv4{B]{G Key:{x            [%ld] %s\n\r"
				"{B[  {Wv5{B]{G Goes to (map):{x  [%ld]\n\r"
				"{B[  {Wv6{B]{G Goes to (mapx):{x [%ld]\n\r"
				"{B[  {Wv7{B]{G Goes to (mapy):{x [%ld]\n\r",
				obj->value[0],
				flag_string(portal_exit_flags, obj->value[1]),
				flag_string(portal_flags, obj->value[2]),
				obj->value[4], get_obj_index(obj->value[4]) ? get_obj_index(obj->value[4])->short_descr : "none",
				obj->value[5], obj->value[6],obj->value[7]);
		}
	    add_buf(buffer, buf);
	    break;

	case ITEM_FURNITURE:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Max people:{x      [%ld]\n\r"
	        "{B[  {Wv1{B]{G Max weight:{x      [%ld]\n\r"
	        "{B[  {Wv2{B]{G Furniture Flags:{x %s\n\r"
	        "{B[  {Wv3{B]{G Heal bonus:{x      [%ld]\n\r"
	        "{B[  {Wv4{B]{G Mana bonus:{x      [%ld]\n\r"
		"{B[  {Wv5{B]{G Move bonus:{x      [%ld]\n\r",
	        obj->value[0],
	        obj->value[1],
	        flag_string(furniture_flags, obj->value[2]),
	        obj->value[3],
	        obj->value[4],
		obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_HERB:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Type:{x            [%s]\n\r"
		"{B[  {Wv1{B]{G Healing:{x         [%ld%%]\n\r"
		"{B[  {Wv2{B]{G Regenerative:{x    [%ld%%]\n\r"
		"{B[  {Wv3{B]{G Refreshing:{x      [%ld%%]\n\r"
		"{B[  {Wv4{B]{G Immunity:{x        [%s]\n\r"
		"{B[  {Wv5{B]{G Resistance:{x      [%s]\n\r"
		"{B[  {Wv6{B]{G Vulnerability:{x   [%s]\n\r"
		"{B[  {Wv7{B]{G Spell:{x           [%s]\n\r",
		herb_table[obj->value[0]].name,
		obj->value[1],
		obj->value[2],
		obj->value[3],
		flag_string(imm_flags, obj->value[4]),
		flag_string(res_flags, obj->value[5]),
		flag_string(vuln_flags, obj->value[6]),
		skill_table[obj->value[7]].name);

	    add_buf(buffer, buf);
	    break;

	case ITEM_SCROLL:
	case ITEM_PILL:
	    break;

	case ITEM_POTION:
        sprintf(buf,
            	"{B[  {Wv5{B]{G Charges:{x                [%ld]\n\r",
            	obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TATTOO:
            sprintf(buf,
            		"{B[  {Wv0{B]{G Touches:{x                [%ld]\n\r"
            		"{B[  {Wv1{B]{G Chance of Fading:{x       [%ld]\n\r",
            		obj->value[0],obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_INK:
            sprintf(buf, "{B[  {Wv0{B]{G Type 1:{x                 [%s]\n\r", flag_string(catalyst_types, obj->value[0]));
	    add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv1{B]{G Type 2:{x                 [%s]\n\r", flag_string(catalyst_types, obj->value[1]));
	    add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv2{B]{G Type 3:{x                 [%s]\n\r", flag_string(catalyst_types, obj->value[2]));
	    add_buf(buffer, buf);
	    break;

	case ITEM_SEXTANT:
            sprintf(buf,
		"{B[  {Wv0{B]{G Percentage of working:{x  [%ld]\n\r",
		obj->value[0]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_SEED:
            sprintf(buf,
		"{B[  {Wv0{B]{G Time before growth:{x     [%ld]\n\r"
		"{B[  {Wv1{B]{G Turns into object vnum:{x [%ld]\n\r",
		obj->value[0],
		obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_ARMOUR:
	    sprintf(buf,
		"{B[  {Wv0{B] {GAc pierce       {x[%ld]\n\r"
		"{B[  {Wv1{B] {GAc bash         {x[%ld]\n\r"
		"{B[  {Wv2{B] {GAc slash        {x[%ld]\n\r"
		"{B[  {Wv3{B] {GAc exotic       {x[%ld]\n\r"
		"{B[  {Wv4{B] {GArmour strength  {x%s\n\r",
		obj->value[0],
		obj->value[1],
		obj->value[2],
		obj->value[3],
		armour_strength_table[obj->value[4]].name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_ARTIFACT:
	    break;

	case ITEM_RANGED_WEAPON:
            sprintf(buf, "{B[  {Wv0{B]{G Ranged Weapon class:{x   %s\n\r",
		     flag_string(ranged_weapon_class, obj->value[0]));
	    add_buf(buffer, buf);

	    sprintf(buf, "{B[  {Wv1{B]{G Number of dice:{x [%ld]\n\r", obj->value[1]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv2{B]{G Type of dice:{x   [%ld]\n\r", obj->value[2]);
	    add_buf(buffer, buf);

	    sprintf(buf, "{B[  {Wv3{B]{G Projectile Distance:{x [%ld]\n\r", obj->value[3]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_WEAPON:
            sprintf(buf, "{B[  {Wv0{B]{G Weapon class:{x   %s\n\r",
		     flag_string(weapon_class, obj->value[0]));
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{G Number of dice:{x [%ld]\n\r", obj->value[1]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv2{B]{G Type of dice:{x   [%ld]\n\r", obj->value[2]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv3{B]{G Type:{x           %s\n\r",
		    attack_table[obj->value[3]].name);
	    add_buf(buffer, buf);
 	    sprintf(buf, "{B[  {Wv4{B]{G Special type:{x   %s\n\r",
		     flag_string(weapon_type2,  obj->value[4]));
	    add_buf(buffer, buf);
	    break;

	case ITEM_SHIP:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{G Move delay:{x [%ld]\n\r"
		"{B[  {Wv2{B]{G Min Crew:{x   [%ld]\n\r"
		"{B[  {Wv3{B]{G Capacity:{x   [%ld]\n\r"
		"{B[  {Wv4{B]{G Max Crew:{x   [%ld]\n\r"
		"{B[  {Wv5{B]{G First Room:{x [%ld]\n\r"
		"{B[  {Wv6{B]{G Hit Points:{x [%ld]\n\r"
		"{B[  {Wv7{B]{G Max Guns:{x   [%ld]\n\r",
		obj->value[0],
		obj->value[1],
		obj->value[2],
                obj->value[3],
                obj->value[4],
                obj->value[5],
                obj->value[6],
                obj->value[7]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CART:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{G Move delay:{x [%ld]\n\r"
		"{B[  {Wv2{B]{G Strength:{x   [%ld]\n\r"
		"{B[  {Wv3{B]{G Capacity:{x    [%ld]\n\r"
		"{B[  {Wv4{B]{G Weight Mult:{x [%ld]\n\r",
		obj->value[0],
		obj->value[1],
		obj->value[2],
                obj->value[3],
                obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TRADE_TYPE:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Trade Type:{x     [%s]\n\r",
		trade_table[ obj->value[0] ].name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CONTAINER:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{G Flags:{x      [%s]\n\r"
		"{B[  {Wv2{B]{G Key:{x     %s [%ld]\n\r"
		"{B[  {Wv3{B]{G Capacity:{x    [%ld]\n\r"
		"{B[  {Wv4{B]{G Weight Mult:{x [%ld]\n\r",
		obj->value[0],
		flag_string(container_flags, obj->value[1]),
                get_obj_index(obj->value[2])
                    ? get_obj_index(obj->value[2])->short_descr
                    : "none",
                obj->value[2],
                obj->value[3],
                obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_WEAPON_CONTAINER:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Weight:{x     [%ld kg]\n\r"
		"{B[  {Wv1{B]{G Weapon Type:{x [%s]\n\r"
		"{B[  {Wv3{B]{G Capacity:{x   [%ld]\n\r"
		"{B[  {Wv4{B]{G Weight Mult:{x[%ld]\n\r",
		obj->value[0],
		flag_string(weapon_class, obj->value[1]),
                obj->value[3],
                obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_DRINK_CON:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Liquid Total:{x [%ld]\n\r"
	        "{B[  {Wv1{B]{G Liquid Left:{x  [%ld]\n\r"
	        "{B[  {Wv2{B]{G Liquid:{x       %s\n\r"
	        "{B[  {Wv3{B]{G Poisoned:{x     %s\n\r",
	        obj->value[0],
	        obj->value[1],
	        liq_table[obj->value[2]].liq_name,
	        obj->value[3] != 0 ? "Yes" : "No");
	    add_buf(buffer, buf);
	    break;

	case ITEM_FOUNTAIN:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Liquid Total:{x [%ld]\n\r"
	        "{B[  {Wv1{B]{G Liquid Left:{x  [%ld]\n\r"
	        "{B[  {Wv2{B]{G Liquid:{x     %s\n\r",
	        obj->value[0],
	        obj->value[1],
	        liq_table[obj->value[2]].liq_name);
	    add_buf(buffer, buf);
	    break;

	case ITEM_FOOD:
	    sprintf(buf,
		"{B[  {Wv0{B]{G Food hours:{x [%ld]\n\r"
		"{B[  {Wv1{B]{G Full hours:{x [%ld]\n\r"
		"{B[  {Wv3{B]{G Poisoned  :{x  %s\n\r"
		"{B[  {Wv4{B]{G Timer     :{x [%ld]\n\r",
		obj->value[0],
		obj->value[1],
		obj->value[3] != 0 ? "Yes" : "No",
		obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_MONEY:
            sprintf(buf, "{B[  {Wv0{B]{G Silver:{x [%ld]\n\r", obj->value[0]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{G Gold:{x   [%ld]\n\r", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

        case ITEM_MIST:
	    sprintf(buf, "{B[  {Wv0{B]{G %%HideObjects:{x    [%ld]\n\r", obj->value[0]);
	    add_buf(buffer, buf);
	    sprintf(buf, "{B[  {Wv1{B]{G %%HideCharacters:{x [%ld]\n\r", obj->value[1]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_CORPSE_NPC:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Type:{x           %s\n\r"
	        "{B[  {Wv1{B]{G Resurrection:{x   %d%%\n\r"
	        "{B[  {Wv2{B]{G Animation:{x      %d%%\n\r"
	        "{B[  {Wv3{B]{G Body Parts:{x     %s\n\r"
	        "{B[  {Wv5{B]{G Mobile (vnum):{x  %d\n\r",
	        flag_string(corpse_types,obj->value[0]),
	        (int)obj->value[1],(int)obj->value[2],
	        flag_string(part_flags, obj->value[3]),
	        (int)obj->value[5]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_INSTRUMENT:
	    sprintf(buf,
	        "{B[  {Wv0{B]{G Type:{x            %s\n\r"
	        "{B[  {Wv1{B]{G Flags:{x           %s\n\r"
	        "{B[  {Wv2{B]{G Min Time Factor:{x %ld%%\n\r"
	        "{B[  {Wv3{B]{G Max Time Factor:{x %ld%%\n\r",
	        flag_string(instrument_types, obj->value[0]),
	        flag_string(instrument_flags, obj->value[1]),
	        obj->value[2],obj->value[3]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_BOOK:
	    sprintf(buf,
		"{B[  {Wv1{B]{G Flags:{x      [%s]\n\r"
		"{B[  {Wv2{B]{G Key:{x     %s [%ld]\n\r",
		flag_string(container_flags, obj->value[1]),
                get_obj_index(obj->value[2])
                    ? get_obj_index(obj->value[2])->short_descr
                    : "none",
                obj->value[2]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_TELESCOPE:
		if( obj->value[4] < 0 )
			sprintf(buf,
				"{B[  {Wv0{B]{G Current Distance:{x  [%ld]\n\r"
				"{B[  {Wv1{B]{G Minimum Distance:{x  [%ld]\n\r"
				"{B[  {Wv2{B]{G Maximum Distance:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{G Bonusview Size:{x    [%ld]\n\r"
				"{B[  {Wv4{B]{G Current Heading:{x   [none]\n\r",
					obj->value[0],
					obj->value[1],
					obj->value[2],
					obj->value[3]);
		else
			sprintf(buf,
				"{B[  {Wv0{B]{G Current Distance:{x  [%ld]\n\r"
				"{B[  {Wv1{B]{G Minimum Distance:{x  [%ld]\n\r"
				"{B[  {Wv2{B]{G Maximum Distance:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{G Bonusview Size:{x    [%ld]\n\r"
				"{B[  {Wv4{B]{G Current Heading:{x   [%ld]\n\r",
					obj->value[0],
					obj->value[1],
					obj->value[2],
					obj->value[3],
					obj->value[4]);
	    add_buf(buffer, buf);
	    break;

	case ITEM_COMPASS:
		if( obj->value[1] > 0 )
		{
			WILDS_DATA *pWilds = get_wilds_from_uid(NULL,obj->value[1]);

			sprintf(buf,
				"{B[  {Wv0{B]{G Accuracy:{x      [%ld]\n\r"
				"{B[  {Wv1{B]{G Wilderness:{x    [%ld] %s\n\r"
				"{B[  {Wv2{B]{G X Coordinate:{x  [%ld]\n\r"
				"{B[  {Wv3{B]{G Y Coordinate:{x  [%ld]\n\r",
					obj->value[0],
					obj->value[1], (pWilds?pWilds->name:"???"),
					obj->value[2],
					obj->value[3]);
		}
		else
		{
			sprintf(buf,
				"{B[  {Wv0{B]{G Accuracy:{x      [%ld]\n\r"
				"{B[  {Wv1{B]{G Wilderness:{x    [none]\n\r",
					obj->value[0]);
		}
	    add_buf(buffer, buf);
		break;

	case ITEM_BODY_PART:
		sprintf(buf,
				"{B[  {Wv0{B]{G Body Parts:{x    %s\n\r"
				"{B[  {Wv1{B]{G Race:{x          %s\n\r",
				flag_string(part_flags, obj->value[0]),
				race_table[obj->value[1]].name);

		add_buf(buffer, buf);
		break;
    }
}


bool set_obj_values(CHAR_DATA *ch, OBJ_INDEX_DATA *pObj, int value_num, char *argument)
{
	long i = 0;
	BUFFER *buffer;
	char buf[MSL];

	switch(pObj->item_type)
	{
	default:
		break;

	case ITEM_LIGHT:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_LIGHT");
			return false;
		case 2:
			send_to_char("HOURS OF LIGHT SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("Spell level set.\n\r\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 4:
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[4] = skill_lookup(argument);
			break;
		case 5:
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[5] = skill_lookup(argument);
			break;
		}
		break;

	case ITEM_WAND:
	case ITEM_STAFF:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_STAFF_WAND");
			return false;
		case 0:
			send_to_char("SPELL LEVEL SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("TOTAL NUMBER OF CHARGES SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("CURRENT NUMBER OF CHARGES SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("SPELL TYPE SET.\n\r", ch);
			pObj->value[3] = skill_lookup(argument);
			break;
		}
		break;

	case ITEM_SCROLL:
	case ITEM_PILL:
		break;

	case ITEM_POTION:
	switch (value_num)
	{
		default:
			do_help(ch, "ITEM_POTION");
			return false;
		case 5:
			send_to_char("TOTAL CHARGES SET\n\r\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
	}
	break;

	case ITEM_TATTOO:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_TATTOO");
			return false;
		case 0:
			send_to_char("TOUCHES SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("FADING CHANCE SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		}
		break;

	case ITEM_INK:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_INK");
			return false;
		case 0:
			send_to_char("TYPE 1 SET.\n\r\n\r", ch);
			pObj->value[0] = flag_lookup(argument,catalyst_types);
			break;
		case 1:
			send_to_char("TYPE 2 SET.\n\r\n\r", ch);
			pObj->value[1] = flag_lookup(argument,catalyst_types);
			break;
		case 2:
			send_to_char("TYPE 3 SET.\n\r\n\r", ch);
			pObj->value[2] = flag_lookup(argument,catalyst_types);
			break;
		}
		break;

	case ITEM_SEXTANT:
		switch(value_num)
		{
		default:
			do_help(ch, "ITEM_SEXTANT");
			return false;
		case 0:
			send_to_char("Accuracy set.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		}
		break;

	case ITEM_SEED:
		switch(value_num)
		{
		default:
			do_help(ch, "ITEM_SEED");
			return false;
		case 0:
			send_to_char("Time set.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			if (atoi(argument) != 0)
			{
				if (!get_obj_index(atoi(argument)))
				{
					send_to_char("No such object exists.\n\r\n\r", ch);
					return false;
				}
			}
			send_to_char("Vnum set.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		}
		break;

/*	case ITEM_ARTIFACT:
		switch(value_num)
		{
		case 0:
			// TODO: UNUSED?
			if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}

			send_to_char("SPELL LEVEL SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			// TODO: UNUSED?
			if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}

			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[1] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		case 2:
			// TODO: UNUSED?
			if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}

			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[2] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		}
		break;*/

	case ITEM_ARMOUR:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_ARMOUR");
			return false;
		case 0:
			send_to_char("AC PIERCE SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("AC BASH SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("AC SLASH SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("AC EXOTIC SET.\n\r\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 4:
			send_to_char("ARMOUR STRENGTH SET.\n\r", ch);
			send_to_char("ARMOUR CLASS SET.\n\r\n\r", ch);

			pObj->value[4] = get_armour_strength(argument);

			set_armour(pObj);

			break;
		case 5:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}

			send_to_char("SPELL LEVEL SET.\n\r\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
		case 6:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[6] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		case 7:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[7] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		}
		break;

	case ITEM_RANGED_WEAPON:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_RANGED_WEAPON");
			return false;
		case 0:
			send_to_char("RANGED WEAPON CLASS SET.\n\r\n\r", ch);
			pObj->value[0] = flag_value(ranged_weapon_class, argument);
			break;
		case 1:
			send_to_char("NUMBER OF DICE SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("TYPE OF DICE SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("PROJECTILE DISTANCE SET.\n\r\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 5:
			// TODO: UNUSED?
			send_to_char("Spell level set.\n\r\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
		case 6:
			// TODO: UNUSED?
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[6] = skill_lookup(argument);
			break;
		case 7:
			// TODO: UNUSED?
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[7] = skill_lookup(argument);
			break;
		}
		break;

	case ITEM_HERB:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_HERB");
			return false;
		case 0:
			for (i = 0; i < MAX_HERB; i++)
			{
				if (!str_prefix(argument, herb_table[i].name))
				break;
			}

			if (i < MAX_HERB)
			{
				pObj->value[0] = i;
				send_to_char("HERB TYPE SET.\n\r", ch);
			}
			else
				send_to_char("Invalid herb type.\n\r", ch);
			break;
		case 1:
			send_to_char("HEALING RATE SET.\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("REGENERATIVE RATE SET.\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("REFRESHING RATE SET.\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 4:
			if ((i = flag_value(imm_flags, argument)) != NO_FLAG)
			{
				pObj->value[4] ^= i;
				send_to_char("IMMUNITY SET.\n\r", ch);
			}
			else
				send_to_char("Invalid immunity.\n\r", ch);
			break;
		case 5:
			if ((i = flag_value(res_flags, argument)) != NO_FLAG)
			{
				pObj->value[5] ^= i;
				send_to_char("RESISTANCE SET.\n\r", ch);
			}
			else
			send_to_char("Invalid resistance.\n\r", ch);
			break;
		case 6:
			if ((i = flag_value(vuln_flags, argument)) != NO_FLAG)
			{
				pObj->value[6] ^= i;
				send_to_char("VULNERABILITY SET.\n\r", ch);
			}
			else
				send_to_char("Invalid vulnerability.\n\r", ch);
			break;
		case 7:
			// TODO: UNUSED?
			if ((i = skill_lookup(argument)) > 0 && skill_table[i].spell_fun != spell_null)
			{
				send_to_char("SPELL SET.\n\r", ch);
				pObj->value[7] = i;
			}
			else if (i == 0)
			{
				send_to_char("SPELL RESET.\n\r", ch);
				pObj->value[7] = 0;
			}
			else
				send_to_char("INVALID ARGUMENT.\n\r", ch);

			break;
		}

		break;

	case ITEM_WEAPON:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_WEAPON");
			return false;
		case 0:
			send_to_char("WEAPON CLASS SET.\n\r\n\r", ch);
			pObj->value[0] = flag_value(weapon_class, argument);
			break;
		case 1:
			send_to_char("NUMBER OF DICE SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("TYPE OF DICE SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("WEAPON TYPE SET.\n\r\n\r", ch);
			pObj->value[3] = attack_lookup(argument);
			break;
		case 4:
			send_to_char("SPECIAL WEAPON TYPE TOGGLED.\n\r\n\r", ch);
			pObj->value[4] ^= (flag_value(weapon_type2, argument) != NO_FLAG ? flag_value(weapon_type2, argument) : 0);
			break;
		case 5:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}
			send_to_char("Spell level set.\n\r\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
		case 6:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[6] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		case 7:
			// TODO: UNUSED?
			if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("You can't do this without an IMP's permission.\n\r", ch);
				return false;
			}
			send_to_char("SPELL SET.\n\r\n\r", ch);
			pObj->value[7] = skill_lookup(argument);
			use_imp_sig(NULL, pObj);
			break;
		}
		break;

	case ITEM_PORTAL:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_PORTAL");
			return false;
		case 0:
			send_to_char("CHARGES SET.\n\r\n\r", ch);
			pObj->value[0] = atoi (argument);
			break;
		case 1:
			send_to_char("EXIT (PORTAL) FLAGS SET.\n\r\n\r", ch);
			pObj->value[1] ^= (flag_value(portal_exit_flags, argument) != NO_FLAG ? flag_value(portal_exit_flags, argument) : 0);
			break;
		case 2:
			{
				send_to_char("PORTAL FLAGS SET.\n\r\n\r", ch);
				int flags = flag_value(portal_flags, argument);

				if( flags != NO_FLAG )
				{
					pObj->value[2] ^= flags;

					if( IS_SET(pObj->value[2], GATE_DUNGEON) )
					{
						REMOVE_BIT(pObj->value[2], GATE_AREARANDOM);
					}

					if( IS_SET(flags, GATE_DUNGEON) && IS_SET(pObj->value[2], GATE_DUNGEON) )
					{
						pObj->value[3] = 0;
						pObj->value[4] = 0;
						pObj->value[5] = 0;
						pObj->value[6] = 0;
						pObj->value[7] = 0;
					}
					else if( IS_SET(flags, GATE_AREARANDOM) && IS_SET(pObj->value[2], GATE_AREARANDOM) )
					{
					}

				}
			}
			break;
		case 3:
			if( IS_SET(pObj->value[2], GATE_DUNGEON) )
			{
				if( !get_dungeon_index(atoi(argument)) )
				{
					send_to_char("THERE IS NO SUCH DUNGEON.\n\r\n\r", ch);
					return false;
				}
				send_to_char("DUNGEON VNUM SET.\n\r\n\r", ch);
			}
			else
				send_to_char("EXIT VNUM SET.\n\r\n\r", ch);
			pObj->value[3] = atoi (argument);
			break;
		case 4:
			if (atoi(argument) != 0)
			{
				if (!get_obj_index(atoi(argument)))
				{
					send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
					return false;
				}

				if (get_obj_index(atoi(argument))->item_type != ITEM_KEY)
				{
					send_to_char("THAT ITEM IS NOT A KEY.\n\r\n\r", ch);
					return false;
				}
			}
			send_to_char("PORTAL KEY SET.\n\r\n\r", ch);
			pObj->value[4] = atoi(argument);
			break;
		case 5:
			if( IS_SET(pObj->value[2], GATE_DUNGEON) )
			{
				send_to_char("DUNGEON FLOOR SET.\n\r\n\r", ch);
			}
			else if( IS_SET(pObj->value[2], GATE_AREARANDOM) || pObj->value[3] == -1 )
			{
				send_to_char("AREA ID SET.\n\r\n\r", ch);
			}
			else if( !IS_SET(pObj->value[2], GATE_DUNGEON) )
			{
				send_to_char("WILDERNESS MAP UID SET.\n\r\n\r", ch);
			}
			pObj->value[5] = atoi (argument);
			break;
		case 6:
			if( !IS_SET(pObj->value[2], GATE_DUNGEON) && !IS_SET(pObj->value[2], GATE_AREARANDOM) && !pObj->value[3] )
			{
				send_to_char("WILDERNESS MAP X-COORDINATE SET.\n\r\n\r", ch);
				pObj->value[6] = atoi (argument);
			}
			break;
		case 7:
			if( !IS_SET(pObj->value[2], GATE_DUNGEON) && !IS_SET(pObj->value[2], GATE_AREARANDOM) && !pObj->value[3] )
			{
				send_to_char("WILDERNESS MAP Y-COORDINATE SET.\n\r\n\r", ch);
				pObj->value[7] = atoi (argument);
			}
			break;
		}
		break;

	case ITEM_FURNITURE:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_FURNITURE");
			return false;
		case 0:
			send_to_char("NUMBER OF PEOPLE SET.\n\r\n\r", ch);
			pObj->value[0] = atoi (argument);
			break;
		case 1:
			send_to_char("MAX WEIGHT SET.\n\r\n\r", ch);
			pObj->value[1] = atoi (argument);
			break;
		case 2:
			send_to_char("FURNITURE FLAGS TOGGLED.\n\r\n\r", ch);
			pObj->value[2] ^= (flag_value(furniture_flags, argument) != NO_FLAG ? flag_value(furniture_flags, argument) : 0);
			break;
		case 3:
			send_to_char("HEAL BONUS SET.\n\r\n\r", ch);
			pObj->value[3] = atoi (argument);
			break;
		case 4:
			send_to_char("MANA BONUS SET.\n\r\n\r", ch);
			pObj->value[4] = atoi (argument);
			break;
		case 5:
			send_to_char("MOVE BONUS SET.\n\r\n\r", ch);
			pObj->value[5] = atoi (argument);
			break;
		}
		break;

/*	case ITEM_SHIP:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_SHIP");
			return false;
		case 0:
			send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("MOVE DELAY SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("MIN CREW SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			send_to_char("CAPACITY SET.\n\r\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 4:
			send_to_char("MAX CREW SET.\n\r\n\r", ch);
			pObj->value[4] = atoi(argument);
			break;
		case 5:
			if (atoi(argument) != 0)
			{
				if (!get_room_index(atoi(argument)))
				{
					send_to_char("THERE IS NO SUCH ROOM.\n\r\n\r", ch);
					return false;
				}
			}
			send_to_char("BOARDING ROOM SET.\n\r\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
		case 6:
			send_to_char("HIT POINTS SET.\n\r", ch);
			pObj->value[6] = atoi(argument);
			break;
		case 7:
			send_to_char("MAX GUNS SET.\n\r\n\r", ch);
			pObj->value[7] = atoi (argument);
			break;
		}
		break;*/

	case ITEM_CART:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_CART");
			return false;
		case 0:
			send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
			pObj->value[0] = atol(argument);
			break;
		case 1:
			send_to_char("DELAY SET.\n\r\n\r", ch);
			pObj->value[1] = atol(argument);
			break;
		case 2:
			send_to_char("STRENGTH SET.\n\r\n\r", ch);
			pObj->value[2] = atol(argument);
			break;
		case 3:
			send_to_char("CART MAX WEIGHT SET.\n\r", ch);
			pObj->value[3] = atol(argument);
			break;
		case 4:
			send_to_char("WEIGHT MULTIPLIER SET.\n\r\n\r", ch);
			pObj->value[4] = atol (argument);
			break;
		case 5:
			send_to_char("VANISH TIME SET.\n\r\n\r", ch);
			pObj->value[5] = atol (argument);
			break;
		}
		break;

	case ITEM_TRADE_TYPE:
		switch(value_num)
		{
		default:
			do_help(ch, "ITEM_TRADE_TYPE");
			return false;

		case 0:
			if ((argument[0] == '\0') || ((i = get_trade_item(argument)) == 0))
			{
				send_to_char("Trade Types:\n\r", ch);

				while(trade_table[ i ].trade_type != -1)
				{
					send_to_char(trade_table[ i ].name, ch);
					send_to_char("\n\r", ch);
					i++;
				}
				break;
			}

			pObj->value[0] = i;
			send_to_char("Trade type set.\n\r", ch);
			break;
		}
		break;

	case ITEM_WEAPON_CONTAINER:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_WEAPON_CONTAINER");
			return false;
		case 0:
			send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			pObj->value[1] = flag_value(weapon_class, argument);
			send_to_char("WEAPON TYPE SET.\n\r\n\r", ch);
			break;
		case 3:
			send_to_char("CONTAINER MAX ITEMS SET.\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;
		case 4:
			send_to_char("WEIGHT MULTIPLIER SET.\n\r\n\r", ch);
			pObj->value[4] = atoi (argument);
			break;
		}
		break;

	case ITEM_CONTAINER:
		switch (value_num)
		{
		int value;

		default:
			do_help(ch, "ITEM_CONTAINER");
			return false;
		case 0:
			send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			if ((value = flag_value(container_flags, argument)) != NO_FLAG)
				TOGGLE_BIT(pObj->value[1], value);
			else
			{
				do_help (ch, "ITEM_CONTAINER");
				return false;
			}
			send_to_char("CONTAINER TYPE SET.\n\r\n\r", ch);
			break;
		case 2:
			if (atoi(argument) != 0)
			{
				if (!get_obj_index(atoi(argument)))
				{
					send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
					return false;
				}

				if (get_obj_index(atoi(argument))->item_type != ITEM_KEY)
				{
					send_to_char("THAT ITEM IS NOT A KEY.\n\r\n\r", ch);
					return false;
				}
			}
			send_to_char("CONTAINER KEY SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			if (atoi (argument) > 225 && ch->tot_level < MAX_LEVEL)
			{
				send_to_char("Sorry, that value is out of range.\n\r", ch);
				return false;
			}

			send_to_char("CONTAINER MAX ITEMS SET.\n\r", ch);
			pObj->value[3] = atoi(argument);
			break;

		case 4:
			if(atoi(argument) <= 0 || atoi(argument) > 1000)
			{
				send_to_char("Weight multiplier must be between 1 and 1000.\n\r",  ch);
				return false;
			}

			if (atoi(argument) < 1000 && !has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL) {
				send_to_char("An imp sig is required to set the weight multiplier below 100%.\n\r", ch);
				return false;
			}

			if (has_imp_sig(NULL, pObj))
				use_imp_sig(NULL, pObj);

			send_to_char("WEIGHT MULTIPLIER SET.\n\r\n\r", ch);
			pObj->value[4] = atoi (argument);
			break;
		}
		break;

	case ITEM_DRINK_CON:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_DRINK");
			return false;
		case 0:
			send_to_char("MAXIMUM AMOUT OF LIQUID HOURS SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("CURRENT AMOUNT OF LIQUID HOURS SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("LIQUID TYPE SET.\n\r\n\r", ch);
			pObj->value[2] = (liq_lookup(argument) != -1 ? liq_lookup(argument) : 0);
			break;
		case 3:
			send_to_char("POISON VALUE TOGGLED.\n\r\n\r", ch);
			pObj->value[3] = (pObj->value[3] == 0) ? 1 : 0;
			break;
		}
		break;

	case ITEM_FOUNTAIN:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_FOUNTAIN");
			return false;
		case 0:
			send_to_char("MAXIMUM AMOUT OF LIQUID HOURS SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("CURRENT AMOUNT OF LIQUID HOURS SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("LIQUID TYPE SET.\n\r\n\r", ch);
			pObj->value[2] = (liq_lookup(argument) != -1 ? liq_lookup(argument) : 0);
			break;
		}
		break;

	case ITEM_FOOD:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_FOOD");
			return false;
		case 0:
			send_to_char("HOURS OF FOOD SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("HOURS OF FULL SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 3:
			send_to_char("POISON VALUE TOGGLED.\n\r\n\r", ch);
			pObj->value[3] = (pObj->value[3] == 0) ? 1 : 0;
			break;
		case 4:
			send_to_char("TIMER TO DISAPPEAR SET.\n\r\n\r", ch);
			pObj->value[4] = atoi(argument);
			break;
		}
		break;

	case ITEM_MONEY:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_MONEY");
			return false;
		case 0:
			send_to_char("SILVER AMOUNT SET.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("GOLD AMOUNT SET.\n\r\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		}
		break;

	case ITEM_MIST:
		switch (value_num)
		{
		default:
			do_help(ch, "ITEM_MIST");
			return false;
		case 0:
			send_to_char("PERCENTAGE TO HIDE OBJECTS SET.\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			send_to_char("PERCENTAGE TO HIDE CHARACTERS SET.\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		}
		break;

	case ITEM_CORPSE_NPC:
		switch (value_num)
		{
		int value;
		default:
			do_help(ch, "ITEM_CORPSE_NPC");
			return false;
		case 0:
			if ((value = flag_value(corpse_types, argument)) == NO_FLAG)
				return false;
			send_to_char("CORPSE TYPE SET.\n\r", ch);
			pObj->value[0] = value;
			break;
		case 1:
			send_to_char("RESURRECTION CHANCE SET.\n\r", ch);
			pObj->value[1] = atoi(argument);
			break;
		case 2:
			send_to_char("ANIMATION CHANCE SET.\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		case 3:
			if ((value = flag_value(part_flags, argument)) == NO_FLAG)
				return false;
			send_to_char("BODY PARTS SET.\n\r", ch);
			pObj->value[3] = value;
			break;
		case 5:
			send_to_char("MOBILE INDEX VNUM SET.\n\r", ch);
			pObj->value[5] = atoi(argument);
			break;
		}
		break;

	case ITEM_INSTRUMENT:
		switch (value_num)
		{
		int value;
		default:
			do_help(ch,"ITEM_INSTRUMENT");
			return false;
		case 0:
			if ((value = flag_value(instrument_types, argument)) == NO_FLAG)
				return false;
			send_to_char("INSTRUMENT TYPE SET.\n\r", ch);
			pObj->value[0] = value;
			break;
		case 1:
			if ((value = flag_value(instrument_flags, argument)) == NO_FLAG)
				return false;
			send_to_char("INSTRUMENT FLAGS TOGGLED.\n\r", ch);
			pObj->value[1] ^= value;
			break;
		case 2:
			value = atoi(argument);
			if( value < 1 || value > 5000)
			{
				send_to_char("Minimum scale factor for playtime can only be between 1% and 5000%.\n\r", ch);
				return false;
			}
			send_to_char("MINIMUM PLAYTIME SCALE FACTOR SET.\n\r", ch);
			pObj->value[2] = value;
			break;
		case 3:
			value = atoi(argument);
			if( value < 1 || value > 5000)
			{
				send_to_char("Maximum scale factor for playtime can only be between 1% and 5000%.\n\r", ch);
				return false;
			}
			send_to_char("MAXIMUM PLAYTIME SCALE FACTOR SET.\n\r", ch);
			pObj->value[3] = value;
			break;
		}
		break;

	case ITEM_BOOK:
		switch (value_num)
		{
		int value;

		default:
			do_help(ch, "ITEM_BOOK");
			return false;
		case 1:
			if ((value = flag_value(container_flags, argument)) != NO_FLAG)
				TOGGLE_BIT(pObj->value[1], value);
			else
			{
				do_help (ch, "ITEM_BOOK");
				return false;
			}
			send_to_char("BOOK (CONTAINER) FLAGS SET.\n\r\n\r", ch);
			break;
		case 2:
			if (atoi(argument) != 0)
			{
				if (!get_obj_index(atoi(argument)))
				{
					send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
					return false;
				}

				if (get_obj_index(atoi(argument))->item_type != ITEM_KEY)
				{
					send_to_char("THAT ITEM IS NOT A KEY.\n\r\n\r", ch);
					return false;
				}
			}
			send_to_char("BOOK KEY SET.\n\r\n\r", ch);
			pObj->value[2] = atoi(argument);
			break;
		}
		break;

	case ITEM_TELESCOPE:
		switch (value_num)
		{
		int value;

		default:
			do_help(ch, "ITEM_TELESCOPE");
			return false;
		case 0:
			value = atoi(argument);
			if( value < 0 || (value > 0 && value < pObj->value[1]) || value > pObj->value[2] )
			{
				sprintf(buf, "TELESCOPE DISTANCE must be 0(for collapsed), or from %ld to %ld.\n\r", pObj->value[1], pObj->value[2]);
				send_to_char(buf, ch);
				return false;
			}
			pObj->value[0] = value;
			send_to_char("TELESCOPE DISTANCE SET\n\r", ch);
			break;
		case 1:
			value = atoi(argument);
			if( value <= 0 )
			{
				send_to_char("TELESCOPE MINIMUM DISTANCE must be greater than zero.\n\r", ch);
				return false;
			}
			if( value > pObj->value[2] )
			{
				sprintf(buf, "TELESCOPE MINIMUM DISTANCE must be less than or equal to %ld.\n\r", pObj->value[2]);
				send_to_char(buf, ch);
				return false;
			}
			pObj->value[1] = value;
			send_to_char("TELESCOPE MINIMUM DISTANCE SET\n\r", ch);
			break;
		case 2:
			value = atoi(argument);
			if( value <= 0 )
			{
				send_to_char("TELESCOPE MAXIMUM DISTANCE must be greater than zero.\n\r", ch);
				return false;
			}
			if( value < pObj->value[1] )
			{
				sprintf(buf, "TELESCOPE MAXIMUM DISTANCE must be greater than or equal to %ld.\n\r", pObj->value[1]);
				send_to_char(buf, ch);
				return false;
			}
			if( value > 50 )
			{
				if( !has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL )
				{
					send_to_char("An imp sig is required to set the TELESCOPE MAXIMUM DISTANCE greater than 50.\n\r", ch);
					return false;
				}

				if (has_imp_sig(NULL, pObj))
					use_imp_sig(NULL, pObj);
			}
			pObj->value[2] = value;
			send_to_char("TELESCOPE MAXIMUM DISTANCE SET\n\r", ch);
			break;
		case 3:
			value = atoi(argument);
			if( value <= 0 )
			{
				send_to_char("TELESCOPE BONUSVIEW must be greater than zero.\n\r", ch);
				return false;
			}
			if( value > 10 )
			{
				if( !has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL )
				{
					send_to_char("An imp sig is required to set the TELESCOPE BONUSVIEW greater than 50.\n\r", ch);
					return false;
				}

				if (has_imp_sig(NULL, pObj))
					use_imp_sig(NULL, pObj);
			}
			pObj->value[3] = value;
			send_to_char("TELESCOPE BONUSVIEW SET\n\r", ch);
			break;
		case 4:
			if( is_number(argument) )
			{
				value = atoi(argument);
				if( value < 0 || value >= 360 )
				{
					send_to_char("TELESCOPE HEADING must be from 0 to 359.\n\r", ch);
					return false;
				}

				pObj->value[4] = value;
				send_to_char("TELESCOPE HEADING SET\n\r", ch);
			}
			else if( !str_cmp(argument, "none") || !str_cmp(argument, "clear") )
			{
				pObj->value[4] = -1;
				send_to_char("TELESCOPE HEADING CLEARED\n\r", ch);
			}
			break;
		}
		break;
	case ITEM_COMPASS:
		switch (value_num)
		{
		int value;
		long wuid;
		WILDS_DATA *pWilds;

		default:
			do_help(ch, "ITEM_COMPASS");
			return false;
		case 0:
			send_to_char("Accuracy set.\n\r\n\r", ch);
			pObj->value[0] = atoi(argument);
			break;
		case 1:
			wuid = atoi(argument);
			if( wuid > 0 )
			{
				pWilds = get_wilds_from_uid(NULL,wuid);
				if( !pWilds )
				{
					send_to_char("Invalid Wilds.\n\r", ch);
					return false;
				}

				pObj->value[1] = wuid;
				pObj->value[2] = pWilds->map_size_x / 2;
				pObj->value[3] = pWilds->map_size_y / 2;

				send_to_char("WILDS set.\n\r", ch);
			}
			else
			{
				pObj->value[1] = 0;
				pObj->value[2] = -1;
				pObj->value[3] = -1;
				send_to_char("WILDS cleared.\n\r", ch);
			}
			break;
		case 2:
			if( !pObj->value[1] )
			{
				send_to_char("Please set the WILDS({Wv1{x) before assigning coordinates.\n\r", ch);
				return false;
			}

			pWilds = get_wilds_from_uid(NULL,pObj->value[1]);
			if( !pWilds )
			{
				send_to_char("Please set the WILDS({Wv1{x) to a valid wilderness before assigning coordinates.\n\r", ch);
				return false;
			}

			value = atoi(argument);
			if( value < 0 || value >= pWilds->map_size_x )
			{
				sprintf(buf, "X COORDINATE must be from 0 to %d.\n\r", pWilds->map_size_x - 1);
				send_to_char(buf, ch);
				return false;
			}

			pObj->value[2] = value;
			send_to_char("X COORDINATE set.\n\r", ch);
			break;
		case 3:
			if( !pObj->value[1] )
			{
				send_to_char("Please set the WILDS({Wv1{x) before assigning coordinates.\n\r", ch);
				return false;
			}

			pWilds = get_wilds_from_uid(NULL,pObj->value[1]);
			if( !pWilds )
			{
				send_to_char("Please set the WILDS({Wv1{x) to a valid wilderness before assigning coordinates.\n\r", ch);
				return false;
			}

			value = atoi(argument);
			if( value < 0 || value >= pWilds->map_size_y )
			{
				sprintf(buf, "Y COORDINATE must be from 0 to %d.\n\r", pWilds->map_size_y - 1);
				send_to_char(buf, ch);
				return false;
			}

			pObj->value[3] = value;
			send_to_char("Y COORDINATE set.\n\r", ch);
			break;
		}
		break;

	case ITEM_BODY_PART:
		switch(value_num)
		{
		int value;
		default:
			do_help(ch, "ITEM_BODY_PART");
			break;

		case 0:
			if ((value = flag_value(part_flags, argument)) == NO_FLAG)
				return false;
			send_to_char("BODY PARTS TOGGLED.\n\r", ch);
			pObj->value[0] ^= value;
			break;

		case 1:
			send_to_char("RACE SET\n\r", ch);
			pObj->value[1] = race_lookup(argument);
			break;

		}
		break;
	}

	buffer = new_buf();
	print_obj_values(pObj, buffer);
	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);

	return true;
}


OEDIT(oedit_show)
{
    OBJ_INDEX_DATA *pObj;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    AFFECT_DATA *paf;
//    ITERATOR it;
//    PROG_LIST *trigger;
    SPELL_DATA *spell;
    int cnt;

    EDIT_OBJ(ch, pObj);

    buffer = new_buf();

    sprintf(buf, "Name:         {B[{x%s{B]{x\n\rArea:         {B[{x%7ld{B] {x%s\n\r",
	pObj->name,
	!pObj->area ? -1        : pObj->area->anum,
	!pObj->area ? "No Area" : pObj->area->name);
    add_buf(buffer, buf);

    sprintf(buf, "Vnum:         {B[{x%7ld{B]{x\n\rType:         {B[{x%s{B]{x\n\r",
	pObj->vnum,
	flag_string(type_flags, pObj->item_type));
    add_buf(buffer, buf);

    sprintf(buf, "Persist:      {B[%s{B]{x\n\r", (pObj->persist ? "{WON" : "{Doff"));
    add_buf(buffer, buf);


    sprintf(buf, "Level:        {B[{x%7d{B]{x\n\r", pObj->level);
    add_buf(buffer, buf);

    sprintf(buf, "Wear flags:   {B[{x%s{B]{x\n\r",
	flag_string(wear_flags, pObj->wear_flags));
    add_buf(buffer, buf);

    sprintf(buf, "Imp sig:      {B[{x%s{B]{x\n\r",
		    pObj->imp_sig);
    add_buf(buffer, buf);

    sprintf(buf, "Creator sig:  {B[{x%s{B]{x\n\r",
		    pObj->creator_sig);
    add_buf(buffer, buf);

    sprintf(buf, "Script Kwds:  {B[{x%s{B]{x\n\r",
    		pObj->skeywds);
    add_buf(buffer, buf);

    sprintf(buf, "Extra flags:  {B[{x%s{B]{x\n\r",
	bitvector_string(4, pObj->extra[0], extra_flags, pObj->extra[1], extra2_flags, pObj->extra[2], extra3_flags, pObj->extra[3], extra4_flags));
    add_buf(buffer, buf);
/*
    sprintf(buf, "Extra2 flags: {B[{x%s{B]{x\n\r",
		    flag_string(extra[1], pObj->extra[1]));
    add_buf(buffer, buf);

    sprintf(buf, "Extra3 flags: {B[{x%s{B]{x\n\r",
		    flag_string(extra[2], pObj->extra[2]));
    add_buf(buffer, buf);

    sprintf(buf, "Extra4 flags: {B[{x%s{B]{x\n\r",
		    flag_string(extra[3], pObj->extra[3]));
    add_buf(buffer, buf);

    sprintf(buf, "OUpdate:      {B[{x%s{B]{x\n\r",
    	pObj->update == true ? "Yes" : "No");
    add_buf(buffer, buf);
*/
    sprintf(buf, "Timer:        {B[{x%d{B]{x\n\r",
        pObj->timer);
    add_buf(buffer, buf);

    sprintf(buf, "Material:     {B[{x%s{B]{x\n\r",                /* ROM */
	pObj->material);
    add_buf(buffer, buf);

    sprintf(buf, "Condition:    {B[{x%7d{B]{x\n\r",               /* ROM */
	pObj->condition);
    add_buf(buffer, buf);

    sprintf(buf, "Fragility:    {B[{x%7s{B]{x\n\r",               /* ROM */
	fragile_table[pObj->fragility].name);

    add_buf(buffer, buf);

    sprintf(buf, "Allwd Fixed:  {B[{x%7d{B]{x\n\r",               /* ROM */
	pObj->times_allowed_fixed);
    add_buf(buffer, buf);

    sprintf(buf, "Weight:       {B[{x%7d{B]{x\n\r"
		 "Cost:         {B[{x%7ld{B]{x\n\r",
	pObj->weight, pObj->cost);
    add_buf(buffer, buf);

    sprintf(buf, "Points:       {B[{x%7d{B]{x\n\r",
         pObj->points);
    add_buf(buffer, buf);

    if( pObj->lock )
    {
		OBJ_INDEX_DATA *lock_key = (pObj->lock->key_vnum > 0) ? get_obj_index(pObj->lock->key_vnum) : NULL;

	    sprintf(buf,"Lock State:\n\r"
	    			"  Key:         {B[{x%7ld{B]{x %s\n\r"
	    			"  Flags:       {B[{x%s{B]{x\n\r"
	    			"  Pick Chance: {B[{x%d%%{B]{x\n\r",
	    			pObj->lock->key_vnum,
	    			lock_key ? lock_key->short_descr : "none",
	    			flag_string(lock_flags, pObj->lock->flags),
	    			pObj->lock->pick_chance);
	    add_buf(buffer, buf);
	}

    if (pObj->extra_descr)
    {
	EXTRA_DESCR_DATA *ed;

	add_buf(buffer, "Ex desc kwd: ");

	for (ed = pObj->extra_descr; ed; ed = ed->next)
	{
	    add_buf(buffer, "[");
	    sprintf(buf, "%s", ed->keyword);
	    add_buf(buffer, buf);
	    add_buf(buffer, "]");
	}

	add_buf(buffer, "\n\r");
    }

    sprintf(buf, "Short desc:{x   %s\n\rLong desc:{x\n\r     %s\n\r",
	pObj->short_descr, pObj->description);
    add_buf(buffer, buf);

    add_buf(buffer, "Description:{x\n\r");
    sprintf(buf, "%s", pObj->full_description);
    add_buf(buffer, buf);

	sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pObj->comments);
	add_buf(buffer, buf);

    for (cnt = 0, paf = pObj->affected; paf; paf = paf->next)
    {
		if( paf->where == TO_OBJECT )
		{
			if (cnt == 0)
			{
				sprintf(buf, "{Y%-6s %-20s %-10s %-10s{x\n\r", "Number", "Affects", "Modifier", "Random");
				add_buf(buffer, buf);

				sprintf(buf, "{Y%-6s %-20s %-10s %-10s{x\n\r", "------", "-------", "--------", "------");
				add_buf(buffer, buf);
			}

			sprintf(buf, "{B[{W%4d{B] {%c%-20s{x %-20d %d%%\n\r",
				cnt,
				(paf->location >= APPLY_SKILL && paf->location < APPLY_SKILL_MAX)?'Y':'G',
				affect_loc_name(paf->location),
				paf->modifier,
				paf->random);

			add_buf(buffer, buf);
			cnt++;
		}
    }

    for (cnt = 0, paf = pObj->affected; paf; paf = paf->next)
    {
		if( paf->where == TO_IMMUNE || paf->where == TO_RESIST || paf->where == TO_VULN )
		{
			char* irv;

			if(paf->where == TO_IMMUNE)
				irv = "Wimmunity";
			else if(paf->where == TO_VULN)
				irv = "Rvulnerability";
			else
				irv = "Gresistance";

			if (cnt == 0)
			{
				sprintf(buf, "{C%-6s %-15s %-15s %-10s{x\n\r", "Number", "Adds", "Modifier", "Random");
				add_buf(buffer, buf);

				sprintf(buf, "{C%-6s %-15s %-15s %-10s{x\n\r", "------", "-------", "--------", "------");
				add_buf(buffer, buf);
			}

			sprintf(buf, "{B[{W%4d{B] {%-16s{x %-15s %d%%\n\r",
				cnt,
				irv,
				imm_bit_name(paf->bitvector),
				paf->random);

			add_buf(buffer, buf);
			cnt++;
		}
    }


    if (pObj->spells)
    {
	cnt = 0;

	sprintf(buf, "{g%-6s %-20s %-10s %-6s{x\n\r", "Number", "Spell", "Level", "Random");
	add_buf(buffer, buf);

	sprintf(buf, "{g%-6s %-20s %-10s %-6s{x\n\r", "------", "-----", "-----", "------");
	add_buf(buffer, buf);

	for (spell = pObj->spells; spell != NULL; spell = spell->next, cnt++)
	{
	    sprintf(buf, "{B[{W%4d{B]{x %-20s %-10d %d%%\n\r",
	        cnt,
	        skill_table[spell->sn].name, spell->level, spell->repop);
	    buf[0] = UPPER(buf[0]);
	    add_buf(buffer, buf);
	}
    }

    if (pObj->catalyst)
    {
		cnt = 0;
		char line_colour = 'x';

		sprintf(buf, "{m%-6s %-20s %-10s %-6s %-6s %-11s{x\n\r", "Number", "Type", "Strength", "Amount", "Random", "Script Name");
		add_buf(buffer, buf);

		sprintf(buf, "{m%-6s %-20s %-10s %-6s %-6s %-11s{x\n\r", "------", "----", "--------", "------", "------", "-----------");
		add_buf(buffer, buf);

		for (paf = pObj->catalyst; paf; paf = paf->next, cnt++) {
			line_colour = ( paf->where == TO_CATALYST_ACTIVE ) ? 'W' : 'x';

			char *name = (IS_NULLSTR(paf->custom_name)) ? "---" : paf->custom_name;

			if(paf->modifier < 0)
				sprintf(buf, "{M[{W%4d{M]{%c %-20s %-10d {Wsource{%c %d%% %s{x\n\r", cnt, line_colour,
					flag_string(catalyst_types,paf->type),paf->level,line_colour,paf->random, name);
			else
				sprintf(buf, "{M[{W%4d{M]{%c %-20s %-10d %-6d %d%% %s{x\n\r", cnt, line_colour,
					flag_string(catalyst_types,paf->type),paf->level,paf->modifier,paf->random, name);
			buf[0] = UPPER(buf[0]);
			add_buf(buffer, buf);
		}
    }

    if (list_size(pObj->waypoints) > 0)
    {
		int cnt = 0;
		ITERATOR wit;
		WAYPOINT_DATA *wp;
		WILDS_DATA *wilds;

		add_buf(buffer, "{BCartographer Waypoints:{x\n\r\n\r");
		add_buf(buffer, "{B     [     Wilderness     ] [ South ] [  East ] [        Name        ]{x\n\r");
		add_buf(buffer, "{B======================================================================={x\n\r");

		iterator_start(&wit, pObj->waypoints);
		while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
		{
			wilds = get_wilds_from_uid(NULL, wp->w);

			char *wname = wilds ? wilds->name : "{D(null){x";

			int wwidth = get_colour_width(wname) + 20;

			sprintf(buf, "{B%3d{b)  {W%-*.*s    {G%5d     %5d    {Y%s{x\n\r",
				++cnt,
				wwidth, wwidth, wname,
				wp->y, wp->x, wp->name);

			add_buf(buffer, buf);
		}

		iterator_stop(&wit);

		add_buf(buffer, "\n\r");
	}


    if (pObj->progs)
		olc_show_progs(buffer, pObj->progs, PRG_OPROG, "ObjProg Vnum");

	if (pObj->index_vars)
		olc_show_index_vars(buffer, pObj->index_vars);


    print_obj_values(pObj, buffer);

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}


OEDIT(oedit_addaffect)
{
    long value;
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf, *pAf_tmp;
    int pMod;
    bool pAdd = false;
    char loc[MAX_STRING_LENGTH];
    char mod[MAX_STRING_LENGTH];
    char randm[MAX_STRING_LENGTH];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, loc);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, randm);

    if (loc[0] == '\0'
    || mod[0] == '\0'
    || randm[0] == '\0'
    || !is_number(randm)
    || !is_number(mod))
    {
	send_to_char("Syntax:  addaffect [location] [#xmod] [#rand]\n\r", ch);
	return false;
    }

    if ((value = flag_value(apply_flags, loc)) == NO_FLAG) /* Hugin */
    {
        send_to_char("Valid affects are:\n\r", ch);
	show_help(ch, "apply");
	return false;
    }

    for (pAf = pObj->affected; pAf != NULL; pAf = pAf->next)
    {
	if (pAf->where == TO_OBJECT && pAf->location == value)
	{
	    sprintf(buf, "There's already a %s modifier on that item.\n\r",
	        flag_string(apply_flags, value));
	    send_to_char(buf, ch);
	    return false;
	}
    }

    switch(value)
    {
        case APPLY_HIT:
	case APPLY_MANA:
		pMod = (int) atoi(mod)/10;
		if (pMod == 0) pMod = 1;
		if (atoi(mod) < 0) pAdd = true;
		break;
	case APPLY_MOVE:
		pMod = (int) atoi(mod)/20;
		if (pMod == 0) pMod = 1;
		if (atoi(mod) < 0) pAdd = true;
		break;
	case APPLY_DEX:
	case APPLY_WIS:
	case APPLY_INT:
	case APPLY_STR:
	case APPLY_CON:
		pMod = atoi(mod);
		if (atoi(mod) < 0) pAdd = true;
		break;
	case APPLY_AC:
		pMod = (int) atoi(mod)/10;
		if (pMod == 0) pMod = 1;
		if (atoi(mod) > 0) pAdd = true;
		break;
	case APPLY_HITROLL:
	case APPLY_DAMROLL:
		pMod = (int) atoi(mod)/2;
		if (pMod == 0) pMod = 1;
		if (atoi(mod) < 0) pAdd = true;
		break;
	default:
		pMod = 1;
		pAdd = false;
		break;
    }

    /*
     * Modify based on random. This prevents people adding 123123
     * negative affects which dont ever actually repop on the item.
     */
    if (pAdd)
    {
        if (atoi(randm) < 10) pMod = 0;
        else if (atoi(randm) < 25) pMod = (int) pMod/4;
        else if (atoi(randm) < 50) pMod = (int) pMod/2;
        else if (atoi(randm) < 80) pMod = (int) pMod*4/5;
    }

    pMod = abs(pMod);

    if (!pAdd && (pObj->points - pMod) < 0)
    {
        send_to_char("You've already added enough positive affects.\n\r", ch);
        return false;
    }

    if (!pAdd)
        pObj->points -= pMod;
    else
        pObj->points += pMod;

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   value;
    pAf->modifier   =   atoi(mod);
    pAf->where	    =   TO_OBJECT;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   0;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(randm);

    if (!pObj->affected)
	pObj->affected = pAf;
    else
    {
	for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
	    ;

        pAf_tmp->next = pAf;
    }

    send_to_char("Affect added.\n\r", ch);
    return true;
}

OEDIT(oedit_addimmune)
{
    long value;
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf, *pAf_tmp;
    int pMod;
    int where;
    bool pAdd = false;
    char loc[MAX_STRING_LENGTH];
    char mod[MAX_STRING_LENGTH];
    char randm[MAX_STRING_LENGTH];
    char buf[MSL];

    EDIT_OBJ(ch, pObj);

    argument = one_argument(argument, loc);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, randm);

    if (loc[0] == '\0'
    || mod[0] == '\0'
    || randm[0] == '\0'
    || !is_number(randm))
    {
		send_to_char("Syntax:  addimmune [immune|resist|vuln] [bit] [#rand]\n\r", ch);
		return false;
    }

    where = flag_value(apply_types, loc);

    if( where != TO_IMMUNE && where != TO_RESIST && where != TO_VULN )
    {
		send_to_char("Syntax:  addimmune [immune|resist|vuln] [bit] [#rand]\n\r", ch);
		return false;
	}

	if( where == TO_IMMUNE )
	{
	    if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL)
	    {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }
	}

	value = flag_value(imm_flags, mod);
	if( value == NO_FLAG || value == 0 )
	{
	    send_to_char("Invalid bit flag\n\r"
			  "Type '? imm' for a list of flags.\n\r", ch);
		return false;
	}

	if ( (value & (~value + 1)) != value )
	{
		send_to_char("You can only put one flag per immunity modifier.\n\r", ch);
		return false;
	}


    for (pAf = pObj->affected; pAf != NULL; pAf = pAf->next)
    {
		if ((pAf->where == TO_IMMUNE || pAf->where == TO_RESIST || pAf->where == TO_VULN) && ((pAf->bitvector & value) != 0))
		{
			sprintf(buf, "There's already an immunity modifier for %s on that item.\n\r",
				flag_string(imm_flags, value));
			send_to_char(buf, ch);
			return false;
		}
    }

    pMod = atoi(randm);

	switch(where)
	{
		case TO_IMMUNE:
			pMod = 5 * pMod / 2;
			break;
		case TO_RESIST:
			break;
		case TO_VULN:
			pAdd = true;
			break;
	}

	pMod = (pMod + 9) / 10;

    if (!pAdd && (pObj->points - pMod) < 0)
    {
        send_to_char("You've already added enough positive affects.\n\r", ch);
        return false;
    }

    if (!pAdd)
        pObj->points -= pMod;
    else
        pObj->points += pMod;

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   APPLY_NONE;
    pAf->modifier   =   0;
    pAf->where	    =   where;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   value;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(randm);

    if (!pObj->affected)
	pObj->affected = pAf;
    else
    {
	for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
	    ;

        pAf_tmp->next = pAf;
    }

    send_to_char("Immunity modifier added.\n\r", ch);
    return true;
}


OEDIT(oedit_addspell)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char name[MSL];
    char level[MSL];
    char rand[MSL];
    SPELL_DATA *spell, *spell_tmp;
    int sn, i;
    bool restricted = true;
    bool spell_restricted = true;

    EDIT_OBJ(ch, pObj);

    if( ch->tot_level == MAX_LEVEL || has_imp_sig(NULL, pObj) )
    	restricted = false;

    if( ch->tot_level == MAX_LEVEL )
    	spell_restricted = false;

    if (restricted &&
    	!(pObj->item_type == ITEM_SCROLL ||
    	pObj->item_type == ITEM_WAND ||
    	pObj->item_type == ITEM_STAFF ||
    	pObj->item_type == ITEM_POTION ||
    	pObj->item_type == ITEM_PILL ||
    	pObj->item_type == ITEM_TATTOO ||
    	pObj->item_type == ITEM_PORTAL))
    {
		send_to_char("You can't do this without an IMP's permission.\n\r", ch);
		return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, level);
    argument = one_argument(argument, rand);

    if (name[0] == '\0' || level[0] == '\0' || rand[0] == '\0'
    ||  !is_number(level) || !is_number(rand))
    {
	send_to_char("Syntax: addspell [spell name] [spell level] [random]\n\r", ch);
	return false;
    }

    if ((sn = skill_lookup(name)) == -1 || (spell_restricted && (skill_table[sn].spell_fun == spell_null)))
    {
		send_to_char("That's not a spell.\n\r", ch);
		return false;
    }

    if (pObj->item_type != ITEM_SCROLL
    &&  pObj->item_type != ITEM_PILL
    &&  pObj->item_type != ITEM_POTION
    &&  pObj->item_type != ITEM_TATTOO
    &&  pObj->item_type != ITEM_STAFF
    &&  pObj->item_type != ITEM_WAND)
    {
	for (spell_tmp = pObj->spells; spell_tmp != NULL; spell_tmp = spell_tmp->next)
	{
	    if (spell_tmp->sn == sn)
	    {
		send_to_char("That spell is already on the object.\n\r", ch);
		return false;
	    }
	}
    }

    if ((i = atoi(level)) < 1 || i > get_trust(ch))
    {
	sprintf(buf, "Level range is 1-%d.\n\r", get_trust(ch));
	send_to_char(buf, ch);
	return false;
    }

    if ((i = atoi(rand)) < 1 || i > 100)
    {
	send_to_char("Random repop must be a percentage 1-100.\n\r", ch);
	return false;
    }

    spell 		= new_spell();
    spell->sn		= sn;
    spell->level	= atoi(level);
    spell->repop	= atoi(rand);
    spell->next = NULL;

    // Add to end of list
    if (pObj->spells == NULL)
	pObj->spells = spell;
    else
    {
	for (spell_tmp = pObj->spells; spell_tmp->next != NULL; spell_tmp = spell_tmp->next)
	    ;

        spell_tmp->next = spell;
    }

    sprintf(buf, "Added spell %s, level %d, random %d.\n\r",
        skill_table[sn].name, spell->level, spell->repop);
    send_to_char(buf, ch);
    return true;
}

OEDIT(oedit_addskill)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char name[MSL];
    char mod[MSL];
    char random[MSL];
    AFFECT_DATA *pAf, *pAf_tmp;
    int sn, i;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
    {
	send_to_char("You can't do this without an IMP's permission.\n\r", ch);
	return false;
    }

    argument = one_argument(argument, name);
    argument = one_argument(argument, mod);
    argument = one_argument(argument, random);

    if (name[0] == '\0' || mod[0] == '\0' || random[0] == '\0' ||  !is_number(mod) || !is_number(random))
    {
	send_to_char("Syntax: addskill [skill name] [#modifier] [random]\n\r", ch);
	return false;
    }

    if ((sn = skill_lookup(name)) == -1)
    {
	send_to_char("That's not a skill.\n\r", ch);
	return false;
    }

    if ((i = atoi(mod)) < -100 || i > 100 || !i)
    {
	send_to_char("Skill modifier must be a positive (1 to 100) or negative (-1 to -100) percentage.\n\r", ch);
	return false;
    }

    if ((i = atoi(random)) < 1 || i > 100)
    {
	send_to_char("Random repop must be a percentage 1-100.\n\r", ch);
	return false;
    }

    pAf             =   new_affect();
    pAf->next	    =   NULL;
    pAf->location   =   APPLY_SKILL+sn;
    pAf->modifier   =   atoi(mod);
    pAf->where	    =   TO_OBJECT;
    pAf->type       =   -1;
    pAf->duration   =   -1;
    pAf->bitvector  =   0;
    pAf->level      =	pObj->level;
    pAf->random	    =   atoi(random);

    if (!pObj->affected)
	pObj->affected = pAf;
    else
    {
	for (pAf_tmp = pObj->affected; pAf_tmp->next != NULL; pAf_tmp = pAf_tmp->next)
	    ;

        pAf_tmp->next = pAf;
    }

    sprintf(buf, "Added skill %s, percent mod %d%%, random %d.\n\r",
        skill_table[sn].name, pAf->modifier, pAf->random);
    send_to_char(buf, ch);
    return true;
}


OEDIT(oedit_addcatalyst)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MSL];
    char type[MSL];
    char strength[MSL];
    char charges[MSL];
    char chance[MIL];
    char where[MIL];
    AFFECT_DATA *cat, *pCat;
    int t, s, c, n, w;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL && !has_imp_sig(NULL, pObj))
    {
	send_to_char("You can't do this without an IMP's permission.\n\r", ch);
	return false;
    }

    argument = one_argument(argument, type);
    argument = one_argument(argument, strength);
    argument = one_argument(argument, charges);
    argument = one_argument(argument, chance);
    argument = one_argument(argument, where);

    if (!type[0] || !strength[0] || !charges[0] || !chance[0]
    ||  !is_number(strength) || (!is_number(charges) && str_prefix(charges,"source")) || !is_number(chance))
    {
	send_to_char("Syntax: addcatalyst [type] [strength] [charges] [chance] [active] [name]\n\r", ch);
	return false;
    }

    if ((t = flag_value(catalyst_types,type)) == NO_FLAG)
    {
	send_to_char("That's not a catalyst type.\n\r", ch);
	return false;
    }

    s = atoi(strength);
    c = atoi(chance);
    w = (where[0] && !str_cmp(where, "active")) ? TO_CATALYST_ACTIVE : TO_CATALYST_DORMANT;

    if (s < 1 || s > CATALYST_MAXSTRENGTH)
    {
		sprintf(buf, "Valid strengths are from 1 to %d.\n\r", CATALYST_MAXSTRENGTH);
		send_to_char(buf, ch);
		return false;
    }

	if(!str_prefix(charges,"source"))
		n = -1;
	else if ((n = atoi(charges)) < 1) {
		send_to_char("Invalid charges.\n\r", ch);
		return false;
	}

	c = URANGE(1,c,100);

    for(cat = pObj->catalyst; cat; cat = cat->next) {
	    if(cat->where == w && cat->type == t && cat->level == s && cat->random == c) {
		    if(cat->modifier < 0 || n < 0)
			    cat->modifier = -1;
		    else
			    cat->modifier += n;
		    break;
	    }
    }

	if(!cat) {
		pCat = new_affect();
		pCat->next = NULL;
		pCat->where = w;
		pCat->modifier = n;
		pCat->type = t;
		pCat->level = s;
		pCat->random = c;

		if( !IS_NULLSTR(argument) )
			pCat->custom_name = str_dup(argument);

		// Add to end of list
		if (!pObj->catalyst)
			pObj->catalyst = pCat;
		else {
			for (cat = pObj->catalyst; cat->next != NULL; cat = cat->next);
			cat->next = pCat;
		}
	}

    send_to_char("Added catalyst.\n\r", ch);
    return true;
}


OEDIT(oedit_delspell)
{
    OBJ_INDEX_DATA *pObj;
    SPELL_DATA *spell, *spell_prev;
    int i, n;

    EDIT_OBJ(ch, pObj);

    if (!is_number(argument))
    {
	send_to_char("Syntax: delspell [#]\n\r", ch);
	return false;
    }

    n = atoi(argument);
    i = 0;
    spell_prev = NULL;
    for (spell = pObj->spells; spell != NULL; spell = spell->next)
    {
	if (i == n)
	    break;

	i++;
	spell_prev = spell;
    }

    if (spell == NULL)
    {
	send_to_char("That spell isn't on the object.\n\r", ch);
	return false;
    }

    // First one on the list
    if (!spell_prev)
    {
	pObj->spells = spell->next;
	free_spell(spell);
    }
    else
    {
	spell_prev->next = spell->next;
	free_spell(spell);
    }

    send_to_char("Spell removed.\n\r", ch);
    return true;
}


OEDIT(oedit_delcatalyst)
{
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *catalyst, *catalyst_prev;
    int i, n;

    EDIT_OBJ(ch, pObj);

    if (!is_number(argument))
    {
	send_to_char("Syntax: delcatalyst [#]\n\r", ch);
	return false;
    }

    n = atoi(argument);
    i = 0;
    catalyst_prev = NULL;
    for (catalyst = pObj->catalyst; catalyst != NULL; catalyst = catalyst->next)
    {
	if (i == n)
	    break;

	i++;
	catalyst_prev = catalyst;
    }

    if (catalyst == NULL)
    {
	send_to_char("That catalyst isn't on the object.\n\r", ch);
	return false;
    }

    // First one on the list
    if (!catalyst_prev)
    {
	pObj->catalyst = catalyst->next;
	free_affect(catalyst);
    }
    else
    {
	catalyst_prev->next = catalyst->next;
	free_affect(catalyst);
    }

    send_to_char("Catalyst removed.\n\r", ch);
    return true;
}


OEDIT(oedit_next)
{
    OBJ_INDEX_DATA *pObj;
    OBJ_INDEX_DATA *nextObj = NULL;
    long next_vnum;

    EDIT_OBJ(ch, pObj);

    next_vnum = pObj->vnum;

    next_vnum++;
    while (nextObj == NULL
    && next_vnum <= pObj->area->max_vnum)
    {
	nextObj = get_obj_index(next_vnum);
	next_vnum++;
    }

    if (nextObj == NULL)
    {
	send_to_char("No next object in area.\n\r", ch);
    }
    else
    {
	edit_done(ch);
	ch->desc->pEdit = (void *)nextObj;
	ch->desc->editor = ED_OBJECT;
    }
    return false;
}

OEDIT(oedit_waypoints)
{
	char buf[MSL];
	char arg[MIL];
	OBJ_INDEX_DATA *pObj;

	EDIT_OBJ(ch, pObj);

	if( pObj->item_type != ITEM_MAP )
	{
		send_to_char("Only MAP objects can have waypoints.\n\r", ch);
		return false;
	}

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  waypoints list\n\r", ch);
		send_to_char("         waypoints add <wilds> <south> <east>[ <name>]\n\r", ch);
		send_to_char("         waypoints delete <#>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "list") )
	{
		if (list_size(pObj->waypoints) > 0)
		{
			int cnt = 0;
			ITERATOR wit;
			WAYPOINT_DATA *wp;
			WILDS_DATA *wilds;

			BUFFER *buffer = new_buf();

			add_buf(buffer, "{BCartographer Waypoints:{x\n\r\n\r");
			add_buf(buffer, "{B     [     Wilderness     ] [ South ] [  East ] [        Name        ]{x\n\r");
			add_buf(buffer, "{B======================================================================={x\n\r");

			iterator_start(&wit, pObj->waypoints);
			while( (wp = (WAYPOINT_DATA *)iterator_nextdata(&wit)) )
			{
				wilds = get_wilds_from_uid(NULL, wp->w);

				char *wname = wilds ? wilds->name : "{D(null){x";

				int wwidth = get_colour_width(wname) + 20;

				sprintf(buf, "{B%3d{b)  {W%-*.*s    {G%5d     %5d    {Y%s{x\n\r",
					++cnt,
					wwidth, wwidth, wname,
					wp->y, wp->x, wp->name);

				add_buf(buffer, buf);
			}

			iterator_stop(&wit);

			add_buf(buffer, "\n\r");

			page_to_char(buffer->string, ch);

			free_buf(buffer);
		}
		else
			send_to_char("No waypoints to display.\n\r", ch);

		return false;
	}

	if( !str_prefix(arg, "add") )
	{
		char arg2[MIL];
		char arg3[MIL];
		char arg4[MIL];

		long uid;
		WILDS_DATA *wilds;
		int x, y;

		argument = one_argument(argument, arg2);
		argument = one_argument(argument, arg3);
		argument = one_argument(argument, arg4);

		if( !is_number(arg2) || !is_number(arg3) || !is_number(arg4) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		uid = atol(arg2);
		wilds = get_wilds_from_uid(NULL, uid);
		if( !wilds )
		{
			send_to_char("No such wilderness.\n\r", ch);
			return false;
		}

		y = atoi(arg3);
		x = atoi(arg4);

		if( y < 0 || y >= wilds->map_size_y )
		{
			sprintf(buf, "South coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_y - 1);
			send_to_char(buf, ch);
			return false;
		}

		if( x < 0 || x >= wilds->map_size_x )
		{
			sprintf(buf, "East coordinate is out of bounds.  Please limit from 0 to %d.\n\r", wilds->map_size_x - 1);
			send_to_char(buf, ch);
			return false;
		}

		WAYPOINT_DATA *wp = new_waypoint();

		free_string(wp->name);
		wp->name = nocolour(argument);
		wp->w = uid;
		wp->x = x;
		wp->y = y;

		if( !pObj->waypoints )
		{
			pObj->waypoints = new_waypoints_list();
		}

		list_appendlink(pObj->waypoints, wp);
		send_to_char("Waypoint added.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "delete") )
	{
		int value;

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		if( !IS_VALID(pObj->waypoints) )
		{
			send_to_char("There are no waypoints to delete.\n\r", ch);
			return false;
		}

		value = atoi(argument);
		if( value < 1 || value > list_size(pObj->waypoints) )
		{
			send_to_char("No such waypoint.\n\r", ch);
			return false;
		}

		list_remnthlink(pObj->waypoints, value);
		send_to_char("Waypoint deleted.\n\r", ch);
		return true;
	}

	oedit_waypoints(ch, "");
	return false;
}

OEDIT(oedit_lock)
{
	char arg[MIL];
	OBJ_INDEX_DATA *pObj;

	EDIT_OBJ(ch, pObj);

	if( argument[0] == '\0' )
	{
		send_to_char("Syntax:  lock add\n\r", ch);
		send_to_char("         lock remove\n\r", ch);
		send_to_char("         lock key [vnum]\n\r", ch);
		send_to_char("         lock key clear\n\r", ch);
		send_to_char("         lock flags [flags]\n\r", ch);
		send_to_char("         lock pick [0-100]\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "add") )
	{
		if( pObj->lock )
		{
			send_to_char("Object already has a lock state.\n\r", ch);
			return false;
		}

		// TODO: Add closeability to weapon_containers and drinkcontainers
		if( pObj->item_type != ITEM_CONTAINER &&
			pObj->item_type != ITEM_PORTAL &&
//			pObj->item_type != ITEM_WEAPON_CONTAINER &&
//			pObj->item_type != ITEM_DRINKCONTAINER &&
			pObj->item_type != ITEM_BOOK )
		{
			send_to_char("Invalid object type.\n\r", ch);
			send_to_char("Only the following types may have lock state added:\n\r", ch);
			send_to_char("{Y*{x CONTAINER\n\r", ch);
			send_to_char("{Y*{x PORTAL\n\r", ch);
//			send_to_char("{Y*{x WEAPON_CONTAINER\n\r", ch);
//			send_to_char("{Y*{x DRINKCONTAINER\n\r", ch);
			send_to_char("{Y*{x BOOK\n\r", ch);
			return false;
		}

		pObj->lock = new_lock_state();
		send_to_char("Lock State added.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "remove") )
	{
		if( !pObj->lock )
		{
			send_to_char("Object does not have a lock state.\n\r", ch);
			return false;
		}


		free_lock_state(pObj->lock);
		pObj->lock = NULL;

		send_to_char("Lock State removed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "key") )
	{
		if( !pObj->lock )
		{
			send_to_char("Object does not have a lock state.\n\r", ch);
			return false;
		}

		if( argument[0] == '\0' )
		{
			send_to_char("Syntax:  lock key [vnum]\n\r", ch);
			send_to_char("         lock key clear\n\r", ch);
			return false;
		}

		if( is_number(argument) )
		{
			long vnum = atol(argument);
			OBJ_INDEX_DATA *key = get_obj_index(vnum);

			if( !key )
			{
				send_to_char("That object does not exist.\n\r", ch);
				return false;
			}

			if( key->item_type != ITEM_KEY )
			{
				send_to_char("That object is not a key.\n\r", ch);
				return false;
			}

			pObj->lock->key_vnum = vnum;
			send_to_char("Lock State key set.\n\r", ch);
			return true;
		}
		else if( !str_prefix(argument, "clear") )
		{
			pObj->lock->key_vnum = 0;
			send_to_char("Lock State key removed.\n\r", ch);
			return true;
		}

		oedit_lock(ch, "lock key");
		return false;
	}

	if( !str_prefix(arg, "flags") )
	{
		if( !pObj->lock )
		{
			send_to_char("Object does not have a lock state.\n\r", ch);
			return false;
		}

		int value = flag_value(lock_flags, argument);

		if( value == NO_FLAG )
		{
			send_to_char("Syntax:  lock flags [flags]\n\r", ch);
			send_to_char("See \"? lock\" for list of flags\n\r\n\r", ch);
			show_help(ch, "lock");
			return false;
		}

		pObj->lock->flags ^= value;
		send_to_char("Lock State flags changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "pick") )
	{
		if( !pObj->lock )
		{
			send_to_char("Object does not have a lock state.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Pick chance must be from 0 to 100.\n\r", ch);
			return false;
		}

		pObj->lock->pick_chance = value;
		send_to_char("Lock State pick chance set.\n\r", ch);
		return true;
	}

	oedit_lock(ch, "");
	return false;
}

OEDIT(oedit_persist)
{
	OBJ_INDEX_DATA *pObj;

	EDIT_OBJ(ch, pObj);


	if (!str_cmp(argument,"on")) {
	    if (!str_cmp(pObj->imp_sig, "none") && ch->tot_level < MAX_LEVEL) {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }

		pObj->persist = true;
	    use_imp_sig(NULL, pObj);
		send_to_char("Persistance enabled.\n\r", ch);
	} else if (!str_cmp(argument,"off")) {
		pObj->persist = false;
		send_to_char("Persistance disabled.\n\r", ch);
	} else {
		send_to_char("Usage: persist on/off\n\r", ch);
		return false;
	}

	return true;
}

OEDIT(oedit_prev)
{
    OBJ_INDEX_DATA *pObj;
    OBJ_INDEX_DATA *prevObj = NULL;
    long prev_vnum;

    EDIT_OBJ(ch, pObj);

    prev_vnum = pObj->vnum;

    prev_vnum--;
    while (prevObj == NULL
    && prev_vnum >= pObj->area->min_vnum)
    {
	prevObj = get_obj_index(prev_vnum);
	prev_vnum--;
    }

    if (prevObj == NULL)
    {
	send_to_char("No previous object in area.\n\r", ch);
    }
    else
    {
	edit_done(ch);
	ch->desc->pEdit = (void *)prevObj;
	ch->desc->editor = ED_OBJECT;
    }
    return false;
}


OEDIT(oedit_delaffect)
{
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf;
    AFFECT_DATA *pAf_prev;
    AFFECT_DATA *pAf_next;
    char affect[MAX_STRING_LENGTH];
    int  value;
    //int  cnt = 0;

    EDIT_OBJ(ch, pObj);

    one_argument(argument, affect);

    if (!is_number(affect) || affect[0] == '\0')
    {
	send_to_char("Syntax:  delaffect [#xaffect]\n\r", ch);
	return false;
    }

    value = atoi(affect);

    if (value < 0)
    {
	send_to_char("Only non-negative affect-numbers allowed.\n\r", ch);
	return false;
    }

    if (!(pAf = pObj->affected))
    {
	send_to_char("OEdit:  Non-existant affect.\n\r", ch);
	return false;
    }

	pAf_prev = NULL;
    for(;pAf;pAf_prev = pAf, pAf = pAf_next)
    {
		pAf_next = pAf->next;

		if( pAf->where == TO_OBJECT )
		{
			if( --value < 0 )
			{
				if( pAf_prev == NULL )
					pObj->affected = pAf_next;
				else
					pAf_prev->next = pAf_next;

				free_affect(pAf);
				send_to_char("Affect removed.\n\r", ch);
				return true;
			}
		}

	}

	send_to_char("No such affect.\n\r", ch);
	return false;
}

OEDIT(oedit_delimmune)
{
    OBJ_INDEX_DATA *pObj;
    AFFECT_DATA *pAf;
    AFFECT_DATA *pAf_prev;
    AFFECT_DATA *pAf_next;
    char affect[MAX_STRING_LENGTH];
    int  value;
    //int  cnt = 0;

    EDIT_OBJ(ch, pObj);

    one_argument(argument, affect);

    if (!is_number(affect) || affect[0] == '\0')
    {
	send_to_char("Syntax:  delimmune [#xaffect]\n\r", ch);
	return false;
    }

    value = atoi(affect);

    if (value < 0)
    {
	send_to_char("Only non-negative affect-numbers allowed.\n\r", ch);
	return false;
    }

    if (!(pAf = pObj->affected))
    {
	send_to_char("OEdit:  Non-existant affect.\n\r", ch);
	return false;
    }

	pAf_prev = NULL;
    for(;pAf;pAf_prev = pAf, pAf = pAf_next)
    {
		pAf_next = pAf->next;

		if( pAf->where == TO_IMMUNE || pAf->where == TO_RESIST || pAf->where == TO_VULN )
		{
			if( --value < 0 )
			{
				if( pAf_prev == NULL )
					pObj->affected = pAf_next;
				else
					pAf_prev->next = pAf_next;

				free_affect(pAf);
				send_to_char("Immunity modifier removed.\n\r", ch);
				return true;
			}
		}

	}

	send_to_char("No such immunity modifier.\n\r", ch);
	return false;
}

OEDIT(oedit_name)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  name [string]\n\r", ch);
	return false;
    }

    free_string(pObj->name);
    pObj->name = str_dup(argument);

    send_to_char("Name set.\n\r", ch);
    return true;
}


OEDIT(oedit_sign)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (ch->tot_level < MAX_LEVEL)
    {
	send_to_char("This is not for you to do.\n\r" , ch);
	return false;
    }

    free_string(pObj->imp_sig);
    pObj->imp_sig = str_dup(ch->name);

    send_to_char("Object signed.\n\r", ch);
    return true;
}

OEDIT(oedit_skeywds)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  skwds [string]\n\r", ch);
	return false;
    }

    free_string(pObj->skeywds);
    pObj->skeywds = str_dup(argument);

    send_to_char("Script keywords set.\n\r", ch);
    return true;
}

OEDIT(oedit_varset)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

	return olc_varset(&pObj->index_vars, ch, argument, false);
}

OEDIT(oedit_varclear)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

	return olc_varclear(&pObj->index_vars, ch, argument, false);
}


OEDIT(oedit_short)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  short [string]\n\r", ch);
	return false;
    }

    free_string(pObj->short_descr);
    pObj->short_descr = str_dup(argument);

    send_to_char("Short description set.\n\r", ch);

    if (IS_SET(ch->act[0], PLR_AUTOSETNAME))
    {
	free_string(pObj->name);
	pObj->name = short_to_name(pObj->short_descr);
	send_to_char("Name keywords set.\n\r", ch);
    }
    return true;
}


OEDIT(oedit_long)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  long [string]\n\r", ch);
	return false;
    }

    strcat(argument, "{x");

    free_string(pObj->description);
    pObj->description = str_dup(argument);
    pObj->description[0] = UPPER(pObj->description[0]);

    send_to_char("Long description set.\n\r", ch);
    return true;
}


bool set_value(CHAR_DATA *ch, OBJ_INDEX_DATA *pObj, char *argument, int value)
{
    if (argument[0] == '\0')
    {
	set_obj_values(ch, pObj, -1, "");
	return false;
    }

    if (set_obj_values(ch, pObj, value, argument))
	return true;

    return false;
}


/* Finds the object and sets its value. */
bool oedit_values(CHAR_DATA *ch, char *argument, int value)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (set_value(ch, pObj, argument, value))
    {
	if (pObj->item_type == ITEM_WEAPON)
	    set_weapon_dice(pObj);

        return true;
    }

    return false;
}


OEDIT(oedit_value0)
{
    if (oedit_values(ch, argument, 0))
        return true;

    return false;
}


OEDIT(oedit_value1)
{
    if (oedit_values(ch, argument, 1))
        return true;

    return false;
}


OEDIT(oedit_value2)
{
    if (oedit_values(ch, argument, 2))
        return true;

    return false;
}


OEDIT(oedit_value3)
{
    if (oedit_values(ch, argument, 3))
        return true;

    return false;
}


OEDIT(oedit_value4)
{
    if (oedit_values(ch, argument, 4))
        return true;

    return false;
}


OEDIT(oedit_value5)
{
    if (oedit_values(ch, argument, 5))
        return true;

    return false;
}


OEDIT(oedit_value6)
{
    if (oedit_values(ch, argument, 6))
        return true;

    return false;
}


OEDIT(oedit_value7)
{
    if (oedit_values(ch, argument, 7))
        return true;

    return false;
}


OEDIT(oedit_weight)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  weight [number]\n\r", ch);
	return false;
    }

    pObj->weight = atoi(argument);

    send_to_char("Weight set.\n\r", ch);
    return true;
}


OEDIT(oedit_cost)
{
    OBJ_INDEX_DATA *pObj;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  cost [number]\n\r", ch);
	return false;
    }

    pObj->cost = atoi(argument);

    send_to_char("Cost set.\n\r", ch);
    return true;
}


OEDIT(oedit_create)
{
    OBJ_INDEX_DATA *pObj;
    AREA_DATA *pArea;
    int  value;
    long auto_vnum = 0;
    int  iHash;

    value = atoi(argument);
    if (argument[0] == '\0' || value == 0)
    {
	//send_to_char("Syntax:  oedit create [vnum]\n\r", ch);
	OBJ_INDEX_DATA *temp_obj;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_obj = get_obj_index(auto_vnum);
	if (temp_obj != NULL)
	{
	    while (temp_obj != NULL)
	    {
		temp_obj = get_obj_index(auto_vnum);
		if (temp_obj == NULL) break;
		auto_vnum++;
	    }
	}

	if (auto_vnum > ch->in_room->area->max_vnum)
	{
	    send_to_char("Sorry, this area has no more space left.\n\r",
		    ch);
	    return false;
	}
    }

    if (auto_vnum != 0)
	value = auto_vnum;

    pArea = get_vnum_area(value);
    if (!pArea)
    {
	send_to_char("OEdit:  That vnum is not assigned an area.\n\r", ch);
	return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("OEdit:  Vnum in an area you cannot build in.\n\r", ch);
	return false;
    }

    if (get_obj_index(value))
    {
	send_to_char("OEdit:  Object vnum already exists.\n\r", ch);
	return false;
    }

    pObj              = new_obj_index();
    pObj->vnum	      = value;
    pObj->area	      = pArea;
    pObj->creator_sig = str_dup(ch->name);

    if (value > top_vnum_obj)
	top_vnum_obj = value;

    iHash                 = value % MAX_KEY_HASH;
    pObj->next		  = obj_index_hash[iHash];
    obj_index_hash[iHash] = pObj;
    ch->desc->pEdit	  = (void *)pObj;

    SET_BIT(pObj->area->area_flags, AREA_CHANGED);
    send_to_char("Object Created.\n\r", ch);
    return true;
}


OEDIT(oedit_ed)
{
	OBJ_INDEX_DATA *pObj;
	EXTRA_DESCR_DATA *ed;
	char command[MAX_INPUT_LENGTH];
	char keyword[MAX_INPUT_LENGTH];
	char copy_item[MAX_INPUT_LENGTH];
	EDIT_OBJ(ch, pObj);

	argument = one_argument(argument, command);
	argument = one_argument(argument, keyword);
	argument = one_argument(argument, copy_item);

	if (command[0] == '\0')
	{
		send_to_char("Syntax:  ed add [keyword]\n\r", ch);
		send_to_char("         ed delete [keyword]\n\r", ch);
		send_to_char("         ed edit [keyword]\n\r", ch);
		send_to_char("         ed format [keyword]\n\r", ch);
		send_to_char("         ed copy old_keyword new_keyword\n\r", ch);
		send_to_char("         ed environment [keyword]\n\r", ch);

		return false;
	}

    if (!str_cmp(command, "environment"))
    {
	if (keyword[0] == '\0')
	{
	    send_to_char("Syntax:  ed environment [keyword]\n\r", ch);
	    return false;
	}

	ed			=   new_extra_descr();
	ed->keyword		=   str_dup(keyword);
	ed->description		= NULL;
	ed->next		=   pObj->extra_descr;
	pObj->extra_descr	=   ed;

	send_to_char("Enviromental extra description added.\n\r", ch);

	return true;
    }

	if (!str_cmp(command, "copy"))
	{
		EXTRA_DESCR_DATA *ed2;

		if (keyword[0] == '\0' || copy_item[0] == '\0')
		{
			send_to_char("Syntax:  ed copy existing_keyword new_keyword\n\r", ch);
			return false;
		}

		for (ed = pObj->extra_descr; ed; ed = ed->next)
		{
			if (is_name(keyword, ed->keyword))
			break;
		}

		if (!ed)
		{
			send_to_char("REdit:  Extra description keyword not found.\n\r", ch);
			return false;
		}

		ed2					= new_extra_descr();
		ed2->keyword		= str_dup(copy_item);
		ed2->next			= pObj->extra_descr;
		pObj->extra_descr	= ed2;
		ed2->description	= str_dup(ed->description);

		send_to_char("Done.\n\r", ch);

		return true;
	}

	if (!str_cmp(command, "add"))
	{
		if (keyword[0] == '\0')
		{
			send_to_char("Syntax:  ed add [keyword]\n\r", ch);
			return false;
		}

		ed					= new_extra_descr();
		ed->keyword			= str_dup(keyword);
		ed->next			= pObj->extra_descr;
		pObj->extra_descr	= ed;

		string_append(ch, &ed->description);

		return true;
	}

	if (!str_cmp(command, "edit"))
	{
		if (keyword[0] == '\0')
		{
			send_to_char("Syntax:  ed edit [keyword]\n\r", ch);
			return false;
		}

		for (ed = pObj->extra_descr; ed; ed = ed->next)
		{
			if (is_name(keyword, ed->keyword))
			break;
		}

		if (!ed)
		{
			send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
			return false;
		}

		if( !ed->description )
			ed->description = str_dup("");

		string_append(ch, &ed->description);

		return true;
	}

	if (!str_cmp(command, "delete"))
	{
		EXTRA_DESCR_DATA *ped = NULL;

		if (keyword[0] == '\0')
		{
			send_to_char("Syntax:  ed delete [keyword]\n\r", ch);
			return false;
		}

		for (ed = pObj->extra_descr; ed; ed = ed->next)
		{
			if (is_name(keyword, ed->keyword))
				break;
			ped = ed;
		}

		if (!ed)
		{
			send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
			return false;
		}

		if (!ped)
			pObj->extra_descr = ed->next;
		else
			ped->next = ed->next;

		free_extra_descr(ed);

		send_to_char("Extra description deleted.\n\r", ch);
		return true;
	}


	if (!str_cmp(command, "format"))
	{
		if (keyword[0] == '\0')
		{
			send_to_char("Syntax:  ed format [keyword]\n\r", ch);
			return false;
		}

		for (ed = pObj->extra_descr; ed; ed = ed->next)
		{
			if (is_name(keyword, ed->keyword))
				break;
		}

		if (!ed)
		{
			send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
			return false;
		}

		if( !ed->description )
		{
			send_to_char("OEdit:  Extra description is an environmental extra description.\n\r", ch);
			return false;
		}

		ed->description = format_string(ed->description);

		send_to_char("Extra description formatted.\n\r", ch);
		return true;
	}

	if (!str_cmp(command, "show"))
	{
		if (keyword[0] == '\0')
		{
			send_to_char("Syntax:  ed show [keyword]\n\r", ch);
			return false;
		}

		for (ed = pObj->extra_descr; ed; ed = ed->next)
		{
			if (is_name(keyword, ed->keyword))
				break;
		}

		if (!ed)
		{
			send_to_char("OEdit:  Extra description keyword not found.\n\r", ch);
			return false;
		}

		if (!ed->description)
		{
			send_to_char("OEdit:  Cannot show environmental extra description.\n\r", ch);
			return false;
		}

		page_to_char(ed->description, ch);

		return true;
	}

	oedit_ed(ch, "");
	return false;
}


OEDIT(oedit_extra)
{
    OBJ_INDEX_DATA *pObj;
    //int value;

    if (argument[0] != '\0')
    {
		EDIT_OBJ(ch, pObj);
		
		long extra[4];

		if (!bitvector_lookup(argument, 4, extra, extra_flags, extra2_flags, extra3_flags, extra4_flags))
		{
			send_to_char("Invalid extra flag.\n\r", ch);
			send_to_char("Type '? extra' for a list of flags.\n\r", ch);
			return false;
		}

	    //TOGGLE_BIT(pObj->extra_flags, value);
		TOGGLE_BIT(pObj->extra[0], extra[0]);
		TOGGLE_BIT(pObj->extra[1], extra[1]);
		TOGGLE_BIT(pObj->extra[2], extra[2]);
		TOGGLE_BIT(pObj->extra[3], extra[3]);

	    send_to_char("Extra flag toggled.\n\r", ch);
	    return true;
	
    }

    send_to_char("Syntax:  extra [flag]\n\r"
		  "Type '? extra' for a list of flags.\n\r", ch);
    return false;
}

/*
OEDIT(oedit_extra2)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_OBJ(ch, pObj);
	
	// We should be adding checks to individual flags.

	if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
	{
	    send_to_char("You can't do this without an IMP's permission.\n\r", ch);
	    return false;
	}


	if ((value = flag_value(extra2_flags, argument)) != NO_FLAG)
	{
	    TOGGLE_BIT(pObj->extra2_flags, value);

	    if (has_imp_sig(NULL, pObj))
		use_imp_sig(NULL, pObj);

	    send_to_char("Extra2 flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  extra2 [flag]\n\r"
		  "Type '? extra2' for a list of flags.\n\r", ch);
    return false;
}

OEDIT(oedit_extra3)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_OBJ(ch, pObj);

	// We should be adding checks to individual flags.

	if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
	{
	    send_to_char("You can't do this without an IMP's permission.\n\r", ch);
	    return false;
	}


	if ((value = flag_value(extra3_flags, argument)) != NO_FLAG)
	{
	    TOGGLE_BIT(pObj->extra3_flags, value);

	    if (has_imp_sig(NULL, pObj))
		use_imp_sig(NULL, pObj);

	    send_to_char("Extra3 flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  extra3 [flag]\n\r"
		  "Type '? extra3' for a list of flags.\n\r", ch);
    return false;
}

OEDIT(oedit_extra4)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_OBJ(ch, pObj);

	// We should be adding checks to individual flags.

	if (!has_imp_sig(NULL, pObj) && ch->tot_level < MAX_LEVEL)
	{
	    send_to_char("You can't do this without an IMP's permission.\n\r", ch);
	    return false;
	}


	if ((value = flag_value(extra4_flags, argument)) != NO_FLAG)
	{
	    TOGGLE_BIT(pObj->extra4_flags, value);

	    if (has_imp_sig(NULL, pObj))
		use_imp_sig(NULL, pObj);

	    send_to_char("Extra4 flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  extra4 [flag]\n\r"
		  "Type '? extra4' for a list of flags.\n\r", ch);
    return false;
}
*/

OEDIT(oedit_wear)
{
    OBJ_INDEX_DATA *pObj;
    int value, wear;

    if (argument[0] != '\0')
    {
	EDIT_OBJ(ch, pObj);

	value = flag_value(wear_flags, argument);

	wear = (pObj->wear_flags ^ value) & ~(ITEM_TAKE|ITEM_CONCEALS|ITEM_NO_SAC);

	if((wear & -wear) != wear) {
		send_to_char("You can't set an object to be worn in more than one spot at once.\n\r", ch);
		return false;
	}

        if ((flag_value(wear_flags, argument) == ITEM_WEAR_BACK)
	&&     pObj->item_type != ITEM_RANGED_WEAPON)
	{
	    send_to_char("Only ranged weapons can be slung behind the back.\n\r", ch);
	    return false;
	}

	if ((flag_value(wear_flags, argument) == ITEM_WEAR_SHOULDER)
	&&     pObj->item_type != ITEM_WEAPON_CONTAINER)
	{
	    send_to_char("Only weapon containers can be worn on the shoulder.\n\r", ch);
	    return false;
	}

	if ((value = flag_value(wear_flags, argument)) != NO_FLAG)
	{
	    TOGGLE_BIT(pObj->wear_flags, value);

	    send_to_char("Wear flag toggled.\n\r", ch);

	    return true;
	}
    }

    send_to_char("Syntax:  wear [flag]\n\r"
		  "Type '? wear' for a list of flags.\n\r", ch);
    return false;
}


OEDIT(oedit_type)
{
	OBJ_INDEX_DATA *pObj;
	int value;
	int i;

	if (argument[0] != '\0')
	{
		EDIT_OBJ(ch, pObj);

		if ((value = flag_value(type_flags, argument)) != NO_FLAG)
		{
			if ((value == ITEM_KEYRING ||
				 value == ITEM_BANK ||
				 value == ITEM_SHARECERT ||
				 value == ITEM_ROOM_DARKNESS ||
				 value == ITEM_ROOM_FLAME ||
				 value == ITEM_SMOKE_BOMB ||
				 value == ITEM_MONEY ||
				 value == ITEM_WITHERING_CLOUD ||
				 value == ITEM_ROOM_ROOMSHIELD ||
				 value == ITEM_CATALYST ||
				 value == ITEM_SHIP ||
				 value == ITEM_SHRINE) &&
				ch->tot_level < MAX_LEVEL)
			{
				send_to_char("Sorry, only an IMP can set that item-type.\n\r",ch);
				return false;
			}

			pObj->item_type = value;

			send_to_char("Type set.\n\r", ch);

			// Clear the values.
			for (i = 0; i < 8; i++)
			{
				pObj->value[i] = 0;
			}

			// Defaults
			if( pObj->item_type == ITEM_TELESCOPE )
			{
				pObj->value[4] = -1;
			}

			if( pObj->lock )
			{
				free_lock_state(pObj->lock);
				pObj->lock = NULL;
			}

			if( pObj->waypoints )
			{
				list_destroy(pObj->waypoints);
				pObj->waypoints = NULL;
			}

			return true;
		}
	}

	send_to_char("Syntax:  type [flag]\n\r"
				"Type '? type' for a list of flags.\n\r", ch);
	return false;
}


OEDIT(oedit_material)
{
    OBJ_INDEX_DATA *pObj;
    int num;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  material [string]\n\r", ch);
	return false;
    }

    if ((num = material_lookup(argument)) == -1)
    {
	send_to_char("Invalid material. Type '? material.'\n\r", ch);
	return false;
    }

    free_string(pObj->material);
    pObj->material = str_dup(material_table[num].name);

    send_to_char("Material set.\n\r", ch);
    return true;
}


OEDIT(oedit_level)
{
    OBJ_INDEX_DATA *pObj;
    char buf[MAX_STRING_LENGTH];
    int armour;
    int armour_exotic;

    EDIT_OBJ(ch, pObj);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  level [number]\n\r", ch);
	return false;
    }

    pObj->level = atoi(argument);

    send_to_char("Level set.\n\r", ch);

    pObj->points = (int)pObj->level/10;
    sprintf(buf, "This object is now assigned {Y%d{x points.\n\r",
		    pObj->points);
    send_to_char(buf, ch);

    /* auto setting weapon dice stuff */
    if (pObj->item_type == ITEM_WEAPON)
    {
        set_weapon_dice(pObj);
	send_to_char("Damage dice set.\n\r", ch);
    }


    /* auto setting armour stuff */
    if (pObj->item_type == ITEM_ARMOUR)
    {
        armour=(int) calc_obj_armour(pObj->level, pObj->value[4]);
	armour_exotic=(int) armour * .90;

	pObj->value[0] = armour;
	pObj->value[1] = armour;
	pObj->value[2] = armour;
	pObj->value[3] = armour_exotic;

	send_to_char("Armour class set.\n\r", ch);
    }

    return true;
}


OEDIT(oedit_condition)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0'
    && (value = atoi (argument)) >= 0
    && (value <= 100))
    {
        EDIT_OBJ(ch, pObj);

        pObj->condition = value;
        send_to_char("Condition set.\n\r", ch);

        return true;
    }

    send_to_char("Syntax:  condition [number]\n\r"
                  "Where number can range from 0 (ruined) to 100 (perfect).\n\r"
,
                  ch);
    return false;
}


OEDIT(oedit_fragility)
{
    OBJ_INDEX_DATA *pObj;
    bool set = false;

    if (argument[0] != '\0')
    {
	EDIT_OBJ(ch, pObj);

	if (!str_cmp(argument, "Solid"))
	{
	    if (!str_cmp(pObj->imp_sig, "none")
	    && ch->tot_level < MAX_LEVEL)
	    {
		send_to_char("You can't do this without an IMP's "
			"permission.\n\r", ch);
		return false;
	    }

	    pObj->fragility = OBJ_FRAGILE_SOLID;
	    set = true;
	    use_imp_sig(NULL, pObj);
	}

	if (!str_cmp(argument, "Strong"))
	{
	    pObj->fragility = OBJ_FRAGILE_STRONG;
	    set = true;
	}

	if (!str_cmp(argument, "Normal"))
	{
	    pObj->fragility = OBJ_FRAGILE_NORMAL;
	    set = true;
	}

	if (!str_cmp(argument, "Weak"))
	{
	    pObj->fragility = OBJ_FRAGILE_WEAK;
	    set = true;
	}

	if (set)
	{
	    send_to_char("Fragility set.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax:  fragility  Solid|Strong|Normal|Weak\n\r"
	    "Fragility.\n\r",
	    ch);
    return false;
}


OEDIT(oedit_allowed_fixed)
{
    OBJ_INDEX_DATA *pObj;
    int value;

    if (argument[0] != '\0'
    && (value = atoi (argument)) >= 0
    && (value <= 100))
    {
	EDIT_OBJ(ch, pObj);

	pObj->times_allowed_fixed = value;
	send_to_char("Allowed Fixed Set.\n\r", ch);

	return true;
    }

    send_to_char("Syntax:  Allowed_fixed [number]\n\r"
		  "Number of times a person can fix the object.\n\r",
		  ch);
    return false;
}


int get_armour_strength(char *argument)
{
    char arg[MAX_STRING_LENGTH];
    argument = one_argument(argument, arg);

    if (!str_cmp(arg, "Heavy"))
	return OBJ_ARMOUR_HEAVY;

    else if (!str_cmp(arg, "Strong"))
	return OBJ_ARMOUR_STRONG;

    else if (!str_cmp(arg, "Medium"))
	return OBJ_ARMOUR_MEDIUM;

    else if (!str_cmp(arg, "Light"))
	return OBJ_ARMOUR_LIGHT;

    else if (!str_cmp(arg, "None"))
	return OBJ_ARMOUR_NOSTRENGTH;

    else
	return OBJ_ARMOUR_NOSTRENGTH;
}


MEDIT(medit_show)
{
	MOB_INDEX_DATA *pMob;
	char buf[MAX_STRING_LENGTH];
//	ITERATOR it;
//	PROG_LIST *trigger;
	BUFFER *buffer;

	EDIT_MOB(ch, pMob);

	buffer = new_buf();

	sprintf(buf, "Name:         {C[{x%s{C]{x\n\rArea:         {C[{x%5ld{C]{x %s\n\r",
		pMob->player_name,
		!pMob->area ? -1        : pMob->area->anum,
		!pMob->area ? "No Area" : pMob->area->name);
	add_buf(buffer, buf);
	sprintf(buf, "Loaded:       {C[{x%d{C]{x\n\r", pMob->count);
	add_buf(buffer, buf);

	sprintf(buf, "Sig:          {C[{x%s{C]{x   Creator: {C[{x%s{C]{x\n\r",
		pMob->sig, pMob->creator_sig);
	add_buf(buffer, buf);

	sprintf(buf, "Act:          {C[{x%s{C]{x\n\r",
		bitmatrix_string(act_flagbank, pMob->act));
	add_buf(buffer, buf);
/*
	sprintf(buf, "Act2:         {C[{x%s{C]{x\n\r",
		flag_string(act2_flags, pMob->act2));
	add_buf(buffer, buf);
*/
	sprintf(buf, "Vnum:         {C[{x%6ld{C]{x  Sex: {C[{x%7s{C]{x  Race: {C[{x%s{C]{x\n\r",
		pMob->vnum,
		pMob->sex == SEX_MALE    ? "male   " :
		pMob->sex == SEX_FEMALE  ? "female " :
		pMob->sex == 3           ? "random " : "neutral",
		race_table[pMob->race].name);
	add_buf(buffer, buf);

    sprintf(buf, "Boss:         {C[%s{C]{x\n\r", (pMob->persist ? "{RYES" : "{gno"));
    add_buf(buffer, buf);

    sprintf(buf, "Persist:      {C[%s{C]{x\n\r", (pMob->persist ? "{WON" : "{Doff"));
    add_buf(buffer, buf);

	if(pMob->attacks < 0) {
		sprintf(buf,
			"Level:        {C[{x%6d{C]{x  Align: {C[{x%6d{C]{x   Owner: {C[{x%s{C]{x\n\r"
			"Hitroll:      {C[{x%6d{C]{x  DamType: {C[{x%s{C]{x\n\r"
			"Movement:     {C[{x%6ld{C]{x  Number of attacks: {C[{Yscripted{C]{x\n\r",
			pMob->level,	pMob->alignment, pMob->owner,
			pMob->hitroll,	attack_table[pMob->dam_type].name,
			pMob->move);
	} else {
		sprintf(buf,
			"Level:        {C[{x%6d{C]{x  Align: {C[{x%6d{C]{x   Owner: {C[{x%s{C]{x\n\r"
			"Hitroll:      {C[{x%6d{C]{x  DamType: {C[{x%s{C]{x\n\r"
			"Movement:     {C[{x%6ld{C]{x  Number of attacks: {C[{x%d{C]{x\n\r",
			pMob->level,	pMob->alignment, pMob->owner,
			pMob->hitroll,	attack_table[pMob->dam_type].name,
			pMob->move, pMob->attacks );
	}
	add_buf(buffer, buf);

	sprintf(buf, "Hit dice:     {C[{x%2dd%-3d+%4d{C]{x ",
		pMob->hit.number,
		pMob->hit.size,
		pMob->hit.bonus);
	add_buf(buffer, buf);

	sprintf(buf, "Damage dice:  {C[{x%2dd%-3d+%4d{C]{x ",
		pMob->damage.number,
		pMob->damage.size,
		pMob->damage.bonus);
	add_buf(buffer, buf);

	sprintf(buf, "Mana dice:    {C[{x%2dd%-3d+%4d{C]{x\n\r",
		pMob->mana.number,
		pMob->mana.size,
		pMob->mana.bonus);
	add_buf(buffer, buf);

	sprintf(buf, "Affected by:  {C[{x%s{C]{x\n\r",
		bitvector_string(2, pMob->affected_by[0], affect_flags, pMob->affected_by[1], affect2_flags));
	add_buf(buffer, buf);
/*
	sprintf(buf, "Affected by2: {C[{x%s{C]{x\n\r",
		flag_string(affect2_flags, pMob->affected_by2));
	add_buf(buffer, buf);
*/
	sprintf(buf, "Armour:        {C[{xpierce: %d  bash: %d  slash: %d  magic: %d{C]{x\n\r",
		pMob->ac[AC_PIERCE], pMob->ac[AC_BASH],
		pMob->ac[AC_SLASH],  pMob->ac[AC_EXOTIC]);
	add_buf(buffer, buf);

	sprintf(buf, "Parts:        {C[{x%s{C]{x\n\r", flag_string(part_flags, pMob->parts));
	add_buf(buffer, buf);

	sprintf(buf, "Imm:          {C[{x%s{C]{x\n\r", flag_string(imm_flags, pMob->imm_flags));
	add_buf(buffer, buf);

	sprintf(buf, "Res:          {C[{x%s{C]{x\n\r", flag_string(res_flags, pMob->res_flags));
	add_buf(buffer, buf);

	sprintf(buf, "Vuln:         {C[{x%s{C]{x\n\r", flag_string(vuln_flags, pMob->vuln_flags));
	add_buf(buffer, buf);

	sprintf(buf, "Off:          {C[{x%s{C]{x\n\r", flag_string(off_flags,  pMob->off_flags));
	add_buf(buffer, buf);

	sprintf(buf, "Size:         {C[{x%s{C]{x\n\r", flag_string(size_flags, pMob->size));
	add_buf(buffer, buf);

	sprintf(buf, "Material:     {C[{x%s{C]{x\n\r", pMob->material);
	add_buf(buffer, buf);

	sprintf(buf, "Start pos:    {C[{x%s{C]{x\n\r", flag_string(position_flags, pMob->start_pos));
	add_buf(buffer, buf);

	sprintf(buf, "Default pos:  {C[{x%s{C]{x\n\r", flag_string(position_flags, pMob->default_pos));
	add_buf(buffer, buf);

	sprintf(buf, "Wealth:       {C[{x%8ld{C]{x\n\r", pMob->wealth);
	add_buf(buffer, buf);

	sprintf(buf, "Script Kwds:  {C[{x%s{C]{x\n\r", pMob->skeywds);
	add_buf(buffer, buf);

	if (pMob->spec_fun) {
		sprintf(buf, "Spec fun:     {C[{x%s{C]{x\n\r",  spec_name(pMob->spec_fun));
		add_buf(buffer, buf);
	}

	sprintf(buf, "Corpse Type:  {C[{x%s{C]{x\n\r", flag_string(corpse_types, pMob->corpse_type));
	add_buf(buffer, buf);

	if (pMob->corpse) {
		OBJ_INDEX_DATA *obj = get_obj_index(pMob->corpse);
		sprintf(buf, "Corpse Obj:   {C[{x%s{C]{x\n\r",  obj->short_descr);
		add_buf(buffer, buf);
	}

	if (pMob->zombie) {
		OBJ_INDEX_DATA *obj = get_obj_index(pMob->zombie);
		sprintf(buf, "Zombie Obj:   {C[{x%s{C]{x\n\r",  obj->short_descr);
		add_buf(buffer, buf);
	}


	sprintf(buf, "Short descr: %s\n\rLong descr:\n\r     %s", pMob->short_descr, pMob->long_descr);
	add_buf(buffer, buf);

	sprintf(buf, "Description:\n\r%s", pMob->description);
	add_buf(buffer, buf);

	sprintf(buf, "\n\r-----\n\r{WBuilders' Comments:{X\n\r%s\n\r-----\n\r", pMob->comments);
	add_buf(buffer, buf);


	/*
	if (pMob->next_crew_for_sale != NULL)
	{
	MOB_INDEX_DATA *temp_mob;
	sprintf(buf,"Crew for hire:\n\r"
	"-------------------\n\r");
	send_to_char(buf, ch);

	for (temp_mob = pMob->next_crew_for_sale; temp_mob != NULL; temp_mob = temp_mob->next_crew_for_sale)
	{
	sprintf(buf, "%-16s %ld\n\r", temp_mob->short_descr, temp_mob->vnum);
	send_to_char(buf, ch);
	}
	}
	*/

	if (pMob->pShop) {
		SHOP_DATA *pShop;
		int iTrade;

		pShop = pMob->pShop;

		sprintf(buf,
			"Shop data for {C[{x%5ld{C]{x:\n\r"
			"  Markup for purchaser: %d%%\n\r"
			"  Markdown for seller:  %d%%\n\r",
			pShop->keeper, pShop->profit_buy, pShop->profit_sell);
		add_buf(buffer, buf);
		sprintf(buf, "  Hours: %d to %d.\n\r", pShop->open_hour, pShop->close_hour);
		add_buf(buffer, buf);

		if( pShop->restock_interval > 0 )
			sprintf(buf, "  Restocking: %d (minutes)\n\r", pShop->restock_interval);
		else
			sprintf(buf, "  Restocking: disabled\n\r");
		add_buf(buffer, buf);

		sprintf(buf, "  Discount Rate: %d%%\n\r", pShop->discount);
		add_buf(buffer, buf);

		sprintf(buf, "  Flags: %s\n\r", flag_string(shop_flags, pShop->flags));
		add_buf(buffer, buf);

		for (iTrade = 0; iTrade < MAX_TRADE; iTrade++) {
			if (pShop->buy_type[iTrade]) {
				if (!iTrade) {
					add_buf(buffer, "  Number Trades Type\n\r");
					add_buf(buffer, "  ------ -----------\n\r");
				}
				sprintf(buf, "  {C[{x%4d{C]{x %s\n\r", iTrade, flag_string(type_flags, pShop->buy_type[iTrade]));
				add_buf(buffer, buf);
			}
		}

		if(pShop->shipyard > 0)
		{
			WILDS_DATA *wilds = get_wilds_from_uid(NULL, pShop->shipyard);

			sprintf(buf, "  Shipyard: %s (%ld) at (%d,%d) to (%d,%d)\n\r",
				wilds?wilds->name:"(null)", pShop->shipyard,
				pShop->shipyard_region[0][0], pShop->shipyard_region[0][1],
				pShop->shipyard_region[1][0], pShop->shipyard_region[1][1]);
			add_buf(buffer, buf);

			sprintf(buf, "            %s\n\r", pShop->shipyard_description);
			add_buf(buffer, buf);
		}

		if(pShop->stock != NULL)
		{
			SHOP_STOCK_DATA *pStock;
			int iStock;
			char lvl[MIL];
			char qty[MIL];
			char pricing[MIL];
			char typ[MIL];
			char hours[MIL];
			char item[MIL];
			char disc[MIL];
			int hwidth, lwidth, qwidth, pwidth;

			for(iStock = 1, pStock = pShop->stock;pStock;pStock = pStock->next, iStock++)
			{
				if(iStock == 1)
				{
					add_buf(buffer, "{G  Stock# Level Quantity Sng Hours    Price(s)    Disc                   Item{x\n\r");
					add_buf(buffer, "{G  ------ ----- -------- --- ----- -------------- ---- --------------------------------------{x\n\r");
				}

				if( pStock->level > 0 )
				{
					sprintf(lvl, "{Y%d{x", pStock->level);
				}
				else
				{
					strcpy(lvl, "{GAuto{x");
				}
				lwidth = get_colour_width(lvl) + 5;

				if( pStock->quantity > 0 )
				{
					if( pStock->restock_rate > 0 )
					{
						sprintf(qty, "{W%d{x / {W%d{x", pStock->quantity, pStock->restock_rate);
					}
					else
					{
						sprintf(qty, "{W%d{x / {D--{x", pStock->quantity);
					}
				}
				else
				{
					strcpy(qty, "   {D--{x   ");
				}
				qwidth = get_colour_width(qty) + 8;

				if( pStock->duration > 0 )
				{
					sprintf(hours, "{G%d{x", pStock->duration);
				}
				else
				{
					strcpy(hours, " {D---{x ");
				}
				hwidth = get_colour_width(hours) + 5;

				if( !IS_NULLSTR(pStock->custom_price) )
				{
					strncpy(pricing, pStock->custom_price, sizeof(pricing)-3);
					strcat(pricing, "{x");
					strcpy(disc, " {D--{x ");
				}
				else
				{
					pricing[0] = '\0';
					int pj = 0;

					if( pStock->silver > 0)
					{
						long silver = pStock->silver % 100;
						long gold = pStock->silver / 100;

						if( gold > 0 )
						{
							if( silver > 0 )
							{
								pj = sprintf(pricing, "{x%ld{Yg{x%ld{Ws{x", gold, silver);
							}
							else
							{
								pj = sprintf(pricing, "{x%ld{Yg{x", gold);
							}
						}
						else
						{
							pj = sprintf(pricing, "{x%ld{Ws{x", silver);
						}
					}

					if( pStock->qp > 0 )
					{
						if( pj > 0 )
						{
							pricing[pj++] = ',';
							pricing[pj++] = ' ';
						}

						pj += sprintf(pricing+pj, "{x%ld{Gqp{x", pStock->qp);
					}

					if( pStock->dp > 0 )
					{
						if( pj > 0 )
						{
							pricing[pj++] = ',';
							pricing[pj++] = ' ';
						}

						pj += sprintf(pricing+pj, "{x%ld{Mdp{x", pStock->dp);
					}

					if( pStock->pneuma > 0 )
					{
						if( pj > 0 )
						{
							pricing[pj++] = ',';
							pricing[pj++] = ' ';
						}

						pj += sprintf(pricing+pj, "{x%ld{Cpn{x", pStock->pneuma);
					}
					pricing[pj] = '\0';
					sprintf(disc, "%3d%%", pStock->discount);
				}
				pwidth = get_colour_width(pricing) + 14;

				switch(pStock->type)
				{
				case STOCK_OBJECT:
					strcpy(typ,"{GOBJECT{x  ");
					if( pStock->vnum > 0 ) {

						OBJ_INDEX_DATA *obj = get_obj_index(pStock->vnum);

						if( !obj ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", obj->short_descr, pStock->vnum);
						}
					}
					else
						strcpy(item, "-invalid-");

					break;
				case STOCK_PET:
					strcpy(typ,"{GPET{x     ");
					if( pStock->vnum > 0 ) {

						MOB_INDEX_DATA *mob = get_mob_index(pStock->vnum);

						if( !mob ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", mob->short_descr, pStock->vnum);
						}
					}
					else
						strcpy(item, "-invalid-");
					break;
				case STOCK_MOUNT:
					strcpy(typ,"{GMOUNT{x   ");
					if( pStock->vnum > 0 ) {

						MOB_INDEX_DATA *mob = get_mob_index(pStock->vnum);

						if( !mob ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", mob->short_descr, pStock->vnum);
						}
					}
					else
						strcpy(item, "-invalid-");
					break;
				case STOCK_GUARD:
					strcpy(typ,"{GGUARD{x   ");
					if( pStock->vnum > 0 ) {

						MOB_INDEX_DATA *mob = get_mob_index(pStock->vnum);

						if( !mob ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", mob->short_descr, pStock->vnum);
						}
					}
					else
						strcpy(item, "-invalid-");
					break;

				case STOCK_CREW:
					strcpy(typ,"{GCREW{x    ");
					if( pStock->vnum > 0 ) {

						MOB_INDEX_DATA *mob = get_mob_index(pStock->vnum);

						if( !mob || !mob->pCrew ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", mob->short_descr, pStock->vnum);
						}
					}
					else
						strcpy(item, "-invalid-");
					break;

				case STOCK_SHIP:
					strcpy(typ,"{GSHIP{x    ");
					if( pStock->vnum > 0 )
					{
						SHIP_INDEX_DATA *ship_index = get_ship_index(pStock->vnum);

						if( !ship_index ) {
							strcpy(item, "-invalid-");
						}
						else
						{
							sprintf(item, "%s (%ld)", ship_index->name, pStock->vnum);
						}

					}
					else
						strcpy(item, "-invalid-");
					break;
				case STOCK_CUSTOM:
					strcpy(typ,"{GCUSTOM{x  ");
					if(IS_NULLSTR(pStock->custom_keyword))
					{
						strcpy(item, "-invalid stock item-");
					}
					else
					{
						strcpy(item, pStock->custom_keyword);
					}
					break;
				}

				sprintf(buf, "  {G[{x%4d{G]{x %-*s %*s  %s  %*s %-*s %s %s%s\n\r", iStock, lwidth, lvl, qwidth, qty, (pStock->singular?"{RY{x":"{GN{x"), hwidth, hours, pwidth, pricing, disc, typ, item);
				add_buf(buffer,buf);

				if( !IS_NULLSTR(pStock->custom_descr) )
				{
					sprintf(buf, "                                                              - %s\n\r", pStock->custom_descr);
					add_buf(buffer, buf);
				}
			}
		}
	}

	if ( IS_VALID(pMob->pCrew) )
	{
		add_buf(buffer, "{CShip Crew Data:{x\n\r");
		add_buf(buffer, "{C================================{x\n\r");

		sprintf(buf, "{CMinimum Rank{c:      {WNYI{x\n\r");
		add_buf(buffer, buf);

		sprintf(buf, "{CScouting Rating{c:   {C[{x%d%%{C]{x\n\r", pMob->pCrew->scouting);
		add_buf(buffer, buf);

		sprintf(buf, "{CGunning Rating{c:    {C[{x%d%%{C]{x\n\r", pMob->pCrew->gunning);
		add_buf(buffer, buf);

		sprintf(buf, "{COarring Rating{c:    {C[{x%d%%{C]{x\n\r", pMob->pCrew->oarring);
		add_buf(buffer, buf);

		sprintf(buf, "{CMechanics Rating{c:  {C[{x%d%%{C]{x\n\r", pMob->pCrew->mechanics);
		add_buf(buffer, buf);

		sprintf(buf, "{CNavigation Rating{c: {C[{x%d%%{C]{x\n\r", pMob->pCrew->navigation);
		add_buf(buffer, buf);

		sprintf(buf, "{CLeadership Rating{c: {C[{x%d%%{C]{x\n\r", pMob->pCrew->leadership);
		add_buf(buffer, buf);

		add_buf(buffer, "\n\r");
	}

	if (pMob->pQuestor)
	{
		QUESTOR_DATA *questor = pMob->pQuestor;

		add_buf(buffer, "{YQuestor data:\n\r");

		sprintf(buf, "  {YScroll Vnum: %ld\n\r", questor->scroll);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->keywords))
			sprintf(buf, "  {YKeywords: (empty){x\n\r");
		else
			sprintf(buf, "  {YKeywords: %s{x\n\r", questor->keywords);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->short_descr))
			sprintf(buf, "  {YShort Description: (empty){x\n\r");
		else
			sprintf(buf, "  {YShort Description: %s{x\n\r", questor->short_descr);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->long_descr))
			sprintf(buf, "  {YDescription: (empty){x\n\r");
		else
			sprintf(buf, "  {YDescription: %s{x\n\r", questor->long_descr);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->header))
			sprintf(buf, "  {YHeader: (empty){x\n\r");
		else
			sprintf(buf, "  {YHeader:\n\r%s{x\n\r", questor->header);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->footer))
			sprintf(buf, "  {YFooter: (empty){x\n\r");
		else
			sprintf(buf, "  {YFooter:\n\r%s{x\n\r", questor->footer);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->prefix))
			sprintf(buf, "  {YPrefix: (empty){x\n\r");
		else
			sprintf(buf, "  {YPrefix: %s{x\n\r", questor->prefix);
		add_buf(buffer, buf);

		if(IS_NULLSTR(questor->suffix))
			sprintf(buf, "  {YSuffix: (empty){x\n\r");
		else
			sprintf(buf, "  {YSuffix: %s{x\n\r", questor->suffix);
		add_buf(buffer, buf);

		if( questor->line_width > 0 )
			sprintf(buf, "  {YWidth:  %d{x\n\r", questor->line_width);
		else
			sprintf(buf, "  {YWidth:  disabled{x\n\r");
		add_buf(buffer, buf);
		add_buf(buffer, "\n\r");
	}

    if (pMob->progs)
		olc_show_progs(buffer, pMob->progs, PRG_MPROG, "MobProg Vnum");

	if (pMob->index_vars)
		olc_show_index_vars(buffer, pMob->index_vars);

	page_to_char(buf_string(buffer), ch);
	free_buf(buffer);
	return false;
}


MEDIT(medit_next)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *nextMob = NULL;
    long next_vnum;

    EDIT_MOB(ch, pMob);

    next_vnum = pMob->vnum;

    next_vnum++;
    while (nextMob == NULL
    && next_vnum <= pMob->area->max_vnum)
    {
	nextMob = get_mob_index(next_vnum);
	next_vnum++;
    }

    if (nextMob == NULL)
    {
	send_to_char("No next mob in area.\n\r", ch);
    }
    else
    {
	edit_done(ch);
	ch->desc->pEdit = (void *)nextMob;
	ch->desc->editor = ED_MOBILE;
    }
    return false;
}

MEDIT(medit_persist)
{
	MOB_INDEX_DATA *pMob;

	EDIT_MOB(ch, pMob);


	if (!str_cmp(argument,"on")) {
	    if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }

		pMob->persist = true;
	    use_imp_sig(pMob, NULL);
		send_to_char("Persistance enabled.\n\r", ch);
	} else if (!str_cmp(argument,"off")) {
		pMob->persist = false;
		send_to_char("Persistance disabled.\n\r", ch);
	} else {
		send_to_char("Usage: persist on/off\n\r", ch);
		return false;
	}

	return true;
}


MEDIT(medit_boss)
{
	MOB_INDEX_DATA *pMob;

	EDIT_MOB(ch, pMob);

	if (!str_cmp(argument,"on")) {
	    if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL) {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }

		pMob->boss = true;
	    use_imp_sig(pMob, NULL);
		send_to_char("Boss status enabled.\n\r", ch);
	} else if (!str_cmp(argument,"off")) {
		pMob->boss= false;
		send_to_char("Boss status disabled.\n\r", ch);
	} else {
		send_to_char("Usage: boss on/off\n\r", ch);
		return false;
	}

	return true;
}

MEDIT(medit_prev)
{
    MOB_INDEX_DATA *pMob;
    MOB_INDEX_DATA *prevMob = NULL;
    long prev_vnum;

    EDIT_MOB(ch, pMob);

    prev_vnum = pMob->vnum;

    prev_vnum--;
    while (prevMob == NULL
    && prev_vnum >= pMob->area->min_vnum)
    {
	prevMob = get_mob_index(prev_vnum);
	prev_vnum--;
    }

    if (prevMob == NULL)
    {
	send_to_char("No previous mob in area.\n\r", ch);
    }
    else
    {
	edit_done(ch);
	ch->desc->pEdit = (void *)prevMob;
	ch->desc->editor = ED_MOBILE;
    }
    return false;
}


MEDIT(medit_attacks)
{
	MOB_INDEX_DATA *pMob;
	int value;

	EDIT_MOB(ch, pMob);


	if (!str_prefix(argument,"scripted"))
		value = -1;
	else if ((value = atoi(argument)) < 0 || value > 10) {
		send_to_char("Invalid number.\n\r", ch);
		return false;
	}

	pMob->attacks = value;
	send_to_char("Number of attacks set.\n\r", ch);
	return true;
}


MEDIT(medit_owner)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  owner [string]\n\r", ch);
	return false;
    }

    free_string(pMob->owner);
    pMob->owner = str_dup(argument);

    send_to_char("Owner set.\n\r", ch);
    return true;
}


MEDIT(medit_create)
{
    MOB_INDEX_DATA *pMob;
    AREA_DATA *pArea;
    long  value;
    int  iHash;
    long auto_vnum = 0;

    value = atol(argument);
    if (argument[0] == '\0' || value == 0)
    {
	//send_to_char("Syntax:  medit create [vnum]\n\r", ch);
	MOB_INDEX_DATA *temp_mob;

	auto_vnum = ch->in_room->area->min_vnum;
	temp_mob = get_mob_index(auto_vnum);
	if (temp_mob != NULL)
	{
	    while (temp_mob != NULL)
	    {
		temp_mob = get_mob_index(auto_vnum);
		if (temp_mob == NULL) break;
		auto_vnum++;
	    }
	}

	if (auto_vnum > ch->in_room->area->max_vnum)
	{
	    send_to_char("Sorry, this area has no more space left.\n\r",
		    ch);
	    return false;
	}
    }

    if (auto_vnum != 0) value = auto_vnum;

    pArea = get_vnum_area(value);

    if (!pArea)
    {
	send_to_char("MEdit:  That vnum is not assigned an area.\n\r", ch);
	return false;
    }

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("MEdit:  Vnum in an area you cannot build in.\n\r", ch);
	return false;
    }

    if (get_mob_index(value))
    {
	send_to_char("MEdit:  Mobile vnum already exists.\n\r", ch);
	return false;
    }

    pMob			= new_mob_index();
    pMob->vnum			= value;
    pMob->area			= pArea;

    if (value > top_vnum_mob)
	top_vnum_mob = value;

    pMob->act[0]			= ACT_IS_NPC;
    pMob->act[1]			= 0;
    iHash			= value % MAX_KEY_HASH;
    pMob->next			= mob_index_hash[iHash];
    mob_index_hash[iHash]	= pMob;
    ch->desc->pEdit		= (void *)pMob;


    // Make sure to set minimum level to 1.
    pMob->level = 1;
    set_mob_hitdice(pMob);
    set_mob_damdice(pMob);
    if (!IS_SET(pMob->act[0], ACT_MOUNT))
	set_mob_movedice(pMob);

    send_to_char("Mobile Created.\n\r", ch);
    SET_BIT(pMob->area->area_flags, AREA_CHANGED);
    free_string(pMob->creator_sig);
    pMob->creator_sig = str_dup(ch->name);
    return true;
}


MEDIT(medit_spec)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  spec [special function]\n\r", ch);
	return false;
    }


    if (!str_cmp(argument, "none"))
    {
        pMob->spec_fun = NULL;

        send_to_char("Spec removed.\n\r", ch);
        return true;
    }

    if (spec_lookup(argument))
    {
	pMob->spec_fun = spec_lookup(argument);
	send_to_char("Spec set.\n\r", ch);
	return true;
    }

    send_to_char("MEdit: No such special function.\n\r", ch);
    return false;
}


MEDIT(medit_damtype)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  damtype [damage message]\n\r", ch);
	send_to_char("For a list of damtypes, type '? weapon'.\n\r", ch);
	return false;
    }

    pMob->dam_type = attack_lookup(argument);
    send_to_char("Damage type set.\n\r", ch);
    return true;
}


MEDIT(medit_align)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  alignment [number]\n\r", ch);
	return false;
    }

    pMob->alignment = atoi(argument);

    send_to_char("Alignment set.\n\r", ch);
    return true;
}


MEDIT(medit_level)
{
    MOB_INDEX_DATA *pMob;
    EDIT_MOB(ch, pMob);
    char buf[MSL];

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  level [number]\n\r", ch);
	return false;
    }

    if (atoi(argument) == 0) {
	send_to_char("Sorry, mob levels start at 1.\n\r", ch);
	return false;
    }

    if (atoi(argument) > MAX_MOB_SKILL_LEVEL) {
	sprintf(buf, "Sorry, max mob level is %d.\n\r", MAX_MOB_SKILL_LEVEL);
	return false;
    }

    pMob->level = atoi(argument);

    send_to_char("Level set.\n\r", ch);
    set_mob_hitdice(pMob);
    send_to_char("Hit Dice set.\n\r", ch);
    set_mob_damdice(pMob);
    send_to_char("Damage dice set.\n\r", ch);

    if (!IS_SET(pMob->act[0], ACT_MOUNT)) {
	set_mob_movedice(pMob);
	send_to_char("Movement dice set.\n\r", ch);
    }

    if (IS_SET(pMob->off_flags, OFF_MAGIC))
    {
	set_mob_manadice(pMob);
	send_to_char("Mana dice set.\n\r", ch);
    }

    return true;

}


MEDIT(medit_desc)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	string_append(ch, &pMob->description);
	return true;
    }

    send_to_char("Syntax:  desc    - line edit\n\r", ch);
    return false;
}

MEDIT(medit_comments)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	string_append(ch, &pMob->comments);
	return true;
    }

    send_to_char("Syntax:  desc    - line edit\n\r", ch);
    return false;
}


MEDIT(medit_long)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  long [string]\n\r", ch);
	return false;
    }

    free_string(pMob->long_descr);
    strcat(argument, "{x\n\r");
    pMob->long_descr = str_dup(argument);
    pMob->long_descr[0] = UPPER(pMob->long_descr[0] );

    send_to_char("Long description set.\n\r", ch);
    return true;
}


MEDIT(medit_short)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  short [string]\n\r", ch);
	return false;
    }

    free_string(pMob->short_descr);
    pMob->short_descr = str_dup(argument);

    send_to_char("Short description set.\n\r", ch);
    if (IS_SET(ch->act[0], PLR_AUTOSETNAME))
    {
	free_string(pMob->player_name);
	pMob->player_name = short_to_name(pMob->short_descr);
	send_to_char("Name keywords set.\n\r", ch);
    }
    return true;
}


MEDIT(medit_name)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  name [string]\n\r", ch);
	return false;
    }

    sprintf(name, "%s%c/%s", PLAYER_DIR, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) == NULL)
    {
	free_string(pMob->player_name);
	pMob->player_name = str_dup(argument);

	send_to_char("Name set.\n\r", ch);
    }
    else
    {
	send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
	fclose(fp);
	return false;
    }

    return true;
}


MEDIT(medit_sign)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 154)
    {
	send_to_char("Sorry, only immortals of level 154 and above can do that.\n\r", ch);
	return false;
    }

    free_string(pMob->sig);
    pMob->sig = str_dup(ch->name);

    send_to_char("Mobile signed.\n\r", ch);

    return true;
}

MEDIT(medit_skeywds)
{
    MOB_INDEX_DATA *pMob;
    char name[MSL];
    FILE *fp;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  skwds [string]\n\r", ch);
	return false;
    }

    sprintf(name, "%s%c/%s", PLAYER_DIR, tolower(argument[0]), capitalize(argument));
    if ((fp = fopen(name, "r")) == NULL)
    {
	free_string(pMob->skeywds);
	pMob->skeywds = str_dup(argument);

	send_to_char("Script keywords set.\n\r", ch);
    }
    else
    {
	send_to_char("Sorry, there is a player with that name, so you can't set it on your mob.\n\r", ch);
	fclose(fp);
	return false;
    }

    return true;
}


MEDIT(medit_varset)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

	return olc_varset(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_varclear)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

	return olc_varclear(&pMob->index_vars, ch, argument, false);
}

MEDIT(medit_corpsetype)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
		EDIT_MOB(ch, pMob);

		if (!str_cmp(argument, "normal") || !str_cmp(argument, "none")) {
			pMob->corpse_type = RAWKILL_NORMAL;

			 send_to_char("Corpse type set.\n\r", ch);
			return true;
		} else if ((value = flag_value(corpse_types, argument)) != NO_FLAG) {
			pMob->corpse_type = value;

			 send_to_char("Corpse type set.\n\r", ch);
			return true;
		}
    }

    send_to_char("Syntax: corpsetype [type]\n\r"
		  "Type '? corpsetypes' for a list of flags.\n\r", ch);
    return false;
}

MEDIT(medit_corpsevnum)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0' && is_number(argument))
    {
	EDIT_MOB(ch, pMob);

	value = atoi(argument);

	if (value > 0) {

		if(!get_obj_index(value)) {
			send_to_char("Object does not exist.\n\r",ch);
			return false;
		} else {
			send_to_char("Corpse object vnum set.\n\r",ch);
			pMob->corpse = value;
			return true;
		}
	} else if(!value) {
		send_to_char("Corpse object cleared.\n\r",ch);
		pMob->corpse = value;
		return true;
	}
    }

    send_to_char("Syntax: corpsevnum [vnum >= 0] (0 to clear)\n\r", ch);
    return false;
}

MEDIT(medit_zombievnum)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0' && is_number(argument))
    {
	EDIT_MOB(ch, pMob);

	value = atoi(argument);

	if (value > 0) {
		if(!get_obj_index(value)) {
			send_to_char("Object does not exist.\n\r",ch);
			return false;
		} else {
			send_to_char("Zombie corpse object set.\n\r",ch);
			pMob->zombie = value;
			return true;
		}
	} else if(!value) {
		send_to_char("Zombie corpse object cleared.\n\r",ch);
		pMob->zombie = value;
		return true;
	}
    }

    send_to_char("Syntax: zombievnum [vnum >= 0] (0 to clear)\n\r", ch);
    return false;
}

MEDIT(medit_shop)
{
    MOB_INDEX_DATA *pMob;
    char command[MAX_INPUT_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char *flag_start;

    argument = one_argument(argument, command);
    flag_start = argument;
    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    EDIT_MOB(ch, pMob);

    if (command[0] == '\0')
    {
		send_to_char("Syntax:  shop assign\n\r", ch);
		send_to_char("         shop remove\n\r\n\r", ch);

		send_to_char("         shop discount [0-100] [reset]\n\r", ch);
		send_to_char("         shop flags [flags]\n\r", ch);
		send_to_char("         shop hours [#xopening] [#xclosing]\n\r", ch);
		send_to_char("         shop profit [#xbuying%] [#xselling%]\n\r", ch);
		send_to_char("         shop restock [minutes]\n\r", ch);
		send_to_char("         shop shipyard clear\n\r", ch);
		send_to_char("         shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
		send_to_char("         shop stock add [type] [value]\n\r", ch);
		send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
		send_to_char("         shop stock [#] description [description]\n\r", ch);
		send_to_char("         shop stock [#] level [level]\n\r", ch);
		send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
		send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
		send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
		send_to_char("         shop stock [#] singular\n\r", ch);
		send_to_char("         shop stock [#] remove\n\r", ch);
		send_to_char("         shop type [#x0-4] [item type]\n\r", ch);
		return false;
    }


    if (!str_prefix(command, "hours"))
    {
		if (arg1[0] == '\0' || !is_number(arg1) ||
			argument[0] == '\0' || !is_number(arg2))
		{
			send_to_char("Syntax:  shop hours [#xopening] [#xclosing]\n\r", ch);
			return false;
		}

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		pMob->pShop->open_hour = atoi(arg1);
		pMob->pShop->close_hour = atoi(arg2);

		send_to_char("Shop hours set.\n\r", ch);
		return true;
    }

    if (!str_prefix(command, "restock"))
    {
		if (arg1[0] == '\0' || !is_number(arg1))
		{
			send_to_char("Syntax:  shop restock [minutes]\n\r", ch);
			send_to_char("   Specify at least 10 minutes, or 0 to disable restocking.\n\r", ch);
			return false;
		}

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		int interval = atoi(arg1);

		if( interval <= 0 )
		{
			send_to_char("Restocking disabled.\n\r", ch);
			pMob->pShop->restock_interval = 0;
			return true;
		}
		else if( interval < 10 )
		{
			send_to_char("Interval too short.\n\rPlease try at least 10 minutes, or 0 to disable restocking.\n\r", ch);
			return false;
		}
		else
		{
			send_to_char("Restocking changed.\n\r", ch);
			pMob->pShop->restock_interval = interval;
			return true;
		}
    }

    if (!str_prefix(command, "shipyard"))
    {
		char arg3[MIL];
		char arg4[MIL];
		char arg5[MIL];
		argument = one_argument(argument, arg3);
		argument = one_argument(argument, arg4);
		argument = one_argument(argument, arg5);

		if( !str_cmp(arg1, "clear") )
		{
			pMob->pShop->shipyard = 0;
			pMob->pShop->shipyard_region[0][0] = 0;
			pMob->pShop->shipyard_region[0][1] = 0;
			pMob->pShop->shipyard_region[1][0] = 0;
			pMob->pShop->shipyard_region[1][1] = 0;

			free_string(pMob->pShop->shipyard_description);
			pMob->pShop->shipyard_description = &str_empty[0];

			send_to_char("Shipyard cleared.\n\r", ch);
			return true;
		}
		if( !is_number(arg1) || !is_number(arg2) || !is_number(arg3) || !is_number(arg4) || !is_number(arg5) || IS_NULLSTR(argument) )
		{
			send_to_char("Syntax:  shop shipyard <wuid> <x1> <y1> <x2> <y2> <description>\n\r", ch);
			send_to_char("         shop shipyard clear\n\r", ch);
			return false;
		}
		long wuid = atol(arg1);
		int x1 = atoi(arg2);
		int y1 = atoi(arg3);
		int x2 = atoi(arg4);
		int y2 = atoi(arg5);

		if( !is_shipyard_valid(wuid, x1, y1, x2, y2) )
		{
			send_to_char("Shipyard not valid.  Please verify wilderness and coordinates.\n\r", ch);
			send_to_char("Make sure Shipyard has safe harbor water tiles next to non-water tiles.\n\r", ch);
			return false;
		}

		pMob->pShop->shipyard = wuid;
		pMob->pShop->shipyard_region[0][0] = x1;
		pMob->pShop->shipyard_region[0][1] = y1;
		pMob->pShop->shipyard_region[1][0] = x2;
		pMob->pShop->shipyard_region[1][1] = y2;

		smash_tilde(argument);
		free_string(pMob->pShop->shipyard_description);
		pMob->pShop->shipyard_description = str_dup(argument);


		send_to_char("Shipyard set.\n\r", ch);
		return true;
	}

    if (!str_prefix(command, "discount"))
    {
		if (arg1[0] == '\0' || !is_number(arg1))
		{
			send_to_char("Syntax:  shop discount [0-100] [reset]\n\r", ch);
			return false;
		}

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		int disc = atoi(arg1);

		if( disc < 0 || disc > 100 )
		{
			send_to_char("Discount must be a percentage (0-100).\n\r", ch);
			return false;
		}

		pMob->pShop->discount = disc;

		if( !str_cmp(arg2, "reset") && pMob->pShop->stock != NULL )
		{
			bool updated = false;
			for(SHOP_STOCK_DATA *stock = pMob->pShop->stock; stock; stock = stock->next)
			{
				if( IS_NULLSTR(stock->custom_keyword) )
				{
					stock->discount = pMob->pShop->discount;
					updated = true;
				}
			}

			if( updated )
				send_to_char("Discount changed, and stock updated.\n\r", ch);
			else
				send_to_char("Discount changed.\n\r", ch);
		}
		else
			send_to_char("Discount changed.\n\r", ch);
		return true;
    }


    if (!str_prefix(command, "profit"))
    {
		if (arg1[0] == '\0' || !is_number(arg1) ||
			argument[0] == '\0' || !is_number(arg2))
		{
			send_to_char("Syntax:  shop profit [#xbuying%] [#xselling%]\n\r", ch);
			return false;
		}

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		pMob->pShop->profit_buy     = atoi(arg1);
		pMob->pShop->profit_sell    = atoi(arg2);

		send_to_char("Shop profit set.\n\r", ch);
		return true;
    }


    if (!str_prefix(command, "type"))
    {
		char buf[MAX_INPUT_LENGTH];
		int value;

		if (arg1[0] == '\0' || !is_number(arg1) || arg2[0] == '\0')
		{
		    send_to_char("Syntax:  shop type [#x0-4] [item type]\n\r", ch);
		    return false;
		}

		if (atoi(arg1) >= MAX_TRADE)
		{
			sprintf(buf, "MEdit:  May sell %d items max.\n\r", MAX_TRADE);
			send_to_char(buf, ch);
			return false;
		}

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		if ((value = flag_value(type_flags, arg2)) == NO_FLAG)
		{
			send_to_char("MEdit:  That type of item is not known.\n\r", ch);
			return false;
		}

		pMob->pShop->buy_type[atoi(arg1)] = value;

		send_to_char("Shop type set.\n\r", ch);
		return true;
    }

    /* shop assign && shop delete by Phoenix */

    if (!str_prefix(command, "assign"))
    {
    	if (pMob->pShop)
    	{
        	send_to_char("Mob already has a shop assigned to it.\n\r", ch);
        	return false;
		}

		pMob->pShop		= new_shop();
		if (!shop_first)
				shop_first	= pMob->pShop;
		if (shop_last)
			shop_last->next	= pMob->pShop;
		shop_last		= pMob->pShop;

		pMob->pShop->keeper	= pMob->vnum;

		send_to_char("New shop assigned to mobile.\n\r", ch);
		return true;
    }

    if (!str_prefix(command, "remove"))
    {
		SHOP_DATA *pShop;

		pShop		= pMob->pShop;
		pMob->pShop	= NULL;

		if (pShop == shop_first)
		{
			if (!pShop->next)
			{
				shop_first = NULL;
				shop_last = NULL;
			}
			else
				shop_first = pShop->next;
		}
		else
		{
			SHOP_DATA *ipShop;

			for (ipShop = shop_first; ipShop; ipShop = ipShop->next)
			{
				if (ipShop->next == pShop)
				{
					if (!pShop->next)
					{
						shop_last = ipShop;
						shop_last->next = NULL;
					}
					else
						ipShop->next = pShop->next;
				}
			}
		}

		free_shop(pShop);

		send_to_char("Mobile is no longer a shopkeeper.\n\r", ch);
		return true;
    }

    if(!str_prefix(command, "flags"))
    {
		int value;
		if (flag_start[0] != '\0')
		{

			if ((value = flag_value(shop_flags, flag_start)) != NO_FLAG)
			{
				pMob->pShop->flags ^= value;

				send_to_char("Shop flags toggled.\n\r", ch);
				return true;
			}
		}

		send_to_char(	"Syntax: shop flags [flag]\n\r"
						"Type '? shop' for a list of flags.\n\r", ch);

		return false;
	}

	if(!str_prefix(command, "stock"))
	{
		SHOP_STOCK_DATA *stock;

		if (!pMob->pShop)
		{
			send_to_char("MEdit:  Please create a shop first (shop assign).\n\r", ch);
			return false;
		}

		if(arg1[0] == '\0')
		{
			send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
			send_to_char("         shop stock add pet [vnum]\n\r", ch);
			send_to_char("         shop stock add mount [vnum]\n\r", ch);
			send_to_char("         shop stock add guard [vnum]\n\r", ch);
			send_to_char("         shop stock add crew [vnum]\n\r", ch);
			send_to_char("         shop stock add ship [vnum]\n\r", ch);
			send_to_char("         shop stock add custom [keyword]\n\r", ch);
			send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
			send_to_char("         shop stock [#] description [description]\n\r", ch);
			send_to_char("         shop stock [#] level [level]\n\r", ch);
			send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
			send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
			send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
			send_to_char("         shop stock [#] singular\n\r", ch);
			send_to_char("         shop stock [#] remove\n\r", ch);
			return false;
		}

		if(!str_prefix(arg1, "add"))
		{
			if(arg2[0] == '\0' || argument[0] == '\0')
			{
				send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
				send_to_char("         shop stock add pet [vnum]\n\r", ch);
				send_to_char("         shop stock add mount [vnum]\n\r", ch);
				send_to_char("         shop stock add guard [vnum]\n\r", ch);
				send_to_char("         shop stock add crew [vnum]\n\r", ch);
				send_to_char("         shop stock add ship [vnum]\n\r", ch);
				send_to_char("         shop stock add custom [keyword]\n\r", ch);
				return false;
			}

			if(!str_prefix(arg2, "object"))
			{
				if(is_number(argument))
				{
					OBJ_INDEX_DATA *item = get_obj_index(atoi(argument));

					if(!item)
					{
						send_to_char("Object does not exist.\n\r", ch);
						return false;
					}

					if(item->item_type == ITEM_MONEY)
					{
						send_to_char("You cannot sell money.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_OBJECT;
					stock->vnum = item->vnum;
					stock->silver = item->cost;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (OBJECT) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
				return false;
			}
			else if(!str_prefix(arg2, "pet"))
			{
				if(is_number(argument))
				{
					MOB_INDEX_DATA *mob = get_mob_index(atoi(argument));

					if(!mob)
					{
						send_to_char("Mobile does not exist.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_PET;
					stock->vnum = mob->vnum;
					stock->silver = 10 * mob->level * mob->level;
					stock->level = mob->level;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (PET) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add pet [vnum]\n\r", ch);
				return false;
			}
			else if(!str_prefix(arg2, "mount"))
			{
				if(is_number(argument))
				{
					MOB_INDEX_DATA *mob = get_mob_index(atoi(argument));

					if(!mob)
					{
						send_to_char("Mobile does not exist.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_MOUNT;
					stock->vnum = mob->vnum;
					stock->silver = 25 * mob->level * mob->level;
					stock->level = mob->level;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (MOUNT) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add mount [vnum]\n\r", ch);
				return false;
			}
			else if(!str_prefix(arg2, "guard"))
			{
				if(is_number(argument))
				{
					MOB_INDEX_DATA *mob = get_mob_index(atoi(argument));

					if(!mob)
					{
						send_to_char("Mobile does not exist.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_GUARD;
					stock->vnum = mob->vnum;
					stock->silver = 50 * mob->level * mob->level;
					stock->level = mob->level;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (GUARD) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add guard [vnum]\n\r", ch);
				return false;
			}
			else if(!str_prefix(arg2, "crew"))
			{
				if(is_number(argument))
				{
					MOB_INDEX_DATA *mob = get_mob_index(atoi(argument));

					if(!mob)
					{
						send_to_char("Mobile does not exist.\n\r", ch);
						return false;
					}

					if(!mob->pCrew)
					{
						send_to_char("Mobile has no Crew definition.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_CREW;
					stock->vnum = mob->vnum;
					stock->silver = 50 * mob->level * mob->level;
					stock->level = mob->level;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (CREW) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add crew [vnum]\n\r", ch);
				return false;
			}
			else if(!str_prefix(arg2, "ship"))
			{
				if( !is_shipyard_valid(pMob->pShop->shipyard,
					pMob->pShop->shipyard_region[0][0],
					pMob->pShop->shipyard_region[0][1],
					pMob->pShop->shipyard_region[1][0],
					pMob->pShop->shipyard_region[1][1]) )
				{
					send_to_char("Shopkeeper needs to have a valid shipyard defined first before you can add a ship.\n\r", ch);
					return false;
				}

				if( is_number(argument) )
				{
					long vnum = atol(argument);
					SHIP_INDEX_DATA *ship;

					if( !(ship = get_ship_index(vnum)) )
					{
						send_to_char("That ship does not exist.\n\r", ch);
						return false;
					}

					if( !IS_VALID(ship->blueprint) || !get_obj_index(ship->ship_object) )
					{
						send_to_char("Ship is incomplete.  Cannot be sold yet.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_SHIP;
					stock->vnum = vnum;
					stock->silver = 100000;	// Default 1000gold
					stock->level = 1;
					stock->discount = pMob->pShop->discount;

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (SHIP) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add ship [vnum]\n\r", ch);
				return false;

			}
			else if(!str_prefix(arg2, "custom"))
			{
				if(!IS_NULLSTR(argument))
				{
					for(stock = pMob->pShop->stock; stock; stock = stock->next)
					{
						if( (stock->type == STOCK_CUSTOM) &&
							!str_cmp(argument, stock->custom_keyword) )
						{
							break;
						}
					}

					if( stock != NULL )
					{
						send_to_char("Keyword already used.\n\r", ch);
						return false;
					}

					stock = new_shop_stock();

					if(!stock)
					{
						send_to_char("{RERROR{W: Unable to create stock item.{x\n\r", ch);
						return false;
					}

					stock->type = STOCK_CUSTOM;
					stock->custom_keyword = str_dup(argument);
					stock->discount = 0;		// They do not handle discounts.
												// If you wish to do discounts, that has to be scripted.

					stock->next = pMob->pShop->stock;
					pMob->pShop->stock = stock;

					send_to_char("Stock item (CUSTOM) added.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock add custom [keyword]\n\r", ch);
				return false;
			}

			send_to_char("Syntax:  shop stock add object [vnum]\n\r", ch);
			send_to_char("         shop stock add pet [vnum]\n\r", ch);
			send_to_char("         shop stock add mount [vnum]\n\r", ch);
			send_to_char("         shop stock add guard [vnum]\n\r", ch);
			send_to_char("         shop stock add custom [keyword]\n\r", ch);
			return false;
		}

		if(is_number(arg1))
		{
			int idx = atoi(arg1);
			stock = get_shop_stock_bypos(pMob->pShop, idx);

			if(!stock)
			{
				send_to_char("Invalid stock number.\n\r", ch);
				return false;
			}

			if(!str_prefix(arg2, "price"))
			{
				char arg3[MIL];

				argument = one_argument(argument, arg3);

				if(!str_prefix(arg3, "silver"))
				{
					if(!is_number(argument))
					{
						send_to_char("Silver price must be a number.\n\r", ch);
						return false;
					}

					int silver = atoi(argument);

					stock->silver = UMAX(silver, 0);
					if( !IS_NULLSTR(stock->custom_price) )
					{
						stock->discount = pMob->pShop->discount;
						free_string(stock->custom_price);
						stock->custom_price = &str_empty[0];
					}
					send_to_char("Stock silver price changed.\n\r", ch);
					return true;
				}

				if(!str_prefix(arg3, "qp"))
				{
					if(!is_number(argument))
					{
						send_to_char("Quest point price must be a number.\n\r", ch);
						return false;
					}

					int qp = atoi(argument);

					stock->qp = UMAX(qp, 0);
					if( !IS_NULLSTR(stock->custom_price) )
					{
						stock->discount = pMob->pShop->discount;
						free_string(stock->custom_price);
						stock->custom_price = &str_empty[0];
					}
					send_to_char("Stock quest point price changed.\n\r", ch);
					return true;
				}

				if(!str_prefix(arg3, "dp"))
				{
					if(!is_number(argument))
					{
						send_to_char("Deity point price must be a number.\n\r", ch);
						return false;
					}

					int dp = atoi(argument);

					stock->dp = UMAX(dp, 0);
					if( !IS_NULLSTR(stock->custom_price) )
					{
						stock->discount = pMob->pShop->discount;
						free_string(stock->custom_price);
						stock->custom_price = &str_empty[0];
					}
					send_to_char("Stock deity point price changed.\n\r", ch);
					return true;
				}

				if(!str_prefix(arg3, "pneuma"))
				{
					if(!is_number(argument))
					{
						send_to_char("Pneuma price must be a number.\n\r", ch);
						return false;
					}

					int pneuma = atoi(argument);

					stock->pneuma = UMAX(pneuma, 0);
					if( !IS_NULLSTR(stock->custom_price) )
					{
						stock->discount = pMob->pShop->discount;
						free_string(stock->custom_price);
						stock->custom_price = &str_empty[0];
					}
					send_to_char("Stock pneuma price changed.\n\r", ch);
					return true;
				}

				if(!str_prefix(arg3, "custom"))
				{
					if(argument[0] == '\0')
					{
						send_to_char("Please specify a custom price string.\n\r", ch);
						send_to_char("Syntax:  shop stock [#] price custom [value]\n\r\n\r", ch);
						send_to_char("If you wish to clear the custom pricing, select a different pricing type.\n\r", ch);
						return false;
					}

					stock->silver = 0;
					stock->qp = 0;
					stock->dp = 0;
					stock->pneuma = 0;
					stock->discount = 0;
					free_string(stock->custom_price);
					stock->custom_price = str_dup(argument);
					send_to_char("Stock custom price changed.\n\r", ch);
					return true;
				}

				send_to_char("Syntax:  shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
				return false;
			}

			if(!str_prefix(arg2, "discount"))
			{
				if( !IS_NULLSTR(stock->custom_price) )
				{
					send_to_char("Stock items with custom pricing do not receive discounts.\n\r", ch);
					send_to_char("Those need to be handled in the CUSTOM_PRICE trigger.\n\r", ch);
					return false;
				}

				if(!is_number(argument))
				{
					send_to_char("Syntax:  shop stock [#] discount [0-100]\n\r", ch);
					return false;
				}

				int disc = atoi(argument);

				if(disc < 0 || disc > 100)
				{
					send_to_char("Discount must be a percentage (0-100).\n\r", ch);
					return false;
				}

				stock->discount = disc;
				send_to_char("Stock discount changed.\n\r", ch);
				return true;
			}

			if(!str_prefix(arg2, "level"))
			{
				if(!is_number(argument))
				{
					send_to_char("Syntax:  shop stock [#] level [level]\n\r", ch);
					return false;
				}

				int lvl = atoi(argument);

				if(lvl < 1)
				{
					stock->level = 0;
					send_to_char("Stock level set to automatic.\n\r", ch);
					return true;
				}

				stock->level = lvl;
				send_to_char("Stock level changed.\n\r", ch);
				return true;
			}

			if(!str_prefix(arg2, "singular"))
			{
				stock->singular = !stock->singular;
				if(stock->singular)
					send_to_char("Stock is now singular.\n\r", ch);
				else
					send_to_char("Stock is no longer singular.\n\r", ch);
				return true;
			}


			if(!str_prefix(arg2, "quantity"))
			{
				if(!str_prefix(argument, "unlimited"))
				{
					stock->quantity = 0;
					stock->restock_rate = 0;
					send_to_char("Stock quantity settings changed.\n\r", ch);
					return true;
				}

				char arg3[MIL];
				argument = one_argument(argument, arg3);
				if(!is_number(arg3) || !is_number(argument))
				{
					send_to_char("Syntax:  shop stock [#] quantity [total] [reset rate]\n\r", ch);
					return false;
				}

				int total = atoi(arg3);
				int rate = atoi(argument);

				if(total < 1)
				{
					send_to_char("Please specify a positive number for limited quantity.\n\r", ch);
					return false;
				}

				stock->quantity = total;
				stock->restock_rate = UMAX(rate, 0);		// A rate of zero means it never restock
				send_to_char("Stock quantity settings changed.\n\r", ch);
				return true;
			}

			if(!str_prefix(arg2, "description"))
			{
				free_string(stock->custom_descr);
				stock->custom_descr = str_dup(argument);

				send_to_char("Stock description changed.\n\r", ch);
				return true;
			}

			if(!str_prefix(arg2, "remove"))
			{
				if( idx < 1 )
				{
					send_to_char("Please specify a positive number.\n\r", ch);
					return false;
				}

				SHOP_STOCK_DATA *prev = NULL;
				for(stock = pMob->pShop->stock;stock;prev = stock, stock = stock->next)
				{
					if(!--idx)
						break;
				}

				if( !stock )
				{
					send_to_char("Invalid stock number.\n\r", ch);
					return false;
				}

				if( prev != NULL )
				{
					prev->next = stock->next;
				}
				else
				{
					pMob->pShop->stock = stock->next;
				}

				free_shop_stock(stock);
				send_to_char("Stock item removed.\n\r", ch);
				return true;
			}

			send_to_char("Syntax:  shop stock [#] description [description]\n\r", ch);
			send_to_char("         shop stock [#] discount [0-100]\n\r", ch);
			send_to_char("         shop stock [#] level [level]\n\r", ch);
			send_to_char("         shop stock [#] price [silver|qp|dp|pneuma|custom] [value]\n\r", ch);
			send_to_char("         shop stock [#] quantity unlimited\n\r", ch);
			send_to_char("         shop stock [#] quantity [total] [reset rate]\n\r", ch);
			send_to_char("         shop stock [#] singular\n\r", ch);
			send_to_char("         shop stock [#] remove\n\r", ch);
			return false;
		}

		medit_shop(ch, "stock");
		return false;
	}

    medit_shop(ch, "");
    return false;
}


MEDIT(medit_sex)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(sex_flags, argument)) != NO_FLAG)
	{
	    pMob->sex = value;

	    send_to_char("Sex set.\n\r", ch);
	    return true;
	}
	else
	if (!str_cmp(argument, "neutral")) // hack
	{
	    pMob->sex = SEX_NEUTRAL;
	    send_to_char("Sex set.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: sex [sex]\n\r"
		  "Type '? sex' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_act)
{
    MOB_INDEX_DATA *pMob;
    //long value;

    if (argument[0] != '\0')
    {
			EDIT_MOB(ch, pMob);

		long bits[2];
		if (bitvector_lookup(argument, 2, bits, act_flags, act2_flags))
		{
			TOGGLE_BIT(pMob->act[0], bits[0]);
			TOGGLE_BIT(pMob->act[1], bits[1]);
			SET_BIT(pMob->act[0], ACT_IS_NPC);	// Force on, all the time

			send_to_char("Act flag toggled.\n\r", ch);
			return true;
	}
    }

    send_to_char("Syntax: act [flag]\n\r"
		  "Type '? act' for a list of flags.\n\r", ch);
    return false;
}

/*
MEDIT(medit_act2)
{
    MOB_INDEX_DATA *pMob;
    long value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(act2_flags, argument)) != NO_FLAG)
	{
	    pMob->act2 ^= value;
	    SET_BIT(pMob->act, ACT_IS_NPC);

	    send_to_char("Act2 flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: act2 [flag]\n\r"
		  "Type '? act2' for a list of flags.\n\r", ch);
    return false;
}
*/

MEDIT(medit_affect)
{
    MOB_INDEX_DATA *pMob;
    //int value;

    if (argument[0] != '\0')
    {
		EDIT_MOB(ch, pMob);
		long bits[2];

		if (bitvector_lookup(argument, 2, bits, affect_flags, affect2_flags))
		{
			TOGGLE_BIT(pMob->affected_by[0], bits[0]);
			TOGGLE_BIT(pMob->affected_by[1], bits[1]);

			send_to_char("Affect flag toggled.\n\r", ch);
			return true;
	}
    }

    send_to_char("Syntax: affect [flag]\n\r"
		  "Type '? affect' for a list of flags.\n\r", ch);
    return false;
}

/*
MEDIT(medit_affect2)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(affect2_flags, argument)) != NO_FLAG)
	{
	    pMob->affected_by2 ^= value;

	    send_to_char("Affect2 flag toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: affect2 [flag]\n\r"
		  "Type '? affect2' for a list of flags.\n\r", ch);
    return false;
}
*/

MEDIT(medit_ac)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];
    int pierce, bash, slash, exotic;

    do   /* So that I can use break and send the syntax in one place */
    {
	if (argument[0] == '\0')  break;

	EDIT_MOB(ch, pMob);
	argument = one_argument(argument, arg);

	if (!is_number(arg))  break;
	pierce = atoi(arg);
	argument = one_argument(argument, arg);

	if (arg[0] != '\0')
	{
	    if (!is_number(arg))  break;
	    bash = atoi(arg);
	    argument = one_argument(argument, arg);
	}
	else
	    bash = pMob->ac[AC_BASH];

	if (arg[0] != '\0')
	{
	    if (!is_number(arg))  break;
	    slash = atoi(arg);
	    argument = one_argument(argument, arg);
	}
	else
	    slash = pMob->ac[AC_SLASH];

	if (arg[0] != '\0')
	{
	    if (!is_number(arg))  break;
	    exotic = atoi(arg);
	}
	else
	    exotic = pMob->ac[AC_EXOTIC];

	pMob->ac[AC_PIERCE] = pierce;
	pMob->ac[AC_BASH]   = bash;
	pMob->ac[AC_SLASH]  = slash;
	pMob->ac[AC_EXOTIC] = exotic;

	send_to_char("Ac set.\n\r", ch);
	return true;
    } while (false);    /* Just do it once.. */

    send_to_char("Syntax:  ac [ac-pierce [ac-bash [ac-slash [ac-exotic]]]]\n\r"
		  "help MOB_AC  gives a list of reasonable ac-values.\n\r", ch);
    return false;
}


MEDIT(medit_form)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(form_flags, argument)) != NO_FLAG)
	{
	    pMob->form ^= value;
	    send_to_char("Form toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: form [flags]\n\r"
		  "Type '? form' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_part)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(part_flags, argument)) != NO_FLAG)
	{
	    pMob->parts ^= value;
	    send_to_char("Parts toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: part [flags]\n\r"
		  "Type '? part' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_immune)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(imm_flags, argument)) != NO_FLAG)
	{
	    pMob->imm_flags ^= value;
	    send_to_char("Immunity toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: imm [flags]\n\r"
		  "Type '? imm' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_res)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(res_flags, argument)) != NO_FLAG)
	{
	    pMob->res_flags ^= value;
	    send_to_char("Resistance toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: res [flags]\n\r"
		  "Type '? res' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_vuln)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(vuln_flags, argument)) != NO_FLAG)
	{
	    pMob->vuln_flags ^= value;
	    send_to_char("Vulnerability toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: vuln [flags]\n\r"
		  "Type '? vuln' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_material)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  material [string]\n\r", ch);
	return false;
    }

    free_string(pMob->material);
    pMob->material = str_dup(argument);

    send_to_char("Material set.\n\r", ch);
    return true;
}


MEDIT(medit_off)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(off_flags, argument)) != NO_FLAG)
	{
	    pMob->off_flags ^= value;
	    send_to_char("Offensive behaviour toggled.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: off [flags]\n\r"
		  "Type '? off' for a list of flags.\n\r", ch);
    return false;
}


MEDIT(medit_size)
{
    MOB_INDEX_DATA *pMob;
    int value;

    if (argument[0] != '\0')
    {
	EDIT_MOB(ch, pMob);

	if ((value = flag_value(size_flags, argument)) != NO_FLAG)
	{
	    pMob->size = value;
	    send_to_char("Size set.\n\r", ch);
	    return true;
	}
    }

    send_to_char("Syntax: size [size]\n\r"
		  "Type '? size' for a list of sizes.\n\r", ch);
    return false;
}


MEDIT(medit_hitdice)
{
    static char syntax[] = "Syntax:  hitdice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (ch->tot_level < 151)
    {
	send_to_char("You do not have permission to edit hit dice.\n\r", ch);
	return false;
    }

    if (argument[0] == '\0')
    {
	send_to_char(syntax, ch);
	return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
	send_to_char(syntax, ch);
	return false;
    }

    pMob->hit.number = atoi(num  );
    pMob->hit.size   = atoi(type );
    pMob->hit.bonus  = atoi(bonus);

    send_to_char("Hitdice set.\n\r", ch);
    return true;
}


MEDIT(medit_manadice)
{
    static char syntax[] = "Syntax:  manadice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char(syntax, ch);
	return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if (!(is_number(num) && is_number(type) && is_number(bonus)))
    {
	send_to_char(syntax, ch);
	return false;
    }

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
	send_to_char(syntax, ch);
	return false;
    }

    pMob->mana.number = atoi(num  );
    pMob->mana.size   = atoi(type );
    pMob->mana.bonus  = atoi(bonus);

    send_to_char("Manadice set.\n\r", ch);
    return true;
}


MEDIT(medit_damdice)
{
    static char syntax[] = "Syntax:  damdice <number> d <type> + <bonus>\n\r";
    char *num, *type, *bonus, *cp;
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0')
    {
	send_to_char(syntax, ch);
	return false;
    }

    num = cp = argument;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp))  *(cp++) = '\0';

    type = cp;

    while (ISDIGIT(*cp)) ++cp;
    while (*cp != '\0' && !ISDIGIT(*cp)) *(cp++) = '\0';

    bonus = cp;

    while (ISDIGIT(*cp)) ++cp;
    if (*cp != '\0') *cp = '\0';

    if (!(is_number(num) && is_number(type) && is_number(bonus)))
    {
	send_to_char(syntax, ch);
	return false;
    }

    if ((!is_number(num  ) || atoi(num  ) < 1)
    ||   (!is_number(type ) || atoi(type ) < 1)
    ||   (!is_number(bonus) || atoi(bonus) < 0))
    {
	send_to_char(syntax, ch);
	return false;
    }

    pMob->damage.number = atoi(num  );
    pMob->damage.size   = atoi(type );
    pMob->damage.bonus  = atoi(bonus);

    send_to_char("Damdice set.\n\r", ch);
    return true;
}


MEDIT(medit_race)
{
    MOB_INDEX_DATA *pMob;
    int race;

    if (argument[0] != '\0'
    && (race = race_lookup(argument)) != 0)
    {
	EDIT_MOB(ch, pMob);

	pMob->race = race;
	pMob->act[0]	  |= race_table[race].act;
	pMob->act[1]	  |= race_table[race].act2;
	pMob->affected_by[0] |= race_table[race].aff;
	pMob->off_flags   |= race_table[race].off;
	pMob->imm_flags   |= race_table[race].imm;
	pMob->res_flags   |= race_table[race].res;
	pMob->vuln_flags  |= race_table[race].vuln;
	pMob->form        |= race_table[race].form;
	pMob->parts       |= race_table[race].parts;

	send_to_char("Race set.\n\r", ch);
	return true;
    }

    if (argument[0] == '?')
    {
	char buf[MAX_STRING_LENGTH];

	send_to_char("Available races are:", ch);

	for (race = 0; race_table[race].name != NULL; race++)
	{
	    if ((race % 3) == 0)
		send_to_char("\n\r", ch);
	    sprintf(buf, " %-15s", race_table[race].name);
	    send_to_char(buf, ch);
	}

	send_to_char("\n\r", ch);
	return false;
    }

    send_to_char("Syntax:  race [race]\n\r"
		  "Type 'race ?' for a list of races.\n\r", ch);
    return false;
}


MEDIT(medit_position)
{
    MOB_INDEX_DATA *pMob;
    char arg[MAX_INPUT_LENGTH];
    int value;

    argument = one_argument(argument, arg);

    switch (arg[0])
    {
    default:
	break;

    case 'S':
    case 's':
	if (str_prefix(arg, "start"))
	    break;

	if ((value = flag_value(position_flags, argument)) == NO_FLAG)
	    break;

	EDIT_MOB(ch, pMob);

	pMob->start_pos = value;
	send_to_char("Start position set.\n\r", ch);
	return true;

    case 'D':
    case 'd':
	if (str_prefix(arg, "default"))
	    break;

	if ((value = flag_value(position_flags, argument)) == NO_FLAG)
	    break;

	EDIT_MOB(ch, pMob);

	pMob->default_pos = value;
	send_to_char("Default position set.\n\r", ch);
	return true;
    }

    send_to_char("Syntax:  position [start/default] [position]\n\r"
		  "Type '? position' for a list of positions.\n\r", ch);
    return false;
}


MEDIT(medit_movedice)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  move [number]\n\r", ch);
	return false;
    }

    pMob->move = atoi(argument);

    send_to_char("Movement set.\n\r", ch);
    return true;
}


MEDIT(medit_gold)
{
    MOB_INDEX_DATA *pMob;
    long value;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  wealth [number]\n\r", ch);
	return false;
    }

    value = atol(argument);

    if (value > 1000 && !has_imp_sig(pMob, NULL))
    {
	send_to_char("Sorry, that's too much. Have an IMP sign this mob if you want to set that much gold.\n\r", ch);
	return false;
    }

    pMob->wealth = value;
    use_imp_sig(pMob, NULL);

    send_to_char("Wealth set.\n\r", ch);
    return true;
}


MEDIT(medit_hitroll)
{
    MOB_INDEX_DATA *pMob;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  hitroll [number]\n\r", ch);
	return false;
    }

    pMob->hitroll = atoi(argument);

    send_to_char("Hitroll set.\n\r", ch);
    return true;
}


void show_liqlist(CHAR_DATA *ch)
{
    int liq;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];

    buffer = new_buf();

    for (liq = 0; liq_table[liq].liq_name != NULL; liq++)
    {
	if ((liq % 21) == 0)
	    add_buf(buffer,"Name                 Colour          Proof Full Thirst Food Ssize\n\r");

	sprintf(buf, "%-20s %-14s %5d %4d %6d %4d %5d\n\r",
		liq_table[liq].liq_name,liq_table[liq].liq_colour,
		liq_table[liq].liq_affect[0],liq_table[liq].liq_affect[1],
		liq_table[liq].liq_affect[2],liq_table[liq].liq_affect[3],
		liq_table[liq].liq_affect[4]);
	add_buf(buffer,buf);
    }

    page_to_char(buf_string(buffer),ch);
    free_buf(buffer);

    return;
}


void show_damlist(CHAR_DATA *ch)
{
    int att;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];

    buffer = new_buf();

    for (att = 0; attack_table[att].name != NULL; att++)
    {
	if ((att % 21) == 0)
	    add_buf(buffer,"Name                 Noun\n\r");

	sprintf(buf, "%-20s %-20s\n\r",
		attack_table[att].name,attack_table[att].noun);
	add_buf(buffer,buf);
    }

    page_to_char(buf_string(buffer),ch);
    free_buf(buffer);

    return;
}


REDIT(redit_owner)
{
    ROOM_INDEX_DATA *pRoom;

    EDIT_ROOM(ch, pRoom);

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  owner [owner]\n\r", ch);
	send_to_char("         owner none\n\r", ch);
	return false;
    }

    free_string(pRoom->owner);
    if (!str_cmp(argument, "none"))
    	pRoom->owner = str_dup("");
    else
	pRoom->owner = str_dup(argument);

    send_to_char("Owner set.\n\r", ch);
    return true;
}


MEDIT (medit_addmprog)
{
    int tindex, value, slot;
    MOB_INDEX_DATA *pMob;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_MOB(ch, pMob);
    argument = one_argument(argument, num);
    argument = one_argument(argument, trigger);
    argument = one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addmprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_MPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "mprog");
	return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

	if(value == TRIG_SPELLCAST) {
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "0");
		}
		else
		{
			int sn = skill_lookup(phrase);
			if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
				send_to_char("Invalid spell for trigger.\n\r",ch);
				return false;
			}
			sprintf(phrase,"%d",sn);
		}
	}
	else if( value == TRIG_EXIT || value == TRIG_EXALL )
	{
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "-1");
		}
		else
		{
			int door = parse_door(phrase);
			if( door < 0 ) {
				send_to_char("Invalid direction for exit/exall trigger.\n\r", ch);
				return false;
			}
			sprintf(phrase,"%d",door);
		}
	}

    if ((code = get_script_index (atol(num), PRG_MPROG)) == NULL)
    {
	send_to_char("No such MOBProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!pMob->progs) pMob->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);

    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pMob->progs[slot], list);

    send_to_char("Mprog Added.\n\r",ch);
    return true;
}


MEDIT (medit_delmprog)
{
    MOB_INDEX_DATA *pMob;
    char mprog[MAX_STRING_LENGTH];
    int value;

    EDIT_MOB(ch, pMob);

    one_argument(argument, mprog);
    if (!is_number(mprog) || mprog[0] == '\0')
    {
       send_to_char("Syntax:  delmprog [#mprog]\n\r",ch);
       return false;
    }

    value = atol (mprog);

    if (value < 0)
    {
        send_to_char("Only non-negative mprog-numbers allowed.\n\r",ch);
        return false;
    }

    if(!edit_deltrigger(pMob->progs,value)) {
	send_to_char("No such mprog.\n\r",ch);
	return false;
    }

    send_to_char("Mprog removed.\n\r", ch);
    return true;
}


MEDIT(medit_addquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_LIST *quest;
    QUEST_INDEX_DATA *pQuestIndex;
    long value;

    EDIT_MOB(ch, pMob);

    value = atol(argument);

    if (argument[0] == '\0' || value <= 0)
    {
	send_to_char("Syntax:  addquest [quest vnum]\n\r", ch);
	return false;
    }

    pQuestIndex = get_quest_index(value);
    if (pQuestIndex == NULL)
    {
	send_to_char("That quest vnum doesn't exist.\n\r", ch);
	return false;
    }

    for (quest = pMob->quests; quest != NULL; quest = quest->next)
    {
	if (quest->vnum == value)
	{
	    send_to_char("That would be redundant as you've already added that quest.\n\r", ch);
	    return false;
	}
    }

    quest = new_quest_list();
    quest->vnum = pQuestIndex->vnum;

    quest->next = pMob->quests;
    pMob->quests = quest;

    send_to_char("Quest added.\n\r", ch);

    return true;
}


MEDIT(medit_delquest)
{
    MOB_INDEX_DATA *pMob;
    QUEST_LIST *quest_list;
    QUEST_LIST *prev_quest_list = NULL;
    int i;
    int counter;

    EDIT_MOB(ch, pMob);

    if (argument[0] == '\0' || !is_number(argument))
    {
	send_to_char("Syntax:  delquest [#]\n\r", ch);
	return false;
    }

    i = atoi (argument);
    counter = 0;
    for (quest_list = pMob->quests; quest_list != NULL;
	  quest_list = quest_list->next)
    {
	if (i == counter)
	    break;

	counter++;
	prev_quest_list = quest_list;
    }

    if (quest_list == NULL)
    {
	send_to_char("Number not found.\n\r", ch);
	return false;
    }

    if (prev_quest_list != NULL)
	prev_quest_list->next = quest_list->next;
    else
	pMob->quests = quest_list->next;

    free_quest_list(quest_list);
    send_to_char("Quest removed.\n\r", ch);
    return true;
}

REDIT(redit_room)
{
    ROOM_INDEX_DATA *room;
    //long value;

		EDIT_ROOM(ch, room);

		long bits[2];
		if (!bitmatrix_lookup(argument, room_flagbank, bits))
		{
			send_to_char("Syntax:  room <flags>\n\r", ch);
			send_to_char("Type '? room' for list of flags.\n\r", ch);
			return false;
		}
		

		if( IS_SET(bits[1], ROOM_BLUEPRINT) )
    	{
			// Only those that can edit blueprints can toggle this flag
			if( !can_edit_blueprints(ch) )
			{
				bits[1] &= ~ROOM_BLUEPRINT;

				if( !bits[1] )
				{
					send_to_char("Syntax: room [flags]\n\r", ch);
					return false;
				}
			}
			else if( !IS_SET(bits[1], ROOM_NOCLONE) && IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
			{
				send_to_char("No-clone room cannot be used in blueprints.\n\r", ch);
				return false;
			}
			else if( IS_SET(bits[1], ROOM_NOCLONE) && !IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
			{
				send_to_char("BLUEPRINT and NO_CLONE cannot mix.\n\r", ch);
				return false;
			}
		}

		if( IS_SET(bits[1], ROOM_NOCLONE) )
		{
			if( !IS_SET(bits[1], ROOM_BLUEPRINT) && IS_SET(room->rs_room_flag[1], ROOM_BLUEPRINT) )
			{
				send_to_char("Blueprint rooms cannot be no-clone.\n\r", ch);
				return false;
			}

			// Check if room is already used in a section
			if( get_blueprint_section_byroom(room->vnum) )
			{
				send_to_char("Room is currently used in a blueprint.\n\r", ch);
				// Clear it out, JIC
				if( IS_SET(room->rs_room_flag[1], ROOM_NOCLONE) )
				{
		    		REMOVE_BIT(room->rs_room_flag[1], ROOM_NOCLONE);
		    		return true;
				}

				return false;
			}
		}

		for(int i = 0; i < 2; i++)
   			TOGGLE_BIT(room->rs_room_flag[i], bits[i]);

		send_to_char("Room flag(s) toggled.\n\r", ch);
		return true;
	
}

/*
REDIT(redit_room2)
{
    ROOM_INDEX_DATA *room;
    int value;

    EDIT_ROOM(ch, room);

    if ((value = flag_value(room2_flags, argument)) == NO_FLAG)
    {
		send_to_char("Syntax: room2 [flags]\n\r", ch);
		return false;
    }

    if( IS_SET(value, ROOM_BLUEPRINT) )
    {
		// Only those that can edit blueprints can toggle this flag
		if( !can_edit_blueprints(ch) )
		{
			value &= ~ROOM_BLUEPRINT;

			if( !value )
			{
				send_to_char("Syntax: room2 [flags]\n\r", ch);
				return false;
			}
		}
		else if( !IS_SET(value, ROOM_NOCLONE) && IS_SET(room->room2_flags, ROOM_NOCLONE) )
		{
			send_to_char("No-clone room cannot be used in blueprints.\n\r", ch);
			return false;
		}
		else if( IS_SET(value, ROOM_NOCLONE) && !IS_SET(room->room2_flags, ROOM_NOCLONE) )
		{
			send_to_char("BLUEPRINT and NO_CLONE cannot mix.\n\r", ch);
			return false;
		}
	}

	if( IS_SET(value, ROOM_NOCLONE) )
	{
		if( !IS_SET(value, ROOM_BLUEPRINT) && IS_SET(room->room2_flags, ROOM_BLUEPRINT) )
		{
			send_to_char("Blueprint rooms cannot be no-clone.\n\r", ch);
			return false;
		}

		// Check if room is already used in a section
		if( get_blueprint_section_byroom(room->vnum) )
		{
			send_to_char("Room is currently used in a blueprint.\n\r", ch);
			// Clear it out, JIC
			if( IS_SET(room->room2_flags, ROOM_NOCLONE) )
			{
			    REMOVE_BIT(room->room2_flags, ROOM_NOCLONE);
			    return true;
			}

			return false;
		}
	}

    TOGGLE_BIT(room->room2_flags, value);
    send_to_char("Room flags toggled.\n\r", ch);
    return true;
}
*/

REDIT(redit_sector)
{
    ROOM_INDEX_DATA *room;
    int value;

    EDIT_ROOM(ch, room);

    // Another hack because the SECT_INSIDE is 0 or the same as FLAG_NONE
    if (!str_cmp(argument, "inside"))
	value = 0;
    else
    if ((value = flag_value(sector_flags, argument)) == NO_FLAG)
    {
	send_to_char("Syntax: sector [type]\n\r", ch);
	return false;
    }

    room->rs_sector_type = value;
    send_to_char("Sector type set.\n\r", ch);

    return true;
}


REDIT(redit_coords)
{
	char arg1[MIL];
	char arg2[MIL];
	char arg3[MIL];
    ROOM_INDEX_DATA *room;
    WILDS_DATA *w;
    int x,y,z;

    EDIT_ROOM(ch, room);

    if(room->wilds) {
	    send_to_char("Wilderness rooms cannot be modified.\n\r",ch);
	    return false;
    }

    if(IS_NULLSTR(argument)) {
	    send_to_char("coords <x> <y> <z>[ <wilds uid>]\n\r",ch);
	    send_to_char("<wilds uid> can be omitted if dealing with a blueprint room.\n\r", ch);
	    return false;
    }

	if(!str_cmp(argument,"none")) {
		room->viewwilds = NULL;
		room->x = 0;
		room->y = 0;
		room->z = 0;
	} else {
		argument = one_argument(argument,arg1);
		argument = one_argument(argument,arg2);
		argument = one_argument(argument,arg3);


		x = atoi(arg1);
		y = atoi(arg2);
		z = atoi(arg3);

		if( !IS_SET(room->room_flag[1], ROOM_BLUEPRINT) )
		{
			w = get_wilds_from_uid(NULL,atoi(argument));
			if(!w) {
				send_to_char("No such wilderness.\n\r",ch);
				return false;
			}

			if(x < 0 || x >= w->map_size_x) {
				send_to_char("Invalid map coordinate.\n\r",ch);
				return false;
			}
			if(y < 0 || y >= w->map_size_y) {
				send_to_char("Invalid map coordinate.\n\r",ch);
				return false;
			}

			room->viewwilds = w;
		}
		else
			room->viewwilds = NULL;

		room->x = x;
		room->y = y;
		room->z = z;

		send_to_char("Coordinate set.\n\r", ch);
	}

	return true;
}

REDIT(redit_locale)
{
    ROOM_INDEX_DATA *room;
    int locale;

    EDIT_ROOM(ch, room);

    if(IS_NULLSTR(argument) || !is_number(argument)) {
	    send_to_char("locale <#locale>\n\r",ch);
	    return false;
    }

    locale = atoi(argument);

    room->locale = locale;

    send_to_char("Locale set.\n\r", ch);

    return true;
}


OEDIT (oedit_addoprog)
{
    int tindex, value, slot;
    PROG_LIST *list;
    SCRIPT_DATA *code;
  OBJ_INDEX_DATA *pObj;
  char trigger[MAX_STRING_LENGTH];
  char phrase[MAX_STRING_LENGTH];
  char num[MAX_STRING_LENGTH];

  EDIT_OBJ(ch, pObj);
  argument=one_argument(argument, num);
  argument=one_argument(argument, trigger);
  argument=one_argument(argument, phrase);

  if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
  {
        send_to_char("Syntax:   addoprog [vnum] [trigger] [phrase]\n\r",ch);
        return false;
  }

    if ((tindex = trigger_index(trigger, PRG_OPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "oprog");
	return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

	if(value == TRIG_SPELLCAST) {
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "0");
		}
		else
		{
			int sn = skill_lookup(phrase);
			if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
				send_to_char("Invalid spell for trigger.\n\r",ch);
				return false;
			}
			sprintf(phrase,"%d",sn);
		}
	}
	else if( value == TRIG_EXIT || value == TRIG_EXALL )
	{
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "-1");
		}
		else
		{
			int door = parse_door(phrase);
			if( door < 0 ) {
				send_to_char("Invalid direction for exit/exall trigger.\n\r", ch);
				return false;
			}
			sprintf(phrase,"%d",door);
		}
	}


  if ((code = get_script_index (atol(num), PRG_OPROG)) == NULL)
  {
        send_to_char("No such OBJProgram.\n\r",ch);
        return false;
  }

    // Make sure this has a list of progs!
    if(!pObj->progs) pObj->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);

    list_appendlink(pObj->progs[slot], list);

  send_to_char("Oprog Added.\n\r",ch);
  return true;
}

OEDIT (oedit_deloprog)
{
    OBJ_INDEX_DATA *pObj;
    char oprog[MAX_STRING_LENGTH];
    long value;

    EDIT_OBJ(ch, pObj);

    one_argument(argument, oprog);
    if (!is_number(oprog) || oprog[0] == '\0')
    {
	send_to_char("Syntax:  deloprog [#oprog]\n\r",ch);
	return false;
    }

    value = atol (oprog);

    if (value < 0)
    {
	send_to_char("Only non-negative oprog-numbers allowed.\n\r",ch);
	return false;
    }

    if(!edit_deltrigger(pObj->progs,value)) {
	send_to_char("No such oprog.\n\r",ch);
	return false;
    }

    send_to_char("Oprog removed.\n\r", ch);
    return true;
}


REDIT (redit_addrprog)
{
    int tindex, value, slot;
    PROG_LIST *list;
    SCRIPT_DATA *code;
    ROOM_INDEX_DATA *pRoom;
    char trigger[MAX_STRING_LENGTH];
    char phrase[MAX_STRING_LENGTH];
    char num[MAX_STRING_LENGTH];

    EDIT_ROOM(ch, pRoom);
    argument=one_argument(argument, num);
    argument=one_argument(argument, trigger);
    argument=one_argument(argument, phrase);

    if (!is_number(num) || trigger[0] =='\0' || phrase[0] =='\0')
    {
	send_to_char("Syntax:   addrprog [vnum] [trigger] [phrase]\n\r",ch);
	return false;
    }

    if ((tindex = trigger_index(trigger, PRG_RPROG)) < 0) {
	send_to_char("Valid flags are:\n\r",ch);
	show_help(ch, "rprog");
	return false;
    }

    value = tindex;//trigger_table[tindex].value;
    slot = trigger_table[tindex].slot;

	if(value == TRIG_SPELLCAST) {
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "0");
		}
		else
		{
			int sn = skill_lookup(phrase);
			if(sn < 0 || skill_table[sn].spell_fun == spell_null) {
				send_to_char("Invalid spell for trigger.\n\r",ch);
				return false;
			}
			sprintf(phrase,"%d",sn);
		}
	}
	else if( value == TRIG_EXIT ||
			 value == TRIG_EXALL ||
			 value == TRIG_OPEN ||
			 value == TRIG_CLOSE ||
			 value == TRIG_KNOCK ||
			 value == TRIG_KNOCKING )
	{
		if( !str_cmp(phrase, "*") )
		{
			strcpy(phrase, "-1");
		}
		else
		{
			int door = parse_door(phrase);
			if( door < 0 ) {
				send_to_char("Invalid direction for exit/exall/open/close/knock/knocking trigger.\n\r", ch);
				return false;
			}
			sprintf(phrase,"%d",door);
		}
	}

    if ((code = get_script_index (atol(num), PRG_RPROG)) == NULL)
    {
	send_to_char("No such ROOMProgram.\n\r",ch);
	return false;
    }

    // Make sure this has a list of progs!
    if(!pRoom->progs->progs) pRoom->progs->progs = new_prog_bank();

    list                  = new_trigger();
    list->vnum            = atol(num);
    list->trig_type       = tindex;
    list->trig_phrase     = str_dup(phrase);
	list->trig_number		= atoi(list->trig_phrase);
    list->numeric		= is_number(list->trig_phrase);
    list->script          = code;
    //SET_BIT(pMob->mprog_flags,value);
    list_appendlink(pRoom->progs->progs[slot], list);

    send_to_char("Rprog Added.\n\r",ch);
    return true;
}


REDIT (redit_delrprog)
{
    ROOM_INDEX_DATA *pRoom;
    char rprog[MAX_STRING_LENGTH];
    long value;

    EDIT_ROOM(ch, pRoom);

    one_argument(argument, rprog);
    if (!is_number(rprog) || rprog[0] == '\0')
    {
	send_to_char("Syntax:  delrprog [#rprog]\n\r",ch);
	return false;
    }

    value = atol (rprog);

    if (value < 0)
    {
	send_to_char("Only non-negative rprog-numbers allowed.\n\r",ch);
	return false;
    }

    if(!edit_deltrigger(pRoom->progs->progs,value)) {
	send_to_char("No such rprog.\n\r",ch);
	return false;
    }

    send_to_char("Rprog removed.\n\r", ch);
    return true;
}


WEDIT ( wedit_create )
{
	LLIST_WILDS_DATA *data;
    WILDS_DATA *pWilds, *pLastWilds;
    WILDS_TERRAIN *pTerrain;
    AREA_DATA *pArea;
    char arg1[MIL];
    char arg2[MIL];
    char *pMap, *pStaticMap;
    long lScount, lMapsize;

    pArea = ch->in_room->area;

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("Wedit: you don't have OLC access to that area.\n\r", ch);
        return false;
    }

    argument = one_argument(argument, arg1);
    one_argument(argument, arg2);

    if (IS_NULLSTR(arg1) || IS_NULLSTR(arg2))
    {
        send_to_char("Wedit (create): Usage:\n\r", ch);
        send_to_char("              :        create <map_size_x> <map_size_y>\n\r", ch);
        return false;
    }

    if (!is_number(arg1) || !is_number(arg2))
    {
        send_to_char("Wedit (create): Wilds map dimensions must be integers.\n\r", ch);
        send_to_char("              : Usage:\n\r", ch);
        send_to_char("              :        create <map_size_x> <map_size_y>\n\r", ch);
        return false;
    }

    pWilds = new_wilds();
    pWilds->pArea = pArea;
    pWilds->name = str_dup("New Wilds");
    pWilds->map_size_x = atoi(arg1);
    pWilds->map_size_y = atoi(arg2);
    lMapsize = pWilds->map_size_x * pWilds->map_size_y;
    pWilds->staticmap = calloc(sizeof(char), lMapsize);
    pWilds->map = calloc(sizeof(char), lMapsize);
    pWilds->uid = ++gconfig.next_wilds_uid;
    gconfig_write();

    pMap = pWilds->map;
    pStaticMap = pWilds->staticmap;

    if((data = alloc_mem(sizeof(LLIST_WILDS_DATA)))) {
		data->wilds = pWilds;
		data->uid = pWilds->uid;

		list_appendlink(loaded_wilds, pWilds);
	}

    for(lScount = 0;lScount < lMapsize; lScount++)
    {
        *pMap++ = 'S';
        *pStaticMap++ = 'S';
    }

    if (pArea->wilds)
    {
        pLastWilds = pArea->wilds;

        while(pLastWilds->next)
            pLastWilds = pLastWilds->next;

        plogf("olc_act.c, wedit_create(): Adding Wilds to existing linked-list.");
        pLastWilds->next = pWilds;
    }
    else
    {
        plogf("olc_act.c, wedit_create(): Adding first Wilds to linked-list.");
        pArea->wilds = pWilds;
    }

    send_to_char("Wedit: New Wilds Region created.\n\r", ch);
    pTerrain = new_terrain(pWilds);
    pTerrain->mapchar = 'S';
    pTerrain->showchar = str_dup("{B~");
    pWilds->pTerrain = pTerrain;
    send_to_char("Wedit: Default wilds terrain mapping completed.\n\r", ch);
    return true;
}

WEDIT ( wedit_delete )
{
    send_to_char("{x[{W wedit delete{x ] Not implemented yet.\n\r\n\r", ch);
    return false;
}

WEDIT ( wedit_show )
{
    WILDS_DATA *pWilds;
    WILDS_TERRAIN *pTerrain;
    char buf[MSL];
    int col = 0;

    pWilds = (WILDS_DATA *)ch->desc->pEdit;
    send_to_char("{x[ {Wwedit show{x ]\n\r\n\r", ch);

    sprintf(buf, "Wilds defined in area {W%ld{x, '{W%s{x'\n\r", pWilds->pArea->anum, pWilds->pArea->name);
    send_to_char( buf, ch );

    sprintf(buf, "Map size: [ {W%d {xx {W%d{x ] ({W%ld{x vrooms)\n\r",
                  pWilds->map_size_x, pWilds->map_size_y,
                  (long)(pWilds->map_size_x * pWilds->map_size_y));
    send_to_char( buf, ch );

    show_map_to_char(ch, ch, 3, 3, true);

    send_to_char("\n\r{C*Terrain Key*{x\n\r", ch);

    for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
    {
        if (pTerrain->mapchar == pWilds->cDefaultTerrain)
        {
            /* Vizz - Handle colour code char exception before send_to_char() */
            if (pTerrain->mapchar == '{')
                sprintf(buf, "Default terrain: '{W{%c{x' '%s{x' {W%-12s{x\n\r\n\r",
                             pTerrain->mapchar, pTerrain->showchar,
                             pTerrain->showname ? pTerrain->showname : "(Not Set)");
            else
                sprintf(buf, "Default terrain: '{W%c{x' '%s{x' {W%-12s{x\n\r\n\r",
                             pTerrain->mapchar, pTerrain->showchar,
                             pTerrain->showname ? pTerrain->showname : "(Not Set)");

            send_to_char(buf, ch);
        }
    }

    send_to_char("Tile Ansi Name        Tile Ansi Name        Tile Ansi Name\n\r", ch);
    for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
    {
        /* Vizz - Handle colour code char exception before send_to_char() */
        if (pTerrain->mapchar == '{')
            sprintf(buf, " '{W{%c{x'  '%s{x' {W%-12s{x{x",
                         pTerrain->mapchar, pTerrain->showchar,
                         pTerrain->showname ? pTerrain->showname : "(Not Set)");
        else
            sprintf(buf, " '{W%c{x'  '%s{x' {W%-12s{x{x",
                         pTerrain->mapchar, pTerrain->showchar,
                         pTerrain->showname ? pTerrain->showname : "(Not Set)");

        send_to_char(buf, ch);

        if (col++ % 3 == 2)
            send_to_char("\n\r", ch);

    }

    send_to_char("\n\r\n\r{C*Current State*{x\n\r", ch);

    sprintf(buf, "Players: {W%d{x\n\r", pWilds->nplayer);
    send_to_char( buf, ch );

    sprintf(buf, "Age: {W%d{x\n\r", pWilds->age);
    send_to_char( buf, ch );

    if (!IS_SET(ch->comm, COMM_COMPACT))
        send_to_char("\n\r", ch);

    return false;
}

WEDIT (wedit_name)
{
    WILDS_DATA *pWilds;

    EDIT_WILDS (ch, pWilds);

    if (argument[0] == '\0')
    {
        send_to_char ("Syntax:  name [name]\n\r", ch);
        return false;
    }

    free_string (pWilds->name);
    pWilds->name = str_dup (argument);

    send_to_char ("Wilds name set.\n\r", ch);
    return true;
}

void correct_vrooms(WILDS_DATA *pWilds, WILDS_TERRAIN *pTerrain)
{
	register ROOM_INDEX_DATA *vroom;
	ITERATOR it;

	iterator_start(&it, pWilds->loaded_vrooms);

	while( (vroom = (ROOM_INDEX_DATA *)iterator_nextdata(&it)) ) {
		if(vroom->parent_template == pTerrain) {
			free_string(vroom->name);
			vroom->name = str_dup(pTerrain->template->name);
			vroom->room_flag[0] = pTerrain->template->room_flag[0];
			vroom->room_flag[1] = pTerrain->template->room_flag[1]|ROOM_VIRTUAL_ROOM;
				vroom->sector_type = pTerrain->template->sector_type;
		}
	}

	iterator_stop(&it);
}

WEDIT ( wedit_terrain )
{
    WILDS_DATA *pWilds;
    WILDS_TERRAIN *pTerrain;
    char token;
    char arg[MIL];
    char arg2[MIL];
    int value;

    EDIT_WILDS (ch, pWilds);

    if (argument[0] == '\0')
    {
        send_to_char ("Syntax:  terrain <token> create\n\r"
                      "         terrain <token> delete\n\r"
                      "         terrain <token> ansi <string>\n\r"
                      "         terrain <token> showname <string>\n\r"
                      "         terrain <token> briefdesc <string>\n\r"
                      "         terrain <token> room_flag <flag>\n\r"
                      "         terrain <token> room2flag <flag>\n\r"
                      "         terrain <token> sector <sector>\n\r"
                      "         terrain list <flag>\n\r", ch);
        return false;
    }

    argument = one_caseful_argument (argument, arg);
    argument = one_caseful_argument (argument, arg2);

    if (!str_cmp(arg, "list"))
    {
        BUFFER *output;
        char buf[MSL];

        output = new_buf();
        add_buf(output, "[{WWedit{x] Full Terrain List:\n\r\n\r");
        add_buf(output, "Token  Ansi  Showname        Sector          Nonroom?  Flags\n\r");

        for(pTerrain=pWilds->pTerrain;pTerrain;pTerrain=pTerrain->next)
        {
            sprintf(buf, " '{W%c{x'   '%s{x'   {W%-15s{x  {W%-15s{x  {W%s%s{x\n\r",
                     pTerrain->mapchar, pTerrain->showchar,
                     pTerrain->showname ? pTerrain->showname : "(Not Set)",
                     flag_string(sector_flags, pTerrain->template->sector_type),
                     pTerrain->nonroom ? "  Yes    " : "  No     ",
                     bitmatrix_string(room_flagbank, pTerrain->template->room_flag));
                     //flag_string(room2_flags, pTerrain->template->room_flag[1]));
            add_buf(output, buf);
        }

        send_to_char(buf_string(output), ch);
        free_buf(output);

        return false;
    }

    token = arg[0];
    pTerrain = get_terrain_by_token(pWilds, token);

    if (!str_cmp(arg2, "create"))
    {

        if (token == '\0')
        {
            send_to_char ("Syntax:\n\r         terrain <token> create\n\r", ch);
            return false;
        }

        if (pTerrain)
        {
            send_to_char ("[Wedit] That token has already been assigned.\n\r", ch);
            return false;
        }

        pTerrain = new_terrain(pWilds);
        add_terrain (pWilds, pTerrain);

        pTerrain->mapchar = token;
        pTerrain->showchar = str_dup(" ");
        pTerrain->showchar[0] = token;
        send_to_char ("[Wedit] Terrain token added.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "delete"))
    {
        if (token == '\0')
        {
            send_to_char ("Syntax:\n\r         terrain <token> delete\n\r", ch);
            return false;
        }

        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (token == pTerrain->pWilds->cDefaultTerrain)
        {
            send_to_char ("[Wedit] You can't delete the default terrain.\n\r", ch);
            return false;
        }

        plogf ("Deleting terrain struct for token '%c'", token);
        del_terrain (pWilds, pTerrain);
        send_to_char ("[Wedit] Terrain token deleted.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "ansi"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> ansi <string>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showchar);
        pTerrain->showchar = str_dup (argument);

        send_to_char ("[Wedit] Terrain ansi set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "showname"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> showname <name>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showname);
        pTerrain->showname = str_dup (argument);

        free_string (pTerrain->template->name);
        pTerrain->template->name = str_dup (argument);

	correct_vrooms(pWilds, pTerrain);

        send_to_char ("[Wedit] Terrain showname set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "briefdesc"))
    {
        if (pTerrain == NULL)
        {
            send_to_char ("[Wedit] That token does not exist.\n\r", ch);
            return false;
        }

        if (argument[0] == '\0')
        {
            send_to_char ("Syntax:  terrain <token> briefdesc <string>\n\r", ch);
            return false;
        }

        free_string (pTerrain->showname);
        pTerrain->showname = str_dup (argument);

        send_to_char ("[Wedit] Terrain briefdesc set.\n\r", ch);
        return true;
    }

    if (!str_cmp(arg2, "room_flag"))
    {
	long room_flag[2];
	if (bitvector_lookup(argument, 2, room_flag, room_flags, room2_flags))
	{
		TOGGLE_BIT(pTerrain->template->room_flag[0], room_flag[0]);
		TOGGLE_BIT(pTerrain->template->room_flag[1], room_flag[1]);
		correct_vrooms(pWilds, pTerrain);
		send_to_char("Room flags toggled.\n\r", ch);
        return true;
	/*
	if ((value = flag_value(room_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> room_flag <flag>\n\r", ch);
	    return false;
	}

	TOGGLE_BIT(pTerrain->template->room_flag[0], value);
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Room flags toggled.\n\r", ch);
        return true;
	*/
    }
/*
    if (!str_cmp(arg2, "room2flag"))
    {
	if ((value = flag_value(room2_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> room2flag <flag>\n\r", ch);
	    return false;
	}

	TOGGLE_BIT(pTerrain->template->room2_flags, value);
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Room2 flags toggled.\n\r", ch);
        return true;
    }
*/
	}
    if (!str_cmp(arg2, "sector"))
    {
	if ((value = flag_value(sector_flags, argument)) == NO_FLAG)
	{
	    send_to_char("Syntax: terrain <token> sector <sector>\n\r", ch);
	    return false;
	}

	pTerrain->template->sector_type =  value;
	correct_vrooms(pWilds, pTerrain);
	send_to_char("Sector set.\n\r", ch);
        return true;
    }

    return false;
}

WEDIT ( wedit_vlink )
{
    WILDS_DATA *pWilds;
    WILDS_VLINK *pVLink;
    char arg[MIL],
         arg2[MIL],
         arg3[MIL],
         arg4[MIL],
         *argsave;

    EDIT_WILDS (ch, pWilds);

    argument = one_argument (argument, arg);
    argsave = argument = one_argument (argument, arg2);
    argument = one_argument (argument, arg3);
    argument = one_argument (argument, arg4);

    if (!arg[0])
    {
       send_to_char("Usage:\n\r", ch);
       send_to_char("       [wedit] vlink link <vlinknum>\n\r", ch);
       send_to_char("               - Applies a vlink into play.\n\r", ch);
       send_to_char("       [wedit] vlink unlink <vlinknum>\n\r", ch);
       send_to_char("               - Removes a vlink from play.\n\r", ch);
       send_to_char("       [wedit] vlink create [<x coor> <y coor> <direction>]\n\r", ch);
       send_to_char("               - Creates a new vlink (unlinked) at your wilds location.\n\r", ch);
       send_to_char("       [wedit] vlink linkage <vlinknum> <to_wilds|from_wilds|two_way>\n\r", ch);
       send_to_char("               - Sets both the current and default linkage of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink direction <vlinknum> <door>\n\r", ch);
       send_to_char("               - Change the direction of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink destination <vlinknum> <vnum>\n\r", ch);
       send_to_char("               - Sets the destination of the unlinked vlink.\n\r", ch);
       send_to_char("       [wedit] vlink location <vlinknum> <x> <y>\n\r", ch);
       send_to_char("               - Sets the location of the vlink.\n\r", ch);
       send_to_char("       [wedit] vlink maptile <vlinknum> <tile>\n\r", ch);
       send_to_char("               - Sets the maptile of the vlink.\n\r", ch);
       send_to_char("       [wedit] vlink delete <vlinknum>\n\r", ch);
       send_to_char("               - Deletes selected vlink.\n\r", ch);
       send_to_char("       [wedit] vlink list\n\r", ch);
       send_to_char("               - Lists the status of all vlinks in wilds you are in.\n\r", ch);
       return false;
    }

    if (!str_cmp(arg, "link"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink link: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (link_vlink(pVLink)) {
			send_to_char("Wedit vlink: Found matching vlnum.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but could not link it.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "unlink"))
    {
        int vlnum = 0;

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink link: Invalid vlinknum. For usage, type 'vlink'", ch);
            return (false);
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (unlink_vlink(pVLink)) {
			send_to_char("Wedit vlink: Found matching vlnum.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but could not unlink it.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink unlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

// VIZZWILDS - CURRENT WORK IN PROGRESS - CREATE ROUTINE ADDITION
    if (!str_cmp(arg, "create"))
    {
	    int x=0, y=0, door=0;
        WILDS_VLINK *temp_pVLink;

        if (arg2[0]) {
		if((!is_number(arg2) || (x = atoi(arg2)) < 0 || x > (pWilds->map_size_x - 1)) ||
			(!is_number(arg3) || (y = atoi(arg3)) < 0 || y > (pWilds->map_size_y - 1)) ||
			((door = parse_direction(arg4)) < 0))
	        {
	            send_to_char("Syntax: vlink create [<x coord> <y coord> <direction>]", ch);
	            return false;
		}
	}

        temp_pVLink = new_vlink();
        temp_pVLink->uid = ++gconfig.next_vlink_uid;	// Give it a UID
        temp_pVLink->wildsorigin_x = x;
        temp_pVLink->wildsorigin_y = y;
        temp_pVLink->door = door;
        temp_pVLink->map_tile = str_dup("{YO");
        temp_pVLink->orig_description = str_dup("");
        temp_pVLink->orig_keyword = str_dup("");
        temp_pVLink->rev_description = str_dup("");
        temp_pVLink->rev_keyword = str_dup("");
        add_vlink(pWilds, temp_pVLink);
        gconfig_write();				// Save the UIDs

        send_to_char("Wedit vlink create: Link created.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "delete"))
    {
        send_to_char("Wedit vlink delete not implemented yet.\n\r", ch);
        return false;
    }

    if (!str_cmp(arg, "linkage"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if(!str_prefix(arg3,"to_wilds")) pVLink->default_linkage = VLINK_TO_WILDS;
			else if(!str_prefix(arg3,"from_wilds")) pVLink->default_linkage = VLINK_FROM_WILDS;
			else if(!str_prefix(arg3,"two_way")) pVLink->default_linkage = VLINK_TO_WILDS|VLINK_FROM_WILDS;
			else {
				printf_to_char(ch, "Wedit vlink: Invalid linkage.  Valid values are {Wto_wilds{x, {Wfrom_wilds{x and {Wtwo_way{x.\n\r", vlnum);
				return false;
			}
			send_to_char("Wedit vlink: Linkage set.\n\r", ch);
			return true;
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "direction"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			value = parse_direction(arg3);
			if(value >= 0) {
				pVLink->door = value;
				send_to_char("Wedit vlink: Door set.\n\r", ch);
				return true;
			} else
				printf_to_char(ch, "Wedit vlink: Invalid direction.\n\r", vlnum);

		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "destination"))
    {
        int vlnum = 0;
        int value;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if (is_number(arg3) && (value = atoi(arg3)) > 0) {
				ROOM_INDEX_DATA *destRoom = get_room_index(value);

				if( !destRoom )
				{
					send_to_char("Wedit vlink: Invalid destination.\n\r", ch);
					return false;
				}

				if( IS_SET(destRoom->room_flag[1], ROOM_BLUEPRINT) ||
					IS_SET(destRoom->area->area_flags, AREA_BLUEPRINT) )
				{
					send_to_char("Wedit vlink: Invalid destination.\n\r", ch);
					return false;
				}

				pVLink->destvnum = value;
				send_to_char("Wedit vlink: Destination set.\n\r", ch);
				return true;
			} else
				send_to_char("Wedit vlink: Invalid destination", ch);
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }


    if (!str_cmp(arg, "location"))
    {
        int vlnum = 0;
        int x, y;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if(pVLink->current_linkage == VLINK_UNLINKED) {
			if (is_number(arg3) && (x = atoi(arg3)) >= 0 && x < pWilds->map_size_x &&
				is_number(arg4) && (y = atoi(arg4)) >= 0 && y < pWilds->map_size_y) {
				pVLink->wildsorigin_x = x;
				pVLink->wildsorigin_y = y;
				send_to_char("Wedit vlink: Location set.\n\r", ch);
				return true;
			} else
				send_to_char("Wedit vlink: Invalid location", ch);
		} else
			printf_to_char(ch, "Wedit vlink: Found vlink %d, but it needs to be unlinked first.\n\r", vlnum);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "maptile"))
    {
        int vlnum = 0;

        if (!pWilds)
        {
            plogf("wilds.c, wedit_vlink(): Failed to link vlink ch->in_wilds is NULL");
            return false;
        }

        if (!is_number(arg2) || (vlnum = atoi(arg2)) < 0)
        {
            send_to_char("Wedit vlink: Invalid vlinknum. Type 'vlink list' for valid vlinknums", ch);
            return false;
        }

        pVLink = get_vlink_from_index(pWilds,vlnum);
        if(pVLink) {
		if (argsave[0]) {
			free_string(pVLink->map_tile);
			pVLink->map_tile = str_dup(argsave);
			send_to_char("Wedit vlink: Map tile set.\n\r", ch);
			return true;
		} else
			send_to_char("Wedit vlink: Invalid maptile", ch);
	} else
		send_to_char("Wedit vlink: Could not find vlink. Try 'vlink show' for a list.", ch);

        return false;
    }

    if (!str_cmp(arg, "list"))
    {
	    int vlnum;
        if (!ch->in_room)
        {
            plogf("wilds.c, vlinks(): ch->in_room invalid.");
            return false;
        }
        else
            if (!ch->in_room->area)
            {
                plogf("wilds.c, vlinks(): ch->in_room->area invalid.");
                return false;
            }
            else
                if (!ch->in_room->area->wilds)
                {
                    plogf("wilds.c, vlinks(): ch->in_room->area->wilds invalid.");
                    return false;
                }

        send_to_char("{x[ {Wwedit vlink{x ]\n\r\n\r", ch);
        send_to_char("[num] [uid]   [x coor] [y coor] [direction] "
                     "[destvnum] [default] [current] [maptile]\n\r", ch);

        for(vlnum = 0,pVLink=pWilds?pWilds->pVLink:ch->in_room->area->wilds->pVLink;pVLink!=NULL;pVLink = pVLink->next)
        {
            printf_to_char(ch, "%-5d ({W%6ld{x)  {W%6d   %6d   %-9s   %-8ld   %10s%10s%s{x\n\r",
            		   vlnum++,
                           pVLink->uid,
                           pVLink->wildsorigin_x,
                           pVLink->wildsorigin_y,
                           dir_name[pVLink->door],
                           pVLink->destvnum,
                           vlinkage_bit_name(pVLink->default_linkage),
                           vlinkage_bit_name(pVLink->current_linkage),
                           pVLink->map_tile);
        }

        if (!IS_SET(ch->comm, COMM_COMPACT))
        {
            send_to_char("\n\r", ch);
        }
    }

    return false;
}

VLEDIT ( vledit_show )
{
    WILDS_VLINK *pVLink;

    pVLink = (WILDS_VLINK *)ch->desc->pEdit;
    send_to_char("{x[ {WVLedit show{x ]\n\r\n\r", ch);

    printf_to_char(ch, "Uid: %ld\n\r",   pVLink->uid);
    printf_to_char(ch, "Wilds vroom x coor: %ld\n\r",   pVLink->wildsorigin_x);
    printf_to_char(ch, "Wilds vroom y coor: %ld\n\r",   pVLink->wildsorigin_y);
    printf_to_char(ch, "Wilds map tile: %ld\n\r", pVLink->map_tile);
    printf_to_char(ch, "Wilds link direction: %s\n\r",   dir_name[pVLink->door]);
    printf_to_char(ch, "Destination room vnum: %ld\n\r",   pVLink->destvnum);
    printf_to_char(ch, "Default linkage flags: %s\n\r",   vlinkage_bit_name(pVLink->default_linkage));
    printf_to_char(ch, "Current linkage flags: %s\n\r\n\r",   vlinkage_bit_name(pVLink->current_linkage));
    printf_to_char(ch, "Wilds link description: %s\n\r",   pVLink->orig_description);
    printf_to_char(ch, "Wilds link keyword: %s\n\r",   pVLink->orig_keyword);
    printf_to_char(ch, "Wilds link rs_flags: %s\n\r",   flag_string(exit_flags, pVLink->orig_rs_flags));
    printf_to_char(ch, "Wilds key obj vnum: %ld\n\r\n\r",   pVLink->orig_key);
    printf_to_char(ch, "Wilds lock flags: %s\n\r",   flag_string(lock_flags, pVLink->orig_lock));
    printf_to_char(ch, "Wilds link pick chance: %d%%\n\r\n\r",   pVLink->orig_pick);
    printf_to_char(ch, "Reverse link description: %s\n\r",   pVLink->rev_description);
    printf_to_char(ch, "Reverse link keyword: %s\n\r",   pVLink->rev_keyword);
    printf_to_char(ch, "Reverse link rs_flags: %s\n\r",   flag_string (exit_flags, pVLink->rev_rs_flags));
    printf_to_char(ch, "Reverse key obj vnum: %ld\n\r\n\r",   pVLink->rev_key);
    printf_to_char(ch, "Reverse lock flags: %s\n\r",   flag_string(lock_flags, pVLink->rev_lock));
    printf_to_char(ch, "Reverse link pick chance: %d%%\n\r\n\r",   pVLink->rev_pick);

    return (false);
}

MEDIT(medit_questor)
{
	MOB_INDEX_DATA *pMob;
	char arg[MIL];

	EDIT_MOB(ch, pMob);

	if(IS_NULLSTR(argument))
	{
		send_to_char("QUESTOR ADD                 Adds questor data to mob.\n\r", ch);
		send_to_char("        REMOVE              Removes questor data from mob.\n\r", ch);
		send_to_char("        SCROLL [vnum]       Sets the scroll object to the specified vnum.\n\r", ch);
		send_to_char("        KEYWORDS [string]   Sets keywords of scroll.\n\r", ch);
		send_to_char("        SHORT [string]      Sets short description of scroll.\n\r", ch);
		send_to_char("        LONG [string]       Sets long description of scroll.\n\r", ch);
		send_to_char("        HEADER              Edits scroll header.\n\r", ch);
		send_to_char("        FOOTER              Edits scroll footer.\n\r", ch);
		send_to_char("        PREFIX [string]     Edits line prefix.\n\r", ch);
		send_to_char("        SUFFIX [string]     Edits line suffix.\n\r", ch);
		send_to_char("        WIDTH [width]       Sets line width.\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if (!str_prefix(arg,"add"))
	{
	    if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
	    {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }

	    if( pMob->pQuestor != NULL )
	    {
			send_to_char("There is already questor data.\n\r", ch);
			return false;
		}

		pMob->pQuestor = new_questor_data();
	    use_imp_sig(pMob, NULL);
		send_to_char("Questor data added.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"remove")) {
	    if (!str_cmp(pMob->sig, "none") && ch->tot_level < MAX_LEVEL)
	    {
			send_to_char("You can't do this without an IMP's permission.\n\r", ch);
			return false;
	    }

	    if( pMob->pQuestor == NULL )
	    {
			send_to_char("There is any questor data.\n\r", ch);
			return false;
		}

		free_questor_data(pMob->pQuestor);
		pMob->pQuestor = NULL;
		send_to_char("Questor data removed.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"scroll")) {
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		long vnum = atoi(argument);
		if( !get_obj_index(vnum) )
		{
			send_to_char("Object does not exist.\n\r", ch);
			return false;
		}

		pMob->pQuestor->scroll = vnum;
		send_to_char("Questor scroll object changed.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"keywords")) {
		free_string(pMob->pQuestor->keywords);
		pMob->pQuestor->keywords = str_dup(argument);

		send_to_char("Keywords set.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"short")) {
		free_string(pMob->pQuestor->short_descr);
		pMob->pQuestor->short_descr = str_dup(argument);

		send_to_char("Short description set.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"long")) {
		free_string(pMob->pQuestor->long_descr);
		pMob->pQuestor->long_descr = str_dup(argument);

		send_to_char("Long description set.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"header")) {
		send_to_char("Editting the Questor Header:\n\r", ch);
		send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
		send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
		send_to_char("\n\r", ch);

		string_append(ch, &pMob->pQuestor->header);
		return true;

	} else if (!str_prefix(arg,"footer")) {
		send_to_char("Editting the Questor Footer:\n\r", ch);
		send_to_char("  Use {Y$PLAYER${x as a placeholder for the player's name.\n\r", ch);
		send_to_char("  Use {Y$QUESTOR${x as a placeholder for the questgiver's name.\n\r", ch);
		send_to_char("\n\r", ch);

		string_append(ch, &pMob->pQuestor->footer);
		return true;

	} else if (!str_prefix(arg,"prefix")) {
		free_string(pMob->pQuestor->prefix);
		pMob->pQuestor->prefix = str_dup(argument);

		send_to_char("Prefix set.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"suffix")) {
		free_string(pMob->pQuestor->suffix);
		pMob->pQuestor->suffix = str_dup(argument);

		send_to_char("Prefix set.\n\r", ch);
		return true;

	} else if (!str_prefix(arg,"width")) {
		if(!is_number(argument))
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int width = atoi(argument);
		if( width <= 0 )
		{
			pMob->pQuestor->line_width = 0;
			send_to_char("Line width disabled.\n\r", ch);
			return true;

		}
		else if(width > 160)
		{
			send_to_char("Width is out of range.  Please specify a number from 1 to 160, or 0 to disable width.\n\r", ch);
			return false;
		}

		pMob->pQuestor->line_width = width;
		send_to_char("Line width set.\n\r", ch);
		return true;

	} else {
		medit_questor(ch, "");
		return false;
	}

	return true;

}

MEDIT( medit_crew )
{
	MOB_INDEX_DATA *pMob;
	char arg[MIL];

	EDIT_MOB(ch, pMob);

	if(IS_NULLSTR(argument))
	{
		send_to_char("Syntax:  crew assign\n\r", ch);
		send_to_char("         crew remove\n\r", ch);
		send_to_char("         crew minrank <rank>\n\r", ch);
		send_to_char("         crew scouting <rating>\n\r", ch);
		send_to_char("         crew gunning <rating>\n\r", ch);
		send_to_char("         crew oarring <rating>\n\r", ch);
		send_to_char("         crew mechanics <rating>\n\r", ch);
		send_to_char("         crew navigation <rating>\n\r", ch);
		send_to_char("         crew leadership <rating>\n\r", ch);
		return false;
	}

	argument = one_argument(argument, arg);

	if( !str_prefix(arg, "assign") )
	{
		if( IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile already has ship crew data.\n\r", ch);
			return false;
		}

		pMob->pCrew = new_ship_crew_index();
		send_to_char("Ship Crew assigned.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "remove") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile has no ship crew data.\n\r", ch);
			return false;
		}

		free_ship_crew_index(pMob->pCrew);
		pMob->pCrew = NULL;
		send_to_char("Ship Crew removed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "minrank") )
	{
		send_to_char("Not implemented yet.\n\r", ch);
		return false;
	}

	if( !str_prefix(arg, "scouting") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->scouting = value;
		send_to_char("Scouting Rating changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "gunning") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->gunning = value;
		send_to_char("Gunning Rating changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "oarring") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->oarring = value;
		send_to_char("Oarring Rating changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "mechanics") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->mechanics = value;
		send_to_char("Mechanics Rating changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "navigation") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->navigation = value;
		send_to_char("Navigation Rating changed.\n\r", ch);
		return true;
	}

	if( !str_prefix(arg, "leadership") )
	{
		if( !IS_VALID(pMob->pCrew) )
		{
			send_to_char("Mobile is not assigned as a ship crew.\n\r", ch);
			return false;
		}

		if( !is_number(argument) )
		{
			send_to_char("That is not a number.\n\r", ch);
			return false;
		}

		int value = atoi(argument);
		if( value < 0 || value > 100 )
		{
			send_to_char("Rating out of range.  Please specify a value from 0 to 100.\n\r", ch);
			return false;
		}

		pMob->pCrew->leadership = value;
		send_to_char("Leadership Rating changed.\n\r", ch);
		return true;
	}


	medit_crew(ch, "");
	return false;
}
