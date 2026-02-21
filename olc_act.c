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
#include "item_types.h"

extern GLOBAL_DATA         gconfig;
extern void oedit_show_type_data(OBJ_INDEX_DATA *pObj, BUFFER *buffer);
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
#define STRUCT_SPELLFUNC	8


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
    {   "material",	NULL,            "Object materials."		 },
    {	"mprog",	trigger_table,	 "MobProgram types."		 },
    {	"oprog",	trigger_table,	 "ObjProgram types."		 },
    {	"rprog",	trigger_table,	 "RoomProgram types."		 },
    {	"tprog",	trigger_table,	 "TokenProgram types."		 },
    {	"aprog",	trigger_table,	 "AreaProgram types."		 },
    {	"iprog",	trigger_table,	 "InstanceProgram types."		 },
    {	"dprog",	trigger_table,	 "DungeonProgram types."		 },
    {	"qprog",	trigger_table,	 "QuestProgram types."		 },
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
    {	"area_region_flags",			STRUCT_FLAGS,		area_region_flags,				"Area region flags."	},
    {	"areawho",				STRUCT_FLAGS,		area_who_titles,			"Type of area for who."	},
    {	"armour",				STRUCT_FLAGS,		ac_type,					"Ac for different attacks."	},
    {	"catalyst",				STRUCT_FLAGS,		catalyst_types,				"Catalyst types."	},
    {	"cmd",					STRUCT_FLAGS,		command_flags,				"Command Flags (CMDEdit)"},
    {	"condition",			STRUCT_FLAGS,		room_condition_flags,		"Room Condition types."	},
    {	"container",			STRUCT_FLAGS,		container_flags,			"Container status."	},
    {	"corpsetypes",			STRUCT_FLAGS,		corpse_types,				"Corpse types."	},
    {	"damageclass",			STRUCT_FLAGS,		damage_classes,				"Types of damages."},
    {	"do_func",				STRUCT_SPELLFUNC,	do_func_table,				"Do_ functions (CMDEdit)"},
    {	"dprog",				STRUCT_TRIGGERS,	trigger_table,				"DungeonProgram types."	},
    {	"qprog",				STRUCT_TRIGGERS,	trigger_table,				"QuestProgram types."	},
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
    {	"liquid",				STRUCT_LIQUID,		NULL,						"Liquid types."	},
    {   "log",					STRUCT_FLAGS,		log_flags,					"Log levels (CMDEdit)"},
    {	"lock",					STRUCT_FLAGS,		lock_flags,					"Lock state types."	},
    {	"material",				STRUCT_MATERIAL,	NULL,					"Object materials."	},
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
    {	"wilderness_regions",		STRUCT_FLAGS,		wilderness_regions,			"Wilderness region names"	},
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

void show_spell_funcs(CHAR_DATA *ch, const struct do_func_type *table)
{
    char buf  [ MAX_STRING_LENGTH ];
    //char buf1 [ MAX_STRING_LENGTH ];
    int  col;
    BUFFER *buffer = new_buf();

    //buf1[0] = '\0';
    col = 0;
    add_buf(buffer, "Functions available for use:\n\r");
    for (int i = 0; table[i].name != NULL; i++)
    {
        sprintf(buf, "%-19.18s", table[i].name);
        add_buf(buffer, buf);
        if (++col % 4 == 0)
            add_buf(buffer, "\n\r");
    }

    if (col % 4 != 0)
    add_buf(buffer, "\n\r");

    if( !ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(buffer->string, ch);
    }

    free_buf(buffer);
    return;
}

static void show_sector_cmds(CHAR_DATA *ch)
{
    BUFFER *buffer;
    char row[MSL];

    buffer = new_buf();

    add_buf(buffer, "{WIdx  Name                 Move Heal Mana{X\n\r");
    add_buf(buffer, "{D---- -------------------- ---- ---- ----{X\n\r");

    for (int i = 0; i < sector_count(); i++)
    {
        snprintf(row, sizeof(row), "{W%-4d %-20s %4d %4d %4d{X\n\r",
            i,
            sector_name(i),
            sector_move_cost(i),
            sector_heal_rate(i),
            sector_mana_rate(i));
        add_buf(buffer, row);
    }

    if (!ch->lines && strlen(buffer->string) > MAX_STRING_LENGTH)
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    else
        page_to_char(buffer->string, ch);

    free_buf(buffer);
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
        if (!str_cmp(help_table[cnt].command, "material"))
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
            else if (!str_prefix(arg, "qprog"))
            {
                send_to_char("QuestProgram Triggers:\n\r", ch);
                for (i = 0, n = 0; trigger_table[i].name != NULL; i++)
                {
                    if (trigger_table[i].quest)
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
                case STRUCT_SPELLFUNC:
                    show_spell_funcs(ch, (const struct do_func_type *)help_table[cnt].structure);
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
                    if (help_table[cnt].structure == sector_flags)
                        show_sector_cmds(ch);
                    else
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
    int count;
    int col;

    buf1[0] = '\0';
    col = 0;
    count = material_count();
    for (i = 0; i < count; i++)
    {
    sprintf(buf, "%-19.18s", material_name(i));
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

/**
 * Check if a script is attached to an entity.
 */
bool edit_script_attached(LLIST **progs, SCRIPT_DATA *script)
{
    if (!progs || !script) return false;

    for (int slot = 0; slot < TRIGSLOT_MAX; slot++) {
        if (!progs[slot]) continue;

        ITERATOR it;
        PROG_LIST *trigger;
        iterator_start(&it, progs[slot]);
        while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
            if (trigger->script == script) {
                iterator_stop(&it);
                return true;
            }
        }
        iterator_stop(&it);
    }

    return false;
}

/**
 * Check if a specific trigger (type + phrase) is already attached to an entity
 * for a specific script.
 */
bool edit_trigger_exists(LLIST **progs, SCRIPT_DATA *script, int trig_type, const char *phrase)
{
    if (!progs || !script) return false;

    int slot = trigger_table[trig_type].slot;
    if (slot < 0 || slot >= TRIGSLOT_MAX || !progs[slot]) return false;

    ITERATOR it;
    PROG_LIST *trigger;
    iterator_start(&it, progs[slot]);
    while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
        if (trigger->script == script &&
            trigger->trig_type == trig_type &&
            !str_cmp(trigger->trig_phrase, phrase)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

/**
 * Delete all triggers for a specific script attached to an entity.
 */
bool edit_delscript(LLIST **progs, SCRIPT_DATA *script)
{
    if (!progs || !script) return false;

    bool found = false;
    for (int slot = 0; slot < TRIGSLOT_MAX; slot++) {
        if (!progs[slot]) continue;

        ITERATOR it;
        PROG_LIST *trigger;
        iterator_start(&it, progs[slot]);
        while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
            if (trigger->script == script) {
                iterator_remcurrent(&it);
                free_trigger(trigger);
                found = true;
            }
        }
        iterator_stop(&it);
    }

    return found;
}

/**
 * Delete a specific trigger (type + phrase) for a specific script attached to an entity.
 */
bool edit_deltrigger_specific(LLIST **progs, SCRIPT_DATA *script, int trig_type, const char *phrase)
{
    if (!progs || !script) return false;

    int slot = trigger_table[trig_type].slot;
    if (slot < 0 || slot >= TRIGSLOT_MAX || !progs[slot]) return false;

    ITERATOR it;
    PROG_LIST *trigger;
    iterator_start(&it, progs[slot]);
    while ((trigger = (PROG_LIST *)iterator_nextdata(&it))) {
        if (trigger->script == script &&
            trigger->trig_type == trig_type &&
            !str_cmp(trigger->trig_phrase, phrase)) {
            iterator_remcurrent(&it);
            free_trigger(trigger);
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

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
    pbugf(LOG_ERROR, "Rprog: No vnum to create entrance or delete, on room %d.",
        pRoom->vnum);
    return false;
    }

    if (!str_cmp(arg, "delete"))
    {
    int16_t rev;

    if (!pRoom->exit[door])
    {
        pbugf(LOG_ERROR, "RProg: Couldn't delete room. %d", pRoom->vnum);
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

AREA_DATA *area = find_area_by_vnum(value, NULL);
if (!area) area = get_system_area_fallback();
if (!get_room_index(area, value))
    {
       pbugf(LOG_ERROR, "Rprog: A link cannot link non-existant room.\n\r");
       return false;
    }

    if (get_room_index(area, value)->exit[rev_dir[door]])
    {
       pbugf(LOG_ERROR, "Rprog: Reverse-side exit to room already exists.\n\r");
       return false;
    }

    if (!pRoom->exit[door])
    {
       pRoom->exit[door] = new_exit();
    pRoom->exit[door]->from_room = pRoom;
    }

    pToRoom = pRoom->exit[door]->u1.to_room = get_room_index(area, value);
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
            pbugf(LOG_ERROR, "change_exit: pToRoom was null! room is %s (%ld), door is %i",
                pRoom->name, pRoom->vnum, door);
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
        WNUM room_wnum;
        AREA_DATA *context;

        if (arg[0] == '\0')
        {
            send_to_char("Syntax:  [direction] link [uid#vnum | #vnum | vnum]\n\r", ch);
            return false;
        }

        context = ch->in_room->area;
        
        if (!parse_widevnum(arg, context, &room_wnum))
        {
            send_to_char("Invalid room vnum format.\n\r", ch);
            return false;
        }

        ROOM_INDEX_DATA *pToRoom = room_wnum.pArea ?
            get_room_index(room_wnum.pArea, room_wnum.vnum) :
            get_room_index_global(room_wnum.vnum);
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

        if( !rooms_in_same_section(pRoom->vnum, room_wnum.vnum) )
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
        // Store cross-area UID for serialization
        pRoom->exit[door]->wilds.area_uid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
        
        door							= rev_dir[door];
        pExit							= new_exit();

        pExit->u1.to_room				= pRoom;
        pExit->orig_door				= door;
        // Reverse exit gets the current room's area UID
        pExit->wilds.area_uid = pRoom->area ? pRoom->area->uid : 0;
        pToRoom->exit[door]				= pExit;
        pExit->from_room				= pToRoom;

        send_to_char("Two-way link established.\n\r", ch);
        return true;
    }

    if (!str_cmp(command, "dig"))
    {
        WNUM room_wnum;
        AREA_DATA *context;

        if (arg[0] == '\0')
        {
            send_to_char("Syntax:  [direction] dig [uid#vnum | #vnum | vnum]\n\r", ch);
            return false;
        }

        if( IS_SET(ch->in_room->room_flag[1], ROOM_BLUEPRINT) ||
            IS_SET(ch->in_room->area->area_flags, ROOM_BLUEPRINT) )
        {
            context = ch->in_room->area;
            if (!parse_widevnum(arg, context, &room_wnum))
            {
                send_to_char("Invalid room vnum format.\n\r", ch);
                return false;
            }

            if( !rooms_in_same_section(pRoom->vnum, room_wnum.vnum) )
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
        WNUM room_wnum;
        AREA_DATA *context;

        if (arg[0] == '\0')
        {
            send_to_char("Syntax:  [direction] room [uid#vnum | #vnum | vnum]\n\r", ch);
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

        context = ch->in_room->area;
        
        if (!parse_widevnum(arg, context, &room_wnum))
        {
            send_to_char("Invalid room vnum format.\n\r", ch);
            return false;
        }

        pToRoom = room_wnum.pArea ?
            get_room_index(room_wnum.pArea, room_wnum.vnum) :
            get_room_index_global(room_wnum.vnum);

        if (!pToRoom)
        {
            send_to_char("REdit:  Cannot link to non-existant room.\n\r", ch);
            return false;
        }

        if( !rooms_in_same_section(pRoom->vnum, room_wnum.vnum) )
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
        // Store cross-area UID for serialization
        pRoom->exit[door]->wilds.area_uid = room_wnum.pArea ? room_wnum.pArea->uid : 0;
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
        if (arg[0] == '\0')
        {
            send_to_char("Syntax:  [direction] key [uid#vnum | #vnum | vnum]\n\r", ch);
            return false;
        }

        if (!pRoom->exit[door])
        {
            send_to_char("Exit doesn't exist.\n\r",ch);
            return false;
        }

        WNUM wnum;
        AREA_DATA *context = ch->in_room->area;

        if (!parse_widevnum(arg, context, &wnum))
        {
            send_to_char("Invalid vnum format.\n\r", ch);
            return false;
        }

        OBJ_INDEX_DATA *pObj = wnum.pArea ?
            get_obj_index(wnum.pArea, wnum.vnum) :
            get_obj_index_global(wnum.vnum);

        if (!pObj)
        {
            send_to_char("REdit:  Item doesn't exist.\n\r", ch);
            return false;
        }

        if (pObj->item_type != ITEM_KEY)
        {
            send_to_char("REdit:  Key doesn't exist.\n\r", ch);
            return false;
        }

        pRoom->exit[door]->door.lock.key_load.auid = wnum.pArea ? wnum.pArea->uid : 0;
        pRoom->exit[door]->door.lock.key_load.vnum = wnum.vnum;
        pRoom->exit[door]->door.lock.key_wnum = wnum;
        pRoom->exit[door]->door.rs_lock.key_load.auid = wnum.pArea ? wnum.pArea->uid : 0;
        pRoom->exit[door]->door.rs_lock.key_load.vnum = wnum.vnum;
        pRoom->exit[door]->door.rs_lock.key_wnum = wnum;

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
        if (!IS_LIGHT(obj)) break;

            if (LIGHT(obj)->duration == -1)
        sprintf(buf, "{B[  {Wv2{B]{G Light:{x  Infinite[-1]\n\r");
            else
        sprintf(buf, "{B[  {Wv2{B]{G Light:{x  [%d]\n\r", LIGHT(obj)->duration);

        add_buf(buffer, buf);
        break;

    case ITEM_WAND:
    case ITEM_STAFF:
        if (!IS_WAND(obj)) break;
            sprintf(buf,
        "{B[  {Wv1{B]{G Charges Total:{x  [%d]\n\r"
        "{B[  {Wv2{B]{G Charges Left:{x   [%d]\n\r",
        WAND(obj)->max_charges,
        WAND(obj)->charges);
        add_buf(buffer, buf);
        break;

    case ITEM_PORTAL:
        if (!IS_PORTAL(obj)) break;
        if( IS_SET(PORTAL(obj)->flags, GATE_DUNGEON) )
        {
            // DUNGEON portal
            sprintf(buf,
                "{B[  {Wv0{B]{G Charges:{x           [%d]\n\r"
                "{B[  {Wv1{B]{G Exit Flags:{x        %s\n\r"
                "{B[  {Wv2{B]{G Portal Flags:{x      %s\n\r"
                "{B[  {Wv3{B]{G Goes to (dungeon):{x [%ld]\n\r"
                "{B[  {Wv5{B]{G Goes to (floor):  {x [%ld]\n\r",
                PORTAL(obj)->charges,
                flag_string(portal_exit_flags, PORTAL(obj)->exit),
                flag_string(portal_flags, PORTAL(obj)->flags),
                PORTAL(obj)->params[0],
                PORTAL(obj)->params[1]);
        }
        else if( IS_SET(PORTAL(obj)->flags, GATE_AREARANDOM) || PORTAL(obj)->params[0] == -1 )
        {
            // AREARANDOM portal
            sprintf(buf,
                "{B[  {Wv0{B]{G Charges:{x        [%d]\n\r"
                "{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
                "{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
                "{B[  {Wv5{B]{G Goes to (area id):{x [%ld]\n\r",
                PORTAL(obj)->charges,
                flag_string(portal_exit_flags, PORTAL(obj)->exit),
                flag_string(portal_flags, PORTAL(obj)->flags),
                PORTAL(obj)->params[1]);
        }
        else if(PORTAL(obj)->params[0] > 0)
        {
            // STATIC portal
            sprintf(buf,
                "{B[  {Wv0{B]{G Charges:{x        [%d]\n\r"
                "{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
                "{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
                "{B[  {Wv3{B]{G Goes to (room):{x [%s]\n\r",
                PORTAL(obj)->charges,
                flag_string(portal_exit_flags, PORTAL(obj)->exit),
                flag_string(portal_flags, PORTAL(obj)->flags),
                widevnum_string(
                    PORTAL(obj)->params[4] > 0 ? get_area_index(PORTAL(obj)->params[4]) : NULL,
                    PORTAL(obj)->params[0], obj->area));
        }
        else
        {
            // WILDERNESS portal
            sprintf(buf,
                "{B[  {Wv0{B]{G Charges:{x        [%d]\n\r"
                "{B[  {Wv1{B]{G Exit Flags:{x     %s\n\r"
                "{B[  {Wv2{B]{G Portal Flags:{x   %s\n\r"
                "{B[  {Wv5{B]{G Goes to (map):{x  [%ld]\n\r"
                "{B[  {Wv6{B]{G Goes to (mapx):{x [%ld]\n\r"
                "{B[  {Wv7{B]{G Goes to (mapy):{x [%ld]\n\r",
                PORTAL(obj)->charges,
                flag_string(portal_exit_flags, PORTAL(obj)->exit),
                flag_string(portal_flags, PORTAL(obj)->flags),
                PORTAL(obj)->params[1], PORTAL(obj)->params[2], PORTAL(obj)->params[3]);
        }
        add_buf(buffer, buf);
        break;

    case ITEM_FURNITURE:
        if (!IS_FURNITURE(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Max people:{x      [%d]\n\r"
            "{B[  {Wv1{B]{G Max weight:{x      [%d]\n\r"
            "{B[  {Wv2{B]{G Furniture Flags:{x %s\n\r"
            "{B[  {Wv3{B]{G Heal bonus:{x      [%d]\n\r"
            "{B[  {Wv4{B]{G Mana bonus:{x      [%d]\n\r"
        "{B[  {Wv5{B]{G Move bonus:{x      [%d]\n\r",
            FURNITURE(obj)->max_people,
            FURNITURE(obj)->max_weight,
            flag_string(furniture_flags, FURNITURE(obj)->flags),
            FURNITURE(obj)->heal_rate,
            FURNITURE(obj)->mana_rate,
        FURNITURE(obj)->move_rate);
        add_buf(buffer, buf);
        break;

    case ITEM_HERB:
        if (!IS_HERB(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Type:{x            [%s]\n\r"
        "{B[  {Wv1{B]{G Healing:{x         [%d%%]\n\r"
        "{B[  {Wv2{B]{G Regenerative:{x    [%d%%]\n\r"
        "{B[  {Wv3{B]{G Refreshing:{x      [%d%%]\n\r"
        "{B[  {Wv4{B]{G Immunity:{x        [%s]\n\r"
        "{B[  {Wv5{B]{G Resistance:{x      [%s]\n\r"
        "{B[  {Wv6{B]{G Vulnerability:{x   [%s]\n\r"
        "{B[  {Wv7{B]{G Spell:{x           [%s]\n\r",
        herb_table[HERB(obj)->type].name,
        HERB(obj)->healing,
        HERB(obj)->regenerative,
        HERB(obj)->refreshing,
        flag_string(imm_flags, HERB(obj)->immunity),
        flag_string(res_flags, HERB(obj)->resistance),
        flag_string(vuln_flags, HERB(obj)->vulnerability),
        skill_table[HERB(obj)->spell].name);

        add_buf(buffer, buf);
        break;

    case ITEM_SCROLL:
    case ITEM_PILL:
        break;

    case ITEM_POTION:
        if (!IS_FLUID_CON(obj)) break;
        sprintf(buf,
                "{B[  {Wv5{B]{G Charges:{x                [%d]\n\r",
                FLUID_CON(obj)->capacity);
        add_buf(buffer, buf);
        break;

    case ITEM_TATTOO:
        if (!IS_TATTOO(obj)) break;
            sprintf(buf,
                    "{B[  {Wv0{B]{G Touches:{x                [%d]\n\r"
                    "{B[  {Wv1{B]{G Chance of Fading:{x       [%d]\n\r",
                    TATTOO(obj)->touches, TATTOO(obj)->fading_chance);
        add_buf(buffer, buf);
        break;

    case ITEM_INK:
        if (!IS_INK(obj)) break;
            sprintf(buf, "{B[  {Wv0{B]{G Type 1:{x                 [%s]\n\r", flag_string(catalyst_types, INK(obj)->types[0]));
        add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv1{B]{G Type 2:{x                 [%s]\n\r", flag_string(catalyst_types, INK(obj)->types[1]));
        add_buf(buffer, buf);
            sprintf(buf, "{B[  {Wv2{B]{G Type 3:{x                 [%s]\n\r", flag_string(catalyst_types, INK(obj)->types[2]));
        add_buf(buffer, buf);
        break;

    case ITEM_SEXTANT:
        if (!IS_SEXTANT(obj)) break;
            sprintf(buf,
        "{B[  {Wv0{B]{G Percentage of working:{x  [%d]\n\r",
        SEXTANT(obj)->accuracy);
        add_buf(buffer, buf);
        break;

    case ITEM_SEED:
        if (!IS_SEED(obj)) break;
            sprintf(buf,
        "{B[  {Wv0{B]{G Time before growth:{x     [%d]\n\r"
        "{B[  {Wv1{B]{G Turns into object vnum:{x [%ld]\n\r",
        SEED(obj)->growth_time,
        SEED(obj)->object_vnum);
        add_buf(buffer, buf);
        break;

    case ITEM_ARMOUR:
        if (!IS_ARMOR(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B] {GAc pierce       {x[%d]\n\r"
        "{B[  {Wv1{B] {GAc bash         {x[%d]\n\r"
        "{B[  {Wv2{B] {GAc slash        {x[%d]\n\r"
        "{B[  {Wv3{B] {GAc exotic       {x[%d]\n\r"
        "{B[  {Wv4{B] {GArmour strength  {x%s\n\r",
        ARMOR(obj)->protection[0],
        ARMOR(obj)->protection[1],
        ARMOR(obj)->protection[2],
        ARMOR(obj)->protection[3],
        armour_strength_table[ARMOR(obj)->armor_strength].name);
        add_buf(buffer, buf);
        break;

    case ITEM_ARTIFACT:
        break;

    case ITEM_RANGED_WEAPON:
        if (!IS_WEAPON(obj)) break;
            sprintf(buf, "{B[  {Wv0{B]{G Ranged Weapon class:{x   %s\n\r",
             flag_string(ranged_weapon_class, WEAPON(obj)->weapon_class));
        add_buf(buffer, buf);

        sprintf(buf, "{B[  {Wv1{B]{G Number of dice:{x [%d]\n\r", WEAPON(obj)->damage.number);
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv2{B]{G Type of dice:{x   [%d]\n\r", WEAPON(obj)->damage.size);
        add_buf(buffer, buf);

        sprintf(buf, "{B[  {Wv3{B]{G Projectile Distance:{x [%d]\n\r", WEAPON(obj)->range);
        add_buf(buffer, buf);
        break;

    case ITEM_WEAPON:
        if (!IS_WEAPON(obj)) break;
            sprintf(buf, "{B[  {Wv0{B]{G Weapon class:{x   %s\n\r",
             flag_string(weapon_class, WEAPON(obj)->weapon_class));
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv1{B]{G Number of dice:{x [%d]\n\r", WEAPON(obj)->damage.number);
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv2{B]{G Type of dice:{x   [%d]\n\r", WEAPON(obj)->damage.size);
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv3{B]{G Type:{x           %s\n\r",
            attack_table[WEAPON(obj)->damage_type].name);
        add_buf(buffer, buf);
         sprintf(buf, "{B[  {Wv4{B]{G Special type:{x   %s\n\r",
             flag_string(weapon_type2, WEAPON(obj)->flags));
        add_buf(buffer, buf);
        break;

    case ITEM_SHIP:
        if (!IS_SHIP_TYPE(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Weight:{x     [%d kg]\n\r"
        "{B[  {Wv1{B]{G Move delay:{x [%d]\n\r"
        "{B[  {Wv2{B]{G Min Crew:{x   [%d]\n\r"
        "{B[  {Wv3{B]{G Capacity:{x   [%d]\n\r"
        "{B[  {Wv4{B]{G Max Crew:{x   [%d]\n\r"
        "{B[  {Wv5{B]{G First Room:{x [%ld]\n\r"
        "{B[  {Wv6{B]{G Hit Points:{x [%d]\n\r"
        "{B[  {Wv7{B]{G Max Guns:{x   [%d]\n\r",
        SHIP_TYPE(obj)->weight,
        SHIP_TYPE(obj)->move_delay,
        SHIP_TYPE(obj)->min_crew,
        SHIP_TYPE(obj)->capacity,
        SHIP_TYPE(obj)->max_crew,
        SHIP_TYPE(obj)->first_room,
        SHIP_TYPE(obj)->hit_points,
        SHIP_TYPE(obj)->max_guns);
        add_buf(buffer, buf);
        break;

    case ITEM_CART:
        if (!IS_CART(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Weight:{x     [%d kg]\n\r"
        "{B[  {Wv1{B]{G Move delay:{x [%d]\n\r"
        "{B[  {Wv2{B]{G Strength:{x   [%d]\n\r"
        "{B[  {Wv3{B]{G Capacity:{x    [%d]\n\r"
        "{B[  {Wv4{B]{G Weight Mult:{x [%d]\n\r",
        CART(obj)->capacity,
        CART(obj)->move_delay,
        CART(obj)->min_strength,
        CART(obj)->max_items,
        CART(obj)->weight_multiplier);
        add_buf(buffer, buf);
        break;

    case ITEM_TRADE_TYPE:
        if (!IS_TRADE(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Trade Type:{x     [%s]\n\r",
        trade_table[ TRADE(obj)->trade_type ].name);
        add_buf(buffer, buf);
        break;

    case ITEM_CONTAINER:
        if (!IS_CONTAINER(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Weight:{x     [%d kg]\n\r"
        "{B[  {Wv1{B]{G Flags:{x      [%s]\n\r"
        "{B[  {Wv3{B]{G Capacity:{x    [%d]\n\r"
        "{B[  {Wv4{B]{G Weight Mult:{x [%d]\n\r",
        CONTAINER(obj)->max_weight,
        flag_string(container_flags, CONTAINER(obj)->flags),
                CONTAINER(obj)->max_items,
                CONTAINER(obj)->weight_multiplier);
        add_buf(buffer, buf);
        break;

    case ITEM_WEAPON_CONTAINER:
        if (!IS_WEAPON_CON(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Weight:{x     [%d kg]\n\r"
        "{B[  {Wv1{B]{G Weapon Type:{x [%s]\n\r"
        "{B[  {Wv3{B]{G Capacity:{x   [%d]\n\r"
        "{B[  {Wv4{B]{G Weight Mult:{x[%d]\n\r",
        WEAPON_CON(obj)->max_weight,
        flag_string(weapon_class, WEAPON_CON(obj)->weapon_type),
                WEAPON_CON(obj)->max_items,
                WEAPON_CON(obj)->weight_multiplier);
        add_buf(buffer, buf);
        break;

    case ITEM_DRINK_CON:
        if (!IS_FLUID_CON(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Liquid Total:{x [%d]\n\r"
            "{B[  {Wv1{B]{G Liquid Left:{x  [%d]\n\r"
            "{B[  {Wv2{B]{G Liquid:{x       %s\n\r"
            "{B[  {Wv3{B]{G Poisoned:{x     %s\n\r",
            FLUID_CON(obj)->capacity,
            FLUID_CON(obj)->amount,
            liquid_name(FLUID_CON(obj)->liquid),
            FLUID_CON(obj)->poison != 0 ? "Yes" : "No");
        add_buf(buffer, buf);
        break;

    case ITEM_FOUNTAIN:
        if (!IS_FLUID_CON(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Liquid Total:{x [%d]\n\r"
            "{B[  {Wv1{B]{G Liquid Left:{x  [%d]\n\r"
            "{B[  {Wv2{B]{G Liquid:{x     %s\n\r",
            FLUID_CON(obj)->capacity,
            FLUID_CON(obj)->amount,
            liquid_name(FLUID_CON(obj)->liquid));
        add_buf(buffer, buf);
        break;

    case ITEM_FOOD:
        if (!IS_FOOD(obj)) break;
        sprintf(buf,
        "{B[  {Wv0{B]{G Food hours:{x [%d]\n\r"
        "{B[  {Wv1{B]{G Full hours:{x [%d]\n\r"
        "{B[  {Wv3{B]{G Poisoned  :{x  %s\n\r"
        "{B[  {Wv4{B]{G Timer     :{x [%d]\n\r",
        FOOD(obj)->hunger,
        FOOD(obj)->full,
        FOOD(obj)->poison != 0 ? "Yes" : "No",
        FOOD(obj)->timer);
        add_buf(buffer, buf);
        break;

    case ITEM_MONEY:
        if (!IS_MONEY(obj)) break;
            sprintf(buf, "{B[  {Wv0{B]{G Silver:{x [%d]\n\r", MONEY(obj)->silver);
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv1{B]{G Gold:{x   [%d]\n\r", MONEY(obj)->gold);
        add_buf(buffer, buf);
        break;

        case ITEM_MIST:
        if (!IS_MIST(obj)) break;
        sprintf(buf, "{B[  {Wv0{B]{G %%HideObjects:{x    [%d]\n\r", MIST(obj)->obscure_objs);
        add_buf(buffer, buf);
        sprintf(buf, "{B[  {Wv1{B]{G %%HideCharacters:{x [%d]\n\r", MIST(obj)->obscure_mobs);
        add_buf(buffer, buf);
        break;

    case ITEM_CORPSE_NPC:
        if (!IS_CORPSE(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Type:{x           %s\n\r"
            "{B[  {Wv1{B]{G Resurrection:{x   %d%%\n\r"
            "{B[  {Wv2{B]{G Animation:{x      %d%%\n\r"
            "{B[  {Wv3{B]{G Body Parts:{x     %s\n\r"
            "{B[  {Wv5{B]{G Mobile (vnum):{x  %ld\n\r",
            flag_string(corpse_types, CORPSE(obj)->corpse_type),
            CORPSE(obj)->resurrection, CORPSE(obj)->animation,
            flag_string(part_flags, CORPSE(obj)->body_parts),
            CORPSE(obj)->mobile_vnum);
        add_buf(buffer, buf);
        break;

    case ITEM_INSTRUMENT:
        if (!IS_INSTRUMENT(obj)) break;
        sprintf(buf,
            "{B[  {Wv0{B]{G Type:{x            %s\n\r"
            "{B[  {Wv1{B]{G Flags:{x           %s\n\r"
            "{B[  {Wv2{B]{G Min Time Factor:{x %d%%\n\r"
            "{B[  {Wv3{B]{G Max Time Factor:{x %d%%\n\r",
            flag_string(instrument_types, INSTRUMENT(obj)->type),
            flag_string(instrument_flags, INSTRUMENT(obj)->flags),
            INSTRUMENT(obj)->beats_min, INSTRUMENT(obj)->beats_max);
        add_buf(buffer, buf);
        break;

    case ITEM_BOOK:
        if (!IS_BOOK(obj)) break;
        sprintf(buf,
        "{B[  {Wv1{B]{G Flags:{x      [%s]\n\r",
        flag_string(container_flags, BOOK(obj)->flags));
        add_buf(buffer, buf);
        break;

    case ITEM_TELESCOPE:
        if (!IS_TELESCOPE(obj)) break;
        if( TELESCOPE(obj)->heading < 0 )
            sprintf(buf,
                "{B[  {Wv0{B]{G Current Distance:{x  [%d]\n\r"
                "{B[  {Wv1{B]{G Minimum Distance:{x  [%d]\n\r"
                "{B[  {Wv2{B]{G Maximum Distance:{x  [%d]\n\r"
                "{B[  {Wv3{B]{G Bonusview Size:{x    [%d]\n\r"
                "{B[  {Wv4{B]{G Current Heading:{x   [none]\n\r",
                    TELESCOPE(obj)->distance,
                    TELESCOPE(obj)->min_distance,
                    TELESCOPE(obj)->max_distance,
                    TELESCOPE(obj)->bonus_view);
        else
            sprintf(buf,
                "{B[  {Wv0{B]{G Current Distance:{x  [%d]\n\r"
                "{B[  {Wv1{B]{G Minimum Distance:{x  [%d]\n\r"
                "{B[  {Wv2{B]{G Maximum Distance:{x  [%d]\n\r"
                "{B[  {Wv3{B]{G Bonusview Size:{x    [%d]\n\r"
                "{B[  {Wv4{B]{G Current Heading:{x   [%d]\n\r",
                    TELESCOPE(obj)->distance,
                    TELESCOPE(obj)->min_distance,
                    TELESCOPE(obj)->max_distance,
                    TELESCOPE(obj)->bonus_view,
                    TELESCOPE(obj)->heading);
        add_buf(buffer, buf);
        break;

    case ITEM_COMPASS:
        if (!IS_COMPASS(obj)) break;
        if( COMPASS(obj)->wuid > 0 )
        {
            WILDS_DATA *pWilds = get_wilds_from_uid(NULL, COMPASS(obj)->wuid);

            sprintf(buf,
                "{B[  {Wv0{B]{G Accuracy:{x      [%d]\n\r"
                "{B[  {Wv1{B]{G Wilderness:{x    [%ld] %s\n\r"
                "{B[  {Wv2{B]{G X Coordinate:{x  [%ld]\n\r"
                "{B[  {Wv3{B]{G Y Coordinate:{x  [%ld]\n\r",
                    COMPASS(obj)->accuracy,
                    COMPASS(obj)->wuid, (pWilds?pWilds->name:"???"),
                    COMPASS(obj)->x,
                    COMPASS(obj)->y);
        }
        else
        {
            sprintf(buf,
                "{B[  {Wv0{B]{G Accuracy:{x      [%d]\n\r"
                "{B[  {Wv1{B]{G Wilderness:{x    [none]\n\r",
                    COMPASS(obj)->accuracy);
        }
        add_buf(buffer, buf);
        break;

    case ITEM_BODY_PART:
        if (!IS_BODY_PART(obj)) break;
        {
            RACE_DATA *part_race = race_lookup_uid((int16_t)BODY_PART(obj)->race_uid);
            sprintf(buf,
                "{B[  {Wv0{B]{G Body Parts:{x    %s\n\r"
                "{B[  {Wv1{B]{G Race:{x          %s\n\r",
                flag_string(part_flags, BODY_PART(obj)->parts),
                part_race ? part_race->name : "unknown");
        }

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
        if (!IS_LIGHT(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_LIGHT");
            return false;
        case 2:
            send_to_char("HOURS OF LIGHT SET.\n\r\n\r", ch);
            LIGHT(pObj)->duration = atoi(argument);
            break;
        }
        break;

    case ITEM_WAND:
    case ITEM_STAFF:
        if (!IS_WAND(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_STAFF_WAND");
            return false;
        case 1:
            send_to_char("TOTAL NUMBER OF CHARGES SET.\n\r\n\r", ch);
            WAND(pObj)->max_charges = atoi(argument);
            break;
        case 2:
            send_to_char("CURRENT NUMBER OF CHARGES SET.\n\r\n\r", ch);
            WAND(pObj)->charges = atoi(argument);
            break;
        }
        break;

    case ITEM_SCROLL:
    case ITEM_PILL:
        break;

    case ITEM_POTION:
    if (!IS_FLUID_CON(pObj)) return false;
    switch (value_num)
    {
        default:
            do_help(ch, "ITEM_POTION");
            return false;
        case 5:
            send_to_char("TOTAL CHARGES SET\n\r\n\r", ch);
            FLUID_CON(pObj)->capacity = atoi(argument);
            break;
    }
    break;

    case ITEM_TATTOO:
        if (!IS_TATTOO(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_TATTOO");
            return false;
        case 0:
            send_to_char("TOUCHES SET.\n\r\n\r", ch);
            TATTOO(pObj)->touches = atoi(argument);
            break;
        case 1:
            send_to_char("FADING CHANCE SET.\n\r\n\r", ch);
            TATTOO(pObj)->fading_chance = atoi(argument);
            break;
        }
        break;

    case ITEM_INK:
        if (!IS_INK(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_INK");
            return false;
        case 0:
            send_to_char("TYPE 1 SET.\n\r\n\r", ch);
            INK(pObj)->types[0] = flag_lookup(argument,catalyst_types);
            break;
        case 1:
            send_to_char("TYPE 2 SET.\n\r\n\r", ch);
            INK(pObj)->types[1] = flag_lookup(argument,catalyst_types);
            break;
        case 2:
            send_to_char("TYPE 3 SET.\n\r\n\r", ch);
            INK(pObj)->types[2] = flag_lookup(argument,catalyst_types);
            break;
        }
        break;

    case ITEM_SEXTANT:
        if (!IS_SEXTANT(pObj)) return false;
        switch(value_num)
        {
        default:
            do_help(ch, "ITEM_SEXTANT");
            return false;
        case 0:
            send_to_char("Accuracy set.\n\r\n\r", ch);
            SEXTANT(pObj)->accuracy = atoi(argument);
            break;
        }
        break;

    case ITEM_SEED:
        if (!IS_SEED(pObj)) return false;
        switch(value_num)
        {
        default:
            do_help(ch, "ITEM_SEED");
            return false;
        case 0:
            send_to_char("Time set.\n\r\n\r", ch);
            SEED(pObj)->growth_time = atoi(argument);
            break;
        case 1:
            if (atoi(argument) != 0)
            {
                WNUM key_wnum = { NULL, 0 };
                OBJ_INDEX_DATA *key_obj;
                parse_widevnum(argument, ch->in_room ? ch->in_room->area : NULL, &key_wnum);
                key_obj = key_wnum.pArea ? get_obj_index(key_wnum.pArea, key_wnum.vnum) : get_obj_index_global(key_wnum.vnum);
                if (!key_obj)
                {
                    send_to_char("No such object exists.\n\r\n\r", ch);
                    return false;
                }
                SEED(pObj)->object_vnum = key_wnum.vnum;
            }
            else
                SEED(pObj)->object_vnum = 0;
            send_to_char("Vnum set.\n\r\n\r", ch);
            break;
        }
        break;

    case ITEM_ARMOUR:
        if (!IS_ARMOR(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_ARMOUR");
            return false;
        case 0:
            send_to_char("AC PIERCE SET.\n\r\n\r", ch);
            ARMOR(pObj)->protection[0] = atoi(argument);
            break;
        case 1:
            send_to_char("AC BASH SET.\n\r\n\r", ch);
            ARMOR(pObj)->protection[1] = atoi(argument);
            break;
        case 2:
            send_to_char("AC SLASH SET.\n\r\n\r", ch);
            ARMOR(pObj)->protection[2] = atoi(argument);
            break;
        case 3:
            send_to_char("AC EXOTIC SET.\n\r\n\r", ch);
            ARMOR(pObj)->protection[3] = atoi(argument);
            break;
        case 4:
            send_to_char("ARMOUR STRENGTH SET.\n\r", ch);
            send_to_char("ARMOUR CLASS SET.\n\r\n\r", ch);

            ARMOR(pObj)->armor_strength = get_armour_strength(argument);

            set_armour(pObj);

            break;
        }
        break;

    case ITEM_RANGED_WEAPON:
        if (!IS_WEAPON(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_RANGED_WEAPON");
            return false;
        case 0:
            send_to_char("RANGED WEAPON CLASS SET.\n\r\n\r", ch);
            WEAPON(pObj)->weapon_class = flag_value(ranged_weapon_class, argument);
            break;
        case 1:
            send_to_char("NUMBER OF DICE SET.\n\r\n\r", ch);
            WEAPON(pObj)->damage.number = atoi(argument);
            break;
        case 2:
            send_to_char("TYPE OF DICE SET.\n\r\n\r", ch);
            WEAPON(pObj)->damage.size = atoi(argument);
            break;
        case 3:
            send_to_char("PROJECTILE DISTANCE SET.\n\r\n\r", ch);
            WEAPON(pObj)->range = atoi(argument);
            break;
        }
        break;

    case ITEM_HERB:
        if (!IS_HERB(pObj)) return false;
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
                HERB(pObj)->type = i;
                send_to_char("HERB TYPE SET.\n\r", ch);
            }
            else
                send_to_char("Invalid herb type.\n\r", ch);
            break;
        case 1:
            send_to_char("HEALING RATE SET.\n\r", ch);
            HERB(pObj)->healing = atoi(argument);
            break;
        case 2:
            send_to_char("REGENERATIVE RATE SET.\n\r", ch);
            HERB(pObj)->regenerative = atoi(argument);
            break;
        case 3:
            send_to_char("REFRESHING RATE SET.\n\r", ch);
            HERB(pObj)->refreshing = atoi(argument);
            break;
        case 4:
            if ((i = flag_value(imm_flags, argument)) != NO_FLAG)
            {
                HERB(pObj)->immunity ^= i;
                send_to_char("IMMUNITY SET.\n\r", ch);
            }
            else
                send_to_char("Invalid immunity.\n\r", ch);
            break;
        case 5:
            if ((i = flag_value(res_flags, argument)) != NO_FLAG)
            {
                HERB(pObj)->resistance ^= i;
                send_to_char("RESISTANCE SET.\n\r", ch);
            }
            else
            send_to_char("Invalid resistance.\n\r", ch);
            break;
        case 6:
            if ((i = flag_value(vuln_flags, argument)) != NO_FLAG)
            {
                HERB(pObj)->vulnerability ^= i;
                send_to_char("VULNERABILITY SET.\n\r", ch);
            }
            else
                send_to_char("Invalid vulnerability.\n\r", ch);
            break;
        case 7:
            if ((i = skill_lookup(argument)) > 0 && skill_table[i].spell_fun != spell_null)
            {
                send_to_char("SPELL SET.\n\r", ch);
                HERB(pObj)->spell = i;
            }
            else if (i == 0)
            {
                send_to_char("SPELL RESET.\n\r", ch);
                HERB(pObj)->spell = 0;
            }
            else
                send_to_char("INVALID ARGUMENT.\n\r", ch);

            break;
        }

        break;

    case ITEM_WEAPON:
        if (!IS_WEAPON(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_WEAPON");
            return false;
        case 0:
            send_to_char("WEAPON CLASS SET.\n\r\n\r", ch);
            WEAPON(pObj)->weapon_class = flag_value(weapon_class, argument);
            break;
        case 1:
            send_to_char("NUMBER OF DICE SET.\n\r\n\r", ch);
            WEAPON(pObj)->damage.number = atoi(argument);
            break;
        case 2:
            send_to_char("TYPE OF DICE SET.\n\r\n\r", ch);
            WEAPON(pObj)->damage.size = atoi(argument);
            break;
        case 3:
            send_to_char("WEAPON TYPE SET.\n\r\n\r", ch);
            WEAPON(pObj)->damage_type = attack_lookup(argument);
            break;
        case 4:
            send_to_char("SPECIAL WEAPON TYPE TOGGLED.\n\r\n\r", ch);
            WEAPON(pObj)->flags ^= (flag_value(weapon_type2, argument) != NO_FLAG ? flag_value(weapon_type2, argument) : 0);
            break;
        }
        break;

    case ITEM_PORTAL:
        if (!IS_PORTAL(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_PORTAL");
            return false;
        case 0:
            send_to_char("CHARGES SET.\n\r\n\r", ch);
            PORTAL(pObj)->charges = atoi(argument);
            break;
        case 1:
            send_to_char("EXIT (PORTAL) FLAGS SET.\n\r\n\r", ch);
            PORTAL(pObj)->exit ^= (flag_value(portal_exit_flags, argument) != NO_FLAG ? flag_value(portal_exit_flags, argument) : 0);
            break;
        case 2:
            {
                send_to_char("PORTAL FLAGS SET.\n\r\n\r", ch);
                int flags = flag_value(portal_flags, argument);

                if( flags != NO_FLAG )
                {
                    PORTAL(pObj)->flags ^= flags;

                    if( IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) )
                    {
                        REMOVE_BIT(PORTAL(pObj)->flags, GATE_AREARANDOM);
                    }

                    if( IS_SET(flags, GATE_DUNGEON) && IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) )
                    {
                        PORTAL(pObj)->params[0] = 0;
                        PORTAL(pObj)->params[1] = 0;
                        PORTAL(pObj)->params[2] = 0;
                        PORTAL(pObj)->params[3] = 0;
                        PORTAL(pObj)->params[4] = 0;
                    }
                }
            }
            break;
        case 3:
            if( IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) )
            {
                DUNGEON_INDEX_DATA *dng = NULL;
                WNUM dng_wnum = { NULL, 0 };
                AREA_DATA *context = ch->in_room ? ch->in_room->area : pObj->area;

                if( parse_widevnum(argument, context, &dng_wnum) && dng_wnum.pArea )
                    dng = get_dungeon_index_for_area(dng_wnum.pArea, dng_wnum.vnum);
                else if( is_number(argument) )
                    dng = get_dungeon_index(atol(argument));

                if( !dng )
                {
                    send_to_char("THERE IS NO SUCH DUNGEON.\n\r\n\r", ch);
                    return false;
                }

                PORTAL(pObj)->params[0] = dng->vnum;
                PORTAL(pObj)->params[4] = dng->area ? dng->area->uid : 0;
                if( PORTAL(pObj)->params[1] < 1 )
                    PORTAL(pObj)->params[1] = 1;
                send_to_char("DUNGEON DESTINATION SET.\n\r\n\r", ch);
            }
            else if( !str_cmp(argument, "-1") )
            {
                PORTAL(pObj)->params[0] = -1;
                PORTAL(pObj)->params[4] = 0;
                send_to_char("AREA RANDOM DESTINATION SET.\n\r\n\r", ch);
            }
            else
            {
                AREA_DATA *context = ch->in_room ? ch->in_room->area : NULL;
                WNUM dest_wnum = { NULL, 0 };

                if( !parse_widevnum(argument, context, &dest_wnum)
                    || !dest_wnum.pArea
                    || !get_room_index(dest_wnum.pArea, dest_wnum.vnum) )
                {
                    send_to_char("NO SUCH DESTINATION ROOM.\n\r\n\r", ch);
                    return false;
                }

                PORTAL(pObj)->params[0] = dest_wnum.vnum;
                PORTAL(pObj)->params[1] = 0;
                PORTAL(pObj)->params[4] = dest_wnum.pArea->uid;
                send_to_char("EXIT DESTINATION SET.\n\r\n\r", ch);
            }
            break;
        case 5:
            if( IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) )
            {
                send_to_char("DUNGEON FLOOR SET.\n\r\n\r", ch);
            }
            else if( IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) || PORTAL(pObj)->params[0] == -1 )
            {
                send_to_char("AREA ID SET.\n\r\n\r", ch);
            }
            else if( !IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) )
            {
                send_to_char("WILDERNESS MAP UID SET.\n\r\n\r", ch);
            }
            PORTAL(pObj)->params[1] = atoi(argument);
            break;
        case 6:
            if( !IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) && !PORTAL(pObj)->params[0] )
            {
                send_to_char("WILDERNESS MAP X-COORDINATE SET.\n\r\n\r", ch);
                PORTAL(pObj)->params[2] = atoi(argument);
            }
            break;
        case 7:
            if( !IS_SET(PORTAL(pObj)->flags, GATE_DUNGEON) && !IS_SET(PORTAL(pObj)->flags, GATE_AREARANDOM) && !PORTAL(pObj)->params[0] )
            {
                send_to_char("WILDERNESS MAP Y-COORDINATE SET.\n\r\n\r", ch);
                PORTAL(pObj)->params[3] = atoi(argument);
            }
            break;
        }
        break;

    case ITEM_FURNITURE:
        if (!IS_FURNITURE(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_FURNITURE");
            return false;
        case 0:
            send_to_char("NUMBER OF PEOPLE SET.\n\r\n\r", ch);
            FURNITURE(pObj)->max_people = atoi(argument);
            break;
        case 1:
            send_to_char("MAX WEIGHT SET.\n\r\n\r", ch);
            FURNITURE(pObj)->max_weight = atoi(argument);
            break;
        case 2:
            send_to_char("FURNITURE FLAGS TOGGLED.\n\r\n\r", ch);
            FURNITURE(pObj)->flags ^= (flag_value(furniture_flags, argument) != NO_FLAG ? flag_value(furniture_flags, argument) : 0);
            break;
        case 3:
            send_to_char("HEAL BONUS SET.\n\r\n\r", ch);
            FURNITURE(pObj)->heal_rate = atoi(argument);
            break;
        case 4:
            send_to_char("MANA BONUS SET.\n\r\n\r", ch);
            FURNITURE(pObj)->mana_rate = atoi(argument);
            break;
        case 5:
            send_to_char("MOVE BONUS SET.\n\r\n\r", ch);
            FURNITURE(pObj)->move_rate = atoi(argument);
            break;
        }
        break;

    case ITEM_CART:
        if (!IS_CART(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_CART");
            return false;
        case 0:
            send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
            CART(pObj)->capacity = atoi(argument);
            break;
        case 1:
            send_to_char("DELAY SET.\n\r\n\r", ch);
            CART(pObj)->move_delay = atoi(argument);
            break;
        case 2:
            send_to_char("STRENGTH SET.\n\r\n\r", ch);
            CART(pObj)->min_strength = atoi(argument);
            break;
        case 3:
            send_to_char("CART MAX WEIGHT SET.\n\r", ch);
            CART(pObj)->max_items = atoi(argument);
            break;
        case 4:
            send_to_char("WEIGHT MULTIPLIER SET.\n\r\n\r", ch);
            CART(pObj)->weight_multiplier = atoi(argument);
            break;
        }
        break;

    case ITEM_TRADE_TYPE:
        if (!IS_TRADE(pObj)) return false;
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

            TRADE(pObj)->trade_type = i;
            send_to_char("Trade type set.\n\r", ch);
            break;
        }
        break;

    case ITEM_WEAPON_CONTAINER:
        if (!IS_WEAPON_CON(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_WEAPON_CONTAINER");
            return false;
        case 0:
            send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
            WEAPON_CON(pObj)->max_weight = atoi(argument);
            break;
        case 1:
            WEAPON_CON(pObj)->weapon_type = flag_value(weapon_class, argument);
            send_to_char("WEAPON TYPE SET.\n\r\n\r", ch);
            break;
        case 3:
            send_to_char("CONTAINER MAX ITEMS SET.\n\r", ch);
            WEAPON_CON(pObj)->max_items = atoi(argument);
            break;
        case 4:
            send_to_char("WEIGHT MULTIPLIER SET.\n\r\n\r", ch);
            WEAPON_CON(pObj)->weight_multiplier = atoi(argument);
            break;
        }
        break;

    case ITEM_CONTAINER:
        if (!IS_CONTAINER(pObj)) return false;
        switch (value_num)
        {
        int value;

        default:
            do_help(ch, "ITEM_CONTAINER");
            return false;
        case 0:
            send_to_char("WEIGHT CAPACITY SET.\n\r\n\r", ch);
            CONTAINER(pObj)->max_weight = atoi(argument);
            break;
        case 1:
            if ((value = flag_value(container_flags, argument)) != NO_FLAG)
                TOGGLE_BIT(CONTAINER(pObj)->flags, value);
            else
            {
                do_help (ch, "ITEM_CONTAINER");
                return false;
            }
            send_to_char("CONTAINER TYPE SET.\n\r\n\r", ch);
            break;
        case 3:
            if (atoi (argument) > 225 && ch->tot_level < MAX_LEVEL)
            {
                send_to_char("Sorry, that value is out of range.\n\r", ch);
                return false;
            }

            send_to_char("CONTAINER MAX ITEMS SET.\n\r", ch);
            CONTAINER(pObj)->max_items = atoi(argument);
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
            CONTAINER(pObj)->weight_multiplier = atoi(argument);
            break;
        }
        break;

    case ITEM_DRINK_CON:
        if (!IS_FLUID_CON(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_DRINK");
            return false;
        case 0:
            send_to_char("MAXIMUM AMOUT OF LIQUID HOURS SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->capacity = atoi(argument);
            break;
        case 1:
            send_to_char("CURRENT AMOUNT OF LIQUID HOURS SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->amount = atoi(argument);
            break;
        case 2:
            send_to_char("LIQUID TYPE SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->liquid = (liq_lookup(argument) != -1 ? liq_lookup(argument) : 0);
            break;
        case 3:
            send_to_char("POISON VALUE TOGGLED.\n\r\n\r", ch);
            FLUID_CON(pObj)->poison = (FLUID_CON(pObj)->poison == 0) ? 1 : 0;
            break;
        }
        break;

    case ITEM_FOUNTAIN:
        if (!IS_FLUID_CON(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_FOUNTAIN");
            return false;
        case 0:
            send_to_char("MAXIMUM AMOUT OF LIQUID HOURS SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->capacity = atoi(argument);
            break;
        case 1:
            send_to_char("CURRENT AMOUNT OF LIQUID HOURS SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->amount = atoi(argument);
            break;
        case 2:
            send_to_char("LIQUID TYPE SET.\n\r\n\r", ch);
            FLUID_CON(pObj)->liquid = (liq_lookup(argument) != -1 ? liq_lookup(argument) : 0);
            break;
        }
        break;

    case ITEM_FOOD:
        if (!IS_FOOD(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_FOOD");
            return false;
        case 0:
            send_to_char("HOURS OF FOOD SET.\n\r\n\r", ch);
            FOOD(pObj)->hunger = atoi(argument);
            break;
        case 1:
            send_to_char("HOURS OF FULL SET.\n\r\n\r", ch);
            FOOD(pObj)->full = atoi(argument);
            break;
        case 3:
            send_to_char("POISON VALUE TOGGLED.\n\r\n\r", ch);
            FOOD(pObj)->poison = (FOOD(pObj)->poison == 0) ? 1 : 0;
            break;
        case 4:
            send_to_char("TIMER TO DISAPPEAR SET.\n\r\n\r", ch);
            FOOD(pObj)->timer = atoi(argument);
            break;
        }
        break;

    case ITEM_MONEY:
        if (!IS_MONEY(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_MONEY");
            return false;
        case 0:
            send_to_char("SILVER AMOUNT SET.\n\r\n\r", ch);
            MONEY(pObj)->silver = atoi(argument);
            break;
        case 1:
            send_to_char("GOLD AMOUNT SET.\n\r\n\r", ch);
            MONEY(pObj)->gold = atoi(argument);
            break;
        }
        break;

    case ITEM_MIST:
        if (!IS_MIST(pObj)) return false;
        switch (value_num)
        {
        default:
            do_help(ch, "ITEM_MIST");
            return false;
        case 0:
            send_to_char("PERCENTAGE TO HIDE OBJECTS SET.\n\r", ch);
            MIST(pObj)->obscure_objs = atoi(argument);
            break;
        case 1:
            send_to_char("PERCENTAGE TO HIDE CHARACTERS SET.\n\r", ch);
            MIST(pObj)->obscure_mobs = atoi(argument);
            break;
        }
        break;

    case ITEM_CORPSE_NPC:
        if (!IS_CORPSE(pObj)) return false;
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
            CORPSE(pObj)->corpse_type = value;
            break;
        case 1:
            send_to_char("RESURRECTION CHANCE SET.\n\r", ch);
            CORPSE(pObj)->resurrection = atoi(argument);
            break;
        case 2:
            send_to_char("ANIMATION CHANCE SET.\n\r", ch);
            CORPSE(pObj)->animation = atoi(argument);
            break;
        case 3:
            if ((value = flag_value(part_flags, argument)) == NO_FLAG)
                return false;
            send_to_char("BODY PARTS SET.\n\r", ch);
            CORPSE(pObj)->body_parts = value;
            break;
        case 5:
            send_to_char("MOBILE INDEX VNUM SET.\n\r", ch);
            CORPSE(pObj)->mobile_vnum = atoi(argument);
            break;
        }
        break;

    case ITEM_INSTRUMENT:
        if (!IS_INSTRUMENT(pObj)) return false;
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
            INSTRUMENT(pObj)->type = value;
            break;
        case 1:
            if ((value = flag_value(instrument_flags, argument)) == NO_FLAG)
                return false;
            send_to_char("INSTRUMENT FLAGS TOGGLED.\n\r", ch);
            INSTRUMENT(pObj)->flags ^= value;
            break;
        case 2:
            value = atoi(argument);
            if( value < 1 || value > 5000)
            {
                send_to_char("Minimum scale factor for playtime can only be between 1% and 5000%.\n\r", ch);
                return false;
            }
            send_to_char("MINIMUM PLAYTIME SCALE FACTOR SET.\n\r", ch);
            INSTRUMENT(pObj)->beats_min = value;
            break;
        case 3:
            value = atoi(argument);
            if( value < 1 || value > 5000)
            {
                send_to_char("Maximum scale factor for playtime can only be between 1% and 5000%.\n\r", ch);
                return false;
            }
            send_to_char("MAXIMUM PLAYTIME SCALE FACTOR SET.\n\r", ch);
            INSTRUMENT(pObj)->beats_max = value;
            break;
        }
        break;

    case ITEM_BOOK:
        if (!IS_BOOK(pObj)) return false;
        switch (value_num)
        {
        int value;

        default:
            do_help(ch, "ITEM_BOOK");
            return false;
        case 1:
            if ((value = flag_value(container_flags, argument)) != NO_FLAG)
                TOGGLE_BIT(BOOK(pObj)->flags, value);
            else
            {
                do_help (ch, "ITEM_BOOK");
                return false;
            }
            send_to_char("BOOK (CONTAINER) FLAGS SET.\n\r\n\r", ch);
            break;
        }
        break;

    case ITEM_TELESCOPE:
        if (!IS_TELESCOPE(pObj)) return false;
        switch (value_num)
        {
        int value;

        default:
            do_help(ch, "ITEM_TELESCOPE");
            return false;
        case 0:
            value = atoi(argument);
            if( value < 0 || (value > 0 && value < TELESCOPE(pObj)->min_distance) || value > TELESCOPE(pObj)->max_distance )
            {
                sprintf(buf, "TELESCOPE DISTANCE must be 0(for collapsed), or from %d to %d.\n\r", TELESCOPE(pObj)->min_distance, TELESCOPE(pObj)->max_distance);
                send_to_char(buf, ch);
                return false;
            }
            TELESCOPE(pObj)->distance = value;
            send_to_char("TELESCOPE DISTANCE SET\n\r", ch);
            break;
        case 1:
            value = atoi(argument);
            if( value <= 0 )
            {
                send_to_char("TELESCOPE MINIMUM DISTANCE must be greater than zero.\n\r", ch);
                return false;
            }
            if( value > TELESCOPE(pObj)->max_distance )
            {
                sprintf(buf, "TELESCOPE MINIMUM DISTANCE must be less than or equal to %d.\n\r", TELESCOPE(pObj)->max_distance);
                send_to_char(buf, ch);
                return false;
            }
            TELESCOPE(pObj)->min_distance = value;
            send_to_char("TELESCOPE MINIMUM DISTANCE SET\n\r", ch);
            break;
        case 2:
            value = atoi(argument);
            if( value <= 0 )
            {
                send_to_char("TELESCOPE MAXIMUM DISTANCE must be greater than zero.\n\r", ch);
                return false;
            }
            if( value < TELESCOPE(pObj)->min_distance )
            {
                sprintf(buf, "TELESCOPE MAXIMUM DISTANCE must be greater than or equal to %d.\n\r", TELESCOPE(pObj)->min_distance);
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
            TELESCOPE(pObj)->max_distance = value;
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
            TELESCOPE(pObj)->bonus_view = value;
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

                TELESCOPE(pObj)->heading = value;
                send_to_char("TELESCOPE HEADING SET\n\r", ch);
            }
            else if( !str_cmp(argument, "none") || !str_cmp(argument, "clear") )
            {
                TELESCOPE(pObj)->heading = -1;
                send_to_char("TELESCOPE HEADING CLEARED\n\r", ch);
            }
            break;
        }
        break;
    case ITEM_COMPASS:
        if (!IS_COMPASS(pObj)) return false;
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
            COMPASS(pObj)->accuracy = atoi(argument);
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

                COMPASS(pObj)->wuid = wuid;
                COMPASS(pObj)->x = pWilds->map_size_x / 2;
                COMPASS(pObj)->y = pWilds->map_size_y / 2;

                send_to_char("WILDS set.\n\r", ch);
            }
            else
            {
                COMPASS(pObj)->wuid = 0;
                COMPASS(pObj)->x = -1;
                COMPASS(pObj)->y = -1;
                send_to_char("WILDS cleared.\n\r", ch);
            }
            break;
        case 2:
            if( !COMPASS(pObj)->wuid )
            {
                send_to_char("Please set the WILDS({Wv1{x) before assigning coordinates.\n\r", ch);
                return false;
            }

            pWilds = get_wilds_from_uid(NULL, COMPASS(pObj)->wuid);
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

            COMPASS(pObj)->x = value;
            send_to_char("X COORDINATE set.\n\r", ch);
            break;
        case 3:
            if( !COMPASS(pObj)->wuid )
            {
                send_to_char("Please set the WILDS({Wv1{x) before assigning coordinates.\n\r", ch);
                return false;
            }

            pWilds = get_wilds_from_uid(NULL, COMPASS(pObj)->wuid);
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

            COMPASS(pObj)->y = value;
            send_to_char("Y COORDINATE set.\n\r", ch);
            break;
        }
        break;

    case ITEM_BODY_PART:
        if (!IS_BODY_PART(pObj)) return false;
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
            BODY_PART(pObj)->parts ^= value;
            break;

        case 1:
            {
                RACE_DATA *race = race_lookup(argument);
                send_to_char("RACE SET\n\r", ch);
                BODY_PART(pObj)->race_uid = race ? race->uid : 0;
            }
            break;

        }
        break;
    }

    buffer = new_buf();
    oedit_show_type_data(pObj, buffer);
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);

    return true;
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

void show_liqlist(CHAR_DATA *ch)
{
    int liq;
    int count;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];

    buffer = new_buf();
    count = liquid_count();

    for (liq = 0; liq < count; liq++)
    {
    if ((liq % 21) == 0)
        add_buf(buffer,"Name                 Colour          Proof Full Thirst Food Ssize\n\r");

    sprintf(buf, "%-20s %-14s %5d %4d %6d %4d %5d\n\r",
        liquid_name(liq), liquid_color(liq),
        liquid_affect(liq, LIQ_AFF_PROOF), liquid_affect(liq, LIQ_AFF_FULL),
        liquid_affect(liq, LIQ_AFF_THIRST), liquid_affect(liq, LIQ_AFF_HUNGER),
        liquid_affect(liq, LIQ_AFF_SSIZE));
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
                room_set_sector_type(vroom, room_sector_type(pTerrain->template));
        }
    }

    iterator_stop(&it);
}



