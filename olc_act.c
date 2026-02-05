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
    {	"cmd",					STRUCT_FLAGS,		command_flags,				"Command Flags (CMDEdit)"},
    {	"condition",			STRUCT_FLAGS,		room_condition_flags,		"Room Condition types."	},
    {	"container",			STRUCT_FLAGS,		container_flags,			"Container status."	},
    {	"corpsetypes",			STRUCT_FLAGS,		corpse_types,				"Corpse types."	},
    {	"damageclass",			STRUCT_FLAGS,		damage_classes,				"Types of damages."},
    {	"do_func",				STRUCT_SPELLFUNC,	do_func_table,				"Do_ functions (CMDEdit)"},
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
    {   "log",					STRUCT_FLAGS,		log_flags,					"Log levels (CMDEdit)"},
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

        // Context: use current area for '#vnum' format, NULL for global lookup
        context = strchr(arg, '#') ? ch->in_room->area : NULL;
        
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
            // Parse widevnum for blueprint validation
            context = strchr(arg, '#') ? ch->in_room->area : NULL;
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

        // Context: use current area for '#vnum' format, NULL for global lookup
        context = strchr(arg, '#') ? ch->in_room->area : NULL;
        
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

if (!get_obj_index_global(value))
{
    send_to_char("REdit:  Item doesn't exist.\n\r", ch);
    return false;
}

if (get_obj_index_global(atol(argument))->item_type != ITEM_KEY)
        {
            send_to_char("REdit:  Item doesn't exist.\n\r", ch);
            return false;
        }

        if (get_obj_index_global(atol(argument))->item_type != ITEM_KEY)
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
                obj->value[4], get_obj_index_global(obj->value[4]) ? get_obj_index_global(obj->value[4])->short_descr : "none",
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
                obj->value[4], get_obj_index_global(obj->value[4]) ? get_obj_index_global(obj->value[4])->short_descr : "none",
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
                obj->value[4], get_obj_index_global(obj->value[4]) ? get_obj_index_global(obj->value[4])->short_descr : "none");
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
                obj->value[4], get_obj_index_global(obj->value[4]) ? get_obj_index_global(obj->value[4])->short_descr : "none",
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
                get_obj_index_global(obj->value[2])
                    ? get_obj_index_global(obj->value[2])->short_descr
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
                get_obj_index_global(obj->value[2])
                    ? get_obj_index_global(obj->value[2])->short_descr
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
        {
            RACE_DATA *part_race = race_lookup_uid((int16_t)obj->value[1]);
            sprintf(buf,
                "{B[  {Wv0{B]{G Body Parts:{x    %s\n\r"
                "{B[  {Wv1{B]{G Race:{x          %s\n\r",
                flag_string(part_flags, obj->value[0]),
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
                if (!get_obj_index_global(atoi(argument)))
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
                if (!get_obj_index_global(atoi(argument)))
                {
                    send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
                    return false;
                }

                if (get_obj_index_global(atoi(argument))->item_type != ITEM_KEY)
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
                if (!get_obj_index_global(atoi(argument)))
                {
                    send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
                    return false;
                }

                if (get_obj_index_global(atoi(argument))->item_type != ITEM_KEY)
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
                if (!get_obj_index_global(atoi(argument)))
                {
                    send_to_char("THERE IS NO SUCH ITEM.\n\r\n\r", ch);
                    return false;
                }

                if (get_obj_index_global(atoi(argument))->item_type != ITEM_KEY)
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
            {
                RACE_DATA *race = race_lookup(argument);
                send_to_char("RACE SET\n\r", ch);
                pObj->value[1] = race ? race->uid : 0;
            }
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



