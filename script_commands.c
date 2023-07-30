#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "tables.h"
#include "scripts.h"
#include "magic.h"
#include "wilds.h"

//#define DEBUG_MODULE
#include "debug.h"

int blueprint_generation_count(BLUEPRINT *bp);
int blueprint_layout_links_count(BLUEPRINT *bp, BLUEPRINT_LAYOUT_SECTION_DATA *ls, int exclude);
int blueprint_layout_room_count(BLUEPRINT *bp, BLUEPRINT_LAYOUT_SECTION_DATA *ls);
BLUEPRINT_LAYOUT_SECTION_DATA *blueprint_get_nth_section(BLUEPRINT *bp, int section_no, BLUEPRINT_LAYOUT_SECTION_DATA **group);
void blueprint_add_weighted_link(LLIST *list, int weight, int section_no, int link_no);

void dungeon_update_level_ordinals(DUNGEON_INDEX_DATA *dng);
int dungeon_index_generation_count(DUNGEON_INDEX_DATA *dng);
DUNGEON_INDEX_LEVEL_DATA *dungeon_index_get_nth_level(DUNGEON_INDEX_DATA *dng, int level_no, DUNGEON_INDEX_LEVEL_DATA **group);
int get_dungeon_index_level_special_exits(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *data);
int get_dungeon_index_level_special_entrances(DUNGEON_INDEX_DATA *dng, DUNGEON_INDEX_LEVEL_DATA *data);
void add_dungeon_index_weighted_exit_data(LLIST *list, int weight, int level_no, int exit_no);
int dungeon_commence(DUNGEON *dng);
int dungeon_completed(DUNGEON *dng);
int dungeon_failed(DUNGEON *dng);

#define PARSE_ARG				(rest = expand_argument(info,rest,arg))
#define PARSE_ARGTYPE(x)		if (!PARSE_ARG || arg->type != ENT_##x) return
#define PARSE_STR(b)			(expand_string(info,rest,(b)))
#define SETRETURN(ret)			info->progs->lastreturn = (ret)
#define IS_TRIGGER(trg)			(info->trigger_type == (trg))
#define ARG_EQUALS(ss)			(!str_cmp(arg->d.str, (ss)))
#define ARG_PREFIX(ss)			(!str_prefix(arg->d.str, (ss)))

const struct script_cmd_type area_cmd_table[] = {
	{ "acttrigger",				scriptcmd_acttrigger,	TRUE,	FALSE	},
	{ "addaura",			scriptcmd_addaura,			TRUE,	TRUE	},
	{ "bribetrigger",			scriptcmd_bribetrigger,	TRUE,	FALSE	},
	{ "call",				scriptcmd_call,				FALSE,	TRUE	},
	{ "directiontrigger",		scriptcmd_directiontrigger,	TRUE,	FALSE	},
	{ "dungeoncommence",	scriptcmd_dungeoncommence,	TRUE,	TRUE	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	TRUE,	TRUE	},
	{ "dungeonfailure",		scriptcmd_dungeonfailure,	TRUE,	TRUE	},
	{ "echoat",				scriptcmd_echoat,			FALSE,	TRUE	},
	{ "emoteattrigger",			scriptcmd_emoteattrigger,	TRUE,	FALSE	},
	{ "emotetrigger",			scriptcmd_emotetrigger,	TRUE,	FALSE	},
	{ "exacttrigger",			scriptcmd_exacttrigger,	TRUE,	FALSE	},
	{ "exittrigger",			scriptcmd_exittrigger,	TRUE,	FALSE	},
	{ "givetrigger",			scriptcmd_givetrigger,	TRUE,	FALSE	},
	{ "greettrigger",			scriptcmd_greettrigger,	TRUE,	FALSE	},
	{ "hprcttrigger",			scriptcmd_hprcttrigger,	TRUE,	FALSE	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	TRUE,	TRUE	},
	{ "mload",				scriptcmd_mload,			FALSE,	TRUE	},
	{ "mute",				scriptcmd_mute,				FALSE,	TRUE	},
	{ "nametrigger",			scriptcmd_nametrigger,	TRUE,	FALSE	},
	{ "numbertrigger",			scriptcmd_numbertrigger,	TRUE,	FALSE	},
	{ "oload",				scriptcmd_oload,			FALSE,	TRUE	},
	{ "percenttokentrigger",	scriptcmd_percenttokentrigger,	TRUE,	FALSE	},
	{ "percenttrigger",			scriptcmd_percenttrigger,	TRUE,	FALSE	},
	{ "reckoning",			scriptcmd_reckoning,		TRUE,	TRUE	},
	{ "remaura",			scriptcmd_remaura,			TRUE,	TRUE	},
	{ "sendfloor",			scriptcmd_sendfloor,		FALSE,	TRUE	},
	{ "settitle",			scriptcmd_settitle,			TRUE,	TRUE	},
	{ "specialkey",			scriptcmd_specialkey,		FALSE,	TRUE	},
	{ "startreckoning",		scriptcmd_startreckoning,	TRUE,	TRUE	},
	{ "treasuremap",		scriptcmd_treasuremap,		FALSE,	TRUE	},
	{ "unlockarea",			scriptcmd_unlockarea,		TRUE,	TRUE	},
	{ "unmute",				scriptcmd_unmute,			FALSE,	TRUE	},
	{ "useontrigger",			scriptcmd_useontrigger,	TRUE,	FALSE	},
	{ "usetrigger",				scriptcmd_usetrigger,	TRUE,	FALSE	},
	{ "usewithtrigger",			scriptcmd_usewithtrigger,	TRUE,	FALSE	},
	{ "varclear",			scriptcmd_varclear,			FALSE,	TRUE	},
	{ "varclearon",			scriptcmd_varclearon,		FALSE,	TRUE	},
	{ "varcopy",			scriptcmd_varcopy,			FALSE,	TRUE	},
	{ "varsave",			scriptcmd_varsave,			FALSE,	TRUE	},
	{ "varsaveon",			scriptcmd_varsaveon,		FALSE,	TRUE	},
	{ "varset",				scriptcmd_varset,			FALSE,	TRUE	},
	{ "varseton",			scriptcmd_varseton,			FALSE,	TRUE	},
	{ "wildernessmap",		scriptcmd_wildernessmap,	FALSE,	TRUE	},
	{ "xcall",				scriptcmd_xcall,			FALSE,	TRUE	},
	{ NULL,					NULL,						FALSE,	FALSE	}
};

const struct script_cmd_type instance_cmd_table[] = {
	{ "acttrigger",				scriptcmd_acttrigger,	TRUE,	FALSE	},
	{ "addaura",			scriptcmd_addaura,			TRUE,	TRUE	},
	{ "bribetrigger",			scriptcmd_bribetrigger,	TRUE,	FALSE	},
	{ "call",				scriptcmd_call,				FALSE,	TRUE	},
	{ "directiontrigger",		scriptcmd_directiontrigger,	TRUE,	FALSE	},
	{ "dungeoncommence",	scriptcmd_dungeoncommence,	TRUE,	TRUE	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	TRUE,	TRUE	},
	{ "dungeonfailure",		scriptcmd_dungeonfailure,	TRUE,	TRUE	},
	{ "echoat",				scriptcmd_echoat,			FALSE,	TRUE	},
	{ "emoteattrigger",			scriptcmd_emoteattrigger,	TRUE,	FALSE	},
	{ "emotetrigger",			scriptcmd_emotetrigger,	TRUE,	FALSE	},
	{ "exacttrigger",			scriptcmd_exacttrigger,	TRUE,	FALSE	},
	{ "exittrigger",			scriptcmd_exittrigger,	TRUE,	FALSE	},
	{ "givetrigger",			scriptcmd_givetrigger,	TRUE,	FALSE	},
	{ "greettrigger",			scriptcmd_greettrigger,	TRUE,	FALSE	},
	{ "hprcttrigger",			scriptcmd_hprcttrigger,	TRUE,	FALSE	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	TRUE,	TRUE	},
	{ "instancefailure",	scriptcmd_instancefailure,	TRUE,	TRUE	},
	{ "layout",				instancecmd_layout,			FALSE,	TRUE	},
	{ "links",				instancecmd_links,			FALSE,	TRUE	},
	{ "loadinstanced",		scriptcmd_loadinstanced,	TRUE,	TRUE	},
	{ "makeinstanced",		scriptcmd_makeinstanced,	TRUE,	TRUE	},
	{ "mload",				scriptcmd_mload,			FALSE,	TRUE	},
	{ "mute",				scriptcmd_mute,				FALSE,	TRUE	},
	{ "nametrigger",			scriptcmd_nametrigger,	TRUE,	FALSE	},
	{ "numbertrigger",			scriptcmd_numbertrigger,	TRUE,	FALSE	},
	{ "oload",				scriptcmd_oload,			FALSE,	TRUE	},
	{ "percenttokentrigger",	scriptcmd_percenttokentrigger,	TRUE,	FALSE	},
	{ "percenttrigger",			scriptcmd_percenttrigger,	TRUE,	FALSE	},
	{ "reckoning",			scriptcmd_reckoning,		TRUE,	TRUE	},
	{ "remaura",			scriptcmd_remaura,			TRUE,	TRUE	},
	{ "sendfloor",			scriptcmd_sendfloor,		FALSE,	TRUE	},
	{ "settitle",			scriptcmd_settitle,			TRUE,	TRUE	},
	{ "specialkey",			scriptcmd_specialkey,		FALSE,	TRUE	},
	{ "specialrooms",		instancecmd_specialrooms,	FALSE,	TRUE	},
	{ "startreckoning",		scriptcmd_startreckoning,	TRUE,	TRUE	},
	{ "treasuremap",		scriptcmd_treasuremap,		FALSE,	TRUE	},
	{ "unlockarea",			scriptcmd_unlockarea,		TRUE,	TRUE	},
	{ "unmute",				scriptcmd_unmute,			FALSE,	TRUE	},
	{ "useontrigger",			scriptcmd_useontrigger,	TRUE,	FALSE	},
	{ "usetrigger",				scriptcmd_usetrigger,	TRUE,	FALSE	},
	{ "usewithtrigger",			scriptcmd_usewithtrigger,	TRUE,	FALSE	},
	{ "varclear",			scriptcmd_varclear,			FALSE,	TRUE	},
	{ "varclearon",			scriptcmd_varclearon,		FALSE,	TRUE	},
	{ "varcopy",			scriptcmd_varcopy,			FALSE,	TRUE	},
	{ "varsave",			scriptcmd_varsave,			FALSE,	TRUE	},
	{ "varsaveon",			scriptcmd_varsaveon,		FALSE,	TRUE	},
	{ "varset",				scriptcmd_varset,			FALSE,	TRUE	},
	{ "varseton",			scriptcmd_varseton,			FALSE,	TRUE	},
	{ "wildernessmap",		scriptcmd_wildernessmap,	FALSE,	TRUE	},
	{ "xcall",				scriptcmd_xcall,			FALSE,	TRUE	},
	{ NULL,					NULL,						FALSE,	FALSE	}
};

const struct script_cmd_type dungeon_cmd_table[] = {
	{ "acttrigger",				scriptcmd_acttrigger,	TRUE,	FALSE	},
	{ "addaura",			scriptcmd_addaura,			TRUE,	TRUE	},
	{ "bribetrigger",			scriptcmd_bribetrigger,	TRUE,	FALSE	},
	{ "call",				scriptcmd_call,				FALSE,	TRUE	},
	{ "directiontrigger",		scriptcmd_directiontrigger,	TRUE,	FALSE	},
	{ "dungeoncommence",	scriptcmd_dungeoncommence,	TRUE,	TRUE	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	TRUE,	TRUE	},
	{ "dungeonfailure",		scriptcmd_dungeonfailure,	TRUE,	TRUE	},
	{ "echoat",				scriptcmd_echoat,			FALSE,	TRUE	},
	{ "emoteattrigger",			scriptcmd_emoteattrigger,	TRUE,	FALSE	},
	{ "emotetrigger",			scriptcmd_emotetrigger,	TRUE,	FALSE	},
	{ "exacttrigger",			scriptcmd_exacttrigger,	TRUE,	FALSE	},
	{ "exittrigger",			scriptcmd_exittrigger,	TRUE,	FALSE	},
	{ "givetrigger",			scriptcmd_givetrigger,	TRUE,	FALSE	},
	{ "greettrigger",			scriptcmd_greettrigger,	TRUE,	FALSE	},
	{ "hprcttrigger",			scriptcmd_hprcttrigger,	TRUE,	FALSE	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	TRUE,	TRUE	},
	{ "levels",				dungeoncmd_levels,			FALSE,	TRUE	},
	{ "loadinstanced",		scriptcmd_loadinstanced,	TRUE,	TRUE	},
	{ "makeinstanced",		scriptcmd_makeinstanced,	TRUE,	TRUE	},
	{ "mload",				scriptcmd_mload,			FALSE,	TRUE	},
	{ "mute",				scriptcmd_mute,				FALSE,	TRUE	},
	{ "nametrigger",			scriptcmd_nametrigger,	TRUE,	FALSE	},
	{ "numbertrigger",			scriptcmd_numbertrigger,	TRUE,	FALSE	},
	{ "oload",				scriptcmd_oload,			FALSE,	TRUE	},
	{ "percenttokentrigger",	scriptcmd_percenttokentrigger,	TRUE,	FALSE	},
	{ "percenttrigger",			scriptcmd_percenttrigger,	TRUE,	FALSE	},
	{ "reckoning",			scriptcmd_reckoning,		TRUE,	TRUE	},
	{ "remaura",			scriptcmd_remaura,			TRUE,	TRUE	},
	{ "sendfloor",			scriptcmd_sendfloor,		FALSE,	TRUE	},
	{ "settitle",			scriptcmd_settitle,			TRUE,	TRUE	},
	{ "specialexits",		dungeoncmd_specialexits,	FALSE,	TRUE	},
	{ "specialkey",			scriptcmd_specialkey,		FALSE,	TRUE	},
	{ "specialrooms",		dungeoncmd_specialrooms,	FALSE,	TRUE	},
	{ "startreckoning",		scriptcmd_startreckoning,	TRUE,	TRUE	},
	{ "treasuremap",		scriptcmd_treasuremap,		FALSE,	TRUE	},
	{ "unlockarea",			scriptcmd_unlockarea,		TRUE,	TRUE	},
	{ "unmute",				scriptcmd_unmute,			FALSE,	TRUE	},
	{ "useontrigger",			scriptcmd_useontrigger,	TRUE,	FALSE	},
	{ "usetrigger",				scriptcmd_usetrigger,	TRUE,	FALSE	},
	{ "usewithtrigger",			scriptcmd_usewithtrigger,	TRUE,	FALSE	},
	{ "varclear",			scriptcmd_varclear,			FALSE,	TRUE	},
	{ "varclearon",			scriptcmd_varclearon,		FALSE,	TRUE	},
	{ "varcopy",			scriptcmd_varcopy,			FALSE,	TRUE	},
	{ "varsave",			scriptcmd_varsave,			FALSE,	TRUE	},
	{ "varsaveon",			scriptcmd_varsaveon,		FALSE,	TRUE	},
	{ "varset",				scriptcmd_varset,			FALSE,	TRUE	},
	{ "varseton",			scriptcmd_varseton,			FALSE,	TRUE	},
	{ "wildernessmap",		scriptcmd_wildernessmap,	FALSE,	TRUE	},
	{ "xcall",				scriptcmd_xcall,			FALSE,	TRUE	},
	{ NULL,					NULL,						FALSE,	FALSE	}
};

int apcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; area_cmd_table[cmd].name; cmd++)
		if (command[0] == area_cmd_table[cmd].name[0] &&
			!str_prefix(command, area_cmd_table[cmd].name))
			return cmd;

	return -1;
}

int ipcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; instance_cmd_table[cmd].name; cmd++)
		if (command[0] == instance_cmd_table[cmd].name[0] &&
			!str_prefix(command, instance_cmd_table[cmd].name))
			return cmd;

	return -1;
}

int dpcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; dungeon_cmd_table[cmd].name; cmd++)
		if (command[0] == dungeon_cmd_table[cmd].name[0] &&
			!str_prefix(command, dungeon_cmd_table[cmd].name))
			return cmd;

	return -1;
}


///////////////////////////////////////////
//
// Function: do_apdump
//
// Section: Script/APROG
//
// Purpose: Displays the current edit source code of an APROG.
//
// Syntax: apdump <vnum>
//
// Restrictions: Viewer must have READ access on the script to see it.
//
void do_apdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *aprg;
	WNUM wnum;

	one_argument(argument, buf);

	if (!parse_widevnum(buf, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  apdump <widevnum>", ch);
		return;
	}

	if (!(aprg = get_script_index(wnum.pArea, wnum.vnum, PRG_APROG))) {
		send_to_char("No such AREAprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,aprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(aprg->edit_src, ch);
}


///////////////////////////////////////////
//
// Function: do_ipdump
//
// Section: Script/IPROG
//
// Purpose: Displays the current edit source code of an IPROG.
//
// Syntax: ipdump <vnum>
//
// Restrictions: Viewer must be able to edit blueprints
//
void do_ipdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *iprg;
	WNUM wnum;

	one_argument(argument, buf);
	if (!parse_widevnum(buf, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  ipdump <widevnum>", ch);
		return;
	}

	if (!(iprg = get_script_index(wnum.pArea, wnum.vnum, PRG_IPROG))) {
		send_to_char("No such INSTANCEprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,iprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(iprg->edit_src, ch);
}

///////////////////////////////////////////
//
// Function: do_dpdump
//
// Section: Script/DPROG
//
// Purpose: Displays the current edit source code of a DPROG.
//
// Syntax: dpdump <vnum>
//
// Restrictions: Viewer must be able to edit dungeons
//
void do_dpdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *dprg;
	WNUM wnum;

	one_argument(argument, buf);
	if (!parse_widevnum(buf, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  dpdump <widevnum>", ch);
		return;
	}

	if (!(dprg = get_script_index(wnum.pArea, wnum.vnum, PRG_DPROG))) {
		send_to_char("No such DUNGEONprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,dprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(dprg->edit_src, ch);
}




//////////////////////////////////////
// A

// ADDAFFECT mobile|object apply-type(string) affect-group(string) skill(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
// ADDAFFECT mobile|object apply-type(string) affect-group(string) skill(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
SCRIPT_CMD(scriptcmd_addaffect)
{
	char *rest;
	int where, group, skill, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AddAffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg->d.str, FALSE)))
			obj = script_get_obj_here(info, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("Addaffect - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_OBJECT2 || where == TO_OBJECT3 || 
			where == TO_OBJECT4 || where == TO_WEAPON)
			group = flag_lookup(arg->d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg->d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;

	//
	// Get SKILL number (built-in skill)
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("Addaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	bv = 0;
	bv2 = 0;
	switch(where)
	{
		case TO_AFFECTS:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;


			//
			// Get BITVECTOR2
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
			default: return;
			}

			if(bv2 == NO_FLAG) bv2 = 0;
			break;

		case TO_OBJECT:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT2:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra2_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT3:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra3_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT4:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra4_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_IMMUNE:
		case TO_RESIST:
		case TO_VULN:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(imm_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_WEAPON:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(weapon_type2,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

	}

	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("Addaffect - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group	= group;
	af.where     = where;
	af.type      = skill;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = NULL;
	af.slot = wear_loc;
	if(mob) affect_join_full(mob, &af);
	else affect_join_full_obj(obj,&af);
}

// ADDAFFECTNAME mobile|object apply-type(string) affect-group(string) name(string) level(number) location(string) modifier(number) duration(number) bitvector(string) bitvector2(string)[ wear-location(object)]
SCRIPT_CMD(scriptcmd_addaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	info->progs->lastreturn = 0;


	//
	// Get mobile or object TARGET
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = script_get_char_room(info, arg->d.str, FALSE)))
			obj = script_get_obj_here(info, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("AddAffectName - NULL target.", 0);
		return;
	}


	//
	// Get APPLY TYPE
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;


	//
	// Get AFFECT GROUP
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(where == TO_OBJECT || where == TO_WEAPON)
			group = flag_lookup(arg->d.str,affgroup_object_flags);
		else
			group = flag_lookup(arg->d.str,affgroup_mobile_flags);
		break;
	default: return;
	}

	if(group == NO_FLAG) return;


	//
	// Get NAME
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: name = create_affect_cname(arg->d.str); break;
	default: return;
	}

	if(!name) {
		bug("AddAffectName - Error allocating affect name.",0);
		return;
	}


	//
	// Get LEVEL
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}


	//
	// Get LOCATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;


	//
	// Get MODIFIER
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}


	//
	// Get DURATION
	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AddAffectName - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	bv = 0;
	bv2 = 0;
	switch(where)
	{
		case TO_AFFECTS:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;


			//
			// Get BITVECTOR2
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
			default: return;
			}

			if(bv2 == NO_FLAG) bv2 = 0;
			break;

		case TO_OBJECT:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT2:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra2_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT3:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra3_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_OBJECT4:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(extra4_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_IMMUNE:
		case TO_RESIST:
		case TO_VULN:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(imm_flags,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;

		case TO_WEAPON:
			//
			// Get BITVECTOR
			if(!(rest = expand_argument(info,rest,arg))) {
				bug("Addaffect - Error in parsing.",0);
				return;
			}

			switch(arg->type) {
			case ENT_STRING: bv = flag_value(weapon_type2,arg->d.str); break;
			default: return;
			}

			if(bv == NO_FLAG) bv = 0;
			break;
	}

	//
	// Get WEAR-LOCATION of object
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("AddAffectName - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group	= group;
	af.where     = where;
	af.type      = -1;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = name;
	af.slot = wear_loc;
	if(mob) affect_join_full(mob, &af);
	else affect_join_full_obj(obj,&af);
}

// APPLYTOXIN mobile string(toxin) int(level) int(duration)
SCRIPT_CMD(scriptcmd_applytoxin)
{
	char *rest;
	CHAR_DATA *victim = NULL;
	int level, duration, toxin;


	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	if( (toxin = toxin_lookup(arg->d.str)) < 0) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;
	level = UMAX(arg->d.num, 1);

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;
	duration = UMAX(5, arg->d.num);

	victim->bitten_type = toxin;
	victim->bitten = UMAX(500/level, 30);
	victim->bitten_level = level;

	if (!IS_SET(victim->affected_by2, AFF2_TOXIN)) {
		AFFECT_DATA af;
		af.where = TO_AFFECTS;
		af.group     = AFFGROUP_BIOLOGICAL;
		af.type  = gsn_toxins;
		af.level = victim->bitten_level;
		af.duration = duration;
		af.location = APPLY_STR;
		af.modifier = -1 * number_range(1,3);
		af.bitvector = 0;
		af.bitvector2 = AFF2_TOXIN;
		af.slot	= WEAR_NONE;
		affect_to_char(victim, &af);
	}

	info->progs->lastreturn = 1;
}


// ATTACH $ENTITY $TARGET $STRING[ $SILENT]
// Attaches the $ENTITY to the given field ($STRING) on $TARGET
// Fields and what they require for ENTITY and TARGET
// * PET: NPC, MOBILE
// * MASTER/FOLLOWER: MOBILE, MOBILE
// * LEADER/GROUP: MOBILE, MOBILE
// * CART/PULL: OBJECT (Pullable), MOBILE
// * ON: FURNITURE, MOBILE/OBJECT
//
// $SILENT is a boolean to indicate whether the action is silent
//
// LASTRETURN will be set to 1 if successful, 0 otherwise
SCRIPT_CMD(scriptcmd_attach)
{
	char *rest;
	char field[MIL];
	CHAR_DATA *entity_mob = NULL;
	CHAR_DATA *target_mob = NULL;
	OBJ_DATA *entity_obj = NULL;
	OBJ_DATA *target_obj = NULL;

	bool show = TRUE;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_MOBILE ) entity_mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) entity_obj = arg->d.obj;
	else
		return;

	if (!entity_mob && !entity_obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE ) target_mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) target_obj = arg->d.obj;
	else
		return;

	if (!target_mob && !target_obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);
	field[MIL] = '\0';

	if(*rest) {
		if (!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			show = !arg->d.boolean;
	}

	if(entity_mob != NULL)
	{
		if(field[0] != '\0')
		{
			if(!str_prefix(field, "pet"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE and doesn't have a pet already
				if(!IS_NPC(entity_mob) || !target_mob || target_mob->pet != NULL)
					return;

				// $ENTITY is following someone else
				if( entity_mob->master != NULL && entity_mob->master != target_mob ) return;

				// Don't allow since their groups is already full
				if( target_mob->num_grouped >= 9 )
					return;

				add_follower(entity_mob, target_mob, show);
				add_grouped(entity_mob, target_mob, show);	// Checks are already done

				target_mob->pet = entity_mob;
				SET_BIT(entity_mob->act, ACT_PET);
				SET_BIT(entity_mob->affected_by, AFF_CHARM);
				entity_mob->comm = COMM_NOTELL|COMM_NOCHANNELS;

			}
			else if(!str_prefix(field, "master") || !str_cmp(field, "follower"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE
				if(!IS_NPC(entity_mob) || !target_mob)
					return;

				// $ENTITY is already following someone
				if( entity_mob->master != NULL ) return;

				add_follower(entity_mob, target_mob, show);
			}
			else if(!str_prefix(field, "leader") || !str_cmp(field, "group"))
			{
				// Make sure ENTITY is an NPC, and the TARGET is a MOBILE and isn't aleady grouped
				if(!IS_NPC(entity_mob) || !target_mob || target_mob->leader != NULL)
					return;

				// $ENTITY is already following someone else
				if( entity_mob->master != NULL && entity_mob->master != target_mob ) return;

				// $ENTITY is already grouped
				if( entity_mob->leader != NULL ) return;

				// Don't allow since their groups is already full
				if( target_mob->num_grouped >= 9 )
					return;

				add_follower(entity_mob, target_mob, show);
				add_grouped(entity_mob, target_mob, show);	// Checks are already done
			}
		}
	}
	else	// entity_obj != NULL
	{
		if(!str_prefix(field,"cart") || !str_prefix(field,"pull") )
		{
			if( target_mob == NULL || target_mob->pulled_cart != NULL ) return;

			// Already being pulled
			if( entity_obj->pulled_by != NULL ) return;

			// Check it is pullable
			if( !is_pullable(entity_obj) ) return;

			target_mob->pulled_cart = entity_obj;
			entity_obj->pulled_by = target_mob;
		}
		else if(!str_prefix(field,"on") )
		{
			// $ENTITY cannot already be on something
			if( entity_obj->on != NULL ) return;

			// $ENTITY is not furniture
			if( entity_obj->item_type != ITEM_FURNITURE ) return;

			if( target_mob != NULL )
			{
				target_mob->on = entity_obj;
			}
			else	// target_obj != NULL
			{
				target_obj->on = entity_obj;
			}
		}

	}

	info->progs->lastreturn = 1;

}


// AWARD mobile string(type) number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp, experience/xp
//
// AWARD church string(type) number(amount)
// Types: gold, pneuma, deity/dp
//
SCRIPT_CMD(scriptcmd_award)
{
	char buf[MSL], *rest;
	char field[MIL];
	char *field_name;
	CHAR_DATA *victim = NULL;
	CHURCH_DATA *church = NULL;
	int amount = 0;


	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_CHURCH: church = arg->d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	if( church ) {
		if( !str_prefix(field, "gold") ) {
			church->gold += amount;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			church->pneuma += amount;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			church->dp += amount;
			field_name = "deity points";

		} else
			return;


		sprintf(buf, "Award logged: Church %s was awarded %d %s", church->name, amount, field_name);
		log_string(buf);

	} else {

		if( !str_prefix(field, "silver") ) {
			victim->silver += amount;
			field_name = "silver";

		} else if( !str_prefix(field, "gold") ) {
			victim->gold += amount;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			victim->pneuma += amount;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			victim->deitypoints += amount;
			field_name = "deity points";

		} else if( !str_prefix(field, "practice") ) {
			victim->practice += amount;
			field_name = "practices";

		} else if( !str_prefix(field, "train") ) {
			victim->train += amount;
			field_name = "trains";

		} else if( !str_prefix(field, "quest") || !str_cmp(field, "qp") ) {
			victim->questpoints += amount;
			field_name = "quest points";

		} else if( !str_prefix(field, "experience") || !str_cmp(field, "xp") ) {
			gain_exp(victim, amount);
			field_name = "experience";

		} else
			return;


		if(!IS_NPC(victim)) {
			sprintf(buf, "Award logged: %s was awarded %d %s", victim->name, amount, field_name);
			log_string(buf);
		}
	}

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// B

// BREATHE $VICTIM $TYPE[ $ATTACKER]
// Does the dragon breath of the given type: acid, fire, frost, gas, lightning
SCRIPT_CMD(scriptcmd_breathe)
{
	static char *breath_names[] = { "acid", "fire", "frost", "gas", "lightning", NULL };
	static sh_int *breath_gsn[] = { &gsn_acid_breath, &gsn_fire_breath, &gsn_frost_breath, &gsn_gas_breath, &gsn_lightning_breath };
	static SPELL_FUN *breath_fun[] = { spell_acid_breath, spell_fire_breath, spell_frost_breath, spell_gas_breath, spell_lightning_breath };
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;
	int i;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	for(i=0;breath_names[i] && str_prefix(arg->d.str,breath_names[i]);i++);

	if(!breath_names[i])
		return;

	attacker = info->mob;
	if(*rest) {
		if(!(rest = expand_argument(info,argument,arg)))
			return;

		switch(arg->type) {
		case ENT_STRING: attacker = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: attacker = arg->d.mob; break;
		default: attacker = NULL; break;
		}
	}

	if(!attacker)
		return;

	(*breath_fun[i]) (*breath_gsn[i] , attacker->tot_level, attacker, victim, TARGET_CHAR, WEAR_NONE);

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// C

// CALL $WIDEVNUM ....
SCRIPT_CMD(scriptcmd_call)
{
	char *rest; //buf[MSL], *rest;
	CHAR_DATA *vch = NULL,*ch = NULL;
	OBJ_DATA *obj1 = NULL,*obj2 = NULL;
	SCRIPT_DATA *script;
	int depth, ret;
	int space;
	WNUM wnum;

	if(!info) return;

	if (!argument[0]) {
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_WIDEVNUM: wnum = arg->d.wnum; break;
	default: return;
	}

	if (info->mob) space = PRG_MPROG;
	else if(info->obj) space = PRG_OPROG;
	else if(info->room) space = PRG_RPROG;
	else if(info->token) space = PRG_TPROG;
	else if(info->area) space = PRG_APROG;
	else if(info->instance) space = PRG_IPROG;
	else if(info->dungeon) space = PRG_DPROG;
	else return;

	if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, space))) {
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: obj1 = script_get_obj_here(info, arg->d.str); break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: obj2 = script_get_obj_here(info, arg->d.str); break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	// Do this to account for possible destructions
	ret = execute_script(script, info->mob, info->obj, info->room, info->token, info->area, info->instance, info->dungeon, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
	if(info->progs) {
		info->progs->lastreturn = ret;
	} else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

//////////////////////////////////////
// D

// DAMAGE mobile|'all' lower upper 'lethal'|'kill'|string damageclass[ attacker]
// DAMAGE mobile|'all' 'level'|'dual'|'remort'|'dualremort' mobile|number 'lethal'|'kill'|string damageclass[ attacker]
SCRIPT_CMD(scriptcmd_damage)
{
	char *rest;
	CHAR_DATA *victim = NULL, *victim_next, *attacker = NULL;
	int low, high, level, value, dc;
	bool fAll = FALSE, fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;


	if(!(rest = expand_argument(info,argument,arg))) {
		//bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = script_get_char_room(info, arg->d.str, FALSE);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll)
		return;

	if (fAll && !info->location)
		return;

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = TRUE; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = TRUE; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = TRUE; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = TRUE; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		return;
	}

	if(!*rest)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(fLevel && !victim)
		return;

	level = victim ? victim->tot_level : 1;

	switch(arg->type) {
	case ENT_NUMBER:
		if(fLevel) level = arg->d.num;
		else high = arg->d.num;
		break;
	case ENT_STRING:
		if(is_number(arg->d.str)) {
			if(fLevel) level = atoi(arg->d.str);
			else high = atoi(arg->d.str);
		} else
			return;
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else
				return;
		} else
			return;
		break;
	default:
//		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_STRING ) return;

		if (!str_cmp(arg->d.str,"kill") || !str_cmp(arg->d.str,"lethal")) fKill = TRUE;
	}

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_STRING ) return;

		dc = damage_class_lookup(arg->d.str);
	} else
		dc = DAM_NONE;

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_MOBILE || !arg->d.mob) return;

		attacker = arg->d.mob;

	} else
		attacker = NULL;


	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = info->location->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			if (victim != info->mob && (!attacker || victim != attacker)) {
				value = fLevel ? dice(low,high) : number_range(low,high);
				damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
			}
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(attacker?attacker:victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
	}
}


// DEDUCT mobile string(type) number(amount)
// Types: silver, gold, pneuma, deity/dp, practice, train, quest/qp
// Returns actual amount deducted
//
// DEDUCT church string(type) number(amount)
// Types: gold, pneuma, deity/dp
// Returns actual amount deducted
//
SCRIPT_CMD(scriptcmd_deduct)
{
	char buf[MSL], *rest;
	char field[MIL];
	char *field_name;
	CHAR_DATA *victim = NULL;
	CHURCH_DATA *church = NULL;
	int amount = 0;


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_CHURCH: church = arg->d.church; break;
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !church) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	if( church ) {
		if( !str_prefix(field, "gold") ) {
			info->progs->lastreturn = UMIN(church->gold, amount);
			church->gold -= info->progs->lastreturn;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			info->progs->lastreturn = UMIN(church->pneuma, amount);
			church->pneuma -= info->progs->lastreturn;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			info->progs->lastreturn = UMIN(church->dp, amount);
			church->dp -= info->progs->lastreturn;
			field_name = "deity points";

		} else
			return;

		sprintf(buf, "Deduct logged: Church %s was deducted %d %s", church->name, amount, field_name);
		log_string(buf);
	} else {
		if( !str_prefix(field, "silver") ) {
			info->progs->lastreturn = UMIN(victim->silver, amount);
			victim->silver -= info->progs->lastreturn;
			field_name = "silver";

		} else if( !str_prefix(field, "gold") ) {
			info->progs->lastreturn = UMIN(victim->gold, amount);
			victim->gold -= info->progs->lastreturn;
			field_name = "gold";

		} else if( !str_prefix(field, "pneuma") ) {
			info->progs->lastreturn = UMIN(victim->pneuma, amount);
			victim->pneuma -= info->progs->lastreturn;
			field_name = "pneuma";

		} else if( !str_prefix(field, "deity") || !str_cmp(field, "dp") ) {
			info->progs->lastreturn = UMIN(victim->deitypoints, amount);
			victim->deitypoints -= info->progs->lastreturn;
			field_name = "deity points";

		} else if( !str_prefix(field, "practice") ) {
			info->progs->lastreturn = UMIN(victim->practice, amount);
			victim->practice -= info->progs->lastreturn;
			field_name = "practices";

		} else if( !str_prefix(field, "train") ) {
			info->progs->lastreturn = UMIN(victim->train, amount);
			victim->train -= info->progs->lastreturn;
			field_name = "trains";

		} else if( !str_prefix(field, "quest") || !str_cmp(field, "qp") ) {
			info->progs->lastreturn = UMIN(victim->questpoints, amount);
			victim->questpoints -= info->progs->lastreturn;
			field_name = "quest points";

		} else
			return;


		if(!IS_NPC(victim)) {
			sprintf(buf, "Deduct logged: %s was deducted %d %s", victim->name, amount, field_name);
			log_string(buf);
		}
	}

}


// DETACH $ENTITY $STRING[ $SILENT]
// Detaches the given field ($STRING) from $ENTITY

// Fields and what they require for ENTITY
// * PET: MOBILE
// * MASTER/FOLLOWER: MOBILE
// * LEADER/GROUP: MOBILE
// * CART: MOBILE
// * ON: MOBILE/OBJECT
//
// $SILENT is a boolean to indicate whether the action is silent
//
// LASTRETURN will be set to 1 if successful, 0 otherwise

SCRIPT_CMD(scriptcmd_detach)
{
	char *rest;
	char field[MIL];
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;

	bool show = TRUE;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_MOBILE ) mob = arg->d.mob;
	else if( arg->type == ENT_OBJECT) obj = arg->d.obj;
	else
		return;

	if (!mob && !obj)
		return;

	if (!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_STRING ) return;
	strncpy(field,arg->d.str,MIL-1);
	field[MIL] = '\0';

	if(*rest) {
		if (!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			show = !arg->d.boolean;
	}

	if( mob )
	{
		if( !str_prefix(field, "pet") )
		{
			if( mob->pet == NULL ) return;

			if( !IS_SET(mob->pet->pIndexData->act, ACT_PET) )
				REMOVE_BIT(mob->pet->act, ACT_PET);

			if( !IS_SET(mob->pet->pIndexData->affected_by, AFF_CHARM) )
				REMOVE_BIT(mob->pet->affected_by, AFF_CHARM);

			// This will not ungroup/unfollow
			mob->pet->comm &= ~(COMM_NOTELL|COMM_NOCHANNELS);
			mob->pet = NULL;
		}
		else if(!str_prefix(field, "master") || !str_cmp(field, "follower"))
		{
			if( mob->master == NULL ) return;

			if( mob->master->pet == mob )
			{
				if( !IS_SET(mob->pet->pIndexData->act, ACT_PET) )
					REMOVE_BIT(mob->pet->act, ACT_PET);

				if( !IS_SET(mob->pet->pIndexData->affected_by, AFF_CHARM) )
					REMOVE_BIT(mob->pet->affected_by, AFF_CHARM);

				// This will not ungroup/unfollow
				mob->pet->comm &= ~(COMM_NOTELL|COMM_NOCHANNELS);
				mob->pet = NULL;

			}

			stop_follower(mob, show);
		}
		else if(!str_prefix(field, "leader") || !str_cmp(field, "group"))
		{
			if( mob->leader == NULL ) return;

			stop_grouped(mob);
		}
		if(!str_prefix(field,"cart") || !str_prefix(field,"pull") )
		{
			if( mob->pulled_cart == NULL ) return;

			mob->pulled_cart->pulled_by = NULL;
			mob->pulled_cart = NULL;
		}
		else if( !str_prefix(field, "on") )
		{
			mob->on = NULL;
		}
	}
	else	// obj != NULL
	{
		if( !str_prefix(field, "on") )
		{
			obj->on = NULL;
		}
	}

	info->progs->lastreturn = 1;
}

// DUNGEONCOMMENCE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeoncommence)
{
	SETRETURN(PRET_BADSYNTAX);

	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_DUNGEON ) {
		int ret = dungeon_commence(arg->d.dungeon);

		SETRETURN(ret);
	}
}

// DUNGEONCOMPLETE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeoncomplete)
{
	SETRETURN(PRET_BADSYNTAX);

	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_DUNGEON ) {
		int ret = dungeon_completed(arg->d.dungeon);

		SETRETURN(ret);
	}
}

// DUNGEONFAILURE $DUNGEON
SCRIPT_CMD(scriptcmd_dungeonfailure)
{
	SETRETURN(PRET_BADSYNTAX);

	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_DUNGEON ) {
		int ret = dungeon_failed(arg->d.dungeon);

		SETRETURN(ret);
	}
}

//////////////////////////////////////
// E

// ECHOAT $MOBILE|ROOM|$AREA|INSTANCE|DUNGEON string
SCRIPT_CMD(scriptcmd_echoat)
{
	char *rest;
	CHAR_DATA *victim = NULL;
	ROOM_INDEX_DATA *room = NULL;
	AREA_DATA *area = NULL;
	INSTANCE *instance = NULL;
	DUNGEON *dungeon = NULL;

	if(!info) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;


	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	case ENT_AREA: area = arg->d.area; break;
	case ENT_INSTANCE: instance = arg->d.instance; break;
	case ENT_DUNGEON: dungeon = arg->d.dungeon; break;
	default: return;
	}

	if ((!victim || !victim->in_room) && !room && !area && !instance && !dungeon)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( !IS_NULLSTR(buffer->string) )
	{
		if( IS_VALID(instance) )
			instance_echo(instance, buffer->string);
		else if( IS_VALID(dungeon) )
			dungeon_echo(dungeon, buffer->string);
		else if( area )
			area_echo(area, buffer->string);
		else if( room )
			room_echo(room, buffer->string);
		else
			act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	free_buf(buffer);
}


// ED $OBJECT|$VROOM CLEAR
//    Clears out the entity's extra descriptions
//
// ED $OBJECT|$VROOM SET $NAME $STRING
//
// ED $OBJECT|$VROOM DELETE $NAME
//
// Extra description manipulation for rooms will only work in Wilderness and Clone rooms for now
//
SCRIPT_CMD(scriptcmd_ed)
{
	char *rest;
	EXTRA_DESCR_DATA **ed = NULL;

	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	if( arg->type == ENT_OBJECT )
		ed = &arg->d.obj->extra_descr;
	else if( arg->type == ENT_ROOM )
	{
		if( !arg->d.room ) return;
		if( arg->d.room->source || arg->d.room->wilds )
			ed = &arg->d.room->extra_descr;
	}

	if( !ed )
		return;

	if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	if( !str_cmp(arg->d.str, "clear") )
	{
		EXTRA_DESCR_DATA *cur, *next;

		for(cur = *ed; cur; cur = next)
		{
			next = cur->next;
			free_extra_descr(cur);
		}

		*ed = NULL;
	}
	else if( !str_cmp(arg->d.str, "delete") )
	{
		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		EXTRA_DESCR_DATA *prev = NULL, *cur;

		for(cur = *ed; cur; cur = cur->next)
		{
			if( is_name(arg->d.str, cur->keyword) )
				break;
			else
				prev = cur;
		}

		if( !cur ) return;

		if( prev )
			prev->next = cur->next;
		else
			*ed = cur->next;

		free_extra_descr(cur);

	}
	else if( !str_cmp(arg->d.str, "set") )
	{
		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		// Save the keyword in a buffer
		BUFFER *tmp_buffer = new_buf();
		add_buf(tmp_buffer, arg->d.str);

		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		{
			free_buf(tmp_buffer);
			return;
		}

		EXTRA_DESCR_DATA *cur;

		for(cur = *ed; cur; cur = cur->next)
		{
			if( is_name(tmp_buffer->string, cur->keyword) )
				break;
		}

		if( !cur )
		{
			cur = new_extra_descr();
			cur->next = *ed;
			*ed = cur;

			free_string(cur->keyword);
			cur->keyword = str_dup(tmp_buffer->string);
		}

		free_string(cur->description);
		cur->description = str_dup(arg->d.str);

		free_buf(tmp_buffer);
	}

	info->progs->lastreturn = 1;
}

// ENTERCOMBAT[ $ATTACKER] $VICTIM[ $SILENT]
// Switches target explicitly without triggering any standard combat scripts
//  - used for scripts in combat to change targets
SCRIPT_CMD(scriptcmd_entercombat)
{
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;

	bool fSilent = FALSE;


	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		attacker = victim;
		if( arg->type == ENT_BOOLEAN ) {
			// VICTIM SILENT syntax
			fSilent = arg->d.boolean == TRUE;
		} else {
			switch(arg->type) {
			case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
			case ENT_MOBILE: victim = arg->d.mob; break;
			default: victim = NULL; break;
			}

			if (!victim)
				return;

			if(*rest ) {
				if(!expand_argument(info,rest,arg))
					return;

				// ATTACKER VICTIM SILENT syntax
				if( arg->type == ENT_BOOLEAN )
					fSilent = arg->d.boolean == TRUE;
			}
		}
	} else if(!info->mob)
		return;
	else
		attacker = info->mob;

	enter_combat(attacker, victim, fSilent);

	if( attacker->fighting == victim )
		info->progs->lastreturn = 1;
}


//////////////////////////////////////
// F

// FADE $PLAYER $DIRECTION $RATING[ $INTERRUPT]
// $PLAYER    - player to force into fading
// $DIRECTION - exit to travel in, "random"|"any" to pick a random exit
// $RATING    - skill rating to mimic the fading, ranging from 1 to 100
// $INTERRUPT - flag (default FALSE) on whether to interrupt the player.
//
// Fails if not a player
// Fails if player is rifting
// Fails if player is pulling a cart
// Fails if player is fighting
// Fails if direction does not exist
// Fails if rating is out of range
// Fails if interrupt was false and the player was busy
//
// LASTRETURN will return -1 for failure, otherwise will return the door number
SCRIPT_CMD(scriptcmd_fade)
{
	char *rest;
	CHAR_DATA *target;
	int door = -1;
	int rating;

	if(!info) return;
	info->progs->lastreturn = -1;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	target = info->mob;
	switch(arg->type) {
	case ENT_STRING: target = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: target = arg->d.mob; break;
	default: target = NULL; break;
	}

	if (!target || !IS_VALID(target) || IS_NPC(target)) return;

	// Block rifter, cart pullers and fighters
	if (IS_SOCIAL(target) || PULLING_CART(target) || (target->fighting != NULL)) return;

	if( IS_SET(target->in_room->area->area_flags, AREA_NO_FADING) ) return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	if( !str_cmp(arg->d.str, "random") || !str_cmp(arg->d.str, "any") )
	{
		door = number_range(1, MAX_DIR);
	}
	else
	{
		door = parse_door(arg->d.str);
		if( door < 0 )
			return;
	}

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return;

	if( arg->d.num < 1 || arg->d.num > 100 )
		return;

	rating = arg->d.num;

	bool busy = (is_char_busy(target) || target->desc->pString != NULL || target->desc->input);

	if( *rest )
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		// Check whether to make the busy flag false
		if( arg->type == ENT_BOOLEAN )
		{
			if( arg->d.boolean ) busy = FALSE;
		}
		else if( arg->type == ENT_NUMBER )
		{
			if( arg->d.num != 0 ) busy = FALSE;
		}
		else if( arg->type == ENT_STRING )
		{
			if( !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") )
				busy = FALSE;
		}

	}

	if( busy ) return;

	// No message.. the LASTRETURN can indicate whether to do that
	target->force_fading = URANGE(1, rating, 100);
    target->fade_dir = door;
    FADE_STATE(target, 4);

	info->progs->lastreturn = door;
}

// FLEE[ mobile[ direction[ conceal[ pursue]]]]
// mobile - target of action (Only optional in mprog)
// direction - direction of flee
//				"none" = random direction (same as doing "flee")
//				"anyway" = random direction (same as used in places like intimidate)
//				"wimpy" = random direction (same as automatic wimpy fleeing)
// conceal - conceal whether the flee is kept hidden from the opponent
// pursue - allows pursuit (only if the flee is successful)
//
// Assigns LASTRETURN with direction of flee
//	if < 0, the flee action FAILED
SCRIPT_CMD(scriptcmd_flee)
{
	char *rest;
	CHAR_DATA *target;
	int door = -1;
	bool conceal = FALSE, pursue = TRUE;
	char fleedata[MIL];
	char *fleearg = str_empty;

	if(!info) return;
	info->progs->lastreturn = -1;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	target = info->mob;
	switch(arg->type) {
	case ENT_STRING: target = script_get_char_room(info, arg->d.str, TRUE); break;
	case ENT_MOBILE: target = arg->d.mob; break;
	default: target = NULL; break;
	}

	if (!target) return;

	if (!target->fighting || !target->in_room)
		return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,arg)))
				return;

		if(arg->type == ENT_STRING) {
			if (!str_cmp(arg->d.str, "none")) {
				fleearg = str_empty;
			} else if (!str_cmp(arg->d.str, "anyway")) {
				strcpy(fleedata,"anyway");
				fleearg = fleedata;
			} else if (!str_cmp(arg->d.str, "wimpy"))
				fleearg = NULL;
			else {
				strncpy(fleedata,arg->d.str,sizeof(fleedata)-1);
				fleearg = fleedata;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,arg)))
					return;

				if( arg->type == ENT_NUMBER )
					conceal = (arg->d.num != 0);
				else if( arg->type == ENT_STRING )
					conceal = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "all");
				else
					return;
			}

			if(*rest) {
				if(!(rest = expand_argument(info,rest,arg)))
					return;

				if( arg->type == ENT_NUMBER )
					pursue = (arg->d.num != 0);
				else if( arg->type == ENT_STRING )
					pursue = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "all");
				else
					return;
			}

		}
	}

	door = do_flee_full(target, fleearg, conceal, pursue);
	info->progs->lastreturn = door;
}


//////////////////////////////////////
// G


// GRANTSKILL player name[ int(rating=1)[ bool(permanent=false)[ string(flags)]]]
// GRANTSKILL player vnum[ int(rating=1)[ bool(permanent=false)[ string(flags)]]]
SCRIPT_CMD(scriptcmd_grantskill)
{
	char *rest;

	CHAR_DATA *mob;
	TOKEN_DATA *token = NULL;
	TOKEN_INDEX_DATA *token_index = NULL;
	int rating = 1;
	int sn = -1;
	long flags = SKILL_AUTOMATIC;
	char source = SKILLSRC_SCRIPT;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_STRING ) {
		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;
	} else if( arg->type == ENT_WIDEVNUM ) {
		token_index = get_token_index(arg->d.wnum.pArea, arg->d.wnum.vnum);

		if( !token_index ) return;
	}
	else
		return;


	if( *rest ) {

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_NUMBER ) return;
		rating = URANGE(1,arg->d.num,100);

		if( *rest ) {
			bool fPerm = FALSE;

			if(!(rest = expand_argument(info,rest,arg)))
				return;

			if( arg->type == ENT_BOOLEAN )
				fPerm = arg->d.boolean;
			else if( arg->type == ENT_NUMBER )
				fPerm = (arg->d.num != 0);
			else if( arg->type == ENT_STRING )
				fPerm = !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "perm");
			else
				return;

			source = fPerm ? SKILLSRC_SCRIPT_PERM : SKILLSRC_SCRIPT;

			if( *rest ) {
				BUFFER *buffer = new_buf();
				expand_string(info,rest,buffer);

				if( buffer->state != BUFFER_SAFE || !*buffer->string ) {
					free_buf(buffer);
					return;
				}

				if (!str_cmp(buf_string(buffer), "none"))
					flags = 0;
				else if ((flags = flag_value(skill_flags, buf_string(buffer))) == NO_FLAG)
					flags = SKILL_AUTOMATIC;

				free_buf(buffer);
			}
		}
	}

	if( token_index )
	{
		if( skill_entry_findtokenindex(mob->sorted_skills, token_index) )
			return;


		token = create_token(token_index);

		if(!token) return;


		if( token_index->value[TOKVAL_SPELL_RATING] > 0 )
			token->value[TOKVAL_SPELL_RATING] = token_index->value[TOKVAL_SPELL_RATING] * rating;
		else
			token->value[TOKVAL_SPELL_RATING] = rating;

		token_to_char_ex(token, mob, source, flags);
	}
	else if(sn > 0 && sn < MAX_SKILL )
	{
		if( skill_entry_findsn(mob->sorted_skills, sn) )
			return;

		mob->pcdata->learned[sn] = rating;
		if( skill_table[sn].spell_fun == spell_null ) {
			skill_entry_addskill(mob, sn, NULL, source, flags);
		} else {
			skill_entry_addspell(mob, sn, NULL, source, flags);
		}
	}
	else
		return;

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// H

//////////////////////////////////////
// I

// INPUTSTRING $PLAYER script-wnum variable
//  Invokes the interal string editor, for use in getting multiline strings from players.
//
//  $PLAYER - player entity to get string from
//  script-wnum - script to call after the editor is closed
//  variable - name of variable to use to store the string (as well as supply the initial string)

SCRIPT_CMD(scriptcmd_inputstring)
{
	char *rest;
	WNUM wnum;
	CHAR_DATA *mob = NULL;

	int type;

	info->progs->lastreturn = 0;

	if(info->mob) type = PRG_MPROG;
	else if(info->obj) type = PRG_OPROG;
	else if(info->room) type = PRG_RPROG;
	else if(info->token) type = PRG_TPROG;
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	// Are they already being prompted
	if(mob->pk_question ||
		mob->remove_question ||
		mob->personal_pk_question ||
		mob->cross_zone_question ||
		mob->pcdata->convert_church != -1 ||
		mob->challenged ||
		mob->remort_question)
		return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("*pInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_WIDEVNUM: wnum = arg->d.wnum; break;
	default: return;
	}

	if(!wnum.pArea || wnum.vnum < 1 || !get_script_index(wnum.pArea, wnum.vnum, type)) return;
	BUFFER *buffer = new_buf();

	expand_string(info,rest,buffer);

	pVARIABLE var = variable_get(*(info->var),buf_string(buffer));

	mob->desc->input = TRUE;
	if( var && var->type == VAR_STRING && !IS_NULLSTR(var->_.s) )
		mob->desc->inputString = str_dup(var->_.s);
	else
		mob->desc->inputString = &str_empty[0];
	mob->desc->input_var = str_dup(buf_string(buffer));
	mob->desc->input_prompt = NULL;
	mob->desc->input_script = wnum;
	mob->desc->input_mob = info->mob;
	mob->desc->input_obj = info->obj;
	mob->desc->input_room = info->room;
	mob->desc->input_tok = info->token;

	string_append(mob, &mob->desc->inputString);
	free_buf(buffer);

	info->progs->lastreturn = 1;
}

// INSTANCECOMPLETE $INSTANCE
SCRIPT_CMD(scriptcmd_instancecomplete)
{
	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_INSTANCE )
	{
		if( !IS_SET(arg->d.instance->flags, (INSTANCE_COMPLETED|INSTANCE_FAILED)) )
		{
			p_percent2_trigger(NULL, arg->d.instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_COMPLETED,NULL,0,0,0,0,0);

			SET_BIT(arg->d.instance->flags, INSTANCE_COMPLETED);
		}
	}
}


// INSTANCEFAILURE $INSTANCE
SCRIPT_CMD(scriptcmd_instancefailure)
{
	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_INSTANCE )
	{
		if( !IS_SET(arg->d.instance->flags, (INSTANCE_COMPLETED|INSTANCE_FAILED)) )
		{
			p_percent2_trigger(NULL, arg->d.instance, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_FAILED,NULL,0,0,0,0,0);

			SET_BIT(arg->d.instance->flags, INSTANCE_FAILED);
		}
	}
}


//////////////////////////////////////
// J

//////////////////////////////////////
// K

//////////////////////////////////////
// L

// [INSTANCE] LAYOUT <command> <parameters>
// Commands:
//  clear
//  add static <section#>
//  add weighted
//  add group
//  weighted <#> <weight> <section#>
//  group <#> add static <section#>
//  group <#> add weighted
//  group <#> weighted <#> <weight> <section#>
//
// Remarks:
//  Only works in instance scripts and called from the BLUEPRINT_SCHEMATIC trigger.
//  Only allows adding entries, save for the full clear command.  No other form of editting.
//
SCRIPT_CMD(instancecmd_layout)
{
	BLUEPRINT *bp;
	char *rest = argument;

	SETRETURN(0);
	if (!IS_VALID(info->instance) || !IS_TRIGGER(TRIG_BLUEPRINT_SCHEMATIC))
		return;

	bp = info->instance->blueprint;

	PARSE_ARGTYPE(STRING);
	if (ARG_PREFIX("clear"))
	{
		list_clear(bp->layout);
	}
	else if (ARG_PREFIX("add"))
	{
		PARSE_ARGTYPE(STRING);

		if (ARG_PREFIX("static"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(bp->sections)) return;

			BLUEPRINT_LAYOUT_SECTION_DATA *ls = new_blueprint_layout_section_data();
			ls->mode = SECTIONMODE_STATIC;
			ls->section = arg->d.num;

			list_appendlink(bp->layout, ls);
		}
		else if (ARG_PREFIX("weighted"))
		{
			BLUEPRINT_LAYOUT_SECTION_DATA *ls = new_blueprint_layout_section_data();
			ls->mode = SECTIONMODE_WEIGHTED;
			ls->total_weight = 0;

			list_appendlink(bp->layout, ls);
		}
		else if (ARG_PREFIX("group"))
		{
			BLUEPRINT_LAYOUT_SECTION_DATA *ls = new_blueprint_layout_section_data();
			ls->mode = SECTIONMODE_GROUP;

			list_appendlink(bp->layout, ls);
		}
		else
			return;
	}
	else if (ARG_PREFIX("weighted"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->layout))
			return;
		
		BLUEPRINT_LAYOUT_SECTION_DATA *ls = (BLUEPRINT_LAYOUT_SECTION_DATA *)list_nthdata(bp->layout, arg->d.num);
		if (ls->mode != SECTIONMODE_WEIGHTED) return;

		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->sections))
			return;
		
		BLUEPRINT_WEIGHTED_SECTION_DATA *weighted = new_weighted_random_section();
		weighted->weight = weight;
		weighted->section = arg->d.num;
		list_appendlink(ls->weighted_sections, ls);
		ls->total_weight += weight;
	}
	else if (ARG_PREFIX("group"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->layout))
			return;
		
		BLUEPRINT_LAYOUT_SECTION_DATA *gls = (BLUEPRINT_LAYOUT_SECTION_DATA *)list_nthdata(bp->layout, arg->d.num);
		if (gls->mode != SECTIONMODE_GROUP) return;

		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("add"))
		{
			if (ARG_PREFIX("static"))
			{
				PARSE_ARGTYPE(NUMBER);
				if (arg->d.num < 1 || arg->d.num > list_size(bp->sections)) return;

				BLUEPRINT_LAYOUT_SECTION_DATA *ls = new_blueprint_layout_section_data();
				ls->mode = SECTIONMODE_STATIC;
				ls->section = arg->d.num;

				list_appendlink(gls->group, ls);
			}
			else if (ARG_PREFIX("weighted"))
			{
				BLUEPRINT_LAYOUT_SECTION_DATA *ls = new_blueprint_layout_section_data();
				ls->mode = SECTIONMODE_WEIGHTED;
				ls->total_weight = 0;

				list_appendlink(gls->group, ls);
			}
			else
				return;
		}
		else if (ARG_PREFIX("weighted"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(gls->group))
				return;
			
			BLUEPRINT_LAYOUT_SECTION_DATA *ls = (BLUEPRINT_LAYOUT_SECTION_DATA *)list_nthdata(gls->group, arg->d.num);
			if (ls->mode != SECTIONMODE_WEIGHTED) return;

			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(bp->sections))
				return;
			
			BLUEPRINT_WEIGHTED_SECTION_DATA *weighted = new_weighted_random_section();
			weighted->weight = weight;
			weighted->section = arg->d.num;
			list_appendlink(ls->weighted_sections, ls);
			ls->total_weight += weight;
		}
		else
			return;
	}
	else
		return;

	SETRETURN(1);
}

// [INSTANCE] LINKS <command> <parameters>
// Commands:
//  clear
//  add static <from-mode> <from-section#> <from-exit#> <to-mode> <to-section#> <to-entry#>
//  add source <to-mode> <to-section#> <to-entry#>
//  add destination <from-mode> <from-section#> <from-exit#>
//  add weighted
//  add group
//  from <#> <weight> <from-mode> <from-section#> <from-exit#>
//  to <#> <weight> <to-mode> <to-section#> <to-entry#>
//  group <#> add static <from-mode> <from-section#> <from-exit#> <to-mode> <to-section#> <to-entry#>
//  group <#> add source <to-mode> <to-section#> <to-entry#>
//  group <#> add destination <from-mode> <from-section#> <from-exit#>
//  group <#> add weighted
//  group <#> from <#> <weight> <from-mode> <from-section#> <from-exit#>
//  group <#> to <#> <weight> <to-mode> <to-section#> <to-entry#>
//
// Parameters:
//  from-mode			"generated" or "ordinal"
//  to-mode				"generated" or "ordinal"
//
// Remarks:
//  Only works in instance scripts and called from the BLUEPRINT_SCHEMATIC trigger.
//  Only allows adding entries, save for the full clear command.  No other form of editting.
//
SCRIPT_CMD(instancecmd_links)
{
	BLUEPRINT_LAYOUT_SECTION_DATA *group;
	BLUEPRINT_LAYOUT_SECTION_DATA *ls;
	BLUEPRINT *bp;
	int link_count;
	char *rest = argument;

	SETRETURN(0);
	if (!IS_VALID(info->instance) || !IS_TRIGGER(TRIG_BLUEPRINT_SCHEMATIC))
		return;
	
	bp = info->instance->blueprint;
	int sections = blueprint_generation_count(bp);

	PARSE_ARGTYPE(STRING);
	if (ARG_PREFIX("clear"))
	{
		list_clear(bp->links);
	}
	else if (ARG_PREFIX("add"))
	{
		PARSE_ARGTYPE(STRING);

		if (ARG_PREFIX("static"))
		{
			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int from_section = arg->d.num;
			if (from_section < 1 || from_section > sections) return;

			ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int from_link_no = arg->d.num;
			if (from_link_no < 1 || from_link_no > link_count) return;

			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int to_section = arg->d.num;
			if (to_section < 1 || to_section > sections) return;

			ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int to_link_no = arg->d.num;
			if (to_link_no < 1 || to_link_no > link_count) return;

			BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
			ll->mode = LINKMODE_STATIC;

			blueprint_add_weighted_link(ll->from, 1, from_mode ? -from_section : from_section, from_link_no);
			ll->total_from = 1;

			blueprint_add_weighted_link(ll->to, 1, to_mode ? -to_section : to_section, to_link_no);
			ll->total_to = 1;

			list_appendlink(bp->links, ll);
		}
		else if(ARG_PREFIX("source"))
		{
			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int to_section = arg->d.num;
			if (to_section < 1 || to_section > sections) return;

			ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int to_link_no = arg->d.num;
			if (to_link_no < 1 || to_link_no > link_count) return;

			BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
			ll->mode = LINKMODE_SOURCE;

			blueprint_add_weighted_link(ll->to, 1, to_mode ? -to_section : to_section, to_link_no);
			ll->total_to = 1;

			list_appendlink(bp->links, ll);
		}
		else if(ARG_PREFIX("destination"))
		{
			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int from_section = arg->d.num;
			if (from_section < 1 || from_section > sections) return;

			ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int from_link_no = arg->d.num;
			if (from_link_no < 1 || from_link_no > link_count) return;
			
			BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
			ll->mode = LINKMODE_DESTINATION;

			blueprint_add_weighted_link(ll->from, 1, from_mode ? -from_section : from_section, from_link_no);
			ll->total_from = 1;

			list_appendlink(bp->links, ll);
		}
		else if(ARG_PREFIX("weighted"))
		{
			BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
			ll->mode = LINKMODE_WEIGHTED;

			list_appendlink(bp->links, ll);
		}
		else if(ARG_PREFIX("group"))
		{
			BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
			ll->mode = LINKMODE_GROUP;

			list_appendlink(bp->links, ll);
		}
		else
			return;
	}
	else if (ARG_PREFIX("from"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->links))
			return;

		BLUEPRINT_LAYOUT_LINK_DATA *ll = (BLUEPRINT_LAYOUT_LINK_DATA *)list_nthdata(bp->links, arg->d.num);
		if (ll->mode != LINKMODE_WEIGHTED && ll->mode != LINKMODE_SOURCE)
			return;
		
		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		PARSE_ARGTYPE(STRING);
		bool from_mode = TRISTATE;
		if (ARG_PREFIX("generated"))
			from_mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			from_mode = TRUE;
		else
			return;

		PARSE_ARGTYPE(NUMBER);
		int from_section = arg->d.num;
		if (from_section < 1 || from_section > sections) return;

		ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
		link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

		if (link_count < 1) return;

		PARSE_ARGTYPE(NUMBER);
		int from_link_no = arg->d.num;
		if (from_link_no < 1 || from_link_no > link_count) return;

		blueprint_add_weighted_link(ll->from, weight, from_mode ? -from_section : from_section, from_link_no);
		ll->total_from += weight;
	}
	else if (ARG_PREFIX("to"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->links))
			return;

		BLUEPRINT_LAYOUT_LINK_DATA *ll = (BLUEPRINT_LAYOUT_LINK_DATA *)list_nthdata(bp->links, arg->d.num);
		if (ll->mode != LINKMODE_WEIGHTED && ll->mode != LINKMODE_DESTINATION)
			return;

		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		PARSE_ARGTYPE(STRING);
		bool to_mode = TRISTATE;
		if (ARG_PREFIX("generated"))
			to_mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			to_mode = TRUE;
		else
			return;

		PARSE_ARGTYPE(NUMBER);
		int to_section = arg->d.num;
		if (to_section < 1 || to_section > sections) return;

		ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
		link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

		if (link_count < 1) return;

		PARSE_ARGTYPE(NUMBER);
		int to_link_no = arg->d.num;
		if (to_link_no < 1 || to_link_no > link_count) return;

		blueprint_add_weighted_link(ll->to, weight, to_mode ? -to_section : to_section, to_link_no);
		ll->total_to += weight;
	}
	else if (ARG_PREFIX("group"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(bp->links))
			return;
		
		BLUEPRINT_LAYOUT_LINK_DATA *gll = (BLUEPRINT_LAYOUT_LINK_DATA *)list_nthdata(bp->links, arg->d.num);
		if (gll->mode != LINKMODE_GROUP) return;

		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("add"))
		{
			PARSE_ARGTYPE(STRING);

			if (ARG_PREFIX("static"))
			{
				PARSE_ARGTYPE(STRING);
				bool from_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					from_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					from_mode = TRUE;
				else
					return;

				PARSE_ARGTYPE(NUMBER);
				int from_section = arg->d.num;
				if (from_section < 1 || from_section > sections) return;

				ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
				link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

				if (link_count < 1) return;

				PARSE_ARGTYPE(NUMBER);
				int from_link_no = arg->d.num;
				if (from_link_no < 1 || from_link_no > link_count) return;

				PARSE_ARGTYPE(STRING);
				bool to_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					to_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					to_mode = TRUE;
				else
					return;

				PARSE_ARGTYPE(NUMBER);
				int to_section = arg->d.num;
				if (to_section < 1 || to_section > sections) return;

				ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
				link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

				if (link_count < 1) return;

				PARSE_ARGTYPE(NUMBER);
				int to_link_no = arg->d.num;
				if (to_link_no < 1 || to_link_no > link_count) return;

				BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
				ll->mode = LINKMODE_STATIC;

				blueprint_add_weighted_link(ll->from, 1, from_mode ? -from_section : from_section, from_link_no);
				ll->total_from = 1;

				blueprint_add_weighted_link(ll->to, 1, to_mode ? -to_section : to_section, to_link_no);
				ll->total_to = 1;

				list_appendlink(gll->group, ll);
			}
			else if (ARG_PREFIX("source"))
			{
				PARSE_ARGTYPE(STRING);
				bool to_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					to_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					to_mode = TRUE;
				else
					return;

				PARSE_ARGTYPE(NUMBER);
				int to_section = arg->d.num;
				if (to_section < 1 || to_section > sections) return;

				ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
				link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

				if (link_count < 1) return;

				PARSE_ARGTYPE(NUMBER);
				int to_link_no = arg->d.num;
				if (to_link_no < 1 || to_link_no > link_count) return;

				BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
				ll->mode = LINKMODE_SOURCE;

				blueprint_add_weighted_link(ll->to, 1, to_mode ? -to_section : to_section, to_link_no);
				ll->total_to = 1;

				list_appendlink(gll->group, ll);
			}
			else if (ARG_PREFIX("destination"))
			{
				PARSE_ARGTYPE(STRING);
				bool from_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					from_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					from_mode = TRUE;
				else
					return;

				PARSE_ARGTYPE(NUMBER);
				int from_section = arg->d.num;
				if (from_section < 1 || from_section > sections) return;

				ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
				link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

				if (link_count < 1) return;

				PARSE_ARGTYPE(NUMBER);
				int from_link_no = arg->d.num;
				if (from_link_no < 1 || from_link_no > link_count) return;
				
				BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
				ll->mode = LINKMODE_DESTINATION;

				blueprint_add_weighted_link(ll->from, 1, from_mode ? -from_section : from_section, from_link_no);
				ll->total_from = 1;

				list_appendlink(gll->group, ll);
			}
			else if (ARG_PREFIX("weighted"))
			{
				BLUEPRINT_LAYOUT_LINK_DATA *ll = new_blueprint_layout_link_data();
				ll->mode = LINKMODE_WEIGHTED;

				list_appendlink(gll->group, ll);
			}
			else
				return;
		}
		else if (ARG_PREFIX("from"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(gll->group))
				return;

			BLUEPRINT_LAYOUT_LINK_DATA *ll = (BLUEPRINT_LAYOUT_LINK_DATA *)list_nthdata(gll->group, arg->d.num);
			if (ll->mode != LINKMODE_WEIGHTED && ll->mode != LINKMODE_SOURCE)
				return;
			
			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int from_section = arg->d.num;
			if (from_section < 1 || from_section > sections) return;

			ls = blueprint_get_nth_section(bp, (from_mode ? -from_section : from_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int from_link_no = arg->d.num;
			if (from_link_no < 1 || from_link_no > link_count) return;

			blueprint_add_weighted_link(ll->from, weight, from_mode ? -from_section : from_section, from_link_no);
			ll->total_from += weight;
		}
		else if (ARG_PREFIX("to"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(gll->group))
				return;

			BLUEPRINT_LAYOUT_LINK_DATA *ll = (BLUEPRINT_LAYOUT_LINK_DATA *)list_nthdata(gll->group, arg->d.num);
			if (ll->mode != LINKMODE_WEIGHTED && ll->mode != LINKMODE_DESTINATION)
				return;

			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			PARSE_ARGTYPE(NUMBER);
			int to_section = arg->d.num;
			if (to_section < 1 || to_section > sections) return;

			ls = blueprint_get_nth_section(bp, (to_mode ? -to_section : to_section), &group);
			link_count = blueprint_layout_links_count(bp, group ? group : ls, 0);

			if (link_count < 1) return;

			PARSE_ARGTYPE(NUMBER);
			int to_link_no = arg->d.num;
			if (to_link_no < 1 || to_link_no > link_count) return;

			blueprint_add_weighted_link(ll->to, weight, to_mode ? -to_section : to_section, to_link_no);
			ll->total_to += weight;
		}
		else
			return;
	}
	else
		return;

	SETRETURN(1);
}

// [DUNGEON] LEVELS <command> <parameters>
// Commands:
//  clear
//  add static <floor#>
//  add weighted
//  add grouped
//  weighted <#> <weight+> <floor#>
//  group <#> add static <floor#>
//  group <#> add weighted
//  group <#> weighted <#> <weight+> <floor#>
//
// Remarks:
//  Only works in dungeon scripts and called from the DUNGEON_SCHEMATIC trigger.
//  Only allows adding entries, save for the full clear command.  No other form of editting.
SCRIPT_CMD(dungeoncmd_levels)
{
	DUNGEON_INDEX_DATA *dng;
	char *rest = argument;

	SETRETURN(0);

	// Only work while in a dungeon script and running the DUNGEON_SCHEMATIC trigger
	if (!IS_VALID(info->dungeon) || !IS_TRIGGER(TRIG_DUNGEON_SCHEMATIC))
		return;

	dng = info->dungeon->index;

	PARSE_ARGTYPE(STRING);

	if (ARG_PREFIX("clear"))
	{
		list_clear(dng->levels);
	}
	else if (ARG_PREFIX("add"))
	{
		PARSE_ARGTYPE(STRING);

		if (ARG_PREFIX("static"))
		{
			PARSE_ARGTYPE(NUMBER);

			if (arg->d.num < 1 || arg->d.num > list_size(dng->floors))
				return;

			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_STATIC;
			level->floor = arg->d.num;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);
		}
		else if (ARG_PREFIX("weighted"))
		{
			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_WEIGHTED;
			level->total_weight = 0;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);
		}
		else if (ARG_PREFIX("group"))
		{
			DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
			level->mode = LEVELMODE_GROUP;
			list_appendlink(dng->levels, level);
			dungeon_update_level_ordinals(dng);
		}
		else
			return;
	}
	else if (ARG_PREFIX("weighted"))
	{
		PARSE_ARGTYPE(NUMBER);

		if (arg->d.num < 1 || arg->d.num > list_size(dng->levels))
			return;

		DUNGEON_INDEX_LEVEL_DATA *level = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, arg->d.num);
		if (!level || level->mode != LEVELMODE_WEIGHTED) return;

		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(dng->floors))
			return;

		DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
		weighted->weight = weight;
		weighted->floor = arg->d.num;
		list_appendlink(level->weighted_floors, weighted);
		level->total_weight += weight;
	}
	else if(ARG_PREFIX("group"))
	{
		PARSE_ARGTYPE(NUMBER);

		if (arg->d.num < 1 || arg->d.num > list_size(dng->levels))
			return;

		DUNGEON_INDEX_LEVEL_DATA *glevel = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(dng->levels, arg->d.num);
		if (!glevel || glevel->mode != LEVELMODE_GROUP) return;

		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("add"))
		{
			PARSE_ARGTYPE(STRING);

			if (ARG_PREFIX("static"))
			{
				PARSE_ARGTYPE(NUMBER);

				if (arg->d.num < 1 || arg->d.num > list_size(dng->floors))
					return;

				DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
				level->mode = LEVELMODE_STATIC;
				level->floor = arg->d.num;
				list_appendlink(glevel->group, level);
				dungeon_update_level_ordinals(dng);
			}
			else if (ARG_PREFIX("weighted"))
			{
				DUNGEON_INDEX_LEVEL_DATA *level = new_dungeon_index_level();
				level->mode = LEVELMODE_WEIGHTED;
				level->total_weight = 0;
				list_appendlink(glevel->group, level);
				dungeon_update_level_ordinals(dng);
			}
			else
				return;
		}
		else if (ARG_PREFIX("weighted"))
		{
			PARSE_ARGTYPE(NUMBER);

			if (arg->d.num < 1 || arg->d.num > list_size(glevel->group))
				return;

			DUNGEON_INDEX_LEVEL_DATA *level = (DUNGEON_INDEX_LEVEL_DATA *)list_nthdata(glevel->group, arg->d.num);
			if (!level || level->mode != LEVELMODE_WEIGHTED) return;

			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(dng->floors))
				return;

			DUNGEON_INDEX_WEIGHTED_FLOOR_DATA *weighted = new_weighted_random_floor();
			weighted->weight = weight;
			weighted->floor = arg->d.num;
			list_appendlink(level->weighted_floors, weighted);
			level->total_weight += weight;
		}
		else
			return;
	}
	else
		return;
	
	SETRETURN(1);
}

// LOADINSTANCED mobile $WNUM|$MOBILE $ROOM[ $VARIABLENAME]
// LOADINSTANCED object $WNUM|$OBJECT $LEVEL room|here|wear $ENTITY[ $VARIABLE]
SCRIPT_CMD(scriptcmd_loadinstanced)
{
	char *rest;

	if( !info ) return;

	info->progs->lastreturn = 0;

	// Require the calling entity being involved in an instance
	bool valid = false;
	if( IS_VALID(info->dungeon) ) valid = true;
	else if(IS_VALID(info->instance) ) valid = true;
	else if( info->room && IS_VALID(info->room->instance_section) ) valid = true;
	else if( info->mob && IS_SET(info->mob->act2, ACT2_INSTANCE_MOB) ) valid = true;
	else if( info->obj && IS_SET(info->obj->extra3_flags, ITEM_INSTANCE_OBJ) ) valid = true;
	else if( info->token )
	{
		if( info->token->room && IS_VALID(info->token->room->instance_section) ) valid = true;
		else if( info->token->player && IS_SET(info->token->player->act2, ACT2_INSTANCE_MOB) ) valid = true;
		else if( info->token->object && IS_SET(info->token->object->extra3_flags, ITEM_INSTANCE_OBJ) ) valid = true;
	}

	// Not a valid caller
	if( !valid ) return;

	if( !(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING )
		return;

	if( IS_NULLSTR(arg->d.str) ) return;

	if( !str_prefix(arg->d.str, "mobile") )
		script_mload(info, rest, arg, true);

	else if( !str_prefix(arg->d.str, "object") )
		script_oload(info, rest, arg, true);
}

// LOCKADD $OBJECT <context>
SCRIPT_CMD(scriptcmd_lockadd)
{
	char *rest = argument;
	OBJ_DATA *obj;
	OCLU_CONTEXT context;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj))
		return;

	obj = arg->d.obj;

	PARSE_ARGTYPE(STRING);

	if (!oclu_get_context(&context, obj, arg->d.str))
		return;

	if (!context.lock)
		return;

	LOCK_STATE *lock = new_lock_state();
	SET_BIT(lock->flags, LOCK_CREATED);

	*(context.lock) = lock;	// Set the lock on the context

	SETRETURN(1);
}

// LOCKREMOVE $OBJECT <context>
SCRIPT_CMD(scriptcmd_lockremove)
{
	char *rest = argument;
	OBJ_DATA *obj;
	OCLU_CONTEXT context;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj))
		return;
	obj = arg->d.obj;

	PARSE_ARGTYPE(STRING);
	if (!oclu_get_context(&context, obj, arg->d.str))
		return;

	LOCK_STATE *lock = *(context.lock);

	if( !lock )
		return;

	// Only CREATED locks with noremove can be stripped off by this command.
	if( IS_SET(lock->flags, LOCK_NOREMOVE) && !IS_SET(lock->flags, LOCK_CREATED))
		return;

	free_lock_state(lock);

	*(context.lock) = NULL;

	SETRETURN(1);
}

// LOCKSET $LOCKSTATE key $WNUM|$OBJECT
// LOCKSET $LOCKSTATE pick $NUMBER
// LOCKSET $LOCKSTATE flag BOP $STRING
// LOCKSET $LOCKSTATE special add $NUMBER $NUMBER
// LOCKSET $LOCKSTATE special add $OBJECT
// LOCKSET $LOCKSTATE special remove $NUMBER $NUMBER
// LOCKSET $LOCKSTATE special remove $OBJECT
// LOCKSET $LOCKSTATE finalize
//
// $LOCKSTATE can be one of the following
//  - $OBJECT <context>
//  - $EXIT $BOOLEAN
//  - $ROOM $STRING $BOOLEAN
//
// BOP = bit operator (|, &, ^, =)
SCRIPT_CMD(scriptcmd_lockset)
{
	if(!info) return;

	char *rest;
	LOCK_STATE *lock = NULL;
	info->progs->lastreturn = 0;

	if (!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type)
	{
	case ENT_OBJECT:
		if (IS_VALID(arg->d.obj))
		{
			OBJ_DATA *obj = arg->d.obj;
			OCLU_CONTEXT context;

			PARSE_ARGTYPE(STRING);
			if (!oclu_get_context(&context, obj, arg->d.str))
				return;

			lock = *(context.lock);
		}
		break;
	
	case ENT_EXIT: {
			EXIT_DATA *ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
			
			if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_BOOLEAN)
				return;

			if (arg->d.boolean)
				lock = ex ? &ex->door.rs_lock : NULL;
			else
				lock = ex ? &ex->door.lock : NULL;
			break;
		}

	case ENT_ROOM: {
			ROOM_INDEX_DATA *room = arg->d.room;

			if (!room) return;

			if(!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING)
				return;
			
			int door = get_num_dir(arg->d.str);
			if (door < 0 || door >= MAX_DIR) return;

			EXIT_DATA *ex = room->exit[door];

			if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_BOOLEAN)
				return;

			if (arg->d.boolean)
				lock = ex ? &ex->door.rs_lock : NULL;
			else
				lock = ex ? &ex->door.lock : NULL;
			break;
		}
	
	default: return;
	}

	if (!lock) return;	 // No lock

	// OLC created locks or finalized scripted locks
	if (!IS_SET(lock->flags, LOCK_CREATED) || IS_SET(lock->flags, LOCK_FINAL))
	{
		if (info->trigger_type == TRIG_SPELL)
		// Check for other trigger types that are from a spell
		{
			if (IS_SET(lock->flags, LOCK_NOMAGIC))
				return;
		}
		else
		{
			if (IS_SET(lock->flags, LOCK_NOSCRIPT))
				return;
		}
	}

	if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING)
		return;

	if (!str_prefix(arg->d.str, "key"))
	{
		// Should this be allowed?

		// KEY $OBJECT|$WNUM|clear|none
		if (!expand_argument(info, rest, arg))
			return;

		OBJ_INDEX_DATA *pObjIndex = NULL;
		bool clear = FALSE;
		if (arg->type == ENT_WIDEVNUM)
		{
			// Verify the key exists
			if(!(pObjIndex = get_obj_index_wnum(arg->d.wnum)))
				return;
		}
		else if (arg->type == ENT_OBJECT)
		{
			pObjIndex = IS_VALID(arg->d.obj) ? arg->d.obj->pIndexData : NULL;
		}
		else if (arg->type == ENT_STRING)
		{
			if (!str_prefix(arg->d.str, "clear") || !str_prefix(arg->d.str, "none"))
				clear = TRUE;
		}

		if (clear)
		{
			// Clear the current key definition
			lock->key_wnum.pArea = NULL;
			lock->key_wnum.vnum = 0;
		}
		else
		{

			// No item
			if (!pObjIndex) return;

			// Not a key
			if (pObjIndex->item_type != ITEM_KEY) return;

			lock->key_wnum.pArea = pObjIndex->area;
			lock->key_wnum.vnum = pObjIndex->vnum;
		}
	}
	else if(!str_prefix(arg->d.str, "pick"))
	{
		if (!expand_argument(info,rest,arg) || arg->type != ENT_NUMBER)
			return;
		
		lock->pick_chance = arg->d.num;
	}
	else if (!str_prefix(arg->d.str, "flags"))
	{
		char bop[MIL];
		int value;
		rest = one_argument(rest, bop);

		if (!expand_argument(info, rest, arg) || arg->type != ENT_STRING)
			return;

		value = script_flag_value(lock_flags, arg->d.str);
		if (value == NO_FLAG) value = 0;

		int mask = ~(LOCK_CREATED|LOCK_FINAL);	// Allow all flags except CREATED and FINAL

		// Filter out flags for existing/finalized locks
		if (!IS_SET(lock->flags, LOCK_CREATED) || IS_SET(lock->flags, LOCK_FINAL))
		{
			mask = (LOCK_LOCKED | LOCK_MAGIC | LOCK_SNAPKEY | LOCK_BROKEN | LOCK_JAMMED);
		}

		// Keep only allowed flags
		value &= mask;

		// Nothing to change
		if (!value) return;

		switch(bop[0])
		{
			case '|':	// Turn on bits
				SET_BIT(lock->flags, value);
				break;
			
			case '^':	// Toggle bits
				TOGGLE_BIT(lock->flags, value);
				break;

			case '&':	// Mask bits
				lock->flags &= (value | ~mask);		// Make sure all the disallowed bits are kept
				break;
			
			case '!':	// Turn off bits
				REMOVE_BIT(lock->flags, value);
				break;

			case '=':
				lock->flags = (lock->flags & ~mask) | value;
				break;
		}
	}
	else if(!str_prefix(arg->d.str, "special"))
	{
		// Only allow modifying these on unfinalized scripted locks.
		if (!IS_SET(lock->flags, LOCK_CREATED) || IS_SET(lock->flags, LOCK_FINAL))
			return;

		if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return;

		unsigned long id0 = 0, id1 = 0;
		if (!str_prefix(arg->d.str, "add"))
		{
			if (!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_OBJECT)
				return;

			if (!IS_VALID(arg->d.obj))
				return;

			bool found = FALSE;
			LLIST_UID_DATA *luid;
			ITERATOR sxit;
			iterator_start(&sxit, lock->special_keys);
			while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&sxit)) )
			{
				if (luid->ptr == arg->d.obj)
				{
					found = TRUE;
					break;
				}
			}
			iterator_stop(&sxit);

			if (!found)
			{
				luid = new_list_uid_data();
				luid->ptr = arg->d.obj;
				luid->id[0] = arg->d.obj->id[0];
				luid->id[1] = arg->d.obj->id[1];

				list_appendlink(lock->special_keys, luid);
			}
		}
		else if(!str_prefix(arg->d.str, "remove"))
		{
			if (!(rest = expand_argument(info,rest,arg)))
				return;

			if (arg->type == ENT_NUMBER)
			{
				id0 = arg->d.num;
				if (!expand_argument(info, rest, arg) || arg->type != ENT_NUMBER)
					return;
				id1 = arg->d.num;
			}
			else if (arg->type == ENT_OBJECT)
			{
				if (!IS_VALID(arg->d.obj))
					return;
				
				id0 = arg->d.obj->id[0];
				id1 = arg->d.obj->id[1];
			}

			if (!id0 && !id1) return;

			LLIST_UID_DATA *luid;
			ITERATOR sxit;
			iterator_start(&sxit, lock->special_keys);
			while( (luid = (LLIST_UID_DATA *)iterator_nextdata(&sxit)) )
			{
				if (luid->id[0] == id0 && luid->id[1] == id1)
				{
					iterator_remcurrent(&sxit);
					break;
				}
			}
			iterator_stop(&sxit);

		}
		else if (!str_prefix(arg->d.str, "clear"))
		{
			list_clear(lock->special_keys);
		}
		else
			return;
	}
	else if(!str_prefix(arg->d.str, "finalize"))
	{
		if (IS_SET(lock->flags, LOCK_CREATED))
			SET_BIT(lock->flags, LOCK_FINAL);
	}
	else
		return;

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// M

// MAKEINSTANCED $MOBILE|$OBJECT
SCRIPT_CMD(scriptcmd_makeinstanced)
{
	// Only IPROG and DPROGs can call this command
	if(!info || (!info->instance && !info->dungeon)) return;

	info->progs->lastreturn = 0;

	if(!expand_argument(info,argument,arg))
		return;

	if( arg->type == ENT_MOBILE )
	{
		if( !IS_NPC(arg->d.mob) ) return;

		if( !arg->d.mob->in_room ||
			!IS_VALID(arg->d.mob->in_room->instance_section) )
			return;

		INSTANCE_SECTION *section = arg->d.mob->in_room->instance_section;

		if( !IS_VALID(section->instance) )
			return;

		INSTANCE *instance = section->instance;

		if( info->instance && info->instance != instance )
			return;

		if( info->dungeon && (info->dungeon != instance->dungeon || !IS_VALID(instance->dungeon)) )
			return;

		if( !IS_SET(arg->d.mob->act2, ACT2_INSTANCE_MOB) )
		{
			SET_BIT(arg->d.mob->act2, ACT2_INSTANCE_MOB);


			list_remlink(instance->mobiles, arg->d.mob);
			if( IS_VALID(instance->dungeon) )
				list_remlink(instance->dungeon->mobiles, arg->d.mob);
		}
	}
	else if( arg->type == ENT_OBJECT )
	{
		if( !arg->d.obj->in_room ||
			!IS_VALID(arg->d.obj->in_room->instance_section) )
			return;

		INSTANCE_SECTION *section = arg->d.obj->in_room->instance_section;

		if( !IS_VALID(section->instance) )
			return;

		INSTANCE *instance = section->instance;

		if( info->instance && info->instance != instance )
			return;

		if( info->dungeon && (info->dungeon != instance->dungeon || !IS_VALID(instance->dungeon)) )
			return;

		if( !IS_SET(arg->d.obj->extra3_flags, ITEM_INSTANCE_OBJ) )
		{
			SET_BIT(arg->d.obj->extra3_flags, ITEM_INSTANCE_OBJ);

			list_remlink(instance->objects, arg->d.obj);
			if( IS_VALID(instance->dungeon) )
				list_remlink(instance->dungeon->objects, arg->d.obj);
		}
	}
	else
		return;



	info->progs->lastreturn = 1;
}

// MLOAD <wnum>[ <room>][ <variable>]
SCRIPT_CMD(scriptcmd_mload)
{
	script_mload(info,argument,arg, false);
}

// MUTE $PLAYER
SCRIPT_CMD(scriptcmd_mute)
{
	if(!info) return;

	info->progs->lastreturn = 0;

	if(!expand_argument(info,argument,arg) || arg->type != ENT_MOBILE || IS_NPC(arg->d.mob) )
		return;

	if( !arg->d.mob->desc )
		return;

	arg->d.mob->desc->muted++;

	info->progs->lastreturn = 1;
}


//////////////////////////////////////
// N

//////////////////////////////////////
// O

// OLOAD <wnum> [<level>] [room|wear|$ENTITY][ <variable>]
SCRIPT_CMD(scriptcmd_oload)
{
	script_oload(info,argument,arg, false);
}


//////////////////////////////////////
// P

// PAGEAT $PLAYER $STRING
SCRIPT_CMD(scriptcmd_pageat)
{
	char *rest;

	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	BUFFER *buffer = new_buf();

	if( expand_string(info, rest, buffer) )
	{
		// only do it if they actually HAVE scroll enabled
		if( mob->lines > 0)
			page_to_char(buffer->string, mob);
		else
			send_to_char(buffer->string, mob);

		info->progs->lastreturn = 1;
	}
	free_buf(buffer);
}

//////////////////////////////////////
// Q

// QUESTACCEPT $PLAYER[ $SCROLL]
// Finalizes the $PLAYER's pending SCRIPTED quest
//
// $PLAYER - player who is on a quest
// $SCROLL - scroll object to give to player (optional)
//
// Fails if the player does not have a pending SCRIPTED quest.
// Fails if the scroll is defined but does not have WEAR_TAKE set OR is worn.
//
// LASTRETURN will be the resulting countdown timer
SCRIPT_CMD(scriptcmd_questaccept)
{
	char *rest;
	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Must be on a scripted quest that is still generating
	if( !mob->quest->generating || !mob->quest->scripted ) return;

	// Check if there is a SCROLL, if so.. give it to the target
	if( *rest )
	{
		OBJ_DATA *scroll;

		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj) )
			return;

		scroll = arg->d.obj;
		if( !CAN_WEAR(scroll, ITEM_TAKE) || scroll->wear_loc != WEAR_NONE )
			return;

		if( scroll->in_room != NULL )
			obj_from_room(scroll);
		else if( scroll->carried_by != NULL )
			obj_from_char(scroll);
		else if( scroll->in_obj != NULL )
			obj_from_obj(scroll);

		obj_to_char(scroll, mob);
	}

	mob->countdown = 0;
	for (QUEST_PART_DATA *qp = mob->quest->parts; qp != NULL; qp = qp->next)
		mob->countdown += qp->minutes;

	mob->quest->generating = FALSE;
	info->progs->lastreturn = mob->countdown;
}

// QUESTCANCEL $PLAYER[ $CLEANUP]
// Cancels the $PLAYER's pending SCRIPTED quest
//
// $PLAYER  - Cancels the pending quest for this player
// $CLEANUP - script (caller space) used to clean up generated quest parts (optional)
//
// Fails if the player does not have a pending scripted quest.
SCRIPT_CMD(scriptcmd_questcancel)
{
	char *rest;
	CHAR_DATA *mob;
	WNUM wnum;
	int type;
	SCRIPT_DATA *script;

	info->progs->lastreturn = 0;

	if(info->mob) type = PRG_MPROG;
	else if(info->obj) type = PRG_OPROG;
	else if(info->room) type = PRG_RPROG;
	else if(info->token) type = PRG_TPROG;
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Must be on a scripted quest that is still generating
	if( !mob->quest->generating || !mob->quest->scripted ) return;

	// Get cleanup script (if there)
	if( *rest )
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM)
			return;

		wnum = arg->d.wnum;

		if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, type)))
			return;

		// Don't care about response
		execute_script(script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,TRIG_NONE,0,0,0,0,0);
	}

	free_quest(mob->quest);
	mob->quest = NULL;

	info->progs->lastreturn = 1;
}

// QUESTCOMPLETE $player $partno
SCRIPT_CMD(scriptcmd_questcomplete)
{
	char *rest;
	CHAR_DATA *mob;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || !IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type != ENT_NUMBER ) return;

	if(check_quest_custom_task(mob, arg->d.num, true))
		info->progs->lastreturn = 1;
}


// QUESTGENERATE $PLAYER $QUESTRECEIVER $PARTCOUNT $PARTSCRIPT

// QUESTRECEIVER cannot be a wilderness room (for now?)
SCRIPT_CMD(scriptcmd_questgenerate)
{
	char *rest;
	CHAR_DATA *mob;
	int qg_type;
	WNUM qg_wnum;
	CHAR_DATA *qr_mob = NULL;
	OBJ_DATA *qr_obj = NULL;
	ROOM_INDEX_DATA *qr_room = NULL;
	int *tempstores;
	int type, parts;
	WNUM wnum;
	SCRIPT_DATA *script;

	info->progs->lastreturn = 0;

	if(info->mob)
	{
		type = PRG_MPROG;
		tempstores = info->mob->tempstore;

		if( !IS_NPC(info->mob) )
			return;

		qg_type = QUESTOR_MOB;
		qg_wnum.pArea = info->mob->pIndexData->area;
		qg_wnum.vnum = info->mob->pIndexData->vnum;
	}
	else if(info->obj)
	{
		type = PRG_OPROG;
		tempstores = info->obj->tempstore;

		qg_type = QUESTOR_OBJ;
		qg_wnum.pArea = info->obj->pIndexData->area;
		qg_wnum.vnum = info->obj->pIndexData->vnum;
	}
	else if(info->room)
	{
		type = PRG_RPROG;
		tempstores = info->room->tempstore;
		if( info->room->wilds || info->room->source )
			return;

		qg_type = QUESTOR_ROOM;
		qg_wnum.pArea = info->room->area;
		qg_wnum.vnum = info->room->vnum;
	}
	else if(info->token)
	{
		type = PRG_TPROG;
		tempstores = info->token->tempstore;

		// Select the owner
		if( info->token->player )
		{
			if( !IS_NPC(info->token->player) )
				return;

			qg_type = QUESTOR_MOB;
			qg_wnum.pArea = info->token->player->pIndexData->area;
			qg_wnum.vnum = info->token->player->pIndexData->vnum;
		}
		else if( info->token->object )
		{
			qg_type = QUESTOR_OBJ;
			qg_wnum.pArea = info->token->object->pIndexData->area;
			qg_wnum.vnum = info->token->object->pIndexData->vnum;
		}
		else if( info->token->room )
		{
			if( info->token->room->wilds || info->token->room->source )
				return;

			qg_type = QUESTOR_ROOM;
			qg_wnum.pArea = info->token->room->area;
			qg_wnum.vnum = info->token->room->vnum;
		}
		else
			return;
	}
	else
		return;


	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob) || IS_QUESTING(arg->d.mob)) return;

	mob = arg->d.mob;

	// Get quest receiver
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE )
	{
		if( !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob) )
			return;

		qr_mob = arg->d.mob;
	}
	else if( arg->type == ENT_OBJECT )
	{
		if( !IS_VALID(arg->d.obj) )
			return;

		qr_obj = arg->d.obj;
	}
	else if( arg->type == ENT_ROOM )
	{
		if( arg->d.room == NULL || (arg->d.room->wilds != NULL) || (arg->d.room->source != NULL) )
			return;

		qr_room = arg->d.room;
	}

	if( !qr_mob && !qr_obj && !qr_room )
		return;

	// Get part count
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: parts = atoi(arg->d.str); break;
	case ENT_NUMBER: parts = arg->d.num; break;
	default: parts = 0; break;
	}

	if( parts < 1 )
		return;

	// Get generator script
	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM)
		return;

	wnum = arg->d.wnum;

	if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, type)))
		return;

	mob->quest = new_quest();
	mob->quest->generating = TRUE;
	mob->quest->scripted = TRUE;
	mob->quest->questgiver_type = qg_type;
	mob->quest->questgiver = qg_wnum;
	if( qr_mob )
	{
		mob->quest->questreceiver_type = QUESTOR_MOB;
		mob->quest->questreceiver.pArea = qr_mob->pIndexData->area;
		mob->quest->questreceiver.vnum = qr_mob->pIndexData->vnum;
	}
	else if( qr_obj )
	{
		mob->quest->questreceiver_type = QUESTOR_OBJ;
		mob->quest->questreceiver.pArea = qr_obj->pIndexData->area;
		mob->quest->questreceiver.vnum = qr_obj->pIndexData->vnum;
	}
	else if( qr_room )
	{
		mob->quest->questreceiver_type = QUESTOR_ROOM;
		mob->quest->questreceiver.pArea = qr_room->area;
		mob->quest->questreceiver.vnum = qr_room->vnum;
	}

	bool success = TRUE;
	for(int i = 0; i < parts; i++)
	{
		QUEST_PART_DATA *part = new_quest_part();

		part->next = mob->quest->parts;
		mob->quest->parts = part;
		part->index = parts - i;

		tempstores[0] = part->index;

		if( execute_script(script, info->mob, info->obj, info->room, info->token, NULL, NULL, NULL, mob, NULL, NULL, NULL, NULL,NULL, NULL,NULL,NULL,TRIG_QUEST_PART,0,0,0,0,0) <= 0 )
		{
			success = FALSE;
			break;
		}
	}

	if( success )
	{
		info->progs->lastreturn = 1;
	}
	else
	{
		free_quest(mob->quest);
		mob->quest = NULL;
	}
}

// QUESTPARTCUSTOM $PLAYER $STRING[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartcustom)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(!IS_NULLSTR(arg->d.str))
		return;

	QUEST_PART_DATA *part = ch->quest->parts;
	sprintf(buf, "{xTask {Y%d{x: %s{x.", part->index, arg->d.str);

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	part->description = str_dup(buf);
	part->custom_task = TRUE;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTGETITEM $PLAYER $OBJECT[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgetitem)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	OBJ_DATA *obj;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_OBJECT || !IS_VALID(arg->d.obj)) return;
	obj = arg->d.obj;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Retrieve {Y%s{x from {Y%s{x in {Y%s{x.",
		part->index,
		obj->short_descr,
		obj->in_room->name,
		obj->in_room->area->name);

	part->description = str_dup(buf);
	free_string(obj->owner);
	obj->owner = str_dup(ch->name);
	part->pObj = obj;
	part->area = obj->pIndexData->area;
	part->obj = obj->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTGOTO $PLAYER first|second|both|$ROOM[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartgoto)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	ROOM_INDEX_DATA *destination;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	destination = NULL;
	switch(arg->type) {
	case ENT_STRING:
		destination = get_random_room(ch, get_continent(arg->d.str));
		break;
	case ENT_ROOM:
		destination = arg->d.room;
		break;
	default: return;
	}

	if(!destination)
		return;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Travel to {Y%s{x in {Y%s{x.",
		part->index,
		destination->name,
		destination->area->name);

	part->description = str_dup(buf);
	part->area = destination->area;
	part->room = destination->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

// QUESTPARTRESCUE $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartrescue)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob)) return;
	target = arg->d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Rescue {Y%s{x from {Y%s{x in {Y%s{x.",
		part->index,
		target->short_descr,
		target->in_room->name,
		target->in_room->area->name);

	part->description = str_dup(buf);
	part->area = target->pIndexData->area;
	part->mob_rescue = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}


// QUESTPARTSLAY $PLAYER $TARGET[ $MINUTES]
SCRIPT_CMD(scriptcmd_questpartslay)
{
	char buf[MSL];
	char *rest;
	CHAR_DATA *ch;
	CHAR_DATA *target;
	int minutes;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating ) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_MOBILE || !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob)) return;
	target = arg->d.mob;

	minutes = number_range(10,20);
	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
			return;

		minutes = UMAX(arg->d.num,1);
	}

	QUEST_PART_DATA *part = ch->quest->parts;

	sprintf(buf, "{xTask {Y%d{x: Slay {Y%s{x.  %s was last seen in {Y%s{x.",
		part->index,
		target->short_descr,
		target->sex == SEX_MALE ? "He" :
		target->sex == SEX_FEMALE ? "She" : "It",
		target->in_room->area->name);

	part->description = str_dup(buf);
	part->area = target->pIndexData->area;
	part->mob = target->pIndexData->vnum;
	part->minutes = minutes;

	info->progs->lastreturn = 1;
}

char *__get_questscroll_args(SCRIPT_VARINFO *info, char *argument, SCRIPT_PARAM *arg,
	char **header, char **footer, int *width, char **prefix, char **suffix)
{
	char *rest;

	*header = NULL;
	*footer = NULL;
	*width = 0;
	*prefix = NULL;
	*suffix = NULL;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING)
		return NULL;

	*header = str_dup(arg->d.str);

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return NULL;

	*footer = str_dup(arg->d.str);

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return NULL;

	if(arg->d.num < 0)
		return NULL;

	*width = arg->d.num;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return NULL;

	*prefix = str_dup(arg->d.str);

	if( *width > 0 )
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
			return NULL;

		*suffix = str_dup(arg->d.str);
	}

	return rest;
}

// QUESTSCROLL $PLAYER $QUESTGIVER $WNUM $HEADER $FOOTER $WIDTH $PREFIX[ $SUFFIX] $VARIABLENAME
SCRIPT_CMD(scriptcmd_questscroll)
{
	char *header, *footer, *prefix, *suffix;
	int width;
	char questgiver[MSL];
	char *rest;
	CHAR_DATA *ch;
	WNUM wnum;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	ch = arg->d.mob;

	// Must be in the generation phase
	if( ch->quest == NULL || !ch->quest->generating || !ch->quest->scripted ) return;

	// Get questreceiver description
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_MOBILE )
	{
		if( !IS_VALID(arg->d.mob) || !IS_NPC(arg->d.mob) )
			return;

		strncpy(questgiver, arg->d.mob->short_descr, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_OBJECT )
	{
		if( !IS_VALID(arg->d.obj) )
			return;

		strncpy(questgiver, arg->d.obj->short_descr, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_ROOM )
	{
		if( arg->d.room == NULL || arg->d.room->wilds || arg->d.room->source )
			return;

		strncpy(questgiver, arg->d.room->name, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else if( arg->type == ENT_STRING )
	{
		if( IS_NULLSTR(arg->d.str) )
			return;

		strncpy(questgiver, arg->d.str, MSL-1);
		questgiver[MSL-1] = '\0';
	}
	else
		return;

	// Get scroll vnum
	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM)
		return;

	wnum = arg->d.wnum;
	if( !wnum.pArea || wnum.vnum < 1 || !get_obj_index(wnum.pArea, wnum.vnum))
		return;

	rest = __get_questscroll_args(info, rest, arg, &header, &footer, &width, &prefix, &suffix);
	if( rest && *rest )
	{
		rest = expand_argument(info,rest,arg);

		if( rest && arg->type == ENT_STRING )
		{
			OBJ_DATA *scroll = generate_quest_scroll(ch,questgiver,wnum,header,footer,prefix,suffix,width);
			if( scroll != NULL )
			{
				variables_set_object(info->var, arg->d.str, scroll);
				info->progs->lastreturn = 1;
			}
		}
	}

	if( header ) free_string(header);
	if( footer ) free_string(footer);
	if( prefix ) free_string(prefix);
	if( suffix ) free_string(suffix);
}


//////////////////////////////////////
// R

// RECKONING FIELD OP NUMBER
// Affects the parameters of a Reckoing.
// Most fields cannot be modified during a reckoning
//
// FIELDS (allowed during a reckoning)
//  cooldown
//
// FIELDS (allowed when no reckoning is active)
//  chance
//  cooldown
//  duration
//  intensity
//
SCRIPT_CMD(scriptcmd_reckoning)
{
	char *rest;
	char field[MIL+1];
	char op[MIL+1];
	int *ptr = NULL;
	int min, max;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING)
		return;

	strncpy(field, arg->d.str, MIL);
	field[MIL] = 0;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING)
		return;

	strncpy(op, arg->d.str, MIL);
	op[MIL] = 0;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER)
		return;

	int value = arg->d.num;

	ptr = NULL;

	if( reckoning_timer > 0 )
	{
		if( !str_cmp(field, "cooldown") )	{ ptr = &reckoning_cooldown; min = RECKONING_COOLDOWN_MIN; max = RECKONING_COOLDOWN_MAX; }
	}
	else
	{
		if( !str_cmp(field, "chance") )			{ ptr = &reckoning_chance; min = RECKONING_CHANCE_MIN; max = RECKONING_CHANCE_MAX; }
		else if( !str_cmp(field, "cooldown") )	{ ptr = &reckoning_cooldown; min = RECKONING_COOLDOWN_MIN; max = RECKONING_COOLDOWN_MAX; }
		else if( !str_cmp(field, "duration") )	{ ptr = &reckoning_duration; min = RECKONING_DURATION_MIN; max = RECKONING_DURATION_MAX; }
		else if( !str_cmp(field, "intensity") )	{ ptr = &reckoning_intensity; min = RECKONING_INTENSITY_MIN; max = RECKONING_INTENSITY_MAX; }
	}

	if( !ptr ) return;

	switch(op[0])
	{
	case '=': *ptr = value; break;
	case '+': *ptr += value; break;
	case '-': *ptr -= value; break;
	default:
		return;
	}

	*ptr = URANGE(min, *ptr, max);

	info->progs->lastreturn = 1;
}


// REVOKESKILL player name
// REVOKESKILL player wnum
SCRIPT_CMD(scriptcmd_revokeskill)
{
	char *rest;
	SKILL_ENTRY *entry;

	CHAR_DATA *mob;
	TOKEN_INDEX_DATA *token_index = NULL;
	int sn = -1;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || IS_NPC(arg->d.mob)) return;

	mob = arg->d.mob;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if( arg->type == ENT_STRING ) {
		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;

		entry = skill_entry_findsn(mob->sorted_skills, sn);

	} else if( arg->type == ENT_WIDEVNUM ) {
		token_index = get_token_index(arg->d.wnum.pArea, arg->d.wnum.vnum);

		if( !token_index ) return;

		entry = skill_entry_findtokenindex(mob->sorted_skills, token_index);
	}
	else
		return;

	if( !entry ) return;

	skill_entry_removeentry(&mob->sorted_skills, entry);
	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// S

// SENDFLOOR $MOBILE $DUNGEON|$DUNGEONROOM $FLOOR $MODE[ 'group'|'all']
SCRIPT_CMD(scriptcmd_sendfloor)
{
	char *rest;
	CHAR_DATA *ch, *vch, *next;
	DUNGEON *dungeon;
	int floor, mode;

	info->progs->lastreturn = 0;

	if( !(rest = expand_argument(info,argument,arg)) || arg->type != ENT_MOBILE )
		return;

	ch = arg->d.mob;

	if( !(rest = expand_argument(info,rest,arg)) )
		return;

	dungeon = NULL;
	if( arg->type == ENT_DUNGEON )
		dungeon = arg->d.dungeon;
	else if( arg->type == ENT_ROOM )
		dungeon = get_room_dungeon(arg->d.room);

	if( !IS_VALID(dungeon) )
		return;

	if( !(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER )
		return;

	floor = arg->d.num;

	if( floor < 1 || floor > list_size(dungeon->floors) )
		return;

	INSTANCE *instance = (INSTANCE *)list_nthdata(dungeon->floors, floor);
	if( !IS_VALID(instance) )
		return;

	if( !instance->entrance )
		return;

	if( !(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING )
		return;

	mode = script_flag_value(transfer_modes, arg->d.str);
	if( mode == NO_FLAG ) mode = TRANSFER_MODE_PORTAL;

	bool group = false;
	bool all = false;
	if( rest && *rest )
	{
		group = !str_prefix(arg->d.str, "group");
		all = !str_prefix(arg->d.str, "all");
	}

	if( group )
	{
		for (vch = ch->in_room->people; vch; vch = next) {
			next = vch->next_in_room;
			if (PROG_FLAG(vch,PROG_AT)) continue;
			if ((!IS_NPC(vch) || !IS_SET(vch->act2,ACT2_INSTANCE_MOB)) &&
				is_same_group(ch,vch)) {
				if (vch->position != POS_STANDING) continue;
				if (room_is_private(instance->entrance, info->mob)) break;
				do_mob_transfer(vch,instance->entrance,false,mode);
			}
		}
	}
	else if( all )
	{
		for (vch = ch->in_room->people; vch; vch = next) {
			next = vch->next_in_room;
			if (PROG_FLAG(vch,PROG_AT)) continue;
			if (!IS_NPC(vch) || !IS_SET(vch->act2,ACT2_INSTANCE_MOB)) {
				if (vch->position != POS_STANDING) continue;
				if (room_is_private(instance->entrance, info->mob)) break;
				do_mob_transfer(vch,instance->entrance,false,mode);
			}
		}
	}
	else
	{
		if( PROG_FLAG(ch,PROG_AT) ) return;

		do_mob_transfer(ch, instance->entrance, false, mode);
	}

	info->progs->lastreturn = 0;
}

SCRIPT_CMD(scriptcmd_setalign)
{
}

SCRIPT_CMD(scriptcmd_setclass)
{
}

SCRIPT_CMD(scriptcmd_setrace)
{
}

SCRIPT_CMD(scriptcmd_setsubclass)
{
}

static int cmd_cmp(void *a, void *b)
{
	return str_cmp((char*)a, (char*)b);
}

// SHOWCOMMAND $PLAYER [STRING]
//
// Function: Appends a command string to the list of extra commands for use in the 'commands' command.
//
// Remarks: can only be used in the SHOWCOMMANDS trigger
SCRIPT_CMD(scriptcmd_showcommand)
{
	char *rest = argument;
	CHAR_DATA *ch;

	SETRETURN(0);
	if (!IS_TRIGGER(TRIG_SHOWCOMMANDS))
		return;

	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;
	
	// Only allow players
	if(!IS_VALID(ch) || IS_NPC(ch)) return;

	// Not in the middle of acquiring commands
	if(!ch->pcdata->extra_commands) return;

	BUFFER *buffer = new_buf();
	if (PARSE_STR(buffer) && !list_contains(ch->pcdata->extra_commands, buffer->string, cmd_cmp))
	{
		list_appendlink(ch->pcdata->extra_commands, str_dup(buffer->string));
		SETRETURN(1);
	}
	free_buf(buffer);
}

// SPAWNDUNGEON $PLAYER $DUNGEONWNUM floor $FLOOR $VARIABLENAME
// SPAWNDUNGEON $PLAYER $DUNGEONWNUM room $SPECIALROOM $VARIABLENAME
// This does not automatically send the player to the dungeon
// That is what the room variable is for.
//
SCRIPT_CMD(scriptcmd_spawndungeon)
{
	char *rest;
	CHAR_DATA *ch;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: ch = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: ch = arg->d.mob; break;
	default: ch = NULL; break;
	}

	if (!ch || IS_NPC(ch))
		return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM)
		return;

	WNUM wnum = arg->d.wnum;
	if (!wnum.pArea || wnum.vnum < 1)
		return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING || !*rest)
		return;

	ROOM_INDEX_DATA *room = NULL;
	if (!str_prefix(arg->d.str, "floor"))
	{
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_NUMBER || !*rest)
			return;

		int floor = arg->d.num;

		room = spawn_dungeon_player_floor(ch, wnum.pArea, wnum.vnum, floor);
	}
	else if(!str_prefix(arg->d.str, "room"))
	{
		// TODO: allow for string named lookups
		if(!(rest = expand_argument(info,rest,arg)) || (arg->type != ENT_NUMBER && arg->type != ENT_STRING) || !*rest)
			return;

		if(arg->type == ENT_NUMBER)
		{
			room = spawn_dungeon_player_special_room(ch, wnum.pArea, wnum.vnum, arg->d.num, NULL);
		}
		else if (arg->type == ENT_STRING)
		{
			room = spawn_dungeon_player_special_room(ch, wnum.pArea, wnum.vnum, 0, arg->d.str);
		}
	}


	if( !room )
		return;

	variables_set_room(info->var,rest,room);
	info->progs->lastreturn = 1;
}

// [DUNGEON] SPECIALEXITS <command> <parameters>
// Commands:
//   clear
//   add static <from-mode> <from-level#> <from-exit#> <to-mode> <to-level#> <to-entry#>
//   add source <to-mode> <to-level#> <to-entry#>
//   add destination <from-mode> <from-level#> <from-exit#>
//   add weighted
//   add group
//   from <#> <weight> <from-mode> <from-level#> <from-exit#>
//   to <#> <weight> <to-mode> <to-level#> <to-entry#>
//   group <#> add static <from-mode> <from-level#> <from-exit#> <to-mode> <to-level#> <to-entry#>
//   group <#> add source <to-mode> <to-level#> <to-entry#>
//   group <#> add destination <from-mode> <from-level#> <from-exit#>
//   group <#> add weighted
//   group <#> from <#> <weight> <from-mode> <from-level#> <from-exit#>
//   group <#> to <#> <weight> <to-mode> <to-level#> <to-entry#>
//
// Parameters:
//  from-mode		generated or ordinal
//  to-mode			generated or ordinal
//
// Remarks:
//   Only works in dungeon scripts and runs on DUNGEON_SCHEMATIC triggers.
//
SCRIPT_CMD(dungeoncmd_specialexits)
{
	DUNGEON_INDEX_DATA *dng;
	DUNGEON_INDEX_LEVEL_DATA *group;
	char *rest = argument;

	SETRETURN(0);

	if (!IS_VALID(info->dungeon) || !IS_TRIGGER(TRIG_DUNGEON_SCHEMATIC))
		return;

	dng = info->dungeon->index;
	int levels = dungeon_index_generation_count(dng);

	PARSE_ARGTYPE(STRING);
	if (ARG_PREFIX("clear"))
	{
		list_clear(dng->special_exits);
	}
	else if (ARG_PREFIX("add"))
	{
		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("static"))
		{
			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			// from-level
			PARSE_ARGTYPE(NUMBER);
			int from_level = arg->d.num;
			if (from_level < 1 || from_level > levels)
				return;
			
			// from-exit
			DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
			int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

			PARSE_ARGTYPE(NUMBER);
			int from_exit = arg->d.num;
			if (from_exit < 1 || from_exit > from_exits)
				return;

			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			// to-level
			PARSE_ARGTYPE(NUMBER);
			int to_level = arg->d.num;
			if (to_level < 1 || to_level > levels)
				return;

			// to-exit
			DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
			int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

			PARSE_ARGTYPE(NUMBER);
			int to_entry = arg->d.num;
			if (to_entry < 1 || to_entry > to_entries)
				return;

			if (from_mode == to_mode && from_level == to_level && from_exit == to_entry)
				return;
			
			DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
			ex->mode = EXITMODE_STATIC;
			ex->total_from = 1;
			ex->total_to = 1;
			
			add_dungeon_index_weighted_exit_data(ex->from, 1, from_mode?-from_level:from_level, from_exit);
			add_dungeon_index_weighted_exit_data(ex->to, 1, to_mode?-to_level:to_level, to_entry);

			list_appendlink(dng->special_exits, ex);
		}
		else if (ARG_PREFIX("source"))
		{
			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			// to-level
			PARSE_ARGTYPE(NUMBER);
			int to_level = arg->d.num;
			if (to_level < 1 || to_level > levels)
				return;

			// to-exit
			DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
			int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

			PARSE_ARGTYPE(NUMBER);
			int to_entry = arg->d.num;
			if (to_entry < 1 || to_entry > to_entries)
				return;

			DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
			ex->mode = EXITMODE_WEIGHTED_SOURCE;
			ex->total_to = 1;
			
			add_dungeon_index_weighted_exit_data(ex->to, 1, to_mode?-to_level:to_level, to_entry);

			list_appendlink(dng->special_exits, ex);
		}
		else if (ARG_PREFIX("destination"))
		{
			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			// from-level
			PARSE_ARGTYPE(NUMBER);
			int from_level = arg->d.num;
			if (from_level < 1 || from_level > levels)
				return;
			
			// from-exit
			DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
			int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

			PARSE_ARGTYPE(NUMBER);
			int from_exit = arg->d.num;
			if (from_exit < 1 || from_exit > from_exits)
				return;

			DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
			ex->mode = EXITMODE_WEIGHTED_DEST;
			ex->total_from = 1;
			
			add_dungeon_index_weighted_exit_data(ex->from, 1, from_mode?-from_level:from_level, from_exit);

			list_appendlink(dng->special_exits, ex);
		}
		else if (ARG_PREFIX("weighted"))
		{
			DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
			ex->mode = EXITMODE_WEIGHTED;

			list_appendlink(dng->special_exits, ex);
		}
		else if (ARG_PREFIX("group"))
		{
			DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
			ex->mode = EXITMODE_GROUP;

			list_appendlink(dng->special_exits, ex);			
		}
		else
			return;
	}
	else if(ARG_PREFIX("from"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(dng->special_exits))
			return;

		DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, arg->d.num);
		if (ex->mode != EXITMODE_WEIGHTED && ex->mode != EXITMODE_WEIGHTED_SOURCE)
			return;

		// weight
		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		// generated|ordinal
		PARSE_ARGTYPE(STRING);
		bool from_mode = TRISTATE;
		if (ARG_PREFIX("generated"))
			from_mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			from_mode = TRUE;
		else
			return;

		// from-level
		PARSE_ARGTYPE(NUMBER);
		int from_level = arg->d.num;
		if (from_level < 1 || from_level > levels)
			return;
		
		// from-exit
		DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
		int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

		PARSE_ARGTYPE(NUMBER);
		int from_exit = arg->d.num;
		if (from_exit < 1 || from_exit > from_exits)
			return;

		add_dungeon_index_weighted_exit_data(ex->from, weight, from_mode?-from_level:from_level, from_exit);
		ex->total_from += weight;
	}
	else if (ARG_PREFIX("to"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(dng->special_exits))
			return;

		DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, arg->d.num);
		if (ex->mode != EXITMODE_WEIGHTED && ex->mode != EXITMODE_WEIGHTED_DEST)
			return;

		// weight
		PARSE_ARGTYPE(NUMBER);
		int weight = arg->d.num;
		if (weight < 1) return;

		// generated|ordinal
		PARSE_ARGTYPE(STRING);
		bool to_mode = TRISTATE;
		if (ARG_PREFIX("generated"))
			to_mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			to_mode = TRUE;
		else
			return;

		// to-level
		PARSE_ARGTYPE(NUMBER);
		int to_level = arg->d.num;
		if (to_level < 1 || to_level > levels)
			return;

		// to-exit
		DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
		int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

		PARSE_ARGTYPE(NUMBER);
		int to_entry = arg->d.num;
		if (to_entry < 1 || to_entry > to_entries)
			return;

		add_dungeon_index_weighted_exit_data(ex->to, weight, to_mode?-to_level:to_level, to_entry);
		ex->total_to += weight;
	}
	else if(ARG_PREFIX("group"))
	{
		PARSE_ARGTYPE(NUMBER);
		if (arg->d.num < 1 || arg->d.num > list_size(dng->special_exits))
			return;

		DUNGEON_INDEX_SPECIAL_EXIT *gex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(dng->special_exits, arg->d.num);
		if (gex->mode != EXITMODE_GROUP)
			return;

		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("add"))
		{
			DUNGEON_INDEX_LEVEL_DATA *group;
			int levels = dungeon_index_generation_count(dng);

			PARSE_ARGTYPE(STRING);
			if (ARG_PREFIX("static"))
			{
				// generated|ordinal
				PARSE_ARGTYPE(STRING);
				bool from_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					from_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					from_mode = TRUE;
				else
					return;

				// from-level
				PARSE_ARGTYPE(NUMBER);
				int from_level = arg->d.num;
				if (from_level < 1 || from_level > levels)
					return;
				
				// from-exit
				DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
				int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

				PARSE_ARGTYPE(NUMBER);
				int from_exit = arg->d.num;
				if (from_exit < 1 || from_exit > from_exits)
					return;

				// generated|ordinal
				PARSE_ARGTYPE(STRING);
				bool to_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					to_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					to_mode = TRUE;
				else
					return;

				// to-level
				PARSE_ARGTYPE(NUMBER);
				int to_level = arg->d.num;
				if (to_level < 1 || to_level > levels)
					return;

				// to-exit
				DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
				int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

				PARSE_ARGTYPE(NUMBER);
				int to_entry = arg->d.num;
				if (to_entry < 1 || to_entry > to_entries)
					return;

				if (from_mode == to_mode && from_level == to_level && from_exit == to_entry)
					return;
				
				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_STATIC;
				ex->total_from = 1;
				ex->total_to = 1;
				
				add_dungeon_index_weighted_exit_data(ex->from, 1, from_mode?-from_level:from_level, from_exit);
				add_dungeon_index_weighted_exit_data(ex->to, 1, to_mode?-to_level:to_level, to_entry);

				list_appendlink(gex->group, ex);
			}
			else if (ARG_PREFIX("source"))
			{
				// generated|ordinal
				PARSE_ARGTYPE(STRING);
				bool to_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					to_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					to_mode = TRUE;
				else
					return;

				// to-level
				PARSE_ARGTYPE(NUMBER);
				int to_level = arg->d.num;
				if (to_level < 1 || to_level > levels)
					return;

				// to-exit
				DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
				int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

				PARSE_ARGTYPE(NUMBER);
				int to_entry = arg->d.num;
				if (to_entry < 1 || to_entry > to_entries)
					return;

				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED_SOURCE;
				ex->total_to = 1;
				
				add_dungeon_index_weighted_exit_data(ex->to, 1, to_mode?-to_level:to_level, to_entry);

				list_appendlink(gex->group, ex);
			}
			else if (ARG_PREFIX("destination"))
			{
				// generated|ordinal
				PARSE_ARGTYPE(STRING);
				bool from_mode = TRISTATE;
				if (ARG_PREFIX("generated"))
					from_mode = FALSE;
				else if (ARG_PREFIX("ordinal"))
					from_mode = TRUE;
				else
					return;

				// from-level
				PARSE_ARGTYPE(NUMBER);
				int from_level = arg->d.num;
				if (from_level < 1 || from_level > levels)
					return;
				
				// from-exit
				DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
				int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

				PARSE_ARGTYPE(NUMBER);
				int from_exit = arg->d.num;
				if (from_exit < 1 || from_exit > from_exits)
					return;

				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED_DEST;
				ex->total_from = 1;
				
				add_dungeon_index_weighted_exit_data(ex->from, 1, from_mode?-from_level:from_level, from_exit);

				list_appendlink(gex->group, ex);
			}
			else if (ARG_PREFIX("weighted"))
			{
				DUNGEON_INDEX_SPECIAL_EXIT *ex = new_dungeon_index_special_exit();
				ex->mode = EXITMODE_WEIGHTED;

				list_appendlink(gex->group, ex);
			}
			else
				return;
		}
		else if(ARG_PREFIX("from"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(gex->group))
				return;

			DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, arg->d.num);
			if (ex->mode != EXITMODE_WEIGHTED && ex->mode != EXITMODE_WEIGHTED_SOURCE)
				return;

			// weight
			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool from_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				from_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				from_mode = TRUE;
			else
				return;

			// from-level
			PARSE_ARGTYPE(NUMBER);
			int from_level = arg->d.num;
			if (from_level < 1 || from_level > levels)
				return;
			
			// from-exit
			DUNGEON_INDEX_LEVEL_DATA *from_level_data = dungeon_index_get_nth_level(dng, (from_mode?-from_level:from_level),&group);
			int from_exits = get_dungeon_index_level_special_exits(dng, group?group:from_level_data);

			PARSE_ARGTYPE(NUMBER);
			int from_exit = arg->d.num;
			if (from_exit < 1 || from_exit > from_exits)
				return;

			add_dungeon_index_weighted_exit_data(gex->from, weight, from_mode?-from_level:from_level, from_exit);
			gex->total_from += weight;
		}
		else if (ARG_PREFIX("to"))
		{
			PARSE_ARGTYPE(NUMBER);
			if (arg->d.num < 1 || arg->d.num > list_size(gex->group))
				return;

			DUNGEON_INDEX_SPECIAL_EXIT *ex = (DUNGEON_INDEX_SPECIAL_EXIT *)list_nthdata(gex->group, arg->d.num);
			if (ex->mode != EXITMODE_WEIGHTED && ex->mode != EXITMODE_WEIGHTED_DEST)
				return;

			// weight
			PARSE_ARGTYPE(NUMBER);
			int weight = arg->d.num;
			if (weight < 1) return;

			// generated|ordinal
			PARSE_ARGTYPE(STRING);
			bool to_mode = TRISTATE;
			if (ARG_PREFIX("generated"))
				to_mode = FALSE;
			else if (ARG_PREFIX("ordinal"))
				to_mode = TRUE;
			else
				return;

			// to-level
			PARSE_ARGTYPE(NUMBER);
			int to_level = arg->d.num;
			if (to_level < 1 || to_level > levels)
				return;

			// to-exit
			DUNGEON_INDEX_LEVEL_DATA *to_level_data = dungeon_index_get_nth_level(dng, (to_mode?-to_level:to_level),&group);
			int to_entries = get_dungeon_index_level_special_entrances(dng, group?group:to_level_data);

			PARSE_ARGTYPE(NUMBER);
			int to_entry = arg->d.num;
			if (to_entry < 1 || to_entry > to_entries)
				return;

			add_dungeon_index_weighted_exit_data(gex->to, weight, to_mode?-to_level:to_level, to_entry);
			gex->total_to += weight;
		}
		else
			return;
	}
	else
		return;

	SETRETURN(1);
}

// SPECIALKEY $SHIP $WIDEVNUM $VARIABLENAME
// TODO: Make it possible to do it for dungeons and instances
SCRIPT_CMD(scriptcmd_specialkey)
{
	char *rest = argument;
	LLIST *keys;
	OBJ_INDEX_DATA *index;
	OBJ_DATA *obj;

	if(!info) return;

	SETRETURN(0);

	if (!PARSE_ARG)
		return;

	keys = NULL;
	if( arg->type == ENT_SHIP )
	{
		keys = IS_VALID(arg->d.ship) ? arg->d.ship->special_keys : NULL;
	}

	if( !IS_VALID(keys) )
		return;

	PARSE_ARGTYPE(WIDEVNUM);

	SPECIAL_KEY_DATA *sk = get_special_key(keys, arg->d.wnum);

	if( !sk )
		return;

	index = get_obj_index(sk->key_wnum.pArea, sk->key_wnum.vnum);

	if( !index || index->item_type != ITEM_KEY )
		return;

	LLIST_UID_DATA *luid = new_list_uid_data();
	if( !luid )
		return;

	obj = create_object(index, 0, true);
	if( IS_VALID(obj) )
	{
		luid->ptr = obj;
		luid->id[0] = obj->id[0];
		luid->id[1] = obj->id[1];
		list_appendlink(sk->list, luid);

		variables_set_object(info->var,rest,obj);

		SETRETURN(1);
		return;
	}

	free_list_uid_data(luid);
}

// [DUNGEON] SPECIALROOMS <command> <parameters>
// Commands:
//  clear
//  add generated|ordinal <level> <special room> <name>
//
// Remarks:
//   Only works in dungeon scripts and runs on DUNGEON_SCHEMATIC triggers.
//
SCRIPT_CMD(dungeoncmd_specialrooms)
{
	DUNGEON_INDEX_DATA *dng;
	char *rest = argument;

	SETRETURN(0);

	if (!IS_VALID(info->dungeon) || !IS_TRIGGER(TRIG_DUNGEON_SCHEMATIC))
		return;

	dng = info->dungeon->index;

	PARSE_ARGTYPE(STRING);
	if (ARG_PREFIX("clear"))
	{
		list_clear(dng->special_rooms);
	}
	else if (ARG_PREFIX("add"))
	{
		int levels = dungeon_index_generation_count(dng);
		bool mode = TRISTATE;

		PARSE_ARGTYPE(STRING);
		if (ARG_PREFIX("generated"))
			mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			mode = TRUE;
		else
			return;
		
		PARSE_ARGTYPE(NUMBER);
		int level = arg->d.num;
		if (level < 1 || level > levels) return;

		PARSE_ARGTYPE(NUMBER);
		int room_no = arg->d.num;
		
		BUFFER *buffer = new_buf();
		bool valid = TRUE;
		if(expand_string(info,rest,buffer) && !IS_NULLSTR(buffer->string) )
		{
			smash_tilde(buffer->string);

			DUNGEON_INDEX_SPECIAL_ROOM *special = new_dungeon_index_special_room();

			free_string(special->name);
			special->name = str_dup(buffer->string);
			special->level = mode ? -level : level;
			special->room = room_no;

			list_appendlink(dng->special_rooms, special);
		}
		else
			valid = FALSE;

		free_buf(buffer);

		if (!valid) return;
	}
	else
		return;

	SETRETURN(1);
}

// [INSTANCE] SPECIALROOMS <command> <parameters>
// Commands:
//  clear
//  add generated|ordinal <section#> <room offset> <name...>
//
// Remarks:
//   Only works in instance scripts and runs on BLUEPRINT_SCHEMATIC triggers.
//
SCRIPT_CMD(instancecmd_specialrooms)
{
	BLUEPRINT *bp;
	char *rest = argument;

	SETRETURN(0);

	if (!IS_VALID(info->instance) || !IS_TRIGGER(TRIG_BLUEPRINT_SCHEMATIC))
		return;
	
	bp = info->instance->blueprint;

	PARSE_ARGTYPE(STRING);
	if (ARG_PREFIX("clear"))
	{
		list_clear(bp->special_rooms);
	}
	else if (ARG_PREFIX("add"))
	{
		PARSE_ARGTYPE(STRING);
		bool mode = TRISTATE;
		if (ARG_PREFIX("generated"))
			mode = FALSE;
		else if (ARG_PREFIX("ordinal"))
			mode = TRUE;
		else
			return;

		PARSE_ARGTYPE(NUMBER);
		int section_no = arg->d.num;
		if (section_no < 1 || section_no > blueprint_generation_count(bp))
			return;

		BLUEPRINT_LAYOUT_SECTION_DATA *group;
		BLUEPRINT_LAYOUT_SECTION_DATA *ls = blueprint_get_nth_section(bp, (mode ? -section_no : section_no), &group);
		int rooms_count = blueprint_layout_room_count(bp, (group ? group : ls));

		PARSE_ARGTYPE(NUMBER);
		int room_offset = arg->d.num;
		if (room_offset < 0 || room_offset >= rooms_count)
			return;

		BUFFER *buffer = new_buf();
		bool valid = TRUE;
		if (expand_string(info, rest, buffer) && !IS_NULLSTR(buffer->string))
		{
			smash_tilde(buffer->string);

			BLUEPRINT_SPECIAL_ROOM *room = new_blueprint_special_room();
			free_string(room->name);
			room->name = str_dup(buffer->string);
			room->section = (mode ? -section_no : section_no);
			room->offset = room_offset;
			list_appendlink(bp->special_rooms, room);
		}
		else
			valid = FALSE;

		free_buf(buffer);
		if (!valid)
			return;
	}
	else
		return;

	SETRETURN(1);
}

// STARTCOMBAT[ $ATTACKER] $VICTIM
SCRIPT_CMD(scriptcmd_startcombat)
{
	char *rest;
	CHAR_DATA *attacker = NULL;
	CHAR_DATA *victim = NULL;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	if(*rest) {
		if(!expand_argument(info,rest,arg))
			return;

		attacker = victim;
		switch(arg->type) {
		case ENT_STRING: victim = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: victim = arg->d.mob; break;
		default: victim = NULL; break;
		}

		if (!victim)
			return;
	} else if(!info->mob)
		return;
	else
		attacker = info->mob;


	// Attacker is fighting already
	if(attacker->fighting)
		return;

	// The victim is fighting someone else in a singleplay room
	if(!IS_NPC(attacker) && victim->fighting != NULL && victim->fighting != attacker && !IS_SET(attacker->in_room->room2_flags, ROOM_MULTIPLAY))
		return;

	// They are not in the same room
	if(attacker->in_room != victim->in_room)
		return;

	// The victim is safe
	if(is_safe(attacker, victim, FALSE)) return;

	// Set them to fighting!
	if(set_fighting(attacker, victim))
		info->progs->lastreturn = 1;
}

// STARTRECKONING[ $DURATION=30[ $SKIP=false]]
SCRIPT_CMD(scriptcmd_startreckoning)
{
	char *rest = argument;
	int duration = 30;
	bool skip = FALSE;

	info->progs->lastreturn = 0;

	if (script_security < 9)
		return;

	if (rest && *rest)
	{
		if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_NUMBER)
			return;

		duration = URANGE(15, arg->d.num, 60);

		if (rest && *rest)
		{
			if (!(rest = expand_argument(info, rest, arg)) || arg->type != ENT_STRING)
				return;

			if (!str_prefix(arg->d.str, "true") || !str_prefix(arg->d.str, "yes"))
				skip = TRUE;
		}
	}

	if (reckoning_timer > 0)
		return;

	reckoning_duration = duration;
	struct tm *reck_time = (struct tm *) localtime(&current_time);
	reck_time->tm_min += reckoning_duration;
	reckoning_timer = (time_t) mktime(reck_time);
	reckoning_cooldown_timer = 0;
	pre_reckoning = skip?5:1;
	
	info->progs->lastreturn = duration;
}

// STOPCOMBAT $MOBILE[ bool(BOTH)]
// Silently stops combat.
// BOTH: causes both sides to stop fighting, defaults to false
SCRIPT_CMD(scriptcmd_stopcombat)
{
	char *rest;

	CHAR_DATA *mob;
	bool fBoth = FALSE;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;

	if(*rest)
	{
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_BOOLEAN )
			fBoth = arg->d.boolean;
		else if( arg->type == ENT_NUMBER )
			fBoth = (arg->d.num != 0);
		else if( arg->type == ENT_STRING )
			fBoth = (!str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"true"));
	}

	stop_fighting(mob, fBoth);

	if( mob->fighting == NULL )
		info->progs->lastreturn = 1;
}


//////////////////////////////////////
// T


// TREASUREMAP $WUID $AREA|'none' $TREASURE $VARIABLENAME
// $WUID         - wilderness map uid
// $AREA         - area to put the object, or supply the string "none" to specify no area
// $TREASURE     - object to hide on the map
// $VARIABLENAME - name of variable to old treasure map object
//
SCRIPT_CMD(scriptcmd_treasuremap)
{
	char *rest;
	WILDS_DATA *wilds;
	AREA_DATA *area;
	OBJ_DATA *treasure;

	if(!info) return;

	info->progs->lastreturn = 0;

	if( !(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER )
		return;

	wilds = get_wilds_from_uid(NULL, arg->d.num);
	if( !wilds ) return;

	if( !(rest = expand_argument(info, rest, arg)) )
		return;

	area = NULL;
	if( arg->type == ENT_AREA )
		area = arg->d.area;
	else if( arg->type == ENT_STRING )
	{
		if( IS_NULLSTR(arg->d.str) || str_prefix(arg->d.str, "none"))
			return;
	}
	else
		return;

	if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_OBJECT )
		return;

	treasure = arg->d.obj;

	if( rest && *rest )
	{
		OBJ_DATA *map = create_treasure_map(wilds, area, treasure);

		if( !map ) return;

		variables_set_object(info->var,rest,map);
		info->progs->lastreturn = 1;
	}

}


//////////////////////////////////////
// U

// UNMUTE $PLAYER
SCRIPT_CMD(scriptcmd_unmute)
{
	if(!info) return;

	info->progs->lastreturn = 0;

	if(!expand_argument(info,argument,arg) || arg->type != ENT_MOBILE || IS_NPC(arg->d.mob) )
		return;

	if( !arg->d.mob->desc )
		return;

	if( arg->d.mob->desc->muted > 0 )
		arg->d.mob->desc->muted--;

	info->progs->lastreturn = 1;
}


// UNLOCKAREA $PLAYER $AREA|$AREANAME|$AUID|$ROOM
SCRIPT_CMD(scriptcmd_unlockarea)
{
	char *rest;
	CHAR_DATA *player;
	AREA_DATA *area;

	if(!info) return;

	info->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_MOBILE || IS_NPC(arg->d.mob) )
		return;

	player = arg->d.mob;

	if(!expand_argument(info,rest,arg))
		return;

	area = NULL;
	if( arg->type == ENT_NUMBER )
	{
		area = get_area_from_uid(arg->d.num);
	}
	else if( arg->type == ENT_STRING )
	{
		for (area = area_first; area != NULL; area = area->next) {
			if (!str_infix(arg->d.str, area->name)) {
				break;
			}
		}
	}
	else if( arg->type == ENT_ROOM )
	{
		area = arg->d.room ? arg->d.room->area : NULL;
	}
	else if( arg->type == ENT_AREA )
	{
		area = arg->d.area;
	}

	if( !area )
		return;

	player_unlock_area(player, area);

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// V

SCRIPT_CMD(scriptcmd_varclear)
{
	if(!info || !info->var) return;

	script_varclearon(info,info->var, argument, arg);
}

SCRIPT_CMD(scriptcmd_varclearon)
{
	VARIABLE **vars;

	if(!info) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? &arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? &arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? &arg->d.token->progs->vars : NULL; break;
	case ENT_AREA: vars = (arg->d.area && arg->d.area->progs) ? &arg->d.area->progs->vars : NULL; break;
	case ENT_INSTANCE: vars = (arg->d.instance && arg->d.instance->progs) ? &arg->d.instance->progs->vars : NULL; break;
	case ENT_DUNGEON: vars = (arg->d.dungeon && arg->d.dungeon->progs) ? &arg->d.dungeon->progs->vars : NULL; break;
	default: vars = NULL; break;
	}

	script_varclearon(info,vars,argument, arg);
}


SCRIPT_CMD(scriptcmd_varcopy)
{
	char oldname[MIL],newname[MIL];

	if(!info || !info->var) return;

	// Get name
	argument = one_argument(argument,oldname);
	if(!oldname[0]) return;
	argument = one_argument(argument,newname);
	if(!newname[0]) return;

	if(!str_cmp(oldname,newname)) return;

	variable_copy(info->var,oldname,newname);
}

SCRIPT_CMD(scriptcmd_varsave)
{
	char name[MIL],arg1[MIL];
	bool on;

	if(!info || !info->var) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,arg1);
	if(!arg1[0]) return;

	on = !str_cmp(arg1,"on") || !str_cmp(arg1,"true") || !str_cmp(arg1,"yes");

	variable_setsave(*info->var,name,on);
}

SCRIPT_CMD(scriptcmd_varsaveon)
{
	char name[MIL],buf[MIL];
	bool on;

	VARIABLE *vars;

	if(!info) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? arg->d.token->progs->vars : NULL; break;
	case ENT_AREA: vars = (arg->d.area && arg->d.area->progs) ? arg->d.area->progs->vars : NULL; break;
	case ENT_INSTANCE: vars = (arg->d.instance && arg->d.instance->progs) ? arg->d.instance->progs->vars : NULL; break;
	case ENT_DUNGEON: vars = (arg->d.dungeon && arg->d.dungeon->progs) ? arg->d.dungeon->progs->vars : NULL; break;
	default: vars = NULL; break;
	}

	if(!vars) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,buf);
	if(!buf[0]) return;

	on = !str_cmp(buf,"on") || !str_cmp(buf,"true") || !str_cmp(buf,"yes");

	variable_setsave(vars,name,on);
}

SCRIPT_CMD(scriptcmd_varset)
{
	if(!info || !info->var) return;

	script_varseton(info,info->var,argument, arg);
}

SCRIPT_CMD(scriptcmd_varseton)
{

	VARIABLE **vars;

	if(!info) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? &arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? &arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? &arg->d.token->progs->vars : NULL; break;
	case ENT_AREA: vars = (arg->d.area && arg->d.area->progs) ? &arg->d.area->progs->vars : NULL; break;
	case ENT_INSTANCE: vars = (arg->d.instance && arg->d.instance->progs) ? &arg->d.instance->progs->vars : NULL; break;
	case ENT_DUNGEON: vars = (arg->d.dungeon && arg->d.dungeon->progs) ? &arg->d.dungeon->progs->vars : NULL; break;
	default: vars = NULL; break;
	}

	script_varseton(info, vars, argument, arg);
}


//////////////////////////////////////
// W

// WILDERNESSMAP $WUID $X $Y $MAP $OFFSET%[ $MARKER]
// $WUID         - wilderness map uid
// $X            - x coordinate
// $Y            - y coordinate
// $MAP          - object to place map text on
// $OFFSET%      - percent chance the X marker is offset +/-1
// $MARKER       - marker to put on the map.  Defaults to {RX{x
//
SCRIPT_CMD(scriptcmd_wildernessmap)
{
	char *rest;
	WILDS_DATA *wilds;
	OBJ_DATA *map;
	int x, y, offset;
	char *marker;

	if(!info) return;

	info->progs->lastreturn = 0;

	if( !(rest = expand_argument(info, argument, arg)) && arg->type != ENT_NUMBER )
		return;

	wilds = get_wilds_from_uid(NULL, arg->d.num);
	if( !wilds ) return;

	if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER )
		return;

	x = arg->d.num;
	if( x < 0 || x >= wilds->map_size_x )
		return;

	if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER )
		return;

	y = arg->d.num;
	if( y < 0 || y >= wilds->map_size_y )
		return;

	if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_OBJECT )
		return;

	map = arg->d.obj;

	if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_NUMBER )
		return;

	offset = arg->d.num;
	if( offset < 0 || offset > 100 )
		return;

	marker = NULL;
	if( rest && *rest )
	{
		if( !(rest = expand_argument(info, rest, arg)) && arg->type != ENT_STRING )
			return;

		marker = arg->d.str;
	}

	if( IS_NULLSTR(marker) )
		marker = "{RX{x";

	char *plain = nocolour(marker);
	int len = strlen(plain);
	free_string(plain);

	// Must be ONE character without color
	if( len != 1 )
		return;

	if( create_wilderness_map(wilds, x, y, map, offset, marker) )
		return;

	info->progs->lastreturn = 1;
}

//////////////////////////////////////
// X

// do_mpxcall
SCRIPT_CMD(scriptcmd_xcall)
{
	char *rest; //buf[MSL], *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;
	AREA_DATA *area = NULL;
	INSTANCE *instance = NULL;
	DUNGEON *dungeon = NULL;
	CHAR_DATA *vch = NULL,*ch = NULL;
	OBJ_DATA *obj1 = NULL,*obj2 = NULL;
	SCRIPT_DATA *script;
	int depth, ret, space = PRG_MPROG;
	WNUM wnum;


	DBG2ENTRY2(PTR,info,PTR,argument);
	if(!info) return;

	info->progs->lastreturn = 0;

	if (!argument[0]) {
		return;
	}

	if(script_security < 5) {
		return;
	}


	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE:	mob = arg->d.mob; space = PRG_MPROG; break;
	case ENT_OBJECT:	obj = arg->d.obj; space = PRG_OPROG; break;
	case ENT_ROOM:		room = arg->d.room; space = PRG_RPROG; break;
	case ENT_TOKEN:		token = arg->d.token; space = PRG_TPROG; break;
	case ENT_AREA:		area = arg->d.area; space = PRG_APROG; break;
	case ENT_INSTANCE:	instance = arg->d.instance; space = PRG_IPROG; break;
	case ENT_DUNGEON:	dungeon = arg->d.dungeon; space = PRG_DPROG; break;
	}

	if(!mob && !obj && !room && !token && !area && !instance && !dungeon) {
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(mob && !IS_NPC(mob)) {
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}


	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM) {
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	wnum = arg->d.wnum;

	if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, space))) {
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = script_get_char_room(info, arg->d.str, FALSE); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: obj1 = script_get_obj_here(info, arg->d.str); break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: obj2 = script_get_obj_here(info, arg->d.str); break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	// Do this to account for possible destructions
	ret = execute_script(script, mob, obj, room, token, area, instance, dungeon, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
	if(info->progs)
		info->progs->lastreturn = ret;
	else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}


//////////////////////////////////////
// Y

//////////////////////////////////////
// Z




// ACTTRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TRIGGER $ACTION...
// $ENACTOR can be MOBILE, OBJECT or ROOM
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_acttrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	BUFFER *action_buffer = NULL;
	char *action = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the ACTION:
	if (!IS_NULLSTR(rest))
	{
		action_buffer = new_buf();
		if (!PARSE_STR(action_buffer))
		{
			free_buf(action_buffer);
			return;
		}
		action = action_buffer->string;
	}

	ret = p_act_trigger(action, actor_m, actor_o, actor_r, ch, vch1, vch2, obj1, obj2, trigger,0,0,0,0,0);

	if (action_buffer) free_buf(action_buffer);

	SETRETURN(ret);
}

// BRIBETRIGGER $ACTOR $ENACTOR $AMOUNT
// $ENACTOR can be MOBILE
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_bribetrigger)
{
	char *rest = argument;
	CHAR_DATA *actor = NULL;
	CHAR_DATA *ch = NULL;
	int amount;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	PARSE_ARGTYPE(MOBILE);
	actor = arg->d.mob;

	// Check that there is valid actor
	if (!IS_VALID(actor))
		return;
	
	// Get the ENACTOR (not optional):
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the AMOUNT (not optional):
	PARSE_ARGTYPE(NUMBER);
	amount = arg->d.num;

	ret = p_bribe_trigger(actor, ch, amount,0,0,0,0,0);

	SETRETURN(ret);
}

// DIRECTIONTRIGGER $MOBILE $ROOM $TRIGGER
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_directiontrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	ROOM_INDEX_DATA *here = NULL;
	int door;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE:
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;
	
	// Get the HERE:
	PARSE_ARGTYPE(ROOM);
	here = arg->d.room;
	
	// Get the DOOR:
	PARSE_ARGTYPE(STRING);
	door = parse_door(arg->d.str);
	if (door < 0 || door >= MAX_DIR) return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, PRG_RPROG);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	ret = p_direction_trigger(ch, here, door, PRG_RPROG, trigger,0,0,0,0,0);

	SETRETURN(ret);
}

// EMOTEATTRIGGER $MOBILE $ENACTOR $EMOTE
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_emoteattrigger)
{
	char *rest = argument;
	CHAR_DATA *mob = NULL;
	CHAR_DATA *ch = NULL;
	BUFFER *emote_buffer = NULL;
	char *emote = NULL;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE
	PARSE_ARGTYPE(MOBILE);
	mob = arg->d.mob;

	// Get the ENACTOR
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;
	
	emote_buffer = new_buf();
	if (!PARSE_STR(emote_buffer))
	{
		free_buf(emote_buffer);
		return;
	}
	emote = emote_buffer->string;

	ret = p_emoteat_trigger(mob, ch, emote,0,0,0,0,0);

	free_buf(emote_buffer);

	SETRETURN(ret);
}

// EMOTETRIGGER $MOBILE $EMOTE
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_emotetrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	BUFFER *emote_buffer = NULL;
	char *emote = NULL;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;
	
	emote_buffer = new_buf();
	if (!PARSE_STR(emote_buffer))
	{
		free_buf(emote_buffer);
		return;
	}
	emote = emote_buffer->string;

	ret = p_emote_trigger(ch, emote,0,0,0,0,0);

	free_buf(emote_buffer);

	SETRETURN(ret);
}

// EXACTTRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TRIGGER $PHRASE...
// $ENACTOR can be MOBILE, OBJECT or ROOM
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_exacttrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	BUFFER *phrase_buffer = NULL;
	char *phrase = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the PHRASE:
	if (!IS_NULLSTR(rest))
	{
		phrase_buffer = new_buf();
		if (!PARSE_STR(phrase_buffer))
		{
			free_buf(phrase_buffer);
			return;
		}
		phrase = phrase_buffer->string;
	}

	ret = p_exact_trigger(phrase, actor_m, actor_o, actor_r, ch, vch1, vch2, obj1, obj2, trigger,0,0,0,0,0);

	if (phrase_buffer) free_buf(phrase_buffer);

	SETRETURN(ret);
}

// EXITTRIGGER $MOBILE $DOOR $TRIGGER
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_exittrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	int dir;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE:
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the DOOR
	PARSE_ARGTYPE(STRING);
	dir = parse_door(arg->d.str);
	if (dir < 0 || dir >= MAX_DIR) return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, PRG_RPROG);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	ret = p_exit_trigger(ch, dir, trigger,0,0,0,0,0);

	SETRETURN(ret);
}


// GIVETRIGGER $ACTOR $ENACTOR $OBJECT $TRIGGER
// $ENACTOR can be MOBILE, OBJECT, ROOM
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_givetrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r))
		return;
	
	// Get the ENACTOR:
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;
	
	// Get the OBJECT:
	PARSE_ARGTYPE(OBJECT);
	obj = arg->d.obj;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	ret = p_give_trigger(actor_m, actor_o, actor_r, ch, obj, trigger,0,0,0,0,0);

	SETRETURN(ret);
}

// GREETTRIGGER $MOBILE $SPACE
// $SPACE can be MOBILE, OBJECT, ROOM
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_greettrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	int space = 0;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the SPACE:
	PARSE_ARGTYPE(STRING);
	if ((space = flag_value(script_spaces,arg->d.str)) == NO_FLAG)
		return;

	ret = p_greet_trigger(ch, space,0,0,0,0,0);

	SETRETURN(ret);
}

// HPRCTTRIGGER $MOBILE $ENACTOR
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_hprcttrigger)
{
	char *rest = argument;
	CHAR_DATA *mob = NULL;
	CHAR_DATA *ch = NULL;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE
	PARSE_ARGTYPE(MOBILE);
	mob = arg->d.mob;

	// Get the ENACTOR
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	ret = p_hprct_trigger(mob, ch,0,0,0,0,0);

	SETRETURN(ret);
}

// NAMETRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TRIGGER $NAME...
// $ENACTOR can be MOBILE, OBJECT or ROOM
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_nametrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	BUFFER *name_buffer = NULL;
	char *name = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the NAME:
	if (!IS_NULLSTR(rest))
	{
		name_buffer = new_buf();
		if (!PARSE_STR(name_buffer))
		{
			free_buf(name_buffer);
			return;
		}

		name = name_buffer->string;
	}

	ret = p_name_trigger(name, actor_m, actor_o, actor_r, ch, vch1, vch2, obj1, obj2, trigger,0,0,0,0,0);

	if (name_buffer) free_buf(name_buffer);

	SETRETURN(ret);
}

// NUMBERTRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $NUMBER $WILDCARD $TRIGGER[ $PHRASE...]
// $ENACTOR can be MOBILE, OBJECT, ROOM, TOKEN
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_numbertrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	TOKEN_DATA *actor_t = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	int number, wildcard;
	BUFFER *phrase_buffer = NULL;
	char *phrase = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	case ENT_TOKEN:		actor_t = arg->d.token;		actor_space = PRG_TPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r && !IS_VALID(actor_t)))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the NUMBER (not optional):
	PARSE_ARGTYPE(NUMBER);
	number = arg->d.num;

	// Get the WILDCARD (not optional);
	PARSE_ARGTYPE(NUMBER);
	wildcard = arg->d.num;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the PHRASE:
	if (!IS_NULLSTR(rest))
	{
		phrase_buffer = new_buf();
		if (!PARSE_STR(phrase_buffer))
		{
			free_buf(phrase_buffer);
			return;
		}

		phrase = phrase_buffer->string;
	}

	ret = p_number_trigger(number, wildcard, actor_m, actor_o, actor_r, actor_t, ch, vch1, vch2, obj1, obj2, trigger, phrase,0,0,0,0,0);

	if (phrase_buffer) free_buf(phrase_buffer);

	SETRETURN(ret);
}

// PERCENTTRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TRIGGER[ $PHRASE...]
// $ENACTOR can be MOBILE, OBJECT, ROOM, TOKEN, AREA, INSTANCE or DUNGEON
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_percenttrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	TOKEN_DATA *actor_t = NULL;
	AREA_DATA *actor_a = NULL;
	INSTANCE *actor_i = NULL;
	DUNGEON *actor_d = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	BUFFER *phrase_buffer = NULL;
	char *phrase = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	case ENT_TOKEN:		actor_t = arg->d.token;		actor_space = PRG_TPROG; break;
	case ENT_AREA:		actor_a = arg->d.area;		actor_space = PRG_APROG; break;
	case ENT_INSTANCE:	actor_i = arg->d.instance;	actor_space = PRG_IPROG; break;
	case ENT_DUNGEON:	actor_d = arg->d.dungeon;	actor_space = PRG_DPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r && !IS_VALID(actor_t) &&
		!actor_a && !IS_VALID(actor_i) && !IS_VALID(actor_d)))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the PHRASE:
	if (!IS_NULLSTR(rest))
	{
		phrase_buffer = new_buf();
		if (!PARSE_STR(phrase_buffer))
		{
			free_buf(phrase_buffer);
			return;
		}

		phrase = phrase_buffer->string;
	}

	if (actor_a || actor_i || actor_d)
		ret = p_percent2_trigger(actor_a, actor_i, actor_d, ch, vch1, vch2, obj1, obj2, trigger, phrase,0,0,0,0,0);
	else
		ret = p_percent_trigger(actor_m, actor_o, actor_r, actor_t, ch, vch1, vch2, obj1, obj2, trigger, phrase,0,0,0,0,0);

	if (phrase_buffer) free_buf(phrase_buffer);

	SETRETURN(ret);
}


// PERCENTTOKENTRIGGER $ACTOR $ENACTOR $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TOKEN $TRIGGER[ $PHRASE...]
// $ENACTOR can be MOBILE, OBJECT, ROOM, TOKEN
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_percenttokentrigger)
{
	char *rest = argument;
	int actor_space = 0;
	CHAR_DATA *actor_m = NULL;
	OBJ_DATA *actor_o = NULL;
	ROOM_INDEX_DATA *actor_r = NULL;
	TOKEN_DATA *actor_t = NULL;
	AREA_DATA *actor_a = NULL;
	INSTANCE *actor_i = NULL;
	DUNGEON *actor_d = NULL;
	CHAR_DATA *ch = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	TOKEN_DATA *token = NULL;
	BUFFER *phrase_buffer = NULL;
	char *phrase = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the ACTOR:
	if (!PARSE_ARG) return;

	switch(arg->type) {
	case ENT_MOBILE:	actor_m = arg->d.mob;		actor_space = PRG_MPROG; break;
	case ENT_OBJECT:	actor_o = arg->d.obj;		actor_space = PRG_OPROG; break;
	case ENT_ROOM:		actor_r = arg->d.room;		actor_space = PRG_RPROG; break;
	case ENT_TOKEN:		actor_t = arg->d.token;		actor_space = PRG_TPROG; break;
	}

	// Check that there is valid actor
	if (!actor_space || (!IS_VALID(actor_m) && !IS_VALID(actor_o) && !actor_r && !IS_VALID(actor_t) &&
		!actor_a && !IS_VALID(actor_i) && !IS_VALID(actor_d)))
		return;
	
	// Get the ENACTOR:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		ch = NULL;
	else if (arg->type == ENT_MOBILE)
		ch = arg->d.mob;
	else
		return;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TOKEN (not optional):
	PARSE_ARGTYPE(TOKEN);
	token = arg->d.token;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, actor_space);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	// Get the PHRASE:
	if (!IS_NULLSTR(rest))
	{
		phrase_buffer = new_buf();
		if (!PARSE_STR(phrase_buffer))
		{
			free_buf(phrase_buffer);
			return;
		}

		phrase = phrase_buffer->string;
	}

	ret = p_percent_token_trigger(actor_m, actor_o, actor_r, actor_t, ch, vch1, vch2, obj1, obj2, token, trigger, phrase,0,0,0,0,0);

	if (phrase_buffer) free_buf(phrase_buffer);

	SETRETURN(ret);
}


// USETRIGGER $MOBILE $OBJECT $TRIGGER
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_usetrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE:
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the OBJECT:
	PARSE_ARGTYPE(OBJECT);
	obj = arg->d.obj;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, PRG_OPROG);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	ret = p_use_trigger(ch, obj, trigger,0,0,0,0,0);

	SETRETURN(ret);
}


// USEONTRIGGER $MOBILE $OBJECT $TRIGGER $TARGET
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_useontrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	BUFFER *target_buffer = NULL;
	char *target = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE:
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the OBJECT:
	PARSE_ARGTYPE(OBJECT);
	obj = arg->d.obj;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, PRG_OPROG);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	if (!IS_NULLSTR(rest))
	{
		target_buffer = new_buf();
		if (!PARSE_STR(target_buffer))
		{
			free_buf(target_buffer);
			return;
		}

		target = target_buffer->string;
	}

	ret = p_use_on_trigger(ch, obj, trigger, target,0,0,0,0,0);

	if (target_buffer) free_buf(target_buffer);

	SETRETURN(ret);
}

// USEWITHTRIGGER $MOBILE $OBJECT $VICTIM1 $VICTIM2 $OBJ1 $OBJ2 $TRIGGER
//
// Returns anything negative if there was anything wrong with it as 0 is a valid return value
//  Check LASTRETURN for this value
SCRIPT_CMD(scriptcmd_usewithtrigger)
{
	char *rest = argument;
	CHAR_DATA *ch = NULL;
	OBJ_DATA *obj = NULL;
	CHAR_DATA *vch1 = NULL;
	CHAR_DATA *vch2 = NULL;
	OBJ_DATA *obj1 = NULL;
	OBJ_DATA *obj2 = NULL;
	int trigger = TRIG_NONE;
	int ret;

	SETRETURN(PRET_BADSYNTAX);	// If it doesn't get to the end, something happened

	// Get the MOBILE
	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	// Get the OBJECT
	PARSE_ARGTYPE(OBJECT);
	obj = arg->d.obj;
	
	// Get the VCH1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch1 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch1 = arg->d.mob;
	else
		return;
	
	// Get the VCH2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		vch2 = NULL;
	else if (arg->type == ENT_MOBILE)
		vch2 = arg->d.mob;
	else
		return;

	// Get the OBJ1:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj1 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj1 = arg->d.obj;
	else
		return;

	// Get the OBJ2:
	if (!PARSE_ARG) return;
	if (arg->type == ENT_STRING && ARG_EQUALS("none"))
		obj2 = NULL;
	else if (arg->type == ENT_OBJECT)
		obj2 = arg->d.obj;
	else
		return;

	// Get the TRIGGER:
	PARSE_ARGTYPE(STRING);
	struct trigger_type *tt = get_trigger_type(arg->d.str, PRG_OPROG);
	if (!tt)
		return;

	if (!tt->scriptable)
	{
		SETRETURN(0);
		return;
	}
	trigger = tt->type;

	ret = p_use_with_trigger(ch, obj, trigger, obj1, obj2, vch1, vch2,0,0,0,0,0);

	SETRETURN(ret);
}




// ADDAURA $MOBILE $NAME $%DESCRIPTION...

// $%DESCRIPTION must use %s in place of the name, as it will get substituted
SCRIPT_CMD(scriptcmd_addaura)
{
	char *rest = argument;
	CHAR_DATA *ch;
	BUFFER *name = NULL;
	BUFFER *desc = NULL;

	SETRETURN(0);

	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	PARSE_ARGTYPE(STRING);
	name = new_buf();
	if (!name) return;
	if (!add_buf(name, arg->d.str))
	{
		free_buf(name);
		return;
	}

	desc = new_buf();
	if (!desc)
	{
		free_buf(name);
		return;
	}
	if (!PARSE_STR(desc))
	{
		free_buf(name);
		free_buf(desc);
		return;
	}

	add_aura_to_char(ch, name->string, desc->string);

	free_buf(name);
	free_buf(desc);

	SETRETURN(1);
}

// REMAURA $MOBILE $NAME
SCRIPT_CMD(scriptcmd_remaura)
{
	char *rest = argument;
	CHAR_DATA *ch;

	SETRETURN(0);

	PARSE_ARGTYPE(MOBILE);
	ch = arg->d.mob;

	PARSE_ARGTYPE(STRING);

	remove_aura_from_char(ch, arg->d.str);

	SETRETURN(1);
}

// SETTITLE $PLAYER $$TITLE...

// $$TITLE must include $$n to indicate where the name will go.
//  The $$n is because the $$ will be reduced to $ after the PARSE_STR macro is complete
SCRIPT_CMD(scriptcmd_settitle)
{
	char *rest = argument;
	CHAR_DATA *ch;
	BUFFER *title;

	SETRETURN(0);

	PARSE_ARGTYPE(MOBILE);
	if(IS_NPC(arg->d.mob)) return;
	ch = arg->d.mob;

	title = new_buf();
	if (!PARSE_STR(title))
	{
		free_buf(title);
		return;
	}

	if (strlen(title->string) > 45)
	{
		title->string[45] = '\0';
	}

	if (str_infix("$n", title->string))
	{
		free_buf(title);
		return;
	}

	set_title(ch, title->string);

	free_buf(title);
	SETRETURN(1);
}


// addspell $OBJECT STRING[ NUMBER]
// addspell $OBJECT TOKEN[ NUMBER]
// addspell $OBJECT WIDEVNUM[ NUMBER]
SCRIPT_CMD(scriptcmd_addspell)
{
	char *rest = argument;
	SPELL_DATA *spell, *spell_new;
	OBJ_DATA *target;
	TOKEN_INDEX_DATA *token;
	int level;
	int sn;
	AFFECT_DATA *paf;

	if(!info || IS_NULLSTR(rest)) return;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;

	target = arg->d.obj;
	level = target->level;

	if (!PARSE_ARG) return;

	if (arg->type == ENT_WIDEVNUM)
	{
		sn = 0;

		token = get_token_index_wnum(arg->d.wnum);

		if (!token || token->type != TOKEN_SPELL) return;
	}
	else if (arg->type == ENT_TOKEN)
	{
		if (!IS_VALID(arg->d.token)) return;

		sn = 0;

		token = arg->d.token->pIndexData;

		if (!token || token->type != TOKEN_SPELL) return;
	}
	else if (arg->type == ENT_STRING)
	{
		if (IS_NULLSTR(arg->d.str)) return;

		token = NULL;

		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;

		// Add security check for the spell function
		if(skill_table[sn].spell_fun == spell_null) return;
	}
	else
		return;


	if( rest && *rest ) {
		PARSE_ARGTYPE(NUMBER);

		// Must be a number, positive and no greater than the object's level
		if(arg->d.num < 1 || arg->d.num > target->level) return;

		level = arg->d.num;
	}

	if (sn > 0)
	{
		// Check if the spell already exists on the object
		for(spell = target->spells; spell != NULL; spell = spell->next)
		{
			if( spell->sn == sn ) {
				spell->level = level;

				// If the object is currently worn and shares affects, update the affect
				if( target->carried_by != NULL && target->wear_loc != WEAR_NONE ) {
					if (target->item_type != ITEM_WAND &&
						target->item_type != ITEM_STAFF &&
						target->item_type != ITEM_SCROLL &&
						target->item_type != ITEM_POTION &&
						target->item_type != ITEM_TATTOO &&
						target->item_type != ITEM_PILL) {


						for( paf = target->carried_by->affected; paf != NULL; paf = paf->next ) {
							if( paf->type > 0 && paf->type == sn && paf->slot == target->wear_loc ) {

								// Update the level if affect's level is higher
								if( paf->level > level )
									paf->level = level;

								// Add security aspect to allow raising the level?

								break;
							}
						}
					}
				}

				SETRETURN(1);
				return;
			}
		}
	}
	else if (token)
	{
		// Check if the spell already exists on the object
		for(spell = target->spells; spell != NULL; spell = spell->next)
		{
			if( spell->token == token ) {
				spell->level = level;

				// If the object is currently worn and shares affects, update the affect
				if( target->carried_by != NULL && target->wear_loc != WEAR_NONE ) {
					if (target->item_type != ITEM_WAND &&
						target->item_type != ITEM_STAFF &&
						target->item_type != ITEM_SCROLL &&
						target->item_type != ITEM_POTION &&
						target->item_type != ITEM_TATTOO &&
						target->item_type != ITEM_PILL) {


						for( paf = target->carried_by->affected; paf != NULL; paf = paf->next ) {
							if( paf->token == token && paf->slot == target->wear_loc ) {

								// Update the level if affect's level is higher
								if( paf->level > level )
									paf->level = level;

								// Add security aspect to allow raising the level?

								break;
							}
						}
					}
				}

				SETRETURN(1);
				return;
			}
		}
	}

	// Spell is new to the object, so add it
	spell_new = new_spell();
	spell_new->sn = sn;
	spell_new->token = token;
	spell_new->level = level;

	spell_new->next = target->spells;
	target->spells = spell_new;


	// If the target is currently being worn and shares affects, add it to the wearer
	if( target->carried_by != NULL && target->wear_loc != WEAR_NONE ) {
		if (target->item_type != ITEM_WAND &&
			target->item_type != ITEM_STAFF &&
			target->item_type != ITEM_SCROLL &&
			target->item_type != ITEM_POTION &&
			target->item_type != ITEM_TATTOO &&
			target->item_type != ITEM_PILL) {
			
			if (sn > 0)
			{
				for (paf = target->carried_by->affected; paf != NULL; paf = paf->next)
				{
					if (paf->type > 0 && paf->type == sn)
						break;
				}

				if (paf == NULL || paf->level < level) {
					affect_strip(target->carried_by, sn);
					obj_cast_spell(sn, level + MAGIC_WEAR_SPELL, target->carried_by, target->carried_by, target);
				}
			}
			else if (token)
			{
				for (paf = target->carried_by->affected; paf != NULL; paf = paf->next)
				{
					if (paf->token && paf->token == token)
						break;
				}

				if (paf == NULL || paf->level < level) {
					affect_strip_token(target->carried_by, token);
					p_token_index_percent_trigger(token, target->carried_by, NULL, NULL, target, NULL, TRIG_APPLY_AFFECT, NULL, level, 0, 0, 0, 0,0,0,0,0,0);
				}
			}
		}
	}

	SETRETURN(1);
}

// remspell $OBJECT STRING[ silent]
// remspell $OBJECT TOKEN[ silent]
// remspell $OBJECT WIDEVNUM[ silent]
SCRIPT_CMD(scriptcmd_remspell)
{

	char *rest = argument;
	SPELL_DATA *spell, *spell_prev;
	OBJ_DATA *target;
	int level;
	int sn;
	TOKEN_INDEX_DATA *token;
	bool found = FALSE, show = TRUE;
	AFFECT_DATA *paf;

	if(!info || IS_NULLSTR(argument)) return;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;

	target = arg->d.obj;

	if (!PARSE_ARG) return;

	if (arg->type == ENT_WIDEVNUM)
	{
		sn = 0;

		token = get_token_index_wnum(arg->d.wnum);

		if (!token || token->type != TOKEN_SPELL) return;
	}
	else if(arg->type == ENT_TOKEN)
	{
		sn = 0;

		token = arg->d.token->pIndexData;

		if (!token || token->type != TOKEN_SPELL) return;
	}
	else if(arg->type == ENT_STRING)
	{
		if (IS_NULLSTR(arg->d.str)) return;

		token = NULL;

		sn = skill_lookup(arg->d.str);
		if( sn <= 0 ) return;

		// Add security check for the spell function
		if(skill_table[sn].spell_fun == spell_null) return;
	}
	else
		return;


	if( rest && *rest ) {
		PARSE_ARGTYPE(STRING);

		if(IS_NULLSTR(arg->d.str)) return;

		if( !str_cmp(arg->d.str, "silent") )
			show = FALSE;
	}


	found = FALSE;

	spell_prev = NULL;
	for(spell = target->spells; spell; spell_prev = spell, spell = spell->next) {
		if( (sn > 0 && spell->sn == sn) || (token && spell->token == token) ) {
			if( spell_prev != NULL )
				spell_prev->next = spell->next;
			else
				target->spells = spell->next;

			level = spell->level;

			free_spell(spell);

			found = TRUE;
			break;
		}
	}

	if( found && target->carried_by != NULL && target->wear_loc != WEAR_NONE) {
		if (target->item_type != ITEM_WAND &&
			target->item_type != ITEM_STAFF &&
			target->item_type != ITEM_SCROLL &&
			target->item_type != ITEM_POTION &&
			target->item_type != ITEM_TATTOO &&
			target->item_type != ITEM_PILL) {

			OBJ_DATA *obj_tmp;
			//int spell_level = level;
			//int found_loc = WEAR_NONE;

			// Find the first affect that matches this spell and is derived from the object
			for (paf = target->carried_by->affected; paf != NULL; paf = paf->next)
			{
				if (((sn > 0 && paf->type == sn) ||
					 (token && paf->token == token)) &&
					paf->slot == target->wear_loc)
					break;
			}

			if( !paf ) {
				// This spell was not applied by this object
				return;
			}

			found = FALSE;
			level = 0;


			// If there's another obj with the same spell put that one on
			for (obj_tmp = target->carried_by->carrying; obj_tmp; obj_tmp = obj_tmp->next_content)
			{
				if( obj_tmp->wear_loc != WEAR_NONE && target != obj_tmp ) {
					for (spell = obj_tmp->spells; spell != NULL; spell = spell ->next) {
						if (((sn > 0 && spell->sn == sn) || (token && spell->token == token)) &&
							spell->level > level ) {
							//level = spell->level;	// Keep the maximum
							//found_loc = obj_tmp->wear_loc;
							found = TRUE;
						}
					}
				}
			}

			if(!found) {
				// No other worn object had this spell available

				if (sn > 0)
				{
					if( show ) {
						if (skill_table[sn].msg_off) {
							send_to_char(skill_table[sn].msg_off, target->carried_by);
							send_to_char("\n\r", target->carried_by);
						}
					}
	
					affect_strip(target->carried_by, sn);
				}
				else if (token)
				{
					p_token_index_percent_trigger(token, target->carried_by, NULL, NULL, target, NULL, TRIG_WEAROFF_AFFECT, (show ? "" : "silent"), 0, 0, 0, 0, 0,0,0,0,0,0);

					affect_strip_token(target->carried_by, token);
				}
			}
		}
	}
}



SCRIPT_CMD(scriptcmd_alterobj)
{
	char buf[2*MIL],field[MIL],*rest;
	int value, num, min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;
	int min = 0, max = 0;
	bool hasmin = FALSE, hasmax = FALSE;
	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info) return;

	SETRETURN(0);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: obj = script_get_obj_here(info, arg->d.str); break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!obj) {
		bug("AlterObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("AlterObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("AlterObj - Error in parsing.",0);
		return;
	}

	field[0] = 0;
	num = -1;

	switch(arg->type) {
	case ENT_STRING:
		if(is_number(arg->d.str)) {
			num = atoi(arg->d.str);
			if(num < 0 || num >= MAX_OBJVALUES) return;
		} else
			strncpy(field,arg->d.str,MIL-1);
		break;
	case ENT_NUMBER:
		num = arg->d.num;
		if(num < 0 || num >= MAX_OBJVALUES) return;
		break;
	default: return;
	}

	if(num < 0 && !field[0]) return;

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("AlterObj - Error in parsing.",0);
		return;
	}

	if(num >= 0) {
		switch(arg->type) {
		case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: value = arg->d.num; break;
		default: return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"AlterObj - Attempting to alter value%d with security %d.\n\r", num, script_security);
			bug(buf, 0);
			return;
		}

		switch (buf[0]) {
		case '+': obj->value[num] += value; break;
		case '-': obj->value[num] -= value; break;
		case '*': obj->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("AlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			obj->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("AlterObj - adjust called with operator % and value 0", 0);
				return;
			}
			obj->value[num] %= value;
			break;

		case '=': obj->value[num] = value; break;
		case '&': obj->value[num] &= value; break;
		case '|': obj->value[num] |= value; break;
		case '!': obj->value[num] &= ~value; break;
		case '^': obj->value[num] ^= value; break;
		default:
			return;
		}
	} else {
		int *ptr = NULL;

		if(!str_cmp(field,"cond"))				ptr = (int*)&obj->condition;
		else if(!str_cmp(field,"container.flags"))	{ ptr = (int *)(IS_CONTAINER(obj)?&CONTAINER(obj)->flags:NULL); flags = container_flags; }
		else if(!str_cmp(field,"container.weight"))	{ ptr = (int *)(IS_CONTAINER(obj)?&CONTAINER(obj)->max_weight:NULL); hasmin = TRUE; min = 0; }
		else if(!str_cmp(field,"container.weight%"))	{ ptr = (int *)(IS_CONTAINER(obj)?&CONTAINER(obj)->weight_multiplier:NULL); hasmin = TRUE; min = 0; hasmax = TRUE; max = 100; }
		else if(!str_cmp(field,"container.volume"))	{ ptr = (int *)(IS_CONTAINER(obj)?&CONTAINER(obj)->max_volume:NULL); hasmin = TRUE; min = 0; }
		else if(!str_cmp(field,"cost"))			{ ptr = (int*)&obj->cost; min_sec = 5; }
		else if(!str_cmp(field,"extra"))		{ ptr = (int*)&obj->extra_flags; flags = extra_flags; }
		else if(!str_cmp(field,"extra2"))		{ ptr = (int*)&obj->extra2_flags; flags = extra2_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra3"))		{ ptr = (int*)&obj->extra3_flags; flags = extra3_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra4"))		{ ptr = (int*)&obj->extra4_flags; flags = extra4_flags; min_sec = 7; }
		else if(!str_cmp(field,"fixes"))		{ ptr = (int*)&obj->times_allowed_fixed; min_sec = 5; }
		else if(!str_cmp(field,"food.hunger"))	{ ptr = (int *)(IS_FOOD(obj)?&FOOD(obj)->hunger:NULL); hasmin = TRUE; min = 0; }
		else if(!str_cmp(field,"food.full"))	{ ptr = (int *)(IS_FOOD(obj)?&FOOD(obj)->full:NULL); hasmin = TRUE; min = 0; }
		else if(!str_cmp(field,"food.poison"))	{ ptr = (int *)(IS_FOOD(obj)?&FOOD(obj)->poison:NULL); hasmin = TRUE; min = 0; hasmax = TRUE; max = 100; }
		else if(!str_cmp(field,"level"))		{ ptr = (int*)&obj->level; min_sec = 5; }
		else if(!str_cmp(field,"light.flags"))	{ ptr = (int *)(IS_LIGHT(obj)?&LIGHT(obj)->flags:NULL); flags = light_flags; }
		else if(!str_cmp(field,"light.duration"))	{ ptr = (int *)(IS_LIGHT(obj)?&LIGHT(obj)->duration:NULL); }
		else if(!str_cmp(field,"repairs"))		ptr = (int*)&obj->times_fixed;
		else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&obj->tempstore[0];
		else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&obj->tempstore[1];
		else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&obj->tempstore[2];
		else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&obj->tempstore[3];
		else if(!str_cmp(field,"tempstore5"))	ptr = (int*)&obj->tempstore[4];
		else if(!str_cmp(field,"timer"))		ptr = (int*)&obj->timer;
		else if(!str_cmp(field,"type"))			{ ptr = (int*)&obj->item_type; flags = type_flags; min_sec = 7; }
		else if(!str_cmp(field,"wear"))			{ ptr = (int*)&obj->wear_flags; flags = wear_flags; }
		else if(!str_cmp(field,"wearloc"))		{ ptr = (int*)&obj->wear_loc; flags = wear_loc_flags; }
		else if(!str_cmp(field,"weight"))		ptr = (int*)&obj->weight;

		if(!ptr) return;

		if(script_security < min_sec) {
			sprintf(buf,"AlterObj - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			bug(buf, 0);
			return;
		}

		if( flags != NULL )
		{
			if( arg->type != ENT_STRING ) return;

			allowarith = FALSE;	// This is a bit vector, no arithmetic operators.
			value = script_flag_value(flags, arg->d.str);

			if( value == NO_FLAG ) value = 0;

			if( flags == extra3_flags )
			{
				REMOVE_BIT(value, ITEM_INSTANCE_OBJ);

				if( buf[0] == '=' || buf[0] == '&' )
				{
					value |= (*ptr & (ITEM_INSTANCE_OBJ));
				}
			}
		}
		else
		{
			switch(arg->type) {
			case ENT_STRING:
				if( is_number(arg->d.str) )
					value = atoi(arg->d.str);
				else
					return;

				break;
			case ENT_NUMBER: value = arg->d.num; break;
			default: return;
			}
		}

		switch (buf[0]) {
		case '+':
			if( !allowarith ) {
				bug("AlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value;
			break;

		case '-':
			if( !allowarith ) {
				bug("AlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value;
			break;

		case '*':
			if( !allowarith ) {
				bug("AlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value;
			break;

		case '/':
			if( !allowarith ) {
				bug("AlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("AlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("AlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("AlterObj - adjust called with operator % and value 0", 0);
				return;
			}
			*ptr %= value;
			break;

		case '=': *ptr = value; break;
		case '&': *ptr &= value; break;
		case '|': *ptr |= value; break;
		case '!': *ptr &= ~value; break;
		case '^': *ptr ^= value; break;
		default:
			return;
		}

		if( ptr )
		{
			if(hasmin && *ptr < min)
				*ptr = min;

			if(hasmax && *ptr > max)
				*ptr = max;
		}
	}

	SETRETURN(1);
}

// ADDTYPE $OBJECT $STRING
// LASTRETURN == 0 for failure
SCRIPT_CMD(scriptcmd_addtype)
{
	char *rest = argument;
	OBJ_DATA *obj;
	int type;

	SETRETURN(0);

	if (script_security < 7) return;

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	obj = arg->d.obj;

	PARSE_ARGTYPE(STRING);
	type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if (type == NO_FLAG) return;

	if (obj->pIndexData->item_type == type) return;

	if (!obj_index_can_add_item_type(obj->pIndexData, type))
		return;

	switch(type)
	{
		case ITEM_CONTAINER:
			if (IS_CONTAINER(obj)) return;

			CONTAINER(obj) = new_container_data();
			break;

		case ITEM_LIGHT:
			if (IS_LIGHT(obj)) return;

			LIGHT(obj) = new_light_data();
			break;

		default:
			return;	// Can't install these types
	}

	SETRETURN(1);
}

// REMTYPE $OBJECT $STRING
SCRIPT_CMD(scriptcmd_remtype)
{
		char *rest = argument;
	OBJ_DATA *obj;
	int type;

	SETRETURN(0);

	if (script_security < 7) return;

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	obj = arg->d.obj;

	PARSE_ARGTYPE(STRING);
	type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if (type == NO_FLAG) return;

	if (obj->pIndexData->item_type == type) return;

	switch(type)
	{
		case ITEM_CONTAINER:
			if (!IS_CONTAINER(obj)) return;

			free_container_data(CONTAINER(obj));
			CONTAINER(obj) = NULL;
			break;

		case ITEM_LIGHT:
			if (!IS_LIGHT(obj)) return;

			free_light_data(LIGHT(obj));
			LIGHT(obj) = NULL;
			break;

		default:
			return;	// Can't install these types
	}

	SETRETURN(1);
}

// ADDWHITELIST $CONTAINER $TYPE[ $SUBTYPE]
SCRIPT_CMD(scriptcmd_addwhitelist)
{
	char *rest = argument;
	OBJ_DATA *container;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	container = arg->d.obj;

	if (!IS_CONTAINER(container)) return;

	PARSE_ARGTYPE(STRING);
	int type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if( type == NO_FLAG) return;

	int subtype = -1;
	if (type == ITEM_WEAPON)
	{
		// Get subtype

		PARSE_ARGTYPE(STRING);
		subtype = stat_lookup(arg->d.str, weapon_class, NO_FLAG);
		if (subtype == NO_FLAG) return;
	}

	ITERATOR it;
	CONTAINER_FILTER *filter;
	iterator_start(&it, CONTAINER(container)->whitelist);
	while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
	{
		if (filter->item_type == type && filter->sub_type == subtype)
		{
			break;
		}
	}
	iterator_stop(&it);

	if(filter) return;

	filter = new_container_filter();
	filter->item_type = type;
	filter->sub_type = subtype;
	list_appendlink(CONTAINER(container)->whitelist, filter);

	SETRETURN(1);
}

// REMWHITELIST $CONTAINER $TYPE[ $SUBTYPE]
SCRIPT_CMD(scriptcmd_remwhitelist)
{
	char *rest = argument;
	OBJ_DATA *container;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	container = arg->d.obj;

	if (!IS_CONTAINER(container)) return;

	PARSE_ARGTYPE(STRING);
	int type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if( type == NO_FLAG) return;

	int subtype = -1;
	if (type == ITEM_WEAPON)
	{
		// Get subtype

		PARSE_ARGTYPE(STRING);
		subtype = stat_lookup(arg->d.str, weapon_class, NO_FLAG);
		if (subtype == NO_FLAG) return;
	}

	ITERATOR it;
	CONTAINER_FILTER *filter;
	iterator_start(&it, CONTAINER(container)->whitelist);
	while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
	{
		if (filter->item_type == type && filter->sub_type == subtype)
		{
			iterator_remcurrent(&it);
			break;
		}
	}
	iterator_stop(&it);

	if (filter) SETRETURN(1);
}

// ADDBLACKLIST $CONTAINER $TYPE[ $SUBTYPE]
SCRIPT_CMD(scriptcmd_addblacklist)
{
	char *rest = argument;
	OBJ_DATA *container;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	container = arg->d.obj;

	if (!IS_CONTAINER(container)) return;

	PARSE_ARGTYPE(STRING);
	int type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if( type == NO_FLAG) return;

	int subtype = -1;
	if (type == ITEM_WEAPON)
	{
		// Get subtype

		PARSE_ARGTYPE(STRING);
		subtype = stat_lookup(arg->d.str, weapon_class, NO_FLAG);
		if (subtype == NO_FLAG) return;
	}

	ITERATOR it;
	CONTAINER_FILTER *filter;
	iterator_start(&it, CONTAINER(container)->blacklist);
	while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
	{
		if (filter->item_type == type && filter->sub_type == subtype)
		{
			break;
		}
	}
	iterator_stop(&it);

	if(filter) return;

	filter = new_container_filter();
	filter->item_type = type;
	filter->sub_type = subtype;
	list_appendlink(CONTAINER(container)->blacklist, filter);

	SETRETURN(1);
}

// REMBLACKLIST $CONTAINER $TYPE[ $SUBTYPE]
SCRIPT_CMD(scriptcmd_remblacklist)
{
	char *rest = argument;
	OBJ_DATA *container;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	container = arg->d.obj;

	if (!IS_CONTAINER(container)) return;

	PARSE_ARGTYPE(STRING);
	int type = stat_lookup(arg->d.str, type_flags, NO_FLAG);
	if( type == NO_FLAG) return;

	int subtype = -1;
	if (type == ITEM_WEAPON)
	{
		// Get subtype

		PARSE_ARGTYPE(STRING);
		subtype = stat_lookup(arg->d.str, weapon_class, NO_FLAG);
		if (subtype == NO_FLAG) return;
	}

	ITERATOR it;
	CONTAINER_FILTER *filter;
	iterator_start(&it, CONTAINER(container)->blacklist);
	while((filter = (CONTAINER_FILTER *)iterator_nextdata(&it)))
	{
		if (filter->item_type == type && filter->sub_type == subtype)
		{
			iterator_remcurrent(&it);
			break;
		}
	}
	iterator_stop(&it);

	if (filter) SETRETURN(1);
}


// ADDFOODBUFF $FOOD apply-type(string) level(number|auto) location(string) modifier(number) duration(number|auto) bitvector(string) bitvector2(string)
SCRIPT_CMD(scriptcmd_addfoodbuff)
{
	char *rest = argument;
	OBJ_DATA *food;

	SETRETURN(0);

	PARSE_ARGTYPE(OBJECT);
	if (!IS_VALID(arg->d.obj)) return;
	food = arg->d.obj;

	if (!IS_FOOD(food)) return;

	PARSE_ARGTYPE(STRING);
	int type = stat_lookup(arg->d.str, food_buff_types, NO_FLAG);
	if (type == NO_FLAG) return;

	int level;
	if (!PARSE_ARG) return;
	if (arg->type == ENT_NUMBER)
	{
		level = UMAX(1, arg->d.num);
	}
	else if (!str_prefix(arg->d.str, "auto"))
	{
		level = 0;
	}
	else
		return;

	PARSE_ARGTYPE(STRING);
	int location = stat_lookup(arg->d.str, apply_flags, NO_FLAG);
	if (location == NO_FLAG) return;

	PARSE_ARGTYPE(NUMBER);
	int modifier = arg->d.num;

	int duration;
	if (!PARSE_ARG) return;
	if (arg->type == ENT_NUMBER)
	{
		duration = UMAX(1, arg->d.num);
	}
	else if (!str_prefix(arg->d.str, "auto"))
	{
		duration = 0;
	}
	else
		return;

	long bitvectors[2];

	PARSE_ARGTYPE(STRING);

	if (!bitvector_lookup(arg->d.str, 2, bitvectors, affect_flags, affect2_flags)) return;

	FOOD_BUFF_DATA *buff = new_food_buff_data();
	buff->where = type;
	buff->level = level;
	buff->location = location;
	buff->modifier = modifier;
	buff->duration = duration;
	buff->bitvector = bitvectors[0];
	buff->bitvector2 = bitvectors[1];

	list_appendlink(FOOD(food)->buffs, buff);

	SETRETURN(1);
}
