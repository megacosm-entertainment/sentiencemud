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

bool is_stat(const struct flag_type *flag_table);
extern GLOBAL_DATA gconfig;
/*
 *  * Local functions.
 *   */
AREA_DATA *get_area_data args ((long anum));
AREA_DATA *get_area_from_uid args ((long uid));
void save_liquids();
void save_skills();
void save_songs();

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
	"SkEdit",
	"LiqEdit",
	"SgEdit",
	"SongEdit",
	"RepEdit",
};

int editor_max_tabs_table[] = {
	0,		// -----
	0,		// AEdit
	0,		// REdit
	0,		// OEdit
	0,		// MEdit
	0,		// MpEdit
	0,		// OpEdit
	0,		// RpEdit
	0,		// ShEdit
	0,		// HEdit
	0,		// TpEdit
	0,		// TEdit
	0,		// PEdit
	0,		// RSGEdit
	0,		// WEdit
	0,		// VLEdit
	0,		// BSEdit
	0,		// BPEdit
	0,		// DNGEdit
	0,		// ApEdit
	0,		// IpEdit
	0,		// DpEdit
	5,		// SkEdit
	0,		// LiqEdit
	0,		// SgEdit
	0,		// SongEdit
	0,		// RepEdit
};

const struct editor_cmd_type editor_table[] =
{
	{ "area",		do_aedit	},
	{ "room",		do_redit	},
	{ "object",		do_oedit	},
	{ "mobile",		do_medit	},
	{ "mpcode",		do_mpedit	},
	{ "opcode",		do_opedit	},
	{ "rpcode",		do_rpedit	},
	{ "ship",		do_shedit	},
	{ "help",		do_hedit	},
	{ "token",		do_tedit	},
	{ "tpcode",		do_tpedit	},
	{ "project",	do_pedit	},
	{ "bpsect",		do_bsedit	},
	{ "blueprint",	do_bpedit	},
	{ "dungeon",	do_dngedit	},
	{ "apcode",		do_apedit	},
	{ "ipcode",		do_ipedit	},
	{ "dpcode",		do_dpedit	},
	{ "skill",		do_skedit	},
	{ "liquid",		do_liqedit	},
	{ "skillgroup",	do_sgedit	},
	{ "song",		do_songedit },
	{ "reputation",	do_repedit	},
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
	{	"name",			aedit_name			},
	{	"open",			aedit_open			},
	{	"placetype",    aedit_placetype		},
	{	"postoffice",   aedit_postoffice	},
	{	"recall",		aedit_recall		},
	{	"regions",		aedit_regions		},
	{	"removetrade",  aedit_remove_trade	},
	{	"repop",		aedit_repop			},
	{	"savage",		aedit_savage		},
	{	"security",		aedit_security		},
	{	"settrade",		aedit_set_trade		},
	{	"show",			aedit_show			},
	{	"varclear",		aedit_varclear		},
	{	"varset",		aedit_varset		},
	{	"viewtrade",	aedit_view_trade	},
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
	{	"region",	redit_region				},
	{	"room",		redit_room					},
	{	"savage",	redit_savage				},
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
	{ "book", 			oedit_type_book			},
	{ "commands",		show_commands			},
	{ "comments",		oedit_comments			},
	{ "condition",		oedit_condition			},
	{ "container",		oedit_type_container	},
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
	{ "fluidcon",		oedit_type_fluid_container },
	{ "food",			oedit_type_food			},
	{ "fragility",		oedit_fragility			},
	{ "furniture",		oedit_type_furniture	},
	{ "level",			oedit_level				},
	{ "light",			oedit_type_light		},
	{ "lock",			oedit_lock				},
	{ "long",			oedit_long				},
	{ "material",		oedit_material			},
	{ "money",			oedit_type_money		},
	{ "name",			oedit_name				},
	{ "next",			oedit_next				},
	{ "oupdate",		oedit_update			},
	{ "page",			oedit_type_page			},
	{ "persist",		oedit_persist			},
	{ "portal",			oedit_type_portal		},
	{ "prev",			oedit_prev				},
	{ "scriptkwd",		oedit_skeywds			},
	{ "scroll",			oedit_type_scroll		},
	{ "short",			oedit_short				},
	{ "show",			oedit_show				},
	{ "sign",			oedit_sign				},
	{ "tattoo",			oedit_type_tattoo		},
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
	{ "wand",			oedit_type_wand			},
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
	{"placetype", wedit_placetype},
	{"region", wedit_region},
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
	{   "addmprog",	medit_addmprog  },
	{   "affect",       medit_affect    },
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
	{	"corpse",	medit_corpse	},
	{	"corpsetype",	medit_corpsetype	},
	{	"zombie",	medit_zombie	},
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
	if (d->incomm[0] == '\'' || d->incomm[0] == '"') return FALSE;

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
	case ED_SKEDIT:
		skedit(d->character, d->incomm);
		break;
	case ED_LIQEDIT:
		liqedit(d->character, d->incomm);
		break;
	case ED_SGEDIT:
		sgedit(d->character, d->incomm);
		break;
	case ED_SONGEDIT:
		songedit(d->character, d->incomm);
		break;
	case ED_REPEDIT:
		repedit(d->character, d->incomm);
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


// Return the edit name of character's editor (%o in prompt)
char *olc_ed_name(CHAR_DATA *ch)
{
	if(ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
		return editor_name_table[ch->desc->editor];

	return editor_name_table[0];
}

int olc_ed_tabs(CHAR_DATA *ch)
{
	if(ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
		return editor_max_tabs_table[ch->desc->editor];

	return 0;
}

void olc_set_editor(CHAR_DATA *ch, int editor, void *data)
{
	ch->desc->pEdit = data;
	ch->desc->editor = editor;
	ch->desc->nEditTab = 0;
	ch->desc->nMaxEditTabs = olc_ed_tabs(ch);
}

void olc_show_item(CHAR_DATA *ch, void *data, OLC_FUN *show_fun, char *argument)
{
	int old_tab = ch->desc->nEditTab;
	void *old_data = ch->desc->pEdit;

	ch->desc->nEditTab = 0;
	ch->desc->pEdit = data;

	(*show_fun)(ch, argument);

	ch->desc->nEditTab = old_tab;
	ch->desc->pEdit = old_data;
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
	SKILL_DATA *skill;
	SKILL_GROUP *group;
	LIQUID *liquid;
	SONG_DATA *song;
	REPUTATION_INDEX_DATA *rep;
	static char buf[20];
	char buf2[MSL];

	buf[0] = '\0';
	switch (ch->desc->editor)
	{
	case ED_AREA:
		pArea = (AREA_DATA *)ch->desc->pEdit;
		sprintf(buf, "%ld", pArea ? pArea->uid : 0);
		break;
	case ED_ROOM:
		pRoom = ch->in_room;
		sprintf(buf, "%ld#%ld", pRoom ? pRoom->area->uid : 0, pRoom ? pRoom->vnum : 0);
		break;
	case ED_OBJECT:
		pObj = (OBJ_INDEX_DATA *)ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", pObj ? pObj->area->uid : 0, pObj ? pObj->vnum : 0);
		break;
	case ED_MOBILE:
		pMob = (MOB_INDEX_DATA *)ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", pMob ? pMob->area->uid : 0, pMob ? pMob->vnum : 0);
		break;
	case ED_MPCODE:
	case ED_OPCODE:
	case ED_RPCODE:
	case ED_TPCODE:
	case ED_APCODE:
	case ED_IPCODE:
	case ED_DPCODE:
		prog = (SCRIPT_DATA *)ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", (prog ? prog->area->uid : 0), (prog ? prog->vnum : 0));
		break;
	case ED_SKEDIT:
		skill = (SKILL_DATA *)ch->desc->pEdit;
		if (skill)
			sprintf(buf, "%s:%d", skill->name, skill->uid);
		else
			sprintf(buf, "--:--");
		break;
	case ED_LIQEDIT:
		liquid = (LIQUID *)ch->desc->pEdit;
		if (IS_VALID(liquid))
			sprintf(buf, "%s:%d", liquid->name, liquid->uid);
		else
			sprintf(buf, "--:--");
		break;
	case ED_SGEDIT:
		group = (SKILL_GROUP *)ch->desc->pEdit;
		if (group)
			sprintf(buf, "%s", group->name);
		else
			sprintf(buf, "--");
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
		sprintf(buf, "%ld#%ld", pShip ? pShip->area->uid : 0, pShip ? pShip->vnum : 0);
		break;

	case ED_TOKEN:
		pTokenIndex = (TOKEN_INDEX_DATA *) ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", pTokenIndex ? pTokenIndex->area->uid : 0, pTokenIndex ? pTokenIndex->vnum : 0);
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
		sprintf(buf, "%ld#%ld", bpsect ? bpsect->area->uid : 0, bpsect ? bpsect->vnum : 0);
		break;

	case ED_BLUEPRINT:
		blueprint = (BLUEPRINT *)ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", blueprint ? blueprint->area->uid : 0, blueprint ? blueprint->vnum : 0);
		break;

	case ED_DUNGEON:
		dungeon = (DUNGEON_INDEX_DATA*)ch->desc->pEdit;
		sprintf(buf, "%ld#%ld", dungeon ? dungeon->area->uid : 0, dungeon ? dungeon->vnum : 0);
		break;
	
	case ED_SONGEDIT:
		song = (SONG_DATA *)ch->desc->pEdit;
		if (song)
			sprintf(buf, "%s:%d", song->name, song->uid);
		else
			strcpy(buf, "--");
		break;

	case ED_REPEDIT:
		rep = (REPUTATION_INDEX_DATA *)ch->desc->pEdit;
		if (IS_VALID(rep))
			sprintf(buf, "%ld#%ld", rep->area->uid, rep->vnum);
		else
			strcpy(buf, "--");
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

	case ED_SKEDIT:
		show_olc_cmds(ch, skedit_table);
		break;

	case ED_LIQEDIT:
		show_olc_cmds(ch, liqedit_table);
		break;

	case ED_SGEDIT:
		show_olc_cmds(ch, sgedit_table);
		break;

	case ED_SONGEDIT:
		show_olc_cmds(ch, songedit_table);
		break;

	case ED_REPEDIT:
		show_olc_cmds(ch, repedit_table);
		break;
	}

	return FALSE;
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
	ch->desc->nEditTab = 0;
	ch->desc->nMaxEditTabs = 0;
	ch->desc->hCat = NULL;
	ch->desc->editor = 0;
	return FALSE;
}


bool has_access_area(CHAR_DATA *ch, AREA_DATA *area)
{
	if (ch->tot_level == MAX_LEVEL)
	return TRUE;

	if (!IS_BUILDER(ch, area))
	return FALSE;

	return TRUE;
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
		projects_changed = TRUE;
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
	WNUM wnum;
	char arg[MAX_STRING_LENGTH];

	if (IS_NPC(ch))
		return;

	argument = one_argument(argument,arg);

	if (parse_widevnum(arg, ch->in_room->area, &wnum))
	{
		if ((token_index = get_token_index(wnum.pArea, wnum.vnum)) == NULL)
		{
			send_to_char("That token vnum does not exist.\n\r", ch);
			return;
		}
	}
	else if (!str_cmp(arg, "create"))
	{
		if (tedit_create(ch, argument))
			ch->desc->editor = ED_TOKEN;

		return;
	}
	else
	{
		send_to_char(
		"Syntax: tedit <widevnum>\n\r"
		"        tedit create <widevnum>\n\r", ch);
		return;
	}

	ch->pcdata->immortal->last_olc_command = current_time;
	olc_set_editor(ch, ED_TOKEN, token_index);
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
	if (!(pArea = get_area_from_uid(value)))
	{
		send_to_char("That area uid does not exist.\n\r", ch);
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
		// TODO: Immortal Roles system
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
	olc_set_editor(ch, ED_AREA, pArea);
}


void do_redit(CHAR_DATA *ch, char *argument)
{
//	char buf[MSL];
	ROOM_INDEX_DATA *pRoom;
	char arg1[MAX_STRING_LENGTH];
	WNUM wnum;

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
	else if (!str_cmp(arg1, "create"))
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
	else if (parse_widevnum(arg1, ch->in_room->area, &wnum))	/* redit <widevnum> */
	{
		pRoom = get_room_index(wnum.pArea, wnum.vnum);

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

		// Set the last OLC stuff to the current room you are editting when you explicitly do "redit <widevnum>"
		if (IS_SET(ch->act[0], PLR_AUTOOLC))
		{
//			sprintf(buf, "AUTOOLC: saving area (%s), region (%s), sector (%s), room (%s), room2 (%s)\n\r",
//				pRoom->area->name,
//				pRoom->region ? pRoom->region->name : "(null)",
//				flag_string(sector_flags, pRoom->sector_type),
//				flag_string(room_flags, pRoom->room_flag[0]),
//				flag_string(room2_flags, pRoom->room_flag[1]));
//			send_to_char(buf, ch);
			ch->desc->last_area = pRoom->area;
			ch->desc->last_area_region = pRoom->region;
			ch->desc->last_room_sector = pRoom->sector_type;
			ch->desc->last_room_flag[0] = pRoom->room_flag[0];
			ch->desc->last_room_flag[1] = pRoom->room_flag[1];
		}
	}
	else if(pRoom && IS_SET(pRoom->room_flag[1],ROOM_VIRTUAL_ROOM))
	{
		send_to_char("REdit : Virtual rooms may not be editted.\n\r", ch);
		return;
	}
	else if (!IS_BUILDER(ch, pRoom->area))
	{
		send_to_char("REdit : Insuficient security to edit room - action logged.\n\r", ch);
		return;
	}
	else
	{
		// Set the last OLC stuff to the current room you are editting when you explicitly do "redit <widevnum>"
		if (IS_SET(ch->act[0], PLR_AUTOOLC))
		{
//			sprintf(buf, "AUTOOLC: saving area (%s), region (%s), sector (%s), room (%s), room2 (%s)\n\r",
//				pRoom->area->name,
//				pRoom->region ? pRoom->region->name : "(null)",
//				flag_string(sector_flags, pRoom->sector_type),
//				flag_string(room_flags, pRoom->room_flag[0]),
//				flag_string(room2_flags, pRoom->room_flag[1]));
//			send_to_char(buf, ch);
			ch->desc->last_area = pRoom->area;
			ch->desc->last_area_region = pRoom->region;
			ch->desc->last_room_sector = pRoom->sector_type;
			ch->desc->last_room_flag[0] = pRoom->room_flag[0];
			ch->desc->last_room_flag[1] = pRoom->room_flag[1];
		}
	}

	ch->pcdata->immortal->last_olc_command = current_time;
	ch->desc->pEdit	= (void *) pRoom;
	ch->desc->editor	= ED_ROOM;
}


void do_oedit(CHAR_DATA *ch, char *argument)
{
	OBJ_INDEX_DATA *pObj;
	char arg1[MAX_STRING_LENGTH];
	WNUM wnum;

	if (IS_NPC(ch))
	return;

	argument = one_argument(argument, arg1);

	if (parse_widevnum(arg1, ch->in_room->area, &wnum))
	{
		if (!(pObj = get_obj_index(wnum.pArea, wnum.vnum)))
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
		olc_set_editor(ch, ED_OBJECT, pObj);
	}
	else
	{
	if (!str_cmp(arg1, "create"))
	{
		if (oedit_create(ch, argument))
			ch->desc->editor = ED_OBJECT;
	}
	}
}


void do_medit(CHAR_DATA *ch, char *argument)
{
	MOB_INDEX_DATA *pMob;
	char arg1[MAX_STRING_LENGTH];
	WNUM wnum;

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (parse_widevnum(arg1, ch->in_room->area, &wnum))
	{
		if (!(pMob = get_mob_index(wnum.pArea, wnum.vnum)))
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
		olc_set_editor(ch, ED_MOBILE, pMob);
		return;
	}
	else
	{
	if (!str_cmp(arg1, "create"))
	{
		if (medit_create(ch, argument))
			ch->desc->editor = ED_MOBILE;
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
	olc_set_editor(ch, ED_PROJECT, project);
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

		olc_set_editor(ch, ED_WILDS, pWilds);
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
	olc_set_editor(ch, ED_WILDS, pWilds);
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
	bool found = FALSE;

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
			found = TRUE;
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
		olc_set_editor(ch, ED_VLINK, pVLink);
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

	send_to_char(" No.  Loads    Description       Location         Vnum   Mx Mn Description\n\r", ch);
	send_to_char("==== ======== ============= =================== ======== ===== ===========\n\r", ch);

	for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
	{
		OBJ_INDEX_DATA  *pObj = NULL;
		MOB_INDEX_DATA  *pMobIndex = NULL;
		OBJ_INDEX_DATA  *pObjIndex = NULL;
		OBJ_INDEX_DATA  *pObjToIndex = NULL;

		final[0] = '\0';
		sprintf(final, "[%2d] ", ++iReset);

		switch (pReset->command)
		{
			default:
				sprintf(buf, "Bad reset command: %c.", pReset->command);
				strcat(final, buf);
				break;

			case 'M':
				if (!(pMobIndex = get_mob_index(pReset->arg1.wnum.pArea, pReset->arg1.wnum.vnum)))
				{
					sprintf(buf, "Load Mobile - Bad Mob %s\n\r", widevnum_string_wnum(pReset->arg1.wnum, pRoom->area));
					strcat(final, buf);
					continue;
				}

				pMob = pMobIndex;
				sprintf(buf, "M[%s] %-13.13s in room                      %2ld-%2ld %-15.15s\n\r",
					widevnum_string_wnum(pReset->arg1.wnum, pRoom->area), pMob->short_descr,
					pReset->arg2, pReset->arg4, pRoom->name);
				strcat(final, buf);
				break;

			case 'O':
				if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea, pReset->arg1.wnum.vnum)))
				{
					sprintf(buf, "Load Object - Bad Object %s\n\r", widevnum_string_wnum(pReset->arg1.wnum, pRoom->area));
					strcat(final, buf);
					continue;
				}

				pObj       = pObjIndex;

				sprintf(buf, "O[%s] %-13.13s in room                            %-15.15s\n\r",
					widevnum_string_wnum(pReset->arg1.wnum, pRoom->area),
					pObj->short_descr,
					pRoom->name);
				strcat(final, buf);
				break;

			case 'P':
				if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea, pReset->arg1.wnum.vnum)))
				{
					sprintf(buf, "Put Object - Bad Object %s\n\r", widevnum_string_wnum(pReset->arg1.wnum, pRoom->area));
					strcat(final, buf);
					continue;
				}

				pObj       = pObjIndex;

				if (!(pObjToIndex = get_obj_index(pReset->arg3.wnum.pArea, pReset->arg3.wnum.vnum)))
				{
					sprintf(buf, "Put Object - Bad To Object %s\n\r", widevnum_string_wnum(pReset->arg3.wnum, pRoom->area));
					strcat(final, buf);
					continue;
				}

				sprintf(buf, "O[%s] %-13.13s inside              O[%s] %2ld-%2ld %-15.15s\n\r",
					widevnum_string_wnum(pReset->arg1.wnum, pRoom->area),
					pObj->short_descr,
					widevnum_string_wnum(pReset->arg3.wnum, pRoom->area),
					pReset->arg2,
					pReset->arg4,
					pObjToIndex->short_descr);
				strcat(final, buf);
				break;

			case 'G':
			case 'E':
				if (!(pObjIndex = get_obj_index(pReset->arg1.wnum.pArea, pReset->arg1.wnum.vnum)))
				{
					sprintf(buf, "Give/Equip Object - Bad Object %s\n\r", widevnum_string_wnum(pReset->arg1.wnum, pRoom->area));
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
					sprintf(buf, "O[%s] %-13.13s in the inventory of S[%s]       %-15.15s\n\r",
						widevnum_string_wnum(pReset->arg1.wnum, pRoom->area),
						pObj->short_descr,
						widevnum_string(pMob->area, pMob->vnum, pRoom->area),
						pMob->short_descr );
				else
					sprintf(buf, "O[%s] %-13.13s %-19.19s M[%s]       %-15.15s\n\r",
						widevnum_string_wnum(pReset->arg1.wnum, pRoom->area),
						pObj->short_descr,
						(pReset->command == 'G') ?
							flag_string(wear_loc_strings, WEAR_NONE) :
							flag_string(wear_loc_strings, pReset->arg4),
						widevnum_string(pMob->area, pMob->vnum, pRoom->area),
						pMob->short_descr);
				strcat(final, buf);
				break;

			case 'R':
				sprintf(buf, "Exits are randomized\n\r");
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
	char arg3[MAX_INPUT_LENGTH];		WNUM wnum3;
	char arg4[MAX_INPUT_LENGTH];
	char arg5[MAX_INPUT_LENGTH];		WNUM wnum5;
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
	else {
		send_to_char("No resets in this room.\n\r", ch);
	}

		return;
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
	if ((!str_cmp(arg2, "mob") && parse_widevnum(arg3, ch->in_room->area, &wnum3))
	  || (!str_cmp(arg2, "obj") && parse_widevnum(arg3, ch->in_room->area, &wnum3)))
	{
		if (!str_cmp(arg2, "mob"))
		{
			if (get_mob_index(wnum3.pArea, wnum3.vnum) == NULL)
			{
				send_to_char("Mob no existe.\n\r",ch);
				return;
			}
			pReset = new_reset_data();
			pReset->command = 'M';
			pReset->arg1.wnum = wnum3;
			pReset->arg2 = is_number(arg4) ? atol(arg4) : 1; /* Max # */
			pReset->arg4 = is_number(arg5) ? atol(arg5) : 1; /* Min # */
		}
		else if (!str_cmp(arg2, "obj"))
		{
			if (get_obj_index(wnum3.pArea, wnum3.vnum) == NULL)
			{
				send_to_char("Object does not exist.\n\r",ch);
				return;
			}
			pReset = new_reset_data();
			pReset->arg1.wnum = wnum3;
			if (!str_prefix(arg4, "inside"))
			{
				OBJ_INDEX_DATA *temp;

				if (!parse_widevnum(arg5, ch->in_room->area, &wnum5))
				{
					send_to_char("Object 2 not found!\n\r", ch);
					return;	
				}

				temp = get_obj_index(wnum5.pArea, wnum5.vnum);
				if (temp == NULL) {
					free_reset_data(pReset);	// Added this because memory leak.
					send_to_char("Object 2 not found!\n\r", ch);
					return;
				}

				// TODO: Add other container types, like weapon container and keyring
				if ((!IS_CONTAINER(temp)) &&
					(temp->item_type != ITEM_CORPSE_NPC))
				{
					send_to_char("Object 2 isn't a container.\n\r", ch);
					return;
				}
				pReset->command = 'P';
				pReset->arg2    = is_number(arg6) ? atol(arg6) : 1;
				pReset->arg3.wnum = wnum5;
				pReset->arg4    = is_number(arg7) ? atol(arg7) : 1;
			}
			else if (!str_cmp(arg4, "room"))
			{
				pReset->command  = 'O';
				pReset->arg2     = 0;
				pReset->arg4     = 0;
			}
			else
			{
				long value;
				if ((value = stat_lookup(arg4, wear_loc_flags, NO_FLAG)) == NO_FLAG)
				{
					send_to_char("Resets: '? wear-loc'\n\r", ch);
					return;
				}

				pReset->arg4 = value;

				if (pReset->arg4 == WEAR_NONE)
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

	sprintf(result, "[%3s] [%-27s] [%-10s] %3s [%-9s]\n\r",
	   "UID", "Area Name", "Filename", "Sec", "Builders");


	for (pArea = area_first; pArea; pArea = pArea->next)
	{
	if (!str_infix(arg, pArea->name))
	{
		sprintf(buf,
		"[%3ld] %-27.27s %-12.12s [%d] [%-10.10s]\n\r",
			pArea->uid,
		pArea->name,
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

	sprintf(buf, "[%-7s] [%-7s] [%-26.26s] [%-10s] %3s [%-10s]\n\r",
	   "Anum", "UID", "Area Name", "Filename", "Sec", "Builders");
	add_buf(buffer, buf);

	if (argument[0] != '\0' && (place_type = flag_value(place_flags, argument)) == NO_FLAG)
	{
		send_to_char("Syntax: alist\n\r", ch);
		send_to_char("        alist <placetype>\n\r", ch);
		return;
	}

	for (pArea = area_first; pArea; pArea = pArea->next)
	{
		if (place_type == 0 || (pArea->region.place_flags == place_type))
		{
			sprintf(buf, "{D[{x%7ld{D]{x {D[{x%7ld{D]{x %s%-26.26s{x %-12.12s {D[{x{B%d{x{D]{x {D[{x%-10.10s{D]{x \n\r",
				pArea->anum,
				pArea->uid,
				pArea->open ? "{G" : "{R",
				pArea->name,
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
		olc_set_editor(ch, ED_HELP, NULL);	// We're editing a helpfile.
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
	olc_set_editor(ch, ED_HELP, NULL);
	ch->desc->hCat = topHelpCat;
}


/*
 * Copy a room.
 */
void do_rcopy(CHAR_DATA *ch, char *argument)
{
	ROOM_INDEX_DATA *old_room;
	ROOM_INDEX_DATA *new_room;
	EXTRA_DESCR_DATA *ed;
	EXTRA_DESCR_DATA *new_ed;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];
	WNUM old_w;
	WNUM new_w;
	int iHash;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: rcopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

	if (get_room_index(old_w.pArea, old_w.vnum) == NULL)
	{
	 	send_to_char("That room doesn't exist.\n\r", ch);
		return;
	}

	if (get_room_index(new_w.pArea, new_w.vnum) != NULL)
	{
	 	send_to_char("That room vnum is already taken.\n\r", ch);
		return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
		send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
		return;
	}


	if (!IS_BUILDER(ch, new_w.pArea))
	{
		send_to_char("You can't build in that area.\n\r", ch);
		return;
	}

	edit_done(ch);

	ch->pcdata->immortal->last_olc_command = current_time;
	old_room = get_room_index(old_w.pArea, old_w.vnum);
	new_room = new_room_index();

	new_room->area                 = new_w.pArea;
	list_appendlink(new_w.pArea->room_list, new_room);	// Add to the area room list

	new_room->vnum                 = new_w.vnum;
	if (new_w.vnum > new_w.pArea->top_vnum_room)
		new_w.pArea->top_vnum_room = new_w.vnum;

	iHash                       = new_w.vnum % MAX_KEY_HASH;
	new_room->next              = new_w.pArea->room_index_hash[iHash];
	new_w.pArea->room_index_hash[iHash]      = new_room;
	olc_set_editor(ch, ED_ROOM, new_room);

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

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);
	send_to_char("Room copied.\n\r", ch);
}


/*
 * Copy a mob.
 */
void do_mcopy(CHAR_DATA *ch, char *argument)
{
	MOB_INDEX_DATA *old_mob;
	MOB_INDEX_DATA *new_mob;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];
	WNUM old_w;
	WNUM new_w;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: mcopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

	if ((old_mob = get_mob_index(old_w.pArea, old_w.vnum)) == NULL)
	{
		send_to_char("That mob doesn't exist.\n\r", ch);
		return;
	}

	if (get_mob_index(new_w.pArea, new_w.vnum) != NULL)
	{
		send_to_char("That mob widevnum is already taken.\n\r", ch);
	return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
		send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
		return;
	}

	if (!IS_BUILDER(ch, new_w.pArea))
	{
		send_to_char("You can't build in that area.\n\r", ch);
		return;
	}

	edit_done(ch);

	ch->pcdata->immortal->last_olc_command = current_time;
	new_mob       = new_mob_index();
	new_mob->vnum = new_w.vnum;
	new_mob->area = new_w.pArea;

	if (new_w.vnum > new_w.pArea->top_vnum_mob)
		new_w.pArea->top_vnum_mob = new_w.vnum;

	int iHash			= new_w.vnum % MAX_KEY_HASH;
	new_mob->next		= new_w.pArea->mob_index_hash[iHash];
	new_w.pArea->mob_index_hash[iHash]	= new_mob;
	olc_set_editor(ch, ED_MOBILE, new_mob);

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
	new_mob->corpse_type = old_mob->corpse_type;
	new_mob->corpse = old_mob->corpse;
	new_mob->zombie = old_mob->zombie;

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);
	send_to_char("Mobile copied.\n\r", ch);
}


/*
 * Copy an obj.
 */
void do_ocopy(CHAR_DATA *ch, char *argument)
{
	OBJ_INDEX_DATA *old_obj;
	OBJ_INDEX_DATA *new_obj;
	EXTRA_DESCR_DATA *ed;
	EXTRA_DESCR_DATA *new_ed;
	AFFECT_DATA *af;
	AFFECT_DATA *new_af;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];
	int i;
	WNUM old_w;
	WNUM new_w;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: ocopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

	if ((old_obj = get_obj_index(old_w.pArea, old_w.vnum)) == NULL)
	{
		send_to_char("That obj doesn't exist.\n\r", ch);
		return;
	}

	if (get_obj_index(new_w.pArea, new_w.vnum) != NULL)
	{
		send_to_char("That obj vnum is already taken.\n\r", ch);
		return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
		send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
		return;
	}

	if (!IS_BUILDER(ch, new_w.pArea))
	{
		send_to_char("You can't build in that area.\n\r", ch);
		return;
	}

	edit_done(ch);

	ch->pcdata->immortal->last_olc_command = current_time;
	new_obj		= new_obj_index();
	new_obj->vnum	= new_w.vnum;
	new_obj->area	= new_w.pArea;

	if (new_w.vnum > new_w.pArea->top_vnum_obj)
		new_w.pArea->top_vnum_obj = new_w.vnum;

	int iHash			= new_w.vnum % MAX_KEY_HASH;
	new_obj->next		= new_w.pArea->obj_index_hash[iHash];
	new_w.pArea->obj_index_hash[iHash]	= new_obj;
	olc_set_editor(ch, ED_OBJECT, new_obj);

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
	new_af->skill  = af->skill;
	new_af->catalyst_type = af->catalyst_type;
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
	new_obj->wear_flags = old_obj->wear_flags;
	new_obj->level = old_obj->level;
	new_obj->condition = old_obj->condition;
	new_obj->count = old_obj->count;
	new_obj->weight = old_obj->weight;
	new_obj->cost = old_obj->cost;
	new_obj->fragility = old_obj->fragility;
	new_obj->times_allowed_fixed = old_obj->times_allowed_fixed;
	new_obj->comments = old_obj->comments;

	for (i = 0; i < MAX_OBJVALUES; i++)
		new_obj->value[i] = old_obj->value[i];

	/*
	if (old_obj->lock)
	{
		new_obj->lock = new_lock_state();
		new_obj->lock->key_wnum = old_obj->lock->key_wnum;
		new_obj->lock->pick_chance = old_obj->lock->pick_chance;
		new_obj->lock->flags = old_obj->lock->flags;

		new_obj->lock->keys = old_obj->lock->keys;		// Do I really need to do this?
	}
	*/

	BOOK(new_obj) = copy_book_data(BOOK(old_obj));
	CONTAINER(new_obj) = copy_container_data(CONTAINER(old_obj));
	FLUID_CON(new_obj) = copy_fluid_container_data(FLUID_CON(old_obj));
	FOOD(new_obj) = copy_food_data(FOOD(old_obj));
	FURNITURE(new_obj) = copy_furniture_data(FURNITURE(old_obj));
	LIGHT(new_obj) = copy_light_data(LIGHT(old_obj));
	MONEY(new_obj) = copy_money_data(MONEY(old_obj));
	PAGE(new_obj) = copy_book_page(PAGE(old_obj));
	PORTAL(new_obj) = copy_portal_data(PORTAL(old_obj), TRUE);

	// Only copy impsig if imp (to block cheaters)
	if (get_trust(ch) == MAX_LEVEL)
	new_obj->imp_sig = str_dup(old_obj->imp_sig);
	else
	new_obj->imp_sig = str_dup("none");

	new_obj->points = old_obj->points;

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);

	send_to_char("Object copied.\n\r", ch);
	//oedit_show(ch, "");
}


/*
 * Copy an rprog.
 */
void do_rpcopy(CHAR_DATA *ch, char *argument)
{
	WNUM old_w;
	WNUM new_w;
	SCRIPT_DATA *old_rpcode;
	SCRIPT_DATA *new_rpc;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: rpcopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

    if (!old_w.pArea) old_w.pArea = ch->in_room->area;
    if (!new_w.pArea) new_w.pArea = ch->in_room->area;

	if ((old_rpcode = get_script_index(old_w.pArea, old_w.vnum, PRG_RPROG)) == NULL)
	{
		send_to_char("That ROOMprog doesn't exist.\n\r", ch);
		return;
	}

	if (get_script_index(new_w.pArea, new_w.vnum, PRG_RPROG) != NULL)
	{
    	send_to_char("That ROOMprog vnum is already taken.\n\r", ch);
    	return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
    	send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
	    return;
	}

	if (!IS_BUILDER(ch, new_w.pArea))
	{
    	send_to_char("You can't build in that area.\n\r", ch);
    	return;
	}

	edit_done(ch);
	ch->pcdata->immortal->last_olc_command = current_time;
	new_rpc = new_script();
	new_rpc->vnum = new_w.vnum;
	new_rpc->edit_src = str_dup(old_rpcode->src);
	new_rpc->area = new_w.pArea;
	compile_script(NULL,new_rpc, new_rpc->edit_src, IFC_R);
	new_rpc->next = new_w.pArea->rprog_list;
	new_w.pArea->rprog_list = new_rpc;

	olc_set_editor(ch, ED_RPCODE, new_rpc);

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);
	send_to_char("RoomProgram code copied.\n\r",ch);
}


/*
 * Copy an mprog.
 */
void do_mpcopy(CHAR_DATA *ch, char *argument)
{
	WNUM old_w;
	WNUM new_w;
	SCRIPT_DATA *old_mpcode;
	SCRIPT_DATA *new_mpc;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: mpcopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

    if (!old_w.pArea) old_w.pArea = ch->in_room->area;
    if (!new_w.pArea) new_w.pArea = ch->in_room->area;

	if ((old_mpcode = get_script_index(old_w.pArea, old_w.vnum, PRG_MPROG)) == NULL)
	{
		send_to_char("That ROOMprog doesn't exist.\n\r", ch);
		return;
	}

	if (get_script_index(new_w.pArea, new_w.vnum, PRG_MPROG) != NULL)
	{
    	send_to_char("That MOBsprog vnum is already taken.\n\r", ch);
    	return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
    	send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
	    return;
	}

	if (!IS_BUILDER(ch, new_w.pArea))
	{
    	send_to_char("You can't build in that area.\n\r", ch);
    	return;
	}

	edit_done(ch);
	ch->pcdata->immortal->last_olc_command = current_time;
	new_mpc = new_script();
	new_mpc->vnum = new_w.vnum;
	new_mpc->edit_src = str_dup(old_mpcode->src);
	new_mpc->area = new_w.pArea;
	compile_script(NULL,new_mpc, new_mpc->edit_src, IFC_R);
	new_mpc->next = new_w.pArea->mprog_list;
	new_w.pArea->mprog_list = new_mpc;

	olc_set_editor(ch, ED_MPCODE, new_mpc);

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);
	send_to_char("MobProgram code copied.\n\r",ch);
}


/*
 * Copy an oprog.
 */
void do_opcopy(CHAR_DATA *ch, char *argument)
{
	WNUM old_w;
	WNUM new_w;
	SCRIPT_DATA *old_opcode;
	SCRIPT_DATA *new_opc;
	char arg[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (!parse_widevnum(arg, ch->in_room->area, &old_w) || !parse_widevnum(arg2, ch->in_room->area, &new_w))
	{
		send_to_char("Syntax: opcopy <old_wnum> <new_wnum>\n\r", ch);
		return;
	}

    if (!old_w.pArea) old_w.pArea = ch->in_room->area;
    if (!new_w.pArea) new_w.pArea = ch->in_room->area;

	if ((old_opcode = get_script_index(old_w.pArea, old_w.vnum, PRG_OPROG)) == NULL)
	{
		send_to_char("That ROOMprog doesn't exist.\n\r", ch);
		return;
	}

	if (get_script_index(new_w.pArea, new_w.vnum, PRG_OPROG) != NULL)
	{
    	send_to_char("That OBJprog vnum is already taken.\n\r", ch);
    	return;
	}

	if (!IS_BUILDER(ch, old_w.pArea))
	{
    	send_to_char("You're not a builder in that area, so you can't copy from it.\n\r", ch);
	    return;
	}

	if (!IS_BUILDER(ch, new_w.pArea))
	{
    	send_to_char("You can't build in that area.\n\r", ch);
    	return;
	}

	edit_done(ch);
	ch->pcdata->immortal->last_olc_command = current_time;
	new_opc = new_script();
	new_opc->vnum = new_w.vnum;
	new_opc->edit_src = str_dup(old_opcode->src);
	new_opc->area = new_w.pArea;
	compile_script(NULL,new_opc, new_opc->edit_src, IFC_R);
	new_opc->next = new_w.pArea->oprog_list;
	new_w.pArea->oprog_list = new_opc;

	olc_set_editor(ch, ED_OPCODE, new_opc);

	SET_BIT(new_w.pArea->area_flags, AREA_CHANGED);
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
	bool range = FALSE;
	long vnum;
	long vnum_min;
	long vnum_max;
	int col = 0;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (arg[0] != '\0' && arg2[0] != '\0')
		range = TRUE;

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

		vnum_min = UMAX(1, vnum_min);
		vnum_max = UMIN(pArea->top_vnum_room, vnum_max);
	}
	else
	{
	vnum_min = 1;
	vnum_max = pArea->top_vnum_room;
	}

	buf1  = new_buf();

	for (vnum = vnum_min; vnum <= vnum_max; vnum++)
	{
		if ((pRoomIndex = get_room_index(pArea, vnum)) != NULL)
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
	found = FALSE;

	for (vnum = 1; vnum <= pArea->top_vnum_mob; vnum++)
	{
		if ((pMobIndex = get_mob_index(pArea, vnum)) != NULL)
		{
			if (fAll || is_name(arg, pMobIndex->player_name))
			{
			char *noc;
			found = TRUE;
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
	found = FALSE;

	for (vnum = 1; vnum <= pArea->top_vnum_obj; vnum++)
	{
		if ((pObjIndex = get_obj_index(pArea, vnum)))
		{
			if (fAll || is_name(arg, pObjIndex->name) ||
				flag_value(type_flags, arg) == pObjIndex->item_type)
			{
				found = TRUE;
				max = strlen_colours_limit(pObjIndex->short_descr,16) + 17;
				sprintf(buf, "{x[%s] %s{x",
					MXPCreateSend(ch->desc, formatf("oedit %ld#%ld", pArea->uid, pObjIndex->vnum), formatf("%5ld", pObjIndex->vnum)),
					MXPCreateSend(ch->desc, formatf("oshow %ld#%ld", pArea->uid, pObjIndex->vnum), formatf("%-*.*s", max, max - 1, pObjIndex->short_descr)));
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
	WNUM wnum;

	if (argument[0] == '\0')
	{
	send_to_char("Syntax:  mshow <widevnum>\n\r", ch);
	return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
	   send_to_char("Please specify a widevnum.\n\r", ch);
	   return;
	}

	if (!(pMob = get_mob_index(wnum.pArea, wnum.vnum)))
	{
	   send_to_char("That mobile does not exist.\n\r", ch);
	   return;
	}

	olc_show_item(ch, pMob, medit_show, argument);
	return;
}


void do_oshow(CHAR_DATA *ch, char *argument)
{
	OBJ_INDEX_DATA *pObj;
	WNUM wnum;

	if (argument[0] == '\0')
	{
	send_to_char("Syntax:  oshow <widevnum>\n\r", ch);
	return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
	   send_to_char("Please specify a widevnum.\n\r", ch);
	   return;
	}

	if (!(pObj = get_obj_index(wnum.pArea, wnum.vnum)))
	{
	send_to_char("That object does not exist.\n\r", ch);
	return;
	}

	olc_show_item(ch, pObj, oedit_show, argument);
}


void do_rshow(CHAR_DATA *ch, char *argument)
{
	ROOM_INDEX_DATA *pRoom, *oldRoom;
	WNUM wnum;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  rshow <widevnum>\n\r", ch);
		return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
	   send_to_char("Please supply a widevnum.\n\r", ch);
	   return;
	}

	if (!(pRoom = get_room_index(wnum.pArea, wnum.vnum)))
	{
		send_to_char("That room does not exist.\n\r", ch);
		return;
	}

	oldRoom = ch->in_room;
	char_from_room(ch);
	char_to_room(ch, pRoom);

	olc_show_item(ch, pRoom, redit_show, argument);

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
	WNUM wnum;

	argument = one_argument(argument, arg);
	if (arg[0] == '\0' || !parse_widevnum(arg, ch->in_room->area, &wnum))
	{
	send_to_char("Syntax: dislink <room widevnum> [junk]\n\r", ch);
	return;
	}

	if ((room = get_room_index(wnum.pArea, wnum.vnum)) == NULL)
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
		SET_BIT(room->area->area_flags, AREA_CHANGED);
	}

	sprintf(buf, "Dislinked room %s (%ld)\n\r", room->name, room->vnum);
	send_to_char(buf, ch);
}


bool has_access_helpcat(CHAR_DATA *ch, HELP_CATEGORY *hcat)
{
	if (IS_NPC(ch))
	return FALSE;

	if (ch->pcdata->security >= 9)
		return TRUE;

	if (strstr(hcat->builders, ch->name)
	||  strstr(hcat->builders, "All"))
	return TRUE;

	if (hcat->security < 9 && ch->pcdata->security > hcat->security)
	return TRUE;

	return FALSE;
}


bool has_access_help(CHAR_DATA *ch, HELP_DATA *help)
{
	if (IS_NPC(ch))
	return FALSE;

	if (ch->pcdata->security >= 9)
		return TRUE;

	if (strstr(help->builders, ch->name)
	||  strstr(help->builders, "All")
	||  strstr(help->hCat->builders, ch->name)
	||  strstr(help->hCat->builders, "All"))
	return TRUE;

	if (help->security < 9 && help->hCat->security < 9
	&&  ch->pcdata->security >= help->security
	&&  ch->pcdata->security >= help->hCat->security)
	return TRUE;

	return FALSE;
}


void do_rjunk(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	AREA_DATA *area;
	ROOM_INDEX_DATA *room;
	bool changed = FALSE;

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

	for (int iHash = 0; iHash < MAX_KEY_HASH; iHash++)
	{
		for( room = area->room_index_hash[iHash]; room; room = room->next)
		{
			if (!str_cmp(room->name, argument))
			{
				dislink_room(room);
				free_string(room->name);
				room->name = str_dup("Null");
				changed = TRUE;
			}
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
	return FALSE;
	}

	if (mob != NULL && obj != NULL)
	{
	bug("check_imp_sig: had both mob and obj.", 0);
	return FALSE;
	}

	if (mob != NULL)
	{
	if (mob->sig == NULL
	||  !str_cmp(mob->sig, "(none)")
	||  !str_cmp(mob->sig, "none")
	||  !str_cmp(mob->sig, "(null)"))
		return FALSE;
	}

	if (obj != NULL)
	{
	if (obj->imp_sig == NULL
	||  !str_cmp(obj->imp_sig, "(none)")
	||  !str_cmp(obj->imp_sig, "none")
	||  !str_cmp(obj->imp_sig, "(null)"))
		return FALSE;
	}

	return TRUE;
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
	WNUM wnum;

	if (argument[0] == '\0')
	{
	send_to_char("Syntax:  tshow <widevnum>\n\r", ch);
	return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
	   send_to_char("Please specify a widevnum.\n\r", ch);
	   return;
	}

	if (!(token_index = get_token_index(wnum.pArea, wnum.vnum)))
	{
	send_to_char("That token does not exist.\n\r", ch);
	return;
	}

	olc_show_item(ch, token_index, tedit_show, argument);
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
	found = FALSE;

	for (vnum = 1; vnum <= pArea->top_vnum_token; vnum++)
	{
	if ((token_index = get_token_index(pArea, vnum)))
	{
		if (fAll || is_name(arg, token_index->name))
		{
		found = TRUE;
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


void obj_index_reset_multitype(OBJ_INDEX_DATA *pObjIndex)
{
	free_book_page(PAGE(pObjIndex));				PAGE(pObjIndex) = NULL;
	free_book_data(BOOK(pObjIndex));				BOOK(pObjIndex) = NULL;
	free_container_data(CONTAINER(pObjIndex));		CONTAINER(pObjIndex) = NULL;
	free_fluid_container_data(FLUID_CON(pObjIndex));FLUID_CON(pObjIndex) = NULL;
	free_food_data(FOOD(pObjIndex));				FOOD(pObjIndex) = NULL;
	free_ink_data(INK(pObjIndex));					INK(pObjIndex) = NULL;
	free_instrument_data(INSTRUMENT(pObjIndex));	INSTRUMENT(pObjIndex) = NULL;
	free_light_data(LIGHT(pObjIndex));				LIGHT(pObjIndex) = NULL;
	free_money_data(MONEY(pObjIndex));				MONEY(pObjIndex) = NULL;
	free_portal_data(PORTAL(pObjIndex));			PORTAL(pObjIndex) = NULL;
	free_scroll_data(SCROLL(pObjIndex));			SCROLL(pObjIndex) = NULL;
	free_tattoo_data(TATTOO(pObjIndex));			TATTOO(pObjIndex) = NULL;
	free_wand_data(WAND(pObjIndex));				WAND(pObjIndex) = NULL;
}

void obj_index_set_primarytype(OBJ_INDEX_DATA *pObjIndex, int item_type)
{
	obj_index_reset_multitype(pObjIndex);

	pObjIndex->item_type = item_type;
	switch(item_type)
	{
		case ITEM_PAGE:			PAGE(pObjIndex) = new_book_page(); break;
		case ITEM_BOOK:			BOOK(pObjIndex) = new_book_data(); break;
		case ITEM_CONTAINER:	CONTAINER(pObjIndex) = new_container_data(); break;
		case ITEM_FLUID_CONTAINER:	FLUID_CON(pObjIndex) = new_fluid_container_data(); break;
		case ITEM_FOOD:			FOOD(pObjIndex) = new_food_data(); break;
		case ITEM_FURNITURE:	FURNITURE(pObjIndex) = new_furniture_data(); break;
		case ITEM_INK:			INK(pObjIndex) = new_ink_data(); break;
		case ITEM_INSTRUMENT:	INSTRUMENT(pObjIndex) = new_instrument_data(); break;
		case ITEM_LIGHT:		LIGHT(pObjIndex) = new_light_data(); break;
		case ITEM_MONEY:		MONEY(pObjIndex) = new_money_data(); break;
		case ITEM_PORTAL:		PORTAL(pObjIndex) = new_portal_data(); break;
		case ITEM_SCROLL:		SCROLL(pObjIndex) = new_scroll_data(); break;
		case ITEM_TATTOO:		TATTOO(pObjIndex) = new_tattoo_data(); break;
		case ITEM_WAND:			WAND(pObjIndex) = new_wand_data(); break;
	}
}


const struct olc_cmd_type liqedit_table[] =
{
	{	"?",			show_help			},
	{	"color",		liqedit_color		},
	{	"commands",		show_commands		},
	{	"create",		liqedit_create		},
	{	"delete",		liqedit_delete		},
	{	"flammable",	liqedit_flammable	},
	{	"fuel",			liqedit_fuel		},
	{	"full",			liqedit_full		},
	{	"hunger",		liqedit_hunger		},
	{	"list",			liqedit_list		},
	{	"maxmana",		liqedit_maxmana		},
	{	"name",			liqedit_name		},
	{	"proof",		liqedit_proof		},
	{	"show",			liqedit_show		},
	{	"thirst",		liqedit_thirst		},
	{	NULL,			0,					}
};


void do_liqedit(CHAR_DATA *ch, char *argument)
{
	LIQUID *liquid;
    char command[MSL];

    argument = one_argument(argument, command);

	if ((liquid = liquid_lookup(command)))
	{
		olc_set_editor(ch, ED_LIQEDIT, liquid);
		return;
	}

	if (!str_cmp(command, "create"))
	{
		liqedit_create(ch, argument);
		return;
	}

	send_to_char("Syntax:  liqedit <name>\n\r", ch);
	send_to_char("         liqedit create <name>\n\r", ch);
}

void do_liqlist(CHAR_DATA *ch, char *argument)
{
	liqedit_list(ch, argument);
}

void liqedit(CHAR_DATA *ch, char *argument)
{
	char command[MIL];
	char arg[MIL];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (get_trust(ch) < MAX_LEVEL)
	{
		send_to_char("LiqEdit:  Insufficient security to edit liquids - action logged.\n\r", ch);
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
		liqedit_show(ch, argument);
		return;
	}

	for (cmd = 0; liqedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, liqedit_table[cmd].name))
		{
			if ((*liqedit_table[cmd].olc_fun) (ch, argument))
			{
				// Save the liquids
				save_liquids();
			}
			return;
		}
	}

	interpret(ch, arg);
}

const struct olc_cmd_type skedit_table[] =
{
	{	"?",			show_help			},
	{	"beats",		skedit_beats		},
	//{	"brandish",		skedit_brandishfunc },
	{	"brew",			skedit_brewfunc		},
	{	"commands",		show_commands		},
	//	"delete",		skedit_delete		},		// TODO: Need to be abke to track usage first
	{	"difficulty",	skedit_difficulty	},
	{	"display",		skedit_display		},
	//{	"equip",		skedit_equipfunc	},
	{	"flags",		skedit_flags		},
	{	"gsn",			skedit_gsn			},
	{	"ink",			skedit_inkfunc		},
	{	"inks",			skedit_inks			},
	{	"install",		skedit_install		},		// As opposed to create
	{	"interrupt",	skedit_interruptfunc},
	{	"level",		skedit_level		},
	{	"list",			skedit_list			},
	{	"mana",			skedit_mana			},
	{	"message",		skedit_message		},
	//{	"name",			skedit_name			},		// Can't change the name
	{	"position",		skedit_position		},
	{	"prebrew",		skedit_prebrewfunc	},
	{	"preink",		skedit_preinkfunc	},
	{	"prescribe",	skedit_prescribefunc},
	{	"prespell",		skedit_prespellfunc	},
	{	"pulse",		skedit_pulsefunc	},
	{	"quaff",		skedit_quafffunc	},
	{	"race",			skedit_race			},
	{	"recite",		skedit_recitefunc	},
	{	"scribe",		skedit_scribefunc	},
	{	"show",			skedit_show			},
	{	"spell",		skedit_spellfunc	},
	{	"target",		skedit_target		},
	{	"touch",		skedit_touchfunc	},
	{	"value",		skedit_value		},
	{	"valuename",	skedit_valuename	},
	{	"zap",			skedit_zapfunc	},
	{	NULL,			NULL				}
};


void do_skedit(CHAR_DATA *ch, char *argument)
{
	SKILL_DATA *skill;
    char command[MSL];

 	if ((skill = get_skill_data(argument)))
	{
		olc_set_editor(ch, ED_SKEDIT, skill);
		return;
	}

   argument = one_argument(argument, command);

	if (!str_cmp(command, "install"))
	{
		skedit_install(ch, argument);
		return;
	}

	send_to_char("Syntax:  skedit <name>\n\r", ch);
	send_to_char("         skedit install <name>\n\r", ch);
}

void do_sklist(CHAR_DATA *ch, char *argument)
{
	skedit_list(ch, argument);
}


void do_skshow(CHAR_DATA *ch, char *argument)
{
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  skshow <skill name>\n\r", ch);
		return;
	}

	SKILL_DATA *skill = get_skill_data(argument);
	if (!IS_VALID(skill))
	{
		send_to_char("There is no skill/spell with that name.\n\r", ch);
		return;		
	}

	olc_show_item(ch, skill, skedit_show, "");
}

void skedit(CHAR_DATA *ch, char *argument)
{
	char command[MIL];
	char arg[MIL];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (get_trust(ch) < MAX_LEVEL)
	{
		send_to_char("SkEdit:  Insufficient security to edit skills - action logged.\n\r", ch);
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
		skedit_show(ch, "");
		return;
	}

	for (cmd = 0; skedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, skedit_table[cmd].name))
		{
			if ((*skedit_table[cmd].olc_fun) (ch, argument))
			{
				save_skills();
			}
			return;
		}
	}

	interpret(ch, arg);
}

const struct olc_cmd_type sgedit_table[] =
{
	{	"?",			show_help			},
	{	"add",			sgedit_add			},
	{	"clear",		sgedit_clear		},
	{	"commands",		show_commands		},
	{	"create",		sgedit_create		},
	{	"remove",		sgedit_remove		},
	{	"show",			sgedit_show			},
	{	NULL,			NULL				}
};

void do_sgedit(CHAR_DATA *ch, char *argument)
{
	SKILL_GROUP *group;
    char command[MSL];

 	if ((group = group_lookup(argument)))
	{
		olc_set_editor(ch, ED_SGEDIT, group);
		return;
	}

   argument = one_argument(argument, command);

	if (!str_cmp(command, "create"))
	{
		sgedit_create(ch, argument);
		return;
	}

	send_to_char("Syntax:  sgedit <name>\n\r", ch);
	send_to_char("         sgedit create <name>\n\r", ch);
}

void show_skill_group(BUFFER *buffer, SKILL_GROUP *group)
{
	int j = 0;
	char buf[MSL];
	char skills[2 * MIL];

	ITERATOR it;
	char *str;
	iterator_start(&it, group->contents);
	while((str = (char *)iterator_nextdata(&it)))
	{
		if (j > 0)
			j += sprintf(skills + j, ", %s", str);
		else
			j += sprintf(skills, "%s", str);

		if (j >= 28)
		{
			skills[25] = '.';
			skills[26] = '.';
			skills[27] = '.';
			j = 28;
			break;
		}
	}
	iterator_stop(&it);
	skills[j] = '\0';

	sprintf(buf, " {%c%-20.20s   %3d   %s{x\n\r", ((group == global_skills) ? 'Y' : 'x'), group->name, list_size(group->contents), skills);
	add_buf(buffer, buf);
}

void do_sglist(CHAR_DATA *ch, char *argument)
{
	char buf[MSL];
	BUFFER *buffer = new_buf();

	ITERATOR it;
	SKILL_GROUP *group;

	add_buf(buffer, "Skill Groups:\n\r");
	add_buf(buffer, "[        Name        ] [ # ] [    Skills/Spells/Groups    ]\n\r");
	add_buf(buffer, "============================================================\n\r");

	show_skill_group(buffer, global_skills);

	iterator_start(&it, skill_groups_list);
	while((group = (SKILL_GROUP *)iterator_nextdata(&it)))
	{
		show_skill_group(buffer, group);
	}
	iterator_stop(&it);

	add_buf(buffer, "------------------------------------------------------------\n\r");
	sprintf(buf, "Total: %d\n\r", list_size(skill_groups_list) + 1);
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



void do_sgshow(CHAR_DATA *ch, char *argument)
{
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  sgshow <group name>\n\r", ch);
		return;
	}

	SKILL_GROUP *group = group_lookup(argument);
	if (!IS_VALID(group))
	{
		send_to_char("There is no group with that name.\n\r", ch);
		return;
	}

	olc_show_item(ch, group, sgedit_show, "");
}

void sgedit(CHAR_DATA *ch, char *argument)
{
	char command[MIL];
	char arg[MIL];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (get_trust(ch) < MAX_LEVEL)
	{
		send_to_char("SGEdit:  Insufficient security to edit skill groups - action logged.\n\r", ch);
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
		sgedit_show(ch, argument);
		return;
	}

	for (cmd = 0; sgedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, sgedit_table[cmd].name))
		{
			if ((*sgedit_table[cmd].olc_fun) (ch, argument))
			{
				save_skills();
			}
			return;
		}
	}

	interpret(ch, arg);
}


const struct olc_cmd_type songedit_table[] =
{
	{	"?",			show_help			},
	{	"beats",		songedit_beats		},
	{	"commands",		show_commands		},
	{	"flags",		songedit_flags		},
	{	"install",		songedit_install	},
	{	"level",		songedit_level		},
	{	"list",			songedit_list		},
	{	"mana",			songedit_mana		},
	{	"presong",		songedit_presongfunc},
	{	"show",			songedit_show		},
	{	"song",			songedit_songfunc	},
	{	"target",		songedit_target		},
	{	NULL,			NULL				}
};

void do_songedit(CHAR_DATA *ch, char *argument)
{
	SONG_DATA *song;
    char command[MSL];

 	if ((song = get_song_data(argument)))
	{
		olc_set_editor(ch, ED_SONGEDIT, song);
		return;
	}

   argument = one_argument(argument, command);

	if (!str_cmp(command, "install"))
	{
		songedit_install(ch, argument);
		return;
	}

	send_to_char("Syntax:  songedit <name>\n\r", ch);
	send_to_char("         songedit install <name>\n\r", ch);
}


void do_songlist(CHAR_DATA *ch, char *argument)
{
	songedit_list(ch, argument);
}

void do_songshow(CHAR_DATA *ch, char *argument)
{
	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  songshow <song name>\n\r", ch);
		return;
	}

	SONG_DATA *song = get_song_data(argument);
	if (!IS_VALID(song))
	{
		send_to_char("There is no song with that name.\n\r", ch);
		return;
	}

	olc_show_item(ch, song, songedit_show, "");
}

void songedit(CHAR_DATA *ch, char *argument)
{
	char command[MIL];
	char arg[MIL];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	if (get_trust(ch) < MAX_LEVEL)
	{
		send_to_char("SONGEdit:  Insufficient security to edit songs - action logged.\n\r", ch);
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
		songedit_show(ch, argument);
		return;
	}

	for (cmd = 0; songedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, songedit_table[cmd].name))
		{
			if ((*songedit_table[cmd].olc_fun) (ch, argument))
			{
				save_songs();
			}
			return;
		}
	}

	interpret(ch, arg);
}


const struct olc_cmd_type repedit_table[] =
{
	{	"?",			show_help			},
	{	"commands",		show_commands		},
	{	"comments",		repedit_comments	},
	{	"create",		repedit_create		},
	{	"description",	repedit_description	},
	{	"initial",		repedit_initial		},
	{	"name",			repedit_name		},
	{	"rank",			repedit_rank		},
	{	"show",			repedit_show		},
	{	"token",		repedit_token		},	// Must do this to allow scripting!
	{	NULL,			NULL				}
};


void do_repedit(CHAR_DATA *ch, char *argument)
{
	REPUTATION_INDEX_DATA *pRep;
	char arg1[MAX_STRING_LENGTH];
	WNUM wnum;

	argument = one_argument(argument, arg1);

	if (IS_NPC(ch))
		return;

	if (parse_widevnum(arg1, ch->in_room->area, &wnum))
	{
		if (!(pRep = get_reputation_index(wnum.pArea, wnum.vnum)))
		{
			send_to_char("RepEdit:  That vnum does not exist.\n\r", ch);
			return;
		}

		if (!has_access_area(ch, pRep->area))
		{
			send_to_char("Insufficient security to edit reputation - action logged.\n\r" , ch);
			return;
		}

		ch->pcdata->immortal->last_olc_command = current_time;
		olc_set_editor(ch, ED_REPEDIT, pRep);
		return;
	}
	else
	{
		if (!str_cmp(arg1, "create"))
		{
			if (repedit_create(ch, argument))
				ch->desc->editor = ED_REPEDIT;
		}

		return;
	}

	send_to_char("RepEdit:  There is no default reputation to edit.\n\r", ch);
}

void repedit(CHAR_DATA *ch, char *argument)
{
	AREA_DATA *pArea;
	REPUTATION_INDEX_DATA *pRep;
	char command[MAX_INPUT_LENGTH];
	char arg[MAX_STRING_LENGTH];
	int  cmd;

	smash_tilde(argument);
	strcpy(arg, argument);
	argument = one_argument(argument, command);

	EDIT_REPUTATION(ch, pRep);
	pArea = pRep->area;

	if (pArea == NULL)
	{
		bug("repedit: pArea was null!", 0);
		return;
	}

	if (!IS_BUILDER(ch, pArea))
	{
		send_to_char("RepEdit: Insufficient security to edit reputation - action logged.\n\r", ch);
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
		repedit_show(ch, argument);
		return;
	}

	for (cmd = 0; repedit_table[cmd].name != NULL; cmd++)
	{
		if (!str_prefix(command, repedit_table[cmd].name))
		{
			if ((*repedit_table[cmd].olc_fun) (ch, argument))
			{
				SET_BIT(pArea->area_flags, AREA_CHANGED);
			}
			return;
		}
	}

	interpret(ch, arg);
}


void do_replist(CHAR_DATA *ch, char *argument)
{
	REPUTATION_INDEX_DATA *pRepIndex;
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
		send_to_char("Syntax:  replist <all|name>\n\r", ch);
		return;
	}

	buf1  = new_buf();
	pArea = ch->in_room->area;
	fAll  = !str_cmp(arg, "all");
	found = FALSE;

	for (vnum = 1; vnum <= pArea->top_reputation_vnum; vnum++)
	{
		if ((pRepIndex = get_reputation_index(pArea, vnum)) != NULL)
		{
			if (fAll || is_name(arg, pRepIndex->name))
			{
				found = TRUE;
				sprintf(buf, "{x[%5ld] %-17.16s{x", pRepIndex->vnum, pRepIndex->name);
				add_buf(buf1, buf);
				if (++col % 3 == 0)
					add_buf(buf1, "\n\r");
			}
		}
	}

	if (!found)
	{
		send_to_char("Reputation(s) not found in this area.\n\r", ch);
		return;
	}

	if (col % 3 != 0)
		add_buf(buf1, "\n\r");

	page_to_char(buf_string(buf1), ch);
	free_buf(buf1);
	return;
}

void do_repshow(CHAR_DATA *ch, char *argument)
{
	REPUTATION_INDEX_DATA *pRep;
	WNUM wnum;

	if (argument[0] == '\0')
	{
		send_to_char("Syntax:  repshow <widevnum>\n\r", ch);
		return;
	}

	if (!parse_widevnum(argument, ch->in_room->area, &wnum))
	{
		send_to_char("Please specify a widevnum.\n\r", ch);
		return;
	}

	if (!(pRep = get_reputation_index(wnum.pArea, wnum.vnum)))
	{
		send_to_char("That reputation does not exist.\n\r", ch);
		return;
	}

	olc_show_item(ch, pRep, repedit_show, argument);
	return;
}


void olc_show_progs(BUFFER *buffer, LLIST **progs, int type, const char *title)
{
	char buf[MSL];
	int cnt, slot;

	for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++)
		if(list_size(progs[slot]) > 0) ++cnt;

	if (cnt > 0) {
		sprintf(buf, "{R%-6s %-20s %-20s %-10s\n\r{x", "Number", title, "Trigger", "Phrase");
		add_buf(buffer, buf);

		sprintf(buf, "{R%-6s %-20s %-20s %-10s\n\r{x", "------", "--------------------", "--------------------", "----------");
		add_buf(buffer, buf);

		for (cnt = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
			ITERATOR it;
			PROG_LIST *trigger;
			iterator_start(&it, progs[slot]);
			while(( trigger = (PROG_LIST *)iterator_nextdata(&it))) {
				char wnum[MIL];
				sprintf(wnum, "%ld#%ld", trigger->wnum.pArea ? trigger->wnum.pArea->uid : 0, trigger->wnum.vnum);
				sprintf(buf, "{C[{W%4d{C]{x %-20s %-20s %s\n\r", cnt,
					wnum,trigger_name(trigger->trig_type),
					trigger_phrase_olcshow(trigger->trig_type,trigger->trig_phrase, (type == PRG_RPROG), (type == PRG_TPROG)));
				add_buf(buffer, buf);
				cnt++;
			}
			iterator_stop(&it);
		}
	}
}

void olc_buffer_show_tabs(CHAR_DATA *ch, BUFFER *buffer, const char **tab_names)
{
	int tab = ch->desc->nEditTab;

	// Show Tabs
	char tab1[MSL];
	char tab2[MSL];
	char buf[MIL];
	
	tab1[0] = '\0';
	tab2[0] = '\0';
	for(int i = 0; tab_names[i]; i++)
	{
		if (i > 0)
		{
			strcat(tab1, "   ");
			strcat(tab2, "__");
		}
		else
			strcat(tab1, " ");

		strcpy(buf, formatf("%d %s", i + 1, tab_names[i]));
		if (tab == i)
		{
			strcat(tab2,"{x/{Y");
			strcat(tab2,buf);
		}
		else
		{
			strcat(tab2,"{x{_/");
			strcat(tab2, (char *)MXPCreateSend(ch->desc, formatf("%d", i+1), formatf("{g%s{x", buf)));
		}
		int l=strlen(tab1);
		int b=strlen(buf);
		tab1[l+b]=' ';
		tab1[l+b+1]='\0';
		memset(&tab1[l], '_', b);

		if (tab == i)
			strcat(tab2, "{x\\");
		else
			strcat(tab2, "{x{_\\{x");
	}
	strcat(tab1, "\n\r");
	strcat(tab2, "_{x\n\r");
	add_buf(buffer, tab1);
	add_buf(buffer, tab2);
	add_buf(buffer, "\n\r");
}

void olc_buffer_show_string(CHAR_DATA *ch, BUFFER *buffer, const char *value, char *command, char *heading, int indent, char *colors)
{
	char buf[MSL];
	int l=indent-strlen_no_colours(heading);
	l=UMAX(l,0);
	sprintf(buf, formatf("{%c%%s%%%ds", colors[0],l), MXPCreateSend(ch->desc,command, heading), "");
	add_buf(buffer, buf);

	if (IS_NULLSTR(value))
		sprintf(buf, "{%c(unset){x\n\r", colors[1]);
	else
		sprintf(buf, "{%c%s{x\n\r", colors[2], value);
	add_buf(buffer, buf);
}

int olc_buffer_show_flags_ex(CHAR_DATA *ch, BUFFER *buffer,
		const struct flag_type *flag_table, 
		long value, char *command, char *heading, 
		int max_width /*77*/,int first_indent /*16*/, int indent /*5*/,
		const char *colors)
{
	int width;
	int found_count=0;
	bool type_table=is_stat(flag_table);
	int lines=0;

	char openbracket='[';
	char closebracket=']';
	if(type_table){
		openbracket='(';
		closebracket=')';
	}

	char buf[MSL];
	int l=first_indent-strlen_no_colours(heading);
	l=UMAX(l,0);
	sprintf(buf, formatf("{%c%%s%%%ds{%c%c", colors[0],l,colors[1],openbracket),
		MXPCreateSend(ch->desc,command, heading), "");
	width=first_indent;

	char flagbuf[MSL];
	flagbuf[0] = '\0';

	bool match;
    for(int flag = 0; !IS_NULLSTR(flag_table[flag].name); flag++)
    {
		match=false;
		if(type_table){
			if(value==flag_table[flag].bit){
				match=true;
			}
		}else{
			if(IS_SET(value,flag_table[flag].bit)){
				match=true;
			}
		}

		if(flag_table[flag].settable){
			if (type_table)
			{
				if(match){
					strcpy(flagbuf,formatf("{%c", colors[2]));
				}else{
					strcpy(flagbuf,formatf("{%c", colors[3]));
				}
			}
			else
			{
				if(match){
					strcpy(flagbuf,formatf("{%c", colors[4]));
				}else{
					strcpy(flagbuf,formatf("{%c", colors[5]));
				}
			}
		}else{
			if(match){
				strcpy(flagbuf,formatf("{%c", colors[6]));
			}else{
				// we don't show these flags
				continue;
			}
		}			

		found_count++;
		width+= strlen(flag_table[flag].name)+1;
		if(width>max_width){
			strcat(buf,"\r\n");
			lines++;
			strcat(buf,formatf(formatf("%%%ds", indent), ""));
			width=indent + strlen(flag_table[flag].name) +1;
		}

		strcat(buf, flagbuf);
		if(flag_table[flag].settable){
			strcat(buf, MXPCreateSend(ch->desc,
							 formatf("%s %s", command, flag_table[flag].name),
							 	flag_table[flag].name));
		}else{
			strcat(buf, flag_table[flag].name);
		}
		strcat(buf," ");
	}
	if(found_count==0){
		strcat(buf,"none ");
	}
	buf[strlen(buf)-1]='\0';
	strcat(buf,formatf("{%c%c\n\r", colors[1], closebracket));


	add_buf(buffer, buf);

	lines++;
	return lines;
}

int olc_buffer_show_flags(CHAR_DATA *ch, BUFFER *buffer,
		const struct flag_type *flag_table, 
		long value, char *command, char *heading, 
		const char *colors)
{
	return olc_buffer_show_flags_ex(ch, buffer, flag_table, value, command, heading, 77, 16, 5, colors);
}
