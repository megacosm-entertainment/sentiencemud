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
#include "merc.h"
#include "tables.h"
#include "interp.h"
#include "olc.h"
#include "recycle.h"
#include "scripts.h"
#include "wilds.h"

extern GLOBAL_DATA gconfig;
/*
 *  * Local functions.
 *   */
AREA_DATA *get_area_data args ((long anum));
AREA_DATA *get_area_from_uid args ((long uid));

char *editor_name_table[] = {
	" ",
	"AEdit",
	"REdit",
	"OEdit",
	"MEdit",
	"MpEdit",
	"OpEdit",
	"RpEdit",
	"ShEdit",
	"HEdit",
	"TpEdit",
	"TEdit",
	"PEdit",
	"RSGEdit",
	"WEdit",
	"VLEdit",
	"BSEdit",
	"BPEdit",
	"DNGEdit",
	"ApEdit",
	"IpEdit",
	"DpEdit",
};

const struct editor_cmd_type editor_table[] =
{
    { "area",		do_aedit	},
    { "room",		do_redit	},
    { "object",		do_oedit	},
    { "mobile",		do_medit	},
    { "mprog",		do_mpedit	},
    { "oprog",		do_opedit	},
    { "rprog",		do_rpedit	},
    { "ship",		do_shedit	},
    { "help",		do_hedit	},
    { "token",		do_tedit	},
    { "tprog",		do_tpedit	},
    { "project",	do_pedit	},
    { "bpsect",		do_bsedit	},
    { "blueprint",	do_bpedit	},
    { "dungeon",	do_dngedit	},
    { "aprog",		do_apedit	},
    { "iprog",		do_ipedit	},
    { "dprog",		do_dpedit	},
    { "wilderness",    do_wedit	},
    { "vlink",		do_vledit	},
    { NULL,			0,			}
};


/* Interpreter Tables */
const struct olc_cmd_type aedit_table[] =
{
	{	"?",			show_help			},
	{	"addaprog",		aedit_addaprog		},
	{	"addtrade",		aedit_add_trade 	},
	{	"age",			aedit_age			},
	{	"airshipland",	aedit_airshipland	},
	{	"areawho",		aedit_areawho		},
	{	"builder",		aedit_builder		},
	{	"commands",		show_commands		},
	{	"comments",		aedit_comments		},
	{	"create",		aedit_create		},
	{	"credits",		aedit_credits		},
	{	"delaprog",		aedit_delaprog		},
	{	"description",	aedit_desc			},
	{	"filename",		aedit_file			},
	{	"flags",		aedit_flags			},
	{	"landx",		aedit_land_x		},
	{	"landy",		aedit_land_y		},
    {   "levels",       aedit_levels        },
	{	"name",			aedit_name			},
    {   "notes",        aedit_notes         },
	{	"open",			aedit_open			},
	{	"placetype",    aedit_placetype		},
	{	"postoffice",   aedit_postoffice	},
	{	"recall",		aedit_recall		},
	{	"removetrade",  aedit_remove_trade	},
	{	"repop",		aedit_repop			},
	{	"security",		aedit_security		},
	{	"settrade",		aedit_set_trade		},
	{	"show",			aedit_show			},
	{	"varclear",		aedit_varclear		},
	{	"varset",		aedit_varset		},
	{	"viewtrade",	aedit_view_trade	},
	{	"vnum",			aedit_vnum			},
	{	"wilds",		aedit_wilds			},
	{	"x",			aedit_x				},
	{	"y",			aedit_y				},
	{	NULL,			0,					}
};


const struct olc_cmd_type redit_table[] =
{
    {   "?",		show_help				},
    {   "addcdesc",	redit_addcdesc			},
    {	"addrprog",	redit_addrprog			},
    {   "commands",	show_commands			},
    {   "comments",  redit_comments			},
    {   "coords",	redit_coords			},
    {   "create",	redit_create			},
    {   "delcdesc",	redit_delcdesc			},
    {	"delrprog",	redit_delrprog			},
    {   "description",	redit_desc			},
    {   "dislink",	redit_dislink			},
    {   "down",		redit_down				},
    {   "east",		redit_east				},
    {   "ed",		redit_ed				},
    {   "editcdesc",	redit_editcdesc		},
    {   "heal",		redit_heal				},
    {   "locale",	redit_locale					},
    {	"mana",		redit_mana					},
    {   "move",		redit_move					},
    {	"mreset",	redit_mreset					},
    {   "name",		redit_name					},
    {   "north",	redit_north					},
    {   "northeast",	redit_northeast					},
    {   "northwest",	redit_northwest					},
    {	"oreset",	redit_oreset					},
    {   "owner",	redit_owner					},
    {	"persist",	redit_persist					},
    {	"room",		redit_room					},
    //{	"room2",	redit_room2					},
    {	"sector",	redit_sector					},
    {	"show",		redit_show					},
    {   "south",	redit_south					},
    {   "southeast",	redit_southeast					},
    {   "southwest",	redit_southwest					},
    {   "up",		redit_up					},
    {   "west",		redit_west					},
    {	"varset",	redit_varset					},
    {	"varclear",	redit_varclear					},
    {	NULL,		0,					}
};


const struct olc_cmd_type oedit_table[] =
{
	{ "?",				show_help				},
	{ "addaffect",		oedit_addaffect			},
	{ "addcatalyst",	oedit_addcatalyst		},
	{ "addimmune",		oedit_addimmune			},
	{ "addoprog",		oedit_addoprog			},
	{ "addskill",		oedit_addskill			},
	{ "addspell",		oedit_addspell			},
	{ "allowedfixed",	oedit_allowed_fixed		},
	{ "commands",		show_commands			},
	{ "comments",		oedit_comments			},
	{ "condition",		oedit_condition			},
	{ "cost",			oedit_cost				},
	{ "create",			oedit_create			},
	{ "delaffect",		oedit_delaffect			},
	{ "delcatalyst",	oedit_delcatalyst		},
	{ "delimmune",		oedit_delimmune			},
	{ "deloprog",		oedit_deloprog			},
	{ "delspell",		oedit_delspell			},
	{ "description",	oedit_desc				},
	{ "ed",				oedit_ed				},
	{ "extra",			oedit_extra				},
	//{ "extra2",			oedit_extra2			},
	//{ "extra3",			oedit_extra3			},
	//{ "extra4",			oedit_extra4			},
	{ "fragility",		oedit_fragility			},
	{ "level",			oedit_level				},
	{ "lock",			oedit_lock				},
	{ "long",			oedit_long				},
	{ "material",		oedit_material			},
	{ "name",			oedit_name				},
	{ "next",			oedit_next				},
	//{ "oupdate",		oedit_update			},
	{ "persist",		oedit_persist			},
	{ "prev",			oedit_prev				},
	{ "scriptkwd",		oedit_skeywds			},
	{ "short",			oedit_short				},
	{ "show",			oedit_show				},
	{ "sign",			oedit_sign				},
	{ "timer",			oedit_timer				},
	{ "type",			oedit_type				},
	{ "v0",				oedit_value0			},
	{ "v1",				oedit_value1			},
	{ "v2",				oedit_value2			},
	{ "v3",				oedit_value3			},
	{ "v4",				oedit_value4			},
	{ "v5",				oedit_value5			},
	{ "v6",				oedit_value6			},
	{ "v7",				oedit_value7			},
	{ "varclear",		oedit_varclear			},
	{ "varset",			oedit_varset			},
	{ "waypoints",		oedit_waypoints			},
	{ "wear",			oedit_wear				},
	{ "weight",			oedit_weight			},
	{ NULL,				0,						}
};


/* VIZZWILDS */
const struct olc_cmd_type wedit_table[] = {
/*  {   command        function    }, */

    {"commands", show_commands},
    {"create", wedit_create},
    {"delete", wedit_delete},
    {"show", wedit_show},
    {"name", wedit_name},
    {"terrain", wedit_terrain},
    {"vlink", wedit_vlink},

    {"?", show_help},

    {NULL, 0,}
};


const struct olc_cmd_type vledit_table[] = {
/*  {   command        function    }, */

    {"commands", show_commands},
    {"show", vledit_show},

    {"?", show_help},

    {NULL, 0,}
};


const struct olc_cmd_type medit_table[] =
{
    {   "?",		show_help	},
    {   "act",          medit_act       },
    //{   "act2",         medit_act2      },
    {   "addmprog",	medit_addmprog  },
    {   "affect",       medit_affect    },
    //{   "affect2",	medit_affect2   },
    {   "alignment",	medit_align	},
    {   "armour",        medit_ac        },
    {   "attacks",	medit_attacks   },
    {   "commands",	show_commands	},
    {   "comments", medit_comments  },
    {   "create",	medit_create	},
    {   "damdice",      medit_damdice   },
    {	"damtype",	medit_damtype	},
    {	"delmprog",	medit_delmprog	},
    {   "description",	medit_desc	},
//    {   "form",         medit_form      },
    {   "hitdice",      medit_hitdice   },
    {   "hitroll",      medit_hitroll   },
    {   "immune",       medit_immune    },
    {   "level",	medit_level	},
    {   "long",		medit_long	},
    {   "manadice",     medit_manadice  },
    {   "material",     medit_material  },
    {   "movedice",     medit_movedice  },
    {   "name",		medit_name	},
    {   "next", 	medit_next      },
    {   "off",          medit_off       },
    {   "owner",	medit_owner	},
    {   "part",         medit_part      },
    {	"persist",		medit_persist	},
    {   "position",     medit_position  },
    {   "prev", 	medit_prev      },
    {	"questor",		medit_questor	},
    {	"crew",		medit_crew	},
    {	"boss",		medit_boss	},
    {   "race",         medit_race      },
    {   "res",          medit_res       },
    {   "sex",          medit_sex       },
    {   "shop",		medit_shop	},
    {   "short",	medit_short	},
    {	"show",		medit_show	},
    {   "sign",		medit_sign	},
    {   "size",         medit_size      },
    {   "spec",		medit_spec	},
    {   "vuln",         medit_vuln      },
    {   "wealth",       medit_gold      },
    {	"scriptkwd",		medit_skeywds	},
    {	"varset",	medit_varset	},
    {	"varclear",	medit_varclear	},
    {	"corpsetype",	medit_corpsetype	},
    {	"corpsevnum",	medit_corpsevnum	},
    {	"zombievnum",	medit_zombievnum	},
    {	NULL,		0,		}
};


const struct olc_cmd_type hedit_table[] =
{
    {   "commands",	show_commands		},
    {	"show",		hedit_show		},
    {	"builder",	hedit_builder		},

    // Categories
    {	"addcategory",	hedit_addcat		},
    {   "description",	hedit_description 	},
    {   "name",		hedit_name		},
    {	"remcategory",	hedit_remcat		},
    {	"opencategory",	hedit_opencat		},
    {	"upcategory",	hedit_upcat		},
    {	"shiftcategory",hedit_shiftcat		},

    // Helpfiles
    {	"delete",	hedit_delete		},
    {   "edit",		hedit_edit		},
    {	"keyword",	hedit_keywords		},
    {   "level",	hedit_level		},
    {	"make",		hedit_make		},
    {	"move",		hedit_move		},
    {   "security",	hedit_security		},
    {   "text",		hedit_text		},
    {   "addtopic",	hedit_addtopic		},
    {   "remtopic",	hedit_remtopic		},
    {   NULL,		0,			}
};

/*
const struct olc_cmd_type shedit_table[] =
{
    {   "addmob",       shedit_addmob    	},
    {   "addwaypoint",  shedit_addwaypoint    	},
    {   "captain",      shedit_captain  	},
    {   "commands",	show_commands		},
    {   "coord",	shedit_coord  		},
    {   "create",	shedit_create		},
    {   "delmob",       shedit_delmob    	},
    {   "delwaypoint",  shedit_delwaypoint    	},
    {   "flag",         shedit_flag     	},
    {   "chance",       shedit_chance     	},
    {   "initial",      shedit_initial     	},
    {   "list",	        shedit_list		},
    {   "name",         shedit_name     	},
    {   "npc",          shedit_npc     	 	},
    {   "npcsub",	shedit_npcsub		},
    {	"show",		shedit_show		},
    {   "type",         shedit_type     	},
    {   NULL,		0,			}
};
*/

const struct olc_cmd_type tedit_table[] =
{
    {   "commands",	show_commands		},
    {	"?",		show_help		},
    {   "comments", tedit_comments  },
    {	"create",	tedit_create		},
    {	"show",		tedit_show		},
    {	"name",		tedit_name		},
    {	"type", 	tedit_type		},
    {	"flags",	tedit_flags		},
    {	"timer",	tedit_timer		},
    {	"ed",		tedit_ed		},
    {   "desc",		tedit_description	},
    {   "value",	tedit_value		},
    {	"valuename",	tedit_valuename		},
    {	"addtprog",	tedit_addtprog	},
    {	"deltprog",	tedit_deltprog	},
    {	"varset",	tedit_varset	},
    {	"varclear",	tedit_varclear	},
    {	NULL,		0			}
};


const struct olc_cmd_type pedit_table[] =
{
    {	"?",		show_help		},
    {	"create",	pedit_create		},
    {	"show",		pedit_show		},
    {	"name",		pedit_name		},
    {	"leader",	pedit_leader		},
    {	"area",		pedit_area		},
    {	"security",	pedit_security		},
    {   "summary",	pedit_summary		},
    {   "description",	pedit_description	},
    {	"pflag",	pedit_pflag		},
    {	"builder",	pedit_builder		},
    {	"completed",	pedit_completed		},
    {	NULL,		0			}
};


/* Executed from comm.c.  Minimizes compiling when changes are made. */
bool run_olc_editor(DESCRIPTOR_DATA *d)
{
    // No command should have a space, so no need for quoting.
    // No OLC command should start with ' or ".
    if (d->incomm[0] == '\'' || d->incomm[0] == '"') return false;
    switch (d->editor)
    {
	case ED_AREA:
	    aedit(d->character, d->incomm);
	    break;
	case ED_ROOM:
	    redit(d->character, d->incomm);
	    break;
	case ED_OBJECT:
	    oedit(d->character, d->incomm);
	    break;
	case ED_MOBILE:
	    medit(d->character, d->incomm);
	    break;
	case ED_MPCODE:
	    mpedit(d->character, d->incomm);
	    break;
	case ED_OPCODE:
	    opedit(d->character, d->incomm);
	    break;
	case ED_RPCODE:
	    rpedit(d->character, d->incomm);
	    break;
	case ED_SHIP:
	    shedit(d->character, d->incomm);
	    break;
	case ED_HELP:
	    hedit(d->character, d->incomm);
	    break;
	case ED_TOKEN:
	    tedit(d->character, d->incomm);
	    break;
	case ED_TPCODE:
	    tpedit(d->character, d->incomm);
	    break;
	case ED_PROJECT:
            pedit(d->character, d->incomm);
	    break;
/* VIZZWILDS */
	case ED_WILDS:
	    wedit(d->character, d->incomm);
	    break;
	case ED_VLINK:
		vledit(d->character, d->incomm);
		break;

	case ED_BPSECT:
		bsedit(d->character, d->incomm);
		break;

	case ED_BLUEPRINT:
		bpedit(d->character, d->incomm);
		break;

	case ED_DUNGEON:
		dngedit(d->character, d->incomm);
		break;

	case ED_APCODE:
	    apedit(d->character, d->incomm);
	    break;
	case ED_IPCODE:
	    ipedit(d->character, d->incomm);
	    break;
	case ED_DPCODE:
	    dpedit(d->character, d->incomm);
	    break;

	default:
	    return false;
    }
    return true;
}


// Return the edit name of character's editor (%o in prompt)
char *olc_ed_name(CHAR_DATA *ch)
{
	if(ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
		return editor_name_table[ch->desc->editor];

	return editor_name_table[0];
}


// Return the edit vnum of character's editor (%O in prompt)
char *olc_ed_vnum(CHAR_DATA *ch)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *pRoom;
    OBJ_INDEX_DATA *pObj;
    MOB_INDEX_DATA *pMob;
    SCRIPT_DATA *prog;
    PROJECT_DATA *project;
    HELP_DATA *help;
    SHIP_INDEX_DATA *pShip;
    TOKEN_INDEX_DATA *pTokenIndex;
    WILDS_DATA *pWilds;
    WILDS_VLINK *pVLink;
    BLUEPRINT_SECTION *bpsect;
    BLUEPRINT *blueprint;
    DUNGEON_INDEX_DATA *dungeon;
    static char buf[20];
    char buf2[MSL];

    buf[0] = '\0';
    switch (ch->desc->editor)
    {
	case ED_AREA:
	    pArea = (AREA_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", pArea ? pArea->anum : 0);
	    break;
	case ED_ROOM:
	    pRoom = ch->in_room;
	    sprintf(buf, "%ld", pRoom ? pRoom->vnum : 0);
	    break;
	case ED_OBJECT:
	    pObj = (OBJ_INDEX_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", pObj ? pObj->vnum : 0);
	    break;
	case ED_MOBILE:
	    pMob = (MOB_INDEX_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", pMob ? pMob->vnum : 0);
	    break;
	case ED_MPCODE:
	case ED_OPCODE:
	case ED_RPCODE:
	case ED_TPCODE:
	case ED_APCODE:
	case ED_IPCODE:
	case ED_DPCODE:
	    prog = (SCRIPT_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", (long int)(prog ? prog->vnum : 0));
	    break;
	case ED_HELP:
	    {
		HELP_CATEGORY *hCat;

		help = (HELP_DATA *)ch->desc->pEdit;

		if (help != NULL)
		{
		    hCat = help->hCat;
		    sprintf(buf, "{x%s", help ? help->keyword : "");

		    for (hCat = help->hCat; hCat->up != NULL; hCat = hCat->up)
		    {
			sprintf(buf2, "{W%s{B/", hCat->name);
			strcat(buf2, buf);
			strcpy(buf, buf2);
		    }

		    sprintf(buf2, "{B/{x");
		    strcat(buf2, buf);
		    strcpy(buf, buf2);
		}
		else
		{
		    HELP_CATEGORY *hCatTmp;

		    hCat = ch->desc->hCat;

		    sprintf(buf, "{W%s{B/{x", ch->desc->hCat->name);

		    for (hCatTmp = hCat->up; hCatTmp != NULL; hCatTmp = hCatTmp->up) {
			sprintf(buf2, "{W%s{B/{x", hCatTmp->name);
			strcat(buf2, buf);
			strcpy(buf, buf2);
		    }
		}
	    }
	    break;

	case ED_PROJECT:
	    project = (PROJECT_DATA *)ch->desc->pEdit;
	    if (project != NULL)
		sprintf(buf, "%s", project->name);
	    else
		sprintf(buf, "None");

	    break;

	case ED_SHIP:
	    pShip = (SHIP_INDEX_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", pShip ? pShip->vnum : 0);
	    break;

	case ED_TOKEN:
	    pTokenIndex = (TOKEN_INDEX_DATA *) ch->desc->pEdit;
	    sprintf(buf, "%ld", pTokenIndex ? pTokenIndex->vnum : 0);
	    break;

/* VIZZWILDS */
	case ED_WILDS:
	    pWilds = (WILDS_DATA *)ch->desc->pEdit;
	    sprintf(buf, "%ld", pWilds ? pWilds->uid : 0);
	    break;

	case ED_VLINK:
		pVLink = (WILDS_VLINK *) ch->desc->pEdit;
		sprintf(buf, "%ld", pVLink ? pVLink->uid : 0);
		break;

	case ED_BPSECT:
	    bpsect = (BLUEPRINT_SECTION *)ch->desc->pEdit;
	    sprintf(buf, "%ld", bpsect ? bpsect->vnum : 0);
	    break;

	case ED_BLUEPRINT:
	    blueprint = (BLUEPRINT *)ch->desc->pEdit;
	    sprintf(buf, "%ld", blueprint ? blueprint->vnum : 0);
	    break;

	case ED_DUNGEON:
	    dungeon = (DUNGEON_INDEX_DATA*)ch->desc->pEdit;
	    sprintf(buf, "%ld", dungeon ? dungeon->vnum : 0);
	    break;

	default:
	    sprintf(buf, " ");
	    break;
    }

    return buf;
}


/* Format up the commands from given table. */
void show_olc_cmds(CHAR_DATA *ch, const struct olc_cmd_type *olc_table)
{
    char buf  [ MAX_STRING_LENGTH ];
    char buf1 [ MAX_STRING_LENGTH ];
    int  cmd;
    int  col;

    buf1[0] = '\0';
    col = 0;
    for (cmd = 0; olc_table[cmd].name != NULL; cmd++)
    {
	sprintf(buf, "%-15.15s", olc_table[cmd].name);
	strcat(buf1, buf);
	if (++col % 5 == 0)
	    strcat(buf1, "\n\r");
    }

    if (col % 5 != 0)
	strcat(buf1, "\n\r");

    send_to_char(buf1, ch);
}


/* Display all OLC commands for your current editor */
bool show_commands(CHAR_DATA *ch, char *argument)
{
	switch (ch->desc->editor)
	{
	case ED_AREA:
		show_olc_cmds(ch, aedit_table);
		break;

	case ED_ROOM:
		show_olc_cmds(ch, redit_table);
		break;

	case ED_OBJECT:
		show_olc_cmds(ch, oedit_table);
		break;

	case ED_MOBILE:
		show_olc_cmds(ch, medit_table);
		break;

	case ED_MPCODE:
		show_olc_cmds(ch, mpedit_table);
		break;

	case ED_OPCODE:
		show_olc_cmds(ch, opedit_table);
		break;

	case ED_RPCODE:
		show_olc_cmds(ch, rpedit_table);
		break;

	case ED_HELP:
		show_olc_cmds(ch, hedit_table);
		break;

	case ED_SHIP:
		show_olc_cmds(ch, shedit_table);
		break;

	case ED_TOKEN:
		show_olc_cmds(ch, tedit_table);
		break;

	case ED_PROJECT:
		show_olc_cmds(ch, pedit_table);
		break;

	case ED_WILDS:
		show_olc_cmds (ch, wedit_table);
		break;

	case ED_VLINK:
		show_olc_cmds (ch, vledit_table);
		break;

	case ED_BPSECT:
		show_olc_cmds(ch, bsedit_table);
		break;

	case ED_BLUEPRINT:
		show_olc_cmds(ch, bpedit_table);
		break;

	case ED_DUNGEON:
		show_olc_cmds(ch, dngedit_table);
		break;

	case ED_APCODE:
		show_olc_cmds(ch, apedit_table);
		break;

	case ED_IPCODE:
		show_olc_cmds(ch, ipedit_table);
		break;

	case ED_DPCODE:
		show_olc_cmds(ch, dpedit_table);
		break;
	}

	return false;
}

// Given "anum" of an area, retrieve its area struct
AREA_DATA *get_area_data(long anum)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
        if (pArea->anum == anum)
            return pArea;
    }

    return 0;
}

// Given "uid" of an area, retrieve its area struct
AREA_DATA *get_area_from_uid (long uid)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea != NULL; pArea = pArea->next)
    {
        if (pArea->uid == uid)
            return pArea;
    }

    return 0;
}


/* Resets builder information on completion. */
bool edit_done(CHAR_DATA *ch)
{
    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit = NULL;
    ch->desc->hCat = NULL;
    ch->desc->editor = 0;
    return false;
}


bool has_access_area(CHAR_DATA *ch, AREA_DATA *area)
{
    if (ch->tot_level == MAX_LEVEL)
	return true;

    if (!IS_BUILDER(ch, area))
	return false;

    return true;
}


// The interpreters are below
void aedit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int  cmd;

    EDIT_AREA(ch, pArea);
    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (get_trust(ch) < MAX_LEVEL - 1)
    {
	send_to_char("AEdit:  Insufficient security to edit area - action logged.\n\r", ch);
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
	aedit_show(ch, argument);
	return;
    }

    for (cmd = 0; aedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, aedit_table[cmd].name))
	{
	    if ((*aedit_table[cmd].olc_fun) (ch, argument))
	    {
		SET_BIT(pArea->area_flags, AREA_CHANGED);
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}


void redit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    ROOM_INDEX_DATA *pRoom;
    char arg[MAX_STRING_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int  cmd;

    EDIT_ROOM_SIMPLE(ch, pRoom);
    pArea = pRoom->area;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!IS_BUILDER(ch, pArea))
    {
        send_to_char("REdit:  Insufficient security to edit room - action logged.\n\r", ch);
	edit_done(ch);
	return;
    }

    if (!str_cmp(command, "done"))
    {
	edit_done(ch);
	return;
    }

    if(room_is_clone(pRoom)) return;

    ch->pcdata->immortal->last_olc_command = current_time;
    if (command[0] == '\0')
    {
	redit_show(ch, argument);
	return;
    }

    for (cmd = 0; redit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, redit_table[cmd].name))
	{
	    if ((*redit_table[cmd].olc_fun) (ch, argument))
	    {
		SET_BIT(pArea->area_flags, AREA_CHANGED);
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}


void oedit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    OBJ_INDEX_DATA *pObj;
    char arg[MAX_STRING_LENGTH];
    char command[MAX_INPUT_LENGTH];
    int  cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    EDIT_OBJ(ch, pObj);
    pArea = pObj->area;

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("OEdit: Insufficient security to edit object - action logged.\n\r", ch);
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
	oedit_show(ch, argument);
	return;
    }

    for (cmd = 0; oedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, oedit_table[cmd].name))
	{
	    if ((*oedit_table[cmd].olc_fun) (ch, argument))
	    {
		SET_BIT(pArea->area_flags, AREA_CHANGED);
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}


void medit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    MOB_INDEX_DATA *pMob;
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_STRING_LENGTH];
    int  cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    EDIT_MOB(ch, pMob);
    pArea = pMob->area;

    if (pArea == NULL)
    {
	bug("medit: pArea was null!", 0);
	return;
    }

    if (!IS_BUILDER(ch, pArea))
    {
	send_to_char("MEdit: Insufficient security to edit area - action logged.\n\r", ch);
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
        medit_show(ch, argument);
        return;
    }

    for (cmd = 0; medit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, medit_table[cmd].name))
	{
	    if ((*medit_table[cmd].olc_fun) (ch, argument))
	    {
		SET_BIT(pArea->area_flags, AREA_CHANGED);
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}

void tedit(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index;
    AREA_DATA *area;
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int cmd;

    EDIT_TOKEN(ch, token_index);

    area = token_index->area;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (ch->tot_level < LEVEL_IMMORTAL)
    {
	send_to_char("TEdit:  Insufficient security - action logged.\n\r", ch);
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
	tedit_show(ch, argument);
	return;
    }

    for (cmd = 0; tedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, tedit_table[cmd].name))
	{
	    if ((*tedit_table[cmd].olc_fun) (ch, argument))
	    {
		SET_BIT(area->area_flags, AREA_CHANGED);
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}


void pedit(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int  cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (get_trust(ch) < MAX_LEVEL)
    {
	send_to_char("PEdit:  Insufficient security to edit projects - action logged.\n\r", ch);
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
	pedit_show(ch, argument);
	return;
    }

    for (cmd = 0; pedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, pedit_table[cmd].name))
	{
	    if ((*pedit_table[cmd].olc_fun) (ch, argument))
	    {
		projects_changed = true;
		return;
	    }
	    else
		return;
	}
    }

    interpret(ch, arg);
}


// Entry points for all editors are below
void do_olc(CHAR_DATA *ch, char *argument)
{
    char command[MAX_INPUT_LENGTH];
    int  cmd;

    if (IS_NPC(ch))
    	return;

    argument = one_argument(argument, command);

    if (command[0] == '\0')
    {
        do_help(ch, "olc");
        return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    /* Search Table and Dispatch Command. */
    for (cmd = 0; editor_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, editor_table[cmd].name))
	{
	    ch->pcdata->immortal->last_olc_command = current_time;
	    (*editor_table[cmd].do_fun) (ch, argument);
	    return;
	}
    }

    /* Invalid command, send help. */
    do_help(ch, "olc");
}

void do_tedit(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index = NULL;
    int value;
    char arg[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
    	return;

    argument = one_argument(argument,arg);

    if (is_number(arg))
    {
	value = atoi(arg);

	if ((token_index = get_token_index(value)) == NULL)
	{
	    send_to_char("That token vnum does not exist.\n\r", ch);
	    return;
	}
    }
    else
    if (!str_cmp(arg, "create"))
    {
	if (tedit_create(ch, argument))
	    ch->desc->editor = ED_TOKEN;

	return;
    }
    else
    {
	send_to_char(
	"Syntax: tedit <vnum>\n\r"
	"        tedit create <vnum>\n\r", ch);
	return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit = (void *)token_index;
    ch->desc->editor = ED_TOKEN;
}


void do_aedit(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *pArea;
    int value;
    char arg[MAX_STRING_LENGTH];

    if (get_trust(ch) < MAX_LEVEL - 1)
    {
	send_to_char("AEdit : Insufficient security to edit area - action logged.\n\r", ch);
	return;
    }

    if (IS_NPC(ch))
    	return;

    pArea	= ch->in_room->area;
    argument	= one_argument(argument,arg);

    if (is_number(arg))
    {
	value = atoi(arg);
	if (!(pArea = get_area_data(value)))
	{
	    send_to_char("That area vnum does not exist.\n\r", ch);
	    return;
	}
    }
    else
    if (arg[0] != '\0' && (pArea = find_area_kwd(arg)) == NULL
    && str_cmp(arg, "create"))
    {
	send_to_char("Area not found.\n\r", ch);
	return;
    }
    else
    if (!str_cmp(arg, "create"))
    {
	if (ch->pcdata->security < 9 || get_trust(ch) < 154)
	{
	    send_to_char("AEdit : Insufficient security to edit area - action logged.\n\r", ch);
	    return;
	}

	aedit_create(ch, "");
	ch->desc->editor = ED_AREA;
	return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit = (void *)pArea;
    ch->desc->editor = ED_AREA;
}


void do_redit(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom;
    char arg1[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
    	return;

    argument = one_argument(argument, arg1);

    pRoom = ch->in_room;

    if (!str_cmp(arg1, "reset"))
    {
	if (!has_access_area(ch, pRoom->area))
	{
	    send_to_char("Insufficient security to reset - action logged.\n\r" , ch);
	    return;
	}

	reset_room(pRoom);
	send_to_char("Room reset.\n\r", ch);
	return;
    }
    else
    if (!str_cmp(arg1, "create"))
    {
	if (redit_create(ch, argument))
	{
	    ch->desc->editor = ED_ROOM;
	    char_from_room(ch);
	    char_to_room(ch, ch->desc->pEdit);
	    SET_BIT(((ROOM_INDEX_DATA *)ch->desc->pEdit)->area->area_flags, AREA_CHANGED);
	}

	return;
    }
    else if (!IS_NULLSTR(arg1))	/* redit <vnum> */
    {
	pRoom = get_room_index(atol(arg1));

	if (!pRoom)
	{
	    send_to_char("REdit : Room does not exist.\n\r", ch);
	    return;
	}

	if (!IS_BUILDER(ch, pRoom->area))
	{
	    send_to_char("REdit : Insufficient security to edit room - action logged.\n\r", ch);
	    return;
	}

	char_from_room(ch);
	char_to_room(ch, pRoom);
    } else if(pRoom && IS_SET(pRoom->room_flag[1],ROOM_VIRTUAL_ROOM)) {
	send_to_char("REdit : Virtual rooms may not be editted.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, pRoom->area))
    {
    	send_to_char("REdit : Insuficient security to edit room - action logged.\n\r", ch);
    	return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit	= (void *) pRoom;
    ch->desc->editor	= ED_ROOM;
}


void do_oedit(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObj;
    AREA_DATA *pArea;
    char arg1[MAX_STRING_LENGTH];
    long value;

    if (IS_NPC(ch))
	return;

    argument = one_argument(argument, arg1);

    if (is_number(arg1))
    {
	value = atol(arg1);
	if (!(pObj = get_obj_index(value)))
	{
	    send_to_char("OEdit:  That vnum does not exist.\n\r", ch);
	    return;
	}

	if (!has_access_area(ch, pObj->area))
	{
	    send_to_char("Insufficient security to edit object - action logged.\n\r" , ch);
	    return;
	}

	ch->pcdata->immortal->last_olc_command = current_time;
	ch->desc->pEdit = (void *)pObj;
	ch->desc->editor = ED_OBJECT;
    }
    else
    {
	if (!str_cmp(arg1, "create"))
	{
	    value = atol(argument);

	    if (argument[0] != '\0')
	    {
		pArea = get_vnum_area(value);

		if (!pArea)
		{
		    send_to_char("OEdit:  That vnum is not assigned an area.\n\r", ch);
		    return;
		}

		if (!has_access_area(ch, pArea))
		{
		    send_to_char("Insufficient security to edit object - action logged.\n\r" , ch);
		    return;
		}
	    }

	    if (oedit_create(ch, argument))
		ch->desc->editor = ED_OBJECT;
	}
    }
}


void do_medit(CHAR_DATA *ch, char *argument)
{
    MOB_INDEX_DATA *pMob;
    AREA_DATA *pArea;
    long value;
    char arg1[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg1);

    if (IS_NPC(ch))
    	return;

    if (is_number(arg1))
    {
	value = atol(arg1);
	if (!(pMob = get_mob_index(value)))
	{
	    send_to_char("MEdit:  That vnum does not exist.\n\r", ch);
	    return;
	}

	if (!has_access_area(ch, pMob->area))
	{
	    send_to_char("Insufficient security to edit mob - action logged.\n\r" , ch);
	    return;
	}

	ch->pcdata->immortal->last_olc_command = current_time;
	ch->desc->pEdit = (void *)pMob;
	ch->desc->editor = ED_MOBILE;
	return;
    }
    else
    {
	if (!str_cmp(arg1, "create"))
	{
	    value = atol(argument);

	    if (argument[0] != '\0') {
		pArea = get_vnum_area(value);

		if (!pArea)
		{
		    send_to_char("MEdit:  That vnum is not assigned an area.\n\r", ch);
		    return;
		}

		if (!IS_BUILDER(ch, pArea))
		{
		    send_to_char("Insufficient security to edit mob - action logged.\n\r" , ch);
		    return;
		}
	    }

	    if (medit_create(ch, argument))
	    {
		//SET_BIT(pArea->area_flags, AREA_CHANGED);
		ch->desc->editor = ED_MOBILE;
	    }
	}

	return;
    }

    send_to_char("MEdit:  There is no default mobile to edit.\n\r", ch);
}


void do_pedit(CHAR_DATA *ch, char *argument)
{
    PROJECT_DATA *project;
    int value;
    int i;
    char arg[MAX_STRING_LENGTH];

    if (get_trust(ch) < MAX_LEVEL)
    {
	send_to_char("PEdit: Insufficient security to edit projects - action logged.\n\r", ch);
	return;
    }

    if (IS_NPC(ch))
    	return;

    argument = one_argument(argument,arg);
    if (arg[0] == '\0') {
	send_to_char("Syntax: pedit <project #|project name>\n\r",  ch);
	return;
    }

    if (is_number(arg))
    {
	value = atoi(arg);
	for (project = project_list, i = 0; project != NULL; project = project->next, i++) {
	    if (i == value)
		break;
	}

	if (project == NULL) {
	    send_to_char("Project number not found.\n\r", ch);
	    return;
	}
    }
    else
    if (arg[0] != '\0' && str_cmp(arg, "create"))
    {
	for (project = project_list; project != NULL; project = project->next) {
	    if (!str_infix(arg, project->name))
		break;
	}

	if (project == NULL) {
	    send_to_char("Project not found.\n\r", ch);
	    return;
	}
    }
    else
    if (!str_cmp(arg, "create"))
    {
	if (get_trust(ch) < MAX_LEVEL)
	{
	    send_to_char("PEdit: Insufficient security to create project - action logged.\n\r", ch);
	    return;
	}

	pedit_create(ch, "");
	ch->desc->editor = ED_PROJECT;
	return;
    }
    else {
	send_to_char("Syntax: pedit <project #|project name>\n\r",  ch);
	return;
    }

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->pEdit = (void *)project;
    ch->desc->editor = ED_PROJECT;
}

/* VIZZWILDS */
/* Wilds Interpreter, called by do_wedit. */
void wedit (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    char arg[MSL];
    char command[MIL];
    int cmd;

    EDIT_WILDS (ch, pWilds);

    if (!pWilds)
    {
        plogf("olc.c, wedit(): pWilds is NULL");
        edit_done (ch);
        return;
    }

    pArea = pWilds->pArea;
    smash_tilde (argument);
    strcpy (arg, argument);
    argument = one_argument (argument, command);

    if (!IS_BUILDER (ch, pArea))
    {
        send_to_char ("WEdit:  You need to have access to that area to modify its wilds.\n\r",
                      ch);
        edit_done (ch);
        return;
    }

    if (!str_cmp (command, "done"))
    {
        edit_done (ch);
        return;
    }

    if (command[0] == '\0')
    {
        wedit_show (ch, argument);
        return;
    }

    /* Search Table and Dispatch Command. */
    for (cmd = 0; wedit_table[cmd].name != NULL; cmd++)
    {
        if (!str_prefix (command, wedit_table[cmd].name))
        {
            if ((*wedit_table[cmd].olc_fun) (ch, argument))
            {
                SET_BIT (pArea->area_flags, AREA_CHANGED);
                return;
            }
            else
                return;
        }
    }

    /* Default to Standard Interpreter. */
    interpret (ch, arg);
    return;
}

void do_wedit (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea = NULL;
    WILDS_DATA *pWilds = NULL,
	       *pLastWilds = NULL;
    WILDS_TERRAIN *pTerrain = NULL;
    char arg1[MIL],
	 arg2[MIL],
	 arg3[MIL],
	 *pMap = NULL,
	 *pStaticMap = NULL;
    long value = 0,
	 lScount = 0,
	 lMapsize = 0;
    int size_x = 0,
        size_y = 0;

    // Mobs don't get access to olc commands.
    if (IS_NPC (ch))
        return;

    // Strip out first argument, if there is one.
    argument = one_argument(argument, arg1);

    // Default to using char's in_wilds pointer, even if it is NULL.
    pWilds = ch->in_wilds;

    // First, check for no arguments to allow quick edit of a current wilds location.
    if (IS_NULLSTR(arg1) && pWilds == NULL)
    {
        send_to_char("Wedit Usage:\n\r", ch);
        send_to_char("               wedit                        - defaults to editing the wilds you are in.\n\r", ch);
        send_to_char("               wedit [wilds uid]            - edit wilds via uid\n\r", ch);
        send_to_char("               wedit create <sizex> <sizey> - create new wilds of specified dimensions\n\r", ch);
        return;
    }
    else
    // wedit <uid>
    if (is_number (arg1))
    {
        value = atol (arg1);

        // Find wilds if it exists
        if ((pWilds = get_wilds_from_uid (NULL, value)) == NULL)
        {
            send_to_char ("Wedit: That wilds index does not exist.\n\r", ch);
            return;
        }

        // Wilds found, but does user have access to the area for OLC edit?
        if (!has_access_area(ch, pWilds->pArea))
        {
            send_to_char("Wedit: Insufficient security to edit wilds - action logged.\n\r", ch);
            return;
        }

        ch->desc->pEdit = (void *) pWilds;
        ch->desc->editor = ED_WILDS;
    }
    else
    {
        if (!str_cmp(arg1, "create"))
        {
            if (IS_NULLSTR(argument))
            {
                send_to_char("Wedit Usage:\n\r", ch);
                send_to_char("               wedit create <sizex> <sizey> - create new wilds of specified dimensions\n\r", ch);
                return;
            }
            else
            {
                argument = one_argument(argument, arg2);
                one_argument(argument, arg3);

                if (is_number(arg2) && is_number(arg3))
                {
                    size_x = atoi(arg2);
                    size_y = atoi(arg3);
		    pArea = ch->in_room->area;
                }

                if (!has_access_area(ch, pArea))
                {
                    send_to_char("Insufficient securiy to edit area - action logged.\n\r", ch);
                    return;
                }
            }

            // Create new wilds and slot it into the area structure
            pWilds = new_wilds();
            pWilds->pArea = ch->in_room->area;
            pWilds->uid = gconfig.next_wilds_uid++;
            gconfig_write();
            pWilds->name = str_dup("New Wilds");
            pWilds->map_size_x = size_x;
            pWilds->map_size_y = size_y;
            lMapsize = pWilds->map_size_x * pWilds->map_size_y;
            pWilds->staticmap = calloc(sizeof(char), lMapsize);
            pWilds->map = calloc(sizeof(char), lMapsize);

            pMap = pWilds->map;
            pStaticMap = pWilds->staticmap;

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

                plogf("olc.c, do_wedit(): Adding Wilds to existing linked-list.");
                pLastWilds->next = pWilds;
            }
            else
            {
                plogf("olc.c, do_wedit(): Adding first Wilds to linked-list.");
                pArea->wilds = pWilds;
            }

            send_to_char("Wedit: New wilds region created.\n\r", ch);
            pTerrain = new_terrain(pWilds);
            pTerrain->mapchar = 'S';
            pTerrain->showchar = str_dup("{B~");
            pWilds->pTerrain = pTerrain;
            send_to_char("Wedit: Default wilds terrain mapping completed.\n\r", ch);

        }

    }

    printf_to_char(ch, "{x[{WWedit{x] Editing Wilds.\n\r");
    ch->desc->pEdit = (void *) pWilds;
    ch->desc->editor = ED_WILDS;
}


/* Wilds Interpreter, called by do_vledit. */
void vledit (CHAR_DATA * ch, char *argument)
{
    AREA_DATA *pArea;
    WILDS_DATA *pWilds;
    WILDS_VLINK *pVLink;
    char arg[MSL];
    char command[MIL];
    int cmd;

    EDIT_VLINK (ch, pVLink);

    if (!pVLink)
    {
        plogf("olc.c, vledit(): pVLink is NULL");
        edit_done (ch);
        return;
    }

    pWilds = pVLink->pWilds;
    pArea = pWilds->pArea;
    smash_tilde (argument);
    strcpy (arg, argument);
    argument = one_argument (argument, command);

    if (!IS_BUILDER (ch, pArea))
    {
        send_to_char ("WEdit:  You need to have access to that area to modify its vlinks.\n\r",
                      ch);
        edit_done (ch);
        return;
    }

    if (!str_cmp (command, "done"))
    {
        edit_done (ch);
        return;
    }

    if (command[0] == '\0')
    {
        vledit_show (ch, argument);
        return;
    }

    /* Search Table and Dispatch Command. */
    for (cmd = 0; vledit_table[cmd].name != NULL; cmd++)
    {
        if (!str_prefix (command, vledit_table[cmd].name))
        {
            if ((*vledit_table[cmd].olc_fun) (ch, argument))
            {
                SET_BIT (pArea->area_flags, AREA_CHANGED);
                return;
            }
            else
                return;
        }
    }

    /* Default to Standard Interpreter. */
    interpret (ch, arg);
    return;
}


void do_vledit (CHAR_DATA * ch, char *argument)
{
    WILDS_VLINK *pVLink = NULL;
    char arg1[MSL];
    char buf[MSL];
    int value = 0;
    bool found = false;

/* Vizz - Mob don't get access to olc commands. */
    if (IS_NPC (ch))
        return;

    argument = one_argument(argument, arg1);

    // First, check for no arguments supplied - if so, display usage info.
    if (!str_cmp(arg1, ""))
    {
        send_to_char ("VLedit Usage:\n\r", ch);
        send_to_char ("              vledit <vlink uid>\n\r", ch);
        return;
    }

    // Next, check for a supplied uid parameter.
    if (is_number (arg1))
    {
        value = atoi (arg1);

        if ((pVLink = get_vlink_from_uid (NULL, value)) == NULL)
        {
            send_to_char ("That vlink uid does not appear to exist.\n\r", ch);
            return;
        }
	else
            found = true;
    }
    else if (!str_cmp (arg1, "show"))
    {
        vledit_show (ch, argument);
        return;
    }

    if (found)
    {
        sprintf(buf, "{x[{Wvledit{x] Editing uid '%ld'\n\r", pVLink->uid);
        send_to_char(buf, ch);
        ch->desc->pEdit = (void *) pVLink;
        ch->desc->editor = ED_VLINK;
    }

    return;
}



void display_resets(CHAR_DATA *ch)
{
    ROOM_INDEX_DATA	*pRoom;
    RESET_DATA		*pReset;
    MOB_INDEX_DATA	*pMob = NULL;
    char 		buf   [ MAX_STRING_LENGTH ];
    char 		final [ MAX_STRING_LENGTH ];
    int 		iReset = 0;

    EDIT_ROOM_VOID(ch, pRoom);
    final[0]  = '\0';

    send_to_char (
	" No.  Loads    Description       Location         Vnum   Mx Mn Description"
	"\n\r"
	"==== ======== ============= =================== ======== ===== ==========="
	"\n\r", ch);

    for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
    {
	OBJ_INDEX_DATA  *pObj;
	MOB_INDEX_DATA  *pMobIndex;
	OBJ_INDEX_DATA  *pObjIndex;
	OBJ_INDEX_DATA  *pObjToIndex;
	ROOM_INDEX_DATA *pRoomIndex;

	final[0] = '\0';
	sprintf(final, "[%2d] ", ++iReset);

	switch (pReset->command)
	{
	    default:
		sprintf(buf, "Bad reset command: %c.", pReset->command);
		strcat(final, buf);
		break;

	    case 'M':
		if (!(pMobIndex = get_mob_index(pReset->arg1)))
		{
		    sprintf(buf, "Load Mobile - Bad Mob %ld\n\r", pReset->arg1);
		    strcat(final, buf);
		    continue;
		}

		if (!(pRoomIndex = get_room_index(pReset->arg3)))
		{
		    sprintf(buf, "Load Mobile - Bad Room %ld\n\r", pReset->arg3);
		    strcat(final, buf);
		    continue;
		}

		pMob = pMobIndex;
		sprintf(buf, "M[%5ld] %-13.13s in room             R[%5ld] %2ld-%2ld %-15.15s\n\r",
			   pReset->arg1, pMob->short_descr, pReset->arg3,
			   pReset->arg2, pReset->arg4, pRoomIndex->name);
		strcat(final, buf);

		/*
		 * Check for pet shop.
		 * -------------------
		 */
		{
		    ROOM_INDEX_DATA *pRoomIndexPrev;

		    pRoomIndexPrev = get_room_index(pRoomIndex->vnum - 1);
		    if (pRoomIndexPrev
			&& IS_SET(pRoomIndexPrev->room_flag[0], ROOM_PET_SHOP))
			final[5] = 'P';
		}

		break;

	    case 'O':
		if (!(pObjIndex = get_obj_index(pReset->arg1)))
		{
		    sprintf(buf, "Load Object - Bad Object %ld\n\r",
			pReset->arg1);
		    strcat(final, buf);
		    continue;
		}

		pObj       = pObjIndex;

		if (!(pRoomIndex = get_room_index(pReset->arg3)))
		{
		    sprintf(buf, "Load Object - Bad Room %ld\n\r", pReset->arg3);
		    strcat(final, buf);
		    continue;
		}

		sprintf(buf, "O[%5ld] %-13.13s in room             "
			      "R[%5ld]       %-15.15s\n\r",
			      pReset->arg1, pObj->short_descr,
			      pReset->arg3, pRoomIndex->name);
		strcat(final, buf);

		break;

	    case 'P':
		if (!(pObjIndex = get_obj_index(pReset->arg1)))
		{
		    sprintf(buf, "Put Object - Bad Object %ld\n\r",
			pReset->arg1);
		    strcat(final, buf);
		    continue;
		}

		pObj       = pObjIndex;

		if (!(pObjToIndex = get_obj_index(pReset->arg3)))
		{
		    sprintf(buf, "Put Object - Bad To Object %ld\n\r",
			pReset->arg3);
		    strcat(final, buf);
		    continue;
		}

		sprintf(buf,
		    "O[%5ld] %-13.13s inside              O[%5ld] %2ld-%2ld %-15.15s\n\r",
		    pReset->arg1,
		    pObj->short_descr,
		    pReset->arg3,
		    pReset->arg2,
		    pReset->arg4,
		    pObjToIndex->short_descr);
		strcat(final, buf);

		break;

	    case 'G':
	    case 'E':
		if (!(pObjIndex = get_obj_index(pReset->arg1)))
		{
		    sprintf(buf, "Give/Equip Object - Bad Object %ld\n\r",
			pReset->arg1);
		    strcat(final, buf);
		    continue;
		}

		pObj       = pObjIndex;

		if (!pMob)
		{
		    sprintf(buf, "Give/Equip Object - No Previous Mobile\n\r");
		    strcat(final, buf);
		    break;
		}

		if (pMob->pShop)
		{
		sprintf(buf,
		    "O[%5ld] %-13.13s in the inventory of S[%5ld]       %-15.15s\n\r",
		    pReset->arg1,
		    pObj->short_descr,
		    pMob->vnum,
		    pMob->short_descr );
		}
		else
		sprintf(buf,
		    "O[%5ld] %-13.13s %-19.19s M[%5ld]       %-15.15s\n\r",
		    pReset->arg1,
		    pObj->short_descr,
		    (pReset->command == 'G') ?
			flag_string(wear_loc_strings, WEAR_NONE)
		      : flag_string(wear_loc_strings, pReset->arg3),
		      pMob->vnum,
		      pMob->short_descr);
		strcat(final, buf);

		break;

	    /*
	     * Doors are set in rs_flags don't need to be displayed.
	     * If you want to display them then uncomment the new_reset
	     * line in the case 'D' in load_resets in db.c and here.
	     */
	     /*
	    case 'D':
		pRoomIndex = get_room_index(pReset->arg1);
		sprintf(buf, "R[%5ld] %s door of %-19.19s reset to %s\n\r",
		    pReset->arg1,
		    capitalize(dir_name[ pReset->arg2 ]),
		    pRoomIndex->name,
		    flag_string(door_resets, pReset->arg3));
		strcat(final, buf);

		break;
		*/
	    /*
	     * End Doors Comment.
	     */
	    case 'R':
		if (!(pRoomIndex = get_room_index(pReset->arg1)))
		{
		    sprintf(buf, "Randomize Exits - Bad Room %ld\n\r",
			pReset->arg1);
		    strcat(final, buf);
		    continue;
		}

		sprintf(buf, "R[%5ld] Exits are randomized in %s\n\r",
		    pReset->arg1, pRoomIndex->name);
		strcat(final, buf);

		break;
	}

	ch->pcdata->immortal->last_olc_command = current_time;
	send_to_char(final, ch);
    }
}


void add_reset(ROOM_INDEX_DATA *room, RESET_DATA *pReset, int index)
{
    RESET_DATA *reset;
    int iReset = 0;

    if (!room->reset_first)
    {
	room->reset_first	= pReset;
	room->reset_last	= pReset;
	pReset->next		= NULL;
	return;
    }

    index--;

    if (index == 0)	/* First slot (1) selected. */
    {
	pReset->next = room->reset_first;
	room->reset_first = pReset;
	return;
    }

    // If negative slot(<= 0 selected) then this will find the last.
    for (reset = room->reset_first; reset->next; reset = reset->next)
    {
	if (++iReset == index)
	    break;
    }

    pReset->next	= reset->next;
    reset->next		= pReset;
    if (!pReset->next)
	room->reset_last = pReset;
}


void do_resets(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    char arg5[MAX_INPUT_LENGTH];
    char arg6[MAX_INPUT_LENGTH];
    char arg7[MAX_INPUT_LENGTH];
    RESET_DATA *pReset = NULL;
    AREA_DATA *area;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);
    argument = one_argument(argument, arg7);

    if (!IS_BUILDER(ch, ch->in_room->area))
    {
	send_to_char("Resets: Invalid security for editing this area.\n\r", ch);
	return;
    }

    area = ch->in_room->area;

    /* show resets */
    if (arg1[0] == '\0')
    {
	if (ch->in_room->reset_first)
	{
	    send_to_char(
		"Resets: M = mobile, R = room, O = object, "
		"P = pet, S = shopkeeper\n\r", ch);
	    display_resets(ch);
	}
	else
	    send_to_char("No resets in this room.\n\r", ch);
    }

    /* take index number and search for commands */
    if (is_number(arg1))
    {
	ROOM_INDEX_DATA *pRoom = ch->in_room;

	/* delete a reset */
	if (!str_cmp(arg2, "delete"))
	{
	    long insert_loc = atol(arg1);

	    if (!ch->in_room->reset_first)
	    {
		send_to_char("No resets in this area.\n\r", ch);
		return;
	    }

	    if (insert_loc - 1 <= 0)
	    {
		pReset = pRoom->reset_first;
		pRoom->reset_first = pRoom->reset_first->next;
		if (!pRoom->reset_first)
		    pRoom->reset_last = NULL;
	    }
	    else
	    {
		long iReset = 0;
		RESET_DATA *prev = NULL;

		for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
		{
		    if (++iReset == insert_loc)
			break;

		    prev = pReset;
		}

		if (!pReset)
		{
		    send_to_char("Reset not found.\n\r", ch);
		    return;
		}

		if (prev)
		    prev->next = prev->next->next;
		else
		    pRoom->reset_first = pRoom->reset_first->next;

		for (pRoom->reset_last = pRoom->reset_first;
		      pRoom->reset_last->next;
		      pRoom->reset_last = pRoom->reset_last->next);
	    }

	    free_reset_data(pReset);
	    send_to_char("Reset deleted.\n\r", ch);
	    SET_BIT(area->area_flags, AREA_CHANGED);
	}
	else
	/* add a reset */
	if ((!str_cmp(arg2, "mob") && is_number(arg3))
	  || (!str_cmp(arg2, "obj") && is_number(arg3)))
	{
	    if (!str_cmp(arg2, "mob"))
	    {
		if (get_mob_index(is_number(arg3) ? atol(arg3) : 1) == NULL)
		{
		    send_to_char("Mob no existe.\n\r",ch);
		    return;
		}
		pReset = new_reset_data();
		pReset->command = 'M';
		pReset->arg1 = atol(arg3);
		pReset->arg2 = is_number(arg4) ? atol(arg4) : 1; /* Max # */
		pReset->arg3 = ch->in_room->vnum;
		pReset->arg4 = is_number(arg5) ? atol(arg5) : 1; /* Min # */
	    }
	    else
	    if (!str_cmp(arg2, "obj"))
	    {
		pReset = new_reset_data();
		pReset->arg1    = atol(arg3);
		if (!str_prefix(arg4, "inside"))
		{
		    OBJ_INDEX_DATA *temp;

		    temp = get_obj_index(is_number(arg5) ? atol(arg5) : 1);
		    if (temp == NULL) {
			send_to_char("Object not found!\n\r", ch);
			return;
		    }

		    if ((temp->item_type != ITEM_CONTAINER) &&
		         (temp->item_type != ITEM_CORPSE_NPC))
		    {
			send_to_char("Object 2 isn't a container.\n\r", ch);
			return;
		    }
		    pReset->command = 'P';
		    pReset->arg2    = is_number(arg6) ? atol(arg6) : 1;
		    pReset->arg3    = is_number(arg5) ? atol(arg5) : 1;
		    pReset->arg4    = is_number(arg7) ? atol(arg7) : 1;
		}
		else
		if (!str_cmp(arg4, "room"))
		{
		    if (get_obj_index(atol(arg3)) == NULL)
		      {
		         send_to_char("Vnum does not exist.\n\r",ch);
		         return;
		      }
		    pReset->command  = 'O';
		    pReset->arg2     = 0;
		    pReset->arg3     = ch->in_room->vnum;
		    pReset->arg4     = 0;
		}
		else
		{
		    if (flag_value(wear_loc_flags, arg4) == NO_FLAG)
		    {
			// Hack because WEAR_LIGHT is same value as NO_FLAG
			if (str_cmp(arg4, "light"))
			{
			    send_to_char("Resets: '? wear-loc'\n\r", ch);
			    return;
			}
		    }

		    if (get_obj_index(atol(arg3)) == NULL)
		    {
			send_to_char("Vnum does not exist.\n\r",ch);
			return;
		    }

		    pReset->arg1 = atol(arg3);
		    pReset->arg3 =
		        (!str_cmp(arg4, "light")) ?
			WEAR_LIGHT :
			flag_value(wear_loc_flags, arg4);

		    if (pReset->arg3 == WEAR_NONE)
			pReset->command = 'G';
		    else
			pReset->command = 'E';
		}
	    }

	    add_reset(ch->in_room, pReset, atol(arg1));
	    SET_BIT(ch->in_room->area->area_flags, AREA_CHANGED);
	    send_to_char("Reset added.\n\r", ch);
	}
	else
	{
	    send_to_char("Syntax: RESET <number> OBJ <vnum> <wear_loc>\n\r", ch);
	    send_to_char("        RESET <number> OBJ <vnum> inside <vnum> [limit] [count]\n\r", ch);
	    send_to_char("        RESET <number> OBJ <vnum> room\n\r", ch);
	    send_to_char("        RESET <number> MOB <vnum> [max #x area] [max #x room]\n\r", ch);
	    send_to_char("        RESET <number> DELETE\n\r", ch);
	}
    }
}


void do_asearch(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    char result[MAX_STRING_LENGTH*2];
    AREA_DATA *pArea;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_char("Search through area list for which keyword?\n\r", ch);
	return;
    }

    sprintf(result, "[%3s] [%-27s] (%-5s-%5s) [%-10s] %3s [%-9s]\n\r",
       "Num", "Area Name", "lvnum", "uvnum", "Filename", "Sec", "Builders");


    for (pArea = area_first; pArea; pArea = pArea->next)
    {
	if (!str_infix(arg, pArea->name))
	{
	    sprintf(buf,
	    "[%3ld] %-27.27s (%-5ld-%5ld) %-12.12s [%d] [%-10.10s]\n\r",
	        pArea->anum,
		pArea->name,
		pArea->min_vnum,
		pArea->max_vnum,
		pArea->file_name,
		pArea->security,
		pArea->builders);
	    strcat(result, buf);
	}
    }

    send_to_char(result, ch);
}


void do_alist(CHAR_DATA *ch, char *argument)
{
    char buf[MAX_STRING_LENGTH];
    AREA_DATA *pArea;
    BUFFER *buffer;
    int place_type = 0;

    buffer = new_buf();

    sprintf(buf, "[%-7s] [%-7s] [%-26.26s] (%-7s-%7s) [%-10s] %3s [%-10s]\n\r",
       "Anum", "UID", "Area Name", "lvnum", "uvnum", "Filename", "Sec", "Builders");
    add_buf(buffer, buf);

    if (argument[0] != '\0'
    && (place_type = flag_value(place_flags, argument)) == NO_FLAG)
    {
	send_to_char("Syntax: alist\n\r"
	             "        alist <placetype>\n\r", ch);
	return;
    }

    for (pArea = area_first; pArea; pArea = pArea->next)
    {
	if (place_type == 0 || (pArea->place_flags == place_type))
	{
	sprintf(buf, "{D[{x%7ld{D]{x {D[{x%7ld{D]{x %s%-26.26s%s {D({x%-7ld{D-{x%7ld{D){x %-12.12s {D[{x{B%d{x{D]{x {D[{x%-10.10s{D]{x \n\r",
	     pArea->anum,
	     pArea->uid,
	     pArea->open ? "{G" : "{R",
	     pArea->name,
	     "{x",
	     pArea->min_vnum,
	     pArea->max_vnum,
	     pArea->file_name,
	     pArea->security,
	     pArea->builders);
	add_buf(buffer, buf);
	}
    }

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


void hedit(CHAR_DATA *ch, char *argument)
{
    char command[MIL];
    char arg[MIL];
    int cmd;

    smash_tilde(argument);
    strcpy(arg, argument);
    argument = one_argument(argument, command);

    if (!IS_IMMORTAL(ch))
    {
	send_to_char("HEdit: Insufficient security.\n\r",ch);
	edit_done(ch);
	return;
    }

    if (!str_cmp(command, "done"))
    {
	if (ch->desc->pEdit == NULL) 	// We aren't editing a helpfile
	    edit_done(ch);
	else {
	    ch->desc->pEdit = NULL; 	// We're editing a helpfile.
	    ch->desc->editor = ED_HELP;
	}

	return;
    }

    if (command[0] == '\0')
    {
        hedit_show(ch, argument);
	return;
    }

    for (cmd = 0; hedit_table[cmd].name != NULL; cmd++)
    {
	if (!str_prefix(command, hedit_table[cmd].name))
	{
            if ((*hedit_table[cmd].olc_fun) (ch, argument ))
	    {
		ch->pcdata->immortal->last_olc_command = current_time;
		if (ch->desc->pEdit != NULL) {
		    HELP_DATA *help = (HELP_DATA *) ch->desc->pEdit;

		    free_string(help->modified_by);
		    help->modified_by = str_dup(ch->name);
		    help->modified = current_time;
		} else {
		    free_string(ch->desc->hCat->modified_by);
		    ch->desc->hCat->modified_by = str_dup(ch->name);
		    ch->desc->hCat->modified = current_time;
		}
	    }

	    return;
	}
    }

    interpret(ch, arg);
}


void do_hedit(CHAR_DATA *ch, char *argument)
{
    /* 2006-07-21 Removed as per Areo's suggestion (Syn)
    if (get_trust(ch) < MAX_LEVEL - 4) {
	send_to_char("Insufficient security to edit helpfiles. Action logged.\n\r", ch);
	return;
    }
    */

    ch->pcdata->immortal->last_olc_command = current_time;
    ch->desc->editor= ED_HELP;
    ch->desc->pEdit = NULL;
    ch->desc->hCat = topHelpCat;
}


/*
 * Copy a room.
 */
void do_rcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    ROOM_INDEX_DATA *old_room;
    ROOM_INDEX_DATA *new_room;
    EXTRA_DESCR_DATA *ed;
    EXTRA_DESCR_DATA *new_ed;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    long old_v;
    long new_v;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    old_v = atol(arg);
    new_v = atol(arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: rcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    if (get_room_index(old_v) == NULL)
    {
 	send_to_char("That room doesn't exist.\n\r", ch);
	return;
    }

    if (get_room_index(new_v) != NULL)
    {
 	send_to_char("That room vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You're not a builder in that area, so you can't "
			 "copy from it.\n\r", ch);
	return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    old_room = get_room_index(old_v);
    new_room = new_room_index();

    new_room->area                 = area;
    list_appendlink(area->room_list, new_room);	// Add to the area room list

    new_room->vnum                 = new_v;
    if (new_v > top_vnum_room)
        top_vnum_room = new_v;

    iHash                       = new_v % MAX_KEY_HASH;
    new_room->next              = room_index_hash[iHash];
    room_index_hash[iHash]      = new_room;
    ch->desc->pEdit             = (void *)new_room;
    ch->desc->editor		= ED_ROOM;

    // Copy extra descs
    for (ed = old_room->extra_descr; ed != NULL; ed = ed->next)
    {
	new_ed = new_extra_descr();
	new_ed->keyword = str_dup(ed->keyword);
	if( ed->description )
		new_ed->description = str_dup(ed->description);
	else
		new_ed->description = NULL;
	new_ed->next = new_room->extra_descr;
	new_room->extra_descr = new_ed;
    }

    new_room->name = str_dup(old_room->name);
    new_room->description = str_dup(old_room->description);
    new_room->owner = str_dup(old_room->owner);
    new_room->room_flag[0] = old_room->room_flag[0];
    new_room->room_flag[1] = old_room->room_flag[1];
    new_room->sector_type = old_room->sector_type;
    new_room->heal_rate = old_room->heal_rate;
    new_room->mana_rate = old_room->mana_rate;
    new_room->move_rate = old_room->move_rate;
    new_room->comments = old_room->comments;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("Room copied.\n\r", ch);
}


/*
 * Copy a mob.
 */
void do_mcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    MOB_INDEX_DATA *old_mob;
    MOB_INDEX_DATA *new_mob;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    long old_v;
    long new_v;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Syntax: mcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    old_v = atol(arg);
    new_v = atol(arg2);

    if ((old_mob = get_mob_index(old_v)) == NULL)
    {
	send_to_char("That mob doesn't exist.\n\r", ch);
	return;
    }

    if (get_mob_index(new_v) != NULL)
    {
        send_to_char("That mob vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You're not a builder in that area, so you can't "
                     "copy from it.\n\r", ch);
	return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
	send_to_char("That vnum is not assigned an area.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_mob       = new_mob_index();
    new_mob->vnum = new_v;
    new_mob->area = area;

    if (new_v > top_vnum_mob)
        top_vnum_mob = new_v;

    iHash			= new_v % MAX_KEY_HASH;
    new_mob->next		= mob_index_hash[iHash];
    mob_index_hash[iHash]	= new_mob;
    ch->desc->pEdit		= (void *)new_mob;
    ch->desc->editor		= ED_MOBILE;

    new_mob->player_name = str_dup(old_mob->player_name);
    new_mob->short_descr = str_dup(old_mob->short_descr);
    new_mob->long_descr  = str_dup(old_mob->long_descr);
    new_mob->description = str_dup(old_mob->description);
    new_mob->comments   = str_dup(old_mob->comments);

    new_mob->act[0]          = old_mob->act[0];
    new_mob->act[1]         = old_mob->act[1];
    new_mob->affected_by[0]  = old_mob->affected_by[0];
    new_mob->affected_by[1] = old_mob->affected_by[1];

    new_mob->alignment    = old_mob->alignment;
    new_mob->level        = old_mob->level;
    new_mob->hitroll      = old_mob->hitroll;
    new_mob->hit.number       = old_mob->hit.number;
    new_mob->hit.size       = old_mob->hit.size;
    new_mob->hit.bonus       = old_mob->hit.bonus;
    new_mob->mana.number      = old_mob->mana.number;
    new_mob->mana.size      = old_mob->mana.size;
    new_mob->mana.bonus      = old_mob->mana.bonus;
    new_mob->damage.number    = old_mob->damage.number;
    new_mob->damage.size    = old_mob->damage.size;
    new_mob->damage.bonus    = old_mob->damage.bonus;
    new_mob->ac[0]        = old_mob->ac[0];
    new_mob->ac[1]        = old_mob->ac[1];
    new_mob->ac[2]        = old_mob->ac[2];
    new_mob->ac[3]        = old_mob->ac[3];
    new_mob->dam_type     = old_mob->dam_type;

    new_mob->off_flags    = old_mob->off_flags;
    new_mob->imm_flags    = old_mob->imm_flags;
    new_mob->res_flags    = old_mob->res_flags;
    new_mob->vuln_flags   = old_mob->vuln_flags;
    new_mob->start_pos    = old_mob->start_pos;
    new_mob->default_pos  = old_mob->default_pos;

    new_mob->sex	  = old_mob->sex;
    new_mob->race	  = old_mob->race;
    new_mob->wealth	  = old_mob->wealth;
    new_mob->form	  = old_mob->form;
    new_mob->parts	  = old_mob->parts;
    new_mob->size	  = old_mob->size;
    new_mob->material     = str_dup(old_mob->material);
    new_mob->move	  = old_mob->move;
    new_mob->attacks      = old_mob->attacks;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("Mobile copied.\n\r", ch);
}


/*
 * Copy an obj.
 */
void do_ocopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    OBJ_INDEX_DATA *old_obj;
    OBJ_INDEX_DATA *new_obj;
    EXTRA_DESCR_DATA *ed;
    EXTRA_DESCR_DATA *new_ed;
    AFFECT_DATA *af;
    AFFECT_DATA *new_af;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];
    int i;
    long old_v;
    long new_v;
    int iHash;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: ocopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    old_v = atol(arg);
    new_v = atol(arg2);

    if ((old_obj = get_obj_index(old_v)) == NULL)
    {
        send_to_char("That obj doesn't exist.\n\r", ch);
	return;
    }

    if (get_obj_index(new_v) != NULL)
    {
        send_to_char("That obj vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You're not a builder in that area, so you can't "
			"copy from it.\n\r", ch);
	return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_obj		= new_obj_index();
    new_obj->vnum	= new_v;
    new_obj->area	= area;

    if (new_v > top_vnum_obj)
        top_vnum_obj = new_v;

    iHash			= new_v % MAX_KEY_HASH;
    new_obj->next		= obj_index_hash[iHash];
    obj_index_hash[iHash]	= new_obj;
    ch->desc->editor		= ED_OBJECT;
    ch->desc->pEdit		= (void *)new_obj;

    // Copy extra descs
    for (ed = old_obj->extra_descr; ed != NULL; ed = ed->next)
    {
	new_ed = new_extra_descr();
	new_ed->keyword = str_dup(ed->keyword);
	if(ed->description)
		new_ed->description = str_dup(ed->description);
	else
		new_ed->description = NULL;
	new_ed->next = new_obj->extra_descr;
	new_obj->extra_descr = new_ed;
    }

    // Copy affects
    for (af = old_obj->affected; af != NULL; af = af->next)
    {
	new_af = new_affect();
	new_af->location = af->location;
	new_af->modifier = af->modifier;
	new_af->where = af->where;
	new_af->type  = af->type;
	new_af->duration = af->duration;
	new_af->bitvector = af->bitvector;
	new_af->level	 = af->level;
	new_af->random  = af->random;
	new_af->next    = new_obj->affected;
	new_obj->affected = new_af;
    }

    new_obj->name = str_dup(old_obj->name);
    new_obj->short_descr = str_dup(old_obj->short_descr);
    new_obj->description = str_dup(old_obj->description);
    new_obj->full_description = str_dup(old_obj->full_description);
    new_obj->material = str_dup(old_obj->material);
    new_obj->item_type =  old_obj->item_type;
    new_obj->extra[0] = old_obj->extra[0];
    new_obj->extra[1] = old_obj->extra[1];
    new_obj->extra[2] = old_obj->extra[2];
    new_obj->extra[3] = old_obj->extra[3];
    new_obj->wear_flags = old_obj->wear_flags;
    new_obj->level = old_obj->level;
    new_obj->condition = old_obj->condition;
    new_obj->count = old_obj->count;
    new_obj->weight = old_obj->weight;
    new_obj->cost = old_obj->cost;
    new_obj->fragility = old_obj->fragility;
    new_obj->times_allowed_fixed = old_obj->times_allowed_fixed;
    new_obj->comments = old_obj->comments;

    for (i = 0; i <= 8; i++)
	new_obj->value[i] = old_obj->value[i];

    // Only copy impsig if imp (to block cheaters)
    if (get_trust(ch) == MAX_LEVEL)
	new_obj->imp_sig = str_dup(old_obj->imp_sig);
    else
	new_obj->imp_sig = str_dup("none");

    new_obj->points = old_obj->points;

    SET_BIT(area->area_flags, AREA_CHANGED);

    send_to_char("Object copied.\n\r", ch);
    //oedit_show(ch, "");
}


/*
 * Copy an rprog.
 */
void do_rpcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    long old_v;
    long new_v;
    SCRIPT_DATA *old_rpcode;
    SCRIPT_DATA *new_rpc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: rpcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    old_v = atol(arg);
    new_v = atol(arg2);

    if ((old_rpcode = get_script_index(old_v, PRG_RPROG)) == NULL)
    {
        send_to_char("That ROOMprog doesn't exist.\n\r", ch);
	return;
    }

    if (get_script_index(new_v, PRG_RPROG) != NULL)
    {
	send_to_char("That ROOMprog vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You're not a builder in that area, so you can't "
			"copy from it.\n\r", ch);
	return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);
    ch->pcdata->immortal->last_olc_command = current_time;
    new_rpc = new_script();
    new_rpc->vnum = new_v;
    new_rpc->edit_src = str_dup(old_rpcode->src);
    new_rpc->area = area;
    compile_script(NULL,new_rpc, new_rpc->edit_src, IFC_R);
    new_rpc->next = rprog_list;
    rprog_list = new_rpc;

    ch->desc->pEdit             = (void *)new_rpc;
    ch->desc->editor            = ED_RPCODE;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("RoomProgram code copied.\n\r",ch);
}


/*
 * Copy an mprog.
 */
void do_mpcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    long old_v;
    long new_v;
    SCRIPT_DATA *old_mpcode;
    SCRIPT_DATA *new_mpc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
	send_to_char("Syntax: mpcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    old_v = atol(arg);
    new_v = atol(arg2);

    if ((old_mpcode = get_script_index(old_v, PRG_MPROG)) == NULL)
    {
	send_to_char("That MOBprog doesn't exist.\n\r", ch);
	return;
    }

    if (get_script_index(new_v, PRG_MPROG) != NULL)
    {
        send_to_char("That MOBprog vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You're not a builder in that area, so you can't "
			"copy from it.\n\r", ch);
	return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
	return;
    }

    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_mpc = new_script();
    new_mpc->vnum = new_v;
    new_mpc->area = area;
    new_mpc->edit_src = str_dup(old_mpcode->src);
    compile_script(NULL,new_mpc, new_mpc->edit_src, IFC_M);
    new_mpc->next = mprog_list;
    mprog_list = new_mpc;

    ch->desc->pEdit             = (void *)new_mpc;
    ch->desc->editor            = ED_MPCODE;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("MobProgram code copied.\n\r",ch);
}


/*
 * Copy an oprog.
 */
void do_opcopy(CHAR_DATA *ch, char *argument)
{
    AREA_DATA *area;
    long old_v;
    long new_v;
    SCRIPT_DATA *old_opcode;
    SCRIPT_DATA *new_opc;
    char arg[MAX_STRING_LENGTH];
    char arg2[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Syntax: opcopy <old_vnum> <new_vnum>\n\r", ch);
	return;
    }

    old_v = atol(arg);
    new_v = atol(arg2);

    if ((old_opcode = get_script_index(old_v, PRG_OPROG)) == NULL)
    {
        send_to_char("That OBJprog doesn't exist.\n\r", ch);
	return;
    }

    if (get_script_index(new_v, PRG_OPROG) != NULL)
    {
        send_to_char("That OBJprog vnum is already taken.\n\r", ch);
	return;
    }

    area = get_vnum_area(old_v);
    if (!IS_BUILDER(ch, area))
    {
        send_to_char("You're not a builder in that area, so you can't "
 		    "copy from it.\n\r", ch);
        return;
    }

    area = get_vnum_area(new_v);
    if (area == NULL)
    {
        send_to_char("That vnum is not assigned an area.\n\r", ch);
        return;
    }

    if (!IS_BUILDER(ch, area))
    {
	send_to_char("You can't build in that area.\n\r", ch);
	return;
    }

    edit_done(ch);

    ch->pcdata->immortal->last_olc_command = current_time;
    new_opc = new_script();
    new_opc->vnum = new_v;
    new_opc->area = area;
    new_opc->edit_src = str_dup(old_opcode->src);
    compile_script(NULL,new_opc, new_opc->edit_src, IFC_O);
    new_opc->next = oprog_list;
    oprog_list = new_opc;

    ch->desc->pEdit             = (void *)new_opc;
    ch->desc->editor            = ED_OPCODE;

    SET_BIT(area->area_flags, AREA_CHANGED);
    send_to_char("ObjProgram code copied.\n\r",ch);
}


void do_rlist(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoomIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    bool range = false;
    long vnum;
    long vnum_min;
    long vnum_max;
    int col = 0;

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if (arg[0] != '\0' && arg2[0] != '\0')
	    range = true;

    if (range && (!is_number(arg) || !is_number (arg2)))
    {
	send_to_char("Syntax: rlist\n\r"
		     "        rlist [min vnum] [max vnum]\n\r", ch);
	return;
    }

    pArea = ch->in_room->area;

    if (range)
    {
	vnum_min = atoi(arg);
	vnum_max = atoi(arg2);

	if (vnum_min < pArea->min_vnum)
	{
	    send_to_char("Minimum vnum is not in the area.\n\r" , ch);
	    return;
	}

	if (vnum_max > pArea->max_vnum)
	{
	    send_to_char("Maximum vnum is not in the area.\n\r", ch);
	    return;
	}
    }
    else
    {
	vnum_min = pArea->min_vnum;
	vnum_max = pArea->max_vnum;
    }

    buf1  = new_buf();

    for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
    {
	if ((pRoomIndex = get_room_index(vnum)) != NULL
	&& vnum >= vnum_min
	&& vnum <= vnum_max)
	{
	    char *noc;
	    noc = nocolour(pRoomIndex->name);
	    sprintf(buf, "[%5ld] %-17.16s", vnum, noc);
	    free_string(noc);
	    if (!add_buf(buf1, buf))
	    {
		send_to_char("Can't output that much data!\n\r"
			"Use rlist <min vnum> <max vnum>\n\r", ch);
		return;
	    }
	    if (++col % 3 == 0)
	    {
		if (!add_buf(buf1, "\n\r"))
		{
		    send_to_char("Can't output that much data!\n\r"
			    "Use rlist <min vnum> <max vnum>\n\r", ch);
		    return;
		}
	    }
	}
    }

    if (col % 3 != 0)
	add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
}


void do_mlist(CHAR_DATA *ch, char *argument)
{
    MOB_INDEX_DATA *pMobIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[ MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool fAll;
    bool found;
    long vnum;
    int col = 0;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax:  mlist <all|name>\n\r", ch);
	return;
    }

    buf1  = new_buf();
    pArea = ch->in_room->area;
    fAll  = !str_cmp(arg, "all");
    found = false;

    for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
    {
	if ((pMobIndex = get_mob_index(vnum)) != NULL)
	{
	    if (fAll || is_name(arg, pMobIndex->player_name))
	    {
		char *noc;
		found = true;
		noc = nocolour(pMobIndex->short_descr);
		sprintf(buf, "{x[%5ld] %-17.16s{x", pMobIndex->vnum, noc);
		add_buf(buf1, buf);
		free_string(noc);
		if (++col % 3 == 0)
		    add_buf(buf1, "\n\r");
	    }
	}
    }

    if (!found)
    {
	send_to_char("Mobile(s) not found in this area.\n\r", ch);
	return;
    }

    if (col % 3 != 0)
	add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
    return;
}

int strlen_colours_limit( const char *str, int limit )
{
    int count;
    int i;

    if ( str == NULL )
        return 0;

    count = 0;
    for ( i = 0; count < limit && str[i] != '\0'; i++ )
    {
	if (str[i] == '{' )
	{
	    i++;
	    continue;
	}

	count++;
    }

    return i - count;
}

void do_olist(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObjIndex;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    //char buf2[MSL];
    char arg[MAX_INPUT_LENGTH];
    bool fAll, found;
    long vnum;
    int col = 0;
    int max;

    one_argument(argument, arg);
    if (arg[0] == '\0')
    {
	send_to_char("Syntax:  olist <all|name|item_type>\n\r", ch);
	return;
    }

    pArea = ch->in_room->area;
    buf1  = new_buf();
    fAll  = !str_cmp(arg, "all");
    found = false;

    for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
    {
	if ((pObjIndex = get_obj_index(vnum)))
	{
	    if (fAll || is_name(arg, pObjIndex->name)
	    || flag_value(type_flags, arg) == pObjIndex->item_type)
	    {
		found = true;/*
		sprintf(buf2, "%s", pObjIndex->short_descr);
		if ((i = (17 - strlen_no_colours(buf2))) > 0) {
		   while (i > 0) {
		       strcat(buf2, " ");
		       i--;
		   }
		}*/
		max = strlen_colours_limit(pObjIndex->short_descr,16) + 17;
		sprintf(buf, "{x[%5ld] %-*.*s{x",
		    pObjIndex->vnum, max, max - 1, pObjIndex->short_descr);
		add_buf(buf1, buf);
		if (++col % 3 == 0)
		    add_buf(buf1, "\n\r");
	    }
	}
    }

    if (!found)
    {
	send_to_char("Object(s) not found in this area.\n\r", ch);
	return;
    }

    if (col % 3 != 0)
	add_buf(buf1, "\n\r");

//	sprintf(buf,"%d\n\r",strlen(buf1->string));
//	send_to_char(buf,ch);

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
    return;
}


void do_mshow(CHAR_DATA *ch, char *argument)
{
    MOB_INDEX_DATA *pMob;
    void *old_edit;
    long value;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  mshow <vnum>\n\r", ch);
	return;
    }

    if (!is_number(argument))
    {
       send_to_char("Vnum must be a number.\n\r", ch);
       return;
    }

    value = atol(argument);
    if (!(pMob = get_mob_index(value)))
    {
       send_to_char("That mobile does not exist.\n\r", ch);
       return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pMob;

    medit_show(ch, argument);
    ch->desc->pEdit = old_edit;
    return;
}


void do_oshow(CHAR_DATA *ch, char *argument)
{
    OBJ_INDEX_DATA *pObj;
    void *old_edit;
    long value;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  oshow <vnum>\n\r", ch);
	return;
    }

    if (!is_number(argument))
    {
       send_to_char("Vnum must be a number.\n\r", ch);
       return;
    }

    value = atol(argument);
    if (!(pObj = get_obj_index(value)))
    {
	send_to_char("That object does not exist.\n\r", ch);
	return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pObj;
    oedit_show(ch, argument);
    ch->desc->pEdit = old_edit;
}


void do_rshow(CHAR_DATA *ch, char *argument)
{
    ROOM_INDEX_DATA *pRoom, *oldRoom;
    void *old_edit;
    long value;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  rshow <vnum>\n\r", ch);
	return;
    }

    if (!is_number(argument))
    {
       send_to_char("Vnum must be a number.\n\r", ch);
       return;
    }

    value = atol(argument);
    if (!(pRoom = get_room_index(value)))
    {
	send_to_char("That room does not exist.\n\r", ch);
	return;
    }

    oldRoom = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, pRoom);

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) pRoom;
    redit_show(ch, argument);
    ch->desc->pEdit = old_edit;

    char_from_room(ch);
    char_to_room(ch, oldRoom);
}


int calc_obj_armour(int level, int strength)
{
    switch (strength)
    {
	case OBJ_ARMOUR_LIGHT:		return level/10;
	case OBJ_ARMOUR_MEDIUM:		return level/5;
	case OBJ_ARMOUR_STRONG:		return level * 3/10;
	case OBJ_ARMOUR_HEAVY:		return level * 2/5;
	case OBJ_ARMOUR_NOSTRENGTH:
	default:			return 0;
    }
}


void set_mob_hitdice(MOB_INDEX_DATA *pMob)
{
    int hp_per_level;
    int hitBonus;
    int hitDiceType;
    int hitNumDice;

    if (pMob->level < 10)
	hp_per_level = 10;
    else
    	hp_per_level = pMob->level;

    hitBonus = ((hp_per_level) * (pMob->level / 2));
    hitNumDice = pMob->level * 0.8;
    hitDiceType = hp_per_level;
    pMob->hit.number = UMAX(1,hitNumDice);
    pMob->hit.size   = UMAX(1, hitDiceType);
    pMob->hit.bonus  = UMAX(1, hitBonus);
}


void set_mob_damdice(MOB_INDEX_DATA *pMobIndex)
{
    int num;
    int type;

    num = (int) (pMobIndex->level + 8) / 10;
    type = (int) (pMobIndex->level + 8) / 4;

    num = UMAX(1, num);
    type = UMAX(8, type);

    pMobIndex->damage.number = num;
    pMobIndex->damage.size = type;
    pMobIndex->damage.bonus = pMobIndex->level;
}


void set_mob_manadice(MOB_INDEX_DATA *pMobIndex)
{
    int num;
    int type;

    num = (int) (pMobIndex->level + 20) / 20;
    type = (int) (pMobIndex->level + 20) / 8;

    num = UMAX(1, num);
    type = UMAX(8, type);

    pMobIndex->mana.number = num;
    pMobIndex->mana.size = type;
    pMobIndex->mana.bonus = pMobIndex->level/2;
}

void set_mob_movedice(MOB_INDEX_DATA *pMobIndex)
{
    pMobIndex->move = 10 + 13 * pMobIndex->level;
}



/* set some weapon dice automatically on an objIndex. */
void set_weapon_dice(OBJ_INDEX_DATA *objIndex)
{
    int num;
    int type;
    char buf[MAX_STRING_LENGTH];

    if (objIndex->item_type != ITEM_WEAPON)
    {
	sprintf(buf, "set_weapon_dice: tried to set on non-weapon "
		"obj, %s, vnum %ld", objIndex->short_descr,
		objIndex->vnum);
	bug(buf, 0);
	return;
    }

    // This allows certain objects to evade the auto-setting when
    // we want them tooo.
    if (objIndex->imp_sig != NULL
    &&   str_cmp(objIndex->imp_sig, "(null)")
    &&   str_cmp(objIndex->imp_sig, "none"))
    {
	sprintf(buf, "set_weapon_dice: imp sig \"%s\" found, not "
		"auto-setting dice, obj %s, vnum %ld",
		objIndex->imp_sig,
		objIndex->short_descr, objIndex->vnum);
	log_string(buf);
	return;
    }

    num = (objIndex->level + 20) / 10;
    type = (objIndex->level + 20) / 4;

    if (IS_SET(objIndex->value[4], WEAPON_TWO_HANDS))
	type = (type * 7)/5 - 1;

    switch(objIndex->value[0])
    {
	case WEAPON_EXOTIC:		type += 3;	num -= 1; 	break;
	case WEAPON_SWORD:		type += 1;	num += 1; 	break;
	case WEAPON_DAGGER:		type -= 2;	num += 1; 	break;
	case WEAPON_SPEAR: 		type += 1;		  	break;
	case WEAPON_MACE:		type += 2;		  	break;
	case WEAPON_AXE:        	type += 2;		  	break;
	case WEAPON_FLAIL:		type += 2;		 	break;
	case WEAPON_WHIP:		type += 1;	num += 1; 	break;
	case WEAPON_POLEARM:		type += 2;		  	break;
	case WEAPON_STAKE:				  	  	break;
	case WEAPON_QUARTERSTAFF:	type += 1;		  	break;
	case WEAPON_ARROW: 		type = (type*7)/4;	num *= 2; 	break;
	case WEAPON_BOLT:		type *= 2;	num *= 2;	break;
	default: 						  	break;
    }

    if (IS_SET(objIndex->extra[1], ITEM_REMORT_ONLY))
    {
	type += 2;
	num += 2;
    }

    num = UMAX(1, num);
    type = UMAX(8, type);

    objIndex->value[1] = num;
    objIndex->value[2] = type;
}


/* set some weapon dice automatically , but for an obj. */
void set_weapon_dice_obj(OBJ_DATA *obj)
{
    int num;
    int type;
    char buf[MAX_STRING_LENGTH];

    if (obj->item_type != ITEM_WEAPON)
    {
	sprintf(buf, "set_weapon_dice: tried to set on non-weapon "
		"obj, %s, vnum %ld", obj->short_descr,
		obj->pIndexData->vnum);
	bug(buf, 0);
	return;
    }

    // This allows certain objects to evade the auto-setting when
    // we want them tooo.
    if (obj->pIndexData->imp_sig != NULL
    &&   str_cmp(obj->pIndexData->imp_sig, "(null)")
    &&   str_cmp(obj->pIndexData->imp_sig, "none"))
    {
	sprintf(buf, "set_weapon_dice: imp sig \"%s\" found, not "
		"auto-setting dice, obj %s, vnum %ld",
		obj->pIndexData->imp_sig,
		obj->short_descr, obj->pIndexData->vnum);
	log_string(buf);
	return;
    }

    num = (obj->level + 20) / 10;
    type = (obj->level + 20) / 4;

    if (IS_SET(obj->value[4], WEAPON_TWO_HANDS))
	type = type * 7/5 - 1;

    switch(obj->value[0])
    {
	case WEAPON_EXOTIC:		type += 3;	num -= 1; 	break;
	case WEAPON_SWORD:		type += 1;	num += 1; 	break;
	case WEAPON_DAGGER:		type -= 2;	num += 1; 	break;
	case WEAPON_SPEAR: 		type += 1;		  	break;
	case WEAPON_MACE:		type += 2;		  	break;
	case WEAPON_AXE:        	type += 2;		  	break;
	case WEAPON_FLAIL:		type += 2;	num -= 1; 	break;
	case WEAPON_WHIP:		type += 1;	num += 1; 	break;
	case WEAPON_POLEARM:		type += 2;		  	break;
	case WEAPON_STAKE:				  	  	break;
	case WEAPON_QUARTERSTAFF:	type += 1;		  	break;
	case WEAPON_ARROW: 		type = (type*7)/4;	num *= 2; 	break;
	case WEAPON_BOLT:		type *= 2;	num *= 2;	break;
	default: 						  	break;
    }

    if (IS_SET(obj->extra[1], ITEM_REMORT_ONLY))
    {
	type += 2;
	num += 2;
    }

    num = UMAX(1, num);
    type = UMAX(8, type);

    obj->value[1] = num;
    obj->value[2] = type;
}


/* set AC for an obj index */
void set_armour(OBJ_INDEX_DATA *objIndex)
{
    int armour;
    int armour_exotic;

    armour = calc_obj_armour(objIndex->level, objIndex->value[4]) ;
    armour_exotic = armour * 9/10;

    objIndex->value[0] = armour;
    objIndex->value[1] = armour;
    objIndex->value[2] = armour;
    objIndex->value[3] = armour_exotic;
}


/* set AC for an obj */
void set_armour_obj(OBJ_DATA *obj)
{
    int armour;
    int armour_exotic;

    armour = calc_obj_armour(obj->level, obj->value[4]) ;
    armour_exotic = armour * 9/10;

    obj->value[0] = armour;
    obj->value[1] = armour;
    obj->value[2] = armour;
    obj->value[3] = armour_exotic;
}


void do_mpdelete(CHAR_DATA *ch, char *argument)
{
}


void do_opdelete(CHAR_DATA *ch, char *argument)
{
}


void do_rpdelete(CHAR_DATA *ch, char *argument)
{
}


void do_dislink(CHAR_DATA *ch, char *argument)
{
    char arg[MSL];
    char buf[MSL];
    ROOM_INDEX_DATA *room;

    argument = one_argument(argument, arg);
    if (arg[0] == '\0' || !is_number(arg))
    {
	send_to_char("Syntax: dislink <room vnum> [junk]\n\r", ch);
	return;
    }

    if ((room = get_room_index(atol(arg))) == NULL)
    {
	send_to_char("There is no such room.\n\r", ch);
	return;
    }

    if (!has_access_area(ch, room->area))
    {
	send_to_char("Insufficient security to edit area - action logged.\n\r", ch);
	sprintf(buf, "do_dislink: %s tried to dislink %s (vnum %ld) in area %s without permissions!",
	    ch->name,
	    room->name,
	    room->vnum,
	    room->area->name);
	log_string(buf);
	return;
    }

    if (dislink_room(room))
	SET_BIT(room->area->area_flags, AREA_CHANGED);

    if (!str_cmp(argument, "junk")) {
	free_string(room->name);
	room->name = str_dup("NULL");
    }

    sprintf(buf, "Dislinked room %s (%ld)\n\r", room->name, room->vnum);
    send_to_char(buf, ch);
}


bool has_access_helpcat(CHAR_DATA *ch, HELP_CATEGORY *hcat)
{
    if (IS_NPC(ch))
	return false;

    if (ch->pcdata->security >= 9)
    	return true;

    if (strstr(hcat->builders, ch->name)
    ||  strstr(hcat->builders, "All"))
	return true;

    if (hcat->security < 9 && ch->pcdata->security > hcat->security)
	return true;

    return false;
}


bool has_access_help(CHAR_DATA *ch, HELP_DATA *help)
{
    if (IS_NPC(ch))
	return false;

    if (ch->pcdata->security >= 9)
    	return true;

    if (strstr(help->builders, ch->name)
    ||  strstr(help->builders, "All")
    ||  strstr(help->hCat->builders, ch->name)
    ||  strstr(help->hCat->builders, "All"))
	return true;

    if (help->security < 9 && help->hCat->security < 9
    &&  ch->pcdata->security >= help->security
    &&  ch->pcdata->security >= help->hCat->security)
	return true;

    return false;
}


void do_rjunk(CHAR_DATA *ch, char *argument)
{
    char buf[MSL];
    long vnum;
    AREA_DATA *area;
    ROOM_INDEX_DATA *room;
    bool changed = false;

    if (ch->in_room == NULL)
	return;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax: rjunk <room name>\n\r", ch);
	return;
    }

    area = ch->in_room->area;

    if (!has_access_area(ch, area))
    {
	send_to_char("Insufficient security to edit area - action logged.\n\r", ch);
	sprintf(buf, "do_dislink: %s tried to rjunk in area %s without permissions!",
	    ch->name,
	    area->name);
	log_string(buf);
	return;
    }

    for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
    {
	if ((room = get_room_index(vnum)) != NULL
	&&     !str_cmp(room->name, argument))
	{
	    dislink_room(room);
	    free_string(room->name);
	    room->name = str_dup("Null");
	    changed = true;
	}
    }

    if (changed)
	SET_BIT(area->area_flags, AREA_CHANGED);

    send_to_char("Done.\n\r", ch);
}


// Check a obj or a mob for an imp sig.
bool has_imp_sig(MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj)
{
    if (mob == NULL && obj == NULL)
    {
	bug("check_imp_sig: both mob and obj were null.", 0);
	return false;
    }

    if (mob != NULL && obj != NULL)
    {
	bug("check_imp_sig: had both mob and obj.", 0);
	return false;
    }

    if (mob != NULL)
    {
	if (mob->sig == NULL
	||  !str_cmp(mob->sig, "(none)")
	||  !str_cmp(mob->sig, "none")
	||  !str_cmp(mob->sig, "(null)"))
	    return false;
    }

    if (obj != NULL)
    {
	if (obj->imp_sig == NULL
	||  !str_cmp(obj->imp_sig, "(none)")
	||  !str_cmp(obj->imp_sig, "none")
	||  !str_cmp(obj->imp_sig, "(null)"))
	    return false;
    }

    return true;
}


void use_imp_sig(MOB_INDEX_DATA *mob, OBJ_INDEX_DATA *obj)
{
    if (mob == NULL && obj == NULL)
    {
	bug("use_imp_sig: both mob and obj were null.", 0);
	return;
    }

    if (mob != NULL && obj != NULL)
	bug("use_imp_sig: had both mob and obj.", 0);

    if (mob != NULL)
    {
	if (mob->sig == NULL)
	    return;
	else
	{
	    free_string(mob->sig);
	    mob->sig = str_dup("none");
	}
    }

    if (obj != NULL)
    {
	if (obj->imp_sig == NULL)
	    return;
	else
	{
	    free_string(obj->imp_sig);
	    obj->imp_sig = str_dup("none");
	}
    }
}


void do_tshow(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index;
    void *old_edit;
    long value;

    if (argument[0] == '\0')
    {
	send_to_char("Syntax:  tshow <vnum>\n\r", ch);
	return;
    }

    if (!is_number(argument))
    {
       send_to_char("Vnum must be a number.\n\r", ch);
       return;
    }

    value = atol(argument);
    if (!(token_index = get_token_index(value)))
    {
	send_to_char("That token does not exist.\n\r", ch);
	return;
    }

    old_edit = ch->desc->pEdit;
    ch->desc->pEdit = (void *) token_index;
    tedit_show(ch, argument);
    ch->desc->pEdit = old_edit;
}


void do_tlist(CHAR_DATA *ch, char *argument)
{
    TOKEN_INDEX_DATA *token_index;
    AREA_DATA *pArea;
    BUFFER *buf1;
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    bool fAll, found;
    long vnum;
    int col = 0;

    one_argument(argument, arg);

    pArea = ch->in_room->area;
    buf1  = new_buf();
    fAll  = arg[0] == '\0';
    found = false;

    for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
    {
	if ((token_index = get_token_index(vnum)))
	{
	    if (fAll || is_name(arg, token_index->name))
	    {
		found = true;
		sprintf(buf, "{Y[{x%5ld{Y]{x %-17.16s{x",
		    token_index->vnum, token_index->name);
		add_buf(buf1, buf);
		if (++col % 3 == 0)
		    add_buf(buf1, "\n\r");
	    }
	}
    }

    if (!found)
    {
	send_to_char("Token(s) not found in this area.\n\r", ch);
	return;
    }

    if (col % 3 != 0)
	add_buf(buf1, "\n\r");

    page_to_char(buf_string(buf1), ch);
    free_buf(buf1);
}

SHOP_STOCK_DATA *get_shop_stock_bypos(SHOP_DATA *shop, int nth)
{
	if(!shop || !shop->stock || nth < 1 ) return NULL;

	SHOP_STOCK_DATA *stock;

	for(stock = shop->stock; stock; stock = stock->next)
	{
		if(!--nth)
			return stock;
	}

	return NULL;


}

void olc_show_progs(BUFFER *buffer, LLIST **progs, int type, const char *title)
{
	char buf[MSL];
	int cnt, slot;

	for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
		if(list_size(progs[slot]) > 0) ++cnt;

	if (cnt > 0) {
		sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "Number", "MobProg Vnum", "Trigger", "Phrase");
		add_buf(buffer, buf);

		sprintf(buf, "{R%-6s %-20s %-10s %-10s\n\r{x", "------", "-------------", "-------", "------");
		add_buf(buffer, buf);

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
            ITERATOR it;
            PROG_LIST *trigger;
			iterator_start(&it, progs[slot]);
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

/* Used for handling projects. */
