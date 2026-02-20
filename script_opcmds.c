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
#include "editors/common.h"
#include "skill_data.h"

extern bool wiznet_script;

static AREA_DATA *script_relative_widevnum_context(AREA_DATA *context_area, const char *argument)
{
    if (!context_area || IS_NULLSTR(argument) || argument[0] != '#')
        return NULL;

    return context_area;
}

const struct script_cmd_type obj_cmd_table[] = {
    { "addaffect",			scriptcmd_addaffect,	true,	true	},
    { "addaffectname",		scriptcmd_addaffectname,true,	true	},
    { "addspell",			scriptcmd_addspell,		true,	true	},
    { "alteraffect",		scriptcmd_alteraffect,		true,	true	},
    { "alterexit",			do_opalterexit,			false,	true	},
    { "altermob",			do_opaltermob,			true,	true	},
    { "alterobj",			scriptcmd_alterobj,			true,	true	},
    { "alterroom",			scriptcmd_alterroom,			true,	true	},
    { "applytoxin",			scriptcmd_applytoxin,	false,	true	},
    { "asound",				scriptcmd_asound,		false,	true	},
    { "at",					scriptcmd_at,			false,	true	},
    { "attach",				scriptcmd_attach,			true,	true	},
    { "award",				scriptcmd_award,		true,	true	},
    { "breathe",			scriptcmd_breathe,		false,	true	},
    { "call",				scriptcmd_call,			false,	true	},
    { "cancel",				scriptcmd_cancel,		false,	false	},
    { "cast",       		do_opcast,				false,	true	},
    { "chargebank",			scriptcmd_chargebank,	false,	true	},
    { "checkpoint",			scriptcmd_checkpoint,	false,	true	},
    { "churchannouncetheft",	scriptcmd_churchannouncetheft,	true, true },
    { "cloneroom",			do_opcloneroom,			true,	true	},
    { "condition",			scriptcmd_condition,		false,	true	},
    { "crier",				scriptcmd_crier,			false,	true	},
    { "damage",				scriptcmd_damage,		false,	true	},
    { "deduct",				scriptcmd_deduct,		true,	true	},
    { "delay",				scriptcmd_delay,			false,	true	},
    { "dequeue",			scriptcmd_dequeue,		false,	false	},
    { "destroyroom",		do_opdestroyroom,		true,	true	},
    { "detach",				scriptcmd_detach,			true,	true	},
    { "dungeoncomplete",	scriptcmd_dungeoncomplete,	true,	true	},
    { "dungeoncommence",	scriptcmd_dungeoncommence,	true,	true	},
    { "dungeonfailure",	scriptcmd_dungeonfailure,	true,	true	},
    { "event",              scriptcmd_event,          false,  true    },
    { "echo",				scriptcmd_echo,			false,	true	},
    { "echoaround",			scriptcmd_echoaround,	false,	true	},
    { "echoat",				scriptcmd_echoat,		false,	true	},
    { "echobattlespam",		scriptcmd_echobattlespam,	false,	true	},
    { "echochurch",			scriptcmd_echochurch,		false,	true	},
    { "echogrouparound",	scriptcmd_echogrouparound,	false,	true	},
    { "echogroupat",		scriptcmd_echogroupat,		false,	true	},
    { "echoleadaround",		scriptcmd_echoleadaround,	false,	true	},
    { "echoleadat",			scriptcmd_echoleadat,		false,	true	},
    { "echonotvict",		scriptcmd_echonotvict,	false,	true	},
    { "echoroom",			scriptcmd_echoroom,		false,	true	},
    { "ed",					scriptcmd_ed,				false,	true	},
    { "entercombat",		scriptcmd_entercombat,	false,	true	},
    { "fade",				scriptcmd_fade,				true,	true	},
    { "fixaffects",			scriptcmd_fixaffects,	false,	true	},
    { "flee",				scriptcmd_flee,			false,	true	},
    { "force",				scriptcmd_force,			false,	true	},
    { "forget",				scriptcmd_forget,		false,	false	},
    { "gdamage",			scriptcmd_gdamage,		false,	true	},
    { "gecho",       		scriptcmd_gecho,			false,	true	},
    { "gforce",				scriptcmd_gforce,		false,	true	},
    { "goto",				scriptcmd_goto,			false,	true	},
    { "grantclass",			scriptcmd_grantclass,	false,	true	},
    { "grantskill",			scriptcmd_grantskill,	false,	true	},
    { "grantsong",			scriptcmd_grantsong,	false,	true	},
    { "group",				do_opgroup,				false,	true	},
    { "gtransfer",			scriptcmd_gtransfer,		false,	true	},
    { "input",				do_opinput,				false,	true	},
    { "inputstring",		scriptcmd_inputstring,	false,	true	},
    { "instancecomplete",	scriptcmd_instancecomplete,	true,	true	},
    { "instancefailure",	scriptcmd_instancefailure,	true,	true	},
    { "interrupt",			do_opinterrupt,			false,	true	},
    { "junk",				do_opjunk,				false,	true	},
    { "link",				do_oplink,				false,	true	},
    { "loadinstanced",		scriptcmd_loadinstanced,	true,	true	},
    { "lockadd",			scriptcmd_lockadd,			false,	true	},
    { "lockremove",			scriptcmd_lockremove,		false,	true	},
    { "mail",				scriptcmd_mail,				true,	true	},
    { "mload",				scriptcmd_mload,			false,	true	},
    { "mute",				scriptcmd_mute,			false,	true	},
    { "oload",				scriptcmd_oload,			false,	true	},
    { "otransfer",			do_opotransfer,			false,	true	},
    { "pageat",				scriptcmd_pageat,			false,	true	},
    { "peace",				scriptcmd_peace,			false,	false	},
    { "persist",			scriptcmd_persist,		false,	true	},
    { "prompt",				do_opprompt,			false,	true	},
    { "purge",				scriptcmd_purge,			false,	false	},
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
    { "queue",				scriptcmd_queue,			false,	true	},
    { "rawkill",			do_oprawkill,			false,	true	},
    { "reckoning",			scriptcmd_reckoning,		true,	true	},
    { "remember",			scriptcmd_remember,	false,	true	},
    { "remort",				scriptcmd_remort,		true,	true	},
    { "remove",				do_opremove,			false,	true	},
    { "remspell",			scriptcmd_remspell,		true,	true	},
    { "resetdice",			scriptcmd_resetdice,		true,	true	},
    { "resetroom",			scriptcmd_resetroom,	true,	true	},
    { "restore",			scriptcmd_restore,		true,	true	},
    { "revokeclass",		scriptcmd_revokeclass,	false,	true	},
    { "revokeskill",		scriptcmd_revokeskill,	false,	true	},
    { "revokesong",			scriptcmd_revokesong,	false,	true	},
    { "saveplayer",			scriptcmd_saveplayer,	false,	true	},
    { "scriptwait",			scriptcmd_scriptwait,	false,	true	},
    { "selfdestruct",		do_opselfdestruct,		false,	false	},
    { "sendfloor",			scriptcmd_sendfloor,		false,	true	},
    { "setclass",			scriptcmd_setclass,		false,	true	},
    { "setrace",			scriptcmd_setrace,		false,	true	},
    { "setrecall",			scriptcmd_setrecall,		false,	true	},
    { "settimer",			scriptcmd_settimer,		false,	true	},
    { "settrait",			scriptcmd_settrait,		false,	true	},
    { "showcommand",		scriptcmd_showcommand,		false,	true	},
    { "showroom",			do_opshowroom,			true,	true	},
    { "skimprove",			do_opskimprove,			true,	true	},
    { "spawndungeon",		scriptcmd_spawndungeon,		true,	true	},
    { "specialkey",			scriptcmd_specialkey,		false,	true	},
    { "startcombat",		scriptcmd_startcombat,	false,	true	},
    { "startreckoning",		scriptcmd_startreckoning,	true,	true	},
    { "stopcombat",			scriptcmd_stopcombat,	false,	true	},
    { "stopreckoning",		scriptcmd_stopreckoning,	true,	true	},
    { "stringmob",			scriptcmd_stringmob,		true,	true	},
    { "stringobj",			scriptcmd_stringobj,		true,	true	},
    { "stripaffect",		scriptcmd_stripaffect,		true,	true	},
    { "stripaffectname",	scriptcmd_stripaffectname,	true,	true	},
    { "transfer",			scriptcmd_transfer,			false,	true	},
    { "treasuremap",		scriptcmd_treasuremap,		false,	true	},
    { "ungroup",			scriptcmd_ungroup,		false,	true	},
    { "unlockarea",			scriptcmd_unlockarea,		true,	true	},
    { "unlockdungeon",		scriptcmd_unlockdungeon,	true,	true	},
    { "unmute",				scriptcmd_unmute,		false,	true	},
    { "usecatalyst",		do_opusecatalyst,		false,	true	},
    { "varclear",			scriptcmd_varclear,		false,	true	},
    { "varclearon",			scriptcmd_varclearon,		false,	true	},
    { "varcopy",			scriptcmd_varcopy,			false,	true	},
    { "varsave",			scriptcmd_varsave,			false,	true	},
    { "varsaveon",			scriptcmd_varsaveon,		false,	true	},
    { "varset",				scriptcmd_varset,			false,	true	},
    { "varseton",			scriptcmd_varseton,		false,	true	},
    { "vforce",				scriptcmd_vforce,		false,	true	},
    { "wildernessmap",		scriptcmd_wildernessmap,	false,	true	},
    { "wiznet",				scriptcmd_wiznet,			false,	true    },
    { "wiretransfer",		scriptcmd_wiretransfer,	false,	true	},
    { "xcall",				scriptcmd_xcall,			false,	true	},
    { "zecho",				scriptcmd_zecho,			false,	true	},
    { "zot",				scriptcmd_zot,			true,	true	},
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
    SCRIPT_DATA *oprg;
    WNUM wnum;

    if (argument[0] == '\0')
    {
        send_to_char("Syntax: opdump <vnum>\n\r", ch);
        return;
    }

    if (!parse_widevnum(argument, script_relative_widevnum_context(ch->in_room ? ch->in_room->area : NULL, argument), &wnum))
    {
        send_to_char("Invalid vnum format.\n\r", ch);
        return;
    }

    if (!(oprg = get_script_index(wnum.pArea, wnum.vnum, PRG_OPROG))) {
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
    OBJ_DATA *obj;
    BUFFER *output = new_buf();

    one_argument(argument, arg);

    if (!arg[0]) {
        send_to_char("Opstat what?\n\r", ch);
        return;
    }

    if (is_number(arg))
    {
        argument = one_argument(argument, arg);
        if (argument[0] != '\0' && is_number(arg) && is_number(argument))
        {
            if ((obj = idfind_object(atoi(arg), atoi(argument))) == NULL)
            {
                send_to_char("Object not found.\n\r", ch);
                return;
            }
        }
        else
        {
            send_to_char("Syntax: opstat <name|IDa IDb>",ch);
            return;
        }	
                
    } else if (!(obj = get_obj_world(ch, arg))) {
        add_buf(output, "No such object.\n\r");
        return;
    }

    sprintf(arg, "Object #%-6ld [%s] ID [%9d:%9d]\n\r", obj->pIndexData->vnum, obj->short_descr, (int)obj->id[0], (int)obj->id[1]);
    add_buf(output, arg);

    if( !IS_NULLSTR(obj->pIndexData->comments) )
    {
        sprintf(arg, "Comments:\n\r%s\n\r", obj->pIndexData->comments);
        add_buf(output, arg);
    }

    sprintf(arg, "Delay   %-6d [%s]\n\r",
        obj->progs->delay,
        obj->progs->target ? obj->progs->target->name : "No target");

    add_buf(output, arg);

    if (!obj->pIndexData->progs)
        add_buf(output, "[No programs set]\n\r");
    else
        olc_show_progs_grouped(output, obj->pIndexData->progs, PRG_OPROG, NULL);

    if(obj->progs->vars)
        pstat_variable_list(output, obj->progs->vars);

    if( !ch->lines && strlen(output->string) > MAX_STRING_LENGTH )
    {
        send_to_char("Too much to display.  Please enable scrolling.\n\r", ch);
    }
    else
    {
        page_to_char(output->string, ch);
    }
}


char *op_getlocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room)
{
    return script_getlocation(info, argument, room);
}

char *op_getolocation(SCRIPT_VARINFO *info, char *argument, ROOM_INDEX_DATA **room, OBJ_DATA **container, CHAR_DATA **carrier, int *wear_loc)
{
    char *rest, *rest2;
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
        case ENT_WIDEVNUM:
            *room = get_room_index(arg->d.wnum.pArea, arg->d.wnum.vnum);
            break;
        case ENT_NUMBER:
            x = arg->d.num;
            if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                rest = rest2;
                y = arg->d.num;
                if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                    rest = rest2;
                    if(!(pWilds = get_wilds_from_uid(NULL, arg->d.num))) break;

                    if (x > (pWilds->map_size_x - 1) || y > (pWilds->map_size_y - 1)) break;

                    if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_STRING &&
                        !str_cmp(arg->d.str,"safe") && !check_for_bad_room(pWilds, x, y))
                        break;

                    rest = rest2;
                    room_used_for_wilderness.wilds = pWilds;
                    room_used_for_wilderness.x = x;
                    room_used_for_wilderness.y = y;
                    *room = &room_used_for_wilderness;
                }
            } else {
                WNUM room_wnum;
                if (resolve_widevnum(x, NULL, &room_wnum))
                    *room = get_room_index(room_wnum.pArea, room_wnum.vnum);
            }
            break;

        case ENT_STRING:
            if(arg->d.str[0] == '@')
                *room = get_exit_dest(obj_room(info->obj), arg->d.str+1);
            else if(!str_cmp(arg->d.str,"here"))
                *room = obj_room(info->obj);
            else if(!str_cmp(arg->d.str,"vroom")) {
                int vnum, id1, id2;
                if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                    rest = rest2;
                    vnum = arg->d.num;
                    if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                        rest = rest2;

                        id1 = arg->d.num;
                        if((rest2 = expand_argument(info,rest,arg)) && arg->type == ENT_NUMBER) {
                            rest = rest2;

                            id2 = arg->d.num;
                            WNUM room_wnum;
                            if (resolve_widevnum(vnum, NULL, &room_wnum))
                                *room = get_clone_room(get_room_index(room_wnum.pArea, room_wnum.vnum), id1, id2);
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
                            for (int iHash = 0; iHash < MAX_KEY_HASH && !loc; iHash++)
                                if ((loc = area->room_index_hash[iHash]) != NULL)
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
            ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
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
    char command[MSL];
    int cmd;

    if(!info->obj) return;

    if (!str_prefix("obj ", argument))
        argument = skip_whitespace(argument+3);

    argument = one_argument(argument, command);

    cmd = opcmd_lookup(command);

    if(cmd < 0) {
        pbugf(LOG_SCRIPTS, "Obj_interpret: invalid cmd from obj %ld: '%s'", info->obj->pIndexData->vnum, command);
        return;
    }

    SCRIPT_PARAM *arg = new_script_param();
    (*obj_cmd_table[cmd].func) (info, argument, arg);
    free_script_param(arg);
    tail_chain();
}

// do_opcall
SCRIPT_CMD(do_opcall)
{
    char *rest;
    CHAR_DATA *vch,*ch;
    OBJ_DATA *obj1,*obj2;
    SCRIPT_DATA *script;
    int depth, ret;
    long vnum;


    if(!info || !info->obj) return;

    if (!argument[0]) {
        pbugf(LOG_SCRIPTS, "OpCall: missing arguments from vnum %d.", VNUM(info->obj));
        return;
    }

    // Call depth checking
    depth = script_call_depth;
    if(script_call_depth == 1) {
        pbugf(LOG_SCRIPTS, "OpCall: maximum call depth exceeded for obj vnum %d.", VNUM(info->obj));
        return;
    } else if(script_call_depth > 1)
        --script_call_depth;


    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, PRG_OPROG, &vnum);
    if (!script || vnum < 1) {
        pbugf(LOG_SCRIPTS, "OpCall: invalid prog from vnum %d.", VNUM(info->obj));
        return;
    }

    ch = vch = NULL;
    obj1 = obj2 = NULL;

    if(*rest) {	// Enactor
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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

    ret = execute_script(script->vnum, script, NULL, info->obj, NULL, NULL, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->obj)
        info->obj->progs->lastreturn = ret;
    else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
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
        pbugf(LOG_SCRIPTS, "MpCast - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpCast - No such spell from vnum %d.", VNUM(info->obj));
        return;
    }

    room = obj_room(info->obj);

    if(*rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpCast - Error in parsing from vnum %ld.", VNUM(info->obj));
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

    proxy = create_mobile(get_reserved_mob_index("mob_objcaster"), false);
    char_to_room(proxy, room);

    proxy->level = info->obj->level;
    free_string(proxy->name);
    proxy->name = str_dup(info->obj->name);
    free_string(proxy->short_descr);
    proxy->short_descr = str_dup(info->obj->short_descr);

    // Make sure they have a reagent for the powerful spells
    reagent = create_object(get_reserved_obj_index("obj_black_moonstone_shard"), 1, false);
    obj_to_char(reagent,proxy);

    switch (skill_table[sn].target) {
    default: pbugf(LOG_SCRIPTS, "obj_cast: bad target for sn %d.", sn); return;
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
        (*skill_table[sn].spell_fun)(skill_find_uid(sn), info->obj->level, proxy, to, target, WEAR_NONE, INVOC_INTERNAL);
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
        pbugf(LOG_SCRIPTS, "OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpDamage - Null victim from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "OpDamage - missing argument from vnum %ld.", VNUM(info->obj));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "OpDamage - missing argument from vnum %ld.", VNUM(info->obj));
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpDamage - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(fLevel && !victim) {
        pbugf(LOG_SCRIPTS, "OpDamage - Level aspect used with null victim from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
            return;
        }
        break;
    case ENT_MOBILE:
        if(fLevel) {
            if(arg->d.mob) level = arg->d.mob->tot_level;
            else {
                pbugf(LOG_SCRIPTS, "OpDamage - Null reference mob from vnum %ld.", VNUM(info->obj));
                return;
            }
            break;
        } else {
            pbugf(LOG_SCRIPTS, "OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
            return;
        }
        break;
    default:
        pbugf(LOG_SCRIPTS, "OpDamage - invalid argument from vnum %ld.", VNUM(info->obj));
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

// do_opforce
SCRIPT_CMD(do_opforce)
{
    char *rest;
    CHAR_DATA *victim = NULL, *next;
    bool fAll = false, forced;


    if(!info || !info->obj) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpForce - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpForce - Null victim from vnum %ld.", VNUM(info->obj));
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

// do_opgoto
SCRIPT_CMD(do_opgoto)
{
    ROOM_INDEX_DATA *dest;

    if(!info || !info->obj || !obj_room(info->obj) || PROG_FLAG(info->obj,PROG_AT)) return;

    if(!argument[0]) {
        pbugf(LOG_SCRIPTS, "Opgoto - No argument from vnum %d.", VNUM(info->obj));
        return;
    }

    op_getlocation(info, argument, &dest);

    if(!dest) {
        pbugf(LOG_SCRIPTS, "Opgoto - Bad location from vnum %d.", VNUM(info->obj));
        return;
    }

    if (info->obj->in_obj) obj_from_obj(info->obj);
    else if (info->obj->carried_by) obj_from_char(info->obj);
    else if (info->obj->in_room) obj_from_room(info->obj);

    // @@@NIB Need to take into account that it is a pulled cart OR used furniture as well

    obj_to_room(info->obj, dest);

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
    AREA_DATA *link_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM link_wnum = wnum_zero;
    unsigned long id1, id2;

    bool del = false;
    bool environ = false;
    EXIT_DATA *ex;

    if(!info || !info->obj) return;

    if(!(rest = expand_argument(info,argument,arg)))
        return;

    context_area = get_area_from_scriptinfo(info);

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
        pbugf(LOG_SCRIPTS, "OPlink used without an argument from room vnum %d.", room->vnum);
        return;
    }

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg)))
        return;

    vnum = -1;
    id1 = id2 = 0;
    switch(arg->type) {
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"delete") ||
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
        } else if (parse_widevnum(arg->d.str, script_relative_widevnum_context(context_area, arg->d.str), &link_wnum) && link_wnum.pArea) {
            vnum = link_wnum.vnum;
            link_area = link_wnum.pArea;
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
        link_area = arg->d.room ? arg->d.room->area : NULL;
        break;
    case ENT_EXIT:
        ex = arg->d.door.r ? arg->d.door.r->exit[arg->d.door.door] : NULL;
        vnum = (ex && ex->u1.to_room) ? ex->u1.to_room->vnum : -1;
        link_area = (ex && ex->u1.to_room) ? ex->u1.to_room->area : NULL;
        break;
    case ENT_MOBILE:
        vnum = (arg->d.mob && arg->d.mob->in_room) ? arg->d.mob->in_room->vnum : -1;
        link_area = (arg->d.mob && arg->d.mob->in_room) ? arg->d.mob->in_room->area : NULL;
        break;
    case ENT_OBJECT:
        vnum = (arg->d.obj && obj_room(arg->d.obj)) ? obj_room(arg->d.obj)->vnum : -1;
        link_area = (arg->d.obj && obj_room(arg->d.obj)) ? obj_room(arg->d.obj)->area : NULL;
        break;
    }

    if(vnum < 0) {
        pbugf(LOG_SCRIPTS, "OPlink - invalid argument in room %d.", room->vnum);
        return;
    }

    WNUM dest_wnum;
    if(id1 > 0 || id2 > 0) {
        if (link_area)
            dest = get_clone_room(get_room_index(link_area, vnum),id1,id2);
        else if (resolve_widevnum(vnum, NULL, &dest_wnum))
            dest = get_clone_room(get_room_index(dest_wnum.pArea, dest_wnum.vnum),id1,id2);
    } else if(vnum > 0) {
        if (link_area)
            dest = get_room_index(link_area, vnum);
        else if (resolve_widevnum(vnum, NULL, &dest_wnum))
            dest = get_room_index(dest_wnum.pArea, dest_wnum.vnum);
    } else if(environ)
        dest = &room_pointer_environment;
    else
        dest = NULL;

    if(!dest && !del) {
        pbugf(LOG_SCRIPTS, "OPlink - invalid destination in room %d.", room->vnum);
        return;
    }

    script_change_exit(room, dest, door);
}

// do_opoload
SCRIPT_CMD(do_opoload)
{
    /*
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
        pbugf(LOG_SCRIPTS, "Opoload - Bad vnum arg from vnum %d.", VNUM(info->obj));
        return;
    }

    if(rest && *rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg)))
            return;

        switch(arg->type) {
        case ENT_NUMBER: level = arg->d.num; break;
        case ENT_STRING: level = arg->d.str ? atoi(arg->d.str) : 0; break;
        case ENT_MOBILE: level = arg->d.mob ? get_mob_level(arg->d.mob) : 0; break;
        case ENT_OBJECT: level = arg->d.obj ? arg->d.obj->pIndexData->level : 0; break;
        default: level = 0; break;
        }

        if(level <= 0) level = info->obj->pIndexData->level;

        if(rest && *rest) {
            argument = rest;
            if(!(rest = expand_argument(info,argument,arg)))
                return;

            //
            // Added 3rd argument
            // omitted - load to current room
            // 'I'     - load to object's container
            // MOBILE  - load to target mobile
            //         - 'W' automatically wear the item if possible
            // OBJECT  - load to target object
            // ROOM    - load to target room
             

            switch(arg->type) {
            case ENT_STRING:
                if (!str_cmp(arg->d.str, "inside") &&
                    IS_SET(pObjIndex->wear_flags, ITEM_TAKE)) {
                    if( info->obj->item_type == ITEM_CONTAINER ||
                        info->obj->item_type == ITEM_CART)
                        fInside = true;

                    else if( info->obj->item_type == ITEM_WEAPON_CONTAINER &&
                        pObjIndex->item_type == ITEM_WEAPON &&
                        IS_WEAPON_CON(info->obj) && IS_WEAPON(pObjIndex) &&
                        WEAPON_CON(info->obj)->weapon_type == WEAPON(pObjIndex)->weapon_class)
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
                        IS_WEAPON(pObjIndex) && IS_WEAPON_CON(arg->d.obj) &&
                        WEAPON(pObjIndex)->weapon_class == WEAPON_CON(arg->d.obj)->weapon_type)
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
    */
    script_oload(info,argument,arg, false);
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
        pbugf(LOG_SCRIPTS, "OpOtransfer - Bad syntax from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: obj = get_obj_here(NULL,obj_room(info->obj), arg->d.str); break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: obj = NULL; break;
    }


    if (!obj) {
        pbugf(LOG_SCRIPTS, "OpOtransfer - Null object from vnum %ld.", VNUM(info->obj));
        return;
    }

    if (PROG_FLAG(obj,PROG_AT)) return;

    if (IS_SET(obj->extra[2], ITEM_NO_TRANSFER) && script_security < MAX_SCRIPT_SECURITY) return;

    argument = op_getolocation(info, rest, &dest, &container, &carrier, &wear_loc);

    if(!dest && !container && !carrier) {
        pbugf(LOG_SCRIPTS, "OpOTransfer - Bad location from vnum %d.", VNUM(info->obj));
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

// remobe <target> <object|all.object|vnum> [count]
SCRIPT_CMD(do_opremove)
{
    CHAR_DATA *victim;
    OBJ_DATA *obj = NULL;
    int vnum = 0, count = 0;
    bool fAll = false;
    AREA_DATA *item_area = NULL;
    AREA_DATA *context_area = NULL;
    WNUM item_wnum = wnum_zero;
    ITERATOR it;

    char name[MIL], *rest;

    if(!info || !info->obj) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpRemove - Bad syntax from vnum %ld.", VNUM(info->obj));
        return;
    }

    context_area = get_area_from_scriptinfo(info);

    switch(arg->type) {
    case ENT_STRING: victim = get_char_room(NULL, obj_room(info->obj), arg->d.str); break;
    case ENT_MOBILE: victim = arg->d.mob; break;
    default: victim = NULL; break;
    }

    if (!victim) {
        pbugf(LOG_SCRIPTS, "OpRemove - Null victim from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!*rest) return;

    argument = rest;
    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpRemove - Bad syntax from vnum %ld.", VNUM(info->obj));
        return;
    }

    name[0] = '\0';
    switch(arg->type) {
    case ENT_NUMBER: vnum = arg->d.num; break;
    case ENT_STRING:
        if(!str_cmp(arg->d.str,"all"))
            fAll = true;
        else if (parse_widevnum(arg->d.str, script_relative_widevnum_context(context_area, arg->d.str), &item_wnum) && item_wnum.pArea) {
            vnum = item_wnum.vnum;
            item_area = item_wnum.pArea;
        }
        else
            strncpy(name,arg->d.str,MIL-1);
        break;
    case ENT_OBJECT: obj = arg->d.obj; break;
    default: break;
    }

    if(!fAll && vnum < 1 && !name[0] && !obj) {
        pbugf(LOG_SCRIPTS, "OpRemove - Invalid object from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!fAll && !obj && *rest) {
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpRemove - Bad syntax from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_NUMBER: count = arg->d.num; break;
        case ENT_STRING: count = atoi(arg->d.str); break;
        default: count = 0; break;
        }

        if(count < 0) {
            pbugf(LOG_SCRIPTS, "OpRemove - Invalid count from vnum %d.", VNUM(info->obj));
            count = 0;
        }
    }

    if(obj) {
        if((obj->wear_loc != WEAR_NONE && obj->carried_by == victim) || 
           (obj->in_obj && obj->in_obj->carried_by == victim)) {
            // Unequip item if it's worn
            if(obj->wear_loc != WEAR_NONE)
                unequip_char(victim, obj, true);
            extract_obj(obj);
        }
    } else {
        // Check items being carried (lcarrying)
        if (victim->lcarrying && IS_VALID(victim->lcarrying)) {
            iterator_start(&it, victim->lcarrying);
            while ((obj = iterator_nextdata(&it))) {
                if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum && (!item_area || obj->pIndexData->area == item_area)) ||
                    (*name && is_name(name, obj->name))) {
                    iterator_remcurrent(&it);
                    extract_obj(obj);

                    if(count > 0 && !--count) break;
                }
            }
            iterator_stop(&it);
        }

        // Check worn items (lworn)
        if (count != 0 && victim->lworn && IS_VALID(victim->lworn)) {
            iterator_start(&it, victim->lworn);
            while ((obj = iterator_nextdata(&it))) {
                if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum && (!item_area || obj->pIndexData->area == item_area)) ||
                    (*name && is_name(name, obj->name))) {
                    iterator_remcurrent(&it);
                    unequip_char(victim, obj, true);
                    extract_obj(obj);

                    if(count > 0 && !--count) break;
                }
            }
            iterator_stop(&it);
        }

        // Check locker items (llocker)
        if (count != 0 && victim->llocker && IS_VALID(victim->llocker)) {
            iterator_start(&it, victim->llocker);
            while ((obj = iterator_nextdata(&it))) {
                if (fAll || (vnum > 0 && obj->pIndexData->vnum == vnum && (!item_area || obj->pIndexData->area == item_area)) ||
                    (*name && is_name(name, obj->name))) {
                    iterator_remcurrent(&it);
                    extract_obj(obj);

                    if(count > 0 && !--count) break;
                }
            }
            iterator_stop(&it);
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
        pbugf(LOG_SCRIPTS, "OpSelfDestruct: BAILED OUT, OBJ IS NOWHERE");
        return;
    }

    if ((vch = info->obj->carried_by) && info->obj->wear_loc != -1)
        unequip_char(vch, info->obj, true);

    extract_obj(info->obj);
    //info->obj = NULL;	// Handled by recycling code
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
        pbugf(LOG_SCRIPTS, "OpInterrupt - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpInterrupt - NULL victim from vnum %ld.", VNUM(info->obj));
        return;
    }

    BUFFER *buffer = new_buf();
    expand_string(info,rest,buffer);
    if(buffer->string[0] != '\0') {
        stop = flag_value(interrupt_action_types,buf);
        if(stop == NO_FLAG) {
            pbugf(LOG_SCRIPTS, "OpInterrupt - invalid interrupt type from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterObj - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterObj - NULL object from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(PROG_FLAG(obj,PROG_AT)) return;

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "OpAlterObj - Missing field type from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAlterObj - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterObj - Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "%s from vnum %ld.", buf, VNUM(info->obj));
            return;
        }

        switch (buf[0]) {
        case '+': obj->value[num] += value; break;
        case '-': obj->value[num] -= value; break;
        case '*': obj->value[num] *= value; break;
        case '/':
            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            obj->value[num] /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
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
        else if(!str_cmp(field,"key"))			{ if( obj->lock ) { ptr = (int*)&obj->lock->key_wnum.vnum; } }
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
            pbugf(LOG_SCRIPTS, "%s from vnum %ld.", buf, VNUM(info->obj));
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
                pbugf(LOG_SCRIPTS, "OpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }

            *ptr += value;
            break;

        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }

            *ptr -= value;
            break;

        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }

            *ptr *= value;
            break;

        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - alterobj called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }

            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterObj - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterObj - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterObj - NULL object from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(PROG_FLAG(obj,PROG_AT)) return;

    if(obj->item_type == ITEM_WEAPON)
        set_weapon_dice_obj(obj);
}



SCRIPT_CMD(do_opaltermob)
{
    char buf[MSL],field[MIL],*rest;
    int value = 0, min_sec = MIN_SCRIPT_SECURITY, min = 0, max = 0;
    CHAR_DATA *mob = NULL;

    int *ptr = NULL;
    long *lptr = NULL;
    bool allowpc = false;
    bool allowarith = true;
    bool allowbitwise = true;
    bool lookuprace = false;
    long race_change_flags = 0;
    bool lookup_attack_type = false;
    bool hasmin = false;
    bool hasmax = false;
    const struct flag_type *flags = NULL;
    const struct flag_type **bank = NULL;
    long temp_flags[4];
    int dirty_stat = -1;

    if(!info || !info->obj) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpAlterMob - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterMob - NULL mobile from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "OpAlterMob - Missing field type from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAlterMob - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    field[0] = 0;

    switch(arg->type) {
    case ENT_STRING: strncpy(field,arg->d.str,MIL-1); break;
    default: return;
    }

    if(!field[0]) return;

/*
    argument = one_argument(rest,buf);

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpAlterMob - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }
*/
    if(!str_cmp(field,"acbash"))		ptr = (int*)&mob->armour[AC_BASH];
    else if(!str_cmp(field,"acexotic"))	ptr = (int*)&mob->armour[AC_EXOTIC];
    else if(!str_cmp(field,"acpierce"))	ptr = (int*)&mob->armour[AC_PIERCE];
    else if(!str_cmp(field,"acslash"))	ptr = (int*)&mob->armour[AC_SLASH];
    else if(!str_cmp(field,"act"))		{ lptr = mob->act; bank = IS_NPC(mob) ? act_flagbank : plr_flagbank; }
    else if(!str_cmp(field,"affect"))	{ lptr = mob->affected_by; bank = affect_flagbank; }
    else if(!str_cmp(field,"alignment"))	ptr = (int*)&mob->alignment;
    else if(!str_cmp(field,"bashed"))	ptr = (int*)&mob->bashed;
    else if(!str_cmp(field,"bind"))		ptr = (int*)&mob->bind;
    else if(!str_cmp(field,"bomb"))		ptr = (int*)&mob->bomb;
    else if(!str_cmp(field,"brew"))		ptr = (int*)&mob->brew;
    else if(!str_cmp(field,"cast"))		ptr = (int*)&mob->cast;
    else if(!str_cmp(field,"comm"))		{ lptr = IS_NPC(mob)?NULL:&mob->comm; allowpc = true; allowarith = false; min_sec = 7; flags = comm_flags; }		// 20140512NIB - Allows for scripted fun with player communications, only bit operators allowed
    else if(!str_cmp(field,"damroll"))	ptr = (int*)&mob->damroll;
    else if(!str_cmp(field,"damtype"))	{ ptr = (int*)&mob->dam_type; allowpc = false; allowarith = false; min_sec = 7; lookup_attack_type = true; }
    else if(!str_cmp(field,"danger"))	{ ptr = (int*)IS_NPC(mob)?NULL:&mob->pcdata->danger_range; allowpc = true; }
    else if(!str_cmp(field,"daze"))		ptr = (int*)&mob->daze;
    else if(!str_cmp(field,"death"))	{ ptr = (IS_NPC(mob) || !IS_DEAD(mob))?NULL:(int*)&mob->time_left_death; allowpc = true; }
    else if(!str_cmp(field,"dicenumber"))	{ ptr = IS_NPC(mob)?(int*)&mob->damage.number:NULL; }
    else if(!str_cmp(field,"dicetype"))	{ ptr = IS_NPC(mob)?(int*)&mob->damage.size:NULL; }
    else if(!str_cmp(field,"dicebonus"))	{ ptr = IS_NPC(mob)?(int*)&mob->damage.bonus:NULL; }
    else if(!str_cmp(field,"drunk"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_DRUNK]; allowpc = true; }
//	else if(!str_cmp(field,"exitdir"))	{ ptr = (long*)&mob->exit_dir; allowpc = true; }
    else if(!str_cmp(field,"exp"))		{ lptr = &mob->exp; allowpc = true; }
    else if(!str_cmp(field,"fade"))		ptr = (int*)&mob->fade;
    else if(!str_cmp(field,"fullness"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_FULL]; allowpc = true; }
    else if(!str_cmp(field,"gold"))		lptr = &mob->gold;
    else if(!str_cmp(field,"hide"))		ptr = (int*)&mob->hide;
    else if(!str_cmp(field,"hit"))		lptr = &mob->hit;
    else if(!str_cmp(field,"hitdamage"))	ptr = (int*)&mob->hit_damage;
    else if(!str_cmp(field,"hitroll"))	ptr = (int*)&mob->hitroll;
    else if(!str_cmp(field,"hunger"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_HUNGER]; allowpc = true; }
    else if(!str_cmp(field,"imm"))		{ lptr = &mob->imm_flags; allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"level"))	ptr = (int*)&mob->tot_level;
    else if(!str_cmp(field,"lostparts"))	{ lptr = &mob->lostparts; allowarith = false; flags = part_flags; }
    else if(!str_cmp(field,"mana"))		lptr = &mob->mana;
    else if(!str_cmp(field,"manastore"))	{ ptr = (int*)&mob->manastore; allowpc = true; }
//	else if(!str_cmp(field,"material"))	ptr = (long*)&mob->material;
    else if(!str_cmp(field,"maxexp"))	lptr = &mob->maxexp;
    else if(!str_cmp(field,"maxhit"))	lptr = &mob->max_hit;
    else if(!str_cmp(field,"maxmana"))	lptr = &mob->max_mana;
    else if(!str_cmp(field,"maxmove"))	lptr = &mob->max_move;
    else if(!str_cmp(field,"mazed"))	{ ptr = (IS_NPC(mob))?NULL:(int*)&mob->maze_time_left; allowpc = true; }
    else if(!str_cmp(field,"modcon"))	{ ptr = (int*)&mob->mod_stat[STAT_CON]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_CON; }
    else if(!str_cmp(field,"moddex"))	{ ptr = (int*)&mob->mod_stat[STAT_DEX]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_DEX; }
    else if(!str_cmp(field,"modint"))	{ ptr = (int*)&mob->mod_stat[STAT_INT]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_INT; }
    else if(!str_cmp(field,"modstr"))	{ ptr = (int*)&mob->mod_stat[STAT_STR]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_STR; }
    else if(!str_cmp(field,"modwis"))	{ ptr = (int*)&mob->mod_stat[STAT_WIS]; allowpc = true; min_sec = IS_NPC(mob)?0:3; dirty_stat = STAT_WIS; }
    else if(!str_cmp(field,"move"))		lptr = &mob->move;
    else if(!str_cmp(field,"music"))	ptr = (int*)&mob->music;
    else if(!str_cmp(field,"norecall"))	ptr = (int*)&mob->no_recall;
    else if(!str_cmp(field,"panic"))	ptr = (int*)&mob->panic;
    else if(!str_cmp(field,"paralyzed"))	ptr = (int*)&mob->paralyzed;
    else if(!str_cmp(field,"paroxysm"))	ptr = (int*)&mob->paroxysm;
    else if(!str_cmp(field,"parts"))	{ lptr = &mob->parts; allowarith = false; flags = part_flags; }
    else if(!str_cmp(field,"permaffects"))	{ lptr = mob->affected_by_perm; bank = affect_flagbank; }
    //else if(!str_cmp(field,"permaffects2"))	{ ptr = (long*)&mob->affected_by_perm[1]; allowarith = false; flags = affect2_flags; }
    else if(!str_cmp(field,"permimm"))	{ lptr = &mob->imm_flags_perm; allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"permres"))	{ lptr = &mob->res_flags_perm; allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"permvuln"))	{ lptr = &mob->vuln_flags_perm; allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"pktimer"))	ptr = (int*)&mob->pk_timer;
    else if(!str_cmp(field,"pneuma"))	lptr = &mob->pneuma;
    else if(!str_cmp(field,"practice"))	ptr = &mob->practice;
    else if(!str_cmp(field,"race"))		{ min_sec = 7; allowarith = false; lookuprace = true; race_change_flags = RACE_CHANGE_SAVE_ORIGINAL; }
    else if(!str_cmp(field,"raceoverlay"))	{ min_sec = 7; allowarith = false; lookuprace = true; race_change_flags = RACE_CHANGE_SAVE_ORIGINAL | RACE_CHANGE_OVERLAY; }
    else if(!str_cmp(field,"racerevert"))	{ min_sec = 7; allowarith = false; lookuprace = true; race_change_flags = RACE_CHANGE_REVERT; }
    else if(!str_cmp(field,"ranged"))	ptr = (int*)&mob->ranged;
    else if(!str_cmp(field,"recite"))	ptr = (int*)&mob->recite;
    else if(!str_cmp(field,"res"))		{ lptr = &mob->res_flags;  allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"resurrect"))	ptr = (int*)&mob->resurrect;
    else if(!str_cmp(field,"reverie"))	ptr = (int*)&mob->reverie;
    else if(!str_cmp(field,"scribe"))	ptr = (int*)&mob->scribe;
    else if(!str_cmp(field,"sex"))		{ ptr = (int*)&mob->sex; min = 0; max = 2; hasmin = hasmax = true; flags = sex_flags; }
    else if(!str_cmp(field,"silver"))	lptr = &mob->silver;
    else if(!str_cmp(field,"size"))		{ ptr = (int*)&mob->size; min = SIZE_TINY; max = SIZE_GIANT; hasmin = hasmax = true; flags = size_flags; }
    else if(!str_cmp(field,"skillchance"))	ptr = (int*)&mob->skill_chance;
    else if(!str_cmp(field,"sublevel"))	ptr = (int*)&mob->level;
    else if(!str_cmp(field,"tempstore1"))	{ ptr = (int*)&mob->tempstore[0]; allowpc = true; }
    else if(!str_cmp(field,"tempstore2"))	{ ptr = (int*)&mob->tempstore[1]; allowpc = true; }
    else if(!str_cmp(field,"tempstore3"))	{ ptr = (int*)&mob->tempstore[2]; allowpc = true; }
    else if(!str_cmp(field,"tempstore4"))	{ ptr = (int*)&mob->tempstore[3]; allowpc = true; }
    else if(!str_cmp(field,"thirst"))	{ ptr = IS_NPC(mob)?NULL:(int*)&mob->pcdata->condition[COND_THIRST]; allowpc = true; }
    else if(!str_cmp(field,"toxinneuro"))	ptr = (int*)&mob->toxin[TOXIN_NEURO];
    else if(!str_cmp(field,"toxinpara"))	ptr = (int*)&mob->toxin[TOXIN_PARALYZE];
    else if(!str_cmp(field,"toxinvenom"))	ptr = (int*)&mob->toxin[TOXIN_VENOM];
    else if(!str_cmp(field,"toxinweak"))	ptr = (int*)&mob->toxin[TOXIN_WEAKNESS];
    else if(!str_cmp(field,"train"))	ptr = &mob->train;
    else if(!str_cmp(field,"trance"))	ptr = (int*)&mob->trance;
    else if(!str_cmp(field,"vuln"))		{ lptr = &mob->vuln_flags; allowarith = false; flags = imm_flags; }
    else if(!str_cmp(field,"wait"))		ptr = (int*)&mob->wait;
    else if(!str_cmp(field,"wildviewx"))	ptr = (int*)&mob->wildview_bonus_x;
    else if(!str_cmp(field,"wildviewy"))	ptr = (int*)&mob->wildview_bonus_y;
    else if(!str_cmp(field,"wimpy"))	ptr = (int*)&mob->wimpy;

    if(!lptr && !ptr) return;

    rest = one_argument(rest,buf);
    int op = cmd_operator_lookup(buf);
    if (op == OPR_UNKNOWN)
        return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "AlterMob - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }


    // MINIMUM to alter ANYTHING not allowed on players on a player
    if(!allowpc && !IS_NPC(mob)) min_sec = 9;

    if(script_security < min_sec) {
        pbugf(LOG_SCRIPTS,"OpAlterMob - Attempting to alter '%s' with security %d from vnum %ld.\n\r", field, script_security, VNUM(info->obj));
        return;
    }

    memset(temp_flags, 0, sizeof(temp_flags));

    if( lookuprace )
    {
        if (IS_SET(race_change_flags, RACE_CHANGE_REVERT)) {
            /* Revert to original race */
            if (op == OPR_ASSIGN)
                char_set_race(mob, NULL, race_change_flags | RACE_CHANGE_SILENT);
        } else {
            if( arg->type != ENT_STRING ) return;

            RACE_DATA *new_race = race_lookup(arg->d.str);
            if (new_race && op == OPR_ASSIGN)
                char_set_race(mob, new_race, race_change_flags | RACE_CHANGE_SILENT);
        }
        return;
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

    if (lptr) {
        switch (op) {
        case OPR_ADD:
            if (!allowarith) {
        pbugf(LOG_SCRIPTS, "TpAlterMob - altermob called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
        return;
    }
            *lptr += value;
            break;

        case OPR_SUB:
            *lptr -= value;
            break;

        case OPR_MULT:
            *lptr *= value;
            break;

        case OPR_DIV:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *lptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *lptr %= value;
            break;

        case OPR_INC:
            *lptr += 1;
            break;
        
        case OPR_DEC:
            *lptr -= 1;
            break;
        
        case OPR_MIN:
            *lptr = UMIN(*lptr, value);
            break;
        
        case OPR_MAX:
            *lptr = UMAX(*lptr, value);
            break;

        case OPR_ASSIGN:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] = temp_flags[i];
            }
            else
                *lptr = value;
            break;

        case OPR_AND:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] &= temp_flags[i];
            }
                if (!allowbitwise) {
        pbugf(LOG_SCRIPTS, "TpAlterMob - altermob called with bitwise operator on a non-bitvector field from vnum %ld.", VNUM(info->obj));
        return;
    }
            else
                *lptr &= value;
            break;

        case OPR_OR:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] |= temp_flags[i];
            }
            else
                *lptr |= value;
            break;

        case OPR_NOT:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] &= ~temp_flags[i];
            }
            else
                *lptr &= ~value;
            break;

        case OPR_XOR:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    lptr[i] ^= temp_flags[i];
            }
            else
                *lptr ^= value;

            break;
        default:
            return;
        }

        if(hasmin && *lptr < min)
            *lptr = min;

        if(hasmax && *lptr > max)
            *lptr = max;

    } else if (ptr) {
        switch (op) {
        case OPR_ADD:
            *ptr += value;
            break;

        case OPR_SUB:
            *ptr -= value;
            break;

        case OPR_MULT:
            *ptr *= value;
            break;

        case OPR_DIV:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr /= value;
            break;

        case OPR_MOD:
            if (!value) {
                pbugf(LOG_SCRIPTS, "AlterMob - altermob called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr %= value;
            break;

        case OPR_INC:
            *ptr += 1;
            break;
        
        case OPR_DEC:
            *ptr -= 1;
            break;
        
        case OPR_MIN:
            *ptr = UMIN(*ptr, value);
            break;
        
        case OPR_MAX:
            *ptr = UMAX(*ptr, value);
            break;

        case OPR_ASSIGN:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    ptr[i] = temp_flags[i];
            }
            else
                *ptr = value;
            break;

        case OPR_AND:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    ptr[i] &= temp_flags[i];
            }
            else
                *ptr &= value;
            break;

        case OPR_OR:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    ptr[i] |= temp_flags[i];
            }
            else
                *ptr |= value;
            break;

        case OPR_NOT:
            if (bank != NULL)
            {
                for(int i = 0; bank[i]; i++)
                    ptr[i] &= ~temp_flags[i];
            }
            else
                *ptr &= ~value;
            break;

        case OPR_XOR:
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
            *ptr = (int)min;

        if(hasmax && *ptr > max)
            *ptr = (int)max;
    }

    if(dirty_stat >= 0 && dirty_stat < MAX_STATS)
        mob->dirty_stat[dirty_stat] = true;
}


SCRIPT_CMD(do_opskimprove)
{
    char skill[MIL],*rest;
    int min_diff, diff, sn =-1;
    CHAR_DATA *mob = NULL;

    TOKEN_DATA *token = NULL;
    bool success = false;

    if(script_security < MIN_SCRIPT_SECURITY) {
        pbugf(LOG_SCRIPTS, "OpSkImprove - Insufficient security from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!info || !info->obj) return;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpSkImprove - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpSkImprove - NULL target from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(mob) {
        if(IS_NPC(mob)) {
            pbugf(LOG_SCRIPTS, "OpSkImprove - NPCs don't have skills to improve yet from vnum %ld.", VNUM(info->obj));
            return;
        }


        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "OpSkImprove - Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpSkImprove - Token is not a spell token from vnum %ld.", VNUM(info->obj));
            return;
        }
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpSkImprove - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: diff = is_number(arg->d.str) ? atoi(arg->d.str) : 0; break;
    case ENT_NUMBER: diff = arg->d.num; break;
    default: return;
    }

    min_diff = 10 - script_security;	// min=10, max=1

    if(diff < min_diff) {
        pbugf(LOG_SCRIPTS, "OpSkImprove - Attempting to use a difficulty multiplier lower than allowed from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpRawkill - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpRawkill - NULL mobile from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(IS_IMMORTAL(mob)) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpRawkill - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_lookup(arg->d.str,corpse_types); break;
    default: return;
    }

    if(type < 0 || type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpRawkill - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpRawkill - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAddAffect - NULL target from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: skill = skill_lookup(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "OpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
    af.skill = skill_find_uid(af.type);
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
        pbugf(LOG_SCRIPTS, "MpAddAffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "MpAddaffect - NULL target from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: where = flag_lookup(arg->d.str,apply_types); break;
    default: return;
    }

    if(where == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }



    switch(arg->type) {
    case ENT_STRING: name = create_affect_cname(arg->d.str); break;
    default: return;
    }

    if(!name) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error allocating affect name from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: loc = flag_lookup(arg->d.str,apply_flags_full); break;
    default: return;
    }

    if(loc == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: mod = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: hours = arg->d.num; break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv = flag_value(affect_flags,arg->d.str); break;
    default: return;
    }

    if(bv == NO_FLAG) bv = 0;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: bv2 = flag_value(affect2_flags,arg->d.str); break;
    default: return;
    }

    if(bv2 == NO_FLAG) bv2 = 0;

    if(rest && *rest) {
        if(!(rest = expand_argument(info,rest,arg))) {
            pbugf(LOG_SCRIPTS, "MpAddaffect - Error in parsing from vnum %ld.", VNUM(info->obj));
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



SCRIPT_CMD(do_opinput)
{
    char *rest, *p;
    long vnum;
    CHAR_DATA *mob = NULL;
    SCRIPT_DATA *script = NULL;


    if(!info || !info->obj) return;

    info->obj->progs->lastreturn = 0;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpInput - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpInput - NULL mobile from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(IS_NPC(mob) || !mob->desc || is_char_busy(mob) || mob->desc->pString != NULL || mob->desc->input) return;

    if( mob->desc->showstr_head != NULL ) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpInput - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    script = get_script_from_arg(info, arg, PRG_OPROG, &vnum);
    if(vnum < 1 || !script) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpInput - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - NULL target from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: type = flag_value(catalyst_types,arg->d.str); break;
    default: return;
    }

    if(type == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: method = flag_value(catalyst_method_types,arg->d.str); break;
    default: return;
    }

    if(method == NO_FLAG) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: amount = arg->d.num; break;
    case ENT_STRING: amount = atoi(arg->d.str); break;
    default: return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: min = arg->d.num; break;
    case ENT_STRING: min = atoi(arg->d.str); break;
    default: return;
    }

    if(min < 1 || min > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_NUMBER: max = arg->d.num; break;
    case ENT_STRING: max = atoi(arg->d.str); break;
    default: return;
    }

    if(max < min || max > CATALYST_MAXSTRENGTH) return;

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpUseCatalyst - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterExit - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    room = obj_room(info->obj);

    switch(arg->type) {
    case ENT_ROOM:
        room = arg->d.room;
        if(!(rest = expand_argument(info,rest,arg)) || arg->type != ENT_STRING) {
            pbugf(LOG_SCRIPTS, "OpAlterExit - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterExit - Missing field type from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpAlterExit - Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpAlterExit - Error in parsing from vnum %ld.", VNUM(info->obj));
            return;
        }

        switch(arg->type) {
        case ENT_NUMBER:
    {
        WNUM room_wnum;
        if (resolve_widevnum(arg->d.num, NULL, &room_wnum))
            room = get_room_index(room_wnum.pArea, room_wnum.vnum);
    }
    break;
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
            pbugf(LOG_SCRIPTS, "OpAlterExit - Empty string used from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpAlterExit - Error in parsing from vnum %ld.", VNUM(info->obj));
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
    else if(!str_cmp(field,"key"))				ptr = (int*)&ex->door.lock.key_wnum.vnum;
    else if(!str_cmp(field,"keyreset"))			{ ptr = (int*)&ex->door.rs_lock.key_wnum.vnum; min_sec = 7; }
    else if(!str_cmp(field,"pick"))				{ ptr = (int*)&ex->door.lock.pick_chance; min = 0; max = 100; hasmin = hasmax = true; }
    else if(!str_cmp(field,"pickreset"))		{ ptr = (int*)&ex->door.rs_lock.pick_chance; min_sec = 7; min = 0; max = 100; hasmin = hasmax = true; }

    if(!ptr && !sptr) return;

    if(script_security < min_sec) {
        sprintf(buf,"OpAlterExit - Attempting to alter '%s' with security %d.\n\r", field, script_security);
        wiznet(buf,NULL,NULL,WIZ_SCRIPTS,0,0);
        pbugf(LOG_SCRIPTS, "OpAlterExit - Attempting to alter '%s' with security %d from vnum %ld.", field, script_security, VNUM(info->obj));
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
                pbugf(LOG_SCRIPTS, "OpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr += value;
            break;
        case '-':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr -= value;
            break;
        case '*':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr *= value;
            break;
        case '/':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *ptr /= value;
            break;
        case '%':
            if( !allowarith ) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - alterexit called with arithmetic operator on a bitonly field from vnum %ld.", VNUM(info->obj));
                return;
            }
            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
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
                pbugf(LOG_SCRIPTS, "OpAlterExit - adjust called with operator / and value 0 from vnum %ld.", VNUM(info->obj));
                return;
            }
            *sptr /= value;
            break;
        case '%':
            if (!value) {
                pbugf(LOG_SCRIPTS, "OpAlterExit - adjust called with operator % and value 0 from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpPrompt - Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpPrompt - NULL mobile from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "OpPrompt - cannot set prompt strings on NPCs from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!*rest) {
        pbugf(LOG_SCRIPTS, "OpPrompt - Missing name type from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpPrompt - Error in parsing from vnum %ld.", VNUM(info->obj));
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

    WNUM source_wnum;
    if (!resolve_widevnum(vnum, NULL, &source_wnum) || !(source = get_room_index(source_wnum.pArea, source_wnum.vnum)))
        return;

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

    WNUM room_wnum;
    if (!resolve_widevnum(vnum, NULL, &room_wnum) || !(room = get_room_index(room_wnum.pArea, room_wnum.vnum)))
        return;

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
        pbugf(LOG_SCRIPTS, "OpShowMap - bad target for showing the map from vnum %ld.", VNUM(info->obj));
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
    int depth, ret, space = PRG_MPROG;
    long vnum;


    if(!info || !info->obj) return;

    if (!argument[0]) {
        pbugf(LOG_SCRIPTS, "OpCall: missing arguments from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(script_security < 5) {
        pbugf(LOG_SCRIPTS, "OpCall: Minimum security needed is 5 from vnum %ld.", VNUM(info->obj));
        return;
    }

    // Call depth checking
    depth = script_call_depth;
    if(script_call_depth == 1) {
        pbugf(LOG_SCRIPTS, "OpCall: maximum call depth exceeded for obj vnum %ld.", VNUM(info->obj));
        return;
    } else if(script_call_depth > 1)
        --script_call_depth;

    if(!(rest = expand_argument(info,argument,arg))) {
        pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpCall: No entity target from vnum %ld.", VNUM(info->obj));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    if(mob && !IS_NPC(mob)) {
        pbugf(LOG_SCRIPTS, "OpCall: Invalid target for xcall.  Players cannot do scripts from vnum %ld.", VNUM(info->obj));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    if(!(rest = expand_argument(info,rest,arg))) {
        pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
        // Restore the call depth to the previous value
        script_call_depth = depth;
        return;
    }

    script = get_script_from_arg(info, arg, space, &vnum);
    if (!script || vnum < 1) {
        pbugf(LOG_SCRIPTS, "OpCall: invalid prog from vnum %ld.", VNUM(info->obj));
        return;
    }

    ch = vch = NULL;
    obj1 = obj2 = NULL;

    if(*rest) {	// Enactor
        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
            // Restore the call depth to the previous value
            script_call_depth = depth;
            return;
        }

        argument = rest;
        if(!(rest = expand_argument(info,argument,arg))) {
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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
            pbugf(LOG_SCRIPTS, "OpCall: Error in parsing from vnum %ld.", VNUM(info->obj));
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

    ret = execute_script(script->vnum, script, mob, obj, room, token, NULL, NULL, NULL, ch, obj1, obj2, vch, NULL,NULL, NULL,info->phrase,info->trigger,info->trigger_type,0,0,0,0,0);
    if(info->obj)
        info->obj->progs->lastreturn = ret;
    else
        info->block->ret_val = ret;

    // restore the call depth to the previous value
    script_call_depth = depth;
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
        pbugf(LOG_SCRIPTS, "OpClearRecall - Bad syntax from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpClearRecall - Null victim from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
    case ENT_MOBILE: hunter = arg->d.mob; break;
    default: hunter = NULL; break;
    }

    if (!hunter) {
        pbugf(LOG_SCRIPTS, "OpHunt - Null hunter from vnum %ld.", VNUM(info->obj));
        return;
    }

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "OpHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: prey = get_char_world(NULL, arg->d.str); break;
    case ENT_MOBILE: prey = arg->d.mob; break;
    default: prey = NULL; break;
    }

    if (!prey) {
        pbugf(LOG_SCRIPTS, "OpHunt - Null prey from vnum %ld.", VNUM(info->obj));
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
        pbugf(LOG_SCRIPTS, "OpStopHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    stay = !str_cmp(arg->d.str,"true") || !str_cmp(arg->d.str,"yes") || !str_cmp(arg->d.str,"stay");

    if(!expand_argument(info,rest,arg)) {
        pbugf(LOG_SCRIPTS, "OpStopHunt - Error in parsing from vnum %ld.", VNUM(info->obj));
        return;
    }

    switch(arg->type) {
    case ENT_STRING: hunter = get_char_world(NULL, arg->d.str); break;
    case ENT_MOBILE: hunter = arg->d.mob; break;
    default: hunter = NULL; break;
    }

    if (!hunter) {
        pbugf(LOG_SCRIPTS, "OpStopHunt - Null hunter from vnum %ld.", VNUM(info->obj));
        return;
    }

    stop_hunt(hunter, stay);
    return;
}

// obj skill <player> <name> <op> <number>
// <op> =, +, -
SCRIPT_CMD(do_opskill)
{
    char buf[MIL];

    char *rest;
    CHAR_DATA *mob = NULL;
    SKILL_ENTRY *entry = NULL;
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

            entry = skill_entry_findsn(mob->sorted_skills, sn);
            if( value == 0 ) {
                if( skill_table[sn].spell_fun == spell_null )
                    skill_entry_removeskill(mob, sn, NULL);
                else
                    skill_entry_removespell(mob, sn, NULL);
            } else {
                if( !entry ) {
                    if( skill_table[sn].spell_fun == spell_null )
                        skill_entry_addskill(mob, sn, NULL, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);
                    else
                        skill_entry_addspell(mob, sn, NULL, SKILLSRC_SCRIPT, SKILL_AUTOMATIC);

                    entry = skill_entry_findsn(mob->sorted_skills, sn);
                }

                if( entry )
                    entry->rating = value;
            }

            mob->pcdata->learned[sn] = value;
            break;

        case '+':
            // Can only modify the skill, you cannot grant a skill using this.  Use the = operator.
            entry = skill_entry_findsn(mob->sorted_skills, sn);
            if(entry && entry->rating > 0)
            {
                value = entry->rating + value;

                if( value < 1 ) value = 1;
                else if( value > 100 ) value = 100;

                entry->rating = value;

                mob->pcdata->learned[sn] = value;
            }
            break;

        case '-':
            // Can only modify the skill, you cannot remove it using this.  Use the = operator.
            entry = skill_entry_findsn(mob->sorted_skills, sn);
            if(entry && entry->rating > 0)
            {
                value = entry->rating - value;

                if( value < 1 ) value = 1;
                else if( value > 100 ) value = 100;

                entry->rating = value;

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
