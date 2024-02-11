/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "scripts.h"
#include "recycle.h"
#include "tables.h"
#include "wilds.h"

#define DEBUG_MODULE
#include "debug.h"


const struct script_cmd_type room_cmd_table[] = {
	{ "acttrigger",				scriptcmd_acttrigger,	true,	false	},
	{ "addaffect",			scriptcmd_addaffect,	true,	true	},
	{ "addaffectname",		scriptcmd_addaffectname,true,	true	},
	{ "addaura",			scriptcmd_addaura,			true,	true	},
	{ "addspell",			scriptcmd_addspell,				true,	true	},
	{ "addstache",			scriptcmd_addstache,		true,	true },
	{ "addtype",			scriptcmd_addtype,			true, true },
	{ "alteraffect",		do_rpalteraffect,		true,	true	},
	{ "alterexit",			do_rpalterexit,			false,	true	},
	{ "altermob",			do_rpaltermob,			true,	true	},
	{ "alterobj",			scriptcmd_alterobjmt,			true,	true	},
	{ "alterroom",			do_rpalterroom,			true,	true	},
	{ "applytoxin",			scriptcmd_applytoxin,	false,	true	},
	{ "asound",				do_rpasound,			false,	true	},
	{ "at",					do_rpat,				false,	true	},
	{ "attach",				scriptcmd_attach,			true,	true	},
	{ "award",				scriptcmd_award,		true,	true	},
	{ "breathe",			scriptcmd_breathe,		false,	true	},
	{ "bribetrigger",			scriptcmd_bribetrigger,	true,	false	},
	{ "call",				do_rpcall,				false,	true	},
	{ "cancel",				do_rpcancel,			false,	false	},
	{ "chargebank",			do_rpchargebank,		false,	true	},
	{ "checkpoint",			do_rpcheckpoint,		false,	true	},
	{ "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
	{ "cloneroom",			do_rpcloneroom,			true,	true	},
	{ "condition",			do_rpcondition,			false,	true	},
	{ "crier",				do_rpcrier,				false,	true	},
	{ "damage",				scriptcmd_damage,		false,	true	},
	{ "deduct",				scriptcmd_deduct,		true,	true	},
	{ "delay",				do_rpdelay,				false,	true	},
	{ "dequeue",			do_rpdequeue,			false,	false	},
	{ "destroyroom",		do_rpdestroyroom,		true,	true	},
	{ "detach",				scriptcmd_detach,			true,	true	},
	{ "directiontrigger",		scriptcmd_directiontrigger,	true,	false	},
	{ "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
	{ "dungeonfailure",		scriptcmd_dungeonfailure,	true,	true	},
	{ "echo",				do_rpecho,				false,	true	},
	{ "echoaround",			do_rpechoaround,		false,	true	},
	{ "echoat",				scriptcmd_echoat,			false,	true	},
	{ "echobattlespam",		do_rpechobattlespam,	false,	true	},
	{ "echochurch",			do_rpechochurch,		false,	true	},
	{ "echogrouparound",	do_rpechogrouparound,	false,	true	},
	{ "echogroupat",		do_rpechogroupat,		false,	true	},
	{ "echoleadaround",		do_rpecholeadaround,	false,	true	},
	{ "echoleadat",			do_rpecholeadat,		false,	true	},
	{ "echonotvict",		do_rpechonotvict,		false,	true	},
	{ "echoroom",			do_rpechoroom,			false,	true	},
	{ "ed",					scriptcmd_ed,				false,	true	},
	{ "emoteattrigger",			scriptcmd_emoteattrigger,	true,	false	},
	{ "emotetrigger",			scriptcmd_emotetrigger,	true,	false	},
	{ "entercombat",		scriptcmd_entercombat,	false,	true	},
	{ "exacttrigger",			scriptcmd_exacttrigger,	true,	false	},
	{ "exittrigger",			scriptcmd_exittrigger,	true,	false	},
	{ "fade",				scriptcmd_fade,				true,	true	},
	{ "fixaffects",			do_rpfixaffects,		false,	true	},
	{ "flee",				scriptcmd_flee,			false,	true	},
	{ "force",				do_rpforce,				false,	true	},
	{ "forget",				do_rpforget,			false,	false	},
	{ "gecho",				do_rpgecho,				false,	true	},
	{ "gforce",				do_rpgforce,			false,	true	},
	{ "givetrigger",			scriptcmd_givetrigger,	true,	false	},
	{ "grantskill",			scriptcmd_grantskill,	false,	true	},
	{ "greettrigger",			scriptcmd_greettrigger,	true,	false	},
	{ "group",				do_rpgroup,				false,	true	},
	{ "gtransfer",			do_rpgtransfer,			false,	true	},
	{ "hprcttrigger",			scriptcmd_hprcttrigger,	true,	false	},
	{ "input",				do_rpinput,				false,	true	},
	{ "inputstring",		scriptcmd_inputstring,	false,	true	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
	{ "interrupt",			do_rpinterrupt,			false,	true	},
	{ "link",				do_rplink,				false,	true	},
	{ "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
	{ "lockadd",			scriptcmd_lockadd,			false,	true	},
	{ "lockremove",			scriptcmd_lockremove,		false,	true	},
	{ "lockset",			scriptcmd_lockset,			false,	true	},
	{ "missionaccept",		scriptcmd_missionaccept,		false,	true	},
	{ "missioncancel",		scriptcmd_missioncancel,		false,	true	},
	{ "missioncomplete",		scriptcmd_missioncomplete,	false,	true	},
	{ "missiongenerate",		scriptcmd_missiongenerate,	false,	true	},
	{ "missionpartcustom",	scriptcmd_missionpartcustom,	true,	true	},
	{ "missionpartgetitem",	scriptcmd_missionpartgetitem,	true,	true	},
	{ "missionpartgoto",		scriptcmd_missionpartgoto,	true,	true	},
	{ "missionpartrescue",	scriptcmd_missionpartrescue,	true,	true	},
	{ "missionpartslay",		scriptcmd_missionpartslay,	true,	true	},
	{ "missionscroll",		scriptcmd_missionscroll,		false,	true	},
	{ "mload",				do_rpmload,				false,	true	},
	{ "mute",				scriptcmd_mute,				false,	true	},
	{ "nametrigger",			scriptcmd_nametrigger,	true,	false	},
	{ "numbertrigger",			scriptcmd_numbertrigger,	true,	false	},
	{ "oload",				scriptcmd_oload,				false,	true	},
	{ "otransfer",			do_rpotransfer,			false,	true	},
	{ "pageat",				scriptcmd_pageat,			false,	true	},
	{ "peace",				do_rppeace,				false,	false	},
	{ "percenttokentrigger",	scriptcmd_percenttokentrigger,	true,	false	},
	{ "percenttrigger",			scriptcmd_percenttrigger,	true,	false	},
	{ "persist",			do_rppersist,			false,	true	},
	{ "prompt",				do_rpprompt,			false,	true	},
	{ "purge",				do_rppurge,				false,	false	},
	{ "queue",				do_rpqueue,				false,	true	},
	{ "rawkill",			do_rprawkill,			false,	true	},
	{ "reassign",			scriptcmd_reassign,			true,	false	},
	{ "reckoning",			scriptcmd_reckoning,		true,	true	},
	{ "remaura",			scriptcmd_remaura,			true,	true	},
	{ "remember",			do_rpremember,			false,	true	},
	{ "remort",				do_rpremort,			true,	true	},
	{ "remove",				do_rpremove,			false,	true	},
	{ "remspell",			scriptcmd_remspell,			true,	true	},
	{ "remstache",			scriptcmd_remstache,		true,	true },
	{ "remtype",			scriptcmd_remtype,			true,	true	},
	{ "resetdice",			do_rpresetdice,			true,	true	},
	{ "restore",			do_rprestore,			true,	true	},
	{ "revokeskill",		scriptcmd_revokeskill,	false,	true	},
	{ "saveplayer",			do_rpsaveplayer,		false,	true	},
	{ "sendfloor",			scriptcmd_sendfloor,		false,	true	},
	{ "setclasslevel",		scriptcmd_setclasslevel,	true,	true	},
	{ "setposition",		scriptcmd_setposition,		true,	true	},
	{ "setrace",			scriptcmd_setrace,			true,	true	},
	{ "setrecall",			do_rpsetrecall,			false,	true	},
	{ "settimer",			do_rpsettimer,			false,	true	},
	{ "settitle",			scriptcmd_settitle,			true,	true	},
	{ "showcommand",		scriptcmd_showcommand,		false,	true	},
	{ "showroom",			do_rpshowroom,			false,	true	},
	{ "skimprove",			do_rpskimprove,			true,	true	},
	{ "spawndungeon",		scriptcmd_spawndungeon,		true,	true	},
	{ "specialkey",			scriptcmd_specialkey,		false,	true	},
	{ "startcombat",		scriptcmd_startcombat,	false,	true	},
	{ "startreckoning",		scriptcmd_startreckoning,	true,	true	},
	{ "stopcombat",			scriptcmd_stopcombat,	false,	true	},
	{ "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
	{ "stringmob",			do_rpstringmob,			true,	true	},
	{ "stringobj",			scriptcmd_stringobjmt,			true,	true	},
	{ "stripaffect",		do_rpstripaffect,		true,	true	},
	{ "stripaffectname",	do_rpstripaffectname,	true,	true	},
	{ "transfer",			do_rptransfer,			false,	true	},
	{ "treasuremap",		scriptcmd_treasuremap,		false,	true	},
	{ "ungroup",			do_rpungroup,			false,	true	},
	{ "unlockarea",			scriptcmd_unlockarea,		true,	true	},
	{ "unmute",				scriptcmd_unmute,			false,	true	},
	{ "usecatalyst",		do_rpusecatalyst,		false,	true	},
	{ "useontrigger",			scriptcmd_useontrigger,	true,	false	},
	{ "usetrigger",				scriptcmd_usetrigger,	true,	false	},
	{ "usewithtrigger",			scriptcmd_usewithtrigger,	true,	false	},
	{ "varclear",			do_rpvarclear,			false,	true	},
	{ "varclearon",			do_rpvarclearon,		false,	true	},
	{ "varcopy",			do_rpvarcopy,			false,	true	},
	{ "varsave",			do_rpvarsave,			false,	true	},
	{ "varsaveon",			do_rpvarsaveon,			false,	true	},
	{ "varset",				do_rpvarset,			false,	true	},
	{ "varseton",			do_rpvarseton,			false,	true	},
	{ "vforce",				do_rpvforce,			false,	true	},
	{ "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
	{ "wiretransfer",		do_rpwiretransfer,		false,	true	},
	{ "xcall",				do_rpxcall,				false,	true	},
	{ "zecho",				do_rpzecho,				false,	true	},
	{ "zot",				do_rpzot,				true,	true	},
	{ NULL,					NULL,					false,	false	}
};

int rpcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; room_cmd_table[cmd].name; cmd++)
		if (command[0] == room_cmd_table[cmd].name[0] &&
			!str_prefix(command, room_cmd_table[cmd].name))
			return cmd;

	return -1;
}

/*
 * Displays the source code of a given ROOMprogram
 *
 * Syntax: rpdump [vnum]
 */
void do_rpdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *rprg;
	WNUM wnum;

	one_argument(argument, buf);
	if (!parse_widevnum(buf, ch->in_room->area, &wnum))
	{
		send_to_char("Syntax:  rpdump <widevnum>\n\r", ch);
		return;
	}

	if (!(rprg = get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG))) {
		send_to_char("No such ROOMprogram.\n\r", ch);
		return;
	}
	if (!area_has_read_access(ch,rprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}
	page_to_char(rprg->edit_src, ch);
}


/*
 * Displays ROOMprogram triggers of a object
 *
 * Syntax: rpstat [vnum]
 */
void do_rpstat(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	ITERATOR it;
	PROG_LIST *rprg;
	ROOM_INDEX_DATA *room;
	int i, slot;
	WNUM wnum;

	one_argument(argument, arg);

	if (!arg[0] || !parse_widevnum(arg, ch->in_room->area, &wnum)) {
		send_to_char("Rpstat where?\n\r", ch);
		return;
	}

	if (!(room = get_room_index(wnum.pArea, wnum.vnum))) {
		send_to_char("No such room.\n\r", ch);
		return;
	}

	sprintf(arg, "Room %ld#%-6ld [%s]\n\r", room->area->uid, room->vnum, room->name);
	send_to_char(arg, ch);

	if( !IS_NULLSTR(room->comments) )
	{
		sprintf(arg, "Comments:\n\r%s\n\r", room->comments);
		send_to_char(arg, ch);
	}

	sprintf(arg, "Delay   %-6d [%s]\n\r",
		room->progs->delay,
		room->progs->target ? room->progs->target->name : "No target");

	send_to_char(arg, ch);

	if (!room->progs->progs)
		send_to_char("[No programs set]\n\r", ch);
	else
	for(i = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
		iterator_start(&it, room->progs->progs[slot]);
		while(( rprg = (PROG_LIST *)iterator_nextdata(&it))) {
			sprintf(arg, "[%2d] Trigger [%-8s] Program [%4ld#%-4ld] Phrase [%s]\n\r",
				++i, trigger_name(rprg->trig_type),
				rprg->wnum.pArea->uid,
				rprg->wnum.vnum,
				trigger_phrase(rprg->trig_type,rprg->trig_phrase));
			send_to_char(arg, ch);
		}
		iterator_stop(&it);
	}

	if(room->progs->vars)
		pstat_variable_list(ch, room->progs->vars);
}


void room_interpret(SCRIPT_VARINFO *info, char *argument)
{
	char buf[MAX_STRING_LENGTH], command[MAX_INPUT_LENGTH];
	int cmd;

	if(!info->room) return;

	if (!str_prefix("room ", argument))
		argument = skip_whitespace(argument+4);

	argument = one_argument(argument, command);

	cmd = rpcmd_lookup(command);

	if(cmd < 0) {
		sprintf(buf, "Room_interpret: invalid cmd from room %ld: '%s'", info->room->vnum, command);
		bug(buf, 0);
		return;
	}

	SCRIPT_PARAM *arg = new_script_param();
	(*room_cmd_table[cmd].func) (info, argument, arg);
	free_script_param(arg);
	tail_chain();
}

char *rp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
{
	char *rest, *rest2;
	long vnum;
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AREA_DATA *area;
	ROOM_INDEX_DATA *loc;
	WILDS_DATA *pWilds;
	SCRIPT_PARAM *arg = new_script_param();
	int x, y;

	*room = NULL;
	if((rest = expand_argument(info,argument,arg))) {
		switch(arg->type) {
		// Nothing was on the string, so it will assume the current room
		case ENT_NONE: *room = info->room; break;
		case ENT_NUMBER:
			x = arg->d.num;
			rest2 = rest;
			if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
				y = arg->d.num;
				if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

					if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

					// if safe is used, it will not go to bad rooms
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
						!str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
						break;

					rest = rest2;
					room_used_for_wilderness.wilds = pWilds;
					room_used_for_wilderness.x = x;
					room_used_for_wilderness.y = y;
					*room = &room_used_for_wilderness;
				}
			}
			else
				rest = rest2;
			break;

		case ENT_WIDEVNUM:
			*room = get_room_index_wnum(arg->d.wnum);
			break;

		case ENT_STRING: // Special named locations
			if(arg->d.str[0] == '@')
				// Points to an exit, like @north or @down
				*room = get_exit_dest(info->room, arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				// Rather self-explanatory
				*room = info->room;
			else if(!str_cmp(arg->d.str,"vroom") || !str_cmp(arg->d.str,"clone")) {
				// Locates a clone room: vroom <vnum> <id1> <id2>
				WNUM wnum;
				int id1, id2;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_WIDEVNUM) {
					rest = rest2;
					wnum = arg->d.wnum;
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						rest = rest2;

						id1 = arg->d.num;
						if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
							rest = rest2;

							id2 = arg->d.num;
							*room = get_clone_room(get_room_index(wnum.pArea, wnum.vnum),id1,id2);
						}
					}
				}
			} else if(!str_cmp(arg->d.str,"wilds")) {
				if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					x = arg->d.num;
					if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						y = arg->d.num;
						if((rest = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
							if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

							if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

							// if safe is used, it will not go to bad rooms
							if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
								!str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
								break;

							room_used_for_wilderness.wilds = pWilds;
							room_used_for_wilderness.x = x;
							room_used_for_wilderness.y = y;
							*room = &room_used_for_wilderness;
						}
					}
				}
			} else {
				loc = NULL;
				for (area = area_first; area; area = area->next) {
					if (!str_infix(arg->d.str, area->name)) {
						if(!(loc = location_to_room(&area->region.recall))) {
							for (vnum = 1; vnum <= area->top_vnum_room; vnum++)
								if ((loc = get_room_index(area, vnum)))
									break;
						}

						break;
					}
				}

				if(!loc) {
					if((victim = get_char_world(NULL, arg->d.str)))
						loc = victim->in_room;
					else if ((obj = get_obj_world(NULL, arg->d.str)))
						loc = obj_room(obj);
				}
				*room = loc;
			}
			break;
		case ENT_MOBILE:
			*room = arg->d.mob ? arg->d.mob->in_room : NULL; break;
		case ENT_OBJECT:
			*room = arg->d.obj ? obj_room(arg->d.obj) : NULL; break;
		case ENT_ROOM:
			*room = arg->d.room; break;
		case ENT_EXIT:
			*room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}
	free_script_param(arg);
	return rest;
}


char *rp_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
{
	char *rest, *rest2;
	long vnum;
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AREA_DATA *area;
	ROOM_INDEX_DATA *loc;
	WILDS_DATA *pWilds;
	SCRIPT_PARAM *arg = new_script_param();
	int x, y;

	*room = NULL;
	*container = NULL;
	*carrier = NULL;
	*wear_loc = WEAR_NONE;
	if((rest = expand_argument(info,argument,arg))) {
		switch(arg->type) {
		case ENT_NONE:	*room = info->room; break;
		case ENT_NUMBER:
			// Can either be a room index or a wilderness room
			// Room: <vnum>
			// Wilderness coordinates: <x> <y> <w>

			x = arg->d.num;
			if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
				rest = rest2;
				y = arg->d.num;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					rest = rest2;
					if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

					if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

					// if safe is used, it will not go to bad rooms
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
						!str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
						break;

					rest = rest2;
					room_used_for_wilderness.wilds = pWilds;
					room_used_for_wilderness.x = x;
					room_used_for_wilderness.y = y;
					*room = &room_used_for_wilderness;
				}
			}
			break;

		case ENT_WIDEVNUM:
			*room = get_room_index_wnum(arg->d.wnum);
			break;

		case ENT_STRING:
			if(arg->d.str[0] == '@')
				*room = get_exit_dest(info->room, arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				*room = info->room;
			else if(!str_cmp(arg->d.str,"vroom")) {
				int id1, id2;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					rest = rest2;
					WNUM wnum = arg->d.wnum;
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						rest = rest2;

						id1 = arg->d.num;
						if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
							rest = rest2;

							id2 = arg->d.num;
							*room = get_clone_room(get_room_index(wnum.pArea, wnum.vnum),id1,id2);
						}
					}
				}
			} else if(!str_cmp(arg->d.str,"wilds")) {

				x = arg->d.num;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					rest = rest2;
					y = arg->d.num;
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						rest = rest2;
						if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

						if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

						// if safe is used, it will not go to bad rooms
						if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
							!str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
							break;

						rest = rest2;
						room_used_for_wilderness.wilds = pWilds;
						room_used_for_wilderness.x = x;
						room_used_for_wilderness.y = y;
						*room = &room_used_for_wilderness;
					}
				}
			} else {
				loc = NULL;
				for (area = area_first; area; area = area->next) {
					if (!str_infix(arg->d.str, area->name)) {
						if(!(loc = location_to_room(&area->region.recall))) {
							for (vnum = 1; vnum <= area->top_vnum_room; vnum++)
								if ((loc = get_room_index(area, vnum)))
									break;
						}

						break;
					}
				}

				if(!loc) {
					if((victim = get_char_world(NULL, arg->d.str)))
						loc = victim->in_room;
					else if ((obj = get_obj_world(NULL, arg->d.str)))
						loc = obj_room(obj);
				}
				*room = loc;
			}
			break;
		case ENT_MOBILE:
			*carrier = arg->d.mob;
			if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING) {
				*wear_loc = flag_value(wear_loc_flags, arg->d.str);
				if(*wear_loc == NO_FLAG) *wear_loc = WEAR_NONE;
			} else {
				*room = *carrier ? (*carrier)->in_room : NULL;
				*carrier = NULL;
			}
			break;
		case ENT_OBJECT:
			*container = arg->d.obj;
			if(!(rest2 = expand_argument(info,rest,arg)) || arg->type != ENT_STRING || str_cmp(arg->d.str,"inside")) {
				*room = *container ? obj_room(*container) : NULL;
				*container = NULL;
			}
			break;
		case ENT_ROOM:
			*room = arg->d.room; break;
		case ENT_EXIT:
			*room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] ) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}

	free_script_param(arg);
	return rest;
}


SCRIPT_CMD(do_rpasound)
{
	ROOM_INDEX_DATA *rooms[MAX_DIR], *room;
	int door, i, j;
	EXIT_DATA *pexit;

	if(!info || !info->room) return;
	if (!argument[0]) return;

	// Verify there are any exits!
	for (door = 0; door < MAX_DIR; door++)
		if ((pexit = info->room->exit[door]) && (room = exit_destination(pexit)) && room != info->room)
			break;

	if (door < MAX_DIR) {
		// Expand the message
		BUFFER *buffer = new_buf();
		expand_string(info,argument,buffer);

		if( buffer->string[0] != '\0' )
		{

			for (i = 0; door < MAX_DIR; door++)
				if ((pexit = info->room->exit[door]) && (room = exit_destination(pexit)) && room != info->room) {
					// Have we been to this room already?
					for(j=0;j < i && rooms[j] != room; j++);

					if(i <= j) {
						// No, so do the message
						MOBtrigger  = false;
						act(buffer->string, room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
						MOBtrigger  = true;
						rooms[i++] = room;
					}
				}
		}
		free_buf(buffer);

	}
}

SCRIPT_CMD(do_rpat)
{
	SCRIPT_VARINFO info2;
	CHAR_DATA *target;
	int delay,sec;
	pVARIABLE vars;
	ROOM_INDEX_DATA *dest;
	bool remote;

	if(!(argument = rp_getlocation(info, argument, &dest))) {
		bug("Rpat - Bad argument from vnum %d.", info->room->vnum);
		return;
	}

	if (!dest) {
		bug("Rpat - Null location from vnum %d.", info->room->vnum);
		return;
	}

	sec = script_security;
	script_security = NO_SCRIPT_SECURITY;

	remote = PROG_FLAG(dest,PROG_AT) ? true : false;

	PROG_SET(dest,PROG_AT);

	info2 = *info;
	target = dest->progs->target;
	vars = dest->progs->vars;
	delay = dest->progs->delay;
	dest->progs->target = info->room->progs->target;
	dest->progs->vars = info->room->progs->vars;
	dest->progs->delay = info->room->progs->delay;
	info->room->progs->target = NULL;
	info->room->progs->vars = NULL;
	info->room->progs->delay = -1;
	info2.room = dest;
	info2.targ = &(dest->progs->target);
	info2.var = &(dest->progs->vars);

	script_interpret(&info2,argument);
	script_security = sec;

	info->room->progs->target = dest->progs->target;
	info->room->progs->vars = dest->progs->vars;
	info->room->progs->delay = dest->progs->delay;
	dest->progs->target = target;
	dest->progs->vars = vars;
	dest->progs->delay = delay;

	if(!remote)
		PROG_CLR(dest,PROG_AT);
}

SCRIPT_CMD(do_rpcall)
{
	char *rest;
	CHAR_DATA *vch,*ch;
	OBJ_DATA *obj1,*obj2;
	SCRIPT_DATA *script;
	int depth, ret;
	WNUM wnum;

	if(!info || !info->room) return;

	if (!argument[0]) {
		bug("RpCall: missing arguments from vnum %d.", (int)info->room->vnum);
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("RpCall: maximum call depth exceeded for vnum %d.", (int)info->room->vnum);
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_WIDEVNUM) {
		bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	wnum = arg->d.wnum;

	if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG))) {
		bug("RpCall: invalid prog from vnum %d.", info->room->vnum);
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, info->room, arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, info->room,arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, info->room, arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, info->room, arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	ret = execute_script(script, NULL, NULL, info->room, NULL, NULL, NULL, NULL, ch, obj1, obj2, vch,NULL, NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
	if(info->room) {
		info->room->progs->lastreturn = ret;
		DBG3MSG1("lastreturn = %d\n", info->room->progs->lastreturn);
	} else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

SCRIPT_CMD(do_rpcancel)
{
	if(!info || !info->room) return;

	info->room->progs->delay = -1;
}

SCRIPT_CMD(do_rpdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *victim_next;
	int low, high, level, value, dc;
	bool fAll = false, fKill = false, fLevel = false, fRemort = false, fTwo = false;


	if(!info || !info->room || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpDamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = true;
		else victim = get_char_room(NULL, info->room, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll) {
		bug("RpDamage - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	if(!*rest) {
		bug("RpDamage - missing argument from vnum %ld.", info->room->vnum);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpDamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = true; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = true; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = true; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = true; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		bug("RpDamage - invalid argument from vnum %ld.", info->room->vnum);
		return;
	}

	if(!*rest) {
		bug("RpDamage - missing argument from vnum %ld.", info->room->vnum);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpDamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	if(fLevel && !victim) {
		bug("RpDamage - Level aspect used with null victim from vnum %ld.", info->room->vnum);
		return;
	}

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
		} else {
			bug("RpDamage - invalid argument from vnum %ld.", info->room->vnum);
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("RpDamage - Null reference mob from vnum %ld.", info->room->vnum);
				return;
			}
			break;
		} else {
			bug("RpDamage - invalid argument from vnum %ld.", info->room->vnum);
			return;
		}
		break;
	default:
		bug("RpDamage - invalid argument from vnum %ld.", info->room->vnum);
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = info->room->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(victim, victim, fKill ? value : UMIN(victim->hit,value), NULL, TYPE_UNDEFINED, dc, false);
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(victim, victim, fKill ? value : UMIN(victim->hit,value), NULL, TYPE_UNDEFINED, dc, false);
	}
}

SCRIPT_CMD(do_rpdelay)
{
	int delay = 0;


	if(!info || !info->room) return;

	if(!expand_argument(info,argument,arg)) {
		bug("RpDelay - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
	case ENT_NUMBER: delay = arg->d.num; break;
	default: delay = 0; break;
	}

	if (delay < 1) {
		bug("RpDelay: invalid delay from vnum %d.", info->room->vnum);
		return;
	}
	info->room->progs->delay = delay;
}

SCRIPT_CMD(do_rpdequeue)
{
	if(!info || !info->room || !info->room->events)
		return;

	wipe_owned_events(info->room->events);
}

// do_rpecho
SCRIPT_CMD(do_rpecho)
{
	if(!info || !info->room) return;

	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		add_buf(buffer,"\n\r");
		room_echo(info->room, buffer->string);
	}
	free_buf(buffer);
}

// do_rpechoroom
// Syntax: room echoroom <location> <string>
SCRIPT_CMD(do_rpechoroom)
{
	char *rest;
	ROOM_INDEX_DATA *room;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: room = arg->d.mob->in_room; break;
	case ENT_OBJECT: room = obj_room(arg->d.obj); break;
	case ENT_ROOM: room = arg->d.room; break;
	case ENT_EXIT: room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] ) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
	default: room = NULL; break;
	}

	if (!room || !room->people) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		add_buf(buffer,"\n\r");
		room_echo(room, buffer->string);
	}
	free_buf(buffer);
}


// do_rpechoaround
SCRIPT_CMD(do_rpechoaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	free_buf(buffer);
}

// do_rpechonotvict
SCRIPT_CMD(do_rpechonotvict)
{
	char *rest;
	CHAR_DATA *victim, *attacker;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != attacker->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act(buffer->string, victim, attacker, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpechobattlespam)
{
	char *rest;
	CHAR_DATA *victim, *attacker, *ch;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != attacker->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
			if (!IS_NPC(ch) && (ch != attacker && ch != victim) && (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM))) {
				act(buffer->string, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
		}
	}

	free_buf(buffer);
}

// do_rpechoat
SCRIPT_CMD(do_rpechoat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	free_buf(buffer);
}

// do_rpechochurch
SCRIPT_CMD(do_rpechochurch)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim) || !victim->church)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		msg_church_members(victim->church, buffer->string);
	}

	free_buf(buffer);
}



// do_rpechogrouparound
SCRIPT_CMD(do_rpechogrouparound)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act_new(buffer->string,victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
	}

	free_buf(buffer);
}

// do_rpechogroupat
SCRIPT_CMD(do_rpechogroupat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act_new(buffer->string,victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);
	}

	free_buf(buffer);
}

// do_rpecholeadaround
SCRIPT_CMD(do_rpecholeadaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->leader || !victim->leader->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}

	free_buf(buffer);
}

// do_rpecholeadat
SCRIPT_CMD(do_rpecholeadat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room,arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->leader || !victim->leader->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *next;
	bool fAll = false, forced;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpForce - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = true;
		else victim = get_char_room(NULL,info->room, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!fAll && !victim) {
		bug("RpForce - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		forced = forced_command;

		if (fAll) {
			for (victim = info->room->people; victim; victim = next) {
				next = victim->next_in_room;
				forced_command = true;
				interpret(victim, buffer->string);
			}
		} else {
			forced_command = true;
			interpret(victim, buffer->string);
		}

		forced_command = forced;
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpforget)
{
	if(!info || !info->room) return;

	info->room->progs->target = NULL;
}

SCRIPT_CMD(do_rpgdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *rch, *rch_next;
	int low, high, level, value, dc;
	bool fKill = false, fLevel = false, fRemort = false, fTwo = false;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpGdamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("RpGdamage - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	if(!*rest) {
		bug("RpGdamage - missing argument from vnum %ld.", info->room->vnum);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpGdamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = true; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = true; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = true; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = true; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		bug("RpGdamage - invalid argument from vnum %ld.", info->room->vnum);
		return;
	}

	if(!*rest) {
		bug("RpGdamage - missing argument from vnum %ld.", info->room->vnum);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpGdamage - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	level = victim->tot_level;
	switch(arg->type) {
	case ENT_NUMBER:
		if(fLevel) level = arg->d.num;
		else high = arg->d.num;
		break;
	case ENT_STRING:
		if(is_number(arg->d.str)) {
			if(fLevel) level = atoi(arg->d.str);
			else high = atoi(arg->d.str);
		} else {
			bug("RpGdamage - invalid argument from vnum %ld.", info->room->vnum);
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("RpGdamage - Null reference mob from vnum %ld.", info->room->vnum);
				return;
			}
			break;
		} else {
			bug("RpGdamage - invalid argument from vnum %ld.", info->room->vnum);
			return;
		}
		break;
	default:
		bug("RpGdamage - invalid argument from vnum %ld.", info->room->vnum);
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	for(rch = info->room->people; rch; rch = rch_next) {
		rch_next = rch->next_in_room;
		if (rch != victim && is_same_group(victim,rch)) {
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(rch, rch, fKill ? value : UMIN(rch->hit,value), NULL, TYPE_UNDEFINED, dc, false);
		}
	}
}

SCRIPT_CMD(do_rpgecho)
{
	DESCRIPTOR_DATA *d;

	if(!info || !info->room) return;

	if (!argument[0]) {
		bug("RpGEcho: missing argument from vnum %d", info->room->vnum);
		return;
	}

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (d = descriptor_list; d; d = d->next)
			if (d->connected == CON_PLAYING) {
				if (IS_IMMORTAL(d->character) && IS_SET(d->character->act[0], PLR_HOLYLIGHT))
					send_to_char("Room echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpgforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *vch, *next;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpGforce - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!victim) {
		bug("RpGforce - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (vch = info->room->people; vch; vch = next) {
			next = vch->next_in_room;
			if (is_same_group(victim,vch))
				interpret(vch, buffer->string);
		}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpgtransfer)
{
	char buf[MIL], buf2[MIL], buf3[MIL], *rest;
	CHAR_DATA *victim, *vch,*next;
	ROOM_INDEX_DATA *dest;
	bool all = false, force = false, quiet = false;
	int mode;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpGtransfer - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("RpGtransfer - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	if (!victim->in_room) return;

	if(!(argument = rp_getlocation(info, rest, &dest))) {
		bug("RpGtransfer - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	if(!dest) {
		bug("RpGtransfer - Bad location from vnum %d.", info->room->vnum);
		return;
	}

	argument = one_argument(argument,buf);
	argument = one_argument(argument,buf2);
	argument = one_argument(argument,buf3);
	all = !str_cmp(buf,"all") || !str_cmp(buf2,"all") || !str_cmp(buf3,"all") || !str_cmp(argument,"all");
	force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(buf3,"force") || !str_cmp(argument,"all");
	quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(buf3,"quiet") || !str_cmp(argument,"all");
	mode = script_flag_value(transfer_modes, buf);
	if( mode == NO_FLAG ) mode = script_flag_value(transfer_modes, buf2);
	if( mode == NO_FLAG ) mode = script_flag_value(transfer_modes, buf3);
	if( mode == NO_FLAG ) mode = script_flag_value(transfer_modes, argument);
	if( mode == NO_FLAG ) mode = TRANSFER_MODE_SILENT;

	for (vch = info->room->people; vch; vch = next) {
		next = vch->next_in_room;
		if (is_same_group(victim,vch)) {
			if (!all && vch->position != POS_STANDING) continue;
			if (!IS_NPC(vch) && !force && room_is_private(dest, info->mob)) break;
			do_mob_transfer(vch,dest,quiet,mode);
		}
	}
}

SCRIPT_CMD(do_rplink)
{
	char *rest;
	ROOM_INDEX_DATA *room, *dest;
	int door;
	WNUM wnum = wnum_zero;
	unsigned long id1, id2;

	bool del = false;
	bool environ = false;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		room = info->room;
		door = get_num_dir(arg->d.str);
		break;
	case ENT_EXIT:
		room = arg->d.door.r;
		door = arg->d.door.door;
		break;
	default:
		room = NULL;
		door = -1;
	}

	if (!room) return;

	if (door < 0) {
		bug("OPlink used without an argument from room vnum %d.", room->vnum);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg)))
		return;

	id1 = id2 = 0;
	switch(arg->type) {
	case ENT_STRING:
		if(parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum))
			;
		else if(!str_cmp(arg->d.str,"delete") ||
			!str_cmp(arg->d.str,"remove") ||
			!str_cmp(arg->d.str,"unlink")) {
			del = true;
		} else if(!str_cmp(arg->d.str,"environment") ||
			!str_cmp(arg->d.str,"environ") ||
			!str_cmp(arg->d.str,"extern") ||
			!str_cmp(arg->d.str,"outside")) {
			environ = true;
		} else if(!str_cmp(arg->d.str,"vroom")) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_WIDEVNUM)
				return;
			wnum = arg->d.wnum;

			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
				return;
			id1 = arg->d.num;

			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
				return;
			id2 = arg->d.num;
		}
		break;
	case ENT_WIDEVNUM:
		wnum = arg->d.wnum;
		break;
	case ENT_ROOM:
		if (arg->d.room)
		{
			wnum.pArea = arg->d.room->area;
			wnum.vnum = arg->d.room->vnum;
		}
		break;
	case ENT_EXIT:
		if (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] && arg->d.door.r->exit[arg->d.door.door]->u1.to_room)
		{
			wnum.pArea = arg->d.door.r->exit[arg->d.door.door]->u1.to_room->area;
			wnum.vnum = arg->d.door.r->exit[arg->d.door.door]->u1.to_room->vnum;
		}
		break;
	case ENT_MOBILE:
		if (arg->d.mob && arg->d.mob->in_room)
		{
			wnum.pArea = arg->d.mob->in_room->area;
			wnum.vnum = arg->d.mob->in_room->vnum;
		}
		break;
	case ENT_OBJECT:
		if (arg->d.obj)
		{
			ROOM_INDEX_DATA *oroom = obj_room(arg->d.obj);

			if (oroom)
			{
				wnum.pArea = oroom->area;
				wnum.vnum = oroom->vnum;
			}
		}
		break;
	}

	if(!del && (!wnum.pArea || wnum.vnum < 1)) {
		bug("RPlink - invalid argument in room %d.", room->vnum);
		return;
	}

	if(del)
		dest = NULL;
	else if(id1 > 0 || id2 > 0)
		dest = get_clone_room(get_room_index(wnum.pArea, wnum.vnum),id1,id2);
	else if(wnum.pArea && wnum.vnum > 0)
		dest = get_room_index(wnum.pArea, wnum.vnum);
	else if(environ)
		dest = &room_pointer_environment;
	else
		dest = NULL;

	if(!dest && !del) {
		bug("RPlink - invalid destination in room %d.", room->vnum);
		return;
	}

	script_change_exit(room, dest, door);
}

SCRIPT_CMD(do_rpmload)
{
	char buf[MIL], *rest;
	WNUM wnum = wnum_zero;
	MOB_INDEX_DATA *pMobIndex;
	CHAR_DATA *victim;


	if(!info || !info->room || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_WIDEVNUM: wnum = arg->d.wnum; break;
	case ENT_STRING: parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum); break;
	case ENT_MOBILE:
		if (arg->d.mob)
		{
			wnum.pArea = arg->d.mob->pIndexData->area;
			wnum.vnum = arg->d.mob->pIndexData->vnum;
		}
		break;
	default: break;
	}

	if (!wnum.pArea || wnum.vnum < 1 || !(pMobIndex = get_mob_index(wnum.pArea, wnum.vnum))) {
		sprintf(buf, "Rpmload: bad mob index (%ld#%ld) from mob %ld", wnum.pArea->uid, wnum.vnum, info->room->vnum);
		bug(buf, 0);
		return;
	}

	victim = create_mobile(pMobIndex, false);
	char_to_room(victim, info->room);
	if(rest && *rest) variables_set_mobile(info->var,rest,victim);
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);
}

SCRIPT_CMD(do_rpoload)
{
	char buf[MIL], *rest;
	long level;
	bool fWear = false;
	WNUM wnum = wnum_zero;
	OBJ_INDEX_DATA *pObjIndex;
	OBJ_DATA *obj;

	CHAR_DATA *to_mob = NULL;
	OBJ_DATA *to_obj = NULL;
	ROOM_INDEX_DATA *to_room = NULL;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_WIDEVNUM: wnum = arg->d.wnum; break;
	case ENT_STRING: parse_widevnum(arg->d.str, get_area_from_scriptinfo(info), &wnum); break;
	case ENT_OBJECT:
		if (arg->d.obj)
		{
			wnum.pArea = arg->d.obj->pIndexData->area;
			wnum.vnum = arg->d.obj->pIndexData->vnum;
		}
		break;
	default: break;
	}

	if (!wnum.pArea || wnum.vnum < 1 || !(pObjIndex = get_obj_index(wnum.pArea, wnum.vnum))) {
		bug("Rpoload - Bad vnum arg from vnum %d.", info->room->vnum);
		return;
	}

	if(rest && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg)))
			return;

		switch(arg->type) {
		case ENT_NUMBER: level = arg->d.num; break;
		case ENT_STRING: level = arg->d.str ? atoi(arg->d.str) : 0; break;
		case ENT_MOBILE:
		{
			CLASS_LEVEL *mcl = arg->d.mob ? get_class_level(arg->d.mob, NULL) : NULL;
			level = mcl ? mcl->level : (arg->d.mob ? arg->d.mob->tot_level : 0); break;
		}
		case ENT_OBJECT: level = arg->d.obj ? arg->d.obj->pIndexData->level : 0; break;
		default: level = 0; break;
		}

		if(level <= 0) level = pObjIndex->level;

		if(rest && *rest) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)))
				return;

			/*
			 * Added 3rd argument
			 * omitted - load to current room
			 * MOBILE  - load to target mobile
			 *         - 'W' automatically wear the item if possible
			 * OBJECT  - load to target object
			 * ROOM    - load to target room
			 */

			switch(arg->type) {
			case ENT_MOBILE:
				to_mob = arg->d.mob;
				if((rest = one_argument(rest,buf))) {
					if (!str_cmp(buf, "wear"))
						fWear = true;
					// use "none" for neither
				}
				break;

			case ENT_OBJECT:
				if( arg->d.obj && IS_SET(pObjIndex->wear_flags, ITEM_TAKE) ) {
					if(IS_CONTAINER(arg->d.obj))
					{
						int subtype = objindex_get_subtype(pObjIndex);
						if (container_is_valid_item_type(arg->d.obj, pObjIndex->item_type, subtype))
							to_obj = arg->d.obj;
					}
					else if(arg->d.obj->item_type == ITEM_CORPSE_NPC || arg->d.obj->item_type == ITEM_CORPSE_PC)
						to_obj = arg->d.obj;
					else
						return;	// Trying to put the item into a non-container won't work
				}
				break;

			case ENT_ROOM:		to_room = arg->d.room; break;
			}
		}


	} else
		level = pObjIndex->level;

	obj = create_object(pObjIndex, level, true);
	if(to_room)
		obj_to_room(obj, to_room);
	else if( to_obj )
		obj_to_obj(obj, to_obj);
	else if( to_mob && CAN_WEAR(obj, ITEM_TAKE) &&
		(to_mob->carry_number < can_carry_n (to_mob)) &&
		(get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
		obj_to_char(obj, to_mob);
		if (fWear)
			wear_obj(to_mob, obj, true);
	} else
		obj_to_room(obj, info->room);

	if(rest && *rest) variables_set_object(info->var,rest,obj);
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL,0,0,0,0,0);
}

SCRIPT_CMD(do_rpotransfer)
{
	char *rest;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *dest;
	OBJ_DATA *container;
	CHAR_DATA *carrier;
	int wear_loc = WEAR_NONE;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpOtransfer - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: obj = get_obj_here(NULL, info->room, arg->d.str);
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: obj = NULL; break;
	}


	if (!obj) {
		bug("RpOtransfer - Null object from vnum %ld.", info->room->vnum);
		return;
	}

	if (PROG_FLAG(obj,PROG_AT)) return;

	if (IS_SET(obj->extra[2], ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

	argument = rp_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

	if(!dest && !container && !carrier) {
		bug("RpOTransfer - Bad location from vnum %d.", info->room->vnum);
		return;
	}

	ROOM_INDEX_DATA *old_room = NULL;

	if (obj->carried_by) {
		if (obj->wear_loc != WEAR_NONE)
			unequip_char(obj->carried_by, obj, true);
		obj_from_char(obj);
	} else if(obj->in_obj)
		obj_from_obj(obj);
	else
	{
		old_room = obj_room(obj);
		obj_from_room(obj);
	}

	if(dest) {
		if(dest->wilds)
			obj_to_vroom(obj, dest->wilds, dest->x, dest->y);
		else
			obj_to_room(obj, dest);

		if (IS_FURNITURE(obj) && IS_SET(FURNITURE(obj)->flags, FURNITURE_KEEP_OCCUPANTS) && old_room)
		{
			if (argument && *argument)
			{
				if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING) {
					return;
				}

				// Optional "occupants"
				if (!str_prefix(arg->d.str, "occupants"))
				{
					bool show = false;
					if(!(rest = expand_argument(info,argument,arg)))
						return;

					if (arg->type == ENT_BOOLEAN)
						show = arg->d.boolean;
					else if (arg->type == ENT_STRING)
						show = !str_prefix(arg->d.str, "show");

					CHAR_DATA *rch, *rch_next;
					for (rch = old_room->people; rch != NULL; rch = rch_next)
					{
						rch_next = rch->next_in_room;
						if (rch->on == obj)
						{
							char_from_room(rch);

							if(dest->wilds)
								char_to_vroom(rch, dest->wilds, dest->x, dest->y);
							else
								char_to_room(rch, dest);

							if (show)
								do_function(rch, &do_look, "auto");
						}
					}
					return;
				}
			}
		}

		if (old_room)
		{
			for (CHAR_DATA *rch = old_room->people; rch != NULL; rch = rch->next_in_room)
				if (rch->on == obj)
				{
					rch->on = NULL;
					rch->on_compartment = NULL;
				}
		}
	}
	else if(container)
	{
		// Kick off occupants if not staying in a room
		if (old_room)
		{
			for (CHAR_DATA *rch = old_room->people; rch != NULL; rch = rch->next_in_room)
				if (rch->on == obj)
				{
					rch->on = NULL;
					rch->on_compartment = NULL;
				}
		}

		obj_to_obj(obj, container);
	}
	else if(carrier)
	{
		// Kick off occupants if not staying in a room
		if (old_room)
		{
			for (CHAR_DATA *rch = old_room->people; rch != NULL; rch = rch->next_in_room)
				if (rch->on == obj)
				{
					rch->on = NULL;
					rch->on_compartment = NULL;
				}
		}

		obj_to_char(obj, carrier);
		if(wear_loc != WEAR_NONE)
			equip_char(carrier, obj, wear_loc);
	}
}

SCRIPT_CMD(do_rppeace)
{
	CHAR_DATA *rch;
	if(!info || !info->room) return;

	for (rch = info->room->people; rch; rch = rch->next_in_room) {
		if (rch->fighting)
			stop_fighting(rch, true);
		if (IS_NPC(rch) && IS_SET(rch->act[0],ACT_AGGRESSIVE))
			REMOVE_BIT(rch->act[0],ACT_AGGRESSIVE);

		// TOOD: Remove AFF2_AGGRESSIVE
	}
}

SCRIPT_CMD(do_rppurge)
{
	char *rest;
	CHAR_DATA **mobs = NULL, *victim = NULL,*vnext;
	OBJ_DATA **objs = NULL, *obj = NULL,*obj_next;
	ROOM_INDEX_DATA *here = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NONE: here = info->room; break;
	case ENT_STRING:
		if (!(victim = get_char_room(NULL, info->room, arg->d.str)))
			obj = get_obj_here(NULL, info->room, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	case ENT_ROOM: here = arg->d.room; break;
	case ENT_EXIT: here = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
	case ENT_OLLIST_MOB: mobs = arg->d.list.ptr.mob; break;
	case ENT_OLLIST_OBJ: objs = arg->d.list.ptr.obj; break;
	default: break;
	}

	if(victim) {
		if (!IS_NPC(victim)) {
			bug("Rppurge - Attempting to purge a PC from vnum %d.", info->room->vnum);
			return;
		}
		extract_char(victim, true);
	} else if(obj)
		extract_obj(obj);
	else if(here) {
		for (victim = here->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && !IS_SET(victim->act[0], ACT_NOPURGE))
				extract_char(victim, true);
		}

		for (obj = here->contents; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (!IS_SET(obj->extra[0], ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else if(mobs) {
		for (victim = *mobs; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && !IS_SET(victim->act[0], ACT_NOPURGE))
				extract_char(victim, true);
		}
	} else if(objs) {
		for (obj = *objs; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (!IS_SET(obj->extra[0], ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else
		bug("Rppurge - Bad argument from vnum %d.", info->room->vnum);
}

SCRIPT_CMD(do_rpqueue)
{
	char *rest;
	int delay;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: delay = arg->d.num; break;
	case ENT_STRING: delay = atoi(arg->d.str); break;
	default:
		bug("RpQueue:  missing arguments from vnum %d.", info->room->vnum);
		return;
	}

	if (delay < 0 || delay > 1000) {
		bug("RpQueue:  unreasonable delay recieved from vnum %d.", info->room->vnum);
		return;
	}

	wait_function(info->room, info, EVENT_ROOMQUEUE, delay, script_interpret, rest);
}

SCRIPT_CMD(do_rpremember)
{
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!expand_argument(info,argument,arg)) {
		bug("RpRemember: Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("RpRemember: Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	info->room->progs->target = victim;
}

SCRIPT_CMD(do_rpremove)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj = NULL, *obj_next;
	int vnum = 0, count = 0;
	bool fAll = false;

	char name[MIL], *rest;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpRemove: Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str);
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("RpRemove: Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	if(!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpRemove: Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	name[0] = '\0';
	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING:
		if(is_number(arg->d.str))
			vnum = atoi(arg->d.str);
		else if(!str_cmp(arg->d.str,"all"))
			fAll = true;
		else
			strncpy(name,arg->d.str,MIL-1);
		break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!fAll && vnum < 1 && !name[0] && !obj) {
		bug ("OpRemove: Invalid object from vnum %ld.", info->room->vnum);
		return;
	}

	if(!fAll && !obj && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpRemove: Bad syntax from vnum %ld.", info->room->vnum);
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER: count = arg->d.num; break;
		case ENT_STRING: count = atoi(arg->d.str); break;
		default: count = 0; break;
		}

		if(count < 0) {
			bug ("RpRemove: Invalid count from vnum %d.", info->room->vnum);
			count = 0;
		}
	}

	if(obj) {
		if(obj->carried_by == victim || (obj->in_obj && obj->in_obj->carried_by == victim))
			extract_obj(obj);
	} else {
		for (obj = victim->carrying; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum) ||
				(*name && is_name(name, obj->pIndexData->skeywds))) {
				extract_obj(obj);

				if(count > 0 && !--count) break;
			}
		}
	}
}

SCRIPT_CMD(do_rptransfer)
{
	char buf[MIL], buf2[MIL], *rest;
	CHAR_DATA *victim = NULL,*vnext;
	ROOM_INDEX_DATA *dest;
	bool all = false, force = false, quiet = false;
	int mode;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpTransfer - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) all = true;
		else victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim && !all) {
		bug("RpTransfer - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	// Crashes on transfer all as victim isn't set at this point.
	//if (!victim->in_room) return;

	argument = rp_getlocation(info, rest, &dest);

	if(!dest) {
		bug("RpTransfer - Bad location from vnum %d.", info->room->vnum);
		return;
	}

	argument = one_argument(argument,buf);
	argument = one_argument(argument,buf2);
	force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
	quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");
	mode = script_flag_value(transfer_modes, buf);
	if( mode == NO_FLAG ) mode = script_flag_value(transfer_modes, buf2);
	if( mode == NO_FLAG ) mode = script_flag_value(transfer_modes, argument);
	if( mode == NO_FLAG ) mode = TRANSFER_MODE_SILENT;

	if (all) {
		for (victim = info->room->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (PROG_FLAG(victim,PROG_AT)) continue;
			if (!IS_NPC(victim)) {
				if (!force && room_is_private(dest, NULL)) break;
			}
			do_mob_transfer(victim,dest,quiet,mode);
		}
		return;
	}

	if (!force && room_is_private(dest, NULL))
		return;

	if (PROG_FLAG(victim,PROG_AT))
		return;

	do_mob_transfer(victim,dest,quiet,mode);
}

SCRIPT_CMD(do_rpvforce)
{
	char *rest;
	int vnum = 0;
	CHAR_DATA *vch, *next;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpVforce - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: break;
	}

	if (vnum < 1) {
		bug("RpVforce - Invalid vnum from vnum %ld.", info->room->vnum);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (vch = info->room->people; vch; vch = next) {
			next = vch->next_in_room;
			if (IS_NPC(vch) &&  vch->pIndexData->vnum == vnum && !vch->fighting)
				interpret(vch, buffer->string);
		}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpzecho)
{
	AREA_DATA *area;
	DESCRIPTOR_DATA *d;

	if(!info || !info->room) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		area = info->room->area;

		for (d = descriptor_list; d; d = d->next)
			if (d->connected == CON_PLAYING &&
				d->character->in_room &&
				d->character->in_room->area == area) {
				if (IS_IMMORTAL(d->character) && IS_SET(d->character->act[0], PLR_HOLYLIGHT))
					send_to_char("Room echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpzot)
{
	CHAR_DATA *victim;


	if(!info || !info->room) return;

	if(!expand_argument(info,argument,arg))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,info->room, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("RpZot - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	send_to_char("{Y***{R****** {WZOT {R******{Y***{x\n\r\n\r", victim);
	send_to_char("{YYou are struck by a bolt of lightning!\n\r{x", victim);
	act("{Y$n is struck by a bolt of lightning!{x", victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	send_to_char("{ROUCH! That really did hurt!{x\n\r", victim);

	victim->hit = 1;
	victim->mana = 1;
	victim->move = 1;
}

SCRIPT_CMD(do_rpvarset)
{
	if(!info || !info->room || !info->var) return;

	script_varseton(info, info->var, argument, arg);
}

SCRIPT_CMD(do_rpvarclear)
{
	if(!info || !info->room || !info->var) return;

	script_varclearon(info, info->var, argument, arg);
}

SCRIPT_CMD(do_rpvarcopy)
{
	char oldname[MIL],newname[MIL];

	if(!info || !info->room || !info->var) return;

	// Get name
	argument = one_argument(argument,oldname);
	if(!oldname[0]) return;
	argument = one_argument(argument,newname);
	if(!newname[0]) return;

	if(!str_cmp(oldname,newname)) return;

	variable_copy(info->var,oldname,newname);
}

SCRIPT_CMD(do_rpvarsave)
{
	char name[MIL],arg1[MIL];
	bool on;

	if(!info || !info->room || !info->var) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,arg1);
	if(!arg1[0]) return;

	on = !str_cmp(arg1,"on") || !str_cmp(arg1,"true") || !str_cmp(arg1,"yes");

	variable_setsave(*info->var,name,on);
}

SCRIPT_CMD(do_rpsettimer)
{
	char buf[MIL],*rest;
	int amt;
	CHAR_DATA *victim = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	default: break;
	}

	if(!victim) {
		bug("RpSetTimer - NULL victim.", 0);
		return;
	}

	if(!*rest) {
		bug("RpSetTimer - Missing timer type.",0);
		return;
	}

	buf[0] = 0;
	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		strncpy(buf,arg->d.str,MIL);
		break;
	default: break;
	}

	if(!*rest) {
		bug("RpSetTimer - Missing timer amount.",0);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amt = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: amt = arg->d.num; break;
	default: amt = 0; break;
	}

	if( amt < 0 )
		return;

	if(!str_cmp(buf,"hiredto"))
	{
		if(IS_NPC(victim))
		{
			SET_BIT(victim->act[1], ACT2_HIRED);
			victim->hired_to = current_time + amt * 60;
			// If amt is zero, the expiration will be handled in update.c
		}
	}
	else if( amt > 0 || script_security >= 5 ) {
		if(!str_cmp(buf,"wait")) WAIT_STATE(victim, amt);
		else if(!str_cmp(buf,"norecall")) NO_RECALL_STATE(victim, amt);
		else if(!str_cmp(buf,"daze")) DAZE_STATE(victim, amt);
		else if(!str_cmp(buf,"panic")) PANIC_STATE(victim, amt);
		else if(!str_cmp(buf,"paroxysm")) PAROXYSM_STATE(victim, amt);
		else if(!str_cmp(buf,"paralyze")) victim->paralyzed = UMAX(victim->paralyzed,amt);
		else if(!str_cmp(buf,"nextmission"))
		{
			if(!IS_NPC(victim))
			{
				victim->nextmission = amt;
			}
		}
	}
}

SCRIPT_CMD(do_rpinterrupt)
{
	char *rest;
	CHAR_DATA *victim = NULL;
	ROOM_INDEX_DATA *here;

	int stop, ret = 0;
	bool silent = false;

	if(!info || !info->room) return;

	here = info->room;

	info->room->progs->lastreturn = 0;	// Nothing was interrupted

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpInterrupt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	default: break;
	}

	if(!victim) {
		bug("RpInterrupt - NULL victim.", 0);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buffer->string[0] != '\0') {
		stop = flag_value(interrupt_action_types,buffer->string);
		if(stop == NO_FLAG) {
			bug("RpInterrupt - invalid interrupt type.", 0);
			free_buf(buffer);
			return;
		}
	} else
		stop = ~INTERRUPT_SILENT;	// stop anything

	ret = 0;

	if (IS_SET(stop,INTERRUPT_SILENT))
		silent = true;

	if (IS_SET(stop,INTERRUPT_CAST) && victim->cast > 0) {
		stop_casting(victim, !silent);
		SET_BIT(ret,INTERRUPT_CAST);
	}

	if (IS_SET(stop,INTERRUPT_MUSIC) && victim->music > 0) {
		stop_music(victim, !silent);
		SET_BIT(ret,INTERRUPT_MUSIC);
	}

	if (IS_SET(stop,INTERRUPT_BREW) && victim->brew > 0) {
		victim->brew = 0;
		victim->brew_info = NULL;
		SET_BIT(ret,INTERRUPT_BREW);
	}

	if (IS_SET(stop,INTERRUPT_REPAIR) && victim->repair > 0) {
		variables_set_object(info->var,"stoprepair",victim->repair_obj);
		victim->repair_obj = NULL;
		victim->repair_amt = 0;
		victim->repair = 0;
		SET_BIT(ret,INTERRUPT_REPAIR);
	}

	if (IS_SET(stop,INTERRUPT_HIDE) && victim->hide > 0) {
		victim->hide = 0;
		SET_BIT(ret,INTERRUPT_HIDE);
	}

	if (IS_SET(stop,INTERRUPT_BIND) && victim->bind > 0) {
		variables_set_mobile(info->var,"stopbind",victim->bind_victim);
		victim->bind = 0;
		victim->bind_victim = NULL;
		SET_BIT(ret,INTERRUPT_BIND);
	}

	if (IS_SET(stop,INTERRUPT_BOMB) && victim->bomb > 0) {
		victim->bomb = 0;
		SET_BIT(ret,INTERRUPT_BOMB);
	}

	if (IS_SET(stop,INTERRUPT_RECITE) && victim->recite > 0) {
		if(victim->cast_target_name)
			variables_set_string(info->var,"stoprecitetarget",victim->cast_target_name,false);
		else
			variables_set_string(info->var,"stoprecitetarget","",false);
		variables_set_object(info->var,"stopreciteobj",victim->recite_scroll);
		victim->recite = 0;
		victim->cast_target_name = NULL;
		victim->recite_scroll = NULL;
		SET_BIT(ret,INTERRUPT_RECITE);
	}


	if (IS_SET(stop,INTERRUPT_REVERIE) && victim->reverie > 0) {
		variables_set_integer(info->var,"stopreverie",victim->reverie_amount);
		// 0:hit->mana,1:mana->hit
		variables_set_integer(info->var,"stopreverietype",(victim->reverie_type == MANA_TO_HIT));
		victim->reverie = 0;
		victim->reverie_amount = 0;
		SET_BIT(ret,INTERRUPT_REVERIE);
	}

	if (IS_SET(stop,INTERRUPT_TRANCE) && victim->trance > 0) {
		victim->trance = 0;
		SET_BIT(ret,INTERRUPT_TRANCE);
	}


	if (IS_SET(stop,INTERRUPT_SCRIBE) && victim->scribe > 0) {
		victim->scribe = 0;
		victim->scribe_info[0] = NULL;
		victim->scribe_info[1] = NULL;
		victim->scribe_info[2] = NULL;
		SET_BIT(ret,INTERRUPT_SCRIBE);
	}

	if (IS_SET(stop,INTERRUPT_INK) && victim->inking > 0) {
		victim->inking = 0;
		victim->ink_info[0] = NULL;
		victim->ink_info[1] = NULL;
		victim->ink_info[2] = NULL;
		SET_BIT(ret,INTERRUPT_INK);
	}

	if (IS_SET(stop, INTERRUPT_IMBUE) && victim->imbuing > 0) {
		victim->imbuing = 0;
		victim->imbue_type = IMBUE_NONE;
		victim->imbue_info[0] = NULL;
		victim->imbue_info[1] = NULL;
		victim->imbue_info[2] = NULL;
		victim->imbue_obj = NULL;
		SET_BIT(ret,INTERRUPT_IMBUE);
	}

	if (IS_SET(stop,INTERRUPT_RANGED) && victim->ranged > 0) {
		if(victim->projectile_victim)
			variables_set_string(info->var,"stoprangedtarget",victim->projectile_victim,false);
		else
			variables_set_string(info->var,"stoprangedtarget","",false);
		variables_set_object(info->var,"stoprangedweapon",victim->projectile_weapon);
		variables_set_object(info->var,"stoprangedammo",victim->projectile);
		variables_set_integer(info->var,"stoprangedist",victim->projectile_range);
		if(victim->projectile_dir == -1)
			variables_set_exit(info->var,"stoprangeexit",NULL);
		else
			variables_set_exit(info->var,"stoprangeexit",here->exit[victim->projectile_dir]);
		victim->ranged = 0;
		victim->projectile_weapon = NULL;
		free_string( victim->projectile_victim );
		victim->projectile_victim = NULL;
		victim->projectile_dir = -1;
		victim->projectile_range = 0;
		victim->projectile = NULL;
		SET_BIT(ret,INTERRUPT_RANGED);
	}

	if (IS_SET(stop,INTERRUPT_RESURRECT) && victim->resurrect > 0) {
		variables_set_object(info->var,"stopresurrectcorpse",victim->resurrect_target);
		if(victim->resurrect_target)
			variables_set_mobile(info->var,"stopresurrect",get_char_world(NULL, victim->resurrect_target->owner));
		else
			variables_set_mobile(info->var,"stopresurrect",NULL);
		victim->resurrect = 0;
		victim->resurrect_target = NULL;
		SET_BIT(ret,INTERRUPT_RESURRECT);
	}

	if (IS_SET(stop,INTERRUPT_FADE) && victim->fade > 0) {
		if(victim->fade_dir == -1)
			variables_set_exit(info->var,"stopfade",NULL);
		else
			variables_set_exit(info->var,"stopfade",here->exit[victim->fade_dir]);
		victim->fade = 0;
		victim->fade_dir = -1;
		SET_BIT(ret,INTERRUPT_FADE);
	}

	if (IS_SET(stop,INTERRUPT_SCRIPT)) {
		if(interrupt_script(victim, silent))
			SET_BIT(ret,INTERRUPT_SCRIPT);
	}

	// Indicate what was stopped, zero being nothing
	info->room->progs->lastreturn = ret;

	free_buf(buffer);
}

SCRIPT_CMD(do_rpalterobj)
{
	char buf[2*MIL],field[MIL],*rest;
	int value, num, min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;
	int min = 0, max = 0;
	bool hasmin = false, hasmax = false;
	bool allowarith = true;
	const struct flag_type *flags = NULL;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,info->room,arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("RpAlterObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("RpAlterObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterObj - Error in parsing.",0);
		return;
	}

	field[0] = 0;
	num = -1;

	switch(arg->type) {
	case ENT_STRING:
		if(is_number(arg->d.str)) {
			num = atoi(arg->d.str);
			if(num < 0 || num >= 8) return;
		} else
			strncpy(field,arg->d.str,MIL-1);
		break;
	case ENT_NUMBER:
		num = arg->d.num;
		if(num < 0 || num >= 8) return;
		break;
	default: return;
	}

	if(num < 0 && !field[0]) return;

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterObj - Error in parsing.",0);
		return;
	}

	if(num >= 0) {
		switch(arg->type) {
		case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: value = arg->d.num; break;
		default: return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"RpAlterObj - Attempting to alter value%d with security %d.\n\r", num, script_security);
			bug(buf, 0);
			return;
		}

		switch (buf[0]) {
		case '+': obj->value[num] += value; break;
		case '-': obj->value[num] -= value; break;
		case '*': obj->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("RpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			obj->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("RpAlterObj - adjust called with operator % and value 0", 0);
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
		else if(!str_cmp(field,"cost"))			{ ptr = (int*)&obj->cost; min_sec = 5; }
		else if(!str_cmp(field,"extra"))		{ ptr = (int*)&obj->extra[0]; flags = extra_flags; }
		else if(!str_cmp(field,"extra2"))		{ ptr = (int*)&obj->extra[1]; flags = extra2_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra3"))		{ ptr = (int*)&obj->extra[2]; flags = extra3_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra4"))		{ ptr = (int*)&obj->extra[3]; flags = extra4_flags; min_sec = 7; }
		else if(!str_cmp(field,"fixes"))		{ ptr = (int*)&obj->times_allowed_fixed; min_sec = 5; }
		else if(!str_cmp(field,"level"))		{ ptr = (int*)&obj->level; min_sec = 5; }
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
			sprintf(buf,"RpAlterObj - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			bug(buf, 0);
			return;
		}

		if( flags != NULL )
		{
			if( arg->type != ENT_STRING ) return;

			allowarith = false;	// This is a bit vector, no arithmetic operators.
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
				bug("RpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value;
			break;

		case '-':
			if( !allowarith ) {
				bug("RpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value;
			break;

		case '*':
			if( !allowarith ) {
				bug("RpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value;
			break;

		case '/':
			if( !allowarith ) {
				bug("RpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("RpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("RpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("RpAlterObj - adjust called with operator % and value 0", 0);
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
}



SCRIPT_CMD(do_rpresetdice)
{
	char *rest;
	OBJ_DATA *obj = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,info->room,arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("RpAlterObj - NULL object.", 0);
		return;
	}

	if(obj->item_type == ITEM_WEAPON)
		set_weapon_dice_obj(obj);
}




SCRIPT_CMD(do_rpaltermob)
{
	char buf[MSL],field[MIL],*rest;
	int value = 0, min_sec = MIN_SCRIPT_SECURITY, min = 0, max = 0;
	CHAR_DATA *mob = NULL;

	int *ptr = NULL;
	bool allowpc = false;
	bool allowarith = true;
	bool allowbitwise = true;
	bool lookuprace = false;
	bool hasmin = false;
	bool hasmax = false;
	bool check_catalyst = false;
	const struct flag_type *flags = NULL;
	const struct flag_type **bank = NULL;
	long temp_flags[4];
	int dirty_stat = -1;
	int catalyst = CATALYST_NONE;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpAlterMob - NULL mobile.", 0);
		return;
	}

	if(!*rest) {
		bug("RpAlterMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterMob - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

	if(!str_cmp(field,"acbash"))		ptr = (int*)&mob->armour[AC_BASH];
	else if(!str_cmp(field,"acexotic"))	ptr = (int*)&mob->armour[AC_EXOTIC];
	else if(!str_cmp(field,"acpierce"))	ptr = (int*)&mob->armour[AC_PIERCE];
	else if(!str_cmp(field,"acslash"))	ptr = (int*)&mob->armour[AC_SLASH];
	else if(!str_cmp(field,"act"))		{ ptr = (int*)mob->act; bank = IS_NPC(mob) ? act_flagbank : plr_flagbank; }
	else if(!str_cmp(field,"affect"))	{ ptr = (int*)mob->affected_by; bank = affect_flagbank; }
	else if(!str_cmp(field,"alignment"))	ptr = (int*)&mob->alignment;
	else if(!str_cmp(field,"bashed"))	ptr = (int*)&mob->bashed;
	else if(!str_cmp(field,"bind"))		ptr = (int*)&mob->bind;
	else if(!str_cmp(field,"bomb"))		ptr = (int*)&mob->bomb;
	else if(!str_cmp(field,"brew"))		ptr = (int*)&mob->brew;
	else if(!str_cmp(field,"cast"))		ptr = (int*)&mob->cast;
	else if(!str_cmp(field,"catalystusage"))	{ check_catalyst = true; }
	else if(!str_cmp(field,"comm"))		{ ptr = IS_NPC(mob)?NULL:(int*)&mob->comm; allowpc = true; allowarith = false; min_sec = 7; flags = comm_flags; }		// 20140512NIB - Allows for scripted fun with player communications, only bit operators allowed
	else if(!str_cmp(field,"damroll"))	ptr = (int*)&mob->damroll;
	else if(!str_cmp(field,"danger"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->danger_range; allowpc = true; }
	else if(!str_cmp(field,"daze"))		ptr = (int*)&mob->daze;
	else if(!str_cmp(field,"death"))	{ ptr = (IS_NPC(mob) || !IS_DEAD(mob))?NULL:(int*)&mob->time_left_death; allowpc = true; }
	else if(!str_cmp(field,"dicenumber"))	{ ptr = IS_NPC(mob)?&mob->damage.number:NULL; }
	else if(!str_cmp(field,"dicetype"))	{ ptr = IS_NPC(mob)?&mob->damage.size:NULL; }
	else if(!str_cmp(field,"dicebonus"))	{ ptr = IS_NPC(mob)?&mob->damage.bonus:NULL; }
	else if(!str_cmp(field,"drunk"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_DRUNK]; allowpc = true; }
//	else if(!str_cmp(field,"exitdir"))	{ ptr = (int*)&mob->exit_dir; allowpc = true; }
	else if(!str_cmp(field,"exp"))		{ ptr = (int*)&mob->exp; allowpc = true; }
	else if(!str_cmp(field,"fade"))		ptr = (int*)&mob->fade;
	else if(!str_cmp(field,"fullness"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_FULL]; allowpc = true; }
	else if(!str_cmp(field,"gold"))		ptr = (int*)&mob->gold;
	else if(!str_cmp(field,"hide"))		ptr = (int*)&mob->hide;
	else if(!str_cmp(field,"hit"))		ptr = (int*)&mob->hit;
	else if(!str_cmp(field,"hitdamage"))	ptr = (int*)&mob->hit_damage;
	else if(!str_cmp(field,"hitroll"))	ptr = (int*)&mob->hitroll;
	else if(!str_cmp(field,"hunger"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_HUNGER]; allowpc = true; }
	else if(!str_cmp(field,"imm"))		{ ptr = (int*)&mob->imm_flags; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"level"))	ptr = (int*)&mob->tot_level;
	else if(!str_cmp(field,"lostparts"))	{ ptr = (int*)&mob->lostparts; allowarith = false; flags = part_flags; }
	else if(!str_cmp(field,"mana"))		ptr = (int*)&mob->mana;
	else if(!str_cmp(field,"manastore"))	{ ptr = (int*)&mob->manastore; allowpc = true; }
	else if(!str_cmp(field,"maxexp"))	ptr = (int*)&mob->maxexp;
	else if(!str_cmp(field,"maxhit"))	ptr = (int*)&mob->max_hit;
	else if(!str_cmp(field,"maxmana"))	ptr = (int*)&mob->max_mana;
	else if(!str_cmp(field,"maxmove"))	ptr = (int*)&mob->max_move;
	else if(!str_cmp(field,"mazed"))	{ ptr = (IS_NPC(mob))?NULL:(int*)&mob->maze_time_left; allowpc = true; }
	else if(!str_cmp(field,"modcon"))	{ ptr = (int*)&mob->mod_stat[STAT_CON]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_CON; }
	else if(!str_cmp(field,"moddex"))	{ ptr = (int*)&mob->mod_stat[STAT_DEX]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_DEX; }
	else if(!str_cmp(field,"modint"))	{ ptr = (int*)&mob->mod_stat[STAT_INT]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_INT; }
	else if(!str_cmp(field,"modstr"))	{ ptr = (int*)&mob->mod_stat[STAT_STR]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_STR; }
	else if(!str_cmp(field,"modwis"))	{ ptr = (int*)&mob->mod_stat[STAT_WIS]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_WIS; }
	else if(!str_cmp(field,"move"))		ptr = (int*)&mob->move;
	else if(!str_cmp(field,"music"))	ptr = (int*)&mob->music;
	else if(!str_cmp(field,"norecall"))	ptr = (int*)&mob->no_recall;
	else if(!str_cmp(field,"panic"))	ptr = (int*)&mob->panic;
	else if(!str_cmp(field,"paralyzed"))	ptr = (int*)&mob->paralyzed;
	else if(!str_cmp(field,"paroxysm"))	ptr = (int*)&mob->paroxysm;
	else if(!str_cmp(field,"parts"))	{ ptr = (int*)&mob->parts; allowarith = false; flags = part_flags; }
	else if(!str_cmp(field,"permaffects"))	{ ptr = (int*)&mob->affected_by_perm[0]; allowarith = false; flags = affect_flags; }
	else if(!str_cmp(field,"permaffects2"))	{ ptr = (int*)&mob->affected_by_perm[1]; allowarith = false; flags = affect2_flags; }
	else if(!str_cmp(field,"permimm"))	{ ptr = (int*)&mob->imm_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"permres"))	{ ptr = (int*)&mob->res_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"permvuln"))	{ ptr = (int*)&mob->vuln_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"pktimer"))	ptr = (int*)&mob->pk_timer;
	else if(!str_cmp(field,"pneuma"))	ptr = (int*)&mob->pneuma;
	else if(!str_cmp(field,"practice"))	ptr = (int*)&mob->practice;
	else if(!str_cmp(field,"race"))		{ ptr = (int*)&mob->race; min_sec = 7; allowarith = false; lookuprace = true; }
	else if(!str_cmp(field,"ranged"))	ptr = (int*)&mob->ranged;
	else if(!str_cmp(field,"recite"))	ptr = (int*)&mob->recite;
	else if(!str_cmp(field,"res"))		{ ptr = (int*)&mob->res_flags;  allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"resurrect"))	ptr = (int*)&mob->resurrect;
	else if(!str_cmp(field,"reverie"))	ptr = (int*)&mob->reverie;
	else if(!str_cmp(field,"scribe"))	ptr = (int*)&mob->scribe;
	else if(!str_cmp(field,"sex"))		{ ptr = (int*)&mob->sex; min = 0; max = 2; hasmin = hasmax = true; flags = sex_flags; }
	else if(!str_cmp(field,"silver"))	ptr = (int*)&mob->silver;
	else if(!str_cmp(field,"size"))		{ ptr = (int*)&mob->size; min = SIZE_TINY; max = SIZE_GIANT; hasmin = hasmax = true; flags = size_flags; }
	else if(!str_cmp(field,"skillchance"))	ptr = (int*)&mob->skill_chance;
//	else if(!str_cmp(field,"sublevel"))	ptr = (int*)&mob->level;
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&mob->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&mob->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&mob->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&mob->tempstore[3];
	else if(!str_cmp(field,"tempstore5"))	ptr = (int*)&mob->tempstore[4];
	else if(!str_cmp(field,"thirst"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_THIRST]; allowpc = true; }
	else if(!str_cmp(field,"toxinneuro"))	ptr = (int*)&mob->toxin[TOXIN_NEURO];
	else if(!str_cmp(field,"toxinpara"))	ptr = (int*)&mob->toxin[TOXIN_PARALYZE];
	else if(!str_cmp(field,"toxinvenom"))	ptr = (int*)&mob->toxin[TOXIN_VENOM];
	else if(!str_cmp(field,"toxinweak"))	ptr = (int*)&mob->toxin[TOXIN_WEAKNESS];
	else if(!str_cmp(field,"train"))	ptr = (int*)&mob->train;
	else if(!str_cmp(field,"trance"))	ptr = (int*)&mob->trance;
	else if(!str_cmp(field,"vuln"))		{ ptr = (int*)&mob->vuln_flags; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"wait"))		ptr = (int*)&mob->wait;
	else if(!str_cmp(field,"wildviewx"))	ptr = (int*)&mob->wildview_bonus_x;
	else if(!str_cmp(field,"wildviewy"))	ptr = (int*)&mob->wildview_bonus_y;
	else if(!str_cmp(field,"wimpy"))	ptr = (int*)&mob->wimpy;

	// Special handling for catalystusage
	if (check_catalyst)
	{
		// Get the catalyst type
		if(!(rest = expand_argument(info,rest,arg)) && arg->type != ENT_STRING) {
			bug("RpAlterMob - Error in parsing.",0);
			return;
		}

		catalyst = stat_lookup(arg->d.str, catalyst_types, CATALYST_NONE);
		if (catalyst == CATALYST_NONE)
		{
			bug("RpAlterMob - Invalid catalyst type selected for catalystusage.",0);
			return;
		}

		ptr = &mob->catalyst_usage[catalyst];
		allowpc = true;
		allowarith = true;
		allowbitwise = false;
	}

	if(!ptr) return;

	rest = one_argument(rest,buf);

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterMob - Error in parsing.",0);
		return;
	}

	// MINIMUM to alter ANYTHING not allowed on players on a player
	if(!allowpc && !IS_NPC(mob)) min_sec = 9;

	if(script_security < min_sec) {
		sprintf(buf,"RpAlterMob - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		bug(buf, 0);
		return;
	}

	memset(temp_flags, 0, sizeof(temp_flags));

	if( lookuprace )
	{
		if( arg->type != ENT_STRING ) return;

		// This is a race, can only be assigned
		allowarith = false;
		allowbitwise = false;
		value = race_lookup(arg->d.str);
	}
	else if( bank != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = false;	// This is a bit vector, no arithmetic operators.
		if (!script_bitmatrix_lookup(arg->d.str, bank, temp_flags))
			return;

		if (bank == act_flagbank)
		{
			REMOVE_BIT(temp_flags[1], ACT2_INSTANCE_MOB);

			if( buf[0] == '=' || buf[0] == '&' )
			{
				if( IS_SET(ptr[1], ACT2_INSTANCE_MOB) ) SET_BIT(temp_flags[1], ACT2_INSTANCE_MOB);
			}
		}		
	}
	else if( flags != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = false;	// This is a bit vector, no arithmetic operators.
		value = script_flag_value(flags, arg->d.str);

		if( value == NO_FLAG ) value = 0;

		if( flags == act2_flags )
		{
			REMOVE_BIT(value, ACT2_INSTANCE_MOB);

			if( buf[0] == '=' || buf[0] == '&' )
			{
				if( IS_SET(*ptr, ACT2_INSTANCE_MOB) ) SET_BIT(value, ACT2_INSTANCE_MOB);
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
		case ENT_NUMBER:
			value = arg->d.num;
			break;
		default: return;
		}
	}

	switch (buf[0]) {
	case '+':
		if( !allowarith ) {
			bug("RpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr += value; break;
	case '-':
		if( !allowarith ) {
			bug("RpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr -= value; break;
	case '*':
		if( !allowarith ) {
			bug("RpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr *= value; break;
	case '/':
		if( !allowarith ) {
			bug("RpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("RpAlterMob - altermob called with operator / and value 0", 0);
			return;
		}
		*ptr /= value;
		break;
	case '%':
		if( !allowarith ) {
			bug("RpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("RpAlterMob - altermob called with operator % and value 0", 0);
			return;
		}
		*ptr %= value;
		break;

	case '=':
		if (bank != NULL)
		{
			for(int i = 0; bank[i]; i++)
				ptr[i] = temp_flags[i];
		}
		else
			*ptr = value;
		break;

	case '&':
		if( !allowbitwise ) {
			bug("RpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		if (bank != NULL)
		{
			for(int i = 0; bank[i]; i++)
				ptr[i] &= temp_flags[i];
		}
		else
			*ptr &= value;
		break;
	case '|':
		if( !allowbitwise ) {
			bug("RpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		if (bank != NULL)
		{
			for(int i = 0; bank[i]; i++)
				ptr[i] |= temp_flags[i];
		}
		else
			*ptr |= value;
		break;
	case '!':
		if( !allowbitwise ) {
			bug("RpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		if (bank != NULL)
		{
			for(int i = 0; bank[i]; i++)
				ptr[i] &= ~temp_flags[i];
		}
		else
			*ptr &= ~value;
		break;
	case '^':
		if( !allowbitwise ) {
			bug("RpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		if (bank != NULL)
		{
			for(int i = 0; bank[i]; i++)
				ptr[i] ^= temp_flags[i];
		}
		else
			*ptr ^= value;

		break;
	default:
		return;
	}

	if(hasmin && *ptr < min)
		*ptr = min;

	if(hasmax && *ptr > max)
		*ptr = max;

	if(dirty_stat >= 0 && dirty_stat < MAX_STATS)
		mob->dirty_stat[dirty_stat] = true;
}


SCRIPT_CMD(do_rpstringmob)
{
	char buf[2*MIL+2],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	CHAR_DATA *mob = NULL;

	bool newlines = false;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpStringMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpStringMob - NULL mobile.", 0);
		return;
	}

	if(!IS_NPC(mob)) {
		bug("RpStringMob - can't change strings on PCs.", 0);
		return;
	}

	if(!*rest) {
		bug("RpStringMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpStringMob - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		if(!str_cmp(field,"name"))				str = (char**)&mob->name;
		else if(!str_cmp(field,"owner"))		{ str = (char**)&mob->owner; min_sec = 5; }
		else if(!str_cmp(field,"short"))		str = (char**)&mob->short_descr;
		else if(!str_cmp(field,"long"))			{ str = (char**)&mob->long_descr; strcat(buf,"\n\r"); newlines = true; }
		else if(!str_cmp(field,"full"))			{ str = (char**)&mob->description; newlines = true; }
		else if(!str_cmp(field,"tempstring"))	str = (char**)&mob->tempstring;
		else
		{
			free_buf(buffer);
			return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"RpStringMob - Attempting to restring '%s' with security %d.\n\r", field, script_security);
			bug(buf, 0);
			free_buf(buffer);
			return;
		}

		strip_newline(buffer->string, newlines);

		free_string(*str);
		*str = str_dup(buffer->string);
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_rpskimprove)
{
	char name[MIL],*rest;
	int min_diff, diff;
	SKILL_DATA *skill = NULL;
	CHAR_DATA *mob = NULL;
	bool success = false;

	if(script_security < MIN_SCRIPT_SECURITY) {
		bug("RpSkImprove - Insufficient security.",0);
		return;
	}

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpSkImprove - NULL target.", 0);
		return;
	}

	if(mob) {
		if(IS_NPC(mob)) {
			bug("RpSkImprove - NPCs don't have skills to improve yet...", 0);
			return;
		}


		if(!(rest = expand_argument(info,rest,arg))) {
			bug("RpSkImprove - Error in parsing.",0);
			return;
		}

		name[0] = 0;

		switch(arg->type) {
		case ENT_STRING: strncpy(name,arg->d.str,MIL-1); break;
		default: return;
		}

		if(!name[0]) return;

		skill = get_skill_data(name);

		if(!IS_VALID(skill)) return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: diff = arg->d.num; break;
	default: return;
	}

	min_diff = 10 - script_security;	// min=10, max=1

	if(diff < min_diff) {
		bug("RpSkImprove - Attempting to use a difficulty multiplier lower than allowed.",0);
		diff = min_diff;
	}

	switch(arg->type) {
	case ENT_NONE: success = true; break;
	case ENT_STRING:
		if(is_number(arg->d.str))
			success = (bool)(atoi(arg->d.str) != 0);
		else
			success = !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"true") ||
				!str_cmp(arg->d.str,"success") || !str_cmp(arg->d.str,"pass");
		break;
	case ENT_NUMBER:
		success = (bool)(arg->d.num != 0);
		break;
	default: success = false;
	}

	check_improve( mob, skill, success, diff );
}


SCRIPT_CMD(do_rprawkill)
{
	char *rest;
	int type;
	bool has_head, show_msg;
	CHAR_DATA *mob = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpRawkill - NULL mobile.", 0);
		return;
	}

	if(IS_IMMORTAL(mob)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
	default: return;
	}

	if(type < 0 || type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:	has_head = true; break;
	case ENT_STRING:
		has_head = !str_cmp(arg->d.str,"true") ||
			!str_cmp(arg->d.str,"yes") ||
			!str_cmp(arg->d.str,"head");
		break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:	show_msg = true; break;
	case ENT_STRING:
		show_msg = !str_cmp(arg->d.str,"true") ||
			!str_cmp(arg->d.str,"yes");
		break;
	default: return;
	}

	{
		ROOM_INDEX_DATA *here = mob->in_room;
		mob->position = POS_STANDING;
		if(!p_percent_trigger(mob, NULL, NULL, NULL, mob, mob, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0))
			p_percent_trigger(NULL, NULL, here, NULL, mob, mob, NULL, NULL, NULL, TRIG_DEATH, NULL,0,0,0,0,0);
	}

	raw_kill(mob, has_head, show_msg, type);
}

SCRIPT_CMD(do_rpaddaffect)
{
	char *rest;
	int where, group, level, loc, mod, hours;
	SKILL_DATA *skill = NULL;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffect <target> <where> <skill> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, info->room, arg->d.str)))
			obj = get_obj_here(NULL, info->room, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("RpAddaffect - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
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

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: skill = get_skill_data(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_find(arg->d.str,apply_flags); break;
	default: return;
	}

	if(loc == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;

	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("MpAddaffect - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group = group;
	af.where = where;
	af.catalyst_type = -1;
	af.skill = skill;
	af.location = loc;
	af.modifier = mod;
	af.level = level;
	af.duration = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = NULL;
	af.slot = wear_loc;
	if(mob) affect_join(mob, &af);
	else affect_join_obj(obj,&af);
}

SCRIPT_CMD(do_rpaddaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffectname <target> <where> <name> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,info->room, arg->d.str)))
			obj = get_obj_here(NULL,info->room, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("MpAddaffect - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
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

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}



	switch(arg->type) {
	case ENT_STRING: name = create_affect_cname(arg->d.str); break;
	default: return;
	}

	if(!name) {
		bug("MpAddaffect - Error allocating affect name.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: level = arg->d.num; break;
	case ENT_STRING: level = atoi(arg->d.str); break;
	case ENT_MOBILE: level = arg->d.mob->tot_level; break;
	case ENT_OBJECT: level = arg->d.obj->level; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_find(arg->d.str,apply_flags); break;
	default: return;
	}

	if(loc == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
	default: return;
	}

	if(bv2 == NO_FLAG) bv2 = 0;

	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("MpAddaffect - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_OBJECT: wear_loc = arg->d.obj ? arg->d.obj->wear_loc : WEAR_NONE; break;
		default: return;
		}
	}

	af.group	= group;
	af.where     = where;
	af.catalyst_type      = -1;
	af.skill = NULL;
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



SCRIPT_CMD(do_rpstripaffect)
{
	char *rest;
	SKILL_DATA *skill = NULL;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffect <target> <skill>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, info->room, arg->d.str)))
			obj = get_obj_here(NULL, info->room, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("RpStripaffect - NULL target.", 0);
		return;
	}


	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpStripaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: skill = get_skill_data(arg->d.str); break;
	default: return;
	}

	if(!IS_VALID(skill)) return;

	if(mob) affect_strip(mob, skill);
	else affect_strip_obj(obj,skill);
}

SCRIPT_CMD(do_rpstripaffectname)
{
	char *rest, *name;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffectname <target> <name>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,info->room, arg->d.str)))
			obj = get_obj_here(NULL,info->room, arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("MpStripaffect - NULL target.", 0);
		return;
	}


	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: name = get_affect_cname(arg->d.str); break;
	default: return;
	}

	if(!name) return;

	if(mob) affect_strip_name(mob, name);
	else affect_strip_name_obj(obj, name);
}


SCRIPT_CMD(do_rpinput)
{
	char *rest, *p;
	WNUM wnum;
	CHAR_DATA *mob = NULL;


	if(!info || !info->room) return;

	info->room->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpInput - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM) {
		bug("RpInput - Error in parsing.",0);
		return;
	}

	wnum = arg->d.wnum;

	if(!wnum.pArea || wnum.vnum < 1 || !get_script_index(wnum.pArea, wnum.vnum, PRG_RPROG)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpInput - Error in parsing.",0);
		return;
	}


	switch(arg->type) {
	case ENT_NONE:		p = NULL; break;
	case ENT_STRING:	p = arg->d.str; break;
	default: return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	mob->desc->input = true;
	mob->desc->input_var = p ? str_dup(p) : NULL;
	mob->desc->input_prompt = str_dup(buffer->string[0] ? buffer->string : " >");
	mob->desc->input_script = wnum;
	mob->desc->input_mob = NULL;
	mob->desc->input_obj = NULL;
	mob->desc->input_room = info->room;
	mob->desc->input_tok = NULL;

	info->room->progs->lastreturn = 1;

	free_buf(buffer);
}

SCRIPT_CMD(do_rpusecatalyst)
{
	char *rest;
	int type, method, amount, show;
	CHAR_DATA *mob = NULL;
	ROOM_INDEX_DATA *room = NULL;


	if(!info || !info->room) return;

	info->room->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpUseCatalyst - Error in parsing.",0);
		return;
	}

	// usecatalyst <target> <type> <method> <amount> <min> <max> <show>

	switch(arg->type) {
	case ENT_STRING: mob = get_char_room(NULL,info->room, arg->d.str); break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	default: break;
	}

	if(!mob && !room) {
		bug("RpUseCatalyst - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
	default: return;
	}

	if(type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
	default: return;
	}

	if(method == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: amount = arg->d.num; break;
	case ENT_STRING: amount = atoi(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: show = flag_value(boolean_types,arg->d.str); break;
	default: return;
	}

	if(show == NO_FLAG) return;

	info->room->progs->lastreturn = use_catalyst(mob,room,type,method,amount,(bool)show);
}

SCRIPT_CMD(do_rpalterexit)
{
	char buf[MSL+2],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY, door;
	ROOM_INDEX_DATA *room;
	EXIT_DATA *ex = NULL;
	int *ptr = NULL;
	int16_t *sptr = NULL;
	char **str;
	int min = 0, max = 0;
	bool hasmin = false, hasmax = false;
	bool allowarith = true;
	bool check_material = false;
	const struct flag_type *flags = NULL;

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterExit - Error in parsing.",0);
		return;
	}

	room = info->room;

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
			bug("RpAlterExit - Error in parsing.",0);
			return;
		}
	case ENT_STRING:
		door = get_num_dir(arg->d.str);
		ex = (door < 0) ? NULL : room->exit[door];
		break;
	case ENT_EXIT:
		ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
		break;
	default: ex = NULL; break;
	}

	if(!ex) return;

	if(!*rest) {
		bug("RpAlterExit - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterExit - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

	if(!str_cmp(field,"room") || !str_prefix(field,"destination")) {
		if(!(rest = expand_argument(info,rest,arg))) {
			bug("RpAlterExit - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_WIDEVNUM:	room = get_room_index_wnum(arg->d.wnum); break;
		case ENT_ROOM:		room = arg->d.room; break;
		case ENT_MOBILE:	room = arg->d.mob->in_room; break;
		case ENT_OBJECT:	room = obj_room(arg->d.obj); break;
		case ENT_EXIT:		room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? arg->d.door.r->exit[arg->d.door.door]->u1.to_room : NULL; break;
		default: return;
		}

		if(!room) return;

		ex->u1.to_room = room;
		return;
	}

	str = NULL;
	if(!str_cmp(field,"keyword"))		str = &ex->keyword;
	else if(!str_cmp(field,"long"))		str = &ex->long_desc;
	else if(!str_cmp(field,"material"))	check_material = true;
	else if(!str_cmp(field,"short"))	str = &ex->short_desc;

	if (check_material)
	{
		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		MATERIAL *material = material_lookup(buf_string(buffer));

		if(!IS_VALID(material)) {
			scriptcmd_bug(info, "RpAlterExit - Invalid material.");
			free_buf(buffer);
			return;
		}

		ex->door.material = material;
		free_buf(buffer);
		return;
	}

	if(str) {
		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if( buffer->string[0] != '\0' )
		{
			free_string(*str);
			*str = str_dup(buffer->string);
		}

		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterExit - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: value = arg->d.num; break;
	default: return;
	}

	if(!str_cmp(field,"flags"))					{ ptr = (int*)&ex->exit_info; flags = exit_flags; }
	else if(!str_cmp(field,"resets"))			{ ptr = (int*)&ex->rs_flags; flags = exit_flags; min_sec = 7; }
	else if(!str_cmp(field,"strength"))			sptr = (int16_t*)&ex->door.strength;

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"RpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		bug(buf, 0);
		return;
	}

	if( flags != NULL )
	{
		if( arg->type != ENT_STRING || !ptr ) return;

		allowarith = false;	// This is a bit vector, no arithmetic operators.
		value = script_flag_value(flags, arg->d.str);

		if( value == NO_FLAG ) value = 0;

		if( flags == exit_flags )
		{
			// Scripted exits cannot change NOUNLINK|PREVFLOOR|NEXTFLOOR
			REMOVE_BIT(value, (EX_NOUNLINK|EX_PREVFLOOR|EX_NEXTFLOOR));

			if( (buf[0] == '=') || (buf[0] == '&') )
			{
				value |= (*ptr & (EX_NOUNLINK|EX_PREVFLOOR|EX_NEXTFLOOR));
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


	if(ptr) {
		switch (buf[0]) {
		case '+':
			if( !allowarith ) {
				bug("RpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr += value;
			break;
		case '-':
			if( !allowarith ) {
				bug("RpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr -= value;
			break;
		case '*':
			if( !allowarith ) {
				bug("RpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr *= value;
			break;
		case '/':
			if( !allowarith ) {
				bug("RpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("RpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("RpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("RpAlterExit - adjust called with operator % and value 0", 0);
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
	} else {
		switch (buf[0]) {
		case '+': *sptr += value; break;
		case '-': *sptr -= value; break;
		case '*': *sptr *= value; break;
		case '/':
			if (!value) {
				bug("RpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("RpAlterExit - adjust called with operator % and value 0", 0);
				return;
			}
			*sptr %= value;
			break;

		case '=': *sptr = value; break;
		case '&': *sptr &= value; break;
		case '|': *sptr |= value; break;
		case '!': *sptr &= ~value; break;
		case '^': *sptr ^= value; break;
		default:
			return;
		}
	}

	if( ptr )
	{
		if(hasmin && *ptr < min)
			*ptr = min;

		if(hasmax && *ptr > max)
			*ptr = max;
	}
}


// SYNTAX: room prompt <player> <name>[ <string>]
SCRIPT_CMD(do_rpprompt)
{
	char name[MIL],*rest;
	CHAR_DATA *mob = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpPrompt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,info->room, arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("RpPrompt - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob)) {
		bug("RpPrompt - cannot set prompt strings on NPCs.", 0);
		return;
	}

	if(!*rest) {
		bug("RpPrompt - Missing name type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpPrompt - Error in parsing.",0);
		return;
	}

	name[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(name,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!name[0]) return;

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		string_vector_set(&mob->pcdata->script_prompts,name,buffer->string);
	}
	free_buf(buffer);
}




SCRIPT_CMD(do_rpvarseton)
{

	VARIABLE **vars;

	if(!info || !info->room) return;

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

SCRIPT_CMD(do_rpvarclearon)
{

	VARIABLE **vars;

	if(!info || !info->room) return;

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

SCRIPT_CMD(do_rpvarsaveon)
{
	char name[MIL],buf[MIL];
	bool on;

	VARIABLE *vars;

	if(!info || !info->room) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? arg->d.token->progs->vars : NULL; break;
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

// cloneroom <vnum> <environment> <var>
SCRIPT_CMD(do_rpcloneroom)
{
	char name[MIL];
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	TOKEN_DATA *tok;
	ROOM_INDEX_DATA *source, *room, *clone;
	bool no_env = false;

	if(!info || !info->room) return;

	info->progs->lastreturn = 0;

	// Get vnum
	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_WIDEVNUM)
		return;

	source = get_room_index_wnum(arg->d.wnum);
	if(!source) return;

	if( IS_SET(source->room_flag[1], ROOM_NOCLONE) )
		return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE:	mob = arg->d.mob; obj = NULL; room = NULL; tok = NULL; break;
	case ENT_OBJECT:	mob = NULL; obj = arg->d.obj; room = NULL; tok = NULL; break;
	case ENT_ROOM:		mob = NULL; obj = NULL; room = arg->d.room; tok = NULL; break;
	case ENT_TOKEN:		mob = NULL; obj = NULL; room = NULL; tok = arg->d.token; break;
	case ENT_STRING:
		mob = NULL;
		obj = NULL;
		room = NULL;
		tok = NULL;
		if(!str_cmp(arg->d.str, "none"))
			no_env = true;
		break;
	default: return;
	}

	if(!mob && !obj && !room && !tok && !no_env) return;

	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_STRING || !arg->d.str || !arg->d.str[0])
		return;

	strncpy(name,arg->d.str,MIL); name[MIL] = 0;

	clone = create_virtual_room(source,false,false);
	if(!clone) return;

	if(!no_env)
		room_to_environment(clone,mob,obj,room,tok);

	variables_set_room(info->var,name,clone);

	info->progs->lastreturn = 1;
}

// alterroom <room> <field> <parameters>
SCRIPT_CMD(do_rpalterroom)
{
	char buf[MSL+2],field[MIL],*rest;
	int value = 0, min_sec = MIN_SCRIPT_SECURITY;
	ROOM_INDEX_DATA *room;
	WILDS_DATA *wilds;

	int *ptr = NULL;
	int16_t *sptr = NULL;
	char **str;
	bool allow_empty = false;
	bool allowarith = true;
	bool allowbitwise = true;
	bool check_sector = false;
	const struct flag_type *flags = NULL;
	const struct flag_type **bank = NULL;
	long temp_flags[4];

	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterRoom - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		break;
	case ENT_WIDEVNUM:
		room = get_room_index_wnum(arg->d.wnum);
		break;
	default: room = NULL; break;
	}

	if(!room || !room_is_clone(room)) return;

	if(!*rest) {
		bug("RpAlterRoom - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterRoom - Error in parsing.",0);
		return;
	}

	field[0] = 0;

        if(!str_cmp(field,"mapid")) {
                if(!(rest = expand_argument(info,rest,arg))) {
                        bug("MPAlterRoom - Error in parsing.",0);
                        return;
                }
                switch(arg->type) {
                case ENT_STRING:
                        if(!str_cmp(arg->d.str,"none"))
                                { room->viewwilds = NULL; }
                        break;
                case ENT_NUMBER:
                        wilds = get_wilds_from_uid(NULL,arg->d.num);
                        if(!wilds){
                                bug("Not a valid wilds uid",0);
                                return;
                        }
                        room->viewwilds=wilds;
                        break;
                default: return;
                }
        }

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

	// Setting the environment of a clone room
	if(!str_cmp(field,"environment") || !str_cmp(field,"environ") ||
		!str_cmp(field,"extern") || !str_cmp(field,"outside")) {

		if(!(rest = expand_argument(info,rest,arg))) {
			bug("RpAlterRoom - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_ROOM:
			room_from_environment(room);
			room_to_environment(room,NULL,NULL,arg->d.room, NULL);
			break;
		case ENT_MOBILE:
			room_from_environment(room);
			room_to_environment(room,arg->d.mob,NULL,NULL, NULL);
			break;
		case ENT_OBJECT:
			room_from_environment(room);
			room_to_environment(room,NULL,arg->d.obj,NULL, NULL);
			break;
		case ENT_TOKEN:
			room_from_environment(room);
			room_to_environment(room,NULL,NULL,NULL,arg->d.token);
			break;
		case ENT_STRING:
			if(!str_cmp(arg->d.str, "none"))
				room_from_environment(room);
		default: return;
		}
	}

	str = NULL;
	if(!str_cmp(field,"name"))		str = &room->name;
	else if(!str_cmp(field,"desc"))		{ str = &room->description; allow_empty = true; }
	else if(!str_cmp(field,"owner"))	{ str = &room->owner; allow_empty = true; min_sec = 9; }

	if(str) {
		if(script_security < min_sec) {
			sprintf(buf,"RpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
			bug(buf, 0);
			return;
		}

		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if(allow_empty || buffer->string[0] != '\0')
		{
			free_string(*str);
			*str = str_dup(buffer->string);
		}

		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpAlterRoom - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"flags"))			{ ptr = (int*)room->room_flag; bank = room_flagbank; }
	else if(!str_cmp(field,"light"))	ptr = (int*)&room->light;
	else if(!str_cmp(field,"sector"))	check_sector = true;
	else if(!str_cmp(field,"sectorflags"))	{ ptr = (int*)&room->sector_flags; flags = sector_flags; allowarith = false; }
	else if(!str_cmp(field,"heal"))		{ ptr = (int*)&room->heal_rate; min_sec = 9; }
	else if(!str_cmp(field,"mana"))		{ ptr = (int*)&room->mana_rate; min_sec = 9; }
	else if(!str_cmp(field,"move"))		{ ptr = (int*)&room->move_rate; min_sec = 1; }
	else if(!str_cmp(field,"mapx"))		{ ptr = (int*)&room->x; min_sec = 5;}
	else if(!str_cmp(field,"mapy"))		{ ptr = (int*)&room->y; min_sec = 5;}
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&room->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&room->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&room->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&room->tempstore[3];
	else if(!str_cmp(field,"tempstore5"))	ptr = (int*)&room->tempstore[4];

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"RpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		bug(buf, 0);
		return;
	}

	if (check_sector)
	{
		if (arg->type != ENT_STRING ) return;

		SECTOR_DATA *sector = get_sector_data(arg->d.str);
		if (!sector)
		{
			return;
		}

		room->sector = sector;
		room->sector_flags = sector->flags;
		return;
	}

	memset(temp_flags, 0, sizeof(temp_flags));

	if( bank != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = false;	// This is a bit vector, no arithmetic operators.
		if (!script_bitmatrix_lookup(arg->d.str, bank, temp_flags))
			return;

		if (bank == room_flagbank)
		{
			REMOVE_BIT(temp_flags[1], ROOM_NOCLONE);
			REMOVE_BIT(temp_flags[1], ROOM_VIRTUAL_ROOM);
			REMOVE_BIT(temp_flags[1], ROOM_BLUEPRINT);

			if( buf[0] == '=' || buf[0] == '&' )
			{
				if( IS_SET(ptr[1], ROOM_NOCLONE) ) SET_BIT(temp_flags[1], ROOM_NOCLONE);
				if( IS_SET(ptr[1], ROOM_VIRTUAL_ROOM) ) SET_BIT(temp_flags[1], ROOM_VIRTUAL_ROOM);
				if( IS_SET(ptr[1], ROOM_BLUEPRINT) ) SET_BIT(temp_flags[1], ROOM_BLUEPRINT);
			}

		}		
	}
	else if( flags != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = false;	// This is a bit vector, no arithmetic operators.
		value = script_flag_value(flags, arg->d.str);

		if( value == NO_FLAG ) value = 0;

		// No special filtering
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

	if(ptr) {
		switch (buf[0]) {
		case '+':
			if( !allowarith ) {
				bug("RpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value; break;
		case '-':
			if( !allowarith ) {
				bug("RpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value; break;
		case '*':
			if( !allowarith ) {
				bug("RpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value; break;
		case '/':
			if( !allowarith ) {
				bug("RpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("RpAlterRoom - alterroom called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("RpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("RpAlterRoom - alterroom called with operator % and value 0", 0);
				return;
			}
			*ptr %= value;
			break;

		case '=':
			if (bank != NULL)
			{
				for(int i = 0; bank[i]; i++)
					ptr[i] = temp_flags[i];
			}
			else
				*ptr = value;
			break;

		case '&':
			if( !allowbitwise ) {
				bug("RpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
				return;
			}

			if (bank != NULL)
			{
				for(int i = 0; bank[i]; i++)
					ptr[i] &= temp_flags[i];
			}
			else
				*ptr &= value;
			break;
		case '|':
			if( !allowbitwise ) {
				bug("RpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
				return;
			}

			if (bank != NULL)
			{
				for(int i = 0; bank[i]; i++)
					ptr[i] |= temp_flags[i];
			}
			else
				*ptr |= value;
			break;
		case '!':
			if( !allowbitwise ) {
				bug("RpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
				return;
			}

			if (bank != NULL)
			{
				for(int i = 0; bank[i]; i++)
					ptr[i] &= ~temp_flags[i];
			}
			else
				*ptr &= ~value;
			break;
		case '^':
			if( !allowbitwise ) {
				bug("RpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
				return;
			}

			if (bank != NULL)
			{
				for(int i = 0; bank[i]; i++)
					ptr[i] ^= temp_flags[i];
			}
			else
				*ptr ^= value;

			break;
		default:
			return;
		}
	} else {
		switch (buf[0]) {
		case '+': *sptr += value; break;
		case '-': *sptr -= value; break;
		case '*': *sptr *= value; break;
		case '/':
			if (!value) {
				bug("RpAlterRoom - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("RpAlterRoom - adjust called with operator % and value 0", 0);
				return;
			}
			*sptr %= value;
			break;

		case '=': *sptr = value; break;
		case '&': *sptr &= value; break;
		case '|': *sptr |= value; break;
		case '!': *sptr &= ~value; break;
		case '^': *sptr ^= value; break;
		default:
			return;
		}
	}
}

// destroyroom <vnum> <id> <id>
// destroyroom <room>
SCRIPT_CMD(do_rpdestroyroom)
{
	unsigned long id1, id2;
	ROOM_INDEX_DATA *room;


	if(!info || !info->room) return;

	info->room->progs->lastreturn = 0;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	// It's a room, extract it directly
	if(arg->type == ENT_ROOM) {
		// Need to block this when done by room to itself
		if(extract_clone_room(arg->d.room->source,arg->d.room->id[0],arg->d.room->id[1],false))
			info->room->progs->lastreturn = 1;
		return;
	}

	if(arg->type != ENT_WIDEVNUM) return;

	room = get_room_index_wnum(arg->d.wnum);
	if(!room) return;

	// Get id
	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	id1 = arg->d.num;

	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	id2 = arg->d.num;

	if(extract_clone_room(room, id1, id2,false))
		info->room->progs->lastreturn = 1;
}

// showroom <viewer> map <mapid> <x> <y> <z> <scale> <width> <height>[ force]
// showroom <viewer> room <room>[ force]
// showroom <viewer> vroom <room> <id>[ force]
SCRIPT_CMD(do_rpshowroom)
{
	CHAR_DATA *viewer = NULL, *next;
	ROOM_INDEX_DATA *room = NULL, *dest;
	WILDS_DATA *wilds = NULL;

	long mapid;
	long x,y;
	long width, height;
	bool force;

	if(!info || !info->room) return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE:	viewer = arg->d.mob; break;
	case ENT_ROOM:		room = arg->d.room; break;
	}

	if(!viewer && !room) {
		bug("RpShowMap - bad target for showing the map", 0);
		return;
	}

	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_STRING)
		return;

	if(!str_cmp(arg->d.str,"map")) {
		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		mapid = arg->d.num;

		wilds = get_wilds_from_uid(NULL,mapid);
		if(!wilds) return;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		x = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		y = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		//z = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		//scale = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		width = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		height = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)))
			return;

		if(arg->type == ENT_STRING)
			force = !str_cmp(arg->d.str,"force");
		else
			force = false;

		dest = get_wilds_vroom(wilds,x,y);
		if(!dest)
			dest = create_wilds_vroom(wilds,x,y);

		// Force limitations please?
		if(width < 5) width = 5;
		if(height < 5) height = 5;

		if(room) {
			for(viewer = room->people; viewer; viewer = next) {
				next = viewer->next_in_room;
				if(!IS_NPC(viewer) && (force || (IS_AWAKE(viewer) && check_vision(viewer,dest,false,false)))) {
					show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, false);
				}
			}
		} else if(!IS_NPC(viewer)) {
			// There is no awake check here since it is to one mob.
			//  This can be used in things like DREAMS, seeing yourself at a certain location!
			show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, false);
		}
		return;
	}

	// Both room and vroom have the same end mechanism, just different fetching mechanisms

	if(!str_cmp(arg->d.str,"room")) {
		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_ROOM)
			return;

		dest = arg->d.room;
	} else if(!str_cmp(arg->d.str,"vroom")) {
		unsigned long id1, id2;
		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_ROOM)
			return;

		dest = arg->d.room;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		id1 = arg->d.num;

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		id2 = arg->d.num;

		dest = get_clone_room(dest,id1,id2);
	} else
		return;

	if(!dest) return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	if(arg->type == ENT_STRING)
		force = !str_cmp(arg->d.str,"force");
	else
		force = false;

	if(room) {
		for(viewer = room->people; viewer; viewer = next) {
			next = viewer->next_in_room;
			if(!IS_NPC(viewer) && (force || (IS_AWAKE(viewer) && check_vision(viewer,dest,false,false))))
				show_room(viewer,dest,true,true,false);
		}
	} else if(!IS_NPC(viewer)) {
		// There is no awake check or vision check here since it is to one mob.
		//  This can be used in things like DREAMS, seeing yourself at a certain location!
		show_room(viewer,dest,true,true,false);
	}
}

SCRIPT_CMD(do_rpxcall)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;
	AREA_DATA *area = NULL;
	INSTANCE *instance = NULL;
	DUNGEON *dungeon = NULL;
	CHAR_DATA *vch,*ch;
	OBJ_DATA *obj1,*obj2;
	SCRIPT_DATA *script;
	int depth, ret, space = PRG_MPROG;
	WNUM wnum;

	if(!info || !info->room) return;

	if (!argument[0]) {
		bug("RpCall: missing arguments from vnum %d.", (int)info->room->vnum);
		return;
	}

	if(script_security < 5) {
		bug("RpCall: Minimum security needed is 5.", info->room->vnum);
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("RpCall: maximum call depth exceeded for vnum %d.", (int)info->room->vnum);
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; space = PRG_MPROG; break;
	case ENT_OBJECT: obj = arg->d.obj; space = PRG_OPROG; break;
	case ENT_ROOM: room = arg->d.room; space = PRG_RPROG; break;
	case ENT_TOKEN: token = arg->d.token; space = PRG_TPROG; break;
	case ENT_AREA: area = arg->d.area; space = PRG_APROG; break;
	case ENT_INSTANCE: instance = arg->d.instance; space = PRG_IPROG; break;
	case ENT_DUNGEON: dungeon = arg->d.dungeon; space = PRG_DPROG; break;
	}

	if(!mob && !obj && !room && !token && !area && !instance && !dungeon) {
		bug("RpCall: No entity target from vnum %ld.", info->room->vnum);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(mob && !IS_NPC(mob)) {
		bug("RpCall: Invalid target for xcall.  Players cannot do scripts.", 0);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_WIDEVNUM) {
		bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	wnum = arg->d.wnum;
	if (!wnum.pArea || wnum.vnum < 1 || !(script = get_script_index(wnum.pArea, wnum.vnum, space))) {
		bug("RpCall: invalid prog from vnum %d.", info->room->vnum);
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, info->room, arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, info->room,arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, info->room, arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpCall: Error in parsing from vnum %ld.", info->room->vnum);
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, info->room, arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	ret = execute_script(script, mob, obj, room, token, area, instance, dungeon, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
	if(info->room) {
		info->room->progs->lastreturn = ret;
		DBG3MSG1("lastreturn = %d\n", info->room->progs->lastreturn);
	} else
 		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}


// do_rpchargebank
// obj chargebank <player> <gold>
SCRIPT_CMD(do_rpchargebank)
{
	char *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpChargeBank - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("RpChargeBank - Non-player victim from vnum %ld.", info->room->vnum);
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("RpChargeBank - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1 || amount > victim->pcdata->bankbalance) return;

	victim->pcdata->bankbalance -= amount;
}

// do_rpwiretransfer
// obj wiretransfer <player> <gold>
// Limited to 1000 gold for security scopes less than 7.
SCRIPT_CMD(do_rpwiretransfer)
{
	char buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpWireTransfer - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, info->room, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("RpWireTransfer - Non-player victim from vnum %ld.", info->room->vnum);
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("RpWireTransfer - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	// If the security on this script execution is
	if(script_security < 7 && amount > 1000) {
		sprintf(buf, "RpWireTransfer logged: attempted to wire %d gold to %s by room %ld", amount, victim->name, info->room->vnum);
		log_string(buf);
		amount = 1000;
	}

	victim->pcdata->bankbalance += amount;

	sprintf(buf, "RpWireTransfer logged: %s was wired %d gold by room %ld", victim->name, amount, info->room->vnum);
	log_string(buf);
}

// do_rpsetrecall
// obj setrecall $MOBILE <location>
// Sets the recall point of the target mobile to the reference of the location
SCRIPT_CMD(do_rpsetrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpSetRecall - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("RpSetRecall - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	argument = rp_getlocation(info, rest, &location);

	if(!location) {
		bug("RpSetRecall - Bad location from vnum %d.", info->room->vnum);
		return;
	}

	if(location->wilds) {
		victim->recall.wuid = location->wilds->uid;
		victim->recall.id[0] = location->x;
		victim->recall.id[1] = location->y;
		victim->recall.id[2] = location->z;
	} else {
		victim->recall.wuid = 0;
		victim->recall.id[0] = location->vnum;
		if(location->source) {
			victim->recall.id[1] = location->id[0];
			victim->recall.id[2] = location->id[1];
		} else {
			victim->recall.id[1] = 0;
			victim->recall.id[2] = 0;
		}
	}
}

// do_rpclearrecall
// obj clearrecall $MOBILE
// Clears the special recall field on the $MOBILE
SCRIPT_CMD(do_rpclearrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
//	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpClearRecall - Bad syntax from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("RpClearRecall - Null victim from vnum %ld.", info->room->vnum);
		return;
	}

	victim->recall.wuid = 0;
	victim->recall.id[0] = 0;
	victim->recall.id[1] = 0;
	victim->recall.id[2] = 0;
}

// HUNT <HUNTER> <PREY>
SCRIPT_CMD(do_rphunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	CHAR_DATA *prey = NULL;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpHunt - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("RpHunt - Null hunter from vnum %ld.", info->room->vnum);
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("RpHunt - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: prey = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: prey = arg->d.mob; break;
	default: prey = NULL; break;
	}

	if (!prey) {
		bug("RpHunt - Null prey from vnum %ld.", info->room->vnum);
		return;
	}

	hunt_char(hunter, prey);
	return;
}

// STOPHUNT <STAY> <HUNTER>
SCRIPT_CMD(do_rpstophunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	bool stay;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING) {
		bug("RpStopHunt - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

	if(!expand_argument(info,rest,arg)) {
		bug("RpStopHunt - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("RpStopHunt - Null hunter from vnum %ld.", info->room->vnum);
		return;
	}

	stop_hunt(hunter, stay);
	return;
}

// Format: PERSIST <MOBILE or OBJECT or ROOM> <STATE>
SCRIPT_CMD(do_rppersist)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	bool persist = false, current = false;


	if(!info || !info->room) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("RpPersist - Error in parsing from vnum %ld.", info->room->vnum);
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; current = mob->persist; break;
	case ENT_OBJECT: obj = arg->d.obj; current = obj->persist; break;
	case ENT_ROOM: room = arg->d.room; current = room->persist; break;
	}

	if(!mob && !obj && !room) {
		bug("RpPersist - NULL target.", info->room->vnum);
		return;
	}

	if(mob)
	{
		if( !IS_NPC(mob) ) return;

		if( IS_SET(mob->act[1], ACT2_INSTANCE_MOB) ) return;
	}

	if(obj)
	{
		if( IS_SET(obj->extra[2], ITEM_INSTANCE_OBJ) ) return;
	}

	if( room )
	{
		if( get_blueprint_section_byroom(room->area, room->vnum) ) return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpPersist - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:   persist = !current; break;
	case ENT_STRING: persist = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"on"); break;
	default: return;
	}

	// Require security to ENABLE persistance
	if(!current && persist && script_security < MAX_SCRIPT_SECURITY) {
		bug("RpPersist - Insufficient security to enable persistance.", info->room->vnum);
		return;
	}

	if(mob) {
		if(persist)
			persist_addmobile(mob);
		else
			persist_removemobile(mob);
	} else if(obj) {
		if(persist)
			persist_addobject(obj);
		else
			persist_removeobject(obj);
	} else if(room) {
		if(persist)
			persist_addroom(room);
		else
			persist_removeroom(room);
	}

	return;
}

#if 0
// room skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_rpskill)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int sn, value;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if ( script_security < 9 ) return;

	if(!(rest = expand_argument(info,argument,arg))) return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob || IS_NPC(mob) ) return;	// only players for now

	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg))) return;

	if(arg->type != ENT_STRING) return;

	sn = skill_lookup(arg->d.str);

	if( sn < 1 || sn >= MAX_SKILL ) return;

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) return;

	switch(arg->type) {
	case ENT_STRING:
		if( is_number(arg->d.str ))
			value = atoi(arg->d.str);
		else
			return;
		break;
	case ENT_NUMBER: value = arg->d.num; break;
	default: return;
	}

	switch(buf[0])
	{
		case '=':	// Set skill
			if( value < 0 ) value = 0;
			else if( value > 100 ) value = 100;

			mob->pcdata->learned[sn] = value;
			break;

		case '+':
			// Can only modify the skill, you cannot grant a skill using this.  Use the = operator.
			if(mob->pcdata->learned[sn] > 0 )
			{
				value = mob->pcdata->learned[sn] + value;

				if( value < 1 ) value = 1;
				else if( value > 100 ) value = 100;

				mob->pcdata->learned[sn] = value;
			}
			break;

		case '-':
			// Can only modify the skill, you cannot remove it using this.  Use the = operator.
			if(mob->pcdata->learned[sn] > 0 )
			{
				value = mob->pcdata->learned[sn] - value;

				if( value < 1 ) value = 1;
				else if( value > 100 ) value = 100;

				mob->pcdata->learned[sn] = value;
			}
			break;

		default:
			return;
	}

	return;
}


// room skillgroup <player> add|remove <group>
SCRIPT_CMD(do_rpskillgroup)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	bool fAdd = false;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if ( script_security < 9 ) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob || IS_NPC(mob) ) return;	// only players for now

	if( !*rest) return;

	argument = one_argument(rest,buf);

	if( !str_cmp(buf, "add") )
		fAdd = true;
	else if(!str_cmp(buf, "remove"))
		fAdd = false;
	else
		return;

	if(!(rest = expand_argument(info,argument,arg))) return;

	if(arg->type != ENT_STRING) return;

	SKILL_GROUP *group = group_lookup(arg->d.str);
	if( IS_VALID(group) )
	{
		if( fAdd )
		{
			if (!list_hasdata(mob->pcdata->group_known, group))
				gn_add(mob,group);
		}
		else
		{
			if (list_hasdata(mob->pcdata->group_known, group))
				gn_remove(mob,group);
		}
	}

	return;
}
#endif

// room condition $PLAYER <condition> <value>
// Adjusts the specified condition by the given value
SCRIPT_CMD(do_rpcondition)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int cond, value;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob || IS_NPC(mob) ) return;	// only players for now

	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		if( !str_cmp(arg->d.str,"drunk") )		cond = COND_DRUNK;
		else if( !str_cmp(arg->d.str,"full") )	cond = COND_FULL;
		else if( !str_cmp(arg->d.str,"thirst") )	cond = COND_THIRST;
		else if( !str_cmp(arg->d.str,"hunger") )	cond = COND_HUNGER;
		else if( !str_cmp(arg->d.str,"stoned") )	cond = COND_STONED;
		else
			return;

		break;
	default: return;
	}

	if(!*rest) return;
	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: value = arg->d.num; break;
	default: return;
	}

	if( script_security < 9 )
	{
		if( value < -1 ) value = -1;
		else if(value > 48) value = 48;
	}

	gain_condition(mob, cond, value);
}


// addspell $OBJECT STRING[ NUMBER]
SCRIPT_CMD(do_rpaddspell)
{
	// TODO: Fix to require type contexts
#if 0
	char *rest;
	SPELL_DATA *spell, *spell_new;
	OBJ_DATA *target;
	int level;
	int sn;
	AFFECT_DATA *paf;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_OBJECT || !arg->d.obj) return;

	target = arg->d.obj;
	level = target->level;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str)) return;

	sn = skill_lookup(arg->d.str);
	if( sn <= 0 ) return;

	// Add security check for the spell function
	if(skill_table[sn].spell_fun == spell_null) return;

	if( rest && *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		// Must be a number, positive and no greater than the object's level
		if(arg->type != ENT_NUMBER || arg->d.num < 1 || arg->d.num > target->level) return;

		level = arg->d.num;

	}

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
						if( paf->type == sn && paf->slot == target->wear_loc ) {

							// Update the level if affect's level is higher
							if( paf->level > level )
								paf->level = level;

							// Add security aspect to allow raising the level?

							break;
						}
					}
				}
			}
			return;
		}
	}

	// Spell is new to the object, so add it
	spell_new = new_spell();
	spell_new->sn = sn;
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

			for (paf = target->carried_by->affected; paf != NULL; paf = paf->next)
			{
				if (paf->type == sn)
					break;
			}

			if (paf == NULL || paf->level < level) {
				affect_strip(target->carried_by, sn);
				obj_cast_spell(sn, level + MAGIC_WEAR_SPELL, target->carried_by, target->carried_by, target);
			}
		}
	}
#endif
}


// remspell $OBJECT STRING[ silent]
SCRIPT_CMD(do_rpremspell)
{
	// TODO: Fix to require type contexts
#if 0
	char *rest;
	SPELL_DATA *spell, *spell_prev;
	OBJ_DATA *target;
	int level;
	int sn;
	bool found = false, show = true;
	AFFECT_DATA *paf;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_OBJECT || !arg->d.obj) return;

	target = arg->d.obj;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str)) return;

	sn = skill_lookup(arg->d.str);
	if( sn <= 0 ) return;

	// Add security check for the spell function
	if(skill_table[sn].spell_fun == spell_null) return;

	if( rest && *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if(arg->type != ENT_STRING || IS_NULLSTR(arg->d.str)) return;

		if( !str_cmp(arg->d.str, "silent") )
			show = false;
	}


	found = false;
	spell_prev = NULL;
	for(spell = target->spells; spell; spell_prev = spell, spell = spell->next) {
		if( spell->sn == sn ) {
			if( spell_prev != NULL )
				spell_prev->next = spell->next;
			else
				target->spells = spell->next;

			level = spell->level;

			free_spell(spell);

			found = true;
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
			int spell_level = level;
			int found_loc = WEAR_NONE;

			// Find the first affect that matches this spell and is derived from the object
			for (paf = target->carried_by->affected; paf != NULL; paf = paf->next)
			{
				if (paf->type == spell->sn && paf->slot == target->wear_loc)
					break;
			}

			if( !paf ) {
				// This spell was not applied by this object
				return;
			}

			found = false;
			level = 0;


			// If there's another obj with the same spell put that one on
			for (obj_tmp = target->carried_by->carrying; obj_tmp; obj_tmp = obj_tmp->next_content)
			{
				if( obj_tmp->wear_loc != WEAR_NONE && target != obj_tmp ) {
					for (spell = obj_tmp->spells; spell != NULL; spell = spell ->next) {
						if (spell->sn == sn && spell->level > level ) {
							level = spell->level;	// Keep the maximum
							found_loc = obj_tmp->wear_loc;
							found = true;
						}
					}
				}
			}

			if(!found) {
				// No other worn object had this spell available

				if( show ) {
					if (skill_table[sn].msg_off) {
						send_to_char(skill_table[sn].msg_off, target->carried_by);
						send_to_char("\n\r", target->carried_by);
					}
				}

				affect_strip(target->carried_by, spell->sn);
			} else if( level > spell_level ) {
				level -= spell_level;		// Get the difference

				// Update all affects to the current maximum and its slot
				for(; paf; paf = paf->next ) {
					paf->level += level;
					paf->slot = found_loc;
				}
			}


		}
	}
#endif
}


// alteraffect $AFFECT STRING OP NUMBER
// Current limitations: only level and duration
// Altering other aspects such as modifiers will require updating the owner of the affect, which isn't available here
SCRIPT_CMD(do_rpalteraffect)
{
	char buf[MIL],field[MIL],*rest;

	AFFECT_DATA *paf;
	int value;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_AFFECT || !arg->d.aff) return;

	paf = arg->d.aff;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("RpAlterAffect - Error in parsing.",0);
		return;
	}

	if( IS_NULLSTR(rest) ) return;

	if( arg->type != ENT_STRING || IS_NULLSTR(arg->d.str) ) return;

	strncpy(field,arg->d.str,MIL-1);


	if( !str_cmp(field, "level") ) {
		argument = one_argument(rest,buf);

		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpAlterAffect - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: value = arg->d.num; break;
		default: return;
		}


		switch(buf[0]) {
		case '=':
			if( value > 0 && value < paf->level )
				paf->level = value;

			break;

		case '+':
			if( value < 0 ) {
				paf->level += value;
				if( paf->level < 1 )
					paf->level = 1;
			}
			break;

		case '-':
			if( value > 0 ) {
				paf->level -= value;
				if( paf->level < 1 )
					paf->level = 1;
			}
			break;

		}

		return;
	}

	if(!str_cmp(field, "duration")) {
		argument = one_argument(rest,buf);

		if(!(rest = expand_argument(info,argument,arg))) {
			bug("RpAlterAffect - Error in parsing.",0);
			return;
		}

		if( paf->slot != WEAR_NONE ) {
			bug("RpAlterAffect - Attempting to modify duration of an object given affect.",0);
			return;
		}

		if( paf->group == AFFGROUP_RACIAL ) {
			bug("RpAlterAffect - Attempting to modify duration of a racial affect.",0);
			return;
		}

		if(!str_cmp(buf, "toggle")) {
			paf->duration = -paf->duration;
			return;
		}


		switch(arg->type) {
		case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: value = arg->d.num; break;
		default: return;
		}

		switch(buf[0]) {
		case '=':
			if( value != 0 ) {
				paf->duration = value;
			}

			break;

		case '+':
			if( paf->duration < 0 )
			{
				paf->duration += value;
				if( paf->duration >= 0 )
					paf->duration = -1;
			}
			else
			{
				paf->duration += value;
				if( paf->duration < 0 )
					paf->duration = 0;
			}
			break;

		case '-':
			if( paf->duration < 0 )
			{
				paf->duration -= value;
				if( paf->duration >= 0 )
					paf->duration = -1;
			}
			else
			{
				paf->duration -= value;
				if( paf->duration < 0 )
					paf->duration = 0;
			}
			break;

		}
	}
}


// Syntax: crier STRING
SCRIPT_CMD(do_rpcrier)
{
	if(!info || !info->room) return;

	BUFFER *buffer = new_buf();
	add_buf(buffer, "{M");
	expand_string(info,argument,buffer);

	if( buffer->string[2] != '\0' )
	{
		add_buf(buffer, "{x");

		crier_announce(buffer->string);
	}
	free_buf(buffer);
}

// Syntax: FIXAFFECTS $MOBILE
SCRIPT_CMD(do_rpfixaffects)
{


	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!expand_argument(info,argument,arg))
		return;

	if(arg->type != ENT_MOBILE) return;

	if(arg->d.mob == NULL) return;

	affect_fix_char(arg->d.mob);
}

// Syntax: saveplayer $PLAYER
SCRIPT_CMD(do_rpsaveplayer)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	info->room->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob)) return;

	save_char_obj(mob);

	info->room->progs->lastreturn = 1;
}

// Syntax:	checkpoint $PLAYER $ROOM
// 			checkpoint $PLAYER VNUM
//			checkpoint $PLAYER none|clear|reset
//
// Sets the checkpoint of the $PLAYER to the destination or clears it.
// - When a checkpoint is set, it will override what location is saved to the pfile.
// - When setting a checkpoint via VNUM and an invalid vnum is given, it will clear the checkpoint.
SCRIPT_CMD(do_rpcheckpoint)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob)) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		if( !str_cmp(arg->d.str, "none") ||
			!str_cmp(arg->d.str, "clear") ||
			!str_cmp(arg->d.str, "reset") )
			mob->checkpoint = NULL;
		break;
	case ENT_WIDEVNUM:
		if( arg->d.wnum.pArea && arg->d.wnum.vnum > 0 )
			mob->checkpoint = get_room_index_wnum(arg->d.wnum);
		break;
	case ENT_ROOM:
		if( arg->d.room != NULL )
			mob->checkpoint = arg->d.room;
		break;
	}
}

// Syntax: remort $PLAYER
//  - prompts them for a class out of what they can do
SCRIPT_CMD(do_rpremort)
{
	char *rest;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	info->room->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

    CHAR_DATA *mob = arg->d.mob;

	// Player must be here...
	if (IS_NPC(mob) || !mob->desc) return;

	// Doesn't have a valid race
	if (!IS_VALID(mob->race)) return;

	// Are they even capable of remorting from their current race?
	if (!IS_VALID(mob->race->premort)) return;

	remort_player(mob);

	info->mob->progs->lastreturn = 1;
}


// Syntax: restore $MOBILE
SCRIPT_CMD(do_rprestore)
{
	char *rest;

	int amount = 100;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	if(*rest) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if(arg->type != ENT_NUMBER) return;

		amount = URANGE(1,arg->d.num,100);
	}

	restore_char(arg->d.mob, NULL, amount);
}

// GROUP npc(FOLLOWER) mobile(LEADER)[ bool(SHOW=true)]
// Follower will only work on an NPC
// LASTRETURN:
// 0 = grouping failed
// 1 = grouping succeeded
SCRIPT_CMD(do_rpgroup)
{
	char *rest;

	CHAR_DATA *follower, *leader;
	bool fShow = true;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	info->room->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || !IS_NPC(arg->d.mob)) return;

	follower = arg->d.mob;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	leader = arg->d.mob;

	if( *rest ) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		if( arg->type == ENT_NUMBER )
		{
			fShow = (arg->d.num != 0);
		}
		else if( arg->type == ENT_STRING )
		{
			fShow = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "show");
		}
		else
			return;
	}

	if(add_grouped(follower, leader, fShow))
		info->room->progs->lastreturn = 1;
}

// UNGROUP mobile[ bool(ALL=false)]
SCRIPT_CMD(do_rpungroup)
{
	char *rest;

	bool fAll = false;

	if(!info || !info->room || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	if( *rest ) {
		if( arg->type == ENT_NUMBER )
		{
			fAll = (arg->d.num != 0);
		}
		else if( arg->type == ENT_STRING )
		{
			fAll = !str_cmp(arg->d.str, "yes") || !str_cmp(arg->d.str, "true") || !str_cmp(arg->d.str, "all");
		}
		else
			return;
	}

	if( fAll ) {
		ITERATOR git;
		CHAR_DATA *leader = (arg->d.mob->leader != NULL) ? arg->d.mob->leader : arg->d.mob;
		CHAR_DATA *follower;

		if( leader->num_grouped < 1 )
			return;

		iterator_start(&git, leader->lgroup);
		while((follower = (CHAR_DATA *)iterator_nextdata(&git)))
			stop_grouped(follower);
		iterator_stop(&git);
	}
	else
	{
		stop_grouped(arg->d.mob);
	}
}

