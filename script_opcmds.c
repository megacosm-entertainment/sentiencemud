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

extern bool wiznet_script;

const struct script_cmd_type obj_cmd_table[] = {
	{ "addaffect",			scriptcmd_addaffect,	true,	true	},
	{ "addaffectname",		scriptcmd_addaffectname,true,	true	},
	{ "addspell",			do_opaddspell,			true,	true	},
	{ "alteraffect",		do_opalteraffect,		true,	true	},
	{ "alterexit",			do_opalterexit,			false,	true	},
	{ "altermob",			do_opaltermob,			true,	true	},
	{ "alterobj",			scriptcmd_alterobj,			true,	true	},
	{ "alterroom",			scriptcmd_alterroom,			true,	true	},
	{ "applytoxin",			scriptcmd_applytoxin,	false,	true	},
	{ "asound",				do_opasound,			false,	true	},
	{ "at",					do_opat,				false,	true	},
	{ "attach",				scriptcmd_attach,			true,	true	},
	{ "award",				scriptcmd_award,		true,	true	},
	{ "breathe",			scriptcmd_breathe,		false,	true	},
	{ "call",				do_opcall,				false,	true	},
	{ "cancel",				do_opcancel,			false,	false	},
	{ "cast",       		do_opcast,				false,	true	},
	{ "chargebank",			do_opchargebank,		false,	true	},
	{ "checkpoint",			do_opcheckpoint,		false,	true	},
	{ "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
	{ "cloneroom",			do_opcloneroom,			true,	true	},
	{ "condition",			do_opcondition,			false,	true	},
	{ "crier",				do_opcrier,				false,	true	},
	{ "damage",				scriptcmd_damage,		false,	true	},
	{ "deduct",				scriptcmd_deduct,		true,	true	},
	{ "delay",				do_opdelay,				false,	true	},
	{ "dequeue",			do_opdequeue,			false,	false	},
	{ "destroyroom",		do_opdestroyroom,		true,	true	},
	{ "detach",				scriptcmd_detach,			true,	true	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
	{ "echo",				do_opecho,				false,	true	},
	{ "echoaround",			do_opechoaround,		false,	true	},
	{ "echoat",				scriptcmd_echoat,		false,	true	},
	{ "echobattlespam",		do_opechobattlespam,	false,	true	},
	{ "echochurch",			do_opechochurch,		false,	true	},
	{ "echogrouparound",	do_opechogrouparound,	false,	true	},
	{ "echogroupat",		do_opechogroupat,		false,	true	},
	{ "echoleadaround",		do_opecholeadaround,	false,	true	},
	{ "echoleadat",			do_opecholeadat,		false,	true	},
	{ "echonotvict",		do_opechonotvict,		false,	true	},
	{ "echoroom",			do_opechoroom,			false,	true	},
	{ "ed",					scriptcmd_ed,				false,	true	},
	{ "entercombat",		scriptcmd_entercombat,	false,	true	},
	{ "fade",				scriptcmd_fade,				true,	true	},
	{ "fixaffects",			do_opfixaffects,		false,	true	},
	{ "flee",				scriptcmd_flee,			false,	true	},
	{ "force",				do_opforce,				false,	true	},
	{ "forget",				do_opforget,			false,	false	},
	{ "gdamage",			do_opgdamage,			false,	true	},
	{ "gecho",       		do_opgecho,				false,	true	},
	{ "gforce",				do_opgforce,			false,	true	},
	{ "goto",				do_opgoto,				false,	true	},
	{ "grantskill",			scriptcmd_grantskill,	false,	true	},
	{ "group",				do_opgroup,				false,	true	},
	{ "gtransfer",			do_opgtransfer,			false,	true	},
	{ "input",				do_opinput,				false,	true	},
	{ "inputstring",		scriptcmd_inputstring,	false,	true	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
	{ "interrupt",			do_opinterrupt,			false,	true	},
	{ "junk",				do_opjunk,				false,	true	},
	{ "link",				do_oplink,				false,	true	},
	{ "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
	{ "lockadd",			scriptcmd_lockadd,			false,	true	},
	{ "lockremove",			scriptcmd_lockremove,		false,	true	},
	{ "mail",				scriptcmd_mail,				true,	true	},
	{ "mload",				do_opmload,				false,	true	},
	{ "mute",				scriptcmd_mute,			false,	true	},
	{ "oload",				do_opoload,				false,	true	},
	{ "otransfer",			do_opotransfer,			false,	true	},
	{ "pageat",				scriptcmd_pageat,			false,	true	},
	{ "peace",				do_oppeace,				false,	false	},
	{ "persist",			do_oppersist,			false,	true	},
	{ "prompt",				do_opprompt,			false,	true	},
	{ "purge",				do_oppurge,				false,	false	},
	{ "questaccept",		scriptcmd_questaccept,		false,	true	},
	{ "questcancel",		scriptcmd_questcancel,		false,	true	},
	{ "questcomplete",		scriptcmd_questcomplete,	false,	true	},
	{ "questgenerate",		scriptcmd_questgenerate,	false,	true	},
	{ "questpartcustom",	scriptcmd_questpartcustom,	true,	true	},
	{ "questpartgetitem",	scriptcmd_questpartgetitem,	true,	true	},
	{ "questpartgoto",		scriptcmd_questpartgoto,	true,	true	},
	{ "questpartrescue",	scriptcmd_questpartrescue,	true,	true	},
	{ "questpartslay",		scriptcmd_questpartslay,	true,	true	},
	{ "questscroll",		scriptcmd_questscroll,		false,	true	},
	{ "queue",				do_opqueue,				false,	true	},
	{ "rawkill",			do_oprawkill,			false,	true	},
	{ "reckoning",			scriptcmd_reckoning,		true,	true	},
	{ "remember",			do_opremember,			false,	true	},
	{ "remort",				do_opremort,			true,	true	},
	{ "remove",				do_opremove,			false,	true	},
	{ "remspell",			do_opremspell,			true,	true	},
	{ "resetdice",			do_opresetdice,			true,	true	},
	{ "resetroom",			scriptcmd_resetroom,	true,	true	},
	{ "restore",			do_oprestore,			true,	true	},
	{ "revokeskill",		scriptcmd_revokeskill,	false,	true	},
	{ "saveplayer",			do_opsaveplayer,		false,	true	},
	{ "scriptwait",			do_opscriptwait,		false,	true	},
	{ "selfdestruct",		do_opselfdestruct,		false,	false	},
	{ "sendfloor",			scriptcmd_sendfloor,		false,	true	},
	{ "setrecall",			do_opsetrecall,			false,	true	},
	{ "settimer",			do_opsettimer,			false,	true	},
	{ "showroom",			do_opshowroom,			true,	true	},
	{ "skimprove",			do_opskimprove,			true,	true	},
	{ "spawndungeon",		scriptcmd_spawndungeon,		true,	true	},
	{ "specialkey",			scriptcmd_specialkey,		false,	true	},
	{ "startcombat",		scriptcmd_startcombat,	false,	true	},
	{ "startreckoning",		scriptcmd_startreckoning,	true,	true	},
	{ "stopcombat",			scriptcmd_stopcombat,	false,	true	},
	{ "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
	{ "stringmob",			do_opstringmob,			true,	true	},
	{ "stringobj",			do_opstringobj,			true,	true	},
	{ "stripaffect",		do_opstripaffect,		true,	true	},
	{ "stripaffectname",	do_opstripaffectname,	true,	true	},
	{ "transfer",			do_optransfer,			false,	true	},
	{ "treasuremap",		scriptcmd_treasuremap,		false,	true	},
	{ "ungroup",			do_opungroup,			false,	true	},
	{ "unlockarea",			scriptcmd_unlockarea,		true,	true	},
	{ "unmute",				scriptcmd_unmute,		false,	true	},
	{ "usecatalyst",		do_opusecatalyst,		false,	true	},
	{ "varclear",			do_opvarclear,			false,	true	},
	{ "varclearon",			do_opvarclearon,		false,	true	},
	{ "varcopy",			do_opvarcopy,			false,	true	},
	{ "varsave",			do_opvarsave,			false,	true	},
	{ "varsaveon",			do_opvarsaveon,			false,	true	},
	{ "varset",				do_opvarset,			false,	true	},
	{ "varseton",			do_opvarseton,			false,	true	},
	{ "vforce",				do_opvforce,			false,	true	},
	{ "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
	{ "wiretransfer",		do_opwiretransfer,		false,	true	},
	{ "xcall",				do_opxcall,				false,	true	},
	{ "zecho",				do_opzecho,				false,	true	},
	{ "zot",				do_opzot,				true,	true	},
	{ NULL,					NULL,					false,	false	}
};

int opcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; obj_cmd_table[cmd].name; cmd++)
		if (command[0] == obj_cmd_table[cmd].name[0] &&
			!str_prefix(command, obj_cmd_table[cmd].name))
			return cmd;

	return -1;
}

/*
 * Displays the source code of a given OBJprogram
 *
 * Syntax: opdump [vnum]
 */
void do_opdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *oprg;

	one_argument(argument, buf);
	if (!(oprg = get_script_index(atoi(buf), PRG_OPROG))) {
		send_to_char("No such OBJprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,oprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(oprg->edit_src, ch);
}


/*
 * Displays OBJprogram triggers of a object
 *
 * Syntax: opstat [name]
 */
void do_opstat(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	PROG_LIST *oprg;
	OBJ_DATA *obj;
	ITERATOR it;
	int i, slot;

	one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Opstat what?\n\r", ch);
		return;
	}

	if (!(obj = get_obj_world(ch, arg))) {
		send_to_char("No such object.\n\r", ch);
		return;
	}

	sprintf(arg, "Object #%-6ld [%s] ID [%9d:%9d]\n\r", obj->pIndexData->vnum, obj->short_descr, (int)obj->id[0], (int)obj->id[1]);
	send_to_char(arg, ch);

	if( !IS_NULLSTR(obj->pIndexData->comments) )
	{
		sprintf(arg, "Comments:\n\r%s\n\r", obj->pIndexData->comments);
		send_to_char(arg, ch);
	}

	sprintf(arg, "Delay   %-6d [%s]\n\r",
		obj->progs->delay,
		obj->progs->target ? obj->progs->target->name : "No target");

	send_to_char(arg, ch);

	if (!obj->pIndexData->progs)
		send_to_char("[No programs set]\n\r", ch);
	else
	for(i = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
		iterator_start(&it, obj->pIndexData->progs[slot]);
		while(( oprg = (PROG_LIST *)iterator_nextdata(&it))) {
			sprintf(arg, "[%2d] Trigger [%-8s] Program [%4ld] Phrase [%s]\n\r",
				++i, trigger_name(oprg->trig_type),
				oprg->vnum,
				trigger_phrase(oprg->trig_type,oprg->trig_phrase));
			send_to_char(arg, ch);
		}
		iterator_stop(&it);
	}

	if(obj->progs->vars)
		pstat_variable_list(ch, obj->progs->vars);
}


char *op_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
{
	char *rest, *rest2;
	long vnum;
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AREA_DATA *area;
	ROOM_INDEX_DATA *loc;
	WILDS_DATA *pWilds;
	SCRIPT_PARAM *arg = new_script_param();
	EXIT_DATA *ex;
	int x, y;

	*room = NULL;
	if((rest = expand_argument(info,argument,arg))) {
		switch(arg->type) {
		// Nothing was on the string, so it will assume the current room
		case ENT_NONE: *room = obj_room(info->obj); break;
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
			} else
			{
				*room = get_room_index(x);
				rest = rest2;
			}
			break;

		case ENT_STRING: // Special named locations
			if(arg->d.str[0] == '@')
				// Points to an exit, like @north or @down
				*room = get_exit_dest(obj_room(info->obj), arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				// Rather self-explanatory
				*room = obj_room(info->obj);
			else if(!str_cmp(arg->d.str,"vroom") || !str_cmp(arg->d.str,"clone")) {
				// Locates a clone room: vroom <vnum> <id1> <id2>
				int vnum,id1, id2;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					rest = rest2;
					vnum = arg->d.num;
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						rest = rest2;

						id1 = arg->d.num;
						if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
							rest = rest2;

							id2 = arg->d.num;
							*room = get_clone_room(get_room_index(vnum),id1,id2);
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
						if(!(loc = location_to_room(&area->recall))) {
							for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
								if ((loc = get_room_index(vnum)))
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
			ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
			*room = ex ? exit_destination(ex) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}

	free_script_param(arg);
	return rest;
}

char *op_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
{
	char *rest, *rest2;
	long vnum;
	CHAR_DATA *victim;
	OBJ_DATA *obj;
	AREA_DATA *area;
	ROOM_INDEX_DATA *loc;
	WILDS_DATA *pWilds;
	SCRIPT_PARAM *arg = new_script_param();
	EXIT_DATA *ex;
	int x, y;

	*room = NULL;
	*container = NULL;
	*carrier = NULL;
	*wear_loc = WEAR_NONE;
	if((rest = expand_argument(info,argument,arg))) {
		switch(arg->type) {
		case ENT_NONE:
			*room = obj_room(info->obj);
			break;
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
			} else
				*room = get_room_index(x);
			break;

		case ENT_STRING:
			if(arg->d.str[0] == '@')
				*room = get_exit_dest(obj_room(info->obj), arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				*room = obj_room(info->obj);
			else if(!str_cmp(arg->d.str,"vroom")) {
				int vnum,id1, id2;
				if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
					rest = rest2;
					vnum = arg->d.num;
					if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
						rest = rest2;

						id1 = arg->d.num;
						if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
							rest = rest2;

							id2 = arg->d.num;
							*room = get_clone_room(get_room_index(vnum),id1,id2);
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
						if(!(loc = location_to_room(&area->recall))) {
							for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
								if ((loc = get_room_index(vnum)))
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
			ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL; break;
			*room = ex ? exit_destination(ex) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}
	free_script_param(arg);
	return rest;
}

void obj_interpret(SCRIPT_VARINFO *info, char *argument)
{
	char command[MSL], buf[2*MSL];
	int cmd;

	if(!info->obj) return;

	if (!str_prefix("obj ", argument))
		argument = skip_whitespace(argument+3);

	argument = one_argument(argument, command);

	cmd = opcmd_lookup(command);

	if(cmd < 0) {
		sprintf(buf, "Obj_interpret: invalid cmd from obj %ld: '%s'", info->obj->pIndexData->vnum, command);
		bug(buf, 0);
		return;
	}

	SCRIPT_PARAM *arg = new_script_param();
	(*obj_cmd_table[cmd].func) (info, argument, arg);
	free_script_param(arg);
	tail_chain();
}

SCRIPT_CMD(do_opasound)
{
	ROOM_INDEX_DATA *here, *room;
	ROOM_INDEX_DATA *rooms[MAX_DIR];
	int door, i, j;
	EXIT_DATA *pexit;

	if(!info || !info->obj || !obj_room(info->obj)) return;
	if (!argument[0]) return;

	here = obj_room(info->obj);

	// Verify there are any exits!
	for (door = 0; door < MAX_DIR; door++)
		if ((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
			break;

	if (door < MAX_DIR) {
		// Expand the message
		BUFFER *buffer = new_buf();
		expand_string(info,argument,buffer);
		if(!buf_string(buffer)[0])
		{
			free_buf(buffer);
			return;
		}

		for (i = 0; door < MAX_DIR; door++)
		{
			if ((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
				// Have we been to this room already?
				for(j=0;j < i && rooms[j] != room; j++);

				if(i <= j) {
					// No, so do the message
					MOBtrigger  = false;
					act(buf_string(buffer), room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
					MOBtrigger  = true;
					rooms[i++] = room;
				}
			}
		}

		free_buf(buffer);
	}
}


// do_opat
SCRIPT_CMD(do_opat)
{
	char *command;
	SCRIPT_VARINFO info2;
	OBJ_DATA *dummy_obj;
	ROOM_INDEX_DATA *location;
	int sec;

	if(!info || !info->obj || !obj_room(info->obj))
		return;

	if(!(command = op_getlocation(info, argument, &location))) {
		bug("OpAt: Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	sec = script_security;
	script_security = NO_SCRIPT_SECURITY;

	if(location == obj_room(info->obj))
		obj_interpret(info, command);
	else {
		dummy_obj = create_object(info->obj->pIndexData, 0, false);
		clone_object(info->obj, dummy_obj);

		info2 = *info;
		info2.obj = dummy_obj;
		dummy_obj->progs->target = info->obj->progs->target;
		dummy_obj->progs->vars = info->obj->progs->vars;
		dummy_obj->progs->delay = info->obj->progs->delay;
		SET_BIT(dummy_obj->progs->entity_flags,PROG_AT);
		info2.targ = &(dummy_obj->progs->target);
		info2.var = &(dummy_obj->progs->vars);

		obj_to_room(dummy_obj, location);
		obj_interpret(&info2, command);

		info->obj->progs->target = dummy_obj->progs->target;
		info->obj->progs->vars = dummy_obj->progs->vars;
		info->obj->progs->delay = dummy_obj->progs->delay;

		// Check for other differences

		dummy_obj->progs->target = NULL;
		dummy_obj->progs->vars = NULL;

		extract_obj(dummy_obj);
	}

	script_security = sec;
}

// do_opcall
SCRIPT_CMD(do_opcall)
{
	char *rest;
	CHAR_DATA *vch,*ch;
	OBJ_DATA *obj1,*obj2;
	SCRIPT_DATA *script;
	int depth, vnum, ret;


	if(!info || !info->obj) return;

	if (!argument[0]) {
		bug("OpCall: missing arguments from vnum %d.", VNUM(info->obj));
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("OpCall: maximum call depth exceeded for obj vnum %d.", VNUM(info->obj));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(script = get_script_index(vnum, PRG_OPROG))) {
		bug("OpCall: invalid prog from vnum %d.", VNUM(info->obj));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	ret = execute_script(script->vnum, script, NULL, info->obj, NULL, NULL, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->obj)
		info->obj->progs->lastreturn = ret;
	else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

// do_opcancel
SCRIPT_CMD(do_opcancel)
{
	if(!info || !info->obj) return;

	info->obj->progs->delay = -1;
}


// do_opcast
//  This doesn't call obj_cast because it needs to deal with expansions
SCRIPT_CMD(do_opcast)
{
	char buf[MIL], *rest;
	CHAR_DATA *proxy = NULL;
	CHAR_DATA *vch = NULL, *wch = NULL;
	OBJ_DATA *obj = NULL;
	OBJ_DATA *reagent;
	ROOM_INDEX_DATA *room;
	void *to = NULL;
	int sn, target = TARGET_NONE;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpCast - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(arg->d.str[0]) {
			sn = skill_lookup(arg->d.str);
			break;
		}
	default: sn = 0; break;
	}

	if (sn < 1 || skill_table[sn].spell_fun == spell_null || sn > MAX_SKILL) {
		bug("OpCast - No such spell from vnum %d.", VNUM(info->obj));
		return;
	}

	room = obj_room(info->obj);

	if(*rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCast - Error in parsing from vnum %ld.", VNUM(info->obj));
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			vch = get_char_room(NULL, obj_room(info->obj), arg->d.str);
			wch = get_char_world(NULL, arg->d.str);
			obj = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
			break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		case ENT_OBJECT: obj = arg->d.obj; break;
		}
	}

	proxy = create_mobile(get_mob_index(MOB_VNUM_OBJCASTER), false);
	char_to_room(proxy, room);

	proxy->level = info->obj->level;
	free_string(proxy->name);
	proxy->name = str_dup(info->obj->name);
	free_string(proxy->short_descr);
	proxy->short_descr = str_dup(info->obj->short_descr);

	// Make sure they have a reagent for the powerful spells
	reagent = create_object(get_obj_index(OBJ_VNUM_SHARD), 1, false);
	obj_to_char(reagent,proxy);

	switch (skill_table[sn].target) {
	default: bug("obj_cast: bad target for sn %d.", sn); return;
	case TAR_IGNORE: to = NULL; break;
	case TAR_CHAR_OFFENSIVE:
	case TAR_CHAR_DEFENSIVE:
	case TAR_CHAR_SELF:
		to = vch;
		target = TARGET_CHAR;
		break;

	case TAR_OBJ_INV:
	case TAR_OBJ_GROUND:
		to = obj;
		target = TARGET_OBJ;
		break;

	case TAR_OBJ_CHAR_OFF:
	case TAR_OBJ_CHAR_DEF:
		if(vch) {
			to = vch;
			target = TARGET_CHAR;
		} else {
			to = obj;
			target = TARGET_OBJ;
		}
		break;

	case TAR_IGNORE_CHAR_DEF:
		if(wch) {
			vch = wch;
			to = wch;
			target = TARGET_CHAR;
		} else if(vch) {
			to = vch;
			target = TARGET_CHAR;
		} else {
			to = obj;
			target = TARGET_OBJ;
		}
		break;
	}

	if (target == TARGET_CHAR && vch) {
		if (is_affected(vch, sn)) return;

		if (!check_spell_deflection(proxy, vch, sn)) {
			extract_char(proxy, true);
			return;
		}
	}

	if ((target == TARGET_CHAR && !vch) ||
		(target == TARGET_OBJ  && !obj) ||
		target == TARGET_ROOM || target == TARGET_NONE)
		(*skill_table[sn].spell_fun)(sn, info->obj->level, proxy, to, target, WEAR_NONE);
	else {
		sprintf(buf, "obj_cast: %s(%ld) couldn't find its target", info->obj->short_descr, info->obj->pIndexData->vnum);
		log_string(buf);
	}

	extract_char(proxy, true);
}

// do_opdamage
SCRIPT_CMD(do_opdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *victim_next;
	int low, high, level, value, dc;
	bool fAll = false, fKill = false, fLevel = false, fRemort = false, fTwo = false;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = true;
		else victim = get_char_room(NULL, obj_room(info->obj), arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("OpDamage - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!*rest) {
		bug("OpDamage - missing argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
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
		bug("OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!*rest) {
		bug("OpDamage - missing argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(fLevel && !victim) {
		bug("OpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->obj));
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
			bug("OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("OpDamage - Null reference mob from vnum %ld.", VNUM(info->obj));
				return;
			}
			break;
		} else {
			bug("OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
			return;
		}
		break;
	default:
		bug("OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = obj_room(info->obj)->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, false);
	}
}

// do_opdelay
SCRIPT_CMD(do_opdelay)
{
	int delay = 0;


	if(!info || !info->obj) return;

	if(!expand_argument(info,argument,arg)) {
		bug("OpDelay - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
	case ENT_NUMBER: delay = arg->d.num; break;
	default: delay = 0; break;
	}

	if (delay < 1) {
		bug("OpDelay: invalid delay from vnum %d.", VNUM(info->obj));
		return;
	}
	info->obj->progs->delay = delay;
}

// do_opdequeue
SCRIPT_CMD(do_opdequeue)
{
	if(!info || !info->obj || !info->obj->events)
		return;

	wipe_owned_events(info->obj->events);
}

// do_opecho
SCRIPT_CMD(do_opecho)
{
	if(!info || !info->obj) return;

	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if(!buf_string(buffer)[0])
	{
		free_buf(buffer);
		return;
	}

	add_buf(buffer,"\n\r");
	room_echo(obj_room(info->obj), buf_string(buffer));
	free_buf(buffer);
}

// do_opechoroom
// Syntax: obj echoroom <location> <string>
SCRIPT_CMD(do_opechoroom)
{
	char *rest;
	ROOM_INDEX_DATA *room;

	EXIT_DATA *ex;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: room = arg->d.mob->in_room; break;
	case ENT_OBJECT: room = obj_room(arg->d.obj); break;
	case ENT_ROOM: room = arg->d.room; break;
	case ENT_EXIT:
		ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
		room = ex ? exit_destination(ex) : NULL; break;
	default: room = NULL; break;
	}

	if (!room || !room->people) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(!buf_string(buffer)[0])
	{
		free_buf(buffer);
		return;
	}

	add_buf(buffer,"\n\r");
	room_echo(room, buf_string(buffer));
	free_buf(buffer);
}



// do_opechoaround
SCRIPT_CMD(do_opechoaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
		act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	free_buf(buffer);
}

// do_opechonotvict
SCRIPT_CMD(do_opechonotvict)
{
	char *rest;
	CHAR_DATA *victim, *attacker;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || attacker->in_room != obj_room(info->obj))
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
		act(buffer->string, victim, attacker, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	free_buf(buffer);
}

SCRIPT_CMD(do_opechobattlespam)
{
	char *rest;
	CHAR_DATA *victim, *attacker, *ch;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || attacker->in_room != obj_room(info->obj))
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
			if (!IS_NPC(ch) && (ch != attacker && ch != victim) && (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM))) {
				act(buffer->string, ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
		}
	}
	free_buf(buffer);
}

// do_opechoat
SCRIPT_CMD(do_opechoat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		act(buffer->string, victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	free_buf(buffer);
}

// do_opechochurch
SCRIPT_CMD(do_opechochurch)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim) || !victim->church)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		msg_church_members(victim->church, buffer->string);
	}
	free_buf(buffer);
}

// do_opechogrouparound
SCRIPT_CMD(do_opechogrouparound)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		act_new(buffer->string,victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
	}
	free_buf(buffer);
}

// do_opechogroupat
SCRIPT_CMD(do_opechogroupat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		act_new(buffer->string,victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);
	}
	free_buf(buffer);
}

// do_opecholeadaround
SCRIPT_CMD(do_opecholeadaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->leader || victim->leader->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	}
	free_buf(buffer);
}

// do_opecholeadat
SCRIPT_CMD(do_opecholeadat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->leader || victim->leader->in_room != obj_room(info->obj))
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		act(buffer->string, victim->leader, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	}
	free_buf(buffer);
}

// do_opforce
SCRIPT_CMD(do_opforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *next;
	bool fAll = false, forced;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpForce - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = true;
		else victim = get_char_room(NULL,obj_room(info->obj), arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!fAll && !victim) {
		bug("OpForce - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buffer->string[0] != '\0')
	{
		forced = forced_command;

		if (fAll) {
			for (victim = obj_room(info->obj)->people; victim; victim = next) {
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

// do_opforget
SCRIPT_CMD(do_opforget)
{
	if(!info || !info->obj) return;

	info->obj->progs->target = NULL;

}

// do_opgdamage
SCRIPT_CMD(do_opgdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *rch, *rch_next;
	int low, high, level, value, dc;
	bool fKill = false, fLevel = false, fRemort = false, fTwo = false;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpGdamage - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("OpGdamage - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!*rest) {
		bug("OpGdamage - missing argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpGdamage - Error in parsing from vnum %ld.", VNUM(info->obj));
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
		bug("OpGdamage - invalid argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!*rest) {
		bug("OpGdamage - missing argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpGdamage - Error in parsing from vnum %ld.", VNUM(info->obj));
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
			bug("OpGdamage - invalid argument from vnum %ld.", VNUM(info->obj));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("OpGdamage - Null reference mob from vnum %ld.", VNUM(info->obj));
				return;
			}
			break;
		} else {
			bug("OpGdamage - invalid argument from vnum %ld.", VNUM(info->obj));
			return;
		}
		break;
	default:
		bug("OpGdamage - invalid argument from vnum %ld.", VNUM(info->obj));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = true;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	for(rch = obj_room(info->obj)->people; rch; rch = rch_next) {
		rch_next = rch->next_in_room;
		if (rch != victim && is_same_group(victim,rch)) {
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(rch, rch, fKill ? value : UMIN(rch->hit,value), TYPE_UNDEFINED, dc, false);
		}
	}
}

// do_opgecho
SCRIPT_CMD(do_opgecho)
{
	DESCRIPTOR_DATA *d;

	if(!info || !info->obj) return;

	if (!argument[0]) {
		bug("OpGEcho: missing argument from vnum %d", VNUM(info->obj));
		return;
	}

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (d = descriptor_list; d; d = d->next)
			if (d->connected == CON_PLAYING) {
				if (IS_IMMORTAL(d->character))
					send_to_char("Obj echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}
	free_buf(buffer);
}

// do_opgforce
SCRIPT_CMD(do_opgforce)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *vch, *next;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpGforce - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!victim) {
		bug("OpGforce - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buffer->string[0] != '\0')
	{
		for (vch = obj_room(info->obj)->people; vch; vch = next) {
			next = vch->next_in_room;
			if (is_same_group(victim,vch))
				interpret(vch, buf);
		}
	}
	free_buf(buffer);
}

// do_opgoto
SCRIPT_CMD(do_opgoto)
{
	ROOM_INDEX_DATA *dest;

	if(!info || !info->obj || !obj_room(info->obj) || PROG_FLAG(info->obj,PROG_AT)) return;

	if(!argument[0]) {
		bug("Opgoto - No argument from vnum %d.", VNUM(info->obj));
		return;
	}

	op_getlocation(info, argument, &dest);

	if(!dest) {
		bug("Opgoto - Bad location from vnum %d.", VNUM(info->obj));
		return;
	}

	if (info->obj->in_obj) obj_from_obj(info->obj);
	else if (info->obj->carried_by) obj_from_char(info->obj);
	else if (info->obj->in_room) obj_from_room(info->obj);

	// @@@NIB Need to take into account that it is a pulled cart OR used furniture as well

	obj_to_room(info->obj, dest);

}

// do_opgtransfer
SCRIPT_CMD(do_opgtransfer)
{
	char buf[MIL], buf2[MIL], buf3[MIL], *rest;
	CHAR_DATA *victim, *vch,*next;
	ROOM_INDEX_DATA *dest;
	bool all = false, force = false, quiet = false;
	int mode;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpGtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("OpGtransfer - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if (!victim->in_room) return;

	if(!(argument = op_getlocation(info, rest, &dest))) {
		bug("OpGtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!dest) {
		bug("OpGtransfer - Bad location from vnum %d.", VNUM(info->obj));
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

	for (vch = victim->in_room->people; vch; vch = next) {
		next = vch->next_in_room;
		if (!IS_NPC(vch) && is_same_group(victim,vch)) {
			if (!all && vch->position != POS_STANDING) continue;
			if (!force && room_is_private(dest, info->mob)) break;
			do_mob_transfer(vch,dest,quiet,mode);
		}
	}
}

// do_opjunk
SCRIPT_CMD(do_opjunk)
{
	OBJ_DATA *obj;
	OBJ_DATA *obj_next;


	if(!info || !info->obj || !argument[0]) return;

	if(expand_argument(info,argument,arg)) {
		switch(arg->type) {
		case ENT_STRING:
			if (str_cmp(arg->d.str, "all") && str_prefix("all.", arg->d.str)) {
				for (obj = info->obj->contains; obj && !is_name(arg->d.str, obj->name); obj = obj->next_content);
			} else {
				for (obj = info->obj->contains; obj; obj = obj_next) {
					obj_next = obj->next_content;
					if (!arg->d.str[3] || is_name(&arg->d.str[4], obj->name))
						extract_obj(obj);
				}
				return;
			}
			break;
		case ENT_OBJECT:
			obj = (arg->d.obj && arg->d.obj->in_obj == info->obj) ? arg->d.obj : NULL;
			break;
		case ENT_OLLIST_OBJ:
			if(arg->d.list.ptr.obj && *(arg->d.list.ptr.obj) && (*arg->d.list.ptr.obj)->in_obj == info->obj) {
				for (obj = *(arg->d.list.ptr.obj); obj; obj = obj_next) {
					obj_next = obj->next_content;
					extract_obj(obj);
				}
			}
			return;
		default: obj = NULL; break;
		}

		if(obj && !PROG_FLAG(obj,PROG_AT)) extract_obj(obj);
	}
}

// do_oplink
SCRIPT_CMD(do_oplink)
{
	char *rest;
	ROOM_INDEX_DATA *room, *dest;
	int door, vnum;
	unsigned long id1, id2;

	bool del = false;
	bool environ = false;
	EXIT_DATA *ex;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		room = obj_room(info->obj);
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

	vnum = -1;
	id1 = id2 = 0;
	switch(arg->type) {
	case ENT_STRING:
		if(is_number(arg->d.str))
			vnum = atoi(arg->d.str);
		else if(!str_cmp(arg->d.str,"delete") ||
			!str_cmp(arg->d.str,"remove") ||
			!str_cmp(arg->d.str,"unlink")) {
			vnum = 0;
			del = true;
		} else if(!str_cmp(arg->d.str,"environment") ||
			!str_cmp(arg->d.str,"environ") ||
			!str_cmp(arg->d.str,"extern") ||
			!str_cmp(arg->d.str,"outside")) {
			vnum = 0;
			environ = true;
		} else if(!str_cmp(arg->d.str,"vroom")) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
				return;
			vnum = arg->d.num;

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
	case ENT_NUMBER:
		vnum = arg->d.num;
		break;
	case ENT_ROOM:
		vnum = arg->d.room ? arg->d.room->vnum : -1;
		break;
	case ENT_EXIT:
		ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
		vnum = (ex && ex->u1.to_room) ? ex->u1.to_room->vnum : -1;
		break;
	case ENT_MOBILE:
		vnum = (arg->d.mob && arg->d.mob->in_room) ? arg->d.mob->in_room->vnum : -1;
		break;
	case ENT_OBJECT:
		vnum = (arg->d.obj && obj_room(arg->d.obj)) ? obj_room(arg->d.obj)->vnum : -1;
		break;
	}

	if(vnum < 0) {
		bug("OPlink - invalid argument in room %d.", room->vnum);
		return;
	}

	if(id1 > 0 || id2 > 0)
		dest = get_clone_room(get_room_index(vnum),id1,id2);
	else if(vnum > 0)
		dest = get_room_index(vnum);
	else if(environ)
		dest = &room_pointer_environment;
	else
		dest = NULL;

	if(!dest && !del) {
		bug("OPlink - invalid destination in room %d.", room->vnum);
		return;
	}

	script_change_exit(room, dest, door);
}

// do_opmload
SCRIPT_CMD(do_opmload)
{
	char buf[MIL], *rest;
	long vnum;
	MOB_INDEX_DATA *pMobIndex;
	CHAR_DATA *victim;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_MOBILE: vnum = arg->d.mob ? arg->d.mob->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(pMobIndex = get_mob_index(vnum))) {
		sprintf(buf, "Opmload: bad mob index (%ld) from mob %ld", vnum, VNUM(info->obj));
		bug(buf, 0);
		return;
	}

	victim = create_mobile(pMobIndex, false);
	char_to_room(victim, obj_room(info->obj));
	if(rest && *rest) variables_set_mobile(info->var,rest,victim);
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

// do_opoload
SCRIPT_CMD(do_opoload)
{
	char buf[MIL], *rest;
	long vnum, level;
	bool fInside = false;
	bool fWear = false;

	OBJ_INDEX_DATA *pObjIndex;
	OBJ_DATA *obj;

	CHAR_DATA *to_mob = NULL;
	OBJ_DATA *to_obj = NULL;
	ROOM_INDEX_DATA *to_room = NULL;

	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_OBJECT: vnum = arg->d.obj ? arg->d.obj->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (!vnum || !(pObjIndex = get_obj_index(vnum))) {
		bug("Opoload - Bad vnum arg from vnum %d.", VNUM(info->obj));
		return;
	}

	if(rest && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg)))
			return;

		switch(arg->type) {
		case ENT_NUMBER: level = arg->d.num; break;
		case ENT_STRING: level = arg->d.str ? atoi(arg->d.str) : 0; break;
		case ENT_MOBILE: level = arg->d.mob ? get_trust(arg->d.mob) : 0; break;
		case ENT_OBJECT: level = arg->d.obj ? arg->d.obj->pIndexData->level : 0; break;
		default: level = 0; break;
		}

		if(level <= 0) level = info->obj->pIndexData->level;

		if(rest && *rest) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)))
				return;

			/*
			 * Added 3rd argument
			 * omitted - load to current room
			 * 'I'     - load to object's container
			 * MOBILE  - load to target mobile
			 *         - 'W' automatically wear the item if possible
			 * OBJECT  - load to target object
			 * ROOM    - load to target room
			 */

			switch(arg->type) {
			case ENT_STRING:
				if (!str_cmp(arg->d.str, "inside") &&
					IS_SET(pObjIndex->wear_flags, ITEM_TAKE)) {
					if( info->obj->item_type == ITEM_CONTAINER ||
						info->obj->item_type == ITEM_CART)
						fInside = true;

					else if( info->obj->item_type == ITEM_WEAPON_CONTAINER &&
						info->obj->value[1] == pObjIndex->value[0] )
						fInside = true;

				}
				break;

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
					if(arg->d.obj->item_type == ITEM_CONTAINER ||
						arg->d.obj->item_type == ITEM_CART)
						to_obj = arg->d.obj;
					else if(arg->d.obj->item_type == ITEM_WEAPON_CONTAINER &&
						pObjIndex->item_type == ITEM_WEAPON &&
						pObjIndex->value[0] == arg->d.obj->value[1])
						to_obj = arg->d.obj;
					else
						return;	// Trying to put the item into a non-container won't work
				}
				break;

			case ENT_ROOM:		to_room = arg->d.room; break;
			}
		}
	} else
		level = info->obj->pIndexData->level;

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
	} else if(fInside)
		obj_to_obj(obj, info->obj);
	else
		obj_to_room(obj, obj_room(info->obj));

	if(rest && *rest) variables_set_object(info->var,rest,obj);
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

// do_opotransfer
SCRIPT_CMD(do_opotransfer)
{
	char *rest;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *dest;
	OBJ_DATA *container;
	CHAR_DATA *carrier;
	int wear_loc = WEAR_NONE;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpOtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: obj = get_obj_here(NULL,obj_room(info->obj), arg->d.str); break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: obj = NULL; break;
	}


	if (!obj) {
		bug("OpOtransfer - Null object from vnum %ld.", VNUM(info->obj));
		return;
	}

	if (PROG_FLAG(obj,PROG_AT)) return;

	if (IS_SET(obj->extra[2], ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

	argument = op_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

	if(!dest && !container && !carrier) {
		bug("OpOTransfer - Bad location from vnum %d.", VNUM(info->obj));
		return;
	}

	if (obj->carried_by) {
		if (obj->wear_loc != WEAR_NONE)
			unequip_char(obj->carried_by, obj, true);
		obj_from_char(obj);
	} else if(obj->in_obj)
		obj_from_obj(obj);
	else
		obj_from_room(obj);

	if(dest) {
		if(dest->wilds)
			obj_to_vroom(obj, dest->wilds, dest->x, dest->y);
		else
			obj_to_room(obj, dest);
	} else if(container)
		obj_to_obj(obj, container);
	else if(carrier) {
		obj_to_char(obj, carrier);
		if(wear_loc != WEAR_NONE)
			equip_char(carrier, obj, wear_loc);
	}
}

// do_oppeace
SCRIPT_CMD(do_oppeace)
{
	CHAR_DATA *rch;
	if(!info || !info->obj || !obj_room(info->obj)) return;

	for (rch = obj_room(info->obj)->people; rch; rch = rch->next_in_room) {
		if (rch->fighting)
			stop_fighting(rch, true);
		if (IS_NPC(rch) && IS_SET(rch->act[0],ACT_AGGRESSIVE))
			REMOVE_BIT(rch->act[0],ACT_AGGRESSIVE);
	}
}

// do_oppurge
SCRIPT_CMD(do_oppurge)
{
	char *rest;
	CHAR_DATA **mobs = NULL, *victim = NULL,*vnext;
	OBJ_DATA **objs = NULL, *obj = NULL,*obj_next;
	ROOM_INDEX_DATA *here = NULL;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NONE: here = obj_room(info->obj); break;
	case ENT_STRING:
		if (!(victim = get_char_room(NULL, obj_room(info->obj), arg->d.str)))
			obj = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
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
			bug("Oppurge - Attempting to purge a PC from vnum %d.", VNUM(info->obj));
			return;
		}
		extract_char(victim, true);
	} else if(obj) {
		if(PROG_FLAG(obj,PROG_AT)) return;
		extract_obj(obj);
	} else if(here) {
		for (victim = here->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && !IS_SET(victim->act[0], ACT_NOPURGE))
				extract_char(victim, true);
		}

		for (obj = here->contents; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (obj != info->obj && !IS_SET(obj->extra[0], ITEM_NOPURGE))
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
			if (obj != info->obj && !IS_SET(obj->extra[0], ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else
		bug("Oppurge - Bad argument from vnum %d.", VNUM(info->obj));

}

// do_opqueue
SCRIPT_CMD(do_opqueue)
{
	char *rest;
	int delay;


	if(!info || !info->obj || PROG_FLAG(info->obj,PROG_AT)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: delay = arg->d.num; break;
	case ENT_STRING: delay = atoi(arg->d.str); break;
	default:
		bug("OpQueue:  missing arguments from obj vnum %d.", VNUM(info->obj));
		return;
	}

	if (delay < 0 || delay > 1000) {
		bug("OpQueue:  unreasonable delay recieved from obj vnum %d.", VNUM(info->obj));
		return;
	}

	wait_function(info->obj, info, EVENT_OBJQUEUE, delay, script_interpret, rest);
}

// do_opremember
SCRIPT_CMD(do_opremember)
{
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!expand_argument(info,argument,arg)) {
		bug("OpRemember: Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("OpRemember: Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	info->obj->progs->target = victim;
}

// do_opremove
SCRIPT_CMD(do_opremove)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj = NULL, *obj_next;
	int vnum = 0, count = 0;
	bool fAll = false;

	char name[MIL], *rest;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpRemove: Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj), arg->d.str);
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("OpRemove: Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpRemove: Bad syntax from vnum %ld.", VNUM(info->obj));
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
		bug ("OpRemove: Invalid object from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!fAll && !obj && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpRemove: Bad syntax from vnum %ld.", VNUM(info->obj));
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER: count = arg->d.num; break;
		case ENT_STRING: count = atoi(arg->d.str); break;
		default: count = 0; break;
		}

		if(count < 0) {
			bug ("OpRemove: Invalid count from vnum %d.", VNUM(info->obj));
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

// do_opselfdestruct
SCRIPT_CMD(do_opselfdestruct)
{
	char buf[MSL];
	CHAR_DATA *vch;

	if(!info || !info->obj || PROG_FLAG(info->obj,PROG_NODESTRUCT) || PROG_FLAG(info->obj,PROG_AT)) return;

	if(script_security < MIN_SCRIPT_SECURITY) {
		sprintf(buf, "OpSelfDestruct: object %s(%ld) trying to self destruct remotely.",
			info->obj->pIndexData->short_descr, info->obj->pIndexData->vnum);
		log_string(buf);
	}

	sprintf(buf, "OpSelfDestruct: object %s(%ld) self-destructed",
		info->obj->pIndexData->short_descr, info->obj->pIndexData->vnum);
	log_string(buf);

	if (!obj_room(info->obj)) {
		bug("OpSelfDestruct: BAILED OUT, OBJ IS NOWHERE", 0);
		return;
	}

	if ((vch = info->obj->carried_by) && info->obj->wear_loc != -1)
		unequip_char(vch, info->obj, true);

	extract_obj(info->obj);
	//info->obj = NULL;	// Handled by recycling code
}

// do_optransfer
SCRIPT_CMD(do_optransfer)
{
	char buf[MIL], buf2[MIL], *rest;
	CHAR_DATA *victim = NULL,*vnext;
	ROOM_INDEX_DATA *dest;
	bool all = false, force = false, quiet = false;
	int mode;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpTransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
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
		bug("OpTransfer - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	// This was crashing on transfer all scripts, as victim is null.
	//if (!victim->in_room) return;

	argument = op_getlocation(info, rest, &dest);

	if(!dest) {
		bug("OpTransfer - Bad location from vnum %d.", VNUM(info->obj));
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
		for (victim = obj_room(info->obj)->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (PROG_FLAG(victim,PROG_AT)) continue;
			if (!IS_NPC(victim)) {
				if (!force && room_is_private(dest, NULL)) break;
				do_mob_transfer(victim,dest,quiet,mode);
			}
		}
		return;
	}

	if (!force && room_is_private(dest, NULL))
		return;

	if (PROG_FLAG(victim,PROG_AT))
		return;

	do_mob_transfer(victim,dest,quiet,mode);
}

// do_opvforce
SCRIPT_CMD(do_opvforce)
{
	char buf[MSL],*rest;
	int vnum = 0;
	CHAR_DATA *vch, *next;


	if(!info || !info->obj || !obj_room(info->obj)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpVforce - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: break;
	}

	if (vnum < 1) {
		bug("OpVforce - Invalid vnum from vnum %ld.", VNUM(info->obj));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buffer->string[0] != '\0')
	{
		for (vch = obj_room(info->obj)->people; vch; vch = next) {
			next = vch->next_in_room;
			if (IS_NPC(vch) &&  vch->pIndexData->vnum == vnum && !vch->fighting)
				interpret(vch, buf);
		}
	}
	free_buf(buffer);
}

// do_opzecho
SCRIPT_CMD(do_opzecho)
{
	AREA_DATA *area;
	DESCRIPTOR_DATA *d;

	if(!info || !info->obj || !obj_room(info->obj)) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{

		area = obj_room(info->obj)->area;

		for (d = descriptor_list; d; d = d->next)
			if (d->connected == CON_PLAYING &&
				d->character->in_room &&
				d->character->in_room->area == area) {
				if (IS_IMMORTAL(d->character))
					send_to_char("Obj echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}
	free_buf(buffer);
}

// do_opzot
SCRIPT_CMD(do_opzot)
{
	CHAR_DATA *victim;


	if(!info || !info->obj) return;

	if(!expand_argument(info,argument,arg))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("OpZot - Null victim from vnum %ld.", VNUM(info->obj));
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

SCRIPT_CMD(do_opvarset)
{
	if(!info || !info->obj || !info->var) return;

	script_varseton(info, info->var, argument, arg);
}

SCRIPT_CMD(do_opvarclear)
{
	if(!info || !info->obj || !info->var) return;

	script_varclearon(info, info->var, argument, arg);
}

SCRIPT_CMD(do_opvarcopy)
{
	char oldname[MIL],newname[MIL];

	if(!info || !info->obj || !info->var) return;

	// Get name
	argument = one_argument(argument,oldname);
	if(!oldname[0]) return;
	argument = one_argument(argument,newname);
	if(!newname[0]) return;

	if(!str_cmp(oldname,newname)) return;

	variable_copy(info->var,oldname,newname);
}

SCRIPT_CMD(do_opvarsave)
{
	char name[MIL],arg1[MIL];
	bool on;

	if(!info || !info->obj || !info->var) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,arg1);
	if(!arg1[0]) return;

	on = !str_cmp(arg1,"on") || !str_cmp(arg1,"true") || !str_cmp(arg1,"yes");

	variable_setsave(*info->var,name,on);
}

// varset name type value
//
//   types:				CALL
//     integer <number>			variable_set_integer
//     string <string>			variable_set_string (shared:false)
//     room <entity>			variable_set_room
//     room <vnum>			variable_set_room
//     mobile <entity>			variable_set_mobile
//     mobile <location> <vnum>		variable_set_mobile
//     mobile <location> <name>		variable_set_mobile
//     mobile <mob_list> <vnum>		variable_set_mobile
//     mobile <mob_list> <name>		variable_set_mobile
//     player <entity>			variable_set_mobile
//     player <name>			variable_set_mobile
//     object <entity>			variable_set_object
//     object <location> <vnum>		variable_set_object
//     object <location> <name>		variable_set_object
//     object <obj_list> <vnum>		variable_set_object
//     object <obj_list> <name>		variable_set_object
//     carry <mobile> <vnum>		variable_set_object
//     carry <mobile> <name>		variable_set_object
//     content <object> <vnum>		variable_set_object
//     content <object> <name>		variable_set_object
//     token <mobile> <vnum>		variable_set_token
//     token <object> <vnum>		variable_set_token
//     token <entity> <vnum>		variable_set_token
//     token <token_list> <vnum>	variable_set_token

//
// Note: <entity> refers to $( ) use
//
// varclear name
// varcopy old new
// varsave name on|off


SCRIPT_CMD(do_opsettimer)
{
	char buf[MIL],*rest;
	int amt;
	CHAR_DATA *victim = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpSetTimer - Error in parsing.",0);
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
		bug("OpSetTimer - NULL victim.", 0);
		return;
	}

	if(!*rest) {
		bug("OpSetTimer - Missing timer type.",0);
		return;
	}

	buf[0] = 0;
	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		strncpy(buf,arg->d.str,MIL);
		break;
	default: break;
	}

	if(!*rest) {
		bug("OpSetTimer - Missing timer amount.",0);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpSetTimer - Error in parsing.",0);
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
			SET_BIT(victim->act[0], ACT2_HIRED);
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
		else if(!str_cmp(buf,"quest"))
		{
			if(!IS_NPC(victim) && IS_QUESTING(victim))
			{
				victim->countdown = amt;
			}
		}
		else if(!str_cmp(buf,"nextquest"))
		{
			if(!IS_NPC(victim))
			{
				victim->nextquest = amt;
			}
		}
	}
}

SCRIPT_CMD(do_opinterrupt)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL;
	ROOM_INDEX_DATA *here;

	int stop, ret = 0;
	bool silent = false;

	if(!info || !info->obj) return;

	here = obj_room(info->obj);

	info->obj->progs->lastreturn = 0;	// Nothing was interrupted

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpInterrupt - Error in parsing.",0);
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
		bug("OpInterrupt - NULL victim.", 0);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buffer->string[0] != '\0') {
		stop = flag_value(interrupt_action_types,buf);
		if(stop == NO_FLAG) {
			bug("OpInterrupt - invalid interrupt type.", 0);
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
		victim->brew_sn = 0;
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
		victim->scribe_sn = 0;
		victim->scribe_sn2 = 0;
		victim->scribe_sn3 = 0;
		SET_BIT(ret,INTERRUPT_SCRIBE);
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
	info->obj->progs->lastreturn = ret;
	free_buf(buffer);
}

/*
SCRIPT_CMD(do_opalterobj)
{
	char buf[2*MIL],field[MIL],*rest;
	int value, num, min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;
	int min, max;
	bool hasmin = false, hasmax = false;
	bool allowarith = true;
	const struct flag_type *flags = NULL;
	const struct flag_type **bank = NULL;
	long temp_flags[4];
	int sec_flags[4];
	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"self"))
			obj = info->obj;
		else
			obj = get_obj_here(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("OpAlterObj - NULL object.", 0);
		return;
	}

	if(PROG_FLAG(obj,PROG_AT)) return;

	if(!*rest) {
		bug("OpAlterObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAlterObj - Error in parsing.",0);
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
		bug("OpAlterObj - Error in parsing.",0);
		return;
	}

	if(num >= 0) {
		switch(arg->type) {
		case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: value = arg->d.num; break;
		default: return;
		}

		if(obj->item_type == ITEM_CONTAINER) {
			if(num == 3 || num == 4) min_sec = 5;
		}

		if(script_security < min_sec) {
			sprintf(buf,"OpAlterObj - Attempting to alter value%d with security %d.\n\r", num, script_security);
			bug(buf, 0);
			return;
		}

		switch (buf[0]) {
		case '+': obj->value[num] += value; break;
		case '-': obj->value[num] -= value; break;
		case '*': obj->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("OpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			obj->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("OpAlterObj - adjust called with operator % and value 0", 0);
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
		else if(!str_cmp(field,"extra2"))		{ ptr = (int*)&obj->extra[1]; flags = extra2_flags; min_sec = 5; }
		else if(!str_cmp(field,"extra3"))		{ ptr = (int*)&obj->extra[2]; flags = extra3_flags; min_sec = 5; }
		else if(!str_cmp(field,"extra4"))		{ ptr = (int*)&obj->extra[3]; flags = extra4_flags; min_sec = 5; }
		else if(!str_cmp(field,"fixes"))		{ ptr = (int*)&obj->times_allowed_fixed; min_sec = 5; }
		else if(!str_cmp(field,"key"))			{ if( obj->lock ) { ptr = (int*)&obj->lock->key_vnum; } }
		else if(!str_cmp(field,"level"))		{ ptr = (int*)&obj->level; min_sec = 5; }
		else if(!str_cmp(field,"lockflags"))	{ if( obj->lock ) { ptr = (int*)&obj->lock->flags; flags = lock_flags; } }
		else if(!str_cmp(field,"pickchance"))	{ if( obj->lock ) { ptr = (int*)&obj->lock->pick_chance; min = 0; max = 100; hasmin = hasmax = true; } }
		else if(!str_cmp(field,"repairs"))		ptr = (int*)&obj->times_fixed;
		else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&obj->tempstore[0];
		else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&obj->tempstore[1];
		else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&obj->tempstore[2];
		else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&obj->tempstore[3];
		else if(!str_cmp(field,"timer"))		ptr = (int*)&obj->timer;
		else if(!str_cmp(field,"type"))			{ ptr = (int*)&obj->item_type; flags = type_flags; min_sec = 7; }
		else if(!str_cmp(field,"wear"))			{ ptr = (int*)&obj->wear_flags; flags = wear_flags; }
		else if(!str_cmp(field,"wearloc"))		{ ptr = (int*)&obj->wear_loc; flags = wear_loc_flags; }
		else if(!str_cmp(field,"weight"))		ptr = (int*)&obj->weight;

		if(!ptr) return;

		if(script_security < min_sec) {
			sprintf(buf,"OpAlterObj - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
			else if( flags == lock_flags )
			{
				int keep = LOCK_CREATED;

				if( !IS_SET(*ptr, LOCK_CREATED) )
				{
					SET_BIT(keep, LOCK_NOREMOVE);
					SET_BIT(keep, LOCK_NOJAM);
				}

				REMOVE_BIT(value, keep);

				if( buf[0] == '=' || buf[0] == '&' )
				{
					value |= (*ptr & keep);
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
				bug("OpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value;
			break;

		case '-':
			if( !allowarith ) {
				bug("OpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value;
			break;

		case '*':
			if( !allowarith ) {
				bug("OpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value;
			break;

		case '/':
			if( !allowarith ) {
				bug("OpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("OpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("OpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("OpAlterObj - adjust called with operator % and value 0", 0);
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
*/


SCRIPT_CMD(do_opresetdice)
{
	char *rest;
	OBJ_DATA *obj = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"self"))
			obj = info->obj;
		else
			obj = get_obj_here(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("OpAlterObj - NULL object.", 0);
		return;
	}

	if(PROG_FLAG(obj,PROG_AT)) return;

	if(obj->item_type == ITEM_WEAPON)
		set_weapon_dice_obj(obj);
}



SCRIPT_CMD(do_opstringobj)
{
	char buf[MSL],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;

	bool newlines = false;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpStringObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("OpStringObj - NULL object.", 0);
		return;
	}

	if(PROG_FLAG(obj,PROG_AT)) return;

	if(!*rest) {
		bug("OpStringObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpStringObj - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING:
		strncpy(field,arg->d.str,MIL-1);
		break;
	default: return;
	}

	if(!field[0]) return;

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(buffer->string[0] != '\0')
	{
		if(!str_cmp(field,"name")) {
			if(obj->old_short_descr)
			{
				free_buf(buffer);
				return;	// Can't change restrings, sorry!
			}
			str = (char**)&obj->name;
		} else if(!str_cmp(field,"owner")) {
			str = (char**)&obj->owner;
			min_sec = 5;
		} else if(!str_cmp(field,"short")) {
			if(obj->old_short_descr)
			{
				free_buf(buffer);
				return;	// Can't change restrings, sorry!
			}
			str = (char**)&obj->short_descr;
		} else if(!str_cmp(field,"long")) {
			if(obj->old_description)
			{
				free_buf(buffer);
				return;	// Can't change restrings, sorry!
			}
			str = (char**)&obj->description;
		} else if(!str_cmp(field,"full")) {
			if(obj->old_full_description)
			{
				free_buf(buffer);
				return;	// Can't change restrings, sorry!
			}
			str = (char**)&obj->full_description;
			newlines = true;
		} else if(!str_cmp(field,"material")) {
			int mat = material_lookup(buf_string(buffer));

			if(mat < 0) {
				bug("OpStringObj - Invalid material.\n\r", 0);
				free_buf(buffer);
				return;
			}

			// Force material to the full name
			clear_buf(buffer);
			add_buf(buffer, material_table[mat].name);

			str = (char**)&obj->material;
		}
		else
		{
			free_buf(buffer);
			return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"OpStringObj - Attempting to restring '%s' with security %d.\n\r", field, script_security);
			bug(buf, 0);
			free_buf(buffer);
			return;
		}

		char *p = buf_string(buffer);
		strip_newline(p,newlines);

		free_string(*str);
		*str = str_dup(p);
	}
	free_buf(buffer);
}

SCRIPT_CMD(do_opaltermob)
{
	char buf[MSL],field[MIL],*rest;
	int value = 0, min_sec = MIN_SCRIPT_SECURITY, min = 0, max = 0;
	CHAR_DATA *mob = NULL;

	long *ptr = NULL;
	bool allowpc = false;
	bool allowarith = true;
	bool allowbitwise = true;
	bool lookuprace = false;
	bool lookup_attack_type = false;
	bool hasmin = false;
	bool hasmax = false;
	const struct flag_type *flags = NULL;
	const struct flag_type **bank = NULL;
	long temp_flags[4];
	int dirty_stat = -1;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("OpAlterMob - NULL mobile.", 0);
		return;
	}

	if(!*rest) {
		bug("OpAlterMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAlterMob - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterMob - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"acbash"))		ptr = (long*)&mob->armour[AC_BASH];
	else if(!str_cmp(field,"acexotic"))	ptr = (long*)&mob->armour[AC_EXOTIC];
	else if(!str_cmp(field,"acpierce"))	ptr = (long*)&mob->armour[AC_PIERCE];
	else if(!str_cmp(field,"acslash"))	ptr = (long*)&mob->armour[AC_SLASH];
	else if(!str_cmp(field,"act"))		{ ptr = (long*)mob->act; bank = IS_NPC(mob) ? act_flagbank : plr_flagbank; }
	else if(!str_cmp(field,"affect"))	{ ptr = (long*)mob->affected_by; bank = affect_flagbank; }
	else if(!str_cmp(field,"alignment"))	ptr = (long*)&mob->alignment;
	else if(!str_cmp(field,"bashed"))	ptr = (long*)&mob->bashed;
	else if(!str_cmp(field,"bind"))		ptr = (long*)&mob->bind;
	else if(!str_cmp(field,"bomb"))		ptr = (long*)&mob->bomb;
	else if(!str_cmp(field,"brew"))		ptr = (long*)&mob->brew;
	else if(!str_cmp(field,"cast"))		ptr = (long*)&mob->cast;
	else if(!str_cmp(field,"comm"))		{ ptr = IS_NPC(mob)?NULL:(long*)&mob->comm; allowpc = true; allowarith = false; min_sec = 7; flags = comm_flags; }		// 20140512NIB - Allows for scripted fun with player communications, only bit operators allowed
	else if(!str_cmp(field,"damroll"))	ptr = (long*)&mob->damroll;
	else if(!str_cmp(field,"damtype"))	{ ptr = (long*)&mob->dam_type; allowpc = false; allowarith = false; min_sec = 7; lookup_attack_type = true; }
	else if(!str_cmp(field,"danger"))	{ ptr = IS_NPC(mob)?NULL:(long*)&mob->pcdata->danger_range; allowpc = true; }
	else if(!str_cmp(field,"daze"))		ptr = (long*)&mob->daze;
	else if(!str_cmp(field,"death"))	{ ptr = (IS_NPC(mob) || !IS_DEAD(mob))?NULL:(long*)&mob->time_left_death; allowpc = true; }
	else if(!str_cmp(field,"dicenumber"))	{ ptr = IS_NPC(mob)?(long*)&mob->damage.number:NULL; }
	else if(!str_cmp(field,"dicetype"))	{ ptr = IS_NPC(mob)?(long*)&mob->damage.size:NULL; }
	else if(!str_cmp(field,"dicebonus"))	{ ptr = IS_NPC(mob)?(long*)&mob->damage.bonus:NULL; }
	else if(!str_cmp(field,"drunk"))	{ ptr = IS_NPC(mob)?NULL:(long*)&mob->pcdata->condition[COND_DRUNK]; allowpc = true; }
//	else if(!str_cmp(field,"exitdir"))	{ ptr = (long*)&mob->exit_dir; allowpc = true; }
	else if(!str_cmp(field,"exp"))		{ ptr = (long*)&mob->exp; allowpc = true; }
	else if(!str_cmp(field,"fade"))		ptr = (long*)&mob->fade;
	else if(!str_cmp(field,"fullness"))	{ ptr = IS_NPC(mob)?NULL:(long*)&mob->pcdata->condition[COND_FULL]; allowpc = true; }
	else if(!str_cmp(field,"gold"))		ptr = (long*)&mob->gold;
	else if(!str_cmp(field,"hide"))		ptr = (long*)&mob->hide;
	else if(!str_cmp(field,"hit"))		ptr = (long*)&mob->hit;
	else if(!str_cmp(field,"hitdamage"))	ptr = (long*)&mob->hit_damage;
	else if(!str_cmp(field,"hitroll"))	ptr = (long*)&mob->hitroll;
	else if(!str_cmp(field,"hunger"))	{ ptr = IS_NPC(mob)?NULL:(long*)&mob->pcdata->condition[COND_HUNGER]; allowpc = true; }
	else if(!str_cmp(field,"imm"))		{ ptr = (long*)&mob->imm_flags; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"level"))	ptr = (long*)&mob->tot_level;
	else if(!str_cmp(field,"lostparts"))	{ ptr = (long*)&mob->lostparts; allowarith = false; flags = part_flags; }
	else if(!str_cmp(field,"mana"))		ptr = (long*)&mob->mana;
	else if(!str_cmp(field,"manastore"))	{ ptr = (long*)&mob->manastore; allowpc = true; }
//	else if(!str_cmp(field,"material"))	ptr = (long*)&mob->material;
	else if(!str_cmp(field,"maxexp"))	ptr = (long*)&mob->maxexp;
	else if(!str_cmp(field,"maxhit"))	ptr = (long*)&mob->max_hit;
	else if(!str_cmp(field,"maxmana"))	ptr = (long*)&mob->max_mana;
	else if(!str_cmp(field,"maxmove"))	ptr = (long*)&mob->max_move;
	else if(!str_cmp(field,"mazed"))	{ ptr = (IS_NPC(mob))?NULL:(long*)&mob->maze_time_left; allowpc = true; }
	else if(!str_cmp(field,"modcon"))	{ ptr = (long*)&mob->mod_stat[STAT_CON]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_CON; }
	else if(!str_cmp(field,"moddex"))	{ ptr = (long*)&mob->mod_stat[STAT_DEX]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_DEX; }
	else if(!str_cmp(field,"modint"))	{ ptr = (long*)&mob->mod_stat[STAT_INT]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_INT; }
	else if(!str_cmp(field,"modstr"))	{ ptr = (long*)&mob->mod_stat[STAT_STR]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_STR; }
	else if(!str_cmp(field,"modwis"))	{ ptr = (long*)&mob->mod_stat[STAT_WIS]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_WIS; }
	else if(!str_cmp(field,"move"))		ptr = (long*)&mob->move;
	else if(!str_cmp(field,"music"))	ptr = (long*)&mob->music;
	else if(!str_cmp(field,"norecall"))	ptr = (long*)&mob->no_recall;
	else if(!str_cmp(field,"panic"))	ptr = (long*)&mob->panic;
	else if(!str_cmp(field,"paralyzed"))	ptr = (long*)&mob->paralyzed;
	else if(!str_cmp(field,"paroxysm"))	ptr = (long*)&mob->paroxysm;
	else if(!str_cmp(field,"parts"))	{ ptr = (long*)&mob->parts; allowarith = false; flags = part_flags; }
	else if(!str_cmp(field,"permaffects"))	{ ptr = (long*)&mob->affected_by_perm; bank = affect_flagbank; }
//	else if(!str_cmp(field,"permaffects2"))	{ ptr = (long*)&mob->affected_by_perm[1]; allowarith = false; flags = affect2_flags; }
	else if(!str_cmp(field,"permimm"))	{ ptr = (long*)&mob->imm_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"permres"))	{ ptr = (long*)&mob->res_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"permvuln"))	{ ptr = (long*)&mob->vuln_flags_perm; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"pktimer"))	ptr = (long*)&mob->pk_timer;
	else if(!str_cmp(field,"pneuma"))	ptr = (long*)&mob->pneuma;
	else if(!str_cmp(field,"practice"))	ptr = (long*)&mob->practice;
	else if(!str_cmp(field,"race"))		{ ptr = (long*)&mob->race; min_sec = 7; allowarith = false; lookuprace = true; }
	else if(!str_cmp(field,"ranged"))	ptr = (long*)&mob->ranged;
	else if(!str_cmp(field,"recite"))	ptr = (long*)&mob->recite;
	else if(!str_cmp(field,"res"))		{ ptr = (long*)&mob->res_flags;  allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"resurrect"))	ptr = (long*)&mob->resurrect;
	else if(!str_cmp(field,"reverie"))	ptr = (long*)&mob->reverie;
	else if(!str_cmp(field,"scribe"))	ptr = (long*)&mob->scribe;
	else if(!str_cmp(field,"sex"))		{ ptr = (long*)&mob->sex; min = 0; max = 2; hasmin = hasmax = true; flags = sex_flags; }
	else if(!str_cmp(field,"silver"))	ptr = (long*)&mob->silver;
	else if(!str_cmp(field,"size"))		{ ptr = (long*)&mob->size; min = SIZE_TINY; max = SIZE_GIANT; hasmin = hasmax = true; flags = size_flags; }
	else if(!str_cmp(field,"skillchance"))	ptr = (long*)&mob->skill_chance;
	else if(!str_cmp(field,"sublevel"))	ptr = (long*)&mob->level;
	else if(!str_cmp(field,"tempstore1"))	{ ptr = (long*)&mob->tempstore[0]; allowpc = true; }
	else if(!str_cmp(field,"tempstore2"))	{ ptr = (long*)&mob->tempstore[1]; allowpc = true; }
	else if(!str_cmp(field,"tempstore3"))	{ ptr = (long*)&mob->tempstore[2]; allowpc = true; }
	else if(!str_cmp(field,"tempstore4"))	{ ptr = (long*)&mob->tempstore[3]; allowpc = true; }
	else if(!str_cmp(field,"thirst"))	{ ptr = IS_NPC(mob)?NULL:(long*)&mob->pcdata->condition[COND_THIRST]; allowpc = true; }
	else if(!str_cmp(field,"toxinneuro"))	ptr = (long*)&mob->toxin[TOXIN_NEURO];
	else if(!str_cmp(field,"toxinpara"))	ptr = (long*)&mob->toxin[TOXIN_PARALYZE];
	else if(!str_cmp(field,"toxinvenom"))	ptr = (long*)&mob->toxin[TOXIN_VENOM];
	else if(!str_cmp(field,"toxinweak"))	ptr = (long*)&mob->toxin[TOXIN_WEAKNESS];
	else if(!str_cmp(field,"train"))	ptr = (long*)&mob->train;
	else if(!str_cmp(field,"trance"))	ptr = (long*)&mob->trance;
	else if(!str_cmp(field,"vuln"))		{ ptr = (long*)&mob->vuln_flags; allowarith = false; flags = imm_flags; }
	else if(!str_cmp(field,"wait"))		ptr = (long*)&mob->wait;
	else if(!str_cmp(field,"wildviewx"))	ptr = (long*)&mob->wildview_bonus_x;
	else if(!str_cmp(field,"wildviewy"))	ptr = (long*)&mob->wildview_bonus_y;
	else if(!str_cmp(field,"wimpy"))	ptr = (long*)&mob->wimpy;

	if(!ptr) return;


	// MINIMUM to alter ANYTHING not allowed on players on a player
	if(!allowpc && !IS_NPC(mob)) min_sec = 9;

	if(script_security < min_sec) {
		sprintf(buf,"OpAlterMob - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
	else if( lookup_attack_type )
	{
		if( arg->type != ENT_STRING ) return;

		// This is an attack type, can only be assigned.
		allowarith = false;
		allowbitwise = false;
		value = attack_lookup(arg->d.str);
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
			bug("OpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr += value; break;
	case '-':
		if( !allowarith ) {
			bug("OpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr -= value; break;
	case '*':
		if( !allowarith ) {
			bug("OpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr *= value; break;
	case '/':
		if( !allowarith ) {
			bug("OpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("OpAlterMob - altermob called with operator / and value 0", 0);
			return;
		}
		*ptr /= value;
		break;
	case '%':
		if( !allowarith ) {
			bug("OpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("OpAlterMob - altermob called with operator % and value 0", 0);
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
			bug("OpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
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
			bug("OpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
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
			bug("OpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
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
			bug("OpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
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


SCRIPT_CMD(do_opstringmob)
{
	char buf[MSL+2],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	CHAR_DATA *mob = NULL;

	bool newlines = false;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpStringMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("OpStringMob - NULL mobile.", 0);
		return;
	}

	if(!IS_NPC(mob)) {
		bug("OpStringMob - can't change strings on PCs.", 0);
		return;
	}

	if(!*rest) {
		bug("OpStringMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpStringMob - Error in parsing.",0);
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

	if(buffer->string[0] != '\0')
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
			sprintf(buf,"OpStringMob - Attempting to restring '%s' with security %d.\n\r", field, script_security);
			bug(buf, 0);
			free_buf(buffer);
			return;
		}

		char *p = buf_string(buffer);
		strip_newline(p, newlines);

		free_string(*str);
		*str = str_dup(p);
	}
	free_buf(buffer);
}

SCRIPT_CMD(do_opskimprove)
{
	char skill[MIL],*rest;
	int min_diff, diff, sn =-1;
	CHAR_DATA *mob = NULL;

	TOKEN_DATA *token = NULL;
	bool success = false;

	if(script_security < MIN_SCRIPT_SECURITY) {
		bug("OpSkImprove - Insufficient security.",0);
		return;
	}

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	case ENT_TOKEN:
		token = arg->d.token;
	default: break;
	}

	if(!mob && !token) {
		bug("OpSkImprove - NULL target.", 0);
		return;
	}

	if(mob) {
		if(IS_NPC(mob)) {
			bug("OpSkImprove - NPCs don't have skills to improve yet...", 0);
			return;
		}


		if(!(rest = expand_argument(info,rest,arg))) {
			bug("OpSkImprove - Error in parsing.",0);
			return;
		}

		skill[0] = 0;

		switch(arg->type) {
		case ENT_STRING: strncpy(skill,arg->d.str,MIL-1); break;
		default: return;
		}

		if(!skill[0]) return;

		sn = skill_lookup(skill);

		if(sn < 1) return;
	} else {
		if(token->pIndexData->type != TOKEN_SKILL && token->pIndexData->type != TOKEN_SPELL) {
			bug("OpSkImprove - Token is not a spell token...", 0);
			return;
		}
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: diff = arg->d.num; break;
	default: return;
	}

	min_diff = 10 - script_security;	// min=10, max=1

	if(diff < min_diff) {
		bug("OpSkImprove - Attempting to use a difficulty multiplier lower than allowed.",0);
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

	if(token)
		token_skill_improve(token->player,token,success,diff);
	else
		check_improve( mob, sn, success, diff );
}


SCRIPT_CMD(do_oprawkill)
{
	char *rest;
	int type;
	bool has_head, show_msg;
	CHAR_DATA *mob = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("OpRawkill - NULL mobile.", 0);
		return;
	}

	if(IS_IMMORTAL(mob)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
	default: return;
	}

	if(type < 0 || type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpRawkill - Error in parsing.",0);
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
		bug("OpRawkill - Error in parsing.",0);
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
		if(!p_percent_trigger(mob, NULL, NULL, NULL, mob, mob, NULL, NULL, NULL, TRIG_DEATH, NULL))
			p_percent_trigger(NULL, NULL, here, NULL, mob, mob, NULL, NULL, NULL, TRIG_DEATH, NULL);
	}

	raw_kill(mob, has_head, show_msg, type);
}


SCRIPT_CMD(do_opaddaffect)
{
	char *rest;
	int where, group, level, loc, mod, hours;
	int skill;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffect <target> <where> <skill> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, obj_room(info->obj), arg->d.str)))
			obj = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("OpAddaffect - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
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
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
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
		bug("OpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAddaffect - Error in parsing.",0);
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
	af.type      = skill;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.custom_name = NULL;
	af.slot = wear_loc;
	if(mob) affect_join(mob, &af);
	else affect_join_obj(obj,&af);
}

SCRIPT_CMD(do_opaddaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffectname <target> <where> <name> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,obj_room(info->obj), arg->d.str)))
			obj = get_obj_here(NULL,obj_room(info->obj), arg->d.str);
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
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
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


SCRIPT_CMD(do_opstripaffect)
{
	char *rest;
	int skill;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffect <target> <skill>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, obj_room(info->obj), arg->d.str)))
			obj = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("OpStripaffect - NULL target.", 0);
		return;
	}


	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpStripaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
	}

	if(skill < 0) return;

	if(mob) affect_strip(mob, skill);
	else affect_strip_obj(obj,skill);
}

SCRIPT_CMD(do_opstripaffectname)
{
	char *rest, *name;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffectname <target> <name>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,obj_room(info->obj), arg->d.str)))
			obj = get_obj_here(NULL,obj_room(info->obj), arg->d.str);
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

SCRIPT_CMD(do_opinput)
{
	char *rest, *p;
	int vnum;
	CHAR_DATA *mob = NULL;


	if(!info || !info->obj) return;

	info->obj->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("OpInput - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: return;
	}

	if(vnum < 1 || !get_script_index(vnum, PRG_OPROG)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpInput - Error in parsing.",0);
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
	mob->desc->input_script = vnum;
	mob->desc->input_mob = NULL;
	mob->desc->input_obj = info->obj;
	mob->desc->input_room = NULL;
	mob->desc->input_tok = NULL;

	info->obj->progs->lastreturn = 1;
	free_buf(buffer);
}

SCRIPT_CMD(do_opusecatalyst)
{
	char *rest;
	int type, method, amount, min, max, show;
	CHAR_DATA *mob = NULL;
	ROOM_INDEX_DATA *room = NULL;


	if(!info || !info->obj) return;

	info->obj->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	// usecatalyst <target> <type> <method> <amount> <min> <max> <show>

	switch(arg->type) {
	case ENT_STRING: mob = get_char_room(NULL,obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	default: break;
	}

	if(!mob && !room) {
		bug("OpUseCatalyst - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
	default: return;
	}

	if(type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
	default: return;
	}

	if(method == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: amount = arg->d.num; break;
	case ENT_STRING: amount = atoi(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: min = arg->d.num; break;
	case ENT_STRING: min = atoi(arg->d.str); break;
	default: return;
	}

	if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: max = arg->d.num; break;
	case ENT_STRING: max = atoi(arg->d.str); break;
	default: return;
	}

	if(max < min || max > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: show = flag_value(boolean_types,arg->d.str); break;
	default: return;
	}

	if(show == NO_FLAG) return;

	info->obj->progs->lastreturn = use_catalyst(mob,room,type,method,amount,min,max,(bool)show);
}

SCRIPT_CMD(do_opalterexit)
{
	char buf[MSL+2],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY, door;
	ROOM_INDEX_DATA *room;
	EXIT_DATA *ex = NULL;
	int *ptr = NULL;
	int16_t *sptr = NULL;
	char **str;
	int min, max;
	bool hasmin = false, hasmax = false;
	bool allowarith = true;
	const struct flag_type *flags = NULL;

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterExit - Error in parsing.",0);
		return;
	}

	room = obj_room(info->obj);

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
			bug("OpAlterExit - Error in parsing.",0);
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
		bug("OpAlterExit - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAlterExit - Error in parsing.",0);
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
			bug("OpAlterExit - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER:	room = get_room_index(arg->d.num); break;
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
	else if(!str_cmp(field,"material"))	str = &ex->door.material;
	else if(!str_cmp(field,"short"))	str = &ex->short_desc;

	if(str) {
		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if(!buffer->string[0]) {
			bug("OpAlterExit - Empty string used.",0);
			free_buf(buffer);
			return;
		}

		free_string(*str);
		*str = str_dup(buffer->string);
		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterExit - Error in parsing.",0);
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
	else if(!str_cmp(field,"lock"))				{ ptr = (int*)&ex->door.lock.flags; flags = lock_flags; }
	else if(!str_cmp(field,"lockreset"))		{ ptr = (int*)&ex->door.rs_lock.flags; flags = lock_flags; min_sec = 7; }
	else if(!str_cmp(field,"key"))				ptr = (int*)&ex->door.lock.key_vnum;
	else if(!str_cmp(field,"keyreset"))			{ ptr = (int*)&ex->door.rs_lock.key_vnum; min_sec = 7; }
	else if(!str_cmp(field,"pick"))				{ ptr = (int*)&ex->door.lock.pick_chance; min = 0; max = 100; hasmin = hasmax = true; }
	else if(!str_cmp(field,"pickreset"))		{ ptr = (int*)&ex->door.rs_lock.pick_chance; min_sec = 7; min = 0; max = 100; hasmin = hasmax = true; }

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"OpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
		else if( flags == lock_flags )
		{
			int keep = LOCK_CREATED;

			if( !IS_SET(*ptr, LOCK_CREATED) )
			{
				// OLC created locks can lose noremove
				SET_BIT(value, LOCK_NOREMOVE);
				SET_BIT(value, LOCK_NOJAM);
			}

			REMOVE_BIT(value, keep);
			if( (buf[0] == '=') || (buf[0] == '&') )
			{
				value |= (*ptr & keep);
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
				bug("OpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr += value;
			break;
		case '-':
			if( !allowarith ) {
				bug("OpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr -= value;
			break;
		case '*':
			if( !allowarith ) {
				bug("OpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr *= value;
			break;
		case '/':
			if( !allowarith ) {
				bug("OpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("OpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("OpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("OpAlterExit - adjust called with operator % and value 0", 0);
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
				bug("OpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("OpAlterExit - adjust called with operator % and value 0", 0);
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

// SYNTAX: obj prompt <player> <name>[ <string>]
SCRIPT_CMD(do_opprompt)
{
	char name[MIL],*rest;
	CHAR_DATA *mob = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpPrompt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,obj_room(info->obj), arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("OpPrompt - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob)) {
		bug("OpPrompt - cannot set prompt strings on NPCs.", 0);
		return;
	}

	if(!*rest) {
		bug("OpPrompt - Missing name type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpPrompt - Error in parsing.",0);
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

SCRIPT_CMD(do_opvarseton)
{

	VARIABLE **vars;

	if(!info || !info->obj) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? &arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? &arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? &arg->d.token->progs->vars : NULL; break;
	default: vars = NULL; break;
	}

	script_varseton(info, vars, argument, arg);
}

SCRIPT_CMD(do_opvarclearon)
{

	VARIABLE **vars;

	if(!info || !info->obj) return;

	// Get the target
	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: vars = (arg->d.mob && IS_NPC(arg->d.mob) && arg->d.mob->progs) ? &arg->d.mob->progs->vars : NULL; break;
	case ENT_OBJECT: vars = (arg->d.obj && arg->d.obj->progs) ? &arg->d.obj->progs->vars : NULL; break;
	case ENT_ROOM: vars = (arg->d.room && arg->d.room->progs) ? &arg->d.room->progs->vars : NULL; break;
	case ENT_TOKEN: vars = (arg->d.token && arg->d.token->progs) ? &arg->d.token->progs->vars : NULL; break;
	default: vars = NULL; break;
	}

	script_varclearon(info,vars,argument, arg);
}

SCRIPT_CMD(do_opvarsaveon)
{
	char name[MIL],buf[MIL];
	bool on;

	VARIABLE *vars;

	if(!info || !info->obj) return;

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
SCRIPT_CMD(do_opcloneroom)
{
	char name[MIL];
	long vnum;
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	TOKEN_DATA *tok;
	ROOM_INDEX_DATA *source, *room, *clone;
	bool no_env = false;

	if(!info || !info->obj) return;

	info->progs->lastreturn = 0;

	// Get vnum
	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	vnum = arg->d.num;

	source = get_room_index(vnum);
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

	log_stringf("do_opcloneroom: variable name '%s'\n", name);

	clone = create_virtual_room(source,false,false);
	if(!clone) return;

	log_stringf("do_opcloneroom: cloned room %ld:%ld\n", clone->vnum, clone->id[1]);

	if(!no_env)
		room_to_environment(clone,mob,obj,room,tok);

	variables_set_room(info->var,name,clone);

	info->progs->lastreturn = 1;
}

// alterroom <room> <field> <parameters>
/*
SCRIPT_CMD(do_opalterroom)
{
	char buf[MSL+2],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY;
	ROOM_INDEX_DATA *room;
	WILDS_DATA *wilds;

	int *ptr = NULL;
	int16_t *sptr = NULL;
	char **str;
	bool allow_empty = false;
	bool allowarith = true;
	bool allowbitwise = true;
	const struct flag_type *flags = NULL;
	const struct flag_type **bank = NULL;
	long temp_flags[4];

	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterRoom - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		break;
	case ENT_NUMBER:
		room = get_room_index(arg->d.num);
		break;
	default: room = NULL; break;
	}

	if(!room || !room_is_clone(room)) return;

	if(!*rest) {
		bug("OpAlterRoom - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAlterRoom - Error in parsing.",0);
		return;
	}

	field[0] = 0;

	switch(arg->type) {
	case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
	default: return;
	}

	if(!field[0]) return;

        if(!str_cmp(field,"mapid")) {
                if(!(rest = expand_argument(info,rest,arg))) {
                        bug("OpAlterRoom - Error in parsing.",0);
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

	// Setting the environment of a clone room
	if(!str_cmp(field,"environment") || !str_cmp(field,"environ") ||
		!str_cmp(field,"extern") || !str_cmp(field,"outside")) {

		if(!(rest = expand_argument(info,rest,arg))) {
			bug("OpAlterRoom - Error in parsing.",0);
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
			sprintf(buf,"OpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
			bug(buf, 0);
			return;
		}

		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if(!allow_empty && !buffer->string[0]) {
			bug("OpAlterRoom - Empty string used.",0);
			free_buf(buffer);
			return;
		}

		free_string(*str);
		*str = str_dup(buffer->string);
		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpAlterRoom - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"flags"))			{ ptr = (int*)room->room_flag; bank = room_flagbank; }
	else if(!str_cmp(field,"light"))	ptr = (int*)&room->light;
	else if(!str_cmp(field,"sector"))	ptr = (int*)&room->sector_type;
	else if(!str_cmp(field,"heal"))		{ ptr = (int*)&room->heal_rate; min_sec = 9; }
	else if(!str_cmp(field,"mana"))		{ ptr = (int*)&room->mana_rate; min_sec = 9; }
	else if(!str_cmp(field,"move"))		{ ptr = (int*)&room->move_rate; min_sec = 1; }
	else if(!str_cmp(field,"mapx"))		{ ptr = (int*)&room->x; min_sec=5; }
	else if(!str_cmp(field,"mapy"))		{ ptr = (int*)&room->y; min_sec=5; }
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&room->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&room->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&room->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&room->tempstore[3];

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"OpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		bug(buf, 0);
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

		// ROOM2 has flags that cannot be manipulated by alterroom
		if( flags == room2_flags )
		{
			REMOVE_BIT(value, ROOM_NOCLONE);

			if( buf[0] == '=' || buf[0] == '&' )
			{
				if( IS_SET(*ptr, ROOM_NOCLONE) ) SET_BIT(value, ROOM_NOCLONE);
				if( IS_SET(*ptr, ROOM_VIRTUAL_ROOM) ) SET_BIT(value, ROOM_VIRTUAL_ROOM);
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
				bug("OpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value; break;
		case '-':
			if( !allowarith ) {
				bug("OpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value; break;
		case '*':
			if( !allowarith ) {
				bug("OpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value; break;
		case '/':
			if( !allowarith ) {
				bug("OpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("OpAlterRoom - alterroom called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("OpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("OpAlterRoom - alterroom called with operator % and value 0", 0);
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
				bug("OpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
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
				bug("OpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
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
				bug("OpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
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
				bug("OpAlterRoom - alterroom called with bitwise operator on a non-bitvector field.", 0);
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
				bug("OpAlterRoom - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("OpAlterRoom - adjust called with operator % and value 0", 0);
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
*/

// destroyroom <vnum> <id> <id>
// destroyroom <room>
SCRIPT_CMD(do_opdestroyroom)
{
	long vnum;
	unsigned long id1, id2;
	ROOM_INDEX_DATA *room;


	if(!info || !info->obj) return;

	info->obj->progs->lastreturn = 0;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	// It's a room, extract it directly
	if(arg->type == ENT_ROOM) {
		// Need to block this when done by room to itself
		if(extract_clone_room(arg->d.room->source,arg->d.room->id[0],arg->d.room->id[1],false))
			info->obj->progs->lastreturn = 1;
		return;
	}

	if(arg->type != ENT_NUMBER) return;

	vnum = arg->d.num;

	room = get_room_index(vnum);
	if(!room) return;

	// Get id
	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	id1 = arg->d.num;

	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	id2 = arg->d.num;

	if(extract_clone_room(room, id1, id2,false))
		info->obj->progs->lastreturn = 1;
}

// showroom <viewer> map <mapid> <x> <y> <z> <scale> <width> <height>[ force]
// showroom <viewer> room <room>[ force]
// showroom <viewer> vroom <room> <id>[ force]
SCRIPT_CMD(do_opshowroom)
{
	CHAR_DATA *viewer = NULL, *next;
	ROOM_INDEX_DATA *room = NULL, *dest;
	WILDS_DATA *wilds = NULL;

	long mapid;
	long x,y;
	long width, height;
	bool force;

	if(!info || !info->obj) return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE:	viewer = arg->d.mob; break;
	case ENT_ROOM:		room = arg->d.room; break;
	}

	if(!viewer && !room) {
		bug("OpShowMap - bad target for showing the map", 0);
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


// do_opxcall
SCRIPT_CMD(do_opxcall)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;
	CHAR_DATA *vch,*ch;
	OBJ_DATA *obj1,*obj2;
	SCRIPT_DATA *script;
	int depth, vnum, ret, space = PRG_MPROG;


	if(!info || !info->obj) return;

	if (!argument[0]) {
		bug("OpCall: missing arguments from vnum %d.", VNUM(info->obj));
		return;
	}

	if(script_security < 5) {
		bug("OpCall: Minimum security needed is 5.", VNUM(info->obj));
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("OpCall: maximum call depth exceeded for obj vnum %d.", VNUM(info->obj));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; space = PRG_MPROG; break;
	case ENT_OBJECT: obj = arg->d.obj; space = PRG_OPROG; break;
	case ENT_ROOM: room = arg->d.room; space = PRG_RPROG; break;
	case ENT_TOKEN: token = arg->d.token; space = PRG_TPROG; break;
	}

	if(!mob && !obj && !room && !token) {
		bug("OpCall: No entity target from vnum %ld.", VNUM(info->obj));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(mob && !IS_NPC(mob)) {
		bug("OpCall: Invalid target for xcall.  Players cannot do scripts.", 0);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(script = get_script_index(vnum, space))) {
		bug("OpCall: invalid prog from vnum %d.", VNUM(info->obj));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, obj_room(info->obj),arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, obj_room(info->obj), arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->obj)
		info->obj->progs->lastreturn = ret;
	else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}


// do_opchargebank
// obj chargebank <player> <gold>
SCRIPT_CMD(do_opchargebank)
{
	char *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpChargeBank - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("OpChargeBank - Non-player victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("OpChargeBank - Error in parsing from vnum %ld.", VNUM(info->obj));
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

// do_opwiretransfer
// obj wiretransfer <player> <gold>
// Limited to 1000 gold for security scopes less than 7.
SCRIPT_CMD(do_opwiretransfer)
{
	char buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,obj_room(info->obj), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("OpWireTransfer - Non-player victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("OpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->obj));
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
		sprintf(buf, "OpWireTransfer logged: attempted to wire %d gold to %s in room %ld by %ld", amount, victim->name, info->obj->in_room->vnum, info->obj->pIndexData->vnum);
		log_string(buf);
		amount = 1000;
	}

	victim->pcdata->bankbalance += amount;

	sprintf(buf, "OpWireTransfer logged: %s was wired %d gold in room %ld by %ld", victim->name, amount, info->obj->in_room->vnum, info->obj->pIndexData->vnum);
	log_string(buf);
}

// do_opsetrecall
// obj setrecall $MOBILE|$ROOM <location>
// Sets the recall point of the target mobile to the reference of the location
SCRIPT_CMD(do_opsetrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
	ROOM_INDEX_DATA *room;
	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpSetRecall - Bad syntax from vnum %ld.", VNUM(info->obj));
		return;
	}

	victim = NULL;
	room = NULL;

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	default: victim = NULL; room = NULL; break;
	}


	if (!victim && !room) {
		bug("OpSetRecall - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	argument = op_getlocation(info, rest, &location);

	if(!location) {
		bug("OpSetRecall - Bad location from vnum %d.", VNUM(info->obj));
		return;
	}

	if (victim)
	{
		if(location->wilds)
			location_set(&victim->recall,location->wilds->uid,location->x,location->y,location->z);
		else if(location->source)
			location_set(&victim->recall,0,location->vnum,0,0);
		else
			location_set(&victim->recall,0,location->vnum,location->id[0],location->id[1]);
	}

	if (room)
	{
		if(location->wilds)
			location_set(&room->recall,location->wilds->uid,location->x,location->y,location->z);
		else if(location->source)
			location_set(&room->recall,0,location->vnum,0,0);
		else
			location_set(&room->recall,0,location->vnum,location->id[0],location->id[1]);
	}	
}

// do_opclearrecall
// obj clearrecall $MOBILE
// Clears the special recall field on the $MOBILE
SCRIPT_CMD(do_opclearrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
//	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpClearRecall - Bad syntax from vnum %ld.", VNUM(info->obj));
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
		bug("OpClearRecall - Null victim from vnum %ld.", VNUM(info->obj));
		return;
	}

	victim->recall.wuid = 0;
	victim->recall.id[0] = 0;
	victim->recall.id[1] = 0;
	victim->recall.id[2] = 0;
}

// HUNT <HUNTER> <PREY>
SCRIPT_CMD(do_ophunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	CHAR_DATA *prey = NULL;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("OpHunt - Null hunter from vnum %ld.", VNUM(info->obj));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("OpHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: prey = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: prey = arg->d.mob; break;
	default: prey = NULL; break;
	}

	if (!prey) {
		bug("OpHunt - Null prey from vnum %ld.", VNUM(info->obj));
		return;
	}

	hunt_char(hunter, prey);
	return;
}

// STOPHUNT <STAY> <HUNTER>
SCRIPT_CMD(do_opstophunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	bool stay;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING) {
		bug("OpStopHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

	if(!expand_argument(info,rest,arg)) {
		bug("OpStopHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("OpStopHunt - Null hunter from vnum %ld.", VNUM(info->obj));
		return;
	}

	stop_hunt(hunter, stay);
	return;
}

// Format: PERSIST <MOBILE or OBJECT or ROOM> <STATE>
SCRIPT_CMD(do_oppersist)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	bool persist = false, current = false;


	if(!info || !info->obj) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("OpPersist - Error in parsing from vnum %ld.", VNUM(info->obj));
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; current = mob->persist; break;
	case ENT_OBJECT: obj = arg->d.obj; current = obj->persist; break;
	case ENT_ROOM: room = arg->d.room; current = room->persist; break;
	}

	if(!mob && !obj && !room) {
		bug("OpPersist - NULL target.", VNUM(info->obj));
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
		if( get_blueprint_section_byroom(room->vnum) ) return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpPersist - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:   persist = !current; break;
	case ENT_STRING: persist = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"on"); break;
	default: return;
	}

	// Require security to ENABLE persistance
	if(!current && persist && script_security < MAX_SCRIPT_SECURITY) {
		bug("OpPersist - Insufficient security to enable persistance.", VNUM(info->obj));
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

// obj skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_opskill)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int sn, value;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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


// obj skillgroup <player> add|remove <group>
SCRIPT_CMD(do_opskillgroup)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int gn;
	bool fAdd = false;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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

	gn = group_lookup(arg->d.str);
	if( gn != -1)
	{
		if( fAdd )
		{
			if( !mob->pcdata->group_known[gn] )
				gn_add(mob,gn);
		}
		else
		{
			if( mob->pcdata->group_known[gn] )
				gn_remove(mob,gn);
		}
	}

	return;
}

// obj condition $PLAYER <condition> <value>
// Adjusts the specified condition by the given value
SCRIPT_CMD(do_opcondition)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int cond, value;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_opaddspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_new;
	OBJ_DATA *target;
	int level;
	int sn;
	AFFECT_DATA *paf;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
}


// remspell $OBJECT STRING[ silent]
SCRIPT_CMD(do_opremspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_prev;
	OBJ_DATA *target;
	int level;
	int sn;
	bool found = false, show = true;
	AFFECT_DATA *paf;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
}


// alteraffect $AFFECT STRING OP NUMBER
// Current limitations: only level and duration
// Altering other aspects such as modifiers will require updating the owner of the affect, which isn't available here
SCRIPT_CMD(do_opalteraffect)
{
	char buf[MIL],field[MIL],*rest;

	AFFECT_DATA *paf;
	int value;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_AFFECT || !arg->d.aff) return;

	paf = arg->d.aff;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("OpAlterAffect - Error in parsing.",0);
		return;
	}

	if( IS_NULLSTR(rest) ) return;

	if( arg->type != ENT_STRING || IS_NULLSTR(arg->d.str) ) return;

	strncpy(field,arg->d.str,MIL-1);


	if( !str_cmp(field, "level") ) {
		argument = one_argument(rest,buf);

		if(!(rest = expand_argument(info,argument,arg))) {
			bug("OpAlterAffect - Error in parsing.",0);
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
			bug("OpAlterAffect - Error in parsing.",0);
			return;
		}

		if( paf->slot != WEAR_NONE ) {
			bug("OpAlterAffect - Attempting to modify duration of an object given affect.",0);
			return;
		}

		if( paf->group == AFFGROUP_RACIAL ) {
			bug("OpAlterAffect - Attempting to modify duration of a racial affect.",0);
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
SCRIPT_CMD(do_opcrier)
{
	if(!info || !info->obj) return;

	BUFFER *buffer = new_buf();
	add_buf(buffer, "{M");
	expand_string(info,argument,buffer);

	if(buffer->string[2] != '\0')
	{
		add_buf(buffer, "{x");

		crier_announce(buffer->string);
	}
	free_buf(buffer);
}


// scriptwait $PLAYER NUMBER VNUM VNUM[ $ACTOR]
// - actor can be a $MOBILE, $OBJECT or $TOKEN
// - scripts must be available for the respective actor type
SCRIPT_CMD(do_opscriptwait)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int wait;
	long success, failure, pulse;
	TOKEN_DATA *actor_token = NULL;
	CHAR_DATA *actor_mob = NULL;
	OBJ_DATA *actor_obj = NULL;
	int prog_type;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	info->obj->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob ) return;	// only players

	// Check that the mob is not busy
	if( is_char_busy( mob ) ) {
		//send_to_char("script_wait: mob busy\n\r", mob);
		return;
	}
	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: wait = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: wait = arg->d.num; break;
	default: return;
	}

	//printf_to_char(mob, "script_wait: wait = %d\n\r", wait);


	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: success = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: success = arg->d.num; break;
	default: return;
	}

	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: failure = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: failure = arg->d.num; break;
	default: return;
	}

	if( !*rest) return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: pulse = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: pulse = arg->d.num; break;
	default: return;
	}

	actor_obj = info->obj;
	prog_type = PRG_OPROG;
	if(rest && *rest) {
		if(!(rest = expand_argument(info,rest,arg)))
			return;

		switch(arg->type) {
		case ENT_MOBILE:
			actor_mob = arg->d.mob;
			actor_obj = NULL;
			actor_token = NULL;
			prog_type = PRG_MPROG;
			break;

		case ENT_OBJECT:
			actor_mob = NULL;
			actor_obj = arg->d.obj;
			actor_token = NULL;
			prog_type = PRG_OPROG;
			break;

		case ENT_TOKEN:
			actor_mob = NULL;
			actor_obj = NULL;
			actor_token = arg->d.token;
			prog_type = PRG_TPROG;
			break;

		}
	}

	if(!actor_mob && !actor_obj && !actor_token) return;

	if(success < 1 || !get_script_index(success, prog_type)) return;
	if(failure < 1 || !get_script_index(failure, prog_type)) return;
	if(pulse > 0 && !get_script_index(pulse, prog_type)) return;

	wait = UMAX(wait, 1);


	mob->script_wait = wait;
	mob->script_wait_mob = actor_mob;
	mob->script_wait_obj = actor_obj;
	mob->script_wait_token = actor_token;
	if( actor_mob ) {
		mob->script_wait_id[0] = actor_mob->id[0];
		mob->script_wait_id[1] = actor_mob->id[1];
	} else if( actor_obj ) {
		mob->script_wait_id[0] = actor_obj->id[0];
		mob->script_wait_id[1] = actor_obj->id[1];
	} else if( actor_token ) {
		mob->script_wait_id[0] = actor_token->id[0];
		mob->script_wait_id[1] = actor_token->id[1];
	}
	mob->script_wait_success = get_script_index(success, prog_type);
	mob->script_wait_failure = get_script_index(failure, prog_type);
	mob->script_wait_pulse = (pulse > 0) ? get_script_index(pulse, prog_type) : NULL;

	//printf_to_char(mob, "script_wait started: wait = %d\n\r", wait);
	//printf_to_char(mob, "script_wait started: success = %d\n\r", success);
	//printf_to_char(mob, "script_wait started: failure = %d\n\r", failure);
	//printf_to_char(mob, "script_wait started: pulse = %d\n\r", pulse);

	// Return how long the command decided
	info->obj->progs->lastreturn = wait;
}

// Syntax: FIXAFFECTS $MOBILE
SCRIPT_CMD(do_opfixaffects)
{


	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	if(!expand_argument(info,argument,arg))
		return;

	if(arg->type != ENT_MOBILE) return;

	if(arg->d.mob == NULL) return;

	affect_fix_char(arg->d.mob);
}

// Syntax:	checkpoint $PLAYER $ROOM
// 			checkpoint $PLAYER VNUM
//			checkpoint $PLAYER none|clear|reset
//
// Sets the checkpoint of the $PLAYER to the destination or clears it.
// - When a checkpoint is set, it will override what location is saved to the pfile.
// - When setting a checkpoint via VNUM and an invalid vnum is given, it will clear the checkpoint.
SCRIPT_CMD(do_opcheckpoint)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
	case ENT_NUMBER:
		if( arg->d.num > 0 )
			mob->checkpoint = get_room_index(arg->d.num);
		break;
	case ENT_ROOM:
		if( arg->d.room != NULL )
			mob->checkpoint = arg->d.room;
		break;
	}
}

// Syntax: saveplayer $PLAYER
SCRIPT_CMD(do_opsaveplayer)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	info->obj->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob)) return;

	save_char_obj(mob);

	info->obj->progs->lastreturn = 1;
}

// Syntax: remort $PLAYER
//  - prompts them for a class out of what they can do
SCRIPT_CMD(do_opremort)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	info->obj->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob)) return;

	// Are they already being prompted
	if(mob->desc->input ||
		mob->pk_question ||
		mob->remove_question ||
		mob->personal_pk_question ||
		mob->cross_zone_question ||
		mob->pcdata->convert_church != -1 ||
		mob->challenged ||
		mob->remort_question)
		return;

	if(IS_REMORT(mob)) return;

    if (mob->tot_level < LEVEL_HERO) return;

    mob->remort_question = true;
    show_multiclass_choices(mob, mob);

	info->obj->progs->lastreturn = 1;
}

// Syntax: restore $MOBILE
SCRIPT_CMD(do_oprestore)
{
	char *rest;

	int amount = 100;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_opgroup)
{
	char *rest;

	CHAR_DATA *follower, *leader;
	bool fShow = true;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

	info->obj->progs->lastreturn = 0;

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
		info->obj->progs->lastreturn = 1;
}

// UNGROUP mobile[ bool(ALL=false)]
SCRIPT_CMD(do_opungroup)
{
	char *rest;

	bool fAll = false;

	if(!info || !info->obj || IS_NULLSTR(argument)) return;

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
