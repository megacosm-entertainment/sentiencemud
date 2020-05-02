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
#include "debug.h"

// Commands used by token scripts
const struct script_cmd_type token_cmd_table[] = {
	{ "addaffect",			scriptcmd_addaffect,		TRUE,	TRUE	},
	{ "addaffectname",		scriptcmd_addaffectname,	TRUE,	TRUE	},
	{ "addspell",			do_tpaddspell,				TRUE,	TRUE	},
	{ "adjust",				do_tpadjust,				FALSE,	TRUE	},
	{ "alteraffect",		do_tpalteraffect,			TRUE,	TRUE	},
	{ "alterexit",			do_tpalterexit,				FALSE,	TRUE	},
	{ "altermob",			do_tpaltermob,				TRUE,	TRUE	},
	{ "alterobj",			do_tpalterobj,				TRUE,	TRUE	},
	{ "alterroom",			do_tpalterroom,				TRUE,	TRUE	},
	{ "applytoxin",			scriptcmd_applytoxin,		FALSE,	TRUE	},
	{ "asound",				do_tpasound,				FALSE,	TRUE	},
	{ "attach",				scriptcmd_attach,			TRUE,	TRUE	},
	{ "award",				scriptcmd_award,			TRUE,	TRUE	},
	{ "breathe",			scriptcmd_breathe,		FALSE,	TRUE	},
	{ "call",				do_tpcall,					FALSE,	TRUE	},
	{ "castfailure",		do_tpcastfailure,			FALSE,	TRUE	},
	{ "castrecover",		do_tpcastrecover,			FALSE,	TRUE	},
	{ "chargebank",			do_tpchargebank,			FALSE,	TRUE	},
	{ "checkpoint",			do_tpcheckpoint,			FALSE,	TRUE	},
	{ "cloneroom",			do_tpcloneroom,				TRUE,	TRUE	},
	{ "condition",			do_tpcondition,				FALSE,	TRUE	},
	{ "crier",				do_tpcrier,					FALSE,	TRUE	},
	{ "damage",				scriptcmd_damage,			FALSE,	TRUE	},
	{ "deduct",				scriptcmd_deduct,			TRUE,	TRUE	},
	{ "dequeue",			do_tpdequeue,				FALSE,	FALSE	},
	{ "destroyroom",		do_tpdestroyroom,			TRUE,	TRUE	},
	{ "detach",				scriptcmd_detach,			TRUE,	TRUE	},
	{ "dungeoncomplete",	scriptcmd_dungeoncomplete,	TRUE,	TRUE	},
	{ "echo",				do_tpecho,					FALSE,	TRUE	},
	{ "echoaround",			do_tpechoaround,			FALSE,	TRUE	},
	{ "echoat",				do_tpechoat,				FALSE,	TRUE	},
	{ "echobattlespam",		do_tpechobattlespam,		FALSE,	TRUE	},
	{ "echochurch",			do_tpechochurch,			FALSE,	TRUE	},
	{ "echogrouparound",	do_tpechogrouparound,		FALSE,	TRUE	},
	{ "echogroupat",		do_tpechogroupat,			FALSE,	TRUE	},
	{ "echoleadaround",		do_tpecholeadaround,		FALSE,	TRUE	},
	{ "echoleadat",			do_tpecholeadat,			FALSE,	TRUE	},
	{ "echonotvict",		do_tpechonotvict,			FALSE,	TRUE	},
	{ "echoroom",			do_tpechoroom,				FALSE,	TRUE	},
	{ "ed",					scriptcmd_ed,				FALSE,	TRUE	},
	{ "entercombat",		scriptcmd_entercombat,		FALSE,	TRUE	},
	{ "fade",				scriptcmd_fade,				TRUE,	TRUE	},
	{ "fixaffects",			do_tpfixaffects,			FALSE,	TRUE	},
	{ "flee",				scriptcmd_flee,				FALSE,	TRUE	},
	{ "force",				do_tpforce,					FALSE,	TRUE	},
	{ "forget",				do_tpforget,				FALSE,	FALSE	},
	{ "gdamage",			do_tpgdamage,				FALSE,	TRUE	},
	{ "gecho",      	 	do_tpgecho,					FALSE,	TRUE	},
	{ "gforce",				do_tpgforce,				FALSE,	TRUE	},
	{ "give",				do_tpgive,					FALSE,	TRUE	},
	{ "goto",				do_tpgoto,					FALSE,	TRUE	},
	{ "grantskill",			scriptcmd_grantskill,		FALSE,	TRUE	},
	{ "group",				do_tpgroup,					FALSE,	TRUE	},
	{ "gtransfer",			do_tpgtransfer,				FALSE,	TRUE	},
	{ "input",				do_tpinput,					FALSE,	TRUE	},
	{ "inputstring",		scriptcmd_inputstring,		FALSE,	TRUE	},
	{ "instancecomplete",	scriptcmd_instancecomplete,	TRUE,	TRUE	},
	{ "interrupt",			do_tpinterrupt,				FALSE,	TRUE	},
	{ "junk",				do_tpjunk,					FALSE,	TRUE	},
	{ "link",				do_tplink,					FALSE,	TRUE	},
	{ "mload",				do_tpmload,					FALSE,	TRUE	},
	{ "oload",				do_tpoload,					FALSE,	TRUE	},
	{ "otransfer",			do_tpotransfer,				FALSE,	TRUE	},
	{ "pageat",				scriptcmd_pageat,			FALSE,	TRUE	},
	{ "peace",				do_tppeace,					FALSE,	FALSE	},
	{ "persist",			do_tppersist,				FALSE,	TRUE	},
	{ "prompt",				do_tpprompt,				FALSE,	TRUE	},
	{ "purge",				do_tppurge,					FALSE,	FALSE	},
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
	{ "queue",				do_tpqueue,					FALSE,	TRUE	},
	{ "raisedead",			do_tpraisedead,				TRUE,	TRUE	},
	{ "rawkill",			do_tprawkill,				FALSE,	TRUE	},
	{ "remember",			do_tpremember,				FALSE,	TRUE	},
	{ "remort",				do_tpremort,				TRUE,	TRUE	},
	{ "remove",				do_tpremove,				FALSE,	TRUE	},
	{ "remspell",			do_tpremspell,				TRUE,	TRUE	},
	{ "resetdice",			do_tpresetdice,				TRUE,	TRUE	},
	{ "restore",			do_tprestore,				TRUE,	TRUE	},
	{ "revokeskill",		scriptcmd_revokeskill,		FALSE,	TRUE	},
	{ "saveplayer",			do_tpsaveplayer,			FALSE,	TRUE	},
	{ "scriptwait",			do_tpscriptwait,			TRUE,	TRUE	},
	{ "setrecall",			do_tpsetrecall,				FALSE,	TRUE	},
	{ "settimer",			do_tpsettimer,				FALSE,	TRUE	},
	{ "showroom",			do_tpshowroom,				TRUE,	TRUE	},
	{ "skimprove",			do_tpskimprove,				TRUE,	TRUE	},
	{ "startcombat",		scriptcmd_startcombat,		FALSE,	TRUE	},
	{ "stopcombat",			scriptcmd_stopcombat,		FALSE,	TRUE	},
	{ "stringmob",			do_tpstringmob,				TRUE,	TRUE	},
	{ "stringobj",			do_tpstringobj,				TRUE,	TRUE	},
	{ "stripaffect",		do_tpstripaffect,			TRUE,	TRUE	},
	{ "stripaffectname",	do_tpstripaffectname,		TRUE,	TRUE	},
	{ "transfer",			do_tptransfer,				FALSE,	TRUE	},
	{ "ungroup",			do_tpungroup,				FALSE,	TRUE	},
	{ "usecatalyst",		do_tpusecatalyst,			FALSE,	TRUE	},
	{ "varclear",			do_tpvarclear,				FALSE,	TRUE	},
	{ "varclearon",			do_tpvarclearon,			FALSE,	TRUE	},
	{ "varcopy",			do_tpvarcopy,				FALSE,	TRUE	},
	{ "varsave",			do_tpvarsave,				FALSE,	TRUE	},
	{ "varsaveon",			do_tpvarsaveon,				FALSE,	TRUE	},
	{ "varset",				do_tpvarset,				FALSE,	TRUE	},
	{ "varseton",			do_tpvarseton,				FALSE,	TRUE	},
	{ "vforce",				do_tpvforce,				FALSE,	TRUE	},
	{ "wiretransfer",		do_tpwiretransfer,			FALSE,	TRUE	},
	{ "xcall",				do_tpxcall,					FALSE,	TRUE	},
	{ "zecho",				do_tpzecho,					FALSE,	TRUE	},
	{ "zot",				do_tpzot,					TRUE,	TRUE	},
	{ NULL,					NULL,						FALSE,	FALSE	}
};

// Commands accessible by other scripts
const struct script_cmd_type tokenother_cmd_table[] = {
	{ "adjust",		do_tpadjust,	FALSE,	TRUE	},
	{ "give",		do_tpgive,		FALSE,	TRUE	},
	{ "junk",		do_tpjunk,		FALSE,	TRUE	},
	{ NULL,			NULL,			FALSE,	FALSE	}
};


int tpcmd_lookup(char *command,bool istoken)
{
	int cmd;

	if(istoken) {
		for (cmd = 0; token_cmd_table[cmd].name; cmd++)
			if (command[0] == token_cmd_table[cmd].name[0] &&
				!str_prefix(command, token_cmd_table[cmd].name))
				return cmd;
	} else {
		for (cmd = 0; tokenother_cmd_table[cmd].name; cmd++)
			if (command[0] == tokenother_cmd_table[cmd].name[0] &&
				!str_prefix(command, tokenother_cmd_table[cmd].name))
				return cmd;
	}

	return -1;
}

/*
 * Displays the source code of a given TOKENprogram
 *
 * Syntax: tpdump [vnum]
 */
void do_tpdump(CHAR_DATA *ch, char *argument)
{
	char buf[ MAX_INPUT_LENGTH ];
	SCRIPT_DATA *tprg;

	one_argument(argument, buf);
	if (!(tprg = get_script_index(atoi(buf), PRG_TPROG))) {
		send_to_char("No such TOKENprogram.\n\r", ch);
		return;
	}
	if (!area_has_read_access(ch,tprg->area)) {
		send_to_char("You do not have permission to view that script.\n\r", ch);
		return;
	}
	page_to_char(tprg->edit_src, ch);
}


/*
 * Displays TOKENprogram triggers of a token
 *
 * Syntax: tpstat [victim] [vnum] [index]
 */
void do_tpstat(CHAR_DATA *ch, char *argument)
{
	char arg[MSL], arg2[MSL], arg3[MSL];
	TOKEN_DATA *token = NULL;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *object = NULL;
	ROOM_INDEX_DATA *room = NULL;
	ITERATOR it;
	PROG_LIST *tprg;
	int i, slot, count;
	long vnum = 0;

	argument = one_argument(argument, arg);
	argument = one_argument(argument, arg2);

	if (arg[0] == '\0') {
		send_to_char("Syntax:  tpstat <mobile name|object name|room> [<count>.]<token vnum>\n\r", ch);
		return;
	}

	if (!str_cmp(arg,"mob")) {
		if ((victim = get_char_world(NULL, arg2)) == NULL) {
			send_to_char("Mobile not found.\n\r", ch);
			return;
		}

		count = number_argument(argument, arg3);
	} else if(!str_cmp(arg, "obj")) {
		if ((object = get_obj_world(NULL, arg2)) == NULL) {
			send_to_char("Object not found.\n\r", ch);
			return;
		}

		count = number_argument(argument, arg3);
	} else if(!str_cmp(arg, "room")) {
		room = ch->in_room;
		count = number_argument(arg2, arg3);
	} else {
		send_to_char("Syntax:  tpstat <mobile name|object name|room> [<count>.]<token vnum>\n\r", ch);
		return;
	}

	if (arg3[0] != '\0') {
		vnum = atol(arg3);

		if (get_token_index(vnum) == NULL) {
			send_to_char("That token vnum does not exist.\n\r", ch);
			return;
		}

		if (victim && !(token = get_token_char(victim, vnum, count))) {
			act("$N doesn't have that token.", ch, victim, NULL, NULL, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if (object && !(token = get_token_obj(object, vnum, count))) {
			act("$p doesn't have that token.", ch, NULL, NULL, object, NULL, NULL, NULL, TO_CHAR);
			return;
		}

		if (room && !(token = get_token_room(room, vnum, count))) {
			send_to_char("The room doesn't have that token.", ch);
			return;
		}

	}

	if(!token) {
		send_to_char("Token not found.\n\r", ch);
		return;
	}
	sprintf(arg, "Token #%-6ld [%s] ID [%08X:%08X]\n\r", token->pIndexData->vnum, token->pIndexData->name, (int)token->id[0], (int)token->id[1]);
	send_to_char(arg, ch);

	if( !IS_NULLSTR(token->pIndexData->comments) )
	{
		sprintf(arg, "Comments:\n\r%s\n\r", token->pIndexData->comments);
		send_to_char(arg, ch);
	}

	sprintf(arg, "Delay   %-6d [%s]\n\r",
		token->progs->delay,
		token->progs->target ? token->progs->target->name : "No target");

	send_to_char(arg, ch);

	if (!token->pIndexData->progs)
		send_to_char("[No programs set]\n\r", ch);
	else
	for(i = 0, slot = 0; slot < TRIGSLOT_MAX; slot++) {
		iterator_start(&it, token->pIndexData->progs[slot]);
		while(( tprg = (PROG_LIST *)iterator_nextdata(&it))) {
			sprintf(arg, "[%2d] Trigger [%-8s] Program [%4ld] Phrase [%s]\n\r",
				++i, trigger_name(tprg->trig_type),
				tprg->vnum,
				trigger_phrase(tprg->trig_type,tprg->trig_phrase));
			send_to_char(arg, ch);
		}
		iterator_stop(&it);
	}

	if(token->progs->vars) {
		pVARIABLE var;

		for(var = token->progs->vars; var; var = var->next) {
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

char *tp_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
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
		case ENT_NONE:	*room = token_room(info->token); break;
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
				*room = get_exit_dest(token_room(info->token), arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				// Rather self-explanatory
				*room = token_room(info->token);
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
			*room = ( arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] ) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}

	free_script_param(arg);
	return rest;
}


char *tp_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
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
		case ENT_NONE:	*room = token_room(info->token); break;
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
				*room = get_exit_dest(token_room(info->token), arg->d.str+1);
			else if(!str_cmp(arg->d.str,"here"))
				*room = token_room(info->token);
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
			*room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
		case ENT_TOKEN:
			*room = token_room(arg->d.token); break;
		}
	}

	free_script_param(arg);
	return rest;
}



void token_interpret(SCRIPT_VARINFO *info, char *argument)
{
	char buf[MAX_STRING_LENGTH], command[MAX_INPUT_LENGTH];
	int cmd;

	if(!info->token) return;

	if (!str_prefix("token ", argument))
		argument = skip_whitespace(argument+5);

	argument = one_argument(argument, command);

	cmd = tpcmd_lookup(command,TRUE);

	if(cmd < 0) {
		sprintf(buf, "Token_interpret: invalid cmd from token %ld: '%s'", info->token->pIndexData->vnum, command);
		bug(buf, 0);
		return;
	}

	SCRIPT_PARAM *arg = new_script_param();
	(*token_cmd_table[cmd].func) (info, argument, arg);
	free_script_param(arg);
	tail_chain();
}

void tokenother_interpret(SCRIPT_VARINFO *info, char *argument)
{
	char buf[MAX_STRING_LENGTH], command[MAX_INPUT_LENGTH];
	int cmd;

	if(info->token) return;
	if(!info->mob && !info->obj && !info->room) return;

	if (!str_prefix("token ", argument))
		argument = skip_whitespace(argument+5);

	argument = one_argument(argument, command);

	cmd = tpcmd_lookup(command,FALSE);

	if(cmd < 0) {
		sprintf(buf, "Tokenother_interpret: invalid cmd: '%s'", command);
		bug(buf, 0);
		return;
	}

	SCRIPT_PARAM *arg = new_script_param();
	(*tokenother_cmd_table[cmd].func) (info, argument, arg);
	free_script_param(arg);
	tail_chain();
}

SCRIPT_CMD(do_tpadjust)
{
	char buf[MSL],*rest, arg2[MIL];
	int vnum = 0, num = -1, value = 0, count = 0;
	int *ptr = NULL;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *object = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;


	if(!info) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAdjust - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!str_cmp(arg->d.str,"self")) {
			if (info->mob) victim = info->mob;
			else if (info->obj) object = info->obj;
			else if (info->room) room = info->room;
		} else	// Strings lock onto mobiles
			victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	case ENT_OBJECT:
		object = arg->d.obj;
		break;
	case ENT_ROOM:
		room = arg->d.room;
		break;
	case ENT_TOKEN:
		token = arg->d.token;
		break;
	default: break;
	}

	if(!token) {
		if(!victim && !object && !room) {
			bug("TpAdjust - NULL victim.", 0);
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpAdjust - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			count = number_argument(arg->d.str, arg2);
			vnum = is_number(arg2) ? atoi(arg2) : 0;
			break;
		case ENT_NUMBER: vnum = arg->d.num; count = 1; break;
		default: break;
		}

		if (vnum < 1 || !get_token_index(vnum)) {
			bug("TpAdjust - invalid token vnum.", 0);
			return;
		}

		if(victim)
			token = get_token_char(victim, vnum, count);
		else if(object)
			token = get_token_obj(object, vnum, count);
		else if(room)
			token = get_token_room(room, vnum, count);

		if (!token) return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAdjust - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(is_number(arg->d.str))					num = atoi(arg->d.str);
		else if(!str_cmp(arg->d.str,"tempstore1"))	ptr = &token->tempstore[0];
		else if(!str_cmp(arg->d.str,"tempstore2"))	ptr = &token->tempstore[1];
		else if(!str_cmp(arg->d.str,"tempstore3"))	ptr = &token->tempstore[2];
		else if(!str_cmp(arg->d.str,"tempstore4"))	ptr = &token->tempstore[3];
		else if(!str_cmp(arg->d.str,"timer"))		ptr = &token->timer;

		break;
	case ENT_NUMBER: num = arg->d.num; break;
	default: break;
	}

	if ((num < 0 || num >= MAX_TOKEN_VALUES) && !ptr) {
		sprintf(buf, "TpAdjust: bad v#");
		bug(buf, 0);
		return;
	}


	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAdjust - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(is_number(arg->d.str))
			value = atoi(arg->d.str);
		else {
			bug("TpAdjust - Invalid value.",0);
			return;
		}
		break;
	case ENT_NUMBER: value = arg->d.num; break;
	default:
		bug("TpAdjust - Invalid value.",0);
		return;
	}

	if(ptr) {
		switch (buf[0]) {
		case '+': *ptr += value; break;
		case '-': *ptr -= value; break;
		case '*': *ptr *= value; break;
		case '/':
			if (!value) {
				bug("TpAdjust - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if (!value) {
				bug("TpAdjust - adjust called with operator % and value 0", 0);
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
		case '+': token->value[num] += value; break;
		case '-': token->value[num] -= value; break;
		case '*': token->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("TpAdjust - adjust called with operator / and value 0", 0);
				return;
			}
			token->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("TpAdjust - adjust called with operator % and value 0", 0);
				return;
			}
			token->value[num] %= value;
			break;

		case '=': token->value[num] = value; break;
		case '&': token->value[num] &= value; break;
		case '|': token->value[num] |= value; break;
		case '!': token->value[num] &= ~value; break;
		case '^': token->value[num] ^= value; break;
		default:
			return;
		}
	}
}

// do_tpcall
SCRIPT_CMD(do_tpcall)
{
	char *rest;
	CHAR_DATA *vch, *ch;
	OBJ_DATA *obj1, *obj2;
	SCRIPT_DATA *script;
	int depth, vnum;

	int ret;

	DBG2ENTRY2(PTR,info,PTR,argument);
	if(info->token) {
		DBG3MSG2("info->token = %s(%d)\n",info->token->name,VNUM(info->token));
	}

	if(!info || !info->token) return;

	if (!argument[0]) {
		bug("TpCall: missing arguments from vnum %d.", VNUM(info->token));
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("TpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->token));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(script = get_script_index(vnum, PRG_TPROG))) {
		bug("TpCall: invalid prog from vnum %d.", VNUM(info->token));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, token_room(info->token), arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, token_room(info->token),arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, token_room(info->token), arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, token_room(info->token), arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	ret = execute_script(script->vnum, script, NULL, NULL, NULL, info->token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->token) {
		info->token->progs->lastreturn = ret;
		DBG3MSG1("lastreturn = %d\n", info->token->progs->lastreturn);
	} else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

// do_tpecho
SCRIPT_CMD(do_tpecho)
{
	if(!info || !info->token) return;

	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		add_buf(buffer,"\n\r");
		room_echo(token_room(info->token), buffer->string);
	}
	free_buf(buffer);
}

// do_tpechoroom
// Syntax: token echoroom <location> <string>
SCRIPT_CMD(do_tpechoroom)
{
	char *rest;
	ROOM_INDEX_DATA *room;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE: room = arg->d.mob->in_room; break;
	case ENT_OBJECT: room = obj_room(arg->d.obj); break;
	case ENT_ROOM: room = arg->d.room; break;
	case ENT_EXIT: room = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door]) ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
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

// do_tpechoaround
SCRIPT_CMD(do_tpechoaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpechonotvict
SCRIPT_CMD(do_tpechonotvict)
{
	char *rest;
	CHAR_DATA *victim, *attacker;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, token_room(info->token),arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

SCRIPT_CMD(do_tpechobattlespam)
{
	char *rest;
	CHAR_DATA *victim, *attacker, *ch;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: attacker = get_char_room(NULL, token_room(info->token),arg->d.str); break;
	case ENT_MOBILE: attacker = arg->d.mob; break;
	default: attacker = NULL; break;
	}

	if (!attacker || !attacker->in_room)
		return;

	if(!(rest = expand_argument(info,rest,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpechoat
SCRIPT_CMD(do_tpechoat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpechochurch
SCRIPT_CMD(do_tpechochurch)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpechogrouparound
SCRIPT_CMD(do_tpechogrouparound)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpechogroupat
SCRIPT_CMD(do_tpechogroupat)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpecholeadaround
SCRIPT_CMD(do_tpecholeadaround)
{
	char *rest;
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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

// do_tpecholeadat
SCRIPT_CMD(do_tpecholeadat)
{
	char *rest;
	CHAR_DATA *victim;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
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


SCRIPT_CMD(do_tpgive)
{
	char buf[MSL],*rest;
	int vnum = 0;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *object = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_INDEX_DATA *token_index;
	TOKEN_DATA *token;


	if(!info) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGive - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!str_cmp(arg->d.str, "self")) {
			victim = info->mob;
			object = info->obj;
			room = info->room;
		} else
			victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	case ENT_OBJECT:
		object = arg->d.obj;
		break;
	case ENT_ROOM:
		room = arg->d.room;
		break;
	default: victim = NULL; object = NULL; room = NULL; break;
	}

	if(!victim && !object && !room) {
		bug("TpGive - NULL target.", 0);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGive - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: break;
	}

	if (vnum < 1 || !(token_index = get_token_index(vnum))) {
		bug("TpGive - invalid token vnum.", 0);
		return;
	}

	if (is_singular_token(token_index)) {
		if (victim && get_token_char(victim, vnum, 1)) {
			sprintf(buf, "TpGive - trying to give a second copy of token %s (%ld) to char %s",
				token_index->name, token_index->vnum, HANDLE(victim));
			bug(buf, 0);
			return;
		} else if (object && get_token_obj(object, vnum, 1)) {
			sprintf(buf, "TpGive - trying to give a second copy of token %s (%ld) to object %s",
				token_index->name, token_index->vnum, object->short_descr);
			bug(buf, 0);
			return;
		} else if (room && get_token_room(room, vnum, 1)) {
			sprintf(buf, "TpGive - trying to give a second copy of token %s (%ld) to room %s",
				token_index->name, token_index->vnum, room->name);
			bug(buf, 0);
			return;
		}
	}

	token = give_token(token_index, victim, object, room);

	if( token ) {
		if( rest && *rest ) variables_set_token(info->var,rest,token);

		p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_GIVEN, NULL);
	}
}

// token junk <token reference>[ <exit code>]
//  exit code is only used on self-junk
SCRIPT_CMD(do_tpjunk)
{
	char *rest;
	char arg2[MIL];
	int vnum = 0/*, ret = 1*/;
	int count = 1;
	CHAR_DATA *victim = NULL;
	OBJ_DATA *object = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;


	if(!info) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpJunk - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if (!str_cmp(arg->d.str,"self")) {
			if (info->mob) victim = info->mob;
			else if (info->obj) object = info->obj;
			else if (info->room) room = info->room;
		} else	// Strings lock onto mobiles
			victim = get_char_world(info->mob, arg->d.str);
		break;
	case ENT_MOBILE:
		victim = arg->d.mob;
		break;
	case ENT_OBJECT:
		object = arg->d.obj;
		break;
	case ENT_ROOM:
		room = arg->d.room;
		break;
	case ENT_TOKEN:
		token = arg->d.token;
		break;
	default: break;
	}

	if(!token) {
		if(!victim && !object && !room) {
			bug("TpJunk - NULL target.", 0);
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpJunk - Error in parsing.",0);
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			count = number_argument(arg->d.str, arg2);
			vnum = is_number(arg2) ? atoi(arg2) : 0;
			break;

		case ENT_NUMBER: vnum = arg->d.num; count = 1; break;
		default: break;
		}

		if(victim)
			token = get_token_char(victim, vnum, count);
		else if(object)
			token = get_token_obj(object, vnum, count);
		else if(room)
			token = get_token_room(room, vnum, count);

		if (!token) return;
	}

	if( token && IS_SET(token->flags, TOKEN_PERMANENT) && script_security < SYSTEM_SCRIPT_SECURITY) {
		bug("TpJunk - Attempting to junk a permanent token with insufficient security.",0);
		return;
	}

	p_percent_trigger(NULL, NULL, NULL, token, NULL, NULL, NULL, NULL, NULL, TRIG_TOKEN_REMOVED, NULL);

	if(info->token && token == info->token) {
		arg->type = ENT_NONE;
		expand_argument(info,rest,arg);
		switch(arg->type) {
		case ENT_STRING: info->block->ret_val = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
		case ENT_NUMBER: info->block->ret_val = arg->d.num; break;
		default: break;
		}
	}

	if(token->player)
		token_from_char(token);
	else if(token->object)
		token_from_obj(token);
	else if(token->room)
		token_from_room(token);
	free_token(token);
}

SCRIPT_CMD(do_tpvarset)
{
	if(!info || !info->token || !info->var) return;

	script_varseton(info, info->var, argument, arg);
}

SCRIPT_CMD(do_tpvarclear)
{
	if(!info || !info->token || !info->var) return;

	script_varclearon(info, info->var, argument, arg);
}

SCRIPT_CMD(do_tpvarcopy)
{
	char oldname[MIL],newname[MIL];

	if(!info || !info->token || !info->var) return;

	// Get name
	argument = one_argument(argument,oldname);
	if(!oldname[0]) return;
	argument = one_argument(argument,newname);
	if(!newname[0]) return;

	if(!str_cmp(oldname,newname)) return;

	variable_copy(info->var,oldname,newname);
}

SCRIPT_CMD(do_tpvarsave)
{
	char name[MIL],arg1[MIL];
	bool on;

	if(!info || !info->token || !info->var) return;

	// Get name
	argument = one_argument(argument,name);
	if(!name[0]) return;
	argument = one_argument(argument,arg1);
	if(!arg1[0]) return;

	on = !str_cmp(arg1,"on") || !str_cmp(arg1,"true") || !str_cmp(arg1,"yes");

	variable_setsave(*info->var,name,on);
}

SCRIPT_CMD(do_tpsettimer)
{
	char buf[MIL],*rest;
	int amt;
	CHAR_DATA *victim = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSetTimer - Error in parsing.",0);
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
		bug("TpSetTimer - NULL victim.", 0);
		return;
	}

	if(!*rest) {
		bug("TpSetTimer - Missing timer type.",0);
		return;
	}

	buf[0] = 0;
	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSetTimer - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		strncpy(buf,arg->d.str,MIL);
		break;
	default: break;
	}

	if(!*rest) {
		bug("TpSetTimer - Missing timer amount.",0);
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSetTimer - Error in parsing.",0);
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

SCRIPT_CMD(do_tpinterrupt)
{
	char *rest;
	CHAR_DATA *victim = NULL;
	ROOM_INDEX_DATA *here;

	int stop, ret = 0;
	bool silent = FALSE;

	if(!info || !info->token) return;

	here = token_room(info->token);

	info->token->progs->lastreturn = 0;	// Nothing was interrupted

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpInterrupt - Error in parsing.",0);
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
		bug("TpInterrupt - NULL victim.", 0);
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);
	if( buffer->string[0] != '\0' ) {
		stop = flag_value(interrupt_action_types,buffer->string);
		if(stop == NO_FLAG) {
			bug("TpInterrupt - invalid interrupt type.", 0);
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
	info->token->progs->lastreturn = ret;
	free_buf(buffer);
}

SCRIPT_CMD(do_tpalterobj)
{
	char buf[2*MIL],field[MIL],*rest;
	int value, num, min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;

	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("TpAlterObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("TpAlterObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAlterObj - Error in parsing.",0);
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
		bug("TpAlterObj - Error in parsing.",0);
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
			sprintf(buf,"TpAlterObj - Attempting to alter value%d with security %d.\n\r", num, script_security);
			bug(buf, 0);
			return;
		}

		switch (buf[0]) {
		case '+': obj->value[num] += value; break;
		case '-': obj->value[num] -= value; break;
		case '*': obj->value[num] *= value; break;
		case '/':
			if (!value) {
				bug("TpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			obj->value[num] /= value;
			break;
		case '%':
			if (!value) {
				bug("TpAlterObj - adjust called with operator % and value 0", 0);
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
			sprintf(buf,"TpAlterObj - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
				bug("TpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value;
			break;

		case '-':
			if( !allowarith ) {
				bug("TpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value;
			break;

		case '*':
			if( !allowarith ) {
				bug("TpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value;
			break;

		case '/':
			if( !allowarith ) {
				bug("TpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("TpAlterObj - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("TpAlterObj - alterobj called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("TpAlterObj - adjust called with operator % and value 0", 0);
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



SCRIPT_CMD(do_tpresetdice)
{
	char *rest;
	OBJ_DATA *obj = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("TpAlterObj - NULL object.", 0);
		return;
	}

	if(obj->item_type == ITEM_WEAPON)
		set_weapon_dice_obj(obj);
}



// do_tpdamage
SCRIPT_CMD(do_tpdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *victim_next;
	int low, high, level, value, dc;
	bool fAll = FALSE, fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = get_char_room(NULL, token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim && !fAll) {
		bug("TpDamage - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!*rest) {
		bug("TpDamage - missing argument from vnum %ld.", VNUM(info->token));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
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
		bug("TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!*rest) {
		bug("TpDamage - missing argument from vnum %ld.", VNUM(info->token));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpDamage - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	if(fLevel && !victim) {
		bug("TpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->token));
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
			bug("TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("TpDamage - Null reference mob from vnum %ld.", VNUM(info->token));
				return;
			}
			break;
		} else {
			bug("TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
			return;
		}
		break;
	default:
		bug("TpDamage - invalid argument from vnum %ld.", VNUM(info->token));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = TRUE;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	if (fAll) {
		for(victim = token_room(info->token)->people; victim; victim = victim_next) {
			victim_next = victim->next_in_room;
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
		}
	} else {
		value = fLevel ? dice(low,high) : number_range(low,high);
		damage(victim, victim, fKill ? value : UMIN(victim->hit,value), TYPE_UNDEFINED, dc, FALSE);
	}
}


// do_tpraisedead
SCRIPT_CMD(do_tpraisedead)
{
	char buf[MIL], *rest;
	CHAR_DATA *victim;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token),arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if(!victim) return;

	if (!IS_DEAD(victim)) {
		sprintf(buf, "TpRaisedead: for token %s(%ld), victim %s wasn't dead!",
			info->token->name,VNUM(info->token),
			victim->name);
		bug(buf, 0);

		info->token->progs->lastreturn = 0;

//		send_to_char("{WAn intense warmth washes over you momentarily.{x\n\r", victim);
		return;
	}

	resurrect_pc(victim);
	info->token->progs->lastreturn = 1;
}

SCRIPT_CMD(do_tpdequeue)
{
	if(!info || !info->token || !info->token->events)
		return;

	wipe_owned_events(info->token->events);
}

SCRIPT_CMD(do_tpqueue)
{
	char *rest;
	int delay;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: delay = arg->d.num; break;
	case ENT_STRING: delay = atoi(arg->d.str); break;
	default:
		bug("TpQueue:  missing arguments from vnum %d.", VNUM(info->token));
		return;
	}

	if (delay < 0 || delay > 1000) {
		bug("TpQueue:  unreasonable delay recieved from vnum %d.", VNUM(info->token));
		return;
	}

	wait_function(info->token, info, EVENT_TOKENQUEUE, delay, script_interpret, rest);
}

SCRIPT_CMD(do_tpgdamage)
{
	char buf[MSL],*rest;
	CHAR_DATA *victim = NULL, *rch, *rch_next;
	int low, high, level, value, dc;
	bool fKill = FALSE, fLevel = FALSE, fRemort = FALSE, fTwo = FALSE;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGdamage - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("TpGdamage - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!*rest) {
		bug("TpGdamage - missing argument from vnum %ld.", VNUM(info->token));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGdamage - Error in parsing from vnum %ld.", VNUM(info->token));
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
		bug("TpGdamage - invalid argument from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!*rest) {
		bug("TpGdamage - missing argument from vnum %ld.", VNUM(info->token));
		return;
	}

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGdamage - Error in parsing from vnum %ld.", VNUM(info->token));
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
			bug("TpGdamage - invalid argument from vnum %ld.", VNUM(info->token));
			return;
		}
		break;
	case ENT_MOBILE:
		if(fLevel) {
			if(arg->d.mob) level = arg->d.mob->tot_level;
			else {
				bug("TpGdamage - Null reference mob from vnum %ld.", VNUM(info->token));
				return;
			}
			break;
		} else {
			bug("TpGdamage - invalid argument from vnum %ld.", VNUM(info->token));
			return;
		}
		break;
	default:
		bug("TpGdamage - invalid argument from vnum %ld.", VNUM(info->token));
		return;
	}

	// No expansion!
	argument = one_argument(rest, buf);
	if (!str_cmp(buf,"kill") || !str_cmp(buf,"lethal")) fKill = TRUE;

	one_argument(argument, buf);
	dc = damage_class_lookup(buf);

	if(fLevel) get_level_damage(level,&low,&high,fRemort,fTwo);

	for(rch = token_room(info->token)->people; rch; rch = rch_next) {
		rch_next = rch->next_in_room;
		if (rch != victim && is_same_group(victim,rch)) {
			value = fLevel ? dice(low,high) : number_range(low,high);
			damage(rch, rch, fKill ? value : UMIN(rch->hit,value), TYPE_UNDEFINED, dc, FALSE);
		}
	}
}

SCRIPT_CMD(do_tpasound)
{
	ROOM_INDEX_DATA *here, *room;
	ROOM_INDEX_DATA *rooms[MAX_DIR];
	int door, i, j;
	EXIT_DATA *pexit;

	if(!info || !info->token || !token_room(info->token)) return;
	if (!argument[0]) return;

	here = token_room(info->token);

	// Verify there are any exits!
	for (door = 0; door < MAX_DIR; door++)
		if ((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here)
			break;

	if (door < MAX_DIR) {
		// Expand the message
		BUFFER *buffer = new_buf();
		expand_string(info,argument,buffer);

		if( buffer->string[0] != '\0' )
		{
			for (i = 0; door < MAX_DIR; door++)
				if ((pexit = here->exit[door]) && (room = exit_destination(pexit)) && room != here) {
					// Have we been to this room already?
					for(j=0;j < i && rooms[j] != room; j++);

					if(i <= j) {
						// No, so do the message
						MOBtrigger  = FALSE;
						act(buffer->string, room->people, NULL, NULL, NULL, NULL, NULL, NULL, TO_ALL);
						MOBtrigger  = TRUE;
						rooms[i++] = room;
					}
				}
		}

		free_buf(buffer);
	}
}

SCRIPT_CMD(do_tpforget)
{
	if(!info || !info->token) return;

	info->token->progs->target = NULL;
}

SCRIPT_CMD(do_tpremember)
{
	CHAR_DATA *victim;

	if(!info || !info->token) return;

	if(!expand_argument(info,argument,arg)) {
		bug("TpRemember: Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("TpRemember: Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	info->token->progs->target = victim;
}

SCRIPT_CMD(do_tppurge)
{
	char *rest;
	CHAR_DATA **mobs = NULL, *victim = NULL,*vnext;
	OBJ_DATA **objs = NULL, *obj = NULL,*obj_next;
	ROOM_INDEX_DATA *here = NULL;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NONE: here = token_room(info->token); break;
	case ENT_STRING:
		if (!(victim = get_char_room(NULL, token_room(info->token), arg->d.str)))
			obj = get_obj_here(NULL, token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	case ENT_ROOM: here = arg->d.room; break;
	case ENT_EXIT: here = arg->d.door.r ? exit_destination(arg->d.door.r->exit[arg->d.door.door]) : NULL; break;
	case ENT_OLLIST_MOB: mobs = arg->d.list.ptr.mob; break;
	case ENT_OLLIST_OBJ: objs = arg->d.list.ptr.obj; break;
	default: break;
	}

	if(victim) {
		if (!IS_NPC(victim)) {
			bug("Oppurge - Attempting to purge a PC from vnum %d.", VNUM(info->token));
			return;
		}
		extract_char(victim, TRUE);
	} else if(obj)
		extract_obj(obj);
	else if(here) {
		for (victim = here->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (IS_NPC(victim) && !IS_SET(victim->act, ACT_NOPURGE))
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
			if (IS_NPC(victim) && !IS_SET(victim->act, ACT_NOPURGE))
				extract_char(victim, TRUE);
		}
	} else if(objs) {
		for (obj = *objs; obj; obj = obj_next) {
			obj_next = obj->next_content;
			if (!IS_SET(obj->extra_flags, ITEM_NOPURGE))
				extract_obj(obj);
		}
	} else
		bug("Oppurge - Bad argument from vnum %d.", VNUM(info->token));

}


SCRIPT_CMD(do_tpzot)
{
	CHAR_DATA *victim;


	if(!info || !info->token) return;

	if(!expand_argument(info,argument,arg))
		return;

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("TpZot - Null victim from vnum %ld.", VNUM(info->token));
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

SCRIPT_CMD(do_tpgecho)
{
	DESCRIPTOR_DATA *d;

	if(!info || !info->token) return;

	if (!argument[0]) {
		bug("TpZEcho: missing argument from vnum %d", VNUM(info->token));
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
					send_to_char("Token echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_tpzecho)
{
	AREA_DATA *area;
	DESCRIPTOR_DATA *d;

	if(!info || !info->token || !token_room(info->token)) return;

	// Expand the message
	BUFFER *buffer = new_buf();
	expand_string(info,argument,buffer);

	if( buffer->string[0] != '\0' )
	{
		area = token_room(info->token)->area;

		for (d = descriptor_list; d; d = d->next)
			if (d->connected == CON_PLAYING &&
				d->character->in_room &&
				d->character->in_room->area == area) {
				if (IS_IMMORTAL(d->character))
					send_to_char("Token echo> ", d->character);
				send_to_char(buffer->string, d->character);
				send_to_char("\n\r", d->character);
			}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_tpvforce)
{
	char *rest;
	int vnum = 0;
	CHAR_DATA *vch, *next;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpVforce - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: vnum = atoi(arg->d.str); break;
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: break;
	}

	if (vnum < 1) {
		bug("TpVforce - Invalid vnum from vnum %ld.", VNUM(info->token));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (vch = token_room(info->token)->people; vch; vch = next) {
			next = vch->next_in_room;
			if (IS_NPC(vch) &&  vch->pIndexData->vnum == vnum && !vch->fighting)
				interpret(vch, buffer->string);
		}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_tpotransfer)
{
	char *rest;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *dest;
	OBJ_DATA *container;
	CHAR_DATA *carrier;
	int wear_loc = WEAR_NONE;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpOtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: obj = get_obj_here(NULL,token_room(info->token), arg->d.str); break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: obj = NULL; break;
	}


	if (!obj) {
		bug("TpOtransfer - Null object from vnum %ld.", VNUM(info->token));
		return;
	}

	if (PROG_FLAG(obj,PROG_AT)) return;

	if (IS_SET(obj->extra3_flags, ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

	argument = tp_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

	if(!dest && !container && !carrier) {
		bug("TpOTransfer - Bad location from vnum %d.", VNUM(info->token));
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

// do_oppeace
SCRIPT_CMD(do_tppeace)
{
	CHAR_DATA *rch;
	if(!info || !info->token || !token_room(info->token)) return;

	for (rch = token_room(info->token)->people; rch; rch = rch->next_in_room) {
		if (rch->fighting)
			stop_fighting(rch, TRUE);
		if (IS_NPC(rch) && IS_SET(rch->act,ACT_AGGRESSIVE))
			REMOVE_BIT(rch->act,ACT_AGGRESSIVE);
	}
}


SCRIPT_CMD(do_tptransfer)
{
	char buf[MIL], *rest;
	CHAR_DATA *victim = NULL,*vnext;
	ROOM_INDEX_DATA *dest;
	bool all = FALSE, force = FALSE, quiet = FALSE;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpTransfer - Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) all = TRUE;
		else victim = get_char_world(NULL, arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim && !all) {
		bug("TpTransfer - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	// Crashing on transfer all as this is not set on 'all' transfers.
	//if (!victim->in_room) return;

	argument = tp_getlocation(info, rest, &dest);

	if(!dest) {
		bug("TpTransfer - Bad location from vnum %d.", VNUM(info->token));
		return;
	}

	argument = one_argument(argument,buf);
	force = !str_cmp(buf,"force") || !str_cmp(argument,"force");
	quiet = !str_cmp(buf,"quiet") || !str_cmp(argument,"quiet");

	if (all) {
		for (victim = token_room(info->token)->people; victim; victim = vnext) {
			vnext = victim->next_in_room;
			if (!IS_NPC(victim)) {
				if (!force && room_is_private(dest, NULL)) break;
				do_mob_transfer(victim,dest,quiet);
			}
		}
		return;
	}

	if (!force && room_is_private(dest, NULL))
		return;

	do_mob_transfer(victim,dest,quiet);
}

SCRIPT_CMD(do_tpremove)
{
	CHAR_DATA *victim;
	OBJ_DATA *obj = NULL, *obj_next;
	int vnum = 0, count = 0;
	bool fAll = FALSE;

	char name[MIL], *rest;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token), arg->d.str);
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim) {
		bug("TpRemove: Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!*rest) return;

	argument = rest;
	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
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
		bug ("TpRemove: Invalid object from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!fAll && !obj && *rest) {
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpRemove: Bad syntax from vnum %ld.", VNUM(info->token));
			return;
		}

		switch(arg->type) {
		case ENT_NUMBER: count = arg->d.num; break;
		case ENT_STRING: count = atoi(arg->d.str); break;
		default: count = 0; break;
		}

		if(count < 0) {
			bug ("TpRemove: Invalid count from vnum %d.", VNUM(info->token));
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

SCRIPT_CMD(do_tpgtransfer)
{
	char buf[MIL], buf2[MIL], *rest;
	CHAR_DATA *victim, *vch,*next;
	ROOM_INDEX_DATA *dest;
	bool all = FALSE, force = FALSE, quiet = FALSE;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}


	if (!victim) {
		bug("TpGtransfer - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if (!victim->in_room) return;

	if(!(argument = tp_getlocation(info, rest, &dest))) {
		bug("TpGtransfer - Bad syntax from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!dest) {
		bug("TpGtransfer - Bad location from vnum %d.", VNUM(info->token));
		return;
	}

	argument = one_argument(argument,buf);
	argument = one_argument(argument,buf2);
	all = !str_cmp(buf,"all") || !str_cmp(buf2,"all") || !str_cmp(argument,"all");
	force = !str_cmp(buf,"force") || !str_cmp(buf2,"force") || !str_cmp(argument,"force");
	quiet = !str_cmp(buf,"quiet") || !str_cmp(buf2,"quiet") || !str_cmp(argument,"quiet");

	for (vch = victim->in_room->people; vch; vch = next) {
		next = vch->next_in_room;
		if (!IS_NPC(vch) && is_same_group(victim,vch)) {
			if (!all && vch->position != POS_STANDING) continue;
			if (!force && room_is_private(dest, NULL)) break;
			do_mob_transfer(vch,dest,quiet);
		}
	}
}



SCRIPT_CMD(do_tplink)
{
	char *rest;
	ROOM_INDEX_DATA *room, *dest;
	int door, vnum;
	unsigned long id1, id2;

	bool del = FALSE;
	bool environ = FALSE;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_STRING:
		room = token_room(info->token);
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
		bug("TPlink used without an argument from room vnum %d.", room->vnum);
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
		// Only allow STATIC links
		vnum = (arg->d.door.r && arg->d.door.r->exit[arg->d.door.door] && arg->d.door.r->exit[arg->d.door.door]->u1.to_room) ? arg->d.door.r->exit[arg->d.door.door]->u1.to_room->vnum : -1;
		break;
	case ENT_MOBILE:
		vnum = (arg->d.mob && arg->d.mob->in_room) ? arg->d.mob->in_room->vnum : -1;
		break;
	case ENT_OBJECT:
		vnum = (arg->d.obj && obj_room(arg->d.obj)) ? obj_room(arg->d.obj)->vnum : -1;
		break;
	}

	if(vnum < 0) {
		bug("TPlink - invalid argument in room %d.", room->vnum);
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
		bug("TPlink - invalid destination in room %d.", room->vnum);
		return;
	}

	script_change_exit(room, dest, door);
}

// do_opmload
SCRIPT_CMD(do_tpmload)
{
	char buf[MIL], *rest;
	long vnum;
	MOB_INDEX_DATA *pMobIndex;
	CHAR_DATA *victim;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_MOBILE: vnum = arg->d.mob ? arg->d.mob->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (vnum < 1 || !(pMobIndex = get_mob_index(vnum))) {
		sprintf(buf, "Tpmload: bad mob index (%ld) from token %ld", vnum, VNUM(info->token));
		bug(buf, 0);
		return;
	}

	victim = create_mobile(pMobIndex, FALSE);
	char_to_room(victim, token_room(info->token));
	if(rest && *rest) variables_set_mobile(info->var,rest,victim);
	p_percent_trigger(victim, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

SCRIPT_CMD(do_tpoload)
{
	char buf[MIL], *rest;
	long vnum, level;
	bool fWear = FALSE;
	OBJ_INDEX_DATA *pObjIndex;
	OBJ_DATA *obj;

	CHAR_DATA *to_mob = NULL;
	OBJ_DATA *to_obj = NULL;
	ROOM_INDEX_DATA *to_room = NULL;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	case ENT_STRING: vnum = arg->d.str ? atoi(arg->d.str) : 0; break;
	case ENT_OBJECT: vnum = arg->d.obj ? arg->d.obj->pIndexData->vnum : 0; break;
	default: vnum = 0; break;
	}

	if (!vnum || !(pObjIndex = get_obj_index(vnum))) {
		bug("Tpoload - Bad vnum arg from vnum %d.", VNUM(info->token));
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

			case ENT_MOBILE:	// $MOBILE [wear/carry]
				to_mob = arg->d.mob;
				if((rest = one_argument(rest,buf))) {
					if (!str_cmp(buf, "wear"))
						fWear = TRUE;
					// use "none/carry"
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
		level = pObjIndex->level;

	obj = create_object(pObjIndex, level, TRUE);
	if(to_room)
		obj_to_room(obj, to_room);
	else if( to_obj )
		obj_to_obj(obj, to_obj);
	else if( to_mob && CAN_WEAR(obj, ITEM_TAKE) &&
		(to_mob->carry_number < can_carry_n (to_mob)) &&
		(get_carry_weight (to_mob) + get_obj_weight (obj) <= can_carry_w (to_mob))) {
		obj_to_char(obj, to_mob);
		if (fWear)
			wear_obj(to_mob, obj, TRUE);
	} else
		obj_to_room(obj, token_room(info->token));

	if(rest && *rest) variables_set_object(info->var,rest,obj);
	p_percent_trigger(NULL, obj, NULL, NULL, NULL, NULL, NULL, NULL, NULL, TRIG_REPOP, NULL);
}

SCRIPT_CMD(do_tpforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *next;
	bool fAll = FALSE, forced;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpForce - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		if(!str_cmp(arg->d.str,"all")) fAll = TRUE;
		else victim = get_char_room(NULL,token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!fAll && !victim) {
		bug("TpForce - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		forced = forced_command;

		if (fAll) {
			for (victim = token_room(info->token)->people; victim; victim = next) {
				next = victim->next_in_room;
				forced_command = TRUE;
				interpret(victim, buffer->string);
			}
		} else {
			forced_command = TRUE;
			interpret(victim, buffer->string);
		}

		forced_command = forced;
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_tpgforce)
{
	char *rest;
	CHAR_DATA *victim = NULL, *vch, *next;


	if(!info || !info->token || !token_room(info->token)) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpGforce - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL, token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: break;
	}

	if (!victim) {
		bug("TpGforce - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	BUFFER *buffer = new_buf();
	expand_string(info,rest,buffer);

	if( buffer->string[0] != '\0' )
	{
		for (vch = token_room(info->token)->people; vch; vch = next) {
			next = vch->next_in_room;
			if (is_same_group(victim,vch))
				interpret(vch, buffer->string);
		}
	}

	free_buf(buffer);
}

SCRIPT_CMD(do_tpgoto)
{
	ROOM_INDEX_DATA *dest;

	if(!info || !info->token || !info->token->player || !info->token->player->in_room) return;

	if(!argument[0]) {
		bug("Tpgoto - No argument from vnum %d.", VNUM(info->token));
		return;
	}

	tp_getlocation(info, argument, &dest);

	if(!dest) {
		bug("Tpgoto - Bad location from vnum %d.", VNUM(info->token));
		return;
	}

	if (info->token->player->fighting) stop_fighting(info->token->player, TRUE);

	char_from_room(info->token->player);
	char_to_room(info->token->player, dest);
}


SCRIPT_CMD(do_tpstringobj)
{
	char buf[MSL],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	OBJ_DATA *obj = NULL;

	bool newlines = FALSE;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpStringObj - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		obj = get_obj_here(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_OBJECT:
		obj = arg->d.obj;
		break;
	default: break;
	}

	if(!obj) {
		bug("TpStringObj - NULL object.", 0);
		return;
	}

	if(!*rest) {
		bug("TpStringObj - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpStringObj - Error in parsing.",0);
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

	if( buffer->string[0] != '\0' )
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
			newlines = TRUE;
		} else if(!str_cmp(field,"material")) {
			int mat = material_lookup(buf);

			if(mat < 0) {
				char buf2[sizeof(buf)+50];
				sprintf(buf2,"TpStringObj - Invalid material '%s'.\n\r", buf);
				bug(buf2, 0);
				return;
			}

			// Force material to the full name
			strcpy(buf,material_table[mat].name);

			str = (char**)&obj->material;
		} else {
			free_buf(buffer);
			return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"TpStringObj - Attempting to restring '%s' with security %d.\n\r", field, script_security);
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

SCRIPT_CMD(do_tpaltermob)
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
	int dirty_stat = -1;
	const struct flag_type *flags = NULL;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("TpAlterMob - NULL mobile.", 0);
		return;
	}

	if(!*rest) {
		bug("TpAlterMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAlterMob - Error in parsing.",0);
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
		bug("TpAlterMob - Error in parsing.",0);
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
		sprintf(buf,"TpAlterMob - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
			bug("TpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr += value; break;
	case '-':
		if( !allowarith ) {
			bug("TpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr -= value; break;
	case '*':
		if( !allowarith ) {
			bug("TpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		*ptr *= value; break;
	case '/':
		if( !allowarith ) {
			bug("TpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("TpAlterMob - altermob called with operator / and value 0", 0);
			return;
		}
		*ptr /= value;
		break;
	case '%':
		if( !allowarith ) {
			bug("TpAlterMob - altermob called with arithmetic operator on a non-arithmetic field.", 0);
			return;
		}

		if (!value) {
			bug("TpAlterMob - altermob called with operator % and value 0", 0);
			return;
		}
		*ptr %= value;
		break;

	case '=':
		*ptr = value;
		break;

	case '&':
		if( !allowbitwise ) {
			bug("TpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr &= value;
		break;
	case '|':
		if( !allowbitwise ) {
			bug("TpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr |= value;
		break;
	case '!':
		if( !allowbitwise ) {
			bug("TpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
			return;
		}

		*ptr &= ~value;
		break;
	case '^':
		if( !allowbitwise ) {
			bug("TpAlterMob - altermob called with bitwise operator on a non-bitvector field.", 0);
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


SCRIPT_CMD(do_tpstringmob)
{
	char buf[MSL+2],field[MIL],*rest, **str;
	int min_sec = MIN_SCRIPT_SECURITY;
	CHAR_DATA *mob = NULL;

	bool newlines = FALSE;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpStringMob - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("TpStringMob - NULL mobile.", 0);
		return;
	}

	if(!IS_NPC(mob)) {
		bug("TpStringMob - can't change strings on PCs.", 0);
		return;
	}

	if(!*rest) {
		bug("TpStringMob - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpStringMob - Error in parsing.",0);
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
		else if(!str_cmp(field,"long"))			{ str = (char**)&mob->long_descr; strcat(buf,"\n\r"); newlines = TRUE; }
		else if(!str_cmp(field,"full"))			{ str = (char**)&mob->description; newlines = TRUE; }
		else if(!str_cmp(field,"tempstring"))	str = (char**)&mob->tempstring;
		else
		{
			free_buf(buffer);
			return;
		}

		if(script_security < min_sec) {
			sprintf(buf,"TpStringMob - Attempting to restring '%s' with security %d.\n\r", field, script_security);
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

SCRIPT_CMD(do_tpskimprove)
{
	char skill[MIL],*rest;
	int min_diff, diff, sn=-1;
	CHAR_DATA *mob = NULL;

	TOKEN_DATA *token = NULL;
	bool success = FALSE;

	if(script_security < MIN_SCRIPT_SECURITY) {
		bug("TpSkImprove - Insufficient security.",0);
		return;
	}

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	case ENT_TOKEN:
		token = arg->d.token;
	default: break;
	}

	if(!mob && !token) {
		bug("TpSkImprove - NULL target.", 0);
		return;
	}

	if(mob) {
		if(IS_NPC(mob)) {
			bug("TpSkImprove - NPCs don't have skills to improve yet...", 0);
			return;
		}


		if(!(rest = expand_argument(info,rest,arg))) {
			bug("TpSkImprove - Error in parsing.",0);
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
			bug("TpSkImprove - Token is not a spell token...", 0);
			return;
		}
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpSkImprove - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
	case ENT_NUMBER: diff = arg->d.num; break;
	default: return;
	}

	min_diff = 10 - script_security;	// min=10, max=1

	if(diff < min_diff) {
		bug("TpSkImprove - Attempting to use a difficulty multiplier lower than allowed.",0);
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

SCRIPT_CMD(do_tprawkill)
{
	char *rest;
	int type;
	bool has_head, show_msg;
	CHAR_DATA *mob = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("TpRawkill - NULL mobile.", 0);
		return;
	}

	if(IS_IMMORTAL(mob)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpRawkill - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
	default: return;
	}

	if(type < 0 || type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpRawkill - Error in parsing.",0);
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
		bug("TpRawkill - Error in parsing.",0);
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

SCRIPT_CMD(do_tpaddaffect)
{
	char *rest;
	int where, group, level, loc, mod, hours;
	int skill;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffect <target> <where> <skill> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, token_room(info->token), arg->d.str)))
			obj = get_obj_here(NULL, token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("TpAddaffect - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
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
		bug("TpAddaffect - Error in parsing.",0);
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
		bug("TpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
	default: return;
	}

	if(loc == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: mod = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: hours = arg->d.num; break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
	default: return;
	}

	if(bv == NO_FLAG) bv = 0;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAddaffect - Error in parsing.",0);
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

	memset(&af,0,sizeof(af));
	af.group	= group;
	af.where     = where;
	af.type      = skill;
	af.location  = loc;
	af.modifier  = mod;
	af.level     = level;
	af.duration  = (hours < 0) ? -1 : hours;
	af.bitvector = bv;
	af.bitvector2 = bv2;
	af.slot = wear_loc;

	if(mob) affect_join(mob, &af);
	else affect_join_obj(obj,&af);
}

SCRIPT_CMD(do_tpaddaffectname)
{
	char *rest, *name = NULL;
	int where, group, level, loc, mod, hours;
	long bv, bv2;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	int wear_loc = WEAR_NONE;

	AFFECT_DATA af;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpAddAffect - Error in parsing.",0);
		return;
	}

	// addaffectname <target> <where> <name> <level> <location> <modifier> <duration> <bitvector> <bitvector2>

	// <target>
	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,token_room(info->token), arg->d.str)))
			obj = get_obj_here(NULL,token_room(info->token), arg->d.str);
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

	// <where>
	switch(arg->type) {
	case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
	default: return;
	}

	if(where == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("MpAddaffect - Error in parsing.",0);
		return;
	}

	// <group>
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


	// <name>
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

	// <level>
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

	// <location>
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


SCRIPT_CMD(do_tpstripaffect)
{
	char *rest;
	int skill;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffect <target> <skill>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL, token_room(info->token), arg->d.str)))
			obj = get_obj_here(NULL, token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_OBJECT: obj = arg->d.obj; break;
	default: break;
	}

	if(!mob && !obj) {
		bug("TpStripaffect - NULL target.", 0);
		return;
	}


	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpStripaffect - Error in parsing.",0);
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

SCRIPT_CMD(do_tpstripaffectname)
{
	char *rest, *name;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("MpStripaffect - Error in parsing.",0);
		return;
	}

	// stripaffectname <target> <name>

	switch(arg->type) {
	case ENT_STRING:
		if (!(mob = get_char_room(NULL,token_room(info->token), arg->d.str)))
			obj = get_obj_here(NULL,token_room(info->token), arg->d.str);
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


SCRIPT_CMD(do_tpinput)
{
	char *rest, *p;
	int vnum;
	CHAR_DATA *mob = NULL;


	if(!info || !info->token) return;

	info->token->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token),arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("TpInput - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

	if( mob->desc->showstr_head != NULL ) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpInput - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: vnum = arg->d.num; break;
	default: return;
	}

	if(vnum < 1 || !get_script_index(vnum, PRG_TPROG)) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpInput - Error in parsing.",0);
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
	mob->desc->input_mob = NULL;
	mob->desc->input_obj = NULL;
	mob->desc->input_room = NULL;
	mob->desc->input_tok = info->token;

	info->token->progs->lastreturn = 1;
	free_buf(buffer);
}

SCRIPT_CMD(do_tpusecatalyst)
{
	char *rest;
	int type, method, amount, min, max, show;
	CHAR_DATA *mob = NULL;
	ROOM_INDEX_DATA *room = NULL;


	if(!info || !info->token) return;

	info->token->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	// usecatalyst <target> <type> <method> <amount> <min> <max> <show>

	switch(arg->type) {
	case ENT_STRING: mob = get_char_room(NULL,token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: mob = arg->d.mob; break;
	case ENT_ROOM: room = arg->d.room; break;
	default: break;
	}

	if(!mob && !room) {
		bug("TpUseCatalyst - NULL target.", 0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
	default: return;
	}

	if(type == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
	default: return;
	}

	if(method == NO_FLAG) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: amount = arg->d.num; break;
	case ENT_STRING: amount = atoi(arg->d.str); break;
	default: return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: min = arg->d.num; break;
	case ENT_STRING: min = atoi(arg->d.str); break;
	default: return;
	}

	if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NUMBER: max = arg->d.num; break;
	case ENT_STRING: max = atoi(arg->d.str); break;
	default: return;
	}

	if(max < min || max > CATALYST_MAXSTRENGTH) return;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpUseCatalyst - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING: show = flag_value(boolean_types,arg->d.str); break;
	default: return;
	}

	if(show == NO_FLAG) return;

	info->token->progs->lastreturn = use_catalyst(mob,room,type,method,amount,min,max,(bool)show);
}

SCRIPT_CMD(do_tpalterexit)
{
	char buf[MSL+2],field[MIL],*rest;
	int value, min_sec = MIN_SCRIPT_SECURITY, door;
	ROOM_INDEX_DATA *room;
	EXIT_DATA *ex = NULL;

	int *ptr = NULL;
	sh_int *sptr = NULL;
	char **str;
	bool allowarith = TRUE;
	const struct flag_type *flags = NULL;

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterExit - Error in parsing.",0);
		return;
	}

	room = token_room(info->token);

	switch(arg->type) {
	case ENT_ROOM:
		room = arg->d.room;
		if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
			bug("TpAlterExit - Error in parsing.",0);
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
		bug("TpAlterExit - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAlterExit - Error in parsing.",0);
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
			bug("TpAlterExit - Error in parsing.",0);
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
		bug("TpAlterExit - Error in parsing.",0);
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
		sprintf(buf,"TpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
				bug("TpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr += value;
			break;
		case '-':
			if( !allowarith ) {
				bug("TpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr -= value;
			break;
		case '*':
			if( !allowarith ) {
				bug("TpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			*ptr *= value;
			break;
		case '/':
			if( !allowarith ) {
				bug("TpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("TpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("TpAlterExit - alterexit called with arithmetic operator on a bitonly field.", 0);
				return;
			}
			if (!value) {
				bug("TpAlterExit - adjust called with operator % and value 0", 0);
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
				bug("TpAlterExit - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("TpAlterExit - adjust called with operator % and value 0", 0);
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

// SYNTAX: token prompt <player> <name>[ <string>]
SCRIPT_CMD(do_tpprompt)
{
	char name[MIL],*rest;
	CHAR_DATA *mob = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpPrompt - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_STRING:
		mob = get_char_room(NULL,token_room(info->token), arg->d.str);
		break;
	case ENT_MOBILE:
		mob = arg->d.mob;
		break;
	default: break;
	}

	if(!mob) {
		bug("TpPrompt - NULL mobile.", 0);
		return;
	}

	if(IS_NPC(mob)) {
		bug("TpPrompt - cannot set prompt strings on NPCs.", 0);
		return;
	}

	if(!*rest) {
		bug("TpPrompt - Missing name type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpPrompt - Error in parsing.",0);
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


SCRIPT_CMD(do_tpvarseton)
{

	VARIABLE **vars;

	if(!info || !info->token) return;

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

SCRIPT_CMD(do_tpvarclearon)
{

	VARIABLE **vars;

	if(!info || !info->token) return;

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

	script_varclearon(info, vars, argument, arg);
}

SCRIPT_CMD(do_tpvarsaveon)
{
	char name[MIL],buf[MIL];
	bool on;

	VARIABLE *vars;

	if(!info || !info->token) return;

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
SCRIPT_CMD(do_tpcloneroom)
{
	char name[MIL];
	long vnum;
	CHAR_DATA *mob;
	OBJ_DATA *obj;
	ROOM_INDEX_DATA *source, *room, *clone;
	TOKEN_DATA *tok;
	bool no_env = FALSE;

	if(!info || !info->token) return;

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
SCRIPT_CMD(do_tpalterroom)
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

	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterRoom - Error in parsing.",0);
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
		bug("TpAlterRoom - Missing field type.",0);
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAlterRoom - Error in parsing.",0);
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
			bug("TpAlterRoom - Error in parsing.",0);
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
			sprintf(buf,"TpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
			wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
			bug(buf, 0);
			return;
		}

		BUFFER *buffer = new_buf();
		expand_string(info,rest,buffer);

		if( buffer->string[0] != '\0' || allow_empty )
		{
			free_string(*str);
			*str = str_dup(buffer->string);
		}

		free_buf(buffer);
		return;
	}

	argument = one_argument(rest,buf);

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpAlterRoom - Error in parsing.",0);
		return;
	}

	if(!str_cmp(field,"room"))			{ ptr = (int*)&room->room_flags; flags = room_flags; }
	else if(!str_cmp(field,"room2"))	{ ptr = (int*)&room->room2_flags; flags = room2_flags; }
	else if(!str_cmp(field,"light"))	ptr = (int*)&room->light;
	else if(!str_cmp(field,"sector"))	ptr = (int*)&room->sector_type;
	else if(!str_cmp(field,"heal"))		{ ptr = (int*)&room->heal_rate; min_sec = 9; }
	else if(!str_cmp(field,"mana"))		{ ptr = (int*)&room->mana_rate; min_sec = 9; }
	else if(!str_cmp(field,"move"))		{ ptr = (int*)&room->move_rate; min_sec = 1; }
	else if(!str_cmp(field,"mapx"))         { ptr = (int*)&room->x; min_sec = 5; }
	else if(!str_cmp(field,"mapy"))         { ptr = (int*)&room->y; min_sec = 5; }
	else if(!str_cmp(field,"tempstore1"))	ptr = (int*)&room->tempstore[0];
	else if(!str_cmp(field,"tempstore2"))	ptr = (int*)&room->tempstore[1];
	else if(!str_cmp(field,"tempstore3"))	ptr = (int*)&room->tempstore[2];
	else if(!str_cmp(field,"tempstore4"))	ptr = (int*)&room->tempstore[3];

	if(!ptr && !sptr) return;

	if(script_security < min_sec) {
		sprintf(buf,"TpAlterRoom - Attempting to alter '%s' with security %d.\n\r", field, script_security);
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
				bug("TpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr += value; break;
		case '-':
			if( !allowarith ) {
				bug("TpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr -= value; break;
		case '*':
			if( !allowarith ) {
				bug("TpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			*ptr *= value; break;
		case '/':
			if( !allowarith ) {
				bug("TpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("TpAlterRoom - alterroom called with operator / and value 0", 0);
				return;
			}
			*ptr /= value;
			break;
		case '%':
			if( !allowarith ) {
				bug("TpAlterRoom - alterroom called with arithmetic operator on a bitonly field.", 0);
				return;
			}

			if (!value) {
				bug("TpAlterRoom - alterroom called with operator % and value 0", 0);
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
				bug("TpAlterRoom - adjust called with operator / and value 0", 0);
				return;
			}
			*sptr /= value;
			break;
		case '%':
			if (!value) {
				bug("TpAlterRoom - adjust called with operator % and value 0", 0);
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

// destroyroom <vnum> <id1> <id2>
// destroyroom <room>
SCRIPT_CMD(do_tpdestroyroom)
{
	long vnum;
	unsigned long id1, id2;
	ROOM_INDEX_DATA *room;


	if(!info || !info->token) return;

	info->token->progs->lastreturn = 0;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	// It's a room, extract it directly
	if(arg->type == ENT_ROOM) {
		// Need to block this when done by room to itself
		if(extract_clone_room(arg->d.room->source,arg->d.room->id[0],arg->d.room->id[1],false))
			info->token->progs->lastreturn = 1;

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
		info->token->progs->lastreturn = 1;
}

// showroom <viewer> map <mapid> <x> <y> <z> <scale> <width> <height>[ force]
// showroom <viewer> room <room>[ force]
// showroom <viewer> vroom <room> <id>[ force]
SCRIPT_CMD(do_tpshowroom)
{
	CHAR_DATA *viewer = NULL, *next;
	ROOM_INDEX_DATA *room = NULL, *dest;
	WILDS_DATA *wilds = NULL;

	long mapid;
	long x,y;
	long width, height;
	bool force;

	if(!info || !info->token) return;

	if(!(argument = expand_argument(info,argument,arg)))
		return;

	switch(arg->type) {
	case ENT_MOBILE:	viewer = arg->d.mob; break;
	case ENT_ROOM:		room = arg->d.room; break;
	}

	if(!viewer && !room) {
		bug("TpShowMap - bad target for showing the map", 0);
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


// do_tpxcall
// xcall <entity> <vnum> <enactor> <victim> <obj1> <obj2>
//
// Requires a level 5 security to do this.
// This will perform the script call on that entity
SCRIPT_CMD(do_tpxcall)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	TOKEN_DATA *token = NULL;
	CHAR_DATA *vch, *ch;
	OBJ_DATA *obj1, *obj2;
	SCRIPT_DATA *script;
	int depth, vnum, space = PRG_MPROG;

	int ret;

	if(!info || !info->token) return;

	if (!argument[0]) {
		bug("TpCall: missing arguments from vnum %d.", VNUM(info->token));
		return;
	}

	if(script_security < 5) {
		bug("TpCall: Minimum security needed is 5.", VNUM(info->token));
		return;
	}

	// Call depth checking
	depth = script_call_depth;
	if(script_call_depth == 1) {
		bug("TpCall: maximum call depth exceeded for mob vnum %d.", VNUM(info->token));
		return;
	} else if(script_call_depth > 1)
		--script_call_depth;


	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
		bug("TpCall: No entity target from vnum %ld.", VNUM(info->token));
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(mob && !IS_NPC(mob)) {
		bug("TpCall: Invalid target for xcall.  Players cannot do scripts.", 0);
		// Restore the call depth to the previous value
		script_call_depth = depth;
		return;
	}

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
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
		bug("TpCall: invalid prog from vnum %d.", VNUM(info->token));
		return;
	}

	ch = vch = NULL;
	obj1 = obj2 = NULL;

	if(*rest) {	// Enactor
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: ch = get_char_room(NULL, token_room(info->token), arg->d.str); break;
		case ENT_MOBILE: ch = arg->d.mob; break;
		default: ch = NULL; break;
		}
	}

	if(ch && *rest) {	// Victim
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING: vch = get_char_room(NULL, token_room(info->token),arg->d.str); break;
		case ENT_MOBILE: vch = arg->d.mob; break;
		default: vch = NULL; break;
		}
	}

	if(*rest) {	// Obj 1
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj1 = get_obj_here(NULL, token_room(info->token), arg->d.str);
			break;
		case ENT_OBJECT: obj1 = arg->d.obj; break;
		default: obj1 = NULL; break;
		}
	}

	if(obj1 && *rest) {	// Obj 2
		argument = rest;
		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpCall: Error in parsing from vnum %ld.", VNUM(info->token));
			// Restore the call depth to the previous value
			script_call_depth = depth;
			return;
		}

		switch(arg->type) {
		case ENT_STRING:
			obj2 = get_obj_here(NULL, token_room(info->token), arg->d.str);
			break;
		case ENT_OBJECT: obj2 = arg->d.obj; break;
		default: obj2 = NULL; break;
		}
	}

	// The last return goes to THIS enactor not the one called for the script
	ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,0,0,0,0,0);
	if(info->token)
		info->token->progs->lastreturn = ret;
	else
		info->block->ret_val = ret;

	// restore the call depth to the previous value
	script_call_depth = depth;
}

// do_tpchargebank
// obj chargebank <player> <gold>
SCRIPT_CMD(do_tpchargebank)
{
	char *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpChargeBank - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("TpChargeBank - Non-player victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("TpChargeBank - Error in parsing from vnum %ld.", VNUM(info->token));
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

// do_tpwiretransfer
// obj wiretransfer <player> <gold>
// Limited to 1000 gold for security scopes less than 7.
SCRIPT_CMD(do_tpwiretransfer)
{
	char buf[MSL], *rest;
	CHAR_DATA *victim;
	int amount = 0;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: victim = get_char_room(NULL,token_room(info->token), arg->d.str); break;
	case ENT_MOBILE: victim = arg->d.mob; break;
	default: victim = NULL; break;
	}

	if (!victim || IS_NPC(victim)) {
		bug("TpWireTransfer - Non-player victim from vnum %ld.", VNUM(info->token));
		return;
	}

	if(!expand_argument(info,rest,arg)) {
		bug("TpWireTransfer - Error in parsing from vnum %ld.", VNUM(info->token));
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
		sprintf(buf, "TpWireTransfer logged: attempted to wire %d gold to %s by token %ld", amount, victim->name, info->token->pIndexData->vnum);
		log_string(buf);
		amount = 1000;
	}

	victim->pcdata->bankbalance += amount;

	sprintf(buf, "TpWireTransfer logged: %s was wired %d gold by token %ld", victim->name, amount, info->token->pIndexData->vnum);
	log_string(buf);
}

// do_tpsetrecall
// obj setrecall $MOBILE <location>
// Sets the recall point of the target mobile to the reference of the location
SCRIPT_CMD(do_tpsetrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSetRecall - Bad syntax from vnum %ld.", VNUM(info->token));
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
		bug("TpSetRecall - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	argument = tp_getlocation(info, rest, &location);

	if(!location) {
		bug("TpSetRecall - Bad location from vnum %d.", VNUM(info->token));
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

// do_tpclearrecall
// obj clearrecall $MOBILE
// Clears the special recall field on the $MOBILE
SCRIPT_CMD(do_tpclearrecall)
{
	char /*buf[MSL],*/ *rest;
	CHAR_DATA *victim;
//	ROOM_INDEX_DATA *location;
//	int amount = 0;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpClearRecall - Bad syntax from vnum %ld.", VNUM(info->token));
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
		bug("TpClearRecall - Null victim from vnum %ld.", VNUM(info->token));
		return;
	}

	victim->recall.wuid = 0;
	victim->recall.id[0] = 0;
	victim->recall.id[1] = 0;
	victim->recall.id[2] = 0;
}

// HUNT[ <HUNTER>] <PREY>
SCRIPT_CMD(do_tphunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	CHAR_DATA *prey = NULL;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpHunt - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_STRING: prey = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: prey = arg->d.mob; break;
	default: prey = NULL; break;
	}

	if (!prey) {
		bug("TpHunt - Null hunter/prey from vnum %ld.", VNUM(info->token));
		return;
	}

	if(*rest) {
		if(!expand_argument(info,rest,arg)) {
			bug("TpHunt - Error in parsing from vnum %ld.", VNUM(info->token));
			return;
		}

		hunter = prey;

		switch(arg->type) {
		case ENT_STRING: prey = get_char_world(info->mob, arg->d.str); break;
		case ENT_MOBILE: prey = arg->d.mob; break;
		default: prey = NULL; break;
		}

		if (!prey) {
			bug("TpHunt - Null prey from vnum %ld.", VNUM(info->token));
			return;
		}
	} else if(!info->token->player) {
		bug("TpHunt - Null hunter from vnum %ld.", VNUM(info->token));
		return;
	} else
		hunter = info->token->player;

	hunt_char(hunter, prey);
	return;
}

// STOPHUNT <STAY>[ <HUNTER>]
SCRIPT_CMD(do_tpstophunt)
{
	char *rest;
	CHAR_DATA *hunter = NULL;
	bool stay;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg)) || arg->type != ENT_STRING) {
		bug("TpStopHunt - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

	if(!expand_argument(info,rest,arg)) {
		bug("TpStopHunt - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_NONE: hunter = info->token->player; break;
	case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
	case ENT_MOBILE: hunter = arg->d.mob; break;
	default: hunter = NULL; break;
	}

	if (!hunter) {
		bug("TpStopHunt - Null hunter from vnum %ld.", VNUM(info->token));
		return;
	}

	stop_hunt(hunter, stay);
	return;
}

// Format: PERSIST <MOBILE or OBJECT or ROOM> <STATE>
SCRIPT_CMD(do_tppersist)
{
	char *rest;
	CHAR_DATA *mob = NULL;
	OBJ_DATA *obj = NULL;
	ROOM_INDEX_DATA *room = NULL;
	bool persist = FALSE, current = FALSE;


	if(!info || !info->token) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpPersist - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

	switch(arg->type) {
	case ENT_MOBILE: mob = arg->d.mob; current = mob->persist; break;
	case ENT_OBJECT: obj = arg->d.obj; current = obj->persist; break;
	case ENT_ROOM: room = arg->d.room; current = room->persist; break;
	}

	if(!mob && !obj && !room) {
		bug("TpPersist - NULL target.", VNUM(info->token));
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
		bug("TpPersist - Error in parsing.",0);
		return;
	}

	switch(arg->type) {
	case ENT_NONE:   persist = !current; break;
	case ENT_STRING: persist = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"on"); break;
	default: return;
	}

	// Require security to ENABLE persistance
	if(!current && persist && script_security < MAX_SCRIPT_SECURITY) {
		bug("TpPersist - Insufficient security to enable persistance.", VNUM(info->token));
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

// token skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_tpskill)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int sn, value;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if ( script_security < 9 ) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSkill - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

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


// token skillgroup <player> add|remove <group>
SCRIPT_CMD(do_tpskillgroup)
{
	char buf[MIL];

	char *rest;
	CHAR_DATA *mob = NULL;
	int gn;
	bool fAdd = FALSE;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if ( script_security < 9 ) return;

	if(!(rest = expand_argument(info,argument,arg))) {
		bug("TpSkill - Error in parsing from vnum %ld.", VNUM(info->token));
		return;
	}

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

// token condition $PLAYER <condition> <value>
// Adjusts the specified condition by the given value
SCRIPT_CMD(do_tpcondition)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int cond, value;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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

// scriptwait $PLAYER NUMBER VNUM VNUM[ $ACTOR]
// - actor can be a $MOBILE, $OBJECT or $TOKEN
// - scripts must be available for the respective actor type
SCRIPT_CMD(do_tpscriptwait)
{

	char *rest;
	CHAR_DATA *mob = NULL;
	int wait;
	long success, failure, pulse;
	TOKEN_DATA *actor_token = NULL;
	CHAR_DATA *actor_mob = NULL;
	OBJ_DATA *actor_obj = NULL;
	int prog_type;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	info->token->progs->lastreturn = 0;

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

	actor_token = info->token;
	prog_type = PRG_TPROG;
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
	info->token->progs->lastreturn = wait;
}

// token castfailure $MOBILE[ MESSAGE]
// This will only work if the token performing the script is a spell token
// This prevents undoing the failure by setting the recovery flag.
SCRIPT_CMD(do_tpcastfailure)
{
	char *rest;
	CHAR_DATA *mob = NULL;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if( info->token->type != TOKEN_SPELL ) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;

	if( mob->cast > 0 && !mob->casting_recovered && mob->cast_successful == MAGICCAST_SUCCESS )
	{
		mob->casting_recovered = TRUE;

		if( rest && *rest )
		{
			BUFFER *buffer = new_buf();
			expand_string(info,rest,buffer);

			if( buffer->string[0] != '\0' )
			{
				add_buf(buffer, "\n\r");
				mob->casting_failure_message = str_dup(buffer->string);
				mob->cast_successful = MAGICCAST_SCRIPT;
			}
			else
			{
				mob->cast_successful = MAGICCAST_FAILURE;

			}

			free_buf(buffer);
			return;
		}

		// Leaving off the message defaults to "You lost your concentration"
		mob->cast_successful = MAGICCAST_FAILURE;
	}
}



// token castrecover $MOBILE
// This will only work if the token performing the script is a spell token

// Success is checkable with if iscastrecovered $MOBILE and if iscastsuccess $MOBILE
// This will nothing if $MOBILE isn't casting or the casting is already flagged successful.

// Recovery depends upon what caused the failure.
// - Room blocks will check both the room tests then the skill, if necessary.
// - Skill failures will ONLY test against the skill, as it passed the room tests
SCRIPT_CMD(do_tpcastrecover)
{

	char *rest;
	CHAR_DATA *mob = NULL;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if( info->token->type != TOKEN_SPELL ) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;

	if( mob->cast > 0 && !mob->casting_recovered && mob->cast_successful != MAGICCAST_SUCCESS )
	{
		bool recover = TRUE;
		int chance;
		mob->casting_recovered = TRUE;
		if(mob->cast_token) {
			if(!IS_SET(mob->cast_token->pIndexData->flags,TOKEN_NOSKILLTEST)) {
				if( mob->cast_successful == MAGICCAST_ROOMBLOCK) {
					chance = 0;

					if (IS_SET(mob->in_room->room2_flags, ROOM_HARD_MAGIC)) chance += 2;
					if (mob->in_room->sector_type == SECT_CURSED_SANCTUM) chance += 2;
					if (!IS_NPC(mob) && chance > 0 && number_range(1,chance) > 1)
						recover = FALSE;
				}

				if( recover ) {
					if (mob->cast_token->pIndexData->value[TOKVAL_SPELL_RATING] > 0) {
						if (number_range(0,mob->cast_token->pIndexData->value[TOKVAL_SPELL_RATING]) > mob->cast_token->value[TOKVAL_SPELL_RATING])
							recover = FALSE;
					} else {
						if (number_percent() > mob->cast_token->value[TOKVAL_SPELL_RATING])
							recover = FALSE;
					}
				}
			}

		} else {
			// This is a skill spell
			if( mob->cast_successful == MAGICCAST_ROOMBLOCK) {
				chance = 0;

				if (IS_SET(mob->in_room->room2_flags, ROOM_HARD_MAGIC)) chance += 2;
				if (mob->in_room->sector_type == SECT_CURSED_SANCTUM) chance += 2;
				if (!IS_NPC(mob) && chance > 0 && number_range(1,chance) > 1)
					recover = FALSE;
			}
			if (recover && number_percent() > get_skill(mob, mob->cast_sn))
				recover = FALSE;
		}

		if(recover)
			mob->cast_successful = MAGICCAST_SUCCESS;
	}
}

// addspell $OBJECT STRING[ NUMBER]
SCRIPT_CMD(do_tpaddspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_new;
	OBJ_DATA *target;
	int level;
	int sn;
	AFFECT_DATA *paf;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_tpremspell)
{

	char *rest;
	SPELL_DATA *spell, *spell_prev;
	OBJ_DATA *target;
	int level;
	int sn;
	bool found = FALSE, show = TRUE;
	AFFECT_DATA *paf;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_tpalteraffect)
{
	char buf[MIL],field[MIL],*rest;

	AFFECT_DATA *paf;
	int value;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_AFFECT || !arg->d.aff) return;

	paf = arg->d.aff;

	if(!(rest = expand_argument(info,rest,arg))) {
		bug("TpAlterAffect - Error in parsing.",0);
		return;
	}

	if( IS_NULLSTR(rest) ) return;

	if( arg->type != ENT_STRING || IS_NULLSTR(arg->d.str) ) return;

	strncpy(field,arg->d.str,MIL-1);


	if( !str_cmp(field, "level") ) {
		argument = one_argument(rest,buf);

		if(!(rest = expand_argument(info,argument,arg))) {
			bug("TpAlterAffect - Error in parsing.",0);
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
			bug("TpAlterAffect - Error in parsing.",0);
			return;
		}

		if( paf->slot != WEAR_NONE ) {
			bug("TpAlterAffect - Attempting to modify duration of an object given affect.",0);
			return;
		}

		if( paf->group == AFFGROUP_RACIAL ) {
			bug("TpAlterAffect - Attempting to modify duration of a racial affect.",0);
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
SCRIPT_CMD(do_tpcrier)
{
	if(!info || !info->token) return;

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

// Syntax:	checkpoint $PLAYER $ROOM
// 			checkpoint $PLAYER VNUM
//			checkpoint $PLAYER none|clear|reset
//
// Sets the checkpoint of the $PLAYER to the destination or clears it.
// - When a checkpoint is set, it will override what location is saved to the pfile.
// - When setting a checkpoint via VNUM and an invalid vnum is given, it will clear the checkpoint.
SCRIPT_CMD(do_tpcheckpoint)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_tpsaveplayer)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	info->token->progs->lastreturn = 0;

	if(!(rest = expand_argument(info,argument,arg)))
		return;

	if(arg->type != ENT_MOBILE || !arg->d.mob) return;

	mob = arg->d.mob;
	if(IS_NPC(mob)) return;

	save_char_obj(mob);

	info->token->progs->lastreturn = 1;
}

// Syntax: FIXAFFECTS $MOBILE
SCRIPT_CMD(do_tpfixaffects)
{


	if(!info || !info->token || IS_NULLSTR(argument)) return;

	if(!expand_argument(info,argument,arg))
		return;

	if(arg->type != ENT_MOBILE) return;

	if(arg->d.mob == NULL) return;

	affect_fix_char(arg->d.mob);
}

// Syntax: remort $PLAYER
//  - prompts them for a class out of what they can do
SCRIPT_CMD(do_tpremort)
{
	char *rest;

    CHAR_DATA *mob;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	info->token->progs->lastreturn = 0;

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

	info->token->progs->lastreturn = 1;
}

// Syntax: restore $MOBILE[ PERCENT]
SCRIPT_CMD(do_tprestore)
{
	char *rest;

	int amount = 100;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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
SCRIPT_CMD(do_tpgroup)
{
	char *rest;

	CHAR_DATA *follower, *leader;
	bool fShow = TRUE;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

	info->token->progs->lastreturn = 0;

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
		info->token->progs->lastreturn = 1;
}

// UNGROUP mobile[ bool(ALL=false)]
SCRIPT_CMD(do_tpungroup)
{
	char *rest;

	bool fAll = FALSE;

	if(!info || !info->token || IS_NULLSTR(argument)) return;

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

