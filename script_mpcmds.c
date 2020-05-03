/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include "merc.h"
#include "scripts.h"
#include "recycle.h"
#include "wilds.h"
#include "tables.h"

//#define DEBUG_MODULE
#include "debug.h"


/*
 * Command table.
 */
const struct script_cmd_type mob_cmd_table[] = {
	{ "addaffect",			scriptcmd_addaffect,		TRUE,	TRUE	},
	{ "addaffectname",		scriptcmd_addaffectname,	TRUE,	TRUE	},
	{ "addspell",			do_mpaddspell,				TRUE,	TRUE	},
	{ "alteraffect",		do_mpalteraffect,			TRUE,	TRUE	},
	{ "alterexit",			do_mpalterexit,				FALSE,	TRUE	},
	{ "altermob",			do_mpaltermob,				TRUE,	TRUE	},
	{ "alterobj",			do_mpalterobj,				TRUE,	TRUE	},
	{ "alterroom",			do_mpalterroom,				TRUE,	TRUE	},
	{ "appear",				do_mpvis,					FALSE,	FALSE	},
	{ "applytoxin",			scriptcmd_applytoxin,		FALSE,	TRUE	},
	{ "asound", 			do_mpasound,				FALSE,	TRUE	},
	{ "assist",				do_mpassist,				FALSE,	TRUE	},
	{ "at",					do_mpat,					FALSE,	TRUE	},
	{ "attach",				scriptcmd_attach,			TRUE,	TRUE	},
	{ "award",				scriptcmd_award,			TRUE,	TRUE	},
	{ "breathe",			scriptcmd_breathe,		FALSE,	TRUE	},
	{ "call",				do_mpcall,					FALSE,	TRUE	},
	{ "cancel",				do_mpcancel,				FALSE,	FALSE	},
	{ "cast",				do_mpcast,					FALSE,	TRUE	},
	{ "chargebank",			do_mpchargebank,			FALSE,	TRUE	},
	{ "chargemoney",		do_mpchargemoney,			FALSE,	TRUE	},
	{ "checkpoint",			do_mpcheckpoint,			FALSE,	TRUE	},
	{ "cloneroom",			do_mpcloneroom,				TRUE,	TRUE	},
	{ "condition",			do_mpcondition,				FALSE,	TRUE	},
	{ "crier",				do_mpcrier,					FALSE,	TRUE	},
	{ "damage",				scriptcmd_damage,			FALSE,	TRUE	},
	{ "deduct",				scriptcmd_deduct,			TRUE,	TRUE	},
	{ "delay",				do_mpdelay,					FALSE,	TRUE	},
	{ "dequeue",			do_mpdequeue,				FALSE,	FALSE	},
	{ "destroyroom",		do_mpdestroyroom,			TRUE,	TRUE	},
	{ "detach",				scriptcmd_detach,			TRUE,	TRUE	},
	{ "disappear",    		do_mpinvis,					FALSE,	FALSE	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	TRUE,	TRUE	},
	{ "echo",				do_mpecho,					FALSE,	TRUE	},
	{ "echoaround",			do_mpechoaround,			FALSE,	TRUE	},
	{ "echoat",				do_mpechoat,				FALSE,	TRUE	},
	{ "echobattlespam",		do_mpechobattlespam,		FALSE,	TRUE	},
	{ "echochurch",			do_mpechochurch,			FALSE,	TRUE	},
	{ "echogrouparound",	do_mpechogrouparound,		FALSE,	TRUE	},
	{ "echogroupat",		do_mpechogroupat,			FALSE,	TRUE	},
	{ "echoleadaround",		do_mpecholeadaround,		FALSE,	TRUE	},
	{ "echoleadat",			do_mpecholeadat,			FALSE,	TRUE	},
	{ "echonotvict",		do_mpechonotvict,			FALSE,	TRUE	},
	{ "echoroom",			do_mpechoroom,				FALSE,	TRUE	},
	{ "ed",					scriptcmd_ed,				FALSE,	TRUE	},
	{ "entercombat",		scriptcmd_entercombat,		FALSE,	TRUE	},
	{ "fade",				scriptcmd_fade,				TRUE,	TRUE	},
	{ "fixaffects",			do_mpfixaffects,			FALSE,	TRUE	},
	{ "flee",				scriptcmd_flee,				FALSE,	FALSE	},
	{ "force",				do_mpforce,					FALSE,	TRUE	},
	{ "forget",				do_mpforget,				FALSE,	TRUE	},
	{ "gdamage",			do_mpgdamage,				FALSE,	TRUE	},
	{ "gecho",				do_mpgecho,					FALSE,	TRUE	},
	{ "gforce",				do_mpgforce,				FALSE,	TRUE	},
	{ "goto",				do_mpgoto,					FALSE,	TRUE	},
	{ "grantskill",			scriptcmd_grantskill,		FALSE,	TRUE	},
	{ "group",				do_mpgroup,					FALSE,	TRUE	},
	{ "gtransfer",			do_mpgtransfer,				FALSE,	TRUE	},
	{ "hunt",				do_mphunt,					FALSE,	TRUE	},
	{ "input",				do_mpinput,					FALSE,	TRUE	},
	{ "inputstring",		scriptcmd_inputstring,		FALSE,	TRUE	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	TRUE,	TRUE	},
	{ "interrupt",			do_mpinterrupt,				FALSE,	TRUE	},
	{ "junk",				do_mpjunk,					FALSE,	TRUE	},
	{ "kill",				do_mpkill,					FALSE,	TRUE	},
	{ "link",				do_mplink,					FALSE,	TRUE	},
	{ "mload",				do_mpmload,					FALSE,	TRUE	},
	{ "mute",				scriptcmd_mute,				FALSE,	TRUE	},
	{ "oload",				do_mpoload,					FALSE,	TRUE	},
	{ "otransfer",			do_mpotransfer,				FALSE,	TRUE	},
	{ "pageat",				scriptcmd_pageat,			FALSE,	TRUE	},
	{ "peace",				do_mppeace,					FALSE,	FALSE	},
	{ "persist",			do_mppersist,				FALSE,	TRUE	},
	{ "prompt",				do_mpprompt,				FALSE,	TRUE	},
	{ "purge",				do_mppurge,					FALSE,	FALSE	},
	{ "questaccept",		scriptcmd_questaccept,		FALSE,	TRUE	},
	{ "questcancel",		scriptcmd_questcancel,		FALSE,	TRUE	},
	{ "questcomplete",		scriptcmd_questcomplete,	FALSE,	TRUE	},
	{ "questgenerate",		scriptcmd_questgenerate,	FALSE,	TRUE	},
	{ "questpartcustom",	scriptcmd_questpartcustom,	TRUE,	TRUE	},
	{ "questpartgetitem",	scriptcmd_questpartgetitem,	TRUE,	TRUE	},
	{ "questpartgoto",		scriptcmd_questpartgoto,	TRUE,	TRUE	},
	{ "questpartrescue",	scriptcmd_questpartrescue,	TRUE,	TRUE	},
	{ "questpartslay",		scriptcmd_questpartslay,	TRUE,	TRUE	},
	{ "questscroll",		scriptcmd_questscroll,		FALSE,	TRUE	},
	{ "queue",				do_mpqueue,					FALSE,	TRUE	},
	{ "raisedead",			do_mpraisedead,				TRUE,	TRUE	},
	{ "rawkill",			do_mprawkill,				FALSE,	TRUE	},
	{ "remember",			do_mpremember,				FALSE,	TRUE	},
	{ "remort",				do_mpremort,				TRUE,	TRUE	},
	{ "remove",				do_mpremove,				FALSE,	TRUE	},
	{ "remspell",			do_mpremspell,				TRUE,	TRUE	},
	{ "resetdice",			do_mpresetdice,				TRUE,	TRUE	},
	{ "restore",			do_mprestore,				TRUE,	TRUE	},
	{ "revokeskill",		scriptcmd_revokeskill,		FALSE,	TRUE	},
	{ "saveplayer",			do_mpsaveplayer,			FALSE,	TRUE	},
	{ "scriptwait",			do_mpscriptwait,			FALSE,	TRUE	},
	{ "selfdestruct",		do_mpselfdestruct,			FALSE,	FALSE	},
	{ "setrecall",			do_mpsetrecall,				FALSE,	TRUE,	},
	{ "settimer",			do_mpsettimer,				FALSE,	TRUE	},
	{ "showroom",			do_mpshowroom,				TRUE,	TRUE	},
	{ "skimprove",			do_mpskimprove,				TRUE,	TRUE	},
	{ "spawndungeon",		scriptcmd_spawndungeon,		TRUE,	TRUE	},
	{ "startcombat",		scriptcmd_startcombat,		FALSE,	TRUE	},
	{ "stopcombat",			scriptcmd_stopcombat,		FALSE,	TRUE	},
	{ "stringmob",			do_mpstringmob,				TRUE,	TRUE	},
	{ "stringobj",			do_mpstringobj,				TRUE,	TRUE	},
	{ "stripaffect",		do_mpstripaffect,			TRUE,	TRUE	},
	{ "stripaffectname",	do_mpstripaffectname,		TRUE,	TRUE	},
	{ "take",				do_mptake,					FALSE,	TRUE	},
	{ "teleport", 			do_mpteleport,				FALSE,	FALSE	},
	{ "transfer",			do_mptransfer,				FALSE,	TRUE	},
	{ "ungroup",			do_mpungroup,				FALSE,	TRUE	},
	{ "unlockarea",			scriptcmd_unlockarea,		TRUE,	TRUE	},
	{ "unmute",				scriptcmd_unmute,			FALSE,	TRUE	},
	{ "usecatalyst",		do_mpusecatalyst,			FALSE,	TRUE	},
	{ "varclear",			do_mpvarclear,				FALSE,	TRUE	},
	{ "varclearon",			do_mpvarclearon,			FALSE,	TRUE	},
	{ "varcopy",			do_mpvarcopy,				FALSE,	TRUE	},
	{ "varsave",			do_mpvarsave,				FALSE,	TRUE	},
	{ "varsaveon",			do_mpvarsaveon,				FALSE,	TRUE	},
	{ "varset",				do_mpvarset,				FALSE,	TRUE	},
	{ "varseton",			do_mpvarseton,				FALSE,	TRUE	},
	{ "vforce",				do_mpvforce,				FALSE,	TRUE	},
	{ "wiretransfer",		do_mpwiretransfer,			FALSE,	TRUE	},
	{ "xcall",				do_mpxcall,					FALSE,	TRUE	},
	{ "zecho",				do_mpzecho,					FALSE,	TRUE	},
	{ "zot",				do_mpzot,					TRUE,	TRUE	},
	{ NULL,					NULL,						FALSE,	FALSE	}
};

///////////////////////////////////////////
//
// Function: mpcmd_lookup
//
// Section: Script/MPROG
//
// Purpose: Searches the mprog command list to the index of the specified command.
//
// Returns: Command index or -1 if not found.
//
int mpcmd_lookup(char *command)
{
	int cmd;

	for (cmd = 0; mob_cmd_table[cmd].name; cmd++)
		if (command[0] == mob_cmd_table[cmd].name[0] &&
			!str_prefix(command, mob_cmd_table[cmd].name))
			return cmd;

	return -1;
}

///////////////////////////////////////////
//
// Function: do_mpdump
//
// Section: Script/MPROG
//
// Purpose: Displays the current edit source code of an MPROG.
//
// Syntax: mpdump <vnum>
//
// Restrictions: Viewer must have READ access on the script to see it.
//
void do_mpdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *mprg;
	long vnum;

	one_argument(argument, buf);
	vnum = atoi(buf);

	if (!(mprg = get_script_index(vnum, PRG_MPROG))) {
		send_to_char("No such MOBprogram.\n\r", ch);
		return;
	}

	if (!area_has_read_access(ch,mprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}

	page_to_char(mprg->edit_src, ch);
}


///////////////////////////////////////////
//
// Function: do_mpstat
//
// Section: Script/MPROG
//
// Purpose: Displays trigger and variable information on the mobile
//
// Syntax: mpstat <mobile>
//
// Restrictions: Viewer must have READ access on the mobile to see it.
//
void do_mpstat(CHAR_DATA *ch, char *argument)
{
	char arg[MAX_STRING_LENGTH];
	ITERATOR it;
	PROG_LIST *mprg;
	CHAR_DATA *victim;
	int i, slot;

	one_argument(argument, arg);

	if (!arg[0]) {
		send_to_char("Mpstat whom?\n\r", ch);
		return;
	}

	if (!(victim = get_char_world(ch, arg))) {
		send_to_char("No such creature.\n\r", ch);
		return;
	}

	if (!IS_NPC(victim)) {
		send_to_char("That is not a mobile.\n\r", ch);
		return;
	}

	sprintf(arg, "Mobile #%-6ld [%s] ID [%8X:%8X]\n\r", victim->pIndexData->vnum, victim->short_descr, (int)victim->id[0], (int)victim->id[1]);
	send_to_char(arg, ch);

	if( !IS_NULLSTR(victim->pIndexData->comments) )
	{
		sprintf(arg, "Comments:\n\r%s\n\r", victim->pIndexData->comments);
		send_to_char(arg, ch);
	}

	sprintf(arg, "Delay   %-6d [%s]\n\r",
	victim->progs->delay,
	victim->progs->target ? victim->progs->target->name : "No target");

	send_to_char(arg, ch);

	if (!victim->pIndexData->progs)
		send_to_char("[No programs set]\n\r", ch);
	else
		for(i = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
			iterator_start(&it, victim->pIndexData->progs[slot]);
			while(( mprg = (PROG_LIST *)iterator_nextdata(&it))) {
				sprintf(arg, "[%2d] Trigger [%-8s] Program [%4ld] Phrase [%s]\n\r",
					++i, trigger_name(mprg->trig_type),
					mprg->vnum,
					trigger_phrase(mprg->trig_type,mprg->trig_phrase));
				send_to_char(arg, ch);
			}
			iterator_stop(&it);
		}

	if(victim->progs->vars) {
		pVARIABLE var;

		for(var = victim->progs->vars; var; var = var->next) {
			switch(var->type) {
			case VAR_INTEGER:
				sprintf(arg,"Name [%-20s] Type[NUMBER] Save[%c] Value[%d]\n\r",
					var->name,var->save?'Y':'N',var->_.i);
				break;
			case VAR_STRING:
			case VAR_STRING_S:
				if( var->_.s && strlen(var->_.s) > MIL )
				{
					sprintf(arg,"Name [%-20s] Type[STRING] Save[%c] Value[%.*s{x...{W(truncated){x]\n\r",
						var->name,var->save?'Y':'N',MIL,var->_.s);
				}
				else
				{
					sprintf(arg,"Name [%-20s] Type[STRING] Save[%c] Value[%s{x]\n\r",
						var->name,var->save?'Y':'N',var->_.s?var->_.s:"(empty)");
				}
				break;
			case VAR_ROOM:
				if(var->_.r) {
					if( var->_.r->wilds )
						sprintf(arg, "Name [%-20s] Type[ROOM  ] Save[%c] Value[%ld <%d,%d,%d>]\n\r", var->name,var->save?'Y':'N',var->_.r->wilds->uid,(int)var->_.r->x,(int)var->_.r->y,(int)var->_.r->z);
					else if( var->_.r->source )
						sprintf(arg, "Name [%-20s] Type[ROOM  ] Save[%c] Value[%s (%d %08X:%08X)]\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->source->vnum,(int)var->_.r->id[0],(int)var->_.r->id[1]);
					else
						sprintf(arg, "Name [%-20s] Type[ROOM  ] Save[%c] Value[%s (%d)]\n\r", var->name,var->save?'Y':'N',var->_.r->name,(int)var->_.r->vnum);
				} else
					sprintf(arg, "Name [%-20s] Type[ROOM  ] Save[%c] Value[-no-where-]\n\r", var->name,var->save?'Y':'N');
				break;
			case VAR_EXIT:
				if(var->_.door.r) {
					if( var->_.door.r->wilds)
						sprintf(arg, "Name [%-20s] Type[EXIT  ] Save[%c] Value[%s at %ld <%d,%d,%d>]\n\r", var->name,var->save?'Y':'N',dir_name[var->_.door.door],var->_.door.r->wilds->uid,(int)var->_.door.r->x,(int)var->_.door.r->y,(int)var->_.door.r->z);
					else if( var->_.door.r->source )
						sprintf(arg, "Name [%-20s] Type[EXIT  ] Save[%c] Value[%s in %s (%d %08X:%08X)]\n\r", var->name,var->save?'Y':'N',dir_name[var->_.door.door],var->_.door.r->name,(int)var->_.door.r->source->vnum,(int)var->_.door.r->id[0],(int)var->_.door.r->id[1]);
					else
						sprintf(arg, "Name [%-20s] Type[EXIT  ] Save[%c] Value[%s in %s (%d)]\n\r", var->name,var->save?'Y':'N',dir_name[var->_.door.door],var->_.door.r->name,(int)var->_.door.r->vnum);
				} else
					sprintf(arg, "Name [%-20s] Type[EXIT  ] Save[%c] Value[-no-exit-]\n\r", var->name,var->save?'Y':'N');
				break;
			case VAR_MOBILE:
				if(var->_.m) {
					if(IS_NPC(var->_.m))
						sprintf(arg, "Name [%-20s] Type[MOBILE] Save[%c] Value[%s (%d)] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',var->_.m->short_descr,(int)var->_.m->pIndexData->vnum,(int)var->_.m->id[0],(int)var->_.m->id[1]);
					else
						sprintf(arg, "Name [%-20s] Type[PLAYER] Save[%c] Value[%s] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',var->_.m->name,(int)var->_.m->id[0],(int)var->_.m->id[1]);
				} else
					sprintf(arg, "Name [%-20s] Type[MOBILE] Save[%c] Value[-no-mobile-]\n\r", var->name,var->save?'Y':'N');
				break;
			case VAR_OBJECT:
				if(var->_.o)
					sprintf(arg, "Name [%-20s] Type[OBJECT] Save[%c] Value[%s (%d)] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',var->_.o->short_descr,(int)var->_.o->pIndexData->vnum,(int)var->_.o->id[0],(int)var->_.o->id[1]);
				else
					sprintf(arg, "Name [%-20s] Type[OBJECT] Save[%c] Value[-no-object-]\n\r", var->name,var->save?'Y':'N');
				break;
			case VAR_TOKEN:
				if(var->_.t)
					sprintf(arg, "Name [%-20s] Type[TOKEN ] Save[%c] Value[%s (%d)] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',var->_.t->name,(int)var->_.t->pIndexData->vnum,(int)var->_.t->id[0],(int)var->_.t->id[1]);
				else
					sprintf(arg, "Name [%-20s] Type[TOKEN ] Save[%c] Value[-no-token-]\n\r", var->name,var->save?'Y':'N');
				break;
			case VAR_MOBILE_ID:
				sprintf(arg, "Name [%-20s] Type[MOBILE] Save[%c] Value[???] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',(int)var->_.mid.a,(int)var->_.mid.b);
				break;
			case VAR_OBJECT_ID:
				sprintf(arg, "Name [%-20s] Type[OBJECT] Save[%c] Value[???] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',(int)var->_.oid.a,(int)var->_.oid.b);
				break;
			case VAR_TOKEN_ID:
				sprintf(arg, "Name [%-20s] Type[TOKEN ] Save[%c] Value[???] ID[%08X:%08X]\n\r", var->name,var->save?'Y':'N',(int)var->_.tid.a,(int)var->_.tid.b);
				break;
			case VAR_BLLIST_MOB: {
				LLIST *mob_list = var->_.list;
				int sz = list_size(mob_list);

				if( sz > 0 )
				{
					sprintf(arg, "Name [%-20s] Type[MOBLST] Save[%c]\n\r", var->name,var->save?'Y':'N');

					LLIST_UID_DATA *data;
					ITERATOR it;
					iterator_start(&it, mob_list);
					while(( data = (LLIST_UID_DATA *)iterator_nextdata(&it)))
					{
						send_to_char(arg, ch);

						CHAR_DATA *m = (CHAR_DATA *)data->ptr;
						if(IS_VALID(m))
						{
							if( IS_NPC(m) )
								sprintf(arg,"      - MOBILE[%s (%d)] ID[%08X:%08X]\n\r", m->short_descr, (int)m->pIndexData->vnum, (int)m->id[0],(int)m->id[1]);
							else
								sprintf(arg,"      - PLAYER[%s] ID[%08X:%08X]\n\r", m->name, (int)m->id[0], (int)m->id[1]);
						}
						else
							sprintf(arg,"      - MOBILE[???] ID[%08X:%08X]\n\r", (int)data->id[0],(int)data->id[1]);
					}
					iterator_stop(&it);
				}
				else
					sprintf(arg, "Name [%-20s] Type[MOBLST] Save[%c] -empty-\n\r", var->name,var->save?'Y':'N');
				break;
			}
			case VAR_BLLIST_OBJ: {

				LLIST *obj_list = var->_.list;
				int sz = list_size(obj_list);

				if( sz > 0 )
				{
					sprintf(arg, "Name [%-20s] Type[OBJLST] Save[%c]\n\r", var->name,var->save?'Y':'N');
					LLIST_UID_DATA *data;
					ITERATOR it;

					iterator_start(&it, obj_list);
					while(( data = (LLIST_UID_DATA *)iterator_nextdata(&it)))
					{
						send_to_char(arg, ch);

						OBJ_DATA *o = (OBJ_DATA *)data->ptr;
						if(IS_VALID(o))
							sprintf(arg,"      - OBJECT[%s (%d)] ID[%08X:%08X]\n\r", o->short_descr, (int)o->pIndexData->vnum, (int)o->id[0], (int)o->id[1]);
						else
							sprintf(arg,"      - OBJECT[???] ID[%08X:%08X] -empty-\n\r", (int)data->id[0], (int)data->id[1]);
					}
					iterator_stop(&it);
				}
				else
					sprintf(arg, "Name [%-20s] Type[OBJLST] Save[%c]\n\r", var->name,var->save?'Y':'N');
				break;
			}
			default:
				sprintf(arg, "Name [%-20s] Type %d not displayed yet.\n\r", var->name,(int)var->type);
				break;
			}

			send_to_char(arg, ch);
		}
	}
}




///////////////////////////////////////////
//
// Function: do_mob
//
// Section: Script/MPROG
//
// Purpose: A special command interpreter for commands that could not be compiled, such as
//		all queued commands.
//
// Syntax: mob <command>
//
// Restrictions: This is an internal interpreter and restricted to unswitched mobs
//
void do_mob(CHAR_DATA *ch, char *argument)
{
	SCRIPT_VARINFO info;

	if (ch->desc)
		return;

	/* Stop crashes from things like "mob delay */
	if (!IS_NPC(ch)) {
		send_to_char("Huh?\n\r", ch);
		return;
	}

	memset(&info,0,sizeof(info));

	info.mob = ch;
	info.var = &ch->progs->vars;
	info.targ = &ch->progs->target;

	mob_interpret(&info, argument);
}

///////////////////////////////////////////
//
// Function: mob_interpret
//
// Section: Script/MPROG
//
// Purpose: Searches and executes the MPROG command.
//
// Notes: This is for situations where the MPROG command could not be compiled.
//
void mob_interpret(SCRIPT_VARINFO *info, char *argument)
{
	char buf[MSL], command[MIL];
	int cmd;

	if(!info->mob) return;

	if (!str_prefix("mob ", argument))
		argument = skip_whitespace(argument+3);

	argument = one_argument(argument, command);

	cmd = mpcmd_lookup(command);
	if(cmd < 0) {
		sprintf(buf, "Mob_interpret: invalid cmd from mob %ld: '%s'", VNUM(info->mob), command);
		bug(buf, 0);
		return;
	}

	SCRIPT_PARAM *arg = new_script_param();
	(*mob_cmd_table[cmd].func) (info, argument, arg);
	free_script_param(arg);

	tail_chain();
}


///////////////////////////////////////////
//
// Function: mp_getlocation
//
// Section: Script/MPROG
//
// Purpose: Extracts a room location from the supplied argument string
//
// Returns: The remainder of the argument string as well as the room pointer value.
//
// Notes: The argument string can be an escaped string or not.
//
char *mp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
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
		case ENT_NONE: *room = info->mob->in_room; break;
		case ENT_NUMBER:
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

					rest = rest2;
					room_used_for_wilderness.wilds = pWilds;
					room_used_for_wilderness.x = x;
					room_used_for_wilderness.y = y;
					*room = &room_used_for_wilderness;
				}
			} else
				*room = get_room_index(x);
			break;

		case ENT_STRING: // Special named locations
			if(arg->d.str[0] == '@')
				// Points to an exit, like @north or @down
				*room = get_exit_dest(info->mob->in_room, arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				// Rather self-explanatory
				*room = info->mob->in_room;
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
				// Named locations: <name>
				loc = NULL;
				// Try area names
				for (area = area_first; area; area = area->next) {
					if (!str_infix(arg->d.str, area->name)) {
						// Get the area's recall location
						if(!(loc = location_to_room(&area->recall))) {
							for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
								if ((loc = get_room_index(vnum)))
									break;
						}

						break;
					}
				}

				if(!loc) {
					// If no area matches, search for mobiles then objects
					if((victim = get_char_world(info->mob, arg->d.str)))
						loc = victim->in_room;
					else if ((obj = get_obj_world(info->mob, arg->d.str)))
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


///////////////////////////////////////////
//
// Function: mp_getolocation
//
// Section: Script/MPROG
//
// Purpose:
//
// Returns:
//
// Notes:
//
char *mp_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
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
			// Nothing was on the string, so it will assume the current room
			*room = info->mob->in_room;
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
			// Special named locations

			if(arg->d.str[0] == '@')
				// Points to an exit, like @north or @down
				*room = get_exit_dest(info->mob->in_room, arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				// Rather self-explanatory
				*room = info->mob->in_room;
			else if(!str_cmp(arg->d.str,"vroom")) {
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
			} else {
				// Named locations: <name>
				loc = NULL;
				// Try area names
				for (area = area_first; area; area = area->next) {
					if (!str_infix(arg->d.str, area->name)) {
						// Get the area's recall location
						if(!(loc = location_to_room(&area->recall))) {
							for (vnum = area->min_vnum; vnum <= area->max_vnum; vnum++)
								if ((loc = get_room_index(vnum)))
									break;
						}

						break;
					}
				}

				if(!loc) {
					// If no area matches, search for mobiles then objects
					if((victim = get_char_world(info->mob, arg->d.str)))
						loc = victim->in_room;
					else if ((obj = get_obj_world(info->mob, arg->d.str)))
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
			ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
			*room = ex ? exit_destination(ex) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}

	free_script_param(arg);
	return rest;
}


///////////////////////////////////////////
//
// Function: do_mpairshipaddwaypoint
//
// Section: Script/MPROG/Command
//
// Purpose: Adds to the airship's waypoint list to go to the landing point of the selected area,
//		either by name or an explict area reference.
//
// Syntax: 	mob airshipaddwaypoint <STRING>
//		mob airshipaddwaypoint <AREA>
//
// Returns: 0 if the waypoint was not done
//          1 if the current room isn't the airship helm room
//          2 if the destination is too far from current location
//
// Notes: This command will eventually be moved to a VEHICLE command set.
//
SCRIPT_CMD(do_mpairshipaddwaypoint)
{
#if 0
	char buf[MSL];
	AREA_DATA *pArea;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;	// Wasn't done yet...

	if(!expand_argument(info,argument,arg)) {
		bug("Mpairshipaddwaypoint - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: pArea = find_area(arg->d.str); break;
	case ENT_AREA: pArea = arg->d.area; break;
	default: pArea = NULL; break;
	}

	if (!pArea) {
		sprintf(buf, "MpAirshipAddWayPoint: no such area '%s'", arg->d.str);
		bug(buf, 0);
		return;
	}

	if (!SAME_PLACE_AREA(info->mob->in_room->area, pArea)) {
		info->mob->progs->lastreturn = 1;	// Too far away
		return;
	}

	if (plith_airship->ship->ship->in_room != info->mob->in_room) {
		info->mob->progs->lastreturn = 2;	// Not here
		return;
	}

	add_move_waypoint(plith_airship->ship, pArea->land_x, pArea->land_y);
	info->mob->progs->lastreturn = 3;	// Ok
#endif
	return;
}

///////////////////////////////////////////
//
// Function: do_mpairshipsetcrash
//
// Section: Script/MPROG/Command
//
// Purpose: Causes the ship given by the particular vnum to crash.
//
// Syntax: 	mob airshipsetcrash <VNUM>
//
// Returns: 1 if the ship was set to crash
//          0 if not...
//
// Notes: Even though this is called airshipsetcrash, this will work on any ship.
//		However, this command will eventually be moved to a VEHICLE command set.
//
SCRIPT_CMD(do_mpairshipsetcrash)
{
#if 0
	long vnum;
	NPC_SHIP_DATA *npc_ship;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;	// Wasn't done yet...

	if(!expand_argument(info,argument,arg)) {
		bug("Mpairshipsetcrash - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = -1; break;
	}

	if(vnum < 0) return;

	for (npc_ship = npc_ship_list; npc_ship; npc_ship = npc_ship->next)
		if (npc_ship->pShipData->vnum == vnum)
			break;

	if (!npc_ship) return;

	npc_ship->captain->ship_crash_time = 12;

	boat_echo(npc_ship->ship, "{RYou feel the airship begin to plummet!!!{x");

	info->mob->progs->lastreturn = 1;
#endif
	return;
}

///////////////////////////////////////////
//
// Function: do_mpasound
//
// Section: Script/MPROG/Command
//
// Purpose: Echoes a message to all surrounding rooms.
//
// Syntax: 	mob asound <message>
//
// Returns:
//
// Notes: This will not cause triggers to fire in the surrounding rooms.
//
SCRIPT_CMD(do_mpasound)
{
	ROOM_INDEX_DATA *was_in_room, *room;
	ROOM_INDEX_DATA *rooms[MAX_DIR];
	int door, i, j;
	EXIT_DATA *pexit;

	if(!info || !info->mob || !info->mob->in_room) return;
	if (!argument[0]) return;

	was_in_room = info->mob->in_room;

	// Verify there are any exits!
	for (door = 0; door < MAX_DIR; door++)
		if ((pexit = was_in_room->exit[door]) && (room = exit_destination(pexit)) && room != was_in_room)
			break;

	if (door < MAX_DIR) {
		// Expand the message
		BUFFER *buffer = new_buf();
		expand_string(info,argument,buffer);

		if(buffer->state == BUFFER_SAFE && buffer->string[0] != '\0')
		{
			for (i = 0; door < MAX_DIR; door++)
				if ((pexit = was_in_room->exit[door]) && (room = exit_destination(pexit)) && room != was_in_room) {
					// Have we been to this room already?
					for(j=0;j < i && rooms[j] != room; j++);

					if(i <= j) {
						// No, so do the message
						MOBtrigger  = FALSE;
						act(buf_string(buffer), room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
						MOBtrigger  = TRUE;
						rooms[i++] = room;
					}
				}
		}
		free_buf(buffer);
	}
}




///////////////////////////////////////////
//
// Function: do_mpassist
//
// Section: Script/MPROG/Command
//
// Purpose: Causes the mobile to attempt to assist the specific character.  The mobile will attempt to group
//		with the target if specified by the optional argument.
//
// Syntax: 	mob assist <character>
// Syntax: 	mob assist <character> group
//
// Returns:	0 if the action could not be performed
//		1 if the mobile could assist the target
//		2 if the mobile could assist and group with the target (only returned if GROUP is supplied)
//
// Notes:
//
SCRIPT_CMD(do_mpassist)
{
	char *rest;
	CHAR_DATA *victim;//, *leader;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;	// Wasn't done yet...

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAssist - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpAssist - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(victim == info->mob || victim->in_room != info->mob->in_room ||
		info->mob->fighting || !victim->fighting)
		return;

	// Almost ready, but group not yet checked
	info->mob->progs->lastreturn = 1;

	// Try to group?
	if(!str_cmp(rest,"group") && !info->mob->leader &&
		!victim->leader && victim->num_grouped < 9) {

		// Yes, add to group
		victim->num_grouped++;
		info->mob->leader = victim;
		info->mob->master = victim;

		// Indicate grouping was successful
		info->mob->progs->lastreturn = 2;
	}

	multi_hit(info->mob, victim->fighting, TYPE_UNDEFINED);
}

///////////////////////////////////////////
//
// Function: do_mpat
//
// Section: Script/MPROG/Command
//
// Purpose: Performs a command at the supplied location.
//
// Syntax: 	mob at <location> <command>
//
// Returns:	0 if the action could not be performed
//		1 if the mobile could assist the target
//		2 if the mobile could assist and group with the target (only returned if GROUP is supplied)
//
// Notes:
//
// do_mpat
// Syntax: mob at <location> <commands>
SCRIPT_CMD(do_mpat)
{
	ROOM_INDEX_DATA *orig, *location;
	char *command;
	OBJ_DATA *on;
	OBJ_DATA *pulled;
	int sec;
	bool remote;

	if(!info || !info->mob || PROG_FLAG(info->mob,PROG_AT)) return;

	if (!argument[0]) {
		bug("Mpat - Bad argument from vnum %d.", VNUM(info->mob));
		return;
	}

	command = mp_getlocation(info, argument, &location);

	if (!location) {
		bug("Mpat - No such location from vnum %d.", IS_NPC(info->mob) ? info->mob->pIndexData->vnum : 0);
		return;
	}

	remote = PROG_FLAG(info->mob,PROG_AT);
	sec = script_security;
	script_security = NO_SCRIPT_SECURITY;
	SET_BIT(info->mob->progs->entity_flags,PROG_AT);
	orig = info->mob->in_room;
	on = info->mob->on;
	pulled = info->mob->pulled_cart;
	char_from_room(info->mob);
	if(location->wilds)
		char_to_vroom(info->mob, location->wilds, location->x, location->y);
	else
		char_to_room(info->mob, location);
	script_interpret(info, command);
	script_security = sec;
	if(!remote) REMOVE_BIT(info->mob->progs->entity_flags,PROG_AT);

	// if the mob is freed, info->mob will be cleared.
	if(info->mob) {
		char_from_room(info->mob);
		char_to_room(info->mob, orig);
		info->mob->on = on;
		info->mob->pulled_cart = pulled;
		return;
	}
}

// do_mpcall
SCRIPT_CMD(do_mpcall)
{
	char *rest; //buf[MSL], *rest;
	CHAR_DATA *vch = NULL,*ch = NULL;
	OBJ_DATA *obj1 = NULL,*obj2 = NULL;
	SCRIPT_DATA *script;
	int depth, vnum, ret;


	DBG2ENTRY2(PTR,info,PTR,argument);
	if(info->mob) {
		DBG3MSG2("info->mob = %s(%d)\n",HANDLE(info->mob),VNUM(info->mob));
	}

	if(!info || !info->mob) return;

	if (!argument[0]) {
		bug("MpCall: missing arguments from vnum %d.", VNUM(info->mob));
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("MpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->mob));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(script = get_script_index(vnum, PRG_MPROG))) {
		bug("MpCall: invalid prog from vnum %d.", VNUM(info->mob));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(info->mob, NULL, arg->d.str); break;
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
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(info->mob, NULL, arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(info->mob, NULL, arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(info->mob, NULL, arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	// Do this to account for possible destructions
	ret = execute_script(script->vnum, script, info->mob, NULL, NULL, NULL, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->mob) {
		info->mob->progs->lastreturn = ret;
		DBG3MSG1("lastreturn = %d\n", info->mob->progs->lastreturn);
	} else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

// do_mpcancel
SCRIPT_CMD(do_mpcancel)
{
	if(!info || !info->mob) return;

	info->mob->progs->delay = -1;
}

// do_mpcast
SCRIPT_CMD(do_mpcast)
{
	CHAR_DATA *vch = NULL;
	OBJ_DATA *obj = NULL;
	void *to = NULL;
	char *rest;
	int sn;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpCast - Error in parsing from vnum %ld.", VNUM(info->mob));
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
		bug("MpCast - No such spell from vnum %d.", VNUM(info->mob));
		return;
	}

	if(*rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCast - Error in parsing from vnum %ld.", VNUM(info->mob));
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			vch = get_char_room(info->mob, NULL, arg->d.str);
			obj = get_obj_here(info->mob, NULL, arg->d.str);
			break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		case ENT_OBJECT: obj = arg->d.obj; break;
		}
	}

	switch (skill_table[sn].target) {
	default: return;
	case TAR_IGNORE: break;
	case TAR_CHAR_OFFENSIVE:
		if (!vch || vch == info->mob) return;
		to = vch;
		break;
	case TAR_CHAR_DEFENSIVE:
		to = vch ? vch : info->mob;
		break;
	case TAR_CHAR_SELF:
		to = info->mob;
		break;
	case TAR_OBJ_CHAR_DEF:
	case TAR_OBJ_CHAR_OFF:
	case TAR_OBJ_INV:
		if (!obj) return;
		to = obj;
	}
	(*skill_table[sn].spell_fun)(sn, info->mob->level, info->mob, to, skill_table[sn].target, WEAR_NONE);
	return;
}


// do_mpchangevesselname
SCRIPT_CMD(do_mpchangevesselname)
{
#if 0
	char buf[MSL], *rest;

	SHIP_DATA *ship;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAwardXP - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(arg->type == ENT_NUMBER)
		sprintf(buf,"%d",arg->d.num);
	else if(arg->type == ENT_STRING)
		strncpy(buf,arg->d.str,MSL-1);
	else
		return;

	for(ship = ((AREA_DATA *) get_sailing_boat_area())->ship_list; ship; ship = ship->next)
		if (!str_cmp(ship->owner_name, info->mob->name))
			break;

	if (!ship) return;

	if (ship->ship_name)
		free_string(ship->ship_name);

	ship->ship_name = str_dup(buf);
#endif
}

// do_mpchargemoney
SCRIPT_CMD(do_mpchargemoney)
{
	char buf[MIL], *rest;
	int roll, min, max, amt;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpChargeMoney - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if (!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: min = atoi(arg->d.str); break;
	case ENT_NUMBER: min = arg->d.num; break;
	default: min = 0; break;
	}

	if(min < 1 || !*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpChargeMoney - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: max = atoi(arg->d.str); break;
	case ENT_NUMBER: max = arg->d.num; break;
	default: max = 0; break;
	}

	if(max < 1) return;

	amt = number_range(min, max);

	one_argument(rest,buf);
	if (!str_cmp(buf,"haggle")) {
		roll = number_percent();
		if (roll < get_skill(victim, gsn_haggle)) {
			amt -= amt * roll / 200;
			check_improve(victim, gsn_haggle, TRUE, 4);
		}
	}

	if ((victim->silver + 100 * victim->gold) < amt) {
		sprintf(buf, "do_mpchargemoney: victim doesnt have enough cash, mob %s(%ld)", HANDLE(info->mob), VNUM(info->mob));
		bug(buf, 0);
	}

	deduct_cost(victim, amt);
}


// do_mpdamage
SCRIPT_CMD(do_mpdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *victim_next;
	int low, high, level, value, dc;
	bool fAll = FALSE, fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = get_char_room(info->mob, NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll) {
		bug("MpDamage - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) {
		bug("MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = TRUE; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = TRUE; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = TRUE; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = TRUE; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) {
		bug("MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(fLevel && !victim) {
		bug("MpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->mob));
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
			bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("MpDamage - Null reference mob from vnum %ld.", VNUM(info->mob));
				return;
			}
			break;
		} else {
			bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
			return;
		}
		break;
	default:
		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = TRUE;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = info->mob->in_room->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			if (victim != info->mob) {
				value = fLevel ? dice(low,high) : number_range(low,high);
				damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
			}
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
	}
}

// do_mpdecdeity
SCRIPT_CMD(do_mpdecdeity)
{
	char *rest;//buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDecDeity - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDecDeity - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpDecDeity - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	victim->deitypoints -= amount;

	if (victim->deitypoints < 0)
		victim->deitypoints = 0;
}

// do_mpdecpneuma
SCRIPT_CMD(do_mpdecpneuma)
{
	char *rest;//buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDecPneuma - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDecPneuma - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpDecPneuma - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	victim->pneuma -= amount;

	if (victim->pneuma < 0)
		victim->pneuma = 0;
}

// do_mpdecprac
SCRIPT_CMD(do_mpdecprac)
{
	char *rest;//buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDecPrac - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDecPrac - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpDecPrac - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	victim->practice -= amount;

	if (victim->practice < 0)
		victim->practice = 0;
}

// do_mpdecquest
SCRIPT_CMD(do_mpdecquest)
{
	char *rest;//buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDecQuest - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDecQuest - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpDecQuest - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	victim->questpoints -= amount;

	if (victim->questpoints < 0)
		victim->questpoints = 0;
}

// do_mpdectrain
SCRIPT_CMD(do_mpdectrain)
{
	char *rest;//buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDecTrain - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDecTrain - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpDecTrain - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: amount = atoi(arg->d.str); break;
	case ENT_NUMBER: amount = arg->d.num; break;
	default: amount = 0; break;
	}

	if(amount < 1) return;

	victim->train -= amount;

	if (victim->train < 0)
		victim->train = 0;
}

// do_mpdelay
SCRIPT_CMD(do_mpdelay)
{
//	char buf[MSL];
	int delay = 0;


	if(!info || !info->mob) return;

	if(!expand_argument(info,argument,arg)) {
		bug("MpDelay - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: delay = is_number(arg->d.str) ? atoi(arg->d.str) : -1; break;
	case ENT_NUMBER: delay = arg->d.num; break;
	default: delay = 0; break;
	}

	if (delay < 1) {
		bug("MpDelay: invalid delay from vnum %d.", VNUM(info->mob));
		return;
	}
	info->mob->progs->delay = delay;
}


// do_mpdequeue
SCRIPT_CMD(do_mpdequeue)
{
	if(!info || !info->mob || !info->mob->events)
		return;

	wipe_owned_events(info->mob->events);
}

// do_mpecho
// Syntax: mpecho <string>
SCRIPT_CMD(do_mpecho)
{
	if(!info || !info->mob) return;

	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if(buf_string(buffer)[0] != '\0')
		act(buf_string(buffer), info->mob, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	free_buf(buffer);
}

// do_mpechoroom
// Syntax: mob echoroom <location> <string>
SCRIPT_CMD(do_mpechoroom)
{
	char *rest;
	ROOM_INDEX_DATA *room;

	EXIT_DATA *ex;

	if(!info || !info->mob) return;

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
	expand_string(info,argument,buffer);

	if(buf_string(buffer)[0] != '\0')
		act(buf_string(buffer), room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
	free_buf(buffer);
}

// do_mpechoaround
// Syntax: mob echoaround <victim> <string>
SCRIPT_CMD(do_mpechoaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room || !victim->in_room->people)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if(buf_string(buffer)[0] != '\0')
		act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);
	free_buf(buffer);
}

// do_mpechonotvict
// Syntax: mob echonotvict <attacker> <victim> <string>
SCRIPT_CMD(do_mpechonotvict)
{
	char *rest;
	CHAR_DATA *victim, *attacker;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != attacker->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if(buf_string(buffer)[0] != '\0')
		act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_NOTVICT);
	free_buf(buffer);
}

// do_mpechobattlespam
// Syntax: mob echobattlespam <attacker> <victim> <string>
SCRIPT_CMD(do_mpechobattlespam)
{
	char *rest;
	CHAR_DATA *victim, *attacker, *ch;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || victim->in_room != attacker->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if(buf_string(buffer)[0] != '\0')
	{
		for (ch = attacker->in_room->people; ch; ch = ch->next_in_room) {
			if (!IS_NPC(ch) && (ch != attacker && ch != victim) && (is_same_group(ch, attacker) || is_same_group(ch, victim) || !IS_SET(ch->comm, COMM_NOBATTLESPAM))) {
				act(buf_string(buffer), ch, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			}
		}
	}
	free_buf(buffer);
}

// do_mpechochurch
// Syntax: mob echochurch <victim> <string>
SCRIPT_CMD(do_mpechochurch)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim) || !victim->church)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		msg_church_members(victim->church, buf_string(buffer));

	free_buf(buffer);
}

// do_mpechoat
// Syntax: mob echoat <victim> <string>
SCRIPT_CMD(do_mpechoat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
	free_buf(buffer);
}

// do_mpechogrouparound
// Syntax: mob echogrouparound <victim> <string>
SCRIPT_CMD(do_mpechogrouparound)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		act_new(buf_string(buffer),victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_NOTFUNC,POS_RESTING,rop_same_group);
	free_buf(buffer);
}

// do_mpechogroupat
// Syntax: mob echogroupat <victim> <string>
SCRIPT_CMD(do_mpechogroupat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || !victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		act_new(buf_string(buffer),victim,NULL,NULL,NULL,NULL,NULL,NULL,TO_FUNC,POS_RESTING,rop_same_group);

	free_buf(buffer);
}

// do_mpecholeadaround
// Syntax: mob echoleadaround <victim> <string>
SCRIPT_CMD(do_mpecholeadaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) return;

	if(victim->leader) victim = victim->leader;

	if(!victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_ROOM);

	free_buf(buffer);
}

// do_mpecholeadat
// Syntax: mob echoleadat <victim> <string>
SCRIPT_CMD(do_mpecholeadat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) return;

	if(victim->leader) victim = victim->leader;

	if(!victim->in_room)
		return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] != '\0' )
		act(buf_string(buffer), victim, NULL, NULL, NULL, NULL, NULL, NULL, TO_CHAR);

	free_buf(buffer);
}

// do_mpflee[ <target>[ <direction>[ <conceal> <pursue>]]]
// <direction> can be a direction (north, south, etc),
//				'none' (for random)
//				'anyway' (for random)
//				or 'wimpy' (cause wimpy flee mode)
SCRIPT_CMD(do_mpflee)
{
	char *rest;

	CHAR_DATA *target;
	int door = MAX_DIR;
	bool conceal = FALSE, pursue = TRUE;
	char fleedata[MIL];
	char *fleearg = str_empty;

	info->mob->progs->lastreturn = MAX_DIR;
	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	target = info->mob;
	switch(arg->type) {
	case ENT_STRING: target = get_char_room(info->mob, NULL, arg->d.str); break;
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
	if (IS_VALID(info->mob)) {
		info->mob->progs->lastreturn = door;
	}

}

// do_mpforce
SCRIPT_CMD(do_mpforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *next;
	bool fAll = FALSE, forced;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = get_char_room(info->mob, NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!fAll && !victim) {
		bug("MpForce - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buf_string(buffer)[0] == '\0' ) {
		bug("MpForce - Error in parsing from vnum %ld.", VNUM(info->mob));
		free_buf(buffer);
		return;
	}

	forced = forced_command;
	if (fAll) {
		for (victim = info->mob->in_room->people; victim; victim = next) {
			next = victim->next_in_room;
			if (get_trust(victim) < get_trust(info->mob)
				&& can_see(info->mob, victim)
				&& (IS_NPC(victim) || !IS_IMMORTAL(victim))) {
				forced_command = TRUE;
				interpret(victim, buf_string(buffer));
			}
		}
	} else {
		if (victim == info->mob)
		{
			free_buf(buffer);
			return;
		}
		if (!IS_NPC(victim) && IS_IMMORTAL(victim))
		{
			free_buf(buffer);
			return;
		}

		forced_command = TRUE;
		interpret(victim, buf_string(buffer));
	}

	forced_command = forced;
	free_buf(buffer);
}

// do_mpforget
SCRIPT_CMD(do_mpforget)
{
	if(!info || !info->mob) return;

	info->mob->progs->target = NULL;
}

// do_mpgdamage
SCRIPT_CMD(do_mpgdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *rch, *rch_next;
	int low, high, level, value, dc;
	bool fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpDamage - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) {
		bug("MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: low = arg->d.num; break;
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"level")) { fLevel = TRUE; break; }
		if(!str_cmp(arg->d.str,"remort")) { fLevel = fRemort = TRUE; break; }
		if(!str_cmp(arg->d.str,"dual")) { fLevel = fTwo = TRUE; break; }
		if(!str_cmp(arg->d.str,"dualremort")) { fLevel = fTwo = fRemort = TRUE; break; }
		if(is_number(arg->d.str)) { low = atoi(arg->d.str); break; }
	default:
		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) {
		bug("MpDamage - missing argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpDamage - Error in parsing from vnum %ld.", VNUM(info->mob));
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
			bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("MpDamage - Null reference mob from vnum %ld.", VNUM(info->mob));
				return;
			}
			break;
		} else {
			bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
			return;
		}
		break;
	default:
		bug("MpDamage - invalid argument from vnum %ld.", VNUM(info->mob));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = TRUE;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	for(rch = info->mob->in_room->people; rch; rch = rch_next) {
		rch_next = rch->next_in_room;
		if (rch != info->mob && rch != victim && is_same_group(victim,rch)) {
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(rch, rch, fKill ? value : UMIN(rch->hit,value), TYPE_UNDEFINED, dc, FALSE);
		}
	}
}

// do_mpgecho
// Syntax: mob gecho <string>
SCRIPT_CMD(do_mpgecho)
{
	DESCRIPTOR_DATA *d;

	if(!info || !info->mob) return;

	if (!argument[0]) {
		bug("MpGEcho: missing argument from vnum %d", VNUM(info->mob));
		return;
	}

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	for (d = descriptor_list; d; d = d->next)
		if (d->connected == CON_PLAYING) {
			if (IS_IMMORTAL(d->character))
				send_to_char("Mob echo> ", d->character);
			send_to_char(buf_string(buffer), d->character);
			send_to_char("\n\r", d->character);
		}

	free_buf(buffer);
}

// do_mpgforce
SCRIPT_CMD(do_mpgforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *vch, *next;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!victim) {
		bug("MpGforce - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);
	if(buf_string(buffer)[0] == '\0') {
		bug("MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	for (vch = info->mob->in_room->people; vch; vch = next) {
		next = vch->next_in_room;
		if (is_same_group(victim,vch) &&
			get_trust(vch) < get_trust(info->mob)
			&& can_see(info->mob, vch)
			&& (IS_NPC(vch) || !IS_IMMORTAL(vch)))
			interpret(vch, buf_string(buffer));
	}

	free_buf(buffer);
}

// do_mpgoto
// Syntax: mob goto <destination>
SCRIPT_CMD(do_mpgoto)
{
	ROOM_INDEX_DATA *dest;

	if(!info || !info->mob || !info->mob->in_room || PROG_FLAG(info->mob,PROG_AT)) return;

	if(!argument[0]) {
		bug("Mpgoto - No argument from vnum %d.", VNUM(info->mob));
		return;
	}

	mp_getlocation(info, argument, &dest);

	if(!dest) {
		bug("Mpgoto - Bad location from vnum %d.", VNUM(info->mob));
		return;
	}

	if (info->mob->fighting) stop_fighting(info->mob, TRUE);

	char_from_room(info->mob);
	if(dest->wilds)
		char_to_vroom(info->mob, dest->wilds, dest->x, dest->y);
	else
		char_to_room(info->mob, dest);
}

// do_mpgtransfer
// Syntax: mob gtransfer <target> [<location> [all]]
SCRIPT_CMD(do_mpgtransfer)
{
	char buf[MIL], buf2[MIL], buf3[MIL], *rest;
	CHAR_DATA *victim, *vch,*next;
	ROOM_INDEX_DATA *dest;
	bool all = FALSE, force = FALSE, quiet = FALSE;
	int mode;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpGtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(info->mob, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("MpGtransfer - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if (!victim->in_room) return;

	if(!(argument = mp_getlocation(info, rest, &dest))) {
		bug("MpGtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!dest) {
		bug("MpGtransfer - Bad location from vnum %d.", VNUM(info->mob));
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

// do_mpinvis
SCRIPT_CMD(do_mpinvis)
{
	if(!info || !info->mob) return;

	info->mob->invis_level = 150;
}

// do_mpjunk
// Syntax: mob junk <item>
SCRIPT_CMD(do_mpjunk)
{
//	char buf[MIL];
	OBJ_DATA *obj;
	OBJ_DATA *obj_next;


	if(!info || !info->mob || !argument[0]) return;

	if(expand_argument(info,argument,arg)) {
		switch(arg->type) {
		case ENT_STRING:
			if (str_cmp(arg->d.str, "all") && str_prefix("all.", arg->d.str)) {
				if (!(obj = get_obj_wear(info->mob, arg->d.str, TRUE)))
					obj = get_obj_carry(info->mob, arg->d.str, info->mob);
			} else {
				for (obj = info->mob->carrying; obj; obj = obj_next) {
					obj_next = obj->next_content;
					if (!arg->d.str[3] || is_name(&arg->d.str[4], obj->name)) {
						if(!PROG_FLAG(obj,PROG_AT)) {
							if (obj->wear_loc != WEAR_NONE)
								unequip_char(info->mob, obj, TRUE);
							extract_obj(obj);
						}
					}
				}
				return;
			}
			break;
		case ENT_OBJECT:
			obj = (arg->d.obj && arg->d.obj->carried_by == info->mob) ? arg->d.obj : NULL;
			break;
		case ENT_OLLIST_OBJ:
			if(!arg->d.list.ptr.obj || !(*arg->d.list.ptr.obj))
				return;

			if((*arg->d.list.ptr.obj)->in_obj && (*arg->d.list.ptr.obj)->in_obj->carried_by != info->mob)
				return;

			for (obj = *(arg->d.list.ptr.obj); obj; obj = obj_next) {
				obj_next = obj->next_content;
				if(!PROG_FLAG(obj,PROG_AT)) {
					if (obj->wear_loc != WEAR_NONE)
						unequip_char(info->mob, obj, TRUE);
					extract_obj(obj);
				}
			}
			return;
		default: obj = NULL; break;
		}

		if(obj && !PROG_FLAG(obj,PROG_AT)) {
			if(obj->wear_loc != WEAR_NONE)
				unequip_char(info->mob, obj, TRUE);
			extract_obj(obj);
		}
	}
}

// do_mpkill
// Syntax: mob kill <victim>
SCRIPT_CMD(do_mpkill)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob || PROG_FLAG(info->mob,PROG_AT)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpKill - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(victim == info->mob || IS_NPC(victim) || victim->in_room != info->mob->in_room ||
		info->mob->position == POS_FIGHTING)
		return;

	if (IS_AFFECTED(info->mob, AFF_CHARM) && info->mob->master == victim) {
		bug("MpKill - Charmed mob attacking master from vnum %d.", VNUM(info->mob));
		return;
	}

	multi_hit(info->mob, victim, TYPE_UNDEFINED);
	return;
}

// do_mplink
SCRIPT_CMD(do_mplink)
{
	char *rest;
	ROOM_INDEX_DATA *room, *dest;
	EXIT_DATA *ex;
	int door, vnum;
	unsigned long id1, id2;

	bool del = FALSE;
	bool environ = FALSE;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		room = info->mob->in_room;
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
		bug("MPlink used without an argument from room vnum %d.", room->vnum);
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
			vnum = arg->d.num;
		else if(!str_cmp(arg->d.str,"delete") ||
			!str_cmp(arg->d.str,"remove") ||
			!str_cmp(arg->d.str,"unlink")) {
			vnum = 0;
			del = TRUE;
		} else if(!str_cmp(arg->d.str,"environment") ||
			!str_cmp(arg->d.str,"environ") ||
			!str_cmp(arg->d.str,"extern") ||
			!str_cmp(arg->d.str,"outside")) {
			vnum = 0;
			environ = TRUE;
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
		vnum = ex ? ex->u1.to_room->vnum : -1;
		break;
	case ENT_MOBILE:
		vnum = (arg->d.mob && arg->d.mob->in_room) ? arg->d.mob->in_room->vnum : -1;
		break;
	case ENT_OBJECT:
		vnum = (arg->d.obj && obj_room(arg->d.obj)) ? obj_room(arg->d.obj)->vnum : -1;
		break;
	}

	if(vnum < 0) {
		bug("MPlink - invalid argument in room %d.", room->vnum);
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
		bug("MPlink - invalid destination in room %d.", room->vnum);
		return;
	}
	script_change_exit(room, dest, door);
}

// do_mpmload
// Syntax: mob mload <vnum>
SCRIPT_CMD(do_mpmload)
{
	char buf[MIL], *rest;
	long vnum;
	MOB_INDEX_DATA *pMobIndex;
	CHAR_DATA *victim;


	if(!info || !info->mob || !info->mob->in_room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_MOBILE: vnum = arg->d.mob ? arg->d.mob->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(pMobIndex = get_mob_index(vnum))) {
		sprintf(buf, "Mpmload: bad mob index (%ld) from mob %ld", vnum, VNUM(info->mob));
		bug(buf, 0);
		return;
	}

	victim = create_mobile(pMobIndex, FALSE);
	char_to_room(victim, info->mob->in_room);
	if(rest && *rest) variables_set_mobile(info->var,rest,victim);
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

// do_mpoload
// Syntax: mob oload <vnum> [<level>] [room|wear|$ENTITY]
SCRIPT_CMD(do_mpoload)
{
	char buf[MIL], *rest;
	long vnum, level;
	bool fToroom = FALSE, fWear = FALSE;
	OBJ_INDEX_DATA *pObjIndex;
	OBJ_DATA *obj;

	CHAR_DATA *to_mob = info->mob;
	OBJ_DATA *to_obj = NULL;
	ROOM_INDEX_DATA *to_room = NULL;

	if(!info || !info->mob || !info->mob->in_room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_OBJECT: vnum = arg->d.obj ? arg->d.obj->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (!vnum || !(pObjIndex = get_obj_index(vnum))) {
		bug("Mpoload - Bad vnum arg from vnum %d.", VNUM(info->mob));
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

		if(level <= 0 || level > get_trust(info->mob))
			level = get_trust(info->mob);

		if(rest && *rest) {
			argument = rest;
			if(!(rest = expand_argument(info,argument,arg)))
				return;

			/*
			 * Added 3rd argument
			 * omitted - load to mobile's inventory
			 * 'none'  - load to mobile's inventory
			 * 'room'  - load to room
			 * 'wear'  - load to mobile and force wear
			 * MOBILE  - load to target mobile
			 *         - 'W' automatically wear
			 * OBJECT  - load to target object
			 * ROOM    - load to target room
			 */

			switch(arg->type) {
			case ENT_STRING:
				if(!str_cmp(arg->d.str, "room"))
					fToroom = TRUE;
				else if(!str_cmp(arg->d.str, "wear"))
					fWear = TRUE;
				break;

			case ENT_MOBILE:
				to_mob = arg->d.mob;
				if((rest = one_argument(rest,buf))) {
					if(!str_cmp(buf, "wear"))
						fWear = TRUE;
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
		level = get_trust(info->mob);

	obj = create_object(pObjIndex, level, TRUE);
	if( to_room )
		obj_to_room(obj, to_room);
	else if( to_obj )
		obj_to_obj(obj, to_obj);
	else if( to_mob && (fWear || !fToroom) && CAN_WEAR(obj, ITEM_TAKE) &&
		(to_mob->carry_number < can_carry_n (to_mob)) &&
		(get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
		obj_to_char(obj, to_mob);
		if (fWear)
			wear_obj(to_mob, obj, TRUE);
	}
	else
		obj_to_room(obj, info->mob->in_room);

	if(rest && *rest) variables_set_object(info->var,rest,obj);
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

// do_mpotransfer
SCRIPT_CMD(do_mpotransfer)
{
	char *rest;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *dest;
	OBJ_DATA *container;
	CHAR_DATA *carrier;
	int wear_loc = WEAR_NONE;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpOtransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: obj = get_obj_here(info->mob, NULL, arg->d.str); break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: obj = NULL; break;
	}


	if (!obj) {
		bug("MpOtransfer - Null object from vnum %ld.", VNUM(info->mob));
		return;
	}

	if (PROG_FLAG(obj,PROG_AT)) return;	// Can't transfer a remote looking object

	if (IS_SET(obj->extra3_flags, ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

	argument = mp_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

	if(!dest && !container && !carrier) {
		bug("MpOtransfer - Bad location from vnum %d.", VNUM(info->mob));
		return;
	}

	if (obj->carried_by) {
		if (obj->wear_loc != WEAR_NONE)
			unequip_char(obj->carried_by, obj, TRUE);
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

// do_mppeace
SCRIPT_CMD(do_mppeace)
{
	CHAR_DATA *rch;
	if(!info || !info->mob || !info->mob->in_room) return;

	for (rch = info->mob->in_room->people; rch; rch = rch->next_in_room) {
		if (rch->fighting)
			stop_fighting(rch, TRUE);
		if (IS_NPC(rch) && IS_SET(rch->act,ACT_AGGRESSIVE))
			REMOVE_BIT(rch->act,ACT_AGGRESSIVE);
	}
}

// do_mppurge
// Syntax mob purge [<target>]
SCRIPT_CMD(do_mppurge)
{
	char *rest;
	CHAR_DATA **mobs = NULL, *victim = NULL,*vnext;
	OBJ_DATA **objs = NULL, *obj = NULL,*obj_next;
	ROOM_INDEX_DATA *here = NULL;

	EXIT_DATA *ex;

	if(!info || !info->mob || !info->mob->in_room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NONE: here = info->mob->in_room; break;
	case ENT_STRING:
		if (!(victim = get_char_room(info->mob, NULL, arg->d.str)))
			obj = get_obj_here(info->mob, NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	case ENT_ROOM: here = arg->d.room; break;
	case ENT_EXIT:
		ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
		here = ex ? exit_destination(ex) : NULL; break;
	case ENT_OLLIST_MOB: mobs = arg->d.list.ptr.mob; break;
	case ENT_OLLIST_OBJ: objs = arg->d.list.ptr.obj; break;
	default: break;
	}

	if(victim) {
		if (!IS_NPC(victim)) {
			bug("Mppurge - Attempting to purge a PC from vnum %d.", VNUM(info->mob));
			return;
		}

		if(PROG_FLAG(victim,PROG_AT)) return;

		extract_char(victim, TRUE);
	} else if(obj) {
		if(PROG_FLAG(obj,PROG_AT)) return;
		extract_obj(obj);
	} else if(here) {
		for (victim = here->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && victim != info->mob && !IS_SET(victim->act, ACT_NOPURGE))
				extract_char(victim, TRUE);
		}

		for (obj = here->contents; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (!IS_SET(obj->extra_flags, ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else if(mobs) {
		for (victim = *mobs; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && victim != info->mob && !IS_SET(victim->act, ACT_NOPURGE))
				extract_char(victim, TRUE);
		}
	} else if(objs) {
		for (obj = *objs; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (!IS_SET(obj->extra_flags, ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else
		bug("Mppurge - Bad argument from vnum %d.", VNUM(info->mob));

}

// do_mpqueue
SCRIPT_CMD(do_mpqueue)
{
	char *rest;
	int delay;


	if(!info || !info->mob || !info->mob->in_room) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: delay = arg->d.num; break;
	case ENT_STRING: delay = atoi(arg->d.str); break;
	default:
		bug("MpQueue:  missing arguments from mob vnum %d.", VNUM(info->mob));
		return;
	}

	if (delay < 0 || delay > 1000) {
		bug("MpQueue:  unreasonable delay recieved from mob vnum %d.", VNUM(info->mob));
		return;
	}

	wait_function(info->mob, info, EVENT_MOBQUEUE, delay, script_interpret, rest);
}

// do_mpraisedead
SCRIPT_CMD(do_mpraisedead)
{
	char buf[MIL], *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob || !info->mob->in_room) return;

	info->mob->progs->lastreturn = -1;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if(!victim) return;

	if (!IS_DEAD(victim)) {
		sprintf(buf, "do_mpraisedead: for mob %s(%ld), victim %s wasn't dead!",
			info->mob->pIndexData->short_descr, info->mob->pIndexData->vnum,
			victim->name);
		bug(buf, 0);

		info->mob->progs->lastreturn = 0;
//		send_to_char("{WAn intense warmth washes over you momentarily.{x\n\r", victim);
		return;
	}

	resurrect_pc(victim);
	info->mob->progs->lastreturn = 1;
}

// do_mpremember
SCRIPT_CMD(do_mpremember)
{
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!expand_argument(info,argument,arg)) {
		bug("MpRemember: Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(info->mob, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpRemember: Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	info->mob->progs->target = victim;
}

// do_mpremove
SCRIPT_CMD(do_mpremove)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj = NULL, *obj_next;
	int vnum = 0, count = 0;
	bool fAll = FALSE;

	char name[MIL], *rest;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpRemove: Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpRemove: Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpRemove: Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	name[0] = '\0';
	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING:
		if(is_number(arg->d.str))
			vnum = atoi(arg->d.str);
		else if(!str_cmp(arg->d.str,"all"))
			fAll = TRUE;
		else
			strncpy(name,arg->d.str,MIL-1);
		break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!fAll && vnum < 1 && !name[0] && !obj) {
		bug ("MpRemove: Invalid object from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!fAll && !obj && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpRemove: Bad syntax from vnum %ld.", VNUM(info->mob));
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER: count = arg->d.num; break;
		case ENT_STRING: count = atoi(arg->d.str); break;
		default: count = 0; break;
		}

		if(count < 0) {
			bug ("MpRemove: Invalid count from vnum %d.", VNUM(info->mob));
			count = 0;
		}
	}

	if(obj) {
		if(victim == obj_carrier(obj) && !PROG_FLAG(obj,PROG_AT))
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

// do_mpselfdestruct
SCRIPT_CMD(do_mpselfdestruct)
{
	char buf[MSL];

	if(!info || !info->mob || !IS_NPC(info->mob) || PROG_FLAG(info->mob,PROG_NODESTRUCT) || PROG_FLAG(info->mob,PROG_AT)) return;

	sprintf(buf, "do_mpselfdestruct: mob %s(%ld) self-destructed", info->mob->pIndexData->short_descr, info->mob->pIndexData->vnum);
	log_string(buf);

	extract_char(info->mob, TRUE);
	//info->mob = NULL;	Handled by the recycling code
}

// do_mptake
/*
 * Lets the mobile take item(s) from the victim.
 * Follows the aspect of remove, where it has a count limit as well.
 * This command will update the mobile's LASTRETURN value (for checking in the script)
 *
 * If force is used, CPK proof items can be taken!
 *
 * Syntax: mob take [victim] [object vnum|'all'|keywords] {force|safe} [count]
 */
SCRIPT_CMD(do_mptake)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj = NULL, *obj_next;
	int vnum = 0, count = 0, taken = 0;
	bool fAll = FALSE, force = FALSE;

	char name[MIL], *rest;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpTake: Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("MpTake: Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpTake: Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	name[0] = '\0';
	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING:
		if(is_number(arg->d.str))
			vnum = atoi(arg->d.str);
		else if(!str_cmp(arg->d.str,"all"))
			fAll = TRUE;
		else
			strncpy(name,arg->d.str,MIL-1);
		break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!fAll && vnum < 1 && !name[0] && !obj) {
		bug ("MpTake: Invalid object from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(*rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpTake: Bad syntax from vnum %ld.", VNUM(info->mob));
			return;
		}

		switch(arg->type) {
		default: break;
		case ENT_STRING: force = !str_cmp(arg->d.str,"force"); break;
		}
	}

	if(!fAll && !obj && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpTake: Bad syntax from vnum %ld.", VNUM(info->mob));
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER: count = arg->d.num; break;
		case ENT_STRING: count = atoi(arg->d.str); break;
		case ENT_OBJECT: obj = arg->d.obj; break;
		default: count = 0; break;
		}

		if(count < 0) {
			bug ("MpTake: Invalid count from vnum %d.", VNUM(info->mob));
			count = 0;
		}
	}

	taken = 0;
	if(obj) {
		if((obj->carried_by == victim || (obj->in_obj && obj->in_obj->carried_by == victim)) &&
			victim->recite_scroll != obj && (force || can_drop_obj(victim,obj,TRUE)) &&
			(info->mob->carry_number < can_carry_n (info->mob)) &&
			((get_carry_weight (info->mob) + get_obj_weight (obj)) <= can_carry_w (info->mob))) {
			obj_from_char(obj);
			obj_to_char(obj,info->mob);
			taken++;
		}
	} else {
		for (obj = victim->carrying; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum) ||
				(*name && is_name(name, obj->pIndexData->skeywds))) {
				// Not even FORCE will allow this...
				if(victim->recite_scroll == obj) continue;
				// Can it be taken?
				if(!force && !can_drop_obj(victim,obj,TRUE)) continue;
				// Can it carry anymore?
				if(info->mob->carry_number >= can_carry_n (info->mob)) break;
				if(get_carry_weight (info->mob) + get_obj_weight (obj) > can_carry_w (info->mob)) break;
				obj_from_char(obj);
				obj_to_char(obj,info->mob);
				taken++;

				if(count > 0 && !--count) break;
			}
		}
	}

	// Update the amount taken, using lastreturn
	info->mob->progs->lastreturn = taken;
}


// do_mpteleport
SCRIPT_CMD(do_mpteleport)
{
	ROOM_INDEX_DATA *room;

	if(!info || !info->mob || PROG_FLAG(info->mob,PROG_AT)) return;

	if ((room = get_random_room(info->mob, BOTH_CONTINENTS))) {
		char_from_room(info->mob);
		char_to_room(info->mob, room);
	}
}

// do_mptransfer
// Syntax: mob transfer <target>|all [<location> [force|safe] [quiet]]
SCRIPT_CMD(do_mptransfer)
{
	char buf[MIL], buf2[MIL], *rest;
	CHAR_DATA *victim = NULL,*vnext;
	ROOM_INDEX_DATA *dest;
	bool all = FALSE, force = FALSE, quiet = TRUE;
	int mode;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpTransfer - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all"))

			all = TRUE;
		else
			victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim && !all) {
		bug("MpTransfer - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	// This was crashing for any transfer all scripts as victim is not set on all.
	//if (!victim) return;

	argument = mp_getlocation(info, rest, &dest);

	if(!dest) {
		bug("MpTransfer - Bad location from vnum %d.", VNUM(info->mob));
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
		for (victim = info->mob->in_room->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (PROG_FLAG(victim,PROG_AT)) continue;
			if (!IS_NPC(victim)) {
				if (!force && room_is_private(dest, info->mob)) break;
				do_mob_transfer(victim,dest,quiet,mode);
			}
		}
		return;
	}

	if (!force && room_is_private(dest,info->mob))
		return;

	if (PROG_FLAG(victim,PROG_AT))
		return;

	do_mob_transfer(victim,dest,quiet,mode);
}

// do_mpvforce
SCRIPT_CMD(do_mpvforce)
{
	char *rest;
	int vnum = 0;
	CHAR_DATA *vch, *next;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpVforce - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: break;
	}

	if (vnum < 1) {
		bug("MpVforce - Invalid vnum from vnum %ld.", VNUM(info->mob));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if(buf_string(buffer)[0] == '\0') {
		bug("MpGforce - Error in parsing from vnum %ld.", VNUM(info->mob));
		free_buf(buffer);
		return;
	}

	for (vch = info->mob->in_room->people; vch; vch = next) {
		next = vch->next_in_room;
		if (IS_NPC(vch) && vch->pIndexData->vnum == vnum &&
			get_trust(vch) < get_trust(info->mob)
			&& can_see(info->mob, vch)
			&& (IS_NPC(vch) || !IS_IMMORTAL(vch)))
			interpret(vch, buf_string(buffer));
	}
	free_buf(buffer);
}

// do_mpvis
SCRIPT_CMD(do_mpvis)
{
	if(!info || !info->mob) return;

	info->mob->invis_level = 0;
}

// do_mpzecho
// Syntax: mob zecho <string>
SCRIPT_CMD(do_mpzecho)
{
	DESCRIPTOR_DATA *d;

	if(!info || !info->mob) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if (!buf_string(buffer)[0]) {
		bug("MpZEcho: missing argument from vnum %d", VNUM(info->mob));
		free_buf(buffer);
		return;
	}

	for (d = descriptor_list; d; d = d->next)
	{
		if (d->connected == CON_PLAYING &&
			d->character->in_room &&
			d->character->in_room->area == info->mob->in_room->area) {
			if (IS_IMMORTAL(d->character))
				send_to_char("Mob echo> ", d->character);
			send_to_char(buf_string(buffer), d->character);
			send_to_char("\n\r", d->character);
		}
	}

	free_buf(buffer);
}

// do_mpzot
// Syntax: mob zot <victim>
SCRIPT_CMD(do_mpzot)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("MpZot - Null victim from vnum %ld.", VNUM(info->mob));
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


SCRIPT_CMD(do_mpvarset)
{
	if(!info || !info->mob || !info->var) return;

	script_varseton(info,info->var,argument, arg);
}

SCRIPT_CMD(do_mpvarclear)
{
	if(!info || !info->mob || !info->var) return;

	script_varclearon(info,info->var, argument, arg);
}

SCRIPT_CMD(do_mpvarcopy)
{
	char oldname[MIL],newname[MIL];

	if(!info || !info->mob || !info->var) return;

	// Get name
	argument = one_argument(argument,oldname);
	if(!oldname[0]) return;
	argument = one_argument(argument,newname);
	if(!newname[0]) return;

	if(!str_cmp(oldname,newname)) return;

	variable_copy(info->var,oldname,newname);
}

SCRIPT_CMD(do_mpvarsave)
{
	char name[MIL],arg1[MIL];
	bool on;

	if(!info || !info->mob || !info->var) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,arg1);
	if(!arg1[0]) return;

	on = !str_cmp(arg1,"on") || !str_cmp(arg1,"true") || !str_cmp(arg1,"yes");

	variable_setsave(*info->var,name,on);
}

SCRIPT_CMD(do_mpsettimer)
{
	char buf[MIL],*rest;
	int amt;
	CHAR_DATA *victim = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"self"))
			victim = info->mob;
		else
			victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	default: break;
	}

	if(!victim) {
		bug("MpSetTimer - NULL victim.", 0);
		return;
	}

	if(!*rest) {
		bug("MpSetTimer - Missing timer type.",0);
		return;
	}

	buf[0] = 0;
	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		strncpy(buf,arg->d.str,MIL);
		break;
	default: break;
	}

	if(!*rest) {
		bug("MpSetTimer - Missing timer amount.",0);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpSetTimer - Error in parsing.",0);
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
			SET_BIT(victim->act2, ACT2_HIRED);
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

SCRIPT_CMD(do_mpinterrupt)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL;
	ROOM_INDEX_DATA *here;

	int stop, ret = 0;
	bool silent = FALSE;

	if(!info || !info->mob) return;

	here = info->mob->in_room;

	info->mob->progs->lastreturn = 0;	// Nothing was interrupted

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpInterrupt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	default: break;
	}

	if(!victim) {
		bug("MpInterrupt - NULL victim.", 0);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if(!buf_string(buffer)[0]) {
		stop = flag_value(interrupt_action_types,buf);
		if(stop == NO_FLAG) {
			bug("MpInterrupt - invalid interrupt type.", 0);
			free_buf(buffer);
			return;
		}
	} else
		stop = ~INTERRUPT_SILENT;	// stop anything

	ret = 0;

	if (IS_SET(stop,INTERRUPT_SILENT))
		silent = TRUE;

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
			variables_set_string(info->var,"stoprecitetarget",victim->cast_target_name,FALSE);
		else
			variables_set_string(info->var,"stoprecitetarget","",FALSE);
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
			variables_set_string(info->var,"stoprangedtarget",victim->projectile_victim,FALSE);
		else
			variables_set_string(info->var,"stoprangedtarget","",FALSE);
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
	info->mob->progs->lastreturn = ret;

	free_buf(buffer);
}

SCRIPT_CMD(do_mpalterobj)
{
	char buf[2*MIL],field[MIL],*rest;
	int value, num, min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;

	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(info->mob,NULL,arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("MpAlterObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("MpAlterObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAlterObj - Error in parsing.",0);
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
		bug("MpAlterObj - Error in parsing.",0);
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
			sprintf(buf,"MpAlterObj - Attempting to alter value%d with security %d.\n\r", num, script_security);
			bug(buf, 0);
			return;
		}

		switch (buf[0]) {
		case '+': obj->value[num] += value; break;
		case '-': obj->value[num] -= value; break;
		case '*': obj->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("MpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			obj->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("MpAlterObj - adjust called with operator % and value 0", 0);
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

		if(!str_cmp(field,"extra"))			{ ptr = (int*)&obj->extra_flags; flags = extra_flags; }
		else if(!str_cmp(field,"extra2"))	{ ptr = (int*)&obj->extra2_flags; flags = extra2_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra3"))	{ ptr = (int*)&obj->extra3_flags; flags = extra3_flags; min_sec = 7; }
		else if(!str_cmp(field,"extra4"))	{ ptr = (int*)&obj->extra4_flags; flags = extra4_flags; min_sec = 7; }
		else if(!str_cmp(field,"wear"))		{ ptr = (int*)&obj->wear_flags; flags = wear_flags; }
		else if(!str_cmp(field,"wearloc"))	{ ptr = (int*)&obj->wear_loc; flags = wear_loc_flags; }
		else if(!str_cmp(field,"weight"))	ptr = (int*)&obj->weight;
		else if(!str_cmp(field,"cond"))		ptr = (int*)&obj->condition;
		else if(!str_cmp(field,"timer"))	ptr = (int*)&obj->timer;
		else if(!str_cmp(field,"level"))	{ ptr = (int*)&obj->level; min_sec = 5; }
		else if(!str_cmp(field,"repairs"))	ptr = (int*)&obj->times_fixed;
		else if(!str_cmp(field,"fixes"))	{ ptr = (int*)&obj->times_allowed_fixed; min_sec = 5; }
		else if(!str_cmp(field,"type"))		{ ptr = (int*)&obj->item_type; flags = type_flags; min_sec = 7; }
		else if(!str_cmp(field,"cost"))		{ ptr = (int*)&obj->cost; min_sec = 5; }
		else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&obj->tempstore[0];
		else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&obj->tempstore[1];
		else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&obj->tempstore[2];
		else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&obj->tempstore[3];

		if(!ptr) return;

		if(script_security < min_sec) {
			sprintf(buf,"MpAlterObj - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
					if( IS_SET(*ptr, ITEM_INSTANCE_OBJ) ) SET_BIT(value, ITEM_INSTANCE_OBJ);
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
				bug("MpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value;
			break;

		case '-':
			if( !allowarith ) {
				bug("MpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value;
			break;

		case '*':
			if( !allowarith ) {
				bug("MpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value;
			break;

		case '/':
			if( !allowarith ) {
				bug("MpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("MpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("MpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("MpAlterObj - adjust called with operator % and value 0", 0);
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
	}
}



SCRIPT_CMD(do_mpresetdice)
{
	char *rest;
	OBJ_DATA *obj = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(info->mob,NULL,arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("MpAlterObj - NULL object.", 0);
		return;
	}

	if(obj->item_type == ITEM_WEAPON)
		set_weapon_dice_obj(obj);
}




SCRIPT_CMD(do_mpstringobj)
{
	char buf[2*MSL],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;

	bool newlines = FALSE;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStringObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(info->mob,NULL,arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("MpStringObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("MpStringObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpStringObj - Error in parsing.",0);
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

	if(!buf_string(buffer)[0]) {
		bug("MpStringObj - Empty string used.",0);
		free_buf(buffer);
		return;
	}

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
		newlines = TRUE;		// allow newlines
	} else if(!str_cmp(field,"material")) {
		int mat = material_lookup(buf_string(buffer));

		if(mat < 0) {
			bug("MpStringObj - Invalid material.\n\r", 0);
			free_buf(buffer);
			return;
		}

		// Force material to the full name
		clear_buf(buffer);
		add_buf(buffer,material_table[mat].name);

		str = (char**)&obj->material;
	}
	else
	{
		free_buf(buffer);
		return;
	}

	if(script_security < min_sec) {
		sprintf(buf,"MpStringObj - Attempting to restring '%s' with security %d.\n\r", field, script_security);
		bug(buf, 0);
		free_buf(buffer);
		return;
	}

	char *p = buf_string(buffer);
	strip_newline(p, newlines);

	free_string(*str);
	*str = str_dup(p);

	free_buf(buffer);
}

SCRIPT_CMD(do_mpaltermob)
{
	char buf[MSL],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY, min = 0, max = 0;
	CHAR_DATA *mob = NULL;

	int *ptr = NULL;
	bool allowpc = FALSE;
	bool allowarith = TRUE;
	bool allowbitwise = TRUE;
	bool lookuprace = FALSE;
	bool hasmin = FALSE;
	bool hasmax = FALSE;
	const struct flag_type *flags = NULL;
	int dirty_stat = -1;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("MpAlterMob - NULL mobile.", 0);
		return;
	}

	if(!*rest) {
		bug("MpAlterMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAlterMob - Error in parsing.",0);
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
		bug("MpAlterMob - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"acbash"))		ptr = (int*)&mob->armour[AC_BASH];
	else if(!str_cmp(field,"acexotic"))	ptr = (int*)&mob->armour[AC_EXOTIC];
	else if(!str_cmp(field,"acpierce"))	ptr = (int*)&mob->armour[AC_PIERCE];
	else if(!str_cmp(field,"acslash"))	ptr = (int*)&mob->armour[AC_SLASH];
	else if(!str_cmp(field,"act"))		{ ptr = (int*)&mob->act; flags = IS_NPC(mob) ? act_flags : plr_flags; }
	else if(!str_cmp(field,"act2"))		{ ptr = (int*)&mob->act2; flags = IS_NPC(mob) ? act2_flags : plr2_flags; }
	else if(!str_cmp(field,"affect"))	{ ptr = (int*)&mob->affected_by; flags = affect_flags; }
	else if(!str_cmp(field,"affect2"))	{ ptr = (int*)&mob->affected_by2; flags = affect2_flags; }
	else if(!str_cmp(field,"alignment"))	ptr = (int*)&mob->alignment;
	else if(!str_cmp(field,"bashed"))	ptr = (int*)&mob->bashed;
	else if(!str_cmp(field,"bind"))		ptr = (int*)&mob->bind;
	else if(!str_cmp(field,"bomb"))		ptr = (int*)&mob->bomb;
	else if(!str_cmp(field,"brew"))		ptr = (int*)&mob->brew;
	else if(!str_cmp(field,"cast"))		ptr = (int*)&mob->cast;
	else if(!str_cmp(field,"comm"))		{ ptr = IS_NPC(mob)?NULL:(int*)&mob->comm; allowpc = TRUE; allowarith = FALSE; min_sec = 7; flags = comm_flags; }		// 20140512NIB - Allows for scripted fun with player communications, only bit operators allowed
	else if(!str_cmp(field,"damroll"))	ptr = (int*)&mob->damroll;
	else if(!str_cmp(field,"danger"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->danger_range; allowpc = TRUE; }
	else if(!str_cmp(field,"daze"))		ptr = (int*)&mob->daze;
	else if(!str_cmp(field,"death"))	{ ptr = (IS_NPC(mob) || !IS_DEAD(mob))?NULL:(int*)&mob->time_left_death; allowpc = TRUE; }
	else if(!str_cmp(field,"dicenumber"))	{ ptr = IS_NPC(mob)?&mob->damage.number:NULL; }
	else if(!str_cmp(field,"dicetype"))	{ ptr = IS_NPC(mob)?&mob->damage.size:NULL; }
	else if(!str_cmp(field,"dicebonus"))	{ ptr = IS_NPC(mob)?&mob->damage.bonus:NULL; }
	else if(!str_cmp(field,"drunk"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_DRUNK]; allowpc = TRUE; }
//	else if(!str_cmp(field,"exitdir"))	{ ptr = (int*)&mob->exit_dir; allowpc = TRUE; }
	else if(!str_cmp(field,"exp"))		{ ptr = (int*)&mob->exp; allowpc = TRUE; }
	else if(!str_cmp(field,"fade"))		ptr = (int*)&mob->fade;
	else if(!str_cmp(field,"fullness"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_FULL]; allowpc = TRUE; }
	else if(!str_cmp(field,"gold"))		ptr = (int*)&mob->gold;
	else if(!str_cmp(field,"hide"))		ptr = (int*)&mob->hide;
	else if(!str_cmp(field,"hit"))		ptr = (int*)&mob->hit;
	else if(!str_cmp(field,"hitdamage"))	ptr = (int*)&mob->hit_damage;
	else if(!str_cmp(field,"hitroll"))	ptr = (int*)&mob->hitroll;
	else if(!str_cmp(field,"hunger"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_HUNGER]; allowpc = TRUE; }
	else if(!str_cmp(field,"imm"))		{ ptr = (int*)&mob->imm_flags; allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"level"))	ptr = (int*)&mob->tot_level;
	else if(!str_cmp(field,"lostparts"))	{ ptr = (int*)&mob->lostparts; allowarith = FALSE; flags = part_flags; }
	else if(!str_cmp(field,"mana"))		ptr = (int*)&mob->mana;
	else if(!str_cmp(field,"manastore"))	{ ptr = (int*)&mob->manastore; allowpc = TRUE; }
//	else if(!str_cmp(field,"material"))	ptr = (int*)&mob->material;
	else if(!str_cmp(field,"maxexp"))	ptr = (int*)&mob->maxexp;
	else if(!str_cmp(field,"maxhit"))	ptr = (int*)&mob->max_hit;
	else if(!str_cmp(field,"maxmana"))	ptr = (int*)&mob->max_mana;
	else if(!str_cmp(field,"maxmove"))	ptr = (int*)&mob->max_move;
	else if(!str_cmp(field,"mazed"))	{ ptr = (IS_NPC(mob))?NULL:(int*)&mob->maze_time_left; allowpc = TRUE; }
	else if(!str_cmp(field,"modcon"))	{ ptr = (int*)&mob->mod_stat[STAT_CON]; allowpc = TRUE; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_CON; }
	else if(!str_cmp(field,"moddex"))	{ ptr = (int*)&mob->mod_stat[STAT_DEX]; allowpc = TRUE; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_DEX; }
	else if(!str_cmp(field,"modint"))	{ ptr = (int*)&mob->mod_stat[STAT_INT]; allowpc = TRUE; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_INT; }
	else if(!str_cmp(field,"modstr"))	{ ptr = (int*)&mob->mod_stat[STAT_STR]; allowpc = TRUE; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_STR; }
	else if(!str_cmp(field,"modwis"))	{ ptr = (int*)&mob->mod_stat[STAT_WIS]; allowpc = TRUE; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_WIS; }
	else if(!str_cmp(field,"move"))		ptr = (int*)&mob->move;
	else if(!str_cmp(field,"music"))	ptr = (int*)&mob->music;
	else if(!str_cmp(field,"norecall"))	ptr = (int*)&mob->no_recall;
	else if(!str_cmp(field,"panic"))	ptr = (int*)&mob->panic;
	else if(!str_cmp(field,"paralyzed"))	ptr = (int*)&mob->paralyzed;
	else if(!str_cmp(field,"paroxysm"))	ptr = (int*)&mob->paroxysm;
	else if(!str_cmp(field,"parts"))	{ ptr = (int*)&mob->parts; allowarith = FALSE; flags = part_flags; }
	else if(!str_cmp(field,"permaffects"))	{ ptr = (int*)&mob->affected_by_perm; allowarith = FALSE; flags = affect_flags; }
	else if(!str_cmp(field,"permaffects2"))	{ ptr = (int*)&mob->affected_by2_perm; allowarith = FALSE; flags = affect2_flags; }
	else if(!str_cmp(field,"permimm"))	{ ptr = (int*)&mob->imm_flags_perm; allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"permres"))	{ ptr = (int*)&mob->res_flags_perm; allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"permvuln"))	{ ptr = (int*)&mob->vuln_flags_perm; allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"pktimer"))	ptr = (int*)&mob->pk_timer;
	else if(!str_cmp(field,"pneuma"))	ptr = (int*)&mob->pneuma;
	else if(!str_cmp(field,"practice"))	ptr = (int*)&mob->practice;
	else if(!str_cmp(field,"race"))		{ ptr = (int*)&mob->race; min_sec = 7; allowarith = FALSE; lookuprace = TRUE; }
	else if(!str_cmp(field,"ranged"))	ptr = (int*)&mob->ranged;
	else if(!str_cmp(field,"recite"))	ptr = (int*)&mob->recite;
	else if(!str_cmp(field,"res"))		{ ptr = (int*)&mob->res_flags;  allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"resurrect"))	ptr = (int*)&mob->resurrect;
	else if(!str_cmp(field,"reverie"))	ptr = (int*)&mob->reverie;
	else if(!str_cmp(field,"scribe"))	ptr = (int*)&mob->scribe;
	else if(!str_cmp(field,"sex"))		{ ptr = (int*)&mob->sex; min = 0; max = 2; hasmin = hasmax = TRUE; flags = sex_flags; }
	else if(!str_cmp(field,"silver"))	ptr = (int*)&mob->silver;
	else if(!str_cmp(field,"size"))		{ ptr = (int*)&mob->size; min = SIZE_TINY; max = SIZE_GIANT; hasmin = hasmax = TRUE; flags = size_flags; }
	else if(!str_cmp(field,"skillchance"))	ptr = (int*)&mob->skill_chance;
	else if(!str_cmp(field,"sublevel"))	ptr = (int*)&mob->level;
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&mob->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&mob->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&mob->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&mob->tempstore[3];
	else if(!str_cmp(field,"thirst"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_THIRST]; allowpc = TRUE; }
	else if(!str_cmp(field,"toxinneuro"))	ptr = (int*)&mob->toxin[TOXIN_NEURO];
	else if(!str_cmp(field,"toxinpara"))	ptr = (int*)&mob->toxin[TOXIN_PARALYZE];
	else if(!str_cmp(field,"toxinvenom"))	ptr = (int*)&mob->toxin[TOXIN_VENOM];
	else if(!str_cmp(field,"toxinweak"))	ptr = (int*)&mob->toxin[TOXIN_WEAKNESS];
	else if(!str_cmp(field,"train"))	ptr = (int*)&mob->train;
	else if(!str_cmp(field,"trance"))	ptr = (int*)&mob->trance;
	else if(!str_cmp(field,"vuln"))		{ ptr = (int*)&mob->vuln_flags; allowarith = FALSE; flags = imm_flags; }
	else if(!str_cmp(field,"wait"))		ptr = (int*)&mob->wait;
	else if(!str_cmp(field,"wildviewx"))	ptr = (int*)&mob->wildview_bonus_x;
	else if(!str_cmp(field,"wildviewy"))	ptr = (int*)&mob->wildview_bonus_y;
	else if(!str_cmp(field,"wimpy"))	ptr = (int*)&mob->wimpy;

	if(!ptr) return;


	// MINIMUM to alter ANYTHING not allowed on players on a player
	if(!allowpc && !IS_NPC(mob)) min_sec = 9;

	if(script_security < min_sec) {
		sprintf(buf,"MpAlterMob - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		bug(buf, 0);
		return;
	}

	if( lookuprace )
	{
		if( arg->type != ENT_STRING ) return;

		// This is a race, can only be assigned
		allowarith = FALSE;
		allowbitwise = FALSE;
		value = race_lookup(arg->d.str);
	}
	else if( flags != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = FALSE;	// This is a bit vector, no arithmetic operators.
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
			bug("MpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr += value; break;
	case '-':
		if( !allowarith ) {
			bug("MpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr -= value; break;
	case '*':
		if( !allowarith ) {
			bug("MpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr *= value; break;
	case '/':
		if( !allowarith ) {
			bug("MpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("MpAlterMob - altermob called with operator / and value 0", 0);
			return;
		}
		*ptr /= value;
		break;
	case '%':
		if( !allowarith ) {
			bug("MpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("MpAlterMob - altermob called with operator % and value 0", 0);
			return;
		}
		*ptr %= value;
		break;

	case '=':
		*ptr = value;
		break;

	case '&':
		if( !allowbitwise ) {
			bug("MpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr &= value;
		break;
	case '|':
		if( !allowbitwise ) {
			bug("MpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr |= value;
		break;
	case '!':
		if( !allowbitwise ) {
			bug("MpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr &= ~value;
		break;
	case '^':
		if( !allowbitwise ) {
			bug("MpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

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
		mob->dirty_stat[dirty_stat] = TRUE;
}


SCRIPT_CMD(do_mpstringmob)
{
	char buf[MSL+2],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	CHAR_DATA *mob = NULL;

	bool newlines = FALSE;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStringMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("MpStringMob - NULL mobile.", 0);
		return;
	}

	if(!IS_NPC(mob)) {
		bug("MpStringMob - can't change strings on PCs.", 0);
		return;
	}

	if(!*rest) {
		bug("MpStringMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpStringMob - Error in parsing.",0);
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

	if(!buf_string(buffer)[0]) {
		bug("MpStringMob - Empty string used.",0);
		free_buf(buffer);
		return;
	}

	if(!str_cmp(field,"name"))				str = (char**)&mob->name;
	else if(!str_cmp(field,"owner"))		{ str = (char**)&mob->owner; min_sec = 5; }
	else if(!str_cmp(field,"short"))		str = (char**)&mob->short_descr;
	else if(!str_cmp(field,"long"))			{ str = (char**)&mob->long_descr; strcat(buf,"\n\r"); newlines = TRUE; }
	else if(!str_cmp(field,"full"))			{ str = (char**)&mob->description; newlines = TRUE; }
	else if(!str_cmp(field,"tempstring"))	str = (char**)&mob->tempstring;
	else
	{
		free_buf(buffer);
		return;
	}


	if(script_security < min_sec) {
		sprintf(buf,"MpStringMob - Attempting to restring '%s' with security %d.\n\r", field, script_security);
		bug(buf, 0);
		free_buf(buffer);
		return;
	}

	char *p = buf_string(buffer);
	strip_newline(p, newlines);

	free_string(*str);
	*str = str_dup(p);

	free_buf(buffer);

}

SCRIPT_CMD(do_mpskimprove)
{
	char skill[MIL],*rest;
	int min_diff, diff, sn = -1;
	CHAR_DATA *mob = NULL;

	TOKEN_DATA *token = NULL;
	bool success = FALSE;

	if(script_security < MIN_SCRIPT_SECURITY) {
		bug("MpSkImprove - Insufficient security.",0);
		return;
	}

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	case ENT_TOKEN:
		token = arg->d.token;
	default: break;
	}

	if(!mob && !token) {
		bug("MpSkImprove - NULL target.", 0);
		return;
	}

	if(mob) {
		if(IS_NPC(mob)) {
			bug("MpSkImprove - NPCs don't have skills to improve yet...", 0);
			return;
		}


		if(!(rest = expand_argument(info,rest,arg))) {
			bug("MpSkImprove - Error in parsing.",0);
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
			bug("MpSkImprove - Token is not a spell token...", 0);
			return;
		}
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: diff = arg->d.num; break;
	default: return;
	}

	min_diff = 10 - script_security;	// min=10, max=1

	if(diff < min_diff) {
		bug("MpSkImprove - Attempting to use a difficulty multiplier lower than allowed.",0);
		diff = min_diff;
	}

	switch(arg->type) {
	case ENT_NONE: success = TRUE; break;
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
	default: success = FALSE;
	}

	if(token)
		token_skill_improve(token->player,token,success,diff);
	else
		check_improve( mob, sn, success, diff );
}

SCRIPT_CMD(do_mpinput)
{
	char *rest, *p;
	int vnum;
	CHAR_DATA *mob = NULL;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("MpInput - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: return;
	}

	if(vnum < 1 || !get_script_index(vnum, PRG_MPROG)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpInput - Error in parsing.",0);
		return;
	}


	switch(arg->type) {
	case ENT_NONE:		p = NULL; break;
	case ENT_STRING:	p = arg->d.str; break;
	default: return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	mob->desc->input = TRUE;
	mob->desc->input_var = p ? str_dup(p) : NULL;
	mob->desc->input_prompt = str_dup(buffer->string[0] ? buffer->string : " >");
	mob->desc->input_script = vnum;
	mob->desc->input_mob = info->mob;
	mob->desc->input_obj = NULL;
	mob->desc->input_room = NULL;
	mob->desc->input_tok = NULL;

	info->mob->progs->lastreturn = 1;
	free_buf(buffer);
}

SCRIPT_CMD(do_mprawkill)
{
	char *rest;
	int type;
	bool has_head, show_msg;
	CHAR_DATA *mob = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("MpRawkill - NULL mobile.", 0);
		return;
	}

	if(IS_IMMORTAL(mob)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
	default: return;
	}

	if(type < 0 || type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:	has_head = TRUE; break;
	case ENT_STRING:
		has_head = !str_cmp(arg->d.str,"true") ||
			!str_cmp(arg->d.str,"yes") ||
			!str_cmp(arg->d.str,"head");
		break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:	show_msg = TRUE; break;
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

SCRIPT_CMD(do_mpaddaffect)
{
	char *rest;
	int where, group, skill, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffect <target> <where> <group> <skill> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(info->mob, NULL, arg->d.str)))
			obj = get_obj_here(info->mob, NULL, arg->d.str);
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
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
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

SCRIPT_CMD(do_mpaddaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffectname <target> <where> <name> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(info->mob, NULL, arg->d.str)))
			obj = get_obj_here(info->mob, NULL, arg->d.str);
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


SCRIPT_CMD(do_mpstripaffect)
{
	char *rest;
	int skill;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffect <target> <skill>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(info->mob, NULL, arg->d.str)))
			obj = get_obj_here(info->mob, NULL, arg->d.str);
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
	case ENT_STRING: skill = skill_lookup(arg->d.str); break;
	default: return;
	}

	if(skill < 0) return;

	if(mob) affect_strip(mob, skill);
	else affect_strip_obj(obj,skill);
}

SCRIPT_CMD(do_mpstripaffectname)
{
	char *rest, *name;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffectname <target> <name>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(info->mob, NULL, arg->d.str)))
			obj = get_obj_here(info->mob, NULL, arg->d.str);
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

SCRIPT_CMD(do_mpusecatalyst)
{
	char *rest;
	int type, method, amount, min, max, show;
	CHAR_DATA *mob = NULL;
	ROOM_INDEX_DATA *room = NULL;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	// usecatalyst <target> <type> <method> <amount> <min> <max> <show>

	switch(arg->type) {
	case ENT_STRING: mob = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	default: break;
	}

	if(!mob && !room) {
		bug("MpUseCatalyst - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
	default: return;
	}

	if(type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
	default: return;
	}

	if(method == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: amount = arg->d.num; break;
	case ENT_STRING: amount = atoi(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: min = arg->d.num; break;
	case ENT_STRING: min = atoi(arg->d.str); break;
	default: return;
	}

	if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: max = arg->d.num; break;
	case ENT_STRING: max = atoi(arg->d.str); break;
	default: return;
	}

	if(max < min || max > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: show = flag_value(boolean_types,arg->d.str); break;
	default: return;
	}

	if(show == NO_FLAG) return;

	info->mob->progs->lastreturn = use_catalyst(mob,room,type,method,amount,min,max,(bool)show);
}
/*
// do_mphunt
// Syntax: mob hunt <victim>
SCRIPT_CMD(do_mphunt)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type == ENT_NONE) {
		info->mob->hunting = NULL;
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(info->mob, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim)
		return;

	info->mob->hunting = victim;
}*/

SCRIPT_CMD(do_mpalterexit)
{
	char buf[MSL+2],field[MIL],*rest;
	int value,min_sec = MIN_SCRIPT_SECURITY, door;
	ROOM_INDEX_DATA *room;
	EXIT_DATA *ex = NULL;

	int *ptr = NULL;
	sh_int *sptr = NULL;
	char **str;
	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterExit - Error in parsing.",0);
		return;
	}

	room = info->mob->in_room;

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
			bug("MpAlterExit - Error in parsing.",0);
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
		bug("MpAlterExit - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAlterExit - Error in parsing.",0);
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
			bug("MpAlterExit - Error in parsing.",0);
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

		if(!buf_string(buffer)[0]) {
			bug("MpAlterExit - Empty string used.",0);
			free_buf(buffer);
			return;
		}

		free_string(*str);
		*str = str_dup(buf_string(buffer));
		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterExit - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: value = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: value = arg->d.num; break;
	default: return;
	}

	if(!str_cmp(field,"flags"))			{ ptr = (int*)&ex->exit_info; flags = exit_flags; }
	else if(!str_cmp(field,"resets"))	{ ptr = (int*)&ex->rs_flags; flags = exit_flags; }
	else if(!str_cmp(field,"strength"))	sptr = (sh_int*)&ex->door.strength;
	else if(!str_cmp(field,"key"))		ptr = (int*)&ex->door.key_vnum;

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"MpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		bug(buf, 0);
		return;
	}

	if( flags != NULL )
	{
		if( arg->type != ENT_STRING || !ptr ) return;

		allowarith = FALSE;	// This is a bit vector, no arithmetic operators.
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
				bug("MpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr += value;
			break;
		case '-':
			if( !allowarith ) {
				bug("MpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr -= value;
			break;
		case '*':
			if( !allowarith ) {
				bug("MpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr *= value;
			break;
		case '/':
			if( !allowarith ) {
				bug("MpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("MpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("MpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("MpAlterExit - adjust called with operator % and value 0", 0);
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
				bug("MpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("MpAlterExit - adjust called with operator % and value 0", 0);
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


// SYNTAX: mob prompt <player> <name>[ <string>]
SCRIPT_CMD(do_mpprompt)
{
	char name[MIL],*rest;
	CHAR_DATA *mob = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpPrompt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(info->mob,NULL,arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("MpPrompt - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob)) {
		bug("MpPrompt - cannot set prompt strings on NPCs.", 0);
		return;
	}

	if(!*rest) {
		bug("MpPrompt - Missing name type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpPrompt - Error in parsing.",0);
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

	// An empty string will clear it

	string_vector_set(&mob->pcdata->script_prompts,name,buf_string(buffer));
	free_buf(buffer);
}

SCRIPT_CMD(do_mpvarseton)
{

	VARIABLE **vars;

	if(!info || !info->mob) return;

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

SCRIPT_CMD(do_mpvarclearon)
{

	VARIABLE **vars;

	if(!info || !info->mob) return;

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

SCRIPT_CMD(do_mpvarsaveon)
{
	char name[MIL],buf[MIL];
	bool on;

	VARIABLE *vars;

	if(!info || !info->mob) return;

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
SCRIPT_CMD(do_mpcloneroom)
{
	char name[MIL];
	long vnum;
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	TOKEN_DATA *tok;
	ROOM_INDEX_DATA *source, *room, *clone;
	bool no_env = FALSE;

	if(!info || !info->mob) return;

	info->progs->lastreturn = 0;

	// Get vnum
	if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
		return;

	vnum = arg->d.num;

	source = get_room_index(vnum);
	if(!source) return;

	if( IS_SET(source->room2_flags, ROOM_NOCLONE) )
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
			no_env = TRUE;
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
SCRIPT_CMD(do_mpalterroom)
{
	char buf[MSL+2],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY;
	ROOM_INDEX_DATA *room;
	WILDS_DATA *wilds;

	int *ptr = NULL;
	sh_int *sptr = NULL;
	char **str;
	bool allow_empty = FALSE;
	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterRoom - Error in parsing.",0);
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
		bug("MpAlterRoom - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAlterRoom - Error in parsing.",0);
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


	// Setting the environment of a clone room
	if(!str_cmp(field,"environment") || !str_cmp(field,"environ") ||
		!str_cmp(field,"extern") || !str_cmp(field,"outside")) {

		if(!(rest = expand_argument(info,rest,arg))) {
			bug("MpAlterRoom - Error in parsing.",0);
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
	else if(!str_cmp(field,"desc"))		{ str = &room->description; allow_empty = TRUE; }
	else if(!str_cmp(field,"owner"))	{ str = &room->owner; allow_empty = TRUE; min_sec = 9; }

	if(str) {
		if(script_security < min_sec) {
			sprintf(buf,"MpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
			bug(buf, 0);
			return;
		}

		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if(!allow_empty && !buf_string(buffer)[0]) {
			bug("MpAlterRoom - Empty string used.",0);
			free_buf(buffer);
			return;
		}

		free_string(*str);
		*str = str_dup(buf_string(buffer));
		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAlterRoom - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"room"))			{ ptr = (int*)&room->room_flags; flags = room_flags; }
	else if(!str_cmp(field,"room2"))	{ ptr = (int*)&room->room2_flags; flags = room2_flags; }
	else if(!str_cmp(field,"light"))	ptr = (int*)&room->light;
	else if(!str_cmp(field,"sector"))	ptr = (int*)&room->sector_type;
	else if(!str_cmp(field,"heal"))		{ ptr = (int*)&room->heal_rate; min_sec = 9; }
	else if(!str_cmp(field,"mana"))		{ ptr = (int*)&room->mana_rate; min_sec = 9; }
	else if(!str_cmp(field,"move"))		{ ptr = (int*)&room->move_rate; min_sec = 1; }
	else if(!str_cmp(field,"mapx"))		{ ptr = (int*)&room->x; min_sec = 5; }
	else if(!str_cmp(field,"mapy"))		{ ptr = (int*)&room->y; min_sec = 5; }
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&room->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&room->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&room->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&room->tempstore[3];

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"MpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
		wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
		bug(buf, 0);
		return;
	}

	if( flags != NULL )
	{
		if( arg->type != ENT_STRING ) return;

		allowarith = FALSE;	// This is a bit vector, no arithmetic operators.
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
				bug("MpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value; break;
		case '-':
			if( !allowarith ) {
				bug("MpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value; break;
		case '*':
			if( !allowarith ) {
				bug("MpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value; break;
		case '/':
			if( !allowarith ) {
				bug("MpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("MpAlterRoom - alterroom called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("MpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("MpAlterRoom - alterroom called with operator % and value 0", 0);
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
				bug("MpAlterRoom - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("MpAlterRoom - adjust called with operator % and value 0", 0);
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
SCRIPT_CMD(do_mpdestroyroom)
{
	long vnum;
	unsigned long id1, id2;
	ROOM_INDEX_DATA *room;


	if(!info || !info->mob) return;

	info->mob->progs->lastreturn = 0;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	// It's a room, extract it directly
	if(arg->type == ENT_ROOM) {
		// Need to block this when done by room to itself
		if(extract_clone_room(arg->d.room->source,arg->d.room->id[0],arg->d.room->id[1],false))
			info->mob->progs->lastreturn = 1;
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
		info->mob->progs->lastreturn = 1;
}


// showroom <viewer> map <mapid> <x> <y> <z> <scale> <width> <height>[ force]
// showroom <viewer> room <room>[ force]
// showroom <viewer> vroom <room> <id>[ force]
SCRIPT_CMD(do_mpshowroom)
{
	CHAR_DATA *viewer = NULL, *next;
	ROOM_INDEX_DATA *room = NULL, *dest;
	WILDS_DATA *wilds = NULL;

	long mapid;
	long x,y;
	long width, height;
	bool force;

	if(!info || !info->mob) return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE:	viewer = arg->d.mob; break;
	case ENT_ROOM:		room = arg->d.room; break;
	}

	if(!viewer && !room) {
		bug("MpShowMap - bad target for showing the map", 0);
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

		//z = arg->d.num; TODO: fixme?

		if(!(argument = expand_argument(info,argument,arg)) || arg->type != ENT_NUMBER)
			return;

		//scale = arg->d.num; TODO: fixme?

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
					show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, FALSE);
				}
			}
		} else if(!IS_NPC(viewer)) {
			// There is no awake check here since it is to one mob.
			//  This can be used in things like DREAMS, seeing yourself at a certain location!

			show_map_to_char_wyx(wilds,x,y, viewer,x,y, width + viewer->wildview_bonus_x, height + viewer->wildview_bonus_y, FALSE);
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


// do_mpxcall
SCRIPT_CMD(do_mpxcall)
{
	char *rest; //buf[MSL], *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;
	CHAR_DATA *vch = NULL,*ch = NULL;
	OBJ_DATA *obj1 = NULL,*obj2 = NULL;
	SCRIPT_DATA *script;
	int depth, vnum, ret, space = PRG_MPROG;


	DBG2ENTRY2(PTR,info,PTR,argument);
	if(info->mob) {
		DBG3MSG2("info->mob = %s(%d)\n",HANDLE(info->mob),VNUM(info->mob));
	}

	if(!info || !info->mob) return;

	if (!argument[0]) {
		bug("MpCall: missing arguments from vnum %d.", VNUM(info->mob));
		return;
	}

	if(script_security < 5) {
		bug("MpCall: Minimum security needed is 5.", VNUM(info->mob));
		return;
	}


	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("MpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->mob));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
		bug("MpCall: No entity target from vnum %ld.", VNUM(info->mob));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(mob && !IS_NPC(mob)) {
		bug("MpCall: Invalid target for xcall.  Players cannot do scripts.", 0);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}


	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
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
		bug("MpCall: invalid prog from vnum %d.", VNUM(info->mob));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(info->mob, NULL, arg->d.str); break;
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
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(info->mob, NULL, arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(info->mob, NULL, arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpCall: Error in parsing from vnum %ld.", VNUM(info->mob));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(info->mob, NULL, arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	// Do this to account for possible destructions
	ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->mob) {
		info->mob->progs->lastreturn = ret;
		DBG3MSG1("lastreturn = %d\n", info->mob->progs->lastreturn);
	} else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}


// do_mpchargebank
// mob chargebank <player> <gold>
SCRIPT_CMD(do_mpchargebank)
{
	char *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpChargeBank - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("MpChargeBank - Non-player victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpChargeBank - Error in parsing from vnum %ld.", VNUM(info->mob));
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

// do_mpwiretransfer
// mob wiretransfer <player> <gold>
// Limited to 1000 gold for security scopes less than 7.
SCRIPT_CMD(do_mpwiretransfer)
{
	char buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(info->mob, NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("MpWireTransfer - Non-player victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("MpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->mob));
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
		sprintf(buf, "MpWireTransfer logged: attempted to wire %d gold to %s in room %ld by %ld", amount, victim->name, info->mob->in_room->vnum, info->mob->pIndexData->vnum);
		log_string(buf);
		amount = 1000;
	}

	victim->pcdata->bankbalance += amount;

	sprintf(buf, "MpWireTransfer logged: %s was wired %d gold in room %ld by %ld", victim->name, amount, info->mob->in_room->vnum, info->mob->pIndexData->vnum);
	log_string(buf);
}

// do_mpsetrecall
// mob setrecall $MOBILE <location>
// Sets the recall point of the target mobile to the reference of the location
SCRIPT_CMD(do_mpsetrecall)
{
	char /*buf[MSL], - Unused???*/ *rest;
	CHAR_DATA *victim;
	ROOM_INDEX_DATA *location;
//	int amount = 0; - Unused???


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpSetRecall - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("MpSetRecall - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	argument = mp_getlocation(info, rest, &location);

	if(!location) {
		bug("MpSetRecall - Bad location from vnum %d.", VNUM(info->mob));
		return;
	}

	if(location->wilds)
		location_set(&victim->recall,location->wilds->uid,location->x,location->y,location->z);
	else if(location->source)
		location_set(&victim->recall,0,location->vnum,0,0);
	else
		location_set(&victim->recall,0,location->vnum,location->id[0],location->id[1]);
}

// do_mpclearrecall
// mob clearrecall $MOBILE
// Clears the special recall field on the $MOBILE
SCRIPT_CMD(do_mpclearrecall)
{
	char /*buf[MSL], - Unused???*/ *rest;
	CHAR_DATA *victim;
//	ROOM_INDEX_DATA *location; - Unused???
//	int amount = 0; - Unused???


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpClearRecall - Bad syntax from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("MpClearRecall - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	location_clear(&victim->recall);
}


// HUNT[ <HUNTER>] <PREY>
SCRIPT_CMD(do_mphunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	CHAR_DATA *prey = NULL;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
	case ENT_MOBILE: prey = arg->d.mob; break;
	default: prey = NULL; break;
	}

	if (!prey) {
		bug("MpStartCombat - Null victim from vnum %ld.", VNUM(info->mob));
		return;
	}

	if(*rest) {
		if(!expand_argument(info,rest,arg)) {
			bug("MpHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
			return;
		}

		hunter = prey;

		switch(arg->type) {
		case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
		case ENT_MOBILE: prey = arg->d.mob; break;
		default: prey = NULL; break;
		}

		if (!prey) {
			bug("MpHunt - Null victim from vnum %ld.", VNUM(info->mob));
			return;
		}
	} else
		hunter = info->mob;

	hunt_char(hunter, prey);
	return;
}

// STOPHUNT <STAY>[ <HUNTER>]
SCRIPT_CMD(do_mpstophunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	bool stay;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING) {
		bug("MpStopHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

	if(!expand_argument(info,rest,arg)) {
		bug("MpStopHunt - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_NONE: hunter = info->mob; break;
	case ENT_STRING: hunter = get_char_world(info->mob, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("MpStopHunt - Null hunter from vnum %ld.", VNUM(info->mob));
		return;
	}

	stop_hunt(hunter, stay);
	return;
}

// Format: PERSIST <MOBILE or OBJECT or ROOM> <STATE>
SCRIPT_CMD(do_mppersist)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	bool persist = FALSE, current = FALSE;


	if(!info || !info->mob) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpPersist - Error in parsing from vnum %ld.", VNUM(info->mob));
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; current = mob->persist; break;
	case ENT_OBJECT: obj = arg->d.obj; current = obj->persist; break;
	case ENT_ROOM: room = arg->d.room; current = room->persist; break;
	}

	if(!mob && !obj && !room) {
		bug("MpPersist - NULL target.", VNUM(info->mob));
		return;
	}

	if(mob)
	{
		if( !IS_NPC(mob) ) return;

		if( IS_SET(mob->act2, ACT2_INSTANCE_MOB) ) return;
	}

	if(obj)
	{
		if( IS_SET(obj->extra3_flags, ITEM_INSTANCE_OBJ) ) return;
	}

	if( room )
	{
		if( get_blueprint_section_byroom(room->vnum) ) return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpPersist - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:   persist = !current; break;
	case ENT_STRING: persist = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"on"); break;
	default: return;
	}

	// Require security to ENABLE persistance
	if(!current && persist && script_security < MAX_SCRIPT_SECURITY) {
		bug("MpPersist - Insufficient security to enable persistance.", VNUM(info->mob));
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

/*
// mob skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_mpskill)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int sn, value;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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
*/

// mob skillgroup <player> add|remove <group>
SCRIPT_CMD(do_mpskillgroup)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int gn;
	bool fAdd = FALSE;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	if ( script_security < 9 ) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob || IS_NPC(mob) ) return;	// only players for now

	if( !*rest) return;

	argument = one_argument(rest,buf);

	if( !str_cmp(buf, "add") )
		fAdd = TRUE;
	else if(!str_cmp(buf, "remove"))
		fAdd = FALSE;
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

// mob condition $PLAYER <condition> <value>
// Adjusts the specified condition by the given value
SCRIPT_CMD(do_mpcondition)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int cond, value;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_mpaddspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_new;
	OBJ_DATA *target;
	int level;
	int sn;
	AFFECT_DATA *paf;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_mpremspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_prev;
	OBJ_DATA *target;
	int level;
	int sn;
	bool found = FALSE, show = TRUE;
	AFFECT_DATA *paf;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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
			show = FALSE;
	}


	found = FALSE;
	spell_prev = NULL;
	for(spell = target->spells; spell; spell_prev = spell, spell = spell->next) {
		if( spell->sn == sn ) {
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

			found = FALSE;
			level = 0;


			// If there's another obj with the same spell put that one on
			for (obj_tmp = target->carried_by->carrying; obj_tmp; obj_tmp = obj_tmp->next_content)
			{
				if( obj_tmp->wear_loc != WEAR_NONE && target != obj_tmp ) {
					for (spell = obj_tmp->spells; spell != NULL; spell = spell ->next) {
						if (spell->sn == sn && spell->level > level ) {
							level = spell->level;	// Keep the maximum
							found_loc = obj_tmp->wear_loc;
							found = TRUE;
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
SCRIPT_CMD(do_mpalteraffect)
{
	char buf[MIL],field[MIL],*rest;

	AFFECT_DATA *paf;
	int value;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_AFFECT || !arg->d.aff) return;

	paf = arg->d.aff;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAlterAffect - Error in parsing.",0);
		return;
	}

	if( IS_NULLSTR(rest) ) return;

	if( arg->type != ENT_STRING || IS_NULLSTR(arg->d.str) ) return;

	strncpy(field,arg->d.str,MIL-1);


	if( !str_cmp(field, "level") ) {
		argument = one_argument(rest,buf);

		if(!(rest = expand_argument(info,argument,arg))) {
			bug("MpAlterAffect - Error in parsing.",0);
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
			bug("MpAlterAffect - Error in parsing.",0);
			return;
		}

		if( paf->slot != WEAR_NONE ) {
			bug("MpAlterAffect - Attempting to modify duration of an object given affect.",0);
			return;
		}

		if( paf->group == AFFGROUP_RACIAL ) {
			bug("MpAlterAffect - Attempting to modify duration of a racial affect.",0);
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
SCRIPT_CMD(do_mpcrier)
{
	if(!info || !info->mob) return;

	BUFFER *buffer = new_buf();
	add_buf(buffer, "{M");
	expand_string(info,argument,buffer);

	if(!buf_string(buffer)[2]) {
		free_buf(buffer);
		return;
	}

	add_buf(buffer, "{x");

	crier_announce(buf_string(buffer));
	free_buf(buffer);
}



// scriptwait $PLAYER NUMBER VNUM VNUM[ $ACTOR]
// - actor can be a $MOBILE, $OBJECT or $TOKEN
// - scripts must be available for the respective actor type
SCRIPT_CMD(do_mpscriptwait)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int wait;
	long success, failure, pulse;
	TOKEN_DATA *actor_token = NULL;
	CHAR_DATA *actor_mob = NULL;
	OBJ_DATA *actor_obj = NULL;
	int prog_type;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	info->mob->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE) return;

	mob = arg->d.mob;

	if( !mob ) return;

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

	actor_mob = info->mob;
	prog_type = PRG_MPROG;
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

	//printf_to_char(mob, "script_wait started: %d\n\r", wait);

	// Return how long the command decided
	info->mob->progs->lastreturn = wait;
}

// Syntax: saveplayer $PLAYER
SCRIPT_CMD(do_mpsaveplayer)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	info->mob->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob)) return;

	save_char_obj(mob);

	info->mob->progs->lastreturn = 1;

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

// Syntax:	checkpoint $PLAYER $ROOM
// 			checkpoint $PLAYER VNUM
//			checkpoint $PLAYER none|clear|reset
//
// Sets the checkpoint of the $PLAYER to the destination or clears it.
// - When a checkpoint is set, it will override what location is saved to the pfile.
// - When setting a checkpoint via VNUM and an invalid vnum is given, it will clear the checkpoint.
SCRIPT_CMD(do_mpcheckpoint)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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

// Syntax: FIXAFFECTS $MOBILE
SCRIPT_CMD(do_mpfixaffects)
{


	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	if(!expand_argument(info,argument,arg))
		return;

	if(arg->type != ENT_MOBILE) return;

	if(arg->d.mob == NULL) return;

	affect_fix_char(arg->d.mob);
}

// Syntax: remort $PLAYER
//  - prompts them for a class out of what they can do
SCRIPT_CMD(do_mpremort)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	info->mob->progs->lastreturn = 0;

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

    mob->remort_question = TRUE;
    show_multiclass_choices(mob, mob);

	info->mob->progs->lastreturn = 1;
}


// Syntax: restore $MOBILE
SCRIPT_CMD(do_mprestore)
{
	char *rest;

	int amount = 100;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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

// GROUP npc(FOLLOWER)[ mobile(LEADER=self)][ bool(SHOW=true)]
// Follower will only work on an NPC
// LASTRETURN:
// 0 = grouping failed
// 1 = grouping succeeded
SCRIPT_CMD(do_mpgroup)
{
	char *rest;

	CHAR_DATA *follower, *leader;
	bool fShow = TRUE;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

	info->mob->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob || !IS_NPC(arg->d.mob)) return;

	follower = arg->d.mob;
	leader = info->mob;

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
		else if( arg->type == ENT_MOBILE && arg->d.mob )
		{
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
		}
		else
			return;
	}

	if(add_grouped(follower, leader, fShow))
		info->mob->progs->lastreturn = 1;
}

// UNGROUP mobile[ bool(ALL=false)]
SCRIPT_CMD(do_mpungroup)
{
	char *rest;

	bool fAll = FALSE;

	if(!info || !info->mob || IS_NULLSTR(argument)) return;

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

